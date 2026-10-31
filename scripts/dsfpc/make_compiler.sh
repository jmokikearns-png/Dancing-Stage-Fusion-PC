#!/usr/bin/env bash
# The compiler Setup.exe runs on the player's PC (setup/compiler in the
# release): conda-forge's Windows build of GCC 16 for x86_64-w64-mingw32
# (UCRT, mingw-w64 12), trimmed to what compiling the recompiled game code
# and linking dsfcore.dll needs.
#   make_compiler.sh CONDA_LIBRARY_DIR OUT_DIR
# CONDA_LIBRARY_DIR: the Library folder of the unpacked conda-forge packages
# gcc_impl_win-64, gxx_impl_win-64, binutils_impl_win-64 (bootstrap build),
# ld_impl_win-64, libgcc-devel_win-64, libstdcxx-devel_win-64 and the
# mingw-w64-ucrt-x86_64 crt/headers/winpthreads packages.
set -euo pipefail
S=$(cd "$1" && pwd); O=$2
T=x86_64-w64-mingw32; V=16.2.0
rm -rf "${O:?}"; mkdir -p "$O/bin" "$O/libexec/gcc/$T/$V" "$O/lib/gcc/$T" "$O/$T/bin" "$O/$T/lib" "$O/$T/sysroot/usr/lib"
cp "$S/bin/$T-gcc.exe" "$S/bin/$T-g++.exe" "$S"/bin/*.dll "$O/bin/"
for f in cc1.exe collect2.exe lto-wrapper.exe liblto_plugin.dll; do
  cp "$S/libexec/gcc/$T/$V/$f" "$O/libexec/gcc/$T/$V/"
done
cp -r "$S/lib/gcc/$T/$V" "$O/lib/gcc/$T/"
rm -rf "${O:?}/lib/gcc/$T/$V/plugin" "$O/lib/gcc/$T/$V/install-tools"
rm -f "$O/lib/gcc/$T/$V"/libstdc++exp.a "$O/lib/gcc/$T/$V"/libstdc++fs.a "$O/lib/gcc/$T/$V"/libgomp*
cp "$S/$T/bin/as.exe" "$S/$T/bin/ld.exe" "$O/$T/bin/"
cp -r "$S/$T/lib/ldscripts" "$O/$T/lib/"
cp -r "$S/$T/sysroot/usr/include" "$O/$T/sysroot/usr/"
# The import and runtime libraries the link uses (ld --trace), and the
# C runtime's own.
for l in dllcrt2.o crt2.o libmingw32.a libmingwex.a libmsvcrt.a libucrt.a libucrtbase.a libmoldname.a \
         libm.a libpthread.a libwinpthread.a libkernel32.a libuser32.a libgdi32.a libadvapi32.a \
         libshell32.a libole32.a liboleaut32.a libuuid.a libcomdlg32.a libwinspool.a libbcrypt.a \
         libdbghelp.a libdinput8.a libimm32.a libiphlpapi.a libopengl32.a libsetupapi.a libshlwapi.a \
         libversion.a libwindowscodecs.a libwinmm.a libws2_32.a; do
  cp "$S/$T/sysroot/usr/lib/$l" "$O/$T/sysroot/usr/lib/"
done
mkdir -p "$O/licenses"
cp -r "$S/../info/licenses/." "$O/licenses/" 2>/dev/null || true
cat > "$O/README.txt" <<'EOF'
GCC 16.2.0 and GNU Binutils 2.46.1 for x86_64-w64-mingw32 (UCRT, mingw-w64 12),
the Windows builds from conda-forge (https://conda-forge.org), trimmed to what
Setup.exe needs. GCC and Binutils are free software under the GNU GPL version 3
(GCC's runtime libraries with the GCC Runtime Library Exception); mingw-w64 is
under its own permissive licences (see licenses). Their source code:
  https://gcc.gnu.org  https://sourceware.org/binutils  https://www.mingw-w64.org
and the exact conda-forge build recipes:
  https://github.com/conda-forge/ctng-compilers-feedstock
  https://github.com/conda-forge/binutils-feedstock
  https://github.com/conda-forge/mingw-w64-feedstock
EOF
du -sh "$O"
