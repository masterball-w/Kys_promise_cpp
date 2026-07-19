#include "PicLoader.h"
#include <SDL3_image/SDL_image.h>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <map>
#include "FileLoader.h"

namespace {
    std::map<std::string, SDL_Surface*>& PicCache() {
        static std::map<std::string, SDL_Surface*> cache;
        return cache;
    }

    std::string MakeCacheKey(const std::string& filename, int num) {
        return filename + "#" + std::to_string(num);
    }
}

int PicLoader::getPicCount(const std::string& filename) {
    std::string path = std::filesystem::exists(filename) ? filename : FileLoader::getResourcePath(filename);
    std::ifstream file(path, std::ios::binary);
    
    if (!file.is_open()) {
        return 0;
    }

    int32_t count = 0;
    file.read(reinterpret_cast<char*>(&count), 4);
    return count;
}

PicImage PicLoader::loadPic(const std::string& filename, int num) {
    PicImage result;
    std::string path = std::filesystem::exists(filename) ? filename : FileLoader::getResourcePath(filename);
    std::ifstream file(path, std::ios::binary);
    
    if (!file.is_open()) {
        std::cerr << "PicLoader: Failed to open file: " << path << std::endl;
        return result;
    }

    int32_t count = 0;
    file.read(reinterpret_cast<char*>(&count), 4);
    if (count <= 0 || num < 0 || num >= count) {
        return result;
    }

    std::vector<int32_t> offsets(static_cast<size_t>(count));
    file.read(reinterpret_cast<char*>(offsets.data()), count * 4);
    if (file.fail()) {
        return result;
    }

    file.seekg(0, std::ios::end);
    int32_t fileSize = static_cast<int32_t>(file.tellg());
    if (fileSize <= 0) {
        return result;
    }

    int32_t headerSize = (count + 1) * 4;
    int32_t startOffset = (num == 0) ? headerSize : offsets[num - 1];
    if (startOffset < 0 || startOffset >= fileSize) {
        return result;
    }

    int32_t endOffset = offsets[num];
    if (endOffset < 0 || endOffset > fileSize) {
        return result;
    }

    int32_t dataLen = endOffset - startOffset - 12;
    if (dataLen <= 0) {
        return result;
    }
    
    file.seekg(startOffset, std::ios::beg);
    if (file.fail()) {
         std::cerr << "PicLoader: Invalid data offset for index " << num << std::endl;
         return result;
    }

    int32_t x = 0, y = 0, black = 0;
    file.read(reinterpret_cast<char*>(&x), 4);
    file.read(reinterpret_cast<char*>(&y), 4);
    file.read(reinterpret_cast<char*>(&black), 4);
    
    result.x = x;
    result.y = y;
    result.black = black;

    std::vector<uint8_t> buffer(dataLen);
    file.read(reinterpret_cast<char*>(buffer.data()), dataLen);
    if (file.gcount() != dataLen) {
        std::cerr << "PicLoader: Failed to read full image data" << std::endl;
        return result;
    }

    SDL_IOStream* io = SDL_IOFromMem(buffer.data(), dataLen);
    if (io) {
        result.surface = IMG_Load_IO(io, true); // true = closes IO automatically
        if (!result.surface) {
             std::cerr << "PicLoader: IMG_Load_IO failed: " << SDL_GetError() << std::endl;
        }
    } else {
        std::cerr << "PicLoader: SDL_IOFromMem failed" << std::endl;
    }

    return result;
}

void PicLoader::freePic(PicImage& pic) {
    if (pic.surface) {
        SDL_DestroySurface(pic.surface);
        pic.surface = nullptr;
    }
}

SDL_Surface* PicLoader::getCachedSurface(const std::string& filename, int num) {
    std::string key = MakeCacheKey(filename, num);
    auto& cache = PicCache();
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;

    PicImage pic = loadPic(filename, num);
    if (!pic.surface) return nullptr;
    cache[key] = pic.surface;
    return pic.surface;
}

void PicLoader::clearCache() {
    for (auto& pair : PicCache()) {
        if (pair.second) SDL_DestroySurface(pair.second);
    }
    PicCache().clear();
}
