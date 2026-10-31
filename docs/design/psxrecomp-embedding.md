# PSXRecomp runtime map for embedding (survey of 2a4824fc)

## PSXRecomp runtime map for embedding in StepMania 2005 (commit 2a4824fc, read-only survey)

Paths: unless a path is given, files are under `/home/user/work/tools/recomp-src/psxrecomp/runtime/src/`; headers are under `/home/user/work/tools/recomp-src/psxrecomp/runtime/include/`. The config loader is `/home/user/work/tools/recomp-src/psxrecomp/recompiler/src/config_loader.cpp`.

**The key fact for embedding:** there is no host main loop. `main()` sets everything up and then calls `psx_scheduler_run(&cpu)` (main.cpp:20007, traps.c:809), which never returns in normal play. The recompiled guest code runs on the main thread. All per-frame host work (input, audio, present, pacing) runs inside a VBlank callback that guest code calls in the middle of its own execution. So "step one frame" means suspending the core inside that callback.

### 1. Entry, startup, threads, pacing, VBlank

**`main()`** is at main.cpp:15876. Startup order:
- **Crash handlers** first: `psx_crash_trace_install_handlers` (15922).
- **Command-line options** (16004-16078): `--game`, `--disc`, `--bios`, `--renderer`, `--memcard-dir`, `--debug-port`, `--headless`, `--headless-opengl`, `--hidden-window`. The `PSX_HEADLESS` environment variable is read at 16079.
- **game.toml**: located at 16087 and loaded by `load_game_config` (16209). `[audio] buffer_ms` is taken at 16219. `settings.toml` is read from the exe directory at 16776.
- **BIOS**: `resolve_bios_for_runtime` (3160) defaults to the bundled `bios/openbios.bin` (main.cpp:217). The BIOS code itself is statically linked; the file only provides ROM bytes. It is validated at 18572 and loaded by `memory_init` at 18588.
- **Renderer**: `gr_set_backend` (18646); headless forces software. Then `gpu_init` (18656) and `apply_internal_resolution` (18688).
- **Devices**: `dma_init`, `mdec_init`, `timers_init`, `interrupts_init`, `sio_init` (18885-18889), `spu_init` (18929), `cdrom_init(disc)` (18930; returns 1 if the disc doesn't mount, 18940).
- **Region**: PAL is detected from the disc at 19007; `gpu_set_power_on_video_mode` at 19017.
- **Memory cards and debug**: `memcard_init_slots` (19053), two `atexit` registrations (19056-19060), `debug_server_init` (19062), `freeze_heartbeat_start` (19071).
- **Headless branch**: 19096-19133.
- **Normal branch**:
  - `SDL_Init(VIDEO|GAMECONTROLLER)` (19175).
  - Controllers opened (19180-19195).
  - Audio: device open and DRC init (19196-19232).
  - `SDL_CreateWindow` (19265).
  - GL context: `gl_renderer_init_context` (19328).
  - Vulkan (19409) or SDL_Renderer fallback (19448-19502).
- **VBlank callback**: `gpu_set_vblank_callback(sdl_vblank_present)` (19513).
- **CPU state**: `CPUState cpu` lives on `main()`'s stack (19650); reset PC 0xBFC00000 (19732).
- **Host hotkeys**: `config.ini` (19738).
- **Savestates**: `savestate_configure` (19770).
- **Run**: `psx_scheduler_run` (20007); teardown at 20071-20081.

**VBlank generation:**
- Period constants are in psx_video_timing.h:27-28: NTSC 564480 cycles, PAL 677376 cycles. The live standard follows GP1(08h) bit 3.
- Call chain:
  1. Guest cycle charges reach `psx_devices_service_to_now` (psx_cycles.c:349) and `advance_devices` (psx_cycles.c:211).
  2. Then `interrupts_advance_cycles` (interrupts.c:532) and `fire_vblank_edge` (interrupts.c:485). Overshoot carries into the next frame.
  3. Then `gpu_vblank_tick` (gpu.c:4131), which runs `mod_runtime_on_vblank` (the vblank plugins), raises the IRQ and calls the callback (gpu.c:4192).
- Netplay defers the callback to `gpu_vblank_flush_present` (gpu.c:4074).
- After the edge, the audio pump also runs (interrupts.c:508).

**Per-frame body** — `sdl_vblank_present` (10068) calls `sdl_vblank_present_body` (8312):
1. Debug server poll and frame record (8331-8334).
2. Overlay autocapture tick and compile pickup (8456-8467).
3. `drain_host_events` (8475).
4. Pad sampling (8535-8543).
5. `sdl_audio_update` (8657).
6. `mod_call_frame_hooks` (8672).
7. Headless returns here (8690).
8. Turbo / FMV-skip early returns.
9. Pacing with `frame_pacer_wait` (8849).
10. Optional low-latency input resample (8863-8879).
11. Present (8933-9469).

After the body: `render_thread_vblank`, `dynres_tick`, and the netplay epilogue (10070-10096).

**Threads:**
- The emulation thread is the `main()` thread. It needs a 64 MiB stack (link options at runtime.cmake:2500 and :2517).
- With the default settings it does everything on that one thread: GP0/GP1 processing, rasterization or GL calls during the frame, present/swap, pacing and audio push.
- Other threads:
  - SDL audio callback (`sdl_drc_callback`, 2682).
  - Debug I/O thread `psx-dbg-io` (debug_server.c:16478). Commands still execute on the emulation thread inside `debug_server_poll`.
  - Freeze-heartbeat watchdog. It suspends the thread that started it to capture stacks (freeze_heartbeat.c:1142-1151).
  - Overlay-capture worker.
  - External gcc/tcc compiler processes (autocompile.c).
- The optional render thread and present thread are OpenGL-only and off by default; DSF's game.toml doesn't enable them. When on, they use an SPSC ring with a 2-frame backpressure bound (render_thread.c, `docs/RENDER_THREAD.md`).

**Frame pacing:**
- A deadline-based wall-clock pacer: `frame_pacer_wait` (frame_pacing.c:46), called at main.cpp:8849 before the present.
- The period `g_frame_period_ms` is 20 ms for PAL (`sync_guest_cadence_to_video_standard`, 3596). It is snapped to the host refresh only if within 2% (3564).
- The pacer always runs (3686). Swap-interval vsync is enabled only when the host refresh matches the guest rate (3654), so PAL on a 60 Hz panel is pacer-only.
- Audio never paces. The SPU is clocked by guest cycles and the DRC bridge absorbs drift.
- Headless skips pacing entirely (8694), so it runs as fast as the host can.

### 2. Video

**GPU core:** GP0/GP1 handling is in gpu.c (`gpu_write_gp0` / `gpu_write_gp1`, gpu.h:21-22). The CPU-side VRAM is `static uint16_t vram[1024*512]` (gpu.c:67; accessor `gpu_get_vram`, gpu.c:4197). Drawing goes through the `gr_*` facade (gpu_render.h, gpu_render.c) to one of three backends:
- **Software rasterizer** (gpu_sw_renderer.c): the default and the fallback. It draws in place into gpu.c's VRAM, with up to a 4x hi-res mirror.
- **OpenGL** (gpu_gl_renderer.c): the GPU holds the authoritative VRAM (`s_hr_fbo` / `s_hr_tex`, :730, at the internal scale). The CPU VRAM is only refreshed by readback (`gl_renderer_sync_cpu`, :6523).
- **Vulkan** (experimental).

**Display area:** `gpu_get_display_info` (gpu.c:4383) fills `GpuDisplayInfo` (gpu.h:35-46): `display_x`/`display_y`, `width` (256/320/368/512/640), `height` (up to 512; interlace doubles it), `depth24`, `disabled`.
- 15-bit pixels: `gpu_display_pixel_argb` (gpu.c:4334).
- 24-bit (FMV) pixels: `gpu_depth24_present_row` (gpu.c:4350), wrapped by `depth24_stage_scanout` (main.cpp:7484). 24-bit frames always come from the CPU copy, even under GL.

**Present path (main.cpp:9053-9469):**
- GL, 15-bit frames: `gl_renderer_present_vram` (gpu_gl_renderer.c:10592) blits the GL framebuffer (FBO) straight to the window, or `present_wide_fbo`.
- Otherwise the frame is staged as ARGB in `sdl_pixel_buf` (9187-9207), via `gr_render_display_hires`, the depth24 path, or a per-pixel loop. It is then sent to `gl_renderer_present` (6401) or to SDL_UpdateTexture / SDL_RenderCopy / SDL_RenderPresent (9318-9454).
- The GL swap goes `gl_swap_with_osd` (9398) → `gl_present_swap` (9365) → `SDL_GL_SwapWindow` (9253).
- Internal scale is set through `gr_set_scale`; up to 32x on GL, 4x on software.

**Existing CPU-pixel readback paths:**

| Path | What it gives |
|---|---|
| `gr_render_display` / `gr_render_display_hires(out, pitch, x, y, w, h)` (gpu_render.h) | Backend-neutral ARGB readout |
| Loop over `gpu_display_pixel_argb` | Native-resolution ARGB; same as main.cpp:9201 and the TCP `screenshot_file` command (debug_server.c:10092) |
| `gl_renderer_capture_display_hires` (gpu_gl_renderer.c:5336) / `gl_renderer_read_display_hires` | `glReadPixels` of the GL VRAM surface at internal scale |
| `headless_present_image_ring_capture` (main.cpp:8086) | A worked example of the right capture logic per mode |
| `present_shot` (main.cpp:2368-2399, done at 9384) | Window readback; not available headless |

The presented-image ring is only 120-row thumbnails, so it is no use for real frames.

**GL requirements:**
- Requests a 3.3 core profile, double buffer, stencil 8 (main.cpp:2245-2276).
- Shaders are `#version 330`.
- Entry points are loaded with `SDL_GL_GetProcAddress` (`load_modern_gl`, :330-389). Required: VAOs, FBOs and renderbuffers, `glBlitFramebuffer`, `glBindFragDataLocationIndexed` (dual-source blending), integer textures. Optional: timer queries, fence sync, MapBuffer.
- It is all-or-nothing and falls back to software on any failure (:6312).

**SDL coupling of the GL renderer:** it is tied to SDL. `gl_renderer_init_context(SDL_Window*)` (:6238) creates its own context with `SDL_GL_CreateContext`. It keeps the window and context in statics `s_win` / `s_ctx` (:392). It calls `SDL_GL_GetDrawableSize` about 22 times and `SDL_GL_MakeCurrent` about 10 times, plus `SDL_GL_SetSwapInterval`, `SDL_GL_SwapWindow`, and a shared context for the present thread (:9318).
- There is no way to hand it an external HGLRC without patching those calls.
- It renders into framebuffer 0 and changes global GL state (program, VAO, blend, viewport, pixel-store), which would clash with StepMania's fixed-function state.
- `gr_*` drawing happens throughout the frame, not just at VBlank. So with GL the emulation thread needs its own current (possibly shared) context the whole time.

**Where SDL video/window/GL calls are made:**
- main.cpp: SDL_Init (19101, 19175), SDL_CreateWindow (19106, 19265), SDL_CreateRenderer (19457), SDL_RenderSetLogicalSize (19474), present calls (9318-9454), SDL_SetWindowTitle (8393), SDL_PollEvent (8161, 7980, 8027), SDL_GetKeyboardState (8718, 6261), SDL_GetCurrentDisplayMode (3535), plus fullscreen and window-icon helpers.
- gpu_gl_renderer.c (listed above).
- host_osd.c (on-screen display via SDL_Renderer).
- local_mouse_sdl.cpp, psx_keybinds.c, host_keymap.c, psx_window_icon.cpp, gpu_vk_renderer.c.

### 3. Audio

**Sample production:** `spu_catch_up` (spu.c:1478) renders, at 768 CPU cycles per sample (spu.c:67), into a 16384-frame queue that drops the oldest samples on overflow (spu.c:68, :1463).
- Format: 44100 Hz, S16, stereo interleaved.
- Per frame: 735 (NTSC) or exactly 882 (PAL) frames.

**Pump:** `sdl_audio_pump` (main.cpp:4044):
1. `spu_catch_up`, then `spu_output_pop` (spu.c:1503) into `sdl_audio_buf[2048*2]` (2653).
2. Fade-in ramp and host volume (4136-4153).
3. `rab_push(&s_drc, ...)` under the SDL audio lock (4164-4166). The legacy `SDL_QueueAudio` path is used only with `PSXRECOMP_AUDIO_LEGACY=1`.

**Who calls the pump:**
- `psx_set_midframe_audio_pump(sdl_audio_pump_midframe)` (19227). It runs at every VBlank edge (interrupts.c:508) and at every 768-cycle sample boundary while the SPU IRQ is enabled (interrupts.c:463-483).
- `sdl_audio_update` (4641) runs once per frame and handles mute/turbo gating.

**Host side:**
- `psx_sdl_audio_open` (psx_sdl_audio.cpp:24 for SDL3 `SDL_OpenAudioDeviceStream`; :111 for SDL2): 44100, S16, stereo, 1024 samples.
- Pull callback `sdl_drc_callback` → `rab_pull` (main.cpp:2682).
- DRC bridge (recomp_audio_drc.h): sinc resampler plus a P-controller (±0.5%). `target_ms` comes from `[audio] buffer_ms`: DSF sets 50; parsed at config_loader.cpp:972, allowed range 30-500.

**Cleanest intercept:** your own pump. Call `psx_set_midframe_audio_pump(my_pump)` after init. In `my_pump`: `spu_catch_up(); n = spu_output_pop(buf, cap);` and push to your sink. You can reuse `rab_push` with `rab_pull` from StepMania's DirectSound mixer thread.
- The minimal-patch alternative is the `rab_push` line at main.cpp:4165.
- Keep `spu_set_output_gate` and the savestate resync (`g_audio_cycle_resync`, 4101-4110).
- Drain at least every ~370 ms or samples are dropped.

### 4. Input

**Per-frame sampling** (main.cpp:8535-8543): `mod_controller_source_begin_frame`, then `sample_pad_into_sio` (7081) or the headless version (7144), then `apply_input_override_port2` (7063) and `env_pad_hold_apply` (`PSX_HOLD_PADS`, 7125). An optional second sample runs after pacing (8863).

**Resolution per port:** `pad_ext_resolve` (pad_external_input.c:64) with the hooks from `pad_ext_main_hooks` (main.cpp:7045), in order:
1. Physical capture: `capture_pad_slot` (6279), from SDL keyboard/gamecontroller. It returns 0 when the port has no assigned device.
2. Controller source, `psx_mod_set_controller_source` (mod_plugins.h:947-951): buttons are the source AND the physical pad. **It works even when the port has no device.**
3. Pad transform, `psx_mod_set_pad_transform` (mod_plugins.h:953-1008; types `PSXModPadFrame` / `PSXModPadOutput` / `PSXModPadTransform`). **It only runs if step 1 or 2 produced a pad.**
4. Mouse policy (P1 only).

The result goes `apply_pad_slot_to_sio` (6341) → `psx_pad_apply_to_sio` → `sio_set_pad_state_slot` / `sio_set_pad_sticks` / `sio_request_pad_type` (sio.h:144-198).

**Pad word:** 16 bits, active-low. Bit layout at main.cpp:4714-4736: Select 0, Start 3, Up 4, Right 5, Down 6, Left 7, L2 8, R2 9, L1 10, R1 11, Triangle 12, Circle 13, Cross 14, Square 15.

**Gating:** sources and transforms never run under netplay, rollback resimulation, selfcheck, headless, or a debug-server override (mod_plugins.h:924-929).

**DSF caveat:** P2 defaults to device "none" (main.cpp:16130-16133). So the DSF plugin's P2 transform (`/home/user/work/port/plugins/dsf_input/dsf_input_plugin.cpp`) never runs unless P2 is given a device. That plugin also finds its window by the "SDL_app" class.

### 5. Headless and the debug server

**`--headless` (16037):**
- Forces the software backend (18646) and skips SDL_Init entirely: no window, audio device or controllers (19096-19133).
- Emulation still runs fully: the CPU VRAM is rasterized, and the SPU renders, but samples are discarded because there is no device (4132).
- The pad is forced to 0xFFFF (all released) unless there is a debug override (7175).
- Present and pacing are skipped (8690-8696), so it runs unpaced.
- `--headless-opengl` uses a hidden SDL GL window (19101-19130). `--hidden-window` runs the full presenter in a hidden window.

**Debug server** (debug_server.c):
- Built only with `PSX_DEBUG_TOOLS`, which defaults OFF for Release (runtime.cmake:84-88). Port 4370 is a compile definition; `--debug-port` overrides it.
- Commands run at each VBlank via `debug_server_poll` (main.cpp:8332).
- Frame commands: `screenshot_file` (10092; syncs GL first, then reads `gpu_display_pixel_rgb`), `screenshot_hires` (10156), `screenshot` (10794), `present_shot` (window only).
- An always-on display ring copies the display and full VRAM every frame (around 9950; `PSX_DISPLAY_RING=0` turns it off).
- Input/control commands: `set_input`, `press`, `turbo`, `step` (16082-16099). Pause/step are effectively removed (17016).

### 6. Global state and `main()` ownership problems

- **No init/step API.** All setup is inline in a ~4200-line `main()` (15876-20081). Everything is file-static singletons (gpu.c `vram`, SPU queue, `sdl_window`, `sdl_audio_device`, `s_drc`, `g_players`). `CPUState` lives on `main()`'s stack with pointers handed out (19797, 19803, `debug_server_set_cpu`). One instance per process.
- **Executable only.** `add_executable` only (runtime.cmake:1610). main.cpp includes `SDL3/SDL_main.h` (158-162). MinGW uses `-mwindows` and a 64 MiB stack. The runtime calls SDL_Init and SDL_Quit itself (20079).
- **setjmp/longjmp.** The core unwinds to `g_scheduler_jmpbuf` (traps.c:699, :744, :806, :815; interrupts.c:1411, :2177). The VBlank callback itself can be longjmp'd out of (savestate/selfcheck load, 10074). So any host code you insert there must not hold locks or RAII across it, and the core must stay on one thread.
- **Process exits:**
  - Window close: `drain_host_events` handles SDL_QUIT with `shutdown_runtime` + `std::exit(0)` (8163-8170; also 7985, 8032, 3799).
  - Fatal paths: `trap_crash` / `psx_fatal_halt` exit(1) (traps.c:215; crash_trace.c:852-872). More `exit(1)` in memory.c:1218/1225 (BIOS file) and traps.c; `std::exit(1)` in gte.cpp:2335/2503; `abort()` in gpu.c and mdec.c.
  - The TCP `quit` command calls exit(0) (debug_server.c:13365).
  - **Starvation watchdog:** `exit(2)` (starvation_ring.c:255-277) if no heartbeat for 4 s, in debug-tools builds only. A host that pauses the core for more than 4 s will kill the process on resume. Use a Release build, or set `PSX_STARVATION_TIMEOUT_US=0`, or call `starvation_watchdog_heartbeat()` after a pause.
- **Handlers it installs:** crash_trace.c:956-976 installs SIGSEGV, SIGINT, SIGTERM, SIGUSR1, SIGABRT, `SetUnhandledExceptionFilter`, `SetConsoleCtrlHandler` and `atexit` — these will hijack StepMania's crash handling. Also `atexit(memcard_flush_all / game_options_save_now)` (19056) and a SIGPROF sampler (host_sampler.c:148).
- **Paths and side effects:** all files are anchored on the executable's directory via `GetModuleFileNameW` (main.cpp:2715), which inside StepMania is StepMania's directory. Overlay autocompile spawns gcc/tcc processes and writes `overlay_captures.json` and a cache (game.toml has `overlay_cache = true`). Many behaviours are switched by environment variables.

### 7. Saves and config paths

- **game.toml lookup:** `--game`, or the baked default "game.toml", via `resolve_existing_runtime_path` (2748). It checks the current working directory first (2755), then the exe directory, then walks upward. DSF's CMake copies game.toml next to the exe.
- **Relative paths inside game.toml** resolve against `find_project_root` (config_loader.cpp:1096-1110): the nearest directory up to 8 levels above game.toml that contains `.gitignore`, `.git` or `CMakeLists.txt`, otherwise game.toml's own directory (`memcard_dir` at config_loader.cpp:415-418). So `memcard_dir = "saves"` may land in the dsf-project folder from a dev build tree.
- **Overrides:** `settings.toml` in the exe directory (16776; its `memcard_dir` is anchored on the exe directory, 16941), then `--memcard-dir` (17073). The default is the exe directory (3462).
- **Memory cards:** `<memcard_dir>/card1.mcd` and `card2.mcd` (memcard.c:136), 128 KiB raw, created if missing, flushed on write and at exit (3961).
- **Savestates:** `<memcard_dir>/<openbios|scph1001>/` (19751-19776). Game options: `<memcard_dir>/<game_id>.options` (17100).
- **config.ini** `[KeyMap]` is read from the exe directory, then the current directory (19738-19749). `input.ini` via `load_input_config` (5101). `bios.cfg` / `disc.cfg` sidecars in the exe directory (2767).

### Recommended seams

The recommended setup: a Release build, the software renderer, and headless-style startup. You need a small patch that splits `main()` into boot (15876 → 19513, without the crash handlers and SDL window/audio/controller code) plus `psx_scheduler_run`. You also need an `add_library` target and `main()` renamed.

- **(a) Step one frame from a host loop.**
  - **Hand-off point:** install your own callback with `gpu_set_vblank_callback(my_vblank)`, or patch the body at the `if (g_headless)` return (main.cpp:8690). By then input, audio, mod frame hooks and overlay ticks have all run.
  - **Two ways to run the core:**
    - Fiber: run `psx_scheduler_run` on a 64 MiB fiber (`psx_fiber_create` / `psx_fiber_switch`, psx_fiber.h) and switch back to StepMania inside the callback. This keeps everything on StepMania's thread; it is safe with the default HLE scheduler and the software renderer.
    - Thread: run it on a dedicated 64 MiB thread and block on a semaphore in the callback.
  - **Pacing:** remove `frame_pacer_wait` (8849) and drive 50 Hz yourself; the DRC bridge absorbs drift.
- **(b) Frame image.** Call `gpu_get_display_info(&di)`. If disabled, show black. If `depth24`, use `depth24_stage_scanout` / `gpu_depth24_present_row` (main.cpp:7484). Otherwise use `gr_render_display(out, pitch, di.display_x, di.display_y, di.width, di.height)` or `gpu_display_pixel_argb`. Under GL instead: `gl_renderer_capture_display_hires` at `gr_scale()`. Upload the result to a StepMania texture and scale to 4:3.
- **(c) Audio samples.** `psx_set_midframe_audio_pump(my_pump)` where `my_pump` does `spu_catch_up(); spu_output_pop(buf, n)` (44.1 kHz S16 stereo, ~882 frames per PAL frame). Feed `rab_push`, and call `rab_pull` from StepMania's DirectSound mixing thread with `target_ms = 50`.
- **(d) Pad words.** At boot: `sio_set_pad_connected(slot, 1)` and `sio_set_pad_analog(slot, 0, 0x80, 0x80, 0x80, 0x80)` for a digital pad. Each frame, after the runtime's own sampling and before the guest resumes: `sio_set_pad_state_slot(slot, ~pressed & 0xFFFF)`. When not headless, `psx_mod_set_controller_source(player, cb)` is the supported route and works without a physical device; the pad transform needs a device on the port.
