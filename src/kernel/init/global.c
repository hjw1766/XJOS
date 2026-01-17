#include <xjos/global.h>
#include <xjos/string.h>
#include <xjos/debug.h>


descriptor_t gdt[GDT_SIZE];     // kernel global descriptor table
gdt_ptr_t gdt_ptr;              // kernel global descriptor table pointer
tss_t tss;                      // task state segment


void descriptor_init(descriptor_t *desc, u32 base, u32 limit) {
    desc->base_low = base & 0xFFFFF;
    desc->base_high = (base >> 24) & 0xFF;
    desc->limit_low = limit & 0xFFFF;
    desc->limit_high = (limit >> 16) & 0x0f;
}


// Taking over the GDT table from the kernel
void gdt_init() {
    DEBUGK("init gdt!!!\n");

    asm volatile("sgdt gdt_ptr");

    // byte copy, gdt[0] 8B gdt[1] 8B... gdt[GDT_SIZE-1] 8B, (gdt_ptr.limit + 1)/8 = index
    memcpy(&gdt, (void *)gdt_ptr.base, gdt_ptr.limit + 1);

    descriptor_t *desc; // add a descriptor to the GDT
    desc = gdt + USER_CODE_IDX; // gdt pointer +4
    descriptor_init(desc, 0, 0xFFFFF);
    desc->segment = 1;  // code segment
    desc->granularity = 1;  // 4KB granularity
    desc->big = 1;     // 32-bit code segment
    desc->long_mode = 0;    // not 64-bit
    desc->present = 1;  // memory present
    desc->DPL = 3;     // ring 3 (user mode)
    desc->type = 0b1010; // Code / Non-conforming / Readable / Not Accessed

    desc = gdt + USER_DATA_IDX; // gdt pointer +5
    descriptor_init(desc, 0, 0xFFFFF);
    desc->segment = 1;  // data segment
    desc->granularity = 1;
    desc->big = 1;
    desc->long_mode = 0;
    desc->present = 1;
    desc->DPL = 3;
    desc->type = 0b0010;    // data / up / Writable / Not Accessed

    gdt_ptr.base = (u32)&gdt;
    gdt_ptr.limit = sizeof(gdt) - 1;

    asm volatile("lgdt gdt_ptr");
}


void tss_init() {
    memset(&tss, 0, sizeof(tss));

    tss.ss0 = KERNEL_DATA_SELECTOR;
    tss.iobase = sizeof(tss);

    descriptor_t *desc = gdt + KERNEL_TSS_IDX;
    descriptor_init(desc, (u32)&tss, sizeof(tss) - 1);  // write TSS to GDT
    desc->segment = 0;   // system segment
    desc->granularity = 0; // byte
    desc->big = 0;
    desc->long_mode = 0;
    desc->present = 1;
    desc->DPL = 0;       // task / invoke
    desc->type = 0b1001; // 32-bit available TSS

    // 0x18 selector load TR reg
    asm volatile(
        "ltr %%ax\n" :: "a"(KERNEL_TSS_SELECTOR)
    );
}