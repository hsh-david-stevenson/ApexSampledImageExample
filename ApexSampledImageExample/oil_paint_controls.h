/* -----------------------------------------------------------------------
 * <copyright file="oil_paint_controls.h" company="Global Graphics Software Ltd">
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
#include "cxxopts.hpp"
#include <cstdint>
#include <cstddef>

// Copy exactly sizeof(OilPaintControls) bytes into the user push-constant
// buffer. Apex appends tileX and tileY at offsets 72 and 76.
struct OilPaintControls
{
    uint32_t width = 1;
    uint32_t height = 1;
    uint32_t radius = 4;
    uint32_t bins = 16;
    float strength = 1.0f;
    float centreWeight = 0.0f;
    float bristleSpacing = 4.0f;
    float strokeLength = 80.0f;
    float strokeAngle = 25.0f;
    float curvature = 3.0f;
    float reliefDepth = 0.8f;
    float lightingStrength = 0.65f;
    float lightAngle = 135.0f;
    float lightElevation = 45.0f;
    float shine = 0.25f;
    float featureFollow = 1.0f;
    uint32_t directionRadius = 4;
    float featureContrast = 0.03f;
};

static_assert(sizeof(OilPaintControls) == 72, "Shader buffer must be 72 bytes");
static_assert(offsetof(OilPaintControls, bristleSpacing) == 24, "Layout mismatch");
static_assert(offsetof(OilPaintControls, shine) == 56, "Layout mismatch");
static_assert(offsetof(OilPaintControls, featureFollow) == 60, "Layout mismatch");
static_assert(offsetof(OilPaintControls, directionRadius) == 64, "Layout mismatch");
static_assert(offsetof(OilPaintControls, featureContrast) == 68, "Layout mismatch");

// Direction smoothing needs r pixels, central differences need one more,
// and relief-normal differences need one more. Match the shader's clamps.
inline uint32_t oilPaintRequiredExtend(const OilPaintControls& controls)
{
    uint32_t extent = controls.radius > 64 ? 64 : controls.radius;
    if (controls.featureFollow > 0.0f && controls.reliefDepth > 0.0f
        && controls.lightingStrength > 0.0f && controls.strength > 0.0f)
    {
        uint32_t r = controls.directionRadius;
        r = r < 1 ? 1 : (r > 8 ? 8 : r);
        extent = extent > r + 2 ? extent : r + 2;
    }
    return extent;
}

void registerOilPaintOptions(cxxopts::Options& options);
OilPaintControls parseOilPaintOptions(const cxxopts::ParseResult& result);
