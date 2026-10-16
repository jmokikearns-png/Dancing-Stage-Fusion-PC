#!/bin/bash
# Cross-build Dancing Stage Fusion PC for Windows x64 (MinGW-w64, posix threads) and package it.
# Requires: the PSXRecomp project in analysis/recomp-feasibility/dsf-project (generated from the user's disc),
#           SDL3-3.4.10.tar.gz vendored in tools/recomp-src/psxrecomp/third_party, libwebp in tools/recomp-src/deps.
# Output: analysis/recomp-feasibility/package/<NAME>/ and deliverables/<NAME>.zip (personal use only; no disc data).
set -euo pipefail
W=/home/user/work
NAME=${1:-DancingStageFusionPC-win64-test2}
TITLE=${2:-test build 2}
SRC=$W/analysis/recomp-feasibility/dsf-project
B=$W/analysis/recomp-feasibility/build-win64
P=$W/tools/recomp-src/psxrecomp
D=$W/analysis/recomp-feasibility/package/$NAME

cmake -S "$SRC" -B "$B" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=$W/scripts/win/toolchain-mingw-w64-posix.cmake \
  -DCMAKE_BUILD_TYPE=Release -DPSX_DEBUG_TOOLS=OFF -DPSX_ENABLE_VULKAN=OFF -DPSX_RECOMP_UI=OFF \
  -DFETCHCONTENT_SOURCE_DIR_PSX_LIBWEBP=$W/tools/recomp-src/deps/libwebp
cmake --build "$B" -j3

# Controller setup tool (same StepMania input library the game links).
TOOL_OUT=$W/builds/controller-setup/DSF-ControllerSetup.exe
mkdir -p "$(dirname "$TOOL_OUT")"
x86_64-w64-mingw32-g++-posix -std=c++17 -O2 -Wall -mwindows \
  -I"$W/wt/sm-platform/platform/include" "$W/port/tools/controller_setup/controller_setup.cpp" \
  "$B/dsfplatform/libdsfplatform.a" -ldinput -ldxguid -ldxerr8 -lwinmm -lgdi32 -luser32 -ladvapi32 -lole32 \
  -static -static-libgcc -static-libstdc++ -o "$TOOL_OUT"

rm -rf "${D:?}"; mkdir -p "$D/licenses" "$D/disc"
cp "$B/DSF_feasibility.exe" "$D/DancingStageFusionPC.exe"
cp "$B/DSF_feasibility.execution.json" "$D/DancingStageFusionPC.execution.json"
cp -r "$B/bios" "$B/assets" "$B/mods" "$D/"
cp "$B/config.ini" "$D/config.ini"
cp "$TOOL_OUT" "$D/DSF-ControllerSetup.exe"
python3 - "$B/game.toml" "$D/game.toml" "$TITLE" <<'PY'
import sys
s = open(sys.argv[1]).read()
out = []
for line in s.splitlines():
    if line.startswith('disc = '):
        line = 'disc = "disc/Dancing Stage Fusion (Europe) (En,Fr,De,Es,It).cue"'
    elif line.startswith('window_title = '):
        line = 'window_title = "Dancing Stage Fusion PC (%s)"' % sys.argv[3]
    out.append(line)
    if line == '[video]':
        out.append('# native = exact original pixels; 720p | 1080p | 1440p | 4k | 5k | 8k | display (match your monitor)')
        out.append('internal_resolution = "1080p"')
open(sys.argv[2], 'w').write('\n'.join(out) + '\n')
PY
cp "$P"/runtime/licenses/*.txt "$D/licenses/"
cp "$P/LICENSE" "$D/licenses/PSXRecomp-LICENSE-PolyForm-Noncommercial-1.0.0.txt"
cp "$P/THIRD_PARTY_ATTRIBUTION.md" "$D/licenses/PSXRecomp-THIRD_PARTY_ATTRIBUTION.md"
cp "$B/_deps/sdl3-src/LICENSE.txt" "$D/licenses/SDL3-LICENSE.txt"
cp "$B/_deps/psx_zlib-src/LICENSE" "$D/licenses/zlib-LICENSE.txt"
cp "$B/bios/OpenBIOS.LICENSE" "$D/licenses/OpenBIOS-LICENSE.txt"
cp "$W/engine/Docs/Copying.txt" "$D/licenses/StepMania-LICENSE-MIT.txt"
sed 's/$/\r/' "$W/scripts/win/README-player.txt" > "$D/README.txt"
printf 'Put your disc image here:\r\n  Dancing Stage Fusion (Europe) (En,Fr,De,Es,It).bin\r\n  Dancing Stage Fusion (Europe) (En,Fr,De,Es,It).cue\r\n(Your own Redump-verified dump of SLES-04163. Never share it.)\r\n' > "$D/disc/PUT_YOUR_DISC_HERE.txt"
mkdir -p "$W/deliverables"; rm -f "$W/deliverables/$NAME.zip"
(cd "$D/.." && zip -q -r -9 "$W/deliverables/$NAME.zip" "$NAME")
if unzip -l "$W/deliverables/$NAME.zip" | grep -E '\.(cue|mcd)$|Dancing Stage Fusion \(Europe\).*\.bin'; then echo "ERROR: disc data in package" >&2; exit 1; fi
echo "packaged: $W/deliverables/$NAME.zip"
