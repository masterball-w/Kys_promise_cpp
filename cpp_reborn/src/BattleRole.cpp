#include "BattleRole.h"
#include "GameManager.h"
#include "Role.h"

BattleRole::BattleRole() : GameObject(27) {}

int16_t BattleRole::getLevel() const {
    // Get the underlying Role ID (rnum)
    int16_t rnum = getRNum();
    // Access GameManager to get the Role object
    // Note: This creates a dependency on GameManager, which is fine for .cpp
    if (rnum >= 0) {
        return GameManager::getInstance().getRole(rnum).getLevel();
    }
    return 1; // Default level 1 if invalid
}

int16_t BattleRole::getRNum() const { return getData(IDX_RNUM); }
void BattleRole::setRNum(int16_t value) { setData(IDX_RNUM, value); }

int16_t BattleRole::getTeam() const { return getData(IDX_TEAM); }
void BattleRole::setTeam(int16_t value) { setData(IDX_TEAM, value); }

int16_t BattleRole::getY() const { return getData(IDX_Y); }
void BattleRole::setY(int16_t value) { setData(IDX_Y, value); }

int16_t BattleRole::getX() const { return getData(IDX_X); }
void BattleRole::setX(int16_t value) { setData(IDX_X, value); }

int16_t BattleRole::getFace() const { return getData(IDX_FACE); }
void BattleRole::setFace(int16_t value) { setData(IDX_FACE, value); }

int16_t BattleRole::getDead() const { return getData(IDX_DEAD); }
void BattleRole::setDead(int16_t value) { setData(IDX_DEAD, value); }

int16_t BattleRole::getStep() const { return getData(IDX_STEP); }
void BattleRole::setStep(int16_t value) { setData(IDX_STEP, value); }

int16_t BattleRole::getActed() const { return getData(IDX_ACTED); }
void BattleRole::setActed(int16_t value) { setData(IDX_ACTED, value); }

int16_t BattleRole::getPic() const { return getData(IDX_PIC); }
void BattleRole::setPic(int16_t value) { setData(IDX_PIC, value); }

int16_t BattleRole::getShowNumber() const { return getData(IDX_SHOWNUMBER); }
void BattleRole::setShowNumber(int16_t value) { setData(IDX_SHOWNUMBER, value); }

int16_t BattleRole::getProgress() const { return getData(IDX_PROGRESS); }
void BattleRole::setProgress(int16_t value) { setData(IDX_PROGRESS, value); }

int16_t BattleRole::getRound() const { return getData(IDX_ROUND); }
void BattleRole::setRound(int16_t value) { setData(IDX_ROUND, value); }

int16_t BattleRole::getSpeed() const { return getData(IDX_SPEED); }
void BattleRole::setSpeed(int16_t value) { setData(IDX_SPEED, value); }

int16_t BattleRole::getExpGot() const { return getData(IDX_EXPGOT); }
void BattleRole::setExpGot(int16_t value) { setData(IDX_EXPGOT, value); }

int16_t BattleRole::getAuto() const { return getData(IDX_AUTO); }
void BattleRole::setAuto(int16_t value) { setData(IDX_AUTO, value); }

int16_t BattleRole::getShow() const { return getData(IDX_SHOW); }
void BattleRole::setShow(int16_t value) { setData(IDX_SHOW, value); }

int16_t BattleRole::getWait() const { return getData(IDX_WAIT); }
void BattleRole::setWait(int16_t value) { setData(IDX_WAIT, value); }

int16_t BattleRole::getFrozen() const { return getData(IDX_FROZEN); }
void BattleRole::setFrozen(int16_t value) { setData(IDX_FROZEN, value); }

int16_t BattleRole::getKilled() const { return getData(IDX_KILLED); }
void BattleRole::setKilled(int16_t value) { setData(IDX_KILLED, value); }

int16_t BattleRole::getKnowledge() const { return getData(IDX_KNOWLEDGE); }
void BattleRole::setKnowledge(int16_t value) { setData(IDX_KNOWLEDGE, value); }

int16_t BattleRole::getLifeAdd() const { return getData(IDX_LIFEADD); }
void BattleRole::setLifeAdd(int16_t value) { setData(IDX_LIFEADD, value); }

int16_t BattleRole::getAddAtt() const { return getData(IDX_ADDATT); }
void BattleRole::setAddAtt(int16_t value) { setData(IDX_ADDATT, value); }

int16_t BattleRole::getAddDef() const { return getData(IDX_ADDDEF); }
void BattleRole::setAddDef(int16_t value) { setData(IDX_ADDDEF, value); }

int16_t BattleRole::getAddSpd() const { return getData(IDX_ADDSPD); }
void BattleRole::setAddSpd(int16_t value) { setData(IDX_ADDSPD, value); }

int16_t BattleRole::getAddStep() const { return getData(IDX_ADDSTEP); }
void BattleRole::setAddStep(int16_t value) { setData(IDX_ADDSTEP, value); }

int16_t BattleRole::getAddDodge() const { return getData(IDX_ADDDODGE); }
void BattleRole::setAddDodge(int16_t value) { setData(IDX_ADDDODGE, value); }

int16_t BattleRole::getPerfectDodge() const { return getData(IDX_PERFECTDODGE); }
void BattleRole::setPerfectDodge(int16_t value) { setData(IDX_PERFECTDODGE, value); }
