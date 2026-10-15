cmake_minimum_required(VERSION 3.25)

project(reunion
    VERSION 1.1.0
    DESCRIPTION "Native port of Reunion (Amnesty Design, 1994)"
    LANGUAGES C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Original game data, used by the tests and, unless REUNION_EMBED_DATA_DIR is
# off (the release preset) or for the web, by the game when it has no other
# copy.
set(REUNION_DATA_DIR "${CMAKE_SOURCE_DIR}/vendor/Reunion_DOS_EN" CACHE PATH
    "Directory holding the original DOS game data")
option(REUNION_EMBED_DATA_DIR "Compile REUNION_DATA_DIR into the game as its last fallback" ON)

# --- Dependencies -----------------------------------------------------------
# Use the system package when it is new enough, otherwise build it from source.
# Set FETCHCONTENT_TRY_FIND_PACKAGE_MODE=NEVER to always build from source.
include(FetchContent)

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
# Optional X11 screensaver extension; its headers are often not installed.
set(SDL_X11_XSCRNSAVER OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.18
    GIT_SHALLOW TRUE
    FIND_PACKAGE_ARGS 3.4 CONFIG)
FetchContent_MakeAvailable(SDL3)
if(EMSCRIPTEN AND TARGET SDL3-static)
    # EM_ASM needs the GNU dialect; our global C_EXTENSIONS OFF breaks it.
    set_target_properties(SDL3-static PROPERTIES C_EXTENSIONS ON)
endif()

# Module player for the ProTracker music (ANIM\*.SPD).
set(BUILD_STATIC ON CACHE BOOL "" FORCE)
set(BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(LIBXMP_DOCS OFF CACHE BOOL "" FORCE)
set(WITH_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(LIBXMP_DISABLE_DEPACKERS ON CACHE BOOL "" FORCE)
set(LIBXMP_DISABLE_PROWIZARD ON CACHE BOOL "" FORCE)
FetchContent_Declare(libxmp
    GIT_REPOSITORY https://github.com/libxmp/libxmp.git
    GIT_TAG libxmp-4.7.3
    GIT_SHALLOW TRUE
    FIND_PACKAGE_ARGS 4.6 CONFIG)
FetchContent_MakeAvailable(libxmp)
if(TARGET xmp_static)
    # libxmp is C90 + GNU extensions (inline); our global C_EXTENSIONS OFF breaks it.
    set_target_properties(xmp_static PROPERTIES C_EXTENSIONS ON)
    # Its Debug flags add -DDEBUG, which makes it log to stderr.
    target_compile_options(xmp_static PRIVATE -UDEBUG -U_DEBUG)
endif()
# System packages usually ship only the shared library.
if(TARGET libxmp::xmp_static)
    set(REUNION_XMP libxmp::xmp_static)
else()
    set(REUNION_XMP libxmp::xmp_shared)
endif()

# OPL2 emulator for the AdLib music (GRWAR\*.PIC). Nuked OPL3, LGPL-2.1
# (LICENSE in the fetched source); upstream has no CMake project.
FetchContent_Declare(nukedopl3
    GIT_REPOSITORY https://github.com/nukeykt/Nuked-OPL3.git
    GIT_TAG 765ec962e473aeb767e4cba74ffdc8f588ffbfe8)
FetchContent_MakeAvailable(nukedopl3)
add_library(nukedopl3 STATIC "${nukedopl3_SOURCE_DIR}/opl3.c")
target_include_directories(nukedopl3 PUBLIC "${nukedopl3_SOURCE_DIR}")

# ZIP archives and CRC-32 for importing the player's copy
# (src/platform/import.h). miniz, MIT (LICENSE in the fetched source); the
# release archive is the single-file version.
FetchContent_Declare(miniz
    URL https://github.com/richgel999/miniz/releases/download/3.1.2/miniz-3.1.2.zip
    URL_HASH SHA256=f0446d863f9c19926ad9483c523fdc42e42b8d4a6a431d27e09d49c79a140d9a
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(miniz)
add_library(miniz STATIC "${miniz_SOURCE_DIR}/miniz.c")
target_include_directories(miniz PUBLIC "${miniz_SOURCE_DIR}")
target_compile_definitions(miniz PUBLIC MINIZ_NO_ZLIB_COMPATIBLE_NAMES)
set_target_properties(miniz PROPERTIES C_STANDARD 99 C_EXTENSIONS ON)

# --- Game -------------------------------------------------------------------
# Everything except main.c goes into a library shared by the game and the
# tests. New source files are picked up automatically (re-run CMake).
file(GLOB_RECURSE REUNION_SOURCES CONFIGURE_DEPENDS src/*.c)
list(REMOVE_ITEM REUNION_SOURCES "${CMAKE_SOURCE_DIR}/src/main.c")

# An object library, so every object is linked: weak stubs (src/game/stub.h)
# are then reliably replaced by the real routines.
add_library(reunion_core OBJECT ${REUNION_SOURCES})
target_include_directories(reunion_core PUBLIC src)
target_link_libraries(reunion_core PUBLIC SDL3::SDL3 miniz PRIVATE ${REUNION_XMP} nukedopl3)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(reunion_core PUBLIC -Wall -Wextra -Wpedantic)
endif()
# ds_initial (src/game/dsinit.c) is packed, so some of its pointers to DS
# fields are unaligned; macOS's chained fixups can't relocate those.
if(APPLE)
    target_link_options(reunion_core PUBLIC LINKER:-no_fixup_chains)
endif()

add_executable(reunion src/main.c)
target_link_libraries(reunion PRIVATE reunion_core)
# On Windows a GUI program, without a console window.
set_target_properties(reunion PROPERTIES WIN32_EXECUTABLE ON)
if(EMSCRIPTEN)
    # The game's waiting loops stay as they are: Asyncify lets
    # timing_wait_retrace() return to the browser and resume (platform/timing.c).
    set_target_properties(reunion PROPERTIES SUFFIX ".html")
    target_link_options(reunion PRIVATE
        -sASYNCIFY -sASYNCIFY_STACK_SIZE=262144 -sSTACK_SIZE=1048576
        -sALLOW_MEMORY_GROWTH -sINITIAL_MEMORY=67108864 -lidbfs.js
        "--js-library=${CMAKE_SOURCE_DIR}/src/platform/web/library.js"
        "--shell-file=${CMAKE_SOURCE_DIR}/src/platform/web/shell.html")
    set_property(TARGET reunion APPEND PROPERTY LINK_DEPENDS
        "${CMAKE_SOURCE_DIR}/src/platform/web/library.js"
        "${CMAKE_SOURCE_DIR}/src/platform/web/shell.html")
elseif(REUNION_EMBED_DATA_DIR)
    target_compile_definitions(reunion PRIVATE REUNION_DEFAULT_DATA_DIR="${REUNION_DATA_DIR}")
endif()

# --- Tests ------------------------------------------------------------------
# Each tests/test_*.c is a standalone program; exit code 77 means "skipped"
# (e.g. no game data). They get the data directory as argv[1].
include(CTest)
if(BUILD_TESTING)
    # The playthrough fixture (docs/playthrough.md), for tests/test_play_*.c.
    file(GLOB REUNION_PLAY_SOURCES CONFIGURE_DEPENDS tests/play/*.c)
    add_library(reunion_play STATIC ${REUNION_PLAY_SOURCES})
    target_include_directories(reunion_play PUBLIC tests)
    target_link_libraries(reunion_play PUBLIC reunion_core)

    # The port's side of the DOSBox-X comparison (tools/dosbox/side_by_side.py).
    add_executable(dosside EXCLUDE_FROM_ALL tools/dosbox/dosside.c)
    target_link_libraries(dosside PRIVATE reunion_play)

    file(GLOB REUNION_TESTS CONFIGURE_DEPENDS tests/test_*.c)
    foreach(src IN LISTS REUNION_TESTS)
        get_filename_component(name "${src}" NAME_WE)
        add_executable(${name} ${src})
        if(name MATCHES "^test_play_" OR name STREQUAL "test_golden")
            target_link_libraries(${name} PRIVATE reunion_play)
        else()
            target_link_libraries(${name} PRIVATE reunion_core)
        endif()
        if(name STREQUAL "test_golden")
            # One test per case (tests/test_golden.c); the playthrough's
            # proof runs only where test_play_game does.
            target_compile_definitions(${name} PRIVATE REUNION_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
            set(cases boot soak1 soak2 soak3 soak4 soak5 soak1_original credits intro victory
                load_J01 load_M01 load_L01 load_X01 load_DOS_1 load_DOS_2)
            if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
                list(APPEND cases proof proof_original)
            endif()
            foreach(c IN LISTS cases)
                add_test(NAME golden_${c} COMMAND ${name} "${REUNION_DATA_DIR}" ${c})
                set_tests_properties(golden_${c} PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 900)
            endforeach()
            continue()
        endif()
        if(name STREQUAL "test_play_game" AND CMAKE_BUILD_TYPE STREQUAL "Debug")
            # The whole game and its two replays take ~5 minutes in a debug
            # build and ~2 in a release one, which also catches what only
            # goes wrong optimised. Run it from build/release.
            continue()
        endif()
        add_test(NAME ${name} COMMAND ${name} "${REUNION_DATA_DIR}")
        set_tests_properties(${name} PROPERTIES SKIP_RETURN_CODE 77)
        if(name MATCHES "^test_play_")
            # Recordings are also replayed by the game itself (reunion --replay).
            add_dependencies(${name} reunion)
            set_tests_properties(${name} PROPERTIES
                ENVIRONMENT "REUNION_BIN=$<TARGET_FILE:reunion>")
        endif()
        if(name STREQUAL "test_play_game")
            set_tests_properties(${name} PROPERTIES TIMEOUT 900)
            # The CD's other languages, from its image next to the floppy
            # release (tests/play/run.h, PLAY_LANGUAGE).
            file(GLOB cd_iso "${REUNION_DATA_DIR}/../*.iso")
            list(SORT cd_iso)
            list(LENGTH cd_iso found)
            if(found GREATER 0)
                list(GET cd_iso 0 cd_iso)
                foreach(lang german french italian)
                    add_test(NAME ${name}_${lang} COMMAND ${name} "${cd_iso}")
                    set_tests_properties(${name}_${lang} PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 900
                        ENVIRONMENT "REUNION_BIN=$<TARGET_FILE:reunion>;PLAY_LANGUAGE=${lang}")
                endforeach()
            endif()
        endif()
    endforeach()
endif()
