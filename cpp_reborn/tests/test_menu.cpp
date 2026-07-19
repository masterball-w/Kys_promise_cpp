#include "GameManager.h"
#include "UIManager.h"
#include "SceneManager.h"
#include <iostream>

// Standalone Menu Test Program
// Allows debugging the Menu System without running the full game loop
int main(int argc, char* argv[]) {
    std::cout << "[TestMenu] Starting Independent Menu Test..." << std::endl;
    
    GameManager& game = GameManager::getInstance();
    
    // Initialize Engine (creates window, renderer, etc.)
    if (!game.Init()) {
        std::cerr << "[TestMenu] Initialization Failed!" << std::endl;
        return -1;
    }

    std::cout << "[TestMenu] Initialization Successful. Setting up Scene..." << std::endl;

    // Ensure System Graphics are loaded
    UIManager::getInstance().LoadSystemGraphics();
    
    // Load a scene to verify transparency (Scene 0: Temple)
    // We need to set camera position so something is drawn
    game.enterScene(0); 
    game.setCameraPosition(38, 38);
    
    // Force a draw once to populate the screen with the scene
    SDL_Renderer* renderer = game.getRenderer();
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SceneManager::getInstance().DrawScene(renderer, 38, 38);
    game.RenderScreenTo(renderer);
    SDL_RenderPresent(renderer);
    
    std::cout << "[TestMenu] Scene drawn. Opening Menu..." << std::endl;

    // Open the Menu directly
    // This blocks until the menu is closed
    UIManager::getInstance().ShowMenu();
    
    std::cout << "[TestMenu] Menu closed. Exiting..." << std::endl;

    // Clean up
    game.Quit();
    
    return 0;
}
