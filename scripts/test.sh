#!/bin/bash
# Pico-RTOS v0.3.1 Test Runner
# Comprehensive test runner for Pico-RTOSn script

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test configuration
RUN_UNIT_TESTS=true
RUN_INTEGRATION_TESTS=true
RUN_PERFORMANCE_TESTS=false
RUN_HARDWARE_TESTS=false
VERBOSE=false
PARALLEL_JOBS=4

usage() {
    cat << EOF
Pico-RTOS v0.3.1 Test Runner

Usage: $0 [OPTIONS]

Options:
    -h, --help              Show this help message
    -u, --unit-only         Run unit tests only
    -i, --integration-only  Run integration tests only
    -p, --performance       Run performance tests
    -w, --hardware          Run hardware-in-the-loop tests
    -v, --verbose           Enable verbose test output
    -j, --jobs N            Number of parallel test jobs (default: 4)
    --all                   Run all test suites
    --quick                 Run quick test suite only

Test Categories:
    Unit Tests              Individual component tests
    Integration Tests       Cross-component interaction tests
    Performance Tests       Timing and resource usage tests
    Hardware Tests          Real hardware validation tests

Examples:
    $0                      # Run unit and integration tests
    $0 --unit-only          # Run unit tests only
    $0 --all --verbose      # Run all tests with verbose output
    $0 --performance        # Include performance benchmarks
EOF
}

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

check_build() {
    if [ ! -d "$BUILD_DIR" ]; then
        log_error "Build directory not found. Run 'scripts/build.sh --tests' first."
        return 1
    fi
    
    if [ ! -f "$BUILD_DIR/Makefile" ] && [ ! -f "$BUILD_DIR/build.ninja" ]; then
        log_error "No build system found in $BUILD_DIR"
        return 1
    fi
    
    return 0
}

run_unit_tests() {
    log_info "Running unit tests..."
    
    local test_dir="$BUILD_DIR/tests/unit"
    local test_count=0
    local passed_count=0
    local failed_tests=()
    
    if [ ! -d "$test_dir" ]; then
        log_warning "Unit test directory not found: $test_dir"
        return 0
    fi
    
    # Find all unit test executables
    local test_files=($(find "$test_dir" -name "*_test" -type f -executable 2>/dev/null || true))
    
    if [ ${#test_files[@]} -eq 0 ]; then
        log_warning "No unit test executables found"
        return 0
    fi
    
    log_info "Found ${#test_files[@]} unit test suites"
    
    for test_file in "${test_files[@]}"; do
        local test_name=$(basename "$test_file")
        test_count=$((test_count + 1))
        
        log_info "Running $test_name..."
        
        if [ "$VERBOSE" = true ]; then
            if "$test_file"; then
                log_success "$test_name PASSED"
                passed_count=$((passed_count + 1))
            else
                log_error "$test_name FAILED"
                failed_tests+=("$test_name")
            fi
        else
            if "$test_file" >/dev/null 2>&1; then
                log_success "$test_name PASSED"
                passed_count=$((passed_count + 1))
            else
                log_error "$test_name FAILED"
                failed_tests+=("$test_name")
            fi
        fi
    done
    
    # Summary
    echo
    log_info "Unit Test Summary:"
    echo "  Total: $test_count"
    echo "  Passed: $passed_count"
    echo "  Failed: $((test_count - passed_count))"
    
    if [ ${#failed_tests[@]} -gt 0 ]; then
        echo "  Failed tests: ${failed_tests[*]}"
        return 1
    fi
    
    return 0
}

run_integration_tests() {
    log_info "Running integration tests..."
    
    local test_dir="$BUILD_DIR/tests/integration"
    local test_count=0
    local passed_count=0
    local failed_tests=()
    
    if [ ! -d "$test_dir" ]; then
        log_warning "Integration test directory not found: $test_dir"
        return 0
    fi
    
    # Find all integration test executables
    local test_files=($(find "$test_dir" -name "*_test" -type f -executable 2>/dev/null || true))
    
    if [ ${#test_files[@]} -eq 0 ]; then
        log_warning "No integration test executables found"
        return 0
    fi
    
    log_info "Found ${#test_files[@]} integration test suites"
    
    for test_file in "${test_files[@]}"; do
        local test_name=$(basename "$test_file")
        test_count=$((test_count + 1))
        
        log_info "Running $test_name..."
        
        if [ "$VERBOSE" = true ]; then
            if "$test_file"; then
                log_success "$test_name PASSED"
                passed_count=$((passed_count + 1))
            else
                log_error "$test_name FAILED"
                failed_tests+=("$test_name")
            fi
        else
            if "$test_file" >/dev/null 2>&1; then
                log_success "$test_name PASSED"
                passed_count=$((passed_count + 1))
            else
                log_error "$test_name FAILED"
                failed_tests+=("$test_name")
            fi
        fi
    done
    
    # Summary
    echo
    log_info "Integration Test Summary:"
    echo "  Total: $test_count"
    echo "  Passed: $passed_count"
    echo "  Failed: $((test_count - passed_count))"
    
    if [ ${#failed_tests[@]} -gt 0 ]; then
        echo "  Failed tests: ${failed_tests[*]}"
        return 1
    fi
    
    return 0
}

run_performance_tests() {
    log_info "Running performance tests..."
    
    local test_dir="$BUILD_DIR/tests/performance"
    
    if [ ! -d "$test_dir" ]; then
        log_warning "Performance test directory not found: $test_dir"
        return 0
    fi
    
    # Find all performance test executables
    local test_files=($(find "$test_dir" -name "*_perf" -type f -executable 2>/dev/null || true))
    
    if [ ${#test_files[@]} -eq 0 ]; then
        log_warning "No performance test executables found"
        return 0
    fi
    
    log_info "Found ${#test_files[@]} performance test suites"
    
    for test_file in "${test_files[@]}"; do
        local test_name=$(basename "$test_file")
        log_info "Running $test_name..."
        
        if "$test_file"; then
            log_success "$test_name completed"
        else
            log_error "$test_name failed"
            return 1
        fi
    done
    
    return 0
}

run_hardware_tests() {
    log_info "Running hardware-in-the-loop tests..."
    
    # Check if hardware is connected
    if ! command -v openocd >/dev/null 2>&1; then
        log_warning "OpenOCD not found - skipping hardware tests"
        return 0
    fi
    
    local test_dir="$BUILD_DIR/tests/hardware"
    
    if [ ! -d "$test_dir" ]; then
        log_warning "Hardware test directory not found: $test_dir"
        return 0
    fi
    
    # Find all hardware test executables
    local test_files=($(find "$test_dir" -name "*_hw" -type f -executable 2>/dev/null || true))
    
    if [ ${#test_files[@]} -eq 0 ]; then
        log_warning "No hardware test executables found"
        return 0
    fi
    
    log_info "Found ${#test_files[@]} hardware test suites"
    log_warning "Hardware tests require connected RP2040 device"
    
    for test_file in "${test_files[@]}"; do
        local test_name=$(basename "$test_file")
        log_info "Running $test_name..."
        
        if "$test_file"; then
            log_success "$test_name completed"
        else
            log_error "$test_name failed"
            return 1
        fi
    done
    
    return 0
}

generate_test_report() {
    local report_file="$BUILD_DIR/test_report.txt"
    
    log_info "Generating test report..."
    
    cat > "$report_file" << EOF
Pico-RTOS v0.3.1 Test Report
Generated: $(date)
============================

Test Configuration:
- Unit Tests: $([ "$RUN_UNIT_TESTS" = true ] && echo "Enabled" || echo "Disabled")
- Integration Tests: $([ "$RUN_INTEGRATION_TESTS" = true ] && echo "Enabled" || echo "Disabled")
- Performance Tests: $([ "$RUN_PERFORMANCE_TESTS" = true ] && echo "Enabled" || echo "Disabled")
- Hardware Tests: $([ "$RUN_HARDWARE_TESTS" = true ] && echo "Enabled" || echo "Disabled")

Build Information:
- Build Type: $(grep CMAKE_BUILD_TYPE "$BUILD_DIR/CMakeCache.txt" 2>/dev/null | cut -d= -f2 || echo "Unknown")
- Compiler: $(arm-none-eabi-gcc --version | head -n1 2>/dev/null || echo "Unknown")
- CMake Version: $(cmake --version | head -n1 2>/dev/null || echo "Unknown")

Test Results:
$(cat "$BUILD_DIR"/*.log 2>/dev/null || echo "No detailed logs available")

EOF
    
    log_success "Test report generated: $report_file"
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            usage
            exit 0
            ;;
        -u|--unit-only)
            RUN_UNIT_TESTS=true
            RUN_INTEGRATION_TESTS=false
            RUN_PERFORMANCE_TESTS=false
            RUN_HARDWARE_TESTS=false
            shift
            ;;
        -i|--integration-only)
            RUN_UNIT_TESTS=false
            RUN_INTEGRATION_TESTS=true
            RUN_PERFORMANCE_TESTS=false
            RUN_HARDWARE_TESTS=false
            shift
            ;;
        -p|--performance)
            RUN_PERFORMANCE_TESTS=true
            shift
            ;;
        -w|--hardware)
            RUN_HARDWARE_TESTS=true
            shift
            ;;
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        -j|--jobs)
            PARALLEL_JOBS="$2"
            shift 2
            ;;
        --all)
            RUN_UNIT_TESTS=true
            RUN_INTEGRATION_TESTS=true
            RUN_PERFORMANCE_TESTS=true
            RUN_HARDWARE_TESTS=true
            shift
            ;;
        --quick)
            RUN_UNIT_TESTS=true
            RUN_INTEGRATION_TESTS=false
            RUN_PERFORMANCE_TESTS=false
            RUN_HARDWARE_TESTS=false
            shift
            ;;
        *)
            log_error "Unknown option: $1"
            usage
            exit 1
            ;;
    esac
done

# Main execution
main() {
    echo "Pico-RTOS v0.3.1 Test Runner"
    echo "============================="
    
    # Check build
    if ! check_build; then
        exit 1
    fi
    
    local overall_result=0
    
    # Run selected test suites
    if [ "$RUN_UNIT_TESTS" = true ]; then
        if ! run_unit_tests; then
            overall_result=1
        fi
    fi
    
    if [ "$RUN_INTEGRATION_TESTS" = true ]; then
        if ! run_integration_tests; then
            overall_result=1
        fi
    fi
    
    if [ "$RUN_PERFORMANCE_TESTS" = true ]; then
        if ! run_performance_tests; then
            overall_result=1
        fi
    fi
    
    if [ "$RUN_HARDWARE_TESTS" = true ]; then
        if ! run_hardware_tests; then
            overall_result=1
        fi
    fi
    
    # Generate report
    generate_test_report
    
    # Final summary
    echo
    if [ $overall_result -eq 0 ]; then
        log_success "All tests completed successfully!"
    else
        log_error "Some tests failed!"
    fi
    
    exit $overall_result
}

# Run main function
main "$@"