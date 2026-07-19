#include <iostream>
#include <vector>
#include "EventManager.h"
#include "GameManager.h"

// Simple mock for testing without full graphics
int main() {
    std::cout << "=== Testing Event Script 1853 ===" << std::endl;

    // 1. Initialize EventManager (Load Scripts)
    if (!EventManager::getInstance().LoadScripts()) {
        std::cerr << "[FAIL] Failed to load kdef scripts" << std::endl;
        return 1;
    }
    std::cout << "[PASS] Scripts loaded." << std::endl;

    // Print event script 1853
    std::cout << "\n=== Printing Event Script 1853 ===" << std::endl;
    EventManager::getInstance().PrintEventScript(1853);
    
    std::cout << "\n=== Test Finished ===" << std::endl;
    return 0;
}
