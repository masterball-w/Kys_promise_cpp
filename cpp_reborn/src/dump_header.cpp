#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <string>

int main() {
    std::string path = "save/Ranger.grp";
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        path = "../save/Ranger.grp";
        file.open(path, std::ios::binary);
    }
    
    if (!file) {
        std::cerr << "Could not open Ranger.grp" << std::endl;
        return 1;
    }

    std::vector<int16_t> buffer(100);
    file.read(reinterpret_cast<char*>(buffer.data()), buffer.size() * 2);
    
    std::cout << "Ranger.grp Header Dump (int16):" << std::endl;
    for (int i = 0; i < 20; ++i) {
        std::cout << "Offset " << (i*2) << ": " << buffer[i] << std::endl;
    }

    // Header Structure Guess:
    // 0: InShip?
    // 1: Scene ID (InSubMap)
    // 2: MainX
    // 3: MainY
    // 4: Face
    
    std::cout << "\nInterpreted:" << std::endl;
    std::cout << "Scene ID (Offset 2?): " << buffer[1] << std::endl;
    std::cout << "MainX (Offset 4?): " << buffer[2] << std::endl;
    std::cout << "MainY (Offset 6?): " << buffer[3] << std::endl;
    std::cout << "Face (Offset 8?): " << buffer[4] << std::endl;

    return 0;
}
