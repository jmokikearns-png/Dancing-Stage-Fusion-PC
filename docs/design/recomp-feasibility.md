# recomp-feasibility: running the ORIGINAL Dancing Stage Fusion code natively on PC

Status: IN PROGRESS (resumed run, 2026-10-16). Sections marked TODO are not yet filled.

Acceptance bar: game logic, menus, timing, scoring, presentation and audio must be the
original PS1 game's. StepMania 2005 (CVS 2005-06-19 snapshot) only donates keyboard /
dance-pad input, input mapping + persistence, and window/display management.

## 0. Inputs (verified)
- EXE extracted: /home/user/work/analysis/recomp-feasibility/SLES_041.63
  (lba 24, 311 Form1 sectors, truncated to 636928 bytes; script
  /home/user/work/scripts/recomp-feasibility/extract_exe.py)
  sha256 16a395d7b22bd03c47764a24905166370c7a8e54b3d78ea718316f49d24bdc5c
  PS-X EXE pc0 0x8002397C, gp0 0, t_addr 0x8001A000, t_size 634880 (end 0x800B5000).

## Resume log
- Previous attempt (2026-10-11/12) left: survey/ (capstone pass), psxrecomp-analyze/,
  dsf-project/ (psxrecomp project, 3106 generated functions), build-linux/ (linked
  DSF_feasibility, never run), readdt_leaves_mips_scan.tsv, cloned tools in
  /home/user/work/tools/recomp-src. No notes file existed. This run re-verifies and continues.

## 2b. Independent EXE measurements (verified; capstone pass)
Script /home/user/work/scripts/recomp-feasibility/exe_survey.py (recursive descent from pc0 +
jal targets + data code pointers + prologue seeding; capstone 5.0.7 in venv-disc).
Output /home/user/work/analysis/recomp-feasibility/survey/exe_survey.json + stdout.txt.
- Recovered code: 433,948 bytes, range 0x80023974..0x80091DA8 (of 634,880 text bytes; the
  rest is data/rodata/tables). ~1,476 function entries reached (psxrecomp's own analyser:
  1,830 functions, 70.8% of image).
- 4,049 jal call sites, 1,167 distinct jal targets; 164 jalr sites (function pointers);
  140 non-return jr sites, 83 recovered jump tables (1,562 entries), 2 unresolved jr.
- GTE/COP2: 353 instructions (mtc2 91, ctc2 85, mfc2 59, swc2 32, lwc2 29, cfc2 26, MVMVA 12,
  AVSZ3 4, NCLIP 3, RTPT 2, RTPS 1, NCCT 2, GPF 2, GPL 1, OP 2, SQR 1, AVSZ4 1). Mostly PsyQ
  LIBGTE/LIBGS; the game is predominantly 2D but does use 3D primitives (background/stage
  effects).
- COP0: 1 mfc0, 1 mtc0. syscall instructions: 2 (0x8003D7BC, 0x8003D7CC = PsyQ
  Enter/ExitCriticalSection). 55 BIOS A0/B0/C0 call thunks (e.g. A0:70/AB/AC/AD,
  B0:4A..50/5C/3F...).
- Load-delay hazards (next insn reads a just-loaded GPR): 0 found. COP mfc/cfc delay hazards: 0.
  Stores into code addresses (lui-based): 0. => no self-modifying code in the main EXE seen.
- 18 jal targets OUTSIDE the EXE: 0x800E8000, 0x800E80C8, 0x800E82B8, 0x800E8300,
  0x800E9504 and 0x801C403C..0x801C6180 => code is loaded at runtime into two overlay regions.

## 2c. Runtime-loaded code (overlays) (verified)
MIPS code scan over all READ_DT.BIN leaves (from the disc-readdt workflow's extraction,
/home/user/work/analysis/disc-readdt/leaves/) -> readdt_leaves_mips_scan.tsv. Entries 226..234
(READ_DT lba 28331..28561, raw "data" leaves) are code. Link base inferred by scoring jal
targets that hit prologues (scripts overlay_base_infer.py, overlay_xcheck.py):

| entry | size | link base | jal -> main-EXE function entries | GTE ops | PsyQ content (sig scan) |
|---|---|---|---|---|---|
| e226 | 262,232 | 0x800E8000 | 100 (mixed code+data) | 160 | LIBSND vibrato helpers |
| e227 | 38,408 | 0x800E8000 | 219 | 0 | LIBSND vibrato helpers |
| e228 | 43,196 | 0x800E8000 | 267 | 2 | LIBSND vibrato helpers |
| e229 | 57,884 | 0x800E8000 | 181 | 0 | GsGetWorkBase |
| e230 | 18,204 | 0x801C4000 | 27 | 6 | - |
| e233 | 24,156 | 0x801C4000 | 82 | 20 | LIBPRESS (DecDCTin/out, vlc) = MDEC movie player |
| e234 | 23,128 | 0x801C4000 | 130 | 0 | - |
| e231, e232 | 17,336 / 996 | ? | 0 | 0 | not code (or not MIPS) |

=> 4 overlays share region A (0x800E8000) and 3 share region B (0x801C4000). All are static
bytes on disc (no evidence of per-load relocation yet), so they can be recompiled ahead of
time, with a "which overlay is resident" guard (CTRStaticRecomp and psxrecomp both do this).

## 3. PsyQ SDK (verified, independent signature scan)
Script /home/user/work/scripts/recomp-feasibility/psyq_sigscan.py with lab313ru
psx_psyq_signatures (all versions); outputs /home/user/work/analysis/recomp-feasibility/psyq/.
- Version: PsyQ 4.4 (LIBAPI C114.OBJ and LIBC A63.OBJ only match the 440 set; 440 has the most
  labels: 2,452). Agrees with the parallel disc-exe workflow (psyq_summary.txt: 197 objects
  match 440, 1 differs). Banner strings: "Library Programs (c) 1993-1997 Sony Computer
  Entertainment Inc.", "$Id: intr.c,v 1.75 1997/02/07", "$Id: sys.c,v 1.140 1998/01/12",
  "$Id: bios.c,v 1.86 1997/03/28".
- Libraries linked in the EXE (public functions found):
  - LIBCD: CdInit, CdControl/F/B, CdSync, CdReady, CdRead2, CdGetSector(2), callbacks,
    CdIntToPos/CdPosToInt, CdMix, streaming St* (StSetRing, StSetStream, StGetNext, StFreeRing,
    StCdInterrupt...) => streaming reads (XA audio + movie data).
  - LIBSPU (SpuInit/Malloc/SetVoiceAttr/SetKey/Reverb/Transfer/IRQ...) + LIBSND (SsInit,
    SsSeq*, SsVab*, SsUtKeyOnV/OffV, reverb, tick mode) => SEQ/VAB sound via libsnd.
  - LIBGPU (ResetGraph, DrawOTag(Env), LoadImage/StoreImage/MoveImage, PutDrawEnv/DispEnv...),
    LIBGS (GsInitGraph, GsSortClear, Gs*Coord*, light/fog, GsLinkObject5, HMD view units),
    LIBGTE (InitGeom, Rot/Trans matrices, RotAverageNclip4, VectorNormal, ratan2, csqrt...).
  - LIBETC: VSync, VSyncCallback(s), DMACallback, InterruptCallback, SetVideoMode.
  - LIBAPI: events, root counters (SetRCnt/GetRCnt/StartRCnt), HookEntryInt, file I/O
    (open/read/lseek...), InitPAD/StartPAD (BIOS pad), _bu_init.
  - LIBPAD (PadInitDirect, PadGetState, PadInfoMode, PadSetAct...: the newer direct pad lib),
    LIBCARD (InitCARD/StartCARD/_card_*), LIBC/LIBC2, LIBMATH (soft float __floatsisf etc.),
    LIBSN (PCopen... PC file server debug stubs, unused on retail), 2MBYTE.OBJ startup.
  - In overlay e233: LIBPRESS (DecDCTin/DecDCTout/DecDCTvlc2...) => MDEC hardware movies.

## 1. Tool survey (partially filled)
See table below; licenses read from the cloned repos (/home/user/work/tools/recomp-src).

| tool | URL | last commit (git log) | license (file read) |
|---|---|---|---|
| PSXRecomp | https://github.com/mstan/psxrecomp (now RetroPortingToolKit) | 2026-10-11, 3,063 commits | PolyForm Noncommercial 1.0.0 |
| RecompOne | https://github.com/BlackLabelHQ/RecompOne | 2026-10-08, 141 commits | MIT (C# output, .NET 10) |
| CTRStaticRecomp | https://github.com/LorenzoAlfaro/CTRStaticRecomp | 2026-10-07 | MIT (own Python recompiler + C++ runtime, Win64 only) |
| N64Recomp | https://github.com/N64Recomp/N64Recomp | 2026-05-27 | MIT (N64 only; design ancestor) |
| splat | https://github.com/ethteck/splat | 2026-10-10 | MIT |
| m2c | https://github.com/matt-kempster/m2c | 2026-09-16 | GPL-3.0 |
| maspsx | https://github.com/mkst/maspsx | 2026-10-02 | MIT |
| psx_psyq_signatures | https://github.com/lab313ru/psx_psyq_signatures | 2022-01-09 | NO license file |
| ghidra_psx_ldr | https://github.com/lab313ru/ghidra_psx_ldr | 2026-10-04 | NO license file |
| decomp.me | https://github.com/decompme/decomp.me | 2026-10-10 | MIT |

TODO: 2 hands-on run results, 4 recommendation, emulator licenses.

## 2a. HANDS-ON: PSXRecomp on SLES_041.63 (verified, 2026-10-16)
Framework: https://github.com/mstan/psxrecomp @ 2a4824fc (2026-10-11), cloned at
/home/user/work/tools/recomp-src/psxrecomp. Game project (made by the earlier attempt with the
framework's tools/new_project_layout/probe_disc.py): /home/user/work/analysis/recomp-feasibility/dsf-project
(game.toml, seeds/ghidra_funcs.txt = 1,160 jal-target seeds, generated/ = 42 C shards,
1,612,866 lines, 3,106 functions, 3,105-entry dispatch table, generated in 2.5 s).
Build: /home/user/work/analysis/recomp-feasibility/build-linux (CMake+Ninja, gcc 13.3, SDL2
2.30, OpenGL renderer, OpenBIOS (MIT, from PCSX-Redux) recompiled as the LLE kernel; ~5 min -j3).
Rebuilt today with -DPSX_DEBUG_TOOLS=ON (TCP debug server); Release binary kept as
runs/DSF_feasibility.release. Zero game-specific code was written.

Run: `./DSF_feasibility --headless --debug-port 4370` (log runs/run2.log), driven by
/home/user/work/scripts/recomp-feasibility/drive.py (press/screenshot over the debug port).
Screenshots in build-linux/runs/shots/ (disc-derived; stay in analysis/):
- r2_a: language select (EN/FR/DE/IT/ES) -> Cross -> "No system data. Create?" (r2_b..d)
  -> YES -> "Saving... Do not remove MEMORY CARD" (r2_e; saves/card1.mcd written) ->
  AUTO SAVE prompt (r2_f..h) -> Caution screen (r2_i) -> title "(c)1998 2004 KONAMI" (r2_j)
  -> How To Play attract with 3D dancer (r2_k..n) -> DEMO MODE gameplay "Come Into My World"
  (r2_o,p) -> Start -> main menu GAME/DOUBLE/WORKOUT/LESSON/RECORDS/OPTIONS (r3_b,c) ->
  Game Mode -> song select "321STARS / DJ SIMON", difficulty list (r3_e) -> 1st STAGE
  "HERE WE GO", arrows scrolling, BOO judgements (no input), fail -> back to menu (r4_a..f).
- Audio: audio_stats taps spu_out (3.4 M audible frames, peak 27044) and cd_in (XA,
  1.6 M audible frames); 10 s spu_out WAV runs/audio/r5_spu_out.wav mean -17.1 dB,
  max -0.8 dB, spectrogram is music (song preview on select screen).
- Interpreter use: overlay code at 0x800E8000 and 0x801C4000 runs in psxrecomp's MIPS
  interpreter (dirty_ram_stats: 20.0 M interpreted insns over ~26k frames; hot ranges
  0x801C6xxx, 0x800E9xxx) because this build has no AOT overlay shards and no bundled tcc
  toolchain ("overlay gaps -> interpreter"). Also kernel-RAM code at 0x0000Dxxx/0x00002xxx
  (OpenBIOS/PsyQ-patched handlers). Runtime reports 2 "diverged" EXE text pages around
  0x8003D184 (PsyQ LIBCARD _patch_card2) - cause not yet analysed (uncertain).
- Headless pacing: ~70-100 guest frames per wall second (unthrottled headless); guest
  video standard correctly detected PAL 50 Hz.
- Caveats: "DEMO MODE ... GAME OVER" text seen in attract demo - not yet compared against a
  reference emulator (uncertain whether identical to real hardware).
