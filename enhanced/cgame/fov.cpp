#include "fov.h"

#include <cgame_mp/cg_main_mp.h>
#include <universal/dvar.h>

namespace
{
const dvar_t *fov16x9 = nullptr;
}

void BO1_RegisterFovDvar()
{
    fov16x9 = _Dvar_RegisterFloat("cg_fov16x9", 120.0f, 0.0f, 120.0f, 1u,
                                 "Horizontal first-person FOV at 16:9 (60 to 120, 0 uses legacy cg_fov)");
}

float BO1_PlayerFov(float legacyFov)
{
    if (!fov16x9 || !cg_thirdPerson || cg_thirdPerson->current.integer)
        return legacyFov;

    const float degrees = fov16x9->current.value;
    return degrees > 0.0f ? bo1::HorizontalFovToEngine(degrees < 60.0f ? 60.0f : degrees)
                          : legacyFov;
}
