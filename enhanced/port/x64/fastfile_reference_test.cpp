#include "fastfile_reference.h"

#include <array>

using bo1::DecodeFastfileReference;
using bo1::ReferenceKind;

static_assert(sizeof(void *) == 8);
static_assert(DecodeFastfileReference(0).kind == ReferenceKind::absent);
static_assert(DecodeFastfileReference(UINT32_MAX).kind == ReferenceKind::inlineData);
static_assert(DecodeFastfileReference(UINT32_MAX - 1).kind == ReferenceKind::inlineWithFixup);
static_assert(DecodeFastfileReference(1).offset == 0);
static_assert(DecodeFastfileReference(0x20000000).offset == 0x1fffffff);
static_assert(DecodeFastfileReference(0xc0000001).block == 6);
static_assert(DecodeFastfileReference(0xe0000001).kind == ReferenceKind::invalid);

int main()
{
    const std::array<std::byte, 8> bytes{};
    const auto reference = DecodeFastfileReference(0x80000004);
    const auto resolved = bo1::ResolveFastfileReference(reference, 4, bytes.data(), bytes.size());
    if (resolved != reinterpret_cast<std::uintptr_t>(bytes.data() + 3))
        return 1;
    if (bo1::ResolveFastfileReference(reference, 3, bytes.data(), bytes.size()) ||
        bo1::ResolveFastfileReference(reference, 4, bytes.data(), 3) ||
        bo1::ResolveFastfileReference(reference, 4, nullptr, bytes.size()) ||
        bo1::ResolveFastfileReference(DecodeFastfileReference(0), 4, bytes.data(), bytes.size()))
        return 1;
}
