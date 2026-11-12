#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace bo1
{
struct PreviewShaders
{
    std::vector<std::uint32_t> vertex;
    std::vector<std::uint32_t> fragment;
};

PreviewShaders ReadPreviewShaders(const std::string &prefix);
}
