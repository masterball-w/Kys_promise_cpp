#pragma once
#include <vector>
#include <memory>
#include <string>
#include <SDL3/SDL.h>
#include <array>
#include <cstdint>
#include "Role.h"
#include "Item.h"
#include "Scene.h"
#include "Magic.h"
#include "PicLoader.h"
#include "BattleEffects.h"

class GameManager {
    // Global Variables (x50 array from Pascal)
    // Range: -32768 to 32767 (mapped to 0..65535)
    std::vector<int16_t> m_x50;

    // MMap Data (480x480)
    std::vector<int16_t> m_earth;
    std::vector<int16_t> m_surface;
    std::vector<int16_t> m_building;
    std::vector<int16_t> m_buildX;
    std::vector<int16_t> m_buildY;
    std::vector<int16_t> m_entrance;

public:
    static GameManager& getInstance();

    // Check if a role has a specific battle effect (via Equipment or Gongti)
    bool CheckBattleEffect(int roleIdx, BattleEffectType type);
    bool CheckBattleEffect(int roleIdx, int stateId); // Overload for raw ID

    // MMap Accessors
    const std::vector<int16_t>& getEarth() const { return m_earth; }
    const std::vector<int16_t>& getSurface() const { return m_surface; }
    const std::vector<int16_t>& getBuilding() const { return m_building; }
    const std::vector<int16_t>& getBuildX() const { return m_buildX; }
    const std::vector<int16_t>& getBuildY() const { return m_buildY; }
    const std::vector<int16_t>& getEntrance() const { return m_entrance; }

    void reSetEntrance();

    int16_t getX50(int index) const {
        if (index < -32768 || index > 32767) return 0;
        return m_x50[index + 32768];
    }

    void setX50(int index, int16_t value) {
        if (index < -32768 || index > 32767) return;
        m_x50[index + 32768] = value;
    }

    // Pascal pansichar(@x50[start]): little-endian bytes packed 2-per-int16
    std::string getX50String(int startIndex) const {
        std::string result;
        for (int bi = 0; bi < 512; ++bi) {
            int wordIdx = startIndex + bi / 2;
            if (wordIdx < -32768 || wordIdx > 32767) break;
            int16_t val = m_x50[wordIdx + 32768];
            uint8_t b = (bi % 2 == 0)
                ? static_cast<uint8_t>(val & 0xFF)
                : static_cast<uint8_t>((val >> 8) & 0xFF);
            if (b == 0) break;
            result.push_back(static_cast<char>(b));
        }
        return result;
    }

    void setX50String(int startIndex, const std::string& str) {
        // Write chars + trailing NUL (Pascal pansichar layout)
        size_t n = str.size();
        if (n > 511) n = 511;
        for (size_t i = 0; i <= n; ++i) {
            uint8_t b = (i < n) ? static_cast<uint8_t>(str[i]) : 0;
            int wordIdx = startIndex + static_cast<int>(i) / 2;
            if (wordIdx < -32768 || wordIdx > 32767) break;
            int16_t cur = m_x50[wordIdx + 32768];
            if ((i % 2) == 0) {
                m_x50[wordIdx + 32768] = static_cast<int16_t>((cur & 0xFF00) | b);
            } else {
                m_x50[wordIdx + 32768] = static_cast<int16_t>((cur & 0x00FF) | (b << 8));
            }
        }
    }

    int getX50StringLength(int startIndex) const {
        return static_cast<int>(getX50String(startIndex).size());
    }

    void setTeamMember(int slot, int roleId) {
        if (slot >= 0 && slot < 6) {
            if (m_teamList.size() <= (size_t)slot) {
                m_teamList.resize(slot + 1, -1);
            }
            m_teamList[slot] = roleId;
        }
    }

    int getTeamMember(int slot) const {
        if (slot >= 0 && slot < (int)m_teamList.size()) {
            return m_teamList[slot];
        }
        return -1;
    }

    bool Init();
    void Run();
    void Quit();

    // Data Access
    Role& getRole(int index);
    int getRoleCount() const { return (int)m_roles.size(); }
    Item& getItem(int index);
    int getItemCount() const { return (int)m_items.size(); }
    Scene& getScene(int index);
    Magic& getMagic(int index);
    int getMagicCount() const { return (int)m_magics.size(); }
    PicImage* getHead(int index); // Get cached head image

    // Shop (WeiShop / RShop): 18 int16 per shop from ranger.grp tail
    int getShopCount() const { return (int)m_shops.size(); }
    int16_t getShopData(int shopId, int index) const;
    void setShopData(int shopId, int index, int16_t value);
    void RebuildShopsFromRaw();
    
    // Global State Helpers
    void setMainMapPosition(int x, int y);
    void setCameraPosition(int x, int y); // Separate Camera Position
    
    // Render Helper
    SDL_Surface* getScreenSurface() { return m_screenSurface; }
    void RenderScreenTo(SDL_Renderer* renderer);
    void getMainMapPosition(int& x, int& y) const { x = m_mainMapX; y = m_mainMapY; }
    void getCameraPosition(int& x, int& y) const { x = m_cameraX; y = m_cameraY; }
    void getSavedWorldPosition(int& x, int& y) const { x = m_savedWorldX; y = m_savedWorldY; }
    void setSavedWorldPosition(int x, int y) { m_savedWorldX = x; m_savedWorldY = y; }
    void setMainMapFace(int face) { m_mainMapFace = face; }
    int getMainMapFace() const { return m_mainMapFace; }
    bool getShowMR() const { return m_showMR; }
    void setShowMR(bool show) { m_showMR = show; }
    void setSubMapFace(int16_t face) { m_subMapFace = face; }
    int16_t getSubMapFace() const { return m_subMapFace; }
    void getFacingTile(int& x, int& y) const;
    int getGameSpeed() const { return m_gameSpeed; }
    void setGameSpeed(int speed) { m_gameSpeed = std::max(1, speed); }
    int16_t getInShip() const { return m_inShip; }
    int getBattleMode() const { return m_battleMode; }
    void setBattleMode(int mode) { m_battleMode = mode; }
    int16_t getGameTime() const { return m_gameTime; }
    void setGameTime(int16_t time) { m_gameTime = time; }
    void ReturnToTitleScreen();
    int getMaxLevel() const;
    
    // Walk Animation
    // KYS sprites typically have 6 or 7 frames.
    // Frame 0 is usually standing.
    // Frames 1-6 are walking cycle.
    // Or 0-5.
    // If user sees "abnormal frame", maybe 7 is too many and it wraps into next sprite?
    // Let's try reducing to 6 frames (0..5).
    void updateWalkFrame() { m_walkFrame = (m_walkFrame + 1) % 6; } 
    void resetWalkFrame() { m_walkFrame = 0; }
    int getWalkFrame() const { return m_walkFrame; }

    void enterScene(int sceneId);
    int getCurrentSceneId() const { return m_currentSceneId; }
    
    // Screen Access
    SDL_Renderer* getRenderer() { return m_renderer; }

    // Item Usage
    void EatOneItem(int roleNum, int itemId, int where = 0);
    int GetRoleMedcine(int roleNum, bool checkEquip = true); // Helper for medicine power
    
    // Inventory & Party
    void AddItem(int itemId, int amount);
    void useItem(int itemId);
    int getItemAmount(int itemId);
    const std::vector<InventoryItem>& getItemList() const { return m_inventory; }
    void ensureInventorySize();
    void setInventorySlot(int slot, int16_t number, int16_t amount);
    InventoryItem getInventorySlot(int slot) const;

    // instruct_50e memory-mapped globals (Pascal AutoRefresh / CurItem / where …)
    int getAutoRefresh() const { return m_autoRefresh; }
    void setAutoRefresh(int v) { m_autoRefresh = v; }
    int getCurItem() const { return m_curItem; }
    void setCurItem(int v) { m_curItem = v; }
    int getCurMagic() const { return m_curMagic; }
    void setCurMagic(int v) { m_curMagic = v; }
    int getWhere() const { return m_where; }
    void setWhere(int v) { m_where = v; }

    // Scene player (Sx/Sy) without forcing camera; camera is Cx/Cy
    void setScenePlayerPosition(int x, int y) { m_mainMapX = x; m_mainMapY = y; }
    void setScenePlayerX(int x) { m_mainMapX = x; }
    void setScenePlayerY(int y) { m_mainMapY = y; }
    void JoinParty(int roleId);
    void LeaveParty(int roleId);
    void Rest();
    
    // Accessors
    const std::vector<int>& getTeamList() const { return m_teamList; }

    // Helper to load initial data
    void loadData(const std::string& savePrefix);

    // Save Path
    std::string m_savePath;
    
    int getNextLevelExp(int level);

    // Save/Load
    void SaveGame(int slot);
    bool LoadGame(int slot);

    // Game Logic
    void LearnMagic(int roleIdx, int magicIdx, int mode);
    bool GetEquipState(int roleIdx, int state);
    
    // Attribute Helpers
    int GetGongtiLevel(int roleIdx, int magicId);
    bool GetGongtiState(int roleIdx, int state);
    void SetGongti(int roleIdx, int magicId);
    int GetRoleAttack(int roleIdx, bool checkEquip = true);
    int GetRoleDefence(int roleIdx, bool checkEquip = true);
    int GetRoleSpeed(int roleIdx, bool checkEquip = true);
    int GetRoleMedPoi(int roleIdx, bool checkEquip = true);
    int GetRoleUsePoi(int roleIdx, bool checkEquip = true);
    int GetRoleDefPoi(int roleIdx, bool checkEquip = true);
    int GetRoleFist(int roleIdx, bool checkEquip = true);
    int GetRoleSword(int roleIdx, bool checkEquip = true);
    int GetRoleKnife(int roleIdx, bool checkEquip = true);
    int GetRoleUnusual(int roleIdx, bool checkEquip = true);
    int GetRoleHidWeapon(int roleIdx, bool checkEquip = true);
    int CheckEquipSet(int e0, int e1, int e2, int e3);

    // Testing Helpers
    void addMagicForTest(const Magic& m) { m_magics.push_back(m); }
    void addRoleForTest(const Role& r) { m_roles.push_back(r); }
    void clearDataForTest() { m_magics.clear(); m_roles.clear(); m_items.clear(); }

private:
    GameManager();
    ~GameManager(); // Changed to non-default to handle SDL cleanup
    GameManager(const GameManager&) = delete;
    GameManager& operator=(const GameManager&) = delete;

    // Game Data
    std::vector<Role> m_roles;
    std::vector<Item> m_items;
    // std::vector<Scene> m_scenes; // Moved to SceneManager
    std::vector<Magic> m_magics;
    std::vector<PicImage> m_heads; // Cached head images
    
    std::vector<int> m_teamList; // Stores role IDs of current party members
    std::vector<InventoryItem> m_inventory;
    std::vector<uint8_t> m_shopRaw;
    std::vector<std::array<int16_t, 18>> m_shops;
    std::vector<int16_t> m_levelUpList;
    std::vector<std::array<int16_t, 4>> m_setNum;
    
    // Runtime State
    enum class GameState {
        TitleScreen,
        CharacterCreation,
        Roaming,
        Battle,
        SystemMenu,
        InventoryMenu
    };
    GameState m_currentState = GameState::TitleScreen;

    // Character creation: name first, then attribute roll (Pascal InitialRole / RandomAttribute)
    enum class CharCreatePhase {
        NameInput,
        AttributeSelect
    };
    CharCreatePhase m_charCreatePhase = CharCreatePhase::NameInput;

    bool m_isRunning;
    int m_currentSceneId;
    int m_mainMapX, m_mainMapY;
    int m_savedWorldX = 0, m_savedWorldY = 0; // Backup for world coordinates when entering a scene
    int m_cameraX, m_cameraY;
    int m_mainMapFace = 0;
    int m_gameSpeed = 10;
    int m_battleMode = 0;
    int m_walkFrame = 0;
    int16_t m_inShip = 0;
    int16_t m_shipX = 0, m_shipY = 0, m_shipFace = 0;
    int16_t m_subMapFace = 0;
    bool m_showMR = true; // Pascal ShowMR: hide protagonist during new-game opening
    int16_t m_time = 0, m_timeEvent = 0, m_randomEvent = 0;
    int16_t m_gameTime = 0;
    int m_autoRefresh = 0;
    int m_curItem = -1;
    int m_curMagic = -1;
    int m_where = 0; // 0=world, 1=scene, 2=battle (Pascal where)
    bool m_playedTitleAnim = false;
    uint32_t m_moveHoldStart = 0;
    uint32_t m_lastMoveTick = 0;
    int m_holdDx = 0;
    int m_holdDy = 0;
    
    // System Menu State
    int m_systemMenuSelection = 0;

    // SDL Resources
    SDL_Window* m_window;
    SDL_Renderer* m_renderer;
    SDL_Surface* m_screenSurface;
    SDL_Texture* m_screenTexture;

    // Helper for Character Creation
    std::string m_characterCreationNameUtf8;
    bool m_characterCreationTextInputActive = false;
    void RandomizeRoleStats(Role& role);
    void InitNewGame();
    void PlayNewGameIntro(); // PlayBeginningMovie(26,0) + StartAmi + Event 101
    
    // Title Menu Helpers
    int m_titleMenuSelection = 0;
    void DrawTitleMenu();

    void UpdateTitleScreen();
    void UpdateCharacterCreation();
    void UpdateSystemMenu();
    void UpdateInventoryMenu();
    
public:
    void UpdateRoaming(); // Make public for EventManager to call
private:
    // 大地图行走与入口检测
    bool CanWalkWorld(int x, int y);
    bool CheckWorldEntrance();
    bool TryEnterScene(int sceneId);  // 尝试进入场景
};
