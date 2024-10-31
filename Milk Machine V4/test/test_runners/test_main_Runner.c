#include "unity.h"
#include "unity_config.h"

extern void setUp(void);
extern void tearDown(void);
extern void test_main(void);

int main(void) {
    UNITY_BEGIN();
    setUp();
    test_main();
    tearDown();
    return UNITY_END();
}