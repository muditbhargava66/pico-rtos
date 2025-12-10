# Pico-RTOS v0.3.1 Makefile
# Provides convenient targets for building and configuring Pico-RTOS

# Default build directory
BUILD_DIR ?= build

# Configuration files
CONFIG_FILE ?= config/.config
CMAKE_CONFIG_FILE ?= config/cmake_config.cmake
HEADER_CONFIG_FILE ?= include/pico_rtos_config.h

# Python interpreter
PYTHON ?= python3

# Check if we're in the project root
ifeq ($(wildcard CMakeLists.txt),)
$(error This Makefile must be run from the project root directory)
endif

# Default target
.PHONY: all
all: configure build

# Help target
.PHONY: help
help:
	@echo "Pico-RTOS v0.3.1 Build System"
	@echo ""
	@echo "Configuration targets:"
	@echo "  menuconfig     - Interactive configuration menu (ncurses)"
	@echo "  guiconfig      - Interactive configuration GUI (tkinter)"
	@echo "  defconfig      - Load default configuration"
	@echo "  savedefconfig  - Save current config as default"
	@echo "  showconfig     - Display current configuration"
	@echo ""
	@echo "Build targets:"
	@echo "  configure      - Generate build configuration from .config"
	@echo "  build          - Build the project"
	@echo "  build-gcc      - Build with GCC toolchain"
	@echo "  build-clang    - Build with Clang toolchain"
	@echo "  build-debug    - Build in Debug mode"
	@echo "  build-release  - Build in Release mode"
	@echo "  clean          - Clean build directory"
	@echo "  rebuild        - Clean and rebuild"
	@echo "  examples       - Build examples only"
	@echo "  tests          - Build tests only"
	@echo ""
	@echo "Flash targets:"
	@echo "  flash-<target> - Flash specific target to Pico (e.g., flash-led_blinking)"
	@echo "  list-targets   - List available flash targets"
	@echo ""
	@echo "Utility targets:"
	@echo "  install-deps   - Install Python dependencies"
	@echo "  setup-sdk      - Set up Pico SDK"
	@echo "  format         - Format source code"
	@echo ""
	@echo "Variables:"
	@echo "  BUILD_DIR      - Build directory (default: build)"
	@echo "  CONFIG_FILE    - Configuration file (default: .config)"
	@echo "  PYTHON         - Python interpreter (default: python3)"

# Configuration targets
.PHONY: menuconfig
menuconfig: install-deps-check
	@echo "Starting menuconfig..."
	$(PYTHON) scripts/menuconfig.py --config-file $(CONFIG_FILE) \
		--cmake-file $(CMAKE_CONFIG_FILE) --header-file $(HEADER_CONFIG_FILE)

.PHONY: guiconfig
guiconfig: install-deps-check
	@echo "Starting GUI configuration..."
	$(PYTHON) scripts/menuconfig.py --config-file $(CONFIG_FILE) \
		--cmake-file $(CMAKE_CONFIG_FILE) --header-file $(HEADER_CONFIG_FILE) --gui

.PHONY: defconfig
defconfig: install-deps-check
	@echo "Loading default configuration..."
	$(PYTHON) scripts/menuconfig.py --config-file $(CONFIG_FILE) \
		--cmake-file $(CMAKE_CONFIG_FILE) --header-file $(HEADER_CONFIG_FILE) \
		--defconfig

.PHONY: savedefconfig
savedefconfig:
	@if [ -f $(CONFIG_FILE) ]; then \
		cp $(CONFIG_FILE) defconfig; \
		echo "Saved current configuration as defconfig"; \
	else \
		echo "No configuration file found. Run 'make menuconfig' first."; \
		exit 1; \
	fi

.PHONY: showconfig
showconfig: install-deps-check
	@$(PYTHON) scripts/menuconfig.py --config-file $(CONFIG_FILE) --show

# Toolchain options
TOOLCHAIN ?=
BUILD_TYPE ?= Release

# Build targets
.PHONY: configure
configure: $(CMAKE_CONFIG_FILE)

$(CMAKE_CONFIG_FILE): $(CONFIG_FILE) scripts/menuconfig.py
	@echo "Generating build configuration..."
	$(PYTHON) scripts/menuconfig.py --config-file $(CONFIG_FILE) \
		--cmake-file $(CMAKE_CONFIG_FILE) --header-file $(HEADER_CONFIG_FILE) \
		--generate

$(CONFIG_FILE):
	@echo "No configuration found. Creating default configuration..."
	@$(MAKE) defconfig

.PHONY: build
build: configure
	@echo "Building Pico-RTOS v0.3.1..."
	@if [ -x scripts/build.sh ]; then \
		scripts/build.sh $(if $(filter Debug,$(BUILD_TYPE)),--debug) \
			$(if $(TOOLCHAIN),--toolchain $(TOOLCHAIN)) \
			--examples --tests; \
	else \
		mkdir -p $(BUILD_DIR); \
		cd $(BUILD_DIR) && cmake -C ../$(CMAKE_CONFIG_FILE) \
			-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) .. && \
		make -j$(shell nproc 2>/dev/null || echo 4); \
	fi

.PHONY: build-gcc
build-gcc:
	@$(MAKE) build TOOLCHAIN=arm-none-eabi-gcc

.PHONY: build-clang
build-clang:
	@$(MAKE) build TOOLCHAIN=arm-none-eabi-clang

.PHONY: build-debug
build-debug:
	@$(MAKE) build BUILD_TYPE=Debug

.PHONY: build-release
build-release:
	@$(MAKE) build BUILD_TYPE=Release

.PHONY: examples
examples: configure
	@echo "Building examples only..."
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake -C ../$(CMAKE_CONFIG_FILE) -DPICO_RTOS_BUILD_TESTS=OFF .. && make -j$(shell nproc 2>/dev/null || echo 4)

.PHONY: tests
tests: configure
	@echo "Building tests only..."
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake -C ../$(CMAKE_CONFIG_FILE) -DPICO_RTOS_BUILD_EXAMPLES=OFF .. && make -j$(shell nproc 2>/dev/null || echo 4)

.PHONY: clean
clean:
	@echo "Cleaning build directory..."
	@rm -rf $(BUILD_DIR)

.PHONY: rebuild
rebuild: clean build

# Flash targets
.PHONY: list-targets
list-targets: build
	@echo "Available flash targets:"
	@find $(BUILD_DIR) -name "*.uf2" -exec basename {} .uf2 \; | sort | sed 's/^/  flash-/'

.PHONY: flash-%
flash-%: build
	@TARGET_NAME=$*; \
	UF2_FILE="$(BUILD_DIR)/$$TARGET_NAME.uf2"; \
	if [ ! -f "$$UF2_FILE" ]; then \
		echo "Error: $$UF2_FILE not found"; \
		echo "Available targets:"; \
		$(MAKE) list-targets; \
		exit 1; \
	fi; \
	echo "Flashing $$TARGET_NAME to Pico..."; \
	if [ -d "/Volumes/RPI-RP2" ]; then \
		cp "$$UF2_FILE" "/Volumes/RPI-RP2/"; \
		echo "Flashed $$TARGET_NAME successfully (macOS)"; \
	elif [ -d "/media/$$USER/RPI-RP2" ]; then \
		cp "$$UF2_FILE" "/media/$$USER/RPI-RP2/"; \
		echo "Flashed $$TARGET_NAME successfully (Linux)"; \
	elif command -v picotool >/dev/null 2>&1; then \
		picotool load "$$UF2_FILE" -f; \
		echo "Flashed $$TARGET_NAME successfully (picotool)"; \
	else \
		echo "Error: Could not find Pico in bootloader mode"; \
		echo "Please:"; \
		echo "1. Hold BOOTSEL button while connecting Pico to USB"; \
		echo "2. Or install picotool for automatic flashing"; \
		exit 1; \
	fi

# Utility targets
.PHONY: install-deps
install-deps:
	@echo "Installing Python dependencies..."
	$(PYTHON) -m pip install kconfiglib

.PHONY: install-deps-check
install-deps-check:
	@$(PYTHON) -c "import kconfiglib" 2>/dev/null || { \
		echo "kconfiglib not found. Installing..."; \
		$(MAKE) install-deps; \
	}

.PHONY: setup-sdk
setup-sdk:
	@echo "Setting up Pico SDK..."
	@if [ ! -d "extern/pico-sdk" ]; then \
		echo "Cloning Pico SDK..."; \
		git submodule add -b master https://github.com/raspberrypi/pico-sdk.git extern/pico-sdk || true; \
		git submodule update --init --recursive; \
	else \
		echo "Pico SDK already exists. Updating..."; \
		git submodule update --init --recursive; \
	fi

.PHONY: format
format:
	@echo "Formatting source code..."
	@if command -v clang-format >/dev/null 2>&1; then \
		find src include examples tests -name "*.c" -o -name "*.h" | xargs clang-format -i; \
		echo "Code formatted successfully"; \
	else \
		echo "clang-format not found. Please install it to format code."; \
	fi

# Development targets
.PHONY: dev-setup
dev-setup: install-deps setup-sdk
	@echo "Development environment setup complete!"
	@echo "Run 'make menuconfig' to configure your build"

.PHONY: quick-start
quick-start: dev-setup defconfig build
	@echo ""
	@echo "Quick start complete!"
	@echo "Your Pico-RTOS is built and ready to use."
	@echo ""
	@echo "Next steps:"
	@echo "1. Connect your Pico in bootloader mode (hold BOOTSEL while connecting)"
	@echo "2. Flash an example: make flash-led_blinking"
	@echo "3. Connect to serial console to see output"
	@echo ""
	@echo "For more options, run: make help"

# Test targets
.PHONY: test-build-system
test-build-system:
	@echo "Running build system tests..."
	$(PYTHON) tests/build_system_test.py

.PHONY: build-info
build-info:
	@echo "Getting build system information..."
	$(PYTHON) scripts/build.py info

# Debug targets
.PHONY: debug-config
debug-config: configure
	@echo "=== Build Configuration Debug ==="
	@echo "CONFIG_FILE: $(CONFIG_FILE)"
	@echo "CMAKE_CONFIG_FILE: $(CMAKE_CONFIG_FILE)"
	@echo "HEADER_CONFIG_FILE: $(HEADER_CONFIG_FILE)"
	@echo "BUILD_DIR: $(BUILD_DIR)"
	@echo "TOOLCHAIN: $(TOOLCHAIN)"
	@echo "BUILD_TYPE: $(BUILD_TYPE)"
	@echo ""
	@$(MAKE) build-info
	@echo ""
	@if [ -f $(CONFIG_FILE) ]; then \
		echo "Configuration file exists:"; \
		ls -la $(CONFIG_FILE); \
	else \
		echo "Configuration file does not exist"; \
	fi
	@echo ""
	@if [ -f $(CMAKE_CONFIG_FILE) ]; then \
		echo "CMake config file exists:"; \
		ls -la $(CMAKE_CONFIG_FILE); \
		echo ""; \
		echo "CMake configuration:"; \
		cat $(CMAKE_CONFIG_FILE); \
	else \
		echo "CMake config file does not exist"; \
	fi

# Prevent make from deleting intermediate files
.SECONDARY:

# Declare phony targets
.PHONY: all help menuconfig guiconfig defconfig savedefconfig showconfig
.PHONY: configure build examples tests clean rebuild
.PHONY: list-targets install-deps install-deps-check setup-sdk format
.PHONY: dev-setup quick-start debug-config