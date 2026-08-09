#pragma once

#include <SDL3/SDL.h>
#include <array>

// Centralized input state to avoid scattered SDL_PollEvent loops.
class InputManager {
public:
    static InputManager& getInstance();

    void BeginFrame();
    void ProcessEvent(const SDL_Event& event);

    bool IsKeyDown(SDL_Scancode sc) const;
    bool WasKeyPressed(SDL_Scancode sc) const;
    bool WasKeyReleased(SDL_Scancode sc) const;

    void FlushEvents();

    /** Returns true when KEY_UP should advance dialogue (Pascal NewTalk parity). */
    static bool IsDialogueAdvanceKey(const SDL_Event& event, uint32_t openTimeMs, uint32_t debounceMs = 280);

private:
    InputManager() = default;

    std::array<bool, SDL_SCANCODE_COUNT> m_current{};
    std::array<bool, SDL_SCANCODE_COUNT> m_pressed{};
    std::array<bool, SDL_SCANCODE_COUNT> m_released{};
};
