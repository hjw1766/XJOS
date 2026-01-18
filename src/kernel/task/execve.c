#include <xjos/types.h>
#include <fs/fs.h>
#include <xjos/memory.h>
#include <xjos/stdlib.h>
#include <xjos/string.h>
#include <xjos/assert.h>
#include <xjos/debug.h>
#include <xjos/task.h>
#include <xjos/global.h>
#include <xjos/arena.h>


#if 0
#include <elf.h>
#endif

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

typedef u32 Elf32_Word;
typedef u32 Elf32_Addr;
typedef u32 Elf32_Off;
typedef u16 Elf32_Half;

// ELF file mark
typedef struct ELFIdent {
    u8 ei_magic[4];     // Magic number 0x7f 'E' 'L' 'F'
    u8 ei_class;        // 1 = 32-bit, 2 = 64-bit
    u8 ei_data;         // 1 = little endian, 2 = big endian
    u8 ei_version;      // ELF version
    u8 ei_pad[16 - 7]; // Padding bytes
} _packed ELFIdent;

// ELF Header
typedef struct Elf32_Ehdr {
    ELFIdent e_ident;    // ELF identification
    Elf32_Half e_type;    // Object file type
    Elf32_Half e_machine; // Machine type
    Elf32_Word e_version; // Object file version
    Elf32_Addr e_entry;   // Entry point address
    Elf32_Off  e_phoff;   // Program header offset
    Elf32_Off  e_shoff;   // Section header offset
    Elf32_Word e_flags;   // Processor-specific flags
    Elf32_Half e_ehsize;  // ELF header size
    Elf32_Half e_phentsize;// Size of program header entry
    Elf32_Half e_phnum;    // Number of program header entries
    Elf32_Half e_shentsize;// Size of section header entry
    Elf32_Half e_shnum;    // Number of section header entries
    Elf32_Half e_shstrndx; // Section name string table index
} Elf32_Ehdr;

// ELF file types
enum Etype {
    ET_NONE = 0,            // No file type
    ET_REL = 1,             // Relocatable file
    ET_EXEC = 2,            // Executable file
    ET_DYN = 3,             // Shared object file
    ET_CORE = 4,            // Core file
    ET_LOPROC = 0xff00,     // Processor-specific
    ET_HIPROC = 0xffff      // Processor-specific
};

// ELF Machine types
enum Emachine {
    EM_NONE = 0,          // No machine
    EM_M32 = 1,           // AT&T WE 32100
    EM_SPARC = 2,         // SPARC
    EM_386 = 3,           // Intel 80386
    EM_68K = 4,           // Motorola 68000
    EM_88K = 5,           // Motorola 88000
    EM_860 = 7,           // Intel 80860
    EM_MIPS = 8           // MIPS RS3000
};

// ELF file versions
enum Eversion {
    EV_NONE = 0,          // Invalid version
    EV_CURRENT = 1        // Current version
};

// Program Header
typedef struct Elf32_Phdr {
    Elf32_Word p_type;    // Type of segment
    Elf32_Off  p_offset;  // Offset in file
    Elf32_Addr p_vaddr;   // Virtual address in memory
    Elf32_Addr p_paddr;   // Physical address (not used)
    Elf32_Word p_filesz;  // Size of segment in file
    Elf32_Word p_memsz;   // Size of segment in memory
    Elf32_Word p_flags;   // Segment attributes
    Elf32_Word p_align;   // Alignment of segment
} Elf32_Phdr;

// segment types
enum SegmentType {
    PT_NULL = 0,          // Unused segment
    PT_LOAD = 1,          // Loadable segment
    PT_DYNAMIC = 2,       // Dynamic linking information
    PT_INTERP = 3,        // Interpreter information
    PT_NOTE = 4,          // Auxiliary information
    PT_SHLIB = 5,         // Reserved
    PT_PHDR = 6,           // Program header table
    PT_LOPROC = 0x70000000,// Processor-specific
    PT_HIPROC = 0x7fffffff // Processor-specific
};

enum SegmentFlags {
    PF_X = 0x1,           // Execute
    PF_W = 0x2,           // Write
    PF_R = 0x4            // Read
};

typedef struct Elf32_Shdr {
    Elf32_Word sh_name;      // Section name (string table index)
    Elf32_Word sh_type;      // Section type
    Elf32_Word sh_flags;     // Section flags
    Elf32_Addr sh_addr;      // Section virtual address
    Elf32_Off  sh_offset;    // Section file offset
    Elf32_Word sh_size;      // Section size in bytes
    Elf32_Word sh_link;      // Link to another section
    Elf32_Word sh_info;      // Additional section information
    Elf32_Word sh_addralign; // Section alignment
    Elf32_Word sh_entsize;   // Entry size if section holds a table
} Elf32_Shdr;

enum SectionType {
    SHT_NULL = 0,           // Inactive section
    SHT_PROGBITS = 1,       // Program data
    SHT_SYMTAB = 2,         // Symbol table
    SHT_STRTAB = 3,         // String table
    SHT_RELA = 4,           // Relocation entries with addends
    SHT_HASH = 5,           // Symbol hash table
    SHT_DYNAMIC = 6,        // Dynamic linking information
    SHT_NOTE = 7,           // Notes
    SHT_NOBITS = 8,         // Program space with no data (bss)
    SHT_REL = 9,            // Relocation entries without addends
    SHT_SHLIB = 10,         // Reserved
    SHT_DYNSYM = 11,         // Dynamic linker symbol table
    SHT_LOPROC = 0x70000000, // Processor-specific
    SHT_HIPROC = 0x7fffffff,  // Processor-specific
    SHT_LOUSER = 0x80000000,
    SHT_HIUSER = 0xffffffff,
};

enum SectionFlags {
    SHF_WRITE = 0x1,        // Writable section
    SHF_ALLOC = 0x2,        // Occupies memory during execution
    SHF_EXECINSTR = 0x4,    // Executable instructions
    SHF_MASKPROC = 0xf0000000 // Processor-specific
};

typedef struct Elf32_Sym {
    Elf32_Word st_name;  // Symbol name (string table index)
    Elf32_Addr st_value; // Symbol value
    Elf32_Word st_size;  // Symbol size
    u8 st_info;             // Symbol type and binding
    u8 st_other;            // Symbol visibility
    Elf32_Half st_shndx; // Section index
} Elf32_Sym;

#define ELF32_ST_BIND(i)   ((i) >> 4)                       // bind
#define ELF32_ST_TYPE(i)   ((i) & 0xf)                      // type
#define ELF32_ST_INFO(b,t) (((b) << 4) + ((t) & 0xf))       // info = bind '+' type

enum SymbolBinding {
    STB_LOCAL = 0,         // Local symbol
    STB_GLOBAL = 1,        // Global symbol
    STB_WEAK = 2,          // Weak symbol
    STB_LOPROC = 13,       // Processor-specific
    STB_HIPROC = 15        // Processor-specific
};

enum SymbolType {
    STT_NOTYPE = 0,        // No type
    STT_OBJECT = 1,        // Data object
    STT_FUNC = 2,          // Function
    STT_SECTION = 3,       // Section
    STT_FILE = 4,          // File
    STT_LOPROC = 13,       // Processor-specific
    STT_HIPROC = 15        // Processor-specific
};

// check ELF header validity
static bool elf_validate(Elf32_Ehdr *ehdr) {
    if (memcmp(&ehdr->e_ident, "\177ELF\1\1\1", 7))
        return false;

    if (ehdr->e_type != ET_EXEC)
        return false;

    if (ehdr->e_machine != EM_386)
        return false;
    
    if (ehdr->e_version != EV_CURRENT)
        return false;

    if (ehdr->e_phentsize != sizeof(Elf32_Phdr))
        return false;

    return true;
}

static void load_segment(inode_t *inode, Elf32_Phdr *phdr) {
    assert(phdr->p_align == 0x1000);        // page aligned
    assert((phdr->p_vaddr & 0xfff) == 0);

    u32 vaddr = phdr->p_vaddr;

    // need pages, .bss may need more
    u32 count = div_round_up(MAX(phdr->p_memsz, phdr->p_filesz), PAGE_SIZE);

    // map pages
    for (size_t i = 0; i < count; i++) {
        u32 addr = vaddr + i * PAGE_SIZE;
        assert(addr >= USER_EXEC_ADDR && addr < USER_MMAP_ADDR);
        link_page(addr);
    }

    inode_read(inode, (char *)vaddr, phdr->p_filesz, phdr->p_offset);
    // 如果有.bss段，清零
    if (phdr->p_filesz < phdr->p_memsz) {
        memset((char *)vaddr + phdr->p_filesz, 0, phdr->p_memsz - phdr->p_filesz);
    }

    // if segment write protected, set page read-only
    if ((phdr->p_flags & PF_W) == 0) {
        for (size_t i = 0; i < count; i++) {
            u32 addr = vaddr + i * PAGE_SIZE;
            page_entry_t *entry = get_entry(addr, false);
            entry->write = false;
            entry->readonly = true;
            flush_tlb(addr);
        }
    }

    task_t *task = running_task();
    if (phdr->p_flags == (PF_R | PF_X)) {
        task->text = vaddr;      // 代码段起始地址
    } else if (phdr->p_flags == (PF_R | PF_W)) {
        task->data = vaddr;      // 数据段起始地址
    }

    // 更新进程的 end 地址，向上取整到页边界
    task->end = MAX(task->end, vaddr + count * PAGE_SIZE);
}

static u32 load_elf(inode_t *inode) {
    link_page(USER_EXEC_ADDR); // link first page for ELF header

    int n = 0;
    // read ELF header
    n = inode_read(inode, (char *)USER_EXEC_ADDR, sizeof(Elf32_Ehdr), 0);
    assert(n == sizeof(Elf32_Ehdr));

    Elf32_Ehdr *ehdr = (Elf32_Ehdr *)USER_EXEC_ADDR;
    if (!elf_validate(ehdr))
        return EOF;

    // read program headers
    Elf32_Phdr *phdr = (Elf32_Phdr *)(USER_EXEC_ADDR + sizeof(Elf32_Ehdr));
    n = inode_read(inode, (char *)phdr, ehdr->e_phnum * ehdr->e_phentsize, ehdr->e_phoff);

    Elf32_Phdr *ptr = phdr;
    for (size_t i = 0; i < ehdr->e_phnum; i++) {
        if (ptr->p_type != PT_LOAD)
            continue;
        load_segment(inode, ptr);
        ptr++;
    }

    return ehdr->e_entry;
}

static int count_argv(char *argv[]) {
    if (!argv)  
        return 0;

    int i = 0;
    while (argv[i])
        i++;
    return i;
}

static u32 copy_argv_envp(char *filename, char *argv[], char *envp[]) {
    int argc = count_argv(argv);
    int envc = count_argv(envp);

    // allocate kernel pages for argv and envp
    u32 pages = alloc_kpage(4);
    u32 pages_end = pages + 4 * PAGE_SIZE;

    // kernel temp stack top
    char *ktop = (char *)pages_end;
    // user stack top
    char *utop = (char *)USER_STACK_TOP;

    // kernel args
    char **argvk = (char **)alloc_kpage(1);
    argvk[argc] = NULL;

    // kernel envs
    char **envpk = argvk + argc + 1;
    envpk[envc] = NULL;

    int len = 0;

    //copy envp
    for (int i = envc - 1; i >= 0; i--) {
        len = strlen(envp[i]) + 1;
        ktop -= len;
        utop -= len;
        memcpy(ktop, envp[i], len);
        envpk[i] = utop;
    }

    // copy argv
    for (int i = argc - 1; i >= 0; i--) {
        len = strlen(argv[i]) + 1;
        ktop -= len;
        utop -= len;
        memcpy(ktop, argv[i], len);
        argvk[i] = utop;
    }

    // // copy filename argv[0]
    // len = strlen(filename) + 1;
    // ktop -= len;
    // utop -= len;
    // memcpy(ktop, filename, len);
    // argvk[0] = utop;

    // store argv and envp pointers
    ktop -= (envc + 1) * 4;
    memcpy(ktop, envpk, (envc + 1) * 4);

    ktop -= (argc + 1) * 4;
    memcpy(ktop, argvk, (argc + 1) * 4);

    ktop -= 4;
    *(int *)ktop = argc;

    assert((u32)ktop > pages);

    // copy to user stack
    len = (pages_end - (u32)ktop);
    utop = (char *)(USER_STACK_TOP - len);
    memcpy(utop, ktop, len);

    free_kpage((u32)argvk, 1);
    free_kpage(pages, 4);

    return (u32)utop;
}

extern int sys_brk();

int sys_execve(char *filename, char *argv[], char *envp[]) {
    inode_t *inode = namei(filename);
    int ret = EOF;
    if (!inode)
        goto rollback;

    if (!ISFILE(inode->desc->mode))
        goto rollback;
    
    if (!permission(inode, P_EXEC))
        goto rollback;
    
    task_t *task = running_task();
    strlcpy(task->name, filename, TASK_NAME_LEN);

    u32 top = copy_argv_envp(filename, argv, envp);

    task->end = USER_EXEC_ADDR;
    sys_brk(USER_EXEC_ADDR); // reset brk

    // load
    u32 entry = load_elf(inode);
    if (entry == EOF)
        goto rollback;

    sys_brk((u32)task->end); // set brk to new end  

    iput(task->iexec);
    task->iexec = inode;

    // 栈顶预留 intr_frame_t
    intr_frame_t *iframe = (intr_frame_t *)((u32)task + PAGE_SIZE - sizeof(intr_frame_t));

    memset(iframe, 0, sizeof(intr_frame_t));
    iframe->cs = USER_CODE_SELECTOR | 3;
    iframe->ds = USER_DATA_SELECTOR | 3;
    iframe->es = USER_DATA_SELECTOR | 3;
    iframe->fs = USER_DATA_SELECTOR | 3;
    iframe->gs = USER_DATA_SELECTOR | 3;
    iframe->ss = USER_DATA_SELECTOR | 3;

    iframe->edx = 0; // todo 动态链接器
    iframe->eip = entry;
    iframe->esp = top; // 用户栈顶

    iframe->eflags = (0x200 | 0x2); // IF=1 (开中断), IOPL=0

    // switch to user mode
    asm volatile (
        "movl %0, %%esp \n"             // load new intr_frame
        "jmp interrupt_exit\n"          // jump to interrupt_exit to iret
        :: "r"(iframe) : "memory");

rollback:
    iput(inode);
    return ret;
}
