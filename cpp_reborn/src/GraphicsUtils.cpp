#include "GraphicsUtils.h"
#include <fstream>
#include <iostream>
#include <cstring>
#include <algorithm>

std::vector<uint8_t> GraphicsUtils::m_fullPaletteData;
std::vector<uint32_t> GraphicsUtils::m_currentPaletteRGBA;

void GraphicsUtils::loadPalette(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to load palette: " << filename << std::endl;
        return;
    }
    
    // Palette file is expected to be 4 * 768 bytes (4 sets of 256 colors * 3 channels)
    m_fullPaletteData.resize(4 * 768);
    file.read(reinterpret_cast<char*>(m_fullPaletteData.data()), m_fullPaletteData.size());
    
    // Initialize current palette with the first set
    resetPalette(0);
}

void GraphicsUtils::resetPalette(int index) {
    if (m_fullPaletteData.empty()) return;
    if (index < 0 || index > 3) index = 0;
    
    size_t offset = index * 768;
    if (offset + 768 > m_fullPaletteData.size()) return;
    
    m_currentPaletteRGBA.resize(256);
    
    for (int i = 0; i < 256; ++i) {
        uint8_t r = m_fullPaletteData[offset + i * 3 + 0];
        uint8_t g = m_fullPaletteData[offset + i * 3 + 1];
        uint8_t b = m_fullPaletteData[offset + i * 3 + 2];
        
        // Convert 6-bit (0-63) to 8-bit (0-255)
        // Original game uses * 4
        m_currentPaletteRGBA[i] = mapRGB(r * 4, g * 4, b * 4);
    }
}

void GraphicsUtils::ChangeCol(uint32_t ticks) {
    if (m_fullPaletteData.empty()) return;
    
    // Original Pascal Logic:
    //   a := $E7 * 3;
    //   temp[0] := ACol[a]; ...
    //   Cycle $E7 down to $E1 (231 down to 225)
    //   b := $E0 * 3;
    //   ACol[b] := temp;
    // This is a rotation of colors 224 to 231 (8 colors).
    // Actually, Pascal loop: for i := $E7 downto $E1 (231 downto 225)
    // b = i*3 (dest), a = (i-1)*3 (src). 
    // This moves color[i-1] to color[i].
    // Then color[224] (E0) gets temp (color[231]).
    // So it's a right shift / rotation of range [224, 231].
    
    // Range 1: 0xE0 - 0xE7 (224 - 231)
    {
        uint32_t last = m_currentPaletteRGBA[0xE7];
        for (int i = 0xE7; i > 0xE0; --i) {
            m_currentPaletteRGBA[i] = m_currentPaletteRGBA[i - 1];
        }
        m_currentPaletteRGBA[0xE0] = last;
    }

    // Range 2: 0xF4 - 0xFC (244 - 252)
    {
        uint32_t last = m_currentPaletteRGBA[0xFC];
        for (int i = 0xFC; i > 0xF4; --i) {
            m_currentPaletteRGBA[i] = m_currentPaletteRGBA[i - 1];
        }
        m_currentPaletteRGBA[0xF4] = last;
    }
}

uint32_t GraphicsUtils::getPaletteColor(int index) {
    if (index < 0 || index >= m_currentPaletteRGBA.size()) return 0;
    return m_currentPaletteRGBA[index];
}

uint32_t GraphicsUtils::mapRGB(uint8_t r, uint8_t g, uint8_t b) {
    // SDL_PIXELFORMAT_ARGB8888 usually means:
    // Byte order: B G R A (on Little Endian) -> 0xAARRGGBB
    // SDL_MapRGB returns this uint32 value.
    
    // We are manually writing to uint32* pixels.
    // If the surface format is ARGB8888:
    // Memory: B G R A
    // Value: 0xAARRGGBB
    
    // If the map is red (should be green/brown?) but shows blue, R and B are swapped.
    // Let's try to swap R and B in our manual packing.
    
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    return (r << 24) | (g << 16) | (b << 8) | 255;
#else
    // Little Endian (x86/Windows)
    // Old: return (255 << 24) | (b << 16) | (g << 8) | r; // 0xAABBGGRR -> BGRA in memory? 
    // Wait, 0xAABBGGRR in uint32 on LE is: R, G, B, A in memory.
    
    // If we see Blue instead of Red:
    // We wanted Red (R=255, B=0), got Blue (R=0, B=255 on screen).
    // This means we wrote B where R should be, or vice versa.
    
    // Let's try standard 0xAARRGGBB format for Little Endian uint32
    // Which is B G R A in memory.
    // return (255 << 24) | (r << 16) | (g << 8) | b;
    
    // Trying swapping R and B from previous implementation
    return (255 << 24) | (r << 16) | (g << 8) | b; 
#endif
}

void GraphicsUtils::DrawPixel(SDL_Surface* surface, int x, int y, uint32_t color) {
    if (!surface) return;
    if (x < 0 || x >= surface->w || y < 0 || y >= surface->h) return;
    
    // Assuming 32-bit surface
    uint32_t* pixels = (uint32_t*)surface->pixels;
    pixels[y * (surface->pitch / 4) + x] = color;
}

uint32_t GraphicsUtils::GetPixel(SDL_Surface* surface, int x, int y) {
    if (!surface) return 0;
    if (x < 0 || x >= surface->w || y < 0 || y >= surface->h) return 0;
    
    uint32_t* pixels = (uint32_t*)surface->pixels;
    return pixels[y * (surface->pitch / 4) + x];
}

void GraphicsUtils::DrawRLE8(SDL_Surface* dest, int x, int y, const uint8_t* rawData, size_t dataSize, int shadow, float scale, int green, int red, int gray, int white) {
    if (!dest || !rawData) return;
    if (dataSize < 8) return;
    
    const uint8_t* ptr = rawData;
    
    int16_t w = ptr[0] | (ptr[1] << 8); ptr += 2;
    int16_t h = ptr[0] | (ptr[1] << 8); ptr += 2;
    int16_t xs = ptr[0] | (ptr[1] << 8); ptr += 2;
    int16_t ys = ptr[0] | (ptr[1] << 8); ptr += 2;
    
    int startX = x - (int)(xs * scale);
    int startY = y - (int)(ys * scale);

    if (m_currentPaletteRGBA.empty()) return;
    
    const uint8_t* palData = m_fullPaletteData.data(); 
    bool hasRawPalette = !m_fullPaletteData.empty();

    int greenTint = std::min(std::max(green, 0), 150);
    int redTint = std::min(std::max(red, 0), 150);
    int grayTint = std::min(std::max(gray, 0), 100);
    int whiteTint = std::min(std::max(white, 0), 255);
    bool useTint = (greenTint > 0 || redTint > 0 || grayTint > 0 || whiteTint > 0);

    for (int iy = 0; iy < h; ++iy) {
        if (ptr >= rawData + dataSize) break;
        
        uint8_t rowPacketCount = *ptr++;
        int currentX = 0;
        int state = 0;
        
        for (int ix = 0; ix < rowPacketCount; ++ix) {
            if (ptr >= rawData + dataSize) break; // Bounds check for pixel data

            uint8_t val = *ptr++;
            
            if (state == 0) {
                currentX += val;
                state = 1;
            } else if (state == 1) {
                state = 2 + val;
            } else {
                // Drawing pixel
                // Original: int px = startX + currentX;
                //           int py = startY + iy;
                
                // Scaled Logic:
                // We draw a rectangle of size ceil(scale) x ceil(scale) at scaled position.
                // Scaled Pos relative to start:
                int scaledRelX = (int)(currentX * scale);
                int scaledRelY = (int)(iy * scale);
                
                int px = startX + scaledRelX;
                int py = startY + scaledRelY;
                
                // Determine block size (simple nearest neighbor)
                int nextScaledRelX = (int)((currentX + 1) * scale);
                int nextScaledRelY = (int)((iy + 1) * scale);
                int blockW = nextScaledRelX - scaledRelX;
                int blockH = nextScaledRelY - scaledRelY;
                
                if (blockW < 1) blockW = 1;
                if (blockH < 1) blockH = 1;

                uint32_t color;
                if (!useTint && shadow == 0) {
                    color = m_currentPaletteRGBA[val];
                } else {
                    int r = 0;
                    int g = 0;
                    int b = 0;
                    if (shadow != 0 && hasRawPalette) {
                        int mul = 4 + shadow;
                        if (mul < 0) mul = 0;
                        r = palData[val * 3 + 0] * mul;
                        g = palData[val * 3 + 1] * mul;
                        b = palData[val * 3 + 2] * mul;
                    } else {
                        uint32_t base = m_currentPaletteRGBA[val];
                        r = (base >> 16) & 0xFF;
                        g = (base >> 8) & 0xFF;
                        b = base & 0xFF;
                    }

                    r = std::min(std::max(r, 0), 255);
                    g = std::min(std::max(g, 0), 255);
                    b = std::min(std::max(b, 0), 255);

                    // Match Pascal DrawBRolePic tint priority: gray > red > green, exclusive.
                    if (grayTint > 0) {
                        int lum = (r * 30 + g * 59 + b * 11) / 100;
                        r = (r * (100 - grayTint) + lum * grayTint) / 100;
                        g = (g * (100 - grayTint) + lum * grayTint) / 100;
                        b = (b * (100 - grayTint) + lum * grayTint) / 100;
                    } else if (redTint > 0) {
                        int factor = 150 - redTint;
                        g = (g * factor) / 150;
                        b = (b * factor) / 150;
                    } else if (greenTint > 0) {
                        int factor = 150 - greenTint;
                        r = (r * factor) / 150;
                        b = (b * factor) / 150;
                    }
                    if (whiteTint > 0) {
                        int factor = 255 - whiteTint;
                        r = (r * factor + 255 * whiteTint) / 255;
                        g = (g * factor + 255 * whiteTint) / 255;
                        b = (b * factor + 255 * whiteTint) / 255;
                    }

                    r = std::min(std::max(r, 0), 255);
                    g = std::min(std::max(g, 0), 255);
                    b = std::min(std::max(b, 0), 255);
                    color = mapRGB(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b));
                }

                // Draw Block
                for (int by = 0; by < blockH; ++by) {
                    for (int bx = 0; bx < blockW; ++bx) {
                        DrawPixel(dest, px + bx, py + by, color);
                    }
                }
                
                currentX++;
                state--;
                if (state == 2) state = 0;
            }
        }
    }
}
