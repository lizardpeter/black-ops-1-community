# Status

- Upstream baseline is KisakBlack commit `6276ed21bc6fbaeab6f2ac6357ff22b82d3c4487`.
- The faithful and enhanced presets target Visual Studio 2022 Win32, in Debug and Release.
- The enhanced executable has its own name and Windows version resource. It shares the upstream multiplayer engine.
- Windows CI checks compilation, linking, and x86 PE output. A separate runtime check with local game data is needed to establish launch and gameplay behavior.

## Multiplayer enhancements

See [the patch records](records/patches.tsv) and [source paths for input, FOV, frame timing, and rendering](docs/modernization.md). Raw mouse motion and FOV changes need a live game check with local assets.

## Local executable bring-up

The enhanced sources now select a local platform backend and exclude the Steam and Bink implementations. The temporary cinematic backend skips movies. The Windows link and launch of this change set are pending, along with native movie playback, legacy graphics and audio removal, and the complete x64 executable.

The native Windows x64 Vulkan probe is in `enhanced/port/x64/gfx_vulkan`. See [Vulkan bring-up](docs/vulkan.md) for the build commands and presentation behavior.
