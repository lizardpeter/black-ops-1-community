# Multiplayer modernization

The enhanced target builds against the pinned KisakBlack tree. CMake copies only the translation units listed in `enhanced/CMakeLists.txt` into its build directory, applies the recorded source patch, and substitutes those units in the link. The faithful target compiles the pinned sources directly. Patch conflicts fail configuration.

| Area | BO1 source path | Implementation |
| --- | --- | --- |
| Mouse motion | `IN_Frame` → `IN_MouseMove` → `CL_MouseEvent` → `CL_GetMouseMovement` → `CL_MouseMove` | A dedicated Win32 message-only window collects relative Raw Input reports in pairs of 64-bit counters. The enhanced client drains them into the existing motion path. Menus still receive absolute cursor coordinates. The engine's sensitivity, filtering, acceleration, and aim limits still apply. |
| FOV and scale | `CG_GetViewFov` → `CG_UpdateCameraTweenFOV` → `CG_CalcFov` | `cg_fov16x9` uses horizontal degrees at 16:9 and defaults to 120. Set it to 0 for the legacy first-person FOV. Positive values below 60 are treated as 60. BO1's `cg_fovScale` still multiplies the engine-space angle in the existing camera path, with a final projection clamp. The original weapon zoom targets and camera-tween path still run. |
| Aspect ratio | `R_StoreWindowSettings` → `vidConfig.aspectRatioWindow` → `cgs.viewAspect` | Auto uses the actual width-to-height ratio for Hor+ projection. Explicit aspect modes continue to use their selected ratios. |
| Frame timing | `Com_Frame_Try_Block_Function` → `SV_FrameInternal`, `CL_RunOncePerClientFrame`, and the screen update | The outer loop supplies elapsed integer milliseconds to server and client. The server advances in `1000 / sv_fps` ms steps. `Pmove_1` integrates to each command's timestamp in slices of up to 66 ms. A higher render cap alone cannot decouple prediction or movement. |
| Presentation and Vulkan | Direct3D9 and device setup in `r_init.cpp`, `R_SyncGpu` → `R_FinishGpuFence`, `RB_SwapBuffers` → D3D9 swap-chain `Present` | The D3D9 presentation interval already requests immediate mode when `r_vsync` is disabled. A Vulkan port needs a BO1-specific inventory of D3D9 shader tokens, vertex declarations, render targets and resolves, render states, and GPU queries before a replacement can preserve scene rendering. |

## Next verification

- Compare movement traces and client/server prediction at different render limits using identical time-stamped inputs. Then prototype separate simulation and presentation clocks with shared movement stepping and render interpolation.
- Exercise Raw Input in a playable session with high-polling mice, focus changes, mouse capture, and menu transitions. Ghosts-style MW2019 sensitivity and render-time mouse late latch require changes farther down the input path.
- Check high FOV and ultrawide with ADS, vehicle cameras, and HUD layout in a playable session. The current test verifies the projection math.
