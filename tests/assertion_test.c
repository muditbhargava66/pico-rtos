#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <setjmp.h>
#include "pico_rtos.h"
#include "pico_rtos/error.h"

// Test configuration
#define TEST_ASSERTION_MESSAGE "Test assertion failure"

// Global variables for testing
static jmp_buf test_jump_buffer;
static bool assertion_handler_called = false;
static pico_rtos_assert_info_t last_assert_info;

// =============================================================================
// TEST HELPER FUNCTIONS
// =============================================================================

#if PICO_RTOS_ENABLE_ENHANCED_ASSERTIONS && PICO_RTOS_ASSERTION_HANDLER_CONFIGURABLE

/**
 * @brief Test assertion handler that captures assertion info
 */
static pico_rtos_assert_action_t test_assertion_handler(const pico_rtos_assert_info_t *assert_info) {
    assertion_handler_called = true;
    last_assert_info = *assert_info;
    
    // Jump back to test to avoid halting
    longjmp(test_jump_buffer, 1);
    
    return PICO_RTOS_ASSERT_ACTION_CONTINUE;
}

/**
 * @brief Test assertion handler that returns continue action
 */
static pico_rtos_assert_action_t continue_assertion_handler(const pico_rtos_assert_info_t *assert_info) {
    assertion_handler_called = true;
    last_assert_info = *assert_info;
    return PICO_RTOS_ASSERT_ACTION_CONTINUE;
}

#endif

/**
 * @brief Reset test state
 */
static void reset_test_state(void) {
    assertion_handler_called = false;
    memset(&last_assert_info, 0, sizeof(last_assert_info));
    pico_rtos_reset_assert_stats();
}

// =============================================================================
// ASSERTION CONFIGURATION TESTS
// =============================================================================

static void test_assertion_configuration(void) {
    printf("Testing assertion configuration...\n");
    
#if PICO_RTOS_ENABLE_ENHANCED_ASSERTIONS
    
    // Test default action
    assert(pico_rtos_get_assert_action() == PICO_RTOS_ASSERT_ACTION_HALT);
    
    // Test setting action
    pico_rtos_set_assert_action(PICO_RTOS_ASSERT_ACTION_CONTINUE);
    assert(pico_rtos_get_assert_action() == PICO_RTOS_ASSERT_ACTION_CONTINUE);
    
    pico_rtos_set_assert_action(PICO_RTOS_ASSERT_ACTION_CALLBACK);
    assert(pico_rtos_get_assert_action() == PICO_RTOS_ASSERT_ACTION_CALLBACK);
    
    // Reset to default
    pico_rtos_set_assert_action(PICO_RTOS_ASSERT_ACTION_HALT);
    
#if PICO_RTOS_ASSERTION_HANDLER_CONFIGURABLE
    // Test handler configuration
    assert(pico_rtos_get_assert_handler() == NULL);
    
    pico_rtos_set_assert_handler(test_assertion_handler);
    assert(pico_rtos_get_assert_handler() == test_assertion_handler);
    
    pico_rtos_set_assert_handler(NULL);
    assert(pico_rtos_get_assert_handler() == NULL);
#endif
    
#endif // PICO_RTOS_ENABLE_ENHANCED_ASSERTIONS
    
    printf("✓ Assertion configuration tests passed\n");
}

// =============================================================================
// ASSERTION STATISTICS TESTS
// =============================================================================

static void test_assertion_statistics(void) {
    printf("Testing assertion statistics...\n");
    
#if PICO_RTOS_ENABLE_ENHANCED_ASSERTIONS
    
    reset_test_state();
    
    // Get initial statistics
    pico_rtos_assert_stats_t stats;
    pico_rtos_get_assert_stats(&stats);
    
    // Should be all zeros initially
    assert(stats.total_assertions == 0);
    assert(stats.failed_assertions == 0);
    assert(stats.continued_assertions == 0);
    assert(stats.halted_assertions == 0);
    
    // Test statistics reset
    pico_rtos_reset_assert_stats();
    pico_rtos_get_assert_stats(&stats);
    assert(stats.total_assertions == 0);
    
#endif // PICO_RTOS_ENABLE_ENHANCED_ASSERTIONS
    
    printf("✓ Assertion statistics tests passed\n");
}

// =============================================================================
// ASSERTION MACRO TESTS
// =============================================================================

static void test_assertion_macros(void) {
    printf("Testing assertion macros...\n");
    
#if PICO_RTOS_ENABLE_ENHANCED_ASSERTIONS
    
    reset_test_state();
    
    // Test successful assertion (should not trigger)
    PICO_RTOS_ASSERT(1 == 1);
    PICO_RTOS_ASSERT_MSG(2 + 2 == 4, "Math should work");
    
    // Verify no assertions failed
    pico_rtos_assert_stats_t stats;
    pico_rtos_get_assert_stats(&stats);
    assert(stats.failed_assertions == 0);
    
    // Test parameter assertion (should be no-op in release builds)
    PICO_RTOS_ASSERT_PARAM(true);
    
    // Test internal assertion
    PICO_RTOS_ASSERT_INTERNAL(1 != 0);
    
#endif // PICO_RTOS_ENABLE_ENHANCED_ASSERTIONS
    
    printf("✓ Assertion macro tests passed\n");
}

// =============================================================================
// ASSERTION FAILURE TESTS
// =============================================================================

static void test_assertion_failures(void) {
    printf("Testing assertion failures...\n");
    
#if PICO_RTOS_ENABLE_ENHANCED_ASSERTIONS && PICO_RTOS_ASSERTION_HANDLER_CONFIGURABLE
    
    reset_test_state();
    
    // Set up continue action to avoid halting
    pico_rtos_set_assert_action(PICO_RTOS_ASSERT_ACTION_CONTINUE);
    pico_rtos_set_assert_handler(continue_assertion_handler);
    
    // Test assertion failure
    PICO_RTOS_ASSERT(1 == 0); // This should fail
    
    // Verify handler was called
    assert(assertion_handler_called);
    assert(last_assert_info.expression != NULL);
    assert(strstr(last_assert_info.expression, "1 == 0") != NULL);
    assert(last_assert_info.file != NULL);
    assert(last_assert_info.line > 0);
    assert(last_assert_info.function != NULL);
    
    // Test assertion with message
    reset_test_state();
    pico_rtos_set_assert_handler(continue_assertion_handler);
    
    PICO_RTOS_ASSERT_MSG(false, TEST_ASSERTION_MESSAGE);
    
    // Verify handler was called with message
    assert(assertion_handler_called);
    assert(last_assert_info.message != NULL);
    assert(strcmp(last_assert_info.message, TEST_ASSERTION_MESSAGE) == 0);
    
    // Test statistics after failures
    pico_rtos_assert_stats_t stats;
    pico_rtos_get_assert_stats(&stats);
    assert(stats.failed_assertions == 2);
    assert(stats.continued_assertions == 2);
    
    // Reset handler
    pico_rtos_set_assert_handler(NULL);
    pico_rtos_set_assert_action(PICO_RTOS_ASSERT_ACTION_HALT);
    
#endif // PICO_RTOS_ENABLE_ENHANCED_ASSERTIONS && PICO_RTOS_ASSERTION_HANDLER_CONFIGURABLE
    
    printf("✓ Assertion failure tests passed\n");
}

// =============================================================================
// ASSERTION HANDLER TESTS
// =============================================================================

static void test_assertion_handlers(void) {
    printf("Testing assertion handlers...\n");
    
#if PICO_RTOS_ENABLE_ENHANCED_ASSERTIONS && PICO_RTOS_ASSERTION_HANDLER_CONFIGURABLE
    
    reset_test_state();
    
    // Test handler that returns different actions
    pico_rtos_set_assert_handler(continue_assertion_handler);
    
    // Test continue action
    PICO_RTOS_ASSERT(0); // Should fail but continue
    assert(assertion_handler_called);
    
    // Verify statistics
    pico_rtos_assert_stats_t stats;
    pico_rtos_get_assert_stats(&stats);
    assert(stats.failed_assertions == 1);
    assert(stats.continued_assertions == 1);
    
    // Test callback action
    reset_test_state();
    pico_rtos_set_assert_action(PICO_RTOS_ASSERT_ACTION_CALLBACK);
    pico_rtos_set_assert_handler(continue_assertion_handler);
    
    PICO_RTOS_ASSERT(false);
    assert(assertion_handler_called);
    
    // Reset
    pico_rtos_set_assert_handler(NULL);
    pico_rtos_set_assert_action(PICO_RTOS_ASSERT_ACTION_HALT);
    
#endif // PICO_RTOS_ENABLE_ENHANCED_ASSERTIONS && PICO_RTOS_ASSERTION_HANDLER_CONFIGURABLE
    
    printf("✓ Assertion handler tests passed\n");
}

// =============================================================================
// STATIC ASSERTION TESTS
// =============================================================================

static void test_static_assertions(void) {
    printf("Testing static assertions...\n");
    
    // These should compile without issues
    PICO_RTOS_STATIC_ASSERT(sizeof(int) >= 2, "int must be at least 2 bytes");
    PICO_RTOS_STATIC_ASSERT(1 + 1 == 2, "Basic math must work");
    
    // Test with constant expressions
    #define TEST_CONSTANT 42
    PICO_RTOS_STATIC_ASSERT(TEST_CONSTANT == 42, "Constant should be 42");
    
    printf("✓ Static assertion tests passed\n");
}

// =============================================================================
// ASSERTION CONTEXT TESTS
// =============================================================================

static void test_assertion_context(void) {
    printf("Testing assertion context capture...\n");
    
#if PICO_RTOS_ENABLE_ENHANCED_ASSERTIONS && PICO_RTOS_ASSERTION_HANDLER_CONFIGURABLE
    
    reset_test_state();
    pico_rtos_set_assert_handler(continue_assertion_handler);
    
    // Trigger assertion to capture context
    PICO_RTOS_ASSERT_MSG(0 != 0, "Context test");
    
    // Verify context information was captured
    assert(assertion_handler_called);
    assert(last_assert_info.expression != NULL);
    assert(last_assert_info.file != NULL);
    assert(last_assert_info.line > 0);
    assert(last_assert_info.function != NULL);
    assert(last_assert_info.timestamp > 0);
    assert(last_assert_info.message != NULL);
    assert(strcmp(last_assert_info.message, "Context test") == 0);
    
    // Verify file name contains this test file
    assert(strstr(last_assert_info.file, "assertion_test.c") != NULL);
    
    // Verify function name
    assert(strstr(last_assert_info.function, "test_assertion_context") != NULL);
    
    // Reset
    pico_rtos_set_assert_handler(NULL);
    
#endif // PICO_RTOS_ENABLE_ENHANCED_ASSERTIONS && PICO_RTOS_ASSERTION_HANDLER_CONFIGURABLE
    
    printf("✓ Assertion context tests passed\n");
}

// =============================================================================
// ASSERTION FAIL TESTS
// =============================================================================

static void test_assertion_fail(void) {
    printf("Testing assertion fail macro...\n");
    
#if PICO_RTOS_ENABLE_ENHANCED_ASSERTIONS && PICO_RTOS_ASSERTION_HANDLER_CONFIGURABLE
    
    reset_test_state();
    pico_rtos_set_assert_handler(continue_assertion_handler);
    
    // Test ASSERT_FAIL macro
    PICO_RTOS_ASSERT_FAIL("Intentional failure");
    
    // Verify handler was called
    assert(assertion_handler_called);
    assert(last_assert_info.expression != NULL);
    assert(strcmp(last_assert_info.expression, "ASSERT_FAIL") == 0);
    assert(last_assert_info.message != NULL);
    assert(strcmp(last_assert_info.message, "Intentional failure") == 0);
    
    // Verify statistics
    pico_rtos_assert_stats_t stats;
    pico_rtos_get_assert_stats(&stats);
    assert(stats.failed_assertions == 1);
    
    // Reset
    pico_rtos_set_assert_handler(NULL);
    
#endif // PICO_RTOS_ENABLE_ENHANCED_ASSERTIONS && PICO_RTOS_ASSERTION_HANDLER_CONFIGURABLE
    
    printf("✓ Assertion fail tests passed\n");
}

// =============================================================================
// DISABLED ASSERTION TESTS
// =============================================================================

static void test_disabled_assertions(void) {
    printf("Testing disabled assertion behavior...\n");
    
#if !PICO_RTOS_ENABLE_ENHANCED_ASSERTIONS
    
    // When assertions are disabled, these should compile to standard assert
    // We can't easily test the disabled behavior without causing actual assertion failures
    // So we'll just verify the macros compile
    
    // These should compile without issues when assertions are disabled
    if (false) {
        PICO_RTOS_ASSERT(true);
        PICO_RTOS_ASSERT_MSG(true, "test");
        PICO_RTOS_ASSERT_PARAM(true);
        PICO_RTOS_ASSERT_INTERNAL(true);
    }
    
#endif
    
    printf("✓ Disabled assertion tests passed\n");
}

// =============================================================================
// INTEGRATION TESTS
// =============================================================================

static void test_assertion_error_integration(void) {
    printf("Testing assertion-error system integration...\n");
    
#if PICO_RTOS_ENABLE_ENHANCED_ASSERTIONS && PICO_RTOS_ASSERTION_HANDLER_CONFIGURABLE
    
    reset_test_state();
    pico_rtos_set_assert_handler(continue_assertion_handler);
    
    // Clear any previous errors
    pico_rtos_clear_last_error();
    
    // Trigger assertion failure
    PICO_RTOS_ASSERT(false);
    
    // Verify assertion was handled
    assert(assertion_handler_called);
    
    // Verify error was also reported
    pico_rtos_error_info_t error_info;
    assert(pico_rtos_get_last_error(&error_info));
    assert(error_info.code == PICO_RTOS_ERROR_CRITICAL_SECTION_VIOLATION);
    
    // Reset
    pico_rtos_set_assert_handler(NULL);
    pico_rtos_clear_last_error();
    
#endif // PICO_RTOS_ENABLE_ENHANCED_ASSERTIONS && PICO_RTOS_ASSERTION_HANDLER_CONFIGURABLE
    
    printf("✓ Assertion-error integration tests passed\n");
}

// =============================================================================
// MAIN TEST FUNCTION
// =============================================================================

int main(void) {
    printf("Starting Pico-RTOS Assertion System Tests\n");
    printf("==========================================\n");
    
    // Initialize RTOS and error system
    assert(pico_rtos_init());
    assert(pico_rtos_error_init());
    
    // Run tests
    test_assertion_configuration();
    test_assertion_statistics();
    test_assertion_macros();
    test_assertion_failures();
    test_assertion_handlers();
    test_static_assertions();
    test_assertion_context();
    test_assertion_fail();
    test_disabled_assertions();
    test_assertion_error_integration();
    
    printf("\n==========================================\n");
    printf("All Assertion System Tests Passed! ✓\n");
    printf("==========================================\n");
    
    return 0;
}