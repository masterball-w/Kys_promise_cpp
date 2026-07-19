#pragma once
#include <string>
#include <vector>
#include <map>
#include <SDL3/SDL.h>

// Simple Sound Manager using SDL3 Native Audio
class SoundManager {
public:
    static SoundManager& getInstance();

    bool Init();
    void Quit();

    // Music (Not fully implemented for native SDL3 without codecs, placeholders)
    void PlayMusic(int musicId);
    void StopMusic();
    int GetMusicVolumeLevel() const;
    void SetMusicVolumeLevel(int level);

    // Sound Effect
    void PlaySound(int soundId);

    // Update loop (for cleaning up streams if needed)
    void Update();

private:
    SoundManager();
    ~SoundManager();
    SoundManager(const SoundManager&) = delete;
    SoundManager& operator=(const SoundManager&) = delete;

    struct AudioData {
        Uint8* buffer;
        Uint32 length;
        SDL_AudioSpec spec;
    };

    // Resources
    std::map<int, AudioData> m_soundCache;
    
    // Active streams (we can keep track to clean them up, or let SDL handle it?)
    // SDL3 streams bound to device need to be destroyed.
    // We can store active streams.
    std::vector<SDL_AudioStream*> m_activeStreams;

    SDL_AudioDeviceID m_deviceId;
    int m_currentMusicId;
    int m_musicVolumeLevel = 8;
};
