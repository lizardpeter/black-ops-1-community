# Native Vulkan bring-up

`enhanced/port/x64/gfx_vulkan` builds a standalone Windows x64 Vulkan 1.4 executable. It creates a Win32 surface, clears and presents a swapchain image, and rebuilds the swapchain when the window changes size. It requests no Vulkan layers. It links the system Vulkan loader and the Win32 window API.

Build with Visual Studio 2022 and a Vulkan 1.4 SDK, then run on a machine with a Vulkan 1.4 driver.

```powershell
cmake -S enhanced/port/x64/gfx_vulkan -B build/vulkan-x64 -G "Visual Studio 17 2022" -A x64
cmake --build build/vulkan-x64 --config Release
.\build\vulkan-x64\Release\bo1_vulkan_probe.exe
```

The title and console report IMMEDIATE or the FIFO fallback. A single command buffer and frame fence cap application GPU submissions at one outstanding frame. A zero-timeout image acquire keeps the message loop responsive. Each swapchain image owns its present-wait semaphore, reused only after acquisition of that image. Resize and shutdown use the base swapchain idle convention. Explicit presentation fences are still needed for guaranteed retirement of present resources. GPU submission depth does not establish display queue depth or physical latency.

The next render target is one real BO1 menu frame. That requires loading a BO1 texture, translating a representative D3D9 shader to SPIR-V, binding the texture and shader, and comparing output with the D3D9 build. The probe currently presents a clear color. The game still uses D3D9.
