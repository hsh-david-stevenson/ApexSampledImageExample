/* -----------------------------------------------------------------------
 * <copyright file="renderoptions.cpp" company="Global Graphics Software Ltd">
 *  Copyright (c) 2026 Global Graphics Software Ltd. All rights reserved.
 * </copyright>
 * <summary>
 *  This example is provided on an "as is" basis and without warranty of any kind.
 *  Global Graphics Software Ltd. does not warrant or make any representations
 *  regarding the use or results of use of this example.
 * </summary>
 * -----------------------------------------------------------------------
 */


// ReSharper disable CppClangTidyReadabilityRedundantStringCstr
#include <locale>
#include <algorithm>
#include <filesystem>

#include <jawsmako/jawsmako.h>
#include <edl/idomcolorspace.h>

#include "renderoptions.h"

#include "cxxopts.hpp"

using namespace JawsMako;
using namespace EDL;
namespace fs = std::filesystem;

void registerRenderingOptions(cxxopts::Options& options)
{
    options.add_options("Rendering")
            ("d,resolution", "Resolution in DPI. Specify two x,y values (e.g., 300,600) for non-square rendering.",
                cxxopts::value<std::vector<int>>()->default_value("150"))

            ("b,bpc", "Output bit depth (per component): 8 or 16",
                cxxopts::value<int>()->default_value("8"))

            ("v,overprintpreview", "Overprint preview (cannot be used with alpha)",
                cxxopts::value<bool>()->implicit_value("true")->default_value("false"))

            ("i,aafactor", "Anti-alias factor: 1, 2, 3 or 4",
                cxxopts::value<int>()->default_value("1"))

            ("B,bigtiff", "Use BigTIFF to support TIFF output larger than 4GB",
                cxxopts::value<bool>()->implicit_value("true")->default_value("false"))

            ("s,sixteen", "Use 16-bit internal rendering. If true, all intermediate rendering is performed using 16-bit depth before being downconverted to 8-bit.",
                cxxopts::value<bool>()->implicit_value("true")->default_value("false"))

            ("r,renderspace", "Rendering color space: gray, rgb or cmyk (always RGB when an artistic effect is applied)",
                cxxopts::value<std::string>()->default_value("rgb"))

            ("R,renderprofile", "Rendering profile: path to a gray, RGB or CMYK ICC profile",
                cxxopts::value<std::string>()->default_value(""))

            ("f,finalspace", "Final color space: gray, rgb or cmyk. Default: same as render space.",
                cxxopts::value<std::string>()->default_value(""))

            ("F,finalprofile", "Final profile: path to a gray, RGB or CMYK ICC profile. Devicelink profiles are permitted, providing its color space matches that of the render space.",
                cxxopts::value<std::string>()->default_value(""))

            ("O,ocgevent", "Optional Content Event: view, print or export.",
                cxxopts::value<std::string>()->default_value("print"))

            ("g,gputhreads", "Threads per GPU. The number of tile threads to use per GPU.",
                cxxopts::value<int>()->default_value("1"))

            ("G,useallgpus", "If true, multiple renderers will be used for different pages if multiple GPUS are present.",
                cxxopts::value<bool>()->implicit_value("true")->default_value("false"))

            ("x,exclude", "Exclude integrated GPUs. If true, integrated GPUs will NOT be included when useallgpus option is true.",
                cxxopts::value<bool>()->implicit_value("true")->default_value("true"))

            ("U,dump", "Debug: Dump render spec.",
                cxxopts::value<bool>()->implicit_value("true")->default_value("false"));
}

void RenderingOptions::parse(const cxxopts::ParseResult& parse_result)
{
    // Resolution
    auto resolutions = parse_result["resolution"].as<std::vector<int >>();
    if (resolutions.empty() || resolutions.size() > 2 ||
        std::any_of(resolutions.begin(), resolutions.end(), [](int dpi) { return dpi <= 0; }))
        throw std::invalid_argument("Resolution requires one or two positive DPI values.");
    xResolution = yResolution = resolutions[0];
    yResolution = resolutions.size() > 1 ? resolutions[1] : xResolution;

    // Bit depth (bits per component)
    depth = static_cast<uint8_t>(parse_result["bpc"].as<int>());
    depth = depth <= 8 ? 8 : 16;

    const int aa = parse_result["aafactor"].as<int>();
    if (aa < 1 || aa > 4) throw std::invalid_argument("Anti-alias factor must be 1-4.");
    aaFactor = static_cast<uint8>(aa);

    overprintPreview = parse_result["overprintpreview"].as<bool>();
    m_bigTIFF= parse_result["bigtiff"].as<bool>();
    use16BitInternalRendering = parse_result["sixteen"].as<bool>();

    // Process space - default is set to RGB
    parseProcessColorSpace(processSpace, parse_result["renderspace"].as<std::string>().c_str());
    auto profilePath = parse_result["renderprofile"].as<std::string>();
    if (fs::exists(profilePath))
    {
        IDOMICCProfilePtr profile = IDOMICCProfile::create(m_jawsMako, IInputStream::createFromFile(m_jawsMako, profilePath.c_str()));
        processSpace = IDOMColorSpaceICCBased::create(m_jawsMako, profile);
    }

    // Final space (-F)
    bool result = parseProcessColorSpace(finalSpace, parse_result["finalspace"].as<std::string>().c_str());
    profilePath = parse_result["finalprofile"].as<std::string>();
    if (fs::exists(profilePath))
    {
        IDOMICCProfilePtr profile = IDOMICCProfile::create(m_jawsMako, IInputStream::createFromFile(m_jawsMako, profilePath.c_str()));
        finalSpace = IDOMColorSpaceICCBased::create(m_jawsMako, profile);
        result = true;
    }

    // If no final space was specified, use process space
    if (!result)
        finalSpace = processSpace;

    // OCG event
    std::string ocgEvent = parse_result["ocgevent"].as<std::string>();
    std::ranges::transform(ocgEvent, ocgEvent.begin(), tolower);
    if (ocgEvent == "view")
        optionalContentEvent = eOCEView;
    else if (ocgEvent == "export")
        optionalContentEvent = eOCEExport;
    else
        optionalContentEvent = eOCEPrint;

    // Multi-GPU and front-end threading
    threadsPerGpu = static_cast<uint32_t>(parse_result["gputhreads"].as<int>());
    useAllGpus = parse_result["useallgpus"].as<bool>();
    excludeIntegratedGpus = parse_result["exclude"].as<bool>();

    dumpRenderSpec = parse_result["dump"].as<bool>();
}

