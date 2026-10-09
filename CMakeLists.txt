cmake_minimum_required(VERSION 3.25)

project(reunion
    VERSION 0.1.0
    DESCRIPTION "Native port of Reunion (Amnesty Design, 1994)"
    LANGUAGES C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Original game data, used by the tests (and as the default data directory).
set(REUNION_DATA_DIR "${CMAKE_SOURCE_DIR}/vendor/Reunion_DOS_EN" CACHE PATH
    "Directory holding the original DOS game data")

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

# --- Game -------------------------------------------------------------------
# Everything except main.c goes into a library shared by the game and the
# tests. New source files are picked up automatically (re-run CMake).
file(GLOB_RECURSE REUNION_SOURCES CONFIGURE_DEPENDS src/*.c)
list(REMOVE_ITEM REUNION_SOURCES "${CMAKE_SOURCE_DIR}/src/main.c")

# An object library, so every object is linked: weak stubs (src/game/stub.h)
# are then reliably replaced by the real routines.
add_library(reunion_core OBJECT ${REUNION_SOURCES})
target_include_directories(reunion_core PUBLIC src)
target_link_libraries(reunion_core PUBLIC SDL3::SDL3 PRIVATE ${REUNION_XMP} nukedopl3)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(reunion_core PUBLIC -Wall -Wextra -Wpedantic)
endif()

add_executable(reunion src/main.c)
target_link_libraries(reunion PRIVATE reunion_core)
target_compile_definitions(reunion PRIVATE REUNION_DEFAULT_DATA_DIR="${REUNION_DATA_DIR}")

# --- Tests ------------------------------------------------------------------
# Each tests/test_*.c is a standalone program; exit code 77 means "skipped"
# (e.g. no game data). They get the data directory as argv[1].
include(CTest)
if(BUILD_TESTING)
    # Differential tests (docs/difftest.md) run the original code under
    # Unicorn: tests/test_diff_*.c, and the tests listed in REUNION_DIFF_USERS.
    option(REUNION_DIFFTEST "Build the differential tests (needs Unicorn)" ON)
    set(REUNION_DIFF_USERS test_travel)
    if(REUNION_DIFFTEST)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(UNICORN QUIET IMPORTED_TARGET unicorn>=2.0)
        endif()
        if(UNICORN_FOUND)
            set(REUNION_UNICORN PkgConfig::UNICORN)
        else()
            # Test-only dependency: a static x86-only build, no tests/samples.
            block()
                set(UNICORN_ARCH "x86" CACHE STRING "" FORCE)
                set(UNICORN_BUILD_TESTS OFF CACHE BOOL "" FORCE)
                set(UNICORN_INSTALL OFF CACHE BOOL "" FORCE)
                set(UNICORN_LEGACY_STATIC_ARCHIVE OFF CACHE BOOL "" FORCE)
                set(BUILD_SHARED_LIBS OFF)
                # QEMU needs GNU C; our global C_EXTENSIONS OFF would break it.
                set(CMAKE_C_EXTENSIONS ON)
                set(CMAKE_C_STANDARD 11)
                FetchContent_Declare(unicorn
                    GIT_REPOSITORY https://github.com/unicorn-engine/unicorn.git
                    GIT_TAG 2.1.4
                    GIT_SHALLOW TRUE
                    SYSTEM
                    EXCLUDE_FROM_ALL)
                FetchContent_MakeAvailable(unicorn)
                # Third-party code: keep our build log free of its warnings.
                foreach(t IN ITEMS unicorn unicorn-common x86_64-softmmu)
                    if(TARGET ${t})
                        target_compile_options(${t} PRIVATE -w)
                    endif()
                endforeach()
            endblock()
            set(REUNION_UNICORN unicorn)
        endif()
        add_library(reunion_diff STATIC tests/diff/diff.c tests/diff/dseg_names.c)
        target_include_directories(reunion_diff PUBLIC tests)
        target_link_libraries(reunion_diff PUBLIC reunion_core ${REUNION_UNICORN})
        target_compile_definitions(reunion_diff PRIVATE
            REUNION_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    endif()

    # The playthrough fixture (docs/playthrough.md), for tests/test_play_*.c.
    file(GLOB REUNION_PLAY_SOURCES CONFIGURE_DEPENDS tests/play/*.c)
    add_library(reunion_play STATIC ${REUNION_PLAY_SOURCES})
    target_include_directories(reunion_play PUBLIC tests)
    target_link_libraries(reunion_play PUBLIC reunion_core)

    file(GLOB REUNION_TESTS CONFIGURE_DEPENDS tests/test_*.c)
    foreach(src IN LISTS REUNION_TESTS)
        get_filename_component(name "${src}" NAME_WE)
        set(diff_user OFF)
        if(name MATCHES "^test_diff_" OR name IN_LIST REUNION_DIFF_USERS)
            set(diff_user ON)
        endif()
        if(diff_user AND NOT REUNION_DIFFTEST)
            continue()
        endif()
        add_executable(${name} ${src})
        if(diff_user)
            target_link_libraries(${name} PRIVATE reunion_diff)
        elseif(name MATCHES "^test_play_")
            target_link_libraries(${name} PRIVATE reunion_play)
        else()
            target_link_libraries(${name} PRIVATE reunion_core)
        endif()
        add_test(NAME ${name} COMMAND ${name} "${REUNION_DATA_DIR}")
        set_tests_properties(${name} PROPERTIES SKIP_RETURN_CODE 77)
        if(name MATCHES "^test_play_")
            # Recordings are also replayed by the game itself (reunion --replay).
            add_dependencies(${name} reunion)
            set_tests_properties(${name} PROPERTIES
                ENVIRONMENT "REUNION_BIN=$<TARGET_FILE:reunion>")
        endif()
    endforeach()
endif()
