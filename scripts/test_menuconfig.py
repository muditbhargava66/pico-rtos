#!/usr/bin/env python3
"""
Test script for Pico-RTOS menuconfig system
This script tests the basic functionality of the menuconfig system
"""

import os
import sys
import tempfile
import shutil
from pathlib import Path

def test_menuconfig_system():
    """Test the menuconfig system functionality"""
    print("Testing Pico-RTOS Menuconfig System...")
    
    # Check if kconfiglib is available
    try:
        import kconfiglib
        print("✓ kconfiglib is available")
    except ImportError:
        print("✗ kconfiglib not found. Install with: pip install kconfiglib")
        return False
    
    # Check if Kconfig file exists
    kconfig_file = Path("Kconfig")
    if not kconfig_file.exists():
        print("✗ Kconfig file not found")
        return False
    print("✓ Kconfig file found")
    
    # Check if menuconfig script exists
    script_file = Path("scripts/menuconfig.py")
    if not script_file.exists():
        print("✗ menuconfig.py script not found")
        return False
    print("✓ menuconfig.py script found")
    
    # Test loading Kconfig
    try:
        kconf = kconfiglib.Kconfig(str(kconfig_file))
        print("✓ Kconfig loaded successfully")
    except Exception as e:
        print(f"✗ Failed to load Kconfig: {e}")
        return False
    
    # Test configuration options
    required_options = [
        'BUILD_EXAMPLES',
        'BUILD_TESTS', 
        'ENABLE_DEBUG',
        'TICK_RATE_HZ',
        'MAX_TASKS',
        'MAX_TIMERS',
        'ENABLE_STACK_CHECKING',
        'ENABLE_MEMORY_TRACKING',
        'ENABLE_ERROR_HISTORY'
    ]
    
    missing_options = []
    for option in required_options:
        try:
            sym = kconf.syms.get(option)
            if sym is None:
                missing_options.append(option)
        except:
            missing_options.append(option)
    
    if missing_options:
        print(f"✗ Missing configuration options: {missing_options}")
        return False
    print("✓ All required configuration options found")
    
    # Test default values
    try:
        # Set some default values
        build_examples = kconf.syms.get('BUILD_EXAMPLES')
        if build_examples:
            build_examples.set_value('y')
        
        tick_rate = kconf.syms.get('TICK_RATE_1000')
        if tick_rate:
            tick_rate.set_value('y')
            
        max_tasks = kconf.syms.get('MAX_TASKS')
        if max_tasks:
            max_tasks.set_value('16')
        print("✓ Default values set successfully")
    except Exception as e:
        print(f"✗ Failed to set default values: {e}")
        return False
    
    # Test configuration file generation
    try:
        with tempfile.TemporaryDirectory() as temp_dir:
            config_file = Path(temp_dir) / "test.config"
            kconf.write_config(str(config_file))
            
            if config_file.exists():
                print("✓ Configuration file generation works")
            else:
                print("✗ Configuration file not generated")
                return False
    except Exception as e:
        print(f"✗ Failed to generate configuration file: {e}")
        return False
    
    # Test menuconfig script import
    try:
        sys.path.insert(0, 'scripts')
        import menuconfig
        print("✓ menuconfig script can be imported")
    except Exception as e:
        print(f"✗ Failed to import menuconfig script: {e}")
        return False
    finally:
        if 'scripts' in sys.path:
            sys.path.remove('scripts')
    
    print("\n✓ All menuconfig system tests passed!")
    return True

def test_makefile_targets():
    """Test if Makefile targets are available"""
    print("\nTesting Makefile targets...")
    
    makefile = Path("Makefile")
    if not makefile.exists():
        print("✗ Makefile not found")
        return False
    
    # Read Makefile and check for required targets
    makefile_content = makefile.read_text()
    required_targets = [
        'menuconfig',
        'guiconfig', 
        'defconfig',
        'showconfig',
        'configure',
        'build'
    ]
    
    missing_targets = []
    for target in required_targets:
        if f".PHONY: {target}" not in makefile_content:
            missing_targets.append(target)
    
    if missing_targets:
        print(f"✗ Missing Makefile targets: {missing_targets}")
        return False
    
    print("✓ All required Makefile targets found")
    return True

def test_file_structure():
    """Test if all required files are present"""
    print("\nTesting file structure...")
    
    required_files = [
        "Kconfig",
        "scripts/menuconfig.py",
        "Makefile",
        "menuconfig.sh",
        "requirements.txt",
        "config/defconfig"
    ]
    
    missing_files = []
    for file_path in required_files:
        if not Path(file_path).exists():
            missing_files.append(file_path)
    
    if missing_files:
        print(f"✗ Missing files: {missing_files}")
        return False
    
    print("✓ All required files present")
    return True

def main():
    """Main test function"""
    print("=" * 50)
    print("Pico-RTOS Menuconfig System Test")
    print("=" * 50)
    
    # Change to project root if needed
    if not Path("CMakeLists.txt").exists():
        print("Please run this test from the project root directory")
        return 1
    
    tests = [
        test_file_structure,
        test_makefile_targets,
        test_menuconfig_system
    ]
    
    passed = 0
    total = len(tests)
    
    for test in tests:
        try:
            if test():
                passed += 1
            else:
                print(f"Test {test.__name__} failed")
        except Exception as e:
            print(f"Test {test.__name__} failed with exception: {e}")
    
    print("\n" + "=" * 50)
    print(f"Test Results: {passed}/{total} tests passed")
    
    if passed == total:
        print("🎉 All tests passed! Menuconfig system is ready to use.")
        print("\nNext steps:")
        print("1. Run 'make menuconfig' to configure your build")
        print("2. Run 'make build' to build with your configuration")
        return 0
    else:
        print("❌ Some tests failed. Please check the output above.")
        return 1

if __name__ == '__main__':
    sys.exit(main())