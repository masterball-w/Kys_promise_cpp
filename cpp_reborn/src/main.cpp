#include "GameManager.h"
#include <iostream>
#include <SDL3/SDL_main.h>

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    std::cout << "Starting KYS C++ Refactor Project..." << std::endl;
    
    GameManager& game = GameManager::getInstance();
    
    if (!game.Init()) {
        std::cerr << "Game Initialization Failed!" << std::endl;
        return -1;
    }
    
    game.Run();
    game.Quit();
    
    return 0;
}
