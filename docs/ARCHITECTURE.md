# PureVisor Architecture Document

## 1. Project Overview

**PureVisor**는 외부 의존성이 전혀 없는(Zero-Dependency) Type-1 Bare-Metal Hypervisor 기반의 
Hyper-Converged Infrastructure (HCI) 엔진입니다.

### 1.1 핵심 목표
- **VM Execution**: Intel VT-x (VMX) / AMD-V (SVM) 직접 활용
- **Distributed Block Storage**: 소프트웨어 정의 분산 스토리지
- **Socket-based Networking**: Raw 소켓/이더넷 프레임 직접 처리

### 1.2 핵심 가치
| Category | Value | Description |
|----------|-------|-------------|
| Infra | Bare-Metal Performance | VMX/SVM 직접 활용, OS 오버헤드 제거 |
| Dev | Low-Level Mastery | 커스텀 메모리 할당자, raw 소켓, 인라인 어셈블리 |

### 1.3 제약 사항
- **No libc**: 필요 시 musl-like 최소 구현
- **No Rust/C++**: Pure C only
- **Target**: x86_64, UEFI Boot, Multi-core Support

---

## 2. System Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                        Management Layer                              │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐                  │
│  │ CLI Console │  │ HTTP Server │  │ Cluster Mgr │                  │
│  └─────────────┘  └─────────────┘  └─────────────┘                  │
├─────────────────────────────────────────────────────────────────────┤
│                         VM Layer                                     │
│  ┌─────────────────────────────────────────────────────────────┐    │
│  │                    Virtual Machines                          │    │
│  │  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐         │    │
│  │  │  VM 1   │  │  VM 2   │  │  VM 3   │  │  VM N   │         │    │
│  │  │(Guest)  │  │(Guest)  │  │(Guest)  │  │(Guest)  │         │    │
│  │  └────┬────┘  └────┬────┘  └────┬────┘  └────┬────┘         │    │
│  └───────┼────────────┼────────────┼────────────┼──────────────┘    │
├──────────┼────────────┼────────────┼────────────┼───────────────────┤
│          │    Hypervisor Core (VMM)│            │                    │
│  ┌───────┴────────────┴────────────┴────────────┴──────────────┐    │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐     │    │
│  │  │ VMCS/   │  │ EPT/NPT  │  │ VM Exit  │  │ Device   │     │    │
│  │  │ VMCB    │  │ Manager  │  │ Handler  │  │ Emulation│     │    │
│  │  └──────────┘  └──────────┘  └──────────┘  └──────────┘     │    │
│  └─────────────────────────────────────────────────────────────┘    │
├─────────────────────────────────────────────────────────────────────┤
│                      I/O Virtualization                              │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐                  │
│  │ Virtio-Net │  │ Virtio-Blk │  │ Virtio-SCSI │                  │
│  └─────────────┘  └─────────────┘  └─────────────┘                  │
├─────────────────────────────────────────────────────────────────────┤
│                    Distributed Storage                               │
│  ┌─────────────────────────────────────────────────────────────┐    │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐     │    │
│  │  │ Block   │  │ Replica  │  │ Stripe   │  │ Metadata │     │    │
│  │  │ Device  │  │ Manager  │  │ Manager  │  │ Store    │     │    │
│  │  └──────────┘  └──────────┘  └──────────┘  └──────────┘     │    │
│  └─────────────────────────────────────────────────────────────┘    │
├─────────────────────────────────────────────────────────────────────┤
│                       Network Stack                                  │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐                  │
│  │ Raw Socket │  │ Ethernet   │  │ Cluster    │                  │
│  │ Handler    │  │ Frame Proc │  │ Protocol   │                  │
│  └─────────────┘  └─────────────┘  └─────────────┘                  │
├─────────────────────────────────────────────────────────────────────┤
│                        Kernel Core                                   │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐            │
│  │ Memory  │  │ Interrupt│  │ SMP      │  │ Scheduler│            │
│  │ Manager │  │ Handler  │  │ Support  │  │          │            │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘            │
├─────────────────────────────────────────────────────────────────────┤
│                         Drivers                                      │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐            │
│  │ NVMe    │  │ AHCI/SATA│  │ e1000    │  │ PCI      │            │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘            │
├─────────────────────────────────────────────────────────────────────┤
│                      Boot & Platform                                 │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐            │
│  │ UEFI    │  │ ACPI     │  │ PML4     │  │ APIC     │            │
│  │ Boot    │  │ Parser   │  │ Paging   │  │ Timer    │            │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘            │
└─────────────────────────────────────────────────────────────────────┘
                              Hardware
        (Intel/AMD x86_64 CPU, NVMe/SATA, NIC, Memory)
```

---

## 3. Directory Structure

```
purevisor/
├── src/
│   ├── boot/           # UEFI boot loader, early init
│   ├── kernel/         # Core kernel (scheduler, IDT, APIC)
│   ├── mm/             # Memory management (buddy, slab, paging)
│   ├── vmm/            # Hypervisor (VMX/SVM, VMCS, EPT)
│   ├── drivers/        # Device drivers (NVMe, AHCI, PCI, NIC)
│   ├── net/            # Network stack (raw socket, protocol)
│   ├── storage/        # Distributed storage engine
│   └── lib/            # Mini libc replacement
├── include/
│   ├── arch/x86_64/    # x86_64 specific headers
│   ├── kernel/         # Kernel headers
│   ├── vmm/            # Hypervisor headers
│   ├── drivers/        # Driver headers
│   ├── net/            # Network headers
│   ├── storage/        # Storage headers
│   └── lib/            # Library headers
├── scripts/            # Build scripts, QEMU launcher
├── docs/               # Documentation
└── tests/              # Test cases
```

---

## 4. Memory Map (x86_64)

```
Virtual Address Space Layout:
┌────────────────────────────────────────┐ 0xFFFF_FFFF_FFFF_FFFF
│              Kernel Space              │
│  ┌──────────────────────────────────┐  │ 0xFFFF_8000_0000_0000
│  │ Direct Physical Mapping          │  │ (Higher Half Kernel)
│  │ (Identity map of all RAM)        │  │
│  ├──────────────────────────────────┤  │ 
│  │ Kernel Heap                      │  │
│  ├──────────────────────────────────┤  │
│  │ Kernel Stack (per CPU)           │  │
│  ├──────────────────────────────────┤  │
│  │ MMIO Regions                     │  │
│  ├──────────────────────────────────┤  │
│  │ VMM Structures (VMCS, EPT)       │  │
│  └──────────────────────────────────┘  │
├────────────────────────────────────────┤ 0x0000_7FFF_FFFF_FFFF
│              Canonical Hole            │ (Non-canonical addresses)
├────────────────────────────────────────┤ 0x0000_0000_0000_0000
│              User Space                │ (Guest Virtual Addresses)
│  (Used by VMs via EPT/NPT)            │
└────────────────────────────────────────┘
```

---

## 5. Boot Sequence

```
1. UEFI Firmware
   │
   ▼
2. PureVisor UEFI Application (boot.efi)
   ├─ Get Memory Map from UEFI
   ├─ Setup Page Tables (PML4)
   ├─ Load Kernel to Higher Half
   │
   ▼
3. Exit Boot Services
   │
   ▼
4. Kernel Entry (Long Mode, 64-bit)
   ├─ Initialize GDT/IDT
   ├─ Initialize APIC
   ├─ Initialize Memory Manager
   │
   ▼
5. SMP Initialization
   ├─ Detect CPU Cores
   ├─ Start Application Processors (APs)
   │
   ▼
6. VMM Initialization
   ├─ Check VMX/SVM Support
   ├─ Enable VMX/SVM
   ├─ Setup VMCS/VMCB Templates
   │
   ▼
7. Device Initialization
   ├─ PCI Enumeration
   ├─ NIC Driver (e1000/virtio)
   ├─ Storage Driver (NVMe/AHCI)
   │
   ▼
8. Cluster Formation
   ├─ Node Discovery
   ├─ Storage Pool Setup
   │
   ▼
9. Ready for VM Creation
```

---

## 6. Phase Development Plan

| Phase | Focus | Key Deliverables |
|-------|-------|------------------|
| 0 | Foundation | Build system, QEMU test env, Hello World kernel |
| 1 | Bare-Metal Boot | UEFI boot, paging, memory manager, IDT, SMP |
| 2 | Hypervisor Core | VMX/SVM, VMCS/VMCB, EPT, VM Exit, Guest boot |
| 3 | I/O Virtualization | Virtio-net, Virtio-blk, raw socket, NIC driver |
| 4 | Distributed Storage | Block device, replica, striping, failover |
| 5 | Clustering | Node discovery, metadata, management UI |
| 6 | Optimization | Zero-copy, lock-free, IOMMU, testing |

---

## 7. Key Data Structures

### 7.1 CPU Context
```c
struct cpu_context {
    uint64_t rax, rbx, rcx, rdx;
    uint64_t rsi, rdi, rbp, rsp;
    uint64_t r8, r9, r10, r11;
    uint64_t r12, r13, r14, r15;
    uint64_t rip, rflags;
    uint64_t cr0, cr2, cr3, cr4;
    // Segment registers, etc.
};
```

### 7.2 Virtual Machine
```c
struct vm {
    uint32_t id;
    enum vm_state state;
    struct vmcs *vmcs;          // Intel VMX
    struct vmcb *vmcb;          // AMD SVM
    struct ept_context *ept;    // Extended Page Tables
    struct vcpu *vcpus;
    struct virtio_device *devices;
    // Memory, I/O, etc.
};
```

### 7.3 Storage Block
```c
struct block_device {
    uint64_t size;
    uint32_t block_size;
    int (*read)(struct block_device *, uint64_t lba, void *buf, size_t count);
    int (*write)(struct block_device *, uint64_t lba, const void *buf, size_t count);
};
```

---

## 8. References

- Intel 64 and IA-32 Architectures Software Developer's Manual
- AMD64 Architecture Programmer's Manual
- UEFI Specification
- OSDev Wiki (https://wiki.osdev.org)
- Virtio Specification
