# Contributing to Pico-RTOS

Thank you for your interest in contributing to Pico-RTOS! This document provides guidelines for contributing to the project.

## Getting Started

### Prerequisites

- Git
- CMake (3.13 or later)
- ARM GCC toolchain
- Raspberry Pi Pico board for testing
- Basic knowledge of C programming and RTOS concepts

### Development Environment Setup

1. **Fork and clone the repository**:
   ```bash
   git clone https://github.com/your-username/pico-rtos.git
   cd pico-rtos
   ```

2. **Set up the development environment**:
   ```bash
   # Initialize submodules
   git submodule update --init --recursive
   
   # Create build directory
   mkdir build && cd build
   cmake ..
   make
   ```

3. **Run tests to ensure everything works**:
   ```bash
   # Flash and test the examples
   cp system_test.uf2 /Volumes/RPI-RP2/
   # Connect to serial output to verify functionality
   ```

## Types of Contributions

We welcome various types of contributions:

### Code Contributions
- Bug fixes
- New features
- Performance improvements
- Code optimizations
- Platform support

### Documentation
- API documentation improvements
- User guide enhancements
- Example code
- Tutorials and guides

### Testing
- Unit tests
- Integration tests
- Hardware testing
- Performance benchmarks

### Issue Reports
- Bug reports
- Feature requests
- Performance issues
- Documentation gaps

## Development Guidelines

### Code Style

Follow these coding standards:

#### C Code Style
```c
// Use descriptive function names with pico_rtos_ prefix
bool pico_rtos_task_create(pico_rtos_task_t *task, const char *name, 
                          pico_rtos_task_function_t function, void *param, 
                          uint32_t stack_size, uint32_t priority);

// Use consistent indentation (4 spaces)
void example_function(void) {
    if (condition) {
        // Code here
        for (int i = 0; i < count; i++) {
            // Loop body
        }
    }
}

// Use meaningful variable names
uint32_t task_priority = 5;
pico_rtos_task_t *current_task = NULL;

// Add comments for complex logic
// Calculate next task to run based on priority and state
pico_rtos_task_t *next_task = find_highest_priority_ready_task();
```

#### Header Files
```c
#ifndef PICO_RTOS_MODULE_H
#define PICO_RTOS_MODULE_H

#include <stdint.h>
#include <stdbool.h>

// Forward declarations
typedef struct pico_rtos_example pico_rtos_example_t;

// Function declarations with documentation
/**
 * @brief Brief description of the function
 * 
 * Detailed description of what the function does,
 * its parameters, and return value.
 * 
 * @param param1 Description of parameter 1
 * @param param2 Description of parameter 2
 * @return Description of return value
 */
bool pico_rtos_example_function(uint32_t param1, const char *param2);

#endif // PICO_RTOS_MODULE_H
```

### Documentation Standards

#### Function Documentation
```c
/**
 * @brief Create a new task
 * 
 * Creates a new task with the specified parameters. The task will be
 * added to the scheduler and will start running when scheduled.
 * 
 * @param task Pointer to task structure to initialize
 * @param name Name of the task (for debugging, can be NULL)
 * @param function Task function to execute
 * @param param Parameter to pass to task function
 * @param stack_size Size of task stack in bytes (minimum 128)
 * @param priority Task priority (higher number = higher priority)
 * @return true if task was created successfully, false otherwise
 * 
 * @note The task structure must remain valid for the lifetime of the task
 * @warning Stack size must be at least 128 bytes
 */
bool pico_rtos_task_create(pico_rtos_task_t *task, const char *name, 
                          pico_rtos_task_function_t function, void *param, 
                          uint32_t stack_size, uint32_t priority);
```

#### Markdown Documentation
- Use clear headings and structure
- Include code examples
- Provide step-by-step instructions
- Add troubleshooting sections
- Keep language clear and concise

### Testing Requirements

#### Unit Tests
All new functions should include unit tests:

```c
// tests/new_feature_test.c
#include <stdio.h>
#include "pico/stdlib.h"
#include "pico_rtos.h"

void test_new_feature(void *param) {
    // Test setup
    
    // Test execution
    bool result = pico_rtos_new_feature();
    
    // Verification
    if (result) {
        printf("Test PASSED: New feature works correctly\n");
    } else {
        printf("Test FAILED: New feature failed\n");
    }
    
    // Cleanup
}

int main() {
    stdio_init_all();
    pico_rtos_init();
    
    pico_rtos_task_t test_task;
    pico_rtos_task_create(&test_task, "Test", test_new_feature, NULL, 512, 1);
    
    pico_rtos_start();
    return 0;
}
```

#### Integration Tests
Test interactions between components:

```c
void integration_test(void *param) {
    // Test multiple RTOS components working together
    pico_rtos_mutex_t mutex;
    pico_rtos_queue_t queue;
    
    // Initialize components
    pico_rtos_mutex_init(&mutex);
    // ... test interactions
}
```

### Performance Considerations

- **Context Switch Time**: Keep context switches fast (target: <100 CPU cycles)
- **Memory Usage**: Minimize RAM usage per task
- **Interrupt Latency**: Ensure low interrupt response times
- **Stack Usage**: Optimize stack usage in RTOS functions

### Safety and Reliability

- **Input Validation**: Always validate function parameters
- **Error Handling**: Provide meaningful error codes and messages
- **Thread Safety**: Ensure all functions are thread-safe where appropriate
- **Stack Overflow Protection**: Maintain stack canary checking
- **Memory Management**: Prevent memory leaks and corruption

## Contribution Process

### 1. Planning

Before starting work:

1. **Check existing issues** to avoid duplicate work
2. **Open an issue** to discuss major changes
3. **Get feedback** on your approach
4. **Break down large features** into smaller, manageable pieces

### 2. Development

1. **Create a feature branch**:
   ```bash
   git checkout -b feature/your-feature-name
   ```

2. **Make incremental commits**:
   ```bash
   git add .
   git commit -m "Add: Brief description of changes"
   ```

3. **Follow commit message conventions**:
   - `Add:` for new features
   - `Fix:` for bug fixes
   - `Update:` for improvements
   - `Remove:` for deletions
   - `Docs:` for documentation changes

4. **Test thoroughly**:
   - Build on clean environment
   - Run all existing tests
   - Test on actual hardware
   - Verify examples still work

### 3. Submission

1. **Update documentation**:
   - Update API reference if needed
   - Add examples for new features
   - Update user guide
   - Add troubleshooting info

2. **Create pull request**:
   - Use descriptive title
   - Explain what changes were made
   - Reference related issues
   - Include testing information

3. **Respond to feedback**:
   - Address review comments
   - Make requested changes
   - Update tests if needed

## Pull Request Guidelines

### PR Title Format
```
[Type]: Brief description of changes

Examples:
Add: Priority inheritance support for mutexes
Fix: Stack overflow detection in task creation
Update: Improve context switch performance
Docs: Add troubleshooting guide for serial issues
```

### PR Description Template
```markdown
## Description
Brief description of what this PR does.

## Changes Made
- List of specific changes
- Another change
- etc.

## Testing
- [ ] Built successfully
- [ ] All existing tests pass
- [ ] New tests added (if applicable)
- [ ] Tested on hardware
- [ ] Examples still work

## Related Issues
Fixes #123
Related to #456

## Additional Notes
Any additional information, concerns, or questions.
```

### Review Process

1. **Automated checks** must pass
2. **Code review** by maintainers
3. **Testing verification**
4. **Documentation review**
5. **Final approval** and merge

## Issue Reporting

### Bug Reports

Use this template for bug reports:

```markdown
## Bug Description
Clear description of the bug.

## Steps to Reproduce
1. Step one
2. Step two
3. Step three

## Expected Behavior
What should happen.

## Actual Behavior
What actually happens.

## Environment
- Pico-RTOS version: 
- Pico SDK version: 
- Toolchain: 
- OS: 

## Additional Information
- Error messages
- Code snippets
- Serial output
- Any other relevant information
```

### Feature Requests

```markdown
## Feature Description
Clear description of the requested feature.

## Use Case
Why is this feature needed? What problem does it solve?

## Proposed Implementation
Ideas for how this could be implemented (optional).

## Alternatives Considered
Other approaches that were considered (optional).

## Additional Context
Any other relevant information.
```

## Code Review Guidelines

### For Contributors

- **Self-review** your code before submitting
- **Test thoroughly** on actual hardware
- **Write clear commit messages**
- **Respond promptly** to review feedback
- **Be open** to suggestions and improvements

### For Reviewers

- **Be constructive** and helpful
- **Focus on code quality** and correctness
- **Consider performance** implications
- **Check documentation** completeness
- **Test when possible**

## Release Process

### Version Numbering

We use semantic versioning (MAJOR.MINOR.PATCH):

- **MAJOR**: Breaking API changes
- **MINOR**: New features, backward compatible
- **PATCH**: Bug fixes, backward compatible

### Release Checklist

- [ ] All tests pass
- [ ] Documentation updated
- [ ] Examples work correctly
- [ ] Performance benchmarks run
- [ ] CHANGELOG.md updated
- [ ] Version numbers updated
- [ ] Release notes prepared

## Community Guidelines

### Code of Conduct

- **Be respectful** and professional
- **Welcome newcomers** and help them learn
- **Focus on technical merit** of contributions
- **Provide constructive feedback**
- **Collaborate effectively**

### Communication

- **Use GitHub issues** for bug reports and feature requests
- **Use pull requests** for code contributions
- **Be clear and concise** in communications
- **Provide context** for your contributions

## Getting Help

If you need help with contributing:

1. **Read the documentation** thoroughly
2. **Check existing issues** and pull requests
3. **Look at the examples** for guidance
4. **Ask questions** in GitHub issues
5. **Start small** with simple contributions

## Recognition

Contributors are recognized in:

- **CONTRIBUTORS.md** file
- **Release notes** for significant contributions
- **Documentation** for major features
- **GitHub contributors** page

Thank you for contributing to Pico-RTOS! Your contributions help make embedded development better for everyone.