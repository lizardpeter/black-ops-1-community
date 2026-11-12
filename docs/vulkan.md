# Native Vulkan bring-up

`enhanced/port/x64/gfx_vulkan` builds a standalone Windows x64 Vulkan 1.4 executable. It creates a Win32 surface, clears and presents a swapchain image, and rebuilds the swapchain when the window changes size. It requests no Vulkan layers. It links the system Vulkan loader and the Win32 window API.

Build with Visual Studio 2022 and a Vulkan 1.4 SDK, then run on a machine with a Vulkan 1.4 driver.

```powershell
cmake -S enhanced/port/x64/gfx_vulkan -B build/vulkan-x64 -G "Visual Studio 17 2022" -A x64
cmake --build build/vulkan-x64 --config Release
.\build\vulkan-x64\Release\bo1_vulkan_probe.exe
```

The title and console report IMMEDIATE or the FIFO fallback. A single command buffer and frame fence cap application GPU submissions at one outstanding frame. A zero-timeout image acquire keeps the message loop responsive. Each swapchain image owns its present-wait semaphore, reused only after acquisition of that image. Resize and shutdown use the base swapchain idle convention. Explicit presentation fences are still needed for guaranteed retirement of present resources. GPU submission depth does not establish display queue depth or physical latency.

The next render target is one real BO1 menu frame. That requires loading a BO1 texture, translating a representative D3D9 shader to SPIR-V, binding the texture and shader, and comparing output with the D3D9 build. The game still uses D3D9.

## Preview a BO1 texture

Pass a loose PC BO1 IWI v13 file extracted from your installation. The probe supports 2D BGRA, BGR, BC1, BC2, and BC3, and displays the largest mip while retaining the image's aspect ratio. Block-compressed formats require GPU BC support. The sample shader is built into the executable from GLSL with the Vulkan SDK's glslang compiler.

```powershell
.\build\vulkan-x64\Release\bo1_vulkan_probe.exe C:\path\to\image.iwi
```

With no image argument, the probe clears the swapchain. With an image, it uploads through a staging buffer and draws with a sampled-image descriptor and dynamic rendering. This exercises the image and shader pipeline. BO1's menu layout, actual material shaders, and retail asset parity remain to be integrated.

## Translate a D3D9 shader pair

Build the separate offline tool with CMake 3.28 or newer and Git. It uses a pinned, statically linked MojoShader SPIR-V backend. Supply matching vertex and pixel DWORD streams from a BO1 material pass. The input files contain the shader tokens only, with no asset or fastfile header.

```powershell
cmake -S enhanced/port/x64/shader_translate -B build/shader-translate
cmake --build build/shader-translate --config Release --target bo1_shader_translate
.\build\shader-translate\Release\bo1_shader_translate.exe vertex.bin pixel.bin menu
```

The tool links the shader stages and writes `menu.vert.spv` and `menu.frag.spv`. It currently assumes float vertex inputs. Porting a material also requires its vertex declaration, descriptor bindings, constants, textures, and render state. The synthetic Shader Model 3 test passed SPIR-V validation. Retail material coverage has yet to be measured.
