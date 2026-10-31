#!/usr/bin/env bash
# The setup kit: everything in dsfcore.dll except the code recompiled from the
# player's disc, prebuilt, so Setup.exe only has to recompile the game
# program and compile and link that part (setup/kit in the release).
#   make_kit.sh BUILD_DIR OUT_DIR
# BUILD_DIR: a dsfcore build (DSF_EMBED=ON) made with the conda-forge UCRT
# toolchain (scripts/win/toolchain-conda-ucrt.cmake), the same compiler the
# release ships for the player.
set -euo pipefail
CB=$(cd "$1" && pwd); K=$2
TC=${DSF_TC:-/home/user/work/tc/linux}
AR=$TC/bin/x86_64-w64-mingw32-ar
rm -rf "${K:?}"; mkdir -p "$K/lib" "$K/include"
cd "$CB"
LINK=$(awk '/^build dsfcore.dll libdsfcore.dll.a:/{p=1} p&&/^$/{exit} p' build.ninja)
OBJS=$(head -1 <<<"$LINK" | tr ' ' '\n' | grep -E '\.(obj|res)$')
# The game's own code: compiled by the player from their disc, never shipped.
GAME=$(grep -E '^CMakeFiles/psx-runtime.dir/generated/' <<<"$OBJS")
KIT=$(grep -vE '^CMakeFiles/psx-runtime.dir/generated/|\.res$' <<<"$OBJS")
[ "$(wc -l <<<"$GAME")" -ge 40 ] || { echo "make_kit: no game objects in $CB" >&2; exit 1; }
"$AR" rcs "$K/lib/libdsfcore_kit.a" $KIT
LIBS=$(grep '^  LINK_LIBRARIES = ' <<<"$LINK" | sed 's/^  LINK_LIBRARIES = //')
FLAGS=$(grep '^  LINK_FLAGS = ' <<<"$LINK" | sed 's/^  LINK_FLAGS = //')
SYS=""
for l in $LIBS; do
  case $l in
    *.a) cp "$l" "$K/lib/"; SYS="$SYS lib/$(basename "$l")";;
    *) SYS="$SYS $l";;
  esac
done
# Link: gcc -shared -o dsfcore.dll <game objects> @kit/link.rsp, run in kit/.
{
  echo "-shared $FLAGS -Wl,--image-base,0x2c0000000 -Wl,--no-insert-timestamp"
  echo "-Wl,--whole-archive lib/libdsfcore_kit.a -Wl,--no-whole-archive"
  echo "-Wl,--start-group$SYS -Wl,--end-group"
} > "$K/link.rsp"

# Compile: the generated files' own flags and defines.  They include only
# psx_runtime.h and what it includes (checked below with -M).
REC=$(grep -A6 '^build CMakeFiles/psx-runtime.dir/generated/[^ ]*_dispatch.c.obj:' build.ninja)
DEF=$(grep '^  DEFINES = ' <<<"$REC" | sed 's/^  DEFINES = //; s/-DPSX_EXECUTION_PROFILE_CONFIG=[^ ]*//')
CFL=$(grep '^  FLAGS = ' <<<"$REC" | sed 's/^  FLAGS = //')
INC=$(grep '^  INCLUDES = ' <<<"$REC" | sed 's/^  INCLUDES = //')
SRC=$(head -1 <<<"$REC" | grep -o ' /[^ ]*_dispatch\.c' | tr -d ' ')
# DEFINES is shell-quoted (ninja runs it through sh): parse it the same way.
eval "ARGS=($CFL $DEF)"
HDRS=$("$TC/bin/x86_64-w64-mingw32-gcc" -M "${ARGS[@]}" $INC "$SRC" | tr ' \\' '\n\n' | grep '\.h$' | grep -v "^$TC/" | grep -v '/generated/')
for h in $HDRS; do cp "$h" "$K/include/"; done
# One argument per line, single-quoted for gcc's @file parser.
{ for a in "${ARGS[@]}" -Iinclude; do printf "'%s'\n" "$a"; done; } > "$K/compile.rsp"
cp "$CB/psx_execution/psx-runtime/profile.h" "$K/include/psx_execution_profile.h"
echo "kit: $(wc -w <<<"$KIT") objects, $(ls "$K/lib" | wc -l) archives, $(wc -w <<<"$HDRS") headers, $(wc -l <<<"$GAME") game files to compile"
