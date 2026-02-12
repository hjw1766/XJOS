#include <xjos/types.h>
#include <xjos/debug.h>
#include <xjos/assert.h>
#include <xjos/memory.h>
#include <xjos/stdlib.h>
#include <xjos/string.h>
#include <xjos/bitmap.h>
#include <xjos/task.h>
#include <xjos/syscall_nr.h>
#include <fs/fs.h>
#include <xjos/printk.h>


#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

extern int sys_read(fd_t fd, char *buf, int len);
extern int sys_lseek(fd_t fd, int offset, int whence);


#define ZONE_VALID 1    // ards Valid zone
#define ZONE_RESERVED 2 // ards Reserved zone

/*  
    directory page index (31 ~ 22)bits
    page table index (21 ~ 12)bits
    page offset (11 ~ 0)bits
*/
#define DIDX(addr) (((u32)addr >> 22) & 0x3ff)          // get addr pde indx 
#define TIDX(addr) (((u32)addr >> 12) & 0x3ff)          // get addr pte indx 
#define IDX(addr) ((u32)(addr) >> 12)                   //get addr page index 
#define PAGE(idx) ((u32)(idx) << 12)                    // page start address
#define ASSERT_PAGE(addr) assert((addr & 0xfff) == 0)   // once page start address

#define PDE_MASK 0XFFC00000     // ->pde

// kernel page directory
#define KERNEL_PAGE_DIR 0x1000

// kernel page table
static u32 KERNEL_PAGE_TABLE[] = {
    0x2000,
    0x3000,
    0x4000,
    0x5000,
};

#define KERNEL_MAP_BITS 0x6000


bitmap_t kernel_map;

typedef struct {
    u64 base;   // memory base
    u64 size;   // memory length
    u32 type;   // memory type
}_packed ards_t;


static u32 memory_base = 0;    // Available memory base, value should = 1M
static u32 memory_size = 0;    // Available memory size
static u32 total_pages = 0;    // all memory pages
static u32 free_pages = 0;     // free memory pages

#define used_pages (total_pages - free_pages)   // used memory pages


void memory_init(u32 magic, u32 addr) {
    u32 count;

    ards_t *ptr;

    // check magic number
    if (magic == XJOS_MAGIC) {
        count = *(u32*)addr;    // 4 bytes count
        ptr = (ards_t*)(addr + 4);  // +4 bytes pointer to ards_t array

        for (int i = 0; i < count; i++, ptr++) {
            LOGK("Memory base 0x%p size 0x%p type %d\n",
                (u32)ptr->base, (u32)ptr->size, (u32)ptr->type);
            // find max valid memory zone
            if (ptr->type == ZONE_VALID && ptr->size > memory_size) {
                memory_base = (u32)ptr->base;
                memory_size = (u32)ptr->size;
            }
        }
    } else {
        panic("Memory init failed: invalid magic number, 0x%p\n", (u32)magic);
    }

    LOGK("ARDS count %d\n", count);
    LOGK("Memory base 0x%p size 0x%p\n", (u32)memory_base, (u32)memory_size);

    assert(memory_base == MEMORY_BASE);
    assert((memory_size & 0xfff) == 0);

    // calculate total and free pages
    total_pages = IDX(memory_size) + IDX(MEMORY_BASE);  // 31M + 1M
    free_pages = IDX(memory_size);

    LOGK("Total pages %d Free pages %d\n", total_pages, free_pages);

    // check system memory size
    if (memory_size < KERNEL_MEMORY_SIZE) {
        panic("System memory is %dM too small, at least %dM needed\n", 
            memory_size / MEMORY_BASE, KERNEL_MEMORY_SIZE / MEMORY_BASE);
    }
}


static u32 start_page = 0;      // free start page index
static u8 *memory_map;          // pyhsical memory map
static u32 memory_map_pages = 0;    // physical memory used pages


void memory_map_init() {

    // init pyhsical memory map, 0x100000
    memory_map = (u8*)memory_base;

    /*
        why need div_round_up?
        Since we need to store the status of each page using one byte, 
        such as 0x00, 0x01, etc., 
        and we have to manage a total of total_pages (8162) pages, 
        we need to use `pages` to store them. 
        8162 divided by 4096 and rounded up gives us 2 pages.
    */
    memory_map_pages = div_round_up(total_pages, PAGE_SIZE);

    LOGK("Memory map page count %d\n", memory_map_pages);
    
    free_pages -= memory_map_pages;

    // clear pyhsical memory map
    memset((void*)memory_base, 0, memory_map_pages * PAGE_SIZE);

    // set start page index
    start_page = IDX(memory_base) + memory_map_pages;
    LOGK("Start page index %d\n", start_page);

    for (size_t i = 0; i < start_page; i++) {
        memory_map[i] = 1;  // set all pages as used
    }

    LOGK("Total pages %d free pages %d\n", total_pages, free_pages);

    // 2048 - 256 = 1792 page, need use bytes / 8 
    u32 length = (IDX(KERNEL_MEMORY_SIZE) - IDX(MEMORY_BASE)) / 8;
    bitmap_init(&kernel_map, (u8*)KERNEL_MAP_BITS, length, IDX(MEMORY_BASE));
    bitmap_scan(&kernel_map, memory_map_pages);

}


// distribute a page memory
static u32 get_page() {
    for (size_t i = start_page; i < total_pages; i++) {
        // find a free page
        if (!memory_map[i]) {
            memory_map[i] = 1;  // set as used
            assert(free_pages > 0);
            free_pages--;

            LOGK("Get page index 0x%x\n", i);

            u32 page = PAGE(i);  // current page address
            LOGK("Get page addr 0x%p\n", page);

            return page;
        }
    }

    panic("No free page available\n");
}


// free a page memory
static void put_page(u32 addr) {
    ASSERT_PAGE(addr);      // page start address

    u32 idx = IDX(addr);
    // idx > 1M, and < total_pages
    assert(idx >= start_page && idx < total_pages);

    assert(memory_map[idx] >= 1);

    // pyhsical refer -1
    memory_map[idx]--;
    if (!memory_map[idx]) {
        free_pages++;
        if (idx < start_page)
            start_page = idx;
    }

    assert(free_pages > 0 && free_pages < total_pages);
    LOGK("Put page addr 0x%p\n", addr);
}


u32 get_cr2() {
    u32 val;
    asm volatile("movl %%cr2, %0" : "=r"(val));
    return val;
}


// get cr3 register value, page directory base address
u32 _inline get_cr3() {
    u32 val;
    asm volatile("movl %%cr3, %0" : "=r"(val));
    return val;
    // store eax, return cr3 value
}


// arg pde is the page directory entry address
_inline void set_cr3(u32 pde) {
    ASSERT_PAGE(pde);
    asm volatile("movl %%eax, %%cr3\n" ::"a"(pde));
}


// set cr3 reg, PE -> 1, enable paging
static _inline void enable_page() {
    asm volatile(
        "movl %cr0, %eax\n"
        "orl $0x80000000, %eax\n"
        "movl %eax, %cr0\n"
    );
}


static void entry_init(page_entry_t *entry, u32 index) {
    *(u32 *)entry = 0;

    entry->present = 1;
    entry->write = 1;
    entry->user = 1;
    entry->index = index;
}


static page_entry_t *get_pde() {
    // get pde[1023] -> pte[1023] -> page directory
    return (page_entry_t *)(0xfffff000);
}


static page_entry_t *get_pte(u32 vaddr, bool create) {
    page_entry_t *pde = get_pde();
    u32 pde_idx = DIDX(vaddr);
    page_entry_t *entry = &pde[pde_idx];    // get pde*

    assert(create || (!create && entry->present));

    page_entry_t *table = (page_entry_t *)(PDE_MASK | (pde_idx << 12));

    if (!entry->present) {
        LOGK("Get and create page table entry for 0x%p\n", vaddr);
        u32 page = get_page();  // page table
        entry_init(entry, IDX(page));
        memset(table, 0, PAGE_SIZE);  
    }

    return table;       // return page table *
}


page_entry_t *get_entry(u32 vaddr, bool create) {
    page_entry_t *pte = get_pte(vaddr, create);
    return &pte[TIDX(vaddr)];
}


void flush_tlb(u32 vaddr) {
    asm volatile("invlpg (%0)" :: "r"(vaddr) : "memory");
}


void mapping_init() {
/*   pyhsical 0x201000 (Page Table)
     +-----------------------------------------------------------------+
     | PTE[0] | PTE[1] | PTE[2] | ... | PTE[1023] |
     +-----------------------------------------------------------------+
     ^        ^        ^
     |        |        |
     0x201000 0x201004 0x201008

     (Physical Page Frames)
     +--------+--------+--------+-----+-----------+
     | PFN 0  | PFN 1  | PFN 2  | ... | PFN 1023  |
     +------------------------------------------------------------------+
     ^        ^        ^               ^
     |        |        |               |
     0x0000   0x1000   0x2000          0x3FF000
*/
    page_entry_t *pde = (page_entry_t*)KERNEL_PAGE_DIR;
    memset(pde, 0, PAGE_SIZE);

    idx_t index = 0;
    // page directory init
    for (idx_t didx = 0; didx < (sizeof(KERNEL_PAGE_TABLE) / 4); didx++) {
        page_entry_t *pte = (page_entry_t*)KERNEL_PAGE_TABLE[didx];
        memset(pte, 0, PAGE_SIZE);

        page_entry_t *entry = &pde[didx];   // pde -> pte
        entry_init(entry, IDX((u32)pte));
        entry->user = 0;    // user stop access kernel page table

        for (idx_t tidx = 0; tidx < 1024; tidx++, index++) {
            // dont mapping the first page, *(null)
            if (index == 0)
                continue;
            page_entry_t *tentry = &pte[tidx];
            entry_init(tentry, index);
            tentry->user = 0; // user stop access kernel page
            memory_map[index] = 1;
        }
    }

    // pde[1023] -> page directory, we can access all page table
    page_entry_t *entry = &pde[1023];
    entry_init(entry, IDX(KERNEL_PAGE_DIR));

    set_cr3((u32)pde);

    enable_page((u32)pde);
}


// find continuous count pages in bitmap
static u32 scan_page(bitmap_t *map, u32 count) {
    assert(count > 0);

    int32 index = bitmap_scan(map, count);

    if (index == EOF)
        panic("Scan page fail!");
    
    idx_t addr = PAGE(index);
    LOGK("Scan page addr 0x%p count %d\n", addr, count);
    return addr;
}


// reset count pages in bitmap
static void reset_page(bitmap_t *map, u32 addr, u32 count) {
    ASSERT_PAGE(addr);
    assert(count > 0);

    idx_t index = IDX(addr);    // page nubmer

    for (idx_t i = 0; i < count; i++) {
        assert(bitmap_test(map, index + i));
        bitmap_set(map, index + i, 0);
    }
}


u32 alloc_kpage(u32 count) {
    assert(count > 0);

    idx_t vaddr = scan_page(&kernel_map, count);
    LOGK("Alloc kernel pages 0x%p count %d\n", vaddr, count);
    memset((void*)vaddr, 0, count * PAGE_SIZE);
    return vaddr;
}


void free_kpage(u32 vaddr, u32 count) {
    ASSERT_PAGE(vaddr);
    assert(count > 0);

    reset_page(&kernel_map, vaddr, count);
    LOGK("free kernel pages 0x%p count %d\n", vaddr, count);
}


void link_page(u32 vaddr) {
    ASSERT_PAGE(vaddr);

    // pte -> page table
    page_entry_t *entry = get_entry(vaddr, true);

    u32 index = IDX(vaddr);

    // page present
    if (entry->present) {
        return;
    }

    u32 paddr = get_page();     // data page
    entry_init(entry, IDX(paddr));
    flush_tlb(vaddr);

    LOGK("Link from 0x%p to 0x%p\n", vaddr, paddr);
}


void unlink_page(u32 vaddr) {
    ASSERT_PAGE(vaddr);

    page_entry_t *pde = get_pde();
    page_entry_t *entry = &pde[DIDX(vaddr)];
    if (!entry->present)    // check page table present
        return;
    
    entry = get_entry(vaddr, false);
    u32 index = IDX(vaddr);

    if (!entry->present) {
        return;
    }

    entry->present = false;

    // dont free page table, local theory
    u32 paddr = PAGE(entry->index);

    // ref count maybe > 1
    // if (memory_map[entry->index] == 1) 
    put_page(paddr);

    flush_tlb(vaddr);
}


// copy page, retrun paddr
static u32 copy_page(void *page) {
    u32 paddr = get_page();
    u32 vaddr = 0;

    // build pde[0] -> pte[0] -> page
    page_entry_t *entry = get_pte(vaddr, false);
    entry_init(entry, IDX(paddr));

    // !bug
    flush_tlb(vaddr);   // update tlb, beacuse old_tlb is 0

    // page write -> 0(paddr)
    memcpy((void *)vaddr, (void *)page, PAGE_SIZE);

    entry->present = false;

    flush_tlb(vaddr);   // Strengthen Assertion
    return paddr;
}


void free_pde() {
    task_t *task = running_task();

    assert(task->uid != KERNEL_USER);

    page_entry_t *pde = get_pde();
    
    // pde 2 - 1022
    for (size_t didx = (sizeof(KERNEL_PAGE_TABLE) / 4); didx < 1023; didx++) {
        page_entry_t *dentry = &pde[didx];
        if (!dentry->present)
            continue;

        page_entry_t *pte = (page_entry_t *)(PDE_MASK | didx << 12);

        // pte 0 - 1023
        for (size_t titx = 0; titx < 1024; titx++) { 
            page_entry_t *entry = &pte[titx];
            if (!entry->present)
                continue;

            assert(memory_map[entry->index] > 0);
            put_page(PAGE(entry->index));   // free page
        }

        put_page(PAGE(dentry->index));      // free page table
    }

    free_kpage(task->pde, 1);               // free pde
    LOGK("free pages %d\n", free_pages);
}


// copy current pde
page_entry_t *copy_pde() {
    task_t *task = running_task();
    page_entry_t *pde = (page_entry_t*)alloc_kpage(1);
    memcpy(pde, (void*)task->pde, PAGE_SIZE);

    page_entry_t *entry = &pde[1023];
    entry_init(entry, IDX(pde));

    // * Isolate parent and child processes
    page_entry_t *dentry;

    for (size_t didx = ((sizeof(KERNEL_PAGE_TABLE) / 4)); didx < 1023; didx++) {
        dentry = &pde[didx];
        if (!dentry->present)
            continue;

        // pde[0] + didx << 12
        page_entry_t *pte = (page_entry_t *)(PDE_MASK | (didx << 12));

        for (size_t tidx = 0; tidx < 1024; tidx++) {
            entry = &pte[tidx];
            if (!entry->present)
                continue;
            
            // present
            assert(memory_map[entry->index] > 0);
            // if dont shared page, read only
            if (!entry->shared) {
                entry->write = false;
            }
            memory_map[entry->index]++;

            assert(memory_map[entry->index] < 255);
        }

        u32 paddr = copy_page(pte);
        dentry->index = IDX(paddr);
    }

    set_cr3(task->pde); // active parent process pde

    return pde;
}


int sys_brk(void *addr) {
    // LOGK("task brk 0x%p\n", addr);
    u32 brk = (u32)addr;

    ASSERT_PAGE(brk);

    task_t *task = running_task();
    assert(task->uid != KERNEL_USER);

    assert((task->end <= brk) && (brk <= USER_MMAP_ADDR));
    u32 old_brk = task->brk;

    // if brk < old_brk, need free page
    if (old_brk > brk) {
        // !bug  brk -> task->brk = brk, so need temp store old_brk
        for (u32 page = brk; page < old_brk; page += PAGE_SIZE) {
            unlink_page(page);
        }
    } else if (IDX(brk - old_brk) > free_pages) {
        return -1; //*translation page fault
    }

    task->brk = brk;
    return 0;
}

void *sys_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    ASSERT_PAGE((u32)addr);

    u32 count = div_round_up(length, PAGE_SIZE);
    u32 vaddr = (u32)addr;

    task_t *task = running_task();
    // if addr is NULL, scan free pages
    if (!vaddr) {
        vaddr = scan_page(task->vmap, count);
    }

    // scan mmap addr (auto)
    u32 vend = vaddr + count * PAGE_SIZE;
    assert(vaddr >= USER_MMAP_ADDR && 
        vend <= USER_MMAP_LIMIT && vaddr < vend);

    for (size_t i = 0; i < count; i++) {
        u32 page = vaddr + i * PAGE_SIZE;
        link_page(page);
        memset((void *)page, 0, PAGE_SIZE);
        bitmap_set(task->vmap, IDX(page), true);

        page_entry_t *entry = get_entry(page, false);
        entry->user = true;
        entry->write = false;
        entry->readonly = true;
        if (prot & PROT_WRITE) {
            entry->readonly = false;
            entry->write = true;
        }
        if (flags & MAP_SHARED) {
            entry->shared = true;
        }
        if (flags & MAP_PRIVATE) {
            entry->privat = true;
        }
        flush_tlb(page);
    }

    if (fd != EOF) {
        sys_lseek(fd, offset, SEEK_SET);
        sys_read(fd, (void *)vaddr, length);    // todo
    }

    return (void *)vaddr;
}

int sys_munmap(void *addr, size_t length) {
    u32 vaddr = (u32)addr;
    
    ASSERT_PAGE(vaddr);
    u32 count = div_round_up(length, PAGE_SIZE);
    u32 vend = vaddr + count * PAGE_SIZE;
    assert(vaddr >= USER_MMAP_ADDR && 
        vaddr <= USER_MMAP_LIMIT && vaddr < vend);

    task_t *task = running_task();
    for (size_t i = 0; i < count; i++) {
        u32 page = vaddr + i * PAGE_SIZE;
        unlink_page(page);
        assert(bitmap_test(task->vmap, IDX(page)));
        bitmap_set(task->vmap, IDX(page), false);
    }

    return 0;
}

typedef struct {
    u8 present : 1;
    u8 write : 1;
    u8 user : 1;
    u8 reserved0 : 1;
    u8 fetch : 1;
    u8 protection : 1;
    u8 shadow : 1;
    u16 reserved1 : 8;
    u8 sgx : 1;
    u16 reserved2;
}_packed page_error_code_t;


void page_fault(u32 vector,
    u32 edi, u32 esi, u32 ebp, u32 esp,
    u32 ebx, u32 edx, u32 ecx, u32 eax,
    u32 gs, u32 fs, u32 es, u32 ds,
    u32 vector0, u32 error, u32 eip, u32 cs, u32 eflags) {
    
    assert(vector == 0xe);
    u32 vaddr = get_cr2();
    LOGK("fault address 0x%p\n", vaddr);

    page_error_code_t *code = (page_error_code_t *)&error;
    task_t *task = running_task();

    // 16M -- 256M
    // assert((KERNEL_MEMORY_SIZE <= vaddr) && (vaddr < USER_STACK_TOP));
    
    // if user process access kernel memory, panic
    if (vaddr < USER_EXEC_ADDR || vaddr >= USER_STACK_TOP) {
        assert(task->uid);
        printk("Segmentation Fault: Invalid memory access at 0x%p by task %s (pid %d)\n",
            vaddr, task->name, task->pid);

        task_exit(-1);
    }

    // * Copy-on-Write (CoW)
    if (code->present && code->write) { 
        page_entry_t *entry = get_entry(vaddr, false);

        if (entry->readonly) {
            panic("Segmentation Fault: Write to Read-Only page at 0x%p\n", vaddr);
        }

        assert(!entry->shared);  // not shared page
        assert(memory_map[entry->index] > 0);
        if (memory_map[entry->index] == 1) {
            // parent process exit
            entry->write = true;
            flush_tlb(vaddr);
            LOGK("write page for 0x%p\n", vaddr);
        } else {
            // >> 12 + << 12, clear offset
            void *page = (void *)PAGE(IDX(vaddr));
            u32 paddr = copy_page(page);
            memory_map[entry->index]--;
            entry_init(entry, IDX(paddr));
            flush_tlb(vaddr);
            LOGK("copy page for 0x%p\n", vaddr);
        }

        return;
    }


    //* Demand Paging
    // stack 254 - 256M, heap < task->brk 
    if (!code->present && (vaddr < task->brk || vaddr >= USER_STACK_BOTTOM)) {
        u32 page = PAGE(IDX(vaddr));
        link_page(page);

        return;
    }

    LOGK("task 0x%p name %s brk 0x%p page fault\n", task, task->name, task->brk);

    panic("page fault!!!");
}