#!/bin/bash
# This script sets up the Pico SDK as a git submodule and initializes it

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${YELLOW}Setting up Raspberry Pi Pico SDK...${NC}"

# Check if git is installed
if ! command -v git &> /dev/null; then
    echo -e "${RED}Error: git is not installed. Please install git and try again.${NC}"
    exit 1
fi

# Create extern directory if it doesn't exist
mkdir -p extern

# Check if the pico-sdk directory already exists
if [ -d "extern/pico-sdk" ]; then
    echo -e "${YELLOW}Pico SDK directory already exists.${NC}"
    
    # Check if it's a git repository
    if [ -d "extern/pico-sdk/.git" ]; then
        echo -e "${GREEN}Updating existing Pico SDK repository...${NC}"
        cd extern/pico-sdk
        git pull
        cd ../..
    else
        echo -e "${YELLOW}Removing existing directory and cloning fresh...${NC}"
        rm -rf extern/pico-sdk
        # Force add submodule even if gitignored
        git submodule add -f -b master https://github.com/raspberrypi/pico-sdk.git extern/pico-sdk
    fi
else
    # Force add submodule even if gitignored
    echo -e "${GREEN}Adding Pico SDK as a submodule...${NC}"
    git submodule add -f -b master https://github.com/raspberrypi/pico-sdk.git extern/pico-sdk
fi

# Initialize submodules (including submodules within pico-sdk)
echo -e "${GREEN}Initializing submodules...${NC}"
git submodule update --init --recursive

# Copy pico_sdk_import.cmake to the root directory
echo -e "${GREEN}Copying pico_sdk_import.cmake to project root...${NC}"
cp extern/pico-sdk/external/pico_sdk_import.cmake .

# Also copy to example directories
echo -e "${GREEN}Copying pico_sdk_import.cmake to example directories...${NC}"
cp extern/pico-sdk/external/pico_sdk_import.cmake examples/led_blinking/
cp extern/pico-sdk/external/pico_sdk_import.cmake examples/task_synchronization/

# Create a .gitmodules file if it doesn't exist
if [ ! -f .gitmodules ]; then
    echo -e "${GREEN}Creating .gitmodules file...${NC}"
    echo "[submodule \"extern/pico-sdk\"]
	path = extern/pico-sdk
	url = https://github.com/raspberrypi/pico-sdk.git
	branch = master" > .gitmodules
fi

echo -e "${GREEN}Pico SDK setup complete!${NC}"
echo -e "${YELLOW}You can now build your project.${NC}"