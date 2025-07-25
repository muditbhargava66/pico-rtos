# ARM Clang Toolchain Configuration for Pico-RTOS
# This toolchain file configures Clang for ARM Cortex-M0+ (RP2040)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR ARM)

# Toolchain settings
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_ASM_COMPILER clang)
set(CMAKE_AR llvm-ar)
set(CMAKE_RANLIB llvm-ranlib)
set(CMAKE_STRIP llvm-strip)
set(CMAKE_OBJCOPY llvm-objcopy)
set(CMAKE_OBJDUMP llvm-objdump)
set(CMAKE_SIZE llvm-size)

# Target triple for ARM Cortex-M0+
set(CLANG_TARGET_TRIPLE "thumbv6m-none-eabi")

# Target CPU and architecture
set(CMAKE_C_FLAGS_INIT "--target=${CLANG_TARGET_TRIPLE} -mcpu=cortex-m0plus -mthumb")
set(CMAKE_CXX_FLAGS_INIT "--target=${CLANG_TARGET_TRIPLE} -mcpu=cortex-m0plus -mthumb")
set(CMAKE_ASM_FLAGS_INIT "--target=${CLANG_TARGET_TRIPLE} -mcpu=cortex-m0plus -mthumb")

# Optimization flags for different build types
set(CMAKE_C_FLAGS_DEBUG "-O0 -g3 -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG")
set(CMAKE_C_FLAGS_MINSIZEREL "-Oz -DNDEBUG")

set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG")
set(CMAKE_CXX_FLAGS_MINSIZEREL "-Oz -DNDEBUG")

# Linker flags
set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,--gc-sections")

# Clang-specific optimizations
set(PICO_RTOS_CLANG_OPTIMIZATION_FLAGS
    "-ffunction-sections"
    "-fdata-sections"
    "-fno-common"
    "-fshort-enums"
    "-fomit-frame-pointer"
)

# Clang-specific warning flags
set(PICO_RTOS_CLANG_WARNING_FLAGS
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
    "-Wfloat-equal"
    "-Wnull-dereference"
    "-Wthread-safety"
    "-Wthread-safety-beta"
    "-Wdocumentation"
    "-Wno-documentation-unknown-command"
)

# Clang-specific debug flags
set(PICO_RTOS_CLANG_DEBUG_FLAGS
    "-glldb"
    "-gdwarf-4"
    "-fstandalone-debug"
)

# Clang-specific sanitizer flags (limited support for embedded)
set(PICO_RTOS_CLANG_SANITIZER_FLAGS
    "-fsanitize=undefined"
    "-fsanitize=integer"
    "-fsanitize=nullability"
    "-fno-sanitize-recover=all"
)

# Set toolchain-specific variables
set(PICO_RTOS_TOOLCHAIN_NAME "Clang")
set(PICO_RTOS_TOOLCHAIN_VERSION_COMMAND "${CMAKE_C_COMPILER} --version")
set(PICO_RTOS_SUPPORTS_LTO ON)
set(PICO_RTOS_SUPPORTS_STACK_PROTECTION ON)
set(PICO_RTOS_SUPPORTS_SANITIZERS ON)

# Don't search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Function to apply Clang-specific flags
function(pico_rtos_apply_clang_flags target)
    target_compile_options(${target} PRIVATE ${PICO_RTOS_CLANG_OPTIMIZATION_FLAGS})
    target_compile_options(${target} PRIVATE ${PICO_RTOS_CLANG_WARNING_FLAGS})
    
    # Apply debug flags in debug builds
    target_compile_options(${target} PRIVATE 
        $<$<CONFIG:Debug>:${PICO_RTOS_CLANG_DEBUG_FLAGS}>
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
    
    # Enable sanitizers in debug builds if supported and requested
    if(PICO_RTOS_SUPPORTS_SANITIZERS AND PICO_RTOS_ENABLE_SANITIZERS)
        target_compile_options(${target} PRIVATE 
            $<$<CONFIG:Debug>:${PICO_RTOS_CLANG_SANITIZER_FLAGS}>
        )
        target_link_options(${target} PRIVATE 
            $<$<CONFIG:Debug>:${PICO_RTOS_CLANG_SANITIZER_FLAGS}>
        )
    endif()
endfunction()