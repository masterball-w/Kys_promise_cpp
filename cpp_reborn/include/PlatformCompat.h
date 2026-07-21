#pragma once
#include <SDL3/SDL.h>
#include <string>

// Cross-platform helpers for Android / non-Windows builds.
namespace PlatformCompat {

/** Discover writable game-data root (contains resource/ and save/). */
std::string discoverDataRoot();

/** Writable save directory ending with '/'. */
std::string discoverSaveDir(const std::string& dataRoot);

/** Install SDL event filter: Android BACK → Escape + right-click cancel. */
void installInputCompat();

/** Transform a single event in-place (BACK → cancel). Returns true if kept. */
bool transformEvent(SDL_Event& e);

}  // namespace PlatformCompat
