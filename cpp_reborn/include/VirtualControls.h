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
 * Tracks held state for movement; discrete taps via consumeTap() (no SDL_PushEvent).
 */
bool handleEvent(SDL_Event& e);

/** Virtual key held (merge with SDL_GetKeyboardState for continuous move). */
bool isScancodeDown(SDL_Scancode sc);

/** One-shot tap from virtual pad (survives quick taps between frames). */
bool consumeTap(SDL_Scancode sc);

/** Clear pending tap latches (call after scene transitions / FlushEvents). */
void clearTapLatches();

/** Release all held virtual keys without synthesizing keyboard events. */
void releaseAll();

/** Draw overlay in logical 640×480 space. */
void draw(SDL_Renderer* renderer);

/** draw() then SDL_RenderPresent. Prefer this over bare Present. */
void present(SDL_Renderer* renderer);

}  // namespace VirtualControls
