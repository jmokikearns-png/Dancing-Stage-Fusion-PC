cmake_minimum_required(VERSION 3.20)
set(PSXRECOMP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/psxrecomp" CACHE PATH "Path to psxrecomp")
project(DSFFeasibility LANGUAGES C CXX)
set(PSX_RECOMP_UI OFF CACHE BOOL "feasibility: no launcher UI" FORCE)
include("${PSXRECOMP_ROOT}/runtime/runtime.cmake")

# Our own code (not disc-derived): plugins, mod packages, shipped config.
set(DSF_PORT_DIR "/home/user/work/port" CACHE PATH "Dancing Stage Fusion PC port sources")
# StepMania 2005 input layer (branch wip/sm-platform of the engine repo).
set(DSF_PLATFORM_DIR "/home/user/work/wt/sm-platform/platform" CACHE PATH "dsfplatform sources")

# DSF_EMBED: build the game as dsfcore.dll for the port's StepMania, which
# provides window, sound, input and pacing (port/core/dsfcore_api.h).
option(DSF_EMBED "Build dsfcore.dll for the StepMania-hosted port" OFF)
if(DSF_EMBED)
    set(PSX_EMBED ON CACHE BOOL "runtime as a host-driven shared library" FORCE)
endif()

if(DSF_EMBED)
    # StepMania integration hooks (port/core/dsfcore_hooks.cpp).
    set(PSX_PRELOADED_MODS "${DSF_PORT_DIR}/mods/embedded")
elseif(WIN32)
    set(PSX_PRELOADED_MODS "${DSF_PORT_DIR}/mods/preloaded")
else()
    # Linux test build: the same hooks, with dsfcore_host_stub.cpp as the host.
    set(PSX_PRELOADED_MODS "${DSF_PORT_DIR}/mods/embedded")
endif()
psxrecomp_add_game_runtime(psx-runtime
    PRELOADED_MODS_DIR "${PSX_PRELOADED_MODS}"
    WINDOW_TITLE "DSF feasibility"
    DEFAULT_GAME_CONFIG_PATH "game.toml"
    MAX_PLAYERS 2
    GEN_MARKER "generated/SLES_041.63_dispatch.c"
    GEN_FULL_GLOB "generated/SLES_041.63_full_*.c"
)
foreach(_c game.toml)
    add_custom_command(TARGET psx-runtime POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/${_c}" "$<TARGET_FILE_DIR:psx-runtime>/${_c}" VERBATIM)
endforeach()

if(DSF_EMBED)
    target_sources(psx-runtime PRIVATE "${DSF_PORT_DIR}/core/dsfcore_embed.cpp"
                                       "${DSF_PORT_DIR}/core/dsfcore_hooks.cpp"
                                       "${DSF_PORT_DIR}/core/dsfcore_menus.cpp"
                                       "${DSF_PORT_DIR}/core/dsfcore_extrasong.cpp"
                                       "${DSF_PORT_DIR}/core/dsfcore_quitmenu.cpp"
                                       "${DSF_PORT_DIR}/core/dsfcore_stepcolors.cpp"
                                       "${DSF_PORT_DIR}/core/dsfcore_timing.cpp")
    target_include_directories(psx-runtime PRIVATE "${DSF_PORT_DIR}/core")
    set_target_properties(psx-runtime PROPERTIES OUTPUT_NAME dsfcore PREFIX "")
elseif(WIN32)
    # Controls through StepMania's input code (keyboard, dance pads, joysticks via DirectInput).
    set(DSF_INPUT_ONLY ON CACHE BOOL "dsfplatform: input stack only" FORCE)
    add_subdirectory("${DSF_PLATFORM_DIR}" dsfplatform)
    target_sources(psx-runtime PRIVATE "${DSF_PORT_DIR}/plugins/dsf_input/dsf_input_plugin.cpp")
    target_link_libraries(psx-runtime PRIVATE dsfplatform)
    add_custom_command(TARGET psx-runtime POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DSF_PORT_DIR}/config/config.ini" "$<TARGET_FILE_DIR:psx-runtime>/config.ini" VERBATIM)
endif()
if(NOT WIN32)
    target_sources(psx-runtime PRIVATE "${DSF_PORT_DIR}/core/dsfcore_host_stub.cpp"
                                       "${DSF_PORT_DIR}/core/dsfcore_hooks.cpp"
                                       "${DSF_PORT_DIR}/core/dsfcore_menus.cpp"
                                       "${DSF_PORT_DIR}/core/dsfcore_extrasong.cpp"
                                       "${DSF_PORT_DIR}/core/dsfcore_quitmenu.cpp"
                                       "${DSF_PORT_DIR}/core/dsfcore_stepcolors.cpp"
                                       "${DSF_PORT_DIR}/core/dsfcore_timing.cpp")
    target_include_directories(psx-runtime PRIVATE "${DSF_PORT_DIR}/core")
endif()
