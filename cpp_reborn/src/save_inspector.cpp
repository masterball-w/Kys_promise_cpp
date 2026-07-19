#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <iomanip>
#include <cstdint>
#include <windows.h>
#include <filesystem>

namespace fs = std::filesystem;

// Constants from GameTypes.h / GameManager.cpp
constexpr int ROLE_DATA_INTS = 91;
constexpr int ROLE_DATA_SIZE = ROLE_DATA_INTS * 2;
constexpr int ITEM_DATA_INTS = 95;
constexpr int ITEM_DATA_SIZE = ITEM_DATA_INTS * 2;
constexpr int SCENE_DATA_SIZE = 52;
constexpr int MAGIC_DATA_SIZE = 58; // From Magic.h (likely 29 ints?) - checking needed
// Actually let's assume offsets tell us the count.

// Helper for GBK to UTF8
std::string gbkToUtf8(const std::string& gbkStr) {
    if (gbkStr.empty()) return "";
    int len = MultiByteToWideChar(CP_ACP, 0, gbkStr.c_str(), -1, NULL, 0);
    if (len == 0) return gbkStr;
    std::vector<wchar_t> wstr(len);
    MultiByteToWideChar(CP_ACP, 0, gbkStr.c_str(), -1, wstr.data(), len);
    
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), -1, NULL, 0, NULL, NULL);
    if (utf8Len == 0) return gbkStr;
    std::vector<char> utf8Str(utf8Len);
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), -1, utf8Str.data(), utf8Len, NULL, NULL);
    return std::string(utf8Str.data());
}

// Helper to read string from int16 array (Pascal style fixed length char arrays packed in int16)
std::string readString(const int16_t* data, int startIdx, int lenBytes) {
    // startIdx is index in int16 array
    // lenBytes is length in bytes
    std::vector<char> buffer(lenBytes + 1, 0);
    memcpy(buffer.data(), data + startIdx, lenBytes);
    return gbkToUtf8(std::string(buffer.data()));
}

struct SaveOffsets {
    int RoleOffset;
    int ItemOffset;
    int SceneOffset;
    int MagicOffset;
    int WeiShopOffset;
    int TotalLen;
};

SaveOffsets loadOffsets(const std::string& idxPath) {
    SaveOffsets offsets = {0};
    std::ifstream idxFile(idxPath, std::ios::binary);
    if (!idxFile) {
        std::cerr << "Error: Could not open " << idxPath << std::endl;
        return offsets;
    }
    
    int32_t buffer[6];
    idxFile.read(reinterpret_cast<char*>(buffer), sizeof(buffer));
    
    offsets.RoleOffset = buffer[0];
    offsets.ItemOffset = buffer[1];
    offsets.SceneOffset = buffer[2];
    offsets.MagicOffset = buffer[3];
    offsets.WeiShopOffset = buffer[4];
    offsets.TotalLen = buffer[5];
    
    return offsets;
}

void inspectSave(const std::string& savePath, const SaveOffsets& offsets) {
    std::ifstream grpFile(savePath, std::ios::binary);
    if (!grpFile) {
        std::cerr << "Error: Could not open " << savePath << std::endl;
        return;
    }
    
    grpFile.seekg(0, std::ios::end);
    size_t fileSize = grpFile.tellg();
    grpFile.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> data(fileSize);
    grpFile.read(reinterpret_cast<char*>(data.data()), fileSize);
    const int16_t* i16Data = reinterpret_cast<const int16_t*>(data.data());

    // 1. Header Info
    std::cout << "\n=== Save Header Info ===\n";
    if (offsets.RoleOffset >= 22) {
        int16_t inShip = i16Data[0];
        int16_t sceneId = i16Data[1];
        int16_t mapX = i16Data[2];
        int16_t mapY = i16Data[3];
        int16_t face = i16Data[4];
        
        std::cout << "Current Scene: " << sceneId << "\n";
        std::cout << "Coordinates: (" << mapX << ", " << mapY << ")\n";
        std::cout << "Facing: " << face << "\n";
        std::cout << "InShip: " << inShip << "\n";
        
        std::cout << "Team: ";
        for (int i = 0; i < 6; ++i) {
             std::cout << i16Data[11 + i] << " ";
        }
        std::cout << "\n";
    }

    // 2. Roles
    std::cout << "\n=== Roles ===\n";
    if (offsets.RoleOffset > 0 && offsets.ItemOffset > offsets.RoleOffset) {
        if (offsets.ItemOffset <= fileSize) {
            int roleCount = (offsets.ItemOffset - offsets.RoleOffset) / ROLE_DATA_SIZE;
            const int16_t* rolePtr = reinterpret_cast<const int16_t*>(data.data() + offsets.RoleOffset);
            
            for (int i = 0; i < roleCount; ++i) {
                const int16_t* r = rolePtr + i * ROLE_DATA_INTS;
                std::string name = readString(r, 4, 10); // Offset 4, 10 bytes
                int16_t hp = r[17];
                int16_t maxHp = r[18];
                int16_t level = r[15];
                
                // Only print valid roles (skip empty names or specific checks if needed)
                // if (!name.empty()) 
                {
                    std::cout << "[" << std::setw(3) << i << "] " 
                              << std::left << std::setw(15) << name 
                              << " LV:" << std::setw(3) << level 
                              << " HP:" << hp << "/" << maxHp << "\n";
                }
            }
        } else {
            std::cout << "(Skipping: File too small for Role data defined in ranger.idx)\n";
        }
    }

    // 3. Items
    std::cout << "\n=== Items (Inventory > 0) ===\n";
    if (offsets.ItemOffset > 0 && offsets.SceneOffset > offsets.ItemOffset) {
        if (offsets.SceneOffset <= fileSize) {
            int itemCount = (offsets.SceneOffset - offsets.ItemOffset) / ITEM_DATA_SIZE;
            const int16_t* itemPtr = reinterpret_cast<const int16_t*>(data.data() + offsets.ItemOffset);
            
            int count = 0;
            for (int i = 0; i < itemCount; ++i) {
                const int16_t* it = itemPtr + i * ITEM_DATA_INTS;
                int16_t amount = it[42]; // Inventory count
                
                if (amount > 0) {
                    std::string name = readString(it, 1, 20); // Offset 1, 20 bytes (10 ints)
                    std::cout << "[" << std::setw(3) << i << "] " 
                              << std::left << std::setw(20) << name 
                              << " Amt: " << amount << "\n";
                    count++;
                }
            }
            if (count == 0) std::cout << "(No items in inventory)\n";
        } else {
            std::cout << "(Skipping: File too small for Item data defined in ranger.idx)\n";
        }
    }
}

int main(int argc, char* argv[]) {
    // Set console output to UTF8
    SetConsoleOutputCP(CP_UTF8);

    std::string saveDir = "save";
    if (argc > 1) saveDir = argv[1];

    if (!fs::exists(saveDir)) {
        std::cout << "Directory '" << saveDir << "' does not exist. Trying '../save'...\n";
        saveDir = "../save";
        if (!fs::exists(saveDir)) {
            std::cerr << "Save directory not found.\n";
            return 1;
        }
    }

    std::string idxPath = saveDir + "/ranger.idx";
    if (!fs::exists(idxPath)) {
        // Try Ranger.idx
        idxPath = saveDir + "/Ranger.idx";
        if (!fs::exists(idxPath)) {
             std::cerr << "ranger.idx not found in " << saveDir << "\n";
             return 1;
        }
    }
    
    SaveOffsets offsets = loadOffsets(idxPath);
    std::cout << "Loaded Offsets from " << idxPath << "\n";
    std::cout << "RoleOffset: " << offsets.RoleOffset << "\n";
    std::cout << "ItemOffset: " << offsets.ItemOffset << "\n";
    
    while (true) {
        std::cout << "\nAvailable Save Files:\n";
        std::vector<std::string> files;
        int idx = 0;
        
        // Always include ranger.grp
        if (fs::exists(saveDir + "/ranger.grp")) files.push_back("ranger.grp");
        else if (fs::exists(saveDir + "/Ranger.grp")) files.push_back("Ranger.grp");

        for (const auto& entry : fs::directory_iterator(saveDir)) {
            std::string filename = entry.path().filename().string();
            // Filter .grp files, excluding ranger.grp (already added)
            if (filename.length() > 4 && filename.substr(filename.length()-4) == ".grp") {
                if (filename != "ranger.grp" && filename != "Ranger.grp" && 
                    filename != "warfld.grp") {
                    files.push_back(filename);
                }
            }
        }
        
        for (int i = 0; i < files.size(); ++i) {
            std::cout << i + 1 << ". " << files[i] << "\n";
        }
        std::cout << "0. Exit\n";
        std::cout << "Select file: ";
        
        int choice;
        std::cin >> choice;
        
        if (choice == 0) break;
        if (choice > 0 && choice <= files.size()) {
            std::string selectedFile = saveDir + "/" + files[choice - 1];
            std::cout << "Inspecting " << selectedFile << "...\n";
            inspectSave(selectedFile, offsets);
            
            std::cout << "\nPress Enter to continue...";
            std::cin.ignore();
            std::cin.get();
        }
    }

    return 0;
}
