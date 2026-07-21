#pragma once
#include <string>
#include <vector>
#include <map>
#include <SDL3/SDL.h>

class SoundManager {
public:
    static SoundManager& getInstance();

    bool Init();
    void Quit();

    void PlayMusic(int musicId);
    void StopMusic();
    int GetMusicVolumeLevel() const;
    void SetMusicVolumeLevel(int level);

    void PlaySound(int soundId);
    void Update();

private:
    SoundManager();
    ~SoundManager();
    SoundManager(const SoundManager&) = delete;
    SoundManager& operator=(const SoundManager&) = delete;

    bool playWavMusic(const std::string& fullPath);

    struct AudioData {
        Uint8* buffer;
        Uint32 length;
        SDL_AudioSpec spec;
    };

    std::map<int, AudioData> m_soundCache;
    std::vector<SDL_AudioStream*> m_activeStreams;

    SDL_AudioDeviceID m_deviceId;
    int m_currentMusicId;
    int m_musicVolumeLevel = 8;

    SDL_AudioStream* m_musicStream = nullptr;
    Uint8* m_musicBuffer = nullptr;
    Uint32 m_musicLength = 0;
    SDL_AudioSpec m_musicSpec{};
};
