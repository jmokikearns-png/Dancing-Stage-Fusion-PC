#!/usr/bin/env bash
# The public release: the program, its data, Setup.exe and setup/ (the
# recompiler, the prebuilt rest of the game core, the compiler).  Nothing
# from the game: Setup.exe builds dsfcore.dll from the player's own disc.
#   make_release.sh VERSION
# Env: CB (the UCRT dsfcore build, for the kit), REPO (this repository),
#      COMPILER (the trimmed Windows GCC, make_compiler.sh),
#      RECOMP (psxrecomp-game.exe built for Windows).
set -euo pipefail
W=/home/user/work
V=${1:?usage: make_release.sh VERSION}
NAME=DancingStageFusionPC-$V
REPO=${REPO:-/home/user/dancing-stage-fusion-pc}
CB=${CB:-$W/analysis/recomp-feasibility/build-win64-ucrt}
COMPILER=${COMPILER:-$W/builds/compiler-win64}
RECOMP=${RECOMP:-$W/builds/recompiler-win64/psxrecomp-game.exe}
TC=${DSF_TC:-$W/tc/linux}
OUT=$W/builds/package
D=$OUT/$NAME

# The program and its data, without a game core, with our own title logo.
env -u LOGO -u LOGO_FROM NO_CORE=1 NO_ZIP=1 CB="$CB" README="$REPO/docs/release/README.txt" \
  bash "$REPO/scripts/dsfpc/package.sh" "$NAME"
rm -f "$D/dsfcore.dll"

S=$D/setup
mkdir -p "$S/recomp/bios" "$S/icon"
cp "$RECOMP" "$S/psxrecomp-game.exe"
cp "$REPO/game-project/game.toml" "$S/recomp/game.toml"
cp "$(sed -n "s/^PSXRECOMP_ROOT:PATH=//p" "$CB/CMakeCache.txt")/bios/SCPH1001.toml" "$S/recomp/bios/"
bash "$REPO/scripts/dsfpc/make_kit.sh" "$CB" "$S/kit"
cp -r "$COMPILER" "$S/compiler"
python3 "$REPO/tools/setup/make_badge.py" "$S/icon/badge.rgba"
"$TC/bin/x86_64-w64-mingw32-gcc" -O2 -Wall -Wextra -Wno-format-truncation -static -s \
  -o "$D/Setup.exe" "$REPO/tools/setup/setup.c"
cp "$COMPILER/README.txt" "$D/licenses/GCC-and-Binutils-README.txt"

# The new songs: "Build New Songs.bat" builds them from the player's
# simfile packs in simfiles\ with songtools\ (Python, ffmpeg, the builder).
# (Not "songs": StepMania would load a Songs folder's simfiles itself.)
bash "$REPO/scripts/dsfpc/make_songtools.sh" "$D/songtools"
cp "$REPO/tools/setup/Build New Songs.bat" "$D/"
mkdir -p "$D/simfiles"
sed 's/$/\r/' "$REPO/tools/extrasong/songs.toml" > "$D/simfiles/songs.toml"
{
  printf 'Put your simfile packs of the PlayStation 2 and arcade versions of\n'
  printf 'Dancing Stage Fusion in this folder, whole, as they come (unzipped).\n'
  printf '"Build New Songs.bat" finds the songs it needs in them by title:\n\n'
  sed -n 's/^title = "\(.*\)"$/  \1/p' "$REPO/tools/extrasong/songs.toml"
  printf '\nThen run "Build New Songs.bat" in the game folder.\n'
} | sed 's/$/\r/' > "$D/simfiles/PUT_YOUR_SIMFILES_HERE.txt"

# Nothing from the game, the player's disc or a local setup in the release.
bad=$(cd "$D" && find . \( \( -iname '*.bin' -not -path './bios/openbios.bin' \) -o -iname '*.cue' -o -iname '*.mcd' -o -name 'dsfcore.dll' \
        -o -name 'StepMania.ini' -o -name 'Keymaps.ini' -o -name 'overlay_captures.json*' \
        -o -path './setup/work*' -o -path './mods/installed*' -o -name '*.log' \
        -o -name '_title logo dsf*' -o -path './Themes*' \) -print)
if [ -n "$bad" ]; then echo "ERROR: not for the release:"; echo "$bad"; exit 1; fi
if grep -rlaE 'SLES_041\.63_(full|dispatch)' "$S/kit" >/dev/null; then
  echo "ERROR: game code in the kit" >&2; exit 1
fi

mkdir -p "$W/deliverables"; rm -f "$W/deliverables/$NAME.zip"
(cd "$OUT" && zip -q -r -9 "$W/deliverables/$NAME.zip" "$NAME")
ls -la "$W/deliverables/$NAME.zip"
