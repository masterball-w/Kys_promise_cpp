#include "LittleGameManager.h"
#include "UIManager.h"
#include "BattleManager.h"
#include "GameManager.h"
#include "FileLoader.h"
#include "TextManager.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iostream>
#include <random>
#include "VirtualControls.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {
constexpr int SCREEN_W = 640;
constexpr int SCREEN_H = 480;
constexpr const char* GAME_PIC = "resource/Game.Pic";

struct SnakeSeg { int x = 0; int y = 0; };
}

LittleGameManager& LittleGameManager::getInstance() {
    static LittleGameManager inst;
    return inst;
}

bool LittleGameManager::HasPetLuck() const {
    return BattleManager::getInstance().GetPetSkill(4, 2);
}

void LittleGameManager::Present() {
    VirtualControls::present(UIManager::getInstance().GetRenderer());
}

void LittleGameManager::WaitAnyKeyLocal() {
    UIManager::getInstance().WaitAnyKey(nullptr, nullptr, nullptr);
}

void LittleGameManager::BlitPic(const PicImage& pic, int x, int y) {
    if (!pic.surface) return;
    BlitSurface(pic.surface, x - pic.x, y - pic.y);
}

void LittleGameManager::BlitSurface(SDL_Surface* surf, int x, int y, double angleDeg) {
    if (!surf) return;
    SDL_Renderer* r = UIManager::getInstance().GetRenderer();
    SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
    if (!tex) return;
    SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
    SDL_FRect dst{ (float)x, (float)y, (float)surf->w, (float)surf->h };
    if (std::abs(angleDeg) < 0.01) {
        SDL_RenderTexture(r, tex, nullptr, &dst);
    } else {
        SDL_FPoint center{ dst.w * 0.5f, dst.h * 0.5f };
        SDL_RenderTextureRotated(r, tex, nullptr, &dst, angleDeg, &center, SDL_FLIP_NONE);
    }
    SDL_DestroyTexture(tex);
}

void LittleGameManager::DrawFallbackRect(int x, int y, int w, int h, uint32_t rgba) {
    UIManager::getInstance().DrawFilledRect(x, y, w, h, rgba, 255);
}

SDL_Surface* LittleGameManager::CropSurface(SDL_Surface* src, int sx, int sy, int sw, int sh) {
    if (!src || sw <= 0 || sh <= 0) return nullptr;
    SDL_Surface* dst = SDL_CreateSurface(sw, sh, SDL_PIXELFORMAT_ARGB8888);
    if (!dst) return nullptr;
    SDL_Rect srcRect{ sx, sy, sw, sh };
    SDL_Rect dstRect{ 0, 0, sw, sh };
    SDL_BlitSurface(src, &srcRect, dst, &dstRect);
    return dst;
}

SDL_Surface* LittleGameManager::RotateSurface90(SDL_Surface* src, int turns) {
    if (!src) return nullptr;
    turns = ((turns % 4) + 4) % 4;
    if (turns == 0) {
        SDL_Surface* copy = SDL_CreateSurface(src->w, src->h, SDL_PIXELFORMAT_ARGB8888);
        if (!copy) return nullptr;
        SDL_BlitSurface(src, nullptr, copy, nullptr);
        return copy;
    }
    SDL_Surface* cur = src;
    SDL_Surface* owned = nullptr;
    for (int t = 0; t < turns; ++t) {
        int nw = cur->h;
        int nh = cur->w;
        SDL_Surface* next = SDL_CreateSurface(nw, nh, SDL_PIXELFORMAT_ARGB8888);
        if (!next) {
            if (owned) SDL_DestroySurface(owned);
            return nullptr;
        }
        if (!SDL_LockSurface(cur) && !SDL_LockSurface(next)) {
            auto* srcPx = static_cast<uint32_t*>(cur->pixels);
            auto* dstPx = static_cast<uint32_t*>(next->pixels);
            int srcPitch = cur->pitch / 4;
            int dstPitch = next->pitch / 4;
            for (int y = 0; y < cur->h; ++y) {
                for (int x = 0; x < cur->w; ++x) {
                    // 90° clockwise
                    dstPx[x * dstPitch + (nh - 1 - y)] = srcPx[y * srcPitch + x];
                }
            }
            SDL_UnlockSurface(cur);
            SDL_UnlockSurface(next);
        }
        if (owned) SDL_DestroySurface(owned);
        owned = next;
        cur = next;
    }
    return owned;
}

std::string LittleGameManager::Ucs2ToUtf8(uint16_t ch) {
    std::string out;
    if (ch == 0 || ch == 0x20) {
        out = " ";
        return out;
    }
    if (ch < 0x80) {
        out.push_back(static_cast<char>(ch));
    } else if (ch < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (ch >> 6)));
        out.push_back(static_cast<char>(0x80 | (ch & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xE0 | (ch >> 12)));
        out.push_back(static_cast<char>(0x80 | ((ch >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (ch & 0x3F)));
    }
    return out;
}

std::vector<uint16_t> LittleGameManager::LoadPoetryChars(int talknum) {
    std::vector<uint16_t> chars;
    auto idxData = FileLoader::loadFile("talk.idx");
    auto grpData = FileLoader::loadFile("talk.grp");
    if (idxData.size() < 4 || grpData.empty()) return chars;

    size_t idxCount = idxData.size() / 4;
    auto readI32 = [&](size_t i) -> int32_t {
        if (i >= idxCount) return (int32_t)grpData.size();
        int32_t v = 0;
        std::memcpy(&v, idxData.data() + i * 4, 4);
        return v;
    };

    int32_t offset = 0;
    int32_t end = 0;
    if (talknum <= 0) {
        offset = 0;
        end = readI32(0);
    } else {
        offset = readI32(static_cast<size_t>(talknum - 1));
        end = readI32(static_cast<size_t>(talknum));
    }
    if (offset < 0 || end <= offset || end > (int32_t)grpData.size()) return chars;

    int len = end - offset;
    std::vector<uint8_t> buf(static_cast<size_t>(len));
    for (int i = 0; i < len; ++i) {
        uint8_t b = static_cast<uint8_t>(grpData[offset + i]) ^ 0xFF;
        if (b == 0xFF) b = 0;
        buf[static_cast<size_t>(i)] = b;
    }
    int n = len / 2;
    chars.reserve(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        uint16_t ch = static_cast<uint16_t>(buf[i * 2] | (buf[i * 2 + 1] << 8));
        if (ch == 0) break;
        chars.push_back(ch);
    }
    return chars;
}

// ===================== FemaleSnake =====================

int LittleGameManager::FemaleSnake() {
    static bool seeded = false;
    if (!seeded) { std::srand(static_cast<unsigned>(std::time(nullptr))); seeded = true; }

    PicImage background = PicLoader::loadPic(GAME_PIC, 4);
    PicImage bodyPic = PicLoader::loadPic(GAME_PIC, 5);
    PicImage headPic = PicLoader::loadPic(GAME_PIC, 6);
    PicImage foodPic[8];
    for (int i = 0; i < 8; ++i) foodPic[i] = PicLoader::loadPic(GAME_PIC, 7 + i);

    std::vector<SnakeSeg> snake(8);
    snake[0] = {1, 0};
    for (size_t i = 1; i < snake.size(); ++i) snake[i] = {0, 0};

    int eatFemale = 0;
    int dest = 1; // right
    int ranX = 0, ranY = 0;
    auto placeFood = [&]() {
        for (;;) {
            ranX = std::rand() % 16;
            ranY = std::rand() % 10;
            bool hit = false;
            for (const auto& s : snake) {
                if (s.x == ranX && s.y == ranY) { hit = true; break; }
            }
            if (!hit) break;
        }
    };
    placeFood();

    auto drawAll = [&]() {
        SDL_Renderer* r = UIManager::getInstance().GetRenderer();
        SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
        SDL_RenderClear(r);
        for (int i = 0; i < 16; ++i) {
            for (int j = 0; j < 10; ++j) {
                if (background.surface) BlitPic(background, 40 * i, 40 * j);
                else DrawFallbackRect(40 * i, 40 * j, 40, 40, 0x204020FF);
            }
        }
        int foodIdx = (eatFemale < 7) ? eatFemale : 7;
        if (foodPic[foodIdx].surface) BlitPic(foodPic[foodIdx], 40 * ranX, 40 * ranY);
        else DrawFallbackRect(40 * ranX + 8, 40 * ranY + 8, 24, 24, 0xFF6699FF);

        for (size_t i = 1; i < snake.size(); ++i) {
            if (bodyPic.surface) BlitPic(bodyPic, 40 * snake[i].x, 40 * snake[i].y);
            else DrawFallbackRect(40 * snake[i].x + 4, 40 * snake[i].y + 4, 32, 32, 0x44AA44FF);
        }
        if (headPic.surface) BlitPic(headPic, 40 * snake[0].x, 40 * snake[0].y);
        else DrawFallbackRect(40 * snake[0].x + 2, 40 * snake[0].y + 2, 36, 36, 0x88FF88FF);

        UIManager::getInstance().DrawShadowTextUtf8(
            " 得分：" + std::to_string(eatFemale) + "  方向键移动  撞身结束",
            10, 410, 0xFFFFFFFF, 0x000000FF);
        Present();
    };

    auto moveSnake = [&](int edest) -> bool {
        // reverse blocked
        if (std::abs(dest - edest) == 2) edest = dest;

        for (int i = (int)snake.size() - 1; i >= 1; --i) snake[i] = snake[i - 1];

        if (snake[0].x == ranX && snake[0].y == ranY) {
            int grow = (eatFemale >= 7) ? 1 : 5;
            ++eatFemale;
            SnakeSeg tail = snake.back();
            snake.resize(snake.size() + grow, tail);
            placeFood();
        }

        switch (edest) {
            case 0: snake[0].y = (snake[0].y >= 0) ? snake[0].y - 1 : 9; break;
            case 1: snake[0].x = (snake[0].x < 15) ? snake[0].x + 1 : 15; break;
            case 2: snake[0].y = (snake[0].y < 9) ? snake[0].y + 1 : 9; break;
            case 3: snake[0].x = (snake[0].x >= 0) ? snake[0].x - 1 : 15; break;
        }
        dest = edest;

        for (size_t i = 1; i < snake.size(); ++i) {
            if (snake[0].x == snake[i].x && snake[0].y == snake[i].y) return true;
        }
        if (snake[0].x < 0 || snake[0].x >= 16 || snake[0].y < 0 || snake[0].y >= 16) return true;
        return false;
    };

    drawAll();
    uint32_t lastTick = SDL_GetTicks();
    int delay = HasPetLuck() ? 150 : 120;
    bool keyReady = true;
    bool running = true;
    SDL_Event ev;

    while (running) {
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT) {
                GameManager::getInstance().Quit();
                running = false;
                break;
            }
            if (ev.type == SDL_EVENT_KEY_UP) keyReady = true;
            if (ev.type == SDL_EVENT_KEY_DOWN && keyReady) {
                keyReady = false;
                int nd = -1;
                switch (ev.key.key) {
                    case SDLK_UP: case SDLK_KP_8: nd = 0; break;
                    case SDLK_RIGHT: case SDLK_KP_6: nd = 1; break;
                    case SDLK_DOWN: case SDLK_KP_2: nd = 2; break;
                    case SDLK_LEFT: case SDLK_KP_4: nd = 3; break;
                    case SDLK_ESCAPE: running = false; break;
                    default: break;
                }
                if (nd >= 0) {
                    if (moveSnake(nd)) {
                        drawAll();
                        WaitAnyKeyLocal();
                        running = false;
                    } else {
                        drawAll();
                    }
                }
            }
        }
        uint32_t now = SDL_GetTicks();
        if (running && now - lastTick >= (uint32_t)delay) {
            lastTick = now;
            if (moveSnake(dest)) {
                drawAll();
                WaitAnyKeyLocal();
                running = false;
            } else {
                drawAll();
            }
        }
        SDL_Delay(8);
    }

    PicLoader::freePic(background);
    PicLoader::freePic(bodyPic);
    PicLoader::freePic(headPic);
    for (int i = 0; i < 8; ++i) PicLoader::freePic(foodPic[i]);
    return eatFemale;
}

// ===================== ShotEagle =====================

bool LittleGameManager::ShotEagle(int aim, int chance) {
    if (aim <= 0) aim = 3;
    if (chance <= 0) chance = 5;
    if (HasPetLuck()) chance *= 2;

    PicImage sheet = PicLoader::loadPic(GAME_PIC, 1);
    PicImage bg = PicLoader::loadPic(GAME_PIC, 2);
    SDL_Surface* eagleFrames[8] = {};
    if (sheet.surface) {
        for (int i = 0; i < 8; ++i) {
            eagleFrames[i] = CropSurface(sheet.surface, (i % 4) * 86, (i / 4) * 65, 86, 65);
        }
    }

    int goal = 0;
    int shotsLeft = chance;
    float birdX = 50.0f;
    float birdY = 80.0f;
    float birdSpeed = 3.0f;
    int birdFrame = 0;
    float angle = 0.0f;
    bool arrowFlying = false;
    float arrowX = 320.0f, arrowY = 430.0f;
    float arrowVX = 0, arrowVY = 0;
    float power = 0;
    bool charging = false;
    bool running = true;
    bool success = false;
    SDL_Event ev;
    uint32_t last = SDL_GetTicks();

    auto resetBird = [&]() {
        birdSpeed = (std::rand() % 2 == 0) ? 3.0f : -3.0f;
        birdX = (birdSpeed > 0) ? -90.0f : (float)SCREEN_W;
        birdY = 60.0f + (float)(std::rand() % 120);
    };
    resetBird();

    while (running) {
        float mx = 0, my = 0;
        SDL_GetMouseState(&mx, &my);

        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT) {
                GameManager::getInstance().Quit();
                running = false;
            } else if (ev.type == SDL_EVENT_KEY_DOWN) {
                if (ev.key.key == SDLK_ESCAPE) { running = false; success = false; }
                else if (ev.key.key == SDLK_LEFT || ev.key.key == SDLK_KP_4) angle -= 3.0f;
                else if (ev.key.key == SDLK_RIGHT || ev.key.key == SDLK_KP_6) angle += 3.0f;
                else if ((ev.key.key == SDLK_SPACE || ev.key.key == SDLK_RETURN) && !arrowFlying && shotsLeft > 0) {
                    charging = true; power = 0;
                }
            } else if (ev.type == SDL_EVENT_KEY_UP) {
                if ((ev.key.key == SDLK_SPACE || ev.key.key == SDLK_RETURN) && charging && !arrowFlying) {
                    charging = false;
                    --shotsLeft;
                    arrowFlying = true;
                    float rad = angle * (float)M_PI / 180.0f;
                    float spd = 8.0f + power * 0.15f;
                    arrowX = SCREEN_W * 0.5f;
                    arrowY = (float)SCREEN_H - 40.0f;
                    arrowVX = std::sin(rad) * spd;
                    arrowVY = -std::cos(rad) * spd;
                }
            } else if (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN && ev.button.button == SDL_BUTTON_LEFT) {
                if (!arrowFlying && shotsLeft > 0) { charging = true; power = 0; }
            } else if (ev.type == SDL_EVENT_MOUSE_BUTTON_UP && ev.button.button == SDL_BUTTON_LEFT) {
                if (charging && !arrowFlying) {
                    charging = false;
                    --shotsLeft;
                    arrowFlying = true;
                    float dx = mx - SCREEN_W * 0.5f;
                    float dy = (float)SCREEN_H - 40.0f - my;
                    angle = std::atan2(dx, dy) * 180.0f / (float)M_PI;
                    float rad = angle * (float)M_PI / 180.0f;
                    float spd = 8.0f + power * 0.15f;
                    arrowX = SCREEN_W * 0.5f;
                    arrowY = (float)SCREEN_H - 40.0f;
                    arrowVX = std::sin(rad) * spd;
                    arrowVY = -std::cos(rad) * spd;
                }
            }
        }

        // mouse aim
        {
            float dx = mx - SCREEN_W * 0.5f;
            float dy = (float)SCREEN_H - 40.0f - my;
            angle = std::atan2(dx, dy) * 180.0f / (float)M_PI;
            if (angle < -80) angle = -80;
            if (angle > 80) angle = 80;
        }
        if (charging) power = std::min(40.0f, power + 0.8f);

        uint32_t now = SDL_GetTicks();
        if (now - last >= 16) {
            last = now;
            birdX += birdSpeed;
            birdFrame = (birdFrame + 1) % 8;
            if (birdX < -100 || birdX > SCREEN_W + 100) resetBird();

            if (arrowFlying) {
                arrowX += arrowVX;
                arrowY += arrowVY;
                arrowVY += 0.12f; // gravity
                float bx = birdX + 43, by = birdY + 32;
                if (std::abs(arrowX - bx) < 40 && std::abs(arrowY - by) < 35) {
                    ++goal;
                    arrowFlying = false;
                    resetBird();
                    if (goal >= aim) { success = true; running = false; }
                }
                if (arrowX < -20 || arrowX > SCREEN_W + 20 || arrowY < -20 || arrowY > SCREEN_H + 20) {
                    arrowFlying = false;
                }
            }
            if (shotsLeft <= 0 && !arrowFlying && goal < aim) {
                success = false;
                running = false;
            }
        }

        SDL_Renderer* r = UIManager::getInstance().GetRenderer();
        SDL_SetRenderDrawColor(r, 40, 80, 140, 255);
        SDL_RenderClear(r);
        if (bg.surface) BlitPic(bg, 0, 0);

        if (eagleFrames[birdFrame]) BlitSurface(eagleFrames[birdFrame], (int)birdX, (int)birdY);
        else DrawFallbackRect((int)birdX, (int)birdY, 70, 40, 0xCCAA22FF);

        // bow
        DrawFallbackRect(SCREEN_W / 2 - 8, SCREEN_H - 50, 16, 40, 0x8B4513FF);
        // aim line
        {
            float rad = angle * (float)M_PI / 180.0f;
            float x2 = SCREEN_W * 0.5f + std::sin(rad) * 60;
            float y2 = SCREEN_H - 40.0f - std::cos(rad) * 60;
            SDL_SetRenderDrawColor(r, 255, 255, 0, 255);
            SDL_RenderLine(r, SCREEN_W * 0.5f, (float)SCREEN_H - 40, x2, y2);
        }
        if (arrowFlying) DrawFallbackRect((int)arrowX - 3, (int)arrowY - 10, 6, 20, 0xFFFFFFFF);
        if (charging) {
            UIManager::getInstance().DrawFilledRect(SCREEN_W / 2 - 50, SCREEN_H - 20, (int)power * 2, 8, 0xFF0000FF, 200);
        }
        UIManager::getInstance().DrawShadowTextUtf8(
            " 射雕 命中 " + std::to_string(goal) + "/" + std::to_string(aim) +
            "  箭 " + std::to_string(shotsLeft) + "  空格蓄力射擊 ESC放棄",
            10, 10, 0xFFFFFFFF, 0x000000FF);
        Present();
        SDL_Delay(8);
    }

    for (int i = 0; i < 8; ++i) if (eagleFrames[i]) SDL_DestroySurface(eagleFrames[i]);
    PicLoader::freePic(sheet);
    PicLoader::freePic(bg);
    UIManager::getInstance().ShowDialogue(success ? " 射雕成功！" : " 射雕失敗…", -1, 0);
    return success;
}

// ===================== Acupuncture =====================

bool LittleGameManager::Acupuncture(int n) {
    if (n <= 0) n = 3;
    int tries = HasPetLuck() ? 6 : 3;

    PicImage body = PicLoader::loadPic(GAME_PIC, 0);
    std::vector<std::pair<int, int>> points;

    // Prefer list/Acupuncture.bin
    {
        auto bin = FileLoader::loadFile("list/Acupuncture.bin");
        if (bin.size() >= 12) {
            std::vector<int16_t> vals(bin.size() / 2);
            std::memcpy(vals.data(), bin.data(), vals.size() * 2);
            int ox = vals.size() > 0 ? vals[0] : 120;
            int oy = vals.size() > 1 ? vals[1] : 40;
            (void)ox; (void)oy;
            for (size_t i = 6; i + 1 < vals.size(); i += 2) {
                if (vals[i] == -1 || vals[i + 1] == -1) break;
                points.push_back({vals[i], vals[i + 1]});
            }
        }
    }
    if (points.empty()) {
        // Fallback grid over body area
        for (int row = 0; row < 5; ++row)
            for (int col = 0; col < 4; ++col)
                points.push_back({180 + col * 70, 80 + row * 60});
    }
    if ((int)points.size() < n) n = (int)points.size();

    UIManager::getInstance().ShowDialogue(
        HasPetLuck() ? " 針灸：依序點亮穴位（機會較多）" : " 針灸：依序點亮穴位", -1, 0);

    std::vector<int> sequence(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) sequence[i] = std::rand() % (int)points.size();

    // Flash sequence
    for (int i = 0; i < n; ++i) {
        SDL_Renderer* r = UIManager::getInstance().GetRenderer();
        SDL_SetRenderDrawColor(r, 20, 20, 30, 255);
        SDL_RenderClear(r);
        if (body.surface) BlitPic(body, 120, 40);
        else DrawFallbackRect(120, 40, 360, 360, 0x886655FF);
        for (size_t p = 0; p < points.size(); ++p) {
            uint32_t c = ((int)p == sequence[i]) ? 0xFFFF00FF : 0x888888FF;
            DrawFallbackRect(points[p].first - 8, points[p].second - 8, 16, 16, c);
        }
        UIManager::getInstance().DrawShadowTextUtf8(" 記住順序…", 20, 420, 0xFFFFFFFF, 0x000000FF);
        Present();
        SDL_Delay(800);
        SDL_SetRenderDrawColor(r, 20, 20, 30, 255);
        SDL_RenderClear(r);
        if (body.surface) BlitPic(body, 120, 40);
        for (auto& pt : points) DrawFallbackRect(pt.first - 8, pt.second - 8, 16, 16, 0x888888FF);
        Present();
        SDL_Delay(300);
    }

    bool success = false;
    for (int attempt = 0; attempt < tries && !success; ++attempt) {
        std::vector<int> input;
        bool waiting = true;
        SDL_Event ev;
        while (waiting) {
            while (SDL_PollEvent(&ev)) {
                if (ev.type == SDL_EVENT_QUIT) {
                    GameManager::getInstance().Quit();
                    PicLoader::freePic(body);
                    return false;
                }
                if (ev.type == SDL_EVENT_KEY_DOWN && ev.key.key == SDLK_ESCAPE) {
                    PicLoader::freePic(body);
                    return false;
                }
                if (ev.type == SDL_EVENT_MOUSE_BUTTON_UP && ev.button.button == SDL_BUTTON_LEFT) {
                    float mx = ev.button.x, my = ev.button.y;
                    int hit = -1;
                    for (size_t p = 0; p < points.size(); ++p) {
                        if (std::abs(mx - points[p].first) <= 12 && std::abs(my - points[p].second) <= 12) {
                            hit = (int)p;
                            break;
                        }
                    }
                    if (hit >= 0) {
                        input.push_back(hit);
                        if ((int)input.size() == n) waiting = false;
                    }
                }
            }
            SDL_Renderer* r = UIManager::getInstance().GetRenderer();
            SDL_SetRenderDrawColor(r, 20, 20, 30, 255);
            SDL_RenderClear(r);
            if (body.surface) BlitPic(body, 120, 40);
            else DrawFallbackRect(120, 40, 360, 360, 0x886655FF);
            for (size_t p = 0; p < points.size(); ++p) {
                bool lit = std::find(input.begin(), input.end(), (int)p) != input.end();
                DrawFallbackRect(points[p].first - 8, points[p].second - 8, 16, 16,
                                lit ? 0x44FF44FF : 0x888888FF);
            }
            UIManager::getInstance().DrawShadowTextUtf8(
                " 點擊穴位  " + std::to_string(input.size()) + "/" + std::to_string(n) +
                "  剩餘機會 " + std::to_string(tries - attempt),
                20, 420, 0xFFFFFFFF, 0x000000FF);
            Present();
            SDL_Delay(8);
        }
        success = (input == sequence);
    }

    PicLoader::freePic(body);
    UIManager::getInstance().ShowDialogue(success ? " 針灸成功！" : " 針灸失敗…", -1, 0);
    return success;
}

// ===================== Lamp (Lights Out) =====================

bool LittleGameManager::Lamp(int c, int beginpic, int whitecount, int /*chance*/) {
    (void)beginpic;
    if (HasPetLuck() && c > 2) --c;
    if (c < 2) c = 2;
    if (c > 8) c = 8;
    int r = c;
    if (whitecount <= 0) whitecount = c;

    std::vector<int> grid(static_cast<size_t>(c * r), 0);
    for (int i = 0; i < whitecount; ++i) {
        int t = std::rand() % (c * r);
        grid[t] = 1 - grid[t];
    }

    int originX = (SCREEN_W - c * 50) / 2;
    int originY = (SCREEN_H - r * 50) / 2;
    int menu = 0;
    bool running = true;
    bool success = false;
    SDL_Event ev;

    auto toggle = [&](int idx) {
        if (idx < 0 || idx >= c * r) return;
        grid[idx] = 1 - grid[idx];
    };
    auto flipAt = [&](int idx) {
        toggle(idx);
        if (idx % c > 0) toggle(idx - 1);
        if (idx % c < c - 1) toggle(idx + 1);
        if (idx / c > 0) toggle(idx - c);
        if (idx / c < r - 1) toggle(idx + c);
    };
    auto allSame = [&]() {
        for (int v : grid) if (v != grid[0]) return false;
        return true;
    };

    while (running) {
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT) {
                GameManager::getInstance().Quit();
                return false;
            }
            if (ev.type == SDL_EVENT_KEY_DOWN) {
                if (ev.key.key == SDLK_ESCAPE) { running = false; success = false; }
                else if (ev.key.key == SDLK_UP || ev.key.key == SDLK_KP_8) menu = (menu - c + c * r) % (c * r);
                else if (ev.key.key == SDLK_DOWN || ev.key.key == SDLK_KP_2) menu = (menu + c) % (c * r);
                else if (ev.key.key == SDLK_LEFT || ev.key.key == SDLK_KP_4) menu = (menu - 1 + c * r) % (c * r);
                else if (ev.key.key == SDLK_RIGHT || ev.key.key == SDLK_KP_6) menu = (menu + 1) % (c * r);
                else if (ev.key.key == SDLK_RETURN || ev.key.key == SDLK_SPACE) flipAt(menu);
            }
            if (ev.type == SDL_EVENT_MOUSE_MOTION) {
                float mx = ev.motion.x, my = ev.motion.y;
                if (mx >= originX && mx < originX + 50 * c && my >= originY && my < originY + 50 * r) {
                    menu = ((int)(mx - originX) / 50) + ((int)(my - originY) / 50) * c;
                }
            }
            if (ev.type == SDL_EVENT_MOUSE_BUTTON_UP && ev.button.button == SDL_BUTTON_LEFT) {
                float mx = ev.button.x, my = ev.button.y;
                if (mx >= originX && mx < originX + 50 * c && my >= originY && my < originY + 50 * r) {
                    menu = ((int)(mx - originX) / 50) + ((int)(my - originY) / 50) * c;
                    flipAt(menu);
                }
            }
        }
        if (allSame()) { success = true; running = false; SDL_Delay(400); }

        SDL_Renderer* ren = UIManager::getInstance().GetRenderer();
        SDL_SetRenderDrawColor(ren, 10, 10, 20, 255);
        SDL_RenderClear(ren);
        UIManager::getInstance().DrawRectangle(originX - 10, originY - 10, c * 50 + 20, r * 50 + 20, 0, 0xFFFFFFFF, 40);
        for (int i = 0; i < c * r; ++i) {
            int px = originX + (i % c) * 50;
            int py = originY + (i / c) * 50;
            DrawFallbackRect(px, py, 48, 48, grid[i] ? 0xEEEEEEFF : 0x333344FF);
            if (i == menu) UIManager::getInstance().DrawRectangle(px, py, 48, 48, 0, 0xFFFF00FF, 0);
        }
        UIManager::getInstance().DrawShadowTextUtf8(" 黑白棋：翻轉相鄰格  ESC放棄", 20, 20, 0xFFFFFFFF, 0x000000FF);
        Present();
        SDL_Delay(8);
    }
    return success;
}

// ===================== Poetry =====================

bool LittleGameManager::Poetry(int talknum, int chance, int cols, int count) {
    if (HasPetLuck()) chance *= 2;
    if (chance <= 0) chance = 10;
    if (cols <= 0) cols = 5;
    if (count <= 0) count = 10;

    auto chars = LoadPoetryChars(talknum);
    if (chars.empty()) {
        // Fallback sample poem chars (UCS-2 CJK)
        const uint16_t sample[] = {0x5C71,0x5DDD,0x7570,0x57DF,0x98A8,0x6708,0x540C,0x5929,0x6587,0x5B57};
        chars.assign(sample, sample + 10);
    }
    if (count > (int)chars.size()) count = (int)chars.size();

    std::vector<uint16_t> pool = chars;
    std::vector<uint16_t> answer(static_cast<size_t>(count), 0x20);
    std::shuffle(pool.begin(), pool.end(), std::mt19937(std::random_device{}()));

    int srcSel = 0;
    int dstSel = 0;
    bool pickingDst = false;
    bool running = true;
    bool success = false;
    SDL_Event ev;

    auto checkWin = [&]() {
        for (int i = 0; i < count; ++i)
            if (answer[i] != chars[i]) return false;
        return true;
    };

    while (running) {
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT) {
                GameManager::getInstance().Quit();
                return false;
            }
            if (ev.type == SDL_EVENT_KEY_DOWN) {
                if (ev.key.key == SDLK_ESCAPE) { running = false; success = false; }
                else if (!pickingDst) {
                    if (ev.key.key == SDLK_LEFT || ev.key.key == SDLK_KP_4) srcSel = (srcSel - 1 + (int)pool.size()) % (int)pool.size();
                    else if (ev.key.key == SDLK_RIGHT || ev.key.key == SDLK_KP_6) srcSel = (srcSel + 1) % (int)pool.size();
                    else if (ev.key.key == SDLK_UP || ev.key.key == SDLK_KP_8) srcSel = (srcSel - cols + (int)pool.size()) % (int)pool.size();
                    else if (ev.key.key == SDLK_DOWN || ev.key.key == SDLK_KP_2) srcSel = (srcSel + cols) % (int)pool.size();
                    else if (ev.key.key == SDLK_RETURN || ev.key.key == SDLK_SPACE) pickingDst = true;
                } else {
                    if (ev.key.key == SDLK_LEFT || ev.key.key == SDLK_KP_4) dstSel = (dstSel - 1 + count) % count;
                    else if (ev.key.key == SDLK_RIGHT || ev.key.key == SDLK_KP_6) dstSel = (dstSel + 1) % count;
                    else if (ev.key.key == SDLK_RETURN || ev.key.key == SDLK_SPACE) {
                        std::swap(pool[srcSel], answer[dstSel]);
                        --chance;
                        pickingDst = false;
                        if (checkWin()) { success = true; running = false; }
                        else if (chance <= 0) {
                            success = checkWin();
                            running = false;
                        }
                    }
                }
            }
            if (ev.type == SDL_EVENT_MOUSE_BUTTON_UP && ev.button.button == SDL_BUTTON_LEFT) {
                float mx = ev.button.x, my = ev.button.y;
                // pool area
                int poolY = 60;
                for (size_t i = 0; i < pool.size(); ++i) {
                    int px = 40 + ((int)i % cols) * 40;
                    int py = poolY + ((int)i / cols) * 40;
                    if (mx >= px && mx < px + 36 && my >= py && my < py + 36) {
                        srcSel = (int)i;
                        pickingDst = true;
                    }
                }
                int ansY = 280;
                for (int i = 0; i < count; ++i) {
                    int px = 40 + (i % cols) * 40;
                    int py = ansY + (i / cols) * 40;
                    if (mx >= px && mx < px + 36 && my >= py && my < py + 36 && pickingDst) {
                        dstSel = i;
                        std::swap(pool[srcSel], answer[dstSel]);
                        --chance;
                        pickingDst = false;
                        if (checkWin()) { success = true; running = false; }
                        else if (chance <= 0) { success = checkWin(); running = false; }
                    }
                }
            }
        }

        SDL_Renderer* ren = UIManager::getInstance().GetRenderer();
        SDL_SetRenderDrawColor(ren, 30, 20, 10, 255);
        SDL_RenderClear(ren);
        UIManager::getInstance().DrawShadowTextUtf8(
            std::string(" 作詩 機會:") + std::to_string(chance) + (pickingDst ? "  選答案格" : "  選字"),
            20, 20, 0xFFFF00FF, 0x000000FF);
        for (size_t i = 0; i < pool.size(); ++i) {
            int px = 40 + ((int)i % cols) * 40;
            int py = 60 + ((int)i / cols) * 40;
            uint32_t frame = ((int)i == srcSel) ? 0xFFFF00FF : 0xFFFFFFFF;
            UIManager::getInstance().DrawRectangle(px, py, 36, 36, 0x222222FF, frame, 30);
            UIManager::getInstance().DrawShadowTextUtf8(Ucs2ToUtf8(pool[i]), px + 6, py + 6, 0xFFFFFFFF, 0x000000FF);
        }
        UIManager::getInstance().DrawShadowTextUtf8(" 答案：", 20, 250, 0xFFFFFFFF, 0x000000FF);
        for (int i = 0; i < count; ++i) {
            int px = 40 + (i % cols) * 40;
            int py = 280 + (i / cols) * 40;
            uint32_t frame = (pickingDst && i == dstSel) ? 0x00FF00FF : 0xAAAAAAAA;
            UIManager::getInstance().DrawRectangle(px, py, 36, 36, 0x222222FF, frame, 30);
            UIManager::getInstance().DrawShadowTextUtf8(Ucs2ToUtf8(answer[i]), px + 6, py + 6, 0xFFFFFFFF, 0x000000FF);
        }
        Present();
        SDL_Delay(8);
    }
    UIManager::getInstance().ShowDialogue(success ? " 作詩成功！" : " 作詩失敗…", -1, 0);
    return success;
}

// ===================== rotoSpellPicture =====================

bool LittleGameManager::RotoSpellPicture(int num, int chance) {
    if (HasPetLuck()) chance *= 2;
    if (chance <= 0) chance = 30;

    PicImage full = PicLoader::loadPic(GAME_PIC, num + 3);
    const int tile = 80;
    const int grid = 5;
    SDL_Surface* baseTiles[25] = {};
    SDL_Surface* rotated[25][4] = {};

    if (full.surface && full.surface->w >= tile * grid && full.surface->h >= tile * grid) {
        for (int i = 0; i < 25; ++i) {
            int sx = (i % grid) * tile;
            int sy = (i / grid) * tile;
            baseTiles[i] = CropSurface(full.surface, sx, sy, tile, tile);
            for (int r = 0; r < 4; ++r) rotated[i][r] = RotateSurface90(baseTiles[i], r);
        }
    }

    struct Cell { int id; int rot; };
    std::vector<Cell> board(25);
    for (int i = 0; i < 25; ++i) board[i] = {i, std::rand() % 4};
    std::shuffle(board.begin(), board.end(), std::mt19937(std::random_device{}()));

    int ox = 150, oy = 40;
    int sel = -1;
    int cursor = 0;
    bool running = true;
    bool success = false;
    SDL_Event ev;

    auto countRight = [&]() {
        int right = 0;
        for (int i = 0; i < 25; ++i)
            if (board[i].id == i && board[i].rot == 0) ++right;
        return right;
    };

    // Preview correct image briefly
    {
        SDL_Renderer* ren = UIManager::getInstance().GetRenderer();
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);
        if (full.surface) BlitSurface(full.surface, ox, oy);
        else {
            for (int i = 0; i < 25; ++i)
                DrawFallbackRect(ox + (i % 5) * tile, oy + (i / 5) * tile, tile - 2, tile - 2,
                                 ((i % 2) ? 0x6688AAFF : 0x446688FF));
        }
        UIManager::getInstance().DrawShadowTextUtf8(" 記住正確拼圖…", 20, 20, 0xFFFFFFFF, 0x000000FF);
        Present();
        SDL_Delay(2000);
    }

    while (running) {
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT) {
                GameManager::getInstance().Quit();
                running = false;
            } else if (ev.type == SDL_EVENT_KEY_DOWN) {
                if (ev.key.key == SDLK_ESCAPE) {
                    if (sel >= 0) sel = -1;
                    else { running = false; success = false; }
                } else if (ev.key.key == SDLK_LEFT || ev.key.key == SDLK_KP_4) cursor = (cursor + 24) % 25;
                else if (ev.key.key == SDLK_RIGHT || ev.key.key == SDLK_KP_6) cursor = (cursor + 1) % 25;
                else if (ev.key.key == SDLK_UP || ev.key.key == SDLK_KP_8) cursor = (cursor + 20) % 25;
                else if (ev.key.key == SDLK_DOWN || ev.key.key == SDLK_KP_2) cursor = (cursor + 5) % 25;
                else if (ev.key.key == SDLK_SPACE) {
                    if (sel < 0) sel = cursor;
                    else if (sel == cursor) sel = -1;
                    else {
                        std::swap(board[sel], board[cursor]);
                        --chance;
                        sel = -1;
                    }
                } else if (ev.key.key == SDLK_RETURN) {
                    board[cursor].rot = (board[cursor].rot + 1) % 4;
                }
            } else if (ev.type == SDL_EVENT_MOUSE_BUTTON_UP) {
                float mx = ev.button.x, my = ev.button.y;
                if (mx >= ox && mx < ox + grid * tile && my >= oy && my < oy + grid * tile) {
                    int cx = ((int)mx - ox) / tile;
                    int cy = ((int)my - oy) / tile;
                    cursor = cy * grid + cx;
                    if (ev.button.button == SDL_BUTTON_LEFT) {
                        if (sel < 0) sel = cursor;
                        else if (sel == cursor) sel = -1;
                        else {
                            std::swap(board[sel], board[cursor]);
                            --chance;
                            sel = -1;
                        }
                    } else if (ev.button.button == SDL_BUTTON_RIGHT) {
                        board[cursor].rot = (board[cursor].rot + 1) % 4;
                    }
                }
            }
        }

        if (countRight() == 25) { success = true; running = false; }
        if (chance <= 0 && countRight() < 25) { success = false; running = false; }

        SDL_Renderer* ren = UIManager::getInstance().GetRenderer();
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);
        // thumbnail
        if (full.surface) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(ren, full.surface);
            if (tex) {
                SDL_FRect dst{ 10, 40, 120, 120 };
                SDL_RenderTexture(ren, tex, nullptr, &dst);
                SDL_DestroyTexture(tex);
            }
        }
        for (int i = 0; i < 25; ++i) {
            int px = ox + (i % grid) * tile;
            int py = oy + (i / grid) * tile;
            int id = board[i].id;
            int rot = board[i].rot;
            if (rotated[id][rot]) BlitSurface(rotated[id][rot], px, py);
            else {
                uint32_t col = 0xFF000000u | ((id * 37) & 0xFF) << 16 | ((id * 73) & 0xFF) << 8 | ((id * 19) & 0xFF);
                DrawFallbackRect(px, py, tile - 2, tile - 2, col | 0xFF);
                // rotation marker
                UIManager::getInstance().DrawShadowTextUtf8(std::to_string(rot * 90), px + 4, py + 4, 0xFFFFFFFF, 0x000000FF);
            }
            if (i == cursor) UIManager::getInstance().DrawRectangle(px, py, tile, tile, 0, 0xFFFF00FF, 0);
            if (i == sel) UIManager::getInstance().DrawRectangle(px + 2, py + 2, tile - 4, tile - 4, 0, 0x00FF00FF, 0);
        }
        UIManager::getInstance().DrawShadowTextUtf8(
            " 拼圖 機會:" + std::to_string(chance) + " 正確:" + std::to_string(countRight()) +
            "/25  左鍵交換 右鍵/回車旋轉",
            10, 10, 0xFFFFFFFF, 0x000000FF);
        Present();
        SDL_Delay(8);
    }

    for (int i = 0; i < 25; ++i) {
        if (baseTiles[i]) SDL_DestroySurface(baseTiles[i]);
        for (int r = 0; r < 4; ++r) if (rotated[i][r]) SDL_DestroySurface(rotated[i][r]);
    }
    PicLoader::freePic(full);
    UIManager::getInstance().ShowDialogue(success ? " 拼圖成功！" : " 拼圖失敗…", -1, 0);
    return success;
}
