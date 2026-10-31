#!/usr/bin/env bash
# Package the StepMania-hosted port for Windows x64 (personal use: dsfcore.dll
# is built from the player's own disc).  Usage: package.sh [NAME]
set -euo pipefail
W=/home/user/work
NAME=${1:-DancingStageFusionPC-win64-test4}
E=$W/wt/dsfpc                                        # engine (branch wip/dsfpc)
SMB=$W/builds/dsfpc-win64                            # Release engine build
CB=$W/analysis/recomp-feasibility/build-win64-core   # dsfcore.dll (DSF_EMBED=ON)
P=$W/tools/recomp-src/psxrecomp
DEPS=$W/builds/windows-build/deps-src
OUT=$W/builds/package
D=$OUT/$NAME

cmake --build "$SMB" -j4 >/dev/null
cmake --build "$CB" -j4 >/dev/null

rm -rf "${D:?}"; mkdir -p "$D/licenses" "$D/disc" "$D/saves"
cp "$SMB/StepMania.exe" "$D/DancingStageFusionPC.exe"
for d in Announcers BGAnimations BackgroundEffects BackgroundTransitions CDTitles \
         Characters Courses Data NoteSkins RandomMovies Songs Themes; do
  cp -r "$E/$d" "$D/"
done
find "$D" -name CVS -type d -prune -exec rm -rf {} +
# The title logo: the player's own copy of the game's logo, rendered into this
# personal package only (never into a repository); without one, our own.
if [ -n "${LOGO:-}" ]; then
  LOGO=$LOGO ESRGAN_MODEL=${ESRGAN_MODEL:-} "$W/port/graphics/theme/build_theme_art.sh" "$D" >/dev/null
fi
# Or the title-logo files of an earlier personal package (LOGO_FROM = its
# Themes/DSF PC/Graphics folder).
if [ -n "${LOGO_FROM:-}" ]; then
  G="$D/Themes/DSF PC/Graphics"
  cp "$LOGO_FROM/ScreenTitleMenu logo.xml" "$LOGO_FROM/_title logo dsf (res 374x176).png" \
     "$LOGO_FROM/_title logo pc (res 126x104).png" "$G/"
  rm -f "$G/_title logo (res 640x120).png"
fi
cp "$CB/dsfcore.dll" "$D/"
cp -r "$CB/bios" "$CB/assets" "$CB/mods" "$D/"
python3 - "$CB/game.toml" "$D/game.toml" <<'PY'
import sys
s = open(sys.argv[1]).read()
out = []
for line in s.splitlines():
    if line.startswith('disc = '):
        line = 'disc = "disc/Dancing Stage Fusion (Europe) (En,Fr,De,Es,It).cue"'
    elif line.startswith('window_title = '):
        line = 'window_title = "Dancing Stage Fusion PC"'
    out.append(line)
open(sys.argv[2], 'w').write('\n'.join(out) + '\n')
PY

L=$D/licenses
cp "$E/Docs/Copying.txt" "$L/StepMania-LICENSE-MIT.txt"
cp "$E/src/libresample/LICENSE.txt" "$L/libresample-LICENSE-LGPL-2.1.txt"
cp "$E/src/lua-5.0/COPYRIGHT" "$L/Lua-5.0-COPYRIGHT.txt"
cp "$E/src/pcre/COPYING" "$L/PCRE-COPYING.txt"
cp "$E/src/crypto51/License.txt" "$L/CryptoPP-5.1-License.txt"
cp "$P/LICENSE" "$L/PSXRecomp-LICENSE-PolyForm-Noncommercial-1.0.0.txt"
cp "$P/THIRD_PARTY_ATTRIBUTION.md" "$L/PSXRecomp-THIRD_PARTY_ATTRIBUTION.md"
cp "$P"/runtime/licenses/*.txt "$L/"
cp "$CB/_deps/sdl3-src/LICENSE.txt" "$L/SDL3-LICENSE.txt"
cp "$CB/bios/OpenBIOS.LICENSE" "$L/OpenBIOS-LICENSE.txt"
cp "$DEPS/zlib/LICENSE" "$L/zlib-LICENSE.txt"
cp "$DEPS/libpng/LICENSE" "$L/libpng-LICENSE.txt"
cp "$DEPS/libjpeg-turbo/LICENSE.md" "$L/libjpeg-turbo-LICENSE.md"
cp "$DEPS/ogg/COPYING" "$L/Ogg-COPYING.txt"
cp "$DEPS/vorbis/COPYING" "$L/Vorbis-COPYING.txt"
for f in exo-2:Exo2 inter:Inter lobster:Lobster; do
  cp "$W/port/graphics/node_modules/@fontsource/${f%%:*}/LICENSE" "$L/${f##*:}-OFL-1.1.txt"
done

sed 's/$/\r/' "${README:-$W/scripts/dsfpc/README-player-test4.txt}" > "$D/README.txt"
printf 'Put your disc image here:\r\n  Dancing Stage Fusion (Europe) (En,Fr,De,Es,It).bin\r\n  Dancing Stage Fusion (Europe) (En,Fr,De,Es,It).cue\r\n(Your own Redump-verified dump of SLES-04163. Never share it.)\r\n' > "$D/disc/PUT_YOUR_DISC_HERE.txt"
printf 'Memory cards (card1.mcd, card2.mcd) are kept here.\r\n' > "$D/saves/README.txt"

# StepMania's own data folders (themes, noteskins and the ones it expects but
# the game never uses) go into one archive the program mounts at start
# (StepMania.cpp, DSF_DATA_FILE); Data stays a folder (settings, logs, cache).
DATA_DIRS="Announcers BGAnimations BackgroundEffects BackgroundTransitions CDTitles Characters Courses NoteSkins RandomMovies Songs Themes"
(cd "$D" && zip -q -r -0 DancingStageFusionPC.dat $DATA_DIRS && rm -rf $DATA_DIRS)
mkdir -p "$W/deliverables"; rm -f "$W/deliverables/$NAME.zip"
(cd "$OUT" && zip -q -r -9 "$W/deliverables/$NAME.zip" "$NAME")
# Never ship disc data, memory cards or local settings.
if unzip -l "$W/deliverables/$NAME.zip" | grep -E -i 'Dancing Stage Fusion.*\.(bin|cue|img|iso)$|\.mcd$|StepMania\.ini|Keymaps\.ini|Static\.ini'; then
  echo "ERROR: disc data, saves or local settings in the package" >&2; exit 1
fi
ls -la "$W/deliverables/$NAME.zip"
