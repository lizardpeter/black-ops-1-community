#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

namespace bo1
{
enum class ReferenceKind
{
    absent,
    inlineData,
    inlineWithFixup,
    blockOffset,
    invalid
};

struct FastfileReference
{
    ReferenceKind kind;
    std::size_t block = 0;
    std::size_t offset = 0;
};

constexpr FastfileReference DecodeFastfileReference(std::uint32_t encoded)
{
    switch (encoded)
    {
    case 0: return {ReferenceKind::absent};
    case UINT32_MAX: return {ReferenceKind::inlineData};
    case UINT32_MAX - 1: return {ReferenceKind::inlineWithFixup};
    }

    const std::uint32_t address = encoded - 1;
    const std::size_t block = address >> 29;
    constexpr std::size_t blockCount = 7;
    if (block >= blockCount)
        return {ReferenceKind::invalid};
    return {ReferenceKind::blockOffset, block, address & 0x1fffffff};
}

inline std::optional<std::uintptr_t> ResolveFastfileReference(
    FastfileReference reference, std::size_t block, const std::byte *base, std::size_t size)
{
    if (reference.kind != ReferenceKind::blockOffset || reference.block != block ||
        !base || reference.offset >= size)
        return std::nullopt;
    return reinterpret_cast<std::uintptr_t>(base + reference.offset);
}
}
