#ifndef BATTLEMANAGER_H
#define BATTLEMANAGER_H

#include <vector>
#include <cstdint>
#include <string>
#include "BattleRole.h"
#include "WarData.h"

class BattleManager {
public:
    static BattleManager& getInstance();

    // Initialize battle system
    bool Init();

    // Start a specific battle
    bool StartBattle(int battleId, int getExp = 0);

    // Main battle loop (blocking)
    void RunBattle();

    // Reset battle state
    void ResetBattle();

    // Load battle field map
    bool LoadBattleField(int fieldNum);

    // Accessors
    BattleRole& getBattleRole(int index);
    int getBattleRoleCount() const;
    int16_t getBattleField(int layer, int x, int y) const;
    void setBattleField(int layer, int x, int y, int16_t val); // Added for testing
    
    // Testing Helper
    void AddBattleRole(const BattleRole& role);
    void ClearBattleRoles();

    void MoveRole(int roleIdx, int x, int y);
    void Attack(int roleIdx, int targetIdx, int magicId);
    void AttackAt(int roleIdx, int targetX, int targetY, int magicId);
    
    // BFS Movement Range
    // Fills m_battleField[3] with step counts (-1 if unreachable)
    void CalSelectableArea(int roleIdx);
    void CalSelectableAreaEx(int roleIdx, int myTeam, int mode);
    bool IsWaterTile(int earthNum) const;

    // Damage Calculation
    int CalHurtValue(int attackerIdx, int targetIdx, int magicId, int level);

    // Battle Menu
    int BattleMenu(int roleIdx);
    void BattleMenuItem(int roleIdx);

    // Target Selection
    bool SelectFriendlyRole(int roleIdx);
    void SelectAuto(int roleIdx);
    bool SelectAim(int roleIdx, int step);

    // Battle status for instruct_50e
    int GetBattleResult() const { return m_battleResult; }
    void SetBattleResult(int v) { m_battleResult = v; }
    bool IsBattleRunning() const { return m_battleRunning; }
    int getCurrentRoleIndex() const { return m_currentRoleIndex; }
    int getCursorX() const { return m_cursorX; }
    int getCursorY() const { return m_cursorY; }
    int getMaxRound() const;
    
    // New Selection Helpers
    bool SelectMove(int roleIdx, int& outX, int& outY);
    bool SelectAttack(int roleIdx, int& outTargetIdx);
    bool SelectMagic(int roleIdx, int& outMagicId);
    bool SelectMagicTarget(int roleIdx, int magicId, int& outX, int& outY);

    // Visual Effects
    void ShowHurtValue(int mode); // 0:Red, 1:Purple, 2:Green, 3:Blue, 4:Cyan
    void ShowHurtValue(const std::string& str, uint32_t color1, uint32_t color2);
    void PlayMagicAmination(int bnum, int magicId, int level, int targetX, int targetY);
    void PlayActionAmination(int bnum, int mode, int targetX, int targetY);
    
    // Actions
    void Medcine(int roleIdx);
    void MedFrozen(int roleIdx);
    void MedPoision(int roleIdx);
    void UsePoision(int roleIdx);
    void UseHiddenWeapen(int roleIdx, int itemIdx);
    void AutoUseItem(int roleIdx, int listType); // listType: 45=HP, 50=MP, 48=Phy
    
    // Logic Helpers (Public for now, or friend)
    void ApplyMedicine(int healerIdx, int targetIdx);
    void ApplyMedFrozen(int healerIdx, int targetIdx);
    void ApplyMedPoision(int healerIdx, int targetIdx);
    void ApplyUsePoision(int attackerIdx, int targetIdx);
    void ApplyHiddenWeapon(int attackerIdx, int targetIdx, int itemIdx);
    
    // Area Logic
    void SetAttackArea(int type, int x, int y, int range, int bx, int by, int step);

    // Core Logic
    void CalHurtRole(int attackerIdx, int magicId, int level);
    void CalPoiHurtLife(int roleIdx);
    void ApplyGongtiStackAttack(int roleIdx);   // State 26 at turn start
    void ApplyGongtiAuraPoison(int roleIdx);    // State 27 at action end
    void ClearGongtiStackAttack();              // State 26 cleanup after battle
    void setForceAutoBattle(bool enabled);
    void setForceAutoBattleFrameLimit(int maxFrames);
    bool GetPetSkill(int petIndex, int skillIndex) const;

private:
    BattleManager();
    ~BattleManager() = default;
    BattleManager(const BattleManager&) = delete;
    BattleManager& operator=(const BattleManager&) = delete;

    int CountProgress();
    void CalMoveAbility();
    void ReArrangeBRole();
    void ShowProgress();
    void UpdateMaxSpeed();
    void RunTurnBasedBattle();
    void RunAtbBattle();
    bool ProcessActorTurn(int actorIdx);
    bool CheckBattleEnd();
    void PollBattleInput();
    void DeductActionProgress(BattleRole& actor);
    void UnlockOneWaiter(int exceptIdx);
    void ClearDeadRolePic();
    void MoveAnimation(int roleIdx, int targetX, int targetY);
    void AddExp();
    void CheckLevelUp();
    void RestoreRoleStatus();
    void AutoBattle(int roleIdx);
    int SelectAutoMode();
    int SelectAutoTarget(int roleIdx);
    void CheckBook();
    void PetEffect();
    void ShowPetEffectMessage(const std::string& text);

    // UI Helpers
    void ShowBMenu(int menuStatus, int menu, int max);
    void RenderBattle(); // Draws map and roles
    void PauseShowActorStatus(int roleIdx, int delayMs = 500);
    void ShowItemMenu(const std::vector<int>& itemIds, int current, int x, int y);
    void ApplyItemEffect(int rnum, int inum, int where = 0);
    // void ApplyMedicine(int healerRoleIdx, int targetRoleIdx); // Moved to public

    // Selection Helpers
    bool TeamModeMenu();
    void ShowModeMenu(int menu);
    void ShowTeamModeMenu(int menu);
    void DrawBFieldWithCursor(int attAreaType, int step, int range);
    void PlayEffectAmination(int bigami, int amiNum, int targetX, int targetY);
    std::vector<int> SelectTeamMembers(const std::vector<int>& candidates);
    int ReMoveHurt(int targetIdx, int attackerIdx);
    int RetortHurt(int targetIdx, int attackerIdx);

    // Load War.sta data for a specific battle
    bool LoadWarData(int battleId);

    // Battle Roles (BRole array)
    std::vector<BattleRole> m_battleRoles;
    
    // Battle Field Map (BField array)
    // 8 layers, 64x64 grid
    // Layers: 0:Ground, 1:Building, 2:Roles, 3:Selectable?, 4:AttackRange?, etc.
    int16_t m_battleField[8][64][64];

    // Current War Data
    WarData m_warData;

    // Helper to read War.sta file
    std::string m_warStaPath;
    std::string m_warFldIdxPath;
    std::string m_warFldGrpPath;

    // Runtime State
    bool m_battleRunning;
    int m_battleResult; // 0: Ongoing/Draw, 1: Win, 2: Lose
    int m_currentRoleIndex;
    int m_getExp; // Exp flag from instruction
    int m_cursorX;
    int m_cursorY;
    bool m_showMoveRange;
    bool m_showAttackRange;
    int m_highlightRoleIndex;
    bool m_forceAutoBattle;
    int m_forceAutoBattleFrameLimit;
    int m_forceAutoBattleFrameCount;
    bool m_exitAutoRequested; // Flag to exit auto battle mode
    int m_actionAnimRoleIndex; // Skip idle sprite while playing action animation
    int m_maxSpeed = 1;
    int m_lastActorIdx = -1;
};

#endif // BATTLEMANAGER_H
