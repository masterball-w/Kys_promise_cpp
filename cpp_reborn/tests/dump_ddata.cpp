#include "FileLoader.h"
#include <iostream>
#include <vector>
#include <cstring>
#include <iomanip>
#include <string>
#include <set>

struct EventData {
    int16_t data[11]; // 0-10
};

struct SceneEventData {
    EventData data[200];
};

static std::vector<uint8_t> loadFirstFound(const std::vector<std::string>& candidates) {
    for (const auto& p : candidates) {
        auto data = FileLoader::loadFile(p);
        if (!data.empty()) {
            std::cout << "Loaded from " << p << std::endl;
            return data;
        }
    }
    return {};
}

static void dumpEventLine(int eventId, const int16_t* e) {
    int16_t cond = e[0];
    int16_t script = e[4];
    int16_t pic = e[5];
    int16_t defPic = e[7];
    int16_t y = e[9];
    int16_t x = e[10];
    std::cout << std::setw(3) << eventId
              << " | cond " << std::setw(3) << cond
              << " | script " << std::setw(6) << script
              << " | pic " << std::setw(6) << pic
              << " | def " << std::setw(6) << defPic
              << " | x,y " << std::setw(2) << x << "," << std::setw(2) << y
              << "\n";
}

int main(int argc, char** argv) {
    int sceneId = 0;
    int centerX = -1;
    int centerY = -1;
    int radius = 6;
    if (argc >= 2) sceneId = std::stoi(argv[1]);
    if (argc >= 4) {
        centerX = std::stoi(argv[2]);
        centerY = std::stoi(argv[3]);
    }
    if (argc >= 5) radius = std::max(0, std::stoi(argv[4]));

    std::cout << "Dumping Scene " << sceneId << " DData..." << std::endl;

    std::vector<uint8_t> grpData;

    grpData = loadFirstFound({
        "alldef.grp",
        "save/alldef.grp",
        "resource/alldef.grp",
        "../resource/alldef.grp",
        "../../resource/alldef.grp",
        "../save/alldef.grp",
        "../../save/alldef.grp"
    });
    
    if (grpData.empty()) {
        std::cerr << "Failed to load alldef.grp" << std::endl;
        return 1;
    }
    
    size_t sceneSize = 4400;
    size_t sceneCount = grpData.size() / sceneSize;
    if (grpData.size() < sceneSize || sceneId < 0 || (size_t)sceneId >= sceneCount) {
        std::cerr << "Invalid sceneId or alldef.grp too small. sceneId=" << sceneId << " scenes=" << sceneCount << std::endl;
        return 1;
    }
    
    const int16_t* ptr = reinterpret_cast<const int16_t*>(grpData.data());
    const int16_t* scenePtr = ptr + (sceneId * 200 * 11);
    
    int printed = 0;
    for (int eventId = 0; eventId < 200; ++eventId) {
        const int16_t* e = scenePtr + eventId * 11;
        int16_t cond = e[0];
        int16_t script = e[4];
        int16_t pic = e[5];
        int16_t defPic = e[7];
        int16_t y = e[9];
        int16_t x = e[10];

        bool interesting = (cond != 0) || (script != 0) || (pic != 0) || (defPic != 0) || (x != 0) || (y != 0);
        if (!interesting) continue;

        if (printed == 0) {
            std::cout << "event | cond | script  | pic    | def    | x,y\n";
        }
        dumpEventLine(eventId, e);
        printed++;
    }
    std::cout << "Interesting events: " << printed << "/200" << std::endl;

    if (centerX >= 0 && centerY >= 0) {
        std::cout << "\nDumping Scene " << sceneId << " SData Layer3 around (" << centerX << "," << centerY << ") r=" << radius << "..." << std::endl;
        auto mapData = loadFirstFound({
            "allsin.grp",
            "save/allsin.grp",
            "resource/allsin.grp",
            "../resource/allsin.grp",
            "../../resource/allsin.grp",
            "../save/allsin.grp",
            "../../save/allsin.grp"
        });
        if (mapData.empty()) {
            std::cerr << "Failed to load allsin.grp" << std::endl;
            return 1;
        }

        size_t sceneMapSize = 6 * 64 * 64 * 2;
        size_t mapSceneCount = mapData.size() / sceneMapSize;
        if (sceneId < 0 || (size_t)sceneId >= mapSceneCount) {
            std::cerr << "Invalid sceneId for allsin.grp. sceneId=" << sceneId << " scenes=" << mapSceneCount << std::endl;
            return 1;
        }

        const int16_t* sPtr = reinterpret_cast<const int16_t*>(mapData.data());
        const int16_t* sceneMapPtr = sPtr + (sceneId * 6 * 64 * 64);
        const int16_t* layer3Ptr = sceneMapPtr + (3 * 64 * 64);

        std::set<int> seenEventIds;
        for (int y = centerY - radius; y <= centerY + radius; ++y) {
            for (int x = centerX - radius; x <= centerX + radius; ++x) {
                int16_t tile = -1;
                if (x >= 0 && x < 64 && y >= 0 && y < 64) {
                    tile = layer3Ptr[x * 64 + y];
                }
                std::cout << std::setw(4) << tile << " ";
                if (tile >= 0 && tile < 200) seenEventIds.insert(tile);
            }
            std::cout << "\n";
        }

        if (!seenEventIds.empty()) {
            std::cout << "\nEvents referenced by Layer3 grid:\n";
            for (int eventId : seenEventIds) {
                const int16_t* e = scenePtr + eventId * 11;
                dumpEventLine(eventId, e);
            }
        } else {
            std::cout << "\n(No event IDs 0..199 found in this grid)\n";
        }
    }

    return 0;
}
