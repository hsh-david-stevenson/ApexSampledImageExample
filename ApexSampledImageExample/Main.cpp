/* -----------------------------------------------------------------------
 * <copyright file="Main.cpp" company="Global Graphics Software Ltd">
 *  Copyright (c) 2026 Global Graphics Software Ltd. All rights reserved.
 * </copyright>
 * <summary>
 *  This example is provided on an "as is" basis and without warranty of any kind.
 *  Global Graphics Software Ltd. does not warrant or make any representations
 *  regarding the use or results of use of this example.
 * </summary>
 * -----------------------------------------------------------------------
 */

#include <climits>
#include <vector>
#include <algorithm>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <cwctype>
#include <jawsmako/jawsmako.h>
#include <jawsmako/apexcustompostprocess.h>
#include <edl/platform_utils.h>
#include <edl/idompath.h>
#include <jawsmako/pdfoutput.h>
#include "renderoptions.h"
#include "oil_paint_controls.h"
#include "pencil_sketch_controls.h"

using namespace JawsMako;
using namespace EDL;

namespace
{

enum eImageFormat
{
    eIFUnknown,
    eIFPNG,
    eIFJPEG,
    eIFPDF,
    eIFTIFF
};

static eImageFormat imageFormatFromPath(const U8String& path)
{
    // Get the extension in lower case
    if (path.size() < 3)
    {
        // Cannot determine the extension if there isn't one!
        std::string message("Cannot determine file extension for path ");
        message += path.c_str();
        throw std::length_error(message);
    }
    size_t extensionPosition = path.find_last_of('.');
    if (extensionPosition != String::npos)
    {
        U8String extension = path.substr(extensionPosition);
        std::transform(extension.begin(), extension.end(), extension.begin(), tolower);
        if (extension == ".png")
            return eIFPNG;
        if (extension == ".jpg" || extension == ".jpeg")
            return eIFJPEG;
        if (extension == ".pdf")
            return eIFPDF;
        if (extension == ".tif" || extension == ".tiff")
            return eIFTIFF;
    }
    std::string message("Unsupported image file type for path ");
    message += path.c_str();
    throw std::invalid_argument(message);
}

// Determine the associated format for a given path from the file extension
static eFileFormat formatFromPath(const U8String& path)
{
    // Get the extension in lower case
    if (path.size() < 3)
    {
        // Cannot determine the extension if there isn't one!
        std::string message("Cannot determine file extension for path ");
        message += path.c_str();
        throw std::length_error(message);
    }

    size_t extensionPosition = path.find_last_of('.');
    if (extensionPosition != String::npos)
    {
        U8String extension = path.substr(extensionPosition);
        std::transform(extension.begin(), extension.end(), extension.begin(), tolower);

        if (extension == ".pdf")
            return eFFPDF;
        if (extension == ".xps")
            return eFFXPS;
        if (extension == ".oxps")
            return eFFOXPS;
        if (extension == ".ps")
            return eFFPS;
        if (extension == ".eps")
            return eFFEPS;
        if (extension == ".pcl")
            return eFFPCL5;
        if (extension == ".pxl")
            return eFFPCLXL;
    }

    std::string message("Unsupported file type for path ");
    message += path.c_str();
    throw std::invalid_argument(message);
}

// Load a shader from the given file
IApexRenderer::IFragmentShaderPtr loadShader(const IJawsMakoPtr& jawsMako, const IApexRendererPtr& apex, const U8String& filename)
{
    IRAInputStreamPtr shaderStream = IInputStream::createFromFile(jawsMako, filename);
    shaderStream->openE();
    int64 length = shaderStream->length();
    if (length < 0 || length > INT_MAX)
    {
        throwEDLError(JM_ERR_GENERAL, L"Error getting shader length, or it's too large!");
    }
    CEDLSimpleBuffer shaderBuff((uint32)length);
    shaderStream->completeReadE((void*)shaderBuff.data(), static_cast<int32>(shaderBuff.size()));
    shaderStream->close();
    return apex->createFragmentShader(shaderBuff.data(), static_cast<uint32>(shaderBuff.size()));
}
}

#ifdef _WIN32
int wmain(int argc, wchar_t* argv[])
#else
int main(int argc, char* argv[])
#endif
{






    // These are declared outside the try as it's important that any
    // resources used by the GPU are released *before* the Apex renderer
    // shuts down. Declaring them here ensures that Apex will be released
    // after anything declared inside the try block below.
    IJawsMakoPtr jawsMako;
    IApexRendererPtr apex;
    try
    {
        jawsMako = IJawsMako::create();
        IJawsMako::enableAllFeatures(jawsMako);

        // Keep UTF-8 argument storage alive while cxxopts parses it.
        std::vector<std::string> arguments;
        std::vector<const char*> argumentPointers;
        for (int index = 0; index < argc; ++index)
        {
#ifdef _WIN32
            arguments.emplace_back(StringToU8String(String(argv[index])).c_str());
#else
            arguments.emplace_back(argv[index]);
#endif
        }
        for (const auto& argument : arguments)
            argumentPointers.push_back(argument.c_str());

        cxxopts::Options options("ApexSampledImageExample", "Render pages with Apex shader effects");
        options.positional_help("<input> [<output>]").show_positional_help().set_width(108);
        registerGeneralOptions(options);
        registerRenderingOptions(options);
        registerOilPaintOptions(options);
        registerPencilSketchOptions(options);
        options.add_options()("h,help", "Show usage");
        const auto result = options.parse(argc, argumentPointers.data());
        if (result.count("help"))
        {
            std::cout << options.help() << '\n';
            return 0;
        }
        
        const GeneralOptions general = parseGeneralOptions(result);
        RenderingOptions renderingOptions(jawsMako);
        renderingOptions.parse(result);
        const OilPaintControls oilPaint = parseOilPaintOptions(result);
        const PencilSketchControls pencilSketch = parsePencilSketchOptions(result);

        U8String inputFilePath = general.inputFilePath.c_str();
        U8String outputFilePath = general.outputFilePath.c_str();

        U8String testFilesPath = R"(..\..\TestFiles\)";
        U8String shaderFilePath = R"(..\..\Shaders\)";

        // We use the file extensions to determine formats unless it is specified otherwise
        // by the first optional argument.
        // Any combination of PDF, XPS, PCL5 or PCLXL input, and 
        // PDF, XPS, PCL5, PCLXL or PostScript output is supported.

        // Declare our input and output pointers
        IInputPtr  input;
        IOutputPtr output;

        // Work out the input file format from the file extension of the input file path
        input = IInput::create(jawsMako, formatFromPath(inputFilePath));

        // We're accessing sequentially, so may as well engage sequential mode
        input->setSequentialMode(true);


        // Open
        IDocumentAssemblyPtr assembly = input->open(inputFilePath);

        // Create the renderer
        apex = IApexRenderer::create(jawsMako);

        // Load the shader for the chosen artistic effect, if any. The shader is a SPIR-V binary file, which is compiled from GLSL
        IApexRenderer::IFragmentShaderPtr regularTextureShader;
        if (general.artisticEffect != "none")
        {
            regularTextureShader =
                loadShader(jawsMako, apex, shaderFilePath + (general.artisticEffect == "oil" ? "oil_paint.spv" : "pencil_sketch.spv"));
        }
        
        // Ok - ready to go.

        // For all documents
        uint32 ordPageNum = 1;
        for (uint32 docNum = 0; assembly->documentExists(docNum); docNum++)
        {
            IDocumentPtr document = assembly->getDocument(docNum);
            for (uint32 pageNum = 0; document->pageExists(pageNum); pageNum++)
            {
                const auto pageNumber = ordPageNum++;
                const bool selected = std::any_of(general.pageRanges.begin(), general.pageRanges.end(),
                    [pageNumber](const GeneralOptions::PageRange& range) {
                        const auto start = range.start.value_or(1);
                        const auto end = range.end.value_or(INT_MAX);
                        return pageNumber >= static_cast<uint32>(std::min(start, end)) &&
                               pageNumber <= static_cast<uint32>(std::max(start, end));
                    });
                if (!selected) continue;
                double resolution = renderingOptions.xResolution;
                IPagePtr page = document->getPage(pageNum);
                IDOMFixedPagePtr content = page->getContent();
                page->revert();
                page->release();

                uint32 width = (uint32)(content->getWidth() / 96.0 * resolution + 0.5);
                uint32 height = (uint32)(content->getHeight() / 96.0 * renderingOptions.yResolution + 0.5);
                uint32 stride = width * 4;
                CEDLSimpleBuffer frameBuffer((size_t)stride * (size_t)height);

                // Try using the main basic colour spaces, and CMYKOGV
                CFrameBufferRenderSpec renderSpec;
                renderSpec.width = (uint32)(content->getWidth() / 96.0 * resolution + 0.5);
                renderSpec.height = (uint32)(content->getHeight() / 96.0 * renderingOptions.yResolution + 0.5);
                renderSpec.sourceRect = FRect(0.0, 0.0, content->getWidth(), content->getHeight());

                renderSpec.buffer = (void*)frameBuffer.data();
                renderSpec.rowStride = (int32)stride;

                // Add paint effect post process
                renderSpec.postProcesses.clear();
                if (general.artisticEffect != "none")
                {
                    renderSpec.processSpace = IDOMColorSpaceDeviceRGB::create(jawsMako);

                    if (general.artisticEffect == "oil")
                    {
                        OilPaintControls oilPaintControls = oilPaint;
                        oilPaintControls.width = renderSpec.width;
                        oilPaintControls.height = renderSpec.height;
                        CEDLSimpleBuffer pushConstants(sizeof(OilPaintControls));
                        memcpy((void*)pushConstants.data(), &oilPaintControls, sizeof(OilPaintControls));
                        renderSpec.postProcesses.append(CCustomColorTexturePostProcessSpec::create(renderingOptions.processSpace,
                            renderingOptions.processSpace,
                            regularTextureShader,
                            oilPaintRequiredExtend(oilPaintControls),
                            IApexRenderer::CTextureVect(),
                            pushConstants));
                    }
                    else if (general.artisticEffect == "pencil")
                    {
                        PencilSketchControls pencilControls = pencilSketch;
                        pencilControls.width = renderSpec.width;
                        pencilControls.height = renderSpec.height;
                        CEDLSimpleBuffer pushConstants(sizeof(PencilSketchControls));
                        memcpy((void*)pushConstants.data(), &pencilControls, sizeof(PencilSketchControls));
                        renderSpec.postProcesses.append(CCustomColorTexturePostProcessSpec::create(renderingOptions.processSpace,
                            renderingOptions.processSpace,
                            regularTextureShader,
                            pencilSketchRequiredExtend(pencilControls),
                            IApexRenderer::CTextureVect(),
                            pushConstants));
                    }
                }
                else
                {
                    renderSpec.processSpace = renderingOptions.processSpace;
                }

                // Convert to final space if required
                if (renderingOptions.finalSpace != renderingOptions.processSpace)
                {
                    renderSpec.postProcesses.append(CColorConversionPostProcessSpec::create(renderingOptions.finalSpace));
                }
                                
                // Render!
                apex->render(content, &renderSpec);


                switch (imageFormatFromPath(outputFilePath))
                {
                case eIFPNG:
                {
                    // Write to a PNG
                    char fileNameBuff[4096];
                    // Build the file name - we expect %u in the file path.
                    edlSnprintfE(fileNameBuff, sizeof(fileNameBuff), outputFilePath.c_str(), pageNumber);

                    // Create a PNG encoding frame
                    IImageFrameWriterPtr frame;
                    (void)IDOMPNGImage::createWriterAndImage(jawsMako,
                        frame,
                        renderingOptions.finalSpace,
                        width, height,
                        8,
                        resolution, resolution,
                        eIECNone,
                        IInputStream::createFromFile(jawsMako, fileNameBuff),
                        IOutputStream::createToFile(jawsMako, fileNameBuff));

                    // Out with it
                    for (uint32 y = 0; y < height; y++)
                    {
                        frame->writeScanLine(&frameBuffer[y * (size_t)stride]);
                    }
                    frame->flushData();
                    break;
                }


                case eIFJPEG:
                {
                    // Write to a PNG
                    char fileNameBuff[4096];
                    // Build the file name - we expect %u in the file path.
                    edlSnprintfE(fileNameBuff, sizeof(fileNameBuff), outputFilePath.c_str(), pageNumber);

                    // Create a JPEG encoding frame
                    IImageFrameWriterPtr frame;
                    (void)IDOMJPEGImage::createWriterAndImage(jawsMako,
                        frame,
                        renderingOptions.finalSpace,
                        width, height,
                        8,
                        resolution, resolution,
                        eIECNone,
                        IInputStream::createFromFile(jawsMako, fileNameBuff),
                        IOutputStream::createToFile(jawsMako, fileNameBuff));

                    // Out with it
                    for (uint32 y = 0; y < height; y++)
                    {
                        frame->writeScanLine(&frameBuffer[y * (size_t)stride]);
                    }
                    frame->flushData();
                    break;
                }

                case eIFTIFF:
                {
                    // Write to a tiff
                    char fileNameBuff[4096];
                    // Build the file name - we expect %u in the file path.
                    edlSnprintfE(fileNameBuff, sizeof(fileNameBuff), outputFilePath.c_str(), pageNumber);

                    // Create a TIFF encoding frame
                    IImageFrameWriterPtr frame;
                    (void)IDOMTIFFImage::createWriterAndImage(jawsMako,
                        frame,
                        renderingOptions.finalSpace,
                        width, height,
                        8,
                        resolution, resolution,
                        IDOMTIFFImage::eTCAuto,
                        IDOMTIFFImage::eTPNone,
                        eIECNone,
                        false,
                        IInputStream::createFromFile(jawsMako, fileNameBuff),
                        IOutputStream::createToFile(jawsMako, fileNameBuff));

                    // Out with it
                    for (uint32 y = 0; y < height; y++)
                    {    
                        frame->writeScanLine(&frameBuffer[y * (size_t)stride]);
                    }
                    frame->flushData();
                    break;
                }

                case eIFPDF:
                {
                    // Write to a PDF
                    char fileNameBuff[4096];
                    // Build the file name - we expect %u in the file path.
                    edlSnprintfE(fileNameBuff, sizeof(fileNameBuff), outputFilePath.c_str(), pageNumber);

                    // Create a Raw image encoding frame
                    IImageFrameWriterPtr frame;
                    const auto image = IDOMRawImage::createWriterAndImage(
                        jawsMako,
                        frame,
                        renderingOptions.finalSpace,
                        width, height,
                        8,
                        resolution, resolution,
                        eIECNone,
                        IInputStream::createFromFile(jawsMako, fileNameBuff),
                        IOutputStream::createToFile(jawsMako, fileNameBuff));

                    // Out with it
                    for (uint32 y = 0; y < height; y++)
                    {
                        frame->writeScanLine(&frameBuffer[y * (size_t)stride]);
                    }
                    frame->flushData();

                    const auto cropBox = FRect(0.0, 0.0, content->getWidth(), content->getHeight());

                    // Wrap in a brush
                    IDOMBrushPtr brush = IDOMImageBrush::create(jawsMako, image, FRect(), FRect());

                    // Wrap in a path
                    IDOMPathNodePtr path = IDOMPathNode::createFilled(jawsMako,
                        IDOMPathGeometry::create(jawsMako, cropBox),
                        brush);

                    // Create new content
                    IDOMFixedPagePtr newContent = IDOMFixedPage::create(jawsMako, content->getWidth(), content->getHeight());
                    newContent->setCropBox(cropBox);
                    newContent->appendChild(path);

                    // Write a PDF with that content
                    const auto outAssembly = IDocumentAssembly::create(jawsMako);
                    const auto outDocument = IDocument::create(jawsMako);
                    outAssembly->appendDocument(outDocument);
                    const auto outPage = IPage::create(jawsMako);
                    outDocument->appendPage(outPage);
                    outPage->setContent(newContent);
                    IPDFOutput::create(jawsMako)->writeAssembly(outAssembly, fileNameBuff);
                    
                    break;
                }
                case eIFUnknown:
                    break;
                }
            }
        }
    }
    catch (IError& e)
    {
        String errorFormatString = getEDLErrorString(e.getErrorCode());
        std::wcerr << L"Exception thrown: " << e.getErrorDescription(errorFormatString) << std::endl;
#ifdef _WIN32
        // On windows, the return code allows larger numbers, and we can return the error code
        return e.getErrorCode();
#else
        // On other platforms, the exit code is masked to the low 8 bits. So here we just return
        // a fixed value.
        return 1;
#endif
    }
    catch (std::exception& e)
    {
        std::wcerr << L"std::exception thrown: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}