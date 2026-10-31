#!/usr/bin/env bash
# The song tools of the release (songtools/): what "Build New Songs.bat" runs
# to build the new songs from the player's simfile packs, with nothing to
# install - a Python with numpy and Pillow, ffmpeg, and the song builder.
#   make_songtools.sh OUT_DIR
# Env: PYWIN (conda-forge's Windows python 3.12 package and its
#      dependencies, unpacked), WHEELS (numpy 2.0.x and Pillow win_amd64
#      wheels for cp312; numpy 2.0 is the version tested), FFWHEEL (imageio-ffmpeg's win_amd64 wheel: a
#      static ffmpeg), REPO, DSF_TC.
set -euo pipefail
O=$1
W=/home/user/work
REPO=${REPO:-/home/user/dancing-stage-fusion-pc}
PYWIN=${PYWIN:-$W/tc/pywin}
WHEELS=${WHEELS:-$W/tc/wheels-songtools}
FFWHEEL=${FFWHEEL:-$W/tc/wheels/imageio_ffmpeg-0.7.0-py3-none-win_amd64.whl}
TC=${DSF_TC:-$W/tc/linux}
rm -rf "${O:?}"; mkdir -p "$O/python" "$O/extrasong" "$O/licenses"

cp -r "$PYWIN/." "$O/python/"
P=$O/python
for w in "$WHEELS"/*.whl; do unzip -qo "$w" -d "$P/Lib/site-packages"; done
# Only what running a script needs.
find "$P" -name '*.pdb' -delete
find "$P" -name '__pycache__' -type d -prune -exec rm -rf {} +
rm -rf "${P:?}/include" "$P/libs" "$P/share" "$P/Tools" "$P/Scripts" "$P/etc" "$P/conda-meta" "$P/info" \
       "$P/Library/include" "$P/Library/lib" "$P/Library/share" "$P/Library/bin/openssl.exe" \
       "$P/Lib/test" "$P/Lib/idlelib" "$P/Lib/tkinter" "$P/Lib/turtledemo" "$P/Lib/ensurepip" \
       "$P/Lib/lib2to3" "$P/Lib/pydoc_data" "$P/Lib/venv" \
       "$P/Lib/site-packages/numpy/"*/tests "$P/Lib/site-packages/numpy/tests" "$P/Lib/site-packages/numpy/f2py"

T=$O/extrasong
cp "$REPO/tools/extrasong/build_extrasong.py" "$REPO/tools/extrasong/konami_lz.py" "$REPO/tools/extrasong/plate_font.py" \
   "$REPO/tools/extrasong/backgrounds.py" "$REPO/tools/extrasong/banner_lettering.py" "$T/"
cp -r "$REPO/tools/extrasong/art" "$T/art"	# our backgrounds, lettered at build time from the player's banners
X=$(mktemp -d); trap 'rm -rf "${X:?}"' EXIT
unzip -qo "$FFWHEEL" 'imageio_ffmpeg/binaries/*' -d "$X"
cp "$X"/imageio_ffmpeg/binaries/ffmpeg-*.exe "$T/ffmpeg.exe"
"$TC/bin/x86_64-w64-mingw32-gcc" -O2 -static -s -o "$T/psxaudio.exe" "$REPO/tools/extrasong/psxaudio.c"

L=$O/licenses
cp "$P/LICENSE_PYTHON.txt" "$L/Python-LICENSE.txt"
cp "$P"/Lib/site-packages/numpy-*.dist-info/LICENSE.txt "$L/numpy-LICENSE.txt"
cp "$P"/Lib/site-packages/pillow-*.dist-info/LICENSE "$L/Pillow-LICENSE.txt" 2>/dev/null || \
  cp "$P"/Lib/site-packages/pillow-*.dist-info/licenses/LICENSE "$L/Pillow-LICENSE.txt"
cat > "$O/README.txt" <<'EOF'
The song tools: what "Build New Songs.bat" runs.

  python\     Python 3.12 (conda-forge's Windows build) with numpy and Pillow
  extrasong\  the song builder (this project's tools/extrasong), ffmpeg.exe
              and psxaudio.exe (the PlayStation audio encoder, built from
              tools/extrasong/psxaudio.c)

Licences: Python (PSF licence), numpy (BSD-3-Clause; its OpenBLAS: BSD),
Pillow (MIT-CMU), see licenses. ffmpeg.exe is the static Windows build of
FFmpeg 7.1 shipped by imageio-ffmpeg (https://github.com/imageio/imageio-ffmpeg),
under the GNU GPL; its source: https://ffmpeg.org/download.html
EOF
du -sh "$O"
