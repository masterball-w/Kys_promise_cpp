#ifndef WARDATA_H
#define WARDATA_H

#include "GameObject.h"
#include <vector>

class WarData : public GameObject {
public:
    WarData();
    virtual ~WarData() = default;

    int16_t getBattleNum() const;
    void setBattleNum(int16_t value);

    // BattleName: array[0..9] of byte;
    // Accessed as bytes, not int16
    std::vector<uint8_t> getBattleName() const;
    
    int16_t getBattleMap() const;
    void setBattleMap(int16_t value);

    int16_t getExp() const;
    void setExp(int16_t value);

    int16_t getBattleMusic() const;
    void setBattleMusic(int16_t value);

    // Arrays are flattened in m_data.
    // Mate: array[0..11] of smallint
    int16_t getMate(int index) const;
    void setMate(int index, int16_t value);

    int16_t getAutoMate(int index) const;
    void setAutoMate(int index, int16_t value);

    int16_t getMateX(int index) const;
    void setMateX(int index, int16_t value);

    int16_t getMateY(int index) const;
    void setMateY(int index, int16_t value);

    // Enemy: array[0..29] of smallint
    int16_t getEnemy(int index) const;
    void setEnemy(int index, int16_t value);

    int16_t getEnemyX(int index) const;
    void setEnemyX(int index, int16_t value);

    int16_t getEnemyY(int index) const;
    void setEnemyY(int index, int16_t value);

    int16_t getGetKongfu(int index) const;
    int16_t getGetItems(int index) const;
    int16_t getGetMoney() const;

    // ... other fields ...

private:
    // Layout based on TWarSta (156 int16s)
    // BattleNum: 0
    // BattleName: 1..5 (10 bytes)
    // BattleMap: 6
    // Exp: 7
    // BattleMusic: 8
    // Mate: 9..20 (12)
    // AutoMate: 21..32 (12)
    // MateX: 33..44 (12)
    // MateY: 45..56 (12)
    // Enemy: 57..86 (30)
    // EnemyX: 87..116 (30)
    // EnemyY: 117..146 (30)
    // BoutEvent: 147
    // OperationEvent: 148
    // GetKongfu: 149..151 (3)
    // GetItems: 152..154 (3)
    // GetMoney: 155

    enum Index {
        IDX_BATTLENUM = 0,
        IDX_BATTLENAME_START = 1, // 5 int16s = 10 bytes
        IDX_BATTLEMAP = 6,
        IDX_EXP = 7,
        IDX_BATTLEMUSIC = 8,
        IDX_MATE_START = 9,
        IDX_AUTOMATE_START = 21,
        IDX_MATEX_START = 33,
        IDX_MATEY_START = 45,
        IDX_ENEMY_START = 57,
        IDX_ENEMYX_START = 87,
        IDX_ENEMYY_START = 117,
        IDX_BOUTEVENT = 147,
        IDX_OPERATIONEVENT = 148,
        IDX_GETKONGFU_START = 149,
        IDX_GETITEMS_START = 152,
        IDX_GETMONEY = 155
    };
};

#endif // WARDATA_H
