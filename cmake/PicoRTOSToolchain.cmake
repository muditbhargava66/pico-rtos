# Pico-RTOS Toolchain Detection and Configuration
# This module provides toolchain detection and configuration for multiple ARM toolchains

# Available toolchains
set(PICO_RTOS_SUPPORTED_TOOLCHAINS
    "arm-none-eabi-gcc"
    "arm-none-eabi-clang"
)

# Default toolchain preference order
set(PICO_RTOS_TOOLCHAIN_PREFERENCE
    "arm-none-eabi-gcc"
    "arm-none-eabi-clang"
)

# Function to detect available toolchains
function(pico_rtos_detect_toolchains)
    set(DETECTED_TOOLCHAINS "")
    
    # Check for GCC
    find_program(ARM_GCC_COMPILER arm-none-eabi-gcc)
    if(ARM_GCC_COMPILER)
        list(APPEND DETECTED_TOOLCHAINS "arm-none-eabi-gcc")
        message(STATUS "Found ARM GCC: ${ARM_GCC_COMPILER}")
        
        # Get GCC version
        execute_process(
            COMMAND ${ARM_GCC_COMPILER} --version
            OUTPUT_VARIABLE GCC_VERSION_OUTPUT
            ERROR_QUIET
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" GCC_VERSION "${GCC_VERSION_OUTPUT}")
        message(STATUS "ARM GCC version: ${GCC_VERSION}")
        set(PICO_RTOS_GCC_VERSION ${GCC_VERSION} PARENT_SCOPE)
    endif()
    
    # Check for Clang
    find_program(CLANG_COMPILER clang)
    if(CLANG_COMPILER)
        # Check if Clang supports ARM targets
        execute_process(
            COMMAND ${CLANG_COMPILER} --print-targets
            OUTPUT_VARIABLE CLANG_TARGETS
            ERROR_QUIET
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if(CLANG_TARGETS MATCHES "thumb")
            list(APPEND DETECTED_TOOLCHAINS "arm-none-eabi-clang")
            message(STATUS "Found ARM Clang: ${CLANG_COMPILER}")
            
            # Get Clang version
            execute_process(
                COMMAND ${CLANG_COMPILER} --version
                OUTPUT_VARIABLE CLANG_VERSION_OUTPUT
                ERROR_QUIET
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )
            string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" CLANG_VERSION "${CLANG_VERSION_OUTPUT}")
            message(STATUS "ARM Clang version: ${CLANG_VERSION}")
            set(PICO_RTOS_CLANG_VERSION ${CLANG_VERSION} PARENT_SCOPE)
        endif()
    endif()
    
    set(PICO_RTOS_DETECTED_TOOLCHAINS ${DETECTED_TOOLCHAINS} PARENT_SCOPE)
endfunction()

# Function to select the best available toolchain
function(pico_rtos_select_toolchain)
    pico_rtos_detect_toolchains()
    
    if(NOT PICO_RTOS_DETECTED_TOOLCHAINS)
        message(FATAL_ERROR "No supported ARM toolchains found. Please install arm-none-eabi-gcc or clang with ARM support.")
    endif()
    
    # Use user-specified toolchain if provided
    if(PICO_RTOS_TOOLCHAIN)
        if(NOT PICO_RTOS_TOOLCHAIN IN_LIST PICO_RTOS_SUPPORTED_TOOLCHAINS)
            message(FATAL_ERROR "Unsupported toolchain: ${PICO_RTOS_TOOLCHAIN}. Supported toolchains: ${PICO_RTOS_SUPPORTED_TOOLCHAINS}")
        endif()
        
        if(NOT PICO_RTOS_TOOLCHAIN IN_LIST PICO_RTOS_DETECTED_TOOLCHAINS)
            message(FATAL_ERROR "Requested toolchain ${PICO_RTOS_TOOLCHAIN} is not available. Available toolchains: ${PICO_RTOS_DETECTED_TOOLCHAINS}")
        endif()
        
        set(SELECTED_TOOLCHAIN ${PICO_RTOS_TOOLCHAIN})
    else()
        # Select based on preference order
        foreach(PREFERRED_TOOLCHAIN ${PICO_RTOS_TOOLCHAIN_PREFERENCE})
            if(PREFERRED_TOOLCHAIN IN_LIST PICO_RTOS_DETECTED_TOOLCHAINS)
                set(SELECTED_TOOLCHAIN ${PREFERRED_TOOLCHAIN})
                break()
            endif()
        endforeach()
    endif()
    
    if(NOT SELECTED_TOOLCHAIN)
        list(GET PICO_RTOS_DETECTED_TOOLCHAINS 0 SELECTED_TOOLCHAIN)
    endif()
    
    message(STATUS "Selected toolchain: ${SELECTED_TOOLCHAIN}")
    set(PICO_RTOS_SELECTED_TOOLCHAIN ${SELECTED_TOOLCHAIN} PARENT_SCOPE)
endfunction()

# Function to configure the selected toolchain
function(pico_rtos_configure_toolchain)
    if(NOT PICO_RTOS_SELECTED_TOOLCHAIN)
        pico_rtos_select_toolchain()
    endif()
    
    # Set the toolchain file - resolve relative to the project root
    set(TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/toolchains/${PICO_RTOS_SELECTED_TOOLCHAIN}.cmake")
    
    if(NOT EXISTS ${TOOLCHAIN_FILE})
        message(FATAL_ERROR "Toolchain file not found: ${TOOLCHAIN_FILE}")
    endif()
    
    # Include the toolchain file
    include(${TOOLCHAIN_FILE})
    
    message(STATUS "Configured toolchain: ${PICO_RTOS_TOOLCHAIN_NAME}")
    
    # Set global variables
    set(PICO_RTOS_ACTIVE_TOOLCHAIN ${PICO_RTOS_SELECTED_TOOLCHAIN} CACHE INTERNAL "Active toolchain")
    set(PICO_RTOS_ACTIVE_TOOLCHAIN_NAME ${PICO_RTOS_TOOLCHAIN_NAME} CACHE INTERNAL "Active toolchain name")
endfunction()

# Function to apply GCC-specific flags
function(pico_rtos_apply_gcc_flags target)
    # Add GCC-specific optimization and warning flags
    target_compile_options(${target} PRIVATE
        -Wall
        -Wextra
        -Wno-unused-parameter
        -ffunction-sections
        -fdata-sections
    )
    
    # Add GCC-specific linker flags
    target_link_options(${target} PRIVATE
        -Wl,--gc-sections
    )
endfunction()

# Function to apply Clang-specific flags
function(pico_rtos_apply_clang_flags target)
    # Add Clang-specific optimization and warning flags
    target_compile_options(${target} PRIVATE
        -Wall
        -Wextra
        -Wno-unused-parameter
        -ffunction-sections
        -fdata-sections
    )
    
    # Add Clang-specific linker flags
    target_link_options(${target} PRIVATE
        -Wl,--gc-sections
    )
endfunction()

# Function to apply toolchain-specific flags to a target
function(pico_rtos_apply_toolchain_flags target)
    if(NOT PICO_RTOS_ACTIVE_TOOLCHAIN)
        message(FATAL_ERROR "No active toolchain configured. Call pico_rtos_configure_toolchain() first.")
    endif()
    
    # Apply toolchain-specific flags
    if(PICO_RTOS_ACTIVE_TOOLCHAIN STREQUAL "arm-none-eabi-gcc")
        pico_rtos_apply_gcc_flags(${target})
    elseif(PICO_RTOS_ACTIVE_TOOLCHAIN STREQUAL "arm-none-eabi-clang")
        pico_rtos_apply_clang_flags(${target})
    endif()
    
    # Apply common flags
    target_compile_options(${target} PRIVATE
        -fno-exceptions
        -fno-rtti
        -fno-threadsafe-statics
    )
    
    # Apply build type specific flags
    target_compile_options(${target} PRIVATE
        $<$<CONFIG:Debug>:-DPICO_RTOS_DEBUG=1>
        $<$<CONFIG:Release>:-DPICO_RTOS_RELEASE=1>
        $<$<CONFIG:RelWithDebInfo>:-DPICO_RTOS_RELEASE=1>
        $<$<CONFIG:MinSizeRel>:-DPICO_RTOS_RELEASE=1>
    )
endfunction()

# Function to print toolchain information
function(pico_rtos_print_toolchain_info)
    message(STATUS "=== Pico-RTOS Toolchain Information ===")
    message(STATUS "Active toolchain: ${PICO_RTOS_ACTIVE_TOOLCHAIN_NAME} (${PICO_RTOS_ACTIVE_TOOLCHAIN})")
    
    if(PICO_RTOS_GCC_VERSION)
        message(STATUS "GCC version: ${PICO_RTOS_GCC_VERSION}")
    endif()
    
    if(PICO_RTOS_CLANG_VERSION)
        message(STATUS "Clang version: ${PICO_RTOS_CLANG_VERSION}")
    endif()
    
    message(STATUS "Detected toolchains: ${PICO_RTOS_DETECTED_TOOLCHAINS}")
    message(STATUS "LTO support: ${PICO_RTOS_SUPPORTS_LTO}")
    message(STATUS "Stack protection support: ${PICO_RTOS_SUPPORTS_STACK_PROTECTION}")
    message(STATUS "Sanitizer support: ${PICO_RTOS_SUPPORTS_SANITIZERS}")
    message(STATUS "=====================================")
endfunction()

# Platform-specific configuration
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    set(PICO_RTOS_HOST_PLATFORM "Windows")
    # Windows-specific toolchain paths
    list(APPEND CMAKE_PROGRAM_PATH
        "C:/Program Files (x86)/GNU Arm Embedded Toolchain"
        "C:/Program Files/GNU Arm Embedded Toolchain"
        "C:/Program Files/LLVM/bin"
    )
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    set(PICO_RTOS_HOST_PLATFORM "macOS")
    # macOS-specific toolchain paths (Homebrew, MacPorts)
    list(APPEND CMAKE_PROGRAM_PATH
        "/usr/local/bin"
        "/opt/homebrew/bin"
        "/opt/local/bin"
    )
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    set(PICO_RTOS_HOST_PLATFORM "Linux")
    # Linux-specific toolchain paths
    list(APPEND CMAKE_PROGRAM_PATH
        "/usr/bin"
        "/usr/local/bin"
        "/opt/gcc-arm-none-eabi/bin"
    )
endif()

message(STATUS "Host platform: ${PICO_RTOS_HOST_PLATFORM}")

# Toolchain validation function
function(pico_rtos_validate_toolchain)
    if(NOT CMAKE_C_COMPILER)
        message(FATAL_ERROR "C compiler not found")
    endif()
    
    if(NOT CMAKE_CXX_COMPILER)
        message(FATAL_ERROR "C++ compiler not found")
    endif()
    
    # Test compilation
    try_compile(COMPILER_TEST_RESULT
        ${CMAKE_BINARY_DIR}/toolchain_test
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/test_sources/toolchain_test.c
        OUTPUT_VARIABLE COMPILER_TEST_OUTPUT
    )
    
    if(NOT COMPILER_TEST_RESULT)
        message(FATAL_ERROR "Toolchain validation failed: ${COMPILER_TEST_OUTPUT}")
    endif()
    
    message(STATUS "Toolchain validation passed")
endfunction()

# Create test source for toolchain validation
file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/cmake/test_sources/toolchain_test.c
"#include <stdint.h>
#include <stdbool.h>

// Test basic ARM Cortex-M0+ features
volatile uint32_t test_var = 0;

int main(void) {
    test_var = 42;
    return (int)test_var;
}
")

# Auto-configure if not already done
if(NOT PICO_RTOS_TOOLCHAIN_CONFIGURED)
    pico_rtos_configure_toolchain()
    set(PICO_RTOS_TOOLCHAIN_CONFIGURED TRUE CACHE INTERNAL "Toolchain configured")
endif()