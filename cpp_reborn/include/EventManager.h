#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include "GameTypes.h"

class EventManager {
public:
    static EventManager& getInstance();

    // Initialize Event System
    bool Init();
    
    // Exposed for testing
    bool LoadScripts();
    bool LoadDialogues();

    void Instruct_JmpScene(int sceneId, int x, int y); // instruct_71 (Public for testing)
    void Instruct_GameOver(); // instruct_62
    int Instruct_50e(int code, int e1, int e2, int e3, int e4, int e5, int e6); // Extended instruction

    // Event Handling
    // Check if there is an event at (x, y) in sceneId
    // isManual: true if triggered by Space/Enter, false if triggered by stepping on it
    void CheckEvent(int sceneId, int x, int y, bool isManual = false);
    
    // Check for item-triggered event (DData index 3)
    void CheckEventWithItem(int sceneId, int x, int y);

    // Check for Auto-Run events (Condition == 0) in the scene
    void CheckAutoEvents(int sceneId);

    // Execute a specific event script by ID
    void ExecuteEvent(int eventScriptId);
    void SetExecutionContext(int sceneId, int eventId);
    void Instruct_FadeIn();  // instruct_13 — also used by new-game intro
    void Instruct_FadeOut(); // instruct_14
    void Instruct_ShowTitle(int talkNum, int color); // instruct_70 / StartAmi


    // Testing Helper
    void AddMockScript(int id, const std::vector<int16_t>& script);
    
    // Debug Helper: Print event script data
    void PrintEventScript(int eventScriptId);

    // Queue mechanism to prevent recursive event execution issues
    void QueueEvent(int scriptId) { m_pendingScriptId = scriptId; }
    int GetPendingEvent() const { return m_pendingScriptId; }
    void ClearPendingEvent() { m_pendingScriptId = -1; }

private:
    int m_pendingScriptId = -1;
 // 新增：锁死当前执行脚本的场景和事件上下文
    int m_executingSceneId = -1;
    int m_executingEventId = -1;
    EventManager();
    ~EventManager() = default;
    EventManager(const EventManager&) = delete;
    EventManager& operator=(const EventManager&) = delete;

    // Helpers
    int16_t ReadScriptArg(int& offset);

    void Instruct_Redraw();
    void Instruct_Dialogue(int talkId, int headId, int mode); // instruct_1
    void Instruct_AddItem(int itemId, int amount); // instruct_2
    void Instruct_ModifyEvent(const std::vector<int16_t>& args); // instruct_3
    int Instruct_Battle(int battleId, int jump1, int jump2, int getExp); // instruct_6
    void Instruct_PlayMusic(int musicId); // instruct_8
    void Instruct_JoinParty(int roleId); // instruct_10
    // 60-67
    void Instruct_Rest(); // instruct_12 (Also instruct_64 sometimes?)
    int Instruct_AskRest(int jump1, int jump2); // instruct_11 - Ask for rest
    int Instruct_CheckMoney(int moneyNeeded, int jump1, int jump2); // instruct_31 - Check money
    void Instruct_LeaveParty(int roleId); // instruct_21
    void Instruct_SetScene(int sceneId, int x, int y, int dir);
    void Instruct_19(int x, int y); // Teleport within scene
    void Instruct_40(int dir); // Set Facing
    
    // New Instructions
    void Instruct_AddAttribute(int roleId, int attrId, int value); // instruct_11
    void Instruct_PlaySound(int soundId); // instruct_50 / 67

    // New Instructions from KYS Promise
    void Instruct_NewTalk0(int headNum, int talkNum, int nameNum, int place, int showHead, int color, int frame); // instruct_68
    void Instruct_ReSetName(int type, int id, int newNameId); // instruct_69
    // void Instruct_JmpScene(int sceneId, int x, int y); // instruct_71 (Moved to public)
    void Instruct_Flash(int color, int time); // (legacy helper)
    void Instruct_Delay(int time); // helper delay (not opcode 17)
    void Instruct_15(); // fail -> title
    void Instruct_17(int snum, int layer, int x, int y, int value); // set SData tile
    void Instruct_51();
    void Instruct_52();
    void Instruct_53();
    void Instruct_54();
    int Instruct_55(int enum_, int value, int jump1, int jump2);
    void Instruct_56(int repute);
    bool Instruct_58(); // Huashan battles; false if failed to title
    void Instruct_59();
    int Instruct_60(int snum, int enum_, int pic, int jump1, int jump2);
    void Instruct_63(int rnum, int sexual);
    void Instruct_64(); // 韦小宝商店 → ShowShop(0)；原版 Pascal 为空实现
    void HandleInstruct43Sub(int subFunc, int arg3, int arg4, int arg5, int arg6); // instruct_43 / 50e·43 共用
    void Instruct_Puzzle(); // instruct_43 sub 540 — 场景推块解谜（非小游戏 UI）
    void Instruct_66(int musicnum);
    void Instruct_67(int soundnum);

    int GetExecutingSceneId() const { return m_executingSceneId; }
    int GetExecutingEventId() const { return m_executingEventId; }

    // Magic / attribute / inventory helpers (kys_event.pas)
    void StudyMagic(int rnum, int magicnum, int newmagicnum, int level, int dismode); // used by instruct_33
    void Instruct_33(int rnum, int magicnum, int dismode);
    void Instruct_34(int rnum, int iq);
    void Instruct_35(int rnum, int magiclistnum, int magicnum, int exp);
    void Instruct_37(int ethics);
    void Instruct_41(int rnum, int inum, int amount);
    void Instruct_22();
    void Instruct_45(int rnum, int speed);
    void Instruct_46(int rnum, int mp);
    void Instruct_47(int rnum, int attack);
    void Instruct_48(int rnum, int hp);
    void Instruct_49(int rnum, int mpPro);
    void ShowAttributeChangeTip(const std::string& roleNameGbk, const std::string& labelUtf8, int delta);
    int GetMagicLevel(int person, int mnum); // -1 if not learned
    
    // Core DData/SData Modifiers
    void Instruct_UpdateEvent(int sceneId, int eventId, int index, int value); // Helper for 26, 38
    void Instruct_26(int sceneId, int eventId, int add1, int add2, int add3); // Modify DData[s, e, 2/3/4] (Wait, etc?)
    void Instruct_38(int sceneId, int layer, int oldPic, int newPic); // Global Replace SData
    void Instruct_27(int eventId, int beginPic, int endPic); // Animation (updates DData[e, 5])
    void Instruct_44(int eventId1, int beginPic1, int endPic1, int eventId2, int beginPic2, int endPic2); // Dual Animation
    void Instruct_ModifyDData(int sceneId, int eventId, int index, int value); // Generic DData modifier
    
    // Movement
    void Instruct_23(int roleId, int poison);
    void Instruct_25(int x1, int y1, int x2, int y2); // Pan Camera
    void Instruct_30(int x1, int y1, int x2, int y2); // Move Main Character
    void Instruct_Movement(int eventId, int x, int y); // Move Event (Not standard instruction but needed for logic)

    // Helper to read arguments from script
    // Returns the value and increments offset
    // int16_t ReadScriptArg(int& offset); // Moved to public

    // Data
    std::vector<int16_t> m_eventScripts; // kdef.grp (viewed as int16)
    std::vector<int32_t> m_eventIndices; // kdef.idx

    std::vector<uint8_t> m_talkData; // talk.grp
    std::vector<int32_t> m_talkIndices; // talk.idx
    
    // Name Data
    std::vector<uint8_t> m_nameData; // name.grp
    std::vector<int32_t> m_nameIndices; // name.idx
    
    // Helper to load name
    std::string GetNameFromData(int nameNum);

    // Context
    int m_currentSceneId = -1;
    int m_currentEventId = -1;
};
