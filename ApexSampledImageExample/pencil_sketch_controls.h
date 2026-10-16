#pragma once
#include "cxxopts.hpp"
#include <cstdint>
#include <cstddef>

// Supply exactly 40 user bytes. Apex appends tileX/tileY at offsets 40/44.
struct PencilSketchControls
{
    uint32_t width = 1;
    uint32_t height = 1;
    uint32_t blurRadius = 8;
    float blurSigma = 4.0f;
    float lineStrength = 1.4f;
    float toneStrength = 0.15f;
    float hatchStrength = 0.35f;
    float hatchSpacing = 6.0f;
    float hatchAngle = -35.0f;
    float paperStrength = 0.025f;
};

static_assert(sizeof(PencilSketchControls) == 40, "Shader layout mismatch");
static_assert(offsetof(PencilSketchControls, paperStrength) == 36,
              "Shader layout mismatch");

inline uint32_t pencilSketchRequiredExtend(const PencilSketchControls& controls)
{
    return controls.blurRadius < 1 ? 1
           : (controls.blurRadius > 32 ? 32 : controls.blurRadius);
}

void registerPencilSketchOptions(cxxopts::Options& options);
PencilSketchControls parsePencilSketchOptions(const cxxopts::ParseResult& result);
