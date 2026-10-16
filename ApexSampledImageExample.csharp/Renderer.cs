using System.Collections.Concurrent;
using System.Globalization;
using System.Text.RegularExpressions;
using JawsMako;

namespace ApexSampledImageExample;

internal static class Renderer
{
    public static void Run(Settings settings)
    {
        ShaderConstants.CheckLayouts();
        string extension = Path.GetExtension(settings.OutputPath).ToLowerInvariant();
        if (extension is not (".png" or ".jpg" or ".jpeg" or ".tif" or ".tiff" or ".pdf"))
            throw new ArgumentException("Output must be PNG, JPEG, TIFF or PDF.");
        if (settings.Depth == 16 && extension is ".jpg" or ".jpeg")
            throw new ArgumentException("JPEG output requires 8 bits per component.");
        // Validate the template before opening an output stream.
        _ = OutputPath(settings.OutputPath, 1);
        Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(settings.OutputPath))!);

        // Dispose in reverse order: page GPU resources, shaders, Apex, then Mako.
        using var native = new NativeScope();
        var mako = native.Own(IJawsMako.create());
        IJawsMako.enableAllFeatures(mako);
        IDOMColorSpace processSpace = Space(native, mako, settings.RenderSpace, settings.RenderProfile);
        if (settings.Art != "none" && processSpace.getNumComponents() != 3)
            processSpace = native.Own(IDOMColorSpaceDeviceRGB.create(mako));
        IDOMColorSpace finalSpace = string.IsNullOrEmpty(settings.FinalSpace) && string.IsNullOrEmpty(settings.FinalProfile)
            ? processSpace : Space(native, mako, settings.FinalSpace, settings.FinalProfile);

        var input = native.Own(IInput.create(mako, InputFormat(settings.InputPath)));
        input.setSequentialMode(true);
        var assembly = native.Own(input.open(settings.InputPath));

        var sessions = new ConcurrentQueue<RenderSession>();
        foreach (int device in DeviceIndexes(settings))
        {
            for (int thread = 0; thread < settings.GpuThreads; ++thread)
            {
                var apex = native.Own(IApexRenderer.create(mako, device));
                IApexRenderer.IFragmentShader? shader = null;
                if (settings.Art != "none")
                {
                    string file = settings.Art == "oil" ? "oil_paint.spv" : "pencil_sketch.spv";
                    byte[] bytes = File.ReadAllBytes(Path.Combine(AppContext.BaseDirectory, "Shaders", file));
                    shader = native.Own(apex.createFragmentShader(bytes, checked((uint)bytes.Length)));
                }
                sessions.Enqueue(new RenderSession(apex, shader));
            }
        }
        // A literal output filename retains C++'s last-page-wins behaviour.
        // Serialize that case so two pages never write the same file concurrently.
        bool numberedOutput = Regex.Matches(settings.OutputPath, @"%%|%(?:0?\d*)u")
            .Any(match => match.Value != "%%");
        int degree = numberedOutput ? sessions.Count : 1;
        var pages = Partitioner.Create(ReadPages(assembly, settings), EnumerablePartitionerOptions.NoBuffering);
        Parallel.ForEach(pages, new ParallelOptions { MaxDegreeOfParallelism = degree }, work =>
        {
            if (!sessions.TryDequeue(out RenderSession? session))
                throw new InvalidOperationException("No renderer available.");
            using (work)
            {
                try { RenderPage(mako, session, work, settings, processSpace, finalSpace, extension); }
                finally { sessions.Enqueue(session); }
            }
        });
    }

    private static IEnumerable<int> DeviceIndexes(Settings settings)
    {
        if (!settings.UseAllGpus) return [-1];
        using var devices = IApexRenderer.enumerateGpus();
        var indexes = new List<int>();
        bool integratedSeen = false;
        for (uint i = 0; i < devices.size(); ++i)
        {
            using var device = devices[i];
            if (!device.discreteDevice)
            {
                if (settings.ExcludeIntegrated || integratedSeen) continue;
                integratedSeen = true;
            }
            indexes.Add(checked((int)device.index));
        }
        if (indexes.Count == 0) throw new InvalidOperationException("No GPU matches the selection settings.");
        return indexes;
    }

    private static IEnumerable<PageWork> ReadPages(IDocumentAssembly assembly, Settings settings)
    {
        uint ordinal = 0;
        for (uint d = 0; assembly.documentExists(d); ++d)
        {
            using var document = assembly.getDocument(d);
            for (uint p = 0; document.pageExists(p); ++p)
            {
                uint number = checked(++ordinal);
                if (!settings.Pages.Any(range => range.Contains(number))) continue;
                using var page = document.getPage(p);
                IDOMFixedPage content = page.getContent();
                PageWork work;
                try
                {
                    page.revert();
                    page.release();
                    work = new PageWork(number, content, document.getOptionalContent());
                }
                catch
                {
                    content.Dispose();
                    throw;
                }
                yield return work;
            }
        }
    }

    private static void RenderPage(IJawsMako mako, RenderSession session, PageWork work,
        Settings settings, IDOMColorSpace processSpace, IDOMColorSpace finalSpace, string extension)
    {
        using var native = new NativeScope();
        uint width = checked((uint)(work.Content.getWidth() / 96.0 * settings.XResolution + 0.5));
        uint height = checked((uint)(work.Content.getHeight() / 96.0 * settings.YResolution + 0.5));
        if (width == 0 || height == 0) throw new ArgumentException("Resolution produces an empty image.");
        int stride = checked((int)width * finalSpace.getNumComponents() * (settings.Depth / 8));
        byte[] buffer = new byte[checked(stride * (int)height)];
        var spec = native.Own(new CFrameBufferRenderSpec
        {
            width = width, height = height, processSpace = processSpace,
            sourceRect = native.Own(new FRect(0, 0, work.Content.getWidth(), work.Content.getHeight())),
            depth = settings.Depth, aaFactor = settings.AaFactor, alpha = false,
            rowStride = stride, hostEndian = false,
            use16BitInternalRendering = settings.Sixteen,
            optionalContent = work.OptionalContent,
            optionalContentEvent = settings.OcgEvent.ToLowerInvariant() switch
            {
                "view" => eOptionalContentEvent.eOCEView,
                "export" => eOptionalContentEvent.eOCEExport,
                _ => eOptionalContentEvent.eOCEPrint
            }
        });
        var post = native.Own(new CEDLVectIPostProcessSpec());
        uint extend = 0;
        if (settings.Art != "none")
        {
            CEDLSimpleBuffer constants;
            if (settings.Art == "oil")
            {
                var controls = settings.Oil;
                controls.Width = width; controls.Height = height;
                extend = controls.RequiredExtend();
                constants = native.Own(ShaderConstants.Create(controls));
            }
            else
            {
                var controls = settings.Pencil;
                controls.Width = width; controls.Height = height;
                extend = controls.RequiredExtend();
                constants = native.Own(ShaderConstants.Create(controls));
            }
            var textures = native.Own(new CEDLVectITexture());
            var effect = native.Own(CCustomColorTexturePostProcessSpec.create(
                processSpace, processSpace, session.Shader!, extend, textures, constants));
            post.append(effect);
        }

        if (finalSpace != processSpace)
            post.append(native.Own(CColorConversionPostProcessSpec.create(finalSpace)));

        if (settings.OverprintPreview)
        {
            using var inks = IRendererTransform.findInks(mako, work.Content);
            using var candidates = IRendererTransform.inkInfoToColorantInfo(mako, inks, finalSpace);
            var spots = native.Own(new CEDLVectColorantInfo());
            for (uint i = 0; i < candidates.size(); ++i)
            {
                using var info = candidates[i];
                if (info.name is not ("Cyan" or "Magenta" or "Yellow" or "Black" or "All" or "None"))
                    spots.append(info);
            }
            if (!spots.empty()) post.append(native.Own(CSpotMergePostProcessSpec.create(spots)));
        }

        spec.postProcesses = post;
        if (settings.Dump)
            Console.WriteLine($"Page {work.Number}: {width}x{height}, DPI={settings.XResolution},{settings.YResolution}, " +
                $"depth={spec.depth}, AA={spec.aaFactor}, internal16={spec.use16BitInternalRendering}, " +
                $"process={processSpace.getNumComponents()} channels, final={finalSpace.getNumComponents()} channels, " +
                $"effect={settings.Art}, extend={extend}, OCG={settings.OcgEvent}");
        session.Apex.render(work.Content, spec, buffer);
        WriteImage(mako, work, settings, finalSpace, extension, width, height, stride, buffer);
    }

    private static void WriteImage(IJawsMako mako, PageWork work, Settings settings,
        IDOMColorSpace space, string extension, uint width, uint height, int stride, byte[] buffer)
    {
        using var native = new NativeScope();
        string path = OutputPath(settings.OutputPath, work.Number);
        Pair_IDOMIMage_IImageFrameWriter pair;
        if (extension == ".pdf")
            pair = native.Own(IDOMRawImage.createWriterAndImage(mako, space, width, height,
                settings.Depth, settings.XResolution, settings.YResolution, eImageExtraChannelType.eIECNone));
        else
        {
            var reader = native.Own(IInputStream.createFromFile(mako, path));
            var writer = native.Own(IOutputStream.createToFile(mako, path));
            pair = native.Own(extension switch
            {
                ".png" => IDOMPNGImage.createWriterAndImage(mako, space, width, height,
                    settings.Depth, settings.XResolution, settings.YResolution, eImageExtraChannelType.eIECNone, reader, writer),
                ".jpg" or ".jpeg" => IDOMJPEGImage.createWriterAndImage(mako, space, width, height,
                    settings.Depth, settings.XResolution, settings.YResolution, 3, reader, writer),
                _ => IDOMTIFFImage.createWriterAndImage(mako, space, width, height,
                    settings.Depth, settings.XResolution, settings.YResolution,
                    IDOMTIFFImage.eTIFFCompression.eTCAuto, IDOMTIFFImage.eTIFFPrediction.eTPNone,
                    eImageExtraChannelType.eIECNone, settings.BigTiff, reader, writer)
            });
        }
        var frame = native.Own(pair.frameWriter);
        var image = native.Own(pair.domImage);
        for (uint y = 0; y < height; ++y)
            frame.writeScanLineOfs(buffer, checked(y * (uint)stride));
        frame.flushData();
        if (extension != ".pdf") return;

        var crop = native.Own(new FRect(0, 0, work.Content.getWidth(), work.Content.getHeight()));
        var brush = native.Own(IDOMImageBrush.create(mako, image, native.Own(new FRect()), crop));
        var geometry = native.Own(IDOMPathGeometry.create(mako, crop));
        var node = native.Own(IDOMPathNode.createFilled(mako, geometry, brush));
        var content = native.Own(IDOMFixedPage.create(mako, work.Content.getWidth(), work.Content.getHeight()));
        content.setCropBox(crop);
        content.appendChild(node);
        var assembly = native.Own(IDocumentAssembly.create(mako));
        var document = native.Own(IDocument.create(mako));
        var page = native.Own(IPage.create(mako));
        page.setContent(content);
        document.appendPage(page);
        assembly.appendDocument(document);
        var output = native.Own(IPDFOutput.create(mako));
        output.writeAssembly(assembly, path);
    }

    private static IDOMColorSpace Space(NativeScope native, IJawsMako mako, string name, string profilePath)
    {
        if (profilePath.Length != 0)
        {
            if (!File.Exists(profilePath)) throw new FileNotFoundException("ICC profile not found.", profilePath);
            var stream = native.Own(IInputStream.createFromFile(mako, profilePath));
            var profile = native.Own(IDOMICCProfile.create(mako, stream));
            return native.Own(IDOMColorSpaceICCBased.create(mako, profile));
        }
        return name.ToLowerInvariant() switch
        {
            "gray" => native.Own(IDOMColorSpaceDeviceGray.create(mako)),
            "cmyk" => native.Own(IDOMColorSpaceDeviceCMYK.create(mako)),
            "rgb" or "" => native.Own(IDOMColorSpaceDeviceRGB.create(mako)),
            _ => throw new ArgumentException("Colour space must be gray, rgb or cmyk.")
        };
    }

    private static eFileFormat InputFormat(string path) => Path.GetExtension(path).ToLowerInvariant() switch
    {
        ".pdf" => eFileFormat.eFFPDF,
        ".xps" => eFileFormat.eFFXPS,
        ".oxps" => eFileFormat.eFFOXPS,
        ".ps" => eFileFormat.eFFPS,
        ".eps" => eFileFormat.eFFEPS,
        ".pcl" => eFileFormat.eFFPCL5,
        ".pxl" => eFileFormat.eFFPCLXL,
        _ => throw new ArgumentException("Unsupported input file type.")
    };

    internal static string OutputPath(string template, uint number)
    {
        return Regex.Replace(template, @"%%|%(?<pad>0?)(?<width>\d*)u|%", match =>
        {
            if (match.Value == "%%") return "%";
            if (match.Value == "%") throw new ArgumentException("Output template accepts only %u, %03u-style page numbers and %%.");
            int width = match.Groups["width"].Value.Length == 0 ? 0
                : int.Parse(match.Groups["width"].Value, CultureInfo.InvariantCulture);
            if (width > 1000) throw new ArgumentException("Page-number padding is too large.");
            return number.ToString(CultureInfo.InvariantCulture).PadLeft(width,
                match.Groups["pad"].Value == "0" ? '0' : ' ');
        });
    }

    private sealed record RenderSession(IApexRenderer Apex, IApexRenderer.IFragmentShader? Shader);

    private sealed record PageWork(uint Number, IDOMFixedPage Content, IOptionalContent? OptionalContent) : IDisposable
    {
        public void Dispose() { OptionalContent?.Dispose(); Content.Dispose(); }
    }
}

// Native wrappers are deterministic resources. Dispose dependent objects first.
internal sealed class NativeScope : IDisposable
{
    private readonly Stack<IDisposable> objects = new();
    public T Own<T>(T value) where T : IDisposable { objects.Push(value); return value; }
    public void Dispose() { while (objects.TryPop(out var value)) value.Dispose(); }
}
