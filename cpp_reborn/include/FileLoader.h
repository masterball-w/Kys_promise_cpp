#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <fstream>
#include <iostream>

class FileLoader {
public:
    static std::vector<uint8_t> loadFile(const std::string& filename);

    static std::vector<uint8_t> loadGroupRecord(const std::string& grpPath, const std::string& idxPath, int index);

    static std::string getResourcePath(const std::string& filename);

    /** Game data root (contains resource/ and preferably save/). */
    static std::string getDataRoot();

    /** Writable save directory ending with separator. */
    static std::string getSaveDir();

    static bool saveFile(const std::string& filename, const void* data, size_t size);
};
