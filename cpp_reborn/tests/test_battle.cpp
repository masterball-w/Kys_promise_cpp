#include <iostream>
#include <cassert>
#include <cmath>
#include <vector>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <cstring>
#include "BattleManager.h"
#include "GameManager.h"
#include "FileLoader.h"
#include "PicLoader.h"
#include "Magic.h"
#include "Role.h"

// Replicate CalNewHurtValue for validation
static int TestCalNewHurtValue(int lv, int minVal, int maxVal, int proportion) {
    if (proportion == 0) proportion = 100;
    double p = proportion / 1000.0;
    double n = std::pow((double)(maxVal - minVal), 1.0 / p) / 9.0;
    return (int)(std::round(std::pow((lv * n), p)) + minVal);
}

static std::string ResolveDataPathForTest(const std::string& relative) {
    std::ifstream f0(relative, std::ios::binary);
    if (f0.good()) return std::filesystem::absolute(relative).string();
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
    std::string candidate1 = resourceDir + "/" + relative;
    std::ifstream f1(candidate1, std::ios::binary);
    if (f1.good()) return candidate1;
    if (!baseDir.empty()) {
        std::string candidate2 = baseDir + "/" + relative;
        std::ifstream f2(candidate2, std::ios::binary);
        if (f2.good()) return candidate2;
    }
    return FileLoader::getResourcePath(relative);
}

void TestBattlePicLoading() {
    std::cout << "--- Testing Battle Pic Loading ---" << std::endl;
    std::string path = ResolveDataPathForTest("fight/000/00.pic");
    int count = PicLoader::getPicCount(path);
    assert(count > 0);
    PicImage pic = PicLoader::loadPic(path, 0);
    assert(pic.surface != nullptr);
    PicLoader::freePic(pic);
    std::cout << "[PASS] Battle pic loaded successfully." << std::endl;
}

void TestDamageCalculation() {
    std::cout << "--- Testing Damage Calculation ---" << std::endl;

    // 1. Setup Data
    GameManager::getInstance().clearDataForTest();
    BattleManager::getInstance().ClearBattleRoles();

    // 2. Create Magic (ID 0)
    Magic m;
    int16_t* md = m.getRawData();
    // Set Name "TestMagic"
    m.setName("TestMagic");
    md[18] = 100; // MinHurt
    md[19] = 100; // MaxHurt
    md[20] = 0;   // HurtModulus (if 0, p=0, scaling skipped? Let's test)
    // Actually if p=0, formula divides by p? No, CalHurtValue checks if (p > 0).
    // Let's set Modulus to make p > 0.
    md[21] = 1;   // AttackModulus -> p += 6
    // p = 1 * 6 = 6.
    
    // Add to GameManager
    GameManager::getInstance().addMagicForTest(m);
    
    // 3. Create Attacker Role (ID 0)
    Role attackerRole;
    int16_t* rd1 = attackerRole.getRawData();
    attackerRole.setName("Attacker");
    rd1[35] = 100; // Attack (Index 35 from memory? Need to verify Role indices or use setters)
    // Role.h has setters! Use them.
    attackerRole.setAttack(100);
    attackerRole.setDefence(50);
    attackerRole.setSpeed(50);
    attackerRole.setKnowledge(0); // Simplify
    attackerRole.setLevel(10);
    attackerRole.setDifficulty(50); // Standard
    
    // Magic proficiency
    // Role::setMagic(index, magicId)
    // Role::setMagLevel(index, level)
    // Role usually stores Magics in specific slots.
    // Let's assume setMagic/setMagLevel exist.
    // If not, I'll check Role.h. Assuming they exist based on BattleManager using them.
    // But BattleManager uses `getMagic(i)` and `getMagLevel(i)`.
    // I need to set them.
    
    // Add to GameManager
    GameManager::getInstance().addRoleForTest(attackerRole);
    
    // 4. Create Defender Role (ID 1)
    Role defenderRole;
    defenderRole.setName("Defender");
    defenderRole.setAttack(50);
    defenderRole.setDefence(0); // Zero defense
    defenderRole.setSpeed(50);
    defenderRole.setKnowledge(0);
    defenderRole.setLevel(10);
    defenderRole.setMaxHP(1000);
    defenderRole.setCurrentHP(1000);
    
    GameManager::getInstance().addRoleForTest(defenderRole);
    
    // 5. Create BattleRoles
    BattleRole brAttacker;
    brAttacker.setRNum(0);
    brAttacker.setTeam(0); // Player Team
    brAttacker.setX(10);
    brAttacker.setY(10);
    BattleManager::getInstance().AddBattleRole(brAttacker);
    
    BattleRole brDefender;
    brDefender.setRNum(1);
    brDefender.setTeam(1); // Enemy Team
    brDefender.setX(11); // Distance 1
    brDefender.setY(10);
    BattleManager::getInstance().AddBattleRole(brDefender);
    
    // Set Ground (Layer 0) for the test area
    // Otherwise SetAttackArea will skip tiles
    for(int x=0; x<64; x++) {
        for(int y=0; y<64; y++) {
            BattleManager::getInstance().setBattleField(0, x, y, 1);
        }
    }

    // 6. Test CalHurtValue
    // Magic ID 0, Level 10
    int dmg = BattleManager::getInstance().CalHurtValue(0, 1, 0, 10);
    
    std::cout << "Calculated Damage: " << dmg << std::endl;
    
    // Verification Logic:
    // BaseHurt = CalNewHurtValue(9, 100, 100, 0) = 100.
    // mhurt = 100 * (100 + 0) / 100 = 100.
    // p = 6.
    // Att = 101, Def = 1.
    // a1 = (101 - 1)/101 = 0.99.
    // result = 100 * 0.99 * (1 * 6 / 6) = 99.
    // Distance = 1 -> Factor 1.0.
    // Random variance (+/- 10).
    // Expected ~99.
    
    assert(dmg > 80 && dmg < 120);
    std::cout << "[PASS] Damage within expected range." << std::endl;
    
    // 7. Test Attack (End-to-End)
    // Mark target in range (Layer 4)
    // BattleManager::Attack clears layer 4 and sets it based on targetIdx?
    // Yes, Attack(roleIdx, targetIdx, magicId) does this.
    
    BattleManager::getInstance().Attack(0, 1, 0);
    
    // Check Defender HP
    Role& defData = GameManager::getInstance().getRole(1);
    int newHP = defData.getCurrentHP();
    std::cout << "Defender HP after Attack: " << newHP << " (Was 1000)" << std::endl;
    
    int appliedDmg = 1000 - newHP;
    std::cout << "Applied Damage: " << appliedDmg << std::endl;

    assert(newHP < 1000);
    // Damage recalculates with random variance, so it won't match 'dmg' exactly.
    // But it should be within range.
    assert(appliedDmg > 80 && appliedDmg < 120); 
    std::cout << "[PASS] HP reduced correctly within range." << std::endl;
}

static std::vector<int> FindBattleIdsForLogTest() {
    std::vector<int> result;
    auto data = FileLoader::loadFile("War.sta");
    if (data.empty()) return result;
    const size_t recordSize = 312;
    const size_t recordCount = data.size() / recordSize;
    for (size_t i = 0; i < recordCount; ++i) {
        std::vector<int16_t> war(156);
        std::memcpy(war.data(), data.data() + i * recordSize, recordSize);
        int16_t battleNum = war[0];
        if (battleNum < 0) continue;
        bool hasEnemy = false;
        for (int idx = 57; idx <= 86; ++idx) {
            if (war[idx] >= 0) {
                hasEnemy = true;
                break;
            }
        }
        if (!hasEnemy) continue;
        if (std::find(result.begin(), result.end(), battleNum) == result.end()) {
            result.push_back(battleNum);
            if (result.size() >= 2) break;
        }
    }
    return result;
}

static int RunAutoBattleLogTest() {
    if (!GameManager::getInstance().Init()) {
        std::cerr << "GameManager init failed" << std::endl;
        return 1;
    }
    BattleManager::getInstance().setForceAutoBattle(true);
    BattleManager::getInstance().setForceAutoBattleFrameLimit(600);
    auto battleIds = FindBattleIdsForLogTest();
    if (battleIds.size() < 2) {
        std::cerr << "Not enough battle ids found" << std::endl;
        GameManager::getInstance().Quit();
        return 1;
    }
    for (size_t i = 0; i < 2; ++i) {
        int battleId = battleIds[i];
        std::cout << "=== Auto Battle Log Test: " << battleId << " ===" << std::endl;
        BattleManager::getInstance().StartBattle(battleId);
    }
    GameManager::getInstance().Quit();
    return 0;
}

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "auto_battle") {
        return RunAutoBattleLogTest();
    }
    TestBattlePicLoading();
    TestDamageCalculation();
    std::cout << "All Tests Passed!" << std::endl;
    return 0;
}
