#include <enhanced/cgame/fov.h>

#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <limits>

int main()
{
    constexpr double halfRadiansPerDegree = 0.008726646259971648;
    for (const float fov : {60.0f, 80.0f, 120.0f})
    {
        const double engineTangent = std::tan(bo1::HorizontalFovToEngine(fov) * halfRadiansPerDegree);
        const double horizontal = std::atan(engineTangent * 0.75 * bo1::DisplayAspect(1920, 1080)) /
                                  halfRadiansPerDegree;
        if (std::abs(horizontal - fov) > 0.0001)
        {
            std::fprintf(stderr, "Expected %.3f degrees at 16:9, got %.6f\n", fov, horizontal);
            return 1;
        }
    }

    const double ultrawideTangent = std::tan(bo1::HorizontalFovToEngine(120.0f) * halfRadiansPerDegree) *
                                    0.75 * bo1::DisplayAspect(3440, 1440);
    const double ultrawide = std::atan(ultrawideTangent) / halfRadiansPerDegree;
    if (ultrawide < 132.0 || ultrawide > 134.0 ||
        bo1::DisplayAspect(0, 0) != 4.0f / 3.0f ||
        bo1::ClampProjectionFov(160.0f * 2.0f) != 170.0f ||
        bo1::ClampProjectionFov(65.0f) != 65.0f ||
        bo1::ClampProjectionFov(std::numeric_limits<float>::quiet_NaN()) != 1.0f)
    {
        std::fprintf(stderr, "Unexpected ultrawide projection or clamp result\n");
        return 1;
    }
}
