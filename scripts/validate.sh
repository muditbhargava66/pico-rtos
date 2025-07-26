#!/bin/bash
# Pico-RTOS v0.3.0 Validation Script
# Comprehensive system validation and quality checks

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Validation flags
CHECK_CODE_STYLE=true
CHECK_DOCUMENTATION=true
CHECK_CONFIGURATION=true
CHECK_BUILD_SYSTEM=true
CHECK_EXAMPLES=true
CHECK_TESTS=true
VERBOSE=false

usage() {
    cat << EOF
Pico-RTOS v0.3.0 Validation Script

Usage: $0 [OPTIONS]

Options:
    -h, --help              Show this help message
    -v, --verbose           Enable verbose output
    --code-style-only       Check code style only
    --docs-only             Check documentation only
    --config-only           Check configuration only
    --build-only            Check build system only
    --examples-only         Check examples only
    --tests-only            Check tests only
    --quick                 Quick validation (essential checks only)

Validation Categories:
    Code Style              Coding standards and formatting
    Documentation           API docs and user guides
    Configuration           Kconfig and build configuration
    Build System            CMake and build scripts
    Examples                Example applications
    Tests                   Test suite completeness

Examples:
    $0                      # Run all validations
    $0 --quick              # Quick essential checks
    $0 --code-style-only    # Check code formatting only
    $0 --verbose            # Detailed validation output
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

check_code_style() {
    log_info "Checking code style and formatting..."
    
    local issues=0
    
    # Check for consistent indentation
    log_info "Checking indentation consistency..."
    local mixed_indent_files=$(find "$PROJECT_ROOT/src" "$PROJECT_ROOT/include" -name "*.c" -o -name "*.h" | xargs grep -l $'\t' 2>/dev/null | head -5)
    if [ -n "$mixed_indent_files" ]; then
        log_warning "Files with mixed indentation found:"
        echo "$mixed_indent_files"
        issues=$((issues + 1))
    fi
    
    # Check for trailing whitespace
    log_info "Checking for trailing whitespace..."
    local trailing_ws_files=$(find "$PROJECT_ROOT/src" "$PROJECT_ROOT/include" -name "*.c" -o -name "*.h" | xargs grep -l '[[:space:]]$' 2>/dev/null | head -5)
    if [ -n "$trailing_ws_files" ]; then
        log_warning "Files with trailing whitespace found:"
        echo "$trailing_ws_files"
        issues=$((issues + 1))
    fi
    
    # Check for long lines
    log_info "Checking for long lines (>120 characters)..."
    local long_line_files=$(find "$PROJECT_ROOT/src" "$PROJECT_ROOT/include" -name "*.c" -o -name "*.h" | xargs grep -l '.\{121\}' 2>/dev/null | head -5)
    if [ -n "$long_line_files" ]; then
        log_warning "Files with long lines found:"
        echo "$long_line_files"
        issues=$((issues + 1))
    fi
    
    # Check for consistent naming conventions
    log_info "Checking naming conventions..."
    local bad_naming=$(find "$PROJECT_ROOT/include" -name "*.h" | xargs grep -n 'typedef.*[a-z][A-Z]' 2>/dev/null | head -3)
    if [ -n "$bad_naming" ]; then
        log_warning "Potential naming convention issues found:"
        echo "$bad_naming"
        issues=$((issues + 1))
    fi
    
    if [ $issues -eq 0 ]; then
        log_success "Code style validation passed"
        return 0
    else
        log_warning "Code style validation found $issues issue(s)"
        return 1
    fi
}

check_documentation() {
    log_info "Checking documentation completeness..."
    
    local issues=0
    
    # Check for README files
    log_info "Checking for README files..."
    local required_readmes=("README.md" "docs/README.md" "examples/README.md")
    for readme in "${required_readmes[@]}"; do
        if [ ! -f "$PROJECT_ROOT/$readme" ]; then
            log_warning "Missing README: $readme"
            issues=$((issues + 1))
        fi
    done
    
    # Check for API documentation
    log_info "Checking API documentation..."
    local header_files=$(find "$PROJECT_ROOT/include" -name "*.h" | wc -l)
    local documented_headers=$(find "$PROJECT_ROOT/include" -name "*.h" | xargs grep -l '/\*\*' 2>/dev/null | wc -l)
    
    if [ $documented_headers -lt $((header_files / 2)) ]; then
        log_warning "Low API documentation coverage: $documented_headers/$header_files headers documented"
        issues=$((issues + 1))
    fi
    
    # Check for changelog
    if [ ! -f "$PROJECT_ROOT/CHANGELOG.md" ]; then
        log_warning "Missing CHANGELOG.md"
        issues=$((issues + 1))
    fi
    
    # Check for license
    if [ ! -f "$PROJECT_ROOT/LICENSE" ]; then
        log_error "Missing LICENSE file"
        issues=$((issues + 1))
    fi
    
    if [ $issues -eq 0 ]; then
        log_success "Documentation validation passed"
        return 0
    else
        log_warning "Documentation validation found $issues issue(s)"
        return 1
    fi
}

check_configuration() {
    log_info "Checking configuration system..."
    
    local issues=0
    
    # Check Kconfig file
    if [ ! -f "$PROJECT_ROOT/Kconfig" ]; then
        log_error "Missing Kconfig file"
        issues=$((issues + 1))
    else
        log_info "Validating Kconfig syntax..."
        if ! python3 -c "
import sys
sys.path.append('$SCRIPT_DIR')
try:
    import kconfiglib
    kconf = kconfiglib.Kconfig('$PROJECT_ROOT/Kconfig')
    print('Kconfig syntax is valid')
except Exception as e:
    print(f'Kconfig syntax error: {e}')
    sys.exit(1)
" 2>/dev/null; then
            log_warning "Kconfig syntax issues detected"
            issues=$((issues + 1))
        fi
    fi
    
    # Check menuconfig script
    if [ ! -f "$PROJECT_ROOT/scripts/menuconfig.py" ]; then
        log_error "Missing menuconfig script"
        issues=$((issues + 1))
    elif [ ! -x "$PROJECT_ROOT/scripts/menuconfig.py" ]; then
        log_warning "menuconfig script is not executable"
        issues=$((issues + 1))
    fi
    
    # Check for default configuration
    if [ ! -f "$PROJECT_ROOT/defconfig" ] && [ ! -f "$PROJECT_ROOT/.config" ]; then
        log_warning "No default configuration found"
        issues=$((issues + 1))
    fi
    
    if [ $issues -eq 0 ]; then
        log_success "Configuration validation passed"
        return 0
    else
        log_warning "Configuration validation found $issues issue(s)"
        return 1
    fi
}

check_build_system() {
    log_info "Checking build system..."
    
    local issues=0
    
    # Check CMakeLists.txt
    if [ ! -f "$PROJECT_ROOT/CMakeLists.txt" ]; then
        log_error "Missing root CMakeLists.txt"
        issues=$((issues + 1))
    else
        log_info "Validating CMake configuration..."
        if ! cmake -S "$PROJECT_ROOT" -B "$PROJECT_ROOT/build_test" -DCMAKE_BUILD_TYPE=Release >/dev/null 2>&1; then
            log_warning "CMake configuration issues detected"
            issues=$((issues + 1))
        else
            rm -rf "$PROJECT_ROOT/build_test"
        fi
    fi
    
    # Check build scripts
    local build_scripts=("scripts/build.sh" "scripts/test.sh")
    for script in "${build_scripts[@]}"; do
        if [ ! -f "$PROJECT_ROOT/$script" ]; then
            log_warning "Missing build script: $script"
            issues=$((issues + 1))
        elif [ ! -x "$PROJECT_ROOT/$script" ]; then
            log_warning "Build script not executable: $script"
            issues=$((issues + 1))
        fi
    done
    
    # Check for Makefile wrapper
    if [ ! -f "$PROJECT_ROOT/Makefile" ]; then
        log_warning "Missing Makefile wrapper"
        issues=$((issues + 1))
    fi
    
    if [ $issues -eq 0 ]; then
        log_success "Build system validation passed"
        return 0
    else
        log_warning "Build system validation found $issues issue(s)"
        return 1
    fi
}

check_examples() {
    log_info "Checking example applications..."
    
    local issues=0
    
    # Check examples directory
    if [ ! -d "$PROJECT_ROOT/examples" ]; then
        log_error "Missing examples directory"
        issues=$((issues + 1))
        return 1
    fi
    
    # Count examples
    local example_count=$(find "$PROJECT_ROOT/examples" -name "CMakeLists.txt" | wc -l)
    if [ $example_count -lt 5 ]; then
        log_warning "Few examples found: $example_count (expected at least 5)"
        issues=$((issues + 1))
    fi
    
    # Check for essential examples
    local essential_examples=("basic_task" "mutex_example" "queue_example")
    for example in "${essential_examples[@]}"; do
        if [ ! -d "$PROJECT_ROOT/examples/$example" ]; then
            log_warning "Missing essential example: $example"
            issues=$((issues + 1))
        fi
    done
    
    # Check v0.3.0 specific examples
    local v030_examples=("event_group_example" "stream_buffer_example" "memory_pool_example")
    for example in "${v030_examples[@]}"; do
        if [ ! -d "$PROJECT_ROOT/examples/$example" ]; then
            log_warning "Missing v0.3.0 example: $example"
            issues=$((issues + 1))
        fi
    done
    
    if [ $issues -eq 0 ]; then
        log_success "Examples validation passed"
        return 0
    else
        log_warning "Examples validation found $issues issue(s)"
        return 1
    fi
}

check_tests() {
    log_info "Checking test suite..."
    
    local issues=0
    
    # Check tests directory
    if [ ! -d "$PROJECT_ROOT/tests" ]; then
        log_error "Missing tests directory"
        issues=$((issues + 1))
        return 1
    fi
    
    # Check test categories
    local test_categories=("unit" "integration")
    for category in "${test_categories[@]}"; do
        if [ ! -d "$PROJECT_ROOT/tests/$category" ]; then
            log_warning "Missing test category: $category"
            issues=$((issues + 1))
        fi
    done
    
    # Count test files
    local test_count=$(find "$PROJECT_ROOT/tests" -name "*_test.c" | wc -l)
    if [ $test_count -lt 10 ]; then
        log_warning "Few test files found: $test_count (expected at least 10)"
        issues=$((issues + 1))
    fi
    
    # Check for v0.3.0 specific tests
    local v030_tests=("event_group_test" "stream_buffer_test" "memory_pool_test" "mpu_test")
    for test in "${v030_tests[@]}"; do
        if ! find "$PROJECT_ROOT/tests" -name "*${test}*" | grep -q .; then
            log_warning "Missing v0.3.0 test: $test"
            issues=$((issues + 1))
        fi
    done
    
    if [ $issues -eq 0 ]; then
        log_success "Tests validation passed"
        return 0
    else
        log_warning "Tests validation found $issues issue(s)"
        return 1
    fi
}

generate_validation_report() {
    local report_file="$PROJECT_ROOT/validation_report.txt"
    
    log_info "Generating validation report..."
    
    cat > "$report_file" << EOF
Pico-RTOS v0.3.0 Validation Report
Generated: $(date)
==================================

Project Structure:
- Source files: $(find "$PROJECT_ROOT/src" -name "*.c" | wc -l) C files
- Header files: $(find "$PROJECT_ROOT/include" -name "*.h" | wc -l) header files
- Example applications: $(find "$PROJECT_ROOT/examples" -name "CMakeLists.txt" | wc -l) examples
- Test files: $(find "$PROJECT_ROOT/tests" -name "*_test.c" | wc -l) test files

Configuration:
- Kconfig file: $([ -f "$PROJECT_ROOT/Kconfig" ] && echo "Present" || echo "Missing")
- Default config: $([ -f "$PROJECT_ROOT/defconfig" ] && echo "Present" || echo "Missing")
- Build scripts: $(find "$PROJECT_ROOT/scripts" -name "*.sh" | wc -l) scripts

Documentation:
- README files: $(find "$PROJECT_ROOT" -name "README.md" | wc -l) files
- License: $([ -f "$PROJECT_ROOT/LICENSE" ] && echo "Present" || echo "Missing")
- Changelog: $([ -f "$PROJECT_ROOT/CHANGELOG.md" ] && echo "Present" || echo "Missing")

Build System:
- CMake files: $(find "$PROJECT_ROOT" -name "CMakeLists.txt" | wc -l) files
- Makefile: $([ -f "$PROJECT_ROOT/Makefile" ] && echo "Present" || echo "Missing")

Version Information:
- Version: v0.3.0
- Git commit: $(git rev-parse --short HEAD 2>/dev/null || echo "Unknown")
- Git status: $(git status --porcelain 2>/dev/null | wc -l) modified files

EOF
    
    log_success "Validation report generated: $report_file"
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            usage
            exit 0
            ;;
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        --code-style-only)
            CHECK_CODE_STYLE=true
            CHECK_DOCUMENTATION=false
            CHECK_CONFIGURATION=false
            CHECK_BUILD_SYSTEM=false
            CHECK_EXAMPLES=false
            CHECK_TESTS=false
            shift
            ;;
        --docs-only)
            CHECK_CODE_STYLE=false
            CHECK_DOCUMENTATION=true
            CHECK_CONFIGURATION=false
            CHECK_BUILD_SYSTEM=false
            CHECK_EXAMPLES=false
            CHECK_TESTS=false
            shift
            ;;
        --config-only)
            CHECK_CODE_STYLE=false
            CHECK_DOCUMENTATION=false
            CHECK_CONFIGURATION=true
            CHECK_BUILD_SYSTEM=false
            CHECK_EXAMPLES=false
            CHECK_TESTS=false
            shift
            ;;
        --build-only)
            CHECK_CODE_STYLE=false
            CHECK_DOCUMENTATION=false
            CHECK_CONFIGURATION=false
            CHECK_BUILD_SYSTEM=true
            CHECK_EXAMPLES=false
            CHECK_TESTS=false
            shift
            ;;
        --examples-only)
            CHECK_CODE_STYLE=false
            CHECK_DOCUMENTATION=false
            CHECK_CONFIGURATION=false
            CHECK_BUILD_SYSTEM=false
            CHECK_EXAMPLES=true
            CHECK_TESTS=false
            shift
            ;;
        --tests-only)
            CHECK_CODE_STYLE=false
            CHECK_DOCUMENTATION=false
            CHECK_CONFIGURATION=false
            CHECK_BUILD_SYSTEM=false
            CHECK_EXAMPLES=false
            CHECK_TESTS=true
            shift
            ;;
        --quick)
            CHECK_CODE_STYLE=false
            CHECK_DOCUMENTATION=true
            CHECK_CONFIGURATION=true
            CHECK_BUILD_SYSTEM=true
            CHECK_EXAMPLES=false
            CHECK_TESTS=false
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
    echo "Pico-RTOS v0.3.0 Validation System"
    echo "==================================="
    
    local overall_result=0
    local checks_run=0
    local checks_passed=0
    
    # Run selected validations
    if [ "$CHECK_CODE_STYLE" = true ]; then
        checks_run=$((checks_run + 1))
        if check_code_style; then
            checks_passed=$((checks_passed + 1))
        else
            overall_result=1
        fi
    fi
    
    if [ "$CHECK_DOCUMENTATION" = true ]; then
        checks_run=$((checks_run + 1))
        if check_documentation; then
            checks_passed=$((checks_passed + 1))
        else
            overall_result=1
        fi
    fi
    
    if [ "$CHECK_CONFIGURATION" = true ]; then
        checks_run=$((checks_run + 1))
        if check_configuration; then
            checks_passed=$((checks_passed + 1))
        else
            overall_result=1
        fi
    fi
    
    if [ "$CHECK_BUILD_SYSTEM" = true ]; then
        checks_run=$((checks_run + 1))
        if check_build_system; then
            checks_passed=$((checks_passed + 1))
        else
            overall_result=1
        fi
    fi
    
    if [ "$CHECK_EXAMPLES" = true ]; then
        checks_run=$((checks_run + 1))
        if check_examples; then
            checks_passed=$((checks_passed + 1))
        else
            overall_result=1
        fi
    fi
    
    if [ "$CHECK_TESTS" = true ]; then
        checks_run=$((checks_run + 1))
        if check_tests; then
            checks_passed=$((checks_passed + 1))
        else
            overall_result=1
        fi
    fi
    
    # Generate report
    generate_validation_report
    
    # Final summary
    echo
    log_info "Validation Summary:"
    echo "  Checks run: $checks_run"
    echo "  Checks passed: $checks_passed"
    echo "  Checks failed: $((checks_run - checks_passed))"
    
    if [ $overall_result -eq 0 ]; then
        log_success "All validations passed!"
    else
        log_warning "Some validations failed!"
    fi
    
    exit $overall_result
}

# Run main function
main "$@"