// Unity tests for LevelDebouncer.  Run: pio test -e native -f test_debouncer
#include <unity.h>
#include "level_debouncer.h"

static LevelDebouncer d;

void setUp()    { d = LevelDebouncer(50); d.reset(false, 0); }
void tearDown() {}

void test_initial_state_matches_reset_value() {
    d.reset(true, 0);
    TEST_ASSERT_TRUE(d.stable());
}

void test_holds_through_short_glitch() {
    // Raw line glitches HIGH for 10 ms then returns LOW — never stable.
    TEST_ASSERT_FALSE(d.update(true,  10));
    TEST_ASSERT_FALSE(d.update(false, 20));
    TEST_ASSERT_FALSE(d.update(false, 60));
    TEST_ASSERT_FALSE(d.stable());
}

void test_accepts_change_after_stable_ms() {
    // Raw HIGH held steadily past 50 ms: stable flips HIGH once.
    TEST_ASSERT_FALSE(d.update(true, 10));     // starts window
    TEST_ASSERT_FALSE(d.update(true, 40));     // still within window
    TEST_ASSERT_TRUE (d.update(true, 60));     // 50 ms elapsed -> change
    TEST_ASSERT_TRUE (d.stable());
    TEST_ASSERT_FALSE(d.update(true, 70));     // no further change reported
}

void test_rapid_toggle_stays_unstable() {
    uint32_t t = 0;
    bool lvl = false;
    bool changed = false;
    for (int i = 0; i < 20; i++) {
        t += 10;
        lvl = !lvl;
        if (d.update(lvl, t)) changed = true;
    }
    TEST_ASSERT_FALSE(changed);
    TEST_ASSERT_FALSE(d.stable());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_initial_state_matches_reset_value);
    RUN_TEST(test_holds_through_short_glitch);
    RUN_TEST(test_accepts_change_after_stable_ms);
    RUN_TEST(test_rapid_toggle_stays_unstable);
    return UNITY_END();
}
