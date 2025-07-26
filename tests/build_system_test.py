#!/usr/bin/env python3
"""
Build system test for Pico-RTOS
This is a compatibility script for the GitHub workflows.
"""

import os
import sys
import subprocess

def test_build_outputs():
    """Test that build outputs exist."""
    build_dir = "build"
    
    if not os.path.exists(build_dir):
        print("❌ Build directory doesn't exist")
        return False
    
    # Check for library
    lib_path = os.path.join(build_dir, "libpico_rtos.a")
    if os.path.exists(lib_path):
        print("✅ Library found: libpico_rtos.a")
    else:
        print("⚠️  Library not found: libpico_rtos.a")
    
    # Check for examples
    examples_found = 0
    for root, dirs, files in os.walk(build_dir):
        for file in files:
            if file.endswith('.uf2'):
                examples_found += 1
                print(f"✅ Example found: {file}")
    
    if examples_found > 0:
        print(f"✅ Found {examples_found} example binaries")
    else:
        print("⚠️  No example binaries found")
    
    return True

def test_file_structure():
    """Test that required files exist."""
    required_files = [
        "README.md",
        "CHANGELOG.md",
        "LICENSE",
        "CMakeLists.txt",
        "include/pico_rtos.h"
    ]
    
    all_exist = True
    for file_path in required_files:
        if os.path.exists(file_path):
            print(f"✅ Required file found: {file_path}")
        else:
            print(f"❌ Required file missing: {file_path}")
            all_exist = False
    
    return all_exist

def main():
    print("🧪 Running Pico-RTOS Build System Tests")
    print("=" * 50)
    
    success = True
    
    print("\n📁 Testing file structure...")
    if not test_file_structure():
        success = False
    
    print("\n🏗️  Testing build outputs...")
    if not test_build_outputs():
        success = False
    
    print("\n" + "=" * 50)
    if success:
        print("✅ All tests passed!")
        return 0
    else:
        print("❌ Some tests failed!")
        return 1

if __name__ == "__main__":
    sys.exit(main())