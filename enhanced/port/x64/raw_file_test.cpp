#include "raw_file.h"

#include <cstdint>

using namespace std::string_view_literals;

namespace
{
std::array<char, 12> Record(std::uint32_t name, std::uint32_t length, std::uint32_t buffer)
{
    std::array<char, 12> bytes{};
    const std::uint32_t fields[]{name, length, buffer};
    for (std::size_t field = 0; field < 3; ++field)
        for (unsigned index = 0; index < 4; ++index)
            bytes[field * 4 + index] = static_cast<char>(fields[field] >> (8 * index));
    return bytes;
}

std::optional<bo1::RawFileView> Decode(const std::array<char, 12> &record,
                                      const bo1::FastfileBlocks &blocks,
                                      std::string_view &inlineStream)
{
    return bo1::DecodeRawFile({record.data(), record.size()}, blocks, inlineStream);
}
}

int main()
{
    bo1::FastfileBlocks blocks{};
    blocks[4] = "Xmaps/test.cfg\0"sv;

    auto stream = "foo\0a\0b\0next"sv;
    auto raw = Decode(Record(UINT32_MAX, 3, 1), blocks, stream);
    if (!raw || raw->name != "foo"sv || raw->payload != "a\0b"sv || stream != "next"sv)
        return 1;

    stream = "\0next"sv;
    raw = Decode(Record(0x80000002, 0, UINT32_MAX), blocks, stream);
    if (!raw || raw->name != "maps/test.cfg"sv || raw->payload != ""sv || stream != "next"sv)
        return 1;

    raw = Decode(Record(0, 0, 0), blocks, stream);
    if (!raw || raw->name || raw->payload || stream != "next"sv)
        return 1;

    stream = "foo\0ab"sv;
    if (Decode(Record(UINT32_MAX, 2, 1), blocks, stream) || stream != "foo\0ab"sv ||
        Decode(Record(0x80000020, 0, 0), blocks, stream) ||
        Decode(Record(UINT32_MAX - 1, 0, 0), blocks, stream) ||
        Decode(Record(0, UINT32_MAX, 1), blocks, stream))
        return 1;
}
