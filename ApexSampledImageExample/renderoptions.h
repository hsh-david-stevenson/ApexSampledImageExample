/* -----------------------------------------------------------------------
 * <copyright file="renderoptions.h" company="Global Graphics Software Ltd">
 *  Copyright (c) 2026 Global Graphics Software Ltd. All rights reserved.
 * </copyright>
 * <summary>
 *  This example is provided on an "as is" basis and without warranty of any kind.
 *  Global Graphics Software Ltd. does not warrant or make any representations
 *  regarding the use or results of use of this example.
 * </summary>
 * -----------------------------------------------------------------------
 */

#pragma once

#include <algorithm>
#include "generaloptions.h"
#include <jawsmako/apexrenderer.h>
#include <jawsmako/renderspec.h>

using namespace EDL;
using namespace JawsMako;

struct RenderingOptions
{
    IJawsMakoPtr               m_jawsMako;
    
    bool                       m_bigTIFF; // If using tiff, whether to use bigTIFF
                               
    uint32                     xResolution;
    uint32                     yResolution;
    uint8                      depth;

    IDOMColorSpacePtr          processSpace;
    IDOMColorSpacePtr          finalSpace;
    
    bool                       overprintPreview;
    uint8                      aaFactor;
    bool                       use16BitInternalRendering;

    IApexRenderer::IFragmentShaderPtr fragmentShader;
    FRect                      cropBox = FRect();

    eOptionalContentEvent      optionalContentEvent;
    
    //
    // Multi-GPU and front-end threading. All are ignored if producing a PDF.
    //
    uint32                     threadsPerGpu; // How many threads to use with each GPU. Can be used even if not using all GPUs
                               
    bool                       useAllGpus;
    bool                       excludeIntegratedGpus; // If true, integrated GPUs are excluded from the above
                               
    bool                       dumpRenderSpec; // If true, then we print out the settings in the renderSpec

    explicit RenderingOptions(const IJawsMakoPtr& jawsMako) : m_jawsMako(jawsMako)
    {

        // Defaults
        xResolution = 150;
        yResolution = 150;
        depth = 8;
        m_bigTIFF = false;
        processSpace = IDOMColorSpaceDeviceRGB::create(m_jawsMako);
        finalSpace = processSpace;
        overprintPreview = false;
        aaFactor = 1;
        use16BitInternalRendering = false;
        optionalContentEvent = eOCEPrint;
        useAllGpus = false;
        excludeIntegratedGpus = true;
        threadsPerGpu = 1;
        dumpRenderSpec = false;
    }

    void parse(const cxxopts::ParseResult& parse_result);

private:
    // Parse a color space from a string
    bool parseProcessColorSpace(IDOMColorSpacePtr &fieldVar, U8String value) const
    {
        std::ranges::transform(value, value.begin(), tolower);
        if (value == "gray")
            fieldVar = IDOMColorSpaceDeviceGray::create(m_jawsMako);
        else if (value == "rgb")
            fieldVar = IDOMColorSpaceDeviceRGB::create(m_jawsMako);
        else if (value == "cmyk")
            fieldVar = IDOMColorSpaceDeviceCMYK::create(m_jawsMako);
        else
            return false;

        return true;
    }
};

void registerRenderingOptions(cxxopts::Options& options);
