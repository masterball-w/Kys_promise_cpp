#pragma once
#include <SDL3/SDL.h>
#include <string>
#include <vector>
#include <memory>
#include <map>

struct PicImage {
    SDL_Surface* surface = nullptr;
    int x = 0;
    int y = 0;
    int black = 0;

    // Helper to free surface automatically if we wanted, 
    // but for now we keep it raw to match other parts of the engine
    // or we should manage it. 
    // In Pascal TPic has a 'pic' field which is PSDL_Surface.
};

class PicLoader {
public:
    // Load a .pic file and retrieve the image at index 'num'
    static PicImage loadPic(const std::string& filename, int num);

    // Get the number of images in the .pic file
    static int getPicCount(const std::string& filename);

    // Free the surface inside PicImage
    static void freePic(PicImage& pic);

    static SDL_Surface* getCachedSurface(const std::string& filename, int num);
    static void clearCache();
};
