# Black Ops 1 Community

Windows x86 multiplayer builds of Call of Duty: Black Ops, based on [SwagSoftware/KisakBlack](https://github.com/SwagSoftware/KisakBlack). The upstream source, credits, and GPL-3.0 license are in `upstream/kisakblack` at the commit pinned by this repository.

| Preset | Executable | Scope |
| --- | --- | --- |
| `faithful` | `KisakBlack.exe` | Upstream multiplayer reconstruction |
| `enhanced` | `BlackOpsMP-Community.exe` | Same multiplayer engine with community changes under `enhanced/`. Windows file properties identify the enhanced build. |

The faithful preset refers to the upstream reconstruction. Retail binary parity, Campaign, and Zombies are outside the current build targets.

## Build on Windows

Install Visual Studio 2022 with **Desktop development with C++**, CMake 3.21 or newer, and the [June 2010 DirectX SDK](https://www.microsoft.com/en-us/download/details.aspx?id=6812). Clone with submodules or initialize them in an existing checkout.

```powershell
git submodule update --init --recursive
cmake --preset faithful
cmake --build --preset faithful-release
cmake --preset enhanced
cmake --build --preset enhanced-release
```

For an extracted SDK, append `-DDXSDK_DIR=C:/path/to/DXSDK` to each configure command. The directory should contain `Include/d3dx9.h` and `Lib/x86`. For Debug, use the `faithful-debug` or `enhanced-debug` build preset.

The executables land in `build/faithful/Release` and `build/enhanced/Release`. To launch either build, use the game data from your own Steam installation and stage the runtime DLLs listed in the [upstream instructions](https://github.com/SwagSoftware/KisakBlack/blob/6276ed21bc6fbaeab6f2ac6357ff22b82d3c4487/README.md) alongside the executable. Compilation can run before staging game data.

See [STATUS.md](STATUS.md) for verification scope.
