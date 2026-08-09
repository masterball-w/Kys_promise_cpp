#include <iostream>
#include <vector>
#include <cassert>
#include "EventManager.h"
#include "GameManager.h"

static int g_mockEventId = 90000;

static int RunMockScript(const std::vector<int16_t>& script) {
    EventManager& em = EventManager::getInstance();
    em.AddMockScript(g_mockEventId, script);
    int startPc = static_cast<int>(em.GetScriptWord(0)); // placeholder; track via size before
    (void)startPc;
    em.ExecuteEvent(g_mockEventId);
    ++g_mockEventId;
    return g_mockEventId;
}

static void TestOpcode0NegativeEnd() {
    std::cout << "--- Test opcode 0 + negative terminator ---" << std::endl;
    std::vector<int16_t> script = {0, -1};
    EventManager::getInstance().ExecuteScriptBuffer(script);
    std::cout << "[PASS] opcode 0 then -1 terminates without crash" << std::endl;
}

static void TestOpcode7Break() {
    std::cout << "--- Test opcode 7 break ---" << std::endl;
    EventManager& em = EventManager::getInstance();
    int base = 0;
    // Use a dedicated buffer: opcode 7 should prevent opcode 24 (unused) from running if we had side effects.
    // Instead verify script stops early: place marker 50 opcode after 7 with code 0 (x50 poke).
  GameManager::getInstance().setX50(100, 999);
    std::vector<int16_t> script = {
        50, 0, 100, 42, 0, 0, 0, 0,  // would set x50[100]=42
        7, 0,                          // break (padding word like Pascal layout)
        50, 0, 100, 77, 0, 0, 0, 0,  // must NOT run
        -1
    };
    em.ExecuteScriptBuffer(script);
    int val = GameManager::getInstance().getX50(100);
    assert(val == 42 && "opcode after 7 must not execute");
    assert(val != 77);
    GameManager::getInstance().setX50(100, 0);
    std::cout << "[PASS] opcode 7 breaks before subsequent instructions" << std::endl;
}

static void TestOpcode61ForwardJump() {
    std::cout << "--- Test opcode 61 forward jump ---" << std::endl;
    EventManager& em = EventManager::getInstance();
    GameManager::getInstance().setX50(101, 0);
    // [50 x8] [61 off=8] [50 x8 skipped] [-1]
    std::vector<int16_t> script = {
        50, 0, 101, 1, 0, 0, 0, 0,
        61, 8, 0,
        50, 0, 101, 2, 0, 0, 0, 0,
        -1
    };
    int base = static_cast<int>(em.GetScriptWord(0)); // not used
    (void)base;
    em.ExecuteScriptBuffer(script);
    assert(GameManager::getInstance().getX50(101) == 1);
    GameManager::getInstance().setX50(101, 0);
    std::cout << "[PASS] opcode 61 forward jump skips intermediate 50" << std::endl;
}

static void TestOpcode61NegativeOffset() {
    std::cout << "--- Test opcode 61 lands on terminator ---" << std::endl;
    EventManager& em = EventManager::getInstance();
    GameManager::getInstance().setX50(102, 0);
    std::vector<int16_t> script = {
        50, 0, 102, 5, 0, 0, 0, 0,
        61, 1, 0,
        -1
    };
    em.ExecuteScriptBuffer(script);
    assert(GameManager::getInstance().getX50(102) == 5);
    GameManager::getInstance().setX50(102, 0);
    std::cout << "[PASS] opcode 61 jump to terminator" << std::endl;
}

static void TestApplyInstruct50MemoryWrite() {
    std::cout << "--- Test instruct_50 memory write (result >= 622592) ---" << std::endl;
    EventManager& em = EventManager::getInstance();
    int pc = 5;
    int result = 655360 * 1 + 12345; // idx=1 -> write at pc+0
    em.ApplyInstruct50Result(pc, result);
    // Write goes to m_eventScripts[pc + idx - 1] = m_eventScripts[5]
    int16_t word = em.GetScriptWord(5);
    assert(word == 12345);
    std::cout << "[PASS] instruct_50 script memory write" << std::endl;
}

static void TestApplyInstruct50Jump() {
    std::cout << "--- Test instruct_50 relative jump ---" << std::endl;
    EventManager& em = EventManager::getInstance();
    int pc = 10;
    em.ApplyInstruct50Result(pc, 5);
    assert(pc == 15);
    std::cout << "[PASS] instruct_50 relative jump" << std::endl;
}

static void TestInstruct50eCase32() {
    std::cout << "--- Test instruct_50e case 32 encoding ---" << std::endl;
    EventManager& em = EventManager::getInstance();
    GameManager::getInstance().setX50(200, 999);
    int result = em.Instruct_50e(32, 0, 200, 0, 0, 0, 0);
    assert(result >= 622592);
    int pc = 10;
    em.ApplyInstruct50Result(pc, result);
    assert(em.GetScriptWord(10) == 999);
    std::cout << "[PASS] instruct_50e case 32 round-trip" << std::endl;
}

int main() {
    std::cout << "=== Event Interpreter Unit Tests ===" << std::endl;

    if (!EventManager::getInstance().LoadScripts()) {
        std::cerr << "[WARN] kdef not loaded; buffer-only tests will run" << std::endl;
    }

    TestApplyInstruct50Jump();
    TestApplyInstruct50MemoryWrite();
    TestInstruct50eCase32();
    TestOpcode0NegativeEnd();
    TestOpcode7Break();
    TestOpcode61ForwardJump();
    TestOpcode61NegativeOffset();

    std::cout << "\n=== All event interpreter tests passed ===" << std::endl;
    return 0;
}
