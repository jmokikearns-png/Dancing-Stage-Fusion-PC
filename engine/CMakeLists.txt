# CMake build for the 2005-06-19 StepMania CVS engine ("3.95 CVS", PRODUCT_VER "4.0 CVS"),
# the base of the Dancing Stage Fusion PC port.
#
# Status: Windows (32-bit) via MinGW-w64 cross-compile is the first target.  The
# structure is meant to grow into the single build system for MSVC (x86/x64),
# MinGW and Linux; the 2005 VC6/VC7 project files under src/ stay as reference only.
#
# Quick start (Linux host, cross to Windows):
#   scripts/windows-build/build-deps-mingw.sh i686            # zlib/png/jpeg/ogg/vorbis
#   SM_DEPS_PREFIX=<prefix> cmake -S . -B build -G Ninja \
#       -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/i686-w64-mingw32.cmake
#   cmake --build build
cmake_minimum_required(VERSION 3.16)
project(StepManiaDSF LANGUAGES C CXX)
if(WIN32)
  enable_language(RC)
endif()

set(SM_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src)
include(cmake/StepManiaSources.cmake)

if(NOT WIN32)
  message(FATAL_ERROR "Only the Windows target is wired up so far (see notes/windows-build.md). "
                      "Linux still uses the 2005 autotools build.")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4 AND SM_WITH_FFMPEG)
  message(WARNING "64-bit build with SM_WITH_FFMPEG: the snapshot's prebuilt ffmpeg DLLs are 32-bit.")
endif()

# ---------------------------------------------------------------------------
# Feature switches.  Defaults reflect what currently builds with MinGW-w64.
# ---------------------------------------------------------------------------
option(SM_WITH_D3D            "Direct3D 8 renderer (RageDisplay_D3D)" ON)
option(SM_WITH_DSHOW          "DirectShow movie driver (needs src/BaseClasses built for this compiler)" OFF)
option(SM_WITH_FFMPEG         "FFmpeg movie driver, linked against the 2005 prebuilt avcodec/avformat DLLs" ON)
# DSF PC: off.  MP3 decoding is libmad, which is GPL; the port plays no MP3s,
# and without it the program stays under StepMania's MIT licence.
option(SM_WITH_MP3            "MP3 decoding (libmad, GPL)" ON)
option(SM_STATIC_RUNTIME      "Link libgcc/libstdc++ statically (MinGW) so no compiler DLLs need shipping" ON)

# ---------------------------------------------------------------------------
# Third-party code that ships *as source* in the snapshot: built here.
# ---------------------------------------------------------------------------
# Lua 5.0 (src/lua-5.0): core + standard libraries.
file(GLOB SM_LUA_CORE ${SM_SRC}/lua-5.0/src/*.c)
file(GLOB SM_LUA_LIB  ${SM_SRC}/lua-5.0/src/lib/*.c)
add_library(sm_lua STATIC ${SM_LUA_CORE} ${SM_LUA_LIB})
target_include_directories(sm_lua PUBLIC ${SM_SRC}/lua-5.0/include PRIVATE ${SM_SRC}/lua-5.0/src)
set_target_properties(sm_lua PROPERTIES C_STANDARD 90)

# libmad 0.15.1b (src/mad-0.15.1b).  config.h comes from the bundled msvc++ dir,
# which only describes standard headers and type sizes (valid for ILP32 MinGW too).
set(SM_MAD_SOURCES bit decoder fixed frame huffman layer12 layer3 stream synth timer version)
list(TRANSFORM SM_MAD_SOURCES PREPEND ${SM_SRC}/mad-0.15.1b/)
list(TRANSFORM SM_MAD_SOURCES APPEND .c)
add_library(sm_mad STATIC ${SM_MAD_SOURCES})
target_include_directories(sm_mad PUBLIC ${SM_SRC}/mad-0.15.1b PRIVATE ${SM_SRC}/mad-0.15.1b/msvc++)
target_compile_definitions(sm_mad PRIVATE HAVE_CONFIG_H ASO_ZEROCHECK
  $<IF:$<EQUAL:${CMAKE_SIZEOF_VOID_P},4>,FPM_INTEL,FPM_64BIT>)

# libresample (src/libresample).  The VC build links the prebuilt resample.dll;
# here it is linked statically from the bundled source.
file(GLOB SM_RESAMPLE_SOURCES ${SM_SRC}/libresample/src/*.c)
add_library(sm_resample STATIC ${SM_RESAMPLE_SOURCES})
target_include_directories(sm_resample PUBLIC ${SM_SRC}/libresample/include)
target_link_libraries(sm_resample PRIVATE m)

# ---------------------------------------------------------------------------
# Third-party code that the snapshot only has as MSVC-built .lib files.  Those
# cannot be linked by MinGW (they need MSVC CRT helpers: _alldiv, _allmul,
# _allshl, _alloca_probe, _chkstk ...), so they come from a dependency prefix
# (scripts/windows-build/build-deps-mingw.sh; vcpkg/MSYS2 packages also fit).
# ---------------------------------------------------------------------------
find_package(ZLIB REQUIRED)
find_path(SM_PNG_INCLUDE_DIR png.h PATH_SUFFIXES libpng12 REQUIRED)
find_library(SM_PNG_LIBRARY NAMES png12_static png12 libpng12 png REQUIRED)
find_package(JPEG REQUIRED)
find_path(SM_VORBIS_INCLUDE_DIR vorbis/vorbisfile.h REQUIRED)
find_library(SM_VORBISFILE_LIBRARY vorbisfile REQUIRED)
find_library(SM_VORBIS_LIBRARY vorbis REQUIRED)
find_library(SM_OGG_LIBRARY ogg REQUIRED)

# FFmpeg: the snapshot ships MinGW-built 2005 DLLs (Program/avcodec.dll,
# Program/avformat.dll), their import libraries (src/ffmpeg/lib/windows/*.lib) and
# matching headers (src/ffmpeg/include).  No source is included.
if(SM_WITH_FFMPEG)
  add_library(sm_ffmpeg INTERFACE)
  target_include_directories(sm_ffmpeg INTERFACE ${SM_SRC}/ffmpeg/include)
  target_link_libraries(sm_ffmpeg INTERFACE
    ${SM_SRC}/ffmpeg/lib/windows/avformat.lib
    ${SM_SRC}/ffmpeg/lib/windows/avcodec.lib)
endif()

# ---------------------------------------------------------------------------
# Build stamp: replaces the VC pre-build step (archutils/Win32/verinc.exe), which
# writes verstub.cpp with version_num/version_time.
# ---------------------------------------------------------------------------
set(SM_BUILD_NUMBER 0 CACHE STRING "Build number reported in logs/crash dumps (version_num)")
string(TIMESTAMP SM_BUILD_TIME "%a %b %d %H:%M:%S %Y" UTC)
configure_file(cmake/verstub.cpp.in ${CMAKE_CURRENT_BINARY_DIR}/verstub.cpp @ONLY)

# ---------------------------------------------------------------------------
# The engine.
# ---------------------------------------------------------------------------
set(SM_SOURCES
  ${SM_SOURCES_CORE} ${SM_SOURCES_CRYPTO} ${SM_SOURCES_PCRE}
  ${SM_SOURCES_ARCH_COMMON} ${SM_SOURCES_WIN32} ${SM_SOURCES_WIN32_RC})
if(SM_WITH_D3D)
  list(APPEND SM_SOURCES ${SM_SOURCES_WIN32_D3D})
endif()
if(SM_WITH_DSHOW)
  list(APPEND SM_SOURCES ${SM_SOURCES_WIN32_DSHOW})
endif()
if(NOT SM_WITH_FFMPEG)
  list(REMOVE_ITEM SM_SOURCES arch/MovieTexture/MovieTexture_FFMpeg.cpp)
endif()
if(NOT SM_WITH_MP3)
  list(REMOVE_ITEM SM_SOURCES RageSoundReader_MP3.cpp)
endif()
list(TRANSFORM SM_SOURCES PREPEND ${SM_SRC}/)

add_executable(StepMania WIN32 ${SM_SOURCES} ${CMAKE_CURRENT_BINARY_DIR}/verstub.cpp)
target_include_directories(StepMania PRIVATE
  ${SM_SRC}
  ${SM_PNG_INCLUDE_DIR} ${SM_VORBIS_INCLUDE_DIR} ${JPEG_INCLUDE_DIRS} ${ZLIB_INCLUDE_DIRS})
# Same defines as the VC projects (WIN32,_WINDOWS,WINDOWS + DEBUG/RELEASE).
target_compile_definitions(StepMania PRIVATE WIN32 _WINDOWS WINDOWS
  $<IF:$<CONFIG:Debug>,DEBUG,RELEASE>)
if(NOT SM_WITH_D3D)
  target_compile_definitions(StepMania PRIVATE SM_NO_D3D)
endif()
if(NOT SM_WITH_DSHOW)
  target_compile_definitions(StepMania PRIVATE SM_NO_DSHOW)
endif()
if(NOT SM_WITH_FFMPEG)
  target_compile_definitions(StepMania PRIVATE SM_NO_FFMPEG)
endif()

if(NOT SM_WITH_MP3)
  target_compile_definitions(StepMania PRIVATE NO_MP3_SUPPORT)
endif()

target_link_libraries(StepMania PRIVATE
  sm_lua $<$<BOOL:${SM_WITH_MP3}>:sm_mad> sm_resample
  ${SM_PNG_LIBRARY} ${JPEG_LIBRARIES}
  ${SM_VORBISFILE_LIBRARY} ${SM_VORBIS_LIBRARY} ${SM_OGG_LIBRARY}
  ZLIB::ZLIB
  $<$<BOOL:${SM_WITH_FFMPEG}>:sm_ffmpeg>
  # Win32 / DirectX system libraries (what the VC build pulls in via #pragma comment(lib)).
  $<$<BOOL:${SM_WITH_D3D}>:d3d8> dxerr8 dinput dsound dxguid
  opengl32 glu32 winmm version setupapi hid dbghelp vfw32 wsock32
  ole32 oleaut32 uuid shell32 gdi32 user32 advapi32 comdlg32)
if(SM_WITH_DSHOW)
  target_link_libraries(StepMania PRIVATE strmiids quartz)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  # 2005 code: closest dialect, no strict aliasing assumptions, warnings kept quiet
  # enough to see real errors.
  target_compile_options(StepMania PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-std=gnu++98>
    -fno-strict-aliasing -Wno-unknown-pragmas -Wno-deprecated-declarations)
  if(SM_STATIC_RUNTIME)
    target_link_options(StepMania PRIVATE -static-libgcc -static-libstdc++ -static)
  endif()
  # Map file for crash-report symbolisation (the VC build converts its .map into
  # StepMania.vdi with archutils/Win32/mapconv; GNU ld maps are a different format).
  target_link_options(StepMania PRIVATE -Wl,-Map=$<TARGET_FILE_DIR:StepMania>/StepMania.map)
endif()
foreach(t sm_lua sm_mad sm_resample)
  if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_options(${t} PRIVATE -w)
  endif()
endforeach()

set_target_properties(StepMania PROPERTIES OUTPUT_NAME StepMania)
