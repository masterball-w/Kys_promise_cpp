#include "GameManager.h"
#include <iostream>

int main(int argc, char* argv[]) {
    std::cout << "Starting KYS C++ Refactor Project..." << std::endl;
    
    GameManager& game = GameManager::getInstance();
    
    // Initialize the engine and load data
    if (!game.Init()) {
        std::cerr << "Game Initialization Failed!" << std::endl;
        return -1;
    }
    
    // Enter the main game loop
    game.Run();
    
    // Clean up
    game.Quit();
    
    return 0;
}
