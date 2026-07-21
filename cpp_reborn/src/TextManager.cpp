#include "TextManager.h"
#include "GameManager.h"
#include "FileLoader.h"
#include "GbkCodec.h"
#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstdint>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {
    bool ContainsRareCJK(const std::string& utf8) {
        for (size_t i = 0; i < utf8.size(); ) {
            unsigned char c = static_cast<unsigned char>(utf8[i]);
            int charLen = 1;
            uint32_t codepoint = 0;
            
            if ((c & 0x80) == 0) {
                codepoint = c;
                charLen = 1;
            } else if ((c & 0xE0) == 0xC0) {
                if (i + 1 >= utf8.size()) break;
                codepoint = ((c & 0x1F) << 6) | (utf8[i + 1] & 0x3F);
                charLen = 2;
            } else if ((c & 0xF0) == 0xE0) {
                if (i + 2 >= utf8.size()) break;
                codepoint = ((c & 0x0F) << 12) | ((utf8[i + 1] & 0x3F) << 6) | (utf8[i + 2] & 0x3F);
                charLen = 3;
            } else if ((c & 0xF8) == 0xF0) {
                if (i + 3 >= utf8.size()) break;
                codepoint = ((c & 0x07) << 18) | ((utf8[i + 1] & 0x3F) << 12) | ((utf8[i + 2] & 0x3F) << 6) | (utf8[i + 3] & 0x3F);
                charLen = 4;
            } else {
                i++;
                continue;
            }
            
            if (codepoint >= 0x3400 && codepoint <= 0x4DBF) {
                return true;
            }
            if (codepoint >= 0x20000 && codepoint <= 0x2A6DF) {
                return true;
            }
            if (codepoint >= 0x2A700 && codepoint <= 0x2B73F) {
                return true;
            }
            if (codepoint >= 0x2B740 && codepoint <= 0x2B81F) {
                return true;
            }
            if (codepoint >= 0x2B820 && codepoint <= 0x2CEAF) {
                return true;
            }
            
            i += charLen;
        }
        return false;
    }

    bool IsStandaloneTrailingFeForGbk(const std::string& s) {
        if (s.empty()) return false;
        if (static_cast<unsigned char>(s.back()) != 0xFE) return false;

        size_t i = 0;
        bool lastWasTwoByte = false;
        while (i < s.size()) {
            unsigned char b0 = static_cast<unsigned char>(s[i]);
            if (b0 < 0x80) {
                i += 1;
                lastWasTwoByte = false;
                continue;
            }

            if (i + 1 < s.size()) {
                unsigned char b1 = static_cast<unsigned char>(s[i + 1]);
                if (b1 >= 0x40 && b1 <= 0xFE && b1 != 0x7F) {
                    i += 2;
                    lastWasTwoByte = true;
                    continue;
                }
            }

            i += 1;
            lastWasTwoByte = false;
        }

        return !lastWasTwoByte;
    }

    bool IsStandaloneTrailingFeForBig5(const std::string& s) {
        if (s.empty()) return false;
        if (static_cast<unsigned char>(s.back()) != 0xFE) return false;

        auto isBig5Trail = [](unsigned char b) -> bool {
            return (b >= 0x40 && b <= 0x7E) || (b >= 0xA1 && b <= 0xFE);
        };

        size_t i = 0;
        bool lastWasTwoByte = false;
        while (i < s.size()) {
            unsigned char b0 = static_cast<unsigned char>(s[i]);
            if (b0 < 0x80) {
                i += 1;
                lastWasTwoByte = false;
                continue;
            }

            if (i + 1 < s.size()) {
                unsigned char b1 = static_cast<unsigned char>(s[i + 1]);
                if (isBig5Trail(b1)) {
                    i += 2;
                    lastWasTwoByte = true;
                    continue;
                }
            }

            i += 1;
            lastWasTwoByte = false;
        }

        return !lastWasTwoByte;
    }

    std::string TrimStringTerminatorBytes(std::string s, uint32_t codePage) {
        size_t zeroPos = s.find('\0');
        if (zeroPos != std::string::npos) s.resize(zeroPos);
        while (!s.empty()) {
            unsigned char b = static_cast<unsigned char>(s.back());
            if (b == 0x00 || b == 0xFF) {
                s.pop_back();
                continue;
            }
            if (b == 0xFE) {
                bool standalone = false;
                if (codePage == 936 || codePage == 54936) standalone = IsStandaloneTrailingFeForGbk(s);
                else if (codePage == 950) standalone = IsStandaloneTrailingFeForBig5(s);

                if (standalone) {
                    s.pop_back();
                    continue;
                }
                break;
            }
            break;
        }
        return s;
    }

#ifdef _WIN32
    std::string ConvertMultiByteToUtf8PascalStyle(UINT codePage, const std::string& bytes) {
        if (bytes.empty()) return "";

        const char* p = bytes.c_str();
        int wideLen = MultiByteToWideChar(codePage, 0, p, -1, NULL, 0);
        if (wideLen <= 0) return "";

        std::vector<wchar_t> wBuf(static_cast<size_t>(wideLen));
        if (MultiByteToWideChar(codePage, 0, p, -1, wBuf.data(), wideLen) == 0) return "";

        int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wBuf.data(), -1, NULL, 0, NULL, NULL);
        if (utf8Len <= 0) return "";

        std::string out(static_cast<size_t>(utf8Len), '\0');
        if (WideCharToMultiByte(CP_UTF8, 0, wBuf.data(), -1, out.data(), utf8Len, NULL, NULL) == 0) return "";

        if (!out.empty() && out.back() == '\0') out.pop_back();
        return out;
    }
#endif
}

TextManager& TextManager::getInstance() {
    static TextManager instance;
    return instance;
}

TextManager::TextManager() : m_font(nullptr) {
}

TextManager::~TextManager() {
    Quit();
}

bool TextManager::Init() {
#ifdef USE_SDL_TTF
    if (!TTF_Init()) {
        std::cerr << "TTF_Init failed: " << SDL_GetError() << std::endl;
        return false;
    }
#else
    std::cout << "TextManager: SDL_ttf not enabled. Text rendering will be simulated." << std::endl;
#endif
    loadConfig();
    return true;
}

void TextManager::Quit() {
#ifdef USE_SDL_TTF
    if (m_font) {
        TTF_CloseFont(m_font);

        m_font = nullptr;
    }
    TTF_Quit();
#endif
}

bool TextManager::loadFont(const std::string& fontPath, int size) {
#ifdef USE_SDL_TTF
    if (m_font) {
        TTF_CloseFont(m_font);
    }
    m_font = TTF_OpenFont(fontPath.c_str(), static_cast<float>(size));
    if (!m_font) {
        std::cerr << "Failed to load font: " << fontPath << " Error: " << SDL_GetError() << std::endl;
        return false;
    }
    return true;
#else
    std::cout << "TextManager: Loading font (simulated): " << fontPath << std::endl;
    return true;
#endif
}

std::string TextManager::gbkToUtf8(const std::string& gbkStr) {
    if (gbkStr.empty()) return "";

#ifdef _WIN32
    std::string utf8;
    
    if (m_textEncoding == TextEncoding::Big5) {
        std::string cleaned = TrimStringTerminatorBytes(gbkStr, 950);
        if (!cleaned.empty()) {
            utf8 = ConvertMultiByteToUtf8PascalStyle(950, cleaned);
        }
    } else if (m_textEncoding == TextEncoding::GBK) {
        std::string cleaned = TrimStringTerminatorBytes(gbkStr, 54936);
        if (!cleaned.empty()) {
            utf8 = ConvertMultiByteToUtf8PascalStyle(54936, cleaned);
        }
    } else {
        std::string cleaned = TrimStringTerminatorBytes(gbkStr, 54936);
        if (cleaned.empty()) return "";
        utf8 = ConvertMultiByteToUtf8PascalStyle(54936, cleaned);
        
        if (!utf8.empty() && ContainsRareCJK(utf8)) {
            std::string big5Cleaned = TrimStringTerminatorBytes(gbkStr, 950);
            std::string big5Utf8 = ConvertMultiByteToUtf8PascalStyle(950, big5Cleaned);
            if (!big5Utf8.empty() && !ContainsRareCJK(big5Utf8)) {
                utf8 = big5Utf8;
            }
        }
    }
    if (utf8.empty()) return "";
    
    if (m_useSimplified) {
        utf8 = traditionalToSimplified(utf8);
    }
    
    return utf8;
#else
    std::string cleaned = TrimStringTerminatorBytes(gbkStr, 54936);
    if (cleaned.empty()) return "";

    std::string utf8 = GbkCodec::toUtf8(cleaned);
    if (utf8.empty()) {
        auto tryIconv = [](const char* to, const char* from, const std::string& in) -> std::string {
            char* out = SDL_iconv_string(to, from, in.c_str(), in.size() + 1);
            if (!out) return {};
            std::string s(out);
            SDL_free(out);
            if (!s.empty() && s.back() == '\0') s.pop_back();
            return s;
        };
        if (m_textEncoding == TextEncoding::Big5) {
            utf8 = tryIconv("UTF-8", "BIG5", cleaned);
            if (utf8.empty()) utf8 = tryIconv("UTF-8", "CP950", cleaned);
        } else {
            utf8 = tryIconv("UTF-8", "GB18030", cleaned);
            if (utf8.empty()) utf8 = tryIconv("UTF-8", "GBK", cleaned);
            if (utf8.empty()) utf8 = tryIconv("UTF-8", "CP936", cleaned);
        }
    }
    if (utf8.empty()) return "";
    if (m_useSimplified) utf8 = traditionalToSimplified(utf8);
    return utf8;
#endif
}

std::string TextManager::big5ToUtf8(const std::string& big5Str) {
    if (big5Str.empty()) return "";

#ifdef _WIN32
    std::string cleaned = TrimStringTerminatorBytes(big5Str, 950);
    if (cleaned.empty()) return "";
    std::string utf8 = ConvertMultiByteToUtf8PascalStyle(950, cleaned);
    if (utf8.empty()) return cleaned;
    
    if (m_useSimplified) {
        utf8 = traditionalToSimplified(utf8);
    }
    
    return utf8;
#else
    std::string cleaned = TrimStringTerminatorBytes(big5Str, 950);
    if (cleaned.empty()) return "";
    char* out = SDL_iconv_string("UTF-8", "BIG5", cleaned.c_str(), cleaned.size() + 1);
    if (!out) out = SDL_iconv_string("UTF-8", "CP950", cleaned.c_str(), cleaned.size() + 1);
    if (!out) return "";
    std::string utf8(out);
    SDL_free(out);
    if (!utf8.empty() && utf8.back() == '\0') utf8.pop_back();
    if (m_useSimplified) utf8 = traditionalToSimplified(utf8);
    return utf8;
#endif
}

std::string TextManager::talkToUtf8(const std::string& talkBytes) {
    if (talkBytes.empty()) return "";

    auto hasReplacement = [](const std::string& s) -> bool {
        return s.find("\xEF\xBF\xBD") != std::string::npos;
    };

#ifdef _WIN32
    if (m_textEncoding == TextEncoding::GBK) {
        return gbkToUtf8(talkBytes);
    }
    if (m_textEncoding == TextEncoding::Big5) {
        return big5ToUtf8(talkBytes);
    }

    // Auto: prefer GBK for this game's talk.grp (see editor talk codec)
    std::string gbkUtf8 = gbkToUtf8(talkBytes);
    if (!gbkUtf8.empty() && !hasReplacement(gbkUtf8) && !ContainsRareCJK(gbkUtf8)) {
        return gbkUtf8;
    }
    std::string big5Utf8 = big5ToUtf8(talkBytes);
    if (!big5Utf8.empty() && !hasReplacement(big5Utf8) && !ContainsRareCJK(big5Utf8)) {
        return big5Utf8;
    }
    if (!gbkUtf8.empty() && !hasReplacement(gbkUtf8)) return gbkUtf8;
    if (!big5Utf8.empty()) return big5Utf8;
    return gbkUtf8;
#else
    if (m_textEncoding == TextEncoding::Big5) return big5ToUtf8(talkBytes);
    if (m_textEncoding == TextEncoding::GBK) return gbkToUtf8(talkBytes);
    std::string gbkUtf8 = gbkToUtf8(talkBytes);
    if (!gbkUtf8.empty() && !hasReplacement(gbkUtf8) && !ContainsRareCJK(gbkUtf8)) return gbkUtf8;
    std::string big5Utf8 = big5ToUtf8(talkBytes);
    if (!big5Utf8.empty() && !hasReplacement(big5Utf8)) return big5Utf8;
    return !gbkUtf8.empty() ? gbkUtf8 : big5Utf8;
#endif
}

std::string TextManager::nameToUtf8(const std::string& ansiStr) {
    if (ansiStr.empty()) return "";

    return gbkToUtf8(ansiStr);
}

std::string TextManager::utf8ToGbk(const std::string& utf8Str) {
    if (utf8Str.empty()) return "";

#ifdef _WIN32
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, NULL, 0);
    if (wideLen == 0) return utf8Str;

    std::vector<wchar_t> wBuf(wideLen);
    MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, wBuf.data(), wideLen);

    int len = WideCharToMultiByte(936, 0, wBuf.data(), -1, NULL, 0, NULL, NULL);
    if (len == 0) return utf8Str;
    std::vector<char> buf(len);
    if (WideCharToMultiByte(936, 0, wBuf.data(), -1, buf.data(), len, NULL, NULL) == 0) return utf8Str;
    std::string ansi(buf.data());
    if (!ansi.empty()) return ansi;
    return utf8Str;
#else
    std::string gbk = GbkCodec::fromUtf8(utf8Str);
    if (!gbk.empty()) return gbk;
    char* out = SDL_iconv_string("GBK", "UTF-8", utf8Str.c_str(), utf8Str.size() + 1);
    if (!out) out = SDL_iconv_string("GB18030", "UTF-8", utf8Str.c_str(), utf8Str.size() + 1);
    if (!out) out = SDL_iconv_string("CP936", "UTF-8", utf8Str.c_str(), utf8Str.size() + 1);
    if (!out) return utf8Str;
    std::string ansi(out);
    SDL_free(out);
    if (!ansi.empty() && ansi.back() == '\0') ansi.pop_back();
    return ansi;
#endif
}

void TextManager::RenderText(const std::string& text, int x, int y, uint32_t color) {
    std::string utf8Text = gbkToUtf8(text);
    RenderTextUtf8(utf8Text, x, y, color);
}

void TextManager::RenderTextUtf8(const std::string& text, int x, int y, uint32_t color, int fontSize) {
    if (text.empty()) return;

    SDL_Color sdlColor;
    sdlColor.r = (color >> 16) & 0xFF;
    sdlColor.g = (color >> 8) & 0xFF;
    sdlColor.b = color & 0xFF;
    sdlColor.a = (color >> 24) & 0xFF;

#ifdef USE_SDL_TTF
    if (!m_font) return;
    
    SDL_Surface* textSurface = TTF_RenderText_Blended(m_font, text.c_str(), 0, sdlColor);
    if (textSurface) {
        SDL_Surface* screen = GameManager::getInstance().getScreenSurface();
        if (screen) {
            SDL_Rect destRect = { x, y, textSurface->w, textSurface->h };
            SDL_BlitSurface(textSurface, NULL, screen, &destRect);
        }
        SDL_DestroySurface(textSurface);
    }
#else
#endif
}

void TextManager::loadConfig() {
    std::string configPath = FileLoader::getDataRoot() + "kys_config.ini";
    std::ifstream file(configPath);
    if (!file.is_open()) {
        file.open("kys_config.ini");
    }
    if (!file.is_open()) {
        std::cout << "[TextManager] No kys_config.ini found, using defaults" << std::endl;
        return;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        
        size_t eqPos = line.find('=');
        if (eqPos == std::string::npos) continue;
        
        std::string key = line.substr(0, eqPos);
        std::string value = line.substr(eqPos + 1);
        
        while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
        while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value = value.substr(1);
        
        if (key == "simplified" || key == "Simplified") {
            m_useSimplified = (value == "1" || value == "true" || value == "True");
            std::cout << "[TextManager] Simplified mode: " << (m_useSimplified ? "true" : "false") << std::endl;
        }
        else if (key == "encoding" || key == "Encoding") {
            if (value == "gbk" || value == "GBK") {
                m_textEncoding = TextEncoding::GBK;
            } else if (value == "big5" || value == "Big5") {
                m_textEncoding = TextEncoding::Big5;
            } else {
                m_textEncoding = TextEncoding::Auto;
            }
            std::cout << "[TextManager] Text encoding: " << value << std::endl;
        }
    }
    
    file.close();
}

std::string TextManager::traditionalToSimplified(const std::string& utf8Str) {
    if (utf8Str.empty() || !m_useSimplified) return utf8Str;
    
#ifdef _WIN32
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, NULL, 0);
    if (wideLen <= 0) return utf8Str;
    
    std::vector<wchar_t> wBuf(wideLen);
    MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, wBuf.data(), wideLen);
    
    LCID lcid = MAKELCID(MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED), SORT_CHINESE_PRCP);
    
    int simplifiedLen = LCMapStringW(lcid, 
                                      LCMAP_SIMPLIFIED_CHINESE,
                                      wBuf.data(), wideLen,
                                      NULL, 0);
    if (simplifiedLen <= 0) return utf8Str;
    
    std::vector<wchar_t> simplifiedBuf(simplifiedLen);
    LCMapStringW(lcid,
                 LCMAP_SIMPLIFIED_CHINESE,
                 wBuf.data(), wideLen,
                 simplifiedBuf.data(), simplifiedLen);
    
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, simplifiedBuf.data(), -1, NULL, 0, NULL, NULL);
    if (utf8Len <= 0) return utf8Str;
    
    std::string result(utf8Len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, simplifiedBuf.data(), -1, &result[0], utf8Len, NULL, NULL);
    
    return result;
#else
    return utf8Str;
#endif
}

std::string TextManager::simplifiedToTraditional(const std::string& utf8Str) {
    if (utf8Str.empty()) return utf8Str;
    
#ifdef _WIN32
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, NULL, 0);
    if (wideLen <= 0) return utf8Str;
    
    std::vector<wchar_t> wBuf(wideLen);
    MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, wBuf.data(), wideLen);
    
    LCID lcid = MAKELCID(MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED), SORT_CHINESE_PRCP);
    
    int traditionalLen = LCMapStringW(lcid,
                                       LCMAP_TRADITIONAL_CHINESE,
                                       wBuf.data(), wideLen,
                                       NULL, 0);
    if (traditionalLen <= 0) return utf8Str;
    
    std::vector<wchar_t> traditionalBuf(traditionalLen);
    LCMapStringW(lcid,
                 LCMAP_TRADITIONAL_CHINESE,
                 wBuf.data(), wideLen,
                 traditionalBuf.data(), traditionalLen);
    
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, traditionalBuf.data(), -1, NULL, 0, NULL, NULL);
    if (utf8Len <= 0) return utf8Str;
    
    std::string result(utf8Len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, traditionalBuf.data(), -1, &result[0], utf8Len, NULL, NULL);
    
    return result;
#else
    return utf8Str;
#endif
}
