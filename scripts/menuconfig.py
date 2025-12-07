#!/usr/bin/env python3
"""
Pico-RTOS v0.3.1 Configuration System
Interactive configuration interface for Pico-RTOS
"""

import os
import sys
import subprocess
import argparse
from pathlib import Path

# Version information
PICO_RTOS_VERSION = "0.3.1"
SCRIPT_DIR = Path(__file__).parent
PROJECT_ROOT = SCRIPT_DIR.parent

# Default configuration paths (can be overridden via command line)
DEFAULT_CONFIG_FILE = '.config'
DEFAULT_CMAKE_FILE = 'cmake_config.cmake'
DEFAULT_HEADER_FILE = 'include/pico_rtos_config.h'

# Global configuration paths (set by command line args)
CONFIG_FILE = None
CMAKE_CONFIG_FILE = None
HEADER_CONFIG_FILE = None

def check_dependencies():
    """Check if required dependencies are available"""
    try:
        import kconfiglib
        return True
    except ImportError:
        print("Error: kconfiglib not found")
        print("Install with: pip install kconfiglib")
        return False

def get_config_file():
    """Get the configuration file path"""
    if CONFIG_FILE:
        return PROJECT_ROOT / CONFIG_FILE
    return PROJECT_ROOT / DEFAULT_CONFIG_FILE

def get_cmake_config_file():
    """Get the CMake configuration file path"""
    if CMAKE_CONFIG_FILE:
        return PROJECT_ROOT / CMAKE_CONFIG_FILE
    return PROJECT_ROOT / DEFAULT_CMAKE_FILE

def get_header_config_file():
    """Get the header configuration file path"""
    if HEADER_CONFIG_FILE:
        return PROJECT_ROOT / HEADER_CONFIG_FILE
    return PROJECT_ROOT / DEFAULT_HEADER_FILE

def run_menuconfig():
    """Run the interactive menuconfig interface"""
    if not check_dependencies():
        return False
    
    try:
        import menuconfig
        # Set up environment
        os.environ['KCONFIG_CONFIG'] = str(get_config_file())
        os.environ['srctree'] = str(PROJECT_ROOT)
        
        # Run menuconfig
        kconfig_file = PROJECT_ROOT / 'Kconfig'
        if not kconfig_file.exists():
            print(f"Error: Kconfig file not found at {kconfig_file}")
            return False
        
        sys.argv = ['menuconfig', str(kconfig_file)]
        menuconfig.main()
        return True
    except Exception as e:
        print(f"Error running menuconfig: {e}")
        return False

def run_guiconfig():
    """Run the GUI configuration interface"""
    if not check_dependencies():
        return False
    
    try:
        import guiconfig
        # Set up environment
        os.environ['KCONFIG_CONFIG'] = str(get_config_file())
        os.environ['srctree'] = str(PROJECT_ROOT)
        
        # Run guiconfig
        kconfig_file = PROJECT_ROOT / 'Kconfig'
        if not kconfig_file.exists():
            print(f"Error: Kconfig file not found at {kconfig_file}")
            return False
        
        sys.argv = ['guiconfig', str(kconfig_file)]
        guiconfig.main()
        return True
    except Exception as e:
        print(f"Error running guiconfig: {e}")
        return False

def generate_config():
    """Generate configuration files from .config"""
    if not check_dependencies():
        return False
    
    try:
        import kconfiglib
        
        # Load Kconfig
        kconfig_file = PROJECT_ROOT / 'Kconfig'
        config_file = get_config_file()
        
        if not kconfig_file.exists():
            print(f"Error: Kconfig file not found at {kconfig_file}")
            return False
        
        kconf = kconfiglib.Kconfig(str(kconfig_file))
        if config_file.exists():
            kconf.load_config(str(config_file))
        else:
            print(f"Warning: {config_file} not found, using defaults")
        
        # Generate CMake config
        cmake_config = get_cmake_config_file()
        cmake_config.parent.mkdir(parents=True, exist_ok=True)
        with open(cmake_config, 'w') as f:
            f.write(f"# Generated CMake configuration for Pico-RTOS v{PICO_RTOS_VERSION}\n")
            f.write("# This file is auto-generated. Do not edit manually.\n\n")
            
            for sym in kconf.unique_defined_syms:
                if sym.type in (kconfiglib.BOOL, kconfiglib.TRISTATE):
                    if sym.str_value == 'y':
                        f.write(f"set(CONFIG_{sym.name} ON)\n")
                    else:
                        f.write(f"set(CONFIG_{sym.name} OFF)\n")
                elif sym.type in (kconfiglib.INT, kconfiglib.HEX):
                    f.write(f"set(CONFIG_{sym.name} {sym.str_value})\n")
                elif sym.type == kconfiglib.STRING:
                    f.write(f'set(CONFIG_{sym.name} "{sym.str_value}")\n')
        
        # Generate C header
        header_file = get_header_config_file()
        header_file.parent.mkdir(parents=True, exist_ok=True)
        
        with open(header_file, 'w') as f:
            f.write(f"/* Generated configuration header for Pico-RTOS v{PICO_RTOS_VERSION} */\n")
            f.write("/* This file is auto-generated. Do not edit manually. */\n\n")
            f.write("#ifndef PICO_RTOS_CONFIG_H\n")
            f.write("#define PICO_RTOS_CONFIG_H\n\n")
            
            for sym in kconf.unique_defined_syms:
                if sym.type == kconfiglib.BOOL:
                    if sym.str_value == 'y':
                        f.write(f"#define CONFIG_{sym.name} 1\n")
                    else:
                        f.write(f"/* #undef CONFIG_{sym.name} */\n")
                elif sym.type == kconfiglib.TRISTATE:
                    if sym.str_value == 'y':
                        f.write(f"#define CONFIG_{sym.name} 1\n")
                    elif sym.str_value == 'm':
                        f.write(f"#define CONFIG_{sym.name}_MODULE 1\n")
                    else:
                        f.write(f"/* #undef CONFIG_{sym.name} */\n")
                elif sym.type in (kconfiglib.INT, kconfiglib.HEX):
                    f.write(f"#define CONFIG_{sym.name} {sym.str_value}\n")
                elif sym.type == kconfiglib.STRING:
                    f.write(f'#define CONFIG_{sym.name} "{sym.str_value}"\n')
            
            f.write("\n#endif /* PICO_RTOS_CONFIG_H */\n")
        
        print("Configuration files generated successfully:")
        print(f"  - {cmake_config}")
        print(f"  - {header_file}")
        return True
    
    except Exception as e:
        print(f"Error generating configuration: {e}")
        import traceback
        traceback.print_exc()
        return False

def show_config():
    """Show current configuration"""
    config_file = get_config_file()
    if not config_file.exists():
        print(f"No configuration found at {config_file}. Run 'make menuconfig' first.")
        return False
    
    print(f"Pico-RTOS v{PICO_RTOS_VERSION} Configuration:")
    print(f"Config file: {config_file}")
    print("=" * 50)
    with open(config_file, 'r') as f:
        for line in f:
            line = line.strip()
            if line and not line.startswith('#'):
                print(line)
    return True

def save_defconfig():
    """Save minimal configuration (defconfig)"""
    if not check_dependencies():
        return False
    
    try:
        import kconfiglib
        
        kconfig_file = PROJECT_ROOT / 'Kconfig'
        config_file = get_config_file()
        defconfig_file = PROJECT_ROOT / 'defconfig'
        
        if not kconfig_file.exists():
            print(f"Error: Kconfig file not found at {kconfig_file}")
            return False
        
        kconf = kconfiglib.Kconfig(str(kconfig_file))
        if config_file.exists():
            kconf.load_config(str(config_file))
        else:
            print(f"Error: {config_file} not found")
            return False
        
        kconf.write_min_config(str(defconfig_file))
        print(f"Minimal configuration saved to {defconfig_file}")
        return True
    
    except Exception as e:
        print(f"Error saving defconfig: {e}")
        return False

def load_defconfig():
    """Load configuration from defconfig or create default config"""
    if not check_dependencies():
        return False
    
    try:
        import kconfiglib
        
        kconfig_file = PROJECT_ROOT / 'Kconfig'
        config_file = get_config_file()
        defconfig_file = PROJECT_ROOT / 'config' / 'defconfig'
        
        # Also check for defconfig in project root
        if not defconfig_file.exists():
            defconfig_file = PROJECT_ROOT / 'defconfig'
        
        if not kconfig_file.exists():
            print(f"Error: Kconfig file not found at {kconfig_file}")
            return False
        
        kconf = kconfiglib.Kconfig(str(kconfig_file))
        
        if defconfig_file.exists():
            kconf.load_config(str(defconfig_file))
            print(f"Configuration loaded from {defconfig_file}")
        else:
            print("No defconfig found, using Kconfig defaults")
        
        # Ensure config directory exists
        config_file.parent.mkdir(parents=True, exist_ok=True)
        
        # Write the configuration
        kconf.write_config(str(config_file))
        print(f"Configuration written to {config_file}")
        
        # Generate config files
        return generate_config()
    
    except Exception as e:
        print(f"Error loading defconfig: {e}")
        import traceback
        traceback.print_exc()
        return False

def main():
    global CONFIG_FILE, CMAKE_CONFIG_FILE, HEADER_CONFIG_FILE
    
    parser = argparse.ArgumentParser(
        description=f'Pico-RTOS v{PICO_RTOS_VERSION} Configuration System',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s                    # Run interactive menuconfig
  %(prog)s --gui              # Run GUI configuration
  %(prog)s --generate         # Generate config files
  %(prog)s --show             # Show current configuration
  %(prog)s --show-config      # Show current configuration (alias)
  %(prog)s --savedefconfig    # Save minimal configuration
  %(prog)s --defconfig        # Load default configuration
  %(prog)s --load-defaults    # Load default configuration (alias)
  
  # With custom paths:
  %(prog)s --config-file config/.config --cmake-file config/cmake_config.cmake
        """
    )
    
    parser.add_argument('--version', action='version', 
                       version=f'Pico-RTOS Configuration System v{PICO_RTOS_VERSION}')
    
    # Path configuration arguments
    parser.add_argument('--config-file', type=str, metavar='PATH',
                       help='Path to .config file (default: .config)')
    parser.add_argument('--cmake-file', type=str, metavar='PATH',
                       help='Path to CMake config output file (default: cmake_config.cmake)')
    parser.add_argument('--header-file', type=str, metavar='PATH',
                       help='Path to C header output file (default: include/pico_rtos_config.h)')
    
    # Action arguments
    parser.add_argument('--gui', action='store_true',
                       help='Run GUI configuration interface')
    parser.add_argument('--generate', action='store_true',
                       help='Generate configuration files from .config')
    parser.add_argument('--show', '--show-config', action='store_true', dest='show',
                       help='Show current configuration')
    parser.add_argument('--savedefconfig', action='store_true',
                       help='Save minimal configuration (defconfig)')
    parser.add_argument('--defconfig', '--load-defaults', action='store_true', dest='defconfig',
                       help='Load default configuration')
    
    args = parser.parse_args()
    
    # Set global configuration paths from arguments
    if args.config_file:
        CONFIG_FILE = args.config_file
    if args.cmake_file:
        CMAKE_CONFIG_FILE = args.cmake_file
    if args.header_file:
        HEADER_CONFIG_FILE = args.header_file
    
    # Change to project root directory
    os.chdir(PROJECT_ROOT)
    
    success = True
    
    if args.gui:
        success = run_guiconfig()
    elif args.generate:
        success = generate_config()
    elif args.show:
        success = show_config()
    elif args.savedefconfig:
        success = save_defconfig()
    elif args.defconfig:
        success = load_defconfig()
    else:
        # Default: run menuconfig
        success = run_menuconfig()
    
    return 0 if success else 1

if __name__ == '__main__':
    sys.exit(main())