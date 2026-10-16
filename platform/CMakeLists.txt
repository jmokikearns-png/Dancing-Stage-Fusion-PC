# dsfplatform: the StepMania 2005 donor platform layer for the Dancing Stage Fusion PC port.
#
# Only these StepMania subsystems are used: keyboard input, dance-pad/joystick input,
# input filtering (timestamps), user-configurable mapping persisted to an ini file,
# and window/display management.  Nothing else from StepMania (no screens, themes,
# songs, notes, sound, scoring) is compiled in.
#
# How the original sources are used unchanged ("overlay"):
#   The original files are compiled from a build-time mirror of ../src.  The mirror
#   holds a copy of every header and of the selected .cpp files, and then the small
#   replacement headers from shim/ are copied over the same relative paths.  This is
#   needed because `#include "X.h"` always searches the including file's own directory
#   first, so a shim header placed on the -I path could never win against src/X.h.
#   Nothing under ../src is modified by this build; the shims are listed in SHIMS.md.
#
#   cmake -S platform -B build -G Ninja                       (Linux x86-64: X11 + /dev/input/js*)
#   cmake -S platform -B build -G Ninja \
#     -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/i686-w64-mingw32.cmake   (Win32: DirectInput + WGL)
cmake_minimum_required(VERSION 3.16)
project(dsfplatform C CXX)

get_filename_component(SM_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../src" ABSOLUTE)
set(DSF_SHIM "${CMAKE_CURRENT_SOURCE_DIR}/shim")
set(OVL "${CMAKE_CURRENT_BINARY_DIR}/overlay")

if(WIN32)
  set(DSF_PLATFORM_WIN32 ON)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(DSF_PLATFORM_LINUX ON)
else()
  message(FATAL_ERROR "dsfplatform: only Linux (X11) and Windows (MinGW) are wired up")
endif()

# ---------------------------------------------------------------------------
# Original StepMania 2005 sources compiled into the library (paths relative to src/).
# ---------------------------------------------------------------------------
set(SM_CORE
  # base utilities
  global.cpp RageUtil.cpp RageLog.cpp RageTimer.cpp RageThreads.cpp RageException.cpp
  RageMath.cpp
  # file layer (needed by IniFile/XmlFile and RageLog)
  RageFile.cpp RageFileBasic.cpp RageFileManager.cpp RageFileDriver.cpp
  RageFileDriverDirect.cpp RageFileDriverDirectHelpers.cpp RageFileDriverMemory.cpp
  RageUtil_FileDB.cpp
  IniFile.cpp XmlFile.cpp DateTime.cpp
  # input stack
  RageInputDevice.cpp RageInput.cpp InputFilter.cpp InputMapper.cpp GameInput.cpp Game.cpp
  arch/InputHandler/InputHandler.cpp
  # display / window
  RageDisplay.cpp RageDisplay_OGL.cpp RageDisplay_OGL_Extensions.cpp
  RageSurface.cpp RageSurfaceUtils.cpp RageSurface_Save_BMP.cpp RageSurfaceUtils_Zoom.cpp
  RageTexture.cpp
)
set(SM_PCRE pcre/chartables.c pcre/get.c pcre/maketables.c pcre/pcre.c pcre/study.c)
set(SM_LINUX
  arch/InputHandler/InputHandler_X11.cpp
  arch/InputHandler/InputHandler_Linux_Joystick.cpp
  arch/LowLevelWindow/LowLevelWindow_X11.cpp
  archutils/Unix/X11Helper.cpp
  arch/Threads/Threads_Pthreads.cpp
  archutils/Unix/LinuxThreadHelpers.cpp
  archutils/Unix/RunningUnderValgrind.cpp
)
set(SM_WIN32
  arch/InputHandler/InputHandler_DirectInput.cpp
  arch/InputHandler/InputHandler_DirectInputHelper.cpp
  arch/LowLevelWindow/LowLevelWindow_Win32.cpp
  archutils/Win32/GraphicsWindow.cpp
  archutils/Win32/AppInstance.cpp
  archutils/Win32/WindowIcon.cpp
  archutils/Win32/arch_setup.cpp
  arch/Threads/Threads_Win32.cpp
)

# Input-only variant for the game: the runtime owns the window and renderer, so
# StepMania's display/window code is left out (src/dsf_hostwindow.cpp supplies the
# one window accessor DirectInput needs).
option(DSF_INPUT_ONLY "Build only the input stack (host owns the window)" OFF)
if(DSF_INPUT_ONLY)
  list(FILTER SM_CORE EXCLUDE REGEX "^(RageDisplay|RageSurface|RageTexture)")
  list(FILTER SM_WIN32 EXCLUDE REGEX "^(arch/LowLevelWindow/|archutils/Win32/(GraphicsWindow|WindowIcon)\\.cpp)")
  list(FILTER SM_LINUX EXCLUDE REGEX "^(arch/LowLevelWindow/|archutils/Unix/X11Helper)")
endif()

set(SM_SOURCES ${SM_CORE} ${SM_PCRE})
if(DSF_PLATFORM_LINUX)
  list(APPEND SM_SOURCES ${SM_LINUX})
else()
  list(APPEND SM_SOURCES ${SM_WIN32})
endif()

# ---------------------------------------------------------------------------
# Build the overlay: all original headers + selected sources, then shims on top.
# ---------------------------------------------------------------------------
file(GLOB_RECURSE SM_HEADERS RELATIVE "${SM_SRC}"
  "${SM_SRC}/*.h")
list(FILTER SM_HEADERS EXCLUDE REGEX "^(crypto|crypto51|ffmpeg|libresample|mad-|lua-|vorbis|SDL|BaseClasses|smpackage|Texture Font Generator)")
file(GLOB_RECURSE DSF_SHIM_FILES RELATIVE "${DSF_SHIM}" "${DSF_SHIM}/*")
list(FILTER DSF_SHIM_FILES EXCLUDE REGEX "\\.md$")
foreach(f ${SM_HEADERS} ${SM_SOURCES})
  list(FIND DSF_SHIM_FILES "${f}" _is_shimmed)
  if(_is_shimmed EQUAL -1)
    configure_file("${SM_SRC}/${f}" "${OVL}/${f}" COPYONLY)
  endif()
endforeach()
foreach(f ${DSF_SHIM_FILES})
  configure_file("${DSF_SHIM}/${f}" "${OVL}/${f}" COPYONLY)
endforeach()

set(OVL_SOURCES "")
foreach(f ${SM_SOURCES})
  list(APPEND OVL_SOURCES "${OVL}/${f}")
endforeach()

# Platform configuration header (the autotools build generates config.h; the MinGW
# build relies on archutils/Win32/arch_setup.h instead).
if(DSF_PLATFORM_LINUX)
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/config/config_linux.h" "${OVL}/config.h" COPYONLY)
endif()

# ---------------------------------------------------------------------------
# The library.
# ---------------------------------------------------------------------------
add_library(dsfplatform STATIC
  ${OVL_SOURCES}
  src/dsf_shims.cpp
  src/dsf_arch.cpp
  src/dsf_ps1game.cpp
  src/dsfplatform.cpp
  src/dsf_hostwindow.cpp
)
if(DSF_INPUT_ONLY)
  target_compile_definitions(dsfplatform PUBLIC DSF_INPUT_ONLY)
  if(DSF_PLATFORM_WIN32)
    # arch_setup.cpp supplies helpers the input code needs (my_usleep, my_localtime_r)
    # but also StepMania's own WinMain; the host program has its own entry point.
    set_source_files_properties("${OVL}/archutils/Win32/arch_setup.cpp" PROPERTIES
      COMPILE_DEFINITIONS "WinMain=dsf_unused_stepmania_WinMain")
  endif()
endif()
target_include_directories(dsfplatform
  PUBLIC  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  PRIVATE "${OVL}")

if(DSF_PLATFORM_LINUX)
  target_compile_definitions(dsfplatform PRIVATE HAVE_CONFIG_H DSF_PLATFORM=1)
  find_package(X11 REQUIRED)
  find_package(OpenGL REQUIRED)
  find_package(Threads REQUIRED)
  target_link_libraries(dsfplatform PUBLIC ${X11_LIBRARIES} OpenGL::GL OpenGL::GLU Threads::Threads)
else()
  target_compile_definitions(dsfplatform PRIVATE WIN32 _WINDOWS WINDOWS RELEASE DSF_PLATFORM=1
    SM_NO_D3D SM_NO_FFMPEG SM_NO_DSHOW)
  target_link_libraries(dsfplatform PUBLIC dinput dxguid dxerr8 opengl32 glu32 winmm gdi32 user32 advapi32 ole32)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_compile_options(dsfplatform PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-std=gnu++98>
    -fno-strict-aliasing -Wno-unknown-pragmas -Wno-deprecated-declarations -Wno-write-strings)
  set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/src/dsfplatform.cpp PROPERTIES COMPILE_OPTIONS "-Wall")
endif()

# ---------------------------------------------------------------------------
# Test harness.
# ---------------------------------------------------------------------------
if(NOT DSF_INPUT_ONLY)
add_executable(dsfplatform-test test/dsfplatform-test.cpp)
target_link_libraries(dsfplatform-test PRIVATE dsfplatform)
# The harness also draws its button overlay with RageDisplay, so it sees the engine
# headers (the runtime proper only needs include/dsfplatform.h).
target_include_directories(dsfplatform-test PRIVATE "${OVL}")
get_target_property(_dsf_defs dsfplatform COMPILE_DEFINITIONS)
target_compile_definitions(dsfplatform-test PRIVATE ${_dsf_defs})
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_compile_options(dsfplatform-test PRIVATE -std=gnu++98 -Wall -Wno-unknown-pragmas)
  if(WIN32)
    target_link_options(dsfplatform-test PRIVATE -static-libgcc -static-libstdc++ -static)
  endif()
endif()
endif()
