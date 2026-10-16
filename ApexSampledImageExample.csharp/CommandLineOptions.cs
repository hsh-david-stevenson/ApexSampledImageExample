using System.CommandLine;
using System.Globalization;
using System.Text.RegularExpressions;

namespace ApexSampledImageExample;

internal sealed class CommandLineOptions
{
    public RootCommand Command { get; } = new("Render pages with Apex shader effects");
    private readonly Dictionary<string, Option> options = new();
    private readonly Argument<string?> input = new("input") { Arity = ArgumentArity.ZeroOrOne };
    private readonly Argument<string?> output = new("output") { Arity = ArgumentArity.ZeroOrOne };

    public CommandLineOptions()
    {
        Command.Arguments.Add(input);
        Command.Arguments.Add(output);
        Add("infile", "", "Path to input file (or supply the input positional argument).");
        Add("outfile", "", "Output path; %u is replaced by the page number. Default: input stem + _%u.tif.");
        Add("pages", "1-", "Comma-delimited page ranges, e.g. 1-10,30,100-.", "-p");
        Add("art", "none", "Artistic effect: none, oil or pencil.");
        Add("resolution", "150", "DPI: one value or x,y for non-square rendering.", "-d");
        Add("bpc", 8, "Output bits per component: 8 or 16.", "-b");
        Add("overprintpreview", false, "Merge spot colours for overprint preview.", "-v");
        Add("aafactor", 1, "Anti-alias factor: 1, 2, 3 or 4.", "-i");
        Add("bigtiff", false, "Use BigTIFF for TIFF output larger than 4GB.", "-B");
        Add("sixteen", false, "Use 16-bit internal rendering.", "-s");
        Add("renderspace", "rgb", "Rendering colour space: gray, rgb or cmyk; artistic effects use RGB.", "-r");
        Add("renderprofile", "", "Rendering ICC profile path.", "-R");
        Add("finalspace", "", "Final colour space: gray, rgb or cmyk; default is render space.", "-f");
        Add("finalprofile", "", "Final ICC profile path, including compatible device links.", "-F");
        Add("ocgevent", "print", "Optional-content event: view, print or export.", "-O");
        Add("gputhreads", 1, "Number of render threads per GPU.", "-g");
        Add("useallgpus", false, "Use all available GPUs for different pages.", "-G");
        Add("exclude", true, "Exclude integrated GPUs when using all GPUs.", "-x");
        Add("dump", false, "Print the effective render specification.", "-U");

        // Names and defaults mirror the current C++ options and control headers.
        Add("op-radius", 4u, "Oil: radius (0-64); larger values make larger daubs.");
        Add("op-bins", 16u, "Oil: bins (2-32); more bins retain more detail.");
        Add("op-strength", 1f, "Oil: blend between original and painted output.");
        Add("op-centre-weight", 0f, "Oil: centre weighting; try 0-3 to retain detail.");
        Add("op-bristle-spacing", 4f, "Oil: spacing between bristles in pixels.");
        Add("op-stroke-length", 80f, "Oil: stroke length in pixels.");
        Add("op-stroke-angle", 25f, "Oil: stroke orientation in degrees.");
        Add("op-curvature", 3f, "Oil: lateral bending in pixels.");
        Add("op-relief-depth", 0.8f, "Oil: paint depth; zero disables relief.");
        Add("op-light-strength", 0.65f, "Oil: relief lighting intensity.");
        Add("op-light-angle", 135f, "Oil: light direction in degrees.");
        Add("op-light-elevation", 45f, "Oil: light elevation in degrees.");
        Add("op-shine", 0.25f, "Oil: highlight strength.");
        Add("op-feature-follow", 1f, "Oil: blend from manual (0) to feature-guided relief (1).");
        Add("op-direction", 4u, "Oil: direction smoothing radius, 1-8 pixels.");
        Add("op-feature", 0.03f, "Oil: feature contrast; lower follows weaker features.");
        Add("ps-blur-radius", 8u, "Pencil: neighbourhood radius, 1-32 pixels.");
        Add("ps-blur-sigma", 4f, "Pencil: smoothing scale.");
        Add("ps-line-strength", 1.4f, "Pencil: contour darkness.");
        Add("ps-tone-strength", 0.15f, "Pencil: continuous graphite shading.");
        Add("ps-hatch-strength", 0.35f, "Pencil: cross-hatching intensity.");
        Add("ps-hatch-spacing", 6f, "Pencil: hatch spacing in pixels.");
        Add("ps-hatch-angle", -35f, "Pencil: hatch direction in degrees.");
        Add("ps-paper-strength", 0.025f, "Pencil: paper grain strength.");
    }

    private void Add<T>(string name, T defaultValue, string description, params string[] aliases)
    {
        var option = new Option<T>("--" + name, aliases)
        {
            Description = description,
            DefaultValueFactory = _ => defaultValue
        };
        if (typeof(T) == typeof(float))
            option.Validators.Add(result =>
            {
                // Invalid numeric tokens already have conversion errors. Do not
                // request their converted value from inside a validator.
                if (result.Tokens.Count != 0 && float.TryParse(result.Tokens[0].Value,
                    NumberStyles.Float, CultureInfo.InvariantCulture, out float value) && !float.IsFinite(value))
                    result.AddError($"--{name} must be finite.");
            });
        options.Add(name, option);
        Command.Options.Add(option);
    }

    private T Get<T>(ParseResult result, string name) => result.GetValue((Option<T>)options[name])!;

    public Settings Read(ParseResult result)
    {
        string inputPath = Get<string>(result, "infile");
        bool namedInput = inputPath.Length != 0;
        if (inputPath.Length == 0) inputPath = result.GetValue(input) ?? "";
        if (inputPath.Length == 0) throw new ArgumentException("An input file path is required.");
        if (!File.Exists(inputPath)) throw new ArgumentException("Input file not found.");
        string outputPath = Get<string>(result, "outfile");
        if (namedInput && result.GetValue(input) is { } positionalOutput)
        {
            if (outputPath.Length != 0 || result.GetValue(output) is not null)
                throw new ArgumentException("Too many positional file paths.");
            outputPath = positionalOutput;
        }
        else if (outputPath.Length == 0) outputPath = result.GetValue(output) ?? "";
        if (outputPath.Length == 0) outputPath = Path.GetFileNameWithoutExtension(inputPath) + "_%u.tif";
        string art = Get<string>(result, "art");
        if (art is not ("none" or "oil" or "pencil"))
            throw new ArgumentException("Artistic effect must be none, oil or pencil.");
        int[] dpi = Get<string>(result, "resolution").Split(',')
            .Select(value => int.TryParse(value, NumberStyles.Integer, CultureInfo.InvariantCulture, out int n) ? n : 0)
            .ToArray();
        if (dpi.Length is < 1 or > 2 || dpi.Any(value => value <= 0))
            throw new ArgumentException("Resolution requires one or two positive DPI values.");
        int aa = Get<int>(result, "aafactor");
        if (aa is < 1 or > 4) throw new ArgumentException("Anti-alias factor must be 1-4.");
        int threads = Get<int>(result, "gputhreads");
        if (threads < 1) throw new ArgumentException("Threads per GPU must be positive.");

        return new Settings
        {
            InputPath = inputPath, OutputPath = outputPath, Art = art,
            Pages = PageRange.Parse(Get<string>(result, "pages")),
            XResolution = (uint)dpi[0], YResolution = (uint)dpi[^1],
            Depth = Get<int>(result, "bpc") <= 8 ? (byte)8 : (byte)16,
            AaFactor = (byte)aa, OverprintPreview = Get<bool>(result, "overprintpreview"),
            BigTiff = Get<bool>(result, "bigtiff"), Sixteen = Get<bool>(result, "sixteen"),
            RenderSpace = Get<string>(result, "renderspace"), RenderProfile = Get<string>(result, "renderprofile"),
            FinalSpace = Get<string>(result, "finalspace"), FinalProfile = Get<string>(result, "finalprofile"),
            OcgEvent = Get<string>(result, "ocgevent"), GpuThreads = threads,
            UseAllGpus = Get<bool>(result, "useallgpus"), ExcludeIntegrated = Get<bool>(result, "exclude"),
            Dump = Get<bool>(result, "dump"),
            Oil = new OilPaintControls
            {
                Width = 1, Height = 1, Radius = Get<uint>(result, "op-radius"), Bins = Get<uint>(result, "op-bins"),
                Strength = Get<float>(result, "op-strength"), CentreWeight = Get<float>(result, "op-centre-weight"),
                BristleSpacing = Get<float>(result, "op-bristle-spacing"), StrokeLength = Get<float>(result, "op-stroke-length"),
                StrokeAngle = Get<float>(result, "op-stroke-angle"), Curvature = Get<float>(result, "op-curvature"),
                ReliefDepth = Get<float>(result, "op-relief-depth"), LightingStrength = Get<float>(result, "op-light-strength"),
                LightAngle = Get<float>(result, "op-light-angle"), LightElevation = Get<float>(result, "op-light-elevation"),
                Shine = Get<float>(result, "op-shine"), FeatureFollow = Get<float>(result, "op-feature-follow"),
                DirectionRadius = Get<uint>(result, "op-direction"), FeatureContrast = Get<float>(result, "op-feature")
            },
            Pencil = new PencilSketchControls
            {
                Width = 1, Height = 1, BlurRadius = Get<uint>(result, "ps-blur-radius"),
                BlurSigma = Get<float>(result, "ps-blur-sigma"), LineStrength = Get<float>(result, "ps-line-strength"),
                ToneStrength = Get<float>(result, "ps-tone-strength"), HatchStrength = Get<float>(result, "ps-hatch-strength"),
                HatchSpacing = Get<float>(result, "ps-hatch-spacing"), HatchAngle = Get<float>(result, "ps-hatch-angle"),
                PaperStrength = Get<float>(result, "ps-paper-strength")
            }
        };
    }
}

internal sealed record Settings
{
    public required string InputPath { get; init; }
    public required string OutputPath { get; init; }
    public required string Art { get; init; }
    public required PageRange[] Pages { get; init; }
    public uint XResolution { get; init; }
    public uint YResolution { get; init; }
    public byte Depth { get; init; }
    public byte AaFactor { get; init; }
    public bool OverprintPreview { get; init; }
    public bool BigTiff { get; init; }
    public bool Sixteen { get; init; }
    public required string RenderSpace { get; init; }
    public required string RenderProfile { get; init; }
    public required string FinalSpace { get; init; }
    public required string FinalProfile { get; init; }
    public required string OcgEvent { get; init; }
    public int GpuThreads { get; init; }
    public bool UseAllGpus { get; init; }
    public bool ExcludeIntegrated { get; init; }
    public bool Dump { get; init; }
    public OilPaintControls Oil { get; init; }
    public PencilSketchControls Pencil { get; init; }
}

internal readonly record struct PageRange(int? Start, int? End)
{
    public bool Contains(uint page)
    {
        int start = Start ?? 1, end = End ?? int.MaxValue;
        return page >= (uint)Math.Min(start, end) && page <= (uint)Math.Max(start, end);
    }

    public static PageRange[] Parse(string text)
    {
        return text.Split(',').Select(token =>
        {
            token = token.Trim();
            if (token.Length == 0 || !Regex.IsMatch(token, @"^(?:\d+|\d*-\d*)$"))
                throw new ArgumentException("Page ranges must contain positive page numbers or from-to ranges.");
            static int? Number(string value) => value.Length == 0 ? null
                : int.TryParse(value, NumberStyles.None, CultureInfo.InvariantCulture, out int n) && n > 0 ? n
                : throw new ArgumentException("Page numbers must be positive integers.");
            string[] parts = token.Split('-');
            return parts.Length == 1 ? new PageRange(Number(parts[0]), Number(parts[0]))
                                     : new PageRange(Number(parts[0]), Number(parts[1]));
        }).ToArray();
    }
}
