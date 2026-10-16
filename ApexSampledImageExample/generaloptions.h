/* -----------------------------------------------------------------------
 * <copyright file="generaloptions.h" company="Global Graphics Software Ltd">
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
#include <optional>
#include <ostream>
#include <string>
#include <vector>
#include <cstdint>
#include "cxxopts.hpp"

struct GeneralOptions
{
    std::string inputFilePath;
    std::string outputFilePath;

    // Represents either a single page or a range
    struct PageRange
    {
        std::optional<int> start;  // if nullopt, range is open on the left
        std::optional<int> end;    // if nullopt, range is open on the right

        bool isSinglePage() const {
            return start && end && *start == *end;
        }

        friend std::ostream& operator<<(std::ostream& os, const PageRange& r) {
            if (r.isSinglePage()) return os << *r.start;
            os << (r.start ? std::to_string(*r.start) : "") << "-";
            if (r.end) os << *r.end;
            return os;
        }
    };

    static std::vector<uint32_t> resolveRanges(const std::vector<PageRange>& ranges, const uint32_t& maxPage);
    static std::vector<PageRange> parsePageRangeSpecs(const std::string& input);
    static std::string pageRangesToString(const std::vector<PageRange>& ranges);
    std::vector<PageRange> pageRanges = parsePageRangeSpecs("1-");
    std::string artisticEffect;
};

void registerGeneralOptions(cxxopts::Options& options);
GeneralOptions parseGeneralOptions(const cxxopts::ParseResult& result);
