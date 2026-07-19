#pragma once
#include "GameTypes.h"
#include <vector>
#include <cstring>
#include <algorithm>

// Base class for game entities that need to maintain binary compatibility
class GameObject {
public:
    GameObject(size_t dataSize) : m_data(dataSize, 0) {}
    virtual ~GameObject() = default;

    // Direct access for serialization
    int16* getRawData() { return m_data.data(); }
    const int16* getRawData() const { return m_data.data(); }
    size_t getDataSize() const { return m_data.size(); }

    void setDataVector(const std::vector<int16>& data) {
        if (data.size() == m_data.size()) {
            m_data = data;
        } else if (data.size() < m_data.size()) {
             std::copy(data.begin(), data.end(), m_data.begin());
        } else {
             m_data = data; // Or resize? Usually we want fixed size.
        }
    }

    void loadFromBuffer(const int16* buffer, size_t size) {
        if (size <= m_data.size()) {
            std::memcpy(m_data.data(), buffer, size * sizeof(int16));
        }
    }

protected:
    std::vector<int16> m_data;

    int16 getData(size_t index) const {
        return (index < m_data.size()) ? m_data[index] : 0;
    }

    void setData(size_t index, int16 value) {
        if (index < m_data.size()) {
            m_data[index] = value;
        }
    }

    // Helper to get string from int16 array
    std::string getString(size_t offset, size_t length) const {
        std::string s;
        const char* ptr = reinterpret_cast<const char*>(&m_data[offset]);
        // Length in bytes = length * 2 (since int16)
        // But Pascal definition says array[0..9] of ansichar. 
        // In the union, this occupies 5 int16s.
        // We read until null terminator or max length
        for (size_t i = 0; i < length * 2; ++i) {
            if (ptr[i] == 0) break;
            s += ptr[i];
        }
        return s;
    }

    // Helper to set string to int16 array
    void setString(size_t offset, size_t length, const std::string& val) {
        char* ptr = reinterpret_cast<char*>(&m_data[offset]);
        std::memset(ptr, 0, length * 2);
        std::strncpy(ptr, val.c_str(), length * 2 - 1);
    }
};
