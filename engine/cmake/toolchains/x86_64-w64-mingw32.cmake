# CMake toolchain file: cross-compile for 64-bit Windows with MinGW-w64 (Debian/Ubuntu packaging).
#
#   cmake -S . -B build -G Ninja \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/x86_64-w64-mingw32.cmake
#
# The Debian/Ubuntu packages ship two flavours of each compiler: "-win32" (Win32 thread
# model, no std::thread) and "-posix" (winpthreads).  The 2005 engine has its own
# thread layer (arch/Threads/Threads_Win32.cpp) and needs neither std::thread nor
# winpthreads, so prefer the -win32 flavour: it avoids a libwinpthread-1.dll dependency.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

set(MINGW_TRIPLE x86_64-w64-mingw32 CACHE STRING "MinGW-w64 target triple")
set(MINGW_FLAVOUR win32 CACHE STRING "Thread model flavour of the Debian mingw-w64 compilers (win32|posix)")

find_program(CMAKE_C_COMPILER   NAMES ${MINGW_TRIPLE}-gcc-${MINGW_FLAVOUR} ${MINGW_TRIPLE}-gcc REQUIRED)
find_program(CMAKE_CXX_COMPILER NAMES ${MINGW_TRIPLE}-g++-${MINGW_FLAVOUR} ${MINGW_TRIPLE}-g++ REQUIRED)
find_program(CMAKE_RC_COMPILER  NAMES ${MINGW_TRIPLE}-windres REQUIRED)
set(CMAKE_AR      ${MINGW_TRIPLE}-ar      CACHE FILEPATH "")
set(CMAKE_RANLIB  ${MINGW_TRIPLE}-ranlib  CACHE FILEPATH "")

# Sysroot of the distro toolchain, plus an optional prefix holding cross-built
# third-party libraries (see scripts/windows-build/build-deps-mingw.sh).  The prefix
# must be a find *root* (not just CMAKE_PREFIX_PATH) because of the ONLY modes below.
set(SM_DEPS_PREFIX "$ENV{SM_DEPS_PREFIX}" CACHE PATH "Prefix with cross-built dependencies")
set(CMAKE_FIND_ROOT_PATH /usr/${MINGW_TRIPLE})
if(SM_DEPS_PREFIX)
  list(APPEND CMAKE_FIND_ROOT_PATH "${SM_DEPS_PREFIX}")
endif()
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES SM_DEPS_PREFIX MINGW_TRIPLE MINGW_FLAVOUR)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Run cross-compiled test programs (try_run) under Wine if it is available.
find_program(WINE_EXECUTABLE NAMES wine)
if(WINE_EXECUTABLE)
  set(CMAKE_CROSSCOMPILING_EMULATOR ${WINE_EXECUTABLE})
endif()
