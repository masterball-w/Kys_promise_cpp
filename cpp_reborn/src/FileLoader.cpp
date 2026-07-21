#include "FileLoader.h"
#include "PlatformCompat.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

const std::string RESOURCE_DIR = "resource/";

namespace {
std::string g_dataRoot;
std::string g_resourcePrefix;
bool g_pathInit = false;

void ensurePathInit() {
    if (g_pathInit) return;
    g_dataRoot = PlatformCompat::discoverDataRoot();
    std::filesystem::path root(g_dataRoot);
    std::error_code ec;
    std::filesystem::path res = root / "resource";
    if (std::filesystem::is_directory(res, ec)) {
        g_resourcePrefix = res.string();
        if (!g_resourcePrefix.empty() && g_resourcePrefix.back() != '/' && g_resourcePrefix.back() != '\\') {
            g_resourcePrefix.push_back('/');
        }
    } else {
        // Legacy: probe relative resource/ like before
        std::vector<std::string> candidates = {
            "resource",
            "../resource",
            "../../resource",
            "../../../resource",
            "game_data/resource",
            "../game_data/resource",
            "../../game_data/resource",
        };
        for (const auto& dir : candidates) {
            std::string testPath = dir + "/smp";
            std::ifstream f(testPath.c_str());
            if (f.good()) {
                g_resourcePrefix = dir + "/";
                break;
            }
        }
        if (g_resourcePrefix.empty()) {
            for (const auto& dir : candidates) {
                if (std::filesystem::exists(dir)) {
                    g_resourcePrefix = dir + "/";
                    break;
                }
            }
        }
        if (g_resourcePrefix.empty()) {
            g_resourcePrefix = RESOURCE_DIR;
        }
    }
    g_pathInit = true;
    std::cout << "[FileLoader] Data root: " << g_dataRoot << std::endl;
    std::cout << "[FileLoader] Resource prefix: " << g_resourcePrefix << std::endl;
}
}  // namespace

std::string FileLoader::getDataRoot() {
    ensurePathInit();
    return g_dataRoot;
}

std::string FileLoader::getSaveDir() {
    ensurePathInit();
    return PlatformCompat::discoverSaveDir(g_dataRoot);
}

std::string FileLoader::getResourcePath(const std::string& filename) {
    ensurePathInit();

    // Absolute path
    if (filename.find(":") != std::string::npos ||
        (!filename.empty() && (filename[0] == '/' || filename[0] == '\\'))) {
        return filename;
    }

    std::string cleanName = filename;
    if (cleanName.find("resource/") == 0) {
        cleanName = cleanName.substr(9);
    } else if (cleanName.find("resource\\") == 0) {
        cleanName = cleanName.substr(9);
    }
    if (cleanName.find("../resource/") == 0) {
        cleanName = cleanName.substr(12);
    }

    // Paths that live next to resource/ (save/, music/, sound/, fight/, eft/, list/)
    const char* siblingRoots[] = {"save/", "music/", "sound/", "fight/", "eft/", "list/", "mmap/"};
    for (const char* sib : siblingRoots) {
        if (cleanName.find(sib) == 0 || filename.find(sib) == 0) {
            std::string rel = (cleanName.find(sib) == 0) ? cleanName : filename;
            return g_dataRoot + rel;
        }
    }
    // Also accept bare "music/0.wav" style already covered; if caller passed music without prefix via getResourcePath("music/x")
    if (cleanName.rfind("music/", 0) == 0 || cleanName.rfind("sound/", 0) == 0 ||
        cleanName.rfind("fight/", 0) == 0 || cleanName.rfind("eft/", 0) == 0 ||
        cleanName.rfind("list/", 0) == 0 || cleanName.rfind("save/", 0) == 0) {
        return g_dataRoot + cleanName;
    }

    return g_resourcePrefix + cleanName;
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

    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    if (size > 0 && file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        return buffer;
    }
    if (size == 0) return buffer;
    return {};
}

bool FileLoader::saveFile(const std::string& filename, const void* data, size_t size) {
    std::string path = filename;
    if (filename.find("save/") == 0 || filename.find("save\\") == 0) {
        path = getSaveDir() + filename.substr(5);
    } else if (filename.find('/') == std::string::npos && filename.find('\\') == std::string::npos) {
        path = getSaveDir() + filename;
    }

    std::filesystem::path parent = std::filesystem::path(path).parent_path();
    if (!parent.empty() && !std::filesystem::exists(parent)) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
    }

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        std::cerr << "Failed to open file for writing: " << path << std::endl;
        return false;
    }
    file.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
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
    std::vector<uint8_t> buffer(static_cast<size_t>(length));
    if (!grpFile.read(reinterpret_cast<char*>(buffer.data()), length)) {
        return {};
    }

    return buffer;
}
