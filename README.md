# Dancing Stage Fusion PC — port sources

Our own code for the native PC port. It contains **no disc-derived data**. The recompiled game C is generated
from the user's disc and lives outside this repository (`analysis/recomp-feasibility/dsf-project/generated`).

| Path | What |
|---|---|
| `plugins/dsf_input/` | PSXRecomp plugin: both PS1 controller ports driven by StepMania 2005 input (dsfplatform, DirectInput); hot-reloads `config/Data/Keymaps.ini`; F1 opens the setup tool |
| `mods/preloaded/packages/dsf.input.stepmania/` | mod package that enables the plugin by default |
| `tools/controller_setup/` | DSF-ControllerSetup.exe: press-to-bind UI for keyboard, dance pads and joysticks (Player 1/2 × 3 boxes) |
| `config/config.ini` | runtime host hotkeys: turbo, rewind, save states and volume keys off; fullscreen kept |
| `game-project/` | PSXRecomp game project files (`CMakeLists.txt`, `game.toml`: PAL, 1080p internal resolution, `[audio] buffer_ms = 50`) |
| `scripts/build_dsf_win64.sh` | MinGW-w64 cross-build and packaging of the Windows release |

The StepMania input library lives in the engine repo, branch `wip/sm-platform` (`platform/`, built with `-DDSF_INPUT_ONLY=ON`).
