cmake_minimum_required(VERSION 3.10)
project(test_suite)

set(CMAKE_CXX_STANDARD 17)

include_directories(../include)
include_directories(../../SDL3-3.4.0/include)

add_executable(test_scene_trigger test_scene_trigger.cpp 
    ../src/GameManager.cpp 
    ../src/SceneManager.cpp 
    ../src/EventManager.cpp 
    ../src/FileLoader.cpp 
    ../src/PicLoader.cpp
    ../src/UIManager.cpp
    ../src/GraphicsUtils.cpp
    ../src/SoundManager.cpp
    ../src/TextManager.cpp
    ../src/BattleManager.cpp
    ../src/BattleRole.cpp
    ../src/WarData.cpp
)

find_library(SDL3_LIB SDL3 NAMES SDL3 PATHS "../../SDL3-3.4.0/lib/x64" "../../SDL3-3.4.0/lib")
if(SDL3_LIB)
    target_link_libraries(test_scene_trigger ${SDL3_LIB})
else()
    message(WARNING "SDL3 library not found, test_scene_trigger might fail to link.")
endif()
