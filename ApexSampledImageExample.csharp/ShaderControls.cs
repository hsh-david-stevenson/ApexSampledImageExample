using System.Runtime.InteropServices;
using JawsMako;

namespace ApexSampledImageExample;

// Only user constants go in these structures: Apex appends tileX and tileY.
[StructLayout(LayoutKind.Sequential, Pack = 4)]
internal struct OilPaintControls
{
    public uint Width, Height, Radius, Bins;
    public float Strength, CentreWeight, BristleSpacing, StrokeLength, StrokeAngle,
        Curvature, ReliefDepth, LightingStrength, LightAngle, LightElevation,
        Shine, FeatureFollow;
    public uint DirectionRadius;
    public float FeatureContrast;

    public readonly uint RequiredExtend()
    {
        uint extent = Math.Min(Radius, 64u);
        if (FeatureFollow > 0 && ReliefDepth > 0 && LightingStrength > 0 && Strength > 0)
            extent = Math.Max(extent, Math.Clamp(DirectionRadius, 1u, 8u) + 2);
        return extent;
    }
}

[StructLayout(LayoutKind.Sequential, Pack = 4)]
internal struct PencilSketchControls
{
    public uint Width, Height, BlurRadius;
    public float BlurSigma, LineStrength, ToneStrength, HatchStrength, HatchSpacing,
        HatchAngle, PaperStrength;

    public readonly uint RequiredExtend() => Math.Clamp(BlurRadius, 1u, 32u);
}

internal static class ShaderConstants
{
    public static CEDLSimpleBuffer Create<T>(T value) where T : unmanaged
    {
        byte[] data = MemoryMarshal.AsBytes(MemoryMarshal.CreateReadOnlySpan(ref value, 1)).ToArray();
        return new CEDLSimpleBuffer(data, checked((uint)data.Length));
    }

    public static void CheckLayouts()
    {
        if (Marshal.SizeOf<OilPaintControls>() != 72 ||
            Marshal.OffsetOf<OilPaintControls>(nameof(OilPaintControls.FeatureContrast)).ToInt32() != 68 ||
            Marshal.SizeOf<PencilSketchControls>() != 40 ||
            Marshal.OffsetOf<PencilSketchControls>(nameof(PencilSketchControls.PaperStrength)).ToInt32() != 36)
            throw new InvalidOperationException("Shader push-constant layout mismatch.");
    }
}
