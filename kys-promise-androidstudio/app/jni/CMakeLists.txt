# Android NDK entry for Kys Promise (SDL3 + shared engine from cpp_reborn).
# PC builds do NOT use this file — use cpp_reborn/CMakeLists.txt directly.
#
# Layout (scheme A — one engine source tree):
#   kys-promise-androidstudio/app/jni/CMakeLists.txt  (this file)
#   ../../cpp_reborn/                                 (shared game code)
#
# First configure will FetchContent SDL3 / SDL3_ttf / SDL3_image (needs network).

cmake_minimum_required(VERSION 3.22)
project(kys_android_jni C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows: SDL_image vendored deps exceed MAX_PATH under deep .cxx/_deps trees.
if(WIN32 AND NOT FETCHCONTENT_BASE_DIR)
    set(FETCHCONTENT_BASE_DIR "C:/cmake_fc" CACHE PATH "Short FetchContent root for Windows")
endif()

include(FetchContent)

set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.2.20
    GIT_SHALLOW TRUE
    EXCLUDE_FROM_ALL
)
FetchContent_Declare(
    SDL3_ttf
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
    GIT_TAG release-3.2.2
    GIT_SHALLOW TRUE
    EXCLUDE_FROM_ALL
)
FetchContent_Declare(
    SDL3_image
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
    GIT_TAG release-3.2.4
    GIT_SHALLOW TRUE
    EXCLUDE_FROM_ALL
)

FetchContent_MakeAvailable(SDL3)

set(SDL3IMAGE_VENDORED ON CACHE BOOL "" FORCE)
set(SDL3TTF_VENDORED ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SDL3_image SDL3_ttf)

# Shared engine → libkys_promise.so
set(KYS_ENGINE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../cpp_reborn")
if(NOT EXISTS "${KYS_ENGINE_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR "cpp_reborn not found at ${KYS_ENGINE_ROOT}")
endif()
add_subdirectory("${KYS_ENGINE_ROOT}" kys_engine_build)
