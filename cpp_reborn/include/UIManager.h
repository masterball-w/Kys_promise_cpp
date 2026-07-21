#ifndef UIMANAGER_H
#define UIMANAGER_H

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <string>
#include <vector>
#include <cstdint>
#include <functional>

class Role;

class UIManager {
public:
    static UIManager& getInstance();

    bool Init(SDL_Renderer* renderer, SDL_Window* window);
    void Cleanup();

    // Resource Loading
    bool LoadSystemGraphics(); // Loads Background.Pic and others
    void PlayTitleAnimation(); // Plays Begin.Pic (full forward)
    void PlayBeginningMovie(int beginNum, int endNum); // Align Pascal PlayBeginningMovie
    void DrawTitleScreen();    // Draws Background.Pic index 0
    void DrawTitleBackground(); // Draws the last frame of Begin.Pic / Background.Pic
    void DrawCharacterCreationNamePrompt(const std::string& nameUtf8);
    void DrawCharacterCreationAttributes(const Role& role);

    // Core UI Drawing
    void DrawRectangle(int x, int y, int w, int h, uint32_t colorin, uint32_t colorframe, int alpha);
    void DrawFilledRect(int x, int y, int w, int h, uint32_t color, int alpha);
    void DrawCenteredTexture(SDL_Texture* tex);
    void DrawText(const std::string& text, int x, int y, uint32_t color, int fontSize = 20); // Assumes GBK input
    void DrawTextUtf8(const std::string& text, int x, int y, uint32_t color, int fontSize = 20); // Assumes UTF-8 input
    void DrawShadowText(const std::string& text, int x, int y, uint32_t color1, uint32_t color2, int fontSize = 20); // Assumes GBK
    void DrawShadowTextUtf8(const std::string& text, int x, int y, uint32_t color1, uint32_t color2, int fontSize = 20); // Assumes UTF-8
    
    // Draw Head Portrait
    void DrawHead(int headId, int x, int y, int green = 0, int red = 0, int gray = 0);
    
    // Draw Item Pic with offset (like Pascal's drawPngPic)
    void DrawItemPicWithOffset(int itemId, int x, int y);

    // Character Creation UI
    void ShowCharacterCreation(const Role& role);

    // Menu System
    void ShowMenu(); // Main game menu (Save, Load, etc.) - Blocking
    void RenderMenuSystem(int menuSelection); // Render one frame of menu
    void SelectShowStatus();
    void ShowStatus(int roleId);
    void ShowSimpleStatus(int roleId, int x, int y, int frozen = 0);
    void DrawHpMpStatus(int roleId, int x, int y);
    void DrawEngShadowText(const std::string& text, int x, int y, uint32_t color1, uint32_t color2, int fontSize = 20);
    int ShowBattleItemMenu(const std::function<void()>& redrawBackground);
    void SelectShowMagic();
    void ShowMagic(int roleId, int selectedIndex = -1);
    void SelectShowSystem();
    void ShowSystem(int selectedIndex, int subMenu = -1, int subSelection = 0);
    void ShowSelect(int row, int menu, const std::vector<std::string>& words, int width);
    void SelectShowSkill();
    void ShowSkill(int petId, int selectedIndex);
    void SelectShowTeammate();
    void ShowTeammate(int tMenu, int rMenu, int position);
    void SelectShowItem();
    void ShowItem(int menuSelection, int selectedIndex = -1, bool inSubmenu = false);
    void ShowShop(int shopId);
    // Mini-game prompt: returns true on success. chancePercent 0-100 (boosted by pet skill 4-2).
    // Enter=挑战, Esc=放弃(失败). Does not silently auto-pass.
    bool RunMiniGame(const std::string& titleUtf8, const std::string& hintUtf8, int chancePercent);
    bool ShowSaveLoadMenu(bool isSave); // Shows save/load slots
    // Pascal CommonMenu / CommonScrollMenu — returns 0..n-1, or -1 on cancel
    int CommonMenu(int x, int y, int width, const std::vector<std::string>& itemsUtf8);
    int CommonScrollMenu(int x, int y, int width, const std::vector<std::string>& itemsUtf8, int visibleCount);
    void ShowVolumeMenu();
    void ShowDialogue(const std::string& text, int headId, int mode, const std::string& nameUtf8 = "", const std::string& nameRawBytes = "", int colorIndex = -1);
    void ShowTitle(const std::string& text, int x, int y, uint32_t color1, uint32_t color2);
    void ShowSceneName(int sceneId);
    int ShowChoice(const std::string& text); // Returns 0 for No/Cancel, 1 for Yes/Confirm
    void ShowItemNotification(int itemId, int amount);
    
    int WaitForKeyPress();
    void WaitAnyKey(int* keycode, int* x, int* y);
    
    // Screen Effects
    void FadeScreen(bool fadeIn);
    void FlashScreen(uint32_t color, int durationMs);
    void CaptureScreen(); // Capture current frame for translucent menus

    // Force screen update (for blocking loops)
    void UpdateScreen();

    SDL_Renderer* GetRenderer() const { return m_renderer; }
    SDL_Window* GetWindow() const { return m_window; }

private:
    struct SkillIcon {
        SDL_Texture* tex = nullptr;
        int x = 0;
        int y = 0;
        int w = 0;
        int h = 0;
    };

    UIManager();
    ~UIManager() = default;
    UIManager(const UIManager&) = delete;
    UIManager& operator=(const UIManager&) = delete;

    SDL_Renderer* m_renderer = nullptr;
    SDL_Window* m_window = nullptr;
    TTF_Font* m_font = nullptr;
    TTF_Font* m_fontEnglish = nullptr;

    // Helper to get color from uint32 (AARRGGBB or similar)
    // Pascal code seems to use 32-bit int for color. 
    // Need to verify color format. SDL3 usually uses SDL_Color.
    SDL_Color Uint32ToColor(uint32_t color);

    // System Graphics from Background.Pic
    SDL_Texture* m_texTitle = nullptr;    // Index 0
    SDL_Texture* m_texMagic = nullptr;    // Index 1
    SDL_Texture* m_texState = nullptr;    // Index 2
    SDL_Texture* m_texSystem = nullptr;   // Index 3
    SDL_Texture* m_texMap = nullptr;      // Index 4
    SDL_Texture* m_texSkill = nullptr;    // Index 5
    SDL_Texture* m_texMenuEsc = nullptr;  // Index 6
    SDL_Texture* m_texMenuEscBack = nullptr; // Index 7
    SDL_Texture* m_texBattle = nullptr;   // Index 8
    SDL_Texture* m_texTeammate = nullptr; // Index 9
    SDL_Texture* m_texMenuItem = nullptr; // Index 10
    
    SDL_Texture* m_texBeginBackground = nullptr; // Last frame of Begin.Pic
    SDL_Texture* m_texMenuBackground = nullptr; // Captured screen for transparency
    std::vector<SkillIcon> m_skillIcons;

    // Helper to load texture from surface and free surface
    SDL_Texture* LoadTextureFromPic(const std::string& filename, int index);
    SDL_Texture* SurfaceToTexture(SDL_Surface* surface);
    void EnsureSkillIconsLoaded();
    void ClearSkillIcons();
    int GetRemainingSkillPoints();
    bool InModeMagic(int roleId);
    void ShowMedcine(int healerId, int menu);
    void ShowMedPoision(int healerId, int menu);
    void MenuMedcine(int healerId);
    void MenuMedPoision(int healerId);
    int SelectItemUser(int menuSelection, int selectedIndex, int itemId, int itemType);

    void ShowDialogueWithCapture(const std::string& text, int headId, int mode);

private:
};

#endif // UIMANAGER_H
