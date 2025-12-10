#!/bin/bash
# Pico-RTOS v0.3.1 Build Script
# Unified build script for all configurations

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"

# Default values
BUILD_TYPE="Release"
CLEAN_BUILD=false
VERBOSE=false
JOBS=$(nproc 2>/dev/null || echo 4)
CONFIG_PROFILE=""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

usage() {
    cat << EOF
Pico-RTOS v0.3.1 Build Script

Usage: $0 [OPTIONS]

Options:
    -h, --help              Show this help message
    -c, --clean             Clean build directory before building
    -d, --debug             Build in Debug mode (default: Release)
    -v, --verbose           Enable verbose build output
    -j, --jobs N            Number of parallel jobs (default: $JOBS)
    -p, --profile PROFILE   Use configuration profile (minimal, default, full)
    --examples              Build example applications
    --tests                 Build test suite

Configuration Profiles:
    minimal                 Minimal RTOS features only
    default                 Standard feature set
    full                    All v0.3.1 features enabled

Examples:
    $0                      # Build with current configuration
    $0 --clean --debug      # Clean debug build
    $0 --profile full       # Build with all features
    $0 --examples --tests   # Build with examples and tests
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

check_dependencies() {
    log_info "Checking build dependencies..."
    
    # Check for required tools
    local missing_tools=()
    
    if ! command -v cmake >/dev/null 2>&1; then
        missing_tools+=("cmake")
    fi
    
    if ! command -v arm-none-eabi-gcc >/dev/null 2>&1; then
        missing_tools+=("arm-none-eabi-gcc")
    fi
    
    if [ ${#missing_tools[@]} -ne 0 ]; then
        log_error "Missing required tools: ${missing_tools[*]}"
        log_error "Please install the missing tools and try again"
        return 1
    fi
    
    log_success "All dependencies found"
    return 0
}

apply_config_profile() {
    local profile="$1"
    local config_file="$PROJECT_ROOT/.config"
    
    log_info "Applying configuration profile: $profile"
    
    case "$profile" in
        minimal)
            cat > "$config_file" << EOF
# Pico-RTOS v0.3.1 Minimal Configuration
BUILD_EXAMPLES=y
BUILD_TESTS=n
ENABLE_DEBUG=n
ENABLE_INSTALL=n
TICK_RATE_1000=y
MAX_TASKS=8
MAX_TIMERS=4
DEFAULT_TASK_STACK_SIZE=512
IDLE_TASK_STACK_SIZE=256
# Disable v0.3.1 features for minimal build
# ENABLE_EVENT_GROUPS is not set
# ENABLE_STREAM_BUFFERS is not set
# ENABLE_MEMORY_POOLS is not set
# ENABLE_MULTI_CORE is not set
# ENABLE_TASK_INSPECTION is not set
# ENABLE_EXECUTION_PROFILING is not set
# ENABLE_SYSTEM_TRACING is not set
# Basic features only
ENABLE_STACK_CHECKING=y
ENABLE_BACKWARD_COMPATIBILITY=y
EOF
            ;;
        default)
            cat > "$config_file" << EOF
# Pico-RTOS v0.3.1 Default Configuration
BUILD_EXAMPLES=y
BUILD_TESTS=y
ENABLE_DEBUG=y
ENABLE_INSTALL=n
TICK_RATE_1000=y
MAX_TASKS=16
MAX_TIMERS=8
DEFAULT_TASK_STACK_SIZE=1024
IDLE_TASK_STACK_SIZE=256
# Enable standard v0.3.1 features
ENABLE_EVENT_GROUPS=y
MAX_EVENT_GROUPS=8
ENABLE_STREAM_BUFFERS=y
MAX_STREAM_BUFFERS=4
ENABLE_MEMORY_POOLS=y
MAX_MEMORY_POOLS=4
# Debugging features
ENABLE_TASK_INSPECTION=y
ENABLE_EXECUTION_PROFILING=y
ENABLE_SYSTEM_TRACING=y
TRACE_BUFFER_SIZE=128
# Quality assurance
ENABLE_SYSTEM_HEALTH_MONITORING=y
ENABLE_WATCHDOG_INTEGRATION=y
# Standard features
ENABLE_STACK_CHECKING=y
ENABLE_MEMORY_TRACKING=y
ENABLE_RUNTIME_STATS=y
ENABLE_BACKWARD_COMPATIBILITY=y
ENABLE_MIGRATION_WARNINGS=y
EOF
            ;;
        full)
            cat > "$config_file" << EOF
# Pico-RTOS v0.3.1 Full Configuration
BUILD_EXAMPLES=y
BUILD_TESTS=y
ENABLE_DEBUG=y
ENABLE_INSTALL=n
TICK_RATE_1000=y
MAX_TASKS=32
MAX_TIMERS=16
DEFAULT_TASK_STACK_SIZE=1024
IDLE_TASK_STACK_SIZE=256
# Enable all v0.3.1 features
ENABLE_EVENT_GROUPS=y
MAX_EVENT_GROUPS=16
ENABLE_STREAM_BUFFERS=y
MAX_STREAM_BUFFERS=8
STREAM_BUFFER_ZERO_COPY_THRESHOLD=128
ENABLE_MEMORY_POOLS=y
MAX_MEMORY_POOLS=8
MEMORY_POOL_STATISTICS=y
ENABLE_MPU_SUPPORT=y
MPU_STACK_GUARD_SIZE=64
# Multi-core support
ENABLE_MULTI_CORE=y
ENABLE_LOAD_BALANCING=y
LOAD_BALANCE_THRESHOLD=75
ENABLE_CORE_AFFINITY=y
ENABLE_IPC_CHANNELS=y
IPC_CHANNEL_BUFFER_SIZE=16
# Debugging and profiling
ENABLE_TASK_INSPECTION=y
ENABLE_EXECUTION_PROFILING=y
PROFILING_BUFFER_SIZE=64
ENABLE_SYSTEM_TRACING=y
TRACE_BUFFER_SIZE=256
ENABLE_ENHANCED_ASSERTIONS=y
# System extensions
ENABLE_IO_ABSTRACTION=y
ENABLE_HIRES_TIMERS=y
HIRES_TIMER_RESOLUTION_US=10
ENABLE_UNIVERSAL_TIMEOUTS=y
# Quality assurance
ENABLE_DEADLOCK_DETECTION=y
ENABLE_SYSTEM_HEALTH_MONITORING=y
HEALTH_CHECK_INTERVAL_MS=1000
ENABLE_WATCHDOG_INTEGRATION=y
WATCHDOG_TIMEOUT_MS=5000
ENABLE_ALERT_SYSTEM=y
# All features enabled
ENABLE_STACK_CHECKING=y
ENABLE_MEMORY_TRACKING=y
ENABLE_RUNTIME_STATS=y
ERROR_HISTORY_SIZE=20
ENABLE_BACKWARD_COMPATIBILITY=y
ENABLE_MIGRATION_WARNINGS=y
# Debug logging
ENABLE_DEBUG_LOGGING=y
LOG_LEVEL_INFO=y
LOG_BUFFER_SIZE=1024
ENABLE_LOG_TIMESTAMPS=y
EOF
            ;;
        *)
            log_error "Unknown configuration profile: $profile"
            log_error "Available profiles: minimal, default, full"
            return 1
            ;;
    esac
    
    # Generate configuration files
    python3 "$SCRIPT_DIR/menuconfig.py" --generate
    log_success "Configuration profile '$profile' applied"
}

build_project() {
    log_info "Starting Pico-RTOS v0.3.1 build..."
    
    # Create build directory
    if [ "$CLEAN_BUILD" = true ] && [ -d "$BUILD_DIR" ]; then
        log_info "Cleaning build directory..."
        rm -rf "$BUILD_DIR"
    fi
    
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    
    # Configure CMake
    local cmake_args=(
        "-DCMAKE_BUILD_TYPE=$BUILD_TYPE"
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
    )
    
    if [ "$BUILD_EXAMPLES" = true ]; then
        cmake_args+=("-DPICO_RTOS_BUILD_EXAMPLES=ON")
    fi
    
    if [ "$BUILD_TESTS" = true ]; then
        cmake_args+=("-DPICO_RTOS_BUILD_TESTS=ON")
    fi
    
    log_info "Configuring build with CMake..."
    cmake "${cmake_args[@]}" "$PROJECT_ROOT"
    
    # Build
    local make_args=("-j$JOBS")
    if [ "$VERBOSE" = true ]; then
        make_args+=("VERBOSE=1")
    fi
    
    log_info "Building with $JOBS parallel jobs..."
    make "${make_args[@]}"
    
    log_success "Build completed successfully!"
    
    # Show build results
    log_info "Build artifacts:"
    if [ -f "libpico_rtos.a" ]; then
        local size=$(stat -f%z "libpico_rtos.a" 2>/dev/null || stat -c%s "libpico_rtos.a" 2>/dev/null || echo "unknown")
        echo "  - libpico_rtos.a ($size bytes)"
    fi
    
    if [ "$BUILD_EXAMPLES" = true ]; then
        echo "  - Example applications in examples/"
    fi
    
    if [ "$BUILD_TESTS" = true ]; then
        echo "  - Test suite in tests/"
    fi
}

# Parse command line arguments
BUILD_EXAMPLES=false
BUILD_TESTS=false

while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            usage
            exit 0
            ;;
        -c|--clean)
            CLEAN_BUILD=true
            shift
            ;;
        -d|--debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        -j|--jobs)
            JOBS="$2"
            shift 2
            ;;
        -p|--profile)
            CONFIG_PROFILE="$2"
            shift 2
            ;;
        --examples)
            BUILD_EXAMPLES=true
            shift
            ;;
        --tests)
            BUILD_TESTS=true
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
    echo "Pico-RTOS v0.3.1 Build System"
    echo "=============================="
    
    # Check dependencies
    if ! check_dependencies; then
        exit 1
    fi
    
    # Apply configuration profile if specified
    if [ -n "$CONFIG_PROFILE" ]; then
        if ! apply_config_profile "$CONFIG_PROFILE"; then
            exit 1
        fi
    fi
    
    # Build the project
    if ! build_project; then
        log_error "Build failed!"
        exit 1
    fi
    
    log_success "Pico-RTOS v0.3.1 build completed successfully!"
}

# Run main function
main "$@"