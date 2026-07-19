#include "InputManager.h"

InputManager& InputManager::getInstance() {
    static InputManager instance;
    return instance;
}

void InputManager::BeginFrame() {
    m_pressed.fill(false);
    m_released.fill(false);
}

void InputManager::ProcessEvent(const SDL_Event& event) {
    if (event.type == SDL_EVENT_KEY_DOWN) {
        SDL_Scancode sc = event.key.scancode;
        if (sc >= 0 && sc < SDL_SCANCODE_COUNT) {
            if (!m_current[sc]) m_pressed[sc] = true;
            m_current[sc] = true;
        }
    } else if (event.type == SDL_EVENT_KEY_UP) {
        SDL_Scancode sc = event.key.scancode;
        if (sc >= 0 && sc < SDL_SCANCODE_COUNT) {
            m_current[sc] = false;
            m_released[sc] = true;
        }
    }
}

bool InputManager::IsKeyDown(SDL_Scancode sc) const {
    if (sc < 0 || sc >= SDL_SCANCODE_COUNT) return false;
    return m_current[sc];
}

bool InputManager::WasKeyPressed(SDL_Scancode sc) const {
    if (sc < 0 || sc >= SDL_SCANCODE_COUNT) return false;
    return m_pressed[sc];
}

bool InputManager::WasKeyReleased(SDL_Scancode sc) const {
    if (sc < 0 || sc >= SDL_SCANCODE_COUNT) return false;
    return m_released[sc];
}

void InputManager::FlushEvents() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_EVENT_QUIT) {
            ProcessEvent(e);
        }
    }
    BeginFrame();
}
