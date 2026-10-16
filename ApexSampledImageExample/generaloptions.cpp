#include <algorithm>
#include <filesystem>
#include <set>
#include <sstream>
#include "generaloptions.h"

namespace fs = std::filesystem;

void registerGeneralOptions(cxxopts::Options& options)
{
    options.add_options("General")
            ("infile", "Path to input file",
                cxxopts::value<std::string>())

            ("outfile", "Path to output file. May include %u to be replaced by a page number. Defaults to a value based on input filename.",
                cxxopts::value<std::string>())

            ("p,pages", "Comma-delimited page ranges as a single number or from-to. Example: 1-10,30,100- means pages 1-10 inclusive, page 30, pages 100 to end.",
                cxxopts::value<std::string>()->default_value("1-"))

            ("art", "Artistic effect to apply (none, oil, pencil)",
                cxxopts::value<std::string>()->default_value("none"));

    options.parse_positional({"infile", "outfile"});
}

GeneralOptions parseGeneralOptions(const cxxopts::ParseResult& parse_result)
{
    GeneralOptions generalOptions;
    // Input file
    if (!parse_result.count("infile"))
        throw std::invalid_argument("An input file path is required.");

    std::string inputFile = parse_result["infile"].as<std::string>();
    if (!fs::exists(inputFile))
        throw std::invalid_argument(std::string("Input file not found."));
    
    generalOptions.inputFilePath = inputFile.c_str();

    // Output file
    std::string outputFile = fs::path(inputFile).stem().string() + "_%u.tif";
    if (parse_result.count("outfile"))
        outputFile = parse_result["outfile"].as<std::string>();
    

    // Ensure output directory exists
    if (fs::path(outputFile).has_parent_path())
    {
        fs::path outputDir = fs::path(outputFile).parent_path();
        if (!fs::exists(outputDir))
            fs::create_directories(outputDir);
    }

    generalOptions.outputFilePath = outputFile;

    // Page range
    generalOptions.pageRanges = GeneralOptions::parsePageRangeSpecs(parse_result["pages"].as<std::string>());

    // Artistic effect
    generalOptions.artisticEffect = parse_result["art"].as<std::string>();
    if (generalOptions.artisticEffect != "none" && generalOptions.artisticEffect != "oil" && generalOptions.artisticEffect != "pencil")
    {
        throw std::invalid_argument("Artistic effect must be none, oil or pencil.");
    }

    return generalOptions;
}

// Page range methods
std::vector<uint32_t> GeneralOptions::resolveRanges(const std::vector<PageRange>& ranges, const uint32_t& maxPage)
{
    std::set<uint32_t> result;  // sorted + deduplicated

    for (const auto& range : ranges)
    {
        if (maxPage == 0) return {};
        uint32_t start = range.start.value_or(1);            // open start defaults to page 1
        uint32_t end = range.end.value_or(maxPage);          // open end defaults to maxPage

        if (start > end) std::swap(start, end);         // tolerate reversed ranges

        // Clamp to legal range
        start = std::max(static_cast<uint32_t>(1), start);
        end = std::min(maxPage, end);

        for (uint64_t i = start; i <= end; ++i)
            result.insert(static_cast<uint32_t>(i));
    }

    return { result.begin(), result.end() };
}

// Parse page ranges expressed in a pagerange parameter
std::vector<GeneralOptions::PageRange> GeneralOptions::parsePageRangeSpecs(const std::string& input)
{
    std::vector<PageRange> ranges;
    if (input.empty() || input.back() == ',')
        throw std::invalid_argument("Page ranges must not be empty.");
    const auto pageNumber = [](const std::string& text) {
        size_t consumed = 0;
        const int page = std::stoi(text, &consumed);
        if (consumed != text.size() || page < 1)
            throw std::invalid_argument("Page numbers must be positive integers.");
        return page;
    };
    std::istringstream stream(input);
    std::string token;
    while (std::getline(stream, token, ','))
    {
        token.erase(0, token.find_first_not_of(" \t"));
        token.erase(token.find_last_not_of(" \t") + 1);
        if (token.empty()) throw std::invalid_argument("Page ranges must not be empty.");
        const auto dash = token.find('-');
        if (dash == std::string::npos)
        {
            const int page = pageNumber(token);
            ranges.push_back({page, page});
        }
        else
        {
            const auto start = token.substr(0, dash);
            const auto end = token.substr(dash + 1);
            ranges.push_back({start.empty() ? std::nullopt : std::make_optional(pageNumber(start)),
                              end.empty() ? std::nullopt : std::make_optional(pageNumber(end))});
        }
    }

    return ranges;
}

// Convert page range back to a string
std::string GeneralOptions::pageRangesToString(const std::vector<PageRange>& ranges)
{
    std::ostringstream out;
    bool first = true;

    for (const auto& range : ranges) {
        if (!first)
            out << ",";

        if (range.isSinglePage()) {
            out << *range.start;
        }
        else {
            // Print start (if present), dash, then end (if present)
            if (range.start)
                out << *range.start;
            out << "-";
            if (range.end)
                out << *range.end;
        }

        first = false;
    }

    return out.str();
}
