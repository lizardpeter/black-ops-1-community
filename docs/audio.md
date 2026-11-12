# Native Windows audio

Use event-driven WASAPI shared mode with `IAudioClient3` for the enhanced x64 endpoint. Query the endpoint mix format and supported periods. Start at the smallest sustainable period and fall back to a supported period when the driver or current engine configuration requires it. Offer exclusive mode only if measured latency warrants it.

BO1's gameplay voices, stream buffers, DSP and routing live in `snd_driver_xaudio2.cpp` and `snd_driver_xaudio2_dsp.cpp`. Bink separately attaches DirectSound in `r_cinematic.cpp` through `BinkSetSoundSystem`. Preserve voice mixing and the game's sound behavior when moving gameplay to a PCM mixer. Route decoded cinematic audio through the same mixer and use the audio clock for movie synchronization.

The output thread should be driven by the endpoint event, registered with MMCSS, and independent of rendering. Preallocate its buffers and keep decoding, file I/O, allocation, and blocking engine calls off the callback path. Measure endpoint period, buffer underruns, and input-to-speaker latency under load before lowering the default. Handle default-device changes and stream invalidation.

`bo1_wasapi_probe` is a Windows x64 diagnostic that prints supported periods for the default playback endpoint and attempts to initialize an event-driven shared stream at the minimum period. It neither starts playback nor measures physical output latency. Build it from `enhanced/port/x64` and run it on a machine with the target audio device. The game still uses its existing audio backends.
