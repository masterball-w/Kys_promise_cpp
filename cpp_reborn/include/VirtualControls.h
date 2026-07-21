#pragma once
#include <SDL3/SDL.h>

// On-screen touch controls (Android / optional desktop):
//   bottom-left D-pad → arrow keys
//   top-right Esc
//   bottom-right OK → Space (confirm)
namespace VirtualControls {

/** Enabled by default on Android; set KYS_VIRTUAL_PAD=1 to force on desktop. */
void init();
void setRenderer(SDL_Renderer* renderer);
void setEnabled(bool on);
bool isEnabled();

/**
 * Handle pointer/finger events. Returns false if consumed (do not pass to game UI).
 * Synthesizes KEY_DOWN/KEY_UP for arrows and Escape; tracks hold for continuous move.
 */
bool handleEvent(SDL_Event& e);

/** Virtual key held (merge with SDL_GetKeyboardState for continuous move). */
bool isScancodeDown(SDL_Scancode sc);

/** Draw overlay in logical 640×480 space. */
void draw(SDL_Renderer* renderer);

/** draw() then SDL_RenderPresent. Prefer this over bare Present. */
void present(SDL_Renderer* renderer);

}  // namespace VirtualControls
