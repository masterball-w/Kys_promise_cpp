#include "GameHooks.h"

GameHooks::MusicHook GameHooks::playMusic = nullptr;
GameHooks::SoundHook GameHooks::playSound = nullptr;
GameHooks::EffectHook GameHooks::playMagicEffect = nullptr;
GameHooks::BattleEndHook GameHooks::onBattleEnd = nullptr;
