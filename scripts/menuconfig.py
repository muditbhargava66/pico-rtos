#!/usr/bin/env python3
"""
Pico-RTOS v0.3.0 Configuration System
Interactive configuration interface for Pico-RTOS
"""

import os
import sys
import subprocess
import argparse
from pathlib import Path

# Version information
PICO_RTOS_VERSION = "0.3.0"
SCRIPT_DIR = Path(__file__).parent
PROJECT_ROOT = SCRIPT_DIR.parent

def check_dependencies():
    """Check if required dependencies are available"""
    try:
        import kconfiglib
        return True
    except ImportError:
        print("Error: kconfiglib not found")
        print("Install with: pip install kconfiglib")
        return False

def run_menuconfig():
    """Run the interactive menuconfig interface"""
    if not check_dependencies():
        return False
    
    try:
        import menuconfig
        # Set up environment
        os.environ['KCONFIG_CONFIG'] = str(PROJECT_ROOT / '.config')
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
        os.environ['KCONFIG_CONFIG'] = str(PROJECT_ROOT / '.config')
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
        config_file = PROJECT_ROOT / '.config'
        
        if not kconfig_file.exists():
            print(f"Error: Kconfig file not found at {kconfig_file}")
            return False
        
        kconf = kconfiglib.Kconfig(str(kconfig_file))
        if config_file.exists():
            kconf.load_config(str(config_file))
        else:
            print("Warning: .config not found, using defaults")
        
        # Generate CMake config
        cmake_config = PROJECT_ROOT / 'cmake_config.cmake'
        with open(cmake_config, 'w') as f:
            f.write(f"# Generated CMake configuration for Pico-RTOS v{PICO_RTOS_VERSION}\n")
            f.write("# This file is auto-generated. Do not edit manually.\n\n")
            
            for node in kconf.node_iter():
                if node.item and node.item.type in (kconfiglib.BOOL, kconfiglib.TRISTATE):
                    if node.item.str_value == 'y':
                        f.write(f"set({node.item.name} ON)\n")
                    else:
                        f.write(f"set({node.item.name} OFF)\n")
                elif node.item and node.item.type in (kconfiglib.INT, kconfiglib.HEX):
                    f.write(f"set({node.item.name} {node.item.str_value})\n")
                elif node.item and node.item.type == kconfiglib.STRING:
                    f.write(f'set({node.item.name} "{node.item.str_value}")\n')
        
        # Generate C header
        header_dir = PROJECT_ROOT / 'include'
        header_dir.mkdir(exist_ok=True)
        header_file = header_dir / 'pico_rtos_config.h'
        
        with open(header_file, 'w') as f:
            f.write(f"/* Generated configuration header for Pico-RTOS v{PICO_RTOS_VERSION} */\n")
            f.write("/* This file is auto-generated. Do not edit manually. */\n\n")
            f.write("#ifndef PICO_RTOS_CONFIG_H\n")
            f.write("#define PICO_RTOS_CONFIG_H\n\n")
            
            for node in kconf.node_iter():
                if node.item and node.item.type == kconfiglib.BOOL:
                    if node.item.str_value == 'y':
                        f.write(f"#define {node.item.name} 1\n")
                    else:
                        f.write(f"/* #undef {node.item.name} */\n")
                elif node.item and node.item.type in (kconfiglib.INT, kconfiglib.HEX):
                    f.write(f"#define {node.item.name} {node.item.str_value}\n")
                elif node.item and node.item.type == kconfiglib.STRING:
                    f.write(f'#define {node.item.name} "{node.item.str_value}"\n')
            
            f.write("\n#endif /* PICO_RTOS_CONFIG_H */\n")
        
        print("Configuration files generated successfully:")
        print(f"  - {cmake_config}")
        print(f"  - {header_file}")
        return True
    
    except Exception as e:
        print(f"Error generating configuration: {e}")
        return False

def show_config():
    """Show current configuration"""
    config_file = PROJECT_ROOT / '.config'
    if not config_file.exists():
        print("No configuration found. Run 'make menuconfig' first.")
        return False
    
    print(f"Pico-RTOS v{PICO_RTOS_VERSION} Configuration:")
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
        config_file = PROJECT_ROOT / '.config'
        defconfig_file = PROJECT_ROOT / 'defconfig'
        
        if not kconfig_file.exists():
            print(f"Error: Kconfig file not found at {kconfig_file}")
            return False
        
        kconf = kconfiglib.Kconfig(str(kconfig_file))
        if config_file.exists():
            kconf.load_config(str(config_file))
        else:
            print("Error: .config not found")
            return False
        
        kconf.write_min_config(str(defconfig_file))
        print(f"Minimal configuration saved to {defconfig_file}")
        return True
    
    except Exception as e:
        print(f"Error saving defconfig: {e}")
        return False

def load_defconfig():
    """Load configuration from defconfig"""
    defconfig_file = PROJECT_ROOT / 'defconfig'
    config_file = PROJECT_ROOT / '.config'
    
    if not defconfig_file.exists():
        print(f"Error: defconfig file not found at {defconfig_file}")
        return False
    
    try:
        # Copy defconfig to .config
        import shutil
        shutil.copy2(defconfig_file, config_file)
        print(f"Configuration loaded from {defconfig_file}")
        
        # Generate config files
        return generate_config()
    
    except Exception as e:
        print(f"Error loading defconfig: {e}")
        return False

def main():
    parser = argparse.ArgumentParser(
        description=f'Pico-RTOS v{PICO_RTOS_VERSION} Configuration System',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s                    # Run interactive menuconfig
  %(prog)s --gui              # Run GUI configuration
  %(prog)s --generate         # Generate config files
  %(prog)s --show             # Show current configuration
  %(prog)s --savedefconfig    # Save minimal configuration
  %(prog)s --defconfig        # Load default configuration
        """
    )
    
    parser.add_argument('--version', action='version', 
                       version=f'Pico-RTOS Configuration System v{PICO_RTOS_VERSION}')
    parser.add_argument('--gui', action='store_true',
                       help='Run GUI configuration interface')
    parser.add_argument('--generate', action='store_true',
                       help='Generate configuration files from .config')
    parser.add_argument('--show', action='store_true',
                       help='Show current configuration')
    parser.add_argument('--savedefconfig', action='store_true',
                       help='Save minimal configuration (defconfig)')
    parser.add_argument('--defconfig', action='store_true',
                       help='Load default configuration')
    
    args = parser.parse_args()
    
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