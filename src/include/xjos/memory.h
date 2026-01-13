#ifndef XJOS_MEMORY_H
#define XJOS_MEMORY_H

#include <xjos/types.h>

#define PAGE_SIZE 0x1000        // one page is 4KB
#define MEMORY_BASE 0x100000     // memory starts at 1M

// kernel memory size
#define KERNEL_MEMORY_SIZE 0x1000000 // 16MB

// kernel cache addr
#define KERNEL_BUFFER_MEM 0x800000

// kernel cache size
#define KERNEL_BUFFER_SIZE 0x400000 // 4MB

// kernel ramdisk addr
#define KERNEL_RAMDISK_MEM (KERNEL_BUFFER_MEM + KERNEL_BUFFER_SIZE)

// kernel ramdisk size
#define KERNEL_RAMDISK_SIZE 0x400000 // 4MB

// user program exec addr
#define USER_EXEC_ADDR KERNEL_MEMORY_SIZE

// user mmap start addr
#define USER_MMAP_ADDR 0x8000000     // 128MB

#define USER_STACK_SIZE 0x400000   // 4MB

// user stack top
#define USER_STACK_TOP 0x10000000    // 256MB

// user stack guard size
#define USER_GUARD_SIZE   0x100000   // 1MB

// 256MB - 4MB = 252MB
#define USER_STACK_BOTTOM (USER_STACK_TOP - USER_STACK_SIZE)

// user mmap limit addr
#define USER_MMAP_LIMIT   (USER_STACK_TOP - USER_STACK_SIZE - USER_GUARD_SIZE)

// user mmap size
#define USER_MMAP_SIZE (USER_MMAP_LIMIT - USER_MMAP_ADDR)   // 123MB
 
#define KERNEL_PAGE_DIR 0x1000

typedef struct {
    u8 present : 1;             // in memroy or not
    u8 write : 1;               // 0 only read, 1 can write / read
    u8 user : 1;                // 0 kernel, 1 user
    u8 pwt : 1;                 // page write through 1/0
    u8 pcd : 1;                 // page cache disable 1/0
    u8 accessed : 1;            // has been accessed
    u8 dirty : 1;               // dirty page, has been written to
    u8 pat : 1;                 // page attribute table, 4K/4M
    u8 global : 1;              // global page, can be swapped
    u8 shared : 1;              // shared page
    u8 privat : 1;             // private page
    u8 readonly : 1;           // read only page
    u32 index : 20;           // page index
}_packed page_entry_t;

u32 get_cr2();
u32 get_cr3();
void set_cr3(u32 pde);

// alloc and free count contiguous kernel pages
u32 alloc_kpage(u32 count);
void free_kpage(u32 vaddr, u32 count);

// get page table entry
page_entry_t *get_entry(u32 vaddr, bool create);

// flush tlb
void flush_tlb(u32 vaddr);

// vaddr <-> paddr
void link_page(u32 vaddr);
void unlink_page(u32 vaddr);

// copy pde
page_entry_t *copy_pde();

void free_pde();

#endif /* XJOS_MEMORY_H */