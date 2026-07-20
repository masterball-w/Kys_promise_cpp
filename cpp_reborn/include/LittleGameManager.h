#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <utility>
#include <SDL3/SDL.h>
#include "PicLoader.h"

// Port of kys_littlegame.pas (+ Puzzle lives in EventManager).
class LittleGameManager {
public:
    static LittleGameManager& getInstance();

    // Returns EatFemale count (written to x50[e3] by instruct_43 -25).
    int FemaleSnake();

    bool ShotEagle(int aim, int chance);
    bool Acupuncture(int n);
    bool Lamp(int c, int beginpic, int whitecount, int chance);
    bool Poetry(int talknum, int chance, int cols, int count);
    bool RotoSpellPicture(int num, int chance);

private:
    LittleGameManager() = default;

    bool HasPetLuck() const;
    void Present();
    void WaitAnyKeyLocal();
    void BlitPic(const PicImage& pic, int x, int y);
    void BlitSurface(SDL_Surface* surf, int x, int y, double angleDeg = 0.0);
    void DrawFallbackRect(int x, int y, int w, int h, uint32_t rgba);
    SDL_Surface* CropSurface(SDL_Surface* src, int sx, int sy, int sw, int sh);
    SDL_Surface* RotateSurface90(SDL_Surface* src, int turns);
    std::vector<uint16_t> LoadPoetryChars(int talknum);
    static std::string Ucs2ToUtf8(uint16_t ch);
};
