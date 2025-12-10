# Pico-RTOS v0.3.1 Configuration Directory

This directory contains configuration files and templates for Pico-RTOS v0.3.1.

## Files

### Configuration Files
- **defconfig**: Default configuration for v0.3.1
- **.config**: Current project configuration (generated)
- **cmake_config.cmake**: Generated CMake configuration (generated)

### Templates
- **v0.3.1_config_template.cmake**: CMake configuration template

## Usage

### Using Default Configuration
```bash
# Load default configuration
cp config/defconfig .config
python3 scripts/menuconfig.py --generate
```

### Interactive Configuration
```bash
# Run menuconfig
python3 scripts/menuconfig.py
# or
./menuconfig.sh
```

### Build Profiles
```bash
# Minimal build
scripts/build.sh --profile minimal

# Default build
scripts/build.sh --profile default

# Full features build
scripts/build.sh --profile full
```

## Configuration Profiles

### Minimal Profile
- Basic RTOS features only
- ~8KB code footprint
- Suitable for resource-constrained applications

### Default Profile
- Standard v0.3.1 features
- ~16KB code footprint
- Recommended for most applications

### Full Profile
- All v0.3.1 features enabled
- ~24KB code footprint
- Maximum functionality for development and testing

## Generated Files

The following files are automatically generated and should not be edited manually:

- `cmake_config.cmake` - CMake configuration variables
- `include/pico_rtos_config.h` - C preprocessor definitions

These files are regenerated whenever the configuration changes.

## Version Control

It's recommended to track the following files in version control:
- `defconfig` - Default configuration
- `.config` - Project-specific configuration

The generated files should be ignored:
- `cmake_config.cmake`
- `include/pico_rtos_config.h`