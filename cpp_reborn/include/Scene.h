#pragma once
#include "GameObject.h"
#include <string>

constexpr size_t SCENE_DATA_SIZE = 26; // Pascal TScene size (0..25)

class Scene : public GameObject {
public:
    Scene() : GameObject(SCENE_DATA_SIZE) {}
    virtual ~Scene() = default;

    using GameObject::getData;
    using GameObject::setData;

    // 0: ListNum
    int16 getListNum() const { return m_data[0]; }

    // 1-5: Name (10 bytes = 5 words)
    std::string getName() const { return getString(1, 5); }

    // 6: ExitMusic
    int16 getExitMusic() const { return m_data[6]; }

    // 7: EntranceMusic
    int16 getEntranceMusic() const { return m_data[7]; }

    // 8: Pallet
    int16 getPallet() const { return m_data[8]; }
    
    // 9: EnCondition
    int16 getEnCondition() const { return m_data[9]; }
    void setEnCondition(int16 v) { m_data[9] = v; }
    
    // 10: MainEntranceY1
    int16 getMainEntranceY1() const { return m_data[10]; }
    // 11: MainEntranceX1
    int16 getMainEntranceX1() const { return m_data[11]; }
    
    // 12: MainEntranceY2
    int16 getMainEntranceY2() const { return m_data[12]; }
    // 13: MainEntranceX2
    int16 getMainEntranceX2() const { return m_data[13]; }
    
    // 14: EntranceY
    int16 getEntranceY() const { return m_data[14]; }
    // 15: EntranceX
    int16 getEntranceX() const { return m_data[15]; }
    
    // 16-18: ExitY (array[0..2])
    int16 getExitY(int index) const { 
        if (index >= 0 && index < 3) return m_data[16 + index];
        return -1;
    }
    
    // 19-21: ExitX (array[0..2])
    int16 getExitX(int index) const { 
        if (index >= 0 && index < 3) return m_data[19 + index];
        return -1;
    }

    // 22: Mapmode
    int16 getMapMode() const { return m_data[22]; }

    // 23: mapnum
    int16 getMapNum() const { return m_data[23]; }
};
