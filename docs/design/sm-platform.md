# sm-platform: StepMania 2005 as donor platform layer (keyboard, pads, mapping, window)

Status: IN PROGRESS (resumed run, 2026-10-16). This file is updated as work proceeds.

## Resume inventory (what the interrupted run left)
- /home/user/work/analysis/sm-platform/closure_naive_*.{txt,tsv}: link-time closure of the 15 platform seed objects
  over the full Linux build (/home/user/work/builds/linux-build/obj/src, 384 objects): 216 objects (naive).
- closure_cut1_*: same with GameState/PrefsManager/ThemeManager/... excluded: still 183 objects, because arch.o
  (driver factory for ALL arch drivers), StepMania.o, RageDisplay.o (screenshots, textures) and Preference.o
  (MessageManager/Lua) pull the rest of the engine back in.
- /home/user/work/scripts/sm-platform/link_closure.py (nm-based closure tool).
- Worktree /home/user/work/wt/sm-platform on wip/sm-platform == wip/linux-build (155e7bb), no commits yet.
- No notes, no build dir, no platform/ code.

(sections below are filled in as the work completes)
