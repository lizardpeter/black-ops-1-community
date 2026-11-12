#include "material_capture.h"

#include <gfx_d3d/r_material.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <string>
#include <string_view>

namespace
{
bool SaveShader(const std::filesystem::path &path, const DWORD *words, std::size_t count)
{
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    return bool(file.write(reinterpret_cast<const char *>(words), count * sizeof(*words)));
}
}

void BO1_CaptureMaterialShaders(const MaterialTechnique &technique)
{
    static const bool enabled = [] {
        const char *setting = std::getenv("BO1_CAPTURE_SHADERS");
        return setting && std::string_view(setting) == "1";
    }();
    if (!enabled)
        return;

    static std::mutex captureLock;
    static unsigned nextPass = 0;
    const std::lock_guard lock(captureLock);
    const std::filesystem::path directory = "bo1-shader-capture";
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (error)
        return;

    for (unsigned i = 0; i < technique.passCount; ++i)
    {
        const auto &pass = technique.passArray[i];
        if (!pass.vertexShader || !pass.pixelShader)
            continue;

        const auto &vertex = pass.vertexShader->prog.loadDef;
        const auto &pixel = pass.pixelShader->prog.loadDef;
        if (!vertex.program || !pixel.program || !vertex.programSize || !pixel.programSize)
            continue;

        auto base = directory / std::to_string(nextPass++);
        auto vertexFile = base;
        auto pixelFile = base;
        if (!SaveShader(vertexFile.replace_extension(".vs.bin"), vertex.program, vertex.programSize) ||
            !SaveShader(pixelFile.replace_extension(".ps.bin"), pixel.program, pixel.programSize))
            continue;

        std::ofstream metadata(base.replace_extension(".txt"), std::ios::trunc);
        metadata << "technique " << std::quoted(technique.name ? technique.name : "") << "\n"
                 << "pass " << i << "\n"
                 << "vertex " << std::quoted(pass.vertexShader->name ? pass.vertexShader->name : "") << "\n"
                 << "pixel " << std::quoted(pass.pixelShader->name ? pass.pixelShader->name : "") << "\n";
        if (pass.vertexDecl)
        {
            const auto count = std::min<unsigned>(pass.vertexDecl->streamCount, 16);
            for (unsigned route = 0; route < count; ++route)
            {
                const auto &stream = pass.vertexDecl->routing.data[route];
                metadata << "stream " << unsigned(stream.source) << " " << unsigned(stream.dest) << "\n";
            }
        }
    }
}
