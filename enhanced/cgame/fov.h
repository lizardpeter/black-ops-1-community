#pragma once

#include <cmath>

namespace bo1
{
inline float HorizontalFovToEngine(float degrees)
{
    constexpr double halfRadiansPerDegree = 0.008726646259971648;
    return static_cast<float>(std::atan(0.75 * std::tan(degrees * halfRadiansPerDegree)) /
                              halfRadiansPerDegree);
}

inline float ClampProjectionFov(float degrees)
{
    if (!(degrees >= 1.0f))
        return 1.0f;
    return degrees > 170.0f ? 170.0f : degrees;
}

inline float DisplayAspect(int width, int height)
{
    return width > 0 && height > 0 ? static_cast<float>(width) / height : 4.0f / 3.0f;
}
}

void BO1_RegisterFovDvar();
float BO1_PlayerFov(float legacyFov);
