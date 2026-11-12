# Windows runtime dependencies

The current enhanced multiplayer executable is Win32. Its Windows workflow records the executable's DLL import table for both Debug and Release. DLLs loaded by Steam, COM, or the game at runtime may be absent from that table.

| Dependency | Current Win32 evidence | x64 implication |
| --- | --- | --- |
| Bink | `binkw32.dll`, via the bundled import library | Requires a compatible native x64 Bink runtime or a replacement decoder. |
| Steam | `steam_api.dll` is linked. KisakBlack's launch instructions also stage `Steam.dll`, `steamclient.dll`, `tier0_s.dll`, and `vstdlib_s.dll`. The bundled Steam client directly imports the last two. | The upstream tree provides `steam_api64.dll` and its import library. The complete x64 Steam startup path needs validation with a compatible installed Steam client. |
| VPX | The executable links `libvpx.lib`, the static archive. The separate `vpx.lib` imports `vpx.dll`. Upstream's launch instructions include `vpx.dll`. | Build the static library for x64. |
| DirectX | D3D9, legacy D3DX9, DirectSound, and XInput are part of the current link. The audio path uses the legacy XAudio2 headers and XAPO libraries. | Port or replace each subsystem. Vulkan only replaces the graphics backend. |
| Runtime-loaded Windows and NVIDIA libraries | The texture-memory probe loads `ddraw.dll` and the minidumper loads `dbghelp.dll`. The NVAPI loader comes from the vendor archive. | Verify native load behavior and capability fallbacks on x64. |

Game data still comes from a local BO1 installation. Use the pinned KisakBlack staging instructions when launching the Win32 build.

## Verified executable imports

[Windows CI](https://github.com/lizardpeter/black-ops-1-community/actions/runs/43158112275) inspected both enhanced executables with `dumpbin /dependents`.

- **Both configurations:** `steam_api.dll`, `binkw32.dll`, `d3d9.dll`, `xinput1_3.dll`, `dsound.dll`, `winmm.dll`, `ws2_32.dll`, `kernel32.dll`, `user32.dll`, `gdi32.dll`, `shell32.dll`, `ole32.dll`, `advapi32.dll`, and `secur32.dll`.
- **Release:** `d3dx9_43.dll`, `msvcp140.dll`, `vcruntime140.dll`, and the Universal CRT API sets.
- **Debug:** `d3dx9d_43.dll`, `msvcp140d.dll`, `vcruntime140d.dll`, `ucrtbased.dll`, and `dbghelp.dll`.

The import tables omit `vpx.dll`. The linked static VPX archive supplies the encoder. A live run is still needed to log the full set of modules loaded by Steam, COM, and vendor libraries.
