#version 450

// Apex texture post-process: opaque RGB input and grayscale RGB output.
layout(binding = 0) uniform sampler2D colourInput;
layout(location = 0) out vec4 resultColour;

layout(push_constant) uniform PencilDetails
{
    // 40-byte user buffer; Apex appends the two tile offsets.
    uint width;
    uint height;
    uint blurRadius;       // 1..32; match _extend to this clamped value.
    float blurSigma;       // Gaussian width in pixels.
    float lineStrength;    // 0..4; zero removes contour lines.
    float toneStrength;    // 0..1; amount of continuous gray shading.
    float hatchStrength;   // 0..1; amount of cross-hatching.
    float hatchSpacing;    // Pixels, minimum 3.
    float hatchAngle;      // Degrees in image coordinates.
    float paperStrength;   // 0..0.2; paper grain contrast.
    int tileX;
    int tileY;
} settings;

float grayAt(ivec2 localPosition)
{
    ivec2 origin = ivec2(settings.tileX, settings.tileY);
    ivec2 size = max(ivec2(settings.width, settings.height), ivec2(1));
    ivec2 pagePosition = clamp(localPosition + origin, ivec2(0), size - 1);
    vec3 colour = texelFetch(colourInput, pagePosition - origin, 0).rgb;
    return dot(colour, vec3(0.2126, 0.7152, 0.0722));
}

float paperNoise(ivec2 pagePosition)
{
    uint h = uint(pagePosition.x) * 0x8da6b343u
             ^ uint(pagePosition.y) * 0xd8163841u;
    h ^= h >> 16;
    h *= 0x7feb352du;
    h ^= h >> 15;
    h *= 0x846ca68bu;
    h ^= h >> 16;
    return float(h & 0x00ffffffu) / 16777215.0;
}

float hatchLine(vec2 p, float angle, float spacing)
{
    vec2 normal = vec2(-sin(angle), cos(angle));
    float phase = dot(p, normal) / spacing;
    float distanceToLine = abs(fract(phase + 0.5) - 0.5) * spacing;
    // Approximately one-pixel pencil lines with a soft antialiasing fringe.
    return 1.0 - smoothstep(0.25, 1.0, distanceToLine);
}

void main()
{
    ivec2 p = ivec2(floor(gl_FragCoord.xy));
    float gray = grayAt(p);
    int radius = int(clamp(settings.blurRadius, 1u, 32u));
    float sigma = max(settings.blurSigma, 0.5);
    float sum = 0.0;
    float weightSum = 0.0;
    for (int y = -radius; y <= radius; ++y)
    {
        for (int x = -radius; x <= radius; ++x)
        {
            if (x * x + y * y > radius * radius)
                continue;
            float weight = exp(-float(x * x + y * y) / (2.0 * sigma * sigma));
            sum += weight * grayAt(p + ivec2(x, y));
            weightSum += weight;
        }
    }
    float blurred = sum / weightSum;

    // Grayscale color-dodge sketch: dark local features become graphite marks.
    // A small regularizer keeps uniform black regions white in outline mode.
    float dodge = clamp((gray + 0.001) / (blurred + 0.001), 0.0, 1.0);
    float lineStrength = clamp(settings.lineStrength, 0.0, 4.0);
    float lines = 1.0;
    if (lineStrength > 0.0)
        lines = pow(dodge, lineStrength);

    // Symmetric central differences add crisp outlines on both sides of edges.
    float gx = 0.5 * (grayAt(p + ivec2(1, 0)) - grayAt(p - ivec2(1, 0)));
    float gy = 0.5 * (grayAt(p + ivec2(0, 1)) - grayAt(p - ivec2(0, 1)));
    lines *= 1.0 - clamp(lineStrength * length(vec2(gx, gy)), 0.0, 1.0);

    // Full-page coordinates keep hatching and paper grain continuous over tiles.
    ivec2 pagePixel = p + ivec2(settings.tileX, settings.tileY);
    vec2 pagePosition = vec2(pagePixel) + vec2(0.5);
    float angle = radians(settings.hatchAngle);
    float spacing = max(settings.hatchSpacing, 3.0);
    float darkness = 1.0 - blurred;
    float first = hatchLine(pagePosition, angle, spacing)
                  * smoothstep(0.15, 0.65, darkness);
    float second = hatchLine(pagePosition, angle + 1.57079632679, spacing)
                   * smoothstep(0.50, 0.90, darkness);
    float hatching = 1.0 - clamp(settings.hatchStrength, 0.0, 1.0)
                           * (1.0 - (1.0 - first) * (1.0 - second));
    float tone = 1.0 - clamp(settings.toneStrength, 0.0, 1.0) * (1.0 - gray);
    float paper = 1.0 - clamp(settings.paperStrength, 0.0, 0.2)
                       * paperNoise(pagePixel);
    float sketch = clamp(lines * tone * hatching * paper, 0.0, 1.0);
    resultColour = vec4(vec3(sketch), 1.0);
}
