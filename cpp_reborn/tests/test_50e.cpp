#include <iostream>
#include <string>
#include <vector>
#include "EventManager.h"
#include "GameManager.h"

static int g_fail = 0;

static void Expect(bool ok, const char* msg) {
    if (ok) {
        std::cout << "[PASS] " << msg << std::endl;
    } else {
        std::cout << "[FAIL] " << msg << std::endl;
        ++g_fail;
    }
}

int main() {
    auto& em = EventManager::getInstance();
    auto& gm = GameManager::getInstance();

    // case 0 assign
    em.Instruct_50e(0, 100, 42, 0, 0, 0, 0);
    Expect(gm.getX50(100) == 42, "case 0 set x50");

    // case 9-12 strings (pansichar packing)
    gm.setX50String(200, "ATK=%d");
    em.Instruct_50e(9, 0, 300, 200, 7, 0, 0);
    Expect(gm.getX50String(300) == "ATK=7", "case 9 format %d");

    gm.setX50String(400, "hello");
    em.Instruct_50e(10, 400, 401, 0, 0, 0, 0);
    Expect(gm.getX50(401) == 5, "case 10 string length");

    gm.setX50String(500, "ab");
    gm.setX50String(510, "cd");
    em.Instruct_50e(11, 520, 500, 510, 0, 0, 0);
    Expect(gm.getX50String(520) == "abcd", "case 11 concat");

    em.Instruct_50e(12, 0, 530, 3, 0, 0, 0); // 0..3 => 4 spaces
    Expect(gm.getX50String(530) == "    ", "case 12 spaces");

    // case 3 arithmetic
    gm.setX50(10, 20);
    em.Instruct_50e(3, 0, 0, 11, 10, 5, 0); // e2=0 add: x50[11]=x50[10]+5
    Expect(gm.getX50(11) == 25, "case 3 add");

    // case 38 random
    em.Instruct_50e(38, 0, 1, 12, 0, 0, 0); // random(1) => 0
    Expect(gm.getX50(12) == 0, "case 38 random(1)");

    // case 25/26 scene / auto-refresh
    gm.enterScene(3);
    em.Instruct_50e(25, 0, 0, 0x295E, 0x001D, 9, 0); // CurScene poke via $1D295E
    Expect(gm.getCurrentSceneId() == 9, "case 25 CurScene poke");
    em.Instruct_50e(26, 0, 0, 0x295E, 0x001D, 13, 0);
    Expect(gm.getX50(13) == 9, "case 26 CurScene peek");

    em.Instruct_50e(25, 0, 0, 0x0006, 0x0000, 1, 0); // AutoRefresh
    Expect(gm.getAutoRefresh() == 1, "case 25 AutoRefresh");

    // inventory poke via range
    gm.setInventorySlot(2, -1, 0);
    // t1 = 0x18FE2C + 8 → slot 2 number
    uint16_t e3 = static_cast<uint16_t>((0x18FE2C + 8) & 0xFFFF);
    uint16_t e4 = static_cast<uint16_t>(((0x18FE2C + 8) >> 16) & 0xFFFF);
    em.Instruct_50e(25, 0, 0, e3, e4, 77, 0);
    Expect(gm.getInventorySlot(2).id == 77, "case 25 item number poke");
    em.Instruct_50e(26, 0, 0, e3, e4, 14, 0);
    Expect(gm.getX50(14) == 77, "case 26 item number peek");

    if (g_fail == 0) {
        std::cout << "\n=== test_50e ALL PASS ===" << std::endl;
        return 0;
    }
    std::cout << "\n=== test_50e FAILURES: " << g_fail << " ===" << std::endl;
    return 1;
}
