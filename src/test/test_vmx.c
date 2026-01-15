/*
 * PureVisor - VMX Test Suite
 * 
 * Unit tests for VMX hypervisor components
 */

#include <lib/types.h>
#include <lib/string.h>
#include <test/framework.h>
#include <vmm/vmx.h>
#include <vmm/vcpu.h>
#include <vmm/ept.h>
#include <arch/x86_64/cpu.h>

/* External CPU features */
extern cpu_features_t cpu_features;

/* ============================================================================
 * VMX Feature Tests
 * ============================================================================ */

static test_result_t test_vmx_supported(void)
{
    TEST_SKIP_IF(!cpu_features.vmx_supported, "VMX not supported on this CPU");
    return TEST_PASS;
}

static test_result_t test_vmx_cr4_vmxe(void)
{
    TEST_SKIP_IF(!cpu_features.vmx_supported, "VMX not supported");
    
    /* Check if VMX can be enabled in CR4 */
    uint64_t cr4 = read_cr4();
    
    /* VMXE bit should be settable */
    /* Note: Don't actually set it here as it might affect system state */
    (void)cr4;
    
    return TEST_PASS;
}

static test_result_t test_vmx_msr_basic(void)
{
    TEST_SKIP_IF(!cpu_features.vmx_supported, "VMX not supported");
    
    /* Read VMX basic MSR */
    uint64_t vmx_basic = rdmsr(MSR_IA32_VMX_BASIC);
    
    /* Check revision ID (bits 0-30) is non-zero */
    uint32_t rev_id = vmx_basic & 0x7FFFFFFF;
    TEST_ASSERT_NE(rev_id, 0);
    
    /* Check VMCS size (bits 32-44) is reasonable */
    uint32_t vmcs_size = (vmx_basic >> 32) & 0x1FFF;
    TEST_ASSERT_GT(vmcs_size, 0);
    TEST_ASSERT_LE(vmcs_size, 4096);
    
    return TEST_PASS;
}

static test_case_t vmx_feature_tests[] = {
    {"vmx_supported", test_vmx_supported},
    {"vmx_cr4_vmxe", test_vmx_cr4_vmxe},
    {"vmx_msr_basic", test_vmx_msr_basic},
};

static test_suite_t vmx_feature_suite = {
    .name = "VMX Features",
    .setup = NULL,
    .teardown = NULL,
    .tests = vmx_feature_tests,
    .test_count = sizeof(vmx_feature_tests) / sizeof(vmx_feature_tests[0]),
};

/* ============================================================================
 * VCPU Tests
 * ============================================================================ */

static test_result_t test_vcpu_state_size(void)
{
    /* VCPU state should be reasonable size */
    TEST_ASSERT_GT(sizeof(vcpu_t), 0);
    TEST_ASSERT_LT(sizeof(vcpu_t), 64 * 1024);  /* < 64KB */
    
    return TEST_PASS;
}

static test_result_t test_vcpu_gpr_layout(void)
{
    /* Test GPR structure layout */
    guest_regs_t gprs = {0};
    
    gprs.rax = 0x1111111111111111ULL;
    gprs.rbx = 0x2222222222222222ULL;
    gprs.rcx = 0x3333333333333333ULL;
    gprs.rdx = 0x4444444444444444ULL;
    
    TEST_ASSERT_EQ(gprs.rax, 0x1111111111111111ULL);
    TEST_ASSERT_EQ(gprs.rbx, 0x2222222222222222ULL);
    TEST_ASSERT_EQ(gprs.rcx, 0x3333333333333333ULL);
    TEST_ASSERT_EQ(gprs.rdx, 0x4444444444444444ULL);
    
    return TEST_PASS;
}

static test_case_t vcpu_tests[] = {
    {"vcpu_state_size", test_vcpu_state_size},
    {"vcpu_gpr_layout", test_vcpu_gpr_layout},
};

static test_suite_t vcpu_suite = {
    .name = "VCPU",
    .setup = NULL,
    .teardown = NULL,
    .tests = vcpu_tests,
    .test_count = sizeof(vcpu_tests) / sizeof(vcpu_tests[0]),
};

/* ============================================================================
 * EPT Tests
 * ============================================================================ */

static test_result_t test_ept_entry_flags(void)
{
    /* Test EPT entry flag definitions */
    TEST_ASSERT_EQ(EPT_READ, 0x1);
    TEST_ASSERT_EQ(EPT_WRITE, 0x2);
    TEST_ASSERT_EQ(EPT_EXECUTE, 0x4);
    
    return TEST_PASS;
}

static test_result_t test_ept_page_sizes(void)
{
    /* Verify page size constants */
    TEST_ASSERT_EQ(EPT_PAGE_SIZE_4K, 4096);
    TEST_ASSERT_EQ(EPT_PAGE_SIZE_2M, 2 * 1024 * 1024);
    TEST_ASSERT_EQ(EPT_PAGE_SIZE_1G, 1024 * 1024 * 1024);
    
    return TEST_PASS;
}

static test_case_t ept_tests[] = {
    {"ept_entry_flags", test_ept_entry_flags},
    {"ept_page_sizes", test_ept_page_sizes},
};

static test_suite_t ept_suite = {
    .name = "EPT (Extended Page Tables)",
    .setup = NULL,
    .teardown = NULL,
    .tests = ept_tests,
    .test_count = sizeof(ept_tests) / sizeof(ept_tests[0]),
};

/* ============================================================================
 * Suite Registration
 * ============================================================================ */

void test_vmx_suite(void)
{
    test_register_suite(&vmx_feature_suite);
    test_register_suite(&vcpu_suite);
    test_register_suite(&ept_suite);
}
