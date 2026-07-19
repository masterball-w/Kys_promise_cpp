#include "FileLoader.h"
#include <iostream>
#include <vector>
#include <iomanip>
#include <cstring>
#include <string>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {
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

    std::string TrimStringTerminatorBytes(std::string s) {
        size_t zeroPos = s.find('\0');
        if (zeroPos != std::string::npos) s.resize(zeroPos);
        while (!s.empty()) {
            unsigned char b = static_cast<unsigned char>(s.back());
            if (b == 0x00 || b == 0xFF) {
                s.pop_back();
                continue;
            }
            if (b == 0xFE) {
                if (IsStandaloneTrailingFeForGbk(s)) {
                    s.pop_back();
                    continue;
                }
                break;
            }
            break;
        }
        return s;
    }

    std::string GbkToUtf8Strict(const std::string& gbkBytes) {
        if (gbkBytes.empty()) return "";

#ifdef _WIN32
        std::string cleaned = TrimStringTerminatorBytes(gbkBytes);
        if (cleaned.empty()) return "";

        int wideLen = MultiByteToWideChar(936, 0, cleaned.c_str(), -1, NULL, 0);
        if (wideLen <= 0) return "";
        std::vector<wchar_t> wBuf(static_cast<size_t>(wideLen));
        if (MultiByteToWideChar(936, 0, cleaned.c_str(), -1, wBuf.data(), wideLen) == 0) return "";

        int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wBuf.data(), -1, NULL, 0, NULL, NULL);
        if (utf8Len <= 0) return "";
        std::string out(static_cast<size_t>(utf8Len), '\0');
        if (WideCharToMultiByte(CP_UTF8, 0, wBuf.data(), -1, out.data(), utf8Len, NULL, NULL) == 0) return "";
        if (!out.empty() && out.back() == '\0') out.pop_back();
        return out;
#else
        return gbkBytes;
#endif
    }
}

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    std::cout << "Scanning events for Pan(25) -> NewTalk0(68) pattern..." << std::endl;

    // Load kdef.idx and kdef.grp
    // Try multiple paths
    std::string paths[] = {
        "kdef.idx", 
        "../resource/kdef.idx", 
        "../../resource/kdef.idx",
        "d:/program/misc/kys-promise-main/resource/kdef.idx"
    };
    
    std::vector<uint8_t> idxData, grpData;
    
    for (const auto& p : paths) {
        idxData = FileLoader::loadFile(p);
        if (!idxData.empty()) {
            std::cout << "Loaded idx from " << p << std::endl;
            std::string grpPath = p;
            grpPath.replace(grpPath.find(".idx"), 4, ".grp");
            grpData = FileLoader::loadFile(grpPath);
            break;
        }
    }

    if (idxData.empty() || grpData.empty()) {
        std::cerr << "Failed to load kdef files." << std::endl;
        return 1;
    }

    std::vector<int16_t> eventScripts(grpData.size() / 2);
    std::memcpy(eventScripts.data(), grpData.data(), grpData.size());

    std::vector<int32_t> eventIndices(idxData.size() / 4);
    std::memcpy(eventIndices.data(), idxData.data(), idxData.size());

    int eventCount = eventIndices.size();
    std::cout << "Total Events: " << eventCount << std::endl;

    auto loadTalk = [&](std::vector<int32_t>& outIdx, std::vector<uint8_t>& outGrp) -> bool {
        std::string basePaths[] = {
            "talk.idx",
            "../resource/talk.idx",
            "../../resource/talk.idx",
            "resource/talk.idx",
            "d:/program/misc/kys-promise-main/resource/talk.idx"
        };

        std::vector<uint8_t> idx;
        std::vector<uint8_t> grp;
        for (const auto& p : basePaths) {
            idx = FileLoader::loadFile(p);
            if (!idx.empty()) {
                std::string grpPath = p;
                auto pos = grpPath.rfind(".idx");
                if (pos != std::string::npos) grpPath.replace(pos, 4, ".grp");
                grp = FileLoader::loadFile(grpPath);
                if (!grp.empty()) {
                    std::cout << "Loaded talk from " << p << std::endl;
                    break;
                }
            }
        }

        if (idx.empty() || grp.empty() || (idx.size() % 4 != 0)) return false;

        outIdx.resize(idx.size() / 4);
        std::memcpy(outIdx.data(), idx.data(), idx.size());
        outGrp = std::move(grp);
        return true;
    };

    auto decodeTalkPascal = [&](const std::vector<int32_t>& talkIdx, const std::vector<uint8_t>& talkGrp, int talkNum) -> std::string {
        if (talkIdx.empty() || talkGrp.empty()) return "";
        int actual = (talkNum > 0) ? (talkNum - 1) : 0;
        if (actual < 0 || actual >= (int)talkIdx.size()) return "";
        int offset = talkIdx[actual];
        int nextOffset = (actual + 1 < (int)talkIdx.size()) ? talkIdx[actual + 1] : (int)talkGrp.size();
        int len = nextOffset - offset;
        if (len <= 0 || offset < 0 || offset + len > (int)talkGrp.size()) return "";
        if (len > 2000) len = 2000;

        std::string bytes;
        bytes.reserve((size_t)len + 1);
        for (int i = 0; i < len; ++i) {
            uint8_t b = talkGrp[offset + i] ^ 0xFF;
            if (b == 0xFF) b = 0;
            if (b == 0) break;
            bytes.push_back((char)b);
        }
        return GbkToUtf8Strict(bytes);
    };

    auto findEventByPc = [&](int pcWordIndex) -> int {
        int pcByte = pcWordIndex * 2;
        for (int i = 0; i < (int)eventIndices.size(); ++i) {
            int start = eventIndices[i];
            int end = (i + 1 < (int)eventIndices.size()) ? eventIndices[i + 1] : (int)(eventScripts.size() * 2);
            if (pcByte >= start && pcByte < end) return i + 1;
        }
        return -1;
    };

    std::vector<int32_t> talkIdx;
    std::vector<uint8_t> talkGrp;
    bool talkOk = loadTalk(talkIdx, talkGrp);

    int targetPcs[] = { 637, 638 };
    for (int pcWord : targetPcs) {
        int eventId = findEventByPc(pcWord);
        std::cout << "PC " << pcWord << " -> Event " << eventId << std::endl;
        if (eventId <= 0) continue;

        int startByte = eventIndices[eventId - 1];
        int rel = pcWord - (startByte / 2);
        if (pcWord < 0 || pcWord >= (int)eventScripts.size()) continue;
        int16_t op = eventScripts[pcWord];
        std::cout << "  Op=" << op << " Rel=" << rel << std::endl;

        if (!talkOk) continue;

        if (op == 1) {
            if (pcWord + 3 < (int)eventScripts.size()) {
                int talkNum = eventScripts[pcWord + 1];
                std::string s = decodeTalkPascal(talkIdx, talkGrp, talkNum);
                std::cout << "  TalkID=" << talkNum << " Text=" << s << std::endl;
            }
        } else if (op == 68) {
            if (pcWord + 7 < (int)eventScripts.size()) {
                int talkNum = eventScripts[pcWord + 2];
                std::string s = decodeTalkPascal(talkIdx, talkGrp, talkNum);
                std::cout << "  TalkNum=" << talkNum << " Text=" << s << std::endl;
            }
        }
    }

    if (talkOk) {
        int hit = 0;
        for (int i = 1; i <= (int)talkIdx.size(); ++i) {
            std::string s = decodeTalkPascal(talkIdx, talkGrp, i);
            if (s.find(u8"過") != std::string::npos) {
                std::cout << "Found TalkID=" << i << " => " << s << std::endl;
                hit++;
                if (hit >= 20) break;
            }
        }
        if (hit == 0) {
            std::cout << "No dialogue contains \"過\"." << std::endl;
        }
    }

    // Dump Event 101
    int eventId = 101;
    if (eventId > 0 && eventId <= eventCount) {
        int offset = eventIndices[eventId - 1];
        int nextOffset = (eventId < eventCount) ? eventIndices[eventId] : eventScripts.size() * 2;
        int len = (nextOffset - offset) / 2;
        int start = offset / 2;
        
        std::cout << "\n--- Dumping Event " << eventId << " ---\n";
        int pc = start;
        int end = start + len;
        
        while (pc < end) {
            int16_t op = eventScripts[pc++];
            std::cout << "PC " << (pc-1) << ": Op " << op;
            
            // Decode args roughly
            if (op == 50) { // PlaySound
                 int arg = eventScripts[pc++];
                 std::cout << " (PlaySound " << arg << ")";
            }
            else if (op == 1) { // Dialogue
                int talkId = eventScripts[pc++];
                int headId = eventScripts[pc++];
                int mode = eventScripts[pc++];
                std::cout << " (Dialogue TalkID=" << talkId << " Head=" << headId << " Mode=" << mode << ")";
            }
            else if (op == 68) { // NewTalk0
                std::cout << " (NewTalk0 args: ";
                for(int k=0; k<7; ++k) std::cout << eventScripts[pc++] << " ";
                std::cout << ")";
            }
            else if (op == 3) { // ModifyEvent
                std::cout << " (ModifyEvent args: ";
                for(int k=0; k<13; ++k) std::cout << eventScripts[pc++] << " ";
                std::cout << ")";
            }
            else if (op == 25) { // Instruct_25(x1, y1, x2, y2)
                 int x1 = eventScripts[pc++];
                 int y1 = eventScripts[pc++];
                 int x2 = eventScripts[pc++];
                 int y2 = eventScripts[pc++];
                 std::cout << " (Pan Screen: " << x1 << "," << y1 << " -> " << x2 << "," << y2 << ")";
            }
            else if (op == 0) {
                 std::cout << " (Redraw/End)";
                 // Check next
                 if (pc < end) {
                     int16_t next = eventScripts[pc];
                     std::cout << " [Next: " << next << "]";
                 }
            }
            // Add more if needed
            
            std::cout << std::endl;
            
            if (op < 0) {
                std::cout << "WARNING: Negative Opcode " << op << std::endl;
            }
        }
    }
    
    // Assume Script ID is something?
    // Or scan all events for one that uses "Jin Xiansheng" dialogue?
    // Jin Xiansheng name? "金先生".
    // I can't search for Chinese string easily without encoding.
    
    // Let's dump all events that look like "Jin Xiansheng" (Dialogue with specific HeadID?)
    // Need to know Jin Xiansheng's HeadID.
    
    // Instead, I will implement a "CheckAutoEvents" simulation.
    // Iterate all events in Scene 52 (if I had DData).
    
    // Check for Event 2235 logic
    std::vector<int> checkEvents = {2235, 2234};
    
    for (int eventId : checkEvents) {
        if (eventId > eventIndices.size()) continue;

        int offset = eventIndices[eventId - 1];
        int nextOffset = (eventId < eventIndices.size()) ? eventIndices[eventId] : eventScripts.size() * 2;
        int lenWords = (nextOffset - offset) / 2;
        int start = offset / 2;

        std::cout << "\nEvent " << eventId << " (Length: " << lenWords << "):" << std::endl;
        
        for (int i = 0; i < lenWords; ++i) {
            int16_t opcode = eventScripts[start + i];
            std::cout << "[" << i << "] Op: " << opcode;

            if (opcode == 68) { // NewTalk0
                std::cout << " (NewTalk0) Args: ";
                for (int k = 1; k <= 7; ++k) std::cout << eventScripts[start + i + k] << " ";
                i += 7; 
            } else if (opcode == 1) { // Dialogue
                std::cout << " (Dialogue) Args: ";
                for (int k = 1; k <= 3; ++k) std::cout << eventScripts[start + i + k] << " ";
                i += 3;
            } else if (opcode == 3) { // ModEvent
                std::cout << " (ModEvent) Args: ";
                // Check if this modifies Scene 52 Event 1
                int s = eventScripts[start + i + 1];
                int e = eventScripts[start + i + 2];
                int cond = eventScripts[start + i + 3];
                std::cout << "S=" << s << " E=" << e << " Cond=" << cond << " ";
                
                for (int k = 1; k <= 13; ++k) std::cout << eventScripts[start + i + k] << " ";
                i += 13;
            } else if (opcode == 25) { // Pan
                int x1 = eventScripts[start + i + 1];
                int y1 = eventScripts[start + i + 2];
                int x2 = eventScripts[start + i + 3];
                int y2 = eventScripts[start + i + 4];
                std::cout << " (Pan) Args: " << x1 << " " << y1 << " " << x2 << " " << y2;
                i += 4;
            } else if (opcode == 0) {
                std::cout << " (Redraw/Exit?)";
            }
            
            std::cout << std::endl;
        }
    }

    return 0;
}
