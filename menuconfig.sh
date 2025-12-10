#!/bin/bash
# Pico-RTOS v0.3.1 Menuconfig Wrapper Script
# This script provides easy access to the menuconfig system

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if we're in the project root
if [ ! -f "CMakeLists.txt" ]; then
    print_error "This script must be run from the Pico-RTOS project root directory"
    exit 1
fi

# Check Python installation
if ! command -v python3 &> /dev/null; then
    print_error "Python 3 is required but not installed"
    exit 1
fi

# Check for kconfiglib
if ! python3 -c "import kconfiglib" 2>/dev/null; then
    print_warning "kconfiglib not found. Installing..."
    python3 -m pip install kconfiglib
    if [ $? -eq 0 ]; then
        print_success "kconfiglib installed successfully"
    else
        print_error "Failed to install kconfiglib"
        exit 1
    fi
fi

# Parse command line arguments
GUI_MODE=false
SHOW_CONFIG=false
LOAD_DEFAULTS=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --gui|-g)
            GUI_MODE=true
            shift
            ;;
        --show|-s)
            SHOW_CONFIG=true
            shift
            ;;
        --defaults|-d)
            LOAD_DEFAULTS=true
            shift
            ;;
        --help|-h)
            echo "Pico-RTOS Menuconfig Wrapper"
            echo ""
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  --gui, -g        Use GUI interface (requires tkinter)"
            echo "  --show, -s       Show current configuration and exit"
            echo "  --defaults, -d   Load default configuration"
            echo "  --help, -h       Show this help message"
            echo ""
            echo "Examples:"
            echo "  $0               # Run text-based menuconfig"
            echo "  $0 --gui         # Run GUI-based configuration"
            echo "  $0 --show        # Show current configuration"
            echo "  $0 --defaults    # Load and show default configuration"
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Build command arguments
ARGS=""
if [ "$GUI_MODE" = true ]; then
    ARGS="$ARGS --gui"
fi
if [ "$SHOW_CONFIG" = true ]; then
    ARGS="$ARGS --show-config"
fi
if [ "$LOAD_DEFAULTS" = true ]; then
    ARGS="$ARGS --load-defaults"
fi

# Run menuconfig
print_info "Starting Pico-RTOS configuration..."
python3 scripts/menuconfig.py $ARGS

if [ $? -eq 0 ]; then
    if [ "$SHOW_CONFIG" = false ]; then
        print_success "Configuration completed successfully"
        echo ""
        print_info "Next steps:"
        echo "  1. Build the project: make build"
        echo "  2. Or use CMake directly: cmake -C cmake_config.cmake ."
        echo "  3. Flash to Pico: make flash-<target>"
    fi
else
    print_error "Configuration failed"
    exit 1
fi