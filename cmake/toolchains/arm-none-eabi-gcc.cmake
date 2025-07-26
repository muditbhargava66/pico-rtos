# ARM GCC Toolchain Configuration for Pico-RTOS
# This toolchain file configures GCC for ARM Cortex-M0+ (RP2040)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR ARM)

# Toolchain prefix
set(TOOLCHAIN_PREFIX arm-none-eabi-)

# Compiler settings
set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++)
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_AR ${TOOLCHAIN_PREFIX}ar)
set(CMAKE_RANLIB ${TOOLCHAIN_PREFIX}ranlib)
set(CMAKE_STRIP ${TOOLCHAIN_PREFIX}strip)
set(CMAKE_OBJCOPY ${TOOLCHAIN_PREFIX}objcopy)
set(CMAKE_OBJDUMP ${TOOLCHAIN_PREFIX}objdump)
set(CMAKE_SIZE ${TOOLCHAIN_PREFIX}size)

# Target CPU and architecture
set(CMAKE_C_FLAGS_INIT "-mcpu=cortex-m0plus -mthumb")
set(CMAKE_CXX_FLAGS_INIT "-mcpu=cortex-m0plus -mthumb")
set(CMAKE_ASM_FLAGS_INIT "-mcpu=cortex-m0plus -mthumb")

# Optimization flags for different build types
set(CMAKE_C_FLAGS_DEBUG "-O0 -g3 -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG")
set(CMAKE_C_FLAGS_MINSIZEREL "-Os -DNDEBUG")

set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG")
set(CMAKE_CXX_FLAGS_MINSIZEREL "-Os -DNDEBUG")

# Linker flags
set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,--gc-sections")

# GCC-specific optimizations
set(PICO_RTOS_GCC_OPTIMIZATION_FLAGS
    "-ffunction-sections"
    "-fdata-sections"
    "-fno-common"
    "-fstack-usage"
)

# GCC-specific warning flags
set(PICO_RTOS_GCC_WARNING_FLAGS
    "-Wall"
    "-Wextra"
    "-Wpedantic"
    "-Wstrict-prototypes"
    "-Wmissing-prototypes"
    "-Wold-style-definition"
    "-Wredundant-decls"
    "-Wnested-externs"
    "-Wbad-function-cast"
    "-Wcast-qual"
    "-Wshadow"
    "-Wundef"
    "-Wwrite-strings"
    "-Wconversion"
    "-Wsign-conversion"
    "-Wunreachable-code"
    "-Wformat=2"
    "-Wformat-security"
    "-Wformat-nonliteral"
    "-Winit-self"
    "-Wswitch-default"
    "-Wswitch-enum"
    "-Wunused-parameter"
    "-Wstrict-overflow=5"
    "-Wfloat-equal"
    "-Wlogical-op"
    "-Wduplicated-cond"
    "-Wduplicated-branches"
    "-Wnull-dereference"
)

# GCC-specific debug flags
set(PICO_RTOS_GCC_DEBUG_FLAGS
    "-ggdb3"
    "-gdwarf-4"
    "-fvar-tracking"
    "-fvar-tracking-assignments"
)

# Set toolchain-specific variables
set(PICO_RTOS_TOOLCHAIN_NAME "GCC")
set(PICO_RTOS_TOOLCHAIN_VERSION_COMMAND "${CMAKE_C_COMPILER} --version")
set(PICO_RTOS_SUPPORTS_LTO ON)
set(PICO_RTOS_SUPPORTS_STACK_PROTECTION ON)
set(PICO_RTOS_SUPPORTS_SANITIZERS OFF) # Not available for embedded targets

# Don't search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Function to apply GCC-specific flags
function(pico_rtos_apply_gcc_flags target)
    target_compile_options(${target} PRIVATE ${PICO_RTOS_GCC_OPTIMIZATION_FLAGS})
    target_compile_options(${target} PRIVATE ${PICO_RTOS_GCC_WARNING_FLAGS})
    
    # Apply debug flags in debug builds
    target_compile_options(${target} PRIVATE 
        $<$<CONFIG:Debug>:${PICO_RTOS_GCC_DEBUG_FLAGS}>
    )
    
    # Enable LTO for release builds if supported
    if(PICO_RTOS_SUPPORTS_LTO)
        set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
        set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE)
    endif()
    
    # Enable stack protection if supported and requested
    if(PICO_RTOS_SUPPORTS_STACK_PROTECTION AND PICO_RTOS_ENABLE_STACK_PROTECTION)
        target_compile_options(${target} PRIVATE "-fstack-protector-strong")
    endif()
endfunction()