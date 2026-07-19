#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Basic Type Definitions matching Pascal sizes
using int16 = int16_t;
using uint16 = uint16_t;
using int32 = int32_t;
using uint32 = uint32_t;

// Constants
constexpr int MAX_NAME_LEN = 10; // Pascal: array[0..9]
constexpr int MAX_INTRO_LEN = 30; // Pascal: array[0..29]

constexpr int MAX_PHYSICAL_POWER = 100;
constexpr int MAX_HP = 999;
constexpr int MAX_MP = 999;
constexpr int LIFE_HURT = 10;

constexpr int MAX_ITEM_AMOUNT = 300;
constexpr int MAX_TEAM_SIZE = 6;
constexpr int BEGIN_BATTLE_ROLE_PIC = 1;

struct Position {
    int x, y;
};

struct Rect {
    int x, y, w, h;
};

struct InventoryItem {
    int16 id = -1;
    int16 amount = 0;
};
