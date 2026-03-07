#include <xjos/cpu.h>


// Check if the CPU supports the CPUID instruction
bool cpu_check_cpuid() {
    bool ret;

    asm volatile (
        "pushfl \n"     // Save EFLAGS

        "pushfl \n"     // get EFLAGS
        "xorl $0x00200000, (%%esp) \n" // Toggle ID bit in EFLAGS
        "popfl \n"      // write back modified EFLAGS

        "pushfl \n"     // get EFLAGS again
        "popl %%eax \n" // write EFLAGS to EAX
        "xor $0x00200000, %%eax \n" // Check if ID
        "andl $0x00200000, %%eax \n" // get ID bit
        "shrl $21, %%eax \n" // Shift ID bit to LSB

        "popfl \n"      // Restore original EFLAGS
        : "=a" (ret)    // Output: ret will hold the result
    );
    return ret;
}

// Get CPU vendor ID string
void cpu_vendor_id(cpu_vendor_t *item) {
    asm volatile(
        "cpuid \n"
        : "=a"(*((u32 *)item + 0)),
          "=b"(*((u32 *)item + 1)),
          "=d"(*((u32 *)item + 2)),
          "=c"(*((u32 *)item + 3))
        : "a"(0));
    item->info[12] = '\0'; // Null-terminate the string
}

void cpu_version(cpu_version_t *ver) {
    asm volatile(
        "cpuid \n"
        : "=a"(*((u32 *)ver + 0)),
          "=b"(*((u32 *)ver + 1)),
          "=c"(*((u32 *)ver + 2)),
          "=d"(*((u32 *)ver + 3))
        : "a"(1));
}