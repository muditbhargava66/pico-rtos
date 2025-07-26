#include <stdint.h>
#include <stdbool.h>

// Test basic ARM Cortex-M0+ features
volatile uint32_t test_var = 0;

int main(void) {
    test_var = 42;
    return (int)test_var;
}
