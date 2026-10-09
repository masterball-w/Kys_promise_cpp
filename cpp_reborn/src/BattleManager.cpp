#include "BattleManager.h"
#include "GameManager.h"
#include "SceneManager.h"
#include "UIManager.h"
#include "SoundManager.h"
#include "EventManager.h"
#include "FileLoader.h"
#include "TextManager.h"
#include "PicLoader.h"
#include "GraphicsUtils.h"
#include "GameTypes.h"
#include "BattleEffects.h"
#include "InputManager.h"
#include "GameHooks.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <iostream>
#include <cmath>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <unordered_map>
#include <vector>
#include "VirtualControls.h"
#ifdef _WIN32
#include <direct.h>
#define getcwd _getcwd
#else
#include <unistd.h>
#endif

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

// --- Helper Functions (Ported from kys_event.pas/kys_battle.pas) ---

static int GetMagicLevel(int rnum, int mnum) {
    if (rnum < 0) return 0;
    Role& role = GameManager::getInstance().getRole(rnum);
    for (int i = 0; i < 10; ++i) {
        if (role.getMagic(i) == mnum) {
            return role.getMagLevel(i);
        }
    }
    return -1;
}

/** Pascal: MagLevel div 100 + 1, clamped to 1..10 */
static int GetMagicBattleLevel(int rnum, int mnum) {
    int raw = GetMagicLevel(rnum, mnum);
    if (raw < 0) return 1;
    int level = raw / 100 + 1;
    if (level < 1) level = 1;
    if (level > 10) level = 10;
    return level;
}

static uint32_t PaletteToRgba(uint32_t color) {
    uint8_t a = (color >> 24) & 0xFF;
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;
    return (r << 24) | (g << 16) | (b << 8) | a;
}

static int GetGongtiLevel(int rnum, int mnum) {
    if (mnum < 0) return 0;
    int magicLevel = GetMagicLevel(rnum, mnum);
    Magic& magic = GameManager::getInstance().getMagic(mnum);
    return std::min((int)magic.getMaxLevel(), magicLevel / 100);
}

static int CheckEquipSet(int e0, int e1, int e2, int e3) {
    return GameManager::getInstance().CheckEquipSet(e0, e1, e2, e3);
}

static std::string PadNumber(int value, int width) {
    std::ostringstream ss;
    ss << std::setw(width) << std::setfill('0') << value;
    return ss.str();
}

static bool FileExists(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return f.good();
}

static void MoveCursorBy(int& x, int& y, int dx, int dy) {
    x = std::clamp(x + dx, 0, 63);
    y = std::clamp(y + dy, 0, 63);
}

static std::string ResolveDataPath(const std::string& relative) {
    std::string initial = FileLoader::getResourcePath(relative);
    if (FileExists(initial)) return initial;
    std::string resourcePath = FileLoader::getResourcePath("smp");
    std::string resourceDir = resourcePath;
    auto pos = resourceDir.find_last_of("/\\");
    if (pos != std::string::npos) {
        resourceDir = resourceDir.substr(0, pos);
    }
    std::string baseDir = resourceDir;
    if (baseDir.size() >= 8) {
        std::string tail = baseDir.substr(baseDir.size() - 8);
        if (tail == "resource") {
            if (baseDir.size() > 9 && (baseDir[baseDir.size() - 9] == '/' || baseDir[baseDir.size() - 9] == '\\')) {
                baseDir = baseDir.substr(0, baseDir.size() - 9);
            } else {
                baseDir = baseDir.substr(0, baseDir.size() - 8);
            }
        }
    }
    char cwdBuf[1024] = {};
    std::string cwd;
    if (getcwd(cwdBuf, sizeof(cwdBuf))) cwd = cwdBuf;
    std::vector<std::string> prefixes;
    if (!cwd.empty()) prefixes.push_back(cwd);
    if (!resourceDir.empty()) prefixes.push_back(resourceDir);
    if (!baseDir.empty()) prefixes.push_back(baseDir);
    prefixes.push_back("");
    prefixes.push_back("build/Debug");
    prefixes.push_back("build/Release");
    prefixes.push_back("cpp_reborn/build/Debug");
    prefixes.push_back("cpp_reborn/build/Release");
    prefixes.push_back("../build/Debug");
    prefixes.push_back("../build/Release");
    prefixes.push_back("../cpp_reborn/build/Debug");
    prefixes.push_back("../cpp_reborn/build/Release");
    for (const auto& prefix : prefixes) {
        std::string candidate = prefix.empty() ? relative : (prefix + "/" + relative);
        if (FileExists(candidate)) return candidate;
    }
    return initial;
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
        // Keep a visible floor so low poison still pulses green on the body/head.
        pulse.green = std::max(24, (poison * wave) / denom);
        if (pulse.green > 120) pulse.green = 120;
    } else if (effect == 1) {
        int denom = (effectIndex == 1) ? 150 : 100;
        pulse.red = std::max(24, (hurt * wave) / denom);
        if (pulse.red > 120) pulse.red = 120;
    } else {
        pulse.gray = std::max(10, (frozen * wave) / 500);
        if (pulse.gray > 80) pulse.gray = 80;
    }
    return pulse;
}

static void BlitPicToScreen(const PicImage& pic, int x, int y) {
    if (!pic.surface) return;
    SDL_Surface* screen = GameManager::getInstance().getScreenSurface();
    if (!screen) return;
    SDL_Rect dst = { x - pic.x, y - pic.y, 0, 0 };
    SDL_SetSurfaceBlendMode(pic.surface, SDL_BLENDMODE_BLEND);
    SDL_BlitSurface(pic.surface, nullptr, screen, &dst);
}

static std::unordered_map<int, PicImage> g_battleIdlePicCache;

static PicImage* GetBattleIdlePic(int headNum, int face) {
    int key = headNum * 10 + face;
    auto it = g_battleIdlePicCache.find(key);
    if (it != g_battleIdlePicCache.end()) return &it->second;
    PicImage pic;
    auto tryLoad = [&](const std::string& relative) -> bool {
        std::string path = ResolveDataPath(relative);
        if (!FileExists(path)) return false;
        int count = PicLoader::getPicCount(path);
        if (count <= 0) return false;
        int frame = 0;
        int perFace = count / 4;
        if (perFace > 0) {
            frame = face * perFace;
            if (frame < 0 || frame >= count) frame = 0;
        } else {
            frame = std::clamp(face, 0, count - 1);
        }
        pic = PicLoader::loadPic(path, frame);
        return pic.surface != nullptr;
    };
    for (int mode = 0; mode <= 4; ++mode) {
        std::string relative = "fight/" + PadNumber(headNum, 3) + "/" + PadNumber(mode, 2) + ".pic";
        if (tryLoad(relative)) {
            auto inserted = g_battleIdlePicCache.emplace(key, std::move(pic));
            return &inserted.first->second;
        }
    }
    return nullptr;
}

static void ClearBattleIdlePicCache() {
    for (auto& kv : g_battleIdlePicCache) {
        PicLoader::freePic(kv.second);
    }
    g_battleIdlePicCache.clear();
}

static int GetRoleAttack(int rnum, bool equip) {
    Role& role = GameManager::getInstance().getRole(rnum);
    int result = role.getAttack();
    
    if (role.getGongti() > -1) {
        int l = GetGongtiLevel(rnum, role.getGongti());
        Magic& magic = GameManager::getInstance().getMagic(role.getGongti());
        result += magic.getAddAtt(l);
    }
    
    if (equip) {
        for (int i = 0; i < 5; ++i) {
            int itemId = role.getEquip(i);
            if (itemId >= 0) {
                Item& item = GameManager::getInstance().getItem(itemId);
                result += item.getAddAttack();
            }
        }
    }
    return result;
}

static int GetRoleDefence(int rnum, bool equip) {
    Role& role = GameManager::getInstance().getRole(rnum);
    int result = role.getDefence();
    
    if (role.getGongti() > -1) {
        int l = GetGongtiLevel(rnum, role.getGongti());
        Magic& magic = GameManager::getInstance().getMagic(role.getGongti());
        result += magic.getAddDef(l);
    }
    
    if (equip) {
        for (int i = 0; i < 5; ++i) {
            int itemId = role.getEquip(i);
            if (itemId >= 0) {
                Item& item = GameManager::getInstance().getItem(itemId);
                result += item.getAddDefence();
            }
        }
    }
    return result;
}

static int GetRoleSpeed(int rnum, bool equip) {
    Role& role = GameManager::getInstance().getRole(rnum);
    int result = role.getSpeed();
    
    if (role.getGongti() > -1) {
        int l = GetGongtiLevel(rnum, role.getGongti());
        Magic& magic = GameManager::getInstance().getMagic(role.getGongti());
        result += magic.getAddSpd(l);
    }
    
    if (equip) {
        for (int i = 0; i < 5; ++i) {
            int itemId = role.getEquip(i);
            if (itemId >= 0) {
                Item& item = GameManager::getInstance().getItem(itemId);
                result += item.getAddSpeed();
            }
        }
    }
    return result;
}

static int GetRoleFist(int rnum, bool equip) {
    Role& role = GameManager::getInstance().getRole(rnum);
    int result = role.getFist();
    
    if (role.getGongti() > -1) {
        int l = GetGongtiLevel(rnum, role.getGongti());
        Magic& magic = GameManager::getInstance().getMagic(role.getGongti());
        if (l == magic.getMaxLevel()) {
            result += magic.getAddFist();
        }
    }
    
    if (equip) {
        for (int i = 0; i < 5; ++i) {
            int itemId = role.getEquip(i);
            if (itemId >= 0) {
                Item& item = GameManager::getInstance().getItem(itemId);
                result += item.getAddFist();
            }
        }
    }
    return result;
}

static int GetRoleSword(int rnum, bool equip) {
    Role& role = GameManager::getInstance().getRole(rnum);
    int result = role.getSword();
    
    if (role.getGongti() > -1) {
        int l = GetGongtiLevel(rnum, role.getGongti());
        Magic& magic = GameManager::getInstance().getMagic(role.getGongti());
        if (l == magic.getMaxLevel()) {
            result += magic.getAddSword();
        }
    }
    
    if (equip) {
        for (int i = 0; i < 5; ++i) {
            int itemId = role.getEquip(i);
            if (itemId >= 0) {
                Item& item = GameManager::getInstance().getItem(itemId);
                result += item.getAddSword();
            }
        }
    }
    return result;
}

static int GetRoleKnife(int rnum, bool equip) {
    Role& role = GameManager::getInstance().getRole(rnum);
    int result = role.getKnife();
    
    if (role.getGongti() > -1) {
        int l = GetGongtiLevel(rnum, role.getGongti());
        Magic& magic = GameManager::getInstance().getMagic(role.getGongti());
        if (l == magic.getMaxLevel()) {
            result += magic.getAddKnife();
        }
    }
    
    if (equip) {
        for (int i = 0; i < 5; ++i) {
            int itemId = role.getEquip(i);
            if (itemId >= 0) {
                Item& item = GameManager::getInstance().getItem(itemId);
                result += item.getAddKnife();
            }
        }
    }
    return result;
}

static int GetRoleUnusual(int rnum, bool equip) {
    Role& role = GameManager::getInstance().getRole(rnum);
    int result = role.getUnusual();
    
    if (role.getGongti() > -1) {
        int l = GetGongtiLevel(rnum, role.getGongti());
        Magic& magic = GameManager::getInstance().getMagic(role.getGongti());
        if (l == magic.getMaxLevel()) {
            result += magic.getAddUnusual();
        }
    }
    
    if (equip) {
        for (int i = 0; i < 5; ++i) {
            int itemId = role.getEquip(i);
            if (itemId >= 0) {
                Item& item = GameManager::getInstance().getItem(itemId);
                result += item.getAddUnusual();
            }
        }
    }
    return result;
}


// Port of CalNewHurtValue
static int CalNewHurtValue(int lv, int minVal, int maxVal, int proportion) {
    if (proportion == 0) proportion = 100;
    double p = proportion / 1000.0;
    double n = std::pow((double)(maxVal - minVal), 1.0 / p) / 9.0;
    return (int)(std::round(std::pow((lv * n), p)) + minVal);
}

static int GetRoleKnowledge(int rnum) {
    if (rnum < 0) return 0;
    return GameManager::getInstance().getRole(rnum).getKnowledge();
}

static int GetRoleLevel(int rnum) {
    if (rnum < 0) return 0;
    return GameManager::getInstance().getRole(rnum).getLevel();
}

static int GetRoleDifficulty(int rnum) {
    // Usually Difficulty is stored in Role 0? Or global?
    // Pascal: rrole[0].difficulty
    return GameManager::getInstance().getRole(0).getDifficulty(); // Assuming Role 0 holds it
}

static int GetRoleMedcine(int rnum, bool equip) {
    Role& role = GameManager::getInstance().getRole(rnum);
    int result = role.getMedcine();
    if (role.getGongti() > -1) {
        int l = GetGongtiLevel(rnum, role.getGongti());
        Magic& magic = GameManager::getInstance().getMagic(role.getGongti());
        if (l == magic.getMaxLevel()) {
            result += magic.getAddMedcine();
        }
    }
    if (equip) {
        for (int i = 0; i < 5; ++i) {
            int itemId = role.getEquip(i);
            if (itemId >= 0) {
                Item& item = GameManager::getInstance().getItem(itemId);
                result += item.getAddMedcine();
            }
        }
    }
    return result;
}

static int GetRoleMedPoi(int rnum, bool equip) {
    Role& role = GameManager::getInstance().getRole(rnum);
    int result = role.getMedPoi();
    if (role.getGongti() > -1) {
        int l = GetGongtiLevel(rnum, role.getGongti());
        Magic& magic = GameManager::getInstance().getMagic(role.getGongti());
        if (l == magic.getMaxLevel()) {
            result += magic.getAddMedPoi();
        }
    }
    if (equip) {
        for (int i = 0; i < 5; ++i) {
            int itemId = role.getEquip(i);
            if (itemId >= 0) {
                Item& item = GameManager::getInstance().getItem(itemId);
                result += item.getAddMedPoi();
            }
        }
    }
    return result;
}

static int GetRoleDefPoi(int rnum, bool equip) {
    Role& role = GameManager::getInstance().getRole(rnum);
    int result = role.getDefPoi();
    if (role.getGongti() > -1) {
        int l = GetGongtiLevel(rnum, role.getGongti());
        Magic& magic = GameManager::getInstance().getMagic(role.getGongti());
        if (l == magic.getMaxLevel()) {
            result += magic.getAddDefPoi();
        }
    }
    if (equip) {
        for (int i = 0; i < 5; ++i) {
            int itemId = role.getEquip(i);
            if (itemId >= 0) {
                Item& item = GameManager::getInstance().getItem(itemId);
                result += item.getAddDefPoi();
            }
        }
    }
    return result;
}

static int GetRoleUsePoi(int rnum, bool equip) {
    Role& role = GameManager::getInstance().getRole(rnum);
    int result = role.getUsePoi();
    if (role.getGongti() > -1) {
        int l = GetGongtiLevel(rnum, role.getGongti());
        Magic& magic = GameManager::getInstance().getMagic(role.getGongti());
        if (l == magic.getMaxLevel()) {
            result += magic.getAddUsePoi();
        }
    }
    if (equip) {
        for (int i = 0; i < 5; ++i) {
            int itemId = role.getEquip(i);
            if (itemId >= 0) {
                Item& item = GameManager::getInstance().getItem(itemId);
                result += item.getAddUsePoi();
            }
        }
    }
    return result;
}

static int GetRoleAttPoi(int rnum, bool equip) {
    Role& role = GameManager::getInstance().getRole(rnum);
    int result = role.getAttPoi();
    if (equip) {
        for (int i = 0; i < 5; ++i) {
            int itemId = role.getEquip(i);
            if (itemId >= 0) {
                Item& item = GameManager::getInstance().getItem(itemId);
                result += item.getAddAttPoi();
            }
        }
    }
    return result;
}

static int GetRoleHidWeapon(int rnum, bool equip) {
    Role& role = GameManager::getInstance().getRole(rnum);
    return role.getHidWeapon();
}

BattleManager::BattleManager()
    : m_battleRunning(false),
      m_battleResult(0),
      m_currentRoleIndex(0),
      m_cursorX(0),
      m_cursorY(0),
      m_showMoveRange(false),
      m_showAttackRange(false),
      m_highlightRoleIndex(-1),
      m_forceAutoBattle(false),
      m_forceAutoBattleFrameLimit(0),
      m_forceAutoBattleFrameCount(0),
      m_exitAutoRequested(false),
      m_actionAnimRoleIndex(-1) {
}

BattleManager& BattleManager::getInstance() {
    static BattleManager instance;
    return instance;
}

bool BattleManager::Init() {
    return true;
}

void BattleManager::setForceAutoBattle(bool enabled) {
    m_forceAutoBattle = enabled;
}

void BattleManager::setForceAutoBattleFrameLimit(int maxFrames) {
    m_forceAutoBattleFrameLimit = std::max(0, maxFrames);
    m_forceAutoBattleFrameCount = 0;
}

bool BattleManager::LoadWarData(int battleId) {
    auto data = FileLoader::loadFile("War.sta");
    if (data.empty()) return false;
    
    const size_t recordSize = 312;
    const size_t recordCount = data.size() / recordSize;
    if (recordCount == 0) return false;
    if (battleId < 0) return false;

    auto loadRecord = [&](size_t index) -> bool {
        if (index >= recordCount) return false;
        const uint8_t* ptr = data.data() + index * recordSize;
        std::vector<int16_t> warVec(156);
        std::memcpy(warVec.data(), ptr, recordSize);
        m_warData.setDataVector(warVec);
        return true;
    };

    size_t targetIndex = static_cast<size_t>(battleId);
    if (targetIndex < recordCount) {
        if (loadRecord(targetIndex) && m_warData.getBattleNum() == battleId) return true;
    }
    
    for (size_t i = 0; i < recordCount; ++i) {
        int16_t battleNum = 0;
        std::memcpy(&battleNum, data.data() + i * recordSize, sizeof(int16_t));
        if (battleNum == battleId) {
            return loadRecord(i);
        }
    }

    if (targetIndex < recordCount) {
        return loadRecord(targetIndex);
    }
    
    std::cerr << "Battle ID " << battleId << " out of range" << std::endl;
    return false;
}

bool BattleManager::LoadBattleField(int fieldNum) {
    int32_t offset = 0;
    if (fieldNum > 0) {
        auto idxData = FileLoader::loadFile("warfld.idx");
        if (idxData.empty()) return false;
        const int32_t* offsets = reinterpret_cast<const int32_t*>(idxData.data());
        int fieldIndex = fieldNum - 1;
        if (fieldIndex < 0 || (fieldIndex + 1) * 4 > idxData.size()) return false;
        offset = offsets[fieldIndex];
    }
    
    auto grpData = FileLoader::loadFile("warfld.grp");
    if (grpData.empty()) return false;
    
    size_t grpSize = grpData.size();
    if (offset < 0) return false;
    size_t base = static_cast<size_t>(offset);
    size_t len = 16384;
    if (base + len > grpSize) return false;
    
    const uint8_t* pData = grpData.data() + base;
    
    for(int i=0; i<8; i++) {
        for(int x=0; x<64; x++) {
            for(int y=0; y<64; y++) {
                if (i == 2 || i == 4 || i == 5) m_battleField[i][x][y] = -1;
                else m_battleField[i][x][y] = 0;
            }
        }
    }
                
    const int16_t* pInt = reinterpret_cast<const int16_t*>(pData);
    for (int x = 0; x < 64; x++) {
        for (int y = 0; y < 64; y++) {
            m_battleField[0][x][y] = pInt[x * 64 + y];
            m_battleField[1][x][y] = pInt[64 * 64 + x * 64 + y];
        }
    }
    return true;
}

std::vector<int> BattleManager::SelectTeamMembers(const std::vector<int>& candidates) {
    std::vector<int> selected;
    if (candidates.empty()) return selected;
    uint32_t mask = 0;
    int menu = 0;
    int memberCount = static_cast<int>(candidates.size());
    int max = memberCount + 1;
    SDL_Event event;
    while (true) {
        RenderBattle();
        int boxX = 180;
        int boxY = 60;
        int boxW = 280;
        int boxH = (max + 1) * 22 + 24;
        UIManager::getInstance().DrawRectangle(boxX, boxY, boxW, boxH, 0x000000, 0xFFFFFFFF, 30);
        UIManager::getInstance().DrawShadowTextUtf8("选择参战队员", boxX + 8, boxY + 6, 0xFFFFFFFF, 0x000000FF);
        uint32_t allColor = (menu == 0) ? 0xFFFF00FF : 0xFFFFFFFF;
        UIManager::getInstance().DrawShadowTextUtf8("    全员出战", boxX + 10, boxY + 30, allColor, 0x000000FF);
        for (int i = 0; i < memberCount; ++i) {
            int roleId = candidates[i];
            std::string nameUtf8 = TextManager::getInstance().gbkToUtf8(GameManager::getInstance().getRole(roleId).getName());
            bool chosen = (mask & (1u << (i + 1))) != 0;
            std::string line = std::string(chosen ? "[x] " : "[ ] ") + nameUtf8;
            uint32_t color = (menu == i + 1) ? 0xFFFF00FF : 0xFFFFFFFF;
            UIManager::getInstance().DrawShadowTextUtf8(line, boxX + 10, boxY + 30 + (i + 1) * 22, color, 0x000000FF);
        }
        uint32_t endColor = (menu == max) ? 0xFFFF00FF : 0xFFFFFFFF;
        UIManager::getInstance().DrawShadowTextUtf8("    开始战斗", boxX + 10, boxY + 30 + max * 22, endColor, 0x000000FF);
        UIManager::getInstance().UpdateScreen();
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                m_battleRunning = false;
                GameManager::getInstance().Quit();
                return selected;
            }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_UP || event.key.key == SDLK_LEFT) {
                    menu -= 1;
                    if (menu < 0) menu = max;
                } else if (event.key.key == SDLK_DOWN || event.key.key == SDLK_RIGHT) {
                    menu += 1;
                    if (menu > max) menu = 0;
                } else if (event.key.key == SDLK_SPACE) {
                    if (menu == 0) {
                        for (int i = 0; i < memberCount; ++i) {
                            mask ^= (1u << (i + 1));
                        }
                    } else if (menu < max) {
                        mask ^= (1u << menu);
                    } else if (menu == max) {
                        if (mask != 0) {
                            goto selection_done;
                        }
                    }
                } else if (event.key.key == SDLK_RETURN) {
                    if (menu == 0) {
                        for (int i = 0; i < memberCount; ++i) {
                            mask ^= (1u << (i + 1));
                        }
                    } else if (menu == max) {
                        if (mask != 0) {
                            goto selection_done;
                        }
                    } else {
                        mask ^= (1u << menu);
                    }
                } else if (event.key.key == SDLK_ESCAPE) {
                    goto selection_done;
                }
            }
        }
        SDL_Delay(10);
    }
selection_done:
    for (int i = 0; i < memberCount; ++i) {
        if (mask & (1u << (i + 1))) {
            selected.push_back(candidates[i]);
        }
    }
    if (selected.empty()) selected.push_back(candidates[0]);
    return selected;
}

bool BattleManager::StartBattle(int battleId, int getExp) {
    m_getExp = getExp;
    m_battleId = battleId;
    std::cout << "Starting Battle: " << battleId << " GetExp: " << getExp << std::endl;
    ClearBattleIdlePicCache();
    
    if (!LoadWarData(battleId)) {
        std::cerr << "Failed to load WarData" << std::endl;
        return false;
    }
    
    if (!LoadBattleField(m_warData.getBattleMap())) {
        std::cerr << "Failed to load BattleField" << std::endl;
    }

    ClearGongtiStackAttack();
    m_battleRoles.clear();
    std::vector<int> candidates;
    const auto& team = GameManager::getInstance().getTeamList();
    for (int roleId : team) {
        if (roleId < 0) continue;
        if (std::find(candidates.begin(), candidates.end(), roleId) == candidates.end()) {
            candidates.push_back(roleId);
        }
    }
    bool hasSelectableSlot = false;
    for (int i = 0; i < 12; ++i) {
        if (m_warData.getMate(i) < 0) {
            hasSelectableSlot = true;
            break;
        }
    }
    bool anyAuto = false;
    for (int i = 0; i < 12; ++i) {
        int roleId = m_warData.getAutoMate(i);
        if (roleId >= 0) {
            anyAuto = true;
        }
    }
    std::vector<int> selectedRoles;
    if (!anyAuto) {
        if (hasSelectableSlot && candidates.size() > 1) {
            selectedRoles = SelectTeamMembers(candidates);
        } else {
            selectedRoles = candidates;
        }
        if (selectedRoles.empty() && !candidates.empty()) {
            selectedRoles.push_back(candidates[0]);
        }
    }
    bool hasPlayer = false;
    std::vector<int> used;
    auto addTeamRole = [&](int roleId, int x, int y) {
        if (roleId < 0) return;
        if (std::find(used.begin(), used.end(), roleId) != used.end()) return;
        BattleRole br;
        br.setRNum(roleId);
        br.setTeam(0);
        br.setX(x);
        br.setY(y);
        Role& rData = GameManager::getInstance().getRole(roleId);
        br.setPic(-1);
        br.setSpeed(rData.getSpeed());
        br.setKnowledge(rData.getKnowledge());
        br.setProgress(0);
        br.setDead(0);
        br.setShow(0);
        br.setFace(2);
        br.setShowNumber(-1);
        br.setAuto(-1);
        br.setRound(1);
        br.setLifeAdd(0);
        m_battleRoles.push_back(br);
        used.push_back(roleId);
        if (roleId == 0) hasPlayer = true;
    };

    if (anyAuto) {
        for (int i = 0; i < 12; ++i) {
            int roleId = m_warData.getAutoMate(i);
            if (roleId >= 0) {
                addTeamRole(roleId, m_warData.getMateY(i), m_warData.getMateX(i));
            }
        }
    } else {
        int selectedIndex = 0;
        for (int i = 0; i < 12; ++i) {
            int roleId = m_warData.getMate(i);
            if (roleId < 0) {
                if (selectedIndex < static_cast<int>(selectedRoles.size())) {
                    roleId = selectedRoles[selectedIndex++];
                }
            }
            if (roleId >= 0) {
                addTeamRole(roleId, m_warData.getMateY(i), m_warData.getMateX(i));
            }
        }
    }

    if (!hasPlayer) {
        addTeamRole(0, m_warData.getMateY(0), m_warData.getMateX(0));
    }

    // 2. Enemies
    for (int i = 0; i < 30; ++i) {
        int roleId = m_warData.getEnemy(i);
        if (roleId >= 0) {
            BattleRole br;
            br.setRNum(roleId);
            br.setTeam(1);
            br.setX(m_warData.getEnemyY(i));
            br.setY(m_warData.getEnemyX(i));
            Role& rData = GameManager::getInstance().getRole(roleId);
            br.setPic(rData.getHeadNum());
            br.setSpeed(rData.getSpeed());
            br.setKnowledge(rData.getKnowledge());
            br.setProgress(0);
            br.setDead(0);
            br.setShow(0);
            br.setFace(1);
            br.setShowNumber(-1);
            br.setAuto(-1);
            br.setRound(1);
            br.setLifeAdd(0);
            m_battleRoles.push_back(br);
        }
    }

    // Place Roles
    for (int i = 0; i < m_battleRoles.size(); ++i) {
        BattleRole& r = m_battleRoles[i];
        if (r.getX() >= 0 && r.getX() < 64 && r.getY() >= 0 && r.getY() < 64) {
            m_battleField[2][r.getX()][r.getY()] = i;
        }
        if (r.getRound() <= 0) r.setRound(1);
    }

    m_battleRunning = true;
    m_forceAutoBattleFrameCount = 0;
    RunBattle();
    ClearBattleIdlePicCache();
    
    return (m_battleResult == 1); 
}

static const uint16_t LEVEL_UP_LIST[30] = {
    50, 150, 300, 500, 750, 1050, 1400, 1800, 2250, 2750, 
    3300, 3900, 4550, 5250, 6000, 6800, 7650, 8550, 9500, 10500, 
    11550, 12650, 13800, 15000, 16250, 17550, 18900, 20300, 21750, 23250
};
constexpr int MAX_LEVEL = 30;

void BattleManager::AddExp() {
    int levels = 0;
    int amount = 0;
    for (const auto& role : m_battleRoles) {
        if (role.getTeam() == 0 && role.getDead() == 0 && role.getRNum() >= 0) {
            Role& r = GameManager::getInstance().getRole(role.getRNum());
            levels += r.getLevel();
            amount++;
        }
    }

    int battleExp = m_warData.getExp();

    for (auto& role : m_battleRoles) {
        if (role.getRNum() >= 0 && role.getTeam() == 0) {
            Role& r = GameManager::getInstance().getRole(role.getRNum());
            int basicValue = role.getExpGot();
            int addItem = 0;

            if (role.getDead() == 0) {
                if (amount == 1) {
                    basicValue += battleExp;
                } else if (levels > 0 && amount > 1) {
                    double factor = (1.0 - (double)r.getLevel() / levels) / (amount - 1);
                    basicValue += (int)(factor * battleExp);
                }
            }

            int add = basicValue;
            addItem = basicValue / 5 * 4;
            if (GetPetSkill(1, 3)) {
                add = (int)(basicValue * 1.5);
                addItem = (int)(basicValue * 1.5);
            } else if (GetPetSkill(1, 1) && role.getRNum() == 0) {
                add = (int)(basicValue * 1.5);
                addItem = (int)(basicValue * 1.5);
            }

            int currentExp = r.getExp();
            r.setExp(std::min(currentExp + add, 65535));

            int currentGongtiExp = r.getGongtiExam();
            r.setGongtiExam(std::min(currentGongtiExp + add * 4 / 5, 65535));
            
            int currentBookExp = r.getExpForBook();
            r.setExpForBook(std::min(currentBookExp + addItem, 65535));
            
            std::cout << "Role " << role.getRNum() << " gained " << add << " Exp." << std::endl;
        }
    }
}

void BattleManager::CheckLevelUp() {
    int maxLevel = GameManager::getInstance().getMaxLevel();
    // Pascal LevelUp: aptitude-based HP/MP growth + odd-level Atk/Spd/Def + skill ticks
    for (int bi = 0; bi < (int)m_battleRoles.size(); ++bi) {
        BattleRole& br = m_battleRoles[bi];
        if (br.getTeam() != 0 || br.getRNum() < 0) continue;
        Role& r = GameManager::getInstance().getRole(br.getRNum());
        int rnum = br.getRNum();

        while (r.getLevel() < maxLevel &&
               r.getExp() >= LEVEL_UP_LIST[std::min(r.getLevel() - 1, 29)]) {
            r.setExp(r.getExp() - LEVEL_UP_LIST[std::min(r.getLevel() - 1, 29)]);
            r.setLevel(r.getLevel() + 1);

            int addPool = (150 - r.getAptitude()) / 10 + (std::rand() % 3) + 1;
            r.setMaxHP(std::min(MAX_HP, r.getMaxHP() + addPool));
            r.setCurrentHP(r.getMaxHP());
            r.setMaxMP(std::min(MAX_MP, r.getMaxMP() + addPool));
            r.setCurrentMP(r.getMaxMP());

            if (r.getLevel() % 2 == 1) {
                r.setAttack(r.getAttack() + 1);
                r.setSpeed(r.getSpeed() + 1);
                r.setDefence(r.getDefence() + 1);
            }

            if (GetRoleMedcine(rnum, false) >= 20) r.setMedcine(r.getMedcine() + 1);
            if (GetRoleUsePoi(rnum, false) >= 20) r.setUsePoi(r.getUsePoi() + 1);
            if (GetRoleMedPoi(rnum, false) >= 20) r.setMedPoi(r.getMedPoi() + 1);
            if (GetRoleFist(rnum, false) >= 20) r.setFist(std::min(200, r.getFist() + 1));
            if (GetRoleSword(rnum, false) >= 20) r.setSword(std::min(200, r.getSword() + 1));
            if (GetRoleKnife(rnum, false) >= 20) r.setKnife(std::min(200, r.getKnife() + 1));
            if (GetRoleUnusual(rnum, false) >= 20) r.setUnusual(std::min(200, r.getUnusual() + 1));
            if (GetRoleHidWeapon(rnum, false) >= 20) r.setHidWeapon(std::min(200, r.getHidWeapon() + 1));

            r.setPhyPower(MAX_PHYSICAL_POWER);
            r.setHurt(0);
            r.setPoision(0);

            std::cout << "Role " << rnum << " Level Up to " << r.getLevel() << std::endl;
            if (br.getTeam() == 0) {
                UIManager::getInstance().ShowStatus(rnum);
            }
        }
    }
}

void BattleManager::RestoreRoleStatus() {
    ClearGongtiStackAttack();

    for (const auto& role : m_battleRoles) {
        int rnum = role.getRNum();
        if (rnum < 0) continue;
        
        Role& r = GameManager::getInstance().getRole(rnum);
        
        if (r.getTeamState() == 1 || r.getTeamState() == 2) {
             int hp = r.getCurrentHP() + r.getMaxHP() / 2;
             if (hp <= 0) hp = 1;
             if (hp > r.getMaxHP()) hp = r.getMaxHP();
             r.setCurrentHP(hp);
             
             int mp = r.getCurrentMP() + r.getMaxMP() / 20;
             if (mp > r.getMaxMP()) mp = r.getMaxMP();
             r.setCurrentMP(mp);
             
             int phy = r.getPhyPower() + MAX_PHYSICAL_POWER / 10;
             if (phy > MAX_PHYSICAL_POWER) phy = MAX_PHYSICAL_POWER;
             r.setPhyPower(phy);
        } else {
             r.setAngry(0);
             r.setHurt(0);
             r.setPoision(0);
             r.setCurrentHP(r.getMaxHP());
             r.setCurrentMP(r.getMaxMP());
             r.setPhyPower(MAX_PHYSICAL_POWER * 9 / 10);
        }
    }
}

void BattleManager::ClearGongtiStackAttack() {
    for (auto& br : m_battleRoles) {
        int stacks = br.getGongtiAtkStacks();
        if (stacks <= 0) continue;
        int rnum = br.getRNum();
        if (rnum >= 0) {
            Role& r = GameManager::getInstance().getRole(rnum);
            int atk = static_cast<int>(r.getAttack()) - stacks * 10;
            r.setAttack(static_cast<int16_t>(std::max(0, atk)));
        }
        br.setGongtiAtkStacks(0);
        br.setGongtiAtkStopped(false);
    }
}

void BattleManager::ApplyGongtiStackAttack(int roleIdx) {
    if (roleIdx < 0 || roleIdx >= (int)m_battleRoles.size()) return;
    BattleRole& actor = m_battleRoles[roleIdx];
    int rnum = actor.getRNum();
    if (rnum < 0 || actor.getDead()) return;

    const bool has26 = GameManager::getInstance().CheckBattleEffect(
        rnum, BattleEffectType::Stack_Attack);

    if (has26) {
        if (!actor.isGongtiAtkStopped() && actor.getGongtiAtkStacks() < 10) {
            actor.setGongtiAtkStacks(actor.getGongtiAtkStacks() + 1);
            Role& role = GameManager::getInstance().getRole(rnum);
            role.setAttack(static_cast<int16_t>(role.getAttack() + 10));
            for (auto& br : m_battleRoles) br.setShowNumber(-1);
            actor.setShowNumber(10);
            ShowHurtValue("攻击", 0x05FFFFFF, 0x07FFFFFF);
        }
    } else if (actor.getGongtiAtkStacks() > 0) {
        // Mid-battle loss of state 26: keep bonus, stop further stacks
        actor.setGongtiAtkStopped(true);
    }
}

void BattleManager::ApplyGongtiAuraPoison(int roleIdx) {
    if (roleIdx < 0 || roleIdx >= (int)m_battleRoles.size()) return;
    BattleRole& actor = m_battleRoles[roleIdx];
    int rnum = actor.getRNum();
    if (rnum < 0 || actor.getDead()) return;
    if (!GameManager::getInstance().CheckBattleEffect(rnum, BattleEffectType::Aura_Poison)) {
        return;
    }

    const int ax = actor.getX();
    const int ay = actor.getY();
    bool any = false;
    for (auto& br : m_battleRoles) br.setShowNumber(-1);

    for (int i = 0; i < (int)m_battleRoles.size(); ++i) {
        if (i == roleIdx) continue;
        BattleRole& enemy = m_battleRoles[i];
        if (enemy.getDead() || enemy.getRNum() < 0) continue;
        if (enemy.getTeam() == actor.getTeam()) continue;
        // 7x7 centered on actor => Chebyshev distance <= 3
        if (std::abs(enemy.getX() - ax) > 3 || std::abs(enemy.getY() - ay) > 3) continue;

        Role& tData = GameManager::getInstance().getRole(enemy.getRNum());
        if (GameManager::getInstance().CheckBattleEffect(enemy.getRNum(), BattleEffectType::Ignore_Debuff) ||
            CheckEquipSet(tData.getEquip(0), tData.getEquip(1), tData.getEquip(2), tData.getEquip(3)) == 4) {
            continue;
        }

        int newPoi = std::min(99, static_cast<int>(tData.getPoision()) + 10);
        int added = newPoi - tData.getPoision();
        if (added <= 0) continue;
        tData.setPoision(static_cast<int16_t>(newPoi));
        enemy.setShowNumber(added);
        any = true;
    }

    if (any) {
        ShowHurtValue(2);
    }
}

namespace {
constexpr int kProgressCycle = 300;

// Free Pascal `mod`/`div` match C++ `%`/`/` for positive divisors: toward-zero, sign follows dividend.
// CRITICAL: do NOT normalize negative mods to [0,299]. After Progress-300 from 299 → -1,
// Pascal treats (-1 mod 300) as -1 (not ready). Normalizing to 299 causes infinite re-actions.
static int ProgressMod(int progress) {
    return progress % kProgressCycle;
}

static int ProgressCycle(int progress) {
    return progress / kProgressCycle;
}

static int ProgressModForDisplay(int progress) {
    int mod = ProgressMod(progress);
    return (mod < 0) ? 0 : mod;
}

static int FindAttackMagicForAuto(int rnum, int& outLevel) {
    Role& role = GameManager::getInstance().getRole(rnum);
    int bestMagic = -1;
    int bestScore = -1;
    for (int i = 0; i < 10; ++i) {
        int m = role.getMagic(i);
        if (m <= 0) continue;
        Magic& mg = GameManager::getInstance().getMagic(m);
        if (mg.getMagicType() == 5 || mg.getEventNum() > 0) continue;
        int lv = GetMagicBattleLevel(rnum, m);
        if (mg.getNeedMP() * lv > role.getCurrentMP()) continue;
        int attDist = mg.getAttDistance(lv - 1);
        if (attDist < 0) continue;
        int score = role.getMagLevel(i) + attDist * 20 + mg.getMoveDistance(lv - 1) * 10;
        if (score > bestScore) {
            bestScore = score;
            bestMagic = m;
            outLevel = lv;
        }
    }
    return bestMagic;
}
}

void BattleManager::UpdateMaxSpeed() {
    m_maxSpeed = 1;
    for (const auto& role : m_battleRoles) {
        if (!role.getDead() && role.getRNum() >= 0) {
            m_maxSpeed = std::max(m_maxSpeed, (int)role.getSpeed());
        }
    }
}

void BattleManager::ReArrangeBRole() {
    const bool atbMode = GameManager::getInstance().getBattleMode() > 0;
    auto roleSpeed = [&](int idx) {
        BattleRole& br = m_battleRoles[idx];
        int rnum = br.getRNum();
        if (rnum < 0 || br.getDead()) return 0;
        int speed = GetRoleSpeed(rnum, true);
        Role& rData = GameManager::getInstance().getRole(rnum);
        if (CheckEquipSet(rData.getEquip(0), rData.getEquip(1), rData.getEquip(2), rData.getEquip(3)) == 5) {
            speed += 30;
        }
        bool petFirst = (GetPetSkill(5, 1) && br.getRNum() == 0) ||
                        (GetPetSkill(5, 3) && br.getTeam() == 0);
        if (petFirst) speed += 100000;
        return speed;
    };

    for (size_t i1 = 0; i1 + 1 < m_battleRoles.size(); ++i1) {
        for (size_t i2 = i1 + 1; i2 < m_battleRoles.size(); ++i2) {
            if (roleSpeed((int)i1) < roleSpeed((int)i2)) {
                std::swap(m_battleRoles[i1], m_battleRoles[i2]);
            }
        }
    }

    for (int x = 0; x < 64; ++x) {
        for (int y = 0; y < 64; ++y) {
            m_battleField[2][x][y] = -1;
            m_battleField[5][x][y] = -1;
        }
    }

    int aliveCount = 0;
    for (const auto& br : m_battleRoles) {
        if (!br.getDead() && br.getRNum() >= 0) ++aliveCount;
    }
    int placed = 0;
    for (size_t i = 0; i < m_battleRoles.size(); ++i) {
        BattleRole& br = m_battleRoles[i];
        if (br.getRNum() < 0) continue;
        if (!br.getDead()) {
            m_battleField[2][br.getX()][br.getY()] = (int16_t)i;
            m_battleField[5][br.getX()][br.getY()] = -1;
            if (atbMode) {
                br.setProgress((int16_t)((aliveCount - placed) * 5));
            }
            ++placed;
        } else {
            m_battleField[2][br.getX()][br.getY()] = -1;
            m_battleField[5][br.getX()][br.getY()] = -1;
        }
    }
}

int BattleManager::CountProgress() {
    int result = -1;
    double b = 1.0;
    // Align with Pascal CountProgress: readiness uses (Progress mod 300), including negatives.
    for (size_t i = 0; i < m_battleRoles.size(); ++i) {
        BattleRole& br = m_battleRoles[i];
        if (br.getRNum() < 0 || br.getDead() || br.getWait() != 0) continue;
        const int mod = ProgressMod(br.getProgress());
        if (mod + br.getSpeed() / 15 >= kProgressCycle - 1) {
            const double a = (kProgressCycle - mod) / 15.0;
            b = std::min(a, b);
            result = (int)i;
            break;
        }
    }

    for (size_t i = 0; i < m_battleRoles.size(); ++i) {
        BattleRole& br = m_battleRoles[i];
        if (br.getRNum() < 0 || br.getDead()) continue;
        if (br.getFrozen() > 0) {
            br.setFrozen(br.getFrozen() - (int)(b * (br.getSpeed() / 15)) / 3);
            continue;
        }
        if (br.getWait() != 0) continue;
        br.setFrozen(0);
        const int n = ProgressCycle(br.getProgress());
        br.setProgress((int16_t)(br.getProgress() + (int)(b * (br.getSpeed() / 15))));
        if (ProgressCycle(br.getProgress()) > n) {
            br.setProgress((int16_t)(n * kProgressCycle + kProgressCycle - 1));
        }
        if ((int)i == result) {
            br.setProgress((int16_t)(n * kProgressCycle + kProgressCycle - 1));
        }
    }
    return result;
}

void BattleManager::ShowProgress() {
    if (GameManager::getInstance().getBattleMode() == 0) return;
    std::vector<BattleProgressEntry> entries;
    entries.reserve(m_battleRoles.size());
    for (size_t i = 0; i < m_battleRoles.size(); ++i) {
        const BattleRole& br = m_battleRoles[i];
        if (br.getRNum() < 0 || br.getDead()) continue;
        BattleProgressEntry e;
        Role& role = GameManager::getInstance().getRole(br.getRNum());
        e.headNum = br.getPic() >= 0 ? br.getPic() : role.getHeadNum();
        e.team = br.getTeam();
        e.progressMod = ProgressModForDisplay(br.getProgress());
        e.selected = (m_battleField[4][br.getX()][br.getY()] > 0);
        entries.push_back(e);
    }
    UIManager::getInstance().DrawBattleProgressBar(entries, 250, 30);
}

void BattleManager::DeductActionProgress(BattleRole& actor) {
    if (GameManager::getInstance().getBattleMode() <= 0) return;
    // Pascal BattleMainControl: Progress := Progress - 300 (may go negative; that is intentional).
    const int before = actor.getProgress();
    actor.setProgress((int16_t)(before - kProgressCycle));
    std::cout << "[ATB] deduct progress " << before << " -> " << actor.getProgress()
              << " mod=" << ProgressMod(actor.getProgress()) << std::endl;
}

void BattleManager::ClearDeadRolePic() {
    for (size_t i = 0; i < m_battleRoles.size(); ++i) {
        BattleRole& br = m_battleRoles[i];
        if (br.getRNum() < 0) continue;
        Role& role = GameManager::getInstance().getRole(br.getRNum());
        if (role.getCurrentHP() <= 0) {
            br.setDead(1);
            br.setShow(1);
            if (br.getX() >= 0 && br.getX() < 64 && br.getY() >= 0 && br.getY() < 64) {
                m_battleField[5][br.getX()][br.getY()] = (int16_t)i;
                m_battleField[2][br.getX()][br.getY()] = -1;
            }
        }
    }
    for (size_t i = 0; i < m_battleRoles.size(); ++i) {
        BattleRole& br = m_battleRoles[i];
        if (br.getDead() != 0) continue;
        if (br.getX() >= 0 && br.getX() < 64 && br.getY() >= 0 && br.getY() < 64) {
            m_battleField[2][br.getX()][br.getY()] = (int16_t)i;
            m_battleField[5][br.getX()][br.getY()] = -1;
        }
    }
}

void BattleManager::GetViewCenter(int& cx, int& cy) const {
    if (m_viewCenterX >= 0 && m_viewCenterY >= 0) {
        cx = m_viewCenterX;
        cy = m_viewCenterY;
        return;
    }
    cx = 32;
    cy = 32;
    if (m_currentRoleIndex >= 0 && m_currentRoleIndex < (int)m_battleRoles.size()) {
        cx = m_battleRoles[m_currentRoleIndex].getX();
        cy = m_battleRoles[m_currentRoleIndex].getY();
    } else if (m_cursorX >= 0) {
        cx = m_cursorX;
        cy = m_cursorY;
    }
}

void BattleManager::SetViewCenter(int x, int y) {
    m_viewCenterX = x;
    m_viewCenterY = y;
}

void BattleManager::ClearViewCenter() {
    m_viewCenterX = -1;
    m_viewCenterY = -1;
}

void BattleManager::UnlockOneWaiter(int exceptIdx) {
    for (size_t k = 0; k < m_battleRoles.size(); ++k) {
        if ((int)k != exceptIdx && m_battleRoles[k].getWait() == 1) {
            m_battleRoles[k].setWait(0);
            break;
        }
    }
}

void BattleManager::PollBattleInput() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            m_battleRunning = false;
            GameManager::getInstance().Quit();
            return;
        }
    }
    const bool* keyState = SDL_GetKeyboardState(NULL);
    if (keyState[SDL_SCANCODE_ESCAPE] || keyState[SDL_SCANCODE_SPACE]) {
        m_exitAutoRequested = true;
    }
    float mouseX = 0.0f, mouseY = 0.0f;
    Uint32 mouseState = SDL_GetMouseState(&mouseX, &mouseY);
    if (mouseState & SDL_BUTTON_RMASK) {
        m_exitAutoRequested = true;
    }
}

bool BattleManager::CheckBattleEnd() {
    ClearDeadRolePic();
    int pAlive = 0;
    int eAlive = 0;
    for (auto& r : m_battleRoles) {
        if (r.getRNum() < 0) continue;
        Role& role = GameManager::getInstance().getRole(r.getRNum());
        // Trust HP as source of truth (Pascal BattleStatus uses Dead, which ClearDeadRolePic syncs from HP).
        if (role.getCurrentHP() <= 0) {
            r.setDead(1);
            continue;
        }
        if (r.getDead() != 0) continue;
        if (r.getTeam() == 0) ++pAlive;
        else ++eAlive;
    }
    if (pAlive > 0 && eAlive > 0) return false;
    if (eAlive == 0 && pAlive >= 0) {
        m_battleResult = 1;
        m_battleRunning = false;
        std::cout << "[BattleEnd] VICTORY p=" << pAlive << " e=" << eAlive << std::endl;
        return true;
    }
    if (pAlive == 0 && eAlive > 0) {
        m_battleResult = 2;
        m_battleRunning = false;
        std::cout << "[BattleEnd] DEFEAT p=" << pAlive << " e=" << eAlive << std::endl;
        return true;
    }
    return false;
}

bool BattleManager::ProcessActorTurn(int actorIdx) {
    if (actorIdx < 0 || actorIdx >= (int)m_battleRoles.size()) return true;
    BattleRole& actor = m_battleRoles[actorIdx];

    if (actor.getLifeAdd() == 0) {
        actor.setAddAtt(std::max(0, actor.getAddAtt() - 1));
        actor.setAddDef(std::max(0, actor.getAddDef() - 1));
        actor.setAddSpd(std::max(0, actor.getAddSpd() - 1));
        actor.setAddDodge(std::max(0, actor.getAddDodge() - 1));
        actor.setAddStep(std::max(0, actor.getAddStep() - 1));
        actor.setPerfectDodge(std::max(0, actor.getPerfectDodge() - 1));

        int rnum = actor.getRNum();
        if (rnum >= 0) {
            Role& role = GameManager::getInstance().getRole(rnum);
            if (GameManager::getInstance().GetEquipState(rnum, 11) ||
                GameManager::getInstance().GetGongtiState(rnum, 11)) {
                int add = role.getMaxHP() / 10;
                add = std::min(add, (int)role.getMaxHP() - (int)role.getCurrentHP());
                for (auto& br : m_battleRoles) br.setShowNumber(-1);
                actor.setShowNumber(add);
                if (add > 0) ShowHurtValue(3);
                role.setCurrentHP(role.getCurrentHP() + add);
            }
            if (GameManager::getInstance().GetEquipState(rnum, 23) ||
                GameManager::getInstance().GetGongtiState(rnum, 23)) {
                int add = role.getMaxMP() / 20;
                add = std::min(add, (int)role.getMaxMP() - (int)role.getCurrentMP());
                for (auto& br : m_battleRoles) br.setShowNumber(-1);
                actor.setShowNumber(add);
                if (add > 0) ShowHurtValue(1);
                role.setCurrentMP(role.getCurrentMP() + add);
            }
            ApplyGongtiStackAttack(actorIdx);
        }
        CalPoiHurtLife(actorIdx);
        actor.setLifeAdd(1);
    }

    if (actor.getFrozen() >= 100) {
        actor.setActed(1);
        actor.setFrozen(actor.getFrozen() - 100);
        if (GameManager::getInstance().getBattleMode() > 0) {
            actor.setProgress(0);
        }
        actor.setRound(actor.getRound() + 1);
        actor.setLifeAdd(0);
        ApplyGongtiAuraPoison(actorIdx);
        // true = keep battling; false = stop (matches caller: if (!ProcessActorTurn()) break)
        return m_battleRunning;
    }
    if (actor.getFrozen() <= 0) {
        actor.setFrozen(0);
    }

    SDL_Event clearEvent;
    while (SDL_PollEvent(&clearEvent)) {
        if (clearEvent.type == SDL_EVENT_QUIT) {
            m_battleRunning = false;
            GameManager::getInstance().Quit();
            return false;
        }
    }

    if (m_exitAutoRequested && actor.getTeam() == 0 && actor.getAuto() >= 0) {
        actor.setAuto(-1);
        m_exitAutoRequested = false;
    }

    if (actor.getTeam() == 0) {
        if (m_forceAutoBattle) {
            actor.setActed(0);
            actor.setWait(0);
            CalMoveAbility();
            AutoBattle(actorIdx);
            actor.setActed(1);
            if (actor.getActed() == 1) {
                actor.setRound(actor.getRound() + 1);
                actor.setLifeAdd(0);
                ApplyGongtiAuraPoison(actorIdx);
            }
        } else {
            actor.setActed(0);
            actor.setWait(0);
            CalMoveAbility();

            if (actor.getAuto() >= 0) {
                AutoBattle(actorIdx);
                actor.setActed(1);
            }

            while (actor.getActed() == 0 && actor.getWait() == 0 && actor.getAuto() < 0) {
                if (CheckBattleEnd()) return false;
                int menuResult = BattleMenu(actorIdx);
                if (menuResult < 0) break;
                if (menuResult == 0) {
                    int mx, my;
                    if (SelectMove(actorIdx, mx, my)) {
                        MoveAnimation(actorIdx, mx, my);
                        InputManager::getInstance().FlushEvents();
                    }
                } else if (menuResult == 1) {
                    int magicId = -1;
                    if (SelectMagic(actorIdx, magicId)) {
                        Magic& magic = GameManager::getInstance().getMagic(magicId);
                        if (magic.getMagicType() == 5) {
                            std::string prompt = "是否設置功體為" + TextManager::getInstance().gbkToUtf8(magic.getName()) + "？";
                            if (UIManager::getInstance().ShowChoice(prompt) == 0) {
                                GameManager::getInstance().getRole(actor.getRNum()).setGongti(magicId);
                            }
                            for (int x = 0; x < 64; ++x) {
                                for (int y = 0; y < 64; ++y) {
                                    m_battleField[4][x][y] = 0;
                                }
                            }
                            m_battleField[4][actor.getX()][actor.getY()] = 1;
                            m_showAttackRange = true;
                            SoundManager::getInstance().PlaySound(magic.getSoundNum());
                            PlayActionAmination(actorIdx, magic.getMagicType(), actor.getX(), actor.getY());
                            PlayMagicAmination(actorIdx, magicId, 10, actor.getX(), actor.getY());
                            m_showAttackRange = false;
                            for (int x = 0; x < 64; ++x) {
                                for (int y = 0; y < 64; ++y) {
                                    m_battleField[4][x][y] = 0;
                                }
                            }
                            CalMoveAbility();
                            actor.setActed(1);
                        } else {
                            int tx, ty;
                            if (SelectMagicTarget(actorIdx, magicId, tx, ty)) {
                                AttackAt(actorIdx, tx, ty, magicId);
                            }
                        }
                    }
                } else if (menuResult == 2) {
                    UsePoision(actorIdx);
                } else if (menuResult == 3) {
                    MedPoision(actorIdx);
                } else if (menuResult == 4) {
                    Medcine(actorIdx);
                } else if (menuResult == 5) {
                    MedFrozen(actorIdx);
                } else if (menuResult == 6) {
                    actor.setActed(1);
                    actor.setProgress((int16_t)std::min(1200, actor.getProgress() + 1));
                } else if (menuResult == 7) {
                    BattleMenuItem(actorIdx);
                } else if (menuResult == 8) {
                    actor.setWait(1);
                    actor.setActed(1);
                } else if (menuResult == 9) {
                    UIManager::getInstance().ShowStatus(actor.getRNum());
                } else if (menuResult == 10) {
                    int rnum = actor.getRNum();
                    Role& rData = GameManager::getInstance().getRole(rnum);
                    int hurt = rData.getHurt();
                    if (hurt < 0) hurt = 0;
                    if (hurt > 100) hurt = 100;
                    int addHp = ((100 - hurt) * rData.getMaxHP()) / 2000;
                    int addMp = ((100 - hurt) * rData.getMaxMP()) / 2000;
                    int addPhy = ((100 - hurt) * MAX_PHYSICAL_POWER) / 2000;
                    rData.setCurrentHP(std::min((int)rData.getMaxHP(), rData.getCurrentHP() + addHp));
                    rData.setCurrentMP(std::min((int)rData.getMaxMP(), rData.getCurrentMP() + addMp));
                    rData.setPhyPower(std::min(MAX_PHYSICAL_POWER, rData.getPhyPower() + addPhy));
                    actor.setActed(1);
                    actor.setProgress((int16_t)(actor.getProgress() - 240));
                    actor.setProgress((int16_t)(actor.getProgress() + ((actor.getStep() * 120) / std::max(1, actor.getSpeed() / 15))));
                } else if (menuResult == 11) {
                    int autoMode = SelectAutoTarget(actorIdx);
                    if (autoMode >= 0) {
                        actor.setAuto(autoMode);
                        AutoBattle(actorIdx);
                    }
                }
                if (actor.getActed() == 1) {
                    actor.setRound(actor.getRound() + 1);
                    actor.setLifeAdd(0);
                    ApplyGongtiAuraPoison(actorIdx);
                }
            }
        }
    } else {
        actor.setActed(0);
        actor.setWait(0);
        CalMoveAbility();
        AutoBattle(actorIdx);
        actor.setActed(1);
        if (actor.getActed() == 1) {
            actor.setRound(actor.getRound() + 1);
            actor.setLifeAdd(0);
            ApplyGongtiAuraPoison(actorIdx);
        }
    }
    // true = keep battling; false = stop (matches caller: if (!ProcessActorTurn()) break)
    return m_battleRunning;
}

void BattleManager::RunTurnBasedBattle() {
    while (m_battleRunning) {
        PollBattleInput();
        CalMoveAbility();
        ReArrangeBRole();

        for (auto& br : m_battleRoles) {
            br.setActed(0);
            br.setShowNumber(0);
            br.setLifeAdd(0);
        }

        int i = 0;
        while (i < (int)m_battleRoles.size() && m_battleRunning) {
            BattleRole& actor = m_battleRoles[i];
            if (actor.getRNum() < 0 || actor.getDead() != 0) {
                ++i;
                continue;
            }

            m_currentRoleIndex = i;
            m_cursorX = actor.getX();
            m_cursorY = actor.getY();
            m_showMoveRange = false;
            m_showAttackRange = false;
            m_highlightRoleIndex = i;

            std::cout << "[TurnBased] i=" << i << " rnum=" << actor.getRNum()
                      << " team=" << actor.getTeam() << " acted=" << actor.getActed() << std::endl;

            // ProcessActorTurn: true=continue battle, false=stop
            if (!ProcessActorTurn(i)) break;

            ClearDeadRolePic();
            if (CheckBattleEnd()) break;

            // Pascal OldBattleMainControl: only advance when Acted=1
            if (actor.getActed() == 1) {
                actor.setLifeAdd(0);
                ++i;
            }

            RenderBattle();
            UIManager::getInstance().UpdateScreen();
            SDL_Delay((200 * GameManager::getInstance().getGameSpeed()) / 10);
        }

        if (m_forceAutoBattle && m_forceAutoBattleFrameLimit > 0) {
            m_forceAutoBattleFrameCount++;
            if (m_forceAutoBattleFrameCount >= m_forceAutoBattleFrameLimit) {
                m_battleRunning = false;
            }
        }
    }
}

void BattleManager::RunAtbBattle() {
    while (m_battleRunning) {
        PollBattleInput();
        CalMoveAbility();

        int actorIdx = CountProgress();
        if (actorIdx >= 0) {
            BattleRole& actor = m_battleRoles[actorIdx];
            m_currentRoleIndex = actorIdx;
            m_cursorX = actor.getX();
            m_cursorY = actor.getY();
            m_showMoveRange = false;
            m_showAttackRange = false;
            m_highlightRoleIndex = actorIdx;
            actor.setActed(0);

            for (auto& br : m_battleRoles) {
                br.setShowNumber(-1);
            }

            if (!ProcessActorTurn(actorIdx)) break;

            ClearDeadRolePic();

            // Pascal: only deduct when Acted=1; Wait also ends the turn.
            if (actor.getWait() == 1 && actor.getActed() == 0) {
                actor.setActed(1);
            }
            if (actor.getActed() == 1) {
                DeductActionProgress(actor);
                SDL_Delay(500);
            } else {
                actor.setActed(1);
                DeductActionProgress(actor);
                SDL_Delay(500);
            }
            UnlockOneWaiter(actorIdx);

            for (int x = 0; x < 64; ++x) {
                for (int y = 0; y < 64; ++y) {
                    m_battleField[4][x][y] = 0;
                }
            }
        }

        if (CheckBattleEnd()) break;

        if (m_forceAutoBattle && m_forceAutoBattleFrameLimit > 0) {
            m_forceAutoBattleFrameCount++;
            if (m_forceAutoBattleFrameCount >= m_forceAutoBattleFrameLimit) {
                m_battleRunning = false;
            }
        }

        RenderBattle();
        UIManager::getInstance().UpdateScreen();
        int delayMs = (m_maxSpeed * GameManager::getInstance().getGameSpeed()) / 1000;
        if (delayMs < 1) delayMs = 1;
        SDL_Delay(delayMs);
    }
}

void BattleManager::RunBattle() {
    const int battleMode = GameManager::getInstance().getBattleMode();
    std::cout << "Entering Battle Loop... battleMode=" << battleMode
              << (battleMode > 0 ? " (ATB)" : " (TurnBased)") << std::endl;
    m_battleResult = 0;
    m_exitAutoRequested = false;
    m_lastActorIdx = -1;

    ReArrangeBRole();
    UpdateMaxSpeed();
    CalMoveAbility();

    if (battleMode > 0) {
        RunAtbBattle();
    } else {
        RunTurnBasedBattle();
    }

    if (m_battleResult == 0) {
        bool pAlive = false;
        bool eAlive = false;
        for(const auto& r : m_battleRoles) {
            if (!r.getDead() && r.getRNum() >= 0) {
                if (r.getTeam() == 0) pAlive = true;
                else eAlive = true;
            }
        }
        if (!pAlive) {
            m_battleResult = 2;
        } else if (!eAlive) {
            m_battleResult = 1;
        } else {
            m_battleResult = 2;
        }
    }

    RestoreRoleStatus();
    GameHooks::notifyBattleEnd(m_battleId, m_battleResult);

    std::string resultText = (m_battleResult == 1) ? " 戰鬥勝利" : " 戰鬥失敗";
    UIManager::getInstance().ShowDialogue(resultText, -1, 0);
    
    SDL_Renderer* renderer = UIManager::getInstance().GetRenderer();
    if (renderer) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        GameManager::getInstance().RenderScreenTo(renderer);
        VirtualControls::present(renderer);
    }

    if (m_battleResult == 1 || (m_battleResult == 2 && m_getExp != 0)) {
        if (m_battleResult == 1) { PetEffect(); }
        AddExp();
        CheckBook();
        CheckLevelUp();
    }
    
    Scene& curScene = GameManager::getInstance().getScene(GameManager::getInstance().getCurrentSceneId());
    int pal = curScene.getPallet();
    if (pal < 0 || pal > 3) pal = 0;
    GraphicsUtils::resetPalette(pal);
    GameManager::getInstance().RedrawRoamingScene();

    int entranceMusic = curScene.getEntranceMusic();
    if (entranceMusic >= 0) {
        SoundManager::getInstance().PlayMusic(entranceMusic);
    }
}

bool BattleManager::GetPetSkill(int petIndex, int skillIndex) const {
    if (petIndex < 1 || petIndex > 5) return false;
    if (skillIndex < 0 || skillIndex > 4) return false;
    Role& pet = GameManager::getInstance().getRole(petIndex);
    return pet.getMagic(skillIndex) > 0;
}

void BattleManager::ShowPetEffectMessage(const std::string& text) {
    UIManager::getInstance().ShowDialogue(text, -1, 0);
}

void BattleManager::PetEffect() {
    if (GetPetSkill(3, 0)) {
        int n = rand() % 100;
        if (n < 60) {
            ShowPetEffectMessage(" 林廚子收集藥材成功");
            GameManager::getInstance().AddItem(291, 5);
            UIManager::getInstance().ShowItemNotification(291, 5);
        }
        n = rand() % 100;
        if (n < 60) {
            ShowPetEffectMessage(" 林廚子收集食材成功");
            GameManager::getInstance().AddItem(269, 5);
            UIManager::getInstance().ShowItemNotification(269, 5);
        }
    }
    if (GetPetSkill(3, 2)) {
        int n = rand() % 100;
        if (n < 30) {
            ShowPetEffectMessage(" 林廚子收集材料成功");
            int itemId = 270 + (rand() % 18);
            GameManager::getInstance().AddItem(itemId, 1);
            UIManager::getInstance().ShowItemNotification(itemId, 1);
        }
    }
    if (GetPetSkill(4, 0)) {
        int n = rand() % 100;
        if (n < 60) {
            ShowPetEffectMessage(" 孔八拉搜刮礦石成功");
            GameManager::getInstance().AddItem(267, 5);
            UIManager::getInstance().ShowItemNotification(267, 5);
        }
        n = rand() % 100;
        if (n < 60) {
            ShowPetEffectMessage(" 孔八拉搜刮硝石成功");
            GameManager::getInstance().AddItem(268, 5);
            UIManager::getInstance().ShowItemNotification(268, 5);
        }
    }
    if (GetPetSkill(1, 0) || GetPetSkill(1, 2) || GetPetSkill(1, 4)) {
        int kf = 0;
        if (GetPetSkill(1, 4)) kf = 100;
        else if (GetPetSkill(1, 2)) kf = 60;
        else if (GetPetSkill(1, 0)) kf = 30;
        for (int i = 0; i < 3; ++i) {
            int itemId = m_warData.getGetKongfu(i);
            if (itemId > -1) {
                int n = rand() % 100;
                if (n < kf) {
                    ShowPetEffectMessage(" 阿賢記錄武功成功");
                    GameManager::getInstance().AddItem(itemId, 1);
                    UIManager::getInstance().ShowItemNotification(itemId, 1);
                }
            }
        }
    }
    if (GetPetSkill(2, 2)) {
        int kf = 50;
        for (int i = 0; i < 3; ++i) {
            int itemId = m_warData.getGetItems(i);
            if (itemId > -1) {
                int n = rand() % 100;
                if (n < kf) {
                    ShowPetEffectMessage(" 阿丑偷竊物品成功");
                    GameManager::getInstance().AddItem(itemId, 1);
                    UIManager::getInstance().ShowItemNotification(itemId, 1);
                }
            }
        }
    }
    if (GetPetSkill(2, 0)) {
        int money = m_warData.getGetMoney();
        int n = 0;
        if (money > 0) {
            n = money / 2 + (rand() % std::max(1, money / 2));
        }
        if (n > 0) {
            ShowPetEffectMessage(" 阿丑偷竊金錢成功");
            GameManager::getInstance().AddItem(0, n);
            UIManager::getInstance().ShowItemNotification(0, n);
        }
    }
}

void BattleManager::CheckBook() {
    for (const auto& role : m_battleRoles) {
        if (role.getTeam() != 0) continue; // Only player team trains books
        if (role.getRNum() < 0) continue;
        
        Role& r = GameManager::getInstance().getRole(role.getRNum());
        int bookItemIdx = r.getPracticeBook();
        
        if (bookItemIdx >= 0) {
            Item& book = GameManager::getInstance().getItem(bookItemIdx);
            int magicIdx = book.getMagic();
            
            if (magicIdx <= 0) continue;
            
            int magicSlot = -1;
            int currentLevel = 0;
            for(int k=0; k<10; ++k) {
                if (r.getMagic(k) == magicIdx) {
                    magicSlot = k;
                    currentLevel = r.getMagLevel(k) / 100 + 1;
                    break;
                }
            }
            if (magicSlot == -1) currentLevel = 1;
            
            int aptitude = r.getAptitude();
            if (CheckEquipSet(r.getEquip(0), r.getEquip(1), r.getEquip(2), r.getEquip(3)) == 2) {
                aptitude = 100;
            }
            
            int baseNeedExp = book.getNeedExp();
            int needExp = 0;
            if (baseNeedExp > 0) {
                 needExp = currentLevel * (baseNeedExp * (8 - aptitude / 15)) / 2;
            } else {
                 needExp = currentLevel * ((-baseNeedExp) * (1 + aptitude / 15)) / 2;
            }
            
            while (r.getPracticeBook() >= 0 && r.getExpForBook() >= needExp && currentLevel < 10) {
                Magic& magic = GameManager::getInstance().getMagic(magicIdx);
                if (magic.getMagicType() == 5 && currentLevel > 1) break;
                
                GameManager::getInstance().EatOneItem(role.getRNum(), bookItemIdx);
                GameManager::getInstance().LearnMagic(role.getRNum(), magicIdx, 1);
                
                std::string msg = "修炼 " + TextManager::getInstance().gbkToUtf8(magic.getName()) + 
                                  " 成功升为 " + std::to_string(currentLevel + 1) + " 级";
                UIManager::getInstance().ShowDialogue(msg, r.getHeadNum(), 0);
                
                r.setExpForBook(r.getExpForBook() - needExp);
                
                currentLevel++; 
                
                if (baseNeedExp > 0) {
                     needExp = currentLevel * (baseNeedExp * (8 - aptitude / 15)) / 2;
                } else {
                     needExp = currentLevel * ((-baseNeedExp) * (1 + aptitude / 15)) / 2;
                }
                
                int actualLevel = 0;
                for(int k=0; k<10; ++k) { if (r.getMagic(k) == magicIdx) actualLevel = r.getMagLevel(k); }
                
                if (actualLevel >= 900 || magic.getMagicType() == 5) {
                    r.setPracticeBook(-1);
                    GameManager::getInstance().AddItem(bookItemIdx, 1);
                    std::string finMsg = TextManager::getInstance().gbkToUtf8(magic.getName()) + " 修炼完成";
                    UIManager::getInstance().ShowDialogue(finMsg, r.getHeadNum(), 0);
                    break;
                }
            }
        }
    }
}

void BattleManager::CalMoveAbility() {
    for (int i = 0; i < m_battleRoles.size(); ++i) {
        BattleRole& br = m_battleRoles[i];
        int rnum = br.getRNum();
        if (rnum <= -1) continue;
        int addSpeed = 0;
        Role& rData = GameManager::getInstance().getRole(rnum);
        if (CheckEquipSet(rData.getEquip(0), rData.getEquip(1), rData.getEquip(2), rData.getEquip(3)) == 5) {
            addSpeed += 30;
        }
        int speed = GetRoleSpeed(rnum, true) + addSpeed;
        if (br.getWait() == 0) {
            int step = speed / 15 + std::min(1, (int)br.getAddStep()) * 3;
            br.setStep(step);
        }
        if (br.getAddSpd() > 0) {
            speed = (speed * 14) / 10;
            if (GameManager::getInstance().GetEquipState(rnum, 3) || GameManager::getInstance().GetGongtiState(rnum, 3)) {
                speed = (speed * 14) / 10;
            }
        }
        br.setSpeed(speed);
        if (rData.getMoveable() > 0) br.setStep(0);
    }
}

void BattleManager::MoveAnimation(int roleIdx, int targetX, int targetY) {
    if (roleIdx < 0 || roleIdx >= (int)m_battleRoles.size()) return;
    BattleRole& role = m_battleRoles[roleIdx];
    const int ax = targetX;
    const int ay = targetY;
    if (ax < 0 || ax >= 64 || ay < 0 || ay >= 64) return;

    const int pathCost = m_battleField[3][ax][ay];
    if (pathCost <= 0) {
        MoveRole(roleIdx, ax, ay);
        return;
    }

    int bx = role.getX();
    int by = role.getY();
    std::vector<int> lineX(pathCost + 1);
    std::vector<int> lineY(pathCost + 1);
    const int xInc[] = {1, -1, 0, 0};
    const int yInc[] = {0, 0, 1, -1};

    lineX[0] = bx;
    lineY[0] = by;
    lineX[pathCost] = ax;
    lineY[pathCost] = ay;
    for (int a = pathCost - 1; a >= 0; --a) {
        for (int i = 0; i < 4; ++i) {
            const int tempx = lineX[a + 1] + xInc[i];
            const int tempy = lineY[a + 1] + yInc[i];
            if (tempx >= 0 && tempx < 64 && tempy >= 0 && tempy < 64 &&
                m_battleField[3][tempx][tempy] == m_battleField[3][lineX[a + 1]][lineY[a + 1]] - 1) {
                lineX[a] = tempx;
                lineY[a] = tempy;
                break;
            }
        }
    }

    int pathStep = 1;
    const int gameSpeed = GameManager::getInstance().getGameSpeed();
    while (role.getStep() > 0 && !(bx == ax && by == ay)) {
        if (lineX[pathStep] > bx) role.setFace(3);
        else if (lineX[pathStep] < bx) role.setFace(0);
        else if (lineY[pathStep] < by) role.setFace(2);
        else role.setFace(1);

        if (m_battleField[2][bx][by] == roleIdx) {
            m_battleField[2][bx][by] = -1;
        }
        bx = lineX[pathStep];
        by = lineY[pathStep];
        if (m_battleField[2][bx][by] == -1) {
            m_battleField[2][bx][by] = roleIdx;
        }
        ++pathStep;
        role.setStep(std::max(0, role.getStep() - 1));

        SetViewCenter(bx, by);
        RenderBattle();
        UIManager::getInstance().UpdateScreen();
        SDL_Delay((gameSpeed * 20) / 10);
    }

    role.setX(bx);
    role.setY(by);
    if (bx >= 0 && bx < 64 && by >= 0 && by < 64) {
        m_battleField[2][bx][by] = roleIdx;
    }
    ClearViewCenter();
}

void BattleManager::MoveRole(int roleIdx, int x, int y) {
    if (roleIdx < 0 || roleIdx >= m_battleRoles.size()) return;
    BattleRole& role = m_battleRoles[roleIdx];
    int stepCost = 0;
    if (x >= 0 && x < 64 && y >= 0 && y < 64) {
        stepCost = m_battleField[3][x][y];
        if (stepCost < 0) stepCost = 0;
    }
    
    if (m_battleField[2][role.getX()][role.getY()] == roleIdx) {
        m_battleField[2][role.getX()][role.getY()] = -1;
    }
    int dx = x - role.getX();
    int dy = y - role.getY();
    if (dx > 0) role.setFace(3);
    else if (dx < 0) role.setFace(0);
    else if (dy < 0) role.setFace(2);
    else if (dy > 0) role.setFace(1);
    //if (dx > 0) role.setFace(3);下面这几个方向是对的
    //else if (dx < 0) role.setFace(0);
    //else if (dy < 0) role.setFace(2);
    //else if (dy > 0) role.setFace(1);
    role.setX(x);
    role.setY(y);
    m_battleField[2][x][y] = roleIdx;
    if (stepCost > 0) {
        role.setStep(std::max(0, role.getStep() - stepCost));
    } else if (role.getStep() > 0 && role.getX() == x && role.getY() == y) {
        role.setStep(0);
    }
}

void BattleManager::CalSelectableArea(int roleIdx) {
    CalSelectableAreaEx(roleIdx, -1, 0);
}

bool BattleManager::IsWaterTile(int earthNum) const {
    int tileType = earthNum / 2;
    if (tileType >= 179 && tileType <= 190) return true;
    if (tileType == 261) return true;
    if (tileType == 511) return true;
    if (tileType >= 224 && tileType <= 232) return true;
    if (tileType >= 662 && tileType <= 674) return true;
    return false;
}

void BattleManager::CalSelectableAreaEx(int roleIdx, int myTeam, int mode) {
    for(int y=0; y<64; y++)
        for(int x=0; x<64; x++)
            m_battleField[3][x][y] = -1;

    if (roleIdx < 0 || roleIdx >= m_battleRoles.size()) return;
    BattleRole& role = m_battleRoles[roleIdx];
    int startX = role.getX();
    int startY = role.getY();
    int maxStep = role.getStep();
    if (maxStep < 0) maxStep = 0;
    
    std::cout << "[Path] roleIdx=" << roleIdx << " pos=(" << startX << "," << startY << ") step=" << maxStep << std::endl;
    std::cout << "[Path] earth at pos=" << m_battleField[0][startX][startY] << " building=" << m_battleField[1][startX][startY] << std::endl;

    std::vector<std::tuple<int, int, int>> queue;
    queue.push_back({startX, startY, 0});
    m_battleField[3][startX][startY] = 0;

    size_t head = 0;
    int expandedCount = 0;
    while(head < queue.size()) {
        auto [cx, cy, curStep] = queue[head++];
        
        if (curStep >= maxStep) continue;

        int dx[] = {0, 0, 1, -1};
        int dy[] = {1, -1, 0, 0};
        
        for(int i=0; i<4; i++) {
            int nx = cx + dx[i];
            int ny = cy + dy[i];
            
            if (nx < 0 || nx >= 64 || ny < 0 || ny >= 64) continue;
            
            if (m_battleField[3][nx][ny] >= 0) continue;
            
            int earthNum = m_battleField[0][nx][ny];
            int buildingNum = m_battleField[1][nx][ny];
            int roleAtPos = m_battleField[2][nx][ny];
            
            if (buildingNum > 0) {
                std::cout << "[Path] BLOCKED (" << nx << "," << ny << ") building=" << buildingNum << std::endl;
                continue;
            }
            
            if (roleAtPos >= 0 && roleAtPos < (int)m_battleRoles.size()) {
                BattleRole& r = m_battleRoles[roleAtPos];
                if (!r.getDead()) {
                    std::cout << "[Path] BLOCKED (" << nx << "," << ny << ") role=" << roleAtPos << std::endl;
                    continue;
                }
            }
            
            if (IsWaterTile(earthNum)) {
                std::cout << "[Path] BLOCKED (" << nx << "," << ny << ") water earth=" << earthNum << std::endl;
                continue;
            }
            
            if (mode == 0 && curStep > 0 && myTeam >= 0) {
                bool hasEnemy = false;
                for (int j = 0; j < 4; ++j) {
                    int ex = nx + dx[j];
                    int ey = ny + dy[j];
                    if (ex >= 0 && ex < 64 && ey >= 0 && ey < 64) {
                        int er = m_battleField[2][ex][ey];
                        if (er >= 0 && er < (int)m_battleRoles.size()) {
                            if (!m_battleRoles[er].getDead() && m_battleRoles[er].getTeam() != myTeam) {
                                hasEnemy = true;
                                break;
                            }
                        }
                    }
                }
                if (hasEnemy) {
                    std::cout << "[Path] BLOCKED (" << nx << "," << ny << ") hasEnemy nearby" << std::endl;
                    continue;
                }
            }
            
            m_battleField[3][nx][ny] = curStep + 1;
            queue.push_back({nx, ny, curStep + 1});
            expandedCount++;
        }
    }
    std::cout << "[Path] Expanded " << expandedCount << " cells, queue size=" << queue.size() << std::endl;
}

int BattleManager::SelectAutoMode() {
    std::vector<std::string> modes = {" 瘋子型", " 傻子型", " 呆子型"};
    int menu = 0;
    int max = 2;
    SDL_Event event;
    
    auto redrawMenu = [&]() {
        RenderBattle();
        // Pascal ShowModeMenu: x=157,y=100, DrawRectangle(x,y,75,74), text at x-17
        int x = 157;
        int y = 100;
        int w = 75;
        int h = 74;
        UIManager::getInstance().DrawRectangle(x, y, w, h, 0x000000, 0xFFFFFFFF, 30);
        for (int i = 0; i <= max; ++i) {
            uint32_t color1 = (i == menu) ? 0x64FFFFFF : 0x21FFFFFF;
            uint32_t color2 = (i == menu) ? 0x66FFFFFF : 0x23FFFFFF;
            UIManager::getInstance().DrawShadowTextUtf8(modes[i], x - 17, y + 3 + i * 22, color1, color2);
        }
        UIManager::getInstance().UpdateScreen();
    };
    
    redrawMenu();
    
    while (true) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                m_battleRunning = false;
                GameManager::getInstance().Quit();
                return -1;
            }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_UP || event.key.key == SDLK_KP_8) {
                    menu -= 1;
                    if (menu < 0) menu = max;
                    redrawMenu();
                }
                if (event.key.key == SDLK_DOWN || event.key.key == SDLK_KP_2) {
                    menu += 1;
                    if (menu > max) menu = 0;
                    redrawMenu();
                }
            }
            if (event.type == SDL_EVENT_KEY_UP) {
                if (event.key.key == SDLK_RETURN || event.key.key == SDLK_SPACE) {
                    return menu;
                }
                if (event.key.key == SDLK_ESCAPE) {
                    return -1;
                }
            }
            if (event.type == SDL_EVENT_MOUSE_MOTION) {
                float xm = 0.0f, ym = 0.0f;
                SDL_GetMouseState(&xm, &ym);
                if (xm >= 100.0f && xm < 200.0f && ym >= 100.0f && ym < (max + 1) * 22.0f + 100.0f) {
                    int next = static_cast<int>((ym - 103.0f) / 22.0f);
                    if (next >= 0 && next <= max && next != menu) {
                        menu = next;
                        redrawMenu();
                    }
                }
            }
            if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    float xm = 0.0f, ym = 0.0f;
                    SDL_GetMouseState(&xm, &ym);
                    if (xm >= 100.0f && xm < 200.0f && ym >= 100.0f && ym < (max + 1) * 22.0f + 100.0f) {
                        return menu;
                    }
                }
                if (event.button.button == SDL_BUTTON_RIGHT) {
                    return -1;
                }
            }
        }
        SDL_Delay(10);
    }
    return -1;
}

int BattleManager::SelectAutoTarget(int roleIdx) {
    std::vector<std::string> modes = {" 全體", " 單人"};
    int menu = 0;
    int max = 1;
    SDL_Event event;
    
    auto redrawMenu = [&]() {
        RenderBattle();
        int x = 157;
        int y = 100;
        int w = 98;
        int h = max * 22 + 28;
        UIManager::getInstance().DrawRectangle(x, y, w, h, 0x000000, 0xFFFFFFFF, 30);
        for (int i = 0; i <= max; ++i) {
            uint32_t color1 = (i == menu) ? 0x64FFFFFF : 0x21FFFFFF;
            uint32_t color2 = (i == menu) ? 0x66FFFFFF : 0x23FFFFFF;
            UIManager::getInstance().DrawShadowTextUtf8(modes[i], x - 17, y + 3 + i * 22, color1, color2);
        }
        UIManager::getInstance().UpdateScreen();
    };
    
    redrawMenu();
    
    while (true) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                m_battleRunning = false;
                GameManager::getInstance().Quit();
                return -1;
            }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_UP || event.key.key == SDLK_KP_8) {
                    menu -= 1;
                    if (menu < 0) menu = max;
                    redrawMenu();
                }
                if (event.key.key == SDLK_DOWN || event.key.key == SDLK_KP_2) {
                    menu += 1;
                    if (menu > max) menu = 0;
                    redrawMenu();
                }
            }
            if (event.type == SDL_EVENT_KEY_UP) {
                if (event.key.key == SDLK_RETURN || event.key.key == SDLK_SPACE) {
                    if (menu == 1) {
                        return SelectAutoMode();
                    } else if (menu == 0) {
                        for (int i = 0; i < m_battleRoles.size(); ++i) {
                            if (m_battleRoles[i].getTeam() == 0 && !m_battleRoles[i].getDead()) {
                                m_battleRoles[i].setAuto(0);
                            }
                        }
                        return 0;
                    }
                }
                if (event.key.key == SDLK_ESCAPE) {
                    return -1;
                }
            }
            if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
                if (event.button.button == SDL_BUTTON_RIGHT) {
                    return -1;
                }
            }
        }
        SDL_Delay(10);
    }
    return -1;
}

void BattleManager::PauseShowActorStatus(int roleIdx, int delayMs) {
    if (roleIdx < 0 || roleIdx >= (int)m_battleRoles.size()) return;
    BattleRole& actor = m_battleRoles[roleIdx];
    const int rnum = actor.getRNum();
    if (rnum < 0) return;

    RenderBattle();
    UIManager::getInstance().ShowSimpleStatus(rnum, 30, 330, actor.getFrozen());
    UIManager::getInstance().UpdateScreen();
    if (delayMs > 0) SDL_Delay(delayMs);
}

void BattleManager::AutoBattle(int roleIdx) {
    if (roleIdx < 0 || roleIdx >= (int)m_battleRoles.size()) return;
    if (m_battleRoles[roleIdx].getTeam() == 0) {
        AutoBattle2(roleIdx);
        return;
    }

    BattleRole& actor = m_battleRoles[roleIdx];
    Role& role = GameManager::getInstance().getRole(actor.getRNum());
    if (actor.getActed() != 0) return;

    PauseShowActorStatus(roleIdx, 350);

    // Self heal / items (same thresholds as Pascal AutoBattle)
    if (actor.getActed() == 0 && role.getCurrentHP() < role.getMaxHP() / 5) {
        if (rand() % 100 < 70) {
            if (GetRoleMedcine(actor.getRNum(), true) >= 50 && role.getPhyPower() >= 50 && rand() % 100 < 50) {
                ApplyMedicine(roleIdx, roleIdx);
            } else {
                AutoUseItem(roleIdx, 45);
            }
        }
    }
    if (actor.getActed()) return;
    if (actor.getActed() == 0 && role.getCurrentMP() < role.getMaxMP() / 5) {
        if (rand() % 100 < 60) AutoUseItem(roleIdx, 50);
    }
    if (actor.getActed()) return;
    if (actor.getActed() == 0 && role.getPhyPower() < 20) {
        if (rand() % 100 < 80) AutoUseItem(roleIdx, 48);
    }
    if (actor.getActed()) return;

    // Random enemy (Pascal Calrnum + aim walk)
    std::vector<int> enemies;
    for (int i = 0; i < (int)m_battleRoles.size(); ++i) {
        if (m_battleRoles[i].getTeam() != actor.getTeam() &&
            m_battleRoles[i].getRNum() >= 0 && m_battleRoles[i].getDead() == 0) {
            enemies.push_back(i);
        }
    }
    if (enemies.empty()) {
        actor.setActed(1);
        return;
    }
    int targetIdx = enemies[rand() % enemies.size()];
    BattleRole& target = m_battleRoles[targetIdx];

    CalSelectableAreaEx(roleIdx, actor.getTeam(), 0);
    int bx = actor.getX(), by = actor.getY();
    int ax1 = target.getX(), ay1 = target.getY();
    int bestX = bx, bestY = by;
    int dis0 = std::abs(ax1 - bx) + std::abs(ay1 - by);
    int xMin = std::min(ax1, bx), xMax = std::max(ax1, bx);
    int yMin = std::min(ay1, by), yMax = std::max(ay1, by);
    for (int x = xMin; x <= xMax; ++x) {
        for (int y = yMin; y <= yMax; ++y) {
            if (m_battleField[3][x][y] < 0) continue;
            int dis = std::abs(ax1 - x) + std::abs(ay1 - y);
            int stepCost = std::abs(x - bx) + std::abs(y - by);
            if (dis < dis0 && stepCost <= actor.getStep()) {
                bestX = x;
                bestY = y;
                dis0 = dis;
            }
        }
    }
    if (m_battleField[3][bestX][bestY] >= 0 && (bestX != bx || bestY != by)) {
        MoveAnimation(roleIdx, bestX, bestY);
    }

    if (actor.getActed() == 0 && role.getPhyPower() >= 10) {
        int level = 1;
        int magicId = FindAttackMagicForAuto(actor.getRNum(), level);
        int attackRange = 1;
        if (magicId > 0) {
            Magic& magic = GameManager::getInstance().getMagic(magicId);
            attackRange = std::max(1, static_cast<int>(magic.getAttDistance(level - 1)));
        }
        int dist = std::abs(actor.getX() - target.getX()) + std::abs(actor.getY() - target.getY());
        if (dist <= attackRange) {
            if (magicId > 0) Attack(roleIdx, targetIdx, magicId);
            else {
                PlayActionAmination(roleIdx, 0, target.getX(), target.getY());
                SoundManager::getInstance().PlaySound(1);
                Role& tData = GameManager::getInstance().getRole(target.getRNum());
                int dmg = std::max(1, GetRoleAttack(actor.getRNum(), true) - GetRoleDefence(target.getRNum(), true) / 2);
                tData.setCurrentHP(std::max(0, tData.getCurrentHP() - dmg));
                target.setShowNumber(dmg);
                target.setFlashTimer(12);
                ShowHurtValue(0);
                actor.setActed(1);
            }
        }
    }

    if (actor.getActed() == 0) {
        int rnum = actor.getRNum();
        if (rnum >= 0) {
            Role& rData = GameManager::getInstance().getRole(rnum);
            int hurt = std::clamp((int)rData.getHurt(), 0, 100);
            rData.setCurrentHP(std::min((int)rData.getMaxHP(), rData.getCurrentHP() + ((100 - hurt) * rData.getMaxHP()) / 2000));
            rData.setCurrentMP(std::min((int)rData.getMaxMP(), rData.getCurrentMP() + ((100 - hurt) * rData.getMaxMP()) / 2000));
            rData.setPhyPower(std::min(MAX_PHYSICAL_POWER, rData.getPhyPower() + ((100 - hurt) * MAX_PHYSICAL_POWER) / 2000));
        }
        actor.setActed(1);
    }
}

void BattleManager::AutoBattle2(int roleIdx) {
    BattleRole& actor = m_battleRoles[roleIdx];
    Role& role = GameManager::getInstance().getRole(actor.getRNum());
    if (actor.getActed() != 0) return;

    if (actor.getTeam() != 0) {
        PauseShowActorStatus(roleIdx, 350);
    } else {
        PauseShowActorStatus(roleIdx, 350);
    }
    
    std::cout << "[Auto] role=" << roleIdx << " rnum=" << actor.getRNum()
              << " team=" << actor.getTeam()
              << " pos=(" << actor.getX() << "," << actor.getY() << ")"
              << " acted=" << actor.getActed()
              << " hp=" << role.getCurrentHP() << "/" << role.getMaxHP()
              << " mp=" << role.getCurrentMP() << "/" << role.getMaxMP()
              << " phy=" << role.getPhyPower()
              << " progress=" << actor.getProgress()
              << std::endl;
    
    // 1. Self Heal / Restore (Priority)
    if (actor.getActed() == 0 && role.getCurrentHP() < role.getMaxHP() / 5) {
        if (rand() % 100 < 70) {
            if (GetRoleMedcine(actor.getRNum(), true) >= 50 && role.getPhyPower() >= 50 && rand() % 100 < 50) {
                ApplyMedicine(roleIdx, roleIdx); // Heal Self
            } else {
                AutoUseItem(roleIdx, 45); // HP Item
            }
        }
    }
    if (actor.getActed()) return;

    if (actor.getActed() == 0 && role.getCurrentMP() < role.getMaxMP() / 5) {
        if (rand() % 100 < 60) {
            AutoUseItem(roleIdx, 50); // MP Item
        }
    }
    if (actor.getActed()) return;

    if (actor.getActed() == 0 && role.getPhyPower() < 20) { // Max is 100
        if (rand() % 100 < 80) {
            AutoUseItem(roleIdx, 48); // Phy Item
        }
    }
    if (actor.getActed()) return;

    // 2. Hidden Weapon (If equipped or in inventory)
    // Simplified: Only if in range of an enemy?
    // Or just check if we can use it.
    // For now, skip complex AI for HiddenWeapon moving.
    
    int targetIdx = -1;
    int minDist = 9999;
    
    for (int i = 0; i < m_battleRoles.size(); ++i) {
        if (m_battleRoles[i].getDead()) continue;
        if (m_battleRoles[i].getTeam() == actor.getTeam()) continue;
        
        int dist = std::abs(m_battleRoles[i].getX() - actor.getX()) + 
                   std::abs(m_battleRoles[i].getY() - actor.getY());
        if (dist < minDist) {
            minDist = dist;
            targetIdx = i;
        }
    }
    
    if (targetIdx != -1) {
        std::cout << "[Auto] target=" << targetIdx
                  << " rnum=" << m_battleRoles[targetIdx].getRNum()
                  << " dist=" << minDist << std::endl;
        // Try Hidden Weapon if in range
        if (GetRoleHidWeapon(actor.getRNum(), true) >= 30) {
             // Check if we have hidden weapon
             for(int i=0; i<4; ++i) {
                 int itemId = role.getTakingItem(i);
                 if (itemId >= 0 && role.getTakingItemAmount(i) > 0) {
                     // Check type? Assuming all items in TakingItem are potentially usable or we check type 4 (Hidden Weapon)
                     Item& item = GameManager::getInstance().getItem(itemId);
                     // if (item.getType() == 4) ...
                     // Assuming we can use it.
                     int range = GetRoleHidWeapon(actor.getRNum(), true) / 15 + 1;
                     if (minDist <= range) {
                         ApplyHiddenWeapon(roleIdx, targetIdx, itemId);
                        std::cout << "[Auto] hiddenWeapon item=" << itemId << " acted=" << actor.getActed() << std::endl;
                         if (actor.getActed()) return;
                     }
                 }
             }
        }

        BattleRole& target = m_battleRoles[targetIdx];
        Role& tData = GameManager::getInstance().getRole(target.getRNum());
        Role& aData = GameManager::getInstance().getRole(actor.getRNum());
        
        // Try best available offensive magic (skip 内功 type 5)
        int level = 1;
        int magicId = FindAttackMagicForAuto(actor.getRNum(), level);
        
        // Calculate move and attack positions
        CalSelectableAreaEx(roleIdx, actor.getTeam(), 0);
        
        int bestMoveX = actor.getX();
        int bestMoveY = actor.getY();
        int bestDistAfterMove = minDist;
        
        int attackRange = 1;
        if (magicId > 0) {
            Magic& magic = GameManager::getInstance().getMagic(magicId);
            attackRange = magic.getAttDistance(level - 1);
            if (attackRange < 1) attackRange = 1;
        }
        
        for (int x = 0; x < 64; x++) {
            for (int y = 0; y < 64; y++) {
                if (m_battleField[3][x][y] < 0) continue;
                int d = std::abs(x - target.getX()) + std::abs(y - target.getY());
                if (d < bestDistAfterMove) {
                    bestDistAfterMove = d;
                    bestMoveX = x;
                    bestMoveY = y;
                }
            }
        }
        
        std::cout << "[Auto] bestMove=(" << bestMoveX << "," << bestMoveY << ") attackRange=" << attackRange 
                  << " targetDist=" << bestDistAfterMove << std::endl;
        
        // Move to best position
        if (bestMoveX != actor.getX() || bestMoveY != actor.getY()) {
            MoveAnimation(roleIdx, bestMoveX, bestMoveY);
        }
        
        const int distAfterMove = std::abs(actor.getX() - target.getX()) + std::abs(actor.getY() - target.getY());
        
        // Attack if in range (use Attack() for action + magic VFX)
        if (distAfterMove <= attackRange && actor.getActed() == 0) {
            if (magicId > 0) {
                std::cout << "[Auto] useMagic=" << magicId << " level=" << level << std::endl;
                Attack(roleIdx, targetIdx, magicId);
            } else {
                std::cout << "[Auto] usePhysical" << std::endl;
                PlayActionAmination(roleIdx, 0, target.getX(), target.getY());
                SoundManager::getInstance().PlaySound(1);
                int att = GetRoleAttack(actor.getRNum(), true);
                int def = GetRoleDefence(target.getRNum(), true);
                int dmg = std::max(1, att - def / 2);
                tData.setCurrentHP(std::max(0, tData.getCurrentHP() - dmg));
                target.setShowNumber(dmg);
                target.setFlashTimer(12);
                ShowHurtValue(0);
                actor.setActed(1);
            }
        }
    }

    // Pascal AutoBattle/AutoBattle2: if nothing succeeded, Rest ends the turn (no ATB progress tweak here).
    if (actor.getActed() == 0) {
        int rnum = actor.getRNum();
        if (rnum >= 0) {
            Role& rData = GameManager::getInstance().getRole(rnum);
            int hurt = rData.getHurt();
            if (hurt < 0) hurt = 0;
            if (hurt > 100) hurt = 100;
            int addHp = ((100 - hurt) * rData.getMaxHP()) / 2000;
            int addMp = ((100 - hurt) * rData.getMaxMP()) / 2000;
            int addPhy = ((100 - hurt) * MAX_PHYSICAL_POWER) / 2000;
            rData.setCurrentHP(std::min((int)rData.getMaxHP(), rData.getCurrentHP() + addHp));
            rData.setCurrentMP(std::min((int)rData.getMaxMP(), rData.getCurrentMP() + addMp));
            rData.setPhyPower(std::min(MAX_PHYSICAL_POWER, rData.getPhyPower() + addPhy));
        }
        actor.setActed(1);
    }
}

// --- Item Usage Implementation ---

void BattleManager::ShowItemMenu(const std::vector<int>& itemIds, int current, int x, int y) {
    if (itemIds.empty()) return;
    int boxW = 160;
    int boxH = 260;
    UIManager::getInstance().DrawRectangle(x, y, boxW, boxH, 0x000000, 0xFFFFFF, 180);
    int maxShow = std::min(10, (int)itemIds.size());
    int cur = current;
    if (cur < 0) cur = 0;
    if (cur >= itemIds.size()) cur = (int)itemIds.size() - 1;
    int startIndex = std::max(0, cur - maxShow + 1);
    for (int i = 0; i < maxShow; ++i) {
        int idx = startIndex + i;
        if (idx >= itemIds.size()) break;
        Item& item = GameManager::getInstance().getItem(itemIds[idx]);
        std::string nameUtf8 = TextManager::getInstance().gbkToUtf8(item.getName());
        int amount = GameManager::getInstance().getItemAmount(itemIds[idx]);
        uint32_t color = (idx == cur) ? 0xFFFF00FF : 0xFFFFFFFF;
        UIManager::getInstance().DrawShadowTextUtf8(nameUtf8, x + 12, y + 12 + i * 22, color, 0x000000FF);
        std::string amt = std::to_string(amount);
        UIManager::getInstance().DrawShadowTextUtf8(amt, x + 120, y + 12 + i * 22, 0xFFFFFFFF, 0x000000FF);
    }
}

void BattleManager::ApplyItemEffect(int rnum, int inum, int where) {
    if (rnum < 0 || inum < 0) return;
    
    Role& role = GameManager::getInstance().getRole(rnum);
    Item& item = GameManager::getInstance().getItem(inum);
    if (item.getEventNum() > 0) {
        EventManager::getInstance().ExecuteEvent(item.getEventNum());
        return;
    }
    GameManager::getInstance().EatOneItem(rnum, inum, where);
    if (where == 0) {
        GameManager::getInstance().useItem(inum);
    } else {
        for (int i = 0; i < 4; ++i) {
            if (role.getTakingItem(i) == inum) {
                int amt = role.getTakingItemAmount(i);
                if (amt > 0) {
                    role.setTakingItemAmount(i, amt - 1);
                    if (role.getTakingItemAmount(i) == 0) {
                        role.setTakingItem(i, -1);
                    }
                }
                break;
            }
        }
    }
}

// Medical (Heal others)
void BattleManager::Medcine(int roleIdx) {
    if (roleIdx < 0 || roleIdx >= m_battleRoles.size()) return;
    BattleRole& actor = m_battleRoles[roleIdx];
    int rnum = actor.getRNum();
    
    int med = GetRoleMedcine(rnum, true);
    int step = med / 15 + 1;
    if (GameManager::getInstance().CheckBattleEffect(rnum, BattleEffectType::Boost_Med_Detox)) {
        step += 2;
    }
    bool manualSelect = (actor.getTeam() == 0 && actor.getAuto() == -1);
    if (manualSelect) {
        if (!SelectAim(roleIdx, step)) return;
    } else {
        m_cursorX = actor.getX();
        m_cursorY = actor.getY();
    }

    int targetIdx = -1;
    if (m_battleField[2][m_cursorX][m_cursorY] >= 0) {
        targetIdx = m_battleField[2][m_cursorX][m_cursorY];
    }
    if (targetIdx >= 0) {
        BattleRole& target = m_battleRoles[targetIdx];
        if (target.getTeam() == actor.getTeam()) {
            ApplyMedicine(roleIdx, targetIdx);
        }
    }
}

void BattleManager::ApplyMedicine(int healerIdx, int targetIdx) {
    BattleRole& actor = m_battleRoles[healerIdx];
    BattleRole& targetBR = m_battleRoles[targetIdx];
    Role& healer = GameManager::getInstance().getRole(actor.getRNum());
    Role& target = GameManager::getInstance().getRole(targetBR.getRNum());
    
    int med = GetRoleMedcine(actor.getRNum(), true);
    int healVal = med * (10 - target.getHurt() / 15) / 10;
    
    if (target.getHurt() - med > 20) healVal = 0;
    if (healVal < 0) healVal = 0;
    if (GameManager::getInstance().CheckBattleEffect(actor.getRNum(), BattleEffectType::Boost_Med_Detox)) {
        healVal = healVal * 3 / 2;
    }
    
    int maxHeal = target.getMaxHP() - target.getCurrentHP();
    healVal = std::min(healVal, maxHeal);
    
    target.setCurrentHP(target.getCurrentHP() + healVal);
    
    int cureHurt = healVal / LIFE_HURT;
    target.setHurt(std::max(0, target.getHurt() - cureHurt));

    if (GetPetSkill(5, 2)) {
        BattleRole& center = m_battleRoles[targetIdx];
        for (int i = 0; i < (int)m_battleRoles.size(); ++i) {
            if (i == targetIdx) continue;
            BattleRole& ally = m_battleRoles[i];
            if (ally.getDead() || ally.getRNum() < 0) continue;
            if (ally.getTeam() != center.getTeam()) continue;
            if (std::abs(ally.getX() - center.getX()) > 3 || std::abs(ally.getY() - center.getY()) > 3) continue;

            Role& allyRole = GameManager::getInstance().getRole(ally.getRNum());
            int areaHeal = med * (10 - allyRole.getHurt() / 15) / 10;
            if (allyRole.getHurt() - med > 20) areaHeal = 0;
            if (areaHeal < 0) areaHeal = 0;
            if (GameManager::getInstance().CheckBattleEffect(actor.getRNum(), BattleEffectType::Boost_Med_Detox)) {
                areaHeal = areaHeal * 3 / 2;
            }
            areaHeal = std::min(areaHeal, (int)allyRole.getMaxHP() - (int)allyRole.getCurrentHP());
            if (areaHeal > 0) {
                actor.setExpGot(actor.getExpGot() + std::max(0, areaHeal / 10));
            }
            allyRole.setCurrentHP(allyRole.getCurrentHP() + areaHeal);
            allyRole.setHurt(std::max(0, allyRole.getHurt() - areaHeal / LIFE_HURT));
            ally.setShowNumber(areaHeal);
            m_battleField[4][ally.getX()][ally.getY()] = 1;
        }
    }

    if (healVal > 0) {
        actor.setExpGot(actor.getExpGot() + std::max(0, healVal / 5));
    }
    actor.setExpGot(actor.getExpGot() + std::max(0, healVal / 10));
    
    targetBR.setShowNumber(healVal);
    SoundManager::getInstance().PlaySound(31);
    PlayActionAmination(healerIdx, 0, targetBR.getX(), targetBR.getY());
    PlayEffectAmination(0, 31, targetBR.getX(), targetBR.getY());
    ShowHurtValue(3);
    
    actor.setActed(1);
    if (!GameManager::getInstance().GetEquipState(actor.getRNum(), 1) &&
        !GameManager::getInstance().GetGongtiState(actor.getRNum(), 1)) {
        healer.setPhyPower(std::max(0, healer.getPhyPower() - 5));
    }
    actor.setProgress(actor.getProgress() - 240);
}

void BattleManager::MedFrozen(int roleIdx) {
    if (roleIdx < 0 || roleIdx >= m_battleRoles.size()) return;
    BattleRole& actor = m_battleRoles[roleIdx];
    int rnum = actor.getRNum();
    Role& rData = GameManager::getInstance().getRole(rnum);
    
    int med = rData.getCurrentMP();
    int step = med / 200 + 1;
    bool manualSelect = (actor.getTeam() == 0 && actor.getAuto() == -1);
    if (manualSelect) {
        if (!SelectAim(roleIdx, step)) return;
    } else {
        m_cursorX = actor.getX();
        m_cursorY = actor.getY();
    }

    int targetIdx = -1;
    if (m_battleField[2][m_cursorX][m_cursorY] >= 0) {
        targetIdx = m_battleField[2][m_cursorX][m_cursorY];
    }
    if (targetIdx >= 0) {
        BattleRole& target = m_battleRoles[targetIdx];
        if (target.getTeam() == actor.getTeam()) {
            ApplyMedFrozen(roleIdx, targetIdx);
        }
    }
}

void BattleManager::ApplyMedFrozen(int healerIdx, int targetIdx) {
    BattleRole& actor = m_battleRoles[healerIdx];
    BattleRole& targetBR = m_battleRoles[targetIdx];
    Role& rData = GameManager::getInstance().getRole(actor.getRNum());
    
    int medcine = GetRoleMedcine(actor.getRNum(), true);
    int cureVal = (rData.getCurrentMP() + medcine * 5) / 3;
    int frozen = targetBR.getFrozen();
    if (cureVal > frozen) cureVal = frozen;
    targetBR.setFrozen(std::max(0, frozen - cureVal));
    targetBR.setShowNumber(cureVal);
    SoundManager::getInstance().PlaySound(32);
    PlayActionAmination(healerIdx, 0, targetBR.getX(), targetBR.getY());
    PlayEffectAmination(0, 32, targetBR.getX(), targetBR.getY());
    ShowHurtValue(4);
    
    actor.setActed(1);
    if (!GameManager::getInstance().GetEquipState(actor.getRNum(), 1) &&
        !GameManager::getInstance().GetGongtiState(actor.getRNum(), 1)) {
        rData.setPhyPower(std::max(0, rData.getPhyPower() - 5));
    }
    actor.setProgress(actor.getProgress() - 240);
}

void BattleManager::MedPoision(int roleIdx) {
    if (roleIdx < 0 || roleIdx >= m_battleRoles.size()) return;
    BattleRole& actor = m_battleRoles[roleIdx];
    int rnum = actor.getRNum();
    
    int medpoi = GetRoleMedPoi(rnum, true);
    int step = medpoi / 15 + 1;
    if (GameManager::getInstance().CheckBattleEffect(rnum, BattleEffectType::Boost_Med_Detox)) {
        step += 2;
    }
    bool manualSelect = (actor.getTeam() == 0 && actor.getAuto() == -1);
    if (manualSelect) {
        if (!SelectAim(roleIdx, step)) return;
    } else {
        m_cursorX = actor.getX();
        m_cursorY = actor.getY();
    }

    int targetIdx = -1;
    if (m_battleField[2][m_cursorX][m_cursorY] >= 0) {
        targetIdx = m_battleField[2][m_cursorX][m_cursorY];
    }
    if (targetIdx >= 0) {
        BattleRole& target = m_battleRoles[targetIdx];
        if (target.getTeam() == actor.getTeam()) {
            ApplyMedPoision(roleIdx, targetIdx);
        }
    }
}

void BattleManager::ApplyMedPoision(int healerIdx, int targetIdx) {
    BattleRole& actor = m_battleRoles[healerIdx];
    BattleRole& targetBR = m_battleRoles[targetIdx];
    Role& rData = GameManager::getInstance().getRole(actor.getRNum());
    Role& tData = GameManager::getInstance().getRole(targetBR.getRNum());
    
    int medpoi = GetRoleMedPoi(actor.getRNum(), true);
    int minuspoi = medpoi;
    int currentPoi = tData.getPoision();
    
    if (minuspoi < currentPoi / 2) minuspoi = 0;
    else if (minuspoi > currentPoi) minuspoi = currentPoi;
    
    minuspoi = std::min(minuspoi, currentPoi);
    if (GameManager::getInstance().CheckBattleEffect(actor.getRNum(), BattleEffectType::Boost_Med_Detox)) {
        minuspoi = minuspoi * 3 / 2;
        minuspoi = std::min(minuspoi, currentPoi);
    }
    
    if (minuspoi > 0) {
        actor.setExpGot(actor.getExpGot() + std::max(0, minuspoi / 5));
    }
    
    tData.setPoision(currentPoi - minuspoi);
    
    if (GetPetSkill(5, 2)) {
        BattleRole& center = m_battleRoles[targetIdx];
        for (int i = 0; i < (int)m_battleRoles.size(); ++i) {
            if (i == targetIdx) continue;
            BattleRole& ally = m_battleRoles[i];
            if (ally.getDead() || ally.getRNum() < 0) continue;
            if (ally.getTeam() != center.getTeam()) continue;
            if (std::abs(ally.getX() - center.getX()) > 3 || std::abs(ally.getY() - center.getY()) > 3) continue;

            Role& allyRole = GameManager::getInstance().getRole(ally.getRNum());
            int areaMinus = medpoi;
            int allyPoi = allyRole.getPoision();
            if (areaMinus < allyPoi / 2) areaMinus = 0;
            else if (areaMinus > allyPoi) areaMinus = allyPoi;
            areaMinus = std::min(areaMinus, allyPoi);
            if (GameManager::getInstance().CheckBattleEffect(actor.getRNum(), BattleEffectType::Boost_Med_Detox)) {
                areaMinus = areaMinus * 3 / 2;
                areaMinus = std::min(areaMinus, allyPoi);
            }
            if (areaMinus > 0) {
                actor.setExpGot(actor.getExpGot() + std::max(0, areaMinus / 5));
            }
            allyRole.setPoision(allyPoi - areaMinus);
            ally.setShowNumber(areaMinus);
            m_battleField[4][ally.getX()][ally.getY()] = 1;
        }
    }

    targetBR.setShowNumber(minuspoi);
    SoundManager::getInstance().PlaySound(33);
    PlayActionAmination(healerIdx, 0, targetBR.getX(), targetBR.getY());
    PlayEffectAmination(0, 33, targetBR.getX(), targetBR.getY());
    ShowHurtValue(4);
    
    actor.setActed(1);
    if (!GameManager::getInstance().GetEquipState(actor.getRNum(), 1) &&
        !GameManager::getInstance().GetGongtiState(actor.getRNum(), 1)) {
        rData.setPhyPower(std::max(0, rData.getPhyPower() - 5));
    }
    actor.setProgress(actor.getProgress() - 240);
}

void BattleManager::UsePoision(int roleIdx) {
    if (roleIdx < 0 || roleIdx >= m_battleRoles.size()) return;
    BattleRole& actor = m_battleRoles[roleIdx];
    int rnum = actor.getRNum();
    
    int usepoi = GetRoleUsePoi(rnum, true);
    int step = usepoi / 15 + 1;
    
    if (SelectAim(roleIdx, step)) {
        int targetIdx = -1;
        if (m_battleField[2][m_cursorX][m_cursorY] >= 0) {
            targetIdx = m_battleField[2][m_cursorX][m_cursorY];
        }
        
        if (targetIdx >= 0) {
            BattleRole& target = m_battleRoles[targetIdx];
            if (target.getTeam() != actor.getTeam()) {
                ApplyUsePoision(roleIdx, targetIdx);
            }
        }
    }
}

void BattleManager::ApplyUsePoision(int attackerIdx, int targetIdx) {
    BattleRole& actor = m_battleRoles[attackerIdx];
    int targetFinal = targetIdx;
    if (GameManager::getInstance().GetEquipState(m_battleRoles[targetFinal].getRNum(), 4) ||
        GameManager::getInstance().GetGongtiState(m_battleRoles[targetFinal].getRNum(), 4)) {
        targetFinal = ReMoveHurt(targetFinal, attackerIdx);
    }
    if (GameManager::getInstance().GetEquipState(m_battleRoles[targetFinal].getRNum(), 5) ||
        GameManager::getInstance().GetGongtiState(m_battleRoles[targetFinal].getRNum(), 5)) {
        targetFinal = RetortHurt(targetFinal, attackerIdx);
    }
    
    BattleRole& targetBR = m_battleRoles[targetFinal];
    Role& rData = GameManager::getInstance().getRole(actor.getRNum());
    Role& tData = GameManager::getInstance().getRole(targetBR.getRNum());
    
    int usepoi = GetRoleUsePoi(actor.getRNum(), true);
    int addpoi = usepoi / 3 - GetRoleDefPoi(targetBR.getRNum(), true) / 4;
    if (addpoi < 0) addpoi = 0;
    int maxAdd = usepoi - tData.getPoision();
    if (maxAdd < 0) maxAdd = 0;
    addpoi = std::min(addpoi, maxAdd);
    
    int diff = GameManager::getInstance().getRole(0).getDifficulty();
    if (actor.getTeam() == 0) addpoi = addpoi * (200 - diff) / 200;
    if (actor.getTeam() == 1) addpoi = addpoi * (200 + diff) / 200;
    
    if (targetBR.getPerfectDodge() > 0) addpoi = 0;
    if (GameManager::getInstance().GetGongtiState(targetBR.getRNum(), 12) ||
        GameManager::getInstance().GetEquipState(targetBR.getRNum(), 12) ||
        CheckEquipSet(tData.getEquip(0), tData.getEquip(1), tData.getEquip(2), tData.getEquip(3)) == 4) {
        addpoi = 0;
    }
    
    if (addpoi > 0) {
        actor.setExpGot(actor.getExpGot() + std::max(0, addpoi / 5));
    }
    
    tData.setPoision(std::min(99, tData.getPoision() + addpoi));
    
    targetBR.setShowNumber(addpoi);
    SoundManager::getInstance().PlaySound(34);
    PlayActionAmination(attackerIdx, 0, targetBR.getX(), targetBR.getY());
    PlayEffectAmination(0, 34, targetBR.getX(), targetBR.getY());
    ShowHurtValue(2);
    
    actor.setActed(1);
    if (!GameManager::getInstance().GetEquipState(actor.getRNum(), 1) &&
        !GameManager::getInstance().GetGongtiState(actor.getRNum(), 1)) {
        rData.setPhyPower(std::max(0, rData.getPhyPower() - 3));
    }
    actor.setProgress(actor.getProgress() - 240);
}

void BattleManager::UseHiddenWeapen(int roleIdx, int itemIdx) {
    if (roleIdx < 0 || roleIdx >= m_battleRoles.size()) return;
    BattleRole& actor = m_battleRoles[roleIdx];
    int rnum = actor.getRNum();
    
    int hidden = GetRoleHidWeapon(rnum, true);
    int step = hidden / 15 + 1;
    if (GameManager::getInstance().CheckBattleEffect(rnum, BattleEffectType::HiddenWeapon_Boost)) {
        step += 1;
    }
    
    Item& item = GameManager::getInstance().getItem(itemIdx);
    if (item.getEventNum() > 0) {
        EventManager::getInstance().ExecuteEvent(item.getEventNum());
        return;
    }
    
    if (SelectAim(roleIdx, step)) {
        int targetIdx = -1;
        if (m_battleField[2][m_cursorX][m_cursorY] >= 0) {
            targetIdx = m_battleField[2][m_cursorX][m_cursorY];
        }
        
        if (targetIdx >= 0) {
            BattleRole& target = m_battleRoles[targetIdx];
            if (target.getTeam() != actor.getTeam()) {
                ApplyHiddenWeapon(roleIdx, targetIdx, itemIdx);
            }
        }
    }
}

void BattleManager::ApplyHiddenWeapon(int attackerIdx, int targetIdx, int itemIdx) {
    BattleRole& actor = m_battleRoles[attackerIdx];
    BattleRole& targetBR = m_battleRoles[targetIdx];
    Role& tData = GameManager::getInstance().getRole(targetBR.getRNum());
    Role& rData = GameManager::getInstance().getRole(actor.getRNum());
    Item& item = GameManager::getInstance().getItem(itemIdx);
    
    int hidden = GetRoleHidWeapon(actor.getRNum(), true);
    
    bool consumed = false;
    for (int i = 0; i < 4; ++i) {
        if (rData.getTakingItem(i) == itemIdx) {
            int amt = rData.getTakingItemAmount(i);
            if (amt > 0) {
                rData.setTakingItemAmount(i, amt - 1);
                if (rData.getTakingItemAmount(i) == 0) rData.setTakingItem(i, -1);
                consumed = true;
            }
            break;
        }
    }
    if (!consumed && actor.getTeam() == 0) {
        GameManager::getInstance().useItem(itemIdx);
    }
    
    int hurt = 0;
    int poi = 0;
    if (targetBR.getPerfectDodge() <= 0) {
        hurt = -(hidden * item.getAddCurrentHP()) / 100;
        if (hurt < 25) hurt = 25;
        int diff = GameManager::getInstance().getRole(0).getDifficulty();
        if (actor.getTeam() == 0) hurt = hurt * (200 - diff) / 200;
        if (actor.getTeam() == 1) hurt = hurt * (200 + diff) / 200;
        tData.setCurrentHP(std::max(0, tData.getCurrentHP() - hurt));
        
        poi = std::max(0, (hidden * item.getAddPoi()) / 100 - GetRoleDefPoi(targetBR.getRNum(), true));
        if (actor.getTeam() == 0) poi = poi * (200 - diff) / 200;
        if (actor.getTeam() == 1) poi = poi * (200 + diff) / 200;
        if (GameManager::getInstance().GetGongtiState(targetBR.getRNum(), 12) ||
            GameManager::getInstance().GetEquipState(targetBR.getRNum(), 12) ||
            CheckEquipSet(tData.getEquip(0), tData.getEquip(1), tData.getEquip(2), tData.getEquip(3)) == 4) {
            poi = 0;
        }
        tData.setPoision(std::min(99, tData.getPoision() + poi));
    }
    
    targetBR.setShowNumber(hurt);
    targetBR.setFlashTimer(12);
    SoundManager::getInstance().PlaySound(item.getAmiNum());
    PlayActionAmination(attackerIdx, 0, targetBR.getX(), targetBR.getY());
    PlayEffectAmination(0, item.getAmiNum(), targetBR.getX(), targetBR.getY());
    ShowHurtValue(0);
    
    actor.setActed(1);
    actor.setProgress(actor.getProgress() - 240);
}

void BattleManager::AutoUseItem(int roleIdx, int listType) {
    if (roleIdx < 0 || roleIdx >= m_battleRoles.size()) return;
    BattleRole& actor = m_battleRoles[roleIdx];
    int rnum = actor.getRNum();
    Role& role = GameManager::getInstance().getRole(rnum);
    
    int bestItemIdx = -1;
    int maxVal = 0;
    int where = 0; // 0: Bag, 1: TakingItem
    
    if (actor.getTeam() != 0) { // Enemy or Ally (Non-Player)
        where = 1;
        for (int i = 0; i < 4; ++i) {
            int itemId = role.getTakingItem(i);
            int amount = role.getTakingItemAmount(i);
            if (itemId >= 0 && amount > 0) {
                Item& item = GameManager::getInstance().getItem(itemId);
                if (item.getEventNum() > 0) continue;
                
                int val = 0;
                if (listType == 45) val = item.getAddCurrentHP();
                else if (listType == 50) val = item.getAddCurrentMP();
                else if (listType == 48) val = item.getAddPhyPower();
                
                if (val > maxVal) {
                    maxVal = val;
                    bestItemIdx = itemId;
                }
            }
        }
    } else { // Player Team
        where = 0;
        const auto& bag = GameManager::getInstance().getItemList();
        for (const auto& inv : bag) {
            if (inv.id < 0 || inv.amount <= 0) continue;
            Item& item = GameManager::getInstance().getItem(inv.id);
            if (item.getItemType() != 3) continue;
            if (item.getEventNum() > 0) continue;
            int val = 0;
            if (listType == 45) val = item.getAddCurrentHP();
            else if (listType == 50) val = item.getAddCurrentMP();
            else if (listType == 48) val = item.getAddPhyPower();
            
            if (val > maxVal) {
                maxVal = val;
                bestItemIdx = inv.id;
            }
        }
    }
    
    if (bestItemIdx >= 0) {
        ApplyItemEffect(rnum, bestItemIdx, where);
        RenderBattle();
        UIManager::getInstance().UpdateScreen();
        SDL_Delay(750);
        actor.setActed(1);
        actor.setProgress(actor.getProgress() - 240);
    }
}

int BattleManager::ReMoveHurt(int targetIdx, int attackerIdx) {
    if (targetIdx < 0 || targetIdx >= m_battleRoles.size()) return targetIdx;
    BattleRole& target = m_battleRoles[targetIdx];
    if (target.getRNum() < 0) return targetIdx;
    int chance = 30 + GameManager::getInstance().getRole(target.getRNum()).getAptitude() / 5;
    if (rand() % 100 >= chance) return targetIdx;
    std::vector<int> candidates;
    for (int i = 0; i < m_battleRoles.size(); ++i) {
        if (i == targetIdx || i == attackerIdx) continue;
        BattleRole& r = m_battleRoles[i];
        if (r.getDead() != 0 || r.getRNum() < 0) continue;
        if (r.getTeam() == target.getTeam()) continue;
        int dist = std::abs(r.getX() - target.getX()) + std::abs(r.getY() - target.getY());
        if (dist <= 5) candidates.push_back(i);
    }
    if (candidates.empty()) return targetIdx;
    return candidates[rand() % candidates.size()];
}

int BattleManager::RetortHurt(int targetIdx, int attackerIdx) {
    if (targetIdx < 0 || targetIdx >= m_battleRoles.size()) return targetIdx;
    BattleRole& target = m_battleRoles[targetIdx];
    if (target.getRNum() < 0) return targetIdx;
    int chance = 30 + GameManager::getInstance().getRole(target.getRNum()).getAptitude() / 5;
    if (rand() % 100 >= chance) return targetIdx;
    return attackerIdx;
}

int BattleManager::CalHurtValue(int attackerIdx, int targetIdx, int magicId, int level) {

    if (attackerIdx < 0 || attackerIdx >= m_battleRoles.size()) return 0;

    BattleRole& attRole = m_battleRoles[attackerIdx];
    BattleRole& tarRole = m_battleRoles[targetIdx];
    int rnum1 = attRole.getRNum();
    int rnum2 = tarRole.getRNum();

    // 1. Calculate Knowledge (Martial Arts Knowledge)
    int k1 = 0;
    int k2 = 0;
    int l1 = 0;
    int c = 0;
    int minKnowledge = 0; // MIN_KNOWLEDGE defaults to 0 in kys_main.pas

    for (const auto& r : m_battleRoles) {
        if (r.getTeam() == attRole.getTeam() && r.getDead() == 0 && r.getKnowledge() > minKnowledge) {
            k1 += r.getKnowledge();
        }
        if (r.getTeam() == tarRole.getTeam() && r.getDead() == 0 && r.getKnowledge() > minKnowledge) {
            k2 += r.getKnowledge();
        }
        if (r.getTeam() == 0 && r.getRNum() >= 0) {
            l1 += r.getLevel();
            c++;
        }
    }
    
    if (c == 0) {
         l1 = GameManager::getInstance().getRole(0).getLevel();
    } else {
         l1 = l1 / c;
    }

    Role& r0 = GameManager::getInstance().getRole(0);
    if (attRole.getTeam() != 0) k1 += l1 * r0.getDifficulty() / 50;
    if (tarRole.getTeam() != 0) k2 += l1 * r0.getDifficulty() / 50;
    
    int knowledge = k1 - k2;
    knowledge = std::min(knowledge, 100);
    knowledge = std::max(knowledge, -100);

    // 2. Base Hurt
    Magic& magic = GameManager::getInstance().getMagic(magicId);
    // Formula: (CalNewHurtValue(...) * (100 + (knowledge * 4) div 5)) div 100;
    int baseHurt = CalNewHurtValue(level - 1, magic.getMinHurt(), magic.getMaxHurt(), magic.getHurtModulus());
    int mhurt = (baseHurt * (100 + (knowledge * 4) / 5)) / 100;

    int p = magic.getAttackModulus() * 6 + magic.getMPModulus() + magic.getSpeedModulus() * 2 + magic.getWeaponModulus() * 2;
    int att = GetRoleAttack(rnum1, true) + 1;
    int def = GetRoleDefence(rnum2, true) + 1;
    
    int wpn1 = 0, wpn2 = 0;
    switch(magic.getMagicType()) {
        case 1: 
            wpn1 = GetRoleFist(rnum1, true) + 1; 
            wpn2 = GetRoleFist(rnum2, true) + 1; 
            break;
        case 2: 
            wpn1 = GetRoleSword(rnum1, true) + 1; 
            wpn2 = GetRoleSword(rnum2, true) + 1; 
            break;
        case 3: 
            wpn1 = GetRoleKnife(rnum1, true) + 1; 
            wpn2 = GetRoleKnife(rnum2, true) + 1; 
            break;
        case 4: 
            wpn1 = GetRoleUnusual(rnum1, true) + 1; 
            wpn2 = GetRoleUnusual(rnum2, true) + 1; 
            break;
    }
    
    int mp1 = GameManager::getInstance().getRole(rnum1).getCurrentMP() + 1;
    int mp2 = GameManager::getInstance().getRole(rnum2).getCurrentMP() + 1;
    
    int spd1 = GetRoleSpeed(rnum1, true) + 1;
    int spd2 = GetRoleSpeed(rnum2, true) + 1;
    
    // CheckEquipSet logic
    Role& r1 = GameManager::getInstance().getRole(rnum1);
    Role& r2 = GameManager::getInstance().getRole(rnum2);
    if (CheckEquipSet(r1.getEquip(0), r1.getEquip(1), r1.getEquip(2), r1.getEquip(3)) == 5) {
        att += 50;
        spd1 += 30;
    }
    if (CheckEquipSet(r2.getEquip(0), r2.getEquip(1), r2.getEquip(2), r2.getEquip(3)) == 5) {
        def -= 25;
        spd2 += 30;
    }
    
    double result = 0;
    att = std::max(att, 1);
    def = std::max(def, 1);
    spd1 = std::max(spd1, 1);
    wpn1 = std::max(wpn1, 1);
    mp1 = std::max(mp1, 1);
    spd2 = std::max(spd2, 1);
    wpn2 = std::max(wpn2, 1);
    mp2 = std::max(mp2, 1);
    
    double a1 = att - def;
    double s1 = spd1 - spd2;
    double w1 = wpn1 - wpn2;
    double m1 = mp1 - mp2;
    
    if (a1 < 5) a1 = 5;
    if (w1 < 5) w1 = 5;
    if (s1 < 5) s1 = 5;
    if (m1 < 5) m1 = 5;
    
    a1 = std::min((a1 / att), 1.0);
    w1 = std::min((w1 / wpn1), 1.0);
    s1 = std::min((s1 / spd1), 1.0);
    m1 = std::min((m1 / mp1), 1.0);
    
    if (p > 0) {
        if (magic.getAttackModulus() > 0)
            result += (int)(mhurt * a1 * (magic.getAttackModulus() * 3 * 2 / (double)p));
        if (magic.getMPModulus() > 0)
            result += (int)(mhurt * m1 * (magic.getMPModulus() / (double)p));
        if (magic.getSpeedModulus() > 0)
            result += (int)(mhurt * s1 * (magic.getSpeedModulus() * 2 / (double)p));
        if (magic.getWeaponModulus() > 0)
            result += (int)(mhurt * w1 * (magic.getWeaponModulus() * 2 / (double)p));
    }
    
    result += (rand() % 10) - (rand() % 10);
    if (result < mhurt / 20.0) {
        result = mhurt / 20.0 + (rand() % 5) - (rand() % 5);
    }
    
    int dis = std::abs(attRole.getX() - tarRole.getX()) + std::abs(attRole.getY() - tarRole.getY());
    if (dis > 10) dis = 10;
    result = result * (100 - (dis - 1) * 3) / 100.0;
    
    if (result <= 0 || level <= 0) {
        result = (rand() % 10) + 1;
    }
    if (result > 9999) result = 9999;
    
    return (int)result;
}

// Applies damage to roles in range — aligned with kys_battle.pas CalHurtRole
void BattleManager::CalHurtRole(int attackerIdx, int magicId, int level) {
    if (attackerIdx < 0 || attackerIdx >= (int)m_battleRoles.size()) return;
    BattleRole& attacker = m_battleRoles[attackerIdx];
    int rnum = attacker.getRNum();
    if (rnum < 0) return;
    Role& attRole = GameManager::getInstance().getRole(rnum);
    Magic& magic = GameManager::getInstance().getMagic(magicId);
    const int battleMode = GameManager::getInstance().getBattleMode();

    for (auto& br : m_battleRoles) {
        br.setShowNumber(-1);
    }

    // Reduce level if MP insufficient
    if (magic.getNeedMP() > 0 && attRole.getCurrentMP() < magic.getNeedMP() * level) {
        level = attRole.getCurrentMP() / std::max(1, (int)magic.getNeedMP());
    }
    if (level > 10) level = 10;
    if (level < 0) level = 0;
    int needmp = magic.getNeedMP() * level;

    int addhpvalue = 0;
    int addmpvalue = 0;

    auto hasNegImmune = [&](int trnum) {
        Role& tr = GameManager::getInstance().getRole(trnum);
        return GameManager::getInstance().CheckBattleEffect(trnum, 12)
            || CheckEquipSet(tr.getEquip(0), tr.getEquip(1), tr.getEquip(2), tr.getEquip(3)) == 4;
    };

    for (int i = 0; i < (int)m_battleRoles.size(); ++i) {
        BattleRole& originalTarget = m_battleRoles[i];
        if (originalTarget.getDead() || originalTarget.getRNum() < 0) continue;
        if (m_battleField[4][originalTarget.getX()][originalTarget.getY()] == 0) continue;
        // Pascal: only hit enemies; never self
        if (originalTarget.getTeam() == attacker.getTeam() || attackerIdx == i) continue;

        int bang = 0;
        int dodge = std::min(1, (int)originalTarget.getAddDodge()) * 30;
        if (originalTarget.getPerfectDodge() > 0) dodge = 100;

        int hurt = CalHurtValue(attackerIdx, i, magicId, level);

        // Team-0 NeedProgress bonus
        if (attacker.getTeam() == 0 && battleMode > 0) {
            hurt += (hurt * magic.getNeedProgress() * level) / 20;
        }
        // 内伤影响：目标受伤越多伤害越高；攻击者内伤越多伤害越低
        Role& origData = GameManager::getInstance().getRole(originalTarget.getRNum());
        hurt = (hurt * (400 + origData.getHurt())) / 400;
        hurt = (hurt * (400 - attRole.getHurt())) / 400;

        // 酒：加攻 / 加防状态
        if (attacker.getAddAtt() > 0) {
            hurt = (int)(hurt * 1.4);
            if (GameManager::getInstance().CheckBattleEffect(rnum, 3)) {
                hurt = (int)(hurt * 1.4);
            }
        }
        if (originalTarget.getAddDef() > 0) {
            hurt = (int)(hurt * 0.6);
            if (GameManager::getInstance().CheckBattleEffect(originalTarget.getRNum(), 3)) {
                hurt = (int)(hurt * 0.6);
            }
        }

        const bool hurtHp = (magic.getHurtType() == 0);
        const bool hurtMp = (magic.getHurtType() == 1);

        if (hurtHp || hurtMp) {
            // 威力加成：HP 用 14+type，吸星用 15+type；13 全体；2 女性
            int typeBoostBase = hurtHp ? 14 : 15;
            bool boost = GameManager::getInstance().CheckBattleEffect(rnum, typeBoostBase + magic.getMagicType())
                || GameManager::getInstance().CheckBattleEffect(rnum, 13)
                || (attRole.getSexual() == 1 && GameManager::getInstance().CheckBattleEffect(rnum, 2));
            if (boost) hurt = (int)(hurt * 1.3);

            if (GameManager::getInstance().CheckBattleEffect(originalTarget.getRNum(), 8)) {
                dodge += 30;
            }

            if (GameManager::getInstance().CheckBattleEffect(rnum, 9)) {
                if (hurtHp) {
                    hurt = (hurt * (7 + ((attRole.getLevel() - 1) % 10))) / 10;
                } else {
                    hurt = (int)(hurt * (1.0 + ((std::rand() % 3) - 1) * 0.5));
                }
            }

            int n = i;
            if (GameManager::getInstance().CheckBattleEffect(originalTarget.getRNum(), 4)) {
                n = ReMoveHurt(i, attackerIdx);
            }
            if (GameManager::getInstance().CheckBattleEffect(originalTarget.getRNum(), 5)) {
                n = RetortHurt(i, attackerIdx);
            }
            if (n < 0 || n >= (int)m_battleRoles.size()) n = i;

            BattleRole& finalTarget = m_battleRoles[n];
            Role& tData = GameManager::getInstance().getRole(finalTarget.getRNum());

            if (battleMode > 1) {
                bang += attRole.getAngry() / 5;
                dodge += tData.getAngry() / 10;
                bang = std::min(bang, 100);
                dodge = std::min(dodge, 100);
                int angryGain = (hurt * 50) / std::max(1, (int)tData.getMaxHP());
                if (angryGain <= 0) angryGain = 1;
                tData.setAngry(std::min(100, tData.getAngry() + angryGain));
            }
            if ((std::rand() % 100) < (bang - 1)) hurt = (int)(hurt * 1.3);
            if ((std::rand() % 100) < dodge) hurt = 0;

            finalTarget.setShowNumber(hurt);

            if (hurtHp) {
                hurt = std::min((int)tData.getCurrentHP(), hurt);
                tData.setCurrentHP(std::max(0, tData.getCurrentHP() - hurt));
                if (tData.getCurrentHP() <= 0) {
                    // Pascal CalHurtRole: only track kill/exp here; Dead is set in ClearDeadRolePic after ShowHurtValue.
                    attacker.setKilled(attacker.getKilled() + 1);
                    attacker.setExpGot(attacker.getExpGot() + tData.getLevel() * 10);
                }
            } else {
                hurt = std::min((int)tData.getCurrentMP(), hurt);
                tData.setCurrentMP(std::max(0, tData.getCurrentMP() - hurt));
            }

            if (hurt > 0) {
                finalTarget.setFlashTimer(12);
            }

            // 武功自带回血回内比例（对本次 hurt）
            addmpvalue += (magic.getAddMpScale() * hurt) / 100;
            // 功体吸内
            int gongti = attRole.getGongti();
            if (gongti > 0) {
                Magic& gMagic = GameManager::getInstance().getMagic(gongti);
                int gLvIndex = std::min(2, std::max(0, (int)gMagic.getMaxLevel()));
                int needExp = gMagic.getNeedExp(gLvIndex);
                if (needExp <= GetMagicLevel(rnum, gongti) && gMagic.getAddMpScale() > 0) {
                    int hurtmp = (hurt * gMagic.getAddMpScale()) / 100;
                    // Pascal drains original cell i's MP; keep that quirk for fidelity
                    Role& drainRole = GameManager::getInstance().getRole(originalTarget.getRNum());
                    if (hurtmp > drainRole.getCurrentMP()) {
                        // Pascal assigns leftover into hurt (unused afterward) — drain full CurrentMP path:
                        hurtmp = drainRole.getCurrentMP();
                    }
                    drainRole.setCurrentMP(std::max(0, drainRole.getCurrentMP() - hurtmp));
                    addmpvalue += hurtmp;
                }
            }

            addhpvalue += (magic.getAddHpScale() * hurt) / 100;
            if (GameManager::getInstance().CheckBattleEffect(rnum, 21)) {
                addhpvalue += hurt / 10;
            }
            if (gongti > 0) {
                Magic& gMagic = GameManager::getInstance().getMagic(gongti);
                if (GetGongtiLevel(rnum, gongti) == gMagic.getMaxLevel() && gMagic.getAddHpScale() > 0) {
                    addhpvalue += (magic.getAddHpScale() * hurt) / 100;
                }
            }

            if (hurt > 0) {
                int tn = finalTarget.getRNum();
                if (!hasNegImmune(tn)) {
                    int addpoi = GetRoleAttPoi(rnum, true) + magic.getPoision() * level
                        - GetRoleDefPoi(tn, true);
                    if (addpoi + tData.getPoision() > 99) addpoi = 99 - tData.getPoision();
                    if (addpoi < 0) addpoi = 0;
                    if (GetRoleDefPoi(tn, true) >= 99) addpoi = 0;
                    tData.setPoision(tData.getPoision() + addpoi);
                }

                if (!hasNegImmune(tn)
                    && !GameManager::getInstance().CheckBattleEffect(tn, 6)) {
                    int injury = ((magic.getMaxInjury() - magic.getMinInjury()) * (level - 1)) / 9
                        + magic.getMinInjury();
                    if (GameManager::getInstance().CheckBattleEffect(rnum, 19)) injury += 30;
                    if (CheckEquipSet(attRole.getEquip(0), attRole.getEquip(1),
                                      attRole.getEquip(2), attRole.getEquip(3)) == 3) {
                        injury = 100;
                    }
                    if ((std::rand() % 100) < injury) {
                        tData.setHurt(std::min(100, tData.getHurt() + (int)std::lround(hurt / 10.0)));
                    }
                }

                if (!hasNegImmune(tn)) {
                    int frozen = ((magic.getMaxPeg() - magic.getMinPeg()) * (level - 1)) / 9
                        + magic.getMinPeg();
                    if (GameManager::getInstance().CheckBattleEffect(rnum, 20)) frozen += 10;
                    if ((std::rand() % 100) < frozen) {
                        int mpSelf = attRole.getCurrentMP();
                        int mpTar = std::max(1, (int)tData.getCurrentMP());
                        int addFrz = (int)std::lround(((mpSelf - (mpTar / 2)) / (double)(mpTar + 1)) * 200.0);
                        finalTarget.setFrozen(finalTarget.getFrozen() + addFrz);
                    }
                    finalTarget.setFrozen(std::min(500, (int)finalTarget.getFrozen()));
                }

                if (GameManager::getInstance().CheckBattleEffect(rnum, 7)) {
                    tData.setPhyPower(std::max(0, tData.getPhyPower() - 5));
                }

                if (GameManager::getInstance().CheckBattleEffect(rnum, 25)) {
                    int hurtmp = hurt / 10;
                    if (hurtmp > tData.getCurrentMP()) hurtmp = tData.getCurrentMP();
                    tData.setCurrentMP(std::max(0, tData.getCurrentMP() - hurtmp));
                    addmpvalue += hurtmp;
                }
            }
        }
    }

    ShowHurtValue(magic.getHurtType());

    // 攻击者消耗
    if (GameManager::getInstance().CheckBattleEffect(rnum, 10)) {
        needmp = (needmp * 4) / 5;
    }
    if (attRole.getAttTwice() == 1) needmp = needmp / 2;
    attRole.setCurrentMP(attRole.getCurrentMP() - needmp);
    attRole.setCurrentHP(attRole.getCurrentHP() - (needmp * attRole.getHurt()) / 100);

    if (!GameManager::getInstance().CheckBattleEffect(rnum, 1)) {
        attRole.setPhyPower(attRole.getPhyPower() - 3);
    }

    attRole.setCurrentHP(attRole.getCurrentHP() - magic.getNeedHP() * ((level + 1) / 2));
    if (attRole.getCurrentHP() < 0) attRole.setCurrentHP(0);
    if (attRole.getCurrentHP() > attRole.getMaxHP()) attRole.setCurrentHP(attRole.getMaxHP());
    if (attRole.getCurrentMP() < 0) attRole.setCurrentMP(0);

    if (battleMode > 1) {
        attRole.setAngry(std::min(100, attRole.getAngry() + 1));
    }

    if (addmpvalue > 0) {
        attacker.setShowNumber(addmpvalue);
        attRole.setCurrentMP(std::min((int)attRole.getMaxMP(), attRole.getCurrentMP() + addmpvalue));
        ShowHurtValue(1); // purple MP
    }
    if (addhpvalue > 0) {
        attacker.setShowNumber(addhpvalue);
        attRole.setCurrentHP(std::min((int)attRole.getMaxHP(), attRole.getCurrentHP() + addhpvalue));
        ShowHurtValue(3); // green heal
    }
}

void BattleManager::SetAttackArea(int type, int ax, int ay, int range, int bx, int by, int step) {
    // Clear Layer 4
    for(int x=0; x<64; ++x)
        for(int y=0; y<64; ++y)
            m_battleField[4][x][y] = 0;

    auto sign = [](int v) { return (v > 0) - (v < 0); };
    for (int i = 0; i < 64; ++i) {
        for (int j = 0; j < 64; ++j) {
            if (m_battleField[0][i][j] <= 0) continue; // Must have ground

            bool inArea = false;
            switch (type) {
                case 0: // Point / Diamond
                    if (std::abs(i - ax) + std::abs(j - ay) <= range) inArea = true;
                    break;
                case 1: // Line (Directional)
                    if (((i == bx) || (j == by)) &&
                        (sign(ax - bx) == sign(i - bx)) &&
                        (sign(ay - by) == sign(j - by)) &&
                        (std::abs(i - bx) <= step) &&
                        (std::abs(j - by) <= step)) {
                        inArea = true;
                    }
                    break;
                case 2: // Cross / Star (Centered at Attacker) — Pascal SetAminationPosition mode 2
                    if ((std::abs(i - bx) == std::abs(j - by) && std::abs(i - bx) <= range) ||
                        (i == bx && std::abs(j - by) <= step) ||
                        (j == by && std::abs(i - bx) <= step) ||
                        (i == bx && j == by)) {
                        inArea = true;
                    }
                    break;
                case 3: // Square
                    if (std::abs(i - ax) <= range && std::abs(j - ay) <= range) inArea = true;
                    break;
                case 4: // Directional Diamond
                    if (std::abs(i - bx) + std::abs(j - by) <= step && std::abs(i - bx) != std::abs(j - by)) {
                        bool dir1 = ((i - bx) * (ax - bx) > 0) && (std::abs(i - bx) > std::abs(j - by));
                        bool dir2 = ((j - by) * (ay - by) > 0) && (std::abs(i - bx) < std::abs(j - by));
                        if (dir1 || dir2) inArea = true;
                    }
                    break;
                case 5: // Directional Square
                    if (std::abs(i - bx) <= step && std::abs(j - by) <= step && std::abs(i - bx) != std::abs(j - by)) {
                        bool dir1 = ((i - bx) * (ax - bx) > 0) && (std::abs(i - bx) > std::abs(j - by));
                        bool dir2 = ((j - by) * (ay - by) > 0) && (std::abs(i - bx) < std::abs(j - by));
                        if (dir1 || dir2) inArea = true;
                    }
                    break;
                case 6: // Far (Same as Point for Area)
                    if (std::abs(i - ax) + std::abs(j - ay) <= range) inArea = true;
                    break;
                case 7: { // Line (Non-directional)
                    if (!(i == bx && j == by) && (std::abs(i - bx) + std::abs(j - by) <= step)) {
                        int dax = ax - bx;
                        int day = ay - by;
                        if ((std::abs(i - bx) <= std::abs(dax)) && (std::abs(j - by) <= std::abs(day))) {
                            if ((std::abs(dax) > std::abs(day)) && (dax != 0)) {
                                if (((i - bx) / (double)dax) > 0) {
                                    int expected = (int)std::lround(((i - bx) * (double)day) / (double)dax) + by;
                                    if (j == expected) inArea = true;
                                }
                            } else if ((day != 0)) {
                                if (((j - by) / (double)day) > 0) {
                                    int expected = (int)std::lround(((j - by) * (double)dax) / (double)day) + bx;
                                    if (i == expected) inArea = true;
                                }
                            }
                        }
                    }
                    break;
                }
                default: // Fallback to Point
                    if (i == ax && j == ay) inArea = true;
                    break;
            }
            
            if (inArea) m_battleField[4][i][j] = 1;
        }
    }
}

void BattleManager::Attack(int roleIdx, int targetIdx, int magicId) {
    if (roleIdx < 0 || roleIdx >= m_battleRoles.size()) return;
    BattleRole& attacker = m_battleRoles[roleIdx];
    int attackerX = attacker.getX();
    int attackerY = attacker.getY();
    int rnum = attacker.getRNum();

    int level = GetMagicBattleLevel(rnum, magicId);
    Magic& magic = GameManager::getInstance().getMagic(magicId);
    Role& rData = GameManager::getInstance().getRole(rnum);

    int step = magic.getMoveDistance(level - 1);
    if (CheckEquipSet(rData.getEquip(0), rData.getEquip(1), rData.getEquip(2), rData.getEquip(3)) == 1) step += 1;
    if (GameManager::getInstance().CheckBattleEffect(rnum, 22)) step += 1;
    if (step < 0) step = 0;
    int range = magic.getAttDistance(level - 1);
    if (range < 0) range = 0;

    int targetX = -1, targetY = -1;
    if (targetIdx >= 0 && targetIdx < (int)m_battleRoles.size()) {
        targetX = m_battleRoles[targetIdx].getX();
        targetY = m_battleRoles[targetIdx].getY();
    } else {
        targetX = m_cursorX;
        targetY = m_cursorY;
    }

    // Pascal SetAminationPosition(AttAreaType, step=MoveDistance, range=AttDistance)
    SetAttackArea(magic.getAttAreaType(), targetX, targetY, range, attackerX, attackerY, step);

    SoundManager::getInstance().PlaySound(magic.getSoundNum());
    PlayActionAmination(roleIdx, magic.getMagicType(), targetX, targetY);
    PlayMagicAmination(roleIdx, magicId, level, targetX, targetY);
    CalHurtRole(roleIdx, magicId, level);

    int twice = (rData.getAttTwice() == 1) ? 2 : 1;
    int battleMode = GameManager::getInstance().getBattleMode();
    int needProg = ((magic.getNeedProgress() * 10 * level) + 100) * 3 - 1;
    if (((attacker.getProgress() + 1) / 3 < needProg / twice)
        && rData.getAngry() == 100 && battleMode > 1) {
        int needFull = needProg;
        rData.setAngry(100 - (((needFull - attacker.getProgress()) * 100) / std::max(1, needFull)) / twice);
        attacker.setProgress(0);
    } else {
        attacker.setProgress(std::max(0, attacker.getProgress() - needProg / twice));
    }
    attacker.setActed(1);
}

void BattleManager::AttackAt(int roleIdx, int targetX, int targetY, int magicId) {
    if (roleIdx < 0 || roleIdx >= m_battleRoles.size()) return;
    if (magicId < 0) return;
    BattleRole& attacker = m_battleRoles[roleIdx];
    int attackerX = attacker.getX();
    int attackerY = attacker.getY();
    int rnum = attacker.getRNum();
    int level = GetMagicBattleLevel(rnum, magicId);
    Magic& magic = GameManager::getInstance().getMagic(magicId);
    Role& rData = GameManager::getInstance().getRole(rnum);

    // MP is consumed inside CalHurtRole (Pascal AttackAction → CalHurtRole)
    int step = magic.getMoveDistance(level - 1);
    if (CheckEquipSet(rData.getEquip(0), rData.getEquip(1), rData.getEquip(2), rData.getEquip(3)) == 1) step += 1;
    if (GameManager::getInstance().CheckBattleEffect(rnum, 22)) step += 1;
    if (step < 0) step = 0;
    int range = magic.getAttDistance(level - 1);
    if (range < 0) range = 0;

    SetAttackArea(magic.getAttAreaType(), targetX, targetY, range, attackerX, attackerY, step);
    SoundManager::getInstance().PlaySound(magic.getSoundNum());
    PlayActionAmination(roleIdx, magic.getMagicType(), targetX, targetY);
    PlayMagicAmination(roleIdx, magicId, level, targetX, targetY);
    CalHurtRole(roleIdx, magicId, level);

    int twice = (rData.getAttTwice() == 1) ? 2 : 1;
    int battleMode = GameManager::getInstance().getBattleMode();
    int needProg = ((magic.getNeedProgress() * 10 * level) + 100) * 3 - 1;
    if (((attacker.getProgress() + 1) / 3 < needProg / twice)
        && rData.getAngry() == 100 && battleMode > 1) {
        int needFull = needProg;
        rData.setAngry(100 - (((needFull - attacker.getProgress()) * 100) / std::max(1, needFull)) / twice);
        attacker.setProgress(0);
    } else {
        attacker.setProgress(std::max(0, attacker.getProgress() - needProg / twice));
    }
    attacker.setActed(1);
}

void BattleManager::ShowBMenu(int menuStatus, int menu, int max) {
    // Pascal ShowBMenu: box(100, 28, 47, ...) text at 83.
    // Pascal DrawText blits every glyph at x_pos+10, so visual Chinese starts ~103.
    const char* items[] = { "移動", "武學", "用毒", "解毒", "醫療", "解穴", "聚氣", "物品", "等待", "狀態", "休息", "自動" };
    int menuX = 100;
    int menuY = 50 - 22;
    uint32_t frameColor = PaletteToRgba(GraphicsUtils::getPaletteColor(0xFF));
    uint32_t selectedColor = PaletteToRgba(GraphicsUtils::getPaletteColor(0x64));
    uint32_t selectedShadow = PaletteToRgba(GraphicsUtils::getPaletteColor(0x66));
    uint32_t normalColor = PaletteToRgba(GraphicsUtils::getPaletteColor(0x21));
    uint32_t normalShadow = PaletteToRgba(GraphicsUtils::getPaletteColor(0x23));
    int alpha = 30 * 255 / 100;
    UIManager::getInstance().DrawRectangle(menuX, menuY, 47, max * 22 + 28, 0x00000000, frameColor, alpha);
    int p = 0;
    for (int i = 0; i < 12; ++i) {
        if ((menuStatus & (1 << i)) == 0) continue;
        uint32_t color = (p == menu) ? selectedColor : normalColor;
        uint32_t shadow = (p == menu) ? selectedShadow : normalShadow;
        UIManager::getInstance().DrawShadowTextUtf8(items[i], 103, 53 - 22 + 22 * p, color, shadow, 20);
        p++;
    }
}

int BattleManager::BattleMenu(int roleIdx) {
    SDL_Event event;
    // Drop residual KEY_UP from previous confirmations (e.g. after moving).
    InputManager::getInstance().FlushEvents();
    int menu = 0;
    int rnum = m_battleRoles[roleIdx].getRNum();
    Role& rData = GameManager::getInstance().getRole(rnum);
    int menuStatus = 0xF80;
    int max = 4;
    if (m_battleRoles[roleIdx].getStep() > 0) {
        menuStatus |= 1;
        max += 1;
    }
    if (rData.getPhyPower() >= 10) {
        int canMagic = 0;
        int battleMode = GameManager::getInstance().getBattleMode();
        int progress = m_battleRoles[roleIdx].getProgress();
        int angry = rData.getAngry();
        for (int i = 0; i < 10; ++i) {
            int id = rData.getMagic(i);
            if (id > 0) {
                Magic& magic = GameManager::getInstance().getMagic(id);
                if (magic.getNeedMP() <= rData.getCurrentMP()) {
                    int lv = rData.getMagLevel(i) / 100;
                    if (lv < 1) lv = 1;
                    int needProgress = magic.getNeedProgress() * 10 * lv + 100;
                    if (battleMode == 0 || angry == 100 || ((progress + 1) / 3) >= needProgress) {
                        canMagic = 1;
                        break;
                    }
                }
            }
        }
        if (canMagic > 0) {
            menuStatus |= 2;
            max += 1;
        }
    }
    if (GetRoleUsePoi(rnum, true) > 0 && rData.getPhyPower() >= 30) {
        menuStatus |= 4;
        max += 1;
    }
    if (GetRoleMedPoi(rnum, true) > 0 && rData.getPhyPower() >= 50) {
        menuStatus |= 8;
        max += 1;
    }
    if (GetRoleMedcine(rnum, true) > 0 && rData.getPhyPower() >= 50) {
        menuStatus |= 16;
        max += 1;
    }
    if ((rData.getCurrentMP() + GetRoleMedcine(rnum, true) * 5) > 200 && rData.getPhyPower() >= 50) {
        menuStatus |= 32;
        max += 1;
    }
    int battleMode = GameManager::getInstance().getBattleMode();
    if (battleMode > 0) {
        menuStatus |= 64;
        max += 1;
    }
    
    auto mapMenuToIndex = [&](int menuIndex) -> int {
        int p = 0;
        for (int i = 0; i < 12; ++i) {
            if ((menuStatus & (1 << i)) > 0) {
                p += 1;
                if (p > menuIndex) return i;
            }
        }
        return -1;
    };

    auto redrawMenu = [&]() {
        RenderBattle();
        int roundNum = m_battleRoles[roleIdx].getRound();
        if (roundNum < 1) roundNum = 1;
        // Pascal: DrawRectangle(10,50,80,28), text at 10-17 with DrawText(+10) => visual ~13
        std::string roundStr = "第" + std::to_string(roundNum) + "回";
        uint32_t frameColor = PaletteToRgba(GraphicsUtils::getPaletteColor(0xFF));
        uint32_t textColor = PaletteToRgba(GraphicsUtils::getPaletteColor(5));
        uint32_t shadowColor = PaletteToRgba(GraphicsUtils::getPaletteColor(7));
        UIManager::getInstance().DrawRectangle(10, 50, 80, 28, 0x000000, frameColor, 30);
        UIManager::getInstance().DrawShadowTextUtf8(roundStr, 13, 52, textColor, shadowColor, 20);
        UIManager::getInstance().ShowSimpleStatus(rnum, 30, 330, m_battleRoles[roleIdx].getFrozen());
        ShowBMenu(menuStatus, menu, max);
        UIManager::getInstance().UpdateScreen();
    };
    
    auto pickMenuByMouse = [&](int& outMenu) -> bool {
        float xm = 0.0f;
        float ym = 0.0f;
        SDL_GetMouseState(&xm, &ym);
        // Pascal mouse hit: xm in [100, 147)
        if (xm >= 100.0f && xm < 147.0f && ym >= (50.0f - 22.0f) && ym < (max * 22.0f + 78.0f - 22.0f)) {
            int next = static_cast<int>((ym - 52.0f + 22.0f) / 22.0f);
            if (next < 0) next = 0;
            if (next > max) next = max;
            outMenu = next;
            return true;
        }
        return false;
    };

    bool selected = false;
    while (!selected) {
        redrawMenu();
        
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                m_battleRunning = false;
                GameManager::getInstance().Quit();
                return -1;
            }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_UP || event.key.key == SDLK_KP_8) {
                    menu -= 1;
                    if (menu < 0) menu = max;
                    redrawMenu();
                }
                if (event.key.key == SDLK_DOWN || event.key.key == SDLK_KP_2) {
                    menu += 1;
                    if (menu > max) menu = 0;
                    redrawMenu();
                }
                if (event.key.key == SDLK_ESCAPE) {
                    return -1;
                }
            }
            if (event.type == SDL_EVENT_KEY_UP) {
                if (event.key.key == SDLK_RETURN || event.key.key == SDLK_KP_ENTER || event.key.key == SDLK_SPACE) {
                    selected = true;
                    break;
                }
            }
            if (event.type == SDL_EVENT_MOUSE_MOTION) {
                int nextMenu = menu;
                if (pickMenuByMouse(nextMenu) && nextMenu != menu) {
                    menu = nextMenu;
                    redrawMenu();
                }
            }
            if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    int nextMenu = menu;
                    if (pickMenuByMouse(nextMenu)) {
                        menu = nextMenu;
                        selected = true;
                        break;
                    }
                }
            }
        }
        SDL_Delay((20 * GameManager::getInstance().getGameSpeed()) / 10);
    }
    int result = mapMenuToIndex(menu);
    return result;
}

bool BattleManager::SelectMove(int roleIdx, int& outX, int& outY) {
    bool done = false;
    SDL_Event event;
    BattleRole& actor = m_battleRoles[roleIdx];
    m_cursorX = actor.getX();
    m_cursorY = actor.getY();
    m_showMoveRange = true;
    m_showAttackRange = false;
    
    CalSelectableArea(roleIdx);
    InputManager::getInstance().FlushEvents();
    
    while (!done) {
        DrawBFieldWithCursor(0, actor.getStep(), 0);
        RenderBattle();
        UIManager::getInstance().DrawShadowTextUtf8("请选择移动位置", 10, 10, 0xFFFFFF, 0x000000);
        UIManager::getInstance().UpdateScreen();
        
        while(SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                m_battleRunning = false;
                GameManager::getInstance().Quit();
                m_showMoveRange = false;
                return false;
            }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                int nx = m_cursorX;
                int ny = m_cursorY;
                if (event.key.key == SDLK_UP || event.key.key == SDLK_KP_8) { MoveCursorBy(nx, ny, -1, 0); }
                if (event.key.key == SDLK_DOWN || event.key.key == SDLK_KP_2) { MoveCursorBy(nx, ny, 1, 0); }
                if (event.key.key == SDLK_LEFT || event.key.key == SDLK_KP_4) { MoveCursorBy(nx, ny, 0, -1); }
                if (event.key.key == SDLK_RIGHT || event.key.key == SDLK_KP_6) { MoveCursorBy(nx, ny, 0, 1); }
                if (m_battleField[3][nx][ny] >= 0) {
                    m_cursorX = nx;
                    m_cursorY = ny;
                }
            }
            if (event.type == SDL_EVENT_KEY_UP) {
                // Align with Pascal SelectAim: confirm on KEY_UP so BattleMenu won't swallow the same press.
                if (event.key.key == SDLK_RETURN || event.key.key == SDLK_SPACE || event.key.key == SDLK_KP_ENTER) {
                    if (m_battleField[3][m_cursorX][m_cursorY] >= 0) {
                        outX = m_cursorX;
                        outY = m_cursorY;
                        m_showMoveRange = false;
                        return true;
                    }
                }
                if (event.key.key == SDLK_ESCAPE) {
                    m_showMoveRange = false;
                    return false;
                }
            }
        }
        SDL_Delay(10);
    }
    m_showMoveRange = false;
    return false;
}

bool BattleManager::SelectAttack(int roleIdx, int& outTargetIdx) {
    bool done = false;
    SDL_Event event;
    BattleRole& actor = m_battleRoles[roleIdx];
    m_cursorX = actor.getX();
    m_cursorY = actor.getY();
    m_showAttackRange = true;
    m_showMoveRange = true;
    
    int range = 1; 
    
    while (!done) {
        DrawBFieldWithCursor(0, range, range);
        RenderBattle();
        UIManager::getInstance().DrawShadowTextUtf8("请选择攻击目标", 10, 10, 0xFFFFFF, 0x000000);
        UIManager::getInstance().UpdateScreen();
        
        while(SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                m_battleRunning = false;
                GameManager::getInstance().Quit();
                m_showAttackRange = false;
                m_showMoveRange = false;
                return false;
            }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                int nx = m_cursorX;
                int ny = m_cursorY;
                if (event.key.key == SDLK_UP) { MoveCursorBy(nx, ny, -1, 0); }
                if (event.key.key == SDLK_DOWN) { MoveCursorBy(nx, ny, 1, 0); }
                if (event.key.key == SDLK_LEFT) { MoveCursorBy(nx, ny, 0, -1); }
                if (event.key.key == SDLK_RIGHT) { MoveCursorBy(nx, ny, 0, 1); }
                if (m_battleField[4][nx][ny] > 0) {
                    m_cursorX = nx;
                    m_cursorY = ny;
                }
                
                if (event.key.key == SDLK_RETURN || event.key.key == SDLK_SPACE) {
                    if (m_battleField[4][m_cursorX][m_cursorY] > 0) {
                         int target = m_battleField[2][m_cursorX][m_cursorY];
                        if (target >= 0 && target != roleIdx) {
                            outTargetIdx = target;
                            m_showAttackRange = false;
                            m_showMoveRange = false;
                            return true;
                        }
                    }
                }
                if (event.key.key == SDLK_ESCAPE) {
                    m_showAttackRange = false;
                    m_showMoveRange = false;
                    return false;
                }
            }
        }
        SDL_Delay(10);
    }
    m_showAttackRange = false;
    return false;
}

bool BattleManager::SelectMagic(int roleIdx, int& outMagicId) {
    if (roleIdx < 0 || roleIdx >= (int)m_battleRoles.size()) return false;
    SDL_Event event;
    BattleRole& actor = m_battleRoles[roleIdx];
    int rnum = actor.getRNum();
    Role& rData = GameManager::getInstance().getRole(rnum);
    int battleMode = GameManager::getInstance().getBattleMode();
    int progress = actor.getProgress();
    int angry = rData.getAngry();

    // Slot bitmask aligned with Pascal SelectMagic / ShowMagicMenu.
    int menuStatus = 0;
    int entryCount = 0;
    for (int i = 0; i < 10; ++i) {
        int mnum = rData.getMagic(i);
        if (mnum <= 0) continue;
        Magic& magic = GameManager::getInstance().getMagic(mnum);
        int lv = rData.getMagLevel(i) / 100;
        if (magic.getMagicType() == 5) {
            menuStatus |= (1 << i);
            entryCount += 1;
            continue;
        }
        if (magic.getNeedMP() > rData.getCurrentMP()) continue;
        int needProgress = magic.getNeedProgress() * 10 * lv + 100;
        if (battleMode == 0 || angry == 100 || ((progress + 1) / 3) >= needProgress) {
            menuStatus |= (1 << i);
            entryCount += 1;
        }
    }
    if (entryCount <= 0) return false;
    int max = entryCount - 1;
    int menu = 0;

    auto mapMenuToSlot = [&](int menuIndex) -> int {
        int p = 0;
        for (int i = 0; i < 10; ++i) {
            if ((menuStatus & (1 << i)) == 0) continue;
            if (p == menuIndex) return i;
            p += 1;
        }
        return -1;
    };

    auto redrawMagicMenu = [&]() {
        RenderBattle();
        // Pascal ShowMagicMenu: DrawRectangle(100, 50, 167, max*22+28)
        // DrawText blits at x+10, so names/levels are drawn at visual 103 / 233.
        uint32_t frameColor = PaletteToRgba(GraphicsUtils::getPaletteColor(0xFF));
        uint32_t sel1 = PaletteToRgba(GraphicsUtils::getPaletteColor(0x64));
        uint32_t sel2 = PaletteToRgba(GraphicsUtils::getPaletteColor(0x66));
        uint32_t nor1 = PaletteToRgba(GraphicsUtils::getPaletteColor(0x21));
        uint32_t nor2 = PaletteToRgba(GraphicsUtils::getPaletteColor(0x23));
        uint32_t gong1 = PaletteToRgba(GraphicsUtils::getPaletteColor(0x05));
        uint32_t gong2 = PaletteToRgba(GraphicsUtils::getPaletteColor(0x07));
        int alpha = 30 * 255 / 100;
        UIManager::getInstance().DrawRectangle(100, 50, 167, max * 22 + 28, 0x00000000, frameColor, alpha);
        int p = 0;
        for (int i = 0; i < 10; ++i) {
            if ((menuStatus & (1 << i)) == 0) continue;
            int mnum = rData.getMagic(i);
            Magic& magic = GameManager::getInstance().getMagic(mnum);
            std::string nameUtf8 = TextManager::getInstance().gbkToUtf8(magic.getName());
            int lv = rData.getMagLevel(i) / 100 + 1;
            bool selected = (p == menu);
            bool isGongti = (magic.getMagicType() == 5);
            uint32_t c1 = selected ? sel1 : (isGongti ? gong1 : nor1);
            uint32_t c2 = selected ? sel2 : (isGongti ? gong2 : nor2);
            UIManager::getInstance().DrawShadowTextUtf8(nameUtf8, 103, 53 + 22 * p, c1, c2, 20);
            if (!isGongti) {
                std::ostringstream lvSs;
                lvSs << std::setw(3) << lv;
                UIManager::getInstance().DrawEngShadowText(lvSs.str(), 233, 53 + 22 * p, c1, c2, 20);
            }
            p += 1;
        }
        UIManager::getInstance().UpdateScreen();
    };

    InputManager::getInstance().FlushEvents();
    while (true) {
        redrawMagicMenu();
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                m_battleRunning = false;
                GameManager::getInstance().Quit();
                return false;
            }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_UP || event.key.key == SDLK_KP_8) {
                    menu -= 1;
                    if (menu < 0) menu = max;
                }
                if (event.key.key == SDLK_DOWN || event.key.key == SDLK_KP_2) {
                    menu += 1;
                    if (menu > max) menu = 0;
                }
            }
            if (event.type == SDL_EVENT_KEY_UP) {
                if (event.key.key == SDLK_RETURN || event.key.key == SDLK_SPACE || event.key.key == SDLK_KP_ENTER) {
                    int slot = mapMenuToSlot(menu);
                    if (slot >= 0) {
                        outMagicId = rData.getMagic(slot);
                        return true;
                    }
                }
                if (event.key.key == SDLK_ESCAPE) return false;
            }
            if (event.type == SDL_EVENT_MOUSE_MOTION || event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
                float xm = 0.0f, ym = 0.0f;
                SDL_GetMouseState(&xm, &ym);
                if (xm >= 100.0f && xm < 267.0f && ym >= 50.0f && ym < (max * 22.0f + 78.0f)) {
                    int next = static_cast<int>((ym - 52.0f) / 22.0f);
                    if (next < 0) next = 0;
                    if (next > max) next = max;
                    menu = next;
                    if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_LEFT) {
                        int slot = mapMenuToSlot(menu);
                        if (slot >= 0) {
                            outMagicId = rData.getMagic(slot);
                            return true;
                        }
                    }
                    if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_RIGHT) {
                        return false;
                    }
                }
            }
        }
        SDL_Delay((20 * GameManager::getInstance().getGameSpeed()) / 10);
    }
}

bool BattleManager::SelectMagicTarget(int roleIdx, int magicId, int& outX, int& outY) {
    if (roleIdx < 0 || roleIdx >= m_battleRoles.size()) return false;
    if (magicId < 0) return false;
    SDL_Event event;
    BattleRole& actor = m_battleRoles[roleIdx];
    m_cursorX = actor.getX();
    m_cursorY = actor.getY();
    m_showAttackRange = true;
    m_showMoveRange = true;
    int level = GetMagicBattleLevel(actor.getRNum(), magicId);
    Magic& magic = GameManager::getInstance().getMagic(magicId);
    int moveRange = magic.getMoveDistance(level - 1);
    Role& rData = GameManager::getInstance().getRole(actor.getRNum());
    if (CheckEquipSet(rData.getEquip(0), rData.getEquip(1), rData.getEquip(2), rData.getEquip(3)) == 1) moveRange += 1;
    if (GameManager::getInstance().CheckBattleEffect(actor.getRNum(), 22)) moveRange += 1;
    if (moveRange < 0) moveRange = 0;
    int attackRange = magic.getAttDistance(level - 1);
    if (attackRange < 0) attackRange = 0;
    int minStep = 0;
    if (magic.getAttAreaType() == 6) {
        minStep = magic.getMinStep();
        if (minStep < 0) minStep = 0;
    }
    for (int x = 0; x < 64; ++x) {
        for (int y = 0; y < 64; ++y) {
            m_battleField[3][x][y] = -1;
            if (std::abs(x - actor.getX()) + std::abs(y - actor.getY()) <= moveRange) {
                m_battleField[3][x][y] = 0;
            }
        }
    }
    while (true) {
        // Pascal DrawBFieldWithCursor(AttAreaType, step=MoveDistance, range=AttDistance)
        DrawBFieldWithCursor(magic.getAttAreaType(), moveRange, attackRange);
        RenderBattle();
        UIManager::getInstance().DrawShadowTextUtf8("请选择目标", 10, 10, 0xFFFFFF, 0x000000);
        UIManager::getInstance().UpdateScreen();
        while(SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                m_battleRunning = false;
                GameManager::getInstance().Quit();
                m_showAttackRange = false;
                m_showMoveRange = false;
                return false;
            }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                int nx = m_cursorX;
                int ny = m_cursorY;
                if (event.key.key == SDLK_UP) { MoveCursorBy(nx, ny, -1, 0); }
                if (event.key.key == SDLK_DOWN) { MoveCursorBy(nx, ny, 1, 0); }
                if (event.key.key == SDLK_LEFT) { MoveCursorBy(nx, ny, 0, -1); }
                if (event.key.key == SDLK_RIGHT) { MoveCursorBy(nx, ny, 0, 1); }
                int dist = std::abs(nx - actor.getX()) + std::abs(ny - actor.getY());
                if (dist <= moveRange && dist > minStep && m_battleField[3][nx][ny] >= 0) {
                    m_cursorX = nx;
                    m_cursorY = ny;
                }
                if (event.key.key == SDLK_RETURN || event.key.key == SDLK_SPACE) {
                    int dist = std::abs(m_cursorX - actor.getX()) + std::abs(m_cursorY - actor.getY());
                    if (dist > minStep && m_battleField[4][m_cursorX][m_cursorY] > 0) {
                        outX = m_cursorX;
                        outY = m_cursorY;
                        m_showAttackRange = false;
                        m_showMoveRange = false;
                        return true;
                    }
                }
                if (event.key.key == SDLK_ESCAPE) {
                    m_showAttackRange = false;
                    m_showMoveRange = false;
                    return false;
                }
            }
        }
        SDL_Delay(10);
    }
    m_showAttackRange = false;
    m_showMoveRange = false;
    return false;
}

bool BattleManager::SelectFriendlyRole(int roleIdx) {
    if (roleIdx < 0 || roleIdx >= m_battleRoles.size()) return false;
    SDL_Event event;
    BattleRole& actor = m_battleRoles[roleIdx];
    m_cursorX = actor.getX();
    m_cursorY = actor.getY();
    m_showAttackRange = true;
    m_showMoveRange = false;
    
    for (int x = 0; x < 64; ++x) {
        for (int y = 0; y < 64; ++y) {
            m_battleField[4][x][y] = 0;
        }
    }
    for (int i = 0; i < m_battleRoles.size(); ++i) {
        if (m_battleRoles[i].getDead()) continue;
        if (m_battleRoles[i].getTeam() != actor.getTeam()) continue;
        int x = m_battleRoles[i].getX();
        int y = m_battleRoles[i].getY();
        if (x >= 0 && x < 64 && y >= 0 && y < 64) {
            m_battleField[4][x][y] = 1;
        }
    }
    
    while (true) {
        RenderBattle();
        UIManager::getInstance().DrawShadowTextUtf8("请选择目标", 10, 10, 0xFFFFFF, 0x000000);
        UIManager::getInstance().UpdateScreen();
        
        while(SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                m_battleRunning = false;
                GameManager::getInstance().Quit();
                m_showAttackRange = false;
                return false;
            }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_UP) { MoveCursorBy(m_cursorX, m_cursorY, -1, 0); }
                if (event.key.key == SDLK_DOWN) { MoveCursorBy(m_cursorX, m_cursorY, 1, 0); }
                if (event.key.key == SDLK_LEFT) { MoveCursorBy(m_cursorX, m_cursorY, 0, -1); }
                if (event.key.key == SDLK_RIGHT) { MoveCursorBy(m_cursorX, m_cursorY, 0, 1); }
                
                if (event.key.key == SDLK_RETURN || event.key.key == SDLK_SPACE) {
                    if (m_battleField[4][m_cursorX][m_cursorY] > 0) {
                        int target = m_battleField[2][m_cursorX][m_cursorY];
                        if (target >= 0 && target < m_battleRoles.size()) {
                            if (!m_battleRoles[target].getDead() && m_battleRoles[target].getTeam() == actor.getTeam()) {
                                m_showAttackRange = false;
                                return true;
                            }
                        }
                    }
                }
                if (event.key.key == SDLK_ESCAPE) {
                    m_showAttackRange = false;
                    return false;
                }
            }
        }
        SDL_Delay(10);
    }
    m_showAttackRange = false;
    return false;
}

void BattleManager::DrawBFieldWithCursor(int attAreaType, int step, int range) {
    int ax = m_cursorX;
    int ay = m_cursorY;
    int bx = ax;
    int by = ay;
    if (m_currentRoleIndex >= 0 && m_currentRoleIndex < m_battleRoles.size()) {
        bx = m_battleRoles[m_currentRoleIndex].getX();
        by = m_battleRoles[m_currentRoleIndex].getY();
    }
    SetAttackArea(attAreaType, ax, ay, range, bx, by, step);
}

bool BattleManager::SelectAim(int roleIdx, int step) {
    bool done = false;
    SDL_Event event;
    BattleRole& actor = m_battleRoles[roleIdx];
    m_cursorX = actor.getX();
    m_cursorY = actor.getY();
    m_showAttackRange = false;
    m_showMoveRange = false;
    for (int x = 0; x < 64; ++x) {
        for (int y = 0; y < 64; ++y) {
            m_battleField[3][x][y] = -1;
            if (std::abs(x - actor.getX()) + std::abs(y - actor.getY()) <= step) {
                m_battleField[3][x][y] = 0;
            }
        }
    }
    for (int x = 0; x < 64; ++x) {
        for (int y = 0; y < 64; ++y) {
            m_battleField[4][x][y] = 0;
        }
    }
    m_showAttackRange = true;
    
    while (!done) {
        for (int x = 0; x < 64; ++x) {
            for (int y = 0; y < 64; ++y) {
                m_battleField[4][x][y] = 0;
            }
        }
        m_battleField[4][m_cursorX][m_cursorY] = 1;
        RenderBattle();
        int aimRole = m_battleField[2][m_cursorX][m_cursorY];
        if (aimRole >= 0 && aimRole < (int)m_battleRoles.size() && !m_battleRoles[aimRole].getDead()) {
            int aimRnum = m_battleRoles[aimRole].getRNum();
            if (aimRnum >= 0) {
                UIManager::getInstance().ShowSimpleStatus(aimRnum, 330, 330, m_battleRoles[aimRole].getFrozen());
            }
        }
        UIManager::getInstance().UpdateScreen();
        
        while(SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                m_battleRunning = false;
                GameManager::getInstance().Quit();
                m_showAttackRange = false;
                return false;
            }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                int nx = m_cursorX;
                int ny = m_cursorY;
                if (event.key.key == SDLK_UP || event.key.key == SDLK_KP_8) { MoveCursorBy(nx, ny, -1, 0); }
                if (event.key.key == SDLK_DOWN || event.key.key == SDLK_KP_2) { MoveCursorBy(nx, ny, 1, 0); }
                if (event.key.key == SDLK_LEFT || event.key.key == SDLK_KP_4) { MoveCursorBy(nx, ny, 0, -1); }
                if (event.key.key == SDLK_RIGHT || event.key.key == SDLK_KP_6) { MoveCursorBy(nx, ny, 0, 1); }
                int dist = std::abs(nx - actor.getX()) + std::abs(ny - actor.getY());
                if (dist <= step && m_battleField[3][nx][ny] >= 0) {
                    m_cursorX = nx;
                    m_cursorY = ny;
                }
            }
            if (event.type == SDL_EVENT_KEY_UP) {
                if (event.key.key == SDLK_RETURN || event.key.key == SDLK_SPACE ||
                    event.key.key == SDLK_KP_ENTER) {
                    int dist = std::abs(m_cursorX - actor.getX()) + std::abs(m_cursorY - actor.getY());
                    if (dist <= step && m_battleField[3][m_cursorX][m_cursorY] >= 0) {
                        // Align with Pascal SelectAim: confirm aim tile directly.
                        m_showAttackRange = false;
                        return true;
                    }
                }
                if (event.key.key == SDLK_ESCAPE) {
                    m_showAttackRange = false;
                    return false;
                }
            }
        }
        SDL_Delay((20 * GameManager::getInstance().getGameSpeed()) / 10);
    }
    m_showAttackRange = false;
    return false;
}

void BattleManager::RenderBattle() {
    SDL_Renderer* renderer = GameManager::getInstance().getRenderer();
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND); // Enable Blending
    SDL_Surface* screen = GameManager::getInstance().getScreenSurface();
    if (screen) {
        SDL_FillSurfaceRect(screen, NULL, 0x000000);
    }
    
    // Draw Map (Simple Isometric Loop)
    int cx = 32, cy = 32;
    GetViewCenter(cx, cy);
    
    // Render tiles
    // Using SceneManager::DrawTile to reuse smp/sdx resources
    // KYS Battle Map: 64x64
    // Layer 0: Ground
    // Layer 1: Object (Building/Tree)
    
    for (int sum = 0; sum <= 126; ++sum) {
        for (int i1 = 0; i1 < 64; ++i1) {
            int i2 = sum - i1;
            if (i2 < 0 || i2 >= 64) continue;
            int x, y;
            SceneManager::getInstance().GetPositionOnScreen(i1, i2, cx, cy, x, y);
            
            if (x < -200 || x > 840 || y < -200 || y > 680) continue; 

            int16_t tile0 = m_battleField[0][i1][i2];
            if (tile0 > 0) {
                int idx = (tile0 / 2) - 1;
                if (idx >= 0) {
                    SceneManager::getInstance().DrawTile(renderer, idx, x, y, 0, 0);
                    int overlayShadow = 0;
                    if (m_showMoveRange || m_showAttackRange) {
                        if (i1 == m_cursorX && i2 == m_cursorY) overlayShadow = 4;
                        else if (m_showAttackRange && m_battleField[4][i1][i2] > 0) overlayShadow = 3;
                        else if (m_showAttackRange && m_battleField[3][i1][i2] >= 0) overlayShadow = 3;
                        else if (m_showMoveRange && m_battleField[3][i1][i2] >= 0) overlayShadow = 1;
                    }
                    if (overlayShadow != 0) {
                        SceneManager::getInstance().DrawTileShadow(renderer, idx, x, y, 0, 0, overlayShadow);
                    }
                }
            }
            
            int16_t tile1 = m_battleField[1][i1][i2];
            if (tile1 > 0) {
                int idx1 = (tile1 / 2) - 1;
                if (idx1 >= 0) {
                    SceneManager::getInstance().DrawTile(renderer, idx1, x, y, 0, 0);
                }
            }
            
            int rIdx = m_battleField[2][i1][i2];
            if (rIdx < 0) rIdx = m_battleField[5][i1][i2];
            if (rIdx >= 0 && rIdx < (int)m_battleRoles.size()) {
                BattleRole& r = m_battleRoles[rIdx];
                if (r.getShow() == 0 && r.getRNum() >= 0 && !r.getDead()) {
                    int headNum = -1;
                    int poison = 0;
                    int hurt = 0;
                    if (r.getPic() >= 0) headNum = r.getPic();
                    Role& roleData = GameManager::getInstance().getRole(r.getRNum());
                    if (headNum < 0) headNum = roleData.getHeadNum();
                    poison = roleData.getPoision();
                    hurt = roleData.getHurt();
                    int face = r.getFace();
                    StatusPulse pulse = ComputeStatusPulse(poison, hurt, r.getFrozen());
                    int flashWhite = 0;
                    if (r.getFlashTimer() > 0) {
                        flashWhite = 255;
                    }
                    if (headNum >= 0) {
                        if (rIdx != m_actionAnimRoleIndex) {
                            int spriteIdx = BEGIN_BATTLE_ROLE_PIC + headNum * 4 + face;
                            SceneManager::getInstance().DrawWarTile(renderer, spriteIdx, x, y, 0, 0, pulse.green, pulse.red, pulse.gray, flashWhite);
                        }
                    } else {
                        SceneManager::getInstance().DrawWarTile(renderer, BEGIN_BATTLE_ROLE_PIC + (r.getTeam() * 5), x, y, 0, 0, pulse.green, pulse.red, pulse.gray, flashWhite);
                    }
                }
            }
        }
    }

    GameManager::getInstance().RenderScreenTo(renderer);
    ShowProgress();
}

void BattleManager::PlayActionAmination(int bnum, int mode, int targetX, int targetY) {
    if (bnum < 0 || bnum >= m_battleRoles.size()) return;
    BattleRole& r = m_battleRoles[bnum];
    int headNum = r.getPic();
    if (headNum < 0 && r.getRNum() >= 0) {
        headNum = GameManager::getInstance().getRole(r.getRNum()).getHeadNum();
    }
    if (headNum < 0) return;
    int dx = targetX - r.getX();
    int dy = targetY - r.getY();
    int dm = std::abs(dx) - std::abs(dy);
    int face = r.getFace();
    if (dm > 0) {
        face = (dx < 0) ? 0 : 3;
    } else if (dm < 0) {
        face = (dy < 0) ? 2 : 1;
    }
    r.setFace(face);
    if (mode < 0) mode = 0;
    std::cout << "[Action] role=" << bnum << " rnum=" << r.getRNum() << " head=" << headNum
              << " mode=" << mode << " face=" << face
              << " target=(" << targetX << "," << targetY << ")" << std::endl;
    SDL_Renderer* renderer = GameManager::getInstance().getRenderer();
    if (!renderer) {
        std::cerr << "[Action] renderer missing" << std::endl;
        return;
    }
    int cx = 0, cy = 0;
    GetViewCenter(cx, cy);
    int drawX, drawY;
    SceneManager::getInstance().GetPositionOnScreen(r.getX(), r.getY(), cx, cy, drawX, drawY);
    m_actionAnimRoleIndex = bnum;
    auto playFromMode = [&](int modeValue) -> bool {
        std::string relative = "fight/" + PadNumber(headNum, 3) + "/" + PadNumber(modeValue, 2) + ".pic";
        std::string path = ResolveDataPath(relative);
        if (!FileExists(path)) {
            std::cout << "[Action] missing file mode=" << modeValue << " relative=" << relative
                      << " resolved=" << path << std::endl;
            return false;
        }
        int count = PicLoader::getPicCount(path);
        if (count <= 0) {
            std::cout << "[Action] empty pic mode=" << modeValue << " path=" << path << std::endl;
            return true;
        }
        int perFace = count / 4;
        if (perFace <= 0) {
            std::cout << "[Action] perFace=0 mode=" << modeValue << " count=" << count << " path=" << path << std::endl;
            return true;
        }
        std::cout << "[Action] play mode=" << modeValue << " count=" << count << " perFace=" << perFace
                  << " path=" << path << std::endl;
        int beginpic = face * perFace;
        int endpic = beginpic + perFace - 1;
        if (beginpic < 0) beginpic = 0;
        if (endpic >= count) endpic = count - 1;
        for (int i = beginpic; i <= endpic; ++i) {
            SDL_PumpEvents();
            PicImage pic = PicLoader::loadPic(path, i);
            if (!pic.surface) continue;
            RenderBattle();
            BlitPicToScreen(pic, drawX, drawY);
            GameManager::getInstance().RenderScreenTo(renderer);
            UIManager::getInstance().UpdateScreen();
            PicLoader::freePic(pic);
            SDL_Delay(40);
        }
        return true;
    };
    if (!playFromMode(mode)) {
        for (int i = 0; i <= 4; ++i) {
            if (playFromMode(i)) break;
        }
    }
    m_actionAnimRoleIndex = -1;
}

void BattleManager::PlayEffectAmination(int bigami, int amiNum, int targetX, int targetY) {
    if (amiNum < 0) return;
    std::string relative = "eft/eft" + PadNumber(amiNum, 3) + ".pic";
    std::string path = ResolveDataPath(relative);
    if (!FileExists(path)) return;
    int count = PicLoader::getPicCount(path);
    if (count <= 0) return;
    SDL_Renderer* renderer = GameManager::getInstance().getRenderer();
    if (!renderer) return;
    int cx = 0, cy = 0;
    GetViewCenter(cx, cy);
    bool restored = false;
    int originalVal = 0;
    if (bigami == 0 && targetX >= 0 && targetX < 64 && targetY >= 0 && targetY < 64) {
        originalVal = m_battleField[4][targetX][targetY];
        if (originalVal <= 0) {
            m_battleField[4][targetX][targetY] = 1;
            restored = true;
        }
    }
    const int gameSpeed = std::max(1, GameManager::getInstance().getGameSpeed());
    for (int i = 0; i < count; ++i) {
        SDL_PumpEvents();
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT) {
                m_battleRunning = false;
            }
        }
        PicImage pic = PicLoader::loadPic(path, i);
        if (!pic.surface) continue;
        RenderBattle();
        if (bigami == 0) {
            int drawn = 0;
            for (int x = 0; x < 64; ++x) {
                for (int y = 0; y < 64; ++y) {
                    if (m_battleField[4][x][y] <= 0) continue;
                    int drawX, drawY;
                    SceneManager::getInstance().GetPositionOnScreen(x, y, cx, cy, drawX, drawY);
                    if (drawX < -200 || drawX > 840 || drawY < -200 || drawY > 680) continue;
                    BlitPicToScreen(pic, drawX, drawY);
                    drawn++;
                }
            }
            int n = 300 - drawn * 3;
            if (pic.surface->w > 120 || pic.surface->h > 120) n -= 5;
            n /= 10;
            if (n > 0) SDL_Delay((n * gameSpeed) / 10);
        } else {
            if (targetX >= 0 && targetX < 64 && targetY >= 0 && targetY < 64) {
                int drawX, drawY;
                SceneManager::getInstance().GetPositionOnScreen(targetX, targetY, cx, cy, drawX, drawY);
                BlitPicToScreen(pic, drawX, drawY);
            }
            int n = 30 + (pic.black - 1) * 10;
            SDL_Delay(((n + 5) * gameSpeed) / 10);
        }
        GameManager::getInstance().RenderScreenTo(renderer);
        UIManager::getInstance().UpdateScreen();
        PicLoader::freePic(pic);
    }
    if (restored) {
        m_battleField[4][targetX][targetY] = originalVal;
    }
}

void BattleManager::PlayMagicAmination(int bnum, int magicId, int level, int targetX, int targetY) {
    if (bnum < 0 || bnum >= m_battleRoles.size()) return;
    if (magicId < 0) return;
    if (GameHooks::tryPlayMagicEffect(bnum, magicId, level, targetX, targetY)) return;
    Magic& magic = GameManager::getInstance().getMagic(magicId);
    int bigami = magic.getBigAmi();
    int amiNum = magic.getAmiNum();
    if (amiNum < 0) return;
    m_currentRoleIndex = bnum;
    PlayEffectAmination(bigami, amiNum, targetX, targetY);
}

void BattleManager::CalPoiHurtLife(int roleIdx) {
    if (roleIdx < 0 || roleIdx >= (int)m_battleRoles.size()) return;
    for (auto& role : m_battleRoles) {
        role.setShowNumber(-1);
    }

    BattleRole& actor = m_battleRoles[roleIdx];
    int rnum = actor.getRNum();
    if (rnum < 0 || actor.getDead()) return;
    Role& role = GameManager::getInstance().getRole(rnum);

    if (role.getPoision() > 0) {
        int hurt = role.getCurrentHP() * role.getPoision() / 200;
        role.setCurrentHP(std::max(1, role.getCurrentHP() - hurt));
        if (hurt > 0) {
            actor.setShowNumber(hurt);
            ShowHurtValue(2);
        }
    }
    if (role.getHurt() > 0) {
        actor.setShowNumber(role.getHurt());
        ShowHurtValue("內傷", 0x10FFFFFF, 0x14FFFFFF);
    } else if (actor.getFrozen() > 100) {
        actor.setShowNumber(actor.getFrozen());
        ShowHurtValue("封穴", 0x64FFFFFF, 0x66FFFFFF);
    } else if (actor.getAddAtt() > 0) {
        ShowHurtValue("金剛", 0x05FFFFFF, 0x07FFFFFF);
    } else if (actor.getAddSpd() > 0) {
        ShowHurtValue("飛仙", 0x05FFFFFF, 0x07FFFFFF);
    } else if (actor.getAddDef() > 0) {
        ShowHurtValue("忘憂", 0x05FFFFFF, 0x07FFFFFF);
    } else if (actor.getAddStep() > 0) {
        ShowHurtValue("神行", 0x05FFFFFF, 0x07FFFFFF);
    } else if (actor.getPerfectDodge() > 0) {
        ShowHurtValue("迷蹤", 0x05FFFFFF, 0x07FFFFFF);
    } else if (actor.getAddDodge() > 0) {
        ShowHurtValue("閃身", 0x05FFFFFF, 0x07FFFFFF);
    }
}

void BattleManager::ShowHurtValue(const std::string& text, uint32_t color1, uint32_t color2) {
    int cx = 32, cy = 32;
    GetViewCenter(cx, cy);

    for (int t = 0; t < 11; ++t) {
        RenderBattle();
        for (int i = 0; i < (int)m_battleRoles.size(); ++i) {
            BattleRole& r = m_battleRoles[i];
            if (r.getShowNumber() >= 0 && !r.getDead() && r.getRNum() >= 0) {
                int drawX, drawY;
                SceneManager::getInstance().GetPositionOnScreen(r.getX(), r.getY(), cx, cy, drawX, drawY);
                int yOffset = t * 2;
                // Pascal: CENTER_X - 10 / CENTER_Y - 60, text status uses x - 20
                UIManager::getInstance().DrawShadowTextUtf8(text, drawX - 20, drawY - 60 - yOffset, color1, color2);
            }
        }
        UIManager::getInstance().UpdateScreen();
        SDL_Delay(20);
    }

    for (auto& role : m_battleRoles) {
        role.setShowNumber(-1);
    }
    RenderBattle();
    UIManager::getInstance().UpdateScreen();
}

void BattleManager::ShowHurtValue(int mode) {
    uint32_t color1 = 0xFFFFFFFF;
    uint32_t color2 = 0xFF000000;
    int sign = -1;
    
    switch (mode) {
        case 0:
            color1 = 0x10FFFFFF;
            color2 = 0x14FFFFFF;
            sign = -1;
            break;
        case 1:
            color1 = 0x50FFFFFF;
            color2 = 0x53FFFFFF;
            sign = -1;
            break;
        case 2:
            color1 = 0x30FFFFFF;
            color2 = 0x32FFFFFF;
            sign = -1;
            break;
        case 3:
            color1 = 0x05FFFFFF;
            color2 = 0x07FFFFFF;
            sign = 1;
            break;
        case 4:
            color1 = 0x91FFFFFF;
            color2 = 0x93FFFFFF;
            sign = -1;
            break;
    }
    
    int cx = 32, cy = 32;
    GetViewCenter(cx, cy);
    
    for (int t = 0; t < 11; ++t) {
        for (int i = 0; i < m_battleRoles.size(); ++i) {
            BattleRole& r = m_battleRoles[i];
            if (r.getFlashTimer() > 0) {
                r.setFlashTimer(r.getFlashTimer() - 1);
            }
        }
        RenderBattle();
        for (int i = 0; i < m_battleRoles.size(); ++i) {
            BattleRole& r = m_battleRoles[i];
            if (r.getShowNumber() >= 0 && !r.getDead() && r.getRNum() >= 0) {
                int drawX, drawY;
                SceneManager::getInstance().GetPositionOnScreen(r.getX(), r.getY(), cx, cy, drawX, drawY);
                int yOffset = t * 2;
                std::string numStr;
                if (r.getShowNumber() == 0) {
                    numStr = "Miss";
                } else {
                    int val = r.getShowNumber();
                    if (val < 0) {
                        numStr = std::to_string(val);
                    } else {
                        numStr = (sign < 0 ? "-" : "+") + std::to_string(val);
                    }
                }
                // Pascal: x = ... + CENTER_X - 10; y = ... + CENTER_Y - 60
                UIManager::getInstance().DrawEngShadowText(numStr, drawX - 10, drawY - 60 - yOffset, color1, color2);
            }
        }
        UIManager::getInstance().UpdateScreen();
        SDL_Delay(20);
    }
    
    for (int i = 0; i < m_battleRoles.size(); ++i) {
        m_battleRoles[i].setShowNumber(-1);
        m_battleRoles[i].setFlashTimer(0);
    }
    RenderBattle();
    UIManager::getInstance().UpdateScreen();
}

void BattleManager::BattleMenuItem(int roleIdx) {
    if (roleIdx < 0 || roleIdx >= (int)m_battleRoles.size()) return;
    BattleRole& actor = m_battleRoles[roleIdx];
    int itemId = UIManager::getInstance().ShowBattleItemMenu([&]() { RenderBattle(); });
    if (itemId < 0) return;

    Item& item = GameManager::getInstance().getItem(itemId);
    if (item.getItemType() == 4) {
        UseHiddenWeapen(roleIdx, itemId);
        return;
    }

    if (item.getEventNum() > 0) {
        EventManager::getInstance().ExecuteEvent(item.getEventNum());
        return;
    }

    m_cursorX = actor.getX();
    m_cursorY = actor.getY();
    ApplyItemEffect(actor.getRNum(), itemId, 0);
    actor.setActed(1);
    actor.setProgress(actor.getProgress() - 240);
    RenderBattle();
    UIManager::getInstance().UpdateScreen();

    bool waiting = true;
    while (waiting) {
        SDL_Event waitEvent;
        while (SDL_PollEvent(&waitEvent)) {
            if (waitEvent.type == SDL_EVENT_QUIT) {
                m_battleRunning = false;
                GameManager::getInstance().Quit();
                return;
            }
            if (waitEvent.type == SDL_EVENT_KEY_DOWN ||
                waitEvent.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                waiting = false;
            }
        }
        SDL_Delay(10);
    }
}

BattleRole& BattleManager::getBattleRole(int index) {
    if (index < 0 || index >= m_battleRoles.size()) {
        static BattleRole dummy;
        return dummy;
    }
    return m_battleRoles[index];
}

int BattleManager::getBattleRoleCount() const {
    return static_cast<int>(m_battleRoles.size());
}

int BattleManager::getMaxRound() const {
    int mx = 0;
    for (const auto& r : m_battleRoles) {
        if (r.getRound() > mx) mx = r.getRound();
    }
    return mx;
}

int16_t BattleManager::getBattleField(int layer, int x, int y) const {
    if (layer < 0 || layer >= 8 || x < 0 || x >= 64 || y < 0 || y >= 64) return 0;
    return m_battleField[layer][x][y];
}

void BattleManager::setBattleField(int layer, int x, int y, int16_t val) {
    if (layer < 0 || layer >= 8 || x < 0 || x >= 64 || y < 0 || y >= 64) return;
    m_battleField[layer][x][y] = val;
}

void BattleManager::AddBattleRole(const BattleRole& role) {
    m_battleRoles.push_back(role);
}

void BattleManager::ClearBattleRoles() {
    m_battleRoles.clear();
}
