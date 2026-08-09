#include "GameManager.h"
#include "SceneManager.h"
#include "UIManager.h"
#include "BattleManager.h"
#include "EventManager.h"
#include "FileLoader.h"
#include "PicLoader.h"
#include "TextManager.h"
#include "GraphicsUtils.h"
#include "InputManager.h"
#include "SoundManager.h"
#include "PlatformCompat.h"
#include "VirtualControls.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <random>
#include <filesystem>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#define getcwd _getcwd
#define access _access
#else
#include <unistd.h>
#endif

namespace {
    void PopBackUtf8(std::string& s) {
        if (s.empty()) return;
        size_t i = s.size() - 1;
        while (i > 0 && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80) {
            --i;
        }
        s.erase(i);
    }

    int sceneFaceToMainMapFace(int sceneFace) {
        switch (sceneFace) {
            case 0: return 1; // North / Up
            case 1: return 3; // East / Right
            case 2: return 2; // West / Left
            case 3: return 0; // South / Down
            default: return 1;
        }
    }

    void applySceneDirection(GameManager& gm, int sceneFace) {
        gm.setSubMapFace(static_cast<int16_t>(sceneFace));
        gm.setMainMapFace(sceneFaceToMainMapFace(sceneFace));
    }

    void applyWorldDirection(GameManager& gm, int dx, int dy) {
        if (dx < 0) gm.setMainMapFace(1);
        else if (dx > 0) gm.setMainMapFace(0);
        else if (dy < 0) gm.setMainMapFace(2);
        else if (dy > 0) gm.setMainMapFace(3);
    }
}

GameManager& GameManager::getInstance() {
    static GameManager instance;
    return instance;
}

GameManager::GameManager() 
    : m_window(nullptr), m_renderer(nullptr), m_screenSurface(nullptr), m_screenTexture(nullptr),
      m_isRunning(false), m_currentSceneId(0), m_mainMapX(0), m_mainMapY(0), 
      m_cameraX(0), m_cameraY(0), m_mainMapFace(0), m_walkFrame(0), m_playedTitleAnim(false),
      m_currentState(GameState::TitleScreen), m_systemMenuSelection(0)
{
    m_x50.resize(65536, 0); // -32768 to 32767 mapped to 0..65535
}

GameManager::~GameManager() {
    Quit();
}

bool GameManager::Init() {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS)) {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return false;
    }

    if (!TTF_Init()) {
        std::cerr << "SDL_ttf could not initialize! SDL_ttf Error: " << SDL_GetError() << std::endl;
        return false;
    }

    m_window = SDL_CreateWindow("KYS Promise (C++ Refactor)", 640, 480, 0); 
    if (!m_window) {
        std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        return false;
    }

    m_renderer = SDL_CreateRenderer(m_window, NULL); // SDL3 defaults are good
    if (!m_renderer) {
        std::cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        return false;
    }
    
    SDL_SetRenderVSync(m_renderer, 1); // Enable VSync

    // Logical resolution
    // SDL3: SDL_SetRenderLogicalPresentation(renderer, w, h, mode)
    SDL_SetRenderLogicalPresentation(m_renderer, 640, 480, SDL_LOGICAL_PRESENTATION_LETTERBOX);

    VirtualControls::init();
    VirtualControls::setRenderer(m_renderer);

    m_screenSurface = SDL_CreateSurface(640, 480, SDL_PIXELFORMAT_ARGB8888);
    m_screenTexture = SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 640, 480);
    if (m_screenTexture) {
        SDL_SetTextureBlendMode(m_screenTexture, SDL_BLENDMODE_NONE);
    }

    if (!UIManager::getInstance().Init(m_renderer, m_window)) {
        std::cerr << "Failed to init UIManager" << std::endl;
        return false;
    }
    loadSettings();

    // ===== 移动逻辑测试 =====
    {
        std::cout << "\n=== 移动逻辑测试 ===" << std::endl;
        int testX = 235, testY = 361;
        int testFace = 0;
        std::cout << "[START] Pos=(" << testX << "," << testY << ")" << std::endl;
        
        // 按三次←
        for (int i = 0; i < 3; i++) {
            int dx = 0, dy = -1;
            testFace = 2;
            testX += dx; testY += dy;
            std::cout << "[LEFT] dx=" << dx << " dy=" << dy << " Face=" << testFace 
                      << " -> Pos=(" << testX << "," << testY << ")" << std::endl;
        }
        
        // 按↓
        {
            int dx = 1, dy = 0;
            testFace = 0;
            testX += dx; testY += dy;
            std::cout << "[DOWN] dx=" << dx << " dy=" << dy << " Face=" << testFace 
                      << " -> Pos=(" << testX << "," << testY << ")" << std::endl;
        }
        
        std::cout << "=== 测试结束 ===\n" << std::endl;
    }
    // =====================

    // Initialize Subsystems
    if (!SceneManager::getInstance().Init()) {
        std::cerr << "Failed to init SceneManager" << std::endl;
        return false;
    }

    if (!EventManager::getInstance().Init()) {
        std::cerr << "Failed to init EventManager" << std::endl;
        return false;
    }

    if (!BattleManager::getInstance().Init()) {
        std::cerr << "Failed to init BattleManager" << std::endl;
        return false;
    }

    // Load MMap Data
    auto loadMMapLayer = [&](const std::string& filename, std::vector<int16_t>& target) {
        std::vector<uint8_t> data = FileLoader::loadFile(filename);
        if (data.size() == 480 * 480 * 2) {
            target.resize(480 * 480);
            memcpy(target.data(), data.data(), data.size());
        } else {
            std::cerr << "Failed to load MMap layer: " << filename << " Size: " << data.size() << std::endl;
            // Fallback to empty
            target.resize(480 * 480, 0);
        }
    };

    loadMMapLayer("resource/earth.002", m_earth);
    loadMMapLayer("resource/surface.002", m_surface);
    loadMMapLayer("resource/building.002", m_building);
    loadMMapLayer("resource/buildx.002", m_buildX);
    loadMMapLayer("resource/buildy.002", m_buildY);
    m_entrance.resize(480 * 480, -1);

    // Find Save Directory (cross-platform)
    std::string savePrefix = FileLoader::getSaveDir();
    // If template ranger is not in writable save dir, fall back to dataRoot/save
    {
        std::string dataSave = FileLoader::getDataRoot() + "save/";
        auto existsRanger = [](const std::string& dir) {
            return access((dir + "ranger.grp").c_str(), 0) == 0
                || access((dir + "Ranger.grp").c_str(), 0) == 0;
        };
        if (!existsRanger(savePrefix) && existsRanger(dataSave)) {
            savePrefix = dataSave;
        }
    }

    m_savePath = savePrefix;
    
    std::cout << "[GameManager] Discovered Save Path: " << savePrefix << std::endl;

    PlatformCompat::installInputCompat();

    // Load initial data
    std::cout << "[GameManager] Loading Scene Data from " << savePrefix << std::endl;
    std::string alldefPath = savePrefix + "alldef.grp";
    if (!SceneManager::getInstance().LoadEventData(alldefPath)) {
         std::cerr << "Failed to load Event Data from " << alldefPath << ", trying resource path 'alldef.grp'..." << std::endl;
         if (!SceneManager::getInstance().LoadEventData("alldef.grp")) {
             std::cerr << "CRITICAL: Failed to load alldef.grp from anywhere!" << std::endl;
         }
    }
    
    std::string allsinPath = savePrefix + "allsin.grp";
    if (!SceneManager::getInstance().LoadMapData(allsinPath)) {
         std::cerr << "Failed to load Map Data from " << allsinPath << ", trying resource path 'allsin.grp'..." << std::endl;
         if (!SceneManager::getInstance().LoadMapData("allsin.grp")) {
             std::cerr << "CRITICAL: Failed to load allsin.grp from anywhere!" << std::endl;
         }
    }

    loadData(savePrefix);

    // Initialize Entrance Map for World Map -> Scene transitions
    reSetEntrance();

    m_currentState = GameState::TitleScreen;
    m_systemMenuSelection = 0;

    m_isRunning = true;
    return true;
}

void GameManager::loadData(const std::string& savePrefix) {
    // KYS loads initial data from "save/ranger.grp" (or similar) when num=0
    // But Pascal code shows:
    // if num = 0 then filename := 'ranger';
    // idx := fileopen(AppPath + 'save/ranger.idx', fmopenread);
    // grp := fileopen(AppPath + 'save/' + filename + '.grp', fmopenread);
    //
    // So for a new game, we should load from "save/ranger.grp".
    // Note: The file might be named "Ranger.grp" (case sensitive on Linux, but likely "ranger.grp" or "Ranger.grp" on Windows)
    
    std::string rolePath = savePrefix + "ranger.grp"; // Try lowercase first as per Pascal
    
    // Check if ranger.grp exists, if not try Ranger.grp
    // Actually FileLoader::loadFile might handle some path logic, but let's be explicit if possible.
    // However, the Pascal code also loads from "save/R1.grp" for slot 1.
    // The initial data seems to be in "ranger.grp".
    
    // Let's use FileLoader to load the whole file first, then parse it.
    // Unlike individual .grp files for Roles/Items (which don't seem to exist separately in this version),
    // KYS stores EVERYTHING in one big save file (ranger.grp / R1.grp).
    // Structure:
    // Header (InShip, Where, Mx, My... ~100 bytes?)
    // TeamList
    // Items
    // Roles
    // Items (Wait, RItem is separate from RItemList?)
    // Scenes
    // Magics
    // Shops
    
    // We need to match this structure to load data correctly!
    // Simply loading "role.grp" was wrong because that file doesn't exist.
    // We must load "ranger.grp" and seek to the correct offsets.
    // Offsets are stored in "ranger.idx".
    
    std::string idxPath = savePrefix + "ranger.idx";
    
    // Direct load (bypass FileLoader which enforces resource path)
    std::ifstream idxFile(idxPath, std::ios::binary | std::ios::ate);
    if (!idxFile) {
         std::cerr << "Failed to open " << idxPath << std::endl;
         return;
    }
    std::streamsize idxSize = idxFile.tellg();
    idxFile.seekg(0, std::ios::beg);
    std::vector<uint8_t> idxData(idxSize);
    idxFile.read((char*)idxData.data(), idxSize);
    
    if (idxData.size() < 24) { // At least 6 integers * 4 bytes
        std::cerr << "Failed to load ranger.idx or invalid size" << std::endl;
        return;
    }
    
    const int32_t* idxPtr = reinterpret_cast<const int32_t*>(idxData.data());
    int RoleOffset = idxPtr[0];
    int ItemOffset = idxPtr[1];
    int SceneOffset = idxPtr[2];
    int MagicOffset = idxPtr[3];
    int WeiShopOffset = idxPtr[4];
    int TotalLen = idxPtr[5];
    
    std::cout << "Ranger IDX Offsets: Role=" << RoleOffset 
              << ", Item=" << ItemOffset 
              << ", Scene=" << SceneOffset 
              << ", Magic=" << MagicOffset << std::endl;
              
    std::ifstream grpFile(rolePath, std::ios::binary | std::ios::ate);
    if (!grpFile) {
        // Try capitalized "Ranger.grp"
        rolePath = savePrefix + "Ranger.grp";
        grpFile.open(rolePath, std::ios::binary | std::ios::ate);
    }
    
    if (!grpFile) {
        std::cerr << "Failed to load ranger.grp" << std::endl;
        return;
    }
    
    std::streamsize grpSize = grpFile.tellg();
    grpFile.seekg(0, std::ios::beg);
    std::vector<uint8_t> saveBytes(grpSize);
    grpFile.read((char*)saveBytes.data(), grpSize);
    
    const uint8_t* dataPtr = saveBytes.data();
    
    // 0. Load Header (Global State)
    if (RoleOffset > 0) {
        // Assume Header is at the beginning
        // KYS Save Header structure (Partial, based on Pascal code):
        // Offset 0: InShip (2)
        // Offset 2: InSubMap (2) -> Current Scene ID
        // Offset 4: MainMapX (2)
        // Offset 6: MainMapY (2)
        // Offset 8: MainMapFace (2)
        
        if (RoleOffset >= 22) {
             m_currentSceneId = *(int16_t*)(dataPtr + 2);
             // 注意：ranger.grp 文件格式中，offset 4 是 My (Y坐标)，offset 6 是 Mx (X坐标)
             // 这与直觉相反，必须与 LoadGame 函数中的读取顺序保持一致
             m_mainMapY = *(int16_t*)(dataPtr + 4);
             m_mainMapX = *(int16_t*)(dataPtr + 6);
             m_mainMapFace = *(int16_t*)(dataPtr + 8);
             
             // Sync SceneManager
             SceneManager::getInstance().SetCurrentScene(m_currentSceneId);
             
             // Check for invalid scene ID (0 is sometimes valid, but usually main menu or test)
             // If Scene ID is 0, it might be uninitialized or wrong.
             // But KYS Scene 0 is a valid scene (usually).
             
             // Fallback: If Scene ID is 0 and X/Y are 0, it's likely a bad header read.
             if (m_currentSceneId == 0 && m_mainMapX == 0 && m_mainMapY == 0) {
                 std::cerr << "WARNING: Loaded suspicious initial state (Scene 0, 0,0). Ranger.grp might be empty/invalid." << std::endl;
                 // Force fallback to known start if user is stuck?
                 // But user said "New Game enters Mongolian Tent" before.
                 // Maybe we should trust it unless it's strictly 0,0,0.
             }
             
             std::cout << "[loadData] Header Loaded: Scene=" << m_currentSceneId 
                       << " Pos=(" << m_mainMapX << "," << m_mainMapY << ")" 
                       << " Face=" << m_mainMapFace << std::endl;
                       
             // Load TeamList
             // TeamList starts at offset 30 (after gametime at offset 28)
             // Pascal header structure:
             //   Inship(0), where(2), My(4), Mx(6), Sy(8), Sx(10), Mface(12), 
             //   shipx(14), shipy(16), time(18), timeevent(20), randomevent(22), 
             //   Sface(24), shipface(26), gametime(28), teamlist(30)
             // KYS TeamList size: 6 (array[0..5] of smallint -> 12 bytes)
             const int TEAM_LIST_OFFSET = 30;
             int16_t* teamSrc = (int16_t*)(dataPtr + TEAM_LIST_OFFSET);
             m_teamList.assign(MAX_TEAM_SIZE, -1); // Reset
             for(int i=0; i<MAX_TEAM_SIZE; ++i) {
                 if (TEAM_LIST_OFFSET + i*2 < RoleOffset) {
                     m_teamList[i] = teamSrc[i];
                 }
             }
             std::cout << "[loadData] TeamList at offset " << TEAM_LIST_OFFSET << ": " << m_teamList[0] << ", " << m_teamList[1] << "..." << std::endl;

             // Inventory follows team list at offset 42 (Pascal Ritemlist)
             const int INV_OFFSET = 42;
             int invSlots = (RoleOffset > INV_OFFSET) ? (RoleOffset - INV_OFFSET) / 4 : 0;
             if (invSlots > MAX_ITEM_AMOUNT) invSlots = MAX_ITEM_AMOUNT;
             m_inventory.assign(MAX_ITEM_AMOUNT, InventoryItem{-1, 0});
             for (int i = 0; i < invSlots; ++i) {
                 const int off = INV_OFFSET + i * 4;
                 if (off + 4 <= RoleOffset) {
                     m_inventory[i].id = *(int16_t*)(dataPtr + off);
                     m_inventory[i].amount = *(int16_t*)(dataPtr + off + 2);
                 }
             }
        }
    }
    
    // 1. Load Roles
    // Size = ItemOffset - RoleOffset
    if (ItemOffset > RoleOffset && ItemOffset <= saveBytes.size()) {
        int roleDataSize = ItemOffset - RoleOffset;
        const int bytesPerRole = static_cast<int>(ROLE_DATA_SIZE * sizeof(int16));
        int numRoles = (bytesPerRole > 0) ? (roleDataSize / bytesPerRole) : 0;
        m_roles.resize(numRoles);
        
        const int16_t* roleSrc = reinterpret_cast<const int16_t*>(dataPtr + RoleOffset);
        for (int i = 0; i < numRoles; ++i) {
            m_roles[i].loadFromBuffer(roleSrc + static_cast<size_t>(i) * ROLE_DATA_SIZE, ROLE_DATA_SIZE);
        }
        std::cout << "Loaded " << numRoles << " roles from ranger.grp" << std::endl;
    }
    
    // 2. Load Items
    // Size = SceneOffset - ItemOffset
    if (SceneOffset > ItemOffset && SceneOffset <= saveBytes.size()) {
        int itemDataSize = SceneOffset - ItemOffset;
        const int bytesPerItem = static_cast<int>(ITEM_DATA_SIZE * sizeof(int16));
        int numItems = (bytesPerItem > 0) ? (itemDataSize / bytesPerItem) : 0;
        m_items.resize(numItems);
        
        const int16_t* itemSrc = reinterpret_cast<const int16_t*>(dataPtr + ItemOffset);
        for (int i = 0; i < numItems; ++i) {
            m_items[i].loadFromBuffer(itemSrc + static_cast<size_t>(i) * ITEM_DATA_SIZE, ITEM_DATA_SIZE);
        }
        std::cout << "Loaded " << numItems << " items from ranger.grp" << std::endl;
    }
    
    // 3. Load Magics
    // Size = WeiShopOffset - MagicOffset
    if (WeiShopOffset > MagicOffset && WeiShopOffset <= saveBytes.size()) {
        int magicDataSize = WeiShopOffset - MagicOffset;
        const int bytesPerMagic = static_cast<int>(MAGIC_DATA_SIZE * sizeof(int16));
        int numMagics = (bytesPerMagic > 0) ? (magicDataSize / bytesPerMagic) : 0;
        m_magics.resize(numMagics);
        
        const int16_t* magicSrc = reinterpret_cast<const int16_t*>(dataPtr + MagicOffset);
        for (int i = 0; i < numMagics; ++i) {
            m_magics[i].loadFromBuffer(magicSrc + static_cast<size_t>(i) * MAGIC_DATA_SIZE, MAGIC_DATA_SIZE);
        }
        std::cout << "Loaded " << numMagics << " magics from ranger.grp" << std::endl;
    }

    // 3b. Load Shops (WeiShop) — TShop = 18 * int16
    if (TotalLen > WeiShopOffset && WeiShopOffset >= 0 && TotalLen <= (int)saveBytes.size()) {
        int shopBytes = TotalLen - WeiShopOffset;
        m_shopRaw.assign(dataPtr + WeiShopOffset, dataPtr + WeiShopOffset + shopBytes);
        RebuildShopsFromRaw();
        std::cout << "Loaded " << m_shops.size() << " shops from ranger.grp" << std::endl;
    }
    
    // 4. Load Scenes
    // Size = MagicOffset - SceneOffset
    // NOTE: In Pascal: RScene: array of TScene;
    // TScene size is 26 * 2 = 52 bytes.
    if (MagicOffset > SceneOffset && MagicOffset <= saveBytes.size()) {
         int sceneDataSize = MagicOffset - SceneOffset;
         const int bytesPerScene = static_cast<int>(SCENE_DATA_SIZE * sizeof(int16));
         int numScenes = (bytesPerScene > 0) ? (sceneDataSize / bytesPerScene) : 0;
         std::vector<Scene> scenes(numScenes);
         
         const int16_t* sceneSrc = reinterpret_cast<const int16_t*>(dataPtr + SceneOffset);
         for (int i = 0; i < numScenes; ++i) {
             scenes[i].loadFromBuffer(sceneSrc + static_cast<size_t>(i) * SCENE_DATA_SIZE, SCENE_DATA_SIZE);
         }
         SceneManager::getInstance().SetScenes(scenes);
         std::cout << "Loaded " << numScenes << " scenes from ranger.grp. Data Size: " << sceneDataSize << " bytes." << std::endl;
    }

    {
        std::string levelPath = m_savePath + "list\\levelup.bin";
        std::ifstream levelFile(levelPath, std::ios::binary | std::ios::ate);
        if (!levelFile) {
            levelPath = "cpp_reborn\\build\\Debug\\list\\levelup.bin";
            levelFile.open(levelPath, std::ios::binary | std::ios::ate);
        }
        if (levelFile) {
            std::streamsize sz = levelFile.tellg();
            levelFile.seekg(0, std::ios::beg);
            std::vector<uint8_t> buf(sz);
            levelFile.read((char*)buf.data(), sz);
            if (sz >= 200) {
                m_levelUpList.resize(100);
                memcpy(m_levelUpList.data(), buf.data(), 200);
                std::cout << "[loadData] Loaded levelup.bin (" << sz << " bytes)" << std::endl;
            } else {
                std::cerr << "[loadData] levelup.bin size unexpected: " << sz << std::endl;
            }
        } else {
            std::cerr << "Failed to load levelup.bin from any known path" << std::endl;
        }
    }

    {
        std::string setPath = m_savePath + "list\\Set.bin";
        std::ifstream setFile(setPath, std::ios::binary | std::ios::ate);
        if (!setFile) {
            setPath = "cpp_reborn\\build\\Debug\\list\\Set.bin";
            setFile.open(setPath, std::ios::binary | std::ios::ate);
        }
        if (setFile) {
            std::streamsize sz = setFile.tellg();
            setFile.seekg(0, std::ios::beg);
            std::vector<uint8_t> buf(sz);
            setFile.read((char*)buf.data(), sz);
            if (sz >= 40) {
                m_setNum.assign(6, { -1, -1, -1, -1 });
                const int16_t* src = reinterpret_cast<const int16_t*>(buf.data());
                for (int i = 1; i <= 5; ++i) {
                    for (int j = 0; j < 4; ++j) {
                        m_setNum[i][j] = src[(i - 1) * 4 + j];
                    }
                }
                std::cout << "[loadData] Loaded Set.bin (" << sz << " bytes)" << std::endl;
            } else {
                std::cerr << "[loadData] Set.bin size unexpected: " << sz << std::endl;
            }
        } else {
            std::cerr << "Failed to load Set.bin from any known path" << std::endl;
        }
    }
}

int GameManager::getNextLevelExp(int level) {
    if (level <= 0) return 0;
    if (m_levelUpList.empty()) return 0;
    int idx = level - 1;
    if (idx < 0) return 0;
    if (idx >= (int)m_levelUpList.size()) return -1;
    return (int)m_levelUpList[idx];
}

int GameManager::getMaxLevel() const {
    int baseMaxLevel = 30;
    int gameTime = m_gameTime;
    if (gameTime <= 0) {
        return baseMaxLevel;
    }
    return baseMaxLevel + gameTime * 5;
}
void GameManager::SaveGame(int slot) {
    std::string filename = (slot == 0) ? "ranger" : "R" + std::to_string(slot);
    std::string grpPath = m_savePath + filename + ".grp";
    std::string idxPath = m_savePath + "ranger.idx";

    std::ifstream idxFile(idxPath, std::ios::binary);
    if (!idxFile) {
        std::cerr << "SaveGame: Cannot open " << idxPath << std::endl;
        return;
    }

    int32_t RoleOffset, ItemOffset, SceneOffset, MagicOffset, WeiShopOffset, TotalLen;
    idxFile.read((char*)&RoleOffset, 4);
    idxFile.read((char*)&ItemOffset, 4);
    idxFile.read((char*)&SceneOffset, 4);
    idxFile.read((char*)&MagicOffset, 4);
    idxFile.read((char*)&WeiShopOffset, 4);
    idxFile.read((char*)&TotalLen, 4);
    idxFile.close();

    std::ofstream grpFile(grpPath, std::ios::binary);
    if (!grpFile) {
        std::cerr << "SaveGame: Cannot create " << grpPath << std::endl;
        return;
    }

    auto write16 = [&](int16_t val) {
        grpFile.write(reinterpret_cast<const char*>(&val), 2);
    };

    // Pascal save header structure (must match LoadR exactly):
    // Inship(0), where(2), My(4), Mx(6), Sy(8), Sx(10), Mface(12), 
    // shipx(14), shipy(16), time(18), timeevent(20), randomevent(22), 
    // Sface(24), shipface(26), gametime(28), teamlist(30)
    
    // where: if 0, write -1 (useless1), otherwise write current scene
    int16_t whereVal = (m_currentSceneId <= 0) ? static_cast<int16_t>(-1) : static_cast<int16_t>(m_currentSceneId);
    
    write16(m_inShip);           // offset 0: Inship
    write16(whereVal);           // offset 2: where (or -1 if scene 0)
    write16(static_cast<int16_t>(m_mainMapY));  // offset 4: My
    write16(static_cast<int16_t>(m_mainMapX));  // offset 6: Mx
    write16(static_cast<int16_t>(m_cameraY));   // offset 8: Sy (camera Y)
    write16(static_cast<int16_t>(m_cameraX));   // offset 10: Sx (camera X)
    write16(static_cast<int16_t>(m_mainMapFace)); // offset 12: Mface
    write16(m_shipX);            // offset 14: shipx
    write16(m_shipY);            // offset 16: shipy
    write16(m_time);             // offset 18: time
    write16(m_timeEvent);        // offset 20: timeevent
    write16(m_randomEvent);      // offset 22: randomevent
    write16(static_cast<int16_t>(m_subMapFace)); // offset 24: Sface
    write16(m_shipFace);         // offset 26: shipface
    write16(m_gameTime);         // offset 28: gametime
    
    // offset 30: teamlist (6 entries)
    for (int i = 0; i < MAX_TEAM_SIZE; ++i) {
        int16_t value = -1;  // Default to -1 (empty slot)
        if (i < static_cast<int>(m_teamList.size())) {
            value = static_cast<int16_t>(m_teamList[i]);
        }
        write16(value);
    }

    int inventoryCount = static_cast<int>(m_inventory.size());
    for (int i = 0; i < MAX_ITEM_AMOUNT; ++i) {
        int16_t id = 0;
        int16_t amount = 0;
        if (i < inventoryCount) {
            id = m_inventory[i].id;
            amount = m_inventory[i].amount;
        }
        write16(id);
        write16(amount);
    }

    std::streampos headerEnd = grpFile.tellp();
    if (headerEnd < RoleOffset) {
        int padSize = static_cast<int>(RoleOffset - headerEnd);
        std::vector<char> padding(padSize, 0);
        grpFile.write(padding.data(), padSize);
    } else if (headerEnd > RoleOffset) {
        std::cerr << "SaveGame: Header exceeds RoleOffset, truncating to RoleOffset." << std::endl;
        grpFile.seekp(RoleOffset, std::ios::beg);
    }

    // 1. Write Roles
    int roleBytesToWrite = ItemOffset - RoleOffset;
    int roleBytesWritten = 0;
    for (const auto& role : m_roles) {
        if (roleBytesWritten + ROLE_DATA_SIZE * 2 <= roleBytesToWrite) {
            grpFile.write((char*)role.getRawData(), ROLE_DATA_SIZE * 2);
            roleBytesWritten += ROLE_DATA_SIZE * 2;
        } else break;
    }
    while (roleBytesWritten < roleBytesToWrite) {
        char zero = 0;
        grpFile.write(&zero, 1);
        roleBytesWritten++;
    }

    // 2. Write Items
    int itemBytesToWrite = SceneOffset - ItemOffset;
    int itemBytesWritten = 0;
    for (const auto& item : m_items) {
        if (itemBytesWritten + ITEM_DATA_SIZE * 2 <= itemBytesToWrite) {
            grpFile.write((char*)item.getRawData(), ITEM_DATA_SIZE * 2);
            itemBytesWritten += ITEM_DATA_SIZE * 2;
        } else break;
    }
    while (itemBytesWritten < itemBytesToWrite) {
        char zero = 0;
        grpFile.write(&zero, 1);
        itemBytesWritten++;
    }

    // 3. Write Scenes
    int sceneBytesToWrite = MagicOffset - SceneOffset;
    int sceneBytesWritten = 0;
    const auto& scenes = SceneManager::getInstance().getScenes();
    for (const auto& scene : scenes) {
        if (sceneBytesWritten + SCENE_DATA_SIZE * 2 <= sceneBytesToWrite) {
            grpFile.write((char*)scene.getRawData(), SCENE_DATA_SIZE * 2);
            sceneBytesWritten += SCENE_DATA_SIZE * 2;
        } else break;
    }
    while (sceneBytesWritten < sceneBytesToWrite) {
        char zero = 0;
        grpFile.write(&zero, 1);
        sceneBytesWritten++;
    }

    // 4. Write Magics
    int magicBytesToWrite = WeiShopOffset - MagicOffset;
    int magicBytesWritten = 0;
    for (const auto& magic : m_magics) {
        if (magicBytesWritten + MAGIC_DATA_SIZE * 2 <= magicBytesToWrite) {
            grpFile.write((char*)magic.getRawData(), MAGIC_DATA_SIZE * 2);
            magicBytesWritten += MAGIC_DATA_SIZE * 2;
        } else break;
    }
    while (magicBytesWritten < magicBytesToWrite) {
        char zero = 0;
        grpFile.write(&zero, 1);
        magicBytesWritten++;
    }

    // 5. Write Shops (WeiShop) — sync structured shops into raw first
    if (!m_shops.empty()) {
        m_shopRaw.resize(m_shops.size() * 18 * sizeof(int16_t));
        for (size_t s = 0; s < m_shops.size(); ++s) {
            std::memcpy(m_shopRaw.data() + s * 36, m_shops[s].data(), 36);
        }
    }
    int shopBytesToWrite = TotalLen - WeiShopOffset;
    if (shopBytesToWrite > 0) {
        if (!m_shopRaw.empty()) {
            // Write preserved shop data, pad/truncate to exact size
            if ((int)m_shopRaw.size() >= shopBytesToWrite) {
                grpFile.write(reinterpret_cast<const char*>(m_shopRaw.data()), shopBytesToWrite);
            } else {
                grpFile.write(reinterpret_cast<const char*>(m_shopRaw.data()), (std::streamsize)m_shopRaw.size());
                int pad = shopBytesToWrite - (int)m_shopRaw.size();
                std::vector<char> zeros(pad, 0);
                grpFile.write(zeros.data(), pad);
            }
        } else {
            std::vector<char> zeros(shopBytesToWrite, 0);
            grpFile.write(zeros.data(), shopBytesToWrite);
        }
    }

    grpFile.close();

    // Slot 0: template snapshot (ranger + allsin + alldef). Autosave uses AUTOSAVE_SLOT (6).
    if (slot == 0) {
        SceneManager::getInstance().SaveMapData(m_savePath + "allsin.grp");
        SceneManager::getInstance().SaveEventData(m_savePath + "alldef.grp");
        std::cout << "Game Saved to Slot 0 (ranger + allsin + alldef)" << std::endl;
        return;
    }

    std::string sFilename = "S" + std::to_string(slot) + ".grp";
    std::string dFilename = "D" + std::to_string(slot) + ".grp";
    
    SceneManager::getInstance().SaveMapData(m_savePath + sFilename);
    SceneManager::getInstance().SaveEventData(m_savePath + dFilename);
    
    std::cout << "Game Saved to Slot " << slot << std::endl;
}

void GameManager::reSetEntrance() {
    // Reset entrance array
    std::fill(m_entrance.begin(), m_entrance.end(), -1);
    
    const auto& scenes = SceneManager::getInstance().getScenes();
    for (size_t i = 0; i < scenes.size(); ++i) {
        const auto& scene = scenes[i];
        
        // Check MainEntrance 1
        int mx1 = scene.getMainEntranceX1();
        int my1 = scene.getMainEntranceY1();
        if (mx1 >= 0 && mx1 < 480 && my1 >= 0 && my1 < 480) {
            m_entrance[mx1 * 480 + my1] = (int16_t)i;
        }
        
        // Check MainEntrance 2
        int mx2 = scene.getMainEntranceX2();
        int my2 = scene.getMainEntranceY2();
        if (mx2 >= 0 && mx2 < 480 && my2 >= 0 && my2 < 480) {
            m_entrance[mx2 * 480 + my2] = (int16_t)i;
        }
    }
    std::cout << "[GameManager] Entrance map reset." << std::endl;
}

bool GameManager::LoadGame(int slot) {
    std::string filename = (slot == 0) ? "ranger" : "R" + std::to_string(slot);
    std::string grpPath = m_savePath + filename + ".grp";
    std::string idxPath = m_savePath + "ranger.idx";

    std::ifstream idxFile(idxPath, std::ios::binary);
    if (!idxFile) {
        std::cerr << "LoadGame: Cannot open " << idxPath << std::endl;
        return false;
    }

    int32_t RoleOffset, ItemOffset, SceneOffset, MagicOffset, WeiShopOffset, TotalLen;
    idxFile.read((char*)&RoleOffset, 4);
    idxFile.read((char*)&ItemOffset, 4);
    idxFile.read((char*)&SceneOffset, 4);
    idxFile.read((char*)&MagicOffset, 4);
    idxFile.read((char*)&WeiShopOffset, 4);
    idxFile.read((char*)&TotalLen, 4);
    idxFile.close();
    
    std::cout << "[LoadGame] Offsets: Role=" << RoleOffset << " Item=" << ItemOffset 
              << " Scene=" << SceneOffset << " Magic=" << MagicOffset << std::endl;

    std::ifstream grpFile(grpPath, std::ios::binary);
    if (!grpFile) {
        if (slot == 0) {
            grpPath = m_savePath + "Ranger.grp";
            grpFile.open(grpPath, std::ios::binary);
        }
        if (!grpFile) {
            std::cerr << "LoadGame: Cannot open " << grpPath << std::endl;
            return false;
        }
    }
    std::cout << "[LoadGame] Opened " << grpPath << std::endl;

    auto read16 = [&](int16_t& val) {
        grpFile.read((char*)&val, 2);
    };

    // Pascal header structure (must match SaveR exactly):
    // Inship(0), where(2), My(4), Mx(6), Sy(8), Sx(10), Mface(12), 
    // shipx(14), shipy(16), time(18), timeevent(20), randomevent(22), 
    // Sface(24), shipface(26), gametime(28), teamlist(30)
    
    read16(m_inShip);           // offset 0: Inship
    int16_t tempWhere;
    read16(tempWhere);          // offset 2: where
    if (tempWhere < 0) {
        m_currentSceneId = -1;
    } else {
        m_currentSceneId = tempWhere;
    }
    
    int16_t tempMy, tempMx, tempSy, tempSx;
    read16(tempMy);             // offset 4: My
    read16(tempMx);             // offset 6: Mx
    read16(tempSy);             // offset 8: Sy
    read16(tempSx);             // offset 10: Sx
    m_mainMapY = tempMy;
    m_mainMapX = tempMx;
    m_cameraY = tempSy;
    m_cameraX = tempSx;
    
    read16((int16_t&)m_mainMapFace); // offset 12: Mface
    read16(m_shipX);            // offset 14: shipx
    read16(m_shipY);            // offset 16: shipy
    read16(m_time);             // offset 18: time
    read16(m_timeEvent);        // offset 20: timeevent
    read16(m_randomEvent);      // offset 22: randomevent
    read16(m_subMapFace);       // offset 24: Sface
    read16(m_shipFace);         // offset 26: shipface
    read16(m_gameTime);         // offset 28: gametime

    std::cout << "[LoadGame] Header loaded. Scene=" << m_currentSceneId << " Pos=(" << m_mainMapX << "," << m_mainMapY << ")" << std::endl;
    setMainMapPosition(m_mainMapX, m_mainMapY);
    resetWalkFrame();

    // offset 30: teamlist (6 entries)
    // CRITICAL: read into int16 then assign — casting int& to int16_t& only patches low 16 bits.
    // Empty slots are -1 (0xFFFFFFFF); patching 42 into that yields 0xFFFFFF2A (<0), hiding teammates.
    m_teamList.assign(MAX_TEAM_SIZE, -1);
    for (int i = 0; i < MAX_TEAM_SIZE; ++i) {
        int16_t slotId = -1;
        read16(slotId);
        m_teamList[i] = slotId;
    }
    std::cout << "[LoadGame] TeamList:";
    for (int i = 0; i < MAX_TEAM_SIZE; ++i) {
        std::cout << " " << m_teamList[i];
    }
    std::cout << std::endl;

    m_inventory.assign(MAX_ITEM_AMOUNT, InventoryItem{-1, 0});
    const int invSlotsInFile = (RoleOffset > 42) ? (RoleOffset - 42) / 4 : 0;
    const int invSlotsToRead = std::min(invSlotsInFile, MAX_ITEM_AMOUNT);
    for (int i = 0; i < invSlotsToRead; ++i) {
        read16(m_inventory[i].id);
        read16(m_inventory[i].amount);
    }
    
    std::cout << "[LoadGame] Loading Roles..." << std::endl;
    grpFile.seekg(RoleOffset, std::ios::beg);
    int roleSize = (ItemOffset - RoleOffset) / (ROLE_DATA_SIZE * 2);
    if (roleSize < 0 || roleSize > 10000) {
        std::cerr << "[LoadGame] ERROR: Invalid roleSize " << roleSize << std::endl;
        return false;
    }
    m_roles.resize(roleSize);
    for(int i=0; i<roleSize; ++i) {
        std::vector<int16_t> buffer(ROLE_DATA_SIZE);
        grpFile.read((char*)buffer.data(), ROLE_DATA_SIZE * 2);
        m_roles[i].setDataVector(buffer);
    }

    std::cout << "[LoadGame] Loading Items..." << std::endl;
    grpFile.seekg(ItemOffset, std::ios::beg);
    int itemSize = (SceneOffset - ItemOffset) / (ITEM_DATA_SIZE * 2);
    if (itemSize < 0 || itemSize > 10000) {
        std::cerr << "[LoadGame] ERROR: Invalid itemSize " << itemSize << std::endl;
        return false;
    }
    m_items.resize(itemSize);
    for(int i=0; i<itemSize; ++i) {
        std::vector<int16_t> buffer(ITEM_DATA_SIZE);
        grpFile.read((char*)buffer.data(), ITEM_DATA_SIZE * 2);
        m_items[i].setDataVector(buffer);
    }

    std::cout << "[LoadGame] Loading Scenes..." << std::endl;
    grpFile.seekg(SceneOffset, std::ios::beg);
    int sceneSize = (MagicOffset - SceneOffset) / (SCENE_DATA_SIZE * 2);
    if (sceneSize < 0 || sceneSize > 10000) {
         std::cerr << "[LoadGame] ERROR: Invalid sceneSize " << sceneSize << std::endl;
         return false;
    }
    std::vector<Scene> scenes(sceneSize);
    for(int i=0; i<sceneSize; ++i) {
        std::vector<int16_t> buffer(SCENE_DATA_SIZE);
        grpFile.read((char*)buffer.data(), SCENE_DATA_SIZE * 2);
        scenes[i].setDataVector(buffer);
    }
    SceneManager::getInstance().SetScenes(scenes);

    std::cout << "[LoadGame] Loading Magics..." << std::endl;
    grpFile.seekg(MagicOffset, std::ios::beg);
    int magicSize = (WeiShopOffset - MagicOffset) / (MAGIC_DATA_SIZE * 2);
    if (magicSize < 0 || magicSize > 10000) {
        std::cerr << "[LoadGame] ERROR: Invalid magicSize " << magicSize << std::endl;
        return false;
    }
    m_magics.resize(magicSize);
    for(int i=0; i<magicSize; ++i) {
        std::vector<int16_t> buffer(MAGIC_DATA_SIZE);
        grpFile.read((char*)buffer.data(), MAGIC_DATA_SIZE * 2);
        m_magics[i].setDataVector(buffer);
    }

    // Preserve Shops (WeiShop) raw bytes for exact re-write on save
    {
        int shopBytesToRead = TotalLen - WeiShopOffset;
        if (shopBytesToRead > 0) {
            m_shopRaw.resize(shopBytesToRead);
            grpFile.seekg(WeiShopOffset, std::ios::beg);
            grpFile.read(reinterpret_cast<char*>(m_shopRaw.data()), shopBytesToRead);
        } else {
            m_shopRaw.clear();
        }
        RebuildShopsFromRaw();
    }

    grpFile.close();
    std::cout << "[LoadGame] ranger.grp loaded successfully." << std::endl;

    std::string sFilename = (slot == 0) ? "allsin.grp" : "S" + std::to_string(slot) + ".grp";
    std::string dFilename = (slot == 0) ? "alldef.grp" : "D" + std::to_string(slot) + ".grp";
    
    std::cout << "[LoadGame] Loading Maps: " << sFilename << " & " << dFilename << std::endl;

    bool mapLoaded = SceneManager::getInstance().LoadMapData(m_savePath + sFilename);
    if (!mapLoaded) {
        std::cout << "[LoadGame] Map data not found for slot " << slot << ", fallback to allsin.grp" << std::endl;
        mapLoaded = SceneManager::getInstance().LoadMapData(m_savePath + "allsin.grp");
    }
    bool eventLoaded = SceneManager::getInstance().LoadEventData(m_savePath + dFilename);
    if (!eventLoaded) {
        std::cout << "[LoadGame] Event data not found for slot " << slot << ", fallback to alldef.grp" << std::endl;
        eventLoaded = SceneManager::getInstance().LoadEventData(m_savePath + "alldef.grp");
    }
    if (!mapLoaded || !eventLoaded) {
        std::cerr << "[LoadGame] Failed to load map or event data after fallback." << std::endl;
        return false;
    }
    
    size_t mapSceneCount = SceneManager::getInstance().GetMapSceneCount();
    size_t eventSceneCount = SceneManager::getInstance().GetEventSceneCount();
    bool sceneValid = (m_currentSceneId < 0) ||
                      (m_currentSceneId < (int)mapSceneCount && m_currentSceneId < (int)eventSceneCount);
    if (!sceneValid) {
        std::cout << "[LoadGame] Scene id out of range, reloading map/event defaults" << std::endl;
        SceneManager::getInstance().LoadMapData(m_savePath + "allsin.grp");
        SceneManager::getInstance().LoadEventData(m_savePath + "alldef.grp");
        mapSceneCount = SceneManager::getInstance().GetMapSceneCount();
        eventSceneCount = SceneManager::getInstance().GetEventSceneCount();
        sceneValid = (m_currentSceneId < 0) ||
                     (m_currentSceneId < (int)mapSceneCount && m_currentSceneId < (int)eventSceneCount);
        if (!sceneValid) {
            m_currentSceneId = -1;
        }
    }
    
    if (m_currentSceneId >= 0) {
        if (m_mainMapX < 0 || m_mainMapX >= SCENE_MAP_SIZE || m_mainMapY < 0 || m_mainMapY >= SCENE_MAP_SIZE) {
            Scene* scene = SceneManager::getInstance().GetScene(m_currentSceneId);
            int fixX = 0;
            int fixY = 0;
            if (scene) {
                fixX = scene->getEntranceX();
                fixY = scene->getEntranceY();
            }
            if (fixX < 0) fixX = 0;
            if (fixX >= SCENE_MAP_SIZE) fixX = SCENE_MAP_SIZE - 1;
            if (fixY < 0) fixY = 0;
            if (fixY >= SCENE_MAP_SIZE) fixY = SCENE_MAP_SIZE - 1;
            setMainMapPosition(fixX, fixY);
        } else {
            setMainMapPosition(m_mainMapX, m_mainMapY);
        }
    } else {
        int clampedX = std::max(0, std::min(479, m_mainMapX));
        int clampedY = std::max(0, std::min(479, m_mainMapY));
        setMainMapPosition(clampedX, clampedY);
    }
    
    SceneManager::getInstance().SetCurrentScene(m_currentSceneId);
    reSetEntrance();

    // Align role TeamState with loaded party header (Pascal keeps both in sync via instruct_10).
    for (int i = 0; i < MAX_TEAM_SIZE; ++i) {
        const int roleId = m_teamList[i];
        if (roleId >= 0 && roleId < (int)m_roles.size()) {
            m_roles[roleId].setTeamState(1);
        }
    }

    if (m_currentSceneId >= 0) {
        bool savedValid = (m_savedWorldX >= 0 && m_savedWorldX < 480 && m_savedWorldY >= 0 && m_savedWorldY < 480) &&
                          !(m_savedWorldX == 0 && m_savedWorldY == 0);
        if (!savedValid) {
            Scene* scene = SceneManager::getInstance().GetScene(m_currentSceneId);
            if (scene) {
                int x1 = scene->getMainEntranceX1();
                int y1 = scene->getMainEntranceY1();
                int x2 = scene->getMainEntranceX2();
                int y2 = scene->getMainEntranceY2();
                if (x1 >= 0 && x1 < 480 && y1 >= 0 && y1 < 480) {
                    m_savedWorldX = x1;
                    m_savedWorldY = y1;
                } else if (x2 >= 0 && x2 < 480 && y2 >= 0 && y2 < 480) {
                    m_savedWorldX = x2;
                    m_savedWorldY = y2;
                }
            }
        }
    }

    // Force Refresh Layer 3 after everything is loaded
    if (m_currentSceneId >= 0) {
        SceneManager::getInstance().RefreshEventLayer(m_currentSceneId);
    }
    
    std::cout << "Game Loaded from Slot " << slot << std::endl;
    return true;
}

void GameManager::ResumeAfterLoad() {
    UIManager::getInstance().ReleaseMenuBackground();

    m_cameraX = m_mainMapX;
    m_cameraY = m_mainMapY;
    SceneManager::getInstance().SetCurrentScene(m_currentSceneId);

    if (m_currentSceneId >= 0) {
        // Pascal NewMenuLoad: WalkInScene(0) → InitialScene, DrawScene, ShowSceneName, CheckEvent3
        SceneManager::getInstance().InitialScene();
        RedrawRoamingScene();
        UIManager::getInstance().ShowSceneName(m_currentSceneId);
        EventManager::getInstance().CheckEvent(m_currentSceneId, m_mainMapX, m_mainMapY, false);
    } else {
        reSetEntrance();
        RedrawRoamingScene();
    }

    UIManager::getInstance().UpdateScreen();
    InputManager::getInstance().FlushEvents();
    VirtualControls::clearTapLatches();
    VirtualControls::releaseAll();
}

// ==========================================
// Logic Helpers Implementation
// ==========================================

void GameManager::LearnMagic(int roleIdx, int magicIdx, int mode) {
    if (roleIdx < 0 || roleIdx >= (int)m_roles.size()) return;
    Role& role = m_roles[roleIdx];
    
    int knownSlot = -1;
    for (int i = 0; i < 10; ++i) {
        if (role.getMagic(i) == magicIdx) {
            knownSlot = i;
            break;
        }
    }

    if (knownSlot != -1) {
        if (mode == 1) {
            int currentLevel = role.getMagLevel(knownSlot);
            if (currentLevel < 900) {
                role.setMagLevel(knownSlot, currentLevel + 100);
            }
        }
    } else {
        for (int i = 0; i < 10; ++i) {
            if (role.getMagic(i) == 0) {
                role.setMagic(i, magicIdx);
                role.setMagLevel(i, 0);
                break;
            }
        }
    }
}


void GameManager::RandomizeRoleStats(Role& role) {
    // Align Pascal ShowRandomAttribute(True)
    std::random_device rd;
    std::mt19937 gen(rd());
    auto ri = [&](int lo, int hi) {
        return std::uniform_int_distribution<int>(lo, hi)(gen);
    };

    role.setMaxHP(51 + ri(0, 49));
    role.setCurrentHP(role.getMaxHP());
    role.setMaxMP(51 + ri(0, 49));
    role.setCurrentMP(role.getMaxMP());
    role.setMPType(static_cast<int16_t>(ri(0, 1)));
    role.setIncLife(static_cast<int16_t>(1 + ri(0, 9)));

    role.setAttack(static_cast<int16_t>(25 + ri(0, 5)));
    role.setSpeed(static_cast<int16_t>(25 + ri(0, 5)));
    role.setDefence(static_cast<int16_t>(25 + ri(0, 5)));
    role.setMedcine(static_cast<int16_t>(25 + ri(0, 5)));
    role.setUsePoi(static_cast<int16_t>(25 + ri(0, 5)));
    role.setMedPoi(static_cast<int16_t>(25 + ri(0, 5)));
    role.setFist(static_cast<int16_t>(25 + ri(0, 5)));
    role.setSword(static_cast<int16_t>(25 + ri(0, 5)));
    role.setKnife(static_cast<int16_t>(25 + ri(0, 5)));
    role.setUnusual(static_cast<int16_t>(25 + ri(0, 5)));
    role.setHidWeapon(static_cast<int16_t>(25 + ri(0, 5)));
    role.setAptitude(static_cast<int16_t>(1 + ri(0, 99)));
}

void GameManager::PlayNewGameIntro() {
    // Pascal InitialRole → PlayBeginningMovie(26, 0); StartAmi;
    // Flush the Enter/Y that confirmed attribute roll so movie/titles are not skipped.
    InputManager::getInstance().FlushEvents();

    UIManager::getInstance().PlayBeginningMovie(26, 0);

    EventManager::getInstance().Instruct_FadeOut();
    UIManager::getInstance().DrawFilledRect(0, 0, 640, 480, 0x000000FF, 255);
    UIManager::getInstance().UpdateScreen();
    // ShowTitle waits for key internally (Pascal ShowTitle)
    EventManager::getInstance().Instruct_ShowTitle(4545, 28515);

    UIManager::getInstance().DrawFilledRect(0, 0, 640, 480, 0x000000FF, 255);
    UIManager::getInstance().UpdateScreen();
    EventManager::getInstance().Instruct_ShowTitle(4546, 28515);
    EventManager::getInstance().Instruct_FadeOut();
}

void GameManager::InitNewGame() {
    // Align Pascal: InitialRole (movie+StartAmi) → CurScene/music → WalkInScene(1)
    std::string loadPath = m_savePath;
    if (loadPath.empty()) {
        loadPath = "save/";
    }
    
    std::string preservedHeroNameGbk;
    if (!m_characterCreationNameUtf8.empty()) {
        preservedHeroNameGbk = TextManager::getInstance().utf8ToGbk(m_characterCreationNameUtf8);
    } else if (getRoleCount() > 0) {
        preservedHeroNameGbk = getRole(0).getName();
    }

    Role rolledSnapshot;
    bool haveRoll = getRoleCount() > 0;
    if (haveRoll) {
        rolledSnapshot = getRole(0);
    }

    loadData(loadPath);

    if (getRoleCount() > 0) {
        if (haveRoll) {
            Role& hero = getRole(0);
            hero.setMaxHP(rolledSnapshot.getMaxHP());
            hero.setCurrentHP(rolledSnapshot.getCurrentHP());
            hero.setMaxMP(rolledSnapshot.getMaxMP());
            hero.setCurrentMP(rolledSnapshot.getCurrentMP());
            hero.setMPType(rolledSnapshot.getMPType());
            hero.setIncLife(rolledSnapshot.getIncLife());
            hero.setAttack(rolledSnapshot.getAttack());
            hero.setSpeed(rolledSnapshot.getSpeed());
            hero.setDefence(rolledSnapshot.getDefence());
            hero.setMedcine(rolledSnapshot.getMedcine());
            hero.setUsePoi(rolledSnapshot.getUsePoi());
            hero.setMedPoi(rolledSnapshot.getMedPoi());
            hero.setFist(rolledSnapshot.getFist());
            hero.setSword(rolledSnapshot.getSword());
            hero.setKnife(rolledSnapshot.getKnife());
            hero.setUnusual(rolledSnapshot.getUnusual());
            hero.setHidWeapon(rolledSnapshot.getHidWeapon());
            hero.setAptitude(rolledSnapshot.getAptitude());
        }
        if (!preservedHeroNameGbk.empty()) {
            getRole(0).setName(preservedHeroNameGbk);
        }
    }

    // Pascal InitialRole: PlayBeginningMovie(26,0) + StartAmi (before WalkInScene)
    PlayNewGameIntro();

    // Pascal Start: showmr := False; CurScene := BEGIN_Scene; playmp3(ExitMusic)
    m_showMR = false;
    m_currentSceneId = 0; // BEGIN_Scene
    m_mainMapX = 40;      // BEGIN_Sx
    m_mainMapY = 38;      // BEGIN_Sy
    m_subMapFace = 0;
    m_mainMapFace = 0;
    m_walkFrame = 0;

    if (SceneManager::getInstance().GetSceneCount() > 0) {
        Scene* scene0 = SceneManager::getInstance().GetScene(0);
        if (scene0) {
            m_savedWorldX = scene0->getMainEntranceX1();
            m_savedWorldY = scene0->getMainEntranceY1();
        }
    }
    if (m_savedWorldX == 0 && m_savedWorldY == 0) {
        m_savedWorldX = 194;
        m_savedWorldY = 267;
    }

    SceneManager::getInstance().SetCurrentScene(m_currentSceneId);
    setMainMapPosition(m_mainMapX, m_mainMapY);

    // Pascal new game: load template S/D then WalkInScene → InitialScene.
    // loadData() only reloads ranger.grp; without refreshing allsin/alldef the
    // in-memory event layer can be empty/stale (e.g. after a previous run's ModEvent).
    {
        const std::string defPath = loadPath + "alldef.grp";
        const std::string sinPath = loadPath + "allsin.grp";
        bool defOk = SceneManager::getInstance().LoadEventData(defPath);
        if (!defOk) {
            defOk = SceneManager::getInstance().LoadEventData(loadPath + "Alldef.grp");
        }
        bool sinOk = SceneManager::getInstance().LoadMapData(sinPath);
        if (!sinOk) {
            sinOk = SceneManager::getInstance().LoadMapData(loadPath + "Allsin.grp");
        }
        std::cout << "[InitNewGame] Reload template alldef=" << (defOk ? "ok" : "FAIL")
                  << " allsin=" << (sinOk ? "ok" : "FAIL") << std::endl;
    }

    // Pascal WalkInScene: InitialScene before first DrawScene / CallEvent(101)
    SceneManager::getInstance().InitialScene();

    Scene* cur = SceneManager::getInstance().GetScene(m_currentSceneId);
    if (cur) {
        SoundManager::getInstance().StopMusic();
        // Pascal: playmp3(RScene[CurScene].ExitMusic, -1)
        int music = cur->getExitMusic();
        if (music < 0) music = cur->getEntranceMusic();
        if (music >= 0) SoundManager::getInstance().PlayMusic(music);
    }

    // WalkInScene(Open=1): DrawScene → CallEvent(BEGIN_EVENT=101) → ShowSceneName → CheckEvent3
    if (m_screenSurface) {
        SDL_FillSurfaceRect(m_screenSurface, NULL, 0xFF000000);
    }
    SceneManager::getInstance().DrawScene(m_renderer, m_cameraX, m_cameraY);
    if (m_screenSurface) RenderScreenTo(m_renderer);
    VirtualControls::present(m_renderer);

    InputManager::getInstance().FlushEvents();
    std::cout << "[InitNewGame] Triggering Opening Event 101..." << std::endl;
    // Pascal CurEvent := BEGIN_EVENT only for DrawScene camera/role rules.
    // Opcode 3's eventId=-2 must NOT resolve to script id 101 (not a DData slot).
    EventManager::getInstance().SetExecutionContext(m_currentSceneId, -1);
    EventManager::getInstance().ExecuteEvent(101);
    EventManager::getInstance().SetExecutionContext(m_currentSceneId, -1);

    if (m_screenSurface) {
        SDL_FillSurfaceRect(m_screenSurface, NULL, 0xFF000000);
    }
    SceneManager::getInstance().DrawScene(m_renderer, m_cameraX, m_cameraY);
    if (m_screenSurface) RenderScreenTo(m_renderer);
    VirtualControls::present(m_renderer);
    UIManager::getInstance().ShowSceneName(m_currentSceneId);

    // Pascal CheckEvent3: walk-on script at current tile
    {
        int16_t ev = SceneManager::getInstance().GetSceneTile(m_currentSceneId, 3, m_mainMapX, m_mainMapY);
        if (ev >= 0) {
            int16_t scriptId = SceneManager::getInstance().GetEventData(m_currentSceneId, ev, 4);
            if (scriptId > 0) {
                EventManager::getInstance().SetExecutionContext(m_currentSceneId, ev);
                EventManager::getInstance().ExecuteEvent(scriptId);
                EventManager::getInstance().SetExecutionContext(m_currentSceneId, -1);
            }
        }
    }

    std::cout << "[InitNewGame] Scene: " << m_currentSceneId
              << " Pos: (" << m_mainMapX << ", " << m_mainMapY << ")"
              << " ShowMR=" << m_showMR << std::endl;
}

void GameManager::Run() {
    while (m_isRunning) {
        SDL_RenderClear(m_renderer);

        if (m_currentState == GameState::TitleScreen) {
            UpdateTitleScreen();
        } else if (m_currentState == GameState::CharacterCreation) {
            UpdateCharacterCreation();
        } else if (m_currentState == GameState::Roaming) {
            UpdateRoaming();
        } else if (m_currentState == GameState::Battle) {
            BattleManager::getInstance().RunBattle();
            m_currentState = GameState::Roaming; 
        } else if (m_currentState == GameState::SystemMenu) {
            UpdateSystemMenu();
        } else if (m_currentState == GameState::InventoryMenu) {
            UpdateInventoryMenu();
        }
        
        // Present is called in Update functions
        SDL_Delay(10);
    }
    std::cout << "Exiting Game Loop..." << std::endl;
}

void GameManager::Quit() {
    m_isRunning = false;
    // SceneManager::getInstance().Cleanup(); // SceneManager does not have Cleanup
    // BattleManager::getInstance().Cleanup();
    // UIManager::getInstance().Cleanup(); // Managed by static instance but good to have explicit cleanup if needed

    if (m_screenTexture) {
        SDL_DestroyTexture(m_screenTexture);
        m_screenTexture = nullptr;
    }
    
    if (m_screenSurface) {
        SDL_DestroySurface(m_screenSurface);
        m_screenSurface = nullptr;
    }

    if (m_renderer) {
        SDL_DestroyRenderer(m_renderer);
        m_renderer = nullptr;
    }

    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }

    TTF_Quit();
    SDL_Quit();
}

void GameManager::UpdateTitleScreen() {
    if (!m_playedTitleAnim) {
        UIManager::getInstance().PlayTitleAnimation();
        VirtualControls::clearTapLatches();
        VirtualControls::releaseAll();
        InputManager::getInstance().FlushEvents();
        m_playedTitleAnim = true;
    }

    auto activateTitleSelection = [&]() {
        if (m_titleMenuSelection == 0) {
            m_currentState = GameState::CharacterCreation;
            m_charCreatePhase = CharCreatePhase::NameInput;
            m_characterCreationNameUtf8 = "金先生";
            if (getRoleCount() > 0) {
                getRole(0).setName(TextManager::getInstance().utf8ToGbk(m_characterCreationNameUtf8));
            }
            SDL_StartTextInput(m_window);
            m_characterCreationTextInputActive = true;
            VirtualControls::clearTapLatches();
            VirtualControls::releaseAll();
            InputManager::getInstance().FlushEvents();
        } else if (m_titleMenuSelection == 1) {
            UIManager::getInstance().DrawTitleBackground();
            DrawTitleMenu();
            UIManager::getInstance().CaptureScreen();
            if (UIManager::getInstance().ShowSaveLoadMenu(false)) {
                m_currentState = GameState::Roaming;
            }
        } else if (m_titleMenuSelection == 2) {
            m_isRunning = false;
        }
    };

    static uint32_t s_lastTitleConfirmMs = 0;
    SDL_Event e;
    while (SDL_PollEvent(&e) != 0) {
        if (e.type == SDL_EVENT_QUIT) {
            m_isRunning = false;
            continue;
        }
        if (!VirtualControls::handleEvent(e)) continue;

        if (e.type == SDL_EVENT_KEY_DOWN) {
            switch (e.key.key) {
                case SDLK_UP:
                case SDLK_KP_8:
                    m_titleMenuSelection--;
                    if (m_titleMenuSelection < 0) m_titleMenuSelection = 2;
                    break;
                case SDLK_DOWN:
                case SDLK_KP_2:
                    m_titleMenuSelection++;
                    if (m_titleMenuSelection > 2) m_titleMenuSelection = 0;
                    break;
                case SDLK_RETURN:
                case SDLK_SPACE:
                case SDLK_KP_ENTER: {
                    uint32_t now = SDL_GetTicks();
                    if (now - s_lastTitleConfirmMs > 250) {
                        s_lastTitleConfirmMs = now;
                        activateTitleSelection();
                    }
                    break;
                }
                case SDLK_ESCAPE:
                    m_isRunning = false;
                    break;
                default:
                    break;
            }
        }
    }

    // Virtual pad only (no SDL_PushEvent — avoids duplicate with PollEvent above).
    if (VirtualControls::consumeTap(SDL_SCANCODE_UP)) {
        m_titleMenuSelection++;
        if (m_titleMenuSelection > 2) m_titleMenuSelection = 0;
    }
    if (VirtualControls::consumeTap(SDL_SCANCODE_DOWN)) {
        m_titleMenuSelection--;
        if (m_titleMenuSelection < 0) m_titleMenuSelection = 2;
    }
    if (VirtualControls::consumeTap(SDL_SCANCODE_SPACE) || VirtualControls::consumeTap(SDL_SCANCODE_RETURN)) {
        uint32_t now = SDL_GetTicks();
        if (now - s_lastTitleConfirmMs > 250) {
            s_lastTitleConfirmMs = now;
            activateTitleSelection();
        }
    }
    if (VirtualControls::consumeTap(SDL_SCANCODE_ESCAPE)) {
        m_isRunning = false;
    }

    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
    SDL_RenderClear(m_renderer);
    UIManager::getInstance().DrawTitleBackground();
    DrawTitleMenu();
    VirtualControls::present(m_renderer);
}

void GameManager::ReturnToTitleScreen() {
    m_currentState = GameState::TitleScreen;
    m_titleMenuSelection = 0;
    SoundManager::getInstance().StopMusic();
    SoundManager::getInstance().PlayMusic(106);
}

void GameManager::DrawTitleMenu() {
    const char* items[] = { "新 游 戏 (Start Game)", "载 入 进 度 (Load Game)", "离 开 游 戏 (Quit Game)" };
    int startX = 220;
    int startY = 300;
    int gapY = 40;
    
    for (int i = 0; i < 3; ++i) {
        uint32_t color = (i == m_titleMenuSelection) ? 0xFFD24DFF : 0xE6E6E6FF;
        uint32_t shadow = (i == m_titleMenuSelection) ? 0x5C1E00FF : 0x000000FF;
        UIManager::getInstance().DrawShadowTextUtf8(items[i], startX, startY + i * gapY, color, shadow, 24);
    }
}

void GameManager::UpdateCharacterCreation() {
    auto confirmCharCreate = [&]() {
        if (m_charCreatePhase == CharCreatePhase::NameInput) {
            if (m_characterCreationTextInputActive) {
                SDL_StopTextInput(m_window);
                m_characterCreationTextInputActive = false;
            }
            if (m_characterCreationNameUtf8.empty()) {
                m_characterCreationNameUtf8 = "金先生";
            } else if (getGameTime() == 0) {
                std::string gbk = TextManager::getInstance().utf8ToGbk(m_characterCreationNameUtf8);
                if (gbk.size() < 2 || (uint8_t)gbk[0] != 0xBD || (uint8_t)gbk[1] != 0xF0) {
                    m_characterCreationNameUtf8 = std::string("金") + m_characterCreationNameUtf8;
                }
            }
            if (getRoleCount() > 0) {
                getRole(0).setName(TextManager::getInstance().utf8ToGbk(m_characterCreationNameUtf8));
                RandomizeRoleStats(getRole(0));
            }
            UIManager::getInstance().MenuDifficult();
            m_charCreatePhase = CharCreatePhase::AttributeSelect;
        } else {
            if (getRoleCount() > 0) {
                getRole(0).setName(TextManager::getInstance().utf8ToGbk(m_characterCreationNameUtf8));
            }
            InitNewGame();
            m_currentState = GameState::Roaming;
        }
    };

    static uint32_t s_lastCharCreateConfirmMs = 0;
    auto confirmCharCreateDebounced = [&]() {
        const uint32_t now = SDL_GetTicks();
        if (now - s_lastCharCreateConfirmMs < 250) return;
        s_lastCharCreateConfirmMs = now;
        confirmCharCreate();
    };

    SDL_Event e;
    while (SDL_PollEvent(&e) != 0) {
        if (e.type == SDL_EVENT_QUIT) {
            m_isRunning = false;
            if (m_characterCreationTextInputActive) {
                SDL_StopTextInput(m_window);
                m_characterCreationTextInputActive = false;
            }
            continue;
        }
        if (!VirtualControls::handleEvent(e)) continue;

        if (e.type == SDL_EVENT_KEY_DOWN) {
            if (m_charCreatePhase == CharCreatePhase::NameInput) {
                if (e.key.key == SDLK_BACKSPACE) {
                    PopBackUtf8(m_characterCreationNameUtf8);
                    if (getRoleCount() > 0) {
                        getRole(0).setName(TextManager::getInstance().utf8ToGbk(m_characterCreationNameUtf8));
                    }
                } else if (e.key.key == SDLK_RETURN) {
                    confirmCharCreateDebounced();
                } else if (e.key.key == SDLK_ESCAPE) {
                    if (m_characterCreationTextInputActive) {
                        SDL_StopTextInput(m_window);
                        m_characterCreationTextInputActive = false;
                    }
                    m_currentState = GameState::TitleScreen;
                }
            } else { // AttributeSelect
                // Pascal RandomAttribute: Y/Return confirm; Esc cancel; any other key re-roll
                if (e.key.key == SDLK_Y || e.key.key == SDLK_RETURN) {
                    confirmCharCreateDebounced();
                    if (m_currentState == GameState::Roaming) return;
                } else if (e.key.key == SDLK_ESCAPE) {
                    m_currentState = GameState::TitleScreen;
                } else {
                    if (getRoleCount() > 0) RandomizeRoleStats(getRole(0));
                }
            }
        } else if (e.type == SDL_EVENT_TEXT_INPUT && m_charCreatePhase == CharCreatePhase::NameInput) {
            std::string appended = m_characterCreationNameUtf8;
            appended += e.text.text;
            std::string gbk = TextManager::getInstance().utf8ToGbk(appended);
            if (!gbk.empty() && gbk.size() <= 9) {
                m_characterCreationNameUtf8 = std::move(appended);
                if (getRoleCount() > 0) getRole(0).setName(gbk);
            }
        }
    }

    if (VirtualControls::consumeTap(SDL_SCANCODE_SPACE) || VirtualControls::consumeTap(SDL_SCANCODE_RETURN)) {
        confirmCharCreateDebounced();
        if (m_currentState == GameState::Roaming) return;
    }
    if (VirtualControls::consumeTap(SDL_SCANCODE_ESCAPE)) {
        if (m_characterCreationTextInputActive) {
            SDL_StopTextInput(m_window);
            m_characterCreationTextInputActive = false;
        }
        m_currentState = GameState::TitleScreen;
    }

    if (m_charCreatePhase == CharCreatePhase::NameInput) {
        UIManager::getInstance().DrawCharacterCreationNamePrompt(m_characterCreationNameUtf8);
    } else if (getRoleCount() > 0) {
        UIManager::getInstance().DrawCharacterCreationAttributes(getRole(0));
    }
    VirtualControls::present(m_renderer);
}

void GameManager::getFacingTile(int& x, int& y) const {
    x = m_mainMapX;
    y = m_mainMapY;
    if (m_currentSceneId >= 0) {
        switch (m_subMapFace) {
            case 0: x -= 1; break;
            case 1: y += 1; break;
            case 2: y -= 1; break;
            case 3: x += 1; break;
        }
    } else {
        switch (m_mainMapFace) {
            case 0: x += 1; break;
            case 1: x -= 1; break;
            case 2: y -= 1; break;
            case 3: y += 1; break;
        }
    }
}

void GameManager::UpdateRoaming() {
    int pendingEvent = EventManager::getInstance().GetPendingEvent();
    if (pendingEvent != -1) {
        EventManager::getInstance().ClearPendingEvent();
        EventManager::getInstance().ExecuteEvent(pendingEvent);
    }

    auto tryMove = [&](int dx, int dy) {
        if (dx == 0 && dy == 0) return;
        int nextX = m_mainMapX + dx;
        int nextY = m_mainMapY + dy;

        if (m_currentSceneId >= 0) {
            if (SceneManager::getInstance().CanWalk(nextX, nextY)) {
                m_mainMapX = nextX;
                m_mainMapY = nextY;
                m_cameraX = m_mainMapX;
                m_cameraY = m_mainMapY;
                std::cout << "[SceneMove] Moved to (" << m_mainMapX << "," << m_mainMapY << ")" << std::endl;
                updateWalkFrame();
                EventManager::getInstance().CheckEvent(m_currentSceneId, m_mainMapX, m_mainMapY, false);

                Scene* scene = SceneManager::getInstance().GetScene(m_currentSceneId);
                if (scene) {
                    bool atExit = false;
                    for (int i = 0; i < 3; ++i) {
                        int exitX = scene->getExitX(i);
                        int exitY = scene->getExitY(i);
                        if (exitX > 0 && exitY > 0 && m_mainMapX == exitX && m_mainMapY == exitY) {
                            atExit = true;
                            break;
                        }
                    }
                    
                    if (atExit) {
                        int worldX = m_savedWorldX;
                        int worldY = m_savedWorldY;
                        bool savedValid = (worldX >= 0 && worldX < 480 && worldY >= 0 && worldY < 480) &&
                                          !(worldX == 0 && worldY == 0);
                        if (!savedValid) {
                            int x1 = scene->getMainEntranceX1();
                            int y1 = scene->getMainEntranceY1();
                            int x2 = scene->getMainEntranceX2();
                            int y2 = scene->getMainEntranceY2();
                            if (x1 >= 0 && x1 < 480 && y1 >= 0 && y1 < 480) {
                                worldX = x1;
                                worldY = y1;
                            } else if (x2 >= 0 && x2 < 480 && y2 >= 0 && y2 < 480) {
                                worldX = x2;
                                worldY = y2;
                            }
                        }
 
                        int16_t entranceScene = SceneManager::getInstance().GetEntrance(worldX, worldY);
                        if (entranceScene >= 0 && entranceScene != m_currentSceneId) {
                            int x1 = scene->getMainEntranceX1();
                            int y1 = scene->getMainEntranceY1();
                            int x2 = scene->getMainEntranceX2();
                            int y2 = scene->getMainEntranceY2();
                            int candidateX = worldX;
                            int candidateY = worldY;
                            if (x1 >= 0 && x1 < 480 && y1 >= 0 && y1 < 480) {
                                candidateX = x1;
                                candidateY = y1;
                            } else if (x2 >= 0 && x2 < 480 && y2 >= 0 && y2 < 480) {
                                candidateX = x2;
                                candidateY = y2;
                            }
                            int16_t candidateScene = SceneManager::getInstance().GetEntrance(candidateX, candidateY);
                            if (candidateScene >= 0 && candidateScene != m_currentSceneId) {
                                worldX = candidateX;
                                worldY = candidateY;
                            }
                            std::cout << "[ExitScene] Entrance collision: target belongs to scene " << entranceScene
                                      << ", using (" << worldX << "," << worldY << ")" << std::endl;
                        }
                        
                        int finalX = worldX;
                        int finalY = worldY;
                        switch (m_subMapFace) {
                            case 0: finalX -= 1; break;
                            case 1: finalY += 1; break;
                            case 2: finalY -= 1; break;
                            case 3: finalX += 1; break;
                        }
                        finalX = std::max(0, std::min(479, finalX));
                        finalY = std::max(0, std::min(479, finalY));
                        std::cout << "[ExitScene] Entrance at (" << worldX << "," << worldY 
                                  << "), offset by face " << m_subMapFace << " to (" << finalX << "," << finalY << ")" << std::endl;
                        worldX = finalX;
                        worldY = finalY;
                        
                        std::cout << "[ExitScene] Exiting scene to world map at (" << worldX << "," << worldY << ")" << std::endl;
                        UIManager::getInstance().FadeScreen(false);
                        
                        m_currentSceneId = -1;
                        SceneManager::getInstance().SetCurrentScene(-1);
                        SceneManager::getInstance().ResetEntrance();
                        
                        m_savedWorldX = worldX;
                        m_savedWorldY = worldY;
                        m_mainMapX = worldX;
                        m_mainMapY = worldY;
                        m_cameraX = m_mainMapX;
                        m_cameraY = m_mainMapY;
                        
                        std::cout << "[ExitScene] Now at world map (" << m_mainMapX << "," << m_mainMapY << ")" << std::endl;
                        
                        if (m_screenSurface) {
                            SDL_FillSurfaceRect(m_screenSurface, NULL, 0xFF000000);
                            SceneManager::getInstance().DrawScene(m_renderer, m_cameraX, m_cameraY);
                        }
                        
                        UIManager::getInstance().FadeScreen(true);
                    }
                }
            }
        } else {
            // 大地图移动逻辑：
            // 1. 先检查面向的下一个位置是否有场景入口
            // 2. 如果有入口，先渲染一帧显示朝向改变，再进入场景（不移动）
            // 3. 如果没有入口，正常移动
            int16_t snum = SceneManager::getInstance().GetEntrance(nextX, nextY);
            if (snum >= 0) {
                std::cout << "[CheckEntrance] Facing entrance of scene " << snum 
                          << " at (" << nextX << "," << nextY << ")" << std::endl;
                
                // 先渲染一帧，显示人物朝向改变
                if (m_screenSurface) {
                    SDL_FillSurfaceRect(m_screenSurface, NULL, 0xFF000000);
                    SceneManager::getInstance().DrawScene(m_renderer, m_cameraX, m_cameraY);
                    RenderScreenTo(m_renderer);
                }
                VirtualControls::present(m_renderer);
                SDL_Delay(100); // 短暂延迟，让玩家看到朝向改变
                
                if (TryEnterScene(snum)) {
                    std::cout << "[CheckEntrance] Successfully entered scene " << snum << std::endl;
                    return;
                }
                std::cout << "[CheckEntrance] TryEnterScene returned false, will move" << std::endl;
            }

            if (CanWalkWorld(nextX, nextY)) {
                m_mainMapX = nextX;
                m_mainMapY = nextY;
                m_cameraX = m_mainMapX;
                m_cameraY = m_mainMapY;
                updateWalkFrame();
                std::cout << "[WorldMove] Moved to (" << m_mainMapX << "," << m_mainMapY << ")" << std::endl;
                int16_t onEntrance = SceneManager::getInstance().GetEntrance(m_mainMapX, m_mainMapY);
                if (onEntrance >= 0) {
                    TryEnterScene(onEntrance);
                }
            }
        }
    };

    InputManager::getInstance().BeginFrame();
    SDL_Event e;
    while (SDL_PollEvent(&e) != 0) {
        InputManager::getInstance().ProcessEvent(e);
        VirtualControls::handleEvent(e);
        if (e.type == SDL_EVENT_QUIT) {
            m_isRunning = false;
        } else if (e.type == SDL_EVENT_KEY_DOWN) {
            int dx = 0, dy = 0;
            uint32_t now = SDL_GetTicks();
            const bool inScene = (m_currentSceneId >= 0);
            switch (e.key.key) {
                case SDLK_UP:    case SDLK_W:
                    dx = -1;
                    if (inScene) applySceneDirection(*this, 0);
                    else applyWorldDirection(*this, -1, 0);
                    break;
                case SDLK_DOWN:  case SDLK_S:
                    dx = 1;
                    if (inScene) applySceneDirection(*this, 3);
                    else applyWorldDirection(*this, 1, 0);
                    break;
                case SDLK_LEFT:  case SDLK_A:
                    dy = -1;
                    if (inScene) applySceneDirection(*this, 2);
                    else applyWorldDirection(*this, 0, -1);
                    break;
                case SDLK_RIGHT: case SDLK_D:
                    dy = 1;
                    if (inScene) applySceneDirection(*this, 1);
                    else applyWorldDirection(*this, 0, 1);
                    break;
                    
                case SDLK_C: 
                    if (!m_teamList.empty()) {
                        UIManager::getInstance().ShowStatus(m_teamList[0]);
                    }
                    break;
                case SDLK_M:
                    if (m_currentSceneId < 0) {
                        UIManager::getInstance().ShowMap();
                    }
                    break;
                case SDLK_1: case SDLK_2: case SDLK_3:
                case SDLK_4: case SDLK_5: case SDLK_6:
                case SDLK_KP_1: case SDLK_KP_2: case SDLK_KP_3:
                case SDLK_KP_4: case SDLK_KP_5: case SDLK_KP_6:
                    break; // handled on KEY_UP via CheckHotkey
                case SDLK_ESCAPE:
                    m_currentState = GameState::SystemMenu;
                    break;
            }
            
            if (dx != 0 || dy != 0) {
                int face = inScene ? m_subMapFace : m_mainMapFace;
                std::cout << "[KEY_DOWN] Key=" << e.key.key << " dx=" << dx << " dy=" << dy 
                          << " Face=" << face << " Pos=(" << m_mainMapX << "," << m_mainMapY << ")" << std::endl;
                m_holdDx = dx;
                m_holdDy = dy;
                m_moveHoldStart = now;
                m_lastMoveTick = now;
                tryMove(dx, dy);
            }
        } else if (e.type == SDL_EVENT_KEY_UP) {
            if (e.key.key >= SDLK_1 && e.key.key <= SDLK_6) {
                UIManager::getInstance().CheckHotkey(e.key.key);
            } else if (e.key.key >= SDLK_KP_1 && e.key.key <= SDLK_KP_6) {
                UIManager::getInstance().CheckHotkey(SDLK_1 + (e.key.key - SDLK_KP_1));
            } else if (e.key.key == SDLK_RETURN || e.key.key == SDLK_SPACE) {
                if (m_currentSceneId >= 0) {
                    int frontX = 0, frontY = 0;
                    getFacingTile(frontX, frontY);
                    EventManager::getInstance().CheckEvent(m_currentSceneId, frontX, frontY, true);
                } else {
                    CheckWorldEntrance();
                }
            }
        }
    }

    if (VirtualControls::consumeTap(SDL_SCANCODE_SPACE) || VirtualControls::consumeTap(SDL_SCANCODE_RETURN)) {
        if (m_currentSceneId >= 0) {
            int frontX = 0, frontY = 0;
            getFacingTile(frontX, frontY);
            EventManager::getInstance().CheckEvent(m_currentSceneId, frontX, frontY, true);
        } else {
            CheckWorldEntrance();
        }
    }

    uint32_t now = SDL_GetTicks();
    const bool* keys = SDL_GetKeyboardState(nullptr);
    auto keyDown = [&](SDL_Scancode sc) {
        return keys[sc] || VirtualControls::isScancodeDown(sc);
    };
    int holdDx = 0;
    int holdDy = 0;
    const bool inScene = (m_currentSceneId >= 0);
    if (keyDown(SDL_SCANCODE_UP) || keyDown(SDL_SCANCODE_W)) {
        holdDx = -1;
        if (inScene) applySceneDirection(*this, 0);
        else applyWorldDirection(*this, -1, 0);
    } else if (keyDown(SDL_SCANCODE_DOWN) || keyDown(SDL_SCANCODE_S)) {
        holdDx = 1;
        if (inScene) applySceneDirection(*this, 3);
        else applyWorldDirection(*this, 1, 0);
    } else if (keyDown(SDL_SCANCODE_LEFT) || keyDown(SDL_SCANCODE_A)) {
        holdDy = -1;
        if (inScene) applySceneDirection(*this, 2);
        else applyWorldDirection(*this, 0, -1);
    } else if (keyDown(SDL_SCANCODE_RIGHT) || keyDown(SDL_SCANCODE_D)) {
        holdDy = 1;
        if (inScene) applySceneDirection(*this, 1);
        else applyWorldDirection(*this, 0, 1);
    }

    const uint32_t initialDelayMs = 80;
    const uint32_t repeatIntervalMs = 60;
    if (holdDx != 0 || holdDy != 0) {
        if (m_holdDx != holdDx || m_holdDy != holdDy) {
            int face = inScene ? m_subMapFace : m_mainMapFace;
            std::cout << "[KEY_HOLD] holdDx=" << holdDx << " holdDy=" << holdDy 
                      << " Face=" << face << " Pos=(" << m_mainMapX << "," << m_mainMapY << ")" << std::endl;
            m_holdDx = holdDx;
            m_holdDy = holdDy;
            m_moveHoldStart = now;
            m_lastMoveTick = now;
            tryMove(holdDx, holdDy);
        } else if (now - m_moveHoldStart >= initialDelayMs &&
                   now - m_lastMoveTick >= repeatIntervalMs) {
            m_lastMoveTick = now;
            tryMove(holdDx, holdDy);
        }
    } else {
        m_holdDx = 0;
        m_holdDy = 0;
    }
    
    if (m_screenSurface) {
        SDL_FillSurfaceRect(m_screenSurface, NULL, 0xFF000000);
        SceneManager::getInstance().DrawScene(m_renderer, m_cameraX, m_cameraY);
        RenderScreenTo(m_renderer);
    }
    VirtualControls::present(m_renderer);
}

bool GameManager::CanWalkWorld(int x, int y) {
    if (x < 0 || x >= 480 || y < 0 || y >= 480) return false;

    SceneManager& sm = SceneManager::getInstance();
    int16_t buildx = sm.GetWorldBuildX(x, y);
    int16_t surface = sm.GetWorldSurface(x, y);
    int16_t earth = sm.GetWorldEarth(x, y);

    bool canwalk = (buildx == 0);

    if (x <= 0 || x >= 479 || y <= 0 || y >= 479 ||
        (surface >= 1692 && surface <= 1700)) {
        canwalk = false;
    }

    if (earth == 838 || (earth >= 612 && earth <= 670)) {
        canwalk = false;
    }

    if ((earth >= 358 && earth <= 362) ||
        (earth >= 506 && earth <= 670) ||
        (earth >= 1016 && earth <= 1022)) {
        if (m_inShip != 1) {
            m_inShip = 1;
        }
        if (earth == 838 || (earth >= 612 && earth <= 670)) {
            canwalk = false;
        } else if (surface >= 1746 && surface <= 1788) {
            canwalk = false;
        } else {
            canwalk = true;
        }
    } else {
        if (m_inShip == 1) {
            m_shipY = static_cast<int16_t>(m_mainMapX);
            m_shipX = static_cast<int16_t>(m_mainMapY);
            m_shipFace = static_cast<int16_t>(m_mainMapFace);
        }
        m_inShip = 0;
    }

    int surfaceHalf = surface / 2;
    if ((surfaceHalf >= 863 && surfaceHalf <= 872) ||
        (surfaceHalf >= 852 && surfaceHalf <= 854) ||
        (surfaceHalf >= 858 && surfaceHalf <= 860)) {
        canwalk = true;
    }

    return canwalk;
}

bool GameManager::TryEnterScene(int sceneId) {
    if (m_currentSceneId != -1) return false;
    if (sceneId < 0) return false;
    
    Scene& scene = getScene(sceneId);
    bool canEntrance = false;

    int16_t enCond = scene.getEnCondition();
    if (enCond == 0) {
        canEntrance = true;
    } else if (enCond == 1) {
        canEntrance = true;
    } else if (enCond == 2) {
        for (int roleId : m_teamList) {
            if (roleId < 0) continue;
            Role& role = getRole(roleId);
            if (role.getSpeed() >= 70) {
                canEntrance = true;
                break;
            }
        }
    } else {
        canEntrance = true;
    }

    if (!canEntrance) return false;

    SaveAutoGame();

    UIManager::getInstance().FadeScreen(false);

    int worldX = m_mainMapX;
    int worldY = m_mainMapY;
    int frontX = m_mainMapX;
    int frontY = m_mainMapY;
    switch (m_mainMapFace) {
        case 0: frontX += 1; break;
        case 1: frontX -= 1; break;
        case 2: frontY -= 1; break;
        case 3: frontY += 1; break;
    }
    if (frontX >= 0 && frontX < 480 && frontY >= 0 && frontY < 480) {
        int16_t frontScene = SceneManager::getInstance().GetEntrance(frontX, frontY);
        if (frontScene == sceneId) {
            worldX = frontX;
            worldY = frontY;
        }
    }
    m_savedWorldX = worldX;
    m_savedWorldY = worldY;

    int16_t entranceX = scene.getEntranceX();
    int16_t entranceY = scene.getEntranceY();

    enterScene(sceneId);
    switch (m_mainMapFace) {
        case 0: m_subMapFace = 3; break;
        case 1: m_subMapFace = 0; break;
        case 2: m_subMapFace = 2; break;
        case 3: m_subMapFace = 1; break;
    }
    m_mainMapFace = 3 - m_mainMapFace;
    resetWalkFrame();

    setMainMapPosition(entranceX, entranceY);

    if (m_screenSurface) {
        SDL_FillSurfaceRect(m_screenSurface, NULL, 0xFF000000);
        SceneManager::getInstance().DrawScene(m_renderer, entranceX, entranceY);
    }
    UIManager::getInstance().ShowSceneName(sceneId);
    UIManager::getInstance().FadeScreen(true);

    return true;
}

bool GameManager::CheckWorldEntrance() {
    if (m_currentSceneId != -1) return false;

    int x = m_mainMapX;
    int y = m_mainMapY;

    // 人脸朝向与前方坐标的对应关系必须与 UpdateRoaming() 中的移动逻辑一致
    // face 0 (DOWN):  X + 1
    // face 1 (UP):    X - 1
    // face 2 (LEFT):  Y - 1
    // face 3 (RIGHT): Y + 1
    switch (m_mainMapFace) {
        case 0: x += 1; break;
        case 1: x -= 1; break;
        case 2: y -= 1; break;
        case 3: y += 1; break;
    }

    int16_t snum = SceneManager::getInstance().GetEntrance(x, y);
    if (snum < 0) return false;

    return TryEnterScene(snum);
}

void GameManager::UpdateSystemMenu() {
    UIManager::getInstance().ShowMenu();
    m_currentState = GameState::Roaming;
}

void GameManager::UpdateInventoryMenu() {
    UIManager::getInstance().SelectShowItem();
    m_currentState = GameState::Roaming;
}

void GameManager::RenderScreenTo(SDL_Renderer* renderer) {
    if (!renderer || !m_screenSurface || !m_screenTexture) return;
    GraphicsUtils::EnsureSurfaceOpaque(m_screenSurface);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_SetTextureBlendMode(m_screenTexture, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_UpdateTexture(m_screenTexture, NULL, m_screenSurface->pixels, m_screenSurface->pitch);
    SDL_RenderTexture(renderer, m_screenTexture, NULL, NULL);
}

void GameManager::DrawRoamingSceneToSurface() {
    if (!m_renderer || !m_screenSurface) return;
    int cx = 0;
    int cy = 0;
    getCameraPosition(cx, cy);
    SceneManager::getInstance().DrawScene(m_renderer, cx, cy);
}

void GameManager::RedrawRoamingScene() {
    DrawRoamingSceneToSurface();
    RenderScreenTo(m_renderer);
}

Role& GameManager::getRole(int index) {
    if (index < 0 || index >= m_roles.size()) {
        static Role dummy; 
        return dummy; 
    }
    return m_roles[index];
}

Item& GameManager::getItem(int index) {
    if (index < 0 || index >= m_items.size()) {
        static Item dummy;
        return dummy;
    }
    return m_items[index];
}

Scene& GameManager::getScene(int index) {
    Scene* scene = SceneManager::getInstance().GetScene(index);
    if (scene) {
        return *scene;
    }
    static Scene dummy;
    return dummy;
}

Magic& GameManager::getMagic(int index) {
    if (index < 0 || index >= m_magics.size()) {
        static Magic dummy;
        return dummy;
    }
    return m_magics[index];
}

PicImage* GameManager::getHead(int index) {
    if (m_heads.empty()) {
        int count = PicLoader::getPicCount("resource/Heads.Pic");
        if (count > 0) {
            m_heads.resize(count);
        }
    }

    if (index >= 0 && index < m_heads.size()) {
        if (m_heads[index].surface == nullptr) {
            m_heads[index] = PicLoader::loadPic("resource/Heads.Pic", index);
        }
        return &m_heads[index];
    }
    return nullptr;
}

void GameManager::setCameraPosition(int x, int y) {
    m_cameraX = x;
    m_cameraY = y;
}

void GameManager::setMainMapPosition(int x, int y) {
    m_mainMapX = x;
    m_mainMapY = y;
    m_cameraX = x;
    m_cameraY = y;
}

void GameManager::enterScene(int sceneId) {
    m_currentSceneId = sceneId;
    SceneManager::getInstance().SetCurrentScene(sceneId);
    if (sceneId >= 0) {
        // Pascal: entering a sub-scene rebuilds SceneImg via InitialScene
        SceneManager::getInstance().InitialScene();
    }
}

void GameManager::AddItem(int itemId, int amount) {
    if (amount == 0) return;
    ensureInventorySize();
    for (auto it = m_inventory.begin(); it != m_inventory.end(); ++it) {
        if (it->id == itemId) {
            int newAmount = it->amount + amount;
            if (amount >= 0 && newAmount < 0) newAmount = 32767;
            if (amount < 0 && newAmount < 0) newAmount = 0;
            if (newAmount > 32767) newAmount = 32767;
            it->amount = static_cast<int16>(newAmount);
            if (it->amount <= 0) {
                it->id = -1;
                it->amount = 0;
            }
            return;
        }
    }
    if (amount < 0) return;
    // Find first empty slot
    for (int i = 0; i < MAX_ITEM_AMOUNT; ++i) {
        if (m_inventory[i].id < 0 || m_inventory[i].amount <= 0) {
            m_inventory[i].id = itemId;
            m_inventory[i].amount = static_cast<int16>(amount);
            if (m_inventory[i].amount > 32767) m_inventory[i].amount = 32767;
            return;
        }
    }
}

void GameManager::ensureInventorySize() {
    if ((int)m_inventory.size() != MAX_ITEM_AMOUNT) {
        m_inventory.resize(MAX_ITEM_AMOUNT);
        for (int i = 0; i < MAX_ITEM_AMOUNT; ++i) {
            if (m_inventory[i].id == 0 && m_inventory[i].amount == 0) {
                m_inventory[i].id = -1;
                m_inventory[i].amount = 0;
            }
        }
    }
}

void GameManager::setInventorySlot(int slot, int16_t number, int16_t amount) {
    ensureInventorySize();
    if (slot < 0 || slot >= MAX_ITEM_AMOUNT) return;
    m_inventory[slot].id = number;
    m_inventory[slot].amount = amount;
}

InventoryItem GameManager::getInventorySlot(int slot) const {
    if (slot < 0 || slot >= (int)m_inventory.size()) return InventoryItem{};
    return m_inventory[slot];
}

int GameManager::getItemAmount(int itemId) {
    for (const auto& item : m_inventory) {
        if (item.id == itemId) return item.amount;
    }
    return 0;
}

void GameManager::useItem(int itemId) {
    if (getItemAmount(itemId) > 0) {
        for (int i = 0; i < (int)m_inventory.size(); ++i) {
            if (m_inventory[i].id == itemId) {
                m_inventory[i].amount--;
                if (m_inventory[i].amount <= 0) {
                    m_inventory[i].id = -1;
                    m_inventory[i].amount = 0;
                }
                break;
            }
        }
        UIManager::getInstance().ShowItemNotification(itemId, -1);
    }
}

void GameManager::EatOneItem(int roleNum, int itemId, int where) {
    if (roleNum < 0 || roleNum >= m_roles.size()) return;
    if (itemId < 0 || itemId >= m_items.size()) return;

    Role& role = m_roles[roleNum];
    Item& item = m_items[itemId];
    
    if (where == 0) {
        if (item.getEquipType() == 0) {
            role.setCurrentHP(std::min((int)role.getMaxHP(), role.getCurrentHP() + item.getAddCurrentHP()));
            role.setCurrentMP(std::min((int)role.getMaxMP(), role.getCurrentMP() + item.getAddCurrentMP()));
            
            role.setMaxHP(std::min(MAX_HP, role.getMaxHP() + item.getAddMaxHP()));
            role.setMaxMP(std::min(MAX_MP, role.getMaxMP() + item.getAddMaxMP()));
            
            role.setPhyPower(std::min(MAX_PHYSICAL_POWER, role.getPhyPower() + item.getAddPhyPower()));
            role.setPoision(std::max(0, role.getPoision() - item.getAddPoi()));
            
            role.setSpeed(std::min(100, role.getSpeed() + item.getAddSpeed()));
            role.setAttack(std::min(100, role.getAttack() + item.getAddAttack()));
            role.setDefence(std::min(100, role.getDefence() + item.getAddDefence()));
            
            role.setMedcine(std::min(100, role.getMedcine() + item.getAddMedcine()));
            role.setMedPoi(std::min(100, role.getMedPoi() + item.getAddMedPoi()));
            role.setUsePoi(std::min(100, role.getUsePoi() + item.getAddUsePoi()));
            role.setDefPoi(std::min(100, role.getDefPoi() + item.getAddDefPoi()));
            
            role.setFist(std::min(100, role.getFist() + item.getAddFist()));
            role.setSword(std::min(100, role.getSword() + item.getAddSword()));
            role.setKnife(std::min(100, role.getKnife() + item.getAddKnife()));
            role.setUnusual(std::min(100, role.getUnusual() + item.getAddUnusual()));
            role.setHidWeapon(std::min(100, role.getHidWeapon() + item.getAddHidWeapon()));
        }
    }
}

int GameManager::GetRoleMedcine(int roleNum, bool checkEquip) {
    Role& role = getRole(roleNum);
    int result = role.getMedcine();
    if (role.getGongti() > -1) {
        int l = GetGongtiLevel(roleNum, role.getGongti());
        Magic& magic = getMagic(role.getGongti());
        if (l == magic.getMaxLevel()) {
            result += magic.getAddMedcine();
        }
    }
    if (checkEquip) {
        for (int i = 0; i < 5; ++i) {
            int itemId = role.getEquip(i);
            if (itemId >= 0) {
                Item& item = getItem(itemId);
                result += item.getAddMedcine();
            }
        }
    }
    return result;
}

int GameManager::GetRoleMedPoi(int roleNum, bool checkEquip) {
    Role& role = getRole(roleNum);
    int result = role.getMedPoi();
    if (role.getGongti() > -1) {
        int l = GetGongtiLevel(roleNum, role.getGongti());
        Magic& magic = getMagic(role.getGongti());
        if (l == magic.getMaxLevel()) {
            result += magic.getAddMedPoi();
        }
    }
    if (checkEquip) {
        for (int i = 0; i < 5; ++i) {
            int itemId = role.getEquip(i);
            if (itemId >= 0) {
                Item& item = getItem(itemId);
                result += item.getAddMedPoi();
            }
        }
    }
    return result;
}

int GameManager::GetRoleUsePoi(int roleNum, bool checkEquip) {
    Role& role = getRole(roleNum);
    int result = role.getUsePoi();
    if (role.getGongti() > -1) {
        int l = GetGongtiLevel(roleNum, role.getGongti());
        Magic& magic = getMagic(role.getGongti());
        if (l == magic.getMaxLevel()) {
            result += magic.getAddUsePoi();
        }
    }
    if (checkEquip) {
        for (int i = 0; i < 5; ++i) {
            int itemId = role.getEquip(i);
            if (itemId >= 0) {
                Item& item = getItem(itemId);
                result += item.getAddUsePoi();
            }
        }
    }
    return result;
}

int GameManager::GetRoleDefPoi(int roleNum, bool checkEquip) {
    Role& role = getRole(roleNum);
    int result = role.getDefPoi();
    if (role.getGongti() > -1) {
        int l = GetGongtiLevel(roleNum, role.getGongti());
        Magic& magic = getMagic(role.getGongti());
        if (l == magic.getMaxLevel()) {
            result += magic.getAddDefPoi();
        }
    }
    if (checkEquip) {
        for (int i = 0; i < 5; ++i) {
            int itemId = role.getEquip(i);
            if (itemId >= 0) {
                Item& item = getItem(itemId);
                result += item.getAddDefPoi();
            }
        }
    }
    return result;
}

int GameManager::GetRoleFist(int roleNum, bool checkEquip) {
    Role& role = getRole(roleNum);
    int result = role.getFist();
    if (role.getGongti() > -1) {
        int l = GetGongtiLevel(roleNum, role.getGongti());
        Magic& magic = getMagic(role.getGongti());
        if (l == magic.getMaxLevel()) {
            result += magic.getAddFist();
        }
    }
    if (checkEquip) {
        for (int i = 0; i < 5; ++i) {
            int itemId = role.getEquip(i);
            if (itemId >= 0) {
                Item& item = getItem(itemId);
                result += item.getAddFist();
            }
        }
    }
    return result;
}

int GameManager::GetRoleSword(int roleNum, bool checkEquip) {
    Role& role = getRole(roleNum);
    int result = role.getSword();
    if (role.getGongti() > -1) {
        int l = GetGongtiLevel(roleNum, role.getGongti());
        Magic& magic = getMagic(role.getGongti());
        if (l == magic.getMaxLevel()) {
            result += magic.getAddSword();
        }
    }
    if (checkEquip) {
        for (int i = 0; i < 5; ++i) {
            int itemId = role.getEquip(i);
            if (itemId >= 0) {
                Item& item = getItem(itemId);
                result += item.getAddSword();
            }
        }
    }
    return result;
}

int GameManager::GetRoleKnife(int roleNum, bool checkEquip) {
    Role& role = getRole(roleNum);
    int result = role.getKnife();
    if (role.getGongti() > -1) {
        int l = GetGongtiLevel(roleNum, role.getGongti());
        Magic& magic = getMagic(role.getGongti());
        if (l == magic.getMaxLevel()) {
            result += magic.getAddKnife();
        }
    }
    if (checkEquip) {
        for (int i = 0; i < 5; ++i) {
            int itemId = role.getEquip(i);
            if (itemId >= 0) {
                Item& item = getItem(itemId);
                result += item.getAddKnife();
            }
        }
    }
    return result;
}

int GameManager::GetRoleUnusual(int roleNum, bool checkEquip) {
    Role& role = getRole(roleNum);
    int result = role.getUnusual();
    if (role.getGongti() > -1) {
        int l = GetGongtiLevel(roleNum, role.getGongti());
        Magic& magic = getMagic(role.getGongti());
        if (l == magic.getMaxLevel()) {
            result += magic.getAddUnusual();
        }
    }
    if (checkEquip) {
        for (int i = 0; i < 5; ++i) {
            int itemId = role.getEquip(i);
            if (itemId >= 0) {
                Item& item = getItem(itemId);
                result += item.getAddUnusual();
            }
        }
    }
    return result;
}

int GameManager::GetRoleHidWeapon(int roleNum, bool checkEquip) {
    Role& role = getRole(roleNum);
    int result = role.getHidWeapon();
    if (role.getGongti() > -1) {
        int l = GetGongtiLevel(roleNum, role.getGongti());
        Magic& magic = getMagic(role.getGongti());
        if (l == magic.getMaxLevel()) {
            result += magic.getAddHidWeapon();
        }
    }
    if (checkEquip) {
        for (int i = 0; i < 5; ++i) {
            int itemId = role.getEquip(i);
            if (itemId >= 0) {
                Item& item = getItem(itemId);
                result += item.getAddHidWeapon();
            }
        }
    }
    return result;
}

int GameManager::GetRoleAttack(int roleNum, bool checkEquip) {
    Role& role = getRole(roleNum);
    int result = role.getAttack();
    if (role.getGongti() > -1) {
        int l = GetGongtiLevel(roleNum, role.getGongti());
        Magic& magic = getMagic(role.getGongti());
        result += magic.getAddAtt(l);
    }
    if (checkEquip) {
        for (int i = 0; i < 5; ++i) {
            int itemId = role.getEquip(i);
            if (itemId >= 0) {
                Item& item = getItem(itemId);
                result += item.getAddAttack();
            }
        }
    }
    return result;
}

bool GameManager::CheckBattleEffect(int roleIdx, BattleEffectType type) {
    return CheckBattleEffect(roleIdx, static_cast<int>(type));
}

bool GameManager::CheckBattleEffect(int roleIdx, int stateId) {
    return GetEquipState(roleIdx, stateId) || GetGongtiState(roleIdx, stateId);
}

int GameManager::GetRoleDefence(int roleNum, bool checkEquip) {
    Role& role = getRole(roleNum);
    int result = role.getDefence();
    if (role.getGongti() > -1) {
        int l = GetGongtiLevel(roleNum, role.getGongti());
        Magic& magic = getMagic(role.getGongti());
        result += magic.getAddDef(l);
    }
    if (checkEquip) {
        for (int i = 0; i < 5; ++i) {
            int itemId = role.getEquip(i);
            if (itemId >= 0) {
                Item& item = getItem(itemId);
                result += item.getAddDefence();
            }
        }
    }
    return result;
}

int GameManager::GetRoleSpeed(int roleNum, bool checkEquip) {
    Role& role = getRole(roleNum);
    int result = role.getSpeed();
    if (role.getGongti() > -1) {
        int l = GetGongtiLevel(roleNum, role.getGongti());
        Magic& magic = getMagic(role.getGongti());
        result += magic.getAddSpd(l);
    }
    if (checkEquip) {
        for (int i = 0; i < 5; ++i) {
            int itemId = role.getEquip(i);
            if (itemId >= 0) {
                Item& item = getItem(itemId);
                result += item.getAddSpeed();
            }
        }
    }
    return result;
}

int GameManager::CheckEquipSet(int e0, int e1, int e2, int e3) {
    if (m_setNum.size() < 6) return -1;
    int result = -1;
    for (int i = 1; i <= 5; ++i) {
        if (m_setNum[i][0] != e0 && m_setNum[i][0] >= 0) continue;
        if (m_setNum[i][1] != e1 && m_setNum[i][1] >= 0) continue;
        if (m_setNum[i][2] != e2 && m_setNum[i][2] >= 0) continue;
        if (m_setNum[i][3] != e3 && m_setNum[i][3] >= 0) continue;
        result = i;
    }
    return result;
}

bool GameManager::GetEquipState(int roleIdx, int state) {
    if (roleIdx < 0 || roleIdx >= getRoleCount()) return false;
    Role& role = getRole(roleIdx);
    for (int i = 0; i < 5; ++i) {
        int itemId = role.getEquip(i);
        if (itemId >= 0) {
            Item& item = getItem(itemId);
            if (item.getBattleEffect() == state) return true;
        }
    }
    return false;
}

int GameManager::GetGongtiLevel(int roleIdx, int magicId) {
    if (magicId < 0) return 0;
    Role& role = getRole(roleIdx);
    int magicLevel = -1;
    for (int i = 0; i < 10; ++i) {
        if (role.getMagic(i) == magicId) {
            magicLevel = role.getMagLevel(i);
            break;
        }
    }
    Magic& magic = getMagic(magicId);
    return std::min((int)magic.getMaxLevel(), magicLevel / 100);
}

bool GameManager::GetGongtiState(int roleIdx, int state) {
    if (roleIdx < 0 || roleIdx >= getRoleCount()) return false;
    Role& role = getRole(roleIdx);
    int gongti = role.getGongti();
    if (gongti < 0) return false;
    Magic& magic = getMagic(gongti);
    if (magic.getMaxLevel() > GetGongtiLevel(roleIdx, gongti)) return false;
    return magic.getBattleState() == state;
}

void GameManager::SetGongti(int roleIdx, int magicId) {
    if (roleIdx < 0 || roleIdx >= getRoleCount()) return;
    Role& role = getRole(roleIdx);
    
    int oldGongti = role.getGongti();
    
    double hpRatio = (role.getMaxHP() > 0) ? (double)role.getCurrentHP() / role.getMaxHP() : 0;
    double mpRatio = (role.getMaxMP() > 0) ? (double)role.getCurrentMP() / role.getMaxMP() : 0;
    
    if (oldGongti > 0) {
        int l = GetGongtiLevel(roleIdx, oldGongti);
        Magic& oldMagic = getMagic(oldGongti);
        role.setMaxHP(role.getMaxHP() - oldMagic.getAddHP(l));
        role.setMaxMP(role.getMaxMP() - oldMagic.getAddMP(l));
    }
    
    role.setGongti(magicId);
    
    if (magicId > 0) {
        int l = GetGongtiLevel(roleIdx, magicId);
        Magic& newMagic = getMagic(magicId);
        role.setMaxHP(role.getMaxHP() + newMagic.getAddHP(l));
        role.setMaxMP(role.getMaxMP() + newMagic.getAddMP(l));
    }
    
    role.setCurrentHP(std::max((int)(role.getMaxHP() * hpRatio), 1));
    role.setCurrentMP(std::max((int)(role.getMaxMP() * mpRatio), 0));
}

void GameManager::JoinParty(int roleId) {
    if (roleId < 0 || roleId >= getRoleCount()) return;
    
    Role& role = getRole(roleId);
    
    // 先设置为TeamState=2 (预备加入状态)
    role.setTeamState(2);
    
    // 获取角色身上的物品并添加到队伍物品栏
    for (int i = 0; i < 4; i++) {
        int itemId = role.getTakingItem(i);
        int itemAmount = role.getTakingItemAmount(i);
        if (itemId >= 0 && itemAmount > 0) {
            AddItem(itemId, itemAmount);
            // 清空角色身上的物品
            role.setTakingItem(i, -1);
            role.setTakingItemAmount(i, 0);
        }
    }
    
    // 将角色加入队伍列表
    for (int i = 0; i < MAX_TEAM_SIZE; i++) {
        if (m_teamList[i] == roleId) {
            // 已经在队伍中
            role.setTeamState(1);
            break;
        } else if (m_teamList[i] < 0) {
            // 找到空位，加入队伍
            m_teamList[i] = roleId;
            role.setTeamState(1);
            std::cout << "[JoinParty] Role " << roleId << " (" << role.getName() << ") joined team at slot " << i << std::endl;
            break;
        }
    }
}

void GameManager::LeaveParty(int roleId) {
    if (roleId < 0 || roleId >= getRoleCount()) return;
    
    // 从队伍列表中移除
    for (int i = 0; i < MAX_TEAM_SIZE; i++) {
        if (m_teamList[i] == roleId) {
            m_teamList[i] = -1;
            std::cout << "[LeaveParty] Role " << roleId << " left team from slot " << i << std::endl;
            break;
        }
    }
    
    // 设置角色状态为不在队伍
    Role& role = getRole(roleId);
    role.setTeamState(0);
}

void GameManager::Rest() {
    for (int i = 0; i < 6; ++i) {
        int roleId = getTeamMember(i);
        if (roleId >= 0) {
            Role& role = getRole(roleId);
            if (role.getHurt() <= 33 && role.getPoision() <= 33) {
                role.setHurt(0);
                role.setPoision(0);
                role.setCurrentHP(role.getMaxHP());
                role.setCurrentMP(role.getMaxMP());
                role.setPhyPower(100);
            }
        }
    }
    for (int i = 0; i < getRoleCount(); ++i) {
        Role& role = getRole(i);
        if (role.getTeamState() == 2) {
            if (role.getHurt() <= 33 && role.getPoision() <= 33) {
                role.setHurt(0);
                role.setPoision(0);
                role.setCurrentHP(role.getMaxHP());
                role.setCurrentMP(role.getMaxMP());
                role.setPhyPower(100);
            }
        }
    }
}

void GameManager::RebuildShopsFromRaw() {
    m_shops.clear();
    const size_t shopBytes = 18 * sizeof(int16_t);
    if (m_shopRaw.size() < shopBytes) return;
    size_t count = m_shopRaw.size() / shopBytes;
    m_shops.resize(count);
    for (size_t s = 0; s < count; ++s) {
        std::memcpy(m_shops[s].data(), m_shopRaw.data() + s * shopBytes, shopBytes);
    }
}

int16_t GameManager::getShopData(int shopId, int index) const {
    if (shopId < 0 || shopId >= (int)m_shops.size()) return 0;
    if (index < 0 || index >= 18) return 0;
    return m_shops[shopId][index];
}

void GameManager::setShopData(int shopId, int index, int16_t value) {
    if (shopId < 0 || shopId >= (int)m_shops.size()) return;
    if (index < 0 || index >= 18) return;
    m_shops[shopId][index] = value;
}

void GameManager::loadSettings() {
    std::string configPath = FileLoader::getDataRoot() + "kys_config.ini";
    std::ifstream file(configPath);
    if (!file.is_open()) {
        file.open("kys_config.ini");
    }
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line)) {
        const size_t eqPos = line.find('=');
        if (eqPos == std::string::npos) continue;
        std::string key = line.substr(0, eqPos);
        std::string value = line.substr(eqPos + 1);
        while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
        while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.erase(value.begin());
        if (key == "battlemode" || key == "battleMode") {
            try {
                m_battleMode = std::min(2, std::max(0, std::stoi(value)));
            } catch (...) {}
        }
    }
}

void GameManager::saveBattleModeSetting() {
    std::string configPath = FileLoader::getDataRoot() + "kys_config.ini";
    std::vector<std::string> lines;
    std::ifstream in(configPath);
    if (!in.is_open()) in.open("kys_config.ini");
    bool found = false;
    if (in.is_open()) {
        std::string line;
        while (std::getline(in, line)) {
            if (line.rfind("battlemode", 0) == 0 || line.rfind("battleMode", 0) == 0) {
                lines.push_back("battlemode=" + std::to_string(m_battleMode));
                found = true;
            } else {
                lines.push_back(line);
            }
        }
    }
    if (!found) {
        lines.push_back("battlemode=" + std::to_string(m_battleMode));
    }
    std::ofstream out(configPath, std::ios::trunc);
    if (!out.is_open()) out.open("kys_config.ini", std::ios::trunc);
    if (!out.is_open()) return;
    for (const auto& line : lines) out << line << '\n';
}
