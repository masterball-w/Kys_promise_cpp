#include "GbkCodec.h"
#include <array>
#include <cstdint>
#include <unordered_map>

namespace {

constexpr int kGbkLeadMin = 0x81;
constexpr int kGbkTrailsPerLead = 190;

static const uint16_t kGbkToUnicode[] = {
#include "gbk_unicode_table.inc"
};

int trailIndex(uint8_t b) {
    if (b >= 0x40 && b <= 0x7E) return static_cast<int>(b) - 0x40;
    if (b >= 0x80 && b <= 0xFE) return static_cast<int>(b) - 0x80 + 63;
    return -1;
}

uint32_t gbkPairToUnicode(uint8_t lead, uint8_t trail) {
    if (lead < kGbkLeadMin) return 0;
    const int ti = trailIndex(trail);
    if (ti < 0) return 0;
    const size_t idx = static_cast<size_t>(lead - kGbkLeadMin) * kGbkTrailsPerLead + static_cast<size_t>(ti);
    if (idx >= sizeof(kGbkToUnicode) / sizeof(kGbkToUnicode[0])) return 0;
    return kGbkToUnicode[idx];
}

void appendUtf8(std::string& out, uint32_t cp) {
    if (cp <= 0x7F) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

bool decodeNextUtf8(const std::string& s, size_t& i, uint32_t& cp) {
    if (i >= s.size()) return false;
    const unsigned char c = static_cast<unsigned char>(s[i]);
    if (c < 0x80) {
        cp = c;
        i += 1;
        return true;
    }
    if ((c & 0xE0) == 0xC0 && i + 1 < s.size()) {
        cp = ((c & 0x1F) << 6) | (static_cast<unsigned char>(s[i + 1]) & 0x3F);
        i += 2;
        return true;
    }
    if ((c & 0xF0) == 0xE0 && i + 2 < s.size()) {
        cp = ((c & 0x0F) << 12) |
             ((static_cast<unsigned char>(s[i + 1]) & 0x3F) << 6) |
             (static_cast<unsigned char>(s[i + 2]) & 0x3F);
        i += 3;
        return true;
    }
    if ((c & 0xF8) == 0xF0 && i + 3 < s.size()) {
        cp = ((c & 0x07) << 18) |
             ((static_cast<unsigned char>(s[i + 1]) & 0x3F) << 12) |
             ((static_cast<unsigned char>(s[i + 2]) & 0x3F) << 6) |
             (static_cast<unsigned char>(s[i + 3]) & 0x3F);
        i += 4;
        return true;
    }
    i += 1;
    return false;
}

const std::unordered_map<uint32_t, std::array<uint8_t, 2>>& reverseMap() {
    static std::unordered_map<uint32_t, std::array<uint8_t, 2>> map;
    static bool built = false;
    if (!built) {
        for (int lead = kGbkLeadMin; lead < 0x100; ++lead) {
            for (int ti = 0; ti < kGbkTrailsPerLead; ++ti) {
                const size_t idx = static_cast<size_t>(lead - kGbkLeadMin) * kGbkTrailsPerLead + static_cast<size_t>(ti);
                const uint32_t cp = kGbkToUnicode[idx];
                if (cp == 0) continue;
                const uint8_t trail = (ti < 63) ? static_cast<uint8_t>(ti + 0x40)
                                                  : static_cast<uint8_t>(ti - 63 + 0x80);
                map[cp] = {static_cast<uint8_t>(lead), trail};
            }
        }
        built = true;
    }
    return map;
}

}  // namespace

namespace GbkCodec {

std::string toUtf8(const std::string& gbkBytes) {
    std::string out;
    out.reserve(gbkBytes.size() * 2);
    for (size_t i = 0; i < gbkBytes.size();) {
        const uint8_t b0 = static_cast<uint8_t>(gbkBytes[i]);
        if (b0 < 0x80) {
            out.push_back(static_cast<char>(b0));
            ++i;
            continue;
        }
        if (i + 1 >= gbkBytes.size()) break;
        const uint8_t b1 = static_cast<uint8_t>(gbkBytes[i + 1]);
        const uint32_t cp = gbkPairToUnicode(b0, b1);
        if (cp == 0) {
            ++i;
            continue;
        }
        appendUtf8(out, cp);
        i += 2;
    }
    return out;
}

std::string fromUtf8(const std::string& utf8Text) {
    const auto& rev = reverseMap();
    std::string out;
    out.reserve(utf8Text.size());
    for (size_t i = 0; i < utf8Text.size();) {
        uint32_t cp = 0;
        if (!decodeNextUtf8(utf8Text, i, cp)) continue;
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
            continue;
        }
        const auto it = rev.find(cp);
        if (it != rev.end()) {
            out.push_back(static_cast<char>(it->second[0]));
            out.push_back(static_cast<char>(it->second[1]));
        }
    }
    return out;
}

}  // namespace GbkCodec
