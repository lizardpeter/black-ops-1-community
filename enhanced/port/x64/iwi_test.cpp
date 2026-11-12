#include "iwi.h"

#include <stdexcept>
#include <string>

namespace
{
std::string Fixture(unsigned format, unsigned flags, unsigned width, unsigned height, std::string pixels)
{
    std::string file(48, '\0');
    file.replace(0, 3, "IWi");
    file[3] = 13;
    file[4] = static_cast<char>(format);
    file[5] = static_cast<char>(flags);
    file[6] = static_cast<char>(width);
    file[8] = static_cast<char>(height);
    file[10] = 1;
    file += pixels;
    for (unsigned i = 0; i < 4; ++i)
        file[16 + i] = static_cast<char>(file.size() >> (8 * i));
    return file;
}

bool Rejected(const std::string &file)
{
    try
    {
        bo1::DecodeIwi(file);
        return false;
    }
    catch (const std::runtime_error &)
    {
        return true;
    }
}
}

int main()
{
    const auto bgr = bo1::DecodeIwi(Fixture(2, 0, 2, 1, "xyzABCDEF"));
    if (bgr.width != 2 || bgr.height != 1 || bgr.format != bo1::IwiFormat::bgra8 ||
        bgr.topMip != std::vector<std::uint8_t>{65, 66, 67, 255, 68, 69, 70, 255})
        return 1;

    for (unsigned format : {11u, 12u, 13u})
    {
        const auto blockSize = format == 11 ? 8u : 16u;
        const auto bc = bo1::DecodeIwi(Fixture(format, 2, 5, 4, std::string(2 * blockSize, 'x')));
        if (bc.topMip != std::vector<std::uint8_t>(2 * blockSize, 'x'))
            return 1;
    }

    auto truncated = Fixture(1, 2, 1, 1, "abcd");
    truncated.pop_back();
    return Rejected(truncated) && Rejected(Fixture(1, 4, 1, 1, "abcd")) &&
           Rejected(Fixture(1, 0, 2, 1, "abcd")) ? 0 : 1;
}
