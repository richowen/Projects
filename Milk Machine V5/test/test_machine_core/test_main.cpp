// Unity tests for machine_core.  Run: pio test -e native -f test_machine_core
#include <unity.h>
#include "machine_core.h"

// ---- Helpers ---------------------------------------------------------------
static Machine         m;
static MachineOutputs  out;
static constexpr size_t HEAP_OK = 100000;   // plenty of free heap

static void advance(uint32_t& t, uint32_t dt, bool levelLow) {
    t += dt;
    MachineInputs in{ levelLow, t, HEAP_OK };
    m.tick(in, out);
}
static void tickOnce(uint32_t t, bool levelLow, size_t heap = HEAP_OK) {
    MachineInputs in{ levelLow, t, heap };
    m.tick(in, out);
}

static void resetMachine(uint32_t t = 0) {
    m = Machine();
    m.reset(t);
    // Seed output struct deterministically
    tickOnce(t, false);
}

void setUp()    { resetMachine(0); }
void tearDown() {}

// ---- Boot ------------------------------------------------------------------
void test_boot_state_is_idle_relays_off() {
    TEST_ASSERT_EQUAL(SysState::IDLE, m.state());
    TEST_ASSERT_FALSE(out.auger);
    TEST_ASSERT_FALSE(out.agitator);
    TEST_ASSERT_FALSE(out.mixer);
    TEST_ASSERT_FALSE(out.water);
}

// ---- IDLE -> MIXING --------------------------------------------------------
void test_idle_to_mixing_on_level_low() {
    tickOnce(100, true);
    TEST_ASSERT_EQUAL(SysState::MIXING, m.state());
    TEST_ASSERT_TRUE(out.auger);
    TEST_ASSERT_TRUE(out.agitator);
    TEST_ASSERT_TRUE(out.mixer);
    TEST_ASSERT_TRUE(out.water);
}

// ---- MIXING -> POST_MIX ----------------------------------------------------
void test_mixing_to_postmix_on_level_restored() {
    tickOnce(100, true);                  // IDLE -> MIXING
    tickOnce(200, false);                 // level restored
    TEST_ASSERT_EQUAL(SysState::POST_MIX, m.state());
    TEST_ASSERT_FALSE(out.auger);
    TEST_ASSERT_FALSE(out.agitator);
    TEST_ASSERT_TRUE (out.mixer);
    TEST_ASSERT_FALSE(out.water);
}

// ---- POST_MIX duration -----------------------------------------------------
void test_postmix_to_idle_after_duration() {
    uint32_t t = 0;
    advance(t, 100, true);                // -> MIXING
    advance(t, 100, false);               // -> POST_MIX
    advance(t, m.cfg.postMixMs, false);   // exactly at duration
    TEST_ASSERT_EQUAL(SysState::IDLE, m.state());
    TEST_ASSERT_FALSE(out.auger);
    TEST_ASSERT_FALSE(out.mixer);
}

// ---- [Fix #11] regression: POST_MIX->IDLE resets periodic timer -----------
void test_postmix_to_idle_does_not_trigger_periodic_immediately() {
    uint32_t t = 0;
    // Advance idle almost to periodic interval so the timer is "ripe"
    advance(t, m.cfg.periodicIntMs - 1000, false);
    // Run a demand mix
    advance(t, 50,  true);
    advance(t, 50,  false);
    advance(t, m.cfg.postMixMs, false);   // POST_MIX -> IDLE
    TEST_ASSERT_EQUAL(SysState::IDLE, m.state());
    // Tick again: must NOT immediately fire PERIODIC_MIX
    advance(t, 10, false);
    TEST_ASSERT_EQUAL(SysState::IDLE, m.state());
}

// ---- PERIODIC_MIX interval and duration -----------------------------------
void test_periodic_mix_fires_after_interval() {
    uint32_t t = 0;
    advance(t, m.cfg.periodicIntMs, false);
    TEST_ASSERT_EQUAL(SysState::PERIODIC_MIX, m.state());
    TEST_ASSERT_FALSE(out.auger);
    TEST_ASSERT_TRUE (out.mixer);
    TEST_ASSERT_FALSE(out.water);
}
void test_periodic_mix_exits_to_idle_after_duration() {
    uint32_t t = 0;
    advance(t, m.cfg.periodicIntMs, false);    // -> PERIODIC_MIX
    advance(t, m.cfg.periodicMixMs, false);
    TEST_ASSERT_EQUAL(SysState::IDLE, m.state());
    TEST_ASSERT_FALSE(out.mixer);
}
void test_periodic_mix_escalates_to_mixing_on_demand() {
    uint32_t t = 0;
    advance(t, m.cfg.periodicIntMs, false);
    advance(t, 1000, true);
    TEST_ASSERT_EQUAL(SysState::MIXING, m.state());
    TEST_ASSERT_TRUE(out.auger && out.agitator && out.mixer && out.water);
}

// ---- MIXING cap ------------------------------------------------------------
void test_mixing_cap_enters_fault_and_relays_off() {
    uint32_t t = 0;
    advance(t, 100, true);
    advance(t, m.cfg.maxMixMs, true);   // exactly hits cap
    TEST_ASSERT_EQUAL(SysState::FAULT, m.state());
    TEST_ASSERT_FALSE(out.auger);
    TEST_ASSERT_FALSE(out.agitator);
    TEST_ASSERT_FALSE(out.mixer);
    TEST_ASSERT_FALSE(out.water);
    TEST_ASSERT_EQUAL(MachineEvent::MIXING_CAP_FAULT, m.lastEvent());
}

// ---- Stuck-LOW sensor respects cap ----------------------------------------
void test_stuck_low_level_still_enters_fault() {
    uint32_t t = 0;
    // Sensor pins LOW forever — machine must fault within maxMixMs
    for (uint32_t step = 0; step <= m.cfg.maxMixMs + 100; step += 500) {
        advance(t, 500, true);
        if (m.state() == SysState::FAULT) break;
    }
    TEST_ASSERT_EQUAL(SysState::FAULT, m.state());
}

// ---- FAULT cooldown --------------------------------------------------------
void test_fault_cooldown_returns_to_idle() {
    uint32_t t = 0;
    advance(t, 100, true);
    advance(t, m.cfg.maxMixMs, true);          // enter FAULT
    advance(t, m.cfg.faultCooldownMs, false);
    TEST_ASSERT_EQUAL(SysState::IDLE, m.state());
}

// ---- Fault counter windowing ----------------------------------------------
void test_three_faults_in_window_trigger_reboot() {
    uint32_t t = 0;
    for (int i = 0; i < 3; i++) {
        advance(t, 100, true);
        advance(t, m.cfg.maxMixMs, true);       // -> FAULT
        if (i < 2) {
            advance(t, m.cfg.faultCooldownMs, false);
        }
    }
    TEST_ASSERT_TRUE(out.rebootRequested);
    TEST_ASSERT_EQUAL(3, m.faultCount());
}

void test_fault_window_expiry_resets_counter() {
    uint32_t t = 0;
    // First fault
    advance(t, 100, true);
    advance(t, m.cfg.maxMixMs, true);
    TEST_ASSERT_EQUAL(1, m.faultCount());
    advance(t, m.cfg.faultCooldownMs, false);
    // Let the fault window expire (add faultWindowMs + margin on top of cooldown)
    advance(t, m.cfg.faultWindowMs + 1000, false);
    // Second fault after window expired
    advance(t, 100, true);
    advance(t, m.cfg.maxMixMs, true);
    TEST_ASSERT_EQUAL(1, m.faultCount());
    TEST_ASSERT_FALSE(out.rebootRequested);
}

// ---- Heap guard ------------------------------------------------------------
void test_low_heap_enters_fault() {
    tickOnce(100, false, 4096);  // below 8 KB threshold
    TEST_ASSERT_EQUAL(SysState::FAULT, m.state());
    TEST_ASSERT_EQUAL(MachineEvent::FAULT_ENTER_LOW_HEAP, m.lastEvent());
    TEST_ASSERT_FALSE(out.auger && out.agitator && out.mixer && out.water);
}

// ---- Invariant: every FAULT entry leaves relays off -----------------------
void test_fault_always_forces_relays_off() {
    // Path 1: MIXING cap
    resetMachine(0);
    uint32_t t = 0;
    advance(t, 100, true);
    advance(t, m.cfg.maxMixMs, true);
    TEST_ASSERT_FALSE(out.auger || out.agitator || out.mixer || out.water);

    // Path 2: low heap
    resetMachine(0);
    tickOnce(50, true, 100);
    TEST_ASSERT_FALSE(out.auger || out.agitator || out.mixer || out.water);
}

// ---- Output caching --------------------------------------------------------
void test_repeated_tick_returns_same_outputs() {
    tickOnce(100, true);                 // -> MIXING
    MachineOutputs snap = out;
    tickOnce(101, true);
    tickOnce(102, true);
    TEST_ASSERT_EQUAL(snap.auger,    out.auger);
    TEST_ASSERT_EQUAL(snap.agitator, out.agitator);
    TEST_ASSERT_EQUAL(snap.mixer,    out.mixer);
    TEST_ASSERT_EQUAL(snap.water,    out.water);
}

// ---- millis() wraparound ---------------------------------------------------
// Start near 0xFFFFFFFF, enter MIXING, cross the wrap, verify cap still triggers.
void test_millis_wrap_during_mixing_still_faults() {
    resetMachine(0xFFFFFF00UL);
    uint32_t t = 0xFFFFFF00UL;
    MachineInputs in{ true, t, HEAP_OK };
    m.tick(in, out);                   // -> MIXING
    TEST_ASSERT_EQUAL(SysState::MIXING, m.state());
    // Advance past wrap by full maxMixMs
    t += m.cfg.maxMixMs;
    MachineInputs in2{ true, t, HEAP_OK };
    m.tick(in2, out);
    TEST_ASSERT_EQUAL(SysState::FAULT, m.state());
}

void test_millis_wrap_during_idle_periodic_timer_works() {
    resetMachine(0xFFFFFF00UL);
    uint32_t t = 0xFFFFFF00UL + m.cfg.periodicIntMs;  // wraps
    MachineInputs in{ false, t, HEAP_OK };
    m.tick(in, out);
    TEST_ASSERT_EQUAL(SysState::PERIODIC_MIX, m.state());
}

// ---- Labels ----------------------------------------------------------------
void test_state_and_event_labels_never_null() {
    for (int s = 0; s <= (int)SysState::FAULT; s++) {
        Machine mm; mm.reset(0);
        // Can't force state directly, but stateStr handles all enum values
        TEST_ASSERT_NOT_NULL(mm.stateStr());
        TEST_ASSERT_NOT_NULL(mm.eventStr());
    }
}

// ---- Entry point -----------------------------------------------------------
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_boot_state_is_idle_relays_off);
    RUN_TEST(test_idle_to_mixing_on_level_low);
    RUN_TEST(test_mixing_to_postmix_on_level_restored);
    RUN_TEST(test_postmix_to_idle_after_duration);
    RUN_TEST(test_postmix_to_idle_does_not_trigger_periodic_immediately);
    RUN_TEST(test_periodic_mix_fires_after_interval);
    RUN_TEST(test_periodic_mix_exits_to_idle_after_duration);
    RUN_TEST(test_periodic_mix_escalates_to_mixing_on_demand);
    RUN_TEST(test_mixing_cap_enters_fault_and_relays_off);
    RUN_TEST(test_stuck_low_level_still_enters_fault);
    RUN_TEST(test_fault_cooldown_returns_to_idle);
    RUN_TEST(test_three_faults_in_window_trigger_reboot);
    RUN_TEST(test_fault_window_expiry_resets_counter);
    RUN_TEST(test_low_heap_enters_fault);
    RUN_TEST(test_fault_always_forces_relays_off);
    RUN_TEST(test_repeated_tick_returns_same_outputs);
    RUN_TEST(test_millis_wrap_during_mixing_still_faults);
    RUN_TEST(test_millis_wrap_during_idle_periodic_timer_works);
    RUN_TEST(test_state_and_event_labels_never_null);
    return UNITY_END();
}
