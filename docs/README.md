# Pico-RTOS v0.3.1 Documentation

<div align="center">

**Real-Time Operating System for Raspberry Pi Pico**

[![Version](https://img.shields.io/badge/version-0.3.1-blue.svg)](../CHANGELOG.md)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](../LICENSE)
[![Status](https://img.shields.io/badge/status-production%20ready-brightgreen.svg)]()

</div>

---

## 📋 Documentation Map

```mermaid
graph TD
    subgraph Quickstart["🚀 Quickstart"]
        GS[Getting Started]
        FT[Flashing & Testing]
    end
    
    subgraph Core["📖 Core Guides"]
        UG[User Guide]
        API[API Reference]
        EC[Error Codes]
    end
    
    subgraph Advanced["⚡ Advanced"]
        MC[Multi-Core SMP]
        PG[Performance]
        LG[Logging]
        MG[Migration]
    end
    
    subgraph Reference["🔧 Reference"]
        TS[Troubleshooting]
        CFG[Menuconfig]
        CTB[Contributing]
    end
    
    GS --> UG
    UG --> API
    UG --> MC
    MC --> PG
    API --> EC
    TS --> EC
```

---

## 🏗️ Architecture Overview

```mermaid
graph TB
    subgraph Application["Application Layer"]
        APP[User Application]
    end
    
    subgraph RTOS["Pico-RTOS Kernel"]
        subgraph Sync["Synchronization"]
            MUT[Mutexes]
            SEM[Semaphores]
            EVT[Event Groups]
            QUE[Queues]
            STR[Stream Buffers]
        end
        
        subgraph Core["Core Services"]
            SCHED[Scheduler]
            TASK[Task Manager]
            TMR[Timer Service]
            MEM[Memory Manager]
        end
        
        subgraph SMP["Multi-Core (v0.3.1)"]
            LB[Load Balancer]
            IPC[IPC Channels]
            AFF[Core Affinity]
        end
        
        subgraph Debug["Debug & Monitoring"]
            PROF[Profiler]
            TRACE[Tracer]
            HEALTH[Health Monitor]
        end
    end
    
    subgraph HAL["Hardware Abstraction"]
        CTX[Context Switch]
        IRQ[Interrupts]
        MPU[Memory Protection]
        WDG[Watchdog]
    end
    
    subgraph HW["RP2040 Hardware"]
        C0[Core 0]
        C1[Core 1]
        SRAM[264KB SRAM]
        PERIPH[Peripherals]
    end
    
    APP --> RTOS
    RTOS --> HAL
    HAL --> HW
```

---

## 🎯 Choose Your Path

| If you want to... | Start here |
|-------------------|------------|
| **Get started quickly** | [Getting Started Guide](getting_started.md) |
| **Flash firmware to Pico** | [Flashing & Testing Guide](flashing_and_testing.md) |
| **Learn RTOS concepts** | [User Guide](user_guide.md) |
| **Look up API functions** | [API Reference](api_reference.md) |
| **Use multi-core features** | [Multi-Core Guide](multicore.md) |
| **Optimize performance** | [Performance Guide](performance_guide.md) |
| **Upgrade from v0.2.x** | [Migration Guide](migration_guide.md) |
| **Debug an issue** | [Troubleshooting](troubleshooting.md) |
| **Contribute to project** | [Contributing Guidelines](contributing.md) |

---

## 📚 Complete Documentation

### 🚀 Getting Started
| Document | Description |
|----------|-------------|
| [**Getting Started**](getting_started.md) | Installation, setup, and first project |
| [**Flashing & Testing**](flashing_and_testing.md) | Hardware workflow and serial monitoring |

### 📖 Core Guides
| Document | Description |
|----------|-------------|
| [**User Guide**](user_guide.md) | Comprehensive RTOS concepts and usage |
| [**API Reference**](api_reference.md) | Complete function documentation |
| [**Error Codes**](error_codes.md) | Error catalog with solutions |

### ⚡ Advanced Topics
| Document | Description |
|----------|-------------|
| [**Multi-Core Guide**](multicore.md) | SMP, load balancing, IPC |
| [**Performance Guide**](performance_guide.md) | Optimization and profiling |
| [**Logging Guide**](logging_guide.md) | Debug logging system |
| [**Migration Guide**](migration_guide.md) | Upgrading from previous versions |

### 🔧 Reference
| Document | Description |
|----------|-------------|
| [**Troubleshooting**](troubleshooting.md) | Common issues and solutions |
| [**Menuconfig Guide**](menuconfig_guide.md) | Configuration system |
| [**Contributing**](contributing.md) | Development guidelines |

---

## ✨ v0.3.1 Feature Highlights

<table>
<tr>
<td width="50%">

### 🔄 Advanced Synchronization
- **Event Groups** — 32-bit event coordination
- **Stream Buffers** — Zero-copy message passing
- **Enhanced Mutexes** — Priority inheritance

</td>
<td width="50%">

### 🖥️ Multi-Core Support
- **SMP Scheduler** — Dual-core on RP2040
- **Load Balancing** — Automatic distribution
- **Core Affinity** — Task pinning

</td>
</tr>
<tr>
<td>

### 🧠 Memory Management
- **Memory Pools** — O(1) allocation
- **MPU Support** — Hardware protection
- **Advanced Stats** — Leak detection

</td>
<td>

### 🔍 Debugging Tools
- **Task Inspection** — Runtime monitoring
- **Profiler** — Execution analysis
- **Health Monitor** — System metrics

</td>
</tr>
</table>

---

## 📊 Quick Reference

### System Limits
| Resource | Default | Range |
|----------|---------|-------|
| Max Tasks | 16 | 1-64 |
| Max Timers | 8 | 1-32 |
| Tick Rate | 1000 Hz | 100-2000 Hz |
| Min Stack | 128 bytes | — |

### Performance Metrics (RP2040 @ 125MHz)
| Operation | Cycles | Time |
|-----------|--------|------|
| Context Switch | ~65 | 0.52 μs |
| Mutex Lock/Unlock | ~28 | 0.22 μs |
| Queue Send/Receive | ~32 | 0.26 μs |
| Memory Pool Alloc | ~18 | 0.14 μs |

---

## 🆘 Getting Help

1. **Search docs** — Use browser find (Ctrl/Cmd+F)
2. **Check examples** — See `examples/` directory
3. **Review troubleshooting** — [Troubleshooting Guide](troubleshooting.md)
4. **Open issue** — [GitHub Issues](https://github.com/muditbhargava66/pico-rtos/issues)

---

<div align="center">

**Pico-RTOS v0.3.1** — *Advanced Synchronization & Multi-Core*

[GitHub](https://github.com/muditbhargava66/pico-rtos) · 
[Issues](https://github.com/muditbhargava66/pico-rtos/issues) · 
[Changelog](../CHANGELOG.md)

</div>