# PureVisor

**Pure C Type-1 Bare-Metal Hypervisor & HCI Engine**

[![Version](https://img.shields.io/badge/version-0.1.0-blue.svg)]()
[![License](https://img.shields.io/badge/license-Proprietary-red.svg)]()

## Overview

PureVisor is a zero-dependency, Pure C implementation of a Type-1 (bare-metal) hypervisor with integrated Hyper-Converged Infrastructure (HCI) capabilities.

### Key Features

- **Zero External Dependencies**: No libc, no Rust, no C++ - Pure C only
- **Type-1 Hypervisor**: Runs directly on hardware (bare-metal)
- **Hardware Virtualization**: Intel VT-x (VMX) and AMD-V (SVM) support
- **Distributed Storage**: Software-defined block storage with replication
- **Raw Networking**: Direct Ethernet frame processing

## Requirements

### Build Requirements
- GCC (x86_64 cross-compiler recommended)
- GNU Make
- LD (GNU linker)

### Runtime Requirements
- x86_64 CPU with Intel VT-x or AMD-V
- UEFI or Multiboot2-compatible bootloader
- Minimum 512MB RAM

### Testing Requirements
- QEMU (qemu-system-x86_64) for emulation

## Quick Start

```bash
# Clone the repository
git clone <repo-url>
cd purevisor

# Build the kernel
make

# Run in QEMU (requires qemu-system-x86_64)
make run

# Debug with GDB
make debug
# In another terminal: gdb -ex "target remote :1234" build/purevisor.elf
```

## Project Structure

```
purevisor/
├── src/
│   ├── boot/           # Boot code (32→64 bit transition)
│   ├── kernel/         # Core kernel (IDT, console, main)
│   ├── mm/             # Memory management (TODO)
│   ├── vmm/            # Hypervisor (VMX/SVM) (TODO)
│   ├── drivers/        # Device drivers (TODO)
│   ├── net/            # Network stack (TODO)
│   ├── storage/        # Distributed storage (TODO)
│   └── lib/            # Mini libc replacement
├── include/
│   ├── arch/x86_64/    # Architecture-specific headers
│   ├── kernel/         # Kernel headers
│   └── lib/            # Library headers
├── scripts/            # Build scripts, linker scripts
├── docs/               # Documentation
├── build/              # Build output (generated)
└── Makefile            # Build system
```

## Development Phases

| Phase | Status | Description |
|-------|--------|-------------|
| 0 | ✅ Complete | Foundation - Build system, basic boot |
| 1 | ✅ Complete | Bare-metal boot, memory manager, APIC, SMP |
| 2 | ✅ Complete | Hypervisor core (VMX) |
| 3 | ✅ Complete | I/O virtualization (Virtio) |
| 4 | ✅ Complete | Distributed storage |
| 5 | ✅ Complete | Clustering & management |
| 6 | ✅ Complete | Optimization & testing |

## Phase 6 Accomplishments

- ✅ Test framework with assertions
- ✅ Unit test suites (PMM, Heap, Paging, VMX, Storage, Cluster)
- ✅ Integration tests
- ✅ Performance benchmarks (Memory, CPU, Storage)
- ✅ TODO items resolved
- ✅ Code optimization

## Build Targets

```bash
make           # Build kernel
make run       # Run in QEMU
make debug     # Run with GDB server
make disasm    # Generate disassembly
make symbols   # Show symbol table
make clean     # Clean build artifacts
make help      # Show all targets
```

## Architecture

### Memory Layout

```
Virtual Address Space (x86_64):
┌────────────────────────────────────┐ 0xFFFF_FFFF_FFFF_FFFF
│         Kernel Space               │
│  ┌────────────────────────────┐    │ 0xFFFF_8000_0000_0000
│  │ Direct Physical Mapping    │    │ (Higher Half)
│  │ Kernel Code/Data           │    │
│  │ Page Tables (PML4)         │    │
│  └────────────────────────────┘    │
├────────────────────────────────────┤
│        Canonical Hole              │
├────────────────────────────────────┤ 0x0000_7FFF_FFFF_FFFF
│         User Space                 │
│   (Guest virtual addresses)        │
└────────────────────────────────────┘ 0x0000_0000_0000_0000
```

### Boot Sequence

1. BIOS/UEFI → Bootloader (GRUB2/QEMU)
2. Multiboot2 header recognized
3. Kernel loaded at 1MB physical
4. Entry in 32-bit protected mode
5. Setup paging (PML4)
6. Enable long mode (64-bit)
7. Jump to kernel_main()
8. Initialize IDT, console
9. Detect CPU features
10. Parse memory map
11. Enter idle loop

## Contributing

This is a proprietary project. Contact the maintainer for contribution guidelines.

## License

Proprietary - All rights reserved

## References

- [Intel 64 and IA-32 Architectures Software Developer's Manual](https://www.intel.com/sdm)
- [AMD64 Architecture Programmer's Manual](https://developer.amd.com/resources/developer-guides-manuals/)
- [OSDev Wiki](https://wiki.osdev.org)
- [Multiboot2 Specification](https://www.gnu.org/software/grub/manual/multiboot2/)
