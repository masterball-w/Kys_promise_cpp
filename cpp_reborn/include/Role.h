#pragma once
#include "GameObject.h"
#include <string>

// Mapping TRole from kys_main.pas
// Size: 91 smallints
constexpr size_t ROLE_DATA_SIZE = 91;

class Role : public GameObject {
public:
    Role() : GameObject(ROLE_DATA_SIZE) {}
    virtual ~Role() = default;

    using GameObject::getData;
    using GameObject::setData;

    // --- Property Accessors based on Pascal Record Layout ---
    // Note: Offsets are in 16-bit words (int16)

    // 0: ListNum
    int16 getListNum() const { return m_data[0]; }
    void setListNum(int16 v) { m_data[0] = v; }

    // 1: HeadNum
    int16 getHeadNum() const { return m_data[1]; }
    void setHeadNum(int16 v) { m_data[1] = v; }

    // 2: IncLife
    int16 getIncLife() const { return m_data[2]; }
    void setIncLife(int16 v) { m_data[2] = v; }
    
    // 3: UnUse
    int16 getUnUse() const { return m_data[3]; }
    void setUnUse(int16 v) { m_data[3] = v; }

    // 4-8: Name (10 bytes = 5 int16s)
    std::string getName() const { return getString(4, 5); }
    void setName(const std::string& v) { setString(4, 5, v); }

    // 9-13: Nick (10 bytes = 5 int16s)
    std::string getNick() const { return getString(9, 5); }
    void setNick(const std::string& v) { setString(9, 5, v); }

    // 14: Sexual
    int16 getSexual() const { return m_data[14]; }
    void setSexual(int16 v) { m_data[14] = v; }

    // 15: Level
    int16 getLevel() const { return m_data[15]; }
    void setLevel(int16 v) { m_data[15] = v; }

    // 16: Exp (uint16 in Pascal)
    uint16 getExp() const { return static_cast<uint16>(m_data[16]); }
    void setExp(uint16 v) { m_data[16] = static_cast<int16>(v); }

    // 17: CurrentHP
    int16 getCurrentHP() const { return m_data[17]; }
    void setCurrentHP(int16 v) { m_data[17] = v; }

    // 18: MaxHP
    int16 getMaxHP() const { return m_data[18]; }
    void setMaxHP(int16 v) { m_data[18] = v; }

    // 19: Hurt
    int16 getHurt() const { return m_data[19]; }
    void setHurt(int16 v) { m_data[19] = v; }

    // 20: Poision
    int16 getPoision() const { return m_data[20]; }
    void setPoision(int16 v) { m_data[20] = v; }

    // 21: PhyPower
    int16 getPhyPower() const { return m_data[21]; }
    void setPhyPower(int16 v) { m_data[21] = v; }

    // 22: ExpForItem (uint16)
    uint16 getExpForItem() const { return static_cast<uint16>(m_data[22]); }
    void setExpForItem(uint16 v) { m_data[22] = static_cast<int16>(v); }

    // 23-27: Equip (array[0..4])
    int16 getEquip(int index) const { return (index >= 0 && index < 5) ? m_data[23 + index] : 0; }
    void setEquip(int index, int16 v) { if (index >= 0 && index < 5) m_data[23 + index] = v; }

    // 28: Gongti
    int16 getGongti() const { return m_data[28]; }
    void setGongti(int16 v) { m_data[28] = v; }

    // 29: TeamState
    int16 getTeamState() const { return m_data[29]; }
    void setTeamState(int16 v) { m_data[29] = v; }

    // 30: Angry
    int16 getAngry() const { return m_data[30]; }
    void setAngry(int16 v) { m_data[30] = v; }

    // 31: GongtiExam (uint16)
    uint16 getGongtiExam() const { return static_cast<uint16>(m_data[31]); }
    void setGongtiExam(uint16 v) { m_data[31] = static_cast<int16>(v); }

    // 32: Moveable
    int16 getMoveable() const { return m_data[32]; }
    void setMoveable(int16 v) { m_data[32] = v; }

    // 33: AddSkillPoint
    int16 getAddSkillPoint() const { return m_data[33]; }
    void setAddSkillPoint(int16 v) { m_data[33] = v; }

    // 34: PetAmount
    int16 getPetAmount() const { return m_data[34]; }
    void setPetAmount(int16 v) { m_data[34] = v; }

    // 35: Impression
    int16 getImpression() const { return m_data[35]; }
    void setImpression(int16 v) { m_data[35] = v; }

    // 36: Reset
    int16 getReset() const { return m_data[36]; }
    void setReset(int16 v) { m_data[36] = v; }

    // 37: difficulty
    int16 getDifficulty() const { return m_data[37]; }
    void setDifficulty(int16 v) { m_data[37] = v; }

    // 38-39: SoundDealy (array[0..1])
    int16 getSoundDealy(int index) const { return (index >= 0 && index < 2) ? m_data[38 + index] : 0; }
    void setSoundDealy(int index, int16 v) { if (index >= 0 && index < 2) m_data[38 + index] = v; }

    // 40: MPType
    int16 getMPType() const { return m_data[40]; }
    void setMPType(int16 v) { m_data[40] = v; }

    // 41: CurrentMP
    int16 getCurrentMP() const { return m_data[41]; }
    void setCurrentMP(int16 v) { m_data[41] = v; }

    // 42: MaxMP
    int16 getMaxMP() const { return m_data[42]; }
    void setMaxMP(int16 v) { m_data[42] = v; }

    // 43: Attack
    int16 getAttack() const { return m_data[43]; }
    void setAttack(int16 v) { m_data[43] = v; }

    // 44: Speed
    int16 getSpeed() const { return m_data[44]; }
    void setSpeed(int16 v) { m_data[44] = v; }

    // 45: Defence
    int16 getDefence() const { return m_data[45]; }
    void setDefence(int16 v) { m_data[45] = v; }

    // 46: Medcine
    int16 getMedcine() const { return m_data[46]; }
    void setMedcine(int16 v) { m_data[46] = v; }

    // 47: UsePoi
    int16 getUsePoi() const { return m_data[47]; }
    void setUsePoi(int16 v) { m_data[47] = v; }

    // 48: MedPoi
    int16 getMedPoi() const { return m_data[48]; }
    void setMedPoi(int16 v) { m_data[48] = v; }

    // 49: DefPoi
    int16 getDefPoi() const { return m_data[49]; }
    void setDefPoi(int16 v) { m_data[49] = v; }

    // 50: Fist
    int16 getFist() const { return m_data[50]; }
    void setFist(int16 v) { m_data[50] = v; }

    // 51: Sword
    int16 getSword() const { return m_data[51]; }
    void setSword(int16 v) { m_data[51] = v; }

    // 52: Knife
    int16 getKnife() const { return m_data[52]; }
    void setKnife(int16 v) { m_data[52] = v; }

    // 53: Unusual
    int16 getUnusual() const { return m_data[53]; }
    void setUnusual(int16 v) { m_data[53] = v; }

    // 54: HidWeapon
    int16 getHidWeapon() const { return m_data[54]; }
    void setHidWeapon(int16 v) { m_data[54] = v; }

    // 55: Knowledge
    int16 getKnowledge() const { return m_data[55]; }
    void setKnowledge(int16 v) { m_data[55] = v; }

    // 56: Ethics
    int16 getEthics() const { return m_data[56]; }
    void setEthics(int16 v) { m_data[56] = v; }

    // 57: AttPoi
    int16 getAttPoi() const { return m_data[57]; }
    void setAttPoi(int16 v) { m_data[57] = v; }

    // 58: AttTwice
    int16 getAttTwice() const { return m_data[58]; }
    void setAttTwice(int16 v) { m_data[58] = v; }

    // 59: Repute
    int16 getRepute() const { return m_data[59]; }
    void setRepute(int16 v) { m_data[59] = v; }

    // 60: Aptitude
    int16 getAptitude() const { return m_data[60]; }
    void setAptitude(int16 v) { m_data[60] = v; }

    // 61: PracticeBook
    int16 getPracticeBook() const { return m_data[61]; }
    void setPracticeBook(int16 v) { m_data[61] = v; }

    // 62: ExpForBook (uint16)
    uint16 getExpForBook() const { return static_cast<uint16>(m_data[62]); }
    void setExpForBook(uint16 v) { m_data[62] = static_cast<int16>(v); }

    // 63-72: Magic (array[0..9])
    int16 getMagic(int index) const { return (index >= 0 && index < 10) ? m_data[63 + index] : 0; }
    void setMagic(int index, int16 v) { if (index >= 0 && index < 10) m_data[63 + index] = v; }

    // 73-82: MagLevel (array[0..9])
    int16 getMagLevel(int index) const { return (index >= 0 && index < 10) ? m_data[73 + index] : 0; }
    void setMagLevel(int index, int16 v) { if (index >= 0 && index < 10) m_data[73 + index] = v; }

    // 83-86: TakingItem (array[0..3])
    int16 getTakingItem(int index) const { return (index >= 0 && index < 4) ? m_data[83 + index] : 0; }
    void setTakingItem(int index, int16 v) { if (index >= 0 && index < 4) m_data[83 + index] = v; }

    // 87-90: TakingItemAmount (array[0..3])
    int16 getTakingItemAmount(int index) const { return (index >= 0 && index < 4) ? m_data[87 + index] : 0; }
    void setTakingItemAmount(int index, int16 v) { if (index >= 0 && index < 4) m_data[87 + index] = v; }
    
};
