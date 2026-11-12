#pragma once

#include <array>
#include <optional>
#include <string_view>

namespace bo1
{
struct RawFileView
{
    std::optional<std::string_view> name;
    std::optional<std::string_view> payload;
};

using FastfileBlocks = std::array<std::string_view, 7>;

// The caller owns the blocks and stream bytes referenced by the returned view.
std::optional<RawFileView> DecodeRawFile(std::string_view record, const FastfileBlocks &blocks,
                                         std::string_view &inlineStream);
}
