# Apex sampled-image example — C#

This .NET 10 x64 console application ports the solution's C++ sample using
Mako 9.1 and System.CommandLine 2.0.12. It shares the existing SPIR-V shaders
in `../Shaders`; builds and publishes copy them into the application's
`Shaders` directory. Shader lookup is relative to the executable, independent
of the current working directory.

Build from this directory:

```powershell
dotnet build
dotnet run -- --help
```

Examples (paths are relative to this project directory):

```powershell
dotnet run -- ../TestFiles/BeerLabels.pdf "renders/oil_%u.png" --art oil
dotnet run -- ../TestFiles/BeerLabels.pdf "renders/pencil_%u.png" --art pencil --ps-hatch-angle -45
dotnet run -- --infile ../TestFiles/BeerLabels.pdf --outfile "renders/page_%03u.tif" -p "1-3,5,10-" -d 300,600
dotnet run -- ../TestFiles/BeerLabels.pdf "renders/gray_%u.tif" -f gray -b 16 -s -B
```

## Command-line compatibility

All current C++ long option names and short aliases are present, including
the 16 `--op-*` controls and eight `--ps-*` controls. Defaults come from the
current C++ source: oil radius **4**, direction smoothing radius **4**,
resolution **150 DPI**, effect **none**. Both positional filenames and
`--infile` / `--outfile` are supported. The default output is
`<input-stem>_%u.tif` in the working directory.

Page numbers are one-based across all documents in the input assembly.
Open and reversed ranges are accepted; overlapping ranges do not render a
page twice. Output templates support `%u`, padding such as `%03u`, and `%%`
for a literal percent sign. As in C++, a literal filename is overwritten by
each selected page; those writes are serialized.

## Rendering

- Inputs: PDF, XPS, SVG, PS, EPS, PCL5 (`.pcl`) and PCLXL (`.pxl`).
- Outputs: PNG, JPEG, TIFF and one rasterized PDF per selected page.
- Artistic shaders use RGB input, including a supplied three-component ICC
  rendering profile. Gray or CMYK process spaces are replaced by RGB when
  an artistic effect is selected. Explicit final-space/profile conversion
  happens after the effect. Without an explicit final space, output stays
  in the effective process space.
- Framebuffer stride uses the actual final component count and output bit
  depth. Non-square DPI is preserved in image metadata. JPEG requires 8-bit
  output; the chosen image encoder must support the requested colour space.
- `--aafactor`, `--bpc`, `--sixteen`, `--bigtiff`, `--ocgevent` and
  `--overprintpreview` are applied, rather than merely parsed as in the
  current C++ main loop. Overprint preview adds the spot-merge post-process.
- `--gputhreads` creates that many independent renderer workers per GPU.
  `--useallgpus` distributes pages across available devices, respecting
  `--exclude` (true by default; use `--exclude=false` to allow integrated
  GPUs). Input enumeration is serialized and rendering is bounded by the
  number of workers. Each renderer owns its own shader.
- `--dump` prints the effective dimensions, DPI, depth, colour components,
  effect, tile extension and related settings.

Invalid page ranges, non-positive DPI, invalid anti-alias factors, non-finite
shader parameters and missing ICC profiles report errors. Out-of-range shader
controls otherwise retain the GLSL clamping behaviour of the C++ sample.

## Push constants and lifetime

`ShaderControls.cs` declares four-byte-packed unmanaged structures and checks
their sizes/last-field offsets before rendering. Oil supplies 72 user bytes
and pencil supplies 40. Apex appends its tile offsets. The overlap helpers
match the C++ versions, including extra pixels for feature direction and
relief normal calculations.

Native objects are disposed deterministically: per-page post-process resources
are released before shaders, shaders before their Apex renderer, and renderers
before the Mako instance. Page resources are released even if rendering or
encoding fails.

## Verification

The port was built without warnings and exercised with all three effects,
PNG/JPEG/TIFF/PDF output, non-square DPI, 16-bit grayscale BigTIFF, colour
conversion, page selection and concurrent rendering. With identical settings,
the BeerLabels PNG pixels matched the existing C++ executable for none, pencil
and oil. The existing C++ executable still advertised direction radius 2;
the current C++ header and this port default to 4, so the comparison explicitly
used 2 in C#.
