#include "UIManager.h"
#include "InputManager.h"
#include "GameManager.h"
#include "SceneManager.h"
#include "PicLoader.h"
#include "SoundManager.h"
#include "TextManager.h"
#include "GraphicsUtils.h"
#include "EventManager.h"
#include "BattleManager.h"
#include "BattleEffects.h"
#include "GameTypes.h"
#include "FileLoader.h"
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "VirtualControls.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <utility>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <vector>

namespace {
    std::string g_loadedFontPath;
    bool IsAsciiOnly(const std::string& text) {
        for (unsigned char c : text) {
            if (c >= 0x80) return false;
        }
        return true;
    }

    bool IsHexDigit(char c) {
        return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
    }

    int HexValue(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
        if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
        return -1;
    }

    uint32_t PaletteIndexToColor(uint8_t index) {
        uint32_t argb = GraphicsUtils::getPaletteColor(index);
        if (argb == 0) return 0;
        uint32_t a = (argb >> 24) & 0xFF;
        uint32_t r = (argb >> 16) & 0xFF;
        uint32_t g = (argb >> 8) & 0xFF;
        uint32_t b = argb & 0xFF;
        return (r << 24) | (g << 16) | (b << 8) | a;
    }

    void DrawStatusBar(int x, int y, int currentValue, int maxValue, uint8_t fillColorIndex) {
        uint32_t bgColor = PaletteIndexToColor(0);
        uint32_t fillColor = PaletteIndexToColor(fillColorIndex);
        uint32_t highlightColor = PaletteIndexToColor(255);
        
        int fillWidth = 50 * currentValue / std::max(1, maxValue);
        
        UIManager::getInstance().DrawFilledRect(x, y, 52, 15, bgColor, 77);
        UIManager::getInstance().DrawFilledRect(x + 1, y + 1, fillWidth, 13, fillColor, 128);
        UIManager::getInstance().DrawFilledRect(x + 1, y + 9, 50, 5, highlightColor, 13);
        UIManager::getInstance().DrawFilledRect(x + 1, y + 12, 50, 3, highlightColor, 8);
        UIManager::getInstance().DrawFilledRect(x + 1, y + 14, 50, 1, highlightColor, 3);
    }

    uint32_t ResolveTextColor(uint32_t color) {
        if ((color & 0x00FFFFFF) == 0x00FFFFFF) {
            uint32_t mapped = PaletteIndexToColor((color >> 24) & 0xFF);
            if (mapped != 0) return mapped;
        }
        return color;
    }

    std::vector<int> GetValidTeamList() {
        const auto& team = GameManager::getInstance().getTeamList();
        std::vector<int> result;
        result.reserve(team.size());
        for (int id : team) {
            if (id >= 0) result.push_back(id);
        }
        return result;
    }

    struct StatusPulse {
        int green = 0;
        int red = 0;
        int gray = 0;
    };

    static StatusPulse ComputeStatusPulse(int poison, int hurt, int frozen) {
        StatusPulse pulse;
        int effects[3];
        int count = 0;
        if (poison > 0) effects[count++] = 0;
        if (hurt > 0) effects[count++] = 1;
        if (frozen > 0) effects[count++] = 2;
        if (count == 0) return pulse;

        uint32_t ticks = SDL_GetTicks();
        int nt2 = static_cast<int>(ticks / 10);
        int nt = (nt2 % 200) * count;
        int phase = (nt / 100) % 2;
        int wave = (phase == 0) ? (nt % 100) : (100 - (nt % 100));
        int effectIndex = 0;
        if (nt >= 400) effectIndex = 2;
        else if (nt >= 200) effectIndex = 1;
        int effect = effects[effectIndex];

        if (effect == 0) {
            int denom = (effectIndex == 1) ? 150 : 100;
            pulse.green = (poison * wave) / denom;
        } else if (effect == 1) {
            int denom = (effectIndex == 1) ? 150 : 100;
            pulse.red = (hurt * wave) / denom;
        } else {
            pulse.gray = (frozen * wave) / 500;
        }
        return pulse;
    }

    static uint32_t PaletteIndexColor(uint8_t index) {
        return (static_cast<uint32_t>(index) << 24) | 0x00FFFFFF;
    }

    static std::string FormatPaddedNumber(int value, int width) {
        std::ostringstream ss;
        ss << std::setw(width) << value;
        return ss.str();
    }

    static void ResolveHpColors(const Role& role, uint32_t& cur1, uint32_t& cur2, uint32_t& max1, uint32_t& max2) {
        int hurt = role.getHurt();
        if (hurt >= 34 && hurt <= 66) {
            cur1 = PaletteIndexColor(0x10);
            cur2 = PaletteIndexColor(0x0E);
        } else if (hurt >= 67) {
            cur1 = PaletteIndexColor(0x14);
            cur2 = PaletteIndexColor(0x16);
        } else {
            cur1 = PaletteIndexColor(0x05);
            cur2 = PaletteIndexColor(0x07);
        }

        int poison = role.getPoision();
        if (poison >= 34 && poison <= 66) {
            max1 = PaletteIndexColor(0x30);
            max2 = PaletteIndexColor(0x32);
        } else if (poison >= 67) {
            max1 = PaletteIndexColor(0x35);
            max2 = PaletteIndexColor(0x37);
        } else {
            max1 = PaletteIndexColor(0x21);
            max2 = PaletteIndexColor(0x23);
        }
    }
}

UIManager& UIManager::getInstance() {
    static UIManager instance;
    return instance;
}

UIManager::UIManager() {
    if (!TTF_Init()) {
        std::cerr << "TTF_Init Error: " << SDL_GetError() << std::endl;
    }
}

bool UIManager::Init(SDL_Renderer* renderer, SDL_Window* window) {
    m_renderer = renderer;
    m_window = window;

    auto resolveFont = [](const char* path) -> std::string {
        if (!path || !path[0]) return {};
        if (path[0] == '/' || (path[1] == ':')) return path;
        if (std::strncmp(path, "resource/", 9) == 0) {
            return FileLoader::getResourcePath(path);
        }
        return path;
    };

    const char* chineseFontPaths[] = {
        "resource/Chinese.ttf",
        "resource/font.ttf",
        "resource/simkai.ttf",
#ifdef _WIN32
        "C:/Windows/Fonts/msyh.ttc",
        "C:/Windows/Fonts/msyh.ttf",
        "C:/Windows/Fonts/msjh.ttc",
        "C:/Windows/Fonts/simsun.ttc",
        "C:/Windows/Fonts/simsun.ttf",
        "C:/Windows/Fonts/simhei.ttf",
        "C:/Windows/Fonts/kaiu.ttf",
        "C:/Windows/Fonts/mingliu.ttc",
        "C:/Windows/Fonts/simkai.ttf",
#endif
        "/system/fonts/NotoSansCJK-Regular.ttc",
        "/system/fonts/NotoSansSC-Regular.otf",
        "/system/fonts/DroidSansFallback.ttf",
        "/system/fonts/Roboto-Regular.ttf"
    };
    
    for (const auto& path : chineseFontPaths) {
        std::string resolved = resolveFont(path);
        m_font = TTF_OpenFont(resolved.c_str(), 20);
        if (m_font) {
            g_loadedFontPath = resolved;
            std::cout << "[UIManager] Loaded font: " << resolved << std::endl;
            break;
        }
    }

    const char* englishFontPaths[] = {
        "resource/English.ttf",
        "resource/font.ttf",
#ifdef _WIN32
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/times.ttf",
        "C:/Windows/Fonts/verdana.ttf",
        "C:/Windows/Fonts/msyh.ttc",
        "C:/Windows/Fonts/msyh.ttf",
#endif
        "/system/fonts/Roboto-Regular.ttf",
        "/system/fonts/DroidSans.ttf"
    };
    for (const auto& path : englishFontPaths) {
        std::string resolved = resolveFont(path);
        m_fontEnglish = TTF_OpenFont(resolved.c_str(), 18);
        if (m_fontEnglish) {
            std::cout << "[UIManager] Loaded English font: " << resolved << std::endl;
            break;
        }
    }
    if (!m_fontEnglish && m_font) {
        m_fontEnglish = m_font;
    }
    if (m_font) {
        TextManager::getInstance().loadFont(g_loadedFontPath, 20);
    }
    
    if (!m_font) {
        std::cerr << "Failed to load font from any known path!" << std::endl;
        // return false; // Don't fail hard for now
    }
    return true;
}

void UIManager::Cleanup() {
    if (m_fontEnglish && m_fontEnglish != m_font) {
        TTF_CloseFont(m_fontEnglish);
    }
    if (m_font) {
        TTF_CloseFont(m_font);
    }
    m_fontEnglish = nullptr;
    m_font = nullptr;
    if (m_texTitle) SDL_DestroyTexture(m_texTitle);
    if (m_texMagic) SDL_DestroyTexture(m_texMagic);
    if (m_texState) SDL_DestroyTexture(m_texState);
    if (m_texSystem) SDL_DestroyTexture(m_texSystem);
    if (m_texMap) SDL_DestroyTexture(m_texMap);
    if (m_texSkill) SDL_DestroyTexture(m_texSkill);
    if (m_texMenuEsc) SDL_DestroyTexture(m_texMenuEsc);
    if (m_texMenuEscBack) SDL_DestroyTexture(m_texMenuEscBack);
    if (m_texBattle) SDL_DestroyTexture(m_texBattle);
    if (m_texTeammate) SDL_DestroyTexture(m_texTeammate);
    if (m_texMenuItem) SDL_DestroyTexture(m_texMenuItem);
    if (m_texMenuBackground) SDL_DestroyTexture(m_texMenuBackground);
    ClearSkillIcons();
    
    TTF_Quit();
}

SDL_Color UIManager::Uint32ToColor(uint32_t color) {
    SDL_Color c;
    c.r = (color >> 16) & 0xFF;
    c.g = (color >> 8) & 0xFF;
    c.b = color & 0xFF;
    c.a = (color >> 24) & 0xFF;
    return c;
}

bool UIManager::LoadSystemGraphics() {
    if (m_texTitle) return true; 

    auto loadTex = [&](int index) -> SDL_Texture* {
        PicImage pic = PicLoader::loadPic("resource/Background.Pic", index);
        if (pic.surface) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(m_renderer, pic.surface);
            if (tex) {
                SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND); // 确保透明通道生效
            }
            PicLoader::freePic(pic);
            return tex;
        }
        return nullptr;
    };

    m_texTitle = loadTex(0);
    m_texMagic = loadTex(1);
    m_texState = loadTex(2);
    m_texSystem = loadTex(3);
    m_texMap = loadTex(4);
    m_texSkill = loadTex(5);
    m_texMenuEsc = loadTex(6);
    m_texMenuEscBack = loadTex(7);
    m_texBattle = loadTex(8);
    m_texTeammate = loadTex(9);
    m_texMenuItem = loadTex(10);
    
    return true;
}

void UIManager::EnsureSkillIconsLoaded() {
    if (!m_skillIcons.empty()) return;
    int count = PicLoader::getPicCount("resource/Skill.pic");
    if (count <= 0) return;
    m_skillIcons.reserve(count);
    for (int i = 0; i < count; ++i) {
        PicImage pic = PicLoader::loadPic("resource/Skill.pic", i);
        SkillIcon icon;
        if (pic.surface) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(m_renderer, pic.surface);
            if (tex) {
                SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
            }
            icon.tex = tex;
            icon.x = pic.x;
            icon.y = pic.y;
            icon.w = pic.surface->w;
            icon.h = pic.surface->h;
            PicLoader::freePic(pic);
        }
        m_skillIcons.push_back(icon);
    }
}

void UIManager::ClearSkillIcons() {
    for (auto& icon : m_skillIcons) {
        if (icon.tex) {
            SDL_DestroyTexture(icon.tex);
            icon.tex = nullptr;
        }
    }
    m_skillIcons.clear();
}

int UIManager::GetRemainingSkillPoints() {
    Role& hero = GameManager::getInstance().getRole(0);
    int addSkill = hero.getAddSkillPoint();
    if (addSkill > 10) addSkill = 10;
    int remaining = addSkill + hero.getLevel();
    for (int i = 1; i <= 5; ++i) {
        Role& pet = GameManager::getInstance().getRole(i);
        for (int s = 0; s < 5; ++s) {
            if (pet.getMagic(s) > 0) remaining -= (s + 1);
        }
    }
    return remaining;
}

void UIManager::CaptureScreen() {
    if (m_texMenuBackground) {
        SDL_DestroyTexture(m_texMenuBackground);
        m_texMenuBackground = nullptr;
    }
    SDL_Surface* surface = GameManager::getInstance().getScreenSurface();
    if (surface) {
        m_texMenuBackground = SDL_CreateTextureFromSurface(m_renderer, surface);
        if (m_texMenuBackground) {
            SDL_SetTextureBlendMode(m_texMenuBackground, SDL_BLENDMODE_NONE);
        }
        return;
    }
    SDL_Surface* readback = SDL_RenderReadPixels(m_renderer, NULL);
    if (readback) {
        m_texMenuBackground = SDL_CreateTextureFromSurface(m_renderer, readback);
        SDL_DestroySurface(readback);
    }
}

void UIManager::DrawRectangle(int x, int y, int w, int h, uint32_t colorin, uint32_t colorframe, int alpha) {
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    SDL_FRect rect = { (float)x, (float)y, (float)w, (float)h };
    Uint8 rin = (colorin >> 24) & 0xFF, gin = (colorin >> 16) & 0xFF, bin = (colorin >> 8) & 0xFF;
    Uint8 rfr = (colorframe >> 24) & 0xFF, gfr = (colorframe >> 16) & 0xFF, bfr = (colorframe >> 8) & 0xFF;
    SDL_SetRenderDrawColor(m_renderer, rin, gin, bin, alpha);
    SDL_RenderFillRect(m_renderer, &rect);
    for (int i1 = 0; i1 <= w; ++i1) {
        for (int i2 = 0; i2 <= h; ++i2) {
            int l1 = i1 + i2;
            int l2 = -(i1 - w) + (i2);
            int l3 = (i1) - (i2 - h);
            int l4 = -(i1 - w) - (i2 - h);
            bool isCorner = !((l1 >= 4) && (l2 >= 4) && (l3 >= 4) && (l4 >= 4));
            bool isEdge = ((l1 >= 4) && (l2 >= 4) && (l3 >= 4) && (l4 >= 4) && ((i1 == 0) || (i1 == w) || (i2 == 0) || (i2 == h))) || ((l1 == 4) || (l2 == 4) || (l3 == 4) || (l4 == 4));
            if (isCorner) {
                SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 0);
                SDL_RenderPoint(m_renderer, (float)(x + i1), (float)(y + i2));
            } else if (isEdge) {
                int a = (int)std::round(250.0 - std::abs((double)i1 / (double)w + (double)i2 / (double)h - 1.0) * 150.0);
                if (a < 0) a = 0;
                if (a > 255) a = 255;
                SDL_SetRenderDrawColor(m_renderer, rfr, gfr, bfr, a);
                SDL_RenderPoint(m_renderer, (float)(x + i1), (float)(y + i2));
            }
        }
    }
}

void UIManager::DrawFilledRect(int x, int y, int w, int h, uint32_t color, int alpha) {
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    Uint8 r = (color >> 24) & 0xFF;
    Uint8 g = (color >> 16) & 0xFF;
    Uint8 b = (color >> 8) & 0xFF;
    SDL_SetRenderDrawColor(m_renderer, r, g, b, alpha);
    SDL_FRect rect = { (float)x, (float)y, (float)w, (float)h };
    SDL_RenderFillRect(m_renderer, &rect);
}

void UIManager::DrawShadowTextUtf8(const std::string& text, int x, int y, uint32_t color1, uint32_t color2, int fontSize) {
    TTF_Font* useFont = (m_fontEnglish && IsAsciiOnly(text)) ? m_fontEnglish : m_font;
    if (!useFont) return;
    
    std::string displayText = TextManager::getInstance().traditionalToSimplified(text);
    
    color1 = ResolveTextColor(color1);
    color2 = ResolveTextColor(color2);

    SDL_Color c2 = { (Uint8)((color2 >> 24) & 0xFF), (Uint8)((color2 >> 16) & 0xFF), (Uint8)((color2 >> 8) & 0xFF), (Uint8)(color2 & 0xFF) };
    SDL_Color c1 = { (Uint8)((color1 >> 24) & 0xFF), (Uint8)((color1 >> 16) & 0xFF), (Uint8)((color1 >> 8) & 0xFF), (Uint8)(color1 & 0xFF) };
    
    SDL_Surface* surf2 = TTF_RenderText_Blended(useFont, displayText.c_str(), 0, c2);
    if (surf2) {
        SDL_Texture* tex2 = SDL_CreateTextureFromSurface(m_renderer, surf2);
        SDL_FRect dst2 = { (float)(x + 1), (float)y, (float)surf2->w, (float)surf2->h };
        SDL_RenderTexture(m_renderer, tex2, NULL, &dst2);
        SDL_DestroyTexture(tex2);
        SDL_DestroySurface(surf2);
    }
    
    SDL_Surface* surf1 = TTF_RenderText_Blended(useFont, displayText.c_str(), 0, c1);
    if (surf1) {
        SDL_Texture* tex1 = SDL_CreateTextureFromSurface(m_renderer, surf1);
        SDL_FRect dst1 = { (float)x, (float)y, (float)surf1->w, (float)surf1->h };
        SDL_RenderTexture(m_renderer, tex1, NULL, &dst1);
        SDL_DestroyTexture(tex1);
        SDL_DestroySurface(surf1);
    }
}

void UIManager::DrawHead(int headId, int x, int y, int green, int red, int gray) {
    if (headId < 0) return; // Prevent invalid head index

    PicImage pic = PicLoader::loadPic("resource/Heads.Pic", headId);
    if (pic.surface) {
         SDL_Texture* tex = SDL_CreateTextureFromSurface(m_renderer, pic.surface);
         // Apply offset like Pascal: x1 := px - Head_Pic[num].x + 1; y1 := py - Head_Pic[num].y + 1;
         float drawX = (float)(x - pic.x + 1);
         float drawY = (float)(y - pic.y + 1);
         SDL_FRect dest = { drawX, drawY, (float)pic.surface->w, (float)pic.surface->h };
         if (green > 0 || red > 0 || gray > 0) {
             int rMod = 255;
             int gMod = 255;
             int bMod = 255;
             int grayTint = std::min(std::max(gray, 0), 100);
             if (grayTint > 0) {
                 int factor = 100 - grayTint;
                 rMod = (rMod * factor) / 100;
                 gMod = (gMod * factor) / 100;
                 bMod = (bMod * factor) / 100;
             }
             int greenTint = std::min(std::max(green, 0), 150);
             if (greenTint > 0) {
                 int factor = 150 - greenTint;
                 rMod = (rMod * factor) / 150;
                 bMod = (bMod * factor) / 150;
             }
             int redTint = std::min(std::max(red, 0), 150);
             if (redTint > 0) {
                 int factor = 150 - redTint;
                 gMod = (gMod * factor) / 150;
                 bMod = (bMod * factor) / 150;
             }
             SDL_SetTextureColorMod(tex, static_cast<uint8_t>(rMod), static_cast<uint8_t>(gMod), static_cast<uint8_t>(bMod));
         }
         SDL_RenderTexture(m_renderer, tex, NULL, &dest);
         SDL_DestroyTexture(tex);
         PicLoader::freePic(pic);
    }
}

// Helper function to draw item pic with offset (like Pascal's drawPngPic)
void UIManager::DrawItemPicWithOffset(int itemId, int x, int y) {
    if (itemId < 0) return;
    
    PicImage pic = PicLoader::loadPic("resource/Items.Pic", itemId);
    if (pic.surface) {
         SDL_Texture* tex = SDL_CreateTextureFromSurface(m_renderer, pic.surface);
         // Apply offset like Pascal: x1 := px - image.x; y1 := py - image.y;
         float drawX = (float)(x - pic.x);
         float drawY = (float)(y - pic.y);
         SDL_FRect dest = { drawX, drawY, (float)pic.surface->w, (float)pic.surface->h };
         SDL_RenderTexture(m_renderer, tex, NULL, &dest);
         SDL_DestroyTexture(tex);
         PicLoader::freePic(pic);
    }
}

void UIManager::RenderMenuSystem(int menuSelection) {
    if (m_texMenuEscBack) {
        // 底图贴图 (Index 7) 包含两个横向并列的 300x300 圆环
        // Pascal 源码中使用 (0, 0, 300, 300) 作为显示底图
        SDL_FRect src = { 0, 0, 300, 300 };
        SDL_FRect dest = { 170, 70, 300, 300 };
        SDL_RenderTexture(m_renderer, m_texMenuEscBack, &src, &dest);
    }
    
    int x = 270;
    int y = 167;
    int N = 102;
    
    int positionX[6] = { x, x + N, x + N, x, x - N, x - N };
    int positionY[6] = { y - 117, y - 58, y + 58, y + 117, y + 58, y - 58 };
    
    for (int i = 0; i < 6; ++i) {
        float srcX = (float)((i % 3) * 100);
        float srcY = (float)((i / 3) * 100);
        
        if (i != menuSelection) {
            srcY += 200; // Not selected
        }
        
        SDL_FRect src = { srcX, srcY, 100, 100 };
        SDL_FRect dest = { (float)positionX[i], (float)positionY[i], 100, 100 };
        
        if (m_texMenuEsc) {
            SDL_RenderTexture(m_renderer, m_texMenuEsc, &src, &dest);
        }
    }
}

void UIManager::ShowMenu() {
    if (!m_texMenuEsc) LoadSystemGraphics();

    GameManager::getInstance().RenderScreenTo(m_renderer);
    CaptureScreen();

    bool running = true;
    int currentSelection = 0;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                GameManager::getInstance().Quit();
                return;
            }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                 if (event.key.key == SDLK_ESCAPE) {
                     running = false;
                 }
                 if (event.key.key == SDLK_DOWN || event.key.key == SDLK_KP_2) {
                     currentSelection = (currentSelection + 1) % 6;
                 } else if (event.key.key == SDLK_UP || event.key.key == SDLK_KP_8) {
                     currentSelection = (currentSelection + 5) % 6;
                 } else if (event.key.key == SDLK_RIGHT || event.key.key == SDLK_KP_6) {
                     // Approximate Pascal right logic
                      if (currentSelection == 0) currentSelection = 1;
                      else if (currentSelection == 3 || currentSelection == 4) currentSelection--;
                      else if (currentSelection == 5) currentSelection = 0;
                 } else if (event.key.key == SDLK_LEFT || event.key.key == SDLK_KP_4) {
                     // Approximate Pascal left logic
                     if (currentSelection == 0) currentSelection = 5;
                     else if (currentSelection == 2 || currentSelection == 3) currentSelection++;
                     else if (currentSelection == 5 || currentSelection == 1) currentSelection--;
                 }
                 
                 if (event.key.key == SDLK_SPACE || event.key.key == SDLK_RETURN) {
                     if (currentSelection == 0) SelectShowMagic();
                     else if (currentSelection == 1) SelectShowStatus();
                     else if (currentSelection == 2) SelectShowSystem();
                     else if (currentSelection == 3) SelectShowTeammate();
                     else if (currentSelection == 4) SelectShowSkill();
                     else if (currentSelection == 5) SelectShowItem();
                 }
            }
        }

        SDL_RenderClear(m_renderer);
        
        if (m_texMenuBackground) {
             SDL_RenderTexture(m_renderer, m_texMenuBackground, NULL, NULL);
        }
        
        RenderMenuSystem(currentSelection);
        
        VirtualControls::present(m_renderer);
        SDL_Delay(16);
    }
}

void UIManager::SelectShowStatus() {
    auto& game = GameManager::getInstance();
    const auto team = GetValidTeamList();
    if (team.empty()) return;

    int currentIdx = 0;
    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                game.Quit();
                return;
            }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                switch (event.key.key) {
                    case SDLK_UP:
                    case SDLK_KP_8:
                    case SDLK_LEFT:
                        currentIdx--;
                        if (currentIdx < 0) currentIdx = team.size() - 1;
                        break;
                    case SDLK_DOWN:
                    case SDLK_KP_2:
                    case SDLK_RIGHT:
                        currentIdx++;
                        if (currentIdx >= (int)team.size()) currentIdx = 0;
                        break;
                    case SDLK_ESCAPE:
                        running = false;
                        break;
                }
            }
        }

        SDL_RenderClear(m_renderer);
        if (m_texMenuBackground) {
            SDL_RenderTexture(m_renderer, m_texMenuBackground, NULL, NULL);
        }

        ShowStatus(team[currentIdx]);

        VirtualControls::present(m_renderer);
        SDL_Delay(16);
    }
}

void UIManager::DrawEngShadowText(const std::string& text, int x, int y, uint32_t color1, uint32_t color2, int fontSize) {
    TTF_Font* useFont = m_fontEnglish ? m_fontEnglish : m_font;
    if (!useFont) return;

    color1 = ResolveTextColor(color1);
    color2 = ResolveTextColor(color2);

    SDL_Color c2 = { (Uint8)((color2 >> 24) & 0xFF), (Uint8)((color2 >> 16) & 0xFF), (Uint8)((color2 >> 8) & 0xFF), (Uint8)(color2 & 0xFF) };
    SDL_Color c1 = { (Uint8)((color1 >> 24) & 0xFF), (Uint8)((color1 >> 16) & 0xFF), (Uint8)((color1 >> 8) & 0xFF), (Uint8)(color1 & 0xFF) };

    SDL_Surface* surf2 = TTF_RenderText_Blended(useFont, text.c_str(), 0, c2);
    if (surf2) {
        SDL_Texture* tex2 = SDL_CreateTextureFromSurface(m_renderer, surf2);
        SDL_FRect dst2 = { (float)(x + 1), (float)y, (float)surf2->w, (float)surf2->h };
        SDL_RenderTexture(m_renderer, tex2, NULL, &dst2);
        SDL_DestroyTexture(tex2);
        SDL_DestroySurface(surf2);
    }

    SDL_Surface* surf1 = TTF_RenderText_Blended(useFont, text.c_str(), 0, c1);
    if (surf1) {
        SDL_Texture* tex1 = SDL_CreateTextureFromSurface(m_renderer, surf1);
        SDL_FRect dst1 = { (float)x, (float)y, (float)surf1->w, (float)surf1->h };
        SDL_RenderTexture(m_renderer, tex1, NULL, &dst1);
        SDL_DestroyTexture(tex1);
        SDL_DestroySurface(surf1);
    }
}

void UIManager::DrawHpMpStatus(int roleId, int x, int y) {
    Role& role = GameManager::getInstance().getRole(roleId);
    DrawShadowTextUtf8(" 生命", x, y + 21, 0x21FFFFFF, 0x23FFFFFF);
    DrawShadowTextUtf8(" 內力", x, y + 42, 0x21FFFFFF, 0x23FFFFFF);
    DrawShadowTextUtf8(" 體力", x, y + 63, 0x21FFFFFF, 0x23FFFFFF);

    uint32_t hpCur1 = 0;
    uint32_t hpCur2 = 0;
    uint32_t hpMax1 = 0;
    uint32_t hpMax2 = 0;
    ResolveHpColors(role, hpCur1, hpCur2, hpMax1, hpMax2);
    DrawEngShadowText(FormatPaddedNumber(role.getCurrentHP(), 4), x + 125, y + 21, hpCur1, hpCur2);
    DrawEngShadowText("/", x + 165, y + 21, 0x63FFFFFF, 0x66FFFFFF);
    DrawEngShadowText(FormatPaddedNumber(role.getMaxHP(), 4), x + 175, y + 21, hpMax1, hpMax2);
    uint8_t hpBarColor = 0x07;
    if (role.getHurt() >= 67) hpBarColor = 0x16;
    else if (role.getHurt() >= 34) hpBarColor = 0x0E;
    DrawStatusBar(x + 65, y + 24, role.getCurrentHP(), role.getMaxHP(), hpBarColor);

    uint8_t mpColor1 = 0x63, mpColor2 = 0x66;
    uint8_t mpBarColor = 0x66;
    if (role.getMPType() == 1) {
        mpColor1 = 0x4E; mpColor2 = 0x50; mpBarColor = 0x50;
    } else if (role.getMPType() == 0) {
        mpColor1 = 0x05; mpColor2 = 0x07; mpBarColor = 0x07;
    }
    uint32_t mpText1 = ((uint32_t)mpColor1 << 24) | 0xFFFFFF;
    uint32_t mpText2 = ((uint32_t)mpColor2 << 24) | 0xFFFFFF;
    DrawEngShadowText(FormatPaddedNumber(role.getCurrentMP(), 4), x + 125, y + 42, mpText1, mpText2);
    DrawEngShadowText("/", x + 165, y + 42, 0x63FFFFFF, 0x66FFFFFF);
    DrawEngShadowText(FormatPaddedNumber(role.getMaxMP(), 4), x + 175, y + 42, mpText1, mpText2);
    if (role.getMaxMP() > 0) {
        DrawStatusBar(x + 65, y + 45, role.getCurrentMP(), role.getMaxMP(), mpBarColor);
    }

    DrawEngShadowText(FormatPaddedNumber(role.getPhyPower(), 4), x + 125, y + 63, 0x05FFFFFF, 0x07FFFFFF);
    DrawEngShadowText("/100", x + 165, y + 63, 0x63FFFFFF, 0x66FFFFFF);
    DrawStatusBar(x + 65, y + 66, role.getPhyPower(), MAX_PHYSICAL_POWER, 0x46);
}

void UIManager::ShowSimpleStatus(int roleId, int x, int y, int frozen) {
    Role& role = GameManager::getInstance().getRole(roleId);
    if (!m_texBattle) LoadSystemGraphics();
    int panelX = x;
    int panelY = y - 20;
    DrawRectangle(panelX, panelY, 300, 115, 0x000000, 0xFFFFFFFF, 30);
    if (m_texBattle) {
        SDL_FRect dest = { (float)panelX, (float)panelY, 300.0f, 115.0f };
        SDL_RenderTexture(m_renderer, m_texBattle, NULL, &dest);
    }
    StatusPulse pulse = ComputeStatusPulse(role.getPoision(), role.getHurt(), frozen);
    DrawHead(role.getHeadNum(), panelX + 22, panelY + 64, pulse.green, pulse.red, pulse.gray);
    std::string nameUtf8 = TextManager::getInstance().gbkToUtf8(role.getName());
    DrawShadowTextUtf8(nameUtf8, panelX + 30, panelY + 86, 0x63FFFFFF, 0x66FFFFFF);
    DrawShadowTextUtf8(" 等級", panelX + 77, panelY + 5, 0x21FFFFFF, 0x23FFFFFF);
    DrawEngShadowText(std::to_string(role.getLevel()), panelX + 143, panelY + 5, 0x05FFFFFF, 0x07FFFFFF);
    DrawHpMpStatus(roleId, panelX + 77, panelY + 5);
}

namespace {
    // Battle item panel is shifted left so it aligns with the 640-wide battle view under TTF rendering.
    constexpr int kBattleItemShiftX = -30;

    void DrawBattleItemFrame(int col, int row) {
        int px = col * 82 + 115 + 12 + kBattleItemShiftX;
        int py = row * 82 + 97 - 14;
        uint32_t frameColor = PaletteIndexToColor(255);
        UIManager::getInstance().DrawRectangle(px, py, 80, 80, 0, frameColor, 0);
        UIManager::getInstance().DrawRectangle(px + 1, py + 1, 78, 78, 0, frameColor, 0);
    }

    void DrawBattleItemPanel(const std::vector<int>& itemIds, int col, int row, int atlu) {
        const int cols = 6;
        const int rows = 3;
        const int infoX = 122 + kBattleItemShiftX;
        const int infoW = 499;
        const int gridX = 115 + 12 + kBattleItemShiftX;
        const int gridY = 95 - 14;
        const char* typeLabels[] = { " 劇情物品", " 神兵寶甲", " 武功秘笈", " 靈丹妙藥", " 傷人暗器" };

        UIManager::getInstance().DrawRectangle(infoX, 16, infoW, 25, 0, 0xFFFFFFFF, 40);
        UIManager::getInstance().DrawRectangle(infoX, 46, infoW, 25, 0, 0xFFFFFFFF, 40);
        UIManager::getInstance().DrawRectangle(infoX, 76, infoW, 252, 0, 0xFFFFFFFF, 40);
        UIManager::getInstance().DrawRectangle(infoX, 335, infoW, 86, 0, 0xFFFFFFFF, 40);

        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                int listIndex = r * cols + c + atlu;
                if (listIndex < 0 || listIndex >= (int)itemIds.size()) continue;
                int itemId = itemIds[listIndex];
                PicImage pic = PicLoader::loadPic("resource/Items.Pic", itemId);
                if (pic.surface) {
                    SDL_Texture* tex = SDL_CreateTextureFromSurface(UIManager::getInstance().GetRenderer(), pic.surface);
                    if (tex) {
                        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
                        SDL_FRect dest = { (float)(gridX + c * 82), (float)(gridY + r * 82), (float)pic.surface->w, (float)pic.surface->h };
                        SDL_RenderTexture(UIManager::getInstance().GetRenderer(), tex, NULL, &dest);
                        SDL_DestroyTexture(tex);
                    }
                    PicLoader::freePic(pic);
                }
            }
        }

        DrawBattleItemFrame(col, row);

        int selectedIndex = row * cols + col + atlu;
        if (selectedIndex >= 0 && selectedIndex < (int)itemIds.size()) {
            int itemId = itemIds[selectedIndex];
            Item& item = GameManager::getInstance().getItem(itemId);
            int amount = GameManager::getInstance().getItemAmount(itemId);
            std::string nameUtf8 = TextManager::getInstance().gbkToUtf8(item.getName());
            std::string introUtf8 = TextManager::getInstance().gbkToUtf8(item.getIntroduction());
            UIManager::getInstance().DrawShadowTextUtf8(nameUtf8, 134 + kBattleItemShiftX, 20, 0x21FFFFFF, 0x23FFFFFF);
            UIManager::getInstance().DrawShadowTextUtf8(introUtf8, 134 + kBattleItemShiftX, 50, 0x05FFFFFF, 0x07FFFFFF);
            UIManager::getInstance().DrawEngShadowText(FormatPaddedNumber(amount, 5), 493 + kBattleItemShiftX, 20, 0x64FFFFFF, 0x66FFFFFF);
            int t = item.getItemType();
            if (t >= 0 && t <= 4) {
                UIManager::getInstance().DrawShadowTextUtf8(typeLabels[t], 109 + kBattleItemShiftX, 337, 0x21FFFFFF, 0x23FFFFFF);
            }
        }
    }
}

int UIManager::ShowBattleItemMenu(const std::function<void()>& redrawBackground) {
    const int cols = 6;
    const int rows = 3;
    const int pageSize = cols * rows;

    std::vector<int> itemIds;
    const auto& inventory = GameManager::getInstance().getItemList();
    for (const auto& it : inventory) {
        if (it.id < 0 || it.amount <= 0) continue;
        Item& item = GameManager::getInstance().getItem(it.id);
        if (item.getItemType() == 3 || item.getItemType() == 4) {
            itemIds.push_back(it.id);
        }
    }
    if (itemIds.empty()) return -1;

    int col = 0;
    int row = 0;
    int atlu = 0;
    bool running = true;
    int selectedItem = -1;
    SDL_Event event;

    while (running) {
        if (redrawBackground) redrawBackground();
        DrawBattleItemPanel(itemIds, col, row, atlu);
        UpdateScreen();

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                GameManager::getInstance().Quit();
                return -1;
            }
            if (event.type != SDL_EVENT_KEY_UP) continue;

            if (event.key.key == SDLK_ESCAPE) {
                return -1;
            }
            if (event.key.key == SDLK_DOWN || event.key.key == SDLK_KP_2) {
                row += 1;
                if (row >= rows) {
                    if (atlu + pageSize < (int)itemIds.size()) atlu += cols;
                    row = rows - 1;
                }
            } else if (event.key.key == SDLK_UP || event.key.key == SDLK_KP_8) {
                row -= 1;
                if (row < 0) {
                    row = 0;
                    if (atlu > 0) atlu -= cols;
                }
            } else if (event.key.key == SDLK_RIGHT || event.key.key == SDLK_KP_6) {
                col = (col + 1) % cols;
            } else if (event.key.key == SDLK_LEFT || event.key.key == SDLK_KP_4) {
                col = (col + cols - 1) % cols;
            } else if (event.key.key == SDLK_PAGEDOWN) {
                atlu += pageSize;
                int maxStart = std::max(0, ((int)itemIds.size() - 1) / cols - rows + 1) * cols;
                if (atlu > maxStart) {
                    atlu = maxStart;
                    row = ((int)itemIds.size() - 1 - atlu) / cols;
                    if (row >= rows) row = rows - 1;
                }
            } else if (event.key.key == SDLK_PAGEUP) {
                atlu -= pageSize;
                if (atlu < 0) {
                    row += atlu / cols;
                    atlu = 0;
                    if (row < 0) row = 0;
                }
            } else if (event.key.key == SDLK_RETURN || event.key.key == SDLK_KP_ENTER || event.key.key == SDLK_SPACE) {
                int selectedIndex = row * cols + col + atlu;
                if (selectedIndex >= 0 && selectedIndex < (int)itemIds.size()) {
                    selectedItem = itemIds[selectedIndex];
                    running = false;
                }
            }
        }
        SDL_Delay(16);
    }
    return selectedItem;
}

void UIManager::ShowStatus(int roleId) {
    if (!m_texState) LoadSystemGraphics();

    if (m_texState) {
        SDL_RenderTexture(m_renderer, m_texState, NULL, NULL);
    }
    
    Role& role = GameManager::getInstance().getRole(roleId);
    const auto team = GetValidTeamList();
    int textOffsetX = 8;
    int textOffsetY = 4;
    int headOffsetY = 6;

    // Draw Team List
    DrawRectangle(15, 15, 90, 10 + team.size() * 22, 0x00000000, 0xFFFFFFFF, 30);
    for (size_t i = 0; i < team.size(); ++i) {
        int id = team[i];
        Role& r = GameManager::getInstance().getRole(id);
        uint32_t color = (id == roleId) ? 0xFFFFFFFF : 0xFFFF00FF;
        std::string nameUtf8 = TextManager::getInstance().gbkToUtf8(r.getName());
        DrawShadowTextUtf8(nameUtf8, 20 + textOffsetX, 20 + i * 22 + textOffsetY, color, 0x000000FF);
    }

    StatusPulse pulse = ComputeStatusPulse(role.getPoision(), role.getHurt(), 0);
    DrawHead(role.getHeadNum(), 137, 88 + headOffsetY, pulse.green, pulse.red, pulse.gray);
    std::string roleNameUtf8 = TextManager::getInstance().gbkToUtf8(role.getName());
    DrawShadowTextUtf8(roleNameUtf8, 115 + textOffsetX, 93 + textOffsetY, 0x64FFFFFF, 0x66FFFFFF);
 
    int x = 90;
    int y = 0;
    DrawShadowTextUtf8("  等級", x + 25 + textOffsetX, y + 94 + 21 + textOffsetY, 0x21FFFFFF, 0x23FFFFFF);
    DrawShadowTextUtf8("  經驗", x + 25 + textOffsetX, y + 94 + 42 + textOffsetY, 0x21FFFFFF, 0x23FFFFFF);
    DrawShadowTextUtf8("  升級", x + 25 + textOffsetX, y + 94 + 63 + textOffsetY, 0x21FFFFFF, 0x23FFFFFF);
    DrawShadowTextUtf8("  攻擊", x + 25 + textOffsetX, y + 115 + 21 * 3 + textOffsetY, 0x63FFFFFF, 0x66FFFFFF);
    DrawShadowTextUtf8("  防禦", x + 25 + textOffsetX, y + 115 + 21 * 4 + textOffsetY, 0x63FFFFFF, 0x66FFFFFF);
    DrawShadowTextUtf8("  輕功", x + 25 + textOffsetX, y + 115 + 21 * 5 + textOffsetY, 0x63FFFFFF, 0x66FFFFFF);
    DrawShadowTextUtf8("  醫療能力", x + 25 + textOffsetX, y + 115 + 21 * 6 + textOffsetY, 0x63FFFFFF, 0x66FFFFFF);
    DrawShadowTextUtf8("  用毒能力", x + 25 + textOffsetX, y + 115 + 21 * 7 + textOffsetY, 0x63FFFFFF, 0x66FFFFFF);
    DrawShadowTextUtf8("  解毒能力", x + 25 + textOffsetX, y + 115 + 21 * 8 + textOffsetY, 0x63FFFFFF, 0x66FFFFFF);
    DrawShadowTextUtf8("  拳掌功夫", x + 25 + textOffsetX, y + 115 + 21 * 9 + textOffsetY, 0x63FFFFFF, 0x66FFFFFF);
    DrawShadowTextUtf8("  御劍能力", x + 25 + textOffsetX, y + 115 + 21 * 10 + textOffsetY, 0x63FFFFFF, 0x66FFFFFF);
    DrawShadowTextUtf8("  耍刀技巧", x + 25 + textOffsetX, y + 115 + 21 * 11 + textOffsetY, 0x63FFFFFF, 0x66FFFFFF);
    DrawShadowTextUtf8("  奇門兵器", x + 25 + textOffsetX, y + 115 + 21 * 12 + textOffsetY, 0x63FFFFFF, 0x66FFFFFF);
    DrawShadowTextUtf8("  暗器技巧", x + 25 + textOffsetX, y + 115 + 21 * 13 + textOffsetY, 0x63FFFFFF, 0x66FFFFFF);
 
    DrawShadowTextUtf8("  中毒", x + 25 + 79 + textOffsetX, y + 115 - 21 + textOffsetY, 0x30FFFFFF, 0x32FFFFFF);
    DrawShadowTextUtf8(std::to_string(role.getPoision()), x + 25 + 150 + textOffsetX, y + 115 - 21 + textOffsetY, 0x63FFFFFF, 0x66FFFFFF);
    DrawShadowTextUtf8("  內傷", x + 30 + 179 + textOffsetX, y + 115 - 21 + textOffsetY, 0x13FFFFFF, 0x16FFFFFF);
    DrawShadowTextUtf8(std::to_string(role.getHurt()), x + 125 + 155 + textOffsetX, y + 115 - 21 + textOffsetY, 0x63FFFFFF, 0x66FFFFFF);
 
    int addatk = 0, adddef = 0, addspeed = 0;
    for (int i = 0; i < 5; ++i) {
        int itemId = role.getEquip(i);
        if (itemId >= 0) {
            Item& item = GameManager::getInstance().getItem(itemId);
            addatk += item.getAddAttack();
            adddef += item.getAddDefence();
            addspeed += item.getAddSpeed();
        }
    }
    if (GameManager::getInstance().CheckEquipSet(role.getEquip(0), role.getEquip(1), role.getEquip(2), role.getEquip(3)) == 5) {
        addatk += 50;
        addspeed += 30;
        adddef += -25;
    }
    auto fmtBasePlus = [](int base, int add) {
        if (add > 0) return std::to_string(base) + "+" + std::to_string(add);
        if (add < 0) return std::to_string(base) + "-" + std::to_string(-add);
        return std::to_string(base);
    };
    DrawShadowTextUtf8(fmtBasePlus(GameManager::getInstance().GetRoleAttack(roleId, false), addatk), x + 145 + textOffsetX, y + 115 + 21 * 3 + textOffsetY, 0x05FFFFFF, 0x07FFFFFF);
    DrawShadowTextUtf8(fmtBasePlus(GameManager::getInstance().GetRoleDefence(roleId, false), adddef), x + 145 + textOffsetX, y + 115 + 21 * 4 + textOffsetY, 0x05FFFFFF, 0x07FFFFFF);
    DrawShadowTextUtf8(fmtBasePlus(GameManager::getInstance().GetRoleSpeed(roleId, false), addspeed), x + 145 + textOffsetX, y + 115 + 21 * 5 + textOffsetY, 0x05FFFFFF, 0x07FFFFFF);
 
    DrawShadowTextUtf8(std::to_string(GameManager::getInstance().GetRoleMedcine(roleId, true)), x + 145 + textOffsetX, y + 115 + 21 * 6 + textOffsetY, 0x05FFFFFF, 0x07FFFFFF);
    DrawShadowTextUtf8(std::to_string(GameManager::getInstance().GetRoleUsePoi(roleId, true)), x + 145 + textOffsetX, y + 115 + 21 * 7 + textOffsetY, 0x05FFFFFF, 0x07FFFFFF);
    DrawShadowTextUtf8(std::to_string(GameManager::getInstance().GetRoleMedPoi(roleId, true)), x + 145 + textOffsetX, y + 115 + 21 * 8 + textOffsetY, 0x05FFFFFF, 0x07FFFFFF);
    DrawShadowTextUtf8(std::to_string(GameManager::getInstance().GetRoleFist(roleId, true)), x + 145 + textOffsetX, y + 115 + 21 * 9 + textOffsetY, 0x05FFFFFF, 0x07FFFFFF);
    DrawShadowTextUtf8(std::to_string(GameManager::getInstance().GetRoleSword(roleId, true)), x + 145 + textOffsetX, y + 115 + 21 * 10 + textOffsetY, 0x05FFFFFF, 0x07FFFFFF);
    DrawShadowTextUtf8(std::to_string(GameManager::getInstance().GetRoleKnife(roleId, true)), x + 145 + textOffsetX, y + 115 + 21 * 11 + textOffsetY, 0x05FFFFFF, 0x07FFFFFF);
    DrawShadowTextUtf8(std::to_string(GameManager::getInstance().GetRoleUnusual(roleId, true)), x + 145 + textOffsetX, y + 115 + 21 * 12 + textOffsetY, 0x05FFFFFF, 0x07FFFFFF);
    DrawShadowTextUtf8(std::to_string(GameManager::getInstance().GetRoleHidWeapon(roleId, true)), x + 145 + textOffsetX, y + 115 + 21 * 13 + textOffsetY, 0x05FFFFFF, 0x07FFFFFF);
 

    DrawShadowTextUtf8(std::to_string(role.getLevel()), x + 145 + textOffsetX, y + 115 + textOffsetY, 0x05FFFFFF, 0x07FFFFFF);
    DrawShadowTextUtf8(std::to_string(role.getExp()), x + 135 + textOffsetX, y + 136 + textOffsetY, 0x05FFFFFF, 0x07FFFFFF);
    int nextExp = GameManager::getInstance().getNextLevelExp(role.getLevel());
    if (nextExp < 0) {
        DrawShadowTextUtf8("    =", x + 135 + textOffsetX, y + 157 + textOffsetY, 0x05FFFFFF, 0x07FFFFFF);
    } else {
        DrawShadowTextUtf8(std::to_string(nextExp), x + 135 + textOffsetX, y + 157 + textOffsetY, 0x05FFFFFF, 0x07FFFFFF);
    }

    int hpBaseX = x + 80 + 25;
    int hpBaseY = y - 85 + 94;
    DrawShadowTextUtf8("  生命", hpBaseX + textOffsetX, hpBaseY + 21 + textOffsetY, 0x21FFFFFF, 0x23FFFFFF);
    DrawShadowTextUtf8("  內力", hpBaseX + textOffsetX, hpBaseY + 42 + textOffsetY, 0x21FFFFFF, 0x23FFFFFF);
    DrawShadowTextUtf8("  體力", hpBaseX + textOffsetX, hpBaseY + 63 + textOffsetY, 0x21FFFFFF, 0x23FFFFFF);
    uint32_t hpCur1 = 0;
    uint32_t hpCur2 = 0;
    uint32_t hpMax1 = 0;
    uint32_t hpMax2 = 0;
    ResolveHpColors(role, hpCur1, hpCur2, hpMax1, hpMax2);
    DrawShadowTextUtf8(FormatPaddedNumber(role.getCurrentHP(), 4), hpBaseX + 125 + textOffsetX, hpBaseY + 21 + textOffsetY, hpCur1, hpCur2);
    DrawShadowTextUtf8("/", hpBaseX + 165 + textOffsetX, hpBaseY + 21 + textOffsetY, 0x63FFFFFF, 0x66FFFFFF);
    DrawShadowTextUtf8(FormatPaddedNumber(role.getMaxHP(), 4), hpBaseX + 175 + textOffsetX, hpBaseY + 21 + textOffsetY, hpMax1, hpMax2);
    
    uint8_t hpBarColor = 0x07;
    if (role.getHurt() >= 67) hpBarColor = 0x16;
    else if (role.getHurt() >= 34) hpBarColor = 0x0E;
    DrawStatusBar(hpBaseX + 65 + textOffsetX, hpBaseY + 22 + textOffsetY, role.getCurrentHP(), role.getMaxHP(), hpBarColor);
    
    uint8_t mpColor1 = 0x63, mpColor2 = 0x66;
    uint8_t mpBarColor = 0x66;
    if (role.getMPType() == 1) {
        mpColor1 = 0x4E; mpColor2 = 0x50; mpBarColor = 0x50;
    } else if (role.getMPType() == 0) {
        mpColor1 = 0x05; mpColor2 = 0x07; mpBarColor = 0x07;
    }
    DrawShadowTextUtf8(FormatPaddedNumber(role.getCurrentMP(), 4), hpBaseX + 125 + textOffsetX, hpBaseY + 42 + textOffsetY, ((uint32_t)mpColor1 << 24) | 0xFFFFFF, ((uint32_t)mpColor2 << 24) | 0xFFFFFF);
    DrawShadowTextUtf8("/", hpBaseX + 165 + textOffsetX, hpBaseY + 42 + textOffsetY, 0x63FFFFFF, 0x66FFFFFF);
    DrawShadowTextUtf8(FormatPaddedNumber(role.getMaxMP(), 4), hpBaseX + 175 + textOffsetX, hpBaseY + 42 + textOffsetY, ((uint32_t)mpColor1 << 24) | 0xFFFFFF, ((uint32_t)mpColor2 << 24) | 0xFFFFFF);
    if (role.getMaxMP() > 0) {
        DrawStatusBar(hpBaseX + 65 + textOffsetX, hpBaseY + 43 + textOffsetY, role.getCurrentMP(), role.getMaxMP(), mpBarColor);
    }
    
    DrawShadowTextUtf8(FormatPaddedNumber(role.getPhyPower(), 4), hpBaseX + 125 + textOffsetX, hpBaseY + 63 + textOffsetY, 0x05FFFFFF, 0x07FFFFFF);
    DrawShadowTextUtf8("/100", hpBaseX + 165 + textOffsetX, hpBaseY + 63 + textOffsetY, 0x63FFFFFF, 0x66FFFFFF);
    DrawStatusBar(hpBaseX + 65 + textOffsetX, hpBaseY + 64 + textOffsetY, role.getPhyPower(), MAX_PHYSICAL_POWER, 0x46);
 
    DrawShadowTextUtf8(" 武器", x + 190 + textOffsetX, y + 115 + 21 * 9 + textOffsetY, 0x05FFFFFF, 0x07FFFFFF);
    if (role.getEquip(0) >= 0) {
        Item& item = GameManager::getInstance().getItem(role.getEquip(0));
        std::string itemNameUtf8 = TextManager::getInstance().gbkToUtf8(item.getName());
        DrawShadowTextUtf8(itemNameUtf8, x + 240 + textOffsetX, y + 115 + 21 * 9 + textOffsetY, 0x63FFFFFF, 0x66FFFFFF);
        DrawItemPicWithOffset(role.getEquip(0), 411, 144);
    } else {
        DrawShadowTextUtf8(" 無", x + 240 + textOffsetX, y + 115 + 21 * 9 + textOffsetY, 0x63FFFFFF, 0x66FFFFFF);
    }
 
    DrawShadowTextUtf8(" 身披", x + 190 + textOffsetX, y + 115 + 21 * 10 + textOffsetY, 0x05FFFFFF, 0x07FFFFFF);
    if (role.getEquip(1) >= 0) {
        Item& item = GameManager::getInstance().getItem(role.getEquip(1));
        std::string itemNameUtf8 = TextManager::getInstance().gbkToUtf8(item.getName());
        DrawShadowTextUtf8(itemNameUtf8, x + 240 + textOffsetX, y + 115 + 21 * 10 + textOffsetY, 0x63FFFFFF, 0x66FFFFFF);
        DrawItemPicWithOffset(role.getEquip(1), 523, 144);
    } else {
        DrawShadowTextUtf8(" 無", x + 240 + textOffsetX, y + 115 + 21 * 10 + textOffsetY, 0x63FFFFFF, 0x66FFFFFF);
    }
 
    DrawShadowTextUtf8(" 頭戴", x + 190 + textOffsetX, y + 115 + 21 * 11 + textOffsetY, 0x05FFFFFF, 0x07FFFFFF);
    if (role.getEquip(2) >= 0) {
        Item& item = GameManager::getInstance().getItem(role.getEquip(2));
        std::string itemNameUtf8 = TextManager::getInstance().gbkToUtf8(item.getName());
        DrawShadowTextUtf8(itemNameUtf8, x + 240 + textOffsetX, y + 115 + 21 * 11 + textOffsetY, 0x63FFFFFF, 0x66FFFFFF);
        DrawItemPicWithOffset(role.getEquip(2), 466, 42);
    } else {
        DrawShadowTextUtf8(" 無", x + 240 + textOffsetX, y + 115 + 21 * 11 + textOffsetY, 0x63FFFFFF, 0x66FFFFFF);
    }
 
    DrawShadowTextUtf8(" 腳踩", x + 190 + textOffsetX, y + 115 + 21 * 12 + textOffsetY, 0x05FFFFFF, 0x07FFFFFF);
    if (role.getEquip(3) >= 0) {
        Item& item = GameManager::getInstance().getItem(role.getEquip(3));
        std::string itemNameUtf8 = TextManager::getInstance().gbkToUtf8(item.getName());
        DrawShadowTextUtf8(itemNameUtf8, x + 240 + textOffsetX, y + 115 + 21 * 12 + textOffsetY, 0x63FFFFFF, 0x66FFFFFF);
        DrawItemPicWithOffset(role.getEquip(3), 466, 318);
    } else {
        DrawShadowTextUtf8(" 無", x + 240 + textOffsetX, y + 115 + 21 * 12 + textOffsetY, 0x63FFFFFF, 0x66FFFFFF);
    }
}

void UIManager::SelectShowMagic() {
    if (!m_texMagic) LoadSystemGraphics();
    
    auto& game = GameManager::getInstance();
    const auto team = GetValidTeamList();
    if (team.empty()) return;

    int currentIdx = 0;
    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                game.Quit();
                return;
            }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                switch (event.key.key) {
                    case SDLK_UP:
                    case SDLK_KP_8:
                        currentIdx--;
                        if (currentIdx < 0) currentIdx = team.size() - 1;
                        break;
                    case SDLK_DOWN:
                    case SDLK_KP_2:
                        currentIdx++;
                        if (currentIdx >= (int)team.size()) currentIdx = 0;
                        break;
                    case SDLK_ESCAPE:
                        running = false;
                        break;
                    case SDLK_RETURN:
                    case SDLK_KP_ENTER:
                    case SDLK_SPACE:
                        if (InModeMagic(team[currentIdx])) {
                            running = false;
                            break;
                        }
                        break;
                }
            }
        }

        SDL_RenderClear(m_renderer);
        if (m_texMenuBackground) {
            SDL_RenderTexture(m_renderer, m_texMenuBackground, NULL, NULL);
        }

        ShowMagic(team[currentIdx], -1);

        VirtualControls::present(m_renderer);
        SDL_Delay(16);
    }
}

void UIManager::ShowMagic(int roleId, int selectedIndex) {
    if (!m_texMagic) LoadSystemGraphics();

    if (m_texMagic) {
        SDL_RenderTexture(m_renderer, m_texMagic, NULL, NULL);
    }

    Role& role = GameManager::getInstance().getRole(roleId);
    const auto team = GetValidTeamList();
    int textOffsetX = 8;
    int extraSkillOffsetX = 6;
    int headOffsetY = 6;

    // Draw Team List
    DrawRectangle(15, 15, 90, 10 + team.size() * 22, 0x00000000, 0xFFFFFFFF, 30);
    for (size_t i = 0; i < team.size(); ++i) {
        int id = team[i];
        Role& r = GameManager::getInstance().getRole(id);
        uint32_t color = (id == roleId) ? 0xFFFFFFFF : 0xFFFF00FF;
        std::string nameUtf8 = TextManager::getInstance().gbkToUtf8(r.getName());
        DrawShadowTextUtf8(nameUtf8, 20 + textOffsetX, 20 + i * 22, color, 0x000000FF);
    }

    StatusPulse pulse = ComputeStatusPulse(role.getPoision(), role.getHurt(), 0);
    DrawHead(role.getHeadNum(), 137, 88 + headOffsetY, pulse.green, pulse.red, pulse.gray);
    std::string roleNameUtf8 = TextManager::getInstance().gbkToUtf8(role.getName());
    DrawShadowTextUtf8(roleNameUtf8, 115 + textOffsetX, 93, 0xFFFFFFFF, 0x000000FF);

    int x = 90;
    int y = 0;

    int hpMpX = x + 25 + textOffsetX;
    int hpMpY = y + 94;
    
    DrawShadowTextUtf8(" 生命", hpMpX, hpMpY + 21, 0x21FFFFFF, 0x23FFFFFF);
    DrawShadowTextUtf8(" 內力", hpMpX, hpMpY + 42, 0x21FFFFFF, 0x23FFFFFF);
    DrawShadowTextUtf8(" 體力", hpMpX, hpMpY + 63, 0x21FFFFFF, 0x23FFFFFF);

    // HP
    uint32_t hpCur1 = 0;
    uint32_t hpCur2 = 0;
    uint32_t hpMax1 = 0;
    uint32_t hpMax2 = 0;
    ResolveHpColors(role, hpCur1, hpCur2, hpMax1, hpMax2);
    DrawShadowTextUtf8(FormatPaddedNumber(role.getCurrentHP(), 4), hpMpX + 125, hpMpY + 21, hpCur1, hpCur2);
    DrawShadowTextUtf8("/", hpMpX + 165, hpMpY + 21, 0x63FFFFFF, 0x66FFFFFF);
    DrawShadowTextUtf8(FormatPaddedNumber(role.getMaxHP(), 4), hpMpX + 175, hpMpY + 21, hpMax1, hpMax2);
    uint8_t hpBarColor = 0x07;
    if (role.getHurt() >= 67) hpBarColor = 0x16;
    else if (role.getHurt() >= 34) hpBarColor = 0x0E;
    DrawStatusBar(hpMpX + 65, hpMpY + 24, role.getCurrentHP(), role.getMaxHP(), hpBarColor);
    
    // MP
    uint8_t mpColor1 = 0x63, mpColor2 = 0x66;
    uint8_t mpBarColor = 0x66;
    if (role.getMPType() == 1) {
        mpColor1 = 0x4E; mpColor2 = 0x50; mpBarColor = 0x50;
    } else if (role.getMPType() == 0) {
        mpColor1 = 0x05; mpColor2 = 0x07; mpBarColor = 0x07;
    }
    DrawShadowTextUtf8(FormatPaddedNumber(role.getCurrentMP(), 4), hpMpX + 125, hpMpY + 42, ((uint32_t)mpColor1 << 24) | 0xFFFFFF, ((uint32_t)mpColor2 << 24) | 0xFFFFFF);
    DrawShadowTextUtf8("/", hpMpX + 165, hpMpY + 42, 0x63FFFFFF, 0x66FFFFFF);
    DrawShadowTextUtf8(FormatPaddedNumber(role.getMaxMP(), 4), hpMpX + 175, hpMpY + 42, ((uint32_t)mpColor1 << 24) | 0xFFFFFF, ((uint32_t)mpColor2 << 24) | 0xFFFFFF);
    if (role.getMaxMP() > 0) {
        DrawStatusBar(hpMpX + 65, hpMpY + 45, role.getCurrentMP(), role.getMaxMP(), mpBarColor);
    }
    
    // PhyPower
    DrawShadowTextUtf8(FormatPaddedNumber(role.getPhyPower(), 4), hpMpX + 125, hpMpY + 63, 0x05FFFFFF, 0x07FFFFFF);
    DrawShadowTextUtf8("/100", hpMpX + 165, hpMpY + 63, 0x63FFFFFF, 0x66FFFFFF);
    DrawStatusBar(hpMpX + 65, hpMpY + 66, role.getPhyPower(), MAX_PHYSICAL_POWER, 0x46);

    // Practice Item
    DrawShadowTextUtf8(" 修煉物品", x + 110 + textOffsetX, y + 216, 0xFFD700FF, 0x000000FF);
    if (role.getPracticeBook() >= 0) {
        Item& book = GameManager::getInstance().getItem(role.getPracticeBook());
        std::string bookNameUtf8 = TextManager::getInstance().gbkToUtf8(book.getName());
        DrawShadowTextUtf8(bookNameUtf8, x + 110 + textOffsetX, y + 237, 0xFFFFFFFF, 0x000000FF);
        
        // Draw Item Pic
        DrawItemPicWithOffset(role.getPracticeBook(), 136, 208);
        
        // Draw Exp Progress
        int needExp = 0;
        if (book.getMagic() >= 0) {
            int magicId = book.getMagic();
            int magicLevel = 0;
            for (int i = 0; i < 10; i++) {
                if (role.getMagic(i) == magicId) {
                    magicLevel = role.getMagLevel(i);
                    break;
                }
            }
            Magic& magic = GameManager::getInstance().getMagic(magicId);
            if (magic.getMagicType() == 5 && magicLevel >= 0) {
                DrawShadowTextUtf8(std::to_string(role.getExpForBook()) + "/=", x + 137 + textOffsetX, y + 258, 0xFFFFFFFF, 0x000000FF);
            } else if (magicLevel < 900) {
                int aptitude = role.getAptitude();
                if (GameManager::getInstance().CheckEquipSet(role.getEquip(0), role.getEquip(1), role.getEquip(2), role.getEquip(3)) == 2) {
                    aptitude = 100;
                }
                if (book.getNeedExp() > 0) {
                    needExp = (book.getNeedExp() * (1 + magicLevel / 100) * (8 - aptitude / 15)) / 2;
                } else {
                    needExp = ((-book.getNeedExp()) * (1 + magicLevel / 100) * (1 + aptitude / 15)) / 2;
                }
                DrawShadowTextUtf8(std::to_string(role.getExpForBook()) + "/" + std::to_string(needExp), x + 137 + textOffsetX, y + 258, 0xFFFFFFFF, 0x000000FF);
            } else {
                DrawShadowTextUtf8(std::to_string(role.getExpForBook()) + "/=", x + 137 + textOffsetX, y + 258, 0xFFFFFFFF, 0x000000FF);
            }
        } else {
            int aptitude = role.getAptitude();
            if (GameManager::getInstance().CheckEquipSet(role.getEquip(0), role.getEquip(1), role.getEquip(2), role.getEquip(3)) == 2) {
                aptitude = 100;
            }
            if (book.getNeedExp() > 0) {
                needExp = (book.getNeedExp() * (8 - aptitude / 15)) / 2;
            } else {
                needExp = ((-book.getNeedExp()) * (1 + aptitude / 15)) / 2;
            }
            DrawShadowTextUtf8(std::to_string(role.getExpForBook()) + "/" + std::to_string(needExp), x + 137 + textOffsetX, y + 258, 0xFFFFFFFF, 0x000000FF);
        }
    } else {
        DrawShadowTextUtf8(" 無", x + 110 + textOffsetX, y + 237, 0xFFFFFFFF, 0x000000FF);
    }

    // Gongti Exp
    DrawShadowTextUtf8(" 功體經驗", x + 25 + textOffsetX, y + 184, 0xFFD700FF, 0x000000FF);
    DrawShadowTextUtf8(std::to_string(role.getGongtiExam()), x + 137 + textOffsetX, y + 184, 0xFFFFFFFF, 0x000000FF);

    // Special Skills (Top Right)
    const char* skills[] = { " 醫療", " 解毒", " 用毒", " 抗毒", " 毒攻", " " };
    uint32_t normalColor = 0xFFFFFFFF;
    uint32_t normalShadow = 0x000000FF;
    uint32_t selectedColor = 0x63FFFFFF;
    uint32_t selectedShadow = 0x66FFFFFF;
    for (int i = 0; i < 5; ++i) {
        bool selected = (selectedIndex == i);
        uint32_t color = selected ? selectedColor : normalColor;
        uint32_t shadow = selected ? selectedShadow : normalShadow;
        DrawShadowTextUtf8(skills[i], x + 248 + textOffsetX + extraSkillOffsetX + 78 * (i % 3), y + (i / 3) * 22 + 58, color, shadow);
    }

    // Magic List
    DrawShadowTextUtf8(" ————所會武功————", x + 247 + textOffsetX + extraSkillOffsetX, y + 102, 0xFFD700FF, 0x000000FF);
    int gongtiId = role.getGongti();
    for (int i = 0; i < 10; ++i) {
        int magicId = role.getMagic(i);
        if (magicId > 0) {
            Magic& m = GameManager::getInstance().getMagic(magicId);
            std::string magicNameUtf8 = TextManager::getInstance().gbkToUtf8(m.getName());
            
            bool selected = (selectedIndex == i + 6);
            bool isGongti = (magicId == gongtiId);
            
            uint32_t color, shadow;
            if (selected) {
                color = selectedColor;
                shadow = selectedShadow;
            } else if (isGongti) {
                color = 0x15FFFFFF;
                shadow = 0x18FFFFFF;
            } else {
                color = 0x05FFFFFF;
                shadow = 0x07FFFFFF;
            }
            DrawShadowTextUtf8(magicNameUtf8, x + 248 + textOffsetX + extraSkillOffsetX + 118 * (i % 2), y + (i / 2) * 22 + 124, color, shadow);
        }
    }

    // Magic Description Box (bottom left)
    if (selectedIndex >= 6) {
        int magicIndex = selectedIndex - 6;
        int magicId = role.getMagic(magicIndex);
        if (magicId > 0) {
            Magic& m = GameManager::getInstance().getMagic(magicId);
            
            std::string magicNameUtf8 = TextManager::getInstance().gbkToUtf8(m.getName());
            DrawShadowTextUtf8(magicNameUtf8, x + 50 + textOffsetX, y + 320, 0x05FFFFFF, 0x07FFFFFF);
            
            int magicLevel = role.getMagLevel(magicIndex);
            std::string levelStr;
            if (m.getMagicType() == 5) {
                int gLevel = GameManager::getInstance().GetGongtiLevel(roleId, magicId);
                if (gLevel == 0) levelStr = " 熟練";
                else if (gLevel == 1) levelStr = " 精純";
                else levelStr = " 化境";
            } else {
                levelStr = std::to_string(magicLevel / 100 + 1);
            }
            DrawShadowTextUtf8(levelStr, x + 188 + textOffsetX, y + 320, 0x05FFFFFF, 0x07FFFFFF);
            
            std::string introGbk = m.getIntroduction();
            std::string formattedIntro;
            for (size_t i1 = 0; i1 < introGbk.length(); ++i1) {
                formattedIntro += introGbk[i1];
                if ((i1 % 18 == 17) && (i1 + 1 < introGbk.length())) {
                    formattedIntro += '*';
                }
            }
            std::string intro = TextManager::getInstance().gbkToUtf8(formattedIntro);
            
            int lineY = y + 345;
            size_t pos = 0;
            int lineCount = 0;
            while (pos < intro.length()) {
                size_t nextPos = intro.find('*', pos);
                if (nextPos == std::string::npos) nextPos = intro.length();
                std::string line = intro.substr(pos, nextPos - pos);
                DrawShadowTextUtf8(line, x + 50 + textOffsetX, lineY, 0x63FFFFFF, 0x66FFFFFF);
                lineY += 22;
                lineCount++;
                pos = nextPos + 1;
            }
            
            if (m.getMagicType() != 5) {
                int levelMultiplier = magicLevel / 100 + 1;
                std::string mpStr = " 內力 " + std::to_string(m.getNeedMP() * levelMultiplier);
                DrawShadowTextUtf8(mpStr, x + 50 + textOffsetX, lineY, 0x63FFFFFF, 0x66FFFFFF);
            } else {
                int gLevel = GameManager::getInstance().GetGongtiLevel(roleId, magicId);
                int attrY = y + 290;
                int attrIdx = 0;
                
                if (m.getAddHP(gLevel) != 0) {
                    std::string label = " 生命 ";
                    std::string value = std::to_string(m.getAddHP(gLevel));
                    DrawShadowTextUtf8(label, x + 248 + 118 * (attrIdx % 2), attrY + (attrIdx / 2) * 22, 0x05FFFFFF, 0x07FFFFFF);
                    DrawShadowTextUtf8(value, x + 298 + 118 * (attrIdx % 2), attrY + (attrIdx / 2) * 22, 0x05FFFFFF, 0x07FFFFFF);
                    attrIdx++;
                }
                if (m.getAddMP(gLevel) != 0) {
                    std::string label = " 內力 ";
                    std::string value = std::to_string(m.getAddMP(gLevel));
                    DrawShadowTextUtf8(label, x + 248 + 118 * (attrIdx % 2), attrY + (attrIdx / 2) * 22, 0x05FFFFFF, 0x07FFFFFF);
                    DrawShadowTextUtf8(value, x + 298 + 118 * (attrIdx % 2), attrY + (attrIdx / 2) * 22, 0x05FFFFFF, 0x07FFFFFF);
                    attrIdx++;
                }
                if (m.getAddAtt(gLevel) != 0) {
                    std::string label = " 攻擊 ";
                    std::string value = std::to_string(m.getAddAtt(gLevel));
                    DrawShadowTextUtf8(label, x + 248 + 118 * (attrIdx % 2), attrY + (attrIdx / 2) * 22, 0x05FFFFFF, 0x07FFFFFF);
                    DrawShadowTextUtf8(value, x + 298 + 118 * (attrIdx % 2), attrY + (attrIdx / 2) * 22, 0x05FFFFFF, 0x07FFFFFF);
                    attrIdx++;
                }
                if (m.getAddDef(gLevel) != 0) {
                    std::string label = " 防禦 ";
                    std::string value = std::to_string(m.getAddDef(gLevel));
                    DrawShadowTextUtf8(label, x + 248 + 118 * (attrIdx % 2), attrY + (attrIdx / 2) * 22, 0x05FFFFFF, 0x07FFFFFF);
                    DrawShadowTextUtf8(value, x + 298 + 118 * (attrIdx % 2), attrY + (attrIdx / 2) * 22, 0x05FFFFFF, 0x07FFFFFF);
                    attrIdx++;
                }
                if (m.getAddSpd(gLevel) != 0) {
                    std::string label = " 輕功 ";
                    std::string value = std::to_string(m.getAddSpd(gLevel));
                    DrawShadowTextUtf8(label, x + 248 + 118 * (attrIdx % 2), attrY + (attrIdx / 2) * 22, 0x05FFFFFF, 0x07FFFFFF);
                    DrawShadowTextUtf8(value, x + 298 + 118 * (attrIdx % 2), attrY + (attrIdx / 2) * 22, 0x05FFFFFF, 0x07FFFFFF);
                    attrIdx++;
                }
                
                if (gLevel == m.getMaxLevel()) {
                    if (m.getAddMedcine() != 0) {
                        std::string label = " 醫療 ";
                        std::string value = std::to_string(m.getAddMedcine());
                        DrawShadowTextUtf8(label, x + 248 + 118 * (attrIdx % 2), attrY + (attrIdx / 2) * 22, 0x05FFFFFF, 0x07FFFFFF);
                        DrawShadowTextUtf8(value, x + 298 + 118 * (attrIdx % 2), attrY + (attrIdx / 2) * 22, 0x05FFFFFF, 0x07FFFFFF);
                        attrIdx++;
                    }
                    if (m.getAddUsePoi() != 0) {
                        std::string label = " 用毒 ";
                        std::string value = std::to_string(m.getAddUsePoi());
                        DrawShadowTextUtf8(label, x + 248 + 118 * (attrIdx % 2), attrY + (attrIdx / 2) * 22, 0x05FFFFFF, 0x07FFFFFF);
                        DrawShadowTextUtf8(value, x + 298 + 118 * (attrIdx % 2), attrY + (attrIdx / 2) * 22, 0x05FFFFFF, 0x07FFFFFF);
                        attrIdx++;
                    }
                    if (m.getAddMedPoi() != 0) {
                        std::string label = " 解毒 ";
                        std::string value = std::to_string(m.getAddMedPoi());
                        DrawShadowTextUtf8(label, x + 248 + 118 * (attrIdx % 2), attrY + (attrIdx / 2) * 22, 0x05FFFFFF, 0x07FFFFFF);
                        DrawShadowTextUtf8(value, x + 298 + 118 * (attrIdx % 2), attrY + (attrIdx / 2) * 22, 0x05FFFFFF, 0x07FFFFFF);
                        attrIdx++;
                    }
                    if (m.getAddDefPoi() != 0) {
                        std::string label = " 抗毒 ";
                        std::string value = std::to_string(m.getAddDefPoi());
                        DrawShadowTextUtf8(label, x + 248 + 118 * (attrIdx % 2), attrY + (attrIdx / 2) * 22, 0x05FFFFFF, 0x07FFFFFF);
                        DrawShadowTextUtf8(value, x + 298 + 118 * (attrIdx % 2), attrY + (attrIdx / 2) * 22, 0x05FFFFFF, 0x07FFFFFF);
                        attrIdx++;
                    }
                    if (m.getAddFist() != 0) {
                        std::string label = " 拳掌 ";
                        std::string value = std::to_string(m.getAddFist());
                        DrawShadowTextUtf8(label, x + 248 + 118 * (attrIdx % 2), attrY + (attrIdx / 2) * 22, 0x05FFFFFF, 0x07FFFFFF);
                        DrawShadowTextUtf8(value, x + 298 + 118 * (attrIdx % 2), attrY + (attrIdx / 2) * 22, 0x05FFFFFF, 0x07FFFFFF);
                        attrIdx++;
                    }
                    if (m.getAddSword() != 0) {
                        std::string label = " 禦劍 ";
                        std::string value = std::to_string(m.getAddSword());
                        DrawShadowTextUtf8(label, x + 248 + 118 * (attrIdx % 2), attrY + (attrIdx / 2) * 22, 0x05FFFFFF, 0x07FFFFFF);
                        DrawShadowTextUtf8(value, x + 298 + 118 * (attrIdx % 2), attrY + (attrIdx / 2) * 22, 0x05FFFFFF, 0x07FFFFFF);
                        attrIdx++;
                    }
                    if (m.getAddKnife() != 0) {
                        std::string label = " 耍刀 ";
                        std::string value = std::to_string(m.getAddKnife());
                        DrawShadowTextUtf8(label, x + 248 + 118 * (attrIdx % 2), attrY + (attrIdx / 2) * 22, 0x05FFFFFF, 0x07FFFFFF);
                        DrawShadowTextUtf8(value, x + 298 + 118 * (attrIdx % 2), attrY + (attrIdx / 2) * 22, 0x05FFFFFF, 0x07FFFFFF);
                        attrIdx++;
                    }
                    if (m.getAddUnusual() != 0) {
                        std::string label = " 奇門 ";
                        std::string value = std::to_string(m.getAddUnusual());
                        DrawShadowTextUtf8(label, x + 248 + 118 * (attrIdx % 2), attrY + (attrIdx / 2) * 22, 0x05FFFFFF, 0x07FFFFFF);
                        DrawShadowTextUtf8(value, x + 298 + 118 * (attrIdx % 2), attrY + (attrIdx / 2) * 22, 0x05FFFFFF, 0x07FFFFFF);
                        attrIdx++;
                    }
                    if (m.getAddHidWeapon() != 0) {
                        std::string label = " 暗器 ";
                        std::string value = std::to_string(m.getAddHidWeapon());
                        DrawShadowTextUtf8(label, x + 248 + 118 * (attrIdx % 2), attrY + (attrIdx / 2) * 22, 0x05FFFFFF, 0x07FFFFFF);
                        DrawShadowTextUtf8(value, x + 298 + 118 * (attrIdx % 2), attrY + (attrIdx / 2) * 22, 0x05FFFFFF, 0x07FFFFFF);
                        attrIdx++;
                    }
                }
                if (m.getBattleState() > 0 && gLevel == m.getMaxLevel()) {
                    std::string stateName = GetBattleEffectDisplayName(m.getBattleState());
                    if (!stateName.empty()) {
                        DrawShadowTextUtf8(" " + stateName,
                            x + 248 + textOffsetX,
                            attrY + ((attrIdx + 1) / 2) * 22,
                            0x63FFFFFF, 0x66FFFFFF);
                    }
                }
            }
        }
    } else if (selectedIndex >= 0 && selectedIndex < 6) {
        std::string desc;
        switch (selectedIndex) {
            case 0: desc = " 給本方隊友治療，增*加其生命值，并減少*其受傷值。*耗費體力3"; break;
            case 1: desc = " 給本方隊友解毒，減*中毒值，但對中毒太*深者無法解毒。*耗費體力3"; break;
            case 2: desc = " 用毒使對方中毒，每*回合生命減少，並且*降低對方醫療效果。*耗費體力3"; break;
            case 3: desc = " 抗擊用毒的能力。"; break;
            case 4: desc = " 武學攻擊中帶有的毒*素傷害。"; break;
            default: desc = " "; break;
        }
        
        int lineY = y + 285 + 40;
        size_t pos = 0;
        while (pos < desc.length()) {
            size_t nextPos = desc.find('*', pos);
            if (nextPos == std::string::npos) nextPos = desc.length();
            std::string line = desc.substr(pos, nextPos - pos);
            DrawShadowTextUtf8(line, x + 35 + textOffsetX, lineY, 0x63FFFFFF, 0x66FFFFFF);
            lineY += 22;
            pos = nextPos + 1;
        }
    }
}

bool UIManager::InModeMagic(int roleId) {
    auto& game = GameManager::getInstance();
    Role& role = game.getRole(roleId);
    int max = 0;
    for (int i = 0; i < 10; ++i) {
        if (role.getMagic(i) > 0) max += 1;
    }
    int num = 0;
    bool running = true;
    SDL_Event event;
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                game.Quit();
                return true;
            }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_ESCAPE) {
                    return false;
                }
                if (event.key.key == SDLK_UP || event.key.key == SDLK_KP_8) {
                    if (num >= 0 && num <= 5) {
                        num -= 3;
                    } else if (num >= 6) {
                        num -= 2;
                    }
                }
                if (event.key.key == SDLK_DOWN || event.key.key == SDLK_KP_2) {
                    if (num >= 3 && num <= 5) {
                        if (max == 0) {
                            num -= 3;
                        } else {
                            num = 6;
                        }
                    } else if (num >= 0 && num <= 2) {
                        num += 3;
                    } else if (num >= 6) {
                        num += 2;
                    }
                    if (num < 0) {
                        if (max == 0) num += 3;
                        else num = (max / 2) * 2 + 5;
                    }
                }
                if (event.key.key == SDLK_RIGHT || event.key.key == SDLK_KP_6) {
                    num += 1;
                }
                if (event.key.key == SDLK_LEFT || event.key.key == SDLK_KP_4) {
                    num -= 1;
                }
                if (num < 0) {
                    if (max == 0) num += 3;
                    else num = max + 5;
                }
                if (num > max + 5) {
                    num = 0;
                }
                if (event.key.key == SDLK_RETURN || event.key.key == SDLK_KP_ENTER || event.key.key == SDLK_SPACE) {
                    if (num == 0) {
                        if (game.GetRoleMedcine(roleId, true) >= 20) {
                            MenuMedcine(roleId);
                        }
                    } else if (num == 1) {
                        if (game.GetRoleMedPoi(roleId, true) >= 20) {
                            MenuMedPoision(roleId);
                        }
                    } else if (num > 5) {
                        int magicIndex = num - 6;
                        if (magicIndex >= 0 && magicIndex < 10) {
                            int magicId = role.getMagic(magicIndex);
                            if (magicId > 0) {
                                Magic& magic = game.getMagic(magicId);
                                if (magic.getMagicType() == 5) {
                                    game.SetGongti(roleId, magicId);
                                }
                            }
                        }
                    }
                }
            }
        }
        SDL_RenderClear(m_renderer);
        ShowMagic(roleId, num);
        VirtualControls::present(m_renderer);
        SDL_Delay(16);
    }
    return false;
}

void UIManager::ShowMedcine(int healerId, int menu) {
    SDL_RenderClear(m_renderer);
    ShowMagic(healerId, 0);
    const auto& team = GameManager::getInstance().getTeamList();
    std::vector<int> valid;
    valid.reserve(team.size());
    for (int roleId : team) {
        if (roleId >= 0) valid.push_back(roleId);
    }
    int x = 338;
    int y = 58;
    int textOffsetX = 8;
    DrawRectangle(x - 8, y - 18, 300, ((int)valid.size() + 1) * 22 + 26, 0x000000CC, 0xFFFFFFFF, 200);
    std::string title = " ——選擇隊友——";
    DrawShadowTextUtf8(title, 337 + textOffsetX, 36, 0x21FFFFFF, 0x23FFFFFF);
    for (int i = 0; i < (int)valid.size(); ++i) {
        int roleId = valid[i];
        Role& r = GameManager::getInstance().getRole(roleId);
        std::string nameUtf8 = TextManager::getInstance().gbkToUtf8(r.getName());
        std::string hp = std::to_string(r.getCurrentHP()) + "/" + std::to_string(r.getMaxHP());
        bool selected = (i == menu);
        uint32_t color = selected ? 0x63FFFFFF : 0x05FFFFFF;
        uint32_t shadow = selected ? 0x66FFFFFF : 0x07FFFFFF;
        DrawShadowTextUtf8(nameUtf8, x + textOffsetX, y + 22 * i, color, shadow);
        DrawShadowTextUtf8(hp, x + 90 + textOffsetX, y + 22 * i, color, shadow);
    }
}

void UIManager::ShowMedPoision(int healerId, int menu) {
    SDL_RenderClear(m_renderer);
    ShowMagic(healerId, 1);
    const auto& team = GameManager::getInstance().getTeamList();
    std::vector<int> valid;
    valid.reserve(team.size());
    for (int roleId : team) {
        if (roleId >= 0) valid.push_back(roleId);
    }
    int x = 338;
    int y = 58;
    int textOffsetX = 8;
    DrawRectangle(x - 8, y - 18, 300, ((int)valid.size() + 1) * 22 + 26, 0x000000CC, 0xFFFFFFFF, 200);
    std::string title = " ——選擇隊友——";
    DrawShadowTextUtf8(title, 337 + textOffsetX, 36, 0x21FFFFFF, 0x23FFFFFF);
    for (int i = 0; i < (int)valid.size(); ++i) {
        int roleId = valid[i];
        Role& r = GameManager::getInstance().getRole(roleId);
        std::string nameUtf8 = TextManager::getInstance().gbkToUtf8(r.getName());
        std::string poi = std::to_string(r.getPoision());
        bool selected = (i == menu);
        uint32_t color = selected ? 0x63FFFFFF : 0x05FFFFFF;
        uint32_t shadow = selected ? 0x66FFFFFF : 0x07FFFFFF;
        DrawShadowTextUtf8(nameUtf8, x + textOffsetX, y + 22 * i, color, shadow);
        DrawShadowTextUtf8(poi, x + 90 + textOffsetX, y + 22 * i, color, shadow);
    }
}

void UIManager::MenuMedcine(int healerId) {
    auto& game = GameManager::getInstance();
    const auto& team = game.getTeamList();
    std::vector<int> valid;
    valid.reserve(team.size());
    for (int roleId : team) {
        if (roleId >= 0) valid.push_back(roleId);
    }
    if (valid.empty()) return;
    int menu = 0;
    int max = (int)valid.size();
    bool running = true;
    SDL_Event event;
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                game.Quit();
                return;
            }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_UP || event.key.key == SDLK_KP_8) {
                    menu -= 1;
                }
                if (event.key.key == SDLK_DOWN || event.key.key == SDLK_KP_2) {
                    menu += 1;
                }
                if (menu < 0) menu = max - 1;
                if (menu >= max) menu = 0;
                if (event.key.key == SDLK_ESCAPE) {
                    return;
                }
                if (event.key.key == SDLK_RETURN || event.key.key == SDLK_KP_ENTER || event.key.key == SDLK_SPACE) {
                    if (menu >= 0 && menu < max) {
                        int targetId = valid[menu];
                        Role& healer = game.getRole(healerId);
                        Role& target = game.getRole(targetId);
                        if (healer.getPhyPower() >= 50) {
                            int med = game.GetRoleMedcine(healerId, true);
                            int addlife = med * (10 - target.getHurt() / 15) / 10;
                            if (target.getHurt() - med > 20) addlife = 0;
                            if (game.CheckBattleEffect(healerId, BattleEffectType::Boost_Med_Detox)) {
                                addlife = addlife * 3 / 2;
                            }
                            int cureHurt = addlife / LIFE_HURT;
                            target.setHurt(std::max(0, target.getHurt() - cureHurt));
                            int maxHeal = target.getMaxHP() - target.getCurrentHP();
                            if (addlife > maxHeal) addlife = maxHeal;
                            target.setCurrentHP(target.getCurrentHP() + addlife);
                            if (addlife > 0) {
                                if (!game.GetEquipState(healerId, 1) && !game.GetGongtiState(healerId, 1)) {
                                    healer.setPhyPower(std::max(0, healer.getPhyPower() - 3));
                                }
                            }
                        }
                    }
                }
            }
        }
        ShowMedcine(healerId, menu);
        VirtualControls::present(m_renderer);
        SDL_Delay(16);
    }
}

void UIManager::MenuMedPoision(int healerId) {
    auto& game = GameManager::getInstance();
    const auto& team = game.getTeamList();
    std::vector<int> valid;
    valid.reserve(team.size());
    for (int roleId : team) {
        if (roleId >= 0) valid.push_back(roleId);
    }
    if (valid.empty()) return;
    int menu = 0;
    int max = (int)valid.size();
    bool running = true;
    SDL_Event event;
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                game.Quit();
                return;
            }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_UP || event.key.key == SDLK_KP_8) {
                    menu -= 1;
                }
                if (event.key.key == SDLK_DOWN || event.key.key == SDLK_KP_2) {
                    menu += 1;
                }
                if (menu < 0) menu = max - 1;
                if (menu >= max) menu = 0;
                if (event.key.key == SDLK_ESCAPE) {
                    return;
                }
                if (event.key.key == SDLK_RETURN || event.key.key == SDLK_KP_ENTER || event.key.key == SDLK_SPACE) {
                    if (menu >= 0 && menu < max) {
                        int targetId = valid[menu];
                        Role& healer = game.getRole(healerId);
                        Role& target = game.getRole(targetId);
                        if (healer.getPhyPower() >= 50) {
                            int minuspoi = game.GetRoleMedPoi(healerId, true);
                            int currentPoi = target.getPoision();
                            if (minuspoi < currentPoi / 2) minuspoi = 0;
                            else if (minuspoi > currentPoi) minuspoi = currentPoi;
                            if (game.CheckBattleEffect(healerId, BattleEffectType::Boost_Med_Detox)) {
                                minuspoi = minuspoi * 3 / 2;
                                minuspoi = std::min(minuspoi, currentPoi);
                            }
                            target.setPoision(currentPoi - minuspoi);
                            if (minuspoi > 0) {
                                if (!game.GetEquipState(healerId, 1) && !game.GetGongtiState(healerId, 1)) {
                                    healer.setPhyPower(std::max(0, healer.getPhyPower() - 3));
                                }
                            }
                        }
                    }
                }
            }
        }
        ShowMedPoision(healerId, menu);
        VirtualControls::present(m_renderer);
        SDL_Delay(16);
    }
}

int UIManager::SelectItemUser(int menuSelection, int selectedIndex, int itemId, int itemType) {
    auto& game = GameManager::getInstance();
    const auto& team = game.getTeamList();
    std::vector<int> valid;
    valid.reserve(team.size());
    for (int roleId : team) {
        if (roleId >= 0) valid.push_back(roleId);
    }
    if (valid.empty()) return -1;
    Item& item = game.getItem(itemId);
    auto meetNeed = [](int need, int stat) {
        if (need == 0) return true;
        if (need > 0) return stat >= need;
        return stat <= -need;
    };
    auto canEquip = [&](int rnum) {
        Role& r = game.getRole(rnum);
        if (item.getNeedSex() >= 0 && item.getNeedSex() != r.getSexual()) return std::make_pair(false, std::string("性别限制"));
        if (!meetNeed(item.getNeedMP(), r.getCurrentMP())) return std::make_pair(false, std::string(item.getNeedMP() > 0 ? "内力不足" : "内力过高"));
        if (!meetNeed(item.getNeedAttack(), game.GetRoleAttack(rnum, true))) return std::make_pair(false, std::string(item.getNeedAttack() > 0 ? "攻击不足" : "攻击过高"));
        if (!meetNeed(item.getNeedSpeed(), game.GetRoleSpeed(rnum, true))) return std::make_pair(false, std::string(item.getNeedSpeed() > 0 ? "轻功不足" : "轻功过高"));
        if (!meetNeed(item.getNeedUsePoi(), game.GetRoleUsePoi(rnum, true))) return std::make_pair(false, std::string(item.getNeedUsePoi() > 0 ? "用毒不足" : "用毒过高"));
        if (!meetNeed(item.getNeedMedcine(), game.GetRoleMedcine(rnum, true))) return std::make_pair(false, std::string(item.getNeedMedcine() > 0 ? "医疗不足" : "医疗过高"));
        if (!meetNeed(item.getNeedMedPoi(), game.GetRoleMedPoi(rnum, true))) return std::make_pair(false, std::string(item.getNeedMedPoi() > 0 ? "解毒不足" : "解毒过高"));
        if (!meetNeed(item.getNeedFist(), game.GetRoleFist(rnum, true))) return std::make_pair(false, std::string(item.getNeedFist() > 0 ? "拳掌不足" : "拳掌过高"));
        if (!meetNeed(item.getNeedSword(), game.GetRoleSword(rnum, true))) return std::make_pair(false, std::string(item.getNeedSword() > 0 ? "御剑不足" : "御剑过高"));
        if (!meetNeed(item.getNeedKnife(), game.GetRoleKnife(rnum, true))) return std::make_pair(false, std::string(item.getNeedKnife() > 0 ? "耍刀不足" : "耍刀过高"));
        if (!meetNeed(item.getNeedUnusual(), game.GetRoleUnusual(rnum, true))) return std::make_pair(false, std::string(item.getNeedUnusual() > 0 ? "奇门不足" : "奇门过高"));
        if (!meetNeed(item.getNeedHidWeapon(), game.GetRoleHidWeapon(rnum, true))) return std::make_pair(false, std::string(item.getNeedHidWeapon() > 0 ? "暗器不足" : "暗器过高"));
        int aptitude = r.getAptitude();
        if (game.CheckEquipSet(r.getEquip(0), r.getEquip(1), r.getEquip(2), r.getEquip(3)) == 2) aptitude = 100;
        if (!meetNeed(item.getNeedAptitude(), aptitude)) return std::make_pair(false, std::string(item.getNeedAptitude() > 0 ? "资质不足" : "资质过高"));
        if (r.getMPType() < 2 && item.getNeedMPType() < 2 && r.getMPType() != item.getNeedMPType()) return std::make_pair(false, std::string("内功类型不符"));
        if (item.getOnlyPracRole() >= 0 && item.getOnlyPracRole() != rnum) return std::make_pair(false, std::string("限定角色"));
        return std::make_pair(true, std::string("可装备"));
    };
    auto canPractice = [&](int rnum) {
        Role& r = game.getRole(rnum);
        if (item.getOnlyPracRole() >= 0 && item.getOnlyPracRole() != rnum) return std::make_pair(false, std::string("限定角色"));
        int aptitude = r.getAptitude();
        if (game.CheckEquipSet(r.getEquip(0), r.getEquip(1), r.getEquip(2), r.getEquip(3)) == 2) aptitude = 100;
        if (!meetNeed(item.getNeedAptitude(), aptitude)) return std::make_pair(false, std::string(item.getNeedAptitude() > 0 ? "资质不足" : "资质过高"));
        int magicId = item.getMagic();
        if (magicId > 0) {
            int learnedCount = 0;
            bool already = false;
            for (int i = 0; i < 10; ++i) {
                if (r.getMagic(i) > 0) learnedCount++;
                if (r.getMagic(i) == magicId) already = true;
            }
            if (learnedCount >= 10 && !already) return std::make_pair(false, std::string("武学已满"));
            Magic& mg = game.getMagic(magicId);
            int level = 0;
            for (int i = 0; i < 10; ++i) {
                if (r.getMagic(i) == magicId) {
                    level = r.getMagLevel(i);
                    break;
                }
            }
            if (mg.getMagicType() == 5 && already) return std::make_pair(false, std::string("内功已学"));
            if (level >= 900) return std::make_pair(false, std::string("修为已满"));
        }
        return std::make_pair(true, std::string("可修炼"));
    };
    int menu = 0;
    int max = (int)valid.size();
    SDL_Event event;
    while (true) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                game.Quit();
                return -1;
            }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_UP || event.key.key == SDLK_KP_8) {
                    menu -= 1;
                }
                if (event.key.key == SDLK_DOWN || event.key.key == SDLK_KP_2) {
                    menu += 1;
                }
                if (menu < 0) menu = max - 1;
                if (menu >= max) menu = 0;
                if (event.key.key == SDLK_ESCAPE) {
                    return -1;
                }
                if (event.key.key == SDLK_RETURN || event.key.key == SDLK_KP_ENTER || event.key.key == SDLK_SPACE) {
                    if (menu >= 0 && menu < max) {
                        return valid[menu];
                    }
                }
            }
        }
        SDL_RenderClear(m_renderer);
        if (m_texMenuBackground) {
            SDL_RenderTexture(m_renderer, m_texMenuBackground, NULL, NULL);
        }
        ShowItem(menuSelection, selectedIndex, true);
        int x = 338;
        int y = 58;
        int textOffsetX = 8;
        int winW = 0;
        int winH = 0;
        SDL_GetWindowSize(m_window, &winW, &winH);
        DrawRectangle(x - 8, y - 18, 300, (max + 1) * 22 + 26, 0x000000CC, 0xFFFFFFFF, 200);
        std::string title = " ——選擇隊友——";
        DrawShadowTextUtf8(title, 337 + textOffsetX, 36, 0x21FFFFFF, 0x23FFFFFF);
        for (int i = 0; i < max; ++i) {
            int roleId = valid[i];
            Role& r = game.getRole(roleId);
            std::string nameUtf8 = TextManager::getInstance().gbkToUtf8(r.getName());
            std::string hp = std::to_string(r.getCurrentHP()) + "/" + std::to_string(r.getMaxHP());
            bool selected = (i == menu);
            uint32_t color = selected ? 0x63FFFFFF : 0x05FFFFFF;
            uint32_t shadow = selected ? 0x66FFFFFF : 0x07FFFFFF;
            DrawShadowTextUtf8(nameUtf8, x + textOffsetX, y + 22 * i, color, shadow);
            DrawShadowTextUtf8(hp, x + 90 + textOffsetX, y + 22 * i, color, shadow);
        }
        if (menu >= 0 && menu < max && (itemType == 1 || itemType == 2)) {
            int roleId = valid[menu];
            std::pair<bool, std::string> res = (itemType == 1) ? canEquip(roleId) : canPractice(roleId);
            std::string hint = res.second.empty()
                ? (res.first ? (itemType == 1 ? "可装备" : "可修炼") : (itemType == 1 ? "不可装备" : "不可修炼"))
                : (res.first ? res.second : ((itemType == 1 ? "不可装备:" : "不可修炼:") + res.second));
            SDL_Color main = res.first ? SDL_Color{ 0, 255, 0, 255 } : SDL_Color{ 255, 64, 64, 255 };
            SDL_Color shadowC = { 0, 0, 0, 255 };
            int boxX = x - 8;
            int boxY = y + max * 22 + 12;
            int boxW = 300;
            int boxH = 60;
            DrawRectangle(boxX, boxY, boxW, boxH, 0x000000CC, 0xFFFFFFFF, 200);
            TTF_Font* useFont = (m_fontEnglish && IsAsciiOnly(hint)) ? m_fontEnglish : m_font;
            if (useFont) {
                int wrapWidth = boxW - 20;
                SDL_Surface* s2 = TTF_RenderText_Blended_Wrapped(useFont, hint.c_str(), 0, shadowC, wrapWidth);
                if (s2) {
                    SDL_Texture* t2 = SDL_CreateTextureFromSurface(m_renderer, s2);
                    SDL_FRect d2 = { (float)(boxX + 11), (float)(boxY + 9), (float)s2->w, (float)s2->h };
                    SDL_RenderTexture(m_renderer, t2, NULL, &d2);
                    SDL_DestroyTexture(t2);
                    SDL_DestroySurface(s2);
                }
                SDL_Surface* s1 = TTF_RenderText_Blended_Wrapped(useFont, hint.c_str(), 0, main, wrapWidth);
                if (s1) {
                    SDL_Texture* t1 = SDL_CreateTextureFromSurface(m_renderer, s1);
                    SDL_FRect d1 = { (float)(boxX + 10), (float)(boxY + 8), (float)s1->w, (float)s1->h };
                    SDL_RenderTexture(m_renderer, t1, NULL, &d1);
                    SDL_DestroyTexture(t1);
                    SDL_DestroySurface(s1);
                }
            }
        }
        VirtualControls::present(m_renderer);
        SDL_Delay(16);
    }
}

void UIManager::SelectShowSystem() {
    bool running = true;
    int currentSelection = 0;
    int subMenu = -1;
    int subSelection = 0;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                GameManager::getInstance().Quit();
                return;
            }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_ESCAPE) {
                    if (subMenu >= 0) {
                        subMenu = -1;
                    } else {
                        running = false;
                    }
                } else if (subMenu < 0) {
                    if (event.key.key == SDLK_DOWN || event.key.key == SDLK_KP_2) {
                        currentSelection = (currentSelection + 1) % 4;
                    } else if (event.key.key == SDLK_UP || event.key.key == SDLK_KP_8) {
                        currentSelection = (currentSelection + 3) % 4;
                    } else if (event.key.key == SDLK_RETURN || event.key.key == SDLK_SPACE) {
                        subMenu = currentSelection;
                        if (subMenu == 0 || subMenu == 1) {
                            subSelection = 0;
                        } else if (subMenu == 2) {
                            subSelection = SoundManager::getInstance().GetMusicVolumeLevel();
                        } else if (subMenu == 3) {
                            subSelection = -1;
                        }
                    }
                } else {
                    if (subMenu == 0) {
                        int maxSlot = 6;
                        if (event.key.key == SDLK_RIGHT || event.key.key == SDLK_KP_6) {
                            subSelection = (subSelection + 1) % maxSlot;
                        } else if (event.key.key == SDLK_LEFT || event.key.key == SDLK_KP_4) {
                            subSelection = (subSelection + maxSlot - 1) % maxSlot;
                        } else if (event.key.key == SDLK_RETURN || event.key.key == SDLK_SPACE) {
                            int slot = (subSelection == 5) ? 6 : (subSelection + 1);
                            bool loaded = GameManager::getInstance().LoadGame(slot);
                            if (loaded) {
                                running = false;
                            } else {
                                ShowDialogue("讀取失敗", 0, 0);
                            }
                        }
                    } else if (subMenu == 1) {
                        int maxSlot = 5;
                        if (event.key.key == SDLK_RIGHT || event.key.key == SDLK_KP_6) {
                            subSelection = (subSelection + 1) % maxSlot;
                        } else if (event.key.key == SDLK_LEFT || event.key.key == SDLK_KP_4) {
                            subSelection = (subSelection + maxSlot - 1) % maxSlot;
                        } else if (event.key.key == SDLK_RETURN || event.key.key == SDLK_SPACE) {
                            int slot = subSelection + 1;
                            GameManager::getInstance().SaveGame(slot);
                            ShowDialogue("進度已保存", 0, 0);
                        }
                    } else if (subMenu == 2) {
                        int maxSlot = 9;
                        if (event.key.key == SDLK_RIGHT || event.key.key == SDLK_KP_6) {
                            subSelection = (subSelection + 1) % maxSlot;
                        } else if (event.key.key == SDLK_LEFT || event.key.key == SDLK_KP_4) {
                            subSelection = (subSelection + maxSlot - 1) % maxSlot;
                        } else if (event.key.key == SDLK_RETURN || event.key.key == SDLK_SPACE) {
                            SoundManager::getInstance().SetMusicVolumeLevel(subSelection);
                        }
                    } else if (subMenu == 3) {
                        int maxSlot = 2;
                        if (event.key.key == SDLK_RIGHT || event.key.key == SDLK_KP_6) {
                            subSelection = (subSelection + 1) % maxSlot;
                        } else if (event.key.key == SDLK_LEFT || event.key.key == SDLK_KP_4) {
                            subSelection = (subSelection + maxSlot - 1) % maxSlot;
                        } else if (event.key.key == SDLK_RETURN || event.key.key == SDLK_SPACE) {
                            if (subSelection == 1) {
                                GameManager::getInstance().Quit();
                                return;
                            } else {
                                subMenu = -1;
                            }
                        }
                    }
                }
            }
            if (event.type == SDL_EVENT_MOUSE_MOTION) {
                float xm = 0.0f, ym = 0.0f;
                SDL_GetMouseState(&xm, &ym);
                if (subMenu >= 0) {
                    int width = 56;
                    if (subMenu == 0) width = 81;
                    else if (subMenu == 1) width = 97;
                    int startX = 118;
                    int startY = 50 + 101 * subMenu;
                    if (ym >= startY && ym < startY + 25) {
                        int idx = (int)((xm - startX) / width);
                        int maxSlot = 9;
                        if (subMenu == 0) maxSlot = 6;
                        else if (subMenu == 1) maxSlot = 5;
                        else if (subMenu == 3) maxSlot = 2;
                        if (idx >= 0 && idx < maxSlot) {
                            subSelection = idx;
                        }
                    }
                } else {
                    for (int i = 0; i < 4; ++i) {
                        int startY = 25 + 101 * i;
                        if (ym >= startY && ym < startY + 25 && xm >= 112 && xm < 500) {
                            currentSelection = i;
                            break;
                        }
                    }
                }
            }
            if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
                float xm = 0.0f, ym = 0.0f;
                SDL_GetMouseState(&xm, &ym);
                if (event.button.button == SDL_BUTTON_RIGHT) {
                    if (subMenu >= 0) {
                        subMenu = -1;
                    } else {
                        running = false;
                    }
                } else if (event.button.button == SDL_BUTTON_LEFT) {
                    if (subMenu >= 0) {
                        int width = 56;
                        if (subMenu == 0) width = 81;
                        else if (subMenu == 1) width = 97;
                        int startX = 118;
                        int startY = 50 + 101 * subMenu;
                        if (ym >= startY && ym < startY + 25 && xm >= startX) {
                            if (subMenu == 0) {
                                int slot = (subSelection == 5) ? 6 : (subSelection + 1);
                                bool loaded = GameManager::getInstance().LoadGame(slot);
                                if (loaded) {
                                    running = false;
                                }
                            } else if (subMenu == 1) {
                                int slot = subSelection + 1;
                                GameManager::getInstance().SaveGame(slot);
                                ShowDialogue("進度已保存", 0, 0);
                            } else if (subMenu == 2) {
                                SoundManager::getInstance().SetMusicVolumeLevel(subSelection);
                            } else if (subMenu == 3) {
                                if (subSelection == 1) {
                                    GameManager::getInstance().Quit();
                                    return;
                                } else {
                                    subMenu = -1;
                                }
                            }
                        }
                    } else {
                        for (int i = 0; i < 4; ++i) {
                            int startY = 25 + 101 * i;
                            if (ym >= startY && ym < startY + 25 && xm >= 112 && xm < 500) {
                                subMenu = i;
                                if (subMenu == 0 || subMenu == 1) {
                                    subSelection = 0;
                                } else if (subMenu == 2) {
                                    subSelection = SoundManager::getInstance().GetMusicVolumeLevel();
                                } else if (subMenu == 3) {
                                    subSelection = -1;
                                }
                                break;
                            }
                        }
                    }
                }
            }
        }

        SDL_RenderClear(m_renderer);
        if (m_texMenuBackground) {
            SDL_RenderTexture(m_renderer, m_texMenuBackground, NULL, NULL);
        }
        ShowSystem(currentSelection, subMenu, subSelection);
        VirtualControls::present(m_renderer);
        SDL_Delay(16);
    }
}

void UIManager::ShowSystem(int selectedIndex, int subMenu, int subSelection) {
    if (!m_texSystem) LoadSystemGraphics();
    if (m_texSystem) {
        SDL_RenderTexture(m_renderer, m_texSystem, NULL, NULL);
    }

    const char* labels[] = {
        " ——————————讀取進度——————————",
        " ——————————保存進度——————————",
        " ——————————音樂音量——————————",
        " ——————————退出離開——————————"
    };

    for (int i = 0; i < 4; ++i) {
        uint32_t color = (i == selectedIndex) ? 0xFFFFFFFF : 0xAAAAAAFF;
        DrawShadowTextUtf8(labels[i], 112, 25 + 110 * i, color, 0x000000FF);
    }

    if (subMenu >= 0) {
        if (subMenu == 0) {
            std::vector<std::string> words = {
                " 進度一", " 進度二", " 進度三", " 進度四", " 進度五", " 自動檔"
            };
            ShowSelect(subMenu, subSelection, words, 81);
        } else if (subMenu == 1) {
            std::vector<std::string> words = {
                " 進度一", " 進度二", " 進度三", " 進度四", " 進度五"
            };
            ShowSelect(subMenu, subSelection, words, 97);
        } else if (subMenu == 2) {
            std::vector<std::string> words = {
                " 零", " 一", " 二", " 三", " 四", " 五", " 六", " 七", " 八"
            };
            ShowSelect(subMenu, subSelection, words, 56);
        } else if (subMenu == 3) {
            std::vector<std::string> words = {
                " 取消", " 退出"
            };
            ShowSelect(subMenu, subSelection, words, 56);
        }
    }
}

void UIManager::ShowSelect(int row, int menu, const std::vector<std::string>& words, int width) {
    for (size_t i = 0; i < words.size(); ++i) {
        int x = 118 + width * (int)i;
        int y = 50 + 110 * row;
        if ((int)i == menu) {
            DrawShadowTextUtf8(words[i], x + 1, y, 0x64FFFFFF, 0x66FFFFFF);
        } else {
            DrawShadowTextUtf8(words[i], x + 1, y, 0x05FFFFFF, 0x07FFFFFF);
        }
    }
}

void UIManager::ShowVolumeMenu() {
    bool running = true;
    int volumeSelection = SoundManager::getInstance().GetMusicVolumeLevel();
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                GameManager::getInstance().Quit();
                return;
            }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_LEFT || event.key.key == SDLK_KP_4) {
                    volumeSelection--;
                    if (volumeSelection < 0) volumeSelection = 8;
                } else if (event.key.key == SDLK_RIGHT || event.key.key == SDLK_KP_6) {
                    volumeSelection++;
                    if (volumeSelection > 8) volumeSelection = 0;
                } else if (event.key.key == SDLK_ESCAPE) {
                    running = false;
                } else if (event.key.key == SDLK_RETURN || event.key.key == SDLK_SPACE) {
                    SoundManager::getInstance().SetMusicVolumeLevel(volumeSelection);
                }
            }
        }

        SDL_RenderClear(m_renderer);
        if (m_texMenuBackground) {
            SDL_RenderTexture(m_renderer, m_texMenuBackground, NULL, NULL);
        }

        DrawRectangle(150, 50, 340, 300, 0, 0xFFFFFFFF, 100);
        DrawShadowTextUtf8("音樂音量", 280, 60, 0xFFFFFFFF, 0x000000FF);

        const char* labels[] = { "零", "一", "二", "三", "四", "五", "六", "七", "八" };
        for (int i = 0; i < 9; ++i) {
            uint32_t color = (i == volumeSelection) ? 0xFFFF00FF : 0xFFFFFFFF;
            DrawShadowTextUtf8(labels[i], 160 + i * 45, 160, color, 0x000000FF);
        }

        VirtualControls::present(m_renderer);
        SDL_Delay(16);
    }
}

void UIManager::SelectShowSkill() {
    GameManager::getInstance().RenderScreenTo(m_renderer);
    CaptureScreen();
    bool running = true;
    int currentPet = 0;
    int currentIdx = 0;
    SDL_Event event;

    while (running) {
        int petCount = GameManager::getInstance().getRole(0).getPetAmount();
        if (petCount < 0) petCount = 0;
        if (petCount > 5) petCount = 5;
        if (petCount == 0) {
            currentPet = 0;
        } else {
            if (currentPet < 0) currentPet = petCount - 1;
            if (currentPet >= petCount) currentPet = 0;
        }
        if (currentIdx < 0) currentIdx = 4;
        if (currentIdx > 4) currentIdx = 0;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                GameManager::getInstance().Quit();
                return;
            }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_ESCAPE) {
                    running = false;
                }
                if (event.key.key == SDLK_LEFT || event.key.key == SDLK_KP_4) {
                    currentIdx -= 1;
                } else if (event.key.key == SDLK_RIGHT || event.key.key == SDLK_KP_6) {
                    currentIdx += 1;
                }
                if (event.key.key == SDLK_DOWN || event.key.key == SDLK_KP_2) {
                    currentIdx++;
                    if (petCount > 0) currentPet = (currentPet + 1) % petCount;
                } else if (event.key.key == SDLK_UP || event.key.key == SDLK_KP_8) {
                    currentIdx--;
                    if (petCount > 0) currentPet = (currentPet - 1 + petCount) % petCount;
                } else if (event.key.key == SDLK_RETURN || event.key.key == SDLK_SPACE) {
                    if (petCount > 0) {
                        int roleId = currentPet + 1;
                        Role& pet = GameManager::getInstance().getRole(roleId);
                        int remaining = GetRemainingSkillPoints();
                        if (pet.getMagic(currentIdx) == 0) {
                            bool canLearn = (currentIdx == 0 || pet.getMagic(currentIdx - 1) > 0) && remaining >= (currentIdx + 1);
                            if (canLearn && ShowChoice("學習此技能？") == 1) {
                                pet.setMagic(currentIdx, 1);
                            }
                        }
                    }
                }
            }
            if (event.type == SDL_EVENT_MOUSE_MOTION) {
                float xm = 0.0f;
                float ym = 0.0f;
                SDL_GetMouseState(&xm, &ym);
                if (petCount > 0 && xm >= 10.0f && xm < 90.0f && ym >= 20.0f && ym < (petCount * 23.0f + 20.0f)) {
                    int idx = (int)((ym - 20.0f) / 23.0f);
                    if (idx < 0) idx = 0;
                    if (idx >= petCount) idx = petCount - 1;
                    currentPet = idx;
                }
                if (xm >= 140.0f && xm < 140.0f + 50.0f * 5.0f && ym >= 120.0f && ym < 170.0f) {
                    int idx = (int)((xm - 140.0f) / 50.0f);
                    if (idx < 0) idx = 0;
                    if (idx > 4) idx = 4;
                    currentIdx = idx;
                }
            }
            if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
                float xm = 0.0f;
                float ym = 0.0f;
                SDL_GetMouseState(&xm, &ym);
                if (event.button.button == SDL_BUTTON_RIGHT) {
                    running = false;
                } else if (event.button.button == SDL_BUTTON_LEFT) {
                    if (petCount > 0 && xm >= 10.0f && xm < 90.0f && ym >= 20.0f && ym < (petCount * 23.0f + 20.0f)) {
                        int idx = (int)((ym - 20.0f) / 23.0f);
                        if (idx < 0) idx = 0;
                        if (idx >= petCount) idx = petCount - 1;
                        currentPet = idx;
                    }
                    if (petCount > 0 && xm >= 140.0f && xm < 140.0f + 50.0f * 5.0f && ym >= 120.0f && ym < 170.0f) {
                        int idx = (int)((xm - 140.0f) / 50.0f);
                        if (idx < 0) idx = 0;
                        if (idx > 4) idx = 4;
                        currentIdx = idx;
                        int roleId = currentPet + 1;
                        Role& pet = GameManager::getInstance().getRole(roleId);
                        int remaining = GetRemainingSkillPoints();
                        if (pet.getMagic(currentIdx) == 0) {
                            bool canLearn = (currentIdx == 0 || pet.getMagic(currentIdx - 1) > 0) && remaining >= (currentIdx + 1);
                            if (canLearn && ShowChoice("學習此技能？") == 1) {
                                pet.setMagic(currentIdx, 1);
                            }
                        }
                    }
                }
            }
        }

        SDL_RenderClear(m_renderer);
        if (m_texMenuBackground) {
            SDL_RenderTexture(m_renderer, m_texMenuBackground, NULL, NULL);
        }
        ShowSkill(currentPet, currentIdx);
        VirtualControls::present(m_renderer);
        SDL_Delay(16);
    }
}

void UIManager::ShowSkill(int petId, int selectedIndex) {
    if (!m_texSkill) LoadSystemGraphics();
    if (m_texSkill) {
        SDL_RenderTexture(m_renderer, m_texSkill, NULL, NULL);
    }
    EnsureSkillIconsLoaded();
    Role& hero = GameManager::getInstance().getRole(0);
    int petCount = hero.getPetAmount();
    if (petCount < 0) petCount = 0;
    if (petCount > 5) petCount = 5;
    int textOffsetX = 8;
    if (petCount == 0) {
        DrawShadowTextUtf8(" ————目前尚無寵物————", 120 + textOffsetX, 50, 0xFFFFFFFF, 0x000000FF);
        return;
    }
    if (petId < 0) petId = 0;
    if (petId >= petCount) petId = petCount - 1;
    if (selectedIndex < 0) selectedIndex = 0;
    if (selectedIndex > 4) selectedIndex = 4;
    int roleId = petId + 1;
    Role& pet = GameManager::getInstance().getRole(roleId);
    std::string heroName = TextManager::getInstance().gbkToUtf8(hero.getName());

    std::vector<std::vector<std::string>> words(6, std::vector<std::string>(5));
    words[1][0] = " 修武： 30％幾率在戰鬥後把對手武功整理出秘笈。";
    words[1][1] = " 伴讀：" + heroName + "戰鬥經驗增加。";
    words[1][2] = " 通武： 60％幾率在戰鬥後把對手武功整理出秘笈。";
    words[1][3] = " 鑽研： 我方全員戰鬥經驗增加。";
    words[1][4] = " 精武： 100％幾率把戰鬥後把對手武功整理出秘笈。";

    words[2][0] = " 斂財： 戰鬥後增加銀兩收入。";
    words[2][1] = " 話術： 從居民口中打探劇情線索。";
    words[2][2] = " 神偷： 戰鬥後偷得對手隨身物品，裝備。";
    words[2][3] = " 劃價： 城市交易打折扣。";
    words[2][4] = " 通靈： 商店能購買隱藏寶物。";

    words[3][0] = " 收集： 收集藥材與普通食材。";
    words[3][1] = " 釀酒： 在酒窖耗費金錢與普通食材釀制各種酒。";
    words[3][2] = " 食神： 收集珍贵材料。";
    words[3][3] = " 煎藥： 在藥爐耗費金錢與藥材製造回復體內，解毒之丹藥。";
    words[3][4] = " 神丹： 在藥爐耗費金錢，特殊藥材煉製改變體質之丹藥，可以隨時改變自身體質練功。";

    words[4][0] = " 搜刮： 收集硝石和普通礦石。";
    words[4][1] = " 淬毒： 在煉鐵爐耗費金錢、普通礦石、藥材製造帶毒暗器。";
    words[4][2] = " 機關： 機關難度降低。";
    words[4][3] = " 鑄師： 在煉鐵爐將防具升級為寶甲。";
    words[4][4] = " 神兵： 在煉鐵爐將兵器升級為神兵。";

    words[5][0] = " 刺探： 戰鬥中可觀看敵人完整狀態。";
    words[5][1] = " 鼓舞：" + heroName + "戰鬥中首先行動。";
    words[5][2] = " 博愛： 醫療解毒可作用到附近三格内隊友。";
    words[5][3] = " 激勵： 戰鬥中我方成員首先移動。";
    words[5][4] = " 光環： 功體特效可作用到附近三格内隊友。";

    DrawRectangle(15, 16, 100, petCount * 23 + 10, 0, 0xFFFFFFFF, 40);
    for (int i = 0; i < petCount; ++i) {
        Role& r = GameManager::getInstance().getRole(i + 1);
        std::string name = TextManager::getInstance().gbkToUtf8(r.getName());
        uint32_t c1 = (i == petId) ? 0xFFFF00FF : 0xFFFFFFFF;
        uint32_t c2 = (i == petId) ? 0x000000FF : 0x000000FF;
        DrawShadowTextUtf8(name, 8 + textOffsetX, 20 + 23 * i, c1, c2);
    }

    DrawHead(pet.getHeadNum(), 140, 90);

    bool learned = pet.getMagic(selectedIndex) > 0;
    if (learned) pet.setMagic(selectedIndex, 1);
    std::string learnedText = learned ? " 已習得" : " 未習得";
    uint32_t learnedColor = learned ? 0xFFFFFFFF : 0x808080FF;
    DrawShadowTextUtf8(learnedText, 130 + textOffsetX, 260, learnedColor, learnedColor);

    int remaining = GetRemainingSkillPoints();
    std::ostringstream rem;
    rem << std::setw(3) << remaining;
    DrawShadowTextUtf8(" 剩餘技能點數：", 220 + textOffsetX, 70, 0xFFFFFFFF, 0x000000FF);
    DrawShadowTextUtf8(rem.str(), 360 + textOffsetX, 70, 0xFFFFFFFF, 0x000000FF);

    for (int i = 0; i < 5; ++i) {
        int iconIndex = petId * 5 + i;
        int baseX = 140 + i * 50;
        int baseY = 120;
        if (pet.getMagic(i) > 0 && iconIndex >= 0 && iconIndex < (int)m_skillIcons.size() && m_skillIcons[iconIndex].tex) {
            const SkillIcon& icon = m_skillIcons[iconIndex];
            SDL_FRect dest = { (float)(baseX - icon.x), (float)(baseY - icon.y), (float)icon.w, (float)icon.h };
            SDL_RenderTexture(m_renderer, icon.tex, NULL, &dest);
        } else {
            DrawRectangle(baseX + 1, baseY + 1, 39, 39, 0, 0xFFFFFFFF, 0);
        }
    }
    DrawRectangle(140 + selectedIndex * 50, 120, 41, 41, 0, 0xFFFFFFFF, 0);

    DrawShadowTextUtf8(words[roleId][selectedIndex], 110 + textOffsetX, 170, 0x00FF00FF, 0x00FF00FF);
    DrawShadowTextUtf8(" 所需技能點數：", 110 + textOffsetX, 230, 0xFFFFFFFF, 0x000000FF);
    std::ostringstream need;
    need << std::setw(3) << (selectedIndex + 1);
    DrawShadowTextUtf8(need.str(), 250 + textOffsetX, 230, 0xFFFFFFFF, 0x000000FF);
}

void UIManager::SelectShowTeammate() {
    bool running = true;
    int tMenu = 1;
    int rMenu = 0;
    int position = 0;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                GameManager::getInstance().Quit();
                return;
            }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_ESCAPE) {
                    running = false;
                }
                // Basic navigation
                if (event.key.key == SDLK_DOWN || event.key.key == SDLK_KP_2) {
                    if (position == 0) { tMenu = (tMenu % 5) + 1; }
                    else { rMenu = (rMenu + 2) % 26; }
                } else if (event.key.key == SDLK_UP || event.key.key == SDLK_KP_8) {
                    if (position == 0) { tMenu = (tMenu == 1) ? 5 : tMenu - 1; }
                    else { rMenu = (rMenu == 0) ? 24 : rMenu - 2; }
                } else if (event.key.key == SDLK_RIGHT || event.key.key == SDLK_KP_6) {
                    position = 1;
                } else if (event.key.key == SDLK_LEFT || event.key.key == SDLK_KP_4) {
                    position = 0;
                }
            }
        }

        SDL_RenderClear(m_renderer);
        if (m_texMenuBackground) {
            SDL_RenderTexture(m_renderer, m_texMenuBackground, NULL, NULL);
        }
        ShowTeammate(tMenu, rMenu, position);
        VirtualControls::present(m_renderer);
        SDL_Delay(16);
    }
}

void UIManager::ShowTeammate(int tMenu, int rMenu, int position) {
    if (!m_texTeammate) LoadSystemGraphics();
    if (m_texTeammate) {
        SDL_RenderTexture(m_renderer, m_texTeammate, NULL, NULL);
    }

    int x1 = 120;
    int x2 = 350;
    int y1 = 35;
    int y2 = 35;

    DrawRectangle(x1 + 15, y1 - 5, 220, 160, 0, 0xFFFFFFFF, 40);
    DrawShadowTextUtf8(" 隊中人員", x1, y1, 0xFFFFFFFF, 0x000000FF);
    
    DrawRectangle(x2 + 15, y2 - 5, 240, 376, 0, 0xFFFFFFFF, 40);
    DrawShadowTextUtf8(" 預備人員", x2, y2, 0xFFFFFFFF, 0x000000FF);

    const auto& team = GameManager::getInstance().getTeamList();
    for (size_t i = 0; i < team.size() && i < 6; ++i) {
        Role& r = GameManager::getInstance().getRole(team[i]);
        uint32_t color = (position == 0 && (int)i == tMenu - 1) ? 0xFFFFFFFF : 0xAAAAAAFF;
        std::string nameUtf8 = TextManager::getInstance().gbkToUtf8(r.getName());
        DrawShadowTextUtf8(nameUtf8, x1 + 20, y1 + 30 + i * 25, color, 0x000000FF);
    }
}

void UIManager::SelectShowItem() {
    bool running = true;
    int menuSelection = 0;
    int gridSelection = 0;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                GameManager::getInstance().Quit();
                return;
            }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_ESCAPE) {
                    running = false;
                }
                if (event.key.key == SDLK_DOWN || event.key.key == SDLK_KP_2) {
                    menuSelection = (menuSelection + 1) % 6;
                } else if (event.key.key == SDLK_UP || event.key.key == SDLK_KP_8) {
                    menuSelection = (menuSelection + 5) % 6;
                } else if (event.key.key == SDLK_RETURN || event.key.key == SDLK_KP_ENTER || event.key.key == SDLK_SPACE) {
                    bool inSubmenu = true;
                    const int cols = 6;
                    int currentSelection = gridSelection;
                    while (inSubmenu) {
                        SDL_Event ev;
                        while (SDL_PollEvent(&ev)) {
                            if (ev.type == SDL_EVENT_QUIT) {
                                GameManager::getInstance().Quit();
                                return;
                            }
                            if (ev.type == SDL_EVENT_KEY_DOWN) {
                                if (ev.key.key == SDLK_ESCAPE) {
                                    inSubmenu = false;
                                    break;
                                }
                                int filteredSize = 0;
                                {
                                    const auto& inventory = GameManager::getInstance().getItemList();
                                    int filterType = (menuSelection == 0) ? 100 : (menuSelection - 1);
                                    for (const auto& it : inventory) {
                                        if (it.id < 0 || it.amount <= 0) continue;
                                        Item& item = GameManager::getInstance().getItem(it.id);
                                        if (filterType == 100 || item.getItemType() == filterType) {
                                            filteredSize++;
                                        }
                                    }
                                }
                                if (filteredSize <= 0) continue;
                                if (currentSelection < 0) currentSelection = 0;
                                if (currentSelection >= filteredSize) currentSelection = filteredSize - 1;
                                if (ev.key.key == SDLK_LEFT || ev.key.key == SDLK_KP_4) {
                                    if (currentSelection % cols == 0) {
                                        currentSelection += (cols - 1);
                                    } else {
                                        currentSelection -= 1;
                                    }
                                } else if (ev.key.key == SDLK_RIGHT || ev.key.key == SDLK_KP_6) {
                                    if (currentSelection % cols == cols - 1) {
                                        currentSelection -= (cols - 1);
                                    } else {
                                        currentSelection += 1;
                                    }
                                } else if (ev.key.key == SDLK_UP || ev.key.key == SDLK_KP_8) {
                                    if (currentSelection / cols == 0) {
                                        int lastRowCount = filteredSize - ((filteredSize / cols) * cols);
                                        if (filteredSize <= cols) {
                                            currentSelection = (filteredSize - 1);
                                        } else {
                                            int lastRow = (filteredSize - 1) / cols;
                                            int col = currentSelection % cols;
                                            int target = lastRow * cols + col;
                                            if (target >= filteredSize) {
                                                target = filteredSize - 1;
                                            }
                                            currentSelection = target;
                                        }
                                    } else {
                                        currentSelection -= cols;
                                    }
                                } else if (ev.key.key == SDLK_DOWN || ev.key.key == SDLK_KP_2) {
                                    int next = currentSelection + cols;
                                    if (next >= filteredSize) {
                                        currentSelection = currentSelection % cols;
                                    } else {
                                        currentSelection = next;
                                    }
                                } else if (ev.key.key == SDLK_RETURN || ev.key.key == SDLK_KP_ENTER || ev.key.key == SDLK_SPACE) {
                                    int chosenItemId = -1;
                                    int chosenAmount = 0;
                                    {
                                        std::vector<InventoryItem> filtered;
                                        const auto& inventory = GameManager::getInstance().getItemList();
                                        int filterType = (menuSelection == 0) ? 100 : (menuSelection - 1);
                                        for (const auto& it : inventory) {
                                            if (it.id < 0 || it.amount <= 0) continue;
                                            Item& item = GameManager::getInstance().getItem(it.id);
                                            if (filterType == 100 || item.getItemType() == filterType) {
                                                filtered.push_back(it);
                                            }
                                        }
                                        if (!filtered.empty()) {
                                            if (currentSelection < 0) currentSelection = 0;
                                            if (currentSelection >= (int)filtered.size()) currentSelection = (int)filtered.size() - 1;
                                            chosenItemId = filtered[currentSelection].id;
                                            chosenAmount = filtered[currentSelection].amount;
                                        }
                                    }
                                    if (chosenItemId >= 0 && chosenAmount > 0) {
                                        Item& item = GameManager::getInstance().getItem(chosenItemId);
                                        int itemType = item.getItemType();
                                        if (itemType == 0) {
                                            if (item.getEventNum() > 0) {
                                                EventManager::getInstance().ExecuteEvent(item.getEventNum());
                                            } else {
                                                int frontX = 0, frontY = 0;
                                                GameManager::getInstance().getFacingTile(frontX, frontY);
                                                int sceneId = GameManager::getInstance().getCurrentSceneId();
                                                if (sceneId >= 0) {
                                                    EventManager::getInstance().CheckEventWithItem(sceneId, frontX, frontY);
                                                }
                                            }
                                        } else if (itemType == 1) {
                                            int rnum = SelectItemUser(menuSelection, currentSelection, chosenItemId, itemType);
                                            if (rnum >= 0) {
                                                Role& r = GameManager::getInstance().getRole(rnum);
                                                auto meetNeed = [](int need, int stat) {
                                                    if (need == 0) return true;
                                                    if (need > 0) return stat >= need;
                                                    return stat <= -need;
                                                };
                                                bool can = true;
                                                if (item.getNeedSex() >= 0 && item.getNeedSex() != r.getSexual()) can = false;
                                                if (can && !meetNeed(item.getNeedMP(), r.getCurrentMP())) can = false;
                                                if (can && !meetNeed(item.getNeedAttack(), GameManager::getInstance().GetRoleAttack(rnum, true))) can = false;
                                                if (can && !meetNeed(item.getNeedSpeed(), GameManager::getInstance().GetRoleSpeed(rnum, true))) can = false;
                                                if (can && !meetNeed(item.getNeedUsePoi(), GameManager::getInstance().GetRoleUsePoi(rnum, true))) can = false;
                                                if (can && !meetNeed(item.getNeedMedcine(), GameManager::getInstance().GetRoleMedcine(rnum, true))) can = false;
                                                if (can && !meetNeed(item.getNeedMedPoi(), GameManager::getInstance().GetRoleMedPoi(rnum, true))) can = false;
                                                if (can && !meetNeed(item.getNeedFist(), GameManager::getInstance().GetRoleFist(rnum, true))) can = false;
                                                if (can && !meetNeed(item.getNeedSword(), GameManager::getInstance().GetRoleSword(rnum, true))) can = false;
                                                if (can && !meetNeed(item.getNeedKnife(), GameManager::getInstance().GetRoleKnife(rnum, true))) can = false;
                                                if (can && !meetNeed(item.getNeedUnusual(), GameManager::getInstance().GetRoleUnusual(rnum, true))) can = false;
                                                if (can && !meetNeed(item.getNeedHidWeapon(), GameManager::getInstance().GetRoleHidWeapon(rnum, true))) can = false;
                                                int aptitude = r.getAptitude();
                                                if (GameManager::getInstance().CheckEquipSet(r.getEquip(0), r.getEquip(1), r.getEquip(2), r.getEquip(3)) == 2) aptitude = 100;
                                                if (can && !meetNeed(item.getNeedAptitude(), aptitude)) can = false;
                                                if (can) {
                                                    if (r.getMPType() < 2 && item.getNeedMPType() < 2 && r.getMPType() != item.getNeedMPType()) can = false;
                                                }
                                                if (can && item.getOnlyPracRole() >= 0 && item.getOnlyPracRole() != rnum) can = false;
                                                if (can) {
                                                    GameManager::getInstance().useItem(chosenItemId);
                                                    int eqType = item.getEquipType();
                                                    if (eqType >= 0 && eqType < 5) {
                                                        r.setEquip(eqType, chosenItemId);
                                                    }
                                                }
                                            }
                                        } else if (itemType == 2) {
                                            int rnum = SelectItemUser(menuSelection, currentSelection, chosenItemId, itemType);
                                            if (rnum >= 0) {
                                                Role& r = GameManager::getInstance().getRole(rnum);
                                                auto meetNeed = [](int need, int stat) {
                                                    if (need == 0) return true;
                                                    if (need > 0) return stat >= need;
                                                    return stat <= -need;
                                                };
                                                bool ok = true;
                                                if (item.getOnlyPracRole() >= 0 && item.getOnlyPracRole() != rnum) ok = false;
                                                int aptitude = r.getAptitude();
                                                if (GameManager::getInstance().CheckEquipSet(r.getEquip(0), r.getEquip(1), r.getEquip(2), r.getEquip(3)) == 2) aptitude = 100;
                                                if (ok && !meetNeed(item.getNeedAptitude(), aptitude)) ok = false;
                                                int magicId = item.getMagic();
                                                if (ok && magicId > 0) {
                                                    int learnedCount = 0;
                                                    bool already = false;
                                                    for (int i = 0; i < 10; ++i) {
                                                        if (r.getMagic(i) > 0) learnedCount++;
                                                        if (r.getMagic(i) == magicId) {
                                                            already = true;
                                                        }
                                                    }
                                                    if (learnedCount >= 10 && !already) ok = false;
                                                    if (ok) {
                                                        Magic& mg = GameManager::getInstance().getMagic(magicId);
                                                        int level = 0;
                                                        for (int i = 0; i < 10; ++i) {
                                                            if (r.getMagic(i) == magicId) {
                                                                level = r.getMagLevel(i);
                                                                break;
                                                            }
                                                        }
                                                        if ((mg.getMagicType() == 5 && already) || level >= 900) ok = false;
                                                    }
                                                }
                                                if (ok) {
                                                    r.setPracticeBook(chosenItemId);
                                                }
                                            }
                                        } else if (itemType == 3) {
                                            if (item.getEventNum() > 0) {
                                                EventManager::getInstance().ExecuteEvent(item.getEventNum());
                                            } else {
                                                int rnum = SelectItemUser(menuSelection, currentSelection, chosenItemId, itemType);
                                                if (rnum >= 0) {
                                                    GameManager::getInstance().EatOneItem(rnum, chosenItemId, 0);
                                                    GameManager::getInstance().useItem(chosenItemId);
                                                }
                                            }
                                        } else if (itemType == 4) {
                                        }
                                    }
                                }
                            }
                        }
                        SDL_RenderClear(m_renderer);
                        if (m_texMenuBackground) {
                            SDL_RenderTexture(m_renderer, m_texMenuBackground, NULL, NULL);
                        }
                        ShowItem(menuSelection, currentSelection, true);
                        VirtualControls::present(m_renderer);
                        SDL_Delay(16);
                    }
                    gridSelection = currentSelection;
                }
            }
        }

        SDL_RenderClear(m_renderer);
        if (m_texMenuBackground) {
            SDL_RenderTexture(m_renderer, m_texMenuBackground, NULL, NULL);
        }
        ShowItem(menuSelection, gridSelection, false);
        VirtualControls::present(m_renderer);
        SDL_Delay(16);
    }
}

void UIManager::ShowItem(int menuSelection, int selectedIndex, bool inSubmenu) {
    if (!m_texMenuItem) LoadSystemGraphics();
    if (m_texMenuItem) {
        SDL_RenderTexture(m_renderer, m_texMenuItem, NULL, NULL);
    }

    const char* labels[] = { " 全部物品", " 劇情物品", " 神兵寶甲", " 武功秘笈", " 靈丹妙藥", " 傷人暗器" };
    int x = 15, y = 15;
    DrawRectangle(x, y, 90, 6 * 22 + 28, 0, 0xFFFFFFFF, 30);

    int listTextOffsetX = -4;
    for (int i = 0; i < 6; ++i) {
        uint32_t color = (i == menuSelection) ? 0xFFFFFFFF : 0xAAAAAAFF;
        DrawShadowTextUtf8(labels[i], x + 5 + listTextOffsetX, y + 5 + 22 * i, color, 0x000000FF);
    }

    const int infoX = 122;
    const int infoW = 499;
    DrawRectangle(infoX, 16, infoW, 25, 0, 0xFFFFFFFF, 40);
    DrawRectangle(infoX, 46, infoW, 25, 0, 0xFFFFFFFF, 40);
    DrawRectangle(infoX, 76, infoW, 252, 0, 0xFFFFFFFF, 40);
    DrawRectangle(infoX, 335, infoW, 86, 0, 0xFFFFFFFF, 40);

    const int cols = 6;
    const int cellW = 82;
    const int cellH = 82;
    const int gridX = 115 + 12;
    const int gridY = 95 - 14;
    const int maxCells = cols * 3;

    std::vector<InventoryItem> filtered;
    const auto& inventory = GameManager::getInstance().getItemList();
    int filterType = (menuSelection == 0) ? 100 : (menuSelection - 1);
    for (const auto& it : inventory) {
        if (it.id < 0 || it.amount <= 0) continue;
        Item& item = GameManager::getInstance().getItem(it.id);
        if (filterType == 100 || item.getItemType() == filterType) {
            filtered.push_back(it);
        }
    }

    if (!filtered.empty() && selectedIndex < 0) selectedIndex = 0;
    if (selectedIndex >= (int)filtered.size()) selectedIndex = (int)filtered.size() - 1;
    int page = 0;
    if (selectedIndex >= 0) page = selectedIndex / maxCells;
    int startIndex = page * maxCells;
    for (int i = 0; i < maxCells; ++i) {
        int idx = startIndex + i;
        if (idx >= (int)filtered.size()) break;
        int col = i % cols;
        int row = i / cols;
        int itemId = filtered[idx].id;
        PicImage pic = PicLoader::loadPic("resource/Items.Pic", itemId);
        if (pic.surface) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(m_renderer, pic.surface);
            if (tex) {
                SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
                SDL_FRect dest = { (float)(gridX + col * cellW), (float)(gridY + row * cellH), (float)pic.surface->w, (float)pic.surface->h };
                SDL_RenderTexture(m_renderer, tex, NULL, &dest);
                SDL_DestroyTexture(tex);
            }
            PicLoader::freePic(pic);
        }
    }

    if (!filtered.empty()) {
        int localIndex = selectedIndex - startIndex;
        if (localIndex < 0) localIndex = 0;
        int selCol = localIndex % cols;
        int selRow = localIndex / cols;
        if (inSubmenu) {
            DrawRectangle(gridX + selCol * cellW - 2, gridY + selRow * cellH - 2, cellW - 4, cellH - 4, 0, 0xFFFFFFFF, 0);
        }

        Item& item = GameManager::getInstance().getItem(filtered[selectedIndex].id);
        std::string nameUtf8 = TextManager::getInstance().gbkToUtf8(item.getName());
        std::string introUtf8 = TextManager::getInstance().gbkToUtf8(item.getIntroduction());
        std::string amountStr = std::to_string(filtered[selectedIndex].amount);
        DrawShadowTextUtf8(nameUtf8, 134, 20, 0xFFFF00FF, 0x000000FF);
        DrawShadowTextUtf8(introUtf8, 134, 50, 0xFFFFFFFF, 0x000000FF);
        DrawShadowTextUtf8(" 數量", 430, 20, 0xFFFF00FF, 0x000000FF);
        DrawShadowTextUtf8(amountStr, 490, 20, 0xFFFFFFFF, 0x000000FF);

        const char* typeLabels[] = { " 劇情物品", " 神兵寶甲", " 武功秘笈", " 靈丹妙藥", " 傷人暗器" };
        int t = item.getItemType();
        //if (t >= 0 && t <= 4) {
        //    DrawShadowTextUtf8(typeLabels[t], 134, 350, 0xFFFFFFFF, 0x000000FF);
        //}
        if (inSubmenu) {
            std::vector<std::string> lines;
            auto joinParts = [](const std::vector<std::string>& parts) {
                std::string result;
                for (size_t i = 0; i < parts.size(); ++i) {
                    if (i > 0) result += " ";
                    result += parts[i];
                }
                return result;
            };
            auto addNeedPart = [](std::vector<std::string>& parts, const std::string& name, int v) {
                if (v == 0) return;
                std::string op = v > 0 ? ">=" : "<=";
                parts.push_back(name + op + std::to_string(std::abs(v)));
            };
            auto addAddPart = [](std::vector<std::string>& parts, const std::string& name, int v) {
                if (v == 0) return;
                std::string sign = v > 0 ? "+" : "";
                parts.push_back(name + sign + std::to_string(v));
            };

            if (t == 1) {
                std::vector<std::string> needParts;
                if (item.getNeedSex() >= 0) {
                    std::string sex = item.getNeedSex() == 0 ? "男" : "女";
                    needParts.push_back("性别:" + sex);
                }
                if (item.getNeedMPType() >= 0) {
                    std::string mpType = item.getNeedMPType() == 0 ? "阴" : "阳";
                    needParts.push_back("内功:" + mpType);
                }
                addNeedPart(needParts, "内力", item.getNeedMP());
                addNeedPart(needParts, "攻击", item.getNeedAttack());
                addNeedPart(needParts, "轻功", item.getNeedSpeed());
                addNeedPart(needParts, "用毒", item.getNeedUsePoi());
                addNeedPart(needParts, "医疗", item.getNeedMedcine());
                addNeedPart(needParts, "解毒", item.getNeedMedPoi());
                addNeedPart(needParts, "拳掌", item.getNeedFist());
                addNeedPart(needParts, "御剑", item.getNeedSword());
                addNeedPart(needParts, "耍刀", item.getNeedKnife());
                addNeedPart(needParts, "奇门", item.getNeedUnusual());
                addNeedPart(needParts, "暗器", item.getNeedHidWeapon());
                addNeedPart(needParts, "资质", item.getNeedAptitude());
                if (item.getOnlyPracRole() >= 0 && item.getOnlyPracRole() < GameManager::getInstance().getRoleCount()) {
                    Role& r = GameManager::getInstance().getRole(item.getOnlyPracRole());
                    needParts.push_back("限定:" + TextManager::getInstance().gbkToUtf8(r.getName()));
                }
                if (!needParts.empty()) {
                    lines.push_back("需求 " + joinParts(needParts));
                }
                std::vector<std::string> addParts;
                addAddPart(addParts, "攻击", item.getAddAttack());
                addAddPart(addParts, "防御", item.getAddDefence());
                addAddPart(addParts, "轻功", item.getAddSpeed());
                addAddPart(addParts, "内力", item.getAddMaxMP());
                addAddPart(addParts, "气血", item.getAddMaxHP());
                addAddPart(addParts, "医疗", item.getAddMedcine());
                addAddPart(addParts, "用毒", item.getAddUsePoi());
                addAddPart(addParts, "解毒", item.getAddMedPoi());
                addAddPart(addParts, "拳掌", item.getAddFist());
                addAddPart(addParts, "御剑", item.getAddSword());
                addAddPart(addParts, "耍刀", item.getAddKnife());
                addAddPart(addParts, "奇门", item.getAddUnusual());
                addAddPart(addParts, "暗器", item.getAddHidWeapon());
                addAddPart(addParts, "学识", item.getAddKnowledge());
                addAddPart(addParts, "道德", item.getAddEthics());
                addAddPart(addParts, "连击", item.getAddAttTwice());
                addAddPart(addParts, "毒攻", item.getAddAttPoi());
                if (item.getBattleEffect() > 0) {
                    std::string effectName = GetBattleEffectDisplayName(item.getBattleEffect());
                    if (!effectName.empty()) {
                        lines.push_back("装备特效：" + effectName);
                    }
                }
                if (!addParts.empty()) {
                    lines.push_back("加成 " + joinParts(addParts));
                }
            } else if (t == 2) {
                int magicId = item.getMagic();
                if (magicId > 0) {
                    Magic& mg = GameManager::getInstance().getMagic(magicId);
                    std::string magicName = TextManager::getInstance().gbkToUtf8(mg.getName());
                    lines.push_back("武功 " + magicName);
                    std::vector<std::string> addParts;
                    int idx = 2;
                    addAddPart(addParts, "攻击", mg.getAddAtt(idx));
                    addAddPart(addParts, "防御", mg.getAddDef(idx));
                    addAddPart(addParts, "轻功", mg.getAddSpd(idx));
                    addAddPart(addParts, "内力", mg.getAddMP(idx));
                    addAddPart(addParts, "气血", mg.getAddHP(idx));
                    addAddPart(addParts, "医疗", mg.getAddMedcine());
                    addAddPart(addParts, "用毒", mg.getAddUsePoi());
                    addAddPart(addParts, "解毒", mg.getAddMedPoi());
                    addAddPart(addParts, "抗毒", mg.getAddDefPoi());
                    addAddPart(addParts, "拳掌", mg.getAddFist());
                    addAddPart(addParts, "御剑", mg.getAddSword());
                    addAddPart(addParts, "耍刀", mg.getAddKnife());
                    addAddPart(addParts, "奇门", mg.getAddUnusual());
                    addAddPart(addParts, "暗器", mg.getAddHidWeapon());
                    if (!addParts.empty()) {
                        lines.push_back("修炼加成 " + joinParts(addParts));
                    }
                    if (mg.getBattleState() > 0) {
                        std::string stateName = GetBattleEffectDisplayName(mg.getBattleState());
                        if (!stateName.empty()) {
                            lines.push_back("功体特效：" + stateName);
                        }
                    }
                }
            } else {
                std::vector<std::string> addParts;
                addAddPart(addParts, "气血", item.getAddCurrentHP());
                addAddPart(addParts, "内力", item.getAddCurrentMP());
                addAddPart(addParts, "体力", item.getAddPhyPower());
                addAddPart(addParts, "中毒", item.getAddPoi());
                addAddPart(addParts, "解毒", item.getAddMedPoi());
                addAddPart(addParts, "抗毒", item.getAddDefPoi());
                addAddPart(addParts, "医疗", item.getAddMedcine());
                addAddPart(addParts, "用毒", item.getAddUsePoi());
                if (!addParts.empty()) {
                    lines.push_back("效果 " + joinParts(addParts));
                }
            }

            if (!lines.empty()) {
                std::string detail;
                for (size_t i = 0; i < lines.size(); ++i) {
                    if (i > 0) detail += "\n";
                    detail += lines[i];
                }
                TTF_Font* useFont = (m_fontEnglish && IsAsciiOnly(detail)) ? m_fontEnglish : m_font;
                if (useFont) {
                    SDL_Color shadow = { 0, 0, 0, 255 };
                    SDL_Color main = { 255, 255, 255, 255 };
                    int wrapWidth = infoW - 20;
                    SDL_Surface* s2 = TTF_RenderText_Blended_Wrapped(useFont, detail.c_str(), 0, shadow, wrapWidth);
                    if (s2) {
                        SDL_Texture* t2 = SDL_CreateTextureFromSurface(m_renderer, s2);
                        SDL_FRect d2 = { (float)(infoX + 11), 343.0f, (float)s2->w, (float)s2->h };
                        SDL_RenderTexture(m_renderer, t2, NULL, &d2);
                        SDL_DestroyTexture(t2);
                        SDL_DestroySurface(s2);
                    }
                    SDL_Surface* s1 = TTF_RenderText_Blended_Wrapped(useFont, detail.c_str(), 0, main, wrapWidth);
                    if (s1) {
                        SDL_Texture* t1 = SDL_CreateTextureFromSurface(m_renderer, s1);
                        SDL_FRect d1 = { (float)(infoX + 10), 342.0f, (float)s1->w, (float)s1->h };
                        SDL_RenderTexture(m_renderer, t1, NULL, &d1);
                        SDL_DestroyTexture(t1);
                        SDL_DestroySurface(s1);
                    }
                }
            }
        }
    }
}

// Stubs for missing implementations
void UIManager::PlayTitleAnimation() {
    PlayBeginningMovie(0, -1);

    // 动画播放完后，加载 Background.Pic 的 Index 0 作为开始菜单背景
    if (m_texBeginBackground) {
        SDL_DestroyTexture(m_texBeginBackground);
        m_texBeginBackground = nullptr;
    }
    
    PicImage bgPic = PicLoader::loadPic("resource/Background.Pic", 0);
    if (bgPic.surface) {
        m_texBeginBackground = SDL_CreateTextureFromSurface(m_renderer, bgPic.surface);
        PicLoader::freePic(bgPic);
    }
}

void UIManager::PlayBeginningMovie(int beginNum, int endNum) {
    int frameCount = PicLoader::getPicCount("resource/Begin.Pic");
    if (frameCount <= 0) return;

    if (beginNum < 0) beginNum = frameCount - 1;
    if (endNum < 0) endNum = frameCount - 1;
    if (beginNum > frameCount - 1) beginNum = frameCount - 1;
    if (endNum > frameCount - 1) endNum = frameCount - 1;

    bool skip = false;
    SDL_Event event;
    auto playFrame = [&](int i) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                skip = true;
                break;
            }
            if (event.type == SDL_EVENT_KEY_UP) {
                if (event.key.key == SDLK_ESCAPE || event.key.key == SDLK_RETURN || event.key.key == SDLK_SPACE) {
                    skip = true;
                    break;
                }
            }
        }
        if (skip) return;
        PicImage pic = PicLoader::loadPic("resource/Begin.Pic", i);
        if (pic.surface) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(m_renderer, pic.surface);
            if (tex) {
                SDL_RenderClear(m_renderer);
                SDL_FRect dest = { 0.0f, 0.0f, 640.0f, 480.0f };
                SDL_RenderTexture(m_renderer, tex, NULL, &dest);
                VirtualControls::present(m_renderer);
                SDL_DestroyTexture(tex);
            }
            PicLoader::freePic(pic);
        }
        SDL_Delay(endNum >= beginNum ? 40 : 16);
    };

    if (endNum >= beginNum) {
        for (int i = beginNum; i <= endNum && !skip; ++i) playFrame(i);
    } else {
        for (int i = beginNum; i >= endNum && !skip; --i) playFrame(i);
    }
}

void UIManager::DrawCharacterCreationNamePrompt(const std::string& nameUtf8) {
    if (m_texBeginBackground) DrawTitleBackground();
    DrawRectangle(100, 100, 440, 200, 0, 0xFFFFFFFF, 200);
    DrawShadowTextUtf8(" 請輸入主角姓名", 120, 120, 0xFFFF00FF, 0x000000FF);
    DrawShadowTextUtf8(" Enter 確認　Backspace 刪除　Esc 返回", 120, 160, 0xCCCCCCCC, 0x000000FF);
    DrawShadowTextUtf8(nameUtf8.empty() ? " " : nameUtf8, 200, 210, 0xFFFFFFFF, 0x000000FF);
}

void UIManager::DrawCharacterCreationAttributes(const Role& role) {
    (void)role;
    // Reuse status panel layout; tip bar at bottom (Pascal ShowRandomAttribute)
    ShowStatus(0);
    DrawShadowTextUtf8(" 資質", 30, 380, 0x21FFFFFF, 0x23FFFFFF);
    DrawShadowTextUtf8(std::to_string(GameManager::getInstance().getRole(0).getAptitude()), 150, 380, 0x63FFFFFF, 0x66FFFFFF);
    DrawShadowTextUtf8(" 選定屬性後按回車，任意鍵重骰，Esc 返回", 210, 380, 0x05FFFFFF, 0x07FFFFFF);
}

void UIManager::DrawTitleScreen() {
    // 绘制开始菜单画面
    DrawTitleBackground();
    // 可以在此处添加按钮绘制逻辑，对应 Pascal 的 drawtitlepic(0, x, y)
}

void UIManager::DrawTitleBackground() {
    if (m_texBeginBackground) {
        int w, h;
        SDL_GetWindowSize(m_window, &w, &h);
        SDL_FRect dest = { 0.0f, 0.0f, (float)w, (float)h };
        SDL_RenderTexture(m_renderer, m_texBeginBackground, NULL, &dest);
    }
}
void UIManager::DrawCenteredTexture(SDL_Texture* tex) {
    if (!tex) return;
    int w, h;
    SDL_GetWindowSize(m_window, &w, &h);
    float tw, th;
    SDL_GetTextureSize(tex, &tw, &th);
    
    SDL_FRect dest = { (float)(w - tw) / 2, (float)(h - th) / 2, tw, th };
    SDL_RenderTexture(m_renderer, tex, NULL, &dest);
}

void UIManager::DrawText(const std::string& text, int x, int y, uint32_t color, int fontSize) {
    // Assumes GBK input, uses TextManager to convert and render
    TextManager::getInstance().RenderText(text, x, y, color);
}

void UIManager::DrawTextUtf8(const std::string& text, int x, int y, uint32_t color, int fontSize) {
    TextManager::getInstance().RenderTextUtf8(text, x, y, color, fontSize);
}

void UIManager::DrawShadowText(const std::string& text, int x, int y, uint32_t color1, uint32_t color2, int fontSize) {
    // Shadow
    TextManager::getInstance().RenderText(text, x + 1, y + 1, color2);
    // Main
    TextManager::getInstance().RenderText(text, x, y, color1);
}

void UIManager::ShowCharacterCreation(const Role& role) {
    if (m_texBeginBackground) {
        DrawTitleBackground();
    }

    DrawRectangle(100, 100, 440, 200, 0, 0xFFFFFFFF, 200);
    if (m_font) {
        const int wrapWidth = 420;
        SDL_Color shadow = { 0, 0, 0, 255 };
        SDL_Color main = { 255, 255, 255, 255 };
        SDL_Surface* s2 = TTF_RenderText_Blended_Wrapped(m_font, "請輸入主角姓名: (Enter 確認 / Backspace 刪除 / R 重骰 / Esc 返回)", 0, shadow, wrapWidth);
        if (s2) {
            SDL_Texture* t2 = SDL_CreateTextureFromSurface(m_renderer, s2);
            SDL_FRect d2 = { 121.0f, 141.0f, (float)s2->w, (float)s2->h };
            SDL_RenderTexture(m_renderer, t2, NULL, &d2);
            SDL_DestroyTexture(t2);
            SDL_DestroySurface(s2);
        }

        SDL_Surface* s1 = TTF_RenderText_Blended_Wrapped(m_font, "請輸入主角姓名: (Enter 確認 / Backspace 刪除 / R 重骰 / Esc 返回)", 0, main, wrapWidth);
        if (s1) {
            SDL_Texture* t1 = SDL_CreateTextureFromSurface(m_renderer, s1);
            SDL_FRect d1 = { 120.0f, 140.0f, (float)s1->w, (float)s1->h };
            SDL_RenderTexture(m_renderer, t1, NULL, &d1);
            SDL_DestroyTexture(t1);
            SDL_DestroySurface(s1);
        }
    }
    DrawShadowTextUtf8(TextManager::getInstance().nameToUtf8(role.getName()), 280, 200, 0xFFFF00FF, 0x000000FF);
}

bool UIManager::ShowSaveLoadMenu(bool isSave) {
    // Align with Pascal:
    //   MenuLoadAtBeginning -> CommonMenu(265, 280, 107, 5)  vertical
    //   MenuSave            -> CommonMenu(133, 30, 67, 4)    vertical
    CaptureScreen();

    bool running = true;
    int currentSelection = 0;
    SDL_Event event;
    bool loaded = false;

    const char* loadSlots[] = {
        " 載入進度一", " 載入進度二", " 載入進度三",
        " 載入進度四", " 載入進度五", " 載入自動檔"
    };
    const char* saveSlots[] = {
        " 進度一", " 進度二", " 進度三", " 進度四", " 進度五"
    };
    const char** slots = isSave ? saveSlots : loadSlots;
    const int slotCount = isSave ? 5 : 6;
    const int maxIndex = slotCount - 1;

    const int menuX = isSave ? 133 : 265;
    const int menuY = isSave ? 30 : 280;
    const int menuW = isSave ? 67 : 107;
    const int menuH = maxIndex * 22 + 28;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                GameManager::getInstance().Quit();
                return false;
            }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_ESCAPE) {
                    running = false;
                } else if (event.key.key == SDLK_DOWN || event.key.key == SDLK_KP_2) {
                    currentSelection = (currentSelection + 1) % slotCount;
                } else if (event.key.key == SDLK_UP || event.key.key == SDLK_KP_8) {
                    currentSelection = (currentSelection + slotCount - 1) % slotCount;
                } else if (event.key.key == SDLK_RETURN || event.key.key == SDLK_SPACE) {
                    if (isSave) {
                        GameManager::getInstance().SaveGame(currentSelection + 1);
                        ShowDialogue("進度已保存", 0, 0);
                        running = false;
                    } else {
                        int slot = (currentSelection == 5) ? 6 : (currentSelection + 1);
                        loaded = GameManager::getInstance().LoadGame(slot);
                        if (loaded) {
                            running = false;
                        } else {
                            ShowDialogue("讀取失敗", 0, 0);
                        }
                    }
                }
            } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
                float mx = 0, my = 0;
                SDL_GetMouseState(&mx, &my);
                if (event.button.button == SDL_BUTTON_RIGHT) {
                    running = false;
                } else if (event.button.button == SDL_BUTTON_LEFT) {
                    if (mx >= menuX && mx < menuX + menuW &&
                        my > menuY && my < menuY + menuH + 1) {
                        int row = static_cast<int>((my - menuY - 2) / 22);
                        if (row < 0) row = 0;
                        if (row > maxIndex) row = maxIndex;
                        currentSelection = row;
                        if (isSave) {
                            GameManager::getInstance().SaveGame(currentSelection + 1);
                            ShowDialogue("進度已保存", 0, 0);
                            running = false;
                        } else {
                            int slot = (currentSelection == 5) ? 6 : (currentSelection + 1);
                            loaded = GameManager::getInstance().LoadGame(slot);
                            if (loaded) {
                                running = false;
                            } else {
                                ShowDialogue("讀取失敗", 0, 0);
                            }
                        }
                    }
                }
            } else if (event.type == SDL_EVENT_MOUSE_MOTION) {
                float mx = 0, my = 0;
                SDL_GetMouseState(&mx, &my);
                if (mx >= menuX && mx < menuX + menuW &&
                    my > menuY && my < menuY + menuH + 1) {
                    int row = static_cast<int>((my - menuY - 2) / 22);
                    if (row < 0) row = 0;
                    if (row > maxIndex) row = maxIndex;
                    currentSelection = row;
                }
            }
        }

        SDL_RenderClear(m_renderer);
        if (m_texMenuBackground) {
            SDL_RenderTexture(m_renderer, m_texMenuBackground, NULL, NULL);
        } else {
            SDL_SetRenderDrawColor(m_renderer, 50, 50, 50, 255);
            SDL_RenderClear(m_renderer);
        }

        // Pascal ShowCommonMenu: DrawRectangle(x, y, w, max*22+28, 0, colcolor(255), 30)
        DrawRectangle(menuX, menuY, menuW, menuH, 0, 0xFFFFFFFF, 30);
        for (int i = 0; i < slotCount; ++i) {
            uint32_t color = (i == currentSelection) ? 0x64FFFFFF : 0x05FFFFFF;
            uint32_t shadow = (i == currentSelection) ? 0x66FFFFFF : 0x07FFFFFF;
            // Pascal: drawshadowtext(..., x - 17, y + 2 + 22 * i, ...)
            DrawShadowTextUtf8(slots[i], menuX - 17, menuY + 2 + 22 * i, color, shadow);
        }

        VirtualControls::present(m_renderer);
        SDL_Delay(16);
    }
    return loaded;
}

int UIManager::CommonMenu(int x, int y, int width, const std::vector<std::string>& itemsUtf8) {
    if (itemsUtf8.empty()) return -1;
    const int count = static_cast<int>(itemsUtf8.size());
    const int maxIndex = count - 1;
    int current = 0;
    bool running = true;
    SDL_Event event;
    const int menuH = maxIndex * 22 + 28;
    uint32_t frame = GraphicsUtils::getPaletteColor(255);

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                GameManager::getInstance().Quit();
                return -1;
            }
            if (!VirtualControls::handleEvent(event)) continue;
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_ESCAPE) return -1;
                if (event.key.key == SDLK_DOWN || event.key.key == SDLK_KP_2)
                    current = (current + 1) % count;
                if (event.key.key == SDLK_UP || event.key.key == SDLK_KP_8)
                    current = (current + count - 1) % count;
                if (event.key.key == SDLK_RETURN || event.key.key == SDLK_SPACE ||
                    event.key.key == SDLK_KP_ENTER)
                    return current;
            } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
                float mx = 0, my = 0;
                SDL_GetMouseState(&mx, &my);
                if (event.button.button == SDL_BUTTON_RIGHT) return -1;
                if (event.button.button == SDL_BUTTON_LEFT &&
                    mx >= x && mx < x + width && my > y && my < y + menuH + 1) {
                    int row = static_cast<int>((my - y - 2) / 22);
                    if (row < 0) row = 0;
                    if (row > maxIndex) row = maxIndex;
                    return row;
                }
            } else if (event.type == SDL_EVENT_MOUSE_MOTION) {
                float mx = 0, my = 0;
                SDL_GetMouseState(&mx, &my);
                if (mx >= x && mx < x + width && my > y && my < y + menuH + 1) {
                    int row = static_cast<int>((my - y - 2) / 22);
                    if (row < 0) row = 0;
                    if (row > maxIndex) row = maxIndex;
                    current = row;
                }
            }
        }

        GameManager::getInstance().RenderScreenTo(m_renderer);
        DrawRectangle(x, y, width, menuH, 0, frame, 30);
        for (int i = 0; i < count; ++i) {
            uint32_t color = (i == current) ? 0x64FFFFFF : 0x05FFFFFF;
            uint32_t shadow = (i == current) ? 0x66FFFFFF : 0x07FFFFFF;
            DrawShadowTextUtf8(itemsUtf8[i], x - 17, y + 2 + 22 * i, color, shadow);
        }
        VirtualControls::present(m_renderer);
        SDL_Delay(16);
    }
    return -1;
}

int UIManager::CommonScrollMenu(int x, int y, int width, const std::vector<std::string>& itemsUtf8, int visibleCount) {
    if (itemsUtf8.empty()) return -1;
    const int count = static_cast<int>(itemsUtf8.size());
    if (visibleCount < 1) visibleCount = 1;
    if (visibleCount > count) visibleCount = count;
    int current = 0;
    int top = 0;
    bool running = true;
    SDL_Event event;
    const int menuH = (visibleCount - 1) * 22 + 28;
    uint32_t frame = GraphicsUtils::getPaletteColor(255);

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                GameManager::getInstance().Quit();
                return -1;
            }
            if (!VirtualControls::handleEvent(event)) continue;
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_ESCAPE) return -1;
                if (event.key.key == SDLK_DOWN || event.key.key == SDLK_KP_2) {
                    if (current < count - 1) ++current;
                    if (current >= top + visibleCount) top = current - visibleCount + 1;
                }
                if (event.key.key == SDLK_UP || event.key.key == SDLK_KP_8) {
                    if (current > 0) --current;
                    if (current < top) top = current;
                }
                if (event.key.key == SDLK_RETURN || event.key.key == SDLK_SPACE ||
                    event.key.key == SDLK_KP_ENTER)
                    return current;
            } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
                float mx = 0, my = 0;
                SDL_GetMouseState(&mx, &my);
                if (event.button.button == SDL_BUTTON_RIGHT) return -1;
                if (event.button.button == SDL_BUTTON_LEFT &&
                    mx >= x && mx < x + width && my > y && my < y + menuH + 1) {
                    int row = static_cast<int>((my - y - 2) / 22);
                    if (row < 0) row = 0;
                    if (row >= visibleCount) row = visibleCount - 1;
                    return top + row;
                }
            }
        }

        GameManager::getInstance().RenderScreenTo(m_renderer);
        DrawRectangle(x, y, width, menuH, 0, frame, 30);
        for (int i = 0; i < visibleCount; ++i) {
            int idx = top + i;
            if (idx >= count) break;
            uint32_t color = (idx == current) ? 0x64FFFFFF : 0x05FFFFFF;
            uint32_t shadow = (idx == current) ? 0x66FFFFFF : 0x07FFFFFF;
            DrawShadowTextUtf8(itemsUtf8[idx], x - 17, y + 2 + 22 * i, color, shadow);
        }
        VirtualControls::present(m_renderer);
        SDL_Delay(16);
    }
    return -1;
}

void UIManager::ShowDialogue(const std::string& text, int headId, int mode, const std::string& nameUtf8, const std::string& nameRawBytes, int colorIndex) {
    SDL_Event event;
    static bool s_showNameDebug = false;

    InputManager::getInstance().FlushEvents();
    const uint32_t openTime = SDL_GetTicks();
    const uint32_t debounceMs = 150;

    auto bytesToHex = [](const std::string& s, size_t maxBytes) -> std::string {
        static const char* kHex = "0123456789ABCDEF";
        std::string out;
        size_t n = std::min(maxBytes, s.size());
        out.reserve(n * 3 + 8);
        for (size_t i = 0; i < n; ++i) {
            unsigned char b = static_cast<unsigned char>(s[i]);
            out.push_back(kHex[(b >> 4) & 0xF]);
            out.push_back(kHex[b & 0xF]);
            if (i + 1 < n) out.push_back(' ');
        }
        if (s.size() > maxBytes) out += " ..";
        return out;
    };

    auto hasReplacement = [](const std::string& s) -> bool {
        for (size_t i = 0; i + 2 < s.size(); ++i) {
            if (static_cast<unsigned char>(s[i]) == 0xEF &&
                static_cast<unsigned char>(s[i + 1]) == 0xBF &&
                static_cast<unsigned char>(s[i + 2]) == 0xBD) {
                return true;
            }
        }
        return false;
    };

    auto sanitizeLabel = [](std::string s) -> std::string {
        while (!s.empty()) {
            unsigned char b = static_cast<unsigned char>(s.back());
            if (b == 0 || b <= 0x20 || b == 0x7F) s.pop_back();
            else break;
        }
        size_t zeroPos = s.find('\0');
        if (zeroPos != std::string::npos) s.resize(zeroPos);
        return s;
    };

    auto buildPlainMapping = [&](const std::string& src, std::vector<size_t>& plainEnds, std::vector<size_t>& originalEnds) -> std::string {
        plainEnds.clear();
        originalEnds.clear();
        std::string plain;
        size_t i = 0;
        while (i < src.size()) {
            unsigned char c = static_cast<unsigned char>(src[i]);
            if (c == '^' && i + 1 < src.size()) {
                char n = src[i + 1];
                if (n == '^' || IsHexDigit(n)) {
                    i += 2;
                    continue;
                }
            }
            size_t j = i + 1;
            while (j < src.size() && (static_cast<unsigned char>(src[j]) & 0xC0) == 0x80) {
                j++;
            }
            plain.append(src.substr(i, j - i));
            plainEnds.push_back(plain.size());
            originalEnds.push_back(j);
            i = j;
        }
        return plain;
    };

    auto takePageText = [&](const std::string& remaining, int wrapWidth, int maxTextHeight) -> std::string {
        if (!m_font || wrapWidth <= 10 || maxTextHeight <= 5) return remaining;
        if (remaining.empty()) return remaining;

        std::vector<size_t> plainEnds;
        std::vector<size_t> originalEnds;
        std::string plain = buildPlainMapping(remaining, plainEnds, originalEnds);
        if (plain.empty() || plainEnds.empty()) return remaining;

        auto fits = [&](size_t endIndex) -> bool {
            SDL_Color c = { 255, 255, 255, 255 };
            std::string candidate = plain.substr(0, endIndex);
            SDL_Surface* s = TTF_RenderText_Blended_Wrapped(m_font, candidate.c_str(), 0, c, wrapWidth);
            if (!s) return true;
            bool ok = s->h <= maxTextHeight;
            SDL_DestroySurface(s);
            return ok;
        };

        size_t lo = 0;
        size_t hi = plainEnds.size();
        size_t best = 0;
        while (lo < hi) {
            size_t mid = (lo + hi) / 2;
            size_t endIndex = plainEnds[mid];
            if (fits(endIndex)) {
                best = endIndex;
                lo = mid + 1;
            } else {
                hi = mid;
            }
        }

        if (best == 0) best = plainEnds.front();
        auto it = std::lower_bound(plainEnds.begin(), plainEnds.end(), best);
        size_t idx = (it == plainEnds.end()) ? plainEnds.size() - 1 : static_cast<size_t>(it - plainEnds.begin());
        size_t originalCut = originalEnds[idx];
        return remaining.substr(0, originalCut);
    };

    std::string showName = sanitizeLabel(nameUtf8);
    std::string remainingText = text;
    std::string rawNameBytes = nameRawBytes;
    uint32_t baseColor = 0xFFFFFFFF;
    if (colorIndex >= 0 && colorIndex <= 255) {
        uint32_t mapped = PaletteIndexToColor(static_cast<uint8_t>(colorIndex));
        if (mapped != 0) baseColor = mapped;
    }
    uint32_t nameColor = (colorIndex >= 0 && colorIndex <= 255) ? baseColor : 0xFFFF00FF;
    uint32_t shadowColor = ResolveTextColor(0x000000FF);
    SDL_Texture* frozenBackground = nullptr;
    SDL_Surface* screenSurface = GameManager::getInstance().getScreenSurface();
    if (screenSurface) {
        frozenBackground = SDL_CreateTextureFromSurface(m_renderer, screenSurface);
    }

    while (true) {
        int w, h;
        SDL_GetWindowSize(m_window, &w, &h);

        int boxH = 150;
        int boxY = h - boxH - 20;
        const int textX = (headId >= 0) ? 150 : 40;
        const int textY = boxY + 40;
        const int wrapWidth = (w - 20) - textX;
        const int maxTextH = (boxY + boxH - 10) - textY;

        std::string pageText = sanitizeLabel(takePageText(remainingText, wrapWidth, maxTextH));
        if (pageText.empty() && !remainingText.empty()) {
            pageText = remainingText.substr(0, 1);
        }

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                GameManager::getInstance().Quit();
                return;
            }
        }

        bool waiting = true;
        while (waiting) {
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_EVENT_QUIT) {
                    GameManager::getInstance().Quit();
                    return;
                }
                if (event.type == SDL_EVENT_KEY_UP) {
                    if (SDL_GetTicks() - openTime < debounceMs) continue;
                    if (event.key.key == SDLK_F3) {
                        s_showNameDebug = !s_showNameDebug;
                    }
                    if (event.key.key == SDLK_SPACE || event.key.key == SDLK_RETURN || event.key.key == SDLK_ESCAPE) {
                        waiting = false;
                    }
                }
                if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
                    if (SDL_GetTicks() - openTime >= debounceMs) {
                        waiting = false;
                    }
                }
            }

            SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
            SDL_RenderClear(m_renderer);
            if (frozenBackground) {
                SDL_RenderTexture(m_renderer, frozenBackground, NULL, NULL);
            } else {
                GameManager::getInstance().RenderScreenTo(m_renderer);
            }

            DrawRectangle(20, boxY, w - 40, boxH, 0x000000CC, 0xFFFFFFFF, 200);

            if (headId >= 0) {
                DrawHead(headId, 40, boxY + 75);
            }

            if (!showName.empty()) {
                int nameX = 40;
                int nameY = boxY + 5;
                if (headId >= 0) {
                    const int headY = boxY + 35;
                    const int approxHeadH = 64;
                    nameY = headY + approxHeadH + 6;
                    const int maxNameY = boxY + boxH - 24;
                    if (nameY > maxNameY) nameY = boxY + 5;
                }
                DrawShadowTextUtf8(showName, nameX, nameY, nameColor, shadowColor);
            }
            if (s_showNameDebug && headId >= 0) {
                if (rawNameBytes.empty()) rawNameBytes = showName;
                std::string d1 = std::string("FONT: ") + (g_loadedFontPath.empty() ? "<none>" : g_loadedFontPath);
                std::string d2 = std::string("RAW(") + std::to_string(rawNameBytes.size()) + "): " + bytesToHex(rawNameBytes, 24);
                std::string d3 = std::string("UTF8(") + std::to_string(showName.size()) + ") repl=" + (hasReplacement(showName) ? "1" : "0") + ": " + bytesToHex(showName, 24);
                DrawShadowTextUtf8(d1, 40, boxY + 22, 0xFFFFFFFF, 0x000000FF);
                DrawShadowTextUtf8(d2, 40, boxY + 38, 0xFFFFFFFF, 0x000000FF);
                DrawShadowTextUtf8(d3, 40, boxY + 54, 0xFFFFFFFF, 0x000000FF);
            }

            if (m_font && wrapWidth > 10 && !pageText.empty()) {
                struct Run {
                    std::string text;
                    uint32_t color;
                };

                auto appendRun = [&](std::vector<Run>& line, const std::string& runText, uint32_t color) {
                    if (runText.empty()) return;
                    if (!line.empty() && line.back().color == color) {
                        line.back().text += runText;
                    } else {
                        line.push_back({ runText, color });
                    }
                };

                auto getFontForText = [&](const std::string& t) -> TTF_Font* {
                    return (m_fontEnglish && IsAsciiOnly(t)) ? m_fontEnglish : m_font;
                };

                std::vector<std::vector<Run>> lines;
                std::vector<Run> currentLine;
                int lineWidth = 0;
                uint32_t currentColor = baseColor;
                int lineHeight = TTF_GetFontHeight(m_font);
                if (lineHeight <= 0) lineHeight = 20;

                size_t i = 0;
                while (i < pageText.size()) {
                    char c = pageText[i];
                    if (c == '^' && i + 1 < pageText.size()) {
                        char n = pageText[i + 1];
                        if (n == '^') {
                            currentColor = baseColor;
                            i += 2;
                            continue;
                        }
                        int hex = HexValue(n);
                        if (hex >= 0) {
                            uint32_t mapped = PaletteIndexToColor(static_cast<uint8_t>(hex));
                            if (mapped != 0) currentColor = mapped;
                            i += 2;
                            continue;
                        }
                    }
                    if (c == '\r' || c == '\n') {
                        lines.push_back(currentLine);
                        currentLine.clear();
                        lineWidth = 0;
                        if (c == '\r' && i + 1 < pageText.size() && pageText[i + 1] == '\n') {
                            i += 2;
                        } else {
                            i += 1;
                        }
                        continue;
                    }

                    size_t j = i + 1;
                    while (j < pageText.size()) {
                        char cj = pageText[j];
                        if (cj == '^' || cj == '\n' || cj == '\r') break;
                        if ((static_cast<unsigned char>(cj) & 0x80) == 0) {
                            j++;
                            continue;
                        }
                        size_t k = j + 1;
                        while (k < pageText.size() && (static_cast<unsigned char>(pageText[k]) & 0xC0) == 0x80) {
                            k++;
                        }
                        j = k;
                    }

                    std::string span = pageText.substr(i, j - i);
                    while (!span.empty()) {
                        int remainingWidth = wrapWidth - lineWidth;
                        if (remainingWidth <= 0) {
                            lines.push_back(currentLine);
                            currentLine.clear();
                            lineWidth = 0;
                            remainingWidth = wrapWidth;
                        }
                        TTF_Font* useFont = getFontForText(span);
                        int measuredWidth = 0;
                        size_t measuredLen = 0;
                        bool ok = TTF_MeasureString(useFont, span.c_str(), span.size(), remainingWidth, &measuredWidth, &measuredLen);
                        if (!ok || measuredLen == 0) {
                            size_t charLen = 1;
                            while (charLen < span.size() && (static_cast<unsigned char>(span[charLen]) & 0xC0) == 0x80) {
                                charLen++;
                            }
                            std::string part = span.substr(0, charLen);
                            appendRun(currentLine, part, currentColor);
                            int w = 0;
                            int h = 0;
                            TTF_GetStringSize(useFont, part.c_str(), part.size(), &w, &h);
                            lineWidth += w;
                            span.erase(0, charLen);
                        } else {
                            std::string part = span.substr(0, measuredLen);
                            appendRun(currentLine, part, currentColor);
                            lineWidth += measuredWidth;
                            span.erase(0, measuredLen);
                        }
                        if (!span.empty()) {
                            lines.push_back(currentLine);
                            currentLine.clear();
                            lineWidth = 0;
                        }
                    }
                    i = j;
                }
                if (!currentLine.empty()) lines.push_back(currentLine);

                int drawY = textY;
                for (const auto& line : lines) {
                    int drawX = textX;
                    for (const auto& run : line) {
                        DrawShadowTextUtf8(run.text, drawX, drawY, run.color, shadowColor);
                        TTF_Font* useFont = getFontForText(run.text);
                        int w = 0;
                        int h = 0;
                        TTF_GetStringSize(useFont, run.text.c_str(), run.text.size(), &w, &h);
                        drawX += w;
                    }
                    drawY += lineHeight;
                }
            }

            VirtualControls::present(m_renderer);
            SDL_Delay(16);
        }

        if (pageText.size() >= remainingText.size()) break;
        remainingText.erase(0, pageText.size());
        while (!remainingText.empty() && (remainingText[0] == '\n' || remainingText[0] == '\r')) {
            remainingText.erase(0, 1);
        }
    }
    if (frozenBackground) {
        SDL_DestroyTexture(frozenBackground);
    }
}

void UIManager::ShowTitle(const std::string& text, int x, int y, uint32_t color1, uint32_t color2) {
    // Align Pascal ShowTitle (StartAmi uses where=3):
    // box (0,60,640,109) alpha 40, cell=25, CHINESE_FONT_SIZE=20, ** = newline, @@ = wait key.
    InputManager::getInstance().FlushEvents();

    const int boxX = 0;
    const int boxY = (y >= 0) ? y : 60;
    const int boxW = 640;
    const int baseBoxH = 109;
    const int fontSize = 20;
    const int cell = 25;
    const int wrapWidth = cell * fontSize; // 500px ≈ 25 Chinese glyphs
    const int lineH = fontSize;

    auto utf8Len = [](const std::string& s) -> size_t {
        size_t n = 0;
        for (size_t i = 0; i < s.size(); ) {
            unsigned char c = static_cast<unsigned char>(s[i]);
            if ((c & 0x80) == 0) i += 1;
            else if ((c & 0xE0) == 0xC0) i += 2;
            else if ((c & 0xF0) == 0xE0) i += 3;
            else i += 4;
            ++n;
        }
        return n;
    };

    // Normalize control sequences in UTF-8 text produced by talkToUtf8
    std::string normalized;
    normalized.reserve(text.size());
    for (size_t i = 0; i < text.size(); ) {
        if (i + 1 < text.size() && text[i] == '*' && text[i + 1] == '*') {
            normalized.push_back('\n');
            i += 2;
            continue;
        }
        if (i + 1 < text.size() && text[i] == '@' && text[i + 1] == '@') {
            normalized.append("\n@@\n");
            i += 2;
            continue;
        }
        // $$ / %% / && — leave a space (name placeholders; StartAmi titles rarely need them)
        if (i + 1 < text.size() &&
            ((text[i] == '$' && text[i + 1] == '$') ||
             (text[i] == '%' && text[i + 1] == '%') ||
             (text[i] == '&' && text[i + 1] == '&'))) {
            normalized.push_back(' ');
            i += 2;
            continue;
        }
        normalized.push_back(text[i]);
        ++i;
    }

    std::string display = TextManager::getInstance().traditionalToSimplified(normalized);

    // Split into pages on @@; within a page wrap by cell width.
    std::vector<std::string> pages;
    {
        std::string cur;
        size_t i = 0;
        while (i < display.size()) {
            if (i + 1 < display.size() && display[i] == '@' && display[i + 1] == '@') {
                pages.push_back(cur);
                cur.clear();
                i += 2;
                while (i < display.size() && (display[i] == '\n' || display[i] == '\r')) ++i;
                continue;
            }
            cur.push_back(display[i]);
            ++i;
        }
        pages.push_back(cur);
    }
    if (pages.empty()) pages.push_back(std::string());

    color1 = ResolveTextColor(color1);
    color2 = ResolveTextColor(color2);
    SDL_Color c2 = { (Uint8)((color2 >> 24) & 0xFF), (Uint8)((color2 >> 16) & 0xFF),
                     (Uint8)((color2 >> 8) & 0xFF), (Uint8)(color2 & 0xFF) };
    SDL_Color c1 = { (Uint8)((color1 >> 24) & 0xFF), (Uint8)((color1 >> 16) & 0xFF),
                     (Uint8)((color1 >> 8) & 0xFF), (Uint8)(color1 & 0xFF) };

    TTF_Font* useFont = m_font;
    if (!useFont) return;

    auto renderPage = [&](const std::string& pageText) {
        // Black full-screen (Pascal where=3)
        SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
        SDL_RenderClear(m_renderer);

        std::string body = pageText;
        while (!body.empty() && (body.back() == '\n' || body.back() == '\r' || body.back() == ' ')) body.pop_back();

        // Estimate lines for box height growth (Pascal extends box as rows increase)
        int approxLines = 1;
        {
            size_t glyphs = 0;
            int lines = 1;
            for (size_t i = 0; i < body.size(); ) {
                if (body[i] == '\n') {
                    ++lines;
                    glyphs = 0;
                    ++i;
                    continue;
                }
                unsigned char c = static_cast<unsigned char>(body[i]);
                if ((c & 0x80) == 0) i += 1;
                else if ((c & 0xE0) == 0xC0) i += 2;
                else if ((c & 0xF0) == 0xE0) i += 3;
                else i += 4;
                ++glyphs;
                if (glyphs >= static_cast<size_t>(cell)) {
                    glyphs = 0;
                    ++lines;
                }
            }
            approxLines = std::max(1, lines);
        }
        int boxH = std::max(baseBoxH, approxLines * lineH + 24);
        if (boxY + boxH > 480) boxH = 480 - boxY;

        // Pascal DrawRectangleWithoutFrame(..., 0, 40) — black fill ~alpha 40/100
        DrawFilledRect(boxX, boxY, boxW, boxH, 0x000000FF, 102);

        if (!body.empty()) {
            // Center horizontally within 640 like Pascal x1 ≈ 300 - len*5
            size_t glyphCount = utf8Len(body);
            int textX = (glyphCount > static_cast<size_t>(cell * 2))
                ? (300 - cell * 10)
                : (300 - static_cast<int>(glyphCount) * 5);
            if (textX < 20) textX = 20;
            if (x >= 0) textX = x;

            int textY = boxY + (boxH / 2) - std::min(approxLines, 5) * (lineH / 2);
            if (textY < boxY + 8) textY = boxY + 8;
            if (y >= 0 && x >= 0) textY = y;

            SDL_Surface* surf2 = TTF_RenderText_Blended_Wrapped(useFont, body.c_str(), 0, c2, wrapWidth);
            if (surf2) {
                SDL_Texture* tex2 = SDL_CreateTextureFromSurface(m_renderer, surf2);
                SDL_FRect dst2 = { (float)(textX + 1), (float)textY, (float)surf2->w, (float)surf2->h };
                SDL_RenderTexture(m_renderer, tex2, NULL, &dst2);
                SDL_DestroyTexture(tex2);
                SDL_DestroySurface(surf2);
            }
            SDL_Surface* surf1 = TTF_RenderText_Blended_Wrapped(useFont, body.c_str(), 0, c1, wrapWidth);
            if (surf1) {
                SDL_Texture* tex1 = SDL_CreateTextureFromSurface(m_renderer, surf1);
                SDL_FRect dst1 = { (float)textX, (float)textY, (float)surf1->w, (float)surf1->h };
                SDL_RenderTexture(m_renderer, tex1, NULL, &dst1);
                SDL_DestroyTexture(tex1);
                SDL_DestroySurface(surf1);
            }
        }
        VirtualControls::present(m_renderer);
    };

    for (const auto& page : pages) {
        if (page.find_first_not_of(" \n\r\t") == std::string::npos && &page != &pages.back()) {
            continue;
        }
        renderPage(page);
        WaitAnyKey(nullptr, nullptr, nullptr);
    }
}

void UIManager::ShowSceneName(int sceneId) {
    Scene* scene = SceneManager::getInstance().GetScene(sceneId);
    if (!scene) return;
    
    std::string sceneNameGbk = scene->getName();
    std::string sceneNameUtf8 = TextManager::getInstance().gbkToUtf8(sceneNameGbk);
    
    int textWidth = sceneNameUtf8.length() * 10;
    int x = 320 - textWidth / 2 + 7;
    int y = 100;
    int w = textWidth + 6;
    int h = 25;
    
    int cx, cy;
    GameManager::getInstance().getCameraPosition(cx, cy);
    GameManager::getInstance().RenderScreenTo(m_renderer);
    
    DrawRectangle(x - 3, y - 3, w + 6, h + 6, 0x00000000, 0x05FFFFFF, 255);
    DrawShadowTextUtf8(sceneNameUtf8, x, y, 0x05FFFFFF, 0x07FFFFFF);
    
    VirtualControls::present(m_renderer);
    
    int entranceMusic = scene->getEntranceMusic();
    if (entranceMusic >= 0) {
        SoundManager::getInstance().PlayMusic(entranceMusic);
    }
    
    SDL_Delay(500);
}

int UIManager::ShowChoice(const std::string& text) {
    bool running = true;
    int selection = 0; // 0=Yes, 1=No
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
             if (event.type == SDL_EVENT_QUIT) {
                GameManager::getInstance().Quit();
                return 0;
            }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_LEFT || event.key.key == SDLK_RIGHT) {
                    selection = !selection;
                }
                if (event.key.key == SDLK_RETURN || event.key.key == SDLK_SPACE) {
                    return (selection == 0) ? 1 : 0; // 1 for Yes
                }
            }
        }

        GameManager::getInstance().RenderScreenTo(m_renderer);
        
        int w, h;
        SDL_GetWindowSize(m_window, &w, &h);
        
        uint32_t bgColor = 0xFFFFFFFF;
        uint32_t selectedColor = 0xFFFFFF00;
        uint32_t selectedShadow = 0xFF000000;
        uint32_t normalColor = 0xFFFFFFFF;
        uint32_t normalShadow = 0xFF000000;
        
        DrawRectangle(w/2 - 150, h/2 - 60, 300, 120, 0x000000CC, bgColor, 255);
        
        DrawShadowTextUtf8(text, w/2 - 100, h/2 - 40, normalColor, normalShadow);

        int yesX = w/2 - 100;
        int noX = w/2;
        int optY = h/2 + 10;
        
        if (selection == 0) {
            DrawRectangle(yesX - 5, optY - 2, 60, 22, 0x00000000, 0xFFFFFF00, 255);
            DrawShadowTextUtf8(" 是", yesX, optY, selectedColor, selectedShadow);
            DrawShadowTextUtf8(" 否", noX, optY, normalColor, normalShadow);
        } else {
            DrawShadowTextUtf8(" 是", yesX, optY, normalColor, normalShadow);
            DrawRectangle(noX - 5, optY - 2, 60, 22, 0x00000000, 0xFFFFFF00, 255);
            DrawShadowTextUtf8(" 否", noX, optY, selectedColor, selectedShadow);
        }

        VirtualControls::present(m_renderer);
        SDL_Delay(16);
    }
    return 0;
}

int UIManager::WaitForKeyPress() {
    SDL_Event event;
    while (true) {
        if (SDL_WaitEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                GameManager::getInstance().Quit();
                return 0;
            }
            if (event.type == SDL_EVENT_KEY_UP) {
                return event.key.key;
            }
            if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
                return event.button.button;
            }
        }
    }
    return 0;
}

void UIManager::WaitAnyKey(int* keycode, int* x, int* y) {
    SDL_Event event;
    while (true) {
        if (SDL_WaitEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                GameManager::getInstance().Quit();
                if (keycode) *keycode = 0;
                if (x) *x = 0;
                if (y) *y = 0;
                return;
            }
            if (event.type == SDL_EVENT_KEY_UP) {
                if (keycode) *keycode = event.key.key;
                if (x) *x = 0;
                if (y) *y = 0;
                return;
            }
            if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
                if (keycode) *keycode = 0;
                if (x) *x = (int)event.button.x;
                if (y) *y = (int)event.button.y;
                return;
            }
        }
    }
}

void UIManager::ShowItemNotification(int itemId, int amount) {
    Item& item = GameManager::getInstance().getItem(itemId);
    std::string nameUtf8 = TextManager::getInstance().gbkToUtf8(item.getName());
    bool gain = amount >= 0;
    int showAmount = std::abs(amount);
    std::string title = gain ? " 得到物品" : " 失去物品";

    PicImage pic = PicLoader::loadPic("resource/Items.Pic", itemId);
    SDL_Texture* itemTex = nullptr;
    int picW = 0;
    int picH = 0;
    if (pic.surface) {
        itemTex = SDL_CreateTextureFromSurface(m_renderer, pic.surface);
        if (itemTex) {
            SDL_SetTextureBlendMode(itemTex, SDL_BLENDMODE_BLEND);
            picW = pic.surface->w;
            picH = pic.surface->h;
        }
    }

    SDL_Texture* frozenBackground = nullptr;
    SDL_Surface* screenSurface = GameManager::getInstance().getScreenSurface();
    if (screenSurface) {
        frozenBackground = SDL_CreateTextureFromSurface(m_renderer, screenSurface);
    }

    bool waiting = true;
    SDL_Event event;
    while (waiting) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                GameManager::getInstance().Quit();
                waiting = false;
            }
            if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                waiting = false;
            }
        }

        int w, h;
        SDL_GetWindowSize(m_window, &w, &h);
        SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
        SDL_RenderClear(m_renderer);
        if (frozenBackground) {
            SDL_RenderTexture(m_renderer, frozenBackground, NULL, NULL);
        } else {
            GameManager::getInstance().RenderScreenTo(m_renderer);
        }

        int boxW = 260;
        int boxH = 140;
        int boxX = (w - boxW) / 2;
        int boxY = (h - boxH) / 2;
        DrawRectangle(boxX, boxY, boxW, boxH, 0x000000CC, 0xFFFFFFFF, 200);

        DrawShadowTextUtf8(title, boxX + 12, boxY + 10, 0xFFFF00FF, 0x000000FF);
        if (itemTex && picW > 0 && picH > 0) {
            SDL_FRect dest = { (float)(boxX + (boxW - picW) / 2), (float)(boxY + 35), (float)picW, (float)picH };
            SDL_RenderTexture(m_renderer, itemTex, NULL, &dest);
        }
        DrawShadowTextUtf8(nameUtf8, boxX + 12, boxY + 35 + picH + 6, 0xFFFFFFFF, 0x000000FF);
        DrawShadowTextUtf8(" 數量", boxX + 12, boxY + 35 + picH + 30, 0xFFFF00FF, 0x000000FF);
        DrawShadowTextUtf8(std::to_string(showAmount), boxX + 70, boxY + 35 + picH + 30, 0xFFFFFFFF, 0x000000FF);

        VirtualControls::present(m_renderer);
        SDL_Delay(16);
    }

    if (frozenBackground) SDL_DestroyTexture(frozenBackground);
    if (itemTex) SDL_DestroyTexture(itemTex);
    if (pic.surface) PicLoader::freePic(pic);
}

void UIManager::FadeScreen(bool fadeIn) {
    // fadeIn: Black -> Transparent
    // !fadeIn: Transparent -> Black
    
    int w, h;
    SDL_GetWindowSize(m_window, &w, &h);
    
    int steps = 20;
    for (int i = 0; i <= steps; ++i) {
        int alpha = fadeIn ? (255 - i * 255 / steps) : (i * 255 / steps);
        
        GameManager::getInstance().RenderScreenTo(m_renderer);
        
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, alpha);
        SDL_FRect rect = { 0.0f, 0.0f, (float)w, (float)h };
        SDL_RenderFillRect(m_renderer, &rect);
        
        VirtualControls::present(m_renderer);
        SDL_Delay(20);
    }
}

void UIManager::FlashScreen(uint32_t color, int durationMs) {
    int w, h;
    SDL_GetWindowSize(m_window, &w, &h);
    
    // Draw colored rect
    GameManager::getInstance().RenderScreenTo(m_renderer);
    
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_renderer, (color >> 24) & 0xFF, (color >> 16) & 0xFF, (color >> 8) & 0xFF, 128);
    SDL_FRect rect = { 0.0f, 0.0f, (float)w, (float)h };
    SDL_RenderFillRect(m_renderer, &rect);
    
    VirtualControls::present(m_renderer);
    SDL_Delay(durationMs);
    
    // Clear effect
    GameManager::getInstance().RenderScreenTo(m_renderer);
    VirtualControls::present(m_renderer);
}

void UIManager::UpdateScreen() {
    VirtualControls::present(m_renderer);
}

void UIManager::ShowShop(int shopId) {
    CaptureScreen();
    int selection = 0;
    int mode = 0; // 0 buy, 1 sell
    bool running = true;
    SDL_Event event;

    struct ShopEntry { int itemId; int price; };
    auto buildBuyStock = [&]() -> std::vector<ShopEntry> {
        std::vector<ShopEntry> stock;
        for (int i = 0; i < 20; ++i) {
            int itemId = GameManager::getInstance().getX50(0x5000 + shopId * 20 + i);
            int price = GameManager::getInstance().getX50(0x5100 + shopId * 20 + i);
            if (itemId > 0 && price >= 0) stock.push_back({itemId, price});
        }
        if (stock.empty() && shopId >= 0 && shopId < GameManager::getInstance().getShopCount()) {
            for (int i = 0; i < 18; ++i) {
                int itemId = GameManager::getInstance().getShopData(shopId, i);
                if (itemId <= 0 || itemId >= GameManager::getInstance().getItemCount()) continue;
                Item& item = GameManager::getInstance().getItem(itemId);
                if (item.getName().empty()) continue;
                int price = item.getPrice();
                if (price < 0) price = 0;
                stock.push_back({itemId, price});
            }
        }
        if (stock.empty()) {
            for (int itemId = 1; itemId <= 10 && itemId < GameManager::getInstance().getItemCount(); ++itemId) {
                Item& item = GameManager::getInstance().getItem(itemId);
                if (!item.getName().empty() && item.getPrice() > 0)
                    stock.push_back({itemId, item.getPrice()});
            }
        }
        return stock;
    };
    auto buildSellStock = [&]() -> std::vector<ShopEntry> {
        std::vector<ShopEntry> stock;
        for (const auto& it : GameManager::getInstance().getItemList()) {
            if (it.id <= 0 || it.amount <= 0) continue;
            Item& item = GameManager::getInstance().getItem(it.id);
            int price = item.getPrice() / 2;
            if (price < 0) price = 0;
            stock.push_back({it.id, price});
        }
        return stock;
    };

    std::vector<ShopEntry> stock = buildBuyStock();

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                GameManager::getInstance().Quit();
                return;
            }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_ESCAPE) running = false;
                else if (event.key.key == SDLK_TAB || event.key.key == SDLK_LEFT || event.key.key == SDLK_RIGHT) {
                    mode = 1 - mode;
                    selection = 0;
                    stock = (mode == 0) ? buildBuyStock() : buildSellStock();
                } else if (event.key.key == SDLK_DOWN || event.key.key == SDLK_KP_2) {
                    if (!stock.empty()) selection = (selection + 1) % (int)stock.size();
                } else if (event.key.key == SDLK_UP || event.key.key == SDLK_KP_8) {
                    if (!stock.empty()) selection = (selection + (int)stock.size() - 1) % (int)stock.size();
                } else if (event.key.key == SDLK_RETURN || event.key.key == SDLK_SPACE) {
                    if (stock.empty()) continue;
                    const ShopEntry& entry = stock[selection];
                    if (mode == 0) {
                        int money = GameManager::getInstance().getItemAmount(0);
                        if (money >= entry.price) {
                            GameManager::getInstance().AddItem(entry.itemId, 1);
                            GameManager::getInstance().AddItem(0, -entry.price);
                            ShowItemNotification(entry.itemId, 1);
                        } else {
                            ShowDialogue(" 銀兩不足", -1, 0);
                        }
                    } else if (GameManager::getInstance().getItemAmount(entry.itemId) > 0) {
                        GameManager::getInstance().AddItem(entry.itemId, -1);
                        GameManager::getInstance().AddItem(0, entry.price);
                        ShowDialogue(" 出售成功 ＋" + std::to_string(entry.price), -1, 0);
                        stock = buildSellStock();
                        if (selection >= (int)stock.size()) selection = std::max(0, (int)stock.size() - 1);
                    }
                }
            }
        }

        SDL_RenderClear(m_renderer);
        if (m_texMenuBackground) SDL_RenderTexture(m_renderer, m_texMenuBackground, NULL, NULL);
        DrawRectangle(80, 60, 480, 320, 0, 0xFFFFFFFF, 40);
        DrawShadowTextUtf8(mode == 0 ? " 商店·購入" : " 商店·賣出", 100, 70, 0xFFFF00FF, 0x000000FF);
        std::string moneyStr = " 銀兩：" + std::to_string(GameManager::getInstance().getItemAmount(0));
        DrawShadowTextUtf8(moneyStr, 380, 70, 0xFFFFFFFF, 0x000000FF);
        DrawShadowTextUtf8(" Tab/←→ 切換買/賣", 100, 95, 0xAAAAAAFF, 0x000000FF);

        size_t shown = std::min(stock.size(), (size_t)10);
        for (size_t i = 0; i < shown; ++i) {
            Item& item = GameManager::getInstance().getItem(stock[i].itemId);
            std::string line = TextManager::getInstance().gbkToUtf8(item.getName()) +
                "  $" + std::to_string(stock[i].price);
            if (mode == 1) line += " x" + std::to_string(GameManager::getInstance().getItemAmount(stock[i].itemId));
            uint32_t color = ((int)i == selection) ? 0xFFFF00FF : 0xFFFFFFFF;
            DrawShadowTextUtf8(line, 100, 120 + (int)i * 22, color, 0x000000FF);
        }
        if (stock.empty()) {
            DrawShadowTextUtf8(mode == 0 ? " （此商店暫無商品）" : " （背包無可賣物品）", 100, 120, 0xFFFFFFFF, 0x000000FF);
        }
        DrawShadowTextUtf8(" 空格/回車確認  ESC離開", 100, 350, 0xAAAAAAFF, 0x000000FF);
        VirtualControls::present(m_renderer);
        SDL_Delay(16);
    }
}

bool UIManager::RunMiniGame(const std::string& titleUtf8, const std::string& hintUtf8, int chancePercent) {
    static bool seeded = false;
    if (!seeded) {
        std::srand(static_cast<unsigned>(std::time(nullptr)));
        seeded = true;
    }
    CaptureScreen();
    int chance = chancePercent;
    if (BattleManager::getInstance().GetPetSkill(4, 2)) {
        chance = std::min(95, chance * 2);
    }
    if (chance < 5) chance = 5;
    if (chance > 95) chance = 95;

    bool decided = false;
    bool success = false;
    SDL_Event event;
    while (!decided) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                GameManager::getInstance().Quit();
                return false;
            }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_ESCAPE) {
                    success = false;
                    decided = true;
                } else if (event.key.key == SDLK_RETURN || event.key.key == SDLK_SPACE) {
                    success = ((std::rand() % 100) < chance);
                    decided = true;
                }
            }
        }
        SDL_RenderClear(m_renderer);
        if (m_texMenuBackground) SDL_RenderTexture(m_renderer, m_texMenuBackground, NULL, NULL);
        DrawRectangle(100, 100, 440, 220, 0, 0xFFFFFFFF, 40);
        DrawShadowTextUtf8(titleUtf8, 120, 120, 0xFFFF00FF, 0x000000FF);
        DrawShadowTextUtf8(hintUtf8, 120, 160, 0xFFFFFFFF, 0x000000FF);
        DrawShadowTextUtf8(" 成功率約 " + std::to_string(chance) + "%", 120, 200, 0xAAAAAAAA, 0x000000FF);
        DrawShadowTextUtf8(" 空格/回車：挑戰　　ESC：放棄(失敗)", 120, 250, 0xAAAAAAFF, 0x000000FF);
        VirtualControls::present(m_renderer);
        SDL_Delay(16);
    }
    ShowDialogue(success ? " 挑戰成功！" : " 挑戰失敗…", -1, 0);
    return success;
}
