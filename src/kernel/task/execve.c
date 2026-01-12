#include <xjos/types.h>
#include <xjos/syscall.h>
#include <fs/fs.h>
#include <xjos/memory.h>
#include <libc/string.h>
#include <libc/assert.h>
#include <xjos/debug.h>


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


int sys_execve(char *filename, char *argv[], char *envp[]) {
    fd_t fd = open(filename, O_RDONLY, 0);
    if (fd == EOF) return EOF;

    // 1. 读取 ELF Header
    Elf32_Ehdr *ehdr = (Elf32_Ehdr *)alloc_kpage(1);
    lseek(fd, 0, SEEK_SET);
    read(fd, (char *)ehdr, sizeof(Elf32_Ehdr));

    // 2. 读取 Section Headers (节头表)
    Elf32_Shdr *shdr = (Elf32_Shdr *)alloc_kpage(1);
    lseek(fd, ehdr->e_shoff, SEEK_SET);
    read(fd, (char *)shdr, ehdr->e_shnum * ehdr->e_shentsize);

    // 3. 读取 .shstrtab (节名字符串表)
    char *shstrtab = (char *)alloc_kpage(1);
    Elf32_Shdr *shstr_hdr = &shdr[ehdr->e_shstrndx];
    lseek(fd, shstr_hdr->sh_offset, SEEK_SET);
    read(fd, shstrtab, shstr_hdr->sh_size);

    int strtab_idx = -1;
    int symtab_idx = -1;

    // 4. 线性遍历节头表，找到 .strtab 和 .symtab 的索引
    for (size_t i = 0; i < ehdr->e_shnum; i++) {
        char *name = shstrtab + shdr[i].sh_name;
        LOGK("[%d] section name: %s (type=%d)\n", i, name, shdr[i].sh_type);

        if (!strcmp(name, ".strtab")) {
            strtab_idx = i;
        } else if (!strcmp(name, ".symtab")) {
            symtab_idx = i;
        }
    }

    // 5. 处理符号名字符串池 (.strtab)
    char *strtab = NULL;
    if (strtab_idx != -1) {
        strtab = (char *)alloc_kpage(1);
        Elf32_Shdr *sptr = &shdr[strtab_idx];
        lseek(fd, sptr->sh_offset, SEEK_SET);
        read(fd, strtab, sptr->sh_size);
        
        // 测试打印：打印池子里的所有字符串
        char *nn = strtab + 1; // 跳过第一个 \0
        while (nn < strtab + sptr->sh_size && *nn) {
            LOGK("strtab_pool_content: %s\n", nn);
            nn += strlen(nn) + 1;
        }
    }

    // 6. 处理符号表 (.symtab)
    if (symtab_idx != -1 && strtab != NULL) {
        Elf32_Sym *symtab = (Elf32_Sym *)alloc_kpage(1);
        Elf32_Shdr *sym_hdr = &shdr[symtab_idx];
        lseek(fd, sym_hdr->sh_offset, SEEK_SET);
        read(fd, (char *)symtab, sym_hdr->sh_size);

        int count = sym_hdr->sh_size / sym_hdr->sh_entsize;
        for (int i = 0; i < count; i++) {
            Elf32_Sym *s = &symtab[i];
            
            char *sym_name = (s->st_name == 0) ? "(no name)" : &strtab[s->st_name];

            LOGK("Sym[%d] Val: 0x%p Size: %d Bind: %d Type: %d Name: %s\n",
                 i,
                 s->st_value,
                 s->st_size,
                 ELF32_ST_BIND(s->st_info),
                 ELF32_ST_TYPE(s->st_info),
                 sym_name);
        }
        free_kpage((u32)symtab, 1);
    }

    // 7. 清理内存
    if (strtab) free_kpage((u32)strtab, 1);
    free_kpage((u32)shstrtab, 1);
    free_kpage((u32)shdr, 1);
    free_kpage((u32)ehdr, 1);
    
    close(fd);
    return 0;
}

