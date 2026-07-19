#pragma once
#include <SDL3/SDL.h>
#include <string>
#include <vector>

#define USE_SDL_TTF

#ifdef USE_SDL_TTF
#include <SDL3_ttf/SDL_ttf.h>
#endif

enum class TextEncoding {
    Auto,
    GBK,
    Big5
};

class TextManager {
public:
    static TextManager& getInstance();

    bool Init();
    void Quit();

    bool loadFont(const std::string& fontPath, int size);

    void RenderText(const std::string& text, int x, int y, uint32_t color);
    void RenderTextUtf8(const std::string& text, int x, int y, uint32_t color, int fontSize = 20);

    std::string gbkToUtf8(const std::string& gbkStr);
    std::string big5ToUtf8(const std::string& big5Str);
    std::string talkToUtf8(const std::string& talkBytes);
    std::string nameToUtf8(const std::string& ansiStr);
    std::string utf8ToGbk(const std::string& utf8Str);
    
    void setUseSimplifiedChinese(bool simplified) { m_useSimplified = simplified; }
    bool getUseSimplifiedChinese() const { return m_useSimplified; }
    
    void setTextEncoding(TextEncoding encoding) { m_textEncoding = encoding; }
    TextEncoding getTextEncoding() const { return m_textEncoding; }
    
    void loadConfig();
    
    std::string traditionalToSimplified(const std::string& utf8Str);
    std::string simplifiedToTraditional(const std::string& utf8Str);

private:
    TextManager();
    ~TextManager();

#ifdef USE_SDL_TTF
    TTF_Font* m_font;
#else
    void* m_font;
#endif
    
    bool m_useSimplified = true;
    TextEncoding m_textEncoding = TextEncoding::GBK;
};
