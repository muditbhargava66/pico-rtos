#!/usr/bin/env python3
"""
Simple build script for Pico-RTOS
This is a compatibility wrapper for the GitHub workflows.
"""

import os
import sys
import subprocess
import argparse

def run_command(cmd, cwd=None):
    """Run a command and return the result."""
    print(f"Running: {cmd}")
    result = subprocess.run(cmd, shell=True, cwd=cwd, capture_output=False)
    return result.returncode == 0

def configure(args):
    """Configure the build."""
    build_dir = "build"
    
    # Create build directory
    os.makedirs(build_dir, exist_ok=True)
    
    # Prepare CMake command
    cmake_args = [
        "cmake", "..",
        f"-DCMAKE_BUILD_TYPE={args.build_type}"
    ]
    
    if args.enable_examples:
        cmake_args.append("-DPICO_RTOS_BUILD_EXAMPLES=ON")
    
    if args.enable_tests:
        cmake_args.append("-DPICO_RTOS_BUILD_TESTS=ON")
    
    # Run CMake
    cmd = " ".join(cmake_args)
    return run_command(cmd, cwd=build_dir)

def build(args):
    """Build the project."""
    build_dir = "build"
    
    if not os.path.exists(build_dir):
        print("Build directory doesn't exist. Run configure first.")
        return False
    
    # Determine number of cores
    try:
        import multiprocessing
        cores = multiprocessing.cpu_count()
    except:
        cores = 1
    
    cmd = f"cmake --build . --parallel {cores}"
    if args.target:
        cmd += f" --target {args.target}"
    
    return run_command(cmd, cwd=build_dir)

def clean(args):
    """Clean the build."""
    import shutil
    build_dir = "build"
    
    if os.path.exists(build_dir):
        shutil.rmtree(build_dir)
        print(f"Cleaned {build_dir}")
    
    return True

def main():
    parser = argparse.ArgumentParser(description="Pico-RTOS Build Script")
    subparsers = parser.add_subparsers(dest="command", help="Available commands")
    
    # Configure command
    config_parser = subparsers.add_parser("configure", help="Configure the build")
    config_parser.add_argument("--build-type", default="Release", 
                              choices=["Debug", "Release", "RelWithDebInfo", "MinSizeRel"],
                              help="Build type")
    config_parser.add_argument("--toolchain", default="arm-none-eabi-gcc",
                              help="Toolchain to use")
    config_parser.add_argument("--enable-examples", action="store_true",
                              help="Enable building examples")
    config_parser.add_argument("--enable-tests", action="store_true",
                              help="Enable building tests")
    config_parser.add_argument("--enable-debug", action="store_true",
                              help="Enable debug features")
    
    # Build command
    build_parser = subparsers.add_parser("build", help="Build the project")
    build_parser.add_argument("--target", help="Specific target to build")
    
    # Clean command
    clean_parser = subparsers.add_parser("clean", help="Clean the build")
    
    args = parser.parse_args()
    
    if not args.command:
        parser.print_help()
        return 1
    
    # Execute command
    if args.command == "configure":
        success = configure(args)
    elif args.command == "build":
        success = build(args)
    elif args.command == "clean":
        success = clean(args)
    else:
        print(f"Unknown command: {args.command}")
        return 1
    
    return 0 if success else 1

if __name__ == "__main__":
    sys.exit(main())