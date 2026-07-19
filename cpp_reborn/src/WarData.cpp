#include "WarData.h"

WarData::WarData() : GameObject(156) {}

int16_t WarData::getBattleNum() const { return getData(IDX_BATTLENUM); }
void WarData::setBattleNum(int16_t value) { setData(IDX_BATTLENUM, value); }

std::vector<uint8_t> WarData::getBattleName() const {
    std::vector<uint8_t> name(10);
    for (int i = 0; i < 5; ++i) {
        int16_t val = getData(IDX_BATTLENAME_START + i);
        name[i * 2] = val & 0xFF;
        name[i * 2 + 1] = (val >> 8) & 0xFF;
    }
    return name;
}

int16_t WarData::getBattleMap() const { return getData(IDX_BATTLEMAP); }
void WarData::setBattleMap(int16_t value) { setData(IDX_BATTLEMAP, value); }

int16_t WarData::getExp() const { return getData(IDX_EXP); }
void WarData::setExp(int16_t value) { setData(IDX_EXP, value); }

int16_t WarData::getBattleMusic() const { return getData(IDX_BATTLEMUSIC); }
void WarData::setBattleMusic(int16_t value) { setData(IDX_BATTLEMUSIC, value); }

int16_t WarData::getMate(int index) const {
    if (index < 0 || index >= 12) return 0;
    return getData(IDX_MATE_START + index);
}
void WarData::setMate(int index, int16_t value) {
    if (index >= 0 && index < 12) setData(IDX_MATE_START + index, value);
}

int16_t WarData::getAutoMate(int index) const {
    if (index < 0 || index >= 12) return 0;
    return getData(IDX_AUTOMATE_START + index);
}
void WarData::setAutoMate(int index, int16_t value) {
    if (index >= 0 && index < 12) setData(IDX_AUTOMATE_START + index, value);
}

int16_t WarData::getMateX(int index) const {
    if (index < 0 || index >= 12) return 0;
    return getData(IDX_MATEX_START + index);
}
void WarData::setMateX(int index, int16_t value) {
    if (index >= 0 && index < 12) setData(IDX_MATEX_START + index, value);
}

int16_t WarData::getMateY(int index) const {
    if (index < 0 || index >= 12) return 0;
    return getData(IDX_MATEY_START + index);
}
void WarData::setMateY(int index, int16_t value) {
    if (index >= 0 && index < 12) setData(IDX_MATEY_START + index, value);
}

int16_t WarData::getEnemy(int index) const {
    if (index < 0 || index >= 30) return 0;
    return getData(IDX_ENEMY_START + index);
}
void WarData::setEnemy(int index, int16_t value) {
    if (index >= 0 && index < 30) setData(IDX_ENEMY_START + index, value);
}

int16_t WarData::getEnemyX(int index) const {
    if (index < 0 || index >= 30) return 0;
    return getData(IDX_ENEMYX_START + index);
}
void WarData::setEnemyX(int index, int16_t value) {
    if (index >= 0 && index < 30) setData(IDX_ENEMYX_START + index, value);
}

int16_t WarData::getEnemyY(int index) const {
    if (index < 0 || index >= 30) return 0;
    return getData(IDX_ENEMYY_START + index);
}
void WarData::setEnemyY(int index, int16_t value) {
    if (index >= 0 && index < 30) setData(IDX_ENEMYY_START + index, value);
}

int16_t WarData::getGetKongfu(int index) const {
    if (index < 0 || index >= 3) return -1;
    return getData(IDX_GETKONGFU_START + index);
}

int16_t WarData::getGetItems(int index) const {
    if (index < 0 || index >= 3) return -1;
    return getData(IDX_GETITEMS_START + index);
}

int16_t WarData::getGetMoney() const {
    return getData(IDX_GETMONEY);
}
