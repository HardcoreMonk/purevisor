# PureVisor Build System
# Pure C, Zero-Dependency Type-1 Hypervisor

# ============================================================================
# Configuration
# ============================================================================

PROJECT     := purevisor
VERSION     := 0.1.0

# Directories
SRCDIR      := src
INCDIR      := include
BUILDDIR    := build
ISODIR      := $(BUILDDIR)/iso

# Output files
KERNEL      := $(BUILDDIR)/purevisor.elf
KERNEL_BIN  := $(BUILDDIR)/purevisor.bin

# ============================================================================
# Toolchain
# ============================================================================

CC          := gcc
LD          := ld
OBJCOPY     := objcopy
OBJDUMP     := objdump

# ============================================================================
# Flags
# ============================================================================

# C flags for freestanding kernel
CFLAGS := -std=c11 \
          -ffreestanding \
          -fno-stack-protector \
          -fno-pic \
          -fno-pie \
          -mno-red-zone \
          -mno-mmx \
          -mno-sse \
          -mno-sse2 \
          -mcmodel=large \
          -Wall \
          -Wextra \
          -Werror \
          -O2 \
          -g \
          -I$(INCDIR) \
          -m64

# Linker flags
LDFLAGS := -nostdlib \
           -z max-page-size=0x1000 \
           -T scripts/kernel.ld

# ============================================================================
# Source Files
# ============================================================================

# Assembly sources (.S files - GAS syntax)
ASM_SOURCES := $(SRCDIR)/boot/boot.S \
               $(SRCDIR)/boot/multiboot2.S \
               $(SRCDIR)/boot/idt.S

# C sources
C_SOURCES := $(SRCDIR)/lib/string.c \
             $(SRCDIR)/kernel/console.c \
             $(SRCDIR)/kernel/idt.c \
             $(SRCDIR)/kernel/apic.c \
             $(SRCDIR)/kernel/smp.c \
             $(SRCDIR)/kernel/main.c \
             $(SRCDIR)/mm/pmm.c \
             $(SRCDIR)/mm/paging.c \
             $(SRCDIR)/mm/heap.c \
             $(SRCDIR)/vmm/vmx.c \
             $(SRCDIR)/vmm/vcpu.c \
             $(SRCDIR)/vmm/ept.c \
             $(SRCDIR)/vmm/vmexit.c \
             $(SRCDIR)/pci/pci.c \
             $(SRCDIR)/virtio/virtio.c \
             $(SRCDIR)/virtio/virtio_blk.c \
             $(SRCDIR)/virtio/virtio_net.c \
             $(SRCDIR)/virtio/virtio_console.c \
             $(SRCDIR)/storage/block.c \
             $(SRCDIR)/storage/pool.c \
             $(SRCDIR)/storage/distributed.c \
             $(SRCDIR)/storage/memblk.c \
             $(SRCDIR)/cluster/node.c \
             $(SRCDIR)/cluster/vm.c \
             $(SRCDIR)/cluster/scheduler.c \
             $(SRCDIR)/mgmt/api.c \
             $(SRCDIR)/test/framework.c \
             $(SRCDIR)/test/benchmark.c \
             $(SRCDIR)/test/test_memory.c \
             $(SRCDIR)/test/test_vmx.c \
             $(SRCDIR)/test/test_storage.c \
             $(SRCDIR)/test/test_cluster.c \
             $(SRCDIR)/test/test_integration.c

# Assembly sources (.S files - GAS syntax)
ASM_SOURCES := $(SRCDIR)/boot/boot.S \
               $(SRCDIR)/boot/multiboot2.S \
               $(SRCDIR)/boot/idt.S \
               $(SRCDIR)/vmm/vmx_asm.S

# Object files
ASM_OBJECTS := $(patsubst $(SRCDIR)/%.S,$(BUILDDIR)/%.o,$(ASM_SOURCES))
C_OBJECTS   := $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(C_SOURCES))
OBJECTS     := $(ASM_OBJECTS) $(C_OBJECTS)

# ============================================================================
# Targets
# ============================================================================

.PHONY: all clean run debug help dirs

all: dirs $(KERNEL)
	@echo ""
	@echo "=== Build Complete ==="
	@echo "Kernel ELF: $(KERNEL)"
	@size $(KERNEL) 2>/dev/null || true

dirs:
	@mkdir -p $(BUILDDIR)/boot
	@mkdir -p $(BUILDDIR)/kernel
	@mkdir -p $(BUILDDIR)/lib
	@mkdir -p $(BUILDDIR)/mm
	@mkdir -p $(BUILDDIR)/vmm
	@mkdir -p $(BUILDDIR)/pci
	@mkdir -p $(BUILDDIR)/virtio
	@mkdir -p $(BUILDDIR)/storage
	@mkdir -p $(BUILDDIR)/cluster
	@mkdir -p $(BUILDDIR)/mgmt
	@mkdir -p $(BUILDDIR)/test

$(KERNEL): $(OBJECTS)
	@echo "[LD] Linking $@"
	$(LD) $(LDFLAGS) -o $@ $^

# Assembly files
$(BUILDDIR)/boot/boot.o: $(SRCDIR)/boot/boot.S
	@echo "[AS] $<"
	@mkdir -p $(dir $@)
	$(CC) -c -g -m64 -o $@ $<

$(BUILDDIR)/boot/multiboot2.o: $(SRCDIR)/boot/multiboot2.S
	@echo "[AS] $<"
	@mkdir -p $(dir $@)
	$(CC) -c -g -m64 -o $@ $<

$(BUILDDIR)/boot/idt.o: $(SRCDIR)/boot/idt.S
	@echo "[AS] $<"
	@mkdir -p $(dir $@)
	$(CC) -c -g -m64 -o $@ $<

$(BUILDDIR)/vmm/vmx_asm.o: $(SRCDIR)/vmm/vmx_asm.S
	@echo "[AS] $<"
	@mkdir -p $(dir $@)
	$(CC) -c -g -m64 -o $@ $<

# C files
$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	@echo "[CC] $<"
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

# ============================================================================
# QEMU Targets
# ============================================================================

QEMU := qemu-system-x86_64
QEMU_OPTS := -m 512M -cpu max -smp 4 -serial stdio -no-reboot -no-shutdown

run: $(KERNEL)
	@echo "[QEMU] Running PureVisor..."
	$(QEMU) $(QEMU_OPTS) -kernel $(KERNEL)

debug: $(KERNEL)
	@echo "[QEMU] Debug mode - connect GDB to localhost:1234"
	$(QEMU) $(QEMU_OPTS) -kernel $(KERNEL) -s -S

# ============================================================================
# Utility Targets
# ============================================================================

disasm: $(KERNEL)
	$(OBJDUMP) -d -M intel $(KERNEL) > $(BUILDDIR)/purevisor.dis
	@echo "Disassembly: $(BUILDDIR)/purevisor.dis"

symbols: $(KERNEL)
	$(OBJDUMP) -t $(KERNEL) | sort

clean:
	@echo "[CLEAN] Removing build directory"
	rm -rf $(BUILDDIR)

help:
	@echo "PureVisor Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all      - Build kernel (default)"
	@echo "  run      - Run in QEMU"
	@echo "  debug    - Run in QEMU with GDB"
	@echo "  disasm   - Disassemble kernel"
	@echo "  symbols  - Show symbols"
	@echo "  clean    - Clean build"
