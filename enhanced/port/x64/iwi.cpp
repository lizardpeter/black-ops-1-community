#include "iwi.h"

#include <algorithm>
#include <stdexcept>

namespace bo1
{
namespace
{
std::uint32_t ReadLE(std::string_view bytes, std::size_t offset, unsigned count)
{
    std::uint32_t value = 0;
    for (unsigned i = 0; i < count; ++i)
        value |= std::uint32_t(static_cast<unsigned char>(bytes[offset + i])) << (8 * i);
    return value;
}
}

IwiImage DecodeIwi(std::string_view bytes)
{
    constexpr std::size_t headerSize = 48;
    if (bytes.size() < headerSize || bytes.substr(0, 3) != "IWi" || bytes[3] != 13)
        throw std::runtime_error("Expected a PC BO1 IWI v13 header");
    if (ReadLE(bytes, 16, 4) != bytes.size())
        throw std::runtime_error("IWI size differs from the header");

    const unsigned flags = static_cast<unsigned char>(bytes[5]);
    const auto width = ReadLE(bytes, 6, 2);
    const auto height = ReadLE(bytes, 8, 2);
    const auto depth = ReadLE(bytes, 10, 2);
    if ((flags & 12) || depth > 1 || !width || !height || width > 32767 || height > 32767)
        throw std::runtime_error("Expected a positive-size 2D IWI");

    IwiFormat format;
    unsigned pixelBytes = 0;
    unsigned blockBytes = 0;
    switch (bytes[4])
    {
    case 1: format = IwiFormat::bgra8; pixelBytes = 4; break;
    case 2: format = IwiFormat::bgra8; pixelBytes = 3; break;
    case 11: format = IwiFormat::bc1; blockBytes = 8; break;
    case 12: format = IwiFormat::bc2; blockBytes = 16; break;
    case 13: format = IwiFormat::bc3; blockBytes = 16; break;
    default: throw std::runtime_error("Unsupported IWI format (expected BGRA, BGR, or BC1–BC3)");
    }

    const auto mipBytes = [&](std::uint32_t w, std::uint32_t h) -> std::uint64_t {
        if (blockBytes)
            return std::uint64_t((w + 3) / 4) * ((h + 3) / 4) * blockBytes;
        return std::uint64_t(w) * h * pixelBytes;
    };
    unsigned mipCount = 1;
    if (!(flags & 2))
    {
        for (unsigned resolution = 1; resolution < std::max(width, height); resolution *= 2)
            ++mipCount;
    }

    std::uint64_t offset = headerSize;
    for (unsigned mip = mipCount - 1; mip > 0; --mip)
        offset += mipBytes(std::max(1u, width >> mip), std::max(1u, height >> mip));
    const auto size = mipBytes(width, height);
    if (offset + size != bytes.size())
        throw std::runtime_error("IWI mip chain does not match its dimensions and format");

    const auto topMip = bytes.substr(static_cast<std::size_t>(offset), static_cast<std::size_t>(size));
    IwiImage image{width, height, format, {}};
    if (pixelBytes == 3)
    {
        image.topMip.reserve(topMip.size() / 3 * 4);
        for (std::size_t i = 0; i < topMip.size(); i += 3)
        {
            image.topMip.insert(image.topMip.end(), topMip.begin() + i, topMip.begin() + i + 3);
            image.topMip.push_back(255);
        }
    }
    else
    {
        image.topMip.assign(topMip.begin(), topMip.end());
    }
    return image;
}
}
