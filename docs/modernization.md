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

## Frame and user-command timing

| Boundary | Source | What to measure before separating clocks |
| --- | --- | --- |
| Outer frame budget | `qcommon/common.cpp`, `Com_Frame_Try_Block_Function` | Event-pump time, measured elapsed time, the minimum time from `com_maxfps`, and the time actually waited in `R_WaitEndTime`. The current loop can advance its bookkeeping by more than the measured elapsed time. |
| Input and commands | `client_mp/cl_main_mp.cpp`, `CL_RunOncePerClientFrame` and `client_mp/cl_input_mp.cpp`, `CL_CreateNewCommands` | Motion counts received and consumed, command numbers, command server times, and commands delivered by `CL_SendCmd`. Track menu, loading, and active play separately. |
| Prediction and authority | `cgame_mp/cg_predict_mp.cpp`, `CG_PredictPlayerState` and `bgame/bg_pmove.cpp`, `Pmove_1` | Replay identical time-stamped commands through client prediction and authoritative movement. Compare command time, position, velocity, jump apex, and correction frequency at 60, 120, 240, and uncapped presentation rates. |
| Screen and GPU | `client_mp/cl_scrn_mp.cpp`, `SCR_UpdateScreen` and `gfx_d3d/rb_backend.cpp`, `RB_SwapBuffers` | Record scene build, GPU fence wait, and actual present timestamps separately. These distinguish render throughput from simulation frequency. |

A fixed simulation clock should start with local multiplayer and use the same command semantics in prediction and on the server. Keep the current wire format until identical command streams demonstrate parity. Interpolate between simulation states for rendering and keep render-only view updates out of authoritative command state.

## D3D9 backend boundary

| Area | BO1 source | Vulkan port dependency |
| --- | --- | --- |
| Device and present | `R_PreCreateWindow`, `R_CreateDevice`, `R_SetD3DPresentParameters`, `R_ResetDevice`, `RB_SwapBuffers` | Swapchain creation, present mode, image lifetime, resize, and device recovery. |
| Shaders and input layout | `database/db_load.cpp` material shader loaders, `gfx_d3d/r_material.cpp`, `gfx_d3d/rb_resource.cpp` | Inventory the D3D9 vertex and pixel shader token streams and declarations. Translate and compare representative shaders, including their register bindings, before wiring SPIR-V into the scene. |
| Buffers and textures | `gfx_d3d/r_buffers.cpp`, `gfx_d3d/r_image.cpp` | Map the default, managed, and system-memory resource usages to staging, GPU allocations, and per-frame uploads. |
| Render targets and resolves | `gfx_d3d/r_rendertarget.cpp`, `R_Resolve` in `rb_backend.cpp`, and `rb_postfx.cpp` | Separate copies, filtered blits, multisample resolves, and sampleable depth. The D3D9 path includes an INTZ depth texture case that needs a format and usage equivalent. |
| Pipeline and draws | `R_SetState` in `r_shade.cpp` and stream binding, sampler state, and `DrawIndexedPrimitive` in `r_state.cpp` | Convert state bits, shaders, declarations, and render target formats into pipeline keys. Compare a captured draw sequence before moving larger render passes. |
| Queries and fences | Query creation in `r_init.cpp` and `R_FinishGpuFence` in `r_setstate_d3d.cpp` | Map occlusion queries and frame-completion tracking separately. Verify latency at present with a live swapchain. |
