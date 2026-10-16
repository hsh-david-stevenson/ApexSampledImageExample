#version 450

// Apex custom colour texture post-process: opaque RGB input/output.
layout(binding = 0) uniform sampler2D colourInput;
layout(location = 0) out vec4 resultColour;

layout(push_constant) uniform OilPaintDetails
{
    // User buffer: eighteen consecutive 32-bit values (72 bytes).
    uint width;
    uint height;
    uint radius;
    uint bins;
    float strength;
    float centreWeight;

    float bristleSpacing;   // Pixels; minimum 2 to limit aliasing.
    float strokeLength;     // Pixels; controls longitudinal variation.
    float strokeAngle;      // Degrees in image coordinates.
    float curvature;        // Lateral bend in pixels.
    float reliefDepth;      // Height in pixel units; zero disables relief.
    float lightingStrength; // 0..1; zero disables relief lighting.
    float lightAngle;       // Degrees in image coordinates.
    float lightElevation;   // Degrees above the image plane, 5..85.
    float shine;            // 0..1; strength of bristle highlights.

    float featureFollow;    // 0 = previous relief; 1 = image-guided relief.
    uint directionRadius;   // Structure-tensor smoothing radius, 1..8 pixels.
    float featureContrast;  // Gradient scale below which manual direction wins.

    // Apex appends these; exclude them from the user buffer.
    int tileX;
    int tileY;
} settings;

const int MAX_BINS = 32;

vec3 readRGB(ivec2 localPosition)
{
    ivec2 origin = ivec2(settings.tileX, settings.tileY);
    ivec2 imageSize = max(ivec2(settings.width, settings.height), ivec2(1));
    ivec2 pagePosition = clamp(localPosition + origin,
                              ivec2(0), imageSize - ivec2(1));
    return texelFetch(colourInput, pagePosition - origin, 0).rgb;
}

int colourBin(vec3 colour, int binCount)
{
    float luminance = dot(colour, vec3(0.2126, 0.7152, 0.0722));
    return clamp(int(floor(luminance * float(binCount))), 0, binCount - 1);
}

// Procedural paint height, evaluated in full-image coordinates. No tile-local
// randomness or derivatives: identical page positions give identical heights.
// This approximates curved bristle relief, not Photoshop's proprietary filter.
float manualPaintHeight(vec2 pagePosition)
{
    const float TAU = 6.28318530718;
    float angle = radians(settings.strokeAngle);
    vec2 along = vec2(cos(angle), sin(angle));
    vec2 across = vec2(-along.y, along.x);
    float u = dot(pagePosition, along);
    float v = dot(pagePosition, across);
    float lengthScale = max(settings.strokeLength, 8.0);
    float spacing = max(settings.bristleSpacing, 2.0);

    // Smooth bends, finer bristles, and variation along each stroke.
    float bend = settings.curvature * (
        0.70 * sin(TAU * u / lengthScale)
        + 0.30 * sin(TAU * (0.43 * u + 0.17 * v) / lengthScale));
    float phase = TAU * (v + bend) / spacing;
    float ridge = 0.5 + 0.5 * cos(phase);
    float envelope = 0.65 + 0.35 * sin(
        TAU * (u / lengthScale + v / (7.0 * spacing)));
    return max(settings.reliefDepth, 0.0) * ridge * ridge * envelope;
}

// Smoothed RGB structure tensor: follows chromatic edges as well as luminance
// edges. Its minor eigenvector is the tangent to the local image structure.
vec2 featureDirection(ivec2 position)
{
    int r = int(clamp(settings.directionRadius, 1u, 8u));
    vec3 tensor = vec3(0.0); // E, F, G
    float weightSum = 0.0;
    float sigmaSquared = max(float(r * r) * 0.5, 0.5);
    for (int y = -r; y <= r; ++y)
    {
        for (int x = -r; x <= r; ++x)
        {
            ivec2 p = position + ivec2(x, y);
            vec3 gx = 0.5 * (readRGB(p + ivec2(1, 0))
                             - readRGB(p - ivec2(1, 0)));
            vec3 gy = 0.5 * (readRGB(p + ivec2(0, 1))
                             - readRGB(p - ivec2(0, 1)));
            float w = exp(-float(x * x + y * y) / (2.0 * sigmaSquared));
            tensor += w * vec3(dot(gx, gx), dot(gx, gy), dot(gy, gy));
            weightSum += w;
        }
    }
    tensor /= weightSum;
    vec2 difference = vec2(tensor.x - tensor.z, 2.0 * tensor.y);
    float separation = length(difference);
    float energy = tensor.x + tensor.z;
    float coherence = separation / max(energy, 1e-10);
    float contrast = max(settings.featureContrast, 0.0001);
    float confidence = coherence * energy / (energy + contrast * contrast);

    // Blend unoriented axes in doubled-angle space, avoiding +180/-180 jumps.
    float manualAngle = radians(settings.strokeAngle);
    vec2 fallback = vec2(cos(2.0 * manualAngle), sin(2.0 * manualAngle));
    vec2 tangentAxis = -difference / max(separation, 1e-10);
    vec2 axis = mix(fallback, tangentAxis, confidence);
    if (dot(axis, axis) < 1e-8)
        axis = fallback;
    float angle = 0.5 * atan(axis.y, axis.x);
    return vec2(cos(angle), sin(angle));
}

// Integer hashing keeps procedural seeds stable at full-page coordinates.
float noiseHash(ivec2 p)
{
    uint h = uint(p.x) * 0x8da6b343u ^ uint(p.y) * 0xd8163841u;
    h ^= h >> 16;
    h *= 0x7feb352du;
    h ^= h >> 15;
    h *= 0x846ca68bu;
    h ^= h >> 16;
    return float(h & 0x00ffffffu) / 16777215.0;
}

float valueNoise(vec2 p)
{
    ivec2 cell = ivec2(floor(p));
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(noiseHash(cell), noiseHash(cell + ivec2(1, 0)), f.x),
               mix(noiseHash(cell + ivec2(0, 1)),
                   noiseHash(cell + ivec2(1, 1)), f.x), f.y);
}

float paintHeight(ivec2 localPosition)
{
    vec2 pagePosition = vec2(localPosition)
                        + vec2(settings.tileX, settings.tileY) + vec2(0.5);
    float follow = clamp(settings.featureFollow, 0.0, 1.0);
    float manual = manualPaintHeight(pagePosition);
    if (follow == 0.0)
        return manual;

    vec2 tangent = featureDirection(localPosition);
    float spacing = max(settings.bristleSpacing, 2.0);
    
    // 33 symmetric taps elongate page-anchored noise along the local tangent.
    // Cap the length to keep tap spacing <= half a bristle spacing. This local
    // convolution avoids rotating a sinusoid's phase at each output pixel.
    float strokeLength = clamp(settings.strokeLength, 8.0, 16.0 * spacing);
    float sum = 0.0;
    float weightSum = 0.0;
    for (int i = -16; i <= 16; ++i)
    {
        float t = float(i) / 16.0;
        float w = exp(-2.0 * t * t);
        vec2 samplePosition = pagePosition + tangent * (0.5 * strokeLength * t);
        sum += w * valueNoise(samplePosition / (0.5 * spacing));
        weightSum += w;
    }
    // Restore some contrast lost to averaging, forming irregular paint ridges.
    float gain = clamp(sqrt(strokeLength / spacing), 1.0, 4.0);
    float ridge = clamp(0.5 + (sum / weightSum - 0.5) * gain, 0.0, 1.0);
    float guided = max(settings.reliefDepth, 0.0) * ridge * ridge;
    return mix(manual, guided, follow);
}

vec3 addRelief(vec3 colour, ivec2 localPosition)
{
    float amount = clamp(settings.lightingStrength, 0.0, 1.0);
    if (amount == 0.0 || settings.reliefDepth <= 0.0)
        return colour;

    float dx = 0.5 * (paintHeight(localPosition + ivec2(1, 0))
                     - paintHeight(localPosition - ivec2(1, 0)));
    float dy = 0.5 * (paintHeight(localPosition + ivec2(0, 1))
                     - paintHeight(localPosition - ivec2(0, 1)));
    vec3 normal = normalize(vec3(-dx, -dy, 1.0));
    float azimuth = radians(settings.lightAngle);
    float elevation = radians(clamp(settings.lightElevation, 5.0, 85.0));
    vec3 light = vec3(cos(elevation) * cos(azimuth),
                      cos(elevation) * sin(azimuth), sin(elevation));

    // Normalize against flat paint, so zero depth preserves the base colour.
    float diffuse = max(dot(normal, light), 0.0) / light.z;
    float shading = mix(1.0, clamp(diffuse, 0.25, 2.0), amount);
    vec3 halfVector = normalize(light + vec3(0, 0, 1));
    float specular = pow(max(dot(normal, halfVector), 0.0), 32.0);
    float flatSpecular = pow(halfVector.z, 32.0);
    float highlight = amount * clamp(settings.shine, 0.0, 1.0)
                      * max(specular - flatSpecular, 0.0);
    return clamp(colour * shading + vec3(highlight), 0.0, 1.0);
}

void main()
{
    ivec2 position = ivec2(floor(gl_FragCoord.xy));
    vec3 original = readRGB(position);
    int radius = int(min(settings.radius, 64u));
    int binCount = int(clamp(settings.bins, 2u, uint(MAX_BINS)));
    float strength = clamp(settings.strength, 0.0, 1.0);

    if (radius == 0 || strength == 0.0)
    {
        resultColour = vec4(mix(original, addRelief(original, position),
                               strength), 1.0);
        return;
    }

    float weights[MAX_BINS];
    vec3 colours[MAX_BINS];
    for (int i = 0; i < MAX_BINS; ++i)
    {
        weights[i] = 0.0;
        colours[i] = vec3(0.0);
    }

    int radiusSquared = radius * radius;
    float falloff = max(settings.centreWeight, 0.0);
    for (int y = -radius; y <= radius; ++y)
    {
        for (int x = -radius; x <= radius; ++x)
        {
            int distanceSquared = x * x + y * y;
            if (distanceSquared > radiusSquared)
                continue;

            vec3 colour = readRGB(position + ivec2(x, y));
            int bin = colourBin(colour, binCount);

            // Zero falloff gives an ordinary, unweighted histogram.
            float weight = exp(-falloff * float(distanceSquared)
                               / float(radiusSquared));
            weights[bin] += weight;
            colours[bin] += weight * colour;
        }
    }

    // Start with the centre pixel's bin, preferring it in exact ties.
    int winner = colourBin(original, binCount);
    for (int i = 0; i < binCount; ++i)
    {
        if (weights[i] > weights[winner])
            winner = i;
    }

    vec3 painted = colours[winner] / weights[winner];
    painted = addRelief(painted, position);
    resultColour = vec4(mix(original, painted, strength), 1.0);
}
