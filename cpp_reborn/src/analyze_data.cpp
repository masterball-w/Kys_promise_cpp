#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <iomanip>
#include <cstdint>
#include <windows.h>
#include <filesystem>

namespace fs = std::filesystem;

// Constants
constexpr int DDATA_EVENT_COUNT = 200;
constexpr int DDATA_INTS_PER_EVENT = 11;
constexpr int DDATA_SCENE_SIZE = DDATA_EVENT_COUNT * DDATA_INTS_PER_EVENT * 2; // 4400 bytes

constexpr int SDATA_LAYERS = 6;
constexpr int SDATA_MAP_SIZE = 64;
constexpr int SDATA_LAYER_SIZE = SDATA_MAP_SIZE * SDATA_MAP_SIZE * 2; // 8192 bytes
constexpr int SDATA_SCENE_SIZE = SDATA_LAYERS * SDATA_LAYER_SIZE; // 49152 bytes

void analyze_alldef(const std::string& path) {
    std::cout << "\n=== Analyzing Event Data (alldef.grp) ===\n";
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "Error: Could not open " << path << "\n";
        return;
    }
    
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    
    int numScenes = fileSize / DDATA_SCENE_SIZE;
    std::cout << "File Size: " << fileSize << " bytes\n";
    std::cout << "Detected Scenes: " << numScenes << "\n";
    
    std::vector<int16_t> buffer(fileSize / 2);
    file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
    
    for (int s = 0; s < numScenes; ++s) {
        if (s != 0) continue; // ONLY ANALYZE SCENE 0

        int activeEvents = 0;
        const int16_t* scenePtr = buffer.data() + (s * DDATA_EVENT_COUNT * DDATA_INTS_PER_EVENT);
        
        std::cout << "\n[Scene " << s << "]\n";
        std::cout << "  ID  | Pic  | Script | X, Y  | DefaultPic | Note\n";
        std::cout << "------+------+--------+-------+------------+------\n";
        
        for (int e = 0; e < DDATA_EVENT_COUNT; ++e) {
            const int16_t* evt = scenePtr + (e * DDATA_INTS_PER_EVENT);
            
            // Heuristic for "Active Event":
            // Usually has a Pic (Index 5) OR a Script (Index 4) OR is at a valid location (Index 9,10 != 0)
            // Event 0 is usually the protagonist/start point if scene is active.
            
            int16_t scriptId = evt[4];
            int16_t picId = evt[5];
            int16_t defPic = evt[7];
            int16_t y = evt[9];
            int16_t x = evt[10];
            
            if (picId != 0 || scriptId != 0 || (x != 0 && y != 0)) {
                activeEvents++;
                std::cout << "  " << std::setw(3) << e << " | "
                          << std::setw(4) << picId << " | "
                          << std::setw(6) << scriptId << " | "
                          << std::setw(2) << x << "," << std::setw(2) << y << " | "
                          << std::setw(10) << defPic << " | ";
                          
                if (e == 0) std::cout << "(Main/Hero?)";
                std::cout << "\n";
            }
        }
        
        if (activeEvents == 0) {
            std::cout << "  (No active events)\n";
        }
    }
}

void analyze_allsin(const std::string& path) {
    std::cout << "\n=== Analyzing Map Data (allsin.grp) ===\n";
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "Error: Could not open " << path << "\n";
        return;
    }
    
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    
    int numScenes = fileSize / SDATA_SCENE_SIZE;
    std::cout << "File Size: " << fileSize << " bytes\n";
    std::cout << "Detected Scenes: " << numScenes << "\n";
    
    // We process scene by scene to save memory if file is huge (though 50MB is fine)
    std::vector<int16_t> sceneBuffer(SDATA_SCENE_SIZE / 2);
    
    for (int s = 0; s < numScenes; ++s) {
        file.read(reinterpret_cast<char*>(sceneBuffer.data()), SDATA_SCENE_SIZE);
        
        // Analyze layers
        int tilesL0 = 0;
        int tilesL1 = 0;
        int tilesL2 = 0;
        int tilesL3 = 0; // Events
        
        const int16_t* l0 = sceneBuffer.data() + 0 * (SDATA_MAP_SIZE * SDATA_MAP_SIZE);
        const int16_t* l1 = sceneBuffer.data() + 1 * (SDATA_MAP_SIZE * SDATA_MAP_SIZE);
        const int16_t* l2 = sceneBuffer.data() + 2 * (SDATA_MAP_SIZE * SDATA_MAP_SIZE);
        const int16_t* l3 = sceneBuffer.data() + 3 * (SDATA_MAP_SIZE * SDATA_MAP_SIZE);
        
        for (int i = 0; i < SDATA_MAP_SIZE * SDATA_MAP_SIZE; ++i) {
            if (l0[i] != 0) tilesL0++;
            if (l1[i] != 0) tilesL1++;
            if (l2[i] != 0) tilesL2++;
            if (l3[i] != 0) tilesL3++;
        }
        
        // Only print interesting scenes (not empty ones)
        if (tilesL0 > 0 || tilesL1 > 0 || tilesL3 > 0) {
            std::cout << "Scene " << std::setw(3) << s << ": "
                      << "L0(Gnd):" << tilesL0 << " "
                      << "L1(Bld):" << tilesL1 << " "
                      << "L2(Dec):" << tilesL2 << " "
                      << "L3(Evt):" << tilesL3 << "\n";
                      
            // If there are events, maybe list the first few unique event IDs found in map?
            if (tilesL3 > 0) {
                std::cout << "  Event IDs on Map: ";
                int printed = 0;
                for (int i = 0; i < SDATA_MAP_SIZE * SDATA_MAP_SIZE; ++i) {
                     if (l3[i] != 0) {
                         // Simple way to show some IDs without duplicates would be nice, 
                         // but for now just show first few
                         // Actually, let's just not clutter output too much.
                     }
                }
                std::cout << "(Use dump_ddata for details)\n";
            }
        }
    }
}

int main(int argc, char* argv[]) {
    SetConsoleOutputCP(CP_UTF8);

    std::string alldefPath = "save/alldef.grp";
    std::string allsinPath = "save/allsin.grp";
    
    if (argc > 1) alldefPath = argv[1];
    if (argc > 2) allsinPath = argv[2];
    
    // Check if paths exist, try ../save fallback
    if (!fs::exists(alldefPath)) {
        if (fs::exists("../save/alldef.grp")) alldefPath = "../save/alldef.grp";
    }
    if (!fs::exists(allsinPath)) {
        if (fs::exists("../save/allsin.grp")) allsinPath = "../save/allsin.grp";
    }

    std::cout << "Select Action:\n";
    std::cout << "1. Analyze alldef.grp (Event Definitions)\n";
    std::cout << "2. Analyze allsin.grp (Map Definitions)\n";
    std::cout << "0. Exit\n";
    std::cout << "Choice: ";
    
    int choice;
    std::cin >> choice;
    
    if (choice == 1) {
        if (fs::exists(alldefPath)) analyze_alldef(alldefPath);
        else std::cerr << "File not found: " << alldefPath << "\n";
    } else if (choice == 2) {
        if (fs::exists(allsinPath)) analyze_allsin(allsinPath);
        else std::cerr << "File not found: " << allsinPath << "\n";
    }
    
    std::cout << "\nPress Enter to exit...";
    std::cin.ignore();
    std::cin.get();

    return 0;
}
