#include "SoundManager.h"
#include "FileLoader.h"
#include <iostream>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib") // Fallback if CMake fails, but CMake is better
#undef PlaySound // Fix conflict with Windows API macro
#endif

SoundManager& SoundManager::getInstance() {
    static SoundManager instance;
    return instance;
}

SoundManager::SoundManager() : m_deviceId(0), m_currentMusicId(-1) {}

SoundManager::~SoundManager() {
    Quit();
}

bool SoundManager::Init() {
    // Open default playback device
    // SDL3: SDL_OpenAudioDevice(devid, spec)
    // We pass NULL for spec to let it choose default or we can request one.
    // We pass SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK as devid.
    
    m_deviceId = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (m_deviceId == 0) {
        std::cerr << "Failed to open audio device: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // Ensure device is unpaused (SDL3 devices might start unpaused, but good to be sure)
    SDL_ResumeAudioDevice(m_deviceId);
    
    std::cout << "SoundManager Initialized (SDL3 Native + WinMM MCI)" << std::endl;
    return true;
}

void SoundManager::Quit() {
    StopMusic();

    // Destroy active streams
    for (auto stream : m_activeStreams) {
        SDL_DestroyAudioStream(stream);
    }
    m_activeStreams.clear();

    // Free cached sounds
    for (auto& pair : m_soundCache) {
        SDL_free(pair.second.buffer);
    }
    m_soundCache.clear();

    if (m_deviceId != 0) {
        SDL_CloseAudioDevice(m_deviceId);
        m_deviceId = 0;
    }
}

void SoundManager::Update() {
    // Clean up finished streams
    auto it = m_activeStreams.begin();
    while (it != m_activeStreams.end()) {
        SDL_AudioStream* stream = *it;
        // Check if stream is empty.
        // SDL_GetAudioStreamQueued returns bytes queued.
        if (SDL_GetAudioStreamQueued(stream) == 0) {
            // Unbind (implicit on destroy) and destroy
            SDL_DestroyAudioStream(stream);
            it = m_activeStreams.erase(it);
        } else {
            ++it;
        }
    }
}

void SoundManager::PlayMusic(int musicId) {
    if (m_currentMusicId == musicId) return;
    
    StopMusic();
    m_currentMusicId = musicId;

#ifdef _WIN32
    // Primary: Windows MCI (supports MIDI/MP3)
    // Supports MIDI, MP3, etc. without external libs
    
    // 1. Find the file
    std::string path;
    const char* exts[] = { ".mid", ".mp3", ".ogg" };
    bool found = false;
    
    for (const char* ext : exts) {
        std::string filename = "music/" + std::to_string(musicId) + ext;
        std::string fullPath = FileLoader::getResourcePath(filename);
        
        // Check if file exists (using FileLoader helper or just trying)
        // Since FileLoader::getResourcePath usually returns a path even if not exists if not checking,
        // we might want to verify. But assuming FileLoader logic is robust enough or we trust it.
        // Let's use std::filesystem or FILE* to check existence quickly if needed, 
        // but let's assume if it returns a valid path we try it.
        // Actually, let's just try to open it with MCI.
        
        // Construct MCI open command
        // "open \"path\" type mpegvideo alias bgm" (for mp3) or "sequencer" (for midi)
        // Simpler: "open \"path\" alias bgm" and let MCI auto-detect.
        
        // Note: MCI doesn't like forward slashes sometimes? It usually handles them, but backslashes are safer on Windows.
        std::string winPath = fullPath;
        std::replace(winPath.begin(), winPath.end(), '/', '\\');
        
        // Check file existence
        FILE* f = fopen(winPath.c_str(), "rb");
        if (f) {
            fclose(f);
            found = true;
            
            std::string cmd = "open \"" + winPath + "\" alias bgm";
            mciSendStringA(cmd.c_str(), NULL, 0, NULL);
            
            // Play with repeat
            mciSendStringA("play bgm repeat", NULL, 0, NULL);
            SetMusicVolumeLevel(m_musicVolumeLevel);
            
            std::cout << "Playing music (MCI): " << winPath << std::endl;
            break;
        }
    }
    
    if (!found) {
        // Fallback: try WAV via SDL3 native audio
        std::string wavPath = FileLoader::getResourcePath("music/" + std::to_string(musicId) + ".wav");
        FILE* f = fopen(wavPath.c_str(), "rb");
        if (f) {
            fclose(f);
            PlaySound(musicId);
            std::cout << "Playing music (WAV fallback): " << wavPath << std::endl;
        } else {
            std::cerr << "Music " << musicId << " not found." << std::endl;
        }
    }
#else
    std::string wavPath = FileLoader::getResourcePath("music/" + std::to_string(musicId) + ".wav");
    FILE* f = fopen(wavPath.c_str(), "rb");
    if (f) {
        fclose(f);
        PlaySound(musicId);
        std::cout << "Playing music (WAV): " << wavPath << std::endl;
    } else {
        std::cout << "Music " << musicId << " requested (no cross-platform codec; place .wav in music/)" << std::endl;
    }
#endif
}

void SoundManager::StopMusic() {
#ifdef _WIN32
    mciSendStringA("close bgm", NULL, 0, NULL);
#endif
    m_currentMusicId = -1;
}

int SoundManager::GetMusicVolumeLevel() const {
    return m_musicVolumeLevel;
}

void SoundManager::SetMusicVolumeLevel(int level) {
    m_musicVolumeLevel = std::clamp(level, 0, 8);
#ifdef _WIN32
    int volume = m_musicVolumeLevel * 125;
    std::string cmd = "setaudio bgm volume to " + std::to_string(volume);
    mciSendStringA(cmd.c_str(), NULL, 0, NULL);
#endif
}

void SoundManager::PlaySound(int soundId) {
    if (m_deviceId == 0) return;

    AudioData* data = nullptr;
    if (m_soundCache.find(soundId) != m_soundCache.end()) {
        data = &m_soundCache[soundId];
    } else {
        // Load WAV
        char buf[32];
        snprintf(buf, sizeof(buf), "e%03d.wav", soundId);
        std::string filename = "sound/" + std::string(buf);
        std::string fullPath = FileLoader::getResourcePath(filename);

        SDL_AudioSpec spec;
        Uint8* buffer = nullptr;
        Uint32 length = 0;

        if (SDL_LoadWAV(fullPath.c_str(), &spec, &buffer, &length)) {
            AudioData newData;
            newData.buffer = buffer;
            newData.length = length;
            newData.spec = spec;
            m_soundCache[soundId] = newData;
            data = &m_soundCache[soundId];
            std::cout << "Loaded sound: " << filename << std::endl;
        } else {
            std::cerr << "Failed to load sound " << soundId << ": " << SDL_GetError() << std::endl;
            // Cache failure? No, retry next time.
            return;
        }
    }

    if (data) {
        // Create stream
        SDL_AudioSpec deviceSpec;
        if (!SDL_GetAudioDeviceFormat(m_deviceId, &deviceSpec, nullptr)) {
            std::cerr << "Failed to get device format" << std::endl;
            return;
        }

        SDL_AudioStream* stream = SDL_CreateAudioStream(&data->spec, &deviceSpec);
        if (stream) {
            if (SDL_PutAudioStreamData(stream, data->buffer, data->length) == 0) {
                 SDL_FlushAudioStream(stream); // Mark no more data input
                 SDL_BindAudioStream(m_deviceId, stream);
                 m_activeStreams.push_back(stream);
            } else {
                std::cerr << "Failed to put audio data: " << SDL_GetError() << std::endl;
                SDL_DestroyAudioStream(stream);
            }
        } else {
             std::cerr << "Failed to create audio stream: " << SDL_GetError() << std::endl;
        }
    }
}
