#include "SoundManager.h"
#include "FileLoader.h"
#include <iostream>
#include <algorithm>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#undef PlaySound
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
    m_deviceId = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (m_deviceId == 0) {
        std::cerr << "Failed to open audio device: " << SDL_GetError() << std::endl;
        return false;
    }
    SDL_ResumeAudioDevice(m_deviceId);
    std::cout << "SoundManager Initialized (SDL3 native"
#ifdef _WIN32
              << " + WinMM MCI"
#endif
              << ")" << std::endl;
    return true;
}

void SoundManager::Quit() {
    StopMusic();

    for (auto stream : m_activeStreams) {
        SDL_DestroyAudioStream(stream);
    }
    m_activeStreams.clear();

    for (auto& pair : m_soundCache) {
        SDL_free(pair.second.buffer);
    }
    m_soundCache.clear();

    if (m_musicBuffer) {
        SDL_free(m_musicBuffer);
        m_musicBuffer = nullptr;
        m_musicLength = 0;
    }

    if (m_deviceId != 0) {
        SDL_CloseAudioDevice(m_deviceId);
        m_deviceId = 0;
    }
}

void SoundManager::Update() {
    // Loop BGM if stream drained
    if (m_musicStream && m_musicBuffer && m_musicLength > 0) {
        if (SDL_GetAudioStreamQueued(m_musicStream) < static_cast<int>(m_musicLength / 4)) {
            SDL_PutAudioStreamData(m_musicStream, m_musicBuffer, static_cast<int>(m_musicLength));
        }
    }

    auto it = m_activeStreams.begin();
    while (it != m_activeStreams.end()) {
        SDL_AudioStream* stream = *it;
        if (SDL_GetAudioStreamQueued(stream) == 0) {
            SDL_DestroyAudioStream(stream);
            it = m_activeStreams.erase(it);
        } else {
            ++it;
        }
    }
}

bool SoundManager::playWavMusic(const std::string& fullPath) {
    if (m_deviceId == 0) return false;

    SDL_AudioSpec spec{};
    Uint8* buffer = nullptr;
    Uint32 length = 0;
    if (!SDL_LoadWAV(fullPath.c_str(), &spec, &buffer, &length)) {
        return false;
    }

    SDL_AudioSpec deviceSpec{};
    if (!SDL_GetAudioDeviceFormat(m_deviceId, &deviceSpec, nullptr)) {
        SDL_free(buffer);
        return false;
    }

    SDL_AudioStream* stream = SDL_CreateAudioStream(&spec, &deviceSpec);
    if (!stream) {
        SDL_free(buffer);
        return false;
    }

    if (!SDL_PutAudioStreamData(stream, buffer, static_cast<int>(length))) {
        std::cerr << "Failed to queue music: " << SDL_GetError() << std::endl;
        SDL_DestroyAudioStream(stream);
        SDL_free(buffer);
        return false;
    }
    SDL_FlushAudioStream(stream);
    SDL_BindAudioStream(m_deviceId, stream);

    m_musicStream = stream;
    m_musicBuffer = buffer;
    m_musicLength = length;
    m_musicSpec = spec;
    std::cout << "Playing music (WAV loop): " << fullPath << std::endl;
    return true;
}

void SoundManager::PlayMusic(int musicId) {
    if (m_currentMusicId == musicId) return;

    StopMusic();
    m_currentMusicId = musicId;

#ifdef _WIN32
    const char* exts[] = { ".mid", ".mp3", ".ogg", ".wav" };
    bool found = false;

    for (const char* ext : exts) {
        std::string filename = "music/" + std::to_string(musicId) + ext;
        std::string fullPath = FileLoader::getResourcePath(filename);
        std::string winPath = fullPath;
        std::replace(winPath.begin(), winPath.end(), '/', '\\');

        FILE* f = fopen(winPath.c_str(), "rb");
        if (!f) continue;
        fclose(f);

        if (std::string(ext) == ".wav") {
            if (playWavMusic(fullPath)) {
                found = true;
                break;
            }
            continue;
        }

        std::string cmd = "open \"" + winPath + "\" alias bgm";
        if (mciSendStringA(cmd.c_str(), NULL, 0, NULL) == 0) {
            mciSendStringA("play bgm repeat", NULL, 0, NULL);
            SetMusicVolumeLevel(m_musicVolumeLevel);
            std::cout << "Playing music (MCI): " << winPath << std::endl;
            found = true;
            break;
        }
    }

    if (!found) {
        std::cerr << "Music " << musicId << " not found." << std::endl;
    }
#else
    // Cross-platform: prefer preconverted WAV (place music/N.wav next to mid/ogg packs)
    const char* exts[] = { ".wav", ".WAV" };
    bool found = false;
    for (const char* ext : exts) {
        std::string fullPath = FileLoader::getResourcePath("music/" + std::to_string(musicId) + ext);
        FILE* f = fopen(fullPath.c_str(), "rb");
        if (!f) continue;
        fclose(f);
        if (playWavMusic(fullPath)) {
            found = true;
            break;
        }
    }
    if (!found) {
        // Hint: OGG/MID need conversion on Android — look for ogg/mid so logs are useful
        std::string ogg = FileLoader::getResourcePath("music/" + std::to_string(musicId) + ".ogg");
        std::string mid = FileLoader::getResourcePath("music/" + std::to_string(musicId) + ".mid");
        FILE* fo = fopen(ogg.c_str(), "rb");
        FILE* fm = fopen(mid.c_str(), "rb");
        if (fo || fm) {
            if (fo) fclose(fo);
            if (fm) fclose(fm);
            std::cout << "Music " << musicId
                      << " found as mid/ogg but no decoder; convert to music/"
                      << musicId << ".wav for Android/Linux." << std::endl;
        } else {
            std::cout << "Music " << musicId << " not found (tried .wav)." << std::endl;
        }
    }
#endif
}

void SoundManager::StopMusic() {
#ifdef _WIN32
    mciSendStringA("close bgm", NULL, 0, NULL);
#endif
    if (m_musicStream) {
        SDL_DestroyAudioStream(m_musicStream);
        m_musicStream = nullptr;
    }
    if (m_musicBuffer) {
        SDL_free(m_musicBuffer);
        m_musicBuffer = nullptr;
        m_musicLength = 0;
    }
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
            return;
        }
    }

    if (data) {
        SDL_AudioSpec deviceSpec;
        if (!SDL_GetAudioDeviceFormat(m_deviceId, &deviceSpec, nullptr)) {
            std::cerr << "Failed to get device format" << std::endl;
            return;
        }

        SDL_AudioStream* stream = SDL_CreateAudioStream(&data->spec, &deviceSpec);
        if (stream) {
            if (SDL_PutAudioStreamData(stream, data->buffer, static_cast<int>(data->length))) {
                 SDL_FlushAudioStream(stream);
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
