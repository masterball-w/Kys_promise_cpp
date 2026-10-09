#pragma once
#include <string>

// Optional extension points. Return true from a hook to replace the built-in path.
// Hooks are process-wide and not thread-safe; set them before GameManager::Run.

struct GameHooks {
    using MusicHook = bool (*)(int musicId);
    using SoundHook = bool (*)(int soundId);
    using EffectHook = bool (*)(int battleRole, int magicId, int level, int targetX, int targetY);
    using BattleEndHook = void (*)(int battleId, int result);

    static void setPlayMusic(MusicHook hook) { playMusic = hook; }
    static void setPlaySound(SoundHook hook) { playSound = hook; }
    static void setPlayMagicEffect(EffectHook hook) { playMagicEffect = hook; }
    static void setOnBattleEnd(BattleEndHook hook) { onBattleEnd = hook; }

    static bool tryPlayMusic(int musicId) {
        return playMusic && playMusic(musicId);
    }
    static bool tryPlaySound(int soundId) {
        return playSound && playSound(soundId);
    }
    static bool tryPlayMagicEffect(int battleRole, int magicId, int level, int targetX, int targetY) {
        return playMagicEffect && playMagicEffect(battleRole, magicId, level, targetX, targetY);
    }
    static void notifyBattleEnd(int battleId, int result) {
        if (onBattleEnd) onBattleEnd(battleId, result);
    }

private:
    static MusicHook playMusic;
    static SoundHook playSound;
    static EffectHook playMagicEffect;
    static BattleEndHook onBattleEnd;
};
