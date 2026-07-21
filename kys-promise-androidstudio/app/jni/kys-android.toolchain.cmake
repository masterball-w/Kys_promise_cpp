# Windows NDK workaround: stock clang++.exe defaults to windows-gnu, so CMake
# try_compile links fail on Android-only lld flags (--no-rosegment).
# Append an explicit --target after the NDK toolchain initializes flags.
#
# try_compile re-includes this file without -DANDROID_NDK; persist NDK path.

set(_KYS_NDK_STAMP "${CMAKE_CURRENT_LIST_DIR}/.kys-ndk-abs")
if(EXISTS "${_KYS_NDK_STAMP}")
  file(READ "${_KYS_NDK_STAMP}" KYS_ANDROID_NDK_ABS)
  string(STRIP "${KYS_ANDROID_NDK_ABS}" KYS_ANDROID_NDK_ABS)
endif()
if(NOT KYS_ANDROID_NDK_ABS)
  if(CMAKE_ANDROID_NDK)
    get_filename_component(KYS_ANDROID_NDK_ABS "${CMAKE_ANDROID_NDK}" ABSOLUTE)
  elseif(ANDROID_NDK)
    get_filename_component(KYS_ANDROID_NDK_ABS "${ANDROID_NDK}" ABSOLUTE)
  elseif(DEFINED ENV{ANDROID_NDK})
    get_filename_component(KYS_ANDROID_NDK_ABS "$ENV{ANDROID_NDK}" ABSOLUTE)
  else()
    message(FATAL_ERROR "kys-android.toolchain: set ANDROID_NDK / CMAKE_ANDROID_NDK")
  endif()
  file(WRITE "${_KYS_NDK_STAMP}" "${KYS_ANDROID_NDK_ABS}")
endif()

include("${KYS_ANDROID_NDK_ABS}/build/cmake/android.toolchain.cmake")

if(CMAKE_HOST_WIN32)
  set(_KYS_API "${ANDROID_PLATFORM_LEVEL}")
  if(NOT _KYS_API)
    set(_KYS_API 29)
  endif()
  if(ANDROID_ABI STREQUAL "armeabi-v7a")
    set(_KYS_TARGET "armv7a-linux-androideabi${_KYS_API}")
  elseif(ANDROID_ABI STREQUAL "x86_64")
    set(_KYS_TARGET "x86_64-linux-android${_KYS_API}")
  elseif(ANDROID_ABI STREQUAL "x86")
    set(_KYS_TARGET "i686-linux-android${_KYS_API}")
  else()
    set(_KYS_TARGET "aarch64-linux-android${_KYS_API}")
  endif()

  foreach(_kys_var CMAKE_C_FLAGS CMAKE_CXX_FLAGS CMAKE_ASM_FLAGS
                   CMAKE_EXE_LINKER_FLAGS CMAKE_SHARED_LINKER_FLAGS CMAKE_MODULE_LINKER_FLAGS)
    if(NOT "${${_kys_var}}" MATCHES "--target=")
      set(${_kys_var} "${${_kys_var}} --target=${_KYS_TARGET}")
      set(${_kys_var} "${${_kys_var}}" CACHE STRING "" FORCE)
    endif()
  endforeach()
  message(STATUS "KYS Android --target=${_KYS_TARGET}")
endif()
