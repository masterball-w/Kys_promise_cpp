#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <fstream>
#include <iostream>

class FileLoader {
public:
    // Helper to read entire file
    static std::vector<uint8_t> loadFile(const std::string& filename);

    // Read a specific record from a Group file (.grp) using an Index file (.idx)
    // The .idx file is assumed to be a list of 4-byte offsets.
    // The size of record 'i' is calculated as offset[i+1] - offset[i].
    static std::vector<uint8_t> loadGroupRecord(const std::string& grpPath, const std::string& idxPath, int index);

    // Helper to get the resource path prefix (e.g., "resource/")
    static std::string getResourcePath(const std::string& filename);

    // Helper to write entire file
    static bool saveFile(const std::string& filename, const void* data, size_t size);
};
