cmake_minimum_required(VERSION 3.25)

project(reunion
    VERSION 0.1.0
    DESCRIPTION "Native port of Reunion (Amnesty Design, 1994)"
    LANGUAGES C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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

# --- Game -------------------------------------------------------------------
add_executable(reunion
    src/main.c
    src/platform/video.c)

target_include_directories(reunion PRIVATE src)
target_link_libraries(reunion PRIVATE SDL3::SDL3)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(reunion PRIVATE -Wall -Wextra -Wpedantic)
endif()
