#include <iostream>
#include <string>
#include <vector>

static uint16_t ReadU16LE(const std::string& s, size_t offset) {
    if (offset + 1 >= s.size()) return 0;
    return static_cast<uint16_t>(static_cast<uint8_t>(s[offset])) |
           (static_cast<uint16_t>(static_cast<uint8_t>(s[offset + 1])) << 8);
}

static bool IsDoubleSurname3Chars(uint16_t w0, uint16_t w2) {
    return (w0 == 0x6EAB && w2 == 0x63AE) ||
           (w0 == 0xE8A6 && w2 == 0xF9AA) ||
           (w0 == 0x46AA && w2 == 0xE8A4) ||
           (w0 == 0x4FA5 && w2 == 0xB0AA) ||
           (w0 == 0x7DBC && w2 == 0x65AE) ||
           (w0 == 0x71A5 && w2 == 0xA8B0) ||
           (w0 == 0xD1BD && w2 == 0xAFB8) ||
           (w0 == 0x71A5 && w2 == 0xC5AA) ||
           (w0 == 0xD3A4 && w2 == 0x76A5) ||
           (w0 == 0xBDA4 && w2 == 0x5DAE) ||
           (w0 == 0xDABC && w2 == 0xA7B6) ||
           (w0 == 0x43AD && w2 == 0xDFAB) ||
           (w0 == 0x71A5 && w2 == 0x7BAE) ||
           (w0 == 0xB9A7 && w2 == 0x43C3) ||
           (w0 == 0x61B0 && w2 == 0xD5C1) ||
           (w0 == 0x74A6 && w2 == 0xE5A4) ||
           (w0 == 0xDDA9 && w2 == 0x5BB6);
}

static std::string ExtractSurnameBytesGbk(const std::string& s) {
    if (s.empty()) return "";
    if (static_cast<uint8_t>(s[0]) < 0x80) return s.substr(0, 1);

    if (s.size() == 4) return s.substr(0, 2);

    if (s.size() == 6) {
        uint16_t w0 = ReadU16LE(s, 0);
        uint16_t w2 = ReadU16LE(s, 2);
        if (IsDoubleSurname3Chars(w0, w2)) return s.substr(0, 4);
        return s.substr(0, 2);
    }

    if (s.size() >= 8) return s.substr(0, 4);

    return (s.size() >= 2) ? s.substr(0, 2) : s;
}

static std::string ReplaceNewTalk0Placeholders(const std::string& text, const std::string& heroName) {
    std::string heroSurname = ExtractSurnameBytesGbk(heroName);
    std::string heroGiven = (heroName.size() > heroSurname.size()) ? heroName.substr(heroSurname.size()) : "";

    std::string out;
    for (size_t i = 0; i < text.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        if (i + 1 < text.size()) {
            unsigned char n = static_cast<unsigned char>(text[i + 1]);
            if (c == '*' && n == '*') { out.push_back('\n'); ++i; continue; }
            if (c == '&' && n == '&') { out += heroName; ++i; continue; }
            if (c == '%' && n == '%') { out += heroGiven; ++i; continue; }
            if (c == '$' && n == '$') { out += heroSurname; ++i; continue; }
        }
        out.push_back(static_cast<char>(c));
    }
    return out;
}

int main() {
    std::string hero3DoubleSurname = "\xAB\x6E\xAE\x63\xBD\xF0";
    std::string hero3SingleSurname = "\xBD\xF0\xD3\xB9\xCF\x88";

    std::string input = "[[$$]]-[[%%]]-[[&&]]\xA3\xA4**";
    std::string out1 = ReplaceNewTalk0Placeholders(input, hero3DoubleSurname);
    std::string out2 = ReplaceNewTalk0Placeholders(input, hero3SingleSurname);

    if (out1.find("\xAB\x6E\xAE\x63") == std::string::npos) return 1;
    if (out1.find("\xBD\xF0") == std::string::npos) return 2;
    if (out1.find("\xA3\xA4") == std::string::npos) return 3;

    if (out2.find("\xBD\xF0") == std::string::npos) return 4;
    if (out2.find("\xD3\xB9\xCF\x88") == std::string::npos) return 5;

    return 0;
}
