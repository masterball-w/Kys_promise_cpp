#pragma once
#include <SDL3/SDL.h>
#include <vector>
#include <string>
#include "GameTypes.h"

struct Color {
    uint8_t r, g, b;
};

class GraphicsUtils {
public:
    // Palette Management
    static void loadPalette(const std::string& filename);
    static void resetPalette(int index = 0);
    static uint32_t getPaletteColor(int index); // Returns mapped 32-bit color
    
    // Palette Animation
    static void ChangeCol(uint32_t ticks); // Cycles palette colors for water effect
    
    // Drawing Primitives
    static void DrawPixel(SDL_Surface* surface, int x, int y, uint32_t color);
    static uint32_t GetPixel(SDL_Surface* surface, int x, int y);
    
    // RLE Rendering
    // Decodes and draws an RLE-encoded image from the buffer
    // rawData: The chunk of data starting at the offset found in .idx
    static void DrawRLE8(SDL_Surface* dest, int x, int y, const uint8_t* rawData, size_t dataSize, int shadow = 0, float scale = 1.0f, int green = 0, int red = 0, int gray = 0, int white = 0);
    
private:
    static std::vector<uint8_t> m_fullPaletteData; // 4 * 256 * 3 bytes
    static std::vector<uint32_t> m_currentPaletteRGBA; // Cached 32-bit colors for current palette
    
    // Internal helper to map RGB to 32-bit format of the surface
    static uint32_t mapRGB(uint8_t r, uint8_t g, uint8_t b);
};
