/*
 * PureVisor - VMX Assembly Routines
 * 
 * VM entry/exit low-level code
 */

.code64
.section .text

.global vmx_vmlaunch
.global vmx_vmresume
.global vmx_vmexit_handler

/* External C handler */
.extern vmexit_handler

/* ============================================================================
 * vmx_vmlaunch - Execute VMLAUNCH
 * 
 * RDI = pointer to vcpu_t structure
 * Returns: 0 on success after VM exit, non-zero on failure
 * ============================================================================ */
vmx_vmlaunch:
    /* Save host callee-saved registers */
    pushq %rbx
    pushq %rbp
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    
    /* Save vcpu pointer for later */
    pushq %rdi
    
    /* Load guest registers from vcpu->regs */
    movq 0(%rdi), %rax      /* rax */
    movq 8(%rdi), %rbx      /* rbx */
    movq 16(%rdi), %rcx     /* rcx */
    movq 24(%rdi), %rdx     /* rdx */
    movq 32(%rdi), %rsi     /* rsi */
    /* rdi loaded last */
    movq 48(%rdi), %rbp     /* rbp */
    movq 56(%rdi), %r8      /* r8 */
    movq 64(%rdi), %r9      /* r9 */
    movq 72(%rdi), %r10     /* r10 */
    movq 80(%rdi), %r11     /* r11 */
    movq 88(%rdi), %r12     /* r12 */
    movq 96(%rdi), %r13     /* r13 */
    movq 104(%rdi), %r14    /* r14 */
    movq 112(%rdi), %r15    /* r15 */
    movq 40(%rdi), %rdi     /* rdi (guest) - load last */
    
    /* Execute VMLAUNCH */
    vmlaunch
    
    /* If we get here, VMLAUNCH failed */
    jmp vmx_launch_failed

/* ============================================================================
 * vmx_vmresume - Execute VMRESUME
 * 
 * RDI = pointer to vcpu_t structure
 * Returns: 0 on success after VM exit, non-zero on failure
 * ============================================================================ */
vmx_vmresume:
    pushq %rbx
    pushq %rbp
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    pushq %rdi
    
    /* Load guest registers */
    movq 0(%rdi), %rax
    movq 8(%rdi), %rbx
    movq 16(%rdi), %rcx
    movq 24(%rdi), %rdx
    movq 32(%rdi), %rsi
    movq 48(%rdi), %rbp
    movq 56(%rdi), %r8
    movq 64(%rdi), %r9
    movq 72(%rdi), %r10
    movq 80(%rdi), %r11
    movq 88(%rdi), %r12
    movq 96(%rdi), %r13
    movq 104(%rdi), %r14
    movq 112(%rdi), %r15
    movq 40(%rdi), %rdi
    
    vmresume
    
    /* If we get here, VMRESUME failed */
    jmp vmx_launch_failed

/* ============================================================================
 * VM Exit Entry Point
 * 
 * This is where the CPU jumps after a VM exit.
 * Host RSP/RIP are restored by hardware from VMCS.
 * ============================================================================ */
vmx_vmexit_handler:
    /* Get vcpu pointer (saved on stack before vmlaunch/vmresume) */
    /* Stack: [vcpu_ptr][r15][r14]...[rbx][return_addr] */
    
    /* Save guest registers to vcpu->regs */
    /* First, we need to get vcpu pointer */
    /* It's at RSP after the pushes: RSP + 0 = return from vmx_vmlaunch */
    
    /* Save guest rdi temporarily */
    pushq %rdi
    
    /* Get vcpu pointer from stack */
    movq 8(%rsp), %rdi      /* vcpu pointer */
    
    /* Save all guest registers */
    movq %rax, 0(%rdi)
    movq %rbx, 8(%rdi)
    movq %rcx, 16(%rdi)
    movq %rdx, 24(%rdi)
    movq %rsi, 32(%rdi)
    popq %rax               /* Restore guest rdi */
    movq %rax, 40(%rdi)
    movq %rbp, 48(%rdi)
    movq %r8, 56(%rdi)
    movq %r9, 64(%rdi)
    movq %r10, 72(%rdi)
    movq %r11, 80(%rdi)
    movq %r12, 88(%rdi)
    movq %r13, 96(%rdi)
    movq %r14, 104(%rdi)
    movq %r15, 112(%rdi)
    
    /* Call C handler: vmexit_handler(vcpu) */
    /* rdi already contains vcpu pointer */
    call vmexit_handler
    
    /* Check return value */
    testl %eax, %eax
    jnz vmx_exit_error
    
    /* Restore vcpu pointer and continue */
    popq %rdi               /* vcpu pointer */
    
    /* Restore host registers */
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbp
    popq %rbx
    
    /* Return success */
    xorl %eax, %eax
    ret

vmx_launch_failed:
    /* Restore vcpu pointer */
    popq %rdi
    
    /* Restore host registers */
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbp
    popq %rbx
    
    /* Return failure */
    movl $1, %eax
    ret

vmx_exit_error:
    /* Pop vcpu pointer */
    popq %rdi
    
    /* Restore host registers */
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbp
    popq %rbx
    
    /* Return error code from handler */
    /* eax already contains error */
    ret
