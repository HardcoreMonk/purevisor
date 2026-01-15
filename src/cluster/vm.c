/*
 * PureVisor - VM Management Implementation
 * 
 * Virtual machine lifecycle management
 */

#include <lib/types.h>
#include <lib/string.h>
#include <cluster/vm.h>
#include <mm/heap.h>
#include <kernel/console.h>
#include <arch/x86_64/cpu.h>

/* ============================================================================
 * State Strings
 * ============================================================================ */

static const char *vm_state_strings[] = {
    [VM_STATE_CREATED]    = "CREATED",
    [VM_STATE_STARTING]   = "STARTING",
    [VM_STATE_RUNNING]    = "RUNNING",
    [VM_STATE_PAUSED]     = "PAUSED",
    [VM_STATE_STOPPING]   = "STOPPING",
    [VM_STATE_STOPPED]    = "STOPPED",
    [VM_STATE_MIGRATING]  = "MIGRATING",
    [VM_STATE_ERROR]      = "ERROR",
};

const char *vm_get_state_string(uint32_t state)
{
    if (state <= VM_STATE_ERROR) {
        return vm_state_strings[state];
    }
    return "INVALID";
}

/* ============================================================================
 * VM Manager
 * ============================================================================ */

int vm_manager_init(vm_manager_t *mgr, cluster_node_t *local_node)
{
    if (!mgr) return -1;
    
    memset(mgr, 0, sizeof(*mgr));
    mgr->local_node = local_node;
    mgr->next_vm_id = 1;
    
    pr_info("VM Manager: Initialized");
    return 0;
}

/* ============================================================================
 * VM Lifecycle
 * ============================================================================ */

static void vm_set_state(vm_manager_t *mgr, virtual_machine_t *vm, uint32_t state)
{
    uint32_t old_state = vm->state;
    vm->state = state;
    
    pr_info("VM '%s': State changed: %s -> %s",
            vm->config.name,
            vm_get_state_string(old_state),
            vm_get_state_string(state));
    
    if (mgr->on_vm_state_change) {
        mgr->on_vm_state_change(mgr, vm, old_state, state);
    }
}

virtual_machine_t *virt_vm_create(vm_manager_t *mgr, vm_config_t *config)
{
    if (!mgr || !config) return NULL;
    
    virtual_machine_t *vm = kmalloc(sizeof(virtual_machine_t), GFP_KERNEL | GFP_ZERO);
    if (!vm) return NULL;
    
    vm->id = mgr->next_vm_id++;
    block_generate_uuid(vm->uuid);
    
    /* Copy configuration */
    memcpy(&vm->config, config, sizeof(vm_config_t));
    
    vm->state = VM_STATE_CREATED;
    vm->created_time = rdtsc();
    vm->host_node = mgr->local_node;
    if (mgr->local_node) {
        vm->host_node_id = mgr->local_node->id;
    }
    
    /* Add to manager */
    vm->next = mgr->vms;
    mgr->vms = vm;
    mgr->vm_count++;
    
    pr_info("VM: Created '%s' (ID=%u, UUID=%s)",
            config->name, vm->id, vm->uuid);
    pr_info("    vCPUs: %u, Memory: %llu MB",
            config->vcpus, config->memory / MB);
    
    return vm;
}

void virt_vm_destroy(vm_manager_t *mgr, virtual_machine_t *vm)
{
    if (!mgr || !vm) return;
    
    /* Must be stopped first */
    if (vm->state == VM_STATE_RUNNING || vm->state == VM_STATE_PAUSED) {
        virt_vm_force_stop(mgr, vm);
    }
    
    /* Remove from manager */
    virtual_machine_t **pp = &mgr->vms;
    while (*pp) {
        if (*pp == vm) {
            *pp = vm->next;
            mgr->vm_count--;
            break;
        }
        pp = &(*pp)->next;
    }
    
    pr_info("VM: Destroyed '%s'", vm->config.name);
    kfree(vm);
}

int virt_vm_start(vm_manager_t *mgr, virtual_machine_t *vm)
{
    if (!mgr || !vm) return -1;
    
    if (vm->state != VM_STATE_CREATED && vm->state != VM_STATE_STOPPED) {
        strncpy(vm->error_msg, "VM not in startable state", 127);
        return -1;
    }
    
    vm_set_state(mgr, vm, VM_STATE_STARTING);
    
    /* In a real implementation, we would:
     * 1. Allocate memory for the VM
     * 2. Create VCPUs
     * 3. Setup EPT mappings
     * 4. Initialize devices
     * 5. Load boot image
     */
    
    vm->started_time = rdtsc();
    vm_set_state(mgr, vm, VM_STATE_RUNNING);
    mgr->running_count++;
    
    if (mgr->local_node) {
        mgr->local_node->vm_count++;
    }
    
    return 0;
}

int virt_vm_stop(vm_manager_t *mgr, virtual_machine_t *vm)
{
    if (!mgr || !vm) return -1;
    
    if (vm->state != VM_STATE_RUNNING && vm->state != VM_STATE_PAUSED) {
        return -1;
    }
    
    vm_set_state(mgr, vm, VM_STATE_STOPPING);
    
    /* Send ACPI shutdown to guest */
    /* For now, just force stop */
    
    vm->stopped_time = rdtsc();
    vm_set_state(mgr, vm, VM_STATE_STOPPED);
    mgr->running_count--;
    
    if (mgr->local_node) {
        mgr->local_node->vm_count--;
    }
    
    return 0;
}

int virt_vm_force_stop(vm_manager_t *mgr, virtual_machine_t *vm)
{
    if (!mgr || !vm) return -1;
    
    if (vm->state == VM_STATE_STOPPED || vm->state == VM_STATE_CREATED) {
        return 0;
    }
    
    /* Force immediate stop */
    vm->stopped_time = rdtsc();
    
    if (vm->state == VM_STATE_RUNNING || vm->state == VM_STATE_PAUSED) {
        mgr->running_count--;
        if (mgr->local_node) {
            mgr->local_node->vm_count--;
        }
    }
    
    vm_set_state(mgr, vm, VM_STATE_STOPPED);
    
    return 0;
}

int virt_vm_pause(vm_manager_t *mgr, virtual_machine_t *vm)
{
    if (!mgr || !vm) return -1;
    
    if (vm->state != VM_STATE_RUNNING) {
        return -1;
    }
    
    vm_set_state(mgr, vm, VM_STATE_PAUSED);
    return 0;
}

int virt_vm_resume(vm_manager_t *mgr, virtual_machine_t *vm)
{
    if (!mgr || !vm) return -1;
    
    if (vm->state != VM_STATE_PAUSED) {
        return -1;
    }
    
    vm_set_state(mgr, vm, VM_STATE_RUNNING);
    return 0;
}

int virt_vm_restart(vm_manager_t *mgr, virtual_machine_t *vm)
{
    if (!mgr || !vm) return -1;
    
    int ret = virt_vm_stop(mgr, vm);
    if (ret != 0) {
        virt_vm_force_stop(mgr, vm);
    }
    
    return virt_vm_start(mgr, vm);
}

virtual_machine_t *virt_vm_find(vm_manager_t *mgr, uint32_t id)
{
    if (!mgr) return NULL;
    
    virtual_machine_t *vm = mgr->vms;
    while (vm) {
        if (vm->id == id) return vm;
        vm = vm->next;
    }
    return NULL;
}

virtual_machine_t *virt_vm_find_by_name(vm_manager_t *mgr, const char *name)
{
    if (!mgr || !name) return NULL;
    
    virtual_machine_t *vm = mgr->vms;
    while (vm) {
        if (strcmp(vm->config.name, name) == 0) return vm;
        vm = vm->next;
    }
    return NULL;
}

void virt_vm_update_stats(virtual_machine_t *vm)
{
    if (!vm || vm->state != VM_STATE_RUNNING) return;
    
    /* In a real implementation, we would gather:
     * - CPU time from VCPU structures
     * - Memory usage from EPT tracking
     * - I/O stats from virtio devices
     */
    
    vm->stats.cpu_time_ns += 1000000;  /* Placeholder */
}

/* ============================================================================
 * Migration
 * ============================================================================ */

bool virt_vm_can_migrate(virtual_machine_t *vm)
{
    if (!vm) return false;
    
    /* Can only migrate running or paused VMs */
    if (vm->state != VM_STATE_RUNNING && vm->state != VM_STATE_PAUSED) {
        return false;
    }
    
    return true;
}

int virt_vm_migrate(vm_manager_t *mgr, virtual_machine_t *vm,
               cluster_node_t *target_node)
{
    if (!mgr || !vm || !target_node) return -1;
    
    if (!virt_vm_can_migrate(vm)) {
        strncpy(vm->error_msg, "VM cannot be migrated", 127);
        return -1;
    }
    
    if (vm->host_node == target_node) {
        return 0;  /* Already on target */
    }
    
    uint32_t prev_state = vm->state;
    vm_set_state(mgr, vm, VM_STATE_MIGRATING);
    
    pr_info("VM '%s': Migrating from '%s' to '%s'",
            vm->config.name,
            vm->host_node ? vm->host_node->name : "local",
            target_node->name);
    
    /* Migration steps (simplified):
     * 1. Pre-copy: iteratively copy dirty memory pages
     * 2. Stop-and-copy: pause VM, copy remaining dirty pages
     * 3. Transfer VCPU state
     * 4. Resume on target
     */
    
    /* Update host */
    if (vm->host_node) {
        vm->host_node->vm_count--;
        vm->host_node->total_migrations++;
    }
    
    vm->host_node = target_node;
    vm->host_node_id = target_node->id;
    target_node->vm_count++;
    
    vm_set_state(mgr, vm, prev_state);
    
    pr_info("VM '%s': Migration complete", vm->config.name);
    
    return 0;
}
