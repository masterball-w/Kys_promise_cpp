#include "VirtualControls.h"
#include <array>
#include <cstdlib>

namespace {

enum class PadId : int {
    Up = 0,
    Down,
    Left,
    Right,
    Esc,
    Confirm,
    Count
};

constexpr int kLogicalW = 640;
constexpr int kLogicalH = 480;

bool g_enabled = false;
bool g_inited = false;
SDL_Renderer* g_renderer = nullptr;
std::array<bool, static_cast<int>(PadId::Count)> g_held{};
std::array<Sint64, static_cast<int>(PadId::Count)> g_owner{};
std::array<bool, static_cast<int>(PadId::Count)> g_tapLatch{};

constexpr Sint64 kMouseOwner = -100;

struct PadRect {
    float x, y, w, h;
};

PadRect rectFor(PadId id) {
    const float bs = 54.f;
    const float gap = 6.f;
    const float ox = 14.f;
    const float oy = static_cast<float>(kLogicalH) - 14.f - (bs * 3.f + gap * 2.f);
    const float cx = ox + bs + gap;
    const float cy = oy + bs + gap;

    switch (id) {
        case PadId::Up:    return {cx, oy, bs, bs};
        case PadId::Down:  return {cx, cy + bs + gap, bs, bs};
        case PadId::Left:  return {ox, cy, bs, bs};
        case PadId::Right: return {cx + bs + gap, cy, bs, bs};
        case PadId::Esc: {
            const float ew = 72.f;
            const float eh = 48.f;
            return {static_cast<float>(kLogicalW) - 14.f - ew, 14.f, ew, eh};
        }
        case PadId::Confirm: {
            // Bottom-right: confirm (Space) — clear of D-pad and Esc
            const float ew = 88.f;
            const float eh = 56.f;
            return {static_cast<float>(kLogicalW) - 14.f - ew,
                    static_cast<float>(kLogicalH) - 14.f - eh, ew, eh};
        }
        default: return {0, 0, 0, 0};
    }
}

bool hit(const PadRect& r, float x, float y) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

PadId hitTest(float x, float y) {
    if (hit(rectFor(PadId::Esc), x, y)) return PadId::Esc;
    if (hit(rectFor(PadId::Confirm), x, y)) return PadId::Confirm;
    for (int i = 0; i < 4; ++i) {
        if (hit(rectFor(static_cast<PadId>(i)), x, y)) return static_cast<PadId>(i);
    }
    return PadId::Count;
}

SDL_Scancode scancodeOf(PadId id) {
    switch (id) {
        case PadId::Up:      return SDL_SCANCODE_UP;
        case PadId::Down:    return SDL_SCANCODE_DOWN;
        case PadId::Left:    return SDL_SCANCODE_LEFT;
        case PadId::Right:   return SDL_SCANCODE_RIGHT;
        case PadId::Esc:     return SDL_SCANCODE_ESCAPE;
        case PadId::Confirm: return SDL_SCANCODE_SPACE;
        default:             return SDL_SCANCODE_UNKNOWN;
    }
}

void release(PadId id) {
    const int i = static_cast<int>(id);
    if (id == PadId::Count) return;
    if (!g_held[i]) return;
    g_held[i] = false;
    g_owner[i] = -1;
}

void press(PadId id, Sint64 owner) {
    const int i = static_cast<int>(id);
    if (id == PadId::Count) return;
    if (g_held[i]) {
        if (g_owner[i] != owner) return;
        return;
    }
    g_held[i] = true;
    g_owner[i] = owner;
    g_tapLatch[i] = true;
}

void releaseOwner(Sint64 owner) {
    for (int i = 0; i < static_cast<int>(PadId::Count); ++i) {
        if (g_held[i] && g_owner[i] == owner) {
            release(static_cast<PadId>(i));
        }
    }
}

void setDirectionFromPoint(float x, float y, Sint64 owner) {
    PadId hitId = hitTest(x, y);
    if (hitId == PadId::Esc || hitId == PadId::Confirm || hitId == PadId::Count) {
        for (int i = 0; i < 4; ++i) {
            if (g_held[i] && g_owner[i] == owner) {
                release(static_cast<PadId>(i));
            }
        }
        return;
    }
    for (int i = 0; i < 4; ++i) {
        auto pid = static_cast<PadId>(i);
        if (pid == hitId) continue;
        if (g_held[i] && g_owner[i] == owner) {
            release(pid);
        }
    }
    press(hitId, owner);
}

bool convertXY(float inX, float inY, float& outX, float& outY) {
    if (!g_renderer) {
        outX = inX;
        outY = inY;
        return true;
    }
    SDL_Event tmp{};
    tmp.type = SDL_EVENT_MOUSE_MOTION;
    tmp.motion.x = inX;
    tmp.motion.y = inY;
    if (SDL_ConvertEventToRenderCoordinates(g_renderer, &tmp)) {
        outX = tmp.motion.x;
        outY = tmp.motion.y;
    } else {
        outX = inX;
        outY = inY;
    }
    return true;
}

bool fingerToLogical(const SDL_Event& e, float& lx, float& ly) {
    int ww = kLogicalW, wh = kLogicalH;
    SDL_Window* w = SDL_GetWindowFromID(e.tfinger.windowID);
    if (w) SDL_GetWindowSize(w, &ww, &wh);
    const float wx = e.tfinger.x * static_cast<float>(ww);
    const float wy = e.tfinger.y * static_cast<float>(wh);
    if (g_renderer && SDL_RenderCoordinatesFromWindow(g_renderer, wx, wy, &lx, &ly)) {
        return true;
    }
    return convertXY(wx, wy, lx, ly);
}

void drawPad(SDL_Renderer* r, PadId id) {
    const PadRect pr = rectFor(id);
    const int i = static_cast<int>(id);
    const bool on = g_held[i];

    SDL_FRect fr{pr.x, pr.y, pr.w, pr.h};
    if (on) {
        SDL_SetRenderDrawColor(r, 80, 160, 255, 180);
    } else {
        SDL_SetRenderDrawColor(r, 20, 20, 20, 120);
    }
    SDL_RenderFillRect(r, &fr);
    SDL_SetRenderDrawColor(r, 255, 255, 255, on ? 220 : 140);
    SDL_RenderRect(r, &fr);

    SDL_SetRenderDrawColor(r, 255, 255, 255, on ? 255 : 180);
    const float cx = pr.x + pr.w * 0.5f;
    const float cy = pr.y + pr.h * 0.5f;
    const float s = 10.f;
    switch (id) {
        case PadId::Up:
            SDL_RenderLine(r, cx, cy - s, cx - s, cy + s * 0.4f);
            SDL_RenderLine(r, cx, cy - s, cx + s, cy + s * 0.4f);
            SDL_RenderLine(r, cx - s, cy + s * 0.4f, cx + s, cy + s * 0.4f);
            break;
        case PadId::Down:
            SDL_RenderLine(r, cx, cy + s, cx - s, cy - s * 0.4f);
            SDL_RenderLine(r, cx, cy + s, cx + s, cy - s * 0.4f);
            SDL_RenderLine(r, cx - s, cy - s * 0.4f, cx + s, cy - s * 0.4f);
            break;
        case PadId::Left:
            SDL_RenderLine(r, cx - s, cy, cx + s * 0.4f, cy - s);
            SDL_RenderLine(r, cx - s, cy, cx + s * 0.4f, cy + s);
            SDL_RenderLine(r, cx + s * 0.4f, cy - s, cx + s * 0.4f, cy + s);
            break;
        case PadId::Right:
            SDL_RenderLine(r, cx + s, cy, cx - s * 0.4f, cy - s);
            SDL_RenderLine(r, cx + s, cy, cx - s * 0.4f, cy + s);
            SDL_RenderLine(r, cx - s * 0.4f, cy - s, cx - s * 0.4f, cy + s);
            break;
        case PadId::Esc: {
            SDL_SetRenderDrawColor(r, 255, 255, 255, on ? 255 : 200);
            SDL_RenderDebugText(r, pr.x + 18.f, pr.y + (pr.h - 8.f) * 0.5f, "ESC");
            break;
        }
        case PadId::Confirm: {
            SDL_SetRenderDrawColor(r, 255, 255, 255, on ? 255 : 200);
            SDL_RenderDebugText(r, pr.x + 22.f, pr.y + (pr.h - 8.f) * 0.5f, "OK");
            break;
        }
        default:
            break;
    }
}

}  // namespace

namespace VirtualControls {

void init() {
    if (g_inited) return;
    g_inited = true;
    g_owner.fill(-1);
    g_held.fill(false);

#ifdef __ANDROID__
    g_enabled = true;
#else
    const char* env = std::getenv("KYS_VIRTUAL_PAD");
    g_enabled = (env && env[0] == '1');
#endif
    if (g_enabled) {
        // Prefer real finger events; avoid duplicate mouse synthetics on Android.
        SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    }
}

void setRenderer(SDL_Renderer* renderer) {
    g_renderer = renderer;
}

void setEnabled(bool on) {
    init();
    if (!on) {
        for (int i = 0; i < static_cast<int>(PadId::Count); ++i) {
            if (g_held[i]) release(static_cast<PadId>(i));
        }
    }
    g_enabled = on;
}

bool isEnabled() {
    init();
    return g_enabled;
}

bool isScancodeDown(SDL_Scancode sc) {
    if (!isEnabled()) return false;
    for (int i = 0; i < static_cast<int>(PadId::Count); ++i) {
        if (g_held[i] && scancodeOf(static_cast<PadId>(i)) == sc) return true;
    }
    return false;
}

bool consumeTap(SDL_Scancode sc) {
    if (!isEnabled()) return false;
    const int confirm = static_cast<int>(PadId::Confirm);
    if ((sc == SDL_SCANCODE_SPACE || sc == SDL_SCANCODE_RETURN) && g_tapLatch[confirm]) {
        g_tapLatch[confirm] = false;
        return true;
    }
    for (int i = 0; i < static_cast<int>(PadId::Count); ++i) {
        if (g_tapLatch[i] && scancodeOf(static_cast<PadId>(i)) == sc) {
            g_tapLatch[i] = false;
            return true;
        }
    }
    return false;
}

void clearTapLatches() {
    g_tapLatch.fill(false);
}

void releaseAll() {
    for (int i = 0; i < static_cast<int>(PadId::Count); ++i) {
        g_held[i] = false;
        g_owner[i] = -1;
    }
}

bool handleEvent(SDL_Event& e) {
    init();
    if (!g_enabled) return true;

    float lx = 0, ly = 0;

    switch (e.type) {
        case SDL_EVENT_FINGER_DOWN: {
            fingerToLogical(e, lx, ly);
            PadId id = hitTest(lx, ly);
            if (id == PadId::Count) return true;
            if (id == PadId::Esc || id == PadId::Confirm) {
                press(id, static_cast<Sint64>(e.tfinger.fingerID));
            } else {
                setDirectionFromPoint(lx, ly, static_cast<Sint64>(e.tfinger.fingerID));
            }
            return false;
        }
        case SDL_EVENT_FINGER_MOTION: {
            Sint64 owner = static_cast<Sint64>(e.tfinger.fingerID);
            bool ownsDir = false;
            for (int i = 0; i < 4; ++i) {
                if (g_held[i] && g_owner[i] == owner) ownsDir = true;
            }
            if (!ownsDir) return true;
            fingerToLogical(e, lx, ly);
            setDirectionFromPoint(lx, ly, owner);
            return false;
        }
        case SDL_EVENT_FINGER_UP: {
            Sint64 owner = static_cast<Sint64>(e.tfinger.fingerID);
            bool owned = false;
            for (int i = 0; i < static_cast<int>(PadId::Count); ++i) {
                if (g_held[i] && g_owner[i] == owner) owned = true;
            }
            if (!owned) return true;
            releaseOwner(owner);
            return false;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN: {
            if (e.button.button != SDL_BUTTON_LEFT) return true;
            convertXY(e.button.x, e.button.y, lx, ly);
            PadId id = hitTest(lx, ly);
            if (id == PadId::Count) return true;
            if (id == PadId::Esc || id == PadId::Confirm) {
                press(id, kMouseOwner);
            } else {
                setDirectionFromPoint(lx, ly, kMouseOwner);
            }
            return false;
        }
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            if (e.button.button != SDL_BUTTON_LEFT) return true;
            bool owned = false;
            for (int i = 0; i < static_cast<int>(PadId::Count); ++i) {
                if (g_held[i] && g_owner[i] == kMouseOwner) owned = true;
            }
            if (!owned) return true;
            releaseOwner(kMouseOwner);
            return false;
        }
        case SDL_EVENT_MOUSE_MOTION: {
            if (!(e.motion.state & SDL_BUTTON_LMASK)) return true;
            bool ownsDir = false;
            for (int i = 0; i < 4; ++i) {
                if (g_held[i] && g_owner[i] == kMouseOwner) ownsDir = true;
            }
            if (!ownsDir) return true;
            convertXY(e.motion.x, e.motion.y, lx, ly);
            setDirectionFromPoint(lx, ly, kMouseOwner);
            return false;
        }
        default:
            return true;
    }
}

void draw(SDL_Renderer* renderer) {
    init();
    if (!g_enabled || !renderer) return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    drawPad(renderer, PadId::Up);
    drawPad(renderer, PadId::Down);
    drawPad(renderer, PadId::Left);
    drawPad(renderer, PadId::Right);
    drawPad(renderer, PadId::Esc);
    drawPad(renderer, PadId::Confirm);
}

void present(SDL_Renderer* renderer) {
    draw(renderer);
    SDL_RenderPresent(renderer);
}

}  // namespace VirtualControls
