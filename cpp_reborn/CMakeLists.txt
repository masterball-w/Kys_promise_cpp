cmake_minimum_required(VERSION 3.15)
project(kys_cpp CXX)

# vcpkg's applocal step calls pwsh.exe on some setups; disable it to avoid build failures.
set(VCPKG_APPLOCAL_DEPS OFF CACHE BOOL "" FORCE)
if(MSVC)
    set(CMAKE_VS_GLOBALS "VcpkgApplocalDeps=false")
endif()

function(disable_vcpkg_applocal targetName)
    if(MSVC)
        set_property(TARGET ${targetName} PROPERTY VS_GLOBAL_VcpkgApplocalDeps "false")
    endif()
endfunction()

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Platform-specific compiler flags
if(MSVC)
    add_compile_options(/W3 /utf-8)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra)
endif()

# SDL3 & SDL3_ttf Configuration
# Default paths for this specific environment (Updated for current workspace)
set(LOCAL_SDL3_ROOT "${CMAKE_SOURCE_DIR}/../SDL3-3.4.0")
set(LOCAL_SDL3_TTF_ROOT "${CMAKE_SOURCE_DIR}/../SDL3_ttf-3.1.0")
set(LOCAL_SDL3_IMAGE_ROOT "${CMAKE_SOURCE_DIR}/../SDL3_image-3.2.6")

# Hint for find_package
if(NOT DEFINED SDL3_DIR)
    set(SDL3_DIR "${LOCAL_SDL3_ROOT}/cmake" CACHE PATH "Path to SDL3 CMake config")
endif()

if(NOT DEFINED SDL3_ttf_DIR)
    set(SDL3_ttf_DIR "${LOCAL_SDL3_TTF_ROOT}/cmake" CACHE PATH "Path to SDL3_ttf CMake config")
endif()

if(NOT DEFINED SDL3_image_DIR)
    set(SDL3_image_DIR "${LOCAL_SDL3_IMAGE_ROOT}/cmake" CACHE PATH "Path to SDL3_image CMake config")
endif()

# SDL2_mixer (optional, for cross-platform music when available)
set(LOCAL_SDL2_MIXER_ROOT "${CMAKE_SOURCE_DIR}/../SDL2_mixer-2.8.1")
option(USE_SDL2_MIXER "Use SDL2_mixer for music playback" OFF)
if(USE_SDL2_MIXER AND EXISTS "${LOCAL_SDL2_MIXER_ROOT}/include/SDL_mixer.h")
    include_directories("${LOCAL_SDL2_MIXER_ROOT}/include")
    if(WIN32)
        link_directories("${LOCAL_SDL2_MIXER_ROOT}/lib/x64")
    endif()
    add_compile_definitions(USE_SDL2_MIXER)
endif()

# Find SDL3 packages
find_package(SDL3 REQUIRED)
find_package(SDL3_ttf REQUIRED)
find_package(SDL3_image REQUIRED)

# Include directories
include_directories(include)
include_directories("${LOCAL_SDL3_ROOT}/include")
include_directories("${LOCAL_SDL3_TTF_ROOT}/include")
include_directories("${LOCAL_SDL3_IMAGE_ROOT}/include")

# Source and Header files
file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE HEADERS "include/*.h")
list(FILTER SOURCES EXCLUDE REGEX ".*dump_header\\.cpp$")
list(FILTER SOURCES EXCLUDE REGEX ".*save_inspector\\.cpp$")
list(FILTER SOURCES EXCLUDE REGEX ".*analyze_data\\.cpp$")

# Add executable
add_executable(kys_cpp ${SOURCES} ${HEADERS})
disable_vcpkg_applocal(kys_cpp)

# Test Executables
add_executable(test_loading tests/test_loading.cpp src/SceneManager.cpp src/FileLoader.cpp src/GraphicsUtils.cpp src/GameManager.cpp src/TextManager.cpp src/PicLoader.cpp src/BattleManager.cpp src/BattleRole.cpp src/EventManager.cpp src/UIManager.cpp src/SoundManager.cpp src/WarData.cpp)
disable_vcpkg_applocal(test_loading)
target_link_libraries(test_loading PRIVATE SDL3::SDL3 SDL3_ttf::SDL3_ttf SDL3_image::SDL3_image)
if(WIN32)
    target_link_libraries(test_loading PRIVATE winmm)
endif()

add_executable(test_event tests/test_event.cpp src/SceneManager.cpp src/FileLoader.cpp src/GraphicsUtils.cpp src/GameManager.cpp src/TextManager.cpp src/PicLoader.cpp src/BattleManager.cpp src/BattleRole.cpp src/EventManager.cpp src/UIManager.cpp src/SoundManager.cpp src/WarData.cpp)
disable_vcpkg_applocal(test_event)
target_link_libraries(test_event PRIVATE SDL3::SDL3 SDL3_ttf::SDL3_ttf SDL3_image::SDL3_image)
if(WIN32)
    target_link_libraries(test_event PRIVATE winmm)
endif()

add_executable(test_placeholder tests/test_placeholder.cpp)
disable_vcpkg_applocal(test_placeholder)

add_executable(test_scene_trigger tests/test_scene_trigger.cpp src/SceneManager.cpp src/FileLoader.cpp src/GraphicsUtils.cpp src/GameManager.cpp src/TextManager.cpp src/PicLoader.cpp src/BattleManager.cpp src/BattleRole.cpp src/EventManager.cpp src/UIManager.cpp src/SoundManager.cpp src/WarData.cpp)
disable_vcpkg_applocal(test_scene_trigger)
target_link_libraries(test_scene_trigger PRIVATE SDL3::SDL3 SDL3_ttf::SDL3_ttf SDL3_image::SDL3_image)
if(WIN32)
    target_link_libraries(test_scene_trigger PRIVATE winmm)
endif()

add_executable(test_battle tests/test_battle.cpp src/SceneManager.cpp src/FileLoader.cpp src/GraphicsUtils.cpp src/GameManager.cpp src/TextManager.cpp src/PicLoader.cpp src/BattleManager.cpp src/BattleRole.cpp src/EventManager.cpp src/UIManager.cpp src/SoundManager.cpp src/WarData.cpp)
disable_vcpkg_applocal(test_battle)
target_link_libraries(test_battle PRIVATE SDL3::SDL3 SDL3_ttf::SDL3_ttf SDL3_image::SDL3_image)
if(WIN32)
    target_link_libraries(test_battle PRIVATE winmm)
endif()

# Independent Menu Test
add_executable(test_menu tests/test_menu.cpp src/SceneManager.cpp src/FileLoader.cpp src/GraphicsUtils.cpp src/GameManager.cpp src/TextManager.cpp src/PicLoader.cpp src/BattleManager.cpp src/BattleRole.cpp src/EventManager.cpp src/UIManager.cpp src/SoundManager.cpp src/WarData.cpp)
disable_vcpkg_applocal(test_menu)
target_link_libraries(test_menu PRIVATE SDL3::SDL3 SDL3_ttf::SDL3_ttf SDL3_image::SDL3_image)
if(WIN32)
    target_link_libraries(test_menu PRIVATE winmm)
    # Copy DLLs for test_menu
    add_custom_command(TARGET test_menu POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${LOCAL_SDL3_ROOT}/lib/x64/SDL3.dll"
        $<TARGET_FILE_DIR:test_menu>)
    add_custom_command(TARGET test_menu POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${LOCAL_SDL3_TTF_ROOT}/lib/x64/SDL3_ttf.dll"
        $<TARGET_FILE_DIR:test_menu>)
    add_custom_command(TARGET test_menu POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${LOCAL_SDL3_IMAGE_ROOT}/lib/x64/SDL3_image.dll"
        $<TARGET_FILE_DIR:test_menu>)
endif()

add_executable(dump_events tests/dump_events.cpp src/FileLoader.cpp)
disable_vcpkg_applocal(dump_events)
target_link_libraries(dump_events PRIVATE)

add_executable(dump_ddata tests/dump_ddata.cpp src/FileLoader.cpp)
disable_vcpkg_applocal(dump_ddata)
target_link_libraries(dump_ddata PRIVATE)

add_executable(save_inspector src/save_inspector.cpp)
disable_vcpkg_applocal(save_inspector)
target_link_libraries(save_inspector PRIVATE)

add_executable(analyze_data src/analyze_data.cpp)
disable_vcpkg_applocal(analyze_data)
target_link_libraries(analyze_data PRIVATE)


# Link libraries
target_link_libraries(kys_cpp PRIVATE SDL3::SDL3 SDL3_ttf::SDL3_ttf SDL3_image::SDL3_image)

# Link Windows Multimedia API (winmm) for MCI audio support
if(WIN32)
    target_link_libraries(kys_cpp PRIVATE winmm)
    if(USE_SDL2_MIXER)
        target_link_libraries(kys_cpp PRIVATE SDL2_mixer)
    endif()
endif()

# Copy DLLs to output directory for debugging
if(WIN32)
    add_custom_command(TARGET kys_cpp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${LOCAL_SDL3_ROOT}/lib/x64/SDL3.dll"
        $<TARGET_FILE_DIR:kys_cpp>)
        
    add_custom_command(TARGET kys_cpp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${LOCAL_SDL3_TTF_ROOT}/lib/x64/SDL3_ttf.dll"
        $<TARGET_FILE_DIR:kys_cpp>)

    add_custom_command(TARGET kys_cpp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${LOCAL_SDL3_IMAGE_ROOT}/lib/x64/SDL3_image.dll"
        $<TARGET_FILE_DIR:kys_cpp>)
endif()
