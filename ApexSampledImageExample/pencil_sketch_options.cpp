/* -----------------------------------------------------------------------
 * <copyright file="pencil_sketch_options.cpp" company="Global Graphics Software Ltd">
 *  Copyright (c) 2026 Global Graphics Software Ltd. All rights reserved.
 * </copyright>
 * <summary>
 *  This example is provided on an "as is" basis and without warranty of any kind.
 *  Global Graphics Software Ltd. does not warrant or make any representations
 *  regarding the use or results of use of this example.
 * </summary>
 * -----------------------------------------------------------------------
 */

#include "pencil_sketch_controls.h"

void registerPencilSketchOptions(cxxopts::Options& options)
{
    options.add_options("Pencil Sketch")
        ("ps-blur-radius", "Blur radius. Neighbourhood size, 1–32 pixels.", 
            cxxopts::value<uint32_t>()->default_value(std::to_string(PencilSketchControls{}.blurRadius)))
        
        ("ps-blur-sigma", "Blur sigma. Smoothing scale", 
            cxxopts::value<float>()->default_value(std::to_string(PencilSketchControls{}.blurSigma)))
        
        ("ps-line-strength", "Line strength. Contour darkness", 
            cxxopts::value<float>()->default_value(std::to_string(PencilSketchControls{}.lineStrength)))
        
        ("ps-tone-strength", "Tone strength. Continuous graphite shading", 
            cxxopts::value<float>()->default_value(std::to_string(PencilSketchControls{}.toneStrength)))
        
        ("ps-hatch-strength", "Hatch strength. Cross-hatching intensity", 
            cxxopts::value<float>()->default_value(std::to_string(PencilSketchControls{}.hatchStrength)))
        
        ("ps-hatch-spacing", "Hatch spacing. Distance between hatch lines", 
            cxxopts::value<float>()->default_value(std::to_string(PencilSketchControls{}.hatchSpacing)))
        
        ("ps-hatch-angle", "Hatch angle. Hatch direction", 
            cxxopts::value<float>()->default_value(std::to_string(PencilSketchControls{}.hatchAngle)))
        
        ("ps-paper-strength", "Paper strength. Paper grain", 
            cxxopts::value<float>()->default_value(std::to_string(PencilSketchControls{}.paperStrength)));
}

PencilSketchControls parsePencilSketchOptions(const cxxopts::ParseResult& result)
{
    PencilSketchControls controls;
    controls.blurRadius = result["ps-blur-radius"].as<uint32_t>();
    controls.blurSigma = result["ps-blur-sigma"].as<float>();
    if (!std::isfinite(controls.blurSigma)) 
        throw std::invalid_argument("ps-blur-sigma must be finite.");

    controls.lineStrength = result["ps-line-strength"].as<float>();
    if (!std::isfinite(controls.lineStrength)) 
        throw std::invalid_argument("ps-line-strength must be finite.");

    controls.toneStrength = result["ps-tone-strength"].as<float>();
    if (!std::isfinite(controls.toneStrength)) 
        throw std::invalid_argument("ps-tone-strength must be finite.");

    controls.hatchStrength = result["ps-hatch-strength"].as<float>();
    if (!std::isfinite(controls.hatchStrength)) 
        throw std::invalid_argument("ps-hatch-strength must be finite.");

    controls.hatchSpacing = result["ps-hatch-spacing"].as<float>();
    if (!std::isfinite(controls.hatchSpacing)) 
        throw std::invalid_argument("ps-hatch-spacing must be finite.");

    controls.hatchAngle = result["ps-hatch-angle"].as<float>();
    if (!std::isfinite(controls.hatchAngle)) 
        throw std::invalid_argument("ps-hatch-angle must be finite.");

    controls.paperStrength = result["ps-paper-strength"].as<float>();
    if (!std::isfinite(controls.paperStrength)) 
        throw std::invalid_argument("ps-paper-strength must be finite.");

    return controls;
}
