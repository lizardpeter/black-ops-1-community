#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace bo1
{
enum class IwiFormat { bgra8, bc1, bc2, bc3 };

struct IwiImage
{
    std::uint32_t width;
    std::uint32_t height;
    IwiFormat format;
    std::vector<std::uint8_t> topMip;
};

// Reads a PC Black Ops IWI v13 2D image. The top mip retains its block compression.
IwiImage DecodeIwi(std::string_view bytes);
}
