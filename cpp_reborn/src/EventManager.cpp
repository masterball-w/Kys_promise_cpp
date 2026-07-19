#include "EventManager.h"
#include "FileLoader.h"
#include "GameManager.h"
#include "SceneManager.h"
#include "TextManager.h"
#include "GraphicsUtils.h"
#include "BattleManager.h"
#include "UIManager.h"
#include "SoundManager.h"
#include "InputManager.h"
#include <iostream>
#include <cstring>
#include <thread>
#include <chrono>
#include <algorithm>

namespace {
    uint16_t ReadU16LE(const std::string& s, size_t offset) {
        if (offset + 1 >= s.size()) return 0;
        return static_cast<uint16_t>(static_cast<uint8_t>(s[offset])) |
               (static_cast<uint16_t>(static_cast<uint8_t>(s[offset + 1])) << 8);
    }

    std::string TrimAtZero(const std::string& s) {
        size_t pos = s.find('\0');
        if (pos == std::string::npos) return s;
        return s.substr(0, pos);
    }

    bool LooksLikeUninitializedName(const std::string& s) {
        if (s.size() >= 2 && static_cast<uint8_t>(s[0]) == 0xFF && static_cast<uint8_t>(s[1]) == 0xFF) return true;
        return false;
    }

    bool IsDoubleSurname3Chars(uint16_t w0, uint16_t w2) {
        return (w0 == 0x6EAB && w2 == 0x63AE) ||
               (w0 == 0xE8A6 && w2 == 0xF9AA) ||
               (w0 == 0x46AA && w2 == 0xE8A4) ||
               (w0 == 0x4FA5 && w2 == 0xB0AA) ||
               (w0 == 0x7DBC && w2 == 0x65AE) ||
               (w0 == 0x71A5 && w2 == 0xA8B0) ||
               (w0 == 0xD1BD && w2 == 0xAFB8) ||
               (w0 == 0x71A5 && w2 == 0xC5AA) ||
               (w0 == 0xD3A4 && w2 == 0x76A5) ||
               (w0 == 0xBDA4 && w2 == 0x5DAE) ||
               (w0 == 0xDABC && w2 == 0xA7B6) ||
               (w0 == 0x43AD && w2 == 0xDFAB) ||
               (w0 == 0x71A5 && w2 == 0x7BAE) ||
               (w0 == 0xB9A7 && w2 == 0x43C3) ||
               (w0 == 0x61B0 && w2 == 0xD5C1) ||
               (w0 == 0x74A6 && w2 == 0xE5A4) ||
               (w0 == 0xDDA9 && w2 == 0x5BB6);
    }

    std::string ExtractSurnameBytesGbk(const std::string& raw) {
        std::string s = TrimAtZero(raw);
        if (s.empty()) return "";
        if (LooksLikeUninitializedName(s)) return "";

        if (static_cast<uint8_t>(s[0]) < 0x80) {
            return s.substr(0, 1);
        }

        if (s.size() == 4) {
            return s.substr(0, 2);
        }

        if (s.size() == 6) {
            uint16_t w0 = ReadU16LE(s, 0);
            uint16_t w2 = ReadU16LE(s, 2);
            if (IsDoubleSurname3Chars(w0, w2)) return s.substr(0, 4);
            return s.substr(0, 2);
        }

        if (s.size() >= 8) {
            return s.substr(0, 4);
        }

        return (s.size() >= 2) ? s.substr(0, 2) : s;
    }
}

EventManager& EventManager::getInstance() {
    static EventManager instance;
    return instance;
}

void EventManager::Instruct_19(int x, int y) {
    // Teleport within scene
    // Pascal: Sx := y; Sy := x; Cx := Sx; Cy := Sy;
    GameManager::getInstance().setMainMapPosition(y, x);
    Instruct_Redraw();
}

void EventManager::Instruct_40(int dir) {
    GameManager::getInstance().setMainMapFace(dir);
}

EventManager::EventManager() {}

bool EventManager::Init() {
    if (!LoadScripts()) {
        std::cerr << "Failed to load event scripts (kdef)" << std::endl;
        return false;
    }
    if (!LoadDialogues()) {
        std::cerr << "Failed to load dialogues (talk)" << std::endl;
        return false;
    }
    return true;
}

bool EventManager::LoadScripts() {
    auto idxData = FileLoader::loadFile("kdef.idx");
    if (idxData.empty()) {
        // Try capitalized if lowercase failed (though Windows is case-insensitive, simple FileLoader might be picky if cached)
        idxData = FileLoader::loadFile("Kdef.idx");
        if (idxData.empty()) return false;
    }

    if (idxData.size() % 4 != 0) {
        std::cerr << "kdef.idx size is not multiple of 4" << std::endl;
        return false;
    }

    size_t count = idxData.size() / 4;
    m_eventIndices.resize(count);
    std::memcpy(m_eventIndices.data(), idxData.data(), idxData.size());
    
    std::cout << "[EventManager] Loaded kdef.idx. Size: " << idxData.size() << " bytes. Script Count: " << count << std::endl;

    auto grpData = FileLoader::loadFile("kdef.grp");
    if (grpData.empty()) {
        grpData = FileLoader::loadFile("Kdef.grp");
        if (grpData.empty()) return false;
    }

    if (grpData.size() % 2 != 0) {
        std::cerr << "kdef.grp size is not multiple of 2" << std::endl;
        return false;
    }

    m_eventScripts.resize(grpData.size() / 2);
    std::memcpy(m_eventScripts.data(), grpData.data(), grpData.size());

    std::cout << "Loaded " << count << " event scripts." << std::endl;
    if (count >= 101) {
        std::cout << "Event 101 offset: " << m_eventIndices[100] << std::endl;
    } else {
        std::cerr << "Event 101 not found (count too low)" << std::endl;
    }

    return true;
}

bool EventManager::LoadDialogues() {
    auto idxData = FileLoader::loadFile("talk.idx");
    if (idxData.empty()) return false;

    size_t count = idxData.size() / 4;
    m_talkIndices.resize(count);
    std::memcpy(m_talkIndices.data(), idxData.data(), idxData.size());

    m_talkData = FileLoader::loadFile("talk.grp");
    if (m_talkData.empty()) {
         std::cerr << "Failed to load talk.grp" << std::endl;
         return false;
    }
    std::cout << "Loaded " << count << " dialogues." << std::endl;

    // Load Name Data (Optional but recommended)
    auto nameIdx = FileLoader::loadFile("name.idx");
    if (!nameIdx.empty()) {
        size_t nc = nameIdx.size() / 4;
        m_nameIndices.resize(nc);
        std::memcpy(m_nameIndices.data(), nameIdx.data(), nameIdx.size());
        m_nameData = FileLoader::loadFile("name.grp");
        if (!m_nameData.empty()) {
             std::cout << "Loaded " << nc << " names." << std::endl;
        }
    }

    return true;
}

std::string EventManager::GetNameFromData(int nameNum) {
    if (nameNum <= 0 || nameNum > m_nameIndices.size()) return "";
    
    int offset = m_nameIndices[nameNum - 1];
    int nextOffset = (nameNum < m_nameIndices.size()) ? m_nameIndices[nameNum] : m_nameData.size();
    
    int len = nextOffset - offset;
    if (len <= 0 || offset + len > m_nameData.size()) return "";
    
    std::string name;
    // Name data is also XOR 0xFF encoded?
    // Pascal: for i := 0 to namelen - 2 do namearray[i] := namearray[i] xor $FF;
    for (int i = 0; i < len; ++i) {
        uint8_t b = m_nameData[offset + i] ^ 0xFF;
        if (b == 0 || b == 0x2A || b < 0x20) break;
        name += (char)b;
    }
    return name;
}

void EventManager::CheckEvent(int sceneId, int x, int y, bool isManual) {
    int16_t eventIndex = SceneManager::getInstance().GetSceneTile(sceneId, 3, x, y);
    
    if (eventIndex >= 0) {
        int16_t scriptId = isManual
            ? SceneManager::getInstance().GetEventData(sceneId, eventIndex, 2)
            : SceneManager::getInstance().GetEventData(sceneId, eventIndex, 4);

        bool shouldTrigger = isManual ? (scriptId >= 0) : (scriptId > 0);
        if (shouldTrigger) {
            m_currentSceneId = sceneId;
            m_currentEventId = eventIndex;
            std::cout << "[CheckEvent] Triggering Event " << eventIndex << " (Script " << scriptId << ") Manual=" << isManual << std::endl;
            ExecuteEvent(scriptId);
        }
    }
}

void EventManager::CheckEventWithItem(int sceneId, int x, int y) {
    int16_t eventIndex = SceneManager::getInstance().GetSceneTile(sceneId, 3, x, y);
    
    if (eventIndex >= 0) {
        int16_t scriptId = SceneManager::getInstance().GetEventData(sceneId, eventIndex, 3);
        
        if (scriptId >= 0) {
            m_currentSceneId = sceneId;
            m_currentEventId = eventIndex;
            std::cout << "[CheckEventWithItem] Triggering Event " << eventIndex << " (Script " << scriptId << ") with Item" << std::endl;
            ExecuteEvent(scriptId);
        }
    }
}

void EventManager::CheckAutoEvents(int sceneId) {
    // Iterate all events to find Auto-Run events (Condition == 0)
    for (int i = 0; i < 200; ++i) {
        int16_t condition = SceneManager::getInstance().GetEventData(sceneId, i, 0);
        int16_t scriptId = SceneManager::getInstance().GetEventData(sceneId, i, 4);
        
        // Condition 0 means Auto-Run
        if (condition == 0 && scriptId > 0) {
            // We found an auto-run event.
            // Update context.
            m_currentSceneId = sceneId;
            m_currentEventId = i;
            
            std::cout << "[CheckAutoEvents] Triggering Auto-Run Event: " << i << " (Script " << scriptId << ")" << std::endl;
            ExecuteEvent(scriptId);
            
            // Usually only one auto-run event happens at a time.
            return; 
        }
    }
}

int16_t EventManager::ReadScriptArg(int& offset) {
    if (offset < 0 || offset >= m_eventScripts.size()) return 0;
    return m_eventScripts[offset++];
}

void EventManager::ExecuteEvent(int eventScriptId) {
    if (eventScriptId <= 0 || eventScriptId > m_eventIndices.size()) {
        std::cerr << "ExecuteEvent: Invalid ID " << eventScriptId << ". Max ID: " << m_eventIndices.size() << std::endl;
        return;
    }

    int offset = m_eventIndices[eventScriptId - 1];
    int nextOffset = (eventScriptId < m_eventIndices.size()) ? m_eventIndices[eventScriptId] : m_eventScripts.size() * 2;
    
    int lengthBytes = nextOffset - offset;
    int lengthWords = lengthBytes / 2;
    
    int scriptStart = offset / 2;
    int scriptEnd = scriptStart + lengthWords;
    
    // Check range validity
    if (scriptStart >= m_eventScripts.size() || scriptEnd > m_eventScripts.size()) {
        std::cerr << "Event " << eventScriptId << " out of bounds! Start: " << scriptStart 
                  << " End: " << scriptEnd << " Size: " << m_eventScripts.size() << std::endl;
        return;
    }
    // 在执行前锁死当前场景和事件 ID
    // 注意：某些脚本（例如开场 Event 101）可能直接被调用，此时 m_currentSceneId/m_currentEventId 还未设置。
    int sceneContext = m_currentSceneId;
    if (sceneContext < 0) {
        sceneContext = GameManager::getInstance().getCurrentSceneId();
    }
    int eventContext = m_currentEventId;
    m_executingSceneId = sceneContext;
    m_executingEventId = eventContext;
    int pc = scriptStart; 
    
    std::cout << "Event " << eventScriptId << " Start. PC: " << pc << " End: " << scriptEnd << std::endl;

    while (pc < scriptEnd) {
        int16_t opcode = ReadScriptArg(pc);
        if (opcode < 0) {
             std::cout << "Negative Opcode " << opcode << " at PC " << (pc-1) << ". Terminating event." << std::endl;
             // Pascal: while e[i] >= 0 do ... 
             // Negative opcode means END of event!
             break;
        }

        std::cout << "Opcode: " << opcode << " at PC: " << (pc-1) << std::endl;

        switch (opcode) {
            case 0: 
                // Instruct 0 is typically Redraw.
                // Pascal logic: after opcode 0, check the NEXT opcode!
                // If the next opcode is < 0, terminate the event!
                Instruct_Redraw(); 
                // Check if we are at end OR next opcode is negative
                if (pc >= scriptEnd) {
                    std::cout << "Event " << eventScriptId << " Ends at Opcode 0 (EOF)." << std::endl;
                    return;
                }
                // Peek next opcode (critical for Pascal compatibility!)
                if (pc < scriptEnd) {
                    int16_t nextOp = m_eventScripts[pc];
                    if (nextOp < 0) {
                        std::cout << "Event " << eventScriptId << " Ends at Opcode 0 followed by Negative Opcode " << nextOp << "." << std::endl;
                        return; // 终止事件！这是 Pascal 的逻辑！
                    }
                }
                break;
            case 1: {
                int talkId = ReadScriptArg(pc);
                int headId = ReadScriptArg(pc);
                int mode = ReadScriptArg(pc);
                Instruct_Dialogue(talkId, headId, mode);
                break;
            }
            case 2: {
                int itemId = ReadScriptArg(pc);
                int amount = ReadScriptArg(pc);
                Instruct_AddItem(itemId, amount);
                break;
            }
            case 3: {
                // Instruct 3: 修改事件属性 (ModifyEvent)
                // Need to read args explicitly to debug
                int snum = ReadScriptArg(pc);
                int enum_ = ReadScriptArg(pc);
                int m = ReadScriptArg(pc);
                int v = ReadScriptArg(pc);
                int arg4 = ReadScriptArg(pc);
                int arg5 = ReadScriptArg(pc);
                int arg6 = ReadScriptArg(pc);
                int arg7 = ReadScriptArg(pc);
                int arg8 = ReadScriptArg(pc);
                int arg9 = ReadScriptArg(pc);
                int arg10 = ReadScriptArg(pc);
                int arg11 = ReadScriptArg(pc);
                int arg12 = ReadScriptArg(pc);
                
                // Debug: Check ModEvent args
                // std::cout << "Opcode 3 Args: S=" << snum << " E=" << enum_ << " M=" << m << " V=" << v 
                //           << " ... 11=" << arg11 << " 12=" << arg12 << std::endl;

                std::vector<int16_t> args;
                args.push_back(snum);
                args.push_back(enum_);
                args.push_back(m);
                args.push_back(v);
                args.push_back(arg4);
                args.push_back(arg5);
                args.push_back(arg6);
                args.push_back(arg7);
                args.push_back(arg8);
                args.push_back(arg9);
                args.push_back(arg10);
                args.push_back(arg11);
                args.push_back(arg12);
                Instruct_ModifyEvent(args);
                break;
            }
            case 4: { 
                int argStart = pc - 1; // 记录 opcode 位置
                int itemNum = ReadScriptArg(pc);
                int jump1 = ReadScriptArg(pc);
                int jump2 = ReadScriptArg(pc);
                bool hasItem = GameManager::getInstance().getItemAmount(itemNum) > 0;
                int jump = hasItem ? jump1 : jump2;
                pc = argStart + jump + 4; // 正确的跳转位置计算
                break;
            }
            case 5: {
                int argStart = pc - 1; // 记录 opcode 位置
                int jump1 = ReadScriptArg(pc);
                int jump2 = ReadScriptArg(pc);
                int choice = UIManager::getInstance().ShowChoice("是否與之戰鬥？"); 
                int jump = (choice == 1) ? jump1 : jump2;
                pc = argStart + jump + 3; // 正确的跳转位置计算
                break;
            }
            case 6: {
                int argStart = pc - 1; // 记录 opcode 位置
                int battleId = ReadScriptArg(pc);
                int jump1 = ReadScriptArg(pc);
                int jump2 = ReadScriptArg(pc);
                int getExp = ReadScriptArg(pc);
                std::cout << "[Opcode 6] argStart=" << argStart << " battleId=" << battleId 
                          << " jump1=" << jump1 << " jump2=" << jump2 << " getExp=" << getExp << std::endl;
                int result = Instruct_Battle(battleId, jump1, jump2, getExp);
                int newPc = argStart + result + 5;
                std::cout << "[Opcode 6] Jump calculation: argStart(" << argStart << ") + result(" << result 
                          << ") + 5 = " << newPc << std::endl;
                pc = newPc;
                std::cout << "[Opcode 6] Next opcode at pc=" << pc << " is " 
                          << (pc < m_eventScripts.size() ? m_eventScripts[pc] : -1) << std::endl;
                break;
            }
            case 8: Instruct_PlayMusic(ReadScriptArg(pc)); break;
            case 9: {
                int argStart = pc - 1; // 记录 opcode 位置
                int jump1 = ReadScriptArg(pc);
                int jump2 = ReadScriptArg(pc);
                int choice = UIManager::getInstance().ShowChoice("是否要求加入？");
                int jump = (choice == 1) ? jump1 : jump2;
                std::cout << "[Opcode 9] AskJoin: choice=" << choice << " jump1=" << jump1 
                          << " jump2=" << jump2 << " selected jump=" << jump << std::endl;
                pc = argStart + jump + 3;
                break;
            }
            case 10: Instruct_JoinParty(ReadScriptArg(pc)); break;
            case 11: {
                int argStart = pc - 1; // 记录 opcode 位置
                int jump1 = ReadScriptArg(pc);
                int jump2 = ReadScriptArg(pc);
                int result = Instruct_AskRest(jump1, jump2);
                pc = argStart + result + 3; // 正确的跳转位置计算
                break;
            }
            case 12: Instruct_Rest(); break;
            case 13: Instruct_FadeIn(); break;
            case 14: Instruct_FadeOut(); break;
            case 16: { // instruct_16: Check if role is in team
                int argStart = pc - 1; // 记录 opcode 位置
                int roleId = ReadScriptArg(pc);
                int jump1 = ReadScriptArg(pc);
                int jump2 = ReadScriptArg(pc);
                int teamState = GameManager::getInstance().getRole(roleId).getTeamState();
                bool inTeam = (teamState == 1 || teamState == 2);
                int jump = inTeam ? jump1 : jump2;
                std::cout << "[instruct_16] Role " << roleId << " TeamState=" << teamState 
                          << " inTeam=" << inTeam << " jump1=" << jump1 << " jump2=" << jump2 
                          << " selected jump=" << jump << std::endl;
                pc = argStart + jump + 4; // 正确的跳转位置计算
                std::cout << "[instruct_16] New PC=" << pc << " Next opcode=" 
                          << (pc < m_eventScripts.size() ? m_eventScripts[pc] : -1) << std::endl;
                break;
            }
            case 17: Instruct_Delay(ReadScriptArg(pc)); break;
            case 18: { // instruct_18: Check if player has item in inventory
                int argStart = pc - 1; // 记录 opcode 位置
                int itemNum = ReadScriptArg(pc);
                int jump1 = ReadScriptArg(pc);
                int jump2 = ReadScriptArg(pc);
                int amount = GameManager::getInstance().getItemAmount(itemNum);
                int jump = (amount > 0) ? jump1 : jump2;
                pc = argStart + jump + 4; // 正确的跳转位置计算
                break;
            }
            case 19: { // Instruct_19(x, y) - Teleport
                int x = ReadScriptArg(pc);
                int y = ReadScriptArg(pc);
                Instruct_19(x, y);
                break;
            }
            case 21: Instruct_LeaveParty(ReadScriptArg(pc)); break;
            case 23: {
                int roleId = ReadScriptArg(pc);
                int poison = ReadScriptArg(pc);
                Instruct_23(roleId, poison);
                break;
            }
            case 25: {
                int x1 = ReadScriptArg(pc);
                int y1 = ReadScriptArg(pc);
                int x2 = ReadScriptArg(pc);
                int y2 = ReadScriptArg(pc);
                Instruct_25(x1, y1, x2, y2);
                break;
            }
            case 26: { // Instruct_26(snum, enum, add1, add2, add3)
                int snum = ReadScriptArg(pc);
                int enum_ = ReadScriptArg(pc);
                int add1 = ReadScriptArg(pc);
                int add2 = ReadScriptArg(pc);
                int add3 = ReadScriptArg(pc);
                Instruct_26(snum, enum_, add1, add2, add3);
                break;
            }
            case 27: { // Instruct_27(enum, beginpic, endpic)
                int enum_ = ReadScriptArg(pc);
                int beginPic = ReadScriptArg(pc);
                int endPic = ReadScriptArg(pc);
                Instruct_27(enum_, beginPic, endPic);
                break;
            }
            case 28: { // instruct_28: Check role ethics in range
                int argStart = pc - 1; // 记录 opcode 位置
                int roleId = ReadScriptArg(pc);
                int e1 = ReadScriptArg(pc);
                int e2 = ReadScriptArg(pc);
                int jump1 = ReadScriptArg(pc);
                int jump2 = ReadScriptArg(pc);
                int ethics = GameManager::getInstance().getRole(roleId).getEthics();
                int jump = (ethics >= e1 && ethics <= e2) ? jump1 : jump2;
                pc = argStart + jump + 6; // 正确的跳转位置计算
                break;
            }
            case 29: { // instruct_29: Check role attack in range
                int argStart = pc - 1; // 记录 opcode 位置
                int roleId = ReadScriptArg(pc);
                int r1 = ReadScriptArg(pc);
                int r2 = ReadScriptArg(pc);
                int jump1 = ReadScriptArg(pc);
                int jump2 = ReadScriptArg(pc);
                int attack = GameManager::getInstance().GetRoleAttack(roleId, false);
                int jump = (attack >= r1 && attack <= r2) ? jump1 : jump2;
                pc = argStart + jump + 6; // 正确的跳转位置计算
                break;
            }
            case 30: { // Instruct_30(x1, y1, x2, y2) - Move Protagonist
                int x1 = ReadScriptArg(pc);
                int y1 = ReadScriptArg(pc);
                int x2 = ReadScriptArg(pc);
                int y2 = ReadScriptArg(pc);
                Instruct_30(x1, y1, x2, y2);
                break;
            }
            case 31: {
                int argStart = pc - 1; // 记录 opcode 位置
                int moneyNeeded = ReadScriptArg(pc);
                int jump1 = ReadScriptArg(pc);
                int jump2 = ReadScriptArg(pc);
                int result = Instruct_CheckMoney(moneyNeeded, jump1, jump2);
                pc = argStart + result + 4; // 正确的跳转位置计算
                break;
            }
            case 32: { // 2 args
                 int itemId = ReadScriptArg(pc);
                 int amount = ReadScriptArg(pc);
                 GameManager::getInstance().AddItem(itemId, amount);
                 break;
            }
            case 33: { // 3 args
                 for(int k=0; k<3; ++k) ReadScriptArg(pc);
                 break;
            }
            case 34: { // 2 args
                 for(int k=0; k<2; ++k) ReadScriptArg(pc);
                 break;
            }
            case 35: { // 4 args
                 for(int k=0; k<4; ++k) ReadScriptArg(pc);
                 break;
            }
            case 36: { // 3 args, returns jump
                 int argStart = pc - 1; // 记录 opcode 位置
                 int sexual = ReadScriptArg(pc);
                 int jump1 = ReadScriptArg(pc);
                 int jump2 = ReadScriptArg(pc);
                 
                 bool condition = false;
                 if (sexual > 255) {
                     int16_t flag = GameManager::getInstance().getX50(0x7000);
                     if (flag == 0) {
                         condition = true;
                     }
                     std::cout << "[instruct_36] sexual=" << sexual << " > 255, checking x50[0x7000]=" << flag 
                               << " condition=" << condition << " jump1=" << jump1 << " jump2=" << jump2 << std::endl;
                 } else {
                     if (GameManager::getInstance().getRole(0).getSexual() == sexual) {
                         condition = true;
                     }
                     std::cout << "[instruct_36] sexual=" << sexual << " <= 255, checking role[0].Sexual=" 
                               << GameManager::getInstance().getRole(0).getSexual() << " condition=" << condition 
                               << " jump1=" << jump1 << " jump2=" << jump2 << std::endl;
                 }
                 
                 pc = argStart + (condition ? jump1 : jump2) + 4; // 正确的跳转位置计算
                 break;
            }
            case 37: ReadScriptArg(pc); break;
            case 38: { // Instruct_38(snum, layernum, oldpic, newpic)
                 int snum = ReadScriptArg(pc);
                 int layernum = ReadScriptArg(pc);
                 int oldpic = ReadScriptArg(pc);
                 int newpic = ReadScriptArg(pc);
                 Instruct_38(snum, layernum, oldpic, newpic);
                 break;
            }
            case 39: {
                int sceneId = ReadScriptArg(pc);
                if (sceneId == -2 || sceneId == -1) {
                    sceneId = GameManager::getInstance().getCurrentSceneId();
                }
                Scene* scene = SceneManager::getInstance().GetScene(sceneId);
                if (scene) {
                    scene->setEnCondition(0);
                }
                break;
            }

            case 40: {
                int dir = ReadScriptArg(pc);
                Instruct_40(dir);
                break;
            }
            case 41: { // 3 args
                 for(int k=0; k<3; ++k) ReadScriptArg(pc);
                 break;
            }
            case 42: { // 2 args, returns jump
                 for(int k=0; k<2; ++k) ReadScriptArg(pc);
                 break;
            }
            case 43: { // instruct_43: Call another event / special functions
                int e1 = ReadScriptArg(pc);
                int subFunc = ReadScriptArg(pc);
                int arg3 = ReadScriptArg(pc);
                int arg4 = ReadScriptArg(pc);
                int arg5 = ReadScriptArg(pc);
                int arg6 = ReadScriptArg(pc);
                
                std::cout << "[instruct_43] e1=" << e1 << " subFunc=" << subFunc 
                          << " arg3=" << arg3 << " arg4=" << arg4 
                          << " arg5=" << arg5 << " arg6=" << arg6 << std::endl;
                
                GameManager::getInstance().setX50(0x7100, arg3);
                GameManager::getInstance().setX50(0x7101, arg4);
                GameManager::getInstance().setX50(0x7102, arg5);
                GameManager::getInstance().setX50(0x7103, arg6);
                
                if (subFunc == -1) {
                    // jmpScene(e3, e4, e5)
                    Instruct_JmpScene(arg3, arg4, arg5);
                } else if (subFunc == -2) {
                    GameManager::getInstance().setX50(0x7000, 0);
                    UIManager::getInstance().ShowDialogue(" （作詩小遊戲暫以自動通過處理）", -1, 0);
                    Instruct_Redraw();
                } else if (subFunc == -3) {
                    // Check pet skill: GetPetSkill(arg3, arg4)
                    GameManager::getInstance().setX50(0x7000, 1);
                    int roleId = arg3;
                    int skillIndex = arg4;
                    if (GameManager::getInstance().getRole(roleId).getMagic(skillIndex) > 0) {
                        GameManager::getInstance().setX50(0x7000, 0);
                    }
                    std::cout << "[instruct_43 -3] role=" << roleId << " skill=" << skillIndex 
                              << " hasSkill=" << (GameManager::getInstance().getX50(0x7000) == 0 ? "yes" : "no") << std::endl;
                } else if (subFunc == -4) {
                    GameManager::getInstance().setX50(0x7000, 0);
                    UIManager::getInstance().ShowDialogue(" （針灸小遊戲暫以自動通過處理）", -1, 0);
                    Instruct_Redraw();
                } else if (subFunc == -5) {
                    GameManager::getInstance().setX50(0x7000, 0);
                } else if (subFunc == -6) {
                    GameManager::getInstance().setX50(0x7000, 0);
                } else if (subFunc == -7) {
                    int randomEvent = GameManager::getInstance().getX50(0x6000);
                    if (randomEvent > 0) ExecuteEvent(randomEvent);
                } else if (subFunc == -8) {
                    // SDL_UpdateRect2 - just redraw
                    Instruct_Redraw();
                } else if (subFunc == -9) {
                    GameManager::getInstance().setX50(0x7000, 0);
                    Instruct_Redraw();
                } else if (subFunc == -10) {
                    GameManager::getInstance().setX50(0x7000, 0);
                    Instruct_Redraw();
                } else if (subFunc == -11) {
                    // AddDefense
                    int roleId = arg3;
                    int def = arg4;
                    Role& role = GameManager::getInstance().getRole(roleId);
                    role.setDefence(role.getDefence() + def);
                    Instruct_Redraw();
                } else if (subFunc == 540) {
                    GameManager::getInstance().setX50(0x7000, 0);
                    UIManager::getInstance().ShowDialogue(" （解謎小遊戲暫以自動通過處理）", -1, 0);
                } else if (subFunc == 1055) {
                    Instruct_Redraw();
                } else if (subFunc > 0) {
                    // CallEvent: Execute another event script
                    // This is the key for pet skill advanced dialogue!
                    std::cout << "[instruct_43] Calling event " << subFunc << std::endl;
                    ExecuteEvent(subFunc);
                }
                break;
            }
            case 44: { // Instruct_44(e1, b1, end1, e2, b2, end2)
                 int e1 = ReadScriptArg(pc);
                 int b1 = ReadScriptArg(pc);
                 int end1 = ReadScriptArg(pc);
                 int e2 = ReadScriptArg(pc);
                 int b2 = ReadScriptArg(pc);
                 int end2 = ReadScriptArg(pc);
                 Instruct_44(e1, b1, end1, e2, b2, end2);
                 break;
            }
            case 50: {
                int args[7];
                for (int k = 0; k < 7; ++k) {
                    args[k] = ReadScriptArg(pc);
                }
                std::cout << "[instruct_50] code=" << args[0] << " e1=" << args[1] << " e2=" << args[2] 
                          << " e3=" << args[3] << " e4=" << args[4] << " e5=" << args[5] << " e6=" << args[6] << std::endl;
                int result = Instruct_50e(args[0], args[1], args[2], args[3], args[4], args[5], args[6]);
                std::cout << "[instruct_50] result=" << result << std::endl;
                if (result != 0) {
                    pc += result;
                }
                break;
            }
            case 62: {
                Instruct_GameOver();
                return;
            }

             // New Opcodes
             case 68: {
                // NewTalk0(e[i + 1], e[i + 2], e[i + 3], e[i + 4], e[i + 5], e[i + 6], e[i + 7]);
                int arg1 = ReadScriptArg(pc);
                int arg2 = ReadScriptArg(pc);
                int arg3 = ReadScriptArg(pc);
                int arg4 = ReadScriptArg(pc);
                int arg5 = ReadScriptArg(pc);
                int arg6 = ReadScriptArg(pc);
                int arg7 = ReadScriptArg(pc);
                Instruct_NewTalk0(arg1, arg2, arg3, arg4, arg5, arg6, arg7);
                break;
            }
             case 69: {
                 // ReSetName(e[i + 1], e[i + 2], e[i + 3]);
                 int arg1 = ReadScriptArg(pc);
                 int arg2 = ReadScriptArg(pc);
                 int arg3 = ReadScriptArg(pc);
                 Instruct_ReSetName(arg1, arg2, arg3);
                 break;
             }
             case 70: {
                 // ShowTiTle(e[i + 1], e[i + 2]);
                 int arg1 = ReadScriptArg(pc);
                 int arg2 = ReadScriptArg(pc);
                 Instruct_ShowTitle(arg1, arg2);
                 break;
             }
             case 71: {
                 // JmpScene(e[i + 1], e[i + 2], e[i + 3]);
                 int arg1 = ReadScriptArg(pc);
                 int arg2 = ReadScriptArg(pc);
                 int arg3 = ReadScriptArg(pc);
                 Instruct_JmpScene(arg1, arg2, arg3);
                 break;
             }
             default:
                // std::cerr << "Unknown opcode: " << opcode << std::endl;
                break;
        }
    }
    m_executingSceneId = -1;
    m_executingEventId = -1;
}

    // Implementations for new opcodes
    
void EventManager::Instruct_NewTalk0(int headNum, int talkNum, int nameNum, int place, int showHead, int color, int frame) {
    // Decode Talk using NewTalk logic (similar to Instruct_Dialogue but might handle args differently)
    // FIX: Talk IDs are 1-based in script, so we subtract 1 to get 0-based index.
    int actualTalkNum = talkNum - 1;

    if (actualTalkNum < 0 || actualTalkNum >= m_talkIndices.size()) {
        std::string err = "Error: TalkID " + std::to_string(talkNum) + " Missing!";
        UIManager::getInstance().ShowDialogue(err, 0, 0);
        return;
    }
    int offset = m_talkIndices[actualTalkNum];
    int nextOffset = (actualTalkNum + 1 < m_talkIndices.size()) ? m_talkIndices[actualTalkNum + 1] : m_talkData.size();
    
    int len = nextOffset - offset;
    
    // Safety check for garbage data - FORCE DISPLAY ERROR
    if (len <= 0 || offset + len > m_talkData.size()) {
         std::string err = "Error: TalkID " + std::to_string(talkNum) + " Empty/Invalid!";
         UIManager::getInstance().ShowDialogue(err, 0, 0);
         return;
    }
    if (len > 2000) len = 2000; // Cap length to avoid reading huge garbage chunks

    // DEBUG: Print raw bytes for talk 247
    if (talkNum == 247) {
        std::cout << "[DEBUG Talk 247] Offset: " << offset << ", Len: " << len << std::endl;
        std::cout << "[DEBUG Talk 247] Raw bytes (first 100): ";
        for (int i = 0; i < len && i < 100; i++) {
            printf("%02X ", m_talkData[offset + i]);
        }
        std::cout << std::endl;
    }

    // Decode with 0xFF terminator check (Pascal NewTalk logic)
    std::vector<uint8_t> decoded;
    decoded.reserve(len);
    
    for (int i = 0; i < len; ++i) {
        uint8_t b = m_talkData[offset + i] ^ 0xFF;
        if (b == 0xFF) { // Pascal NewTalk uses 0xFF as terminator
            b = 0;
            break; // Stop decoding
        }
        decoded.push_back(b);
        if (b == 0) break; // Null terminator
    }
    
    // DEBUG: Print decoded bytes for talk 247
    if (talkNum == 247) {
        std::cout << "[DEBUG Talk 247] Decoded bytes (first 100): ";
        for (size_t i = 0; i < decoded.size() && i < 100; i++) {
            printf("%02X ", decoded[i]);
        }
        std::cout << std::endl;
        std::cout << "[DEBUG Talk 247] Looking for '头'(CD B7), '疼'(CC DB), '过'(B9 FD)" << std::endl;
        for (size_t i = 0; i + 1 < decoded.size(); i++) {
            if ((decoded[i] == 0xCD && decoded[i+1] == 0xB7) ||
                (decoded[i] == 0xCC && decoded[i+1] == 0xDB) ||
                (decoded[i] == 0xB9 && decoded[i+1] == 0xFD)) {
                printf("[DEBUG Talk 247] Found at pos %zu: %02X %02X\n", i, decoded[i], decoded[i+1]);
            }
        }
    }
    
    // Check for trailing garbage:
    // If we hit a null terminator, we are good.
    // But sometimes the string might continue with garbage if we didn't hit null.
    // Let's force null termination if we didn't push 0.
    if (decoded.empty() || decoded.back() != 0) {
        decoded.push_back(0);
    }
    
    // Convert to String and Clean up
    std::string fullText;
    int p = 0;
    while (p < decoded.size() && decoded[p] != 0) {
        fullText += (char)decoded[p];
        p++;
    }

    // Process Placeholders
    // NOTE: heroName and heroNick must remain in GBK encoding until after gbkToUtf8 is called on cleanText
    std::string heroName = GameManager::getInstance().getRole(0).getName();
    std::string heroNick = GameManager::getInstance().getRole(0).getNick();

    // Ensure heroName has a default if empty (keep in GBK encoding!)
    if (heroName.empty() || LooksLikeUninitializedName(heroName)) 
        heroName = "金先生";  // Keep as GBK, not UTF-8!
    
    std::string heroSurname = ExtractSurnameBytesGbk(heroName);
    std::string heroGiven = (heroName.size() > heroSurname.size()) ? heroName.substr(heroSurname.size()) : "";

    std::string cleanText;
    for (size_t i = 0; i < fullText.length(); ++i) {
        unsigned char c = (unsigned char)fullText[i];
        
        // Check for double-char control codes (KYS Standard)
        if (i + 1 < fullText.length()) {
            char nextC = fullText[i + 1];
            
            // ** = Newline
            if (c == '*' && nextC == '*') { 
                cleanText += '\n'; 
                i++; 
                continue; 
            }
            
            // && = Full Name (Hero)
            if (c == '&' && nextC == '&') { 
                cleanText += heroName; 
                i++; 
                continue; 
            }
            
            // %% = Given Name (Hero Name - Surname)
            if (c == '%' && nextC == '%') { 
                cleanText += heroGiven;
                i++; 
                continue; 
            }
            
            // $$ = Surname
            if (c == '$' && nextC == '$') { 
                cleanText += heroSurname; 
                i++; 
                continue; 
            }
            
            // @@ = Wait for Key (Pause)
            // For now, we just remove it to avoid displaying '@'
            if (c == '@' && nextC == '@') { 
                i++; 
                continue; 
            }
            
            // ## = Delay
            if (c == '#' && nextC == '#') { 
                i++; 
                continue; 
            }
        }

        // Control Codes
        if (c == '^') {
            if (i + 1 < fullText.length()) {
                // Skip color code
                i++;
                continue;
            }
        }
        else if (c == '@') {
            // Legacy Placeholder @0, @N (Keep if not @@)
            if (i + 1 < fullText.length()) {
                char nextC = fullText[i+1];
                if (nextC == '0') {
                    cleanText += heroName;
                    i++;
                } else if (nextC == 'N') {
                    cleanText += heroNick;
                    i++;
                } else {
                    cleanText += (char)c;
                }
            } else {
                cleanText += (char)c;
            }
        }
        else {
            cleanText += (char)c;
        }
    }
    
    // Handle Name
    std::string showName;
    if (nameNum > 0) {
         showName = GetNameFromData(nameNum);
    } else if (nameNum == -2) {
        // Use Name of Role with HeadNum
        int roleCount = GameManager::getInstance().getRoleCount();
        for (int i = 0; i < roleCount; ++i) { 
             Role& r = GameManager::getInstance().getRole(i);
             if (r.getHeadNum() == headNum) {
                 showName = r.getName();
                 break; 
             }
        }
        // If not found, check if HeadNum is 0 (Protagonist)
        if (showName.empty() && headNum == 0 && roleCount > 0) {
            showName = GameManager::getInstance().getRole(0).getName();
        }
    }
    if (showName.empty() && headNum == 0) {
        showName = heroName;
    }

    std::string utf8Text = TextManager::getInstance().talkToUtf8(cleanText);
    std::string utf8Name = TextManager::getInstance().nameToUtf8(showName);
    
    // Head ID Logic:
    int drawHead = (showHead == 0) ? headNum : -1;
    
    UIManager::getInstance().ShowDialogue(utf8Text, drawHead, place, utf8Name, showName, color);
}

void EventManager::Instruct_ReSetName(int type, int id, int newNameId) {
    // Rename role or item?
    // type: 0=Role, 1=Item?
    // Not implemented yet.
}

void EventManager::Instruct_Dialogue(int talkId, int headId, int mode) {
    // FIX: Talk IDs are 1-based in script.
    int actualTalkId = talkId - 1;
    if (actualTalkId < 0 || actualTalkId >= m_talkIndices.size()) return;
    int offset = m_talkIndices[actualTalkId];
    int nextOffset = (actualTalkId + 1 < m_talkIndices.size()) ? m_talkIndices[actualTalkId + 1] : m_talkData.size();
    
    int len = nextOffset - offset;
    // Safety check for garbage data
    if (len <= 0 || offset + len > m_talkData.size()) return;
    if (len > 2000) len = 2000;

    std::vector<uint8_t> decoded(len + 1, 0);
    for (int i = 0; i < len; ++i) {
        uint8_t b = static_cast<uint8_t>(m_talkData[offset + i] ^ 0xFF);
        if (b == 0xFF) {
            b = 0;
            decoded[i] = 0;
            break;
        }
        decoded[i] = b;
        if (b == 0) break;
    }
    
    int p = 0;
    for (int i = 0; i < len; ++i) {
        if (decoded[i] == 0) {
            std::string part((char*)&decoded[p]);
            if (!part.empty()) {
                std::string heroName = GameManager::getInstance().getRole(0).getName();
                std::string heroNick = GameManager::getInstance().getRole(0).getNick();
                if (heroName.empty() || LooksLikeUninitializedName(heroName))
                    heroName = "金先生";

                size_t pos;
                while ((pos = part.find("@0")) != std::string::npos) {
                    part.replace(pos, 2, heroName);
                }
                while ((pos = part.find("@N")) != std::string::npos) {
                    part.replace(pos, 2, heroNick);
                }
                
                std::string surname = ExtractSurnameBytesGbk(heroName);
                std::string given = (heroName.length() > surname.length()) ? heroName.substr(surname.length()) : "";

                while ((pos = part.find("&&")) != std::string::npos) {
                    part.replace(pos, 2, heroName);
                }
                while ((pos = part.find("%%")) != std::string::npos) {
                    part.replace(pos, 2, given);
                }
                while ((pos = part.find("$$")) != std::string::npos) {
                    part.replace(pos, 2, surname);
                }

                std::string utf8Text = TextManager::getInstance().talkToUtf8(part);
                UIManager::getInstance().ShowDialogue(utf8Text, headId, mode);
            }
            p = i + 1;
        }
    }
}

void EventManager::Instruct_ShowTitle(int talkNum, int color) {
    // Instruction 70: Show a big title on screen
    int actualTalkNum = (talkNum > 0) ? talkNum - 1 : 0;
    
    if (actualTalkNum < 0 || actualTalkNum >= m_talkIndices.size()) return;
    int offset = m_talkIndices[actualTalkNum];
    int nextOffset = (actualTalkNum + 1 < m_talkIndices.size()) ? m_talkIndices[actualTalkNum + 1] : m_talkData.size();
    
    int len = nextOffset - offset;
    if (len <= 0 || offset + len > m_talkData.size()) return;
    if (len > 2000) len = 2000;

    std::vector<uint8_t> decoded;
    decoded.reserve(len);
    for (int i = 0; i < len; ++i) {
        uint8_t b = m_talkData[offset + i] ^ 0xFF;
        if (b == 0xFF) break;
        decoded.push_back(b);
    }
    
    std::string text(decoded.begin(), decoded.end());
    // Basic placeholder replacement (copy from NewTalk0 if needed, but Title usually static)
    
    std::string utf8Text = TextManager::getInstance().talkToUtf8(text);
    
    uint32_t colorMain = GraphicsUtils::getPaletteColor(5);
    uint32_t colorShadow = GraphicsUtils::getPaletteColor(7);
    
    UIManager::getInstance().ShowTitle(utf8Text, -1, -1, colorMain, colorShadow);
}

void EventManager::Instruct_AddItem(int itemId, int amount) {
    GameManager::getInstance().AddItem(itemId, amount);
    UIManager::getInstance().ShowItemNotification(itemId, amount);
}

void EventManager::Instruct_ModifyEvent(const std::vector<int16_t>& args) {
    if (args.size() < 13) return;

    int sceneId = args[0];
    int eventId = args[1];
    
    if (sceneId == -2) sceneId = m_executingSceneId; // 修正：使用执行时的场景 ID
    if (eventId == -2) eventId = m_executingEventId; // 修正：处理当前事件 ID
    
    SceneManager& sm = SceneManager::getInstance();

    // Capture OLD state for change detection
    int oldCondition = sm.GetEventData(sceneId, eventId, 0);
    int oldScriptId = sm.GetEventData(sceneId, eventId, 4);

    // Pascal Logic:
    // list[0]..list[12]
    // Reverting: DData Index 9 is Y, Index 10 is X.
    // If list[11] == -2, use current Y (Index 9)
    // If list[12] == -2, use current X (Index 10)
    
    int arg11 = args[11];
    int arg12 = args[12];
    
    int currentY = sm.GetEventData(sceneId, eventId, 9);
    int currentX = sm.GetEventData(sceneId, eventId, 10);
    
    if (arg11 == -2) arg11 = currentY;
    if (arg12 == -2) arg12 = currentX;

    // Clear old SData (Layer 3)
    if (currentX >= 0 && currentY >= 0) {
        if (sm.GetSceneTile(sceneId, 3, currentX, currentY) == eventId) {
            sm.SetSceneTile(sceneId, 3, currentX, currentY, -1); 
        }
    }

    // Update DData
    // args[2..12] map to DData[0..10]
    for (int i = 0; i <= 10; ++i) {
        if (2 + i < (int)args.size()) {
            int val = args[2 + i];
            if (val != -2) { 
                 sm.SetEventData(sceneId, eventId, i, val);
            }
        }
    }

    // Explicitly handle X/Y if provided in args[12]/args[11]
    if (arg12 != -2) sm.SetEventData(sceneId, eventId, 10, arg12); // X
    if (arg11 != -2) sm.SetEventData(sceneId, eventId, 9, arg11);  // Y

    // Set new SData
    int newY = sm.GetEventData(sceneId, eventId, 9);
    int newX = sm.GetEventData(sceneId, eventId, 10);
    
    if (newX >= 0 && newY >= 0) {
        sm.SetSceneTile(sceneId, 3, newX, newY, eventId);
    }

    // Force redraw
    Instruct_Redraw();

    // Check for Auto-Trigger (Condition == 0)
    // Only trigger if the event is in the CURRENT scene
    // RELAXED CHECK: If args[0] is -2, it means current scene.
    // If args[0] is explicitly set, check if it matches current scene.
    int currentSceneId = GameManager::getInstance().getCurrentSceneId();
    bool isCurrentScene = (sceneId == -2) || (sceneId == -1) || (sceneId == currentSceneId); // Fixed: -1 also means current scene in KYS scripts
    
    std::cout << "ModEvent Check: Scene=" << sceneId << " (Cur=" << currentSceneId << ") Event=" << eventId << std::endl;

    if (isCurrentScene) {
        // Re-fetch sceneId if it was -2 or -1
        if (sceneId == -2 || sceneId == -1) sceneId = currentSceneId;

        int newCondition = sm.GetEventData(sceneId, eventId, 0);
        int newScriptId = sm.GetEventData(sceneId, eventId, 4);
        
        // Debug Log
        std::cout << "ModEvent: ID=" << eventId << " Cond: " << oldCondition << "->" << newCondition 
                   << " Script: " << oldScriptId << "->" << newScriptId << std::endl;

        // Trigger if:
        // 1. Condition is NOW 0.
        // 2. Script ID is valid (>0).
        // 3. Something relevant changed:
        //    a. Condition changed to 0 (Activated)
        //    b. Script changed (while Condition is 0) (Updated behavior)
        bool shouldTrigger = false;
        
        if (newCondition == 0 && newScriptId > 0) {
            // Only trigger if the SCRIPT changed.
            // This prevents auto-triggering "Doors" (Exit Events) when we just enable them (Condition 1->0).
            // Doors usually have a constant Script ID.
            // Cutscenes usually involve assigning a NEW Script ID (e.g. 0 -> 2235).
            if (oldScriptId != newScriptId) {
                // EXCEPTION: If the event is at the PLAYER'S current location, trigger it immediately.
                // If we are enabling an event at a DISTANT location (e.g. Exit at 38,38 while player is at 28,15),
                // we should NOT trigger it.
                
                int eventX = sm.GetEventData(sceneId, eventId, 10);
                int eventY = sm.GetEventData(sceneId, eventId, 9);
                
                // If event has valid coordinates (not 0,0)
                if (eventX > 0 && eventY > 0) {
                    int playerX, playerY;
                    GameManager::getInstance().getMainMapPosition(playerX, playerY);
                    
                    if (playerX != eventX || playerY != eventY) {
                        shouldTrigger = false;
                        std::cout << "ModEvent: Blocked Auto-Trigger for distant event " << eventId 
                                  << " at (" << eventX << "," << eventY << ") Player: (" << playerX << "," << playerY << ")" << std::endl;
                    } else {
                        shouldTrigger = true;
                    }
                } else {
                    // Event at 0,0 or undefined -> Likely a system event or global auto-event.
                    shouldTrigger = true;
                }
            }
            // FORCE TRIGGER: If user insists on Scene 52 Event 0 logic
            // Event 2234 modifies Event 0 to Script 2235.
            // We MUST catch this.
            if (eventId == 0 && newScriptId == 2235) {
                shouldTrigger = true;
            }
        }
        
        if (shouldTrigger) {
             std::cout << "Instruct_ModifyEvent: Queuing Auto-Trigger Event " << eventId 
                       << " (Script " << newScriptId << ")" << std::endl;
             // Queue it instead of executing immediately!
             QueueEvent(newScriptId);
        }
    }
}

int EventManager::Instruct_Battle(int battleId, int jump1, int jump2, int getExp) {
    std::cout << "[Instruct_Battle] Starting battle " << battleId 
              << " jump1=" << jump1 << " jump2=" << jump2 << " getExp=" << getExp << std::endl;
    bool result = BattleManager::getInstance().StartBattle(battleId, getExp);
    int jumpResult = result ? jump1 : jump2;
    std::cout << "[Instruct_Battle] Battle result: " << (result ? "VICTORY" : "DEFEAT") 
              << " returning jump=" << jumpResult << std::endl;
    return jumpResult;
}

void EventManager::Instruct_PlayMusic(int musicId) {
    SoundManager::getInstance().PlayMusic(musicId);
}

void EventManager::Instruct_JoinParty(int roleId) {
    GameManager::getInstance().JoinParty(roleId);
}

int EventManager::Instruct_AskRest(int jump1, int jump2) {
    int w, h;
    SDL_GetWindowSize(UIManager::getInstance().GetWindow(), &w, &h);
    int centerX = w / 2;
    int centerY = h / 2;
    
    uint32_t color1 = GraphicsUtils::getPaletteColor(5);
    uint32_t color2 = GraphicsUtils::getPaletteColor(7);
    uint32_t frameColor = GraphicsUtils::getPaletteColor(255);
    
    GameManager::getInstance().RenderScreenTo(UIManager::getInstance().GetRenderer());
    
    UIManager::getInstance().DrawRectangle(centerX - 75, centerY - 85, 150, 30, 0, frameColor, 30);
    UIManager::getInstance().DrawShadowTextUtf8(" 是否需要住宿？", centerX - 70, centerY - 82, color1, color2);
    
    int selection = 0;
    bool running = true;
    SDL_Event event;
    
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                GameManager::getInstance().Quit();
                return jump2;
            }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_LEFT || event.key.key == SDLK_UP) {
                    selection = (selection + 1) % 2;
                } else if (event.key.key == SDLK_RIGHT || event.key.key == SDLK_DOWN) {
                    selection = (selection + 1) % 2;
                } else if (event.key.key == SDLK_RETURN || event.key.key == SDLK_SPACE || event.key.key == SDLK_Y) {
                    running = false;
                } else if (event.key.key == SDLK_ESCAPE || event.key.key == SDLK_N) {
                    selection = 1;
                    running = false;
                }
            }
        }
        
        GameManager::getInstance().RenderScreenTo(UIManager::getInstance().GetRenderer());
        UIManager::getInstance().DrawRectangle(centerX - 75, centerY - 85, 150, 30, 0, frameColor, 30);
        UIManager::getInstance().DrawShadowTextUtf8(" 是否需要住宿？", centerX - 70, centerY - 82, color1, color2);
        
        uint32_t selColor1 = GraphicsUtils::getPaletteColor(0x64);
        uint32_t selColor2 = GraphicsUtils::getPaletteColor(0x66);
        uint32_t norColor1 = GraphicsUtils::getPaletteColor(5);
        uint32_t norColor2 = GraphicsUtils::getPaletteColor(7);
        
        if (selection == 0) {
            UIManager::getInstance().DrawShadowTextUtf8(" 要求", centerX - 49, centerY - 50, selColor1, selColor2);
            UIManager::getInstance().DrawShadowTextUtf8(" 取消", centerX + 1, centerY - 50, norColor1, norColor2);
        } else {
            UIManager::getInstance().DrawShadowTextUtf8(" 要求", centerX - 49, centerY - 50, norColor1, norColor2);
            UIManager::getInstance().DrawShadowTextUtf8(" 取消", centerX + 1, centerY - 50, selColor1, selColor2);
        }
        
        SDL_RenderPresent(UIManager::getInstance().GetRenderer());
        SDL_Delay(16);
    }
    
    Instruct_Redraw();
    
    return (selection == 0) ? jump1 : jump2;
}

void EventManager::Instruct_Rest() {
    GameManager& gm = GameManager::getInstance();
    
    for (int i = 0; i < 6; ++i) {
        int roleId = gm.getTeamMember(i);
        if (roleId >= 0) {
            Role& role = gm.getRole(roleId);
            if (role.getHurt() <= 33 && role.getPoision() <= 33) {
                role.setHurt(0);
                role.setPoision(0);
                role.setCurrentHP(role.getMaxHP());
                role.setCurrentMP(role.getMaxMP());
                role.setPhyPower(100);
            }
        }
    }
    
    for (int i = 0; i < gm.getRoleCount(); ++i) {
        Role& role = gm.getRole(i);
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

int EventManager::Instruct_CheckMoney(int moneyNeeded, int jump1, int jump2) {
    int moneyCount = GameManager::getInstance().getItemAmount(0);
    return (moneyCount >= moneyNeeded) ? jump1 : jump2;
}

void EventManager::Instruct_FadeIn() {
    UIManager::getInstance().FadeScreen(true);
}

void EventManager::Instruct_FadeOut() {
    UIManager::getInstance().FadeScreen(false);
}

void EventManager::Instruct_LeaveParty(int roleId) {
    GameManager::getInstance().LeaveParty(roleId);
}

void EventManager::Instruct_SetScene(int sceneId, int x, int y, int dir) {
    // Support jumping to World Map (sceneId = -1)
    if (sceneId == -1) {
        int currentSceneId = GameManager::getInstance().getCurrentSceneId();
        int worldX = 240;
        int worldY = 240;
        bool worldSet = false;
        if (x >= 0 && y >= 0) {
            int targetRow = x;
            int targetCol = y;
            worldX = targetCol;
            worldY = targetRow;
            worldSet = true;
        }
        if (!worldSet) {
            int savedX, savedY;
            GameManager::getInstance().getSavedWorldPosition(savedX, savedY);
            bool savedValid = (savedX >= 0 && savedX < 480 && savedY >= 0 && savedY < 480) &&
                              !(savedX == 0 && savedY == 0);
            if (savedValid) {
                worldX = savedX;
                worldY = savedY;
                worldSet = true;
            }
        }
        if (!worldSet && currentSceneId >= 0) {
            Scene* scene = SceneManager::getInstance().GetScene(currentSceneId);
            if (scene) {
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
        }
        GameManager::getInstance().enterScene(-1);
        GameManager::getInstance().setMainMapPosition(worldX, worldY);
        if (dir >= 0) {
            GameManager::getInstance().setMainMapFace(dir);
        }
        Instruct_Redraw();
        return;
    }
    
    // Normal scene change

    GameManager::getInstance().enterScene(sceneId);
    int targetRow = x;
    int targetCol = y;
    if (x == -2 || y == -2 || (x < 0 && y < 0)) {
        Scene* scene = SceneManager::getInstance().GetScene(sceneId);
        if (scene) {
            if (x == -2 || x < 0) targetRow = scene->getEntranceY();
            if (y == -2 || y < 0) targetCol = scene->getEntranceX();
        }
    }
    if (targetRow >= 0 && targetCol >= 0) { 
        GameManager::getInstance().setMainMapPosition(targetCol, targetRow);
    }
    // Set Direction if provided (Pascal uses -1 for no change sometimes, or explicit 0..3)
    // Assuming dir is valid direction if >= 0.
    if (dir >= 0) {
        GameManager::getInstance().setMainMapFace(dir);
    }
}

void EventManager::Instruct_AddAttribute(int roleId, int attrId, int value) {
    Role& role = GameManager::getInstance().getRole(roleId);
    switch (attrId) {
        case 0: role.setCurrentHP(role.getCurrentHP() + value); break;
        case 1: role.setMaxHP(role.getMaxHP() + value); break;
        case 2: role.setCurrentMP(role.getCurrentMP() + value); break;
        case 3: role.setMaxMP(role.getMaxMP() + value); break;
        case 4: role.setAttack(role.getAttack() + value); break;
        case 5: role.setSpeed(role.getSpeed() + value); break;
        case 6: role.setDefence(role.getDefence() + value); break;
        case 7: role.setMedcine(role.getMedcine() + value); break;
        case 8: role.setUsePoi(role.getUsePoi() + value); break;
        case 9: role.setMedPoi(role.getMedPoi() + value); break;
        case 10: role.setDefPoi(role.getDefPoi() + value); break;
        case 11: role.setFist(role.getFist() + value); break;
        case 12: role.setSword(role.getSword() + value); break;
        case 13: role.setKnife(role.getKnife() + value); break;
        case 14: role.setUnusual(role.getUnusual() + value); break;
        case 15: role.setHidWeapon(role.getHidWeapon() + value); break;
        case 16: role.setKnowledge(role.getKnowledge() + value); break;
        case 17: role.setEthics(role.getEthics() + value); break;
        case 18: role.setAttPoi(role.getAttPoi() + value); break;
        case 19: role.setAttTwice(role.getAttTwice() + value); break;
        case 20: role.setRepute(role.getRepute() + value); break;
        case 21: role.setAptitude(role.getAptitude() + value); break;
        case 22: role.setExp(role.getExp() + value); break;
        default: break;
    }
}

void EventManager::Instruct_PlaySound(int soundId) {
    SoundManager::getInstance().PlaySound(soundId);
}

void EventManager::Instruct_GameOver() {
    Instruct_FadeOut();

    UIManager::getInstance().DrawRectangle(0, 0, 640, 440, 0, 0, 100);
    UIManager::getInstance().UpdateScreen();

    Instruct_ShowTitle(4547, 28515);
    Instruct_FadeOut();
    Instruct_ShowTitle(4548, 28515);
    Instruct_FadeIn();

    bool done = false;
    SDL_Event event;
    while (!done) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                GameManager::getInstance().Quit();
                return;
            }
            if (event.type == SDL_EVENT_KEY_UP) {
                done = true;
                break;
            }
        }
        SDL_Delay(16);
    }

    int16_t gameTime = GameManager::getInstance().getGameTime();
    GameManager::getInstance().setGameTime((int16_t)std::max((int)gameTime, 1 + (int)gameTime));
    if (GameManager::getInstance().LoadGame(0)) {
        GameManager::getInstance().SaveGame(0);
    }
    GameManager::getInstance().ReturnToTitleScreen();
}

int EventManager::Instruct_50e(int code, int e1, int e2, int e3, int e4, int e5, int e6) {
    int result = 0;
    
    switch (code) {
        case 0:
            GameManager::getInstance().setX50(e1, e2);
            break;
        case 1: {
            int t1 = e3;
            if (e1 & 1) t1 = GameManager::getInstance().getX50(e3);
            int idx4 = e4;
            if ((e1 >> 1) & 1) idx4 = GameManager::getInstance().getX50(e4);
            int val = e5;
            if ((e1 >> 2) & 1) val = GameManager::getInstance().getX50(e5);
            t1 = t1 + idx4;
            GameManager::getInstance().setX50(t1, val);
            if (e2 == 1) {
                int currentVal = GameManager::getInstance().getX50(t1);
                GameManager::getInstance().setX50(t1, currentVal & 0xFF);
            }
            break;
        }
        case 2: {
            int t1 = e3;
            if (e1 & 1) t1 = GameManager::getInstance().getX50(e3);
            int idx4 = e4;
            if ((e1 >> 1) & 1) idx4 = GameManager::getInstance().getX50(e4);
            t1 = t1 + idx4;
            int val = GameManager::getInstance().getX50(t1);
            if (e2 == 1) val = val & 0xFF;
            GameManager::getInstance().setX50(e5, val);
            break;
        }
        case 3: {
            int t1 = e5;
            if (e1 & 1) t1 = GameManager::getInstance().getX50(e5);
            int val4 = GameManager::getInstance().getX50(e4);
            switch (e2) {
                case 0: GameManager::getInstance().setX50(e3, val4 + t1); break;
                case 1: GameManager::getInstance().setX50(e3, val4 - t1); break;
                case 2: GameManager::getInstance().setX50(e3, val4 * t1); break;
                case 3: GameManager::getInstance().setX50(e3, val4 / t1); break;
                case 4: GameManager::getInstance().setX50(e3, val4 % t1); break;
                case 5: GameManager::getInstance().setX50(e3, (uint16_t)val4 / t1); break;
            }
            break;
        }
        case 4: {
            int t1 = e4;
            if (e1 & 1) t1 = GameManager::getInstance().getX50(e4);
            GameManager::getInstance().setX50(0x7000, 0);
            int val3 = GameManager::getInstance().getX50(e3);
            switch (e2) {
                case 0: if (!(val3 < t1)) GameManager::getInstance().setX50(0x7000, 1); break;
                case 1: if (!(val3 <= t1)) GameManager::getInstance().setX50(0x7000, 1); break;
                case 2: if (!(val3 == t1)) GameManager::getInstance().setX50(0x7000, 1); break;
                case 3: if (!(val3 != t1)) GameManager::getInstance().setX50(0x7000, 1); break;
                case 4: if (!(val3 >= t1)) GameManager::getInstance().setX50(0x7000, 1); break;
                case 5: if (!(val3 > t1)) GameManager::getInstance().setX50(0x7000, 1); break;
                case 6: GameManager::getInstance().setX50(0x7000, 0); break;
                case 7: GameManager::getInstance().setX50(0x7000, 1); break;
            }
            break;
        }
        case 16: {
            int idx3 = e3;
            int idx4 = e4;
            int val = e5;
            if (e1 & 1) idx3 = GameManager::getInstance().getX50(e3);
            if ((e1 >> 1) & 1) idx4 = GameManager::getInstance().getX50(e4);
            if ((e1 >> 2) & 1) val = GameManager::getInstance().getX50(e5);
            switch (e2) {
                case 0: GameManager::getInstance().getRole(idx3).setData(idx4 / 2, val); break;
                case 1: GameManager::getInstance().getItem(idx3).setData(idx4 / 2, val); break;
                case 2: SceneManager::getInstance().GetScene(idx3)->setData(idx4 / 2, val); break;
                case 3: GameManager::getInstance().getMagic(idx3).setData(idx4 / 2, val); break;
            }
            break;
        }
        case 17: {
            int idx3 = e3;
            int idx4 = e4;
            if (e1 & 1) idx3 = GameManager::getInstance().getX50(e3);
            if ((e1 >> 1) & 1) idx4 = GameManager::getInstance().getX50(e4);
            switch (e2) {
                case 0: GameManager::getInstance().setX50(e5, GameManager::getInstance().getRole(idx3).getData(idx4 / 2)); break;
                case 1: GameManager::getInstance().setX50(e5, GameManager::getInstance().getItem(idx3).getData(idx4 / 2)); break;
                case 2: GameManager::getInstance().setX50(e5, SceneManager::getInstance().GetScene(idx3)->getData(idx4 / 2)); break;
                case 3: GameManager::getInstance().setX50(e5, GameManager::getInstance().getMagic(idx3).getData(idx4 / 2)); break;
            }
            break;
        }
        case 18: {
            int slot = e2;
            int roleId = e3;
            if (e1 & 1) slot = GameManager::getInstance().getX50(e2);
            if ((e1 >> 1) & 1) roleId = GameManager::getInstance().getX50(e3);
            GameManager::getInstance().setTeamMember(slot, roleId);
            break;
        }
        case 19: {
            int slot = e2;
            if (e1 & 1) slot = GameManager::getInstance().getX50(e2);
            GameManager::getInstance().setX50(e3, GameManager::getInstance().getTeamMember(slot));
            break;
        }
        case 20: {
            int itemIdx = e2;
            if (e1 & 1) itemIdx = GameManager::getInstance().getX50(e2);
            GameManager::getInstance().setX50(e3, GameManager::getInstance().getItemAmount(itemIdx));
            break;
        }
        case 32: {
            int val3 = e3;
            if (e1 & 1) val3 = GameManager::getInstance().getX50(e3);
            result = 655360 * (val3 + 1) + GameManager::getInstance().getX50(e2);
            break;
        }
        case 35: {
            int keyVal = 0, mouseX = 0, mouseY = 0;
            UIManager::getInstance().WaitAnyKey(&keyVal, &mouseX, &mouseY);
            GameManager::getInstance().setX50(e1, keyVal);
            GameManager::getInstance().setX50(e2, mouseX);
            GameManager::getInstance().setX50(e3, mouseY - 30);
            switch (keyVal) {
                case SDLK_LEFT: GameManager::getInstance().setX50(e1, 154); break;
                case SDLK_RIGHT: GameManager::getInstance().setX50(e1, 156); break;
                case SDLK_UP: GameManager::getInstance().setX50(e1, 158); break;
                case SDLK_DOWN: GameManager::getInstance().setX50(e1, 152); break;
                case SDLK_KP_4: GameManager::getInstance().setX50(e1, 154); break;
                case SDLK_KP_6: GameManager::getInstance().setX50(e1, 156); break;
                case SDLK_KP_8: GameManager::getInstance().setX50(e1, 158); break;
                case SDLK_KP_2: GameManager::getInstance().setX50(e1, 152); break;
            }
            break;
        }
        case 36: {
            int posX = e3;
            int posY = e4;
            int colorIdx = e5;
            if (e1 & 1) posX = GameManager::getInstance().getX50(e3);
            if ((e1 >> 1) & 1) posY = GameManager::getInstance().getX50(e4);
            if ((e1 >> 2) & 1) colorIdx = GameManager::getInstance().getX50(e5);
            
            std::string text = GameManager::getInstance().getX50String(e2);
            std::string textUtf8 = TextManager::getInstance().gbkToUtf8(text);
            
            int width = textUtf8.length() * 10 + 25;
            int height = 27;
            
            uint32_t color1 = GraphicsUtils::getPaletteColor(colorIdx & 0xFF);
            uint32_t color2 = GraphicsUtils::getPaletteColor((colorIdx >> 8) & 0xFF);
            uint32_t frameColor = GraphicsUtils::getPaletteColor(255);
            
            UIManager::getInstance().DrawRectangle(posX, posY, width, height, 0, frameColor, 30);
            UIManager::getInstance().DrawShadowTextUtf8(textUtf8, posX + 8, posY + 3, color1, color2);
            SDL_RenderPresent(UIManager::getInstance().GetRenderer());
            
            int key = UIManager::getInstance().WaitForKeyPress();
            if (key == SDLK_Y) {
                GameManager::getInstance().setX50(0x7000, 0);
            } else {
                GameManager::getInstance().setX50(0x7000, 1);
            }
            break;
        }
        case 24: {
            int gameTime = GameManager::getInstance().getGameTime();
            if (e2 == -22) {
                GameManager::getInstance().setX50(0x7000, 1);
                if (gameTime >= e3) {
                    GameManager::getInstance().setX50(0x7000, 0);
                }
            }
            break;
        }
        case 43: {
            int subFunc = e2;
            int arg3 = e3;
            int arg4 = e4;
            int arg5 = e5;
            int arg6 = e6;
            
            if (e1 & 1) subFunc = GameManager::getInstance().getX50(e2);
            if ((e1 >> 1) & 1) arg3 = GameManager::getInstance().getX50(e3);
            if ((e1 >> 2) & 1) arg4 = GameManager::getInstance().getX50(e4);
            if ((e1 >> 3) & 1) arg5 = GameManager::getInstance().getX50(e5);
            if ((e1 >> 4) & 1) arg6 = GameManager::getInstance().getX50(e6);
            
            GameManager::getInstance().setX50(0x7100, arg3);
            GameManager::getInstance().setX50(0x7101, arg4);
            GameManager::getInstance().setX50(0x7102, arg5);
            GameManager::getInstance().setX50(0x7103, arg6);
            
            std::cout << "[instruct_50e case 43] subFunc=" << subFunc 
                      << " arg3=" << arg3 << " arg4=" << arg4 
                      << " arg5=" << arg5 << " arg6=" << arg6 << std::endl;
            
            if (subFunc == -1) {
                Instruct_JmpScene(arg3, arg4, arg5);
            } else if (subFunc == -2) {
                GameManager::getInstance().setX50(0x7000, 0);
                UIManager::getInstance().ShowDialogue(" （作詩小遊戲暫以自動通過處理）", -1, 0);
                Instruct_Redraw();
            } else if (subFunc == -3) {
                GameManager::getInstance().setX50(0x7000, 1);
                int roleId = arg3;
                int skillIndex = arg4;
                if (GameManager::getInstance().getRole(roleId).getMagic(skillIndex) > 0) {
                    GameManager::getInstance().setX50(0x7000, 0);
                }
                std::cout << "[instruct_50e case 43 -3] role=" << roleId << " skill=" << skillIndex 
                          << " hasSkill=" << (GameManager::getInstance().getX50(0x7000) == 0 ? "yes" : "no") << std::endl;
            } else if (subFunc == -4) {
                GameManager::getInstance().setX50(0x7000, 0);
                UIManager::getInstance().ShowDialogue(" （針灸小遊戲暫以自動通過處理）", -1, 0);
                Instruct_Redraw();
            } else if (subFunc == -5) {
                GameManager::getInstance().setX50(0x7000, 0);
            } else if (subFunc == -6) {
                GameManager::getInstance().setX50(0x7000, 0);
            } else if (subFunc == -7) {
                int randomEvent = GameManager::getInstance().getX50(0x6000);
                if (randomEvent > 0) ExecuteEvent(randomEvent);
            } else if (subFunc == -8) {
                Instruct_Redraw();
            } else if (subFunc == -9) {
                GameManager::getInstance().setX50(0x7000, 0);
            } else if (subFunc == -10) {
                GameManager::getInstance().setX50(0x7000, 0);
                Instruct_Redraw();
            } else if (subFunc == -11) {
                int roleId = arg3;
                int def = arg4;
                Role& role = GameManager::getInstance().getRole(roleId);
                role.setDefence(role.getDefence() + def);
                Instruct_Redraw();
            } else if (subFunc == 540) {
                GameManager::getInstance().setX50(0x7000, 0);
                UIManager::getInstance().ShowDialogue(" （解謎小遊戲暫以自動通過處理）", -1, 0);
            } else if (subFunc == 1055) {
                Instruct_Redraw();
            } else if (subFunc > 0) {
                std::cout << "[instruct_50e case 43] Calling event " << subFunc << std::endl;
                ExecuteEvent(subFunc);
            }
            break;
        }
        default:
            break;
    }
    
    return result;
}

void EventManager::Instruct_Flash(int color, int time) {
    UIManager::getInstance().FlashScreen(color, time);
}

void EventManager::Instruct_Delay(int time) {
    SDL_Delay(time * 20); // KYS unit ~20ms
}

void EventManager::Instruct_Redraw() {
    SDL_Renderer* renderer = UIManager::getInstance().GetRenderer();
    if (!renderer) return;

    int cx, cy;
    GameManager::getInstance().getCameraPosition(cx, cy);

    // Clear background
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    // Draw Scene (updates surface and uploads to texture)
    SceneManager::getInstance().DrawScene(renderer, cx, cy);

    // CRITICAL: Update texture from surface before presenting
    GameManager::getInstance().RenderScreenTo(renderer);

    // Present
    UIManager::getInstance().UpdateScreen();
}

void EventManager::Instruct_UpdateEvent(int sceneId, int eventId, int index, int value) {
   if (sceneId == -2) sceneId = m_executingSceneId; // 使用锁死的上下文
    if (eventId == -2) eventId = m_executingEventId; // 必须处理 eventId 为 -2 的情况
    SceneManager::getInstance().SetEventData(sceneId, eventId, index, value);
}

void EventManager::Instruct_26(int sceneId, int eventId, int add1, int add2, int add3) {
    if (sceneId == -2) sceneId = m_executingSceneId;
    if (eventId == -2) eventId = m_executingEventId;
    
    int16_t val2 = SceneManager::getInstance().GetEventData(sceneId, eventId, 2);
    int16_t val3 = SceneManager::getInstance().GetEventData(sceneId, eventId, 3);
    int16_t val4 = SceneManager::getInstance().GetEventData(sceneId, eventId, 4);
    
    SceneManager::getInstance().SetEventData(sceneId, eventId, 2, val2 + add1);
    SceneManager::getInstance().SetEventData(sceneId, eventId, 3, val3 + add2);
    SceneManager::getInstance().SetEventData(sceneId, eventId, 4, val4 + add3);
}

void EventManager::Instruct_38(int sceneId, int layer, int oldPic, int newPic) {
    if (sceneId == -2) sceneId = m_executingSceneId;
    
    // Iterate over all tiles in layer and replace oldPic with newPic
    // Pascal: Sdata[snum, layernum, i1, i2]
    // Optimization: SceneManager could expose a "ReplaceTile" function
    // But for now, we can iterate.
    for (int x = 0; x < 64; ++x) {
        for (int y = 0; y < 64; ++y) {
            if (SceneManager::getInstance().GetSceneTile(sceneId, layer, x, y) == oldPic) {
                SceneManager::getInstance().SetSceneTile(sceneId, layer, x, y, newPic);
            }
        }
    }
}

void EventManager::PrintEventScript(int eventScriptId) {
    if (eventScriptId <= 0 || eventScriptId > m_eventIndices.size()) {
        std::cerr << "PrintEventScript: Invalid ID " << eventScriptId << ". Max ID: " << m_eventIndices.size() << std::endl;
        return;
    }

    int offset = m_eventIndices[eventScriptId - 1];
    int nextOffset = (eventScriptId < m_eventIndices.size()) ? m_eventIndices[eventScriptId] : m_eventScripts.size() * 2;
    
    int lengthBytes = nextOffset - offset;
    int lengthWords = lengthBytes / 2;
    
    int scriptStart = offset / 2;
    int scriptEnd = scriptStart + lengthWords;
    
    std::cout << "Event " << eventScriptId << " - Start: " << scriptStart << " End: " << scriptEnd << " Length: " << lengthWords << " words" << std::endl;
    std::cout << "Script data:" << std::endl;
    
    for (int i = 0; i < lengthWords && (scriptStart + i) < m_eventScripts.size(); ++i) {
        if (i % 10 == 0) {
            std::cout << std::endl << "[" << (scriptStart + i) << "] ";
        }
        std::cout << m_eventScripts[scriptStart + i] << " ";
    }
    std::cout << std::endl;
}

void EventManager::Instruct_27(int eventId, int beginPic, int endPic) {
    // Animation: Updates DData[e, 5] (Pic) from beginPic to endPic
    // In KYS, this is blocking animation.
    int sceneId = m_executingSceneId;
    if (eventId == -2) eventId = m_executingEventId;
    
    // Store original pic? Pascal logic:
    // oldpic := DData[CurScene, enum, 5];
    // loop...
    // DData[..., 5] := DData[..., 7]; (Reset to default?)
    // Actually, looking at Pascal:
    // DData[CurScene, enum, 5] := picsign * i;
    // ...
    // DData[CurScene, enum, 5] := DData[CurScene, enum, 7];
    
    int step = (beginPic <= endPic) ? 1 : -1;
    for (int i = beginPic; i != endPic + step; i += step) {
        SceneManager::getInstance().SetEventData(sceneId, eventId, 5, i);
        Instruct_Redraw(); // Helper to update screen
        int animDelay = (65 * GameManager::getInstance().getGameSpeed()) / 10;
        SDL_Delay(animDelay);
    }
    
    // Restore to Index 7 (Default Pic)
    int16_t defaultPic = SceneManager::getInstance().GetEventData(sceneId, eventId, 7);
    SceneManager::getInstance().SetEventData(sceneId, eventId, 5, defaultPic);
    Instruct_Redraw();
}

void EventManager::Instruct_23(int roleId, int poison) {
    Role& role = GameManager::getInstance().getRole(roleId);
    role.setUsePoi(poison);
}

void EventManager::Instruct_44(int eventId1, int beginPic1, int endPic1, int eventId2, int beginPic2, int endPic2) {
    // Dual Animation
    int sceneId = m_executingSceneId;
    if (eventId1 == -2) eventId1 = m_executingEventId;
    // eventId2 can also be -2, although rare for dual animation
    if (eventId2 == -2) eventId2 = m_executingEventId;

    int len1 = abs(endPic1 - beginPic1);
    int len2 = abs(endPic2 - beginPic2);
    int len = std::max(len1, len2);
    
    int step1 = (beginPic1 <= endPic1) ? 1 : -1;
    int step2 = (beginPic2 <= endPic2) ? 1 : -1;
    
    for (int i = 0; i <= len; ++i) {
        int pic1 = beginPic1 + (i <= len1 ? i * step1 : len1 * step1);
        int pic2 = beginPic2 + (i <= len2 ? i * step2 : len2 * step2);
        
        SceneManager::getInstance().SetEventData(sceneId, eventId1, 5, pic1);
        SceneManager::getInstance().SetEventData(sceneId, eventId2, 5, pic2);
        Instruct_Redraw();
        SDL_Delay(65);
    }
    
    // Restore
    int16_t def1 = SceneManager::getInstance().GetEventData(sceneId, eventId1, 7);
    int16_t def2 = SceneManager::getInstance().GetEventData(sceneId, eventId2, 7);
    SceneManager::getInstance().SetEventData(sceneId, eventId1, 5, def1);
    SceneManager::getInstance().SetEventData(sceneId, eventId2, 5, def2);
    Instruct_Redraw();
}

void EventManager::Instruct_30(int x1, int y1, int x2, int y2) {
    // Move Main Character (Walk)
    // Needs Pathfinding. For now, simple teleport or straight line?
    // User asked for "Logic to update event layer".
    // This instruction moves the protagonist.
    // Let's implement teleport for now to satisfy movement,
    // but ideally we need the "Walk" function.
    
    int curX, curY;
    GameManager::getInstance().getMainMapPosition(curX, curY);
    int startRow = x1;
    int startCol = y1;
    int targetRow = x2;
    int targetCol = y2;
    if (startRow == -2) startRow = curY;
    if (startCol == -2) startCol = curX;
    if (targetRow == -2) targetRow = curY;
    if (targetCol == -2) targetCol = curX;
    int finalX = targetCol;
    int finalY = targetRow;
    if (finalX >= 0 && finalY >= 0) {
        GameManager::getInstance().setMainMapPosition(finalX, finalY);
        Instruct_Redraw();
    }
}

void EventManager::Instruct_JmpScene(int sceneId, int x, int y) {
    GameManager::getInstance().enterScene(sceneId);

    int targetRow = x;
    int targetCol = y;

    if (x == -2 || y == -2) {
        Scene* scene = SceneManager::getInstance().GetScene(sceneId);
        if (scene) {
            if (x == -2) targetRow = scene->getEntranceY();
            if (y == -2) targetCol = scene->getEntranceX();
        } else {
             std::cerr << "Instruct_JmpScene: Scene " << sceneId << " not found!" << std::endl;
        }
    }

    int finalX = targetCol;
    int finalY = targetRow;
    if (finalX >= 0 && finalY >= 0) {
        GameManager::getInstance().setMainMapPosition(finalX, finalY);
    }
    
    Instruct_Redraw();
    
    SDL_Renderer* renderer = GameManager::getInstance().getRenderer();
    SceneManager::getInstance().DrawScene(renderer, finalX, finalY);
    UIManager::getInstance().ShowSceneName(sceneId);
    GameManager::getInstance().UpdateRoaming();
    CheckEvent(sceneId, finalX, finalY, false);
}

void EventManager::Instruct_Movement(int eventId, int x, int y) {
    // Standard "Move Event" logic
    // Usually updates DData indices 9, 10 and updates SData (Layer 3)
    // Assuming DData structure:
    // Index 9: X, Index 10: Y (Wait, check Pascal struct)
    // Pascal TScene.Address.Data is array[0..25]
    // DData is array[0..199, 0..10]
    // Pascal code: 
    // DData[s, e, 9] is Y? DData[s, e, 10] is X?
    // Let's check kys_main.pas or kys_engine.pas usage.
    // In instruct_27: UpdateScene(DData[..., 10], DData[..., 9], ...)
    // So 10 is X, 9 is Y?
    
    int sceneId = m_executingSceneId;
    if (eventId == -2) eventId = m_executingEventId;

    int oldX = SceneManager::getInstance().GetEventData(sceneId, eventId, 10);
    int oldY = SceneManager::getInstance().GetEventData(sceneId, eventId, 9);
    
    // Update DData
    SceneManager::getInstance().SetEventData(sceneId, eventId, 10, x);
    SceneManager::getInstance().SetEventData(sceneId, eventId, 9, y);
    
    // Update SData (Layer 3)
    SceneManager::getInstance().UpdateEventPosition(sceneId, eventId, oldX, oldY, x, y);
}

void EventManager::Instruct_25(int x1, int y1, int x2, int y2) {
    // Pan Camera
    // x1, x2: Row (Y) -> m_cameraY
    // y1, y2: Col (X) -> m_cameraX
    
    int currentX, currentY;
    GameManager::getInstance().getCameraPosition(currentX, currentY); // Use Camera Position

    if (x1 == -2) x1 = currentY;
    if (y1 == -2) y1 = currentX;

    SDL_Renderer* renderer = GameManager::getInstance().getRenderer();

    // 1. Move Y (Row) from x1 to x2, keeping X (Col) at y1
    int s = (x2 > x1) ? 1 : ((x2 < x1) ? -1 : 0);
    if (s != 0) {
        for (int i = x1 + s; i != x2 + s; i += s) {
            // Consume input events
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_EVENT_QUIT) {
                    GameManager::getInstance().Quit();
                    return;
                }
            }
            SceneManager::getInstance().DrawScene(renderer, y1, i); // Draw at (X, Y)
            GameManager::getInstance().RenderScreenTo(renderer);
            SDL_RenderPresent(renderer);
            int panDelay = (25 * GameManager::getInstance().getGameSpeed()) / 10;
            SDL_Delay(panDelay);
        }
    }

    // 2. Move X (Col) from y1 to y2, keeping Y (Row) at x2
    s = (y2 > y1) ? 1 : ((y2 < y1) ? -1 : 0);
    if (s != 0) {
        for (int i = y1 + s; i != y2 + s; i += s) {
            // Consume input events
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_EVENT_QUIT) {
                    GameManager::getInstance().Quit();
                    return;
                }
            }
            SceneManager::getInstance().DrawScene(renderer, i, x2); // Draw at (X, Y)
            GameManager::getInstance().RenderScreenTo(renderer);
            SDL_RenderPresent(renderer);
            SDL_Delay(25);
        }
    }

    // Update Global Position (Camera Center ONLY)
    // DO NOT update Player Position (m_mainMapX/Y) or Facing
    GameManager::getInstance().setCameraPosition(y2, x2);

    InputManager::getInstance().FlushEvents();
    SDL_Delay(100);
}
