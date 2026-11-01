#!/usr/bin/env bash
# Fetch (pinned tags) and cross-build the third-party libraries the 2005 engine needs
# but whose *source* is not in the snapshot (the snapshot only has MSVC-built .libs
# for these, which MinGW cannot link: they need MSVC CRT helpers such as _alldiv,
# _allmul, _allshl, _alloca_probe, _chkstk).
#
# Usage: build-deps-mingw.sh [i686|x86_64] [SRC_DIR] [PREFIX]
#   defaults: x86_64, /home/user/work/builds/windows-build/deps-src,
#             /home/user/work/builds/deps-<arch>
# Produces static libraries + headers in PREFIX, consumed by the engine's CMake via
#   -DSM_DEPS_PREFIX=PREFIX (the 1.0 engine was built against deps-x86_64).
set -euo pipefail
ARCH=${1:-x86_64}
SRC=${2:-/home/user/work/builds/windows-build/deps-src}
PREFIX=${3:-/home/user/work/builds/deps-${ARCH}}
TOOLCHAIN=$(cd "$(dirname "$0")/../.." && pwd)/engine/cmake/toolchains/${ARCH}-w64-mingw32.cmake
BLD=${PREFIX}-build
JOBS=${JOBS:-3}
mkdir -p "$SRC" "$PREFIX" "$BLD"

# name|git url|tag  -- third-party code only (no StepMania code is fetched here).
DEPS=(
  "zlib|https://github.com/madler/zlib.git|v1.3.1"
  "libpng|https://github.com/pnggroup/libpng.git|v1.2.59"
  "libjpeg-turbo|https://github.com/libjpeg-turbo/libjpeg-turbo.git|3.0.4"
  "ogg|https://github.com/xiph/ogg.git|v1.3.5"
  "vorbis|https://github.com/xiph/vorbis.git|v1.3.7"
)
for d in "${DEPS[@]}"; do
  IFS='|' read -r name url tag <<<"$d"
  if [ ! -d "$SRC/$name/.git" ]; then
    git -c advice.detachedHead=false clone --quiet --depth 1 --branch "$tag" "$url" "$SRC/$name"
  fi
  echo "$name $tag $(git -C "$SRC/$name" rev-parse HEAD)" >> "$PREFIX/VERSIONS.txt.new"
done
sort -u "$PREFIX/VERSIONS.txt.new" > "$PREFIX/VERSIONS.txt"; rm -f "$PREFIX/VERSIONS.txt.new"

export SM_DEPS_PREFIX="$PREFIX"
common=(-G Ninja -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_INSTALL_PREFIX="$PREFIX" -DCMAKE_PREFIX_PATH="$PREFIX"
        -DBUILD_SHARED_LIBS=OFF -DCMAKE_POLICY_DEFAULT_CMP0074=NEW
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5)
build() { # name extra-cmake-args...
  local name=$1; shift
  cmake -S "$SRC/$name" -B "$BLD/$name" "${common[@]}" "$@" > "$BLD/$name.configure.log" 2>&1 \
    || { tail -30 "$BLD/$name.configure.log"; exit 1; }
  cmake --build "$BLD/$name" -j"$JOBS" > "$BLD/$name.build.log" 2>&1 \
    || { tail -30 "$BLD/$name.build.log"; exit 1; }
  cmake --install "$BLD/$name" > "$BLD/$name.install.log" 2>&1
  echo "built $name"
}

build zlib -DZLIB_BUILD_EXAMPLES=OFF
# zlib's CMake always builds the DLL too; drop it so everything links statically.
rm -f "$PREFIX/lib/libzlib.dll.a" "$PREFIX/bin/libzlib.dll"
[ -f "$PREFIX/lib/libzlibstatic.a" ] && cp "$PREFIX/lib/libzlibstatic.a" "$PREFIX/lib/libz.a"
# libpng 1.2's CMake adds -DZLIB_DLL for every non-MSVC build, which makes zconf.h
# declare zlib's functions __declspec(dllimport) (unresolvable __imp_inflate etc.
# against the static zlib). CMake puts CMAKE_C_FLAGS after the definitions, so -U wins.
build libpng -DPNG_SHARED=OFF -DPNG_STATIC=ON -DPNG_TESTS=OFF -DCMAKE_C_FLAGS=-UZLIB_DLL \
      -DZLIB_INCLUDE_DIR="$PREFIX/include" -DZLIB_LIBRARY="$PREFIX/lib/libz.a"
build libjpeg-turbo -DENABLE_SHARED=OFF -DENABLE_STATIC=ON -DWITH_TURBOJPEG=OFF -DWITH_JPEG8=OFF -DWITH_JPEG7=OFF
build ogg -DINSTALL_DOCS=OFF -DBUILD_TESTING=OFF
build vorbis -DOGG_ROOT="$PREFIX" -DBUILD_TESTING=OFF
ls -la "$PREFIX/lib"
