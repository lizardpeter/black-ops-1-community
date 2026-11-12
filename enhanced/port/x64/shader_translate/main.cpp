#include <mojoshader.h>

#include <cstdio>
#include <fstream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
using Shader = std::unique_ptr<const MOJOSHADER_parseData, decltype(&MOJOSHADER_freeParseData)>;

Shader ReadShader(const char *path, MOJOSHADER_shaderType stage)
{
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input)
        throw std::runtime_error(std::string("Cannot open ") + path);
    const auto size = static_cast<std::streamoff>(input.tellg());
    if (size < 8 || size % 4 || size > std::numeric_limits<unsigned int>::max())
        throw std::runtime_error(std::string("Invalid D3D9 token stream size: ") + path);
    input.seekg(0);
    std::vector<unsigned char> tokens(static_cast<std::size_t>(size));
    if (!input.read(reinterpret_cast<char *>(tokens.data()), size))
        throw std::runtime_error(std::string("Cannot read ") + path);
    Shader parsed(MOJOSHADER_parse(MOJOSHADER_PROFILE_SPIRV, "main", tokens.data(),
                                   static_cast<unsigned int>(size), nullptr, 0, nullptr, 0,
                                   nullptr, nullptr, nullptr),
                  MOJOSHADER_freeParseData);
    if (parsed->error_count)
    {
        for (int index = 0; index < parsed->error_count; ++index)
        {
            const auto &error = parsed->errors[index];
            std::fprintf(stderr, "%s:%d: %s\n", path, error.error_position, error.error);
        }
        throw std::runtime_error("D3D9 shader translation failed");
    }
    if (parsed->shader_type != stage || !parsed->output)
        throw std::runtime_error(std::string("Unexpected shader stage: ") + path);
    return parsed;
}

void SaveModule(const std::string &path, const MOJOSHADER_parseData &shader, int patchSize)
{
    const int size = shader.output_len - patchSize;
    if (size < 20 || size % 4)
        throw std::runtime_error("Invalid SPIR-V module size");
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.write(shader.output, size))
        throw std::runtime_error("Cannot write " + path);
}
}

int main(int argc, char **argv)
{
    if (argc != 4)
    {
        std::fprintf(stderr, "Usage: bo1_shader_translate vertex.bin pixel.bin output-prefix\n");
        return 1;
    }
    try
    {
        const auto vertex = ReadShader(argv[1], MOJOSHADER_TYPE_VERTEX);
        const auto pixel = ReadShader(argv[2], MOJOSHADER_TYPE_PIXEL);
        std::vector<MOJOSHADER_vertexAttribute> attributes;
        for (int index = 0; index < vertex->attribute_count; ++index)
        {
            const auto &attribute = vertex->attributes[index];
            // First pass accepts floating-point shader inputs. Packed declarations need engine metadata.
            attributes.push_back({attribute.usage, MOJOSHADER_VERTEXELEMENTFORMAT_VECTOR4, attribute.index});
        }

        const int patchSize = MOJOSHADER_linkSPIRVShaders(vertex.get(), pixel.get(), attributes.data(),
                                                         static_cast<int>(attributes.size()));
        if (patchSize <= 0)
            throw std::runtime_error("Could not link D3D9 shader stages");
        SaveModule(std::string(argv[3]) + ".vert.spv", *vertex, patchSize);
        SaveModule(std::string(argv[3]) + ".frag.spv", *pixel, patchSize);
        std::printf("Vertex inputs: %d. Pixel samplers: %d.\n", vertex->attribute_count, pixel->sampler_count);
    }
    catch (const std::exception &error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
