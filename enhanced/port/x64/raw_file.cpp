#include "raw_file.h"
#include "fastfile_reference.h"

namespace bo1
{
namespace
{
std::uint32_t ReadLittleEndian32(std::string_view bytes)
{
    std::uint32_t value = 0;
    for (unsigned index = 0; index < 4; ++index)
        value |= std::uint32_t(static_cast<unsigned char>(bytes[index])) << (8 * index);
    return value;
}
}

std::optional<RawFileView> DecodeRawFile(std::string_view record, const FastfileBlocks &blocks,
                                         std::string_view &inlineStream)
{
    if (record.size() != 12)
        return std::nullopt;

    const auto name = DecodeFastfileReference(ReadLittleEndian32(record));
    const auto length = ReadLittleEndian32(record.substr(4));
    const auto buffer = ReadLittleEndian32(record.substr(8));
    if (length >= INT32_MAX)
        return std::nullopt;

    RawFileView result;
    auto remaining = inlineStream;
    if (name.kind == ReferenceKind::inlineData || name.kind == ReferenceKind::blockOffset)
    {
        auto nameBytes = remaining;
        if (name.kind == ReferenceKind::blockOffset)
        {
            const auto block = blocks[name.block];
            if (name.offset >= block.size())
                return std::nullopt;
            nameBytes = block.substr(name.offset);
        }

        const auto end = nameBytes.find('\0');
        if (end == std::string_view::npos)
            return std::nullopt;
        result.name = nameBytes.substr(0, end);
        if (name.kind == ReferenceKind::inlineData)
            remaining.remove_prefix(end + 1);
    }
    else if (name.kind != ReferenceKind::absent)
    {
        return std::nullopt;
    }

    // Load_RawFile treats the serialized buffer field as a presence flag.
    if (buffer)
    {
        if (length >= remaining.size())
            return std::nullopt;
        result.payload = remaining.substr(0, length);
        remaining.remove_prefix(length + 1);
    }

    inlineStream = remaining;
    return result;
}
}
