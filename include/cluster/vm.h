/*
 * PureVisor - VM Management Header
 * 
 * Virtual machine lifecycle management
 */

#ifndef _PUREVISOR_CLUSTER_VM_H
#define _PUREVISOR_CLUSTER_VM_H

#include <lib/types.h>
#include <cluster/node.h>
#include <vmm/vcpu.h>

/* ============================================================================
 * VM Constants
 * ============================================================================ */

#define VM_MAX_NAME             64
#define VM_MAX_VCPUS            64
#define VM_MAX_DISKS            16
#define VM_MAX_NICS             8
#define VM_MAX_DESCRIPTION      256

/* VM states */
#define VM_STATE_CREATED        0
#define VM_STATE_STARTING       1
#define VM_STATE_RUNNING        2
#define VM_STATE_PAUSED         3
#define VM_STATE_STOPPING       4
#define VM_STATE_STOPPED        5
#define VM_STATE_MIGRATING      6
#define VM_STATE_ERROR          7

/* VM boot types */
#define VM_BOOT_BIOS            0
#define VM_BOOT_UEFI            1
#define VM_BOOT_DIRECT          2   /* Direct kernel boot */

/* Disk types */
#define VM_DISK_VIRTIO          0
#define VM_DISK_IDE             1
#define VM_DISK_SATA            2
#define VM_DISK_NVME            3

/* NIC types */
#define VM_NIC_VIRTIO           0
#define VM_NIC_E1000            1
#define VM_NIC_RTL8139          2

/* ============================================================================
 * VM Configuration
 * ============================================================================ */

typedef struct vm_disk_config {
    char name[VM_MAX_NAME];
    char path[128];             /* Volume path or file */
    uint64_t size;
    uint32_t type;
    bool readonly;
    bool bootable;
    uint32_t boot_order;
} vm_disk_config_t;

typedef struct vm_nic_config {
    char name[VM_MAX_NAME];
    uint8_t mac[6];
    char network[VM_MAX_NAME];  /* Network/bridge name */
    uint32_t type;
    uint32_t vlan;
    bool enabled;
} vm_nic_config_t;

typedef struct vm_config {
    /* Basic */
    char name[VM_MAX_NAME];
    char description[VM_MAX_DESCRIPTION];
    
    /* CPU */
    uint32_t vcpus;
    uint32_t sockets;
    uint32_t cores;
    uint32_t threads;
    char cpu_model[64];
    
    /* Memory */
    uint64_t memory;            /* Bytes */
    uint64_t max_memory;
    bool hugepages;
    
    /* Boot */
    uint32_t boot_type;
    char kernel_path[128];      /* For direct boot */
    char initrd_path[128];
    char cmdline[256];
    
    /* Devices */
    vm_disk_config_t disks[VM_MAX_DISKS];
    uint32_t disk_count;
    
    vm_nic_config_t nics[VM_MAX_NICS];
    uint32_t nic_count;
    
    /* Features */
    bool nested_virt;
    bool enable_kvm;
    bool autostart;
    
    /* Placement */
    char preferred_node[NODE_MAX_NAME];
    char required_tags[NODE_MAX_TAGS][NODE_TAG_MAX_LEN];
    uint32_t required_tag_count;
} vm_config_t;

/* ============================================================================
 * VM Statistics
 * ============================================================================ */

typedef struct vm_stats {
    /* CPU */
    uint64_t cpu_time_ns;
    uint32_t cpu_percent;
    
    /* Memory */
    uint64_t memory_used;
    uint64_t memory_peak;
    uint64_t swap_used;
    
    /* I/O */
    uint64_t disk_read_bytes;
    uint64_t disk_write_bytes;
    uint64_t disk_read_ops;
    uint64_t disk_write_ops;
    
    /* Network */
    uint64_t net_rx_bytes;
    uint64_t net_tx_bytes;
    uint64_t net_rx_packets;
    uint64_t net_tx_packets;
    
    /* VMX */
    uint64_t vmexit_count;
    uint64_t vmentry_count;
} vm_stats_t;

/* ============================================================================
 * Virtual Machine
 * ============================================================================ */

typedef struct virtual_machine {
    /* Identity */
    uint32_t id;
    char uuid[BLOCK_MAX_UUID];
    
    /* Configuration */
    vm_config_t config;
    
    /* State */
    uint32_t state;
    uint64_t created_time;
    uint64_t started_time;
    uint64_t stopped_time;
    
    /* VCPUs */
    vcpu_t *vcpus[VM_MAX_VCPUS];
    uint32_t vcpu_count;
    
    /* Host node */
    cluster_node_t *host_node;
    uint32_t host_node_id;
    
    /* Statistics */
    vm_stats_t stats;
    
    /* Error info */
    char error_msg[128];
    int error_code;
    
    /* List linkage */
    struct virtual_machine *next;
} virtual_machine_t;

/* ============================================================================
 * VM Manager
 * ============================================================================ */

typedef struct vm_manager {
    virtual_machine_t *vms;
    uint32_t vm_count;
    uint32_t running_count;
    
    /* Local node reference */
    cluster_node_t *local_node;
    
    /* ID allocation */
    uint32_t next_vm_id;
    
    /* Callbacks */
    void (*on_vm_state_change)(struct vm_manager *mgr, virtual_machine_t *vm,
                                uint32_t old_state, uint32_t new_state);
} vm_manager_t;

/* ============================================================================
 * VM API
 * ============================================================================ */

/**
 * vm_manager_init - Initialize VM manager
 */
int vm_manager_init(vm_manager_t *mgr, cluster_node_t *local_node);

/**
 * virt_vm_create - Create a new VM
 * @mgr: VM manager
 * @config: VM configuration
 */
virtual_machine_t *virt_vm_create(vm_manager_t *mgr, vm_config_t *config);

/**
 * virt_vm_destroy - Destroy a VM
 */
void virt_vm_destroy(vm_manager_t *mgr, virtual_machine_t *vm);

/**
 * virt_vm_start - Start a VM
 */
int virt_vm_start(vm_manager_t *mgr, virtual_machine_t *vm);

/**
 * virt_vm_stop - Stop a VM (graceful)
 */
int virt_vm_stop(vm_manager_t *mgr, virtual_machine_t *vm);

/**
 * virt_vm_force_stop - Force stop a VM
 */
int virt_vm_force_stop(vm_manager_t *mgr, virtual_machine_t *vm);

/**
 * virt_vm_pause - Pause a VM
 */
int virt_vm_pause(vm_manager_t *mgr, virtual_machine_t *vm);

/**
 * virt_vm_resume - Resume a paused VM
 */
int virt_vm_resume(vm_manager_t *mgr, virtual_machine_t *vm);

/**
 * virt_vm_restart - Restart a VM
 */
int virt_vm_restart(vm_manager_t *mgr, virtual_machine_t *vm);

/**
 * virt_vm_find - Find VM by ID
 */
virtual_machine_t *virt_vm_find(vm_manager_t *mgr, uint32_t id);

/**
 * virt_vm_find_by_name - Find VM by name
 */
virtual_machine_t *virt_vm_find_by_name(vm_manager_t *mgr, const char *name);

/**
 * vm_get_state_string - Get state as string
 */
const char *vm_get_state_string(uint32_t state);

/**
 * virt_vm_update_stats - Update VM statistics
 */
void virt_vm_update_stats(virtual_machine_t *vm);

/* ============================================================================
 * Migration API
 * ============================================================================ */

/**
 * virt_vm_migrate - Live migrate VM to another node
 * @vm: VM to migrate
 * @target_node: Destination node
 */
int virt_vm_migrate(vm_manager_t *mgr, virtual_machine_t *vm, 
               cluster_node_t *target_node);

/**
 * virt_vm_can_migrate - Check if VM can be migrated
 */
bool virt_vm_can_migrate(virtual_machine_t *vm);

#endif /* _PUREVISOR_CLUSTER_VM_H */
