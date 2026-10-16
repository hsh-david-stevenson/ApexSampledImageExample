#include "renderoptions.h"
#include "oil_paint_controls.h"
#include "pencil_sketch_controls.h"
#include <iostream>
#include <stdexcept>

void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

int main()
{
    try
    {
        cxxopts::Options options("tests");
        registerGeneralOptions(options);
        registerRenderingOptions(options);
        registerOilPaintOptions(options);
        registerPencilSketchOptions(options);
        const char* defaults[] = {"tests", "Main.cpp"};
        auto result = options.parse(2, defaults);
        const auto general = parseGeneralOptions(result);
        const auto oil = parseOilPaintOptions(result);
        const auto pencil = parsePencilSketchOptions(result);
        require(general.m_outputFilePath == "Main_%u.tif", "default output path");
        require(GeneralOptions::resolveRanges(general.m_pageRanges, 3) == std::vector<uint32_t>({1,2,3}), "open range default");
        require(oil.radius == OilPaintControls{}.radius && oil.featureContrast == OilPaintControls{}.featureContrast, "oil defaults");
        require(pencil.blurRadius == PencilSketchControls{}.blurRadius && pencil.paperStrength == PencilSketchControls{}.paperStrength, "pencil defaults");
        auto mako = IJawsMako::create();
        RenderingOptions rendering(mako);
        rendering.parse(result);
        require(rendering.m_xResolution == 150 && rendering.m_yResolution == 150 && rendering.m_depth == 8, "rendering defaults");
        require(rendering.m_aaFactor == 1 && rendering.m_gamma == 1.0f && rendering.m_maxDrops == 1, "rendering defaults extraction");
        const char* supplied[] = {"tests", "--infile", "Main.cpp", "--outfile", "result.png", "--pages", "2,4-", "--resolution", "300,600", "--aafactor", "3", "--gamma", "2", "--drops", "4", "--oil-paint-radius", "20", "--oil-paint-shine", "0.75", "--pencil-sketch-blur-radius", "12", "--pencil-sketch-hatch-angle", "-45"};
        result = options.parse(sizeof(supplied)/sizeof(*supplied), supplied);
        const auto customGeneral = parseGeneralOptions(result);
        rendering.parse(result);
        const auto customOil = parseOilPaintOptions(result);
        const auto customPencil = parsePencilSketchOptions(result);
        require(customGeneral.m_outputFilePath == "result.png", "explicit output path");
        require(GeneralOptions::resolveRanges(customGeneral.m_pageRanges, 6) == std::vector<uint32_t>({2,4,5,6}), "selected pages");
        require(rendering.m_xResolution == 300 && rendering.m_yResolution == 600 && rendering.m_aaFactor == 3 && rendering.m_gamma == 2 && rendering.m_maxDrops == 4, "render overrides");
        require(customOil.radius == 20 && customOil.shine == 0.75f && customOil.strength == oil.strength, "oil overrides and remaining defaults");
        require(customPencil.blurRadius == 12 && customPencil.hatchAngle == -45 && customPencil.lineStrength == pencil.lineStrength, "pencil overrides and remaining defaults");
        for (const auto& invalid : {"0", "1x", "1,,3", "1-2-3", "1,"})
        {
            bool rejected = false;
            try { GeneralOptions::parsePageRangeSpecs(invalid); }
            catch (const std::exception&) { rejected = true; }
            require(rejected, "invalid page range accepted");
        }
        std::cout << "All command-line settings checks passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
