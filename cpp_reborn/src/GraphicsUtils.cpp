#include "GraphicsUtils.h"
#include <fstream>
#include <iostream>
#include <cstring>
#include <algorithm>

std::vector<uint8_t> GraphicsUtils::m_fullPaletteData;
std::vector<uint32_t> GraphicsUtils::m_currentPaletteRGBA;
int GraphicsUtils::m_scenePalletTint = 0;

void GraphicsUtils::setScenePalletTint(int pallet) {
    if (pallet < 0 || pallet > 3) pallet = 0;
    m_scenePalletTint = pallet;
}

bool GraphicsUtils::paletteSetIsEmpty(int index) {
    if (m_fullPaletteData.size() < 768) return true;
    if (index < 0 || index > 3) return true;
    const size_t offset = static_cast<size_t>(index) * 768;
    if (offset + 768 > m_fullPaletteData.size()) return true;
    for (size_t i = 0; i < 768; ++i) {
        if (m_fullPaletteData[offset + i] != 0) return false;
    }
    return true;
}

void GraphicsUtils::applyScenePalletTint(int& r, int& g, int& b) {
    switch (m_scenePalletTint) {
        case 1:
            r = (69 * r) / 100;
            g = (73 * g) / 100;
            b = (75 * b) / 100;
            break;
        case 2:
            r = (85 * r) / 100;
            g = (75 * g) / 100;
            b = (30 * b) / 100;
            break;
        case 3:
            r = (25 * r) / 100;
            g = (68 * g) / 100;
            b = (45 * b) / 100;
            break;
        default:
            break;
    }
}

void GraphicsUtils::loadPalette(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to load palette: " << filename << std::endl;
        return;
    }
    
    // Pascal loads 4 * 768 bytes into Col[0..3]. Some distributions ship a single set.
    m_fullPaletteData.resize(4 * 768, 0);
    file.read(reinterpret_cast<char*>(m_fullPaletteData.data()), m_fullPaletteData.size());
    const std::streamsize bytesRead = file.gcount();
    if (bytesRead > 0 && bytesRead <= 768) {
        for (int set = 1; set < 4; ++set) {
            std::memcpy(m_fullPaletteData.data() + set * 768, m_fullPaletteData.data(), 768);
        }
    }
    
    // Initialize current palette with the first set
    resetPalette(0);
}

void GraphicsUtils::resetPalette(int index) {
    if (m_fullPaletteData.empty()) return;
    if (index < 0 || index > 3) index = 0;
    if (paletteSetIsEmpty(index)) index = 0;
    
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
    if (index < 0 || index >= (int)m_currentPaletteRGBA.size()) return 0;
    return m_currentPaletteRGBA[index];
}

uint32_t GraphicsUtils::getUIPaletteColor(int index) {
    if (m_fullPaletteData.empty()) return 0;
    if (index < 0 || index > 255) return 0;
    size_t offset = index * 3; // palette set 0
    if (offset + 2 >= 768) return 0;
    uint8_t r = m_fullPaletteData[offset + 0];
    uint8_t g = m_fullPaletteData[offset + 1];
    uint8_t b = m_fullPaletteData[offset + 2];
    return mapRGB(r * 4, g * 4, b * 4);
}

void GraphicsUtils::setAColByte(int byteIndex, uint8_t value) {
    if (m_fullPaletteData.empty()) return;
    // Operate on active set 0 working copy mirrored in m_currentPaletteRGBA
    // ACol is 768 bytes (256*3). We keep edits on a side buffer sized like set 0.
    if (m_fullPaletteData.size() < 768) return;
    if (byteIndex < 0 || byteIndex >= 768) return;
    m_fullPaletteData[byteIndex] = value;
    int colorIdx = byteIndex / 3;
    uint8_t r = m_fullPaletteData[colorIdx * 3 + 0];
    uint8_t g = m_fullPaletteData[colorIdx * 3 + 1];
    uint8_t b = m_fullPaletteData[colorIdx * 3 + 2];
    if (m_currentPaletteRGBA.size() < 256) m_currentPaletteRGBA.resize(256);
    m_currentPaletteRGBA[colorIdx] = mapRGB(r * 4, g * 4, b * 4);
}

uint8_t GraphicsUtils::getAColByte(int byteIndex) {
    if (m_fullPaletteData.empty() || byteIndex < 0 || byteIndex >= 768) return 0;
    return m_fullPaletteData[byteIndex];
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

void GraphicsUtils::BlendRectangleOnSurface(SDL_Surface* surface, int x, int y, int w, int h,
                                            uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                                            int alphaPercent) {
    if (!surface || alphaPercent <= 0) return;
    if (alphaPercent > 100) alphaPercent = 100;

    const int xEnd = x + w;
    const int yEnd = y + h;
    for (int i1 = x; i1 <= xEnd; ++i1) {
        for (int i2 = y; i2 <= yEnd; ++i2) {
            if (i1 < 0 || i2 < 0 || i1 >= surface->w || i2 >= surface->h) continue;

            const uint32_t pix = GetPixel(surface, i1, i2);
            uint8_t pix1 = static_cast<uint8_t>(pix & 0xFF);
            uint8_t pix2 = static_cast<uint8_t>((pix >> 8) & 0xFF);
            uint8_t pix3 = static_cast<uint8_t>((pix >> 16) & 0xFF);
            uint8_t pix4 = static_cast<uint8_t>((pix >> 24) & 0xFF);

            pix1 = static_cast<uint8_t>((alphaPercent * r + (100 - alphaPercent) * pix1) / 100);
            pix2 = static_cast<uint8_t>((alphaPercent * g + (100 - alphaPercent) * pix2) / 100);
            pix3 = static_cast<uint8_t>((alphaPercent * b + (100 - alphaPercent) * pix3) / 100);
            // Keep alpha opaque — transparent fade pixels make SDL_RenderTexture show black.
            (void)a;
            (void)pix4;

            const uint32_t blended = pix1 | (static_cast<uint32_t>(pix2) << 8) |
                                   (static_cast<uint32_t>(pix3) << 16) |
                                   (static_cast<uint32_t>(255) << 24);
            DrawPixel(surface, i1, i2, blended);
        }
    }
}

void GraphicsUtils::EnsureSurfaceOpaque(SDL_Surface* surface) {
    if (!surface || !surface->pixels) return;
    const int w = surface->w;
    const int h = surface->h;
    if (w <= 0 || h <= 0) return;
    const int pitchPixels = surface->pitch / 4;
    uint32_t* row = static_cast<uint32_t*>(surface->pixels);
    for (int y = 0; y < h; ++y) {
        uint32_t* pixels = row + y * pitchPixels;
        for (int x = 0; x < w; ++x) {
            pixels[x] |= 0xFF000000u;
        }
    }
}

void GraphicsUtils::DrawRLE8(SDL_Surface* dest, int x, int y, const uint8_t* rawData, size_t dataSize, int shadow, float scale, int green, int red, int gray, int white) {
    if (!dest || !rawData) return;
    if (dataSize < 8) return;
    
    const uint8_t* ptr = rawData;
    
    int16_t w = ptr[0] | (ptr[1] << 8); ptr += 2;
    int16_t h = ptr[0] | (ptr[1] << 8); ptr += 2;
    // Pascal DrawRLE8Pic: xs/ys := header + 1
    int16_t xs = (ptr[0] | (ptr[1] << 8)) + 1; ptr += 2;
    int16_t ys = (ptr[0] | (ptr[1] << 8)) + 1; ptr += 2;
    
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
                    applyScenePalletTint(r, g, b);
                    color = mapRGB(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b));
                }

                if (!useTint && shadow == 0 && m_scenePalletTint > 0) {
                    int r = (color >> 16) & 0xFF;
                    int g = (color >> 8) & 0xFF;
                    int b = color & 0xFF;
                    applyScenePalletTint(r, g, b);
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
