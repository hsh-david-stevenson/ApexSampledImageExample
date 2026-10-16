/* -----------------------------------------------------------------------
 * <copyright file="oil_paint_options.cpp" company="Global Graphics Software Ltd">
 *  Copyright (c) 2026 Global Graphics Software Ltd. All rights reserved.
 * </copyright>
 * <summary>
 *  This example is provided on an "as is" basis and without warranty of any kind.
 *  Global Graphics Software Ltd. does not warrant or make any representations
 *  regarding the use or results of use of this example.
 * </summary>
 * -----------------------------------------------------------------------
 */

#include "oil_paint_controls.h"

void registerOilPaintOptions(cxxopts::Options& options)
{
    options.add_options("Oil Paint")
        ("op-radius", "Radius (0-64). Larger values make larger daubs.",
            cxxopts::value<uint32_t>()->default_value(std::to_string(OilPaintControls{}.radius)))

        ("op-bins", "Bins (2-32). More bins result in more detailed textures.", 
            cxxopts::value<uint32_t>()->default_value(std::to_string(OilPaintControls{}.bins)))

        ("op-strength", "Strength. Blends between original and painted output.", 
            cxxopts::value<float>()->default_value(std::to_string(OilPaintControls{}.strength)))

        ("op-centre-weight", "Centre weight. Higher values favour nearby samples; try 0-3 to retain more detail.",
            cxxopts::value<float>()->default_value(std::to_string(OilPaintControls{}.centreWeight)))

        ("op-bristle-spacing", "Bristle spacing. Smaller values create more densely packed bristles.", 
            cxxopts::value<float>()->default_value(std::to_string(OilPaintControls{}.bristleSpacing)))

        ("op-stroke-length", "Stroke Length. Controls the length of each brush stroke.", 
            cxxopts::value<float>()->default_value(std::to_string(OilPaintControls{}.strokeLength)))

        ("op-stroke-angle", "Stroke Angle. Controls the orientation of each brush stroke.", 
            cxxopts::value<float>()->default_value(std::to_string(OilPaintControls{}.strokeAngle)))

        ("op-curvature", "Curvature. Amount of lateral bending, in pixels", 
            cxxopts::value<float>()->default_value(std::to_string(OilPaintControls{}.curvature)))

        ("op-relief-depth", "Relief depth. Raised-paint depth; zero disables relief.", 
            cxxopts::value<float>()->default_value(std::to_string(OilPaintControls{}.reliefDepth)))

        ("op-light-strength", "Lighting Strength. Controls the intensity of the lighting effect.", 
            cxxopts::value<float>()->default_value(std::to_string(OilPaintControls{}.lightingStrength)))

        ("op-light-angle", "Light Angle. Controls the direction of the lighting.", 
            cxxopts::value<float>()->default_value(std::to_string(OilPaintControls{}.lightAngle)))

        ("op-light-elevation", "Light Elevation. Controls the elevation of the lighting.", 
            cxxopts::value<float>()->default_value(std::to_string(OilPaintControls{}.lightElevation)))

        ("op-shine", "Shine. Controls the highlight strength (shininess) of the painted surface.", 
            cxxopts::value<float>()->default_value(std::to_string(OilPaintControls{}.shine)))

        ("op-feature-follow", "Feature follow. Controls how well the paint follows image features. Blend from previous relief (0) to guided relief (1).", 
            cxxopts::value<float>()->default_value(std::to_string(OilPaintControls{}.featureFollow)))

        ("op-direction", "Direction smoothing radius, 1-8 pixels", 
            cxxopts::value<uint32_t>()->default_value(std::to_string(OilPaintControls{}.directionRadius)))

        ("op-feature", "Feature contrast. Controls the contrast of image features. Lower values follow weaker image features.", 
            cxxopts::value<float>()->default_value(std::to_string(OilPaintControls{}.featureContrast)));
}

OilPaintControls parseOilPaintOptions(const cxxopts::ParseResult& result)
{
    OilPaintControls controls;
    controls.radius = result["op-radius"].as<uint32_t>();
    controls.bins = result["op-bins"].as<uint32_t>();
    controls.strength = result["op-strength"].as<float>();
    if (!std::isfinite(controls.strength))
        throw std::invalid_argument("Strength must be finite.");

    controls.centreWeight = result["op-centre-weight"].as<float>();
    if (!std::isfinite(controls.centreWeight))
        throw std::invalid_argument("Centre weight must be finite.");

    controls.bristleSpacing = result["op-bristle-spacing"].as<float>();
    if (!std::isfinite(controls.bristleSpacing)) 
        throw std::invalid_argument("Bristle spacing must be finite.");

    controls.strokeLength = result["op-stroke-length"].as<float>();
    if (!std::isfinite(controls.strokeLength)) 
        throw std::invalid_argument("Stroke length must be finite.");

    controls.strokeAngle = result["op-stroke-angle"].as<float>();
    if (!std::isfinite(controls.strokeAngle)) 
        throw std::invalid_argument("Stroke angle must be finite.");

    controls.curvature = result["op-curvature"].as<float>();
    if (!std::isfinite(controls.curvature)) 
        throw std::invalid_argument("Curvature must be finite.");

    controls.reliefDepth = result["op-relief-depth"].as<float>();
    if (!std::isfinite(controls.reliefDepth)) 
        throw std::invalid_argument("Relief depth must be finite.");

    controls.lightingStrength = result["op-light-strength"].as<float>();
    if (!std::isfinite(controls.lightingStrength)) 
        throw std::invalid_argument("Lighting strength must be finite.");

    controls.lightAngle = result["op-light-angle"].as<float>();
    if (!std::isfinite(controls.lightAngle)) 
        throw std::invalid_argument("Light angle must be finite.");

    controls.lightElevation = result["op-light-elevation"].as<float>();
    if (!std::isfinite(controls.lightElevation)) 
        throw std::invalid_argument("Elevation must be finite.");

    controls.shine = result["op-shine"].as<float>();
    if (!std::isfinite(controls.shine)) 
        throw std::invalid_argument("Shine must be finite.");

    controls.featureFollow = result["op-feature-follow"].as<float>();
    if (!std::isfinite(controls.featureFollow)) 
        throw std::invalid_argument("Feature follow must be finite.");

    controls.directionRadius = result["op-direction"].as<uint32_t>();
    controls.featureContrast = result["op-feature"].as<float>();
    if (!std::isfinite(controls.featureContrast)) 
        throw std::invalid_argument("Feature contrast must be finite.");

    return controls;
}

