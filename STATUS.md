# Status

- Upstream baseline is KisakBlack commit `6276ed21bc6fbaeab6f2ac6357ff22b82d3c4487`.
- The faithful and enhanced presets target Visual Studio 2022 Win32, in Debug and Release.
- The enhanced executable has its own name and Windows version resource. It shares the upstream multiplayer engine.
- Windows CI checks compilation, linking, and x86 PE output. A separate runtime check with local game data is needed to establish launch and gameplay behavior.
