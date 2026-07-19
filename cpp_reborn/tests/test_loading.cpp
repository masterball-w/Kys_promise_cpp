#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include "SceneManager.h"
#include "FileLoader.h"
#include "GameManager.h"

// Mock GameManager for testing SceneManager purely
// Since SceneManager uses GameManager singleton, we need to ensure it doesn't crash.
// But we are linking against the real GameManager.cpp, so we just need to ensure Init() isn't called or is partial.

void TestFileExists(const std::string& path) {
    std::ifstream f(path.c_str());
    if (f.good()) {
        std::cout << "[PASS] File found: " << path << std::endl;
    } else {
        std::cout << "[FAIL] File NOT found: " << path << std::endl;
    }
}

int main(int argc, char* argv[]) {
    std::cout << "=== KYS Loading Test ===" << std::endl;
    
    // 1. Test File Paths
    std::cout << "\n--- Checking Resource Files ---" << std::endl;
    TestFileExists("resource/smp");
    TestFileExists("resource/sdx");
    TestFileExists("resource/mmap.grp");
    TestFileExists("resource/mmap.idx");
    TestFileExists("resource/cloud.grp");
    TestFileExists("resource/cloud.idx");
    TestFileExists("../resource/smp"); // Check fallback path
    
    std::cout << "\n--- Checking Save Files ---" << std::endl;
    TestFileExists("save/alldef.grp");
    TestFileExists("save/allsin.grp");
    TestFileExists("../save/alldef.grp");
    
    // 2. Test SceneManager Init (Resource Loading)
    std::cout << "\n--- Testing SceneManager::Init (Resource Load) ---" << std::endl;
    if (SceneManager::getInstance().Init()) {
        std::cout << "[PASS] SceneManager::Init() returned true." << std::endl;
    } else {
        std::cout << "[FAIL] SceneManager::Init() returned false." << std::endl;
    }
    
    // 3. Test Scene Data Loading
    std::cout << "\n--- Testing Scene Data Load ---" << std::endl;
    // Try standard path
    if (SceneManager::getInstance().LoadEventData("save/alldef.grp")) {
        std::cout << "[PASS] Loaded save/alldef.grp" << std::endl;
    } else if (SceneManager::getInstance().LoadEventData("../save/alldef.grp")) {
        std::cout << "[PASS] Loaded ../save/alldef.grp" << std::endl;
    } else {
        std::cout << "[FAIL] Failed to load alldef.grp from standard paths" << std::endl;
    }
    
    if (SceneManager::getInstance().LoadMapData("save/allsin.grp")) {
        std::cout << "[PASS] Loaded save/allsin.grp" << std::endl;
    } else if (SceneManager::getInstance().LoadMapData("../save/allsin.grp")) {
        std::cout << "[PASS] Loaded ../save/allsin.grp" << std::endl;
    } else {
        std::cout << "[FAIL] Failed to load allsin.grp from standard paths" << std::endl;
    }
    
    // 4. Test Accessors (Crash Check)
    std::cout << "\n--- Testing Data Access (Crash Check) ---" << std::endl;
    int16_t tile = SceneManager::getInstance().GetSceneTile(70, 0, 20, 20);
    std::cout << "GetSceneTile(70, 0, 20, 20) = " << tile << std::endl;
    
    int16_t event = SceneManager::getInstance().GetEventData(70, 0, 0);
    std::cout << "GetEventData(70, 0, 0) = " << event << std::endl;
    
    std::cout << "\n=== Test Finished ===" << std::endl;
    return 0;
}
