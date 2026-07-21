#pragma once
#include "GameObject.h"
#include <string>

constexpr size_t MAGIC_DATA_SIZE = 111;

class Magic : public GameObject {
public:
    Magic() : GameObject(MAGIC_DATA_SIZE) {}
    virtual ~Magic() = default;

    using GameObject::getData;
    using GameObject::setData;

    // 0: ListNum
    int16 getListNum() const { return m_data[0]; }

    // 1-5: Name (10 bytes = 5 words)
    std::string getName() const { return getString(1, 5); }
    void setName(const std::string& v) { setString(1, 5, v); }

    // ... other properties ...
    
    // 6: Useless
    int16 getUseless() const { return m_data[6]; }

    // 7: NeedHP
    int16 getNeedHP() const { return m_data[7]; }

    // 8: MinStep
    int16 getMinStep() const { return m_data[8]; }

    // 9: BigAmi
    int16 getBigAmi() const { return m_data[9]; }

    // 10: EventNum
    int16 getEventNum() const { return m_data[10]; }
    
    // 11: SoundNum
    int16 getSoundNum() const { return m_data[11]; }

    // 12: MagicType
    int16 getMagicType() const { return m_data[12]; }

    // 13: AmiNum (Ambit?)
    int16 getAmiNum() const { return m_data[13]; }

    // 14: HurtType
    int16 getHurtType() const { return m_data[14]; }

    // 15: AttAreaType
    int16 getAttAreaType() const { return m_data[15]; }

    // 16: NeedMP
    int16 getNeedMP() const { return m_data[16]; }

    // 17: Poision
    int16 getPoision() const { return m_data[17]; }

    // 18: MinHurt
    int16 getMinHurt() const { return m_data[18]; }

    // 19: MaxHurt
    int16 getMaxHurt() const { return m_data[19]; }

    // 20: HurtModulus
    int16 getHurtModulus() const { return m_data[20]; }

    // 21: AttackModulus
    int16 getAttackModulus() const { return m_data[21]; }

    // 22: MPModulus
    int16 getMPModulus() const { return m_data[22]; }

    // 23: SpeedModulus
    int16 getSpeedModulus() const { return m_data[23]; }

    // 24: WeaponModulus
    int16 getWeaponModulus() const { return m_data[24]; }

    // 25: NeedProgress
    int16 getNeedProgress() const { return m_data[25]; }

    // 26: AddMpScale
    int16 getAddMpScale() const { return m_data[26]; }

    // 27: AddHpScale
    int16 getAddHpScale() const { return m_data[27]; }

    // 28-37: MoveDistance (array[0..9])
    int16 getMoveDistance(int level) const { return (level >= 0 && level < 10) ? m_data[28 + level] : 0; }

    // 38-47: AttDistance (array[0..9])
    int16 getAttDistance(int level) const { return (level >= 0 && level < 10) ? m_data[38 + level] : 0; }
    
    // 48-50: AddHP (array[0..2])
    int16 getAddHP(int index) const { return (index >= 0 && index < 3) ? m_data[48 + index] : 0; }

    // 51-53: AddMP (array[0..2])
    int16 getAddMP(int index) const { return (index >= 0 && index < 3) ? m_data[51 + index] : 0; }

    // 54-56: AddAtt (array[0..2])
    int16 getAddAtt(int index) const { return (index >= 0 && index < 3) ? m_data[54 + index] : 0; }

    // 57-59: AddDef (array[0..2])
    int16 getAddDef(int index) const { return (index >= 0 && index < 3) ? m_data[57 + index] : 0; }

    // 60-62: AddSpd (array[0..2])
    int16 getAddSpd(int index) const { return (index >= 0 && index < 3) ? m_data[60 + index] : 0; }
    
    int16 getAddMedcine() const { return m_data[67]; }
    int16 getAddUsePoi() const { return m_data[68]; }
    int16 getAddMedPoi() const { return m_data[69]; }
    int16 getAddDefPoi() const { return m_data[70]; }

    // 63-66: Peg / Injury (封穴 / 内伤)
    int16 getMinPeg() const { return m_data[63]; }
    int16 getMaxPeg() const { return m_data[64]; }
    int16 getMinInjury() const { return m_data[65]; }
    int16 getMaxInjury() const { return m_data[66]; }

    // 77-79: NeedExp[0..2] (功体等阶需求)
    int16 getNeedExp(int index) const { return (index >= 0 && index < 3) ? m_data[77 + index] : 0; }
    
    // 71: AddFist
    int16 getAddFist() const { return m_data[71]; }

    // 72: AddSword
    int16 getAddSword() const { return m_data[72]; }

    // 73: AddKnife
    int16 getAddKnife() const { return m_data[73]; }

    // 74: AddUnusual
    int16 getAddUnusual() const { return m_data[74]; }

    // 75: AddHidWeapon
    int16 getAddHidWeapon() const { return m_data[75]; }

    // 76: BattleState
    int16 getBattleState() const { return m_data[76]; }
    void setBattleState(int16 v) { m_data[76] = v; }

    // 80: MaxLevel
    int16 getMaxLevel() const { return m_data[80]; }

    // 81-110: Introduction
    std::string getIntroduction() const { return getString(81, 30); }
};
