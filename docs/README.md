# Pico-RTOS v0.3.1 Documentation

Welcome to the Pico-RTOS documentation! This directory contains comprehensive guides and references for using **Pico-RTOS v0.3.1 "Advanced Synchronization & Multi-Core"**.

## 📚 Documentation Overview

### 🚀 **Getting Started**
- **[Getting Started Guide](getting_started.md)** - Your first steps with Pico-RTOS
  - Installation and setup
  - Building your first project
  - Running examples
  - Basic usage patterns

### 🔧 **Hardware Integration**
- **[Flashing and Testing Guide](flashing_and_testing.md)** - Complete hardware workflow
  - How to flash firmware to Pico
  - Serial communication setup
  - Monitoring output and debugging
  - Testing examples and applications
  - Hardware troubleshooting

### 📖 **User Documentation**
- **[User Guide](user_guide.md)** - Comprehensive usage documentation
  - Core concepts and architecture
  - Task management and scheduling
  - Synchronization primitives (including v0.3.1 features)
  - Memory management and protection
  - Multi-core programming
  - System monitoring and debugging

### 📋 **API Reference**
- **[API Reference v0.3.1](api_reference_v0.3.1.md)** - Complete v0.3.1 function documentation
  - All RTOS functions with parameters and examples
  - New v0.3.1 APIs (Event Groups, Stream Buffers, Memory Pools, etc.)
  - Multi-core and SMP functions
  - Debugging and profiling APIs
  - Return values and error codes

### 🔄 **Migration & Compatibility**
- **[Migration Guide v0.3.1](migration_guide_v0.3.1.md)** - Upgrading from v0.2.1
  - Step-by-step migration process
  - New features overview
  - Configuration changes
  - Compatibility considerations
  - Troubleshooting migration issues

### 🚨 **Error Handling & Debugging**
- **[Error Code Reference](error_codes.md)** - Comprehensive error documentation
  - Complete error code catalog with descriptions
  - Common causes and solutions
  - Error handling best practices
  - Debugging with error history

- **[Logging System Guide](logging_guide.md)** - Debug logging documentation
  - Multi-level logging system
  - Subsystem filtering
  - Performance considerations
  - Custom output functions

### ⚙️ **Configuration & Build System**
- **[Menuconfig Guide](menuconfig_guide.md)** - Interactive configuration system
  - Terminal and GUI interfaces
  - v0.3.1 configuration options
  - Build profiles (minimal, default, full)
  - Build system integration

### 🔍 **Advanced Features**

#### Multi-Core Programming
- **[Multi-Core Programming Guide](multicore_programming_guide.md)** - SMP development
  - Dual-core scheduler usage
  - Load balancing and core affinity
  - Inter-core communication
  - Synchronization across cores
  - Performance optimization

#### Performance & Optimization
- **[Performance Guide v0.3.1](performance_guide_v0.3.1.md)** - Optimization techniques
  - Real-time performance analysis
  - Memory optimization
  - Multi-core scaling
  - Profiling and benchmarking

#### Troubleshooting
- **[Troubleshooting v0.3.1](troubleshooting_v0.3.1.md)** - Problem solving
  - v0.3.1 specific issues
  - Multi-core debugging
  - Performance troubleshooting
  - Configuration problems

### 🤝 **Development**
- **[Contributing Guidelines](contributing.md)** - How to contribute
  - Development environment setup
  - Code style and standards
  - Testing requirements
  - Pull request process

## 🎯 v0.3.1 Feature Documentation

### New Synchronization Primitives
- **Event Groups**: 32-bit event coordination with any/all semantics
- **Stream Buffers**: Zero-copy message passing with variable-length support
- **Enhanced Mutexes**: Advanced priority inheritance

### Multi-Core Support (SMP)
- **Dual-Core Scheduler**: Symmetric multiprocessing for RP2040
- **Load Balancing**: Automatic workload distribution
- **Inter-Core Communication**: High-performance IPC channels
- **Core Affinity**: Task binding to specific cores

### Enhanced Memory Management
- **Memory Pools**: O(1) deterministic allocation
- **MPU Support**: Hardware memory protection
- **Advanced Statistics**: Comprehensive memory monitoring

### Debugging & Profiling Tools
- **Runtime Task Inspection**: Non-intrusive monitoring
- **Execution Profiling**: High-resolution performance analysis
- **System Tracing**: Configurable event logging
- **Enhanced Assertions**: Advanced debugging support

### Production Quality Features
- **Deadlock Detection**: Runtime prevention system
- **Health Monitoring**: Comprehensive system metrics
- **Watchdog Integration**: Hardware reliability
- **Alert System**: Proactive monitoring

## 📖 Quick Navigation

### For New Users
1. **[Getting Started](getting_started.md)** - Installation and first project
2. **[Flashing and Testing](flashing_and_testing.md)** - Run examples on hardware
3. **[User Guide](user_guide.md)** - Learn core concepts
4. **[API Reference v0.3.1](api_reference_v0.3.1.md)** - Function documentation

### For Upgrading Users
1. **[Migration Guide v0.3.1](migration_guide_v0.3.1.md)** - Upgrade from v0.2.1
2. **[Performance Guide v0.3.1](performance_guide_v0.3.1.md)** - Optimize your application
3. **[Multi-Core Programming Guide](multicore_programming_guide.md)** - Use SMP features

### For Advanced Development
1. **[Multi-Core Programming Guide](multicore_programming_guide.md)** - SMP development
2. **[Performance Guide v0.3.1](performance_guide_v0.3.1.md)** - Optimization techniques
3. **[Contributing Guidelines](contributing.md)** - Development workflow

### For Troubleshooting
1. **[Troubleshooting v0.3.1](troubleshooting_v0.3.1.md)** - v0.3.1 specific issues
2. **[Migration Guide v0.3.1](migration_guide_v0.3.1.md)** - Migration problems
3. **[Performance Guide v0.3.1](performance_guide_v0.3.1.md)** - Performance issues

## 🔧 Documentation Features

### ✅ **Production Ready v0.3.1**
All documentation covers the complete v0.3.1 feature set:
- Advanced synchronization primitives
- Multi-core support and SMP scheduling
- Enhanced memory management with MPU support
- Comprehensive debugging and profiling tools
- Production-grade quality assurance features
- 100% backward compatibility with v0.2.1

### 💡 **Practical Examples**
Every guide includes:
- Working code examples for v0.3.1 features
- Step-by-step instructions
- Real-world usage patterns
- Multi-core programming examples
- Performance optimization techniques

### 🚀 **Hardware Optimized**
Documentation covers:
- RP2040 dual-core specific features
- ARM Cortex-M0+ optimizations
- Hardware debugging with new tools
- Multi-core hardware considerations

## 📊 Documentation Status

| Document | Status | Version | Features |
|----------|--------|---------|----------|
| Getting Started | ✅ Complete | v0.3.1 | Updated for v0.3.1 |
| User Guide | ✅ Complete | v0.3.1 | Multi-core, new APIs |
| API Reference v0.3.1 | ✅ Complete | v0.3.1 | All v0.3.1 APIs |
| Migration Guide v0.3.1 | ✅ Complete | v0.3.1 | v0.2.1 → v0.3.1 |
| Multi-Core Guide | ✅ Complete | v0.3.1 | SMP programming |
| Performance Guide v0.3.1 | ✅ Complete | v0.3.1 | Optimization |
| Troubleshooting v0.3.1 | ✅ Complete | v0.3.1 | v0.3.1 issues |
| Menuconfig Guide | ✅ Complete | v0.3.1 | v0.3.1 config |
| Error Code Reference | ✅ Complete | v0.3.1 | Updated codes |
| Logging Guide | ✅ Complete | v0.3.1 | Enhanced logging |
| Flashing and Testing | ✅ Complete | v0.3.1 | Updated workflow |
| Contributing | ✅ Complete | v0.3.1 | v0.3.1 workflow |

## 🆕 What's New in v0.3.1 Documentation

### New Guides
- **Multi-Core Programming Guide**: Complete SMP development guide
- **Performance Guide v0.3.1**: Optimization and benchmarking
- **Migration Guide v0.3.1**: Smooth upgrade path from v0.2.1
- **API Reference v0.3.1**: Complete v0.3.1 API documentation

### Enhanced Guides
- **User Guide**: Updated with v0.3.1 features and multi-core concepts
- **Troubleshooting**: v0.3.1 specific issues and solutions
- **Menuconfig Guide**: New configuration options and profiles

### Updated Examples
- All code examples updated for v0.3.1
- Multi-core programming examples
- New synchronization primitive examples
- Performance optimization examples

## 🤝 Getting Help

If you need assistance:

1. **Search Documentation** - Use browser search across all guides
2. **Check Examples** - Working code in `examples/` directory
3. **Review Migration Guide** - For upgrade issues
4. **Consult Troubleshooting** - Common problems and solutions
5. **Open GitHub Issue** - For bugs or questions
6. **Read Source Code** - Well-commented implementation

## 🔄 Contributing to Documentation

Documentation improvements are welcome! See [Contributing Guidelines](contributing.md) for:
- Improving existing documentation
- Adding new examples and tutorials
- Fixing errors and typos
- Translating documentation
- Adding multi-core examples

---

**Welcome to Pico-RTOS v0.3.1 - Advanced Synchronization & Multi-Core!** 🚀