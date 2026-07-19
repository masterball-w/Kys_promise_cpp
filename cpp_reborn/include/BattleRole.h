#ifndef BATTLEROLE_H
#define BATTLEROLE_H

#include "GameObject.h"

class BattleRole : public GameObject {
public:
    BattleRole();
    virtual ~BattleRole() = default;

    int16_t getRNum() const;
    void setRNum(int16_t value);

    int16_t getTeam() const;
    void setTeam(int16_t value);

    int16_t getY() const;
    void setY(int16_t value);

    int16_t getX() const;
    void setX(int16_t value);

    int16_t getFace() const;
    void setFace(int16_t value);

    int16_t getDead() const;
    void setDead(int16_t value);

    int16_t getStep() const;
    void setStep(int16_t value);

    int16_t getActed() const;
    void setActed(int16_t value);

    int16_t getPic() const;
    void setPic(int16_t value);

    int16_t getShowNumber() const;
    void setShowNumber(int16_t value);

    int16_t getProgress() const;
    void setProgress(int16_t value);

    int16_t getRound() const;
    void setRound(int16_t value);

    int16_t getSpeed() const;
    void setSpeed(int16_t value);

    int16_t getExpGot() const;
    void setExpGot(int16_t value);

    int16_t getAuto() const;
    void setAuto(int16_t value);

    int16_t getShow() const;
    void setShow(int16_t value);

    int16_t getWait() const;
    void setWait(int16_t value);

    int16_t getFrozen() const;
    void setFrozen(int16_t value);

    int16_t getKilled() const;
    void setKilled(int16_t value);

    int16_t getKnowledge() const;
    void setKnowledge(int16_t value);

    int16_t getLifeAdd() const;
    void setLifeAdd(int16_t value);

    int16_t getAddAtt() const;
    void setAddAtt(int16_t value);

    int16_t getAddDef() const;
    void setAddDef(int16_t value);

    int16_t getAddSpd() const;
    void setAddSpd(int16_t value);

    int16_t getAddStep() const;
    void setAddStep(int16_t value);

    int16_t getAddDodge() const;
    void setAddDodge(int16_t value);

    int16_t getPerfectDodge() const;
    void setPerfectDodge(int16_t value);

    int getFlashTimer() const { return m_flashTimer; }
    void setFlashTimer(int value) { m_flashTimer = value; }
    void decFlashTimer() { if (m_flashTimer > 0) m_flashTimer--; }

    int16_t getLevel() const;

private:
    // Indices based on TBattleRole definition in kys_main.pas
    enum Index {
        IDX_RNUM = 0,
        IDX_TEAM = 1,
        IDX_Y = 2,
        IDX_X = 3,
        IDX_FACE = 4,
        IDX_DEAD = 5,
        IDX_STEP = 6,
        IDX_ACTED = 7,
        IDX_PIC = 8,
        IDX_SHOWNUMBER = 9,
        IDX_PROGRESS = 10,
        IDX_ROUND = 11,
        IDX_SPEED = 12,
        IDX_EXPGOT = 13,
        IDX_AUTO = 14,
        IDX_SHOW = 15,
        IDX_WAIT = 16,
        IDX_FROZEN = 17,
        IDX_KILLED = 18,
        IDX_KNOWLEDGE = 19,
        IDX_LIFEADD = 20,
        IDX_ADDATT = 21,
        IDX_ADDDEF = 22,
        IDX_ADDSPD = 23,
        IDX_ADDSTEP = 24,
        IDX_ADDDODGE = 25,
        IDX_PERFECTDODGE = 26
    };

    int m_flashTimer = 0;
};

#endif
