#include <iostream>
#include "EventManager.h"
#include "GameManager.h"
#include "SceneManager.h"
#include "FileLoader.h"

int main() {
    std::cout << "=== Testing Scene Trigger Logic ===" << std::endl;
    
    // 1. Init (Minimal)
    // We skip full GameManager::Init because it creates Window/Renderer which might fail in some envs
    // But we need FileLoader and SceneManager initialized.
    
    // 1. Load Data via GameManager (loads ranger.grp/idx and alldef.grp)
    std::cout << "Loading Game Data via GameManager..." << std::endl;
    GameManager::getInstance().loadData("../UPedit/save/");
    
    // Check if Scene 0 is loaded
    Scene* s0 = SceneManager::getInstance().GetScene(0);
    if (s0) {
        std::cout << "Scene 0 Loaded successfully." << std::endl;
        std::cout << "Scene 0 Exits:" << std::endl;
        for(int i=0; i<3; ++i) {
             std::cout << "  Exit " << i << ": (" << s0->getExitX(i) << ", " << s0->getExitY(i) << ")" << std::endl;
        }
    } else {
        std::cerr << "Scene 0 NOT loaded. loadData might have failed." << std::endl;
        // Fallback manual load if needed (omitted for now)
    }

    // Load SData manually (loadData doesn't load SData for all scenes, usually on demand or SceneManager::LoadMapData)
    // Actually GameManager calls LoadMapData? No, SceneManager::DrawScene loads it?
    // Let's load it manually for the test.
    if (!SceneManager::getInstance().LoadMapData("../UPedit/save/allsin.grp")) {
          std::cerr << "Failed to load allsin.grp. Try alternative path..." << std::endl;
          if (!SceneManager::getInstance().LoadMapData("resource/allsin.grp")) {
              std::cerr << "Failed to load allsin.grp (fallback). Test might fail." << std::endl;
          }
    }

    int testSceneId = 0;
    int testX = 10;
    int testY = 10;
    int testEventIndex = 1;
    int testScriptId = 9999;

    int hallX = 28;
    int hallY = 14;
    std::cout << "Scene " << testSceneId << " Layer 3 at (" << hallX << "," << hallY << "): "
              << SceneManager::getInstance().GetSceneTile(testSceneId, 3, hallX, hallY) << std::endl;
    for (int e = 0; e < 200; ++e) {
        int16_t ex = SceneManager::getInstance().GetEventData(testSceneId, e, 10);
        int16_t ey = SceneManager::getInstance().GetEventData(testSceneId, e, 9);
        if ((ex == hallX && ey == hallY) || (ex == hallY && ey == hallX)) {
            int16_t pic = SceneManager::getInstance().GetEventData(testSceneId, e, 5);
            std::cout << "Event " << e << " at (" << ex << "," << ey << ") Pic=" << pic << std::endl;
        }
    }
    
    // Inject Event at (10, 10) in Layer 3
    SceneManager::getInstance().SetSceneTile(testSceneId, 3, testX, testY, testEventIndex);
    
    // Set Script ID for Event 1 to 9999
    // Index 4 is Script ID
    SceneManager::getInstance().SetEventData(testSceneId, testEventIndex, 4, testScriptId);
    
    std::cout << "Injecting Event " << testEventIndex << " at (" << testX << "," << testY << ") with Script " << testScriptId << std::endl;
    
    // Verify Injection
    int16_t val = SceneManager::getInstance().GetSceneTile(testSceneId, 3, testX, testY);
    std::cout << "Read back Layer 3 at (" << testX << "," << testY << "): " << val << std::endl;
    
    int16_t script = SceneManager::getInstance().GetEventData(testSceneId, testEventIndex, 4);
    std::cout << "Read back Script ID for Event " << testEventIndex << ": " << script << std::endl;

    // 3. Call Instruct_JmpScene
    std::cout << "Calling Instruct_JmpScene(" << testSceneId << ", " << testX << ", " << testY << ")..." << std::endl;
    
    // Note: Instruct_JmpScene calls GameManager functions which might rely on SDL
    // But we didn't init SDL.
    // GameManager::setMainMapPosition just sets ints.
    // Instruct_Redraw calls UIManager::UpdateScreen which might crash if no Renderer.
    // But we can try.
    
    try {
        EventManager::getInstance().Instruct_JmpScene(testSceneId, testX, testY);
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Unknown Exception (likely SDL nullptr access)" << std::endl;
    }
    
    std::cout << "=== Test Finished ===" << std::endl;
    return 0;
}
