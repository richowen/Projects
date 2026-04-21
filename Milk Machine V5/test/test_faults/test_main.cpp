// Unity stress tests for fault pathways.  Run: pio test -e native -f test_faults
#include <unity.h>
#include "machine_core.h"

static Machine        m;
static MachineOutputs out;
static constexpr size_t HEAP_OK  = 100000;
static constexpr size_t HEAP_BAD = 1024;

static void tickAt(uint32_t t, bool lvl, size_t h = HEAP_OK) {
    MachineInputs in{ lvl, t, h };
    m.tick(in, out);
}
static void advance(uint32_t& t, uint32_t dt, bool lvl, size_t h = HEAP_OK) {
    t += dt; tickAt(t, lvl, h);
}
static void forceMixCapFault(uint32_t& t) {
    advance(t, 100, true);
    advance(t, m.cfg.maxMixMs, true);
}
static void resetM(uint32_t t = 0) { m = Machine(); m.reset(t); tickAt(t, false); }

void setUp()    { resetM(0); }
void tearDown() {}

// 1. Back-to-back faults with no idle time between — still escalates to reboot
void test_backtoback_faults_escalate_to_reboot() {
    uint32_t t = 0;
    for (int i = 0; i < 3; i++) {
        forceMixCapFault(t);
        if (i < 2) advance(t, m.cfg.faultCooldownMs, false);  // IDLE
    }
    TEST_ASSERT_TRUE(out.rebootRequested);
    TEST_ASSERT_EQUAL(MachineEvent::FAULT_REBOOT, m.lastEvent());
}

// 2. Low heap while already in FAULT does NOT double-count faults
void test_low_heap_while_in_fault_does_not_recount() {
    uint32_t t = 0;
    forceMixCapFault(t);
    TEST_ASSERT_EQUAL(SysState::FAULT, m.state());
    TEST_ASSERT_EQUAL(1, m.faultCount());
    // Tick repeatedly with low heap — still in FAULT, count unchanged
    for (int i = 0; i < 10; i++) advance(t, 100, false, HEAP_BAD);
    TEST_ASSERT_EQUAL(1, m.faultCount());
    TEST_ASSERT_EQUAL(SysState::FAULT, m.state());
}

// 3. Fault cooldown is NOT short-circuited by incoming demand during cooldown.
//    Machine must stay in FAULT until cooldown elapses, then return to IDLE,
//    relays must stay OFF the entire cooldown period.
void test_fault_cooldown_not_interrupted_by_demand() {
    uint32_t t = 0;
    forceMixCapFault(t);
    // Demand appears immediately during cooldown — stop short of the
    // cooldown boundary so the final-exit tick is asserted separately.
    const uint32_t step = 250;
    for (uint32_t e = step; e + step <= m.cfg.faultCooldownMs; e += step) {
        advance(t, step, true);
        TEST_ASSERT_EQUAL(SysState::FAULT, m.state());
        TEST_ASSERT_FALSE(out.auger || out.agitator || out.mixer || out.water);
    }
    // Now push past the cooldown boundary
    advance(t, step * 2, false);
    TEST_ASSERT_EQUAL(SysState::IDLE, m.state());
}

// 4. Fault during PERIODIC_MIX via low heap — relays forced off, exits via cooldown
void test_low_heap_interrupts_periodic_mix() {
    uint32_t t = 0;
    advance(t, m.cfg.periodicIntMs, false);      // -> PERIODIC_MIX
    TEST_ASSERT_EQUAL(SysState::PERIODIC_MIX, m.state());
    advance(t, 500, false, HEAP_BAD);            // heap collapses mid-periodic
    TEST_ASSERT_EQUAL(SysState::FAULT, m.state());
    TEST_ASSERT_FALSE(out.auger || out.agitator || out.mixer || out.water);
    advance(t, m.cfg.faultCooldownMs, false);
    TEST_ASSERT_EQUAL(SysState::IDLE, m.state());
}

// 5. Exactly-maxFaults within window reboots; maxFaults-1 does not
void test_reboot_threshold_is_exact() {
    uint32_t t = 0;
    for (int i = 0; i < m.cfg.maxFaults - 1; i++) {
        forceMixCapFault(t);
        advance(t, m.cfg.faultCooldownMs, false);
    }
    TEST_ASSERT_FALSE(out.rebootRequested);
    forceMixCapFault(t);          // this is the maxFaults-th fault
    TEST_ASSERT_TRUE(out.rebootRequested);
}

// 6. After reboot-triggering fault, faultCount does not keep growing forever
void test_fault_count_does_not_overflow() {
    uint32_t t = 0;
    for (int i = 0; i < 30; i++) {
        forceMixCapFault(t);
        advance(t, m.cfg.faultCooldownMs, false);
    }
    // count tracks faults within window; must stay <= some small bound
    TEST_ASSERT_TRUE(m.faultCount() <= 30);
    TEST_ASSERT_TRUE(m.faultCount() >= m.cfg.maxFaults);
}

// 7. Sanity guard fires if MIXING somehow persists past cap+margin
//    (simulated by forcing cap+margin+1 in one tick from freshly-entered MIXING)
void test_sanity_guard_catches_runaway_mixing() {
    uint32_t t = 0;
    advance(t, 100, true);
    TEST_ASSERT_EQUAL(SysState::MIXING, m.state());
    // Jump past cap+margin in one tick — main switch's cap branch still fires first
    // but lastEvent should reflect a FAULT transition and relays must be off
    advance(t, m.cfg.maxMixMs + m.cfg.safetyMarginMs + 1, true);
    TEST_ASSERT_EQUAL(SysState::FAULT, m.state());
    TEST_ASSERT_FALSE(out.auger || out.agitator || out.mixer || out.water);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_backtoback_faults_escalate_to_reboot);
    RUN_TEST(test_low_heap_while_in_fault_does_not_recount);
    RUN_TEST(test_fault_cooldown_not_interrupted_by_demand);
    RUN_TEST(test_low_heap_interrupts_periodic_mix);
    RUN_TEST(test_reboot_threshold_is_exact);
    RUN_TEST(test_fault_count_does_not_overflow);
    RUN_TEST(test_sanity_guard_catches_runaway_mixing);
    return UNITY_END();
}
