#include "preview_shaders.h"

#include <fstream>
#include <map>
#include <set>
#include <stdexcept>

namespace bo1
{
namespace
{
struct InterfaceVariable
{
    int location = -1;
    int set = -1;
    int binding = -1;
    int storage = -1;
};

std::vector<std::uint32_t> ReadModule(const std::string &path, bool fragment)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    const auto size = static_cast<std::streamoff>(file.tellg());
    if (!file || size < 20 || size % 4 || size > 16 * 1024 * 1024)
        throw std::runtime_error("Invalid SPIR-V file: " + path);
    std::vector<std::uint32_t> words(static_cast<std::size_t>(size / 4));
    file.seekg(0);
    if (!file.read(reinterpret_cast<char *>(words.data()), size) || words[0] != 0x07230203)
        throw std::runtime_error("Expected little-endian SPIR-V: " + path);

    std::map<std::uint32_t, InterfaceVariable> interface;
    bool entryPoint = false;
    for (std::size_t offset = 5; offset < words.size();)
    {
        const auto opcode = words[offset] & 0xffff;
        const auto length = words[offset] >> 16;
        if (!length || length > words.size() - offset)
            throw std::runtime_error("Invalid SPIR-V instruction: " + path);
        const auto *op = words.data() + offset;
        if (opcode == 15 && length >= 5 && op[1] == (fragment ? 4u : 0u) &&
            op[3] == 0x6e69616d && op[4] == 0)
            entryPoint = true;
        if (opcode == 59 && length >= 4)
            interface[op[2]].storage = static_cast<int>(op[3]);
        if (opcode == 71 && length == 4)
        {
            if (op[2] == 30)
                interface[op[1]].location = static_cast<int>(op[3]);
            else if (op[2] == 33)
                interface[op[1]].binding = static_cast<int>(op[3]);
            else if (op[2] == 34)
                interface[op[1]].set = static_cast<int>(op[3]);
        }
        offset += length;
    }

    std::set<int> inputs, outputs;
    unsigned samplers = 0;
    for (const auto &[id, variable] : interface)
    {
        if (variable.storage == 1 && variable.location >= 0)
            inputs.insert(variable.location);
        if (variable.storage == 3 && variable.location >= 0)
            outputs.insert(variable.location);
        if (variable.storage == 0 || variable.storage == 2 ||
            variable.storage == 9 || variable.storage == 12)
        {
            if (!fragment || variable.storage != 0 || variable.set != 2 || variable.binding != 0)
                throw std::runtime_error("The preview only supports the translated pixel texture at set 2, binding 0");
            ++samplers;
        }
    }

    const auto expectedInputs = fragment ? std::set<int>{0} : std::set<int>{0, 1};
    if (!entryPoint || inputs != expectedInputs || outputs != std::set<int>{0} ||
        samplers != (fragment ? 1u : 0u))
        throw std::runtime_error("Shader interface does not match the position/UV preview: " + path);
    return words;
}
}

PreviewShaders ReadPreviewShaders(const std::string &prefix)
{
    return {ReadModule(prefix + ".vert.spv", false), ReadModule(prefix + ".frag.spv", true)};
}
}
