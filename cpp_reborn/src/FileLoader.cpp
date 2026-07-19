#include "FileLoader.h"
#include <filesystem>
#include <iostream>
#include <vector>

const std::string RESOURCE_DIR = "resource/";

std::string FileLoader::getResourcePath(const std::string& filename) {
    static std::string resourcePrefix = "";
    static bool initialized = false;
    
    // If input is absolute path, use it directly
    if (filename.find(":") != std::string::npos || filename.find("/") == 0 || filename.find("\\") == 0) {
        return filename;
    }

    if (!initialized) {
        // Candidates for resource directory
        std::vector<std::string> candidates = {
            "resource",
            "../resource",
            "../../resource",
            "../../../resource",
            "Debug/resource",
            "build/Debug/resource",
            "build/Release/resource",
            "cpp_reborn/resource",
            "cpp_reborn/build/Debug/resource",
            "cpp_reborn/build/Release/resource",
            "../cpp_reborn/resource",
            "../cpp_reborn/build/Debug/resource",
            "../cpp_reborn/build/Release/resource"
        };
 
         // 1. Try to find a resource dir that contains key file 'smp'
         for (const auto& dir : candidates) {
             std::string testPath = dir + "/smp";
             std::ifstream f(testPath.c_str());
             if (f.good()) {
                 resourcePrefix = dir + "/";
                 break;
             }
         }

        // 2. If not found, fall back to any existing resource dir
        if (resourcePrefix.empty()) {
            for (const auto& dir : candidates) {
                if (std::filesystem::exists(dir)) {
                    resourcePrefix = dir + "/";
                    break;
                }
            }
        }
        
        // 3. Final fallback
        if (resourcePrefix.empty()) {
             resourcePrefix = RESOURCE_DIR;
        }

        initialized = true;
        std::cout << "[FileLoader] Discovered resource path: " << resourcePrefix << std::endl;
    }

    std::string cleanName = filename;
    
    // Debug: Print input
    // std::cout << "[FileLoader] Input: " << filename << std::endl;

    // Remove "resource/" or "resource\" from start of filename if present to avoid duplication
    if (cleanName.find("resource/") == 0) {
        cleanName = cleanName.substr(9);
    } else if (cleanName.find("resource\\") == 0) {
        cleanName = cleanName.substr(9);
    }
    
    // Handle "../resource/" case if present
    if (cleanName.find("../resource/") == 0) {
        cleanName = cleanName.substr(12);
    }

    // std::cout << "[FileLoader] Final Path: " << resourcePrefix + cleanName << std::endl;
    return resourcePrefix + cleanName;
}

std::vector<uint8_t> FileLoader::loadFile(const std::string& filename) {
    std::string path = getResourcePath(filename);
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "Failed to open file: " << path << std::endl;
        return {};
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(size);
    if (file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        return buffer;
    }
    return {};
}

bool FileLoader::saveFile(const std::string& filename, const void* data, size_t size) {
    // We assume saving to the same folder structure or a dedicated save folder
    // But getResourcePath finds the *read* path.
    // For saving, we usually want to save to a specific writable directory.
    // For now, let's assume saving to "save/" directory relative to CWD.
    
    // Ensure "save" directory exists
    if (!std::filesystem::exists("save")) {
        std::filesystem::create_directory("save");
    }
    
    // Filename usually comes in as "save/r1.grp" or just "r1.grp"
    // If it has "save/" prefix, we use it. If not, we prepend it?
    // Let's rely on caller providing correct relative path (e.g., "save/r1.grp")
    
    std::ofstream file(filename, std::ios::binary | std::ios::trunc);
    if (!file) {
        std::cerr << "Failed to open file for writing: " << filename << std::endl;
        return false;
    }
    
    file.write(reinterpret_cast<const char*>(data), size);
    return file.good();
}

std::vector<uint8_t> FileLoader::loadGroupRecord(const std::string& grpName, const std::string& idxName, int index) {
    std::string idxPath = getResourcePath(idxName);
    std::string grpPath = getResourcePath(grpName);

    std::ifstream idxFile(idxPath, std::ios::binary);
    if (!idxFile) {
        std::cerr << "Failed to open index file: " << idxPath << std::endl;
        return {};
    }

    int32_t offsetCurrent = 0;
    int32_t offsetNext = 0;

    idxFile.seekg(index * 4);
    if (!idxFile.read(reinterpret_cast<char*>(&offsetCurrent), 4)) {
        return {};
    }

    if (!idxFile.read(reinterpret_cast<char*>(&offsetNext), 4)) {
        std::ifstream grpFileCheck(grpPath, std::ios::binary | std::ios::ate);
        if (grpFileCheck) {
            offsetNext = static_cast<int32_t>(grpFileCheck.tellg());
        } else {
            return {};
        }
    }

    int32_t length = offsetNext - offsetCurrent;
    if (length <= 0) {
        return {};
    }

    std::ifstream grpFile(grpPath, std::ios::binary);
    if (!grpFile) {
        return {};
    }

    grpFile.seekg(offsetCurrent);
    std::vector<uint8_t> buffer(length);
    if (!grpFile.read(reinterpret_cast<char*>(buffer.data()), length)) {
        return {};
    }

    return buffer;
}
