cmake_minimum_required(VERSION 3.18)
project(kys_cpp CXX)

# Dual-target engine:
#   Desktop (default): executable kys_cpp + optional tests/tools
#   Android (ANDROID=ON via NDK): shared library kys_promise for SDLActivity
#
# Android Studio loads this via kys-promise-androidstudio/app/jni/CMakeLists.txt
# which FetchContent's SDL3* then add_subdirectory(cpp_reborn).

set(VCPKG_APPLOCAL_DEPS OFF CACHE BOOL "" FORCE)
if(MSVC)
    set(CMAKE_VS_GLOBALS "VcpkgApplocalDeps=false")
endif()

function(disable_vcpkg_applocal targetName)
    if(MSVC)
        set_property(TARGET ${targetName} PROPERTY VS_GLOBAL_VcpkgApplocalDeps "false")
    endif()
endfunction()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W3 /utf-8)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra)
endif()

option(KYS_BUILD_TESTS "Build desktop test/tool executables" ON)
if(ANDROID)
    set(KYS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
endif()

# ---- SDL3 discovery ----
# On Android, parent jni/CMakeLists is expected to provide SDL3::SDL3 etc.
# On desktop, use vendored Windows/prebuilt packages next to the repo.
if(NOT ANDROID)
    set(LOCAL_SDL3_ROOT "${CMAKE_SOURCE_DIR}/../SDL3-3.4.0")
    set(LOCAL_SDL3_TTF_ROOT "${CMAKE_SOURCE_DIR}/../SDL3_ttf-3.1.0")
    set(LOCAL_SDL3_IMAGE_ROOT "${CMAKE_SOURCE_DIR}/../SDL3_image-3.2.6")

    if(NOT DEFINED SDL3_DIR)
        set(SDL3_DIR "${LOCAL_SDL3_ROOT}/cmake" CACHE PATH "Path to SDL3 CMake config")
    endif()
    if(NOT DEFINED SDL3_ttf_DIR)
        set(SDL3_ttf_DIR "${LOCAL_SDL3_TTF_ROOT}/cmake" CACHE PATH "Path to SDL3_ttf CMake config")
    endif()
    if(NOT DEFINED SDL3_image_DIR)
        set(SDL3_image_DIR "${LOCAL_SDL3_IMAGE_ROOT}/cmake" CACHE PATH "Path to SDL3_image CMake config")
    endif()

    set(LOCAL_SDL2_MIXER_ROOT "${CMAKE_SOURCE_DIR}/../SDL2_mixer-2.8.1")
    option(USE_SDL2_MIXER "Use SDL2_mixer for music playback" OFF)
    if(USE_SDL2_MIXER AND EXISTS "${LOCAL_SDL2_MIXER_ROOT}/include/SDL_mixer.h")
        include_directories("${LOCAL_SDL2_MIXER_ROOT}/include")
        if(WIN32)
            link_directories("${LOCAL_SDL2_MIXER_ROOT}/lib/x64")
        endif()
        add_compile_definitions(USE_SDL2_MIXER)
    endif()

    find_package(SDL3 REQUIRED)
    find_package(SDL3_ttf REQUIRED)
    find_package(SDL3_image REQUIRED)

    include_directories("${LOCAL_SDL3_ROOT}/include")
    include_directories("${LOCAL_SDL3_TTF_ROOT}/include")
    include_directories("${LOCAL_SDL3_IMAGE_ROOT}/include")
else()
    # Targets must already exist (created by jni/CMakeLists.txt FetchContent)
    if(NOT TARGET SDL3::SDL3)
        message(FATAL_ERROR "Android build requires SDL3::SDL3 (provide via jni/CMakeLists.txt)")
    endif()
endif()

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")

# ---- Engine sources (shared by PC + Android) ----
# GbkCodec.cpp: built-in GBK transcoder for Android (no OS iconv).
file(GLOB_RECURSE KYS_ENGINE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE KYS_ENGINE_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")
list(FILTER KYS_ENGINE_SOURCES EXCLUDE REGEX ".*dump_header\\.cpp$")
list(FILTER KYS_ENGINE_SOURCES EXCLUDE REGEX ".*save_inspector\\.cpp$")
list(FILTER KYS_ENGINE_SOURCES EXCLUDE REGEX ".*analyze_data\\.cpp$")

set(KYS_SDL_LIBS SDL3::SDL3 SDL3_ttf::SDL3_ttf SDL3_image::SDL3_image)

if(ANDROID)
    # SDLActivity loads libkys_promise.so (see getLibraries)
    add_library(kys_promise SHARED ${KYS_ENGINE_SOURCES} ${KYS_ENGINE_HEADERS})
    target_link_libraries(kys_promise PRIVATE ${KYS_SDL_LIBS} android log)
    # Prefer hidden visibility; SDL_main is exported via SDL_main.h linkage
    target_compile_definitions(kys_promise PRIVATE KYS_ANDROID=1)
else()
    add_executable(kys_cpp ${KYS_ENGINE_SOURCES} ${KYS_ENGINE_HEADERS})
    disable_vcpkg_applocal(kys_cpp)
    target_link_libraries(kys_cpp PRIVATE ${KYS_SDL_LIBS})
    if(WIN32)
        target_link_libraries(kys_cpp PRIVATE winmm)
        if(USE_SDL2_MIXER)
            target_link_libraries(kys_cpp PRIVATE SDL2_mixer)
        endif()
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
endif()

# ---- Desktop-only tests / tools ----
if(KYS_BUILD_TESTS)
    set(KYS_CORE_SRCS
        src/SceneManager.cpp src/FileLoader.cpp src/GraphicsUtils.cpp
        src/GameManager.cpp src/TextManager.cpp src/PicLoader.cpp
        src/BattleManager.cpp src/BattleRole.cpp src/EventManager.cpp
        src/UIManager.cpp src/SoundManager.cpp src/GameHooks.cpp src/WarData.cpp
        src/PlatformCompat.cpp src/InputManager.cpp src/LittleGameManager.cpp
        src/VirtualControls.cpp
    )

    function(kys_add_test name)
        add_executable(${name} ${ARGN})
        disable_vcpkg_applocal(${name})
        target_link_libraries(${name} PRIVATE ${KYS_SDL_LIBS})
        if(WIN32)
            target_link_libraries(${name} PRIVATE winmm)
        endif()
    endfunction()

    kys_add_test(test_loading tests/test_loading.cpp ${KYS_CORE_SRCS})
    kys_add_test(test_event tests/test_event.cpp ${KYS_CORE_SRCS})
    kys_add_test(test_50e tests/test_50e.cpp ${KYS_CORE_SRCS})
    kys_add_test(test_placeholder tests/test_placeholder.cpp)
    kys_add_test(test_scene_trigger tests/test_scene_trigger.cpp ${KYS_CORE_SRCS})
    kys_add_test(test_battle tests/test_battle.cpp ${KYS_CORE_SRCS})
    kys_add_test(test_menu tests/test_menu.cpp ${KYS_CORE_SRCS})

    if(WIN32)
        foreach(dll_target test_menu)
            add_custom_command(TARGET ${dll_target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${LOCAL_SDL3_ROOT}/lib/x64/SDL3.dll"
                $<TARGET_FILE_DIR:${dll_target}>)
            add_custom_command(TARGET ${dll_target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${LOCAL_SDL3_TTF_ROOT}/lib/x64/SDL3_ttf.dll"
                $<TARGET_FILE_DIR:${dll_target}>)
            add_custom_command(TARGET ${dll_target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${LOCAL_SDL3_IMAGE_ROOT}/lib/x64/SDL3_image.dll"
                $<TARGET_FILE_DIR:${dll_target}>)
        endforeach()
    endif()

    add_executable(dump_events tests/dump_events.cpp src/FileLoader.cpp src/PlatformCompat.cpp)
    disable_vcpkg_applocal(dump_events)
    target_link_libraries(dump_events PRIVATE SDL3::SDL3)

    add_executable(dump_ddata tests/dump_ddata.cpp src/FileLoader.cpp src/PlatformCompat.cpp)
    disable_vcpkg_applocal(dump_ddata)
    target_link_libraries(dump_ddata PRIVATE SDL3::SDL3)

    add_executable(save_inspector src/save_inspector.cpp)
    disable_vcpkg_applocal(save_inspector)

    add_executable(analyze_data src/analyze_data.cpp)
    disable_vcpkg_applocal(analyze_data)
endif()
