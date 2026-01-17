#include <xjos/string.h>

/*
 * strcpy - Copy a string
 * @dest: Destination string
 * @src: Source string
 * * Optimization:
 * Direct call to optimized memcpy and strlen.
 * This is faster than a byte-by-byte loop because memcpy uses hardware 'rep movsl'.
 */
char *strcpy(char *dest, const char *src) {
    // No NULL check: Let it crash (Fail Fast)
    memcpy(dest, src, strlen(src) + 1);
    return dest;
}


/*
 * strcat - Concatenate two strings
 * @dest: Destination string
 * @src: Source string
 */
char *strcat(char *dest, const char *src) {
    // 1. Find the end of dest string
    char *ptr = dest + strlen(dest);

    // 2. Append src to the end
    strcpy(ptr, src);

    return dest;
}


/*
 * strlcpy - Copy a string with size limit
 * @dst: Destination buffer
 * @src: Source string
 * @size: Size of destination buffer
 * Returns: Total length of the string it tried to create (length of src)
 */
size_t strlcpy(char *dst, const char *src, size_t size) {
    // 1. Get source length first (SWAR optimized)
    size_t srclen = strlen(src);

    // 2. Check if we have buffer space
    if (size > 0) {
        // Calculate actual bytes to copy: min(srclen, size - 1)
        size_t copylen = (srclen >= size) ? size - 1 : srclen;

        // 3. Use ASM optimized memcpy
        memcpy(dst, src, copylen);

        // 4. Null-terminate the result
        dst[copylen] = EOS;
    }

    return srclen; 
}


/*
 * strlen - Calculate the length of a string
 * @str: Input string
 * * Optimization: 
 * SWAR (SIMD Within A Register) algorithm.
 * Reads 4 bytes at a time to check for null terminator.
 */
size_t strlen(const char *str) {
    const char *char_ptr;
    const u32 *long_ptr;
    u32 longword, himagic, lomagic;

    // 1. align handle
    // Loop until pointer is 4-byte aligned
    for (char_ptr = str; ((u32)char_ptr & 3) != 0; char_ptr++) {
        if (*char_ptr == EOS)
            return char_ptr - str;
    }

    // 2. convert to long pointer
    long_ptr = (const u32 *)char_ptr;

    // Magic bits for finding 0 in a 32-bit word
    himagic = 0x80808080L;
    lomagic = 0x01010101L;

    for (;;) {
        longword = *long_ptr++;

        // Check if any byte in longword is zero
        // Formula: ((x - 0x01) & ~x & 0x80)
        if (((longword - lomagic) & ~longword & himagic) != 0) {
            
            // Found a potential zero, revert to byte checking
            const char *cp = (const char *)(long_ptr - 1);

            if (cp[0] == EOS) return cp - str;
            if (cp[1] == EOS) return cp - str + 1;
            if (cp[2] == EOS) return cp - str + 2;
            return cp - str + 3;
        }
    }
}


/*
 * strcmp - Compare two strings
 * * Optimization:
 * 1. SWAR: Compares 4 bytes (u32) at a time using stride logic.
 * 2. Checks for Null terminator inside the 32-bit word.
 */
int strcmp(const char *lhs, const char *rhs) {
    const u32 *l_u32, *r_u32;
    
    // 1. Handle unaligned bytes until lhs is 4-byte aligned
    // (Ideally we align both, but aligning one helps)
    while (((u32)lhs & 3) || ((u32)rhs & 3)) {
        if (*lhs != *rhs || *lhs == EOS) {
            return (int)*(const u8 *)lhs - (int)*(const u8 *)rhs;
        }
        lhs++;
        rhs++;
    }

    // 2. Fast Path: 32-bit comparison
    l_u32 = (const u32 *)lhs;
    r_u32 = (const u32 *)rhs;

    u32 himagic = 0x80808080L;
    u32 lomagic = 0x01010101L;

    while (1) {
        u32 l_val = *l_u32;
        u32 r_val = *r_u32;

        if (l_val != r_val) {
            // Difference found within this word
            break;
        }

        // They are equal, but did we hit the end of the string?
        // Check for zero byte in l_val
        if (((l_val - lomagic) & ~l_val & himagic) != 0) {
            // Logic: Strings are identical up to here, and we found a terminator.
            // They are fully equal.
            return 0;
        }

        l_u32++;
        r_u32++;
    }

    // 3. Fallback: Byte-by-byte for the mismatch/tail
    const u8 *l = (const u8 *)l_u32;
    const u8 *r = (const u8 *)r_u32;

    while (*l == *r && *l != EOS) {
        l++;
        r++;
    }

    return (int)*l - (int)*r;
}


/*
 * strchr - Find first occurrence of character
 * * Optimization:
 * SWAR algorithm checking for both Null terminator AND target char
 * in parallel (4 bytes at a time).
 */
char *strchr(const char *str, int ch) {
    const u32 *long_ptr;
    u32 longword, magic_zero, magic_char;
    u32 himagic = 0x80808080L;
    u32 lomagic = 0x01010101L;
    u8 c = (u8)ch;

    // 1. Align to 4 bytes
    for (; ((u32)str & 3) != 0; str++) {
        if (*str == c) return (char *)str;
        if (*str == EOS) return NULL;
    }

    long_ptr = (const u32 *)str;
    
    // Prepare mask: e.g., if looking for 'A' (0x41), mask is 0x41414141
    u32 char_mask = c | (c << 8) | (c << 16) | (c << 24);

    for (;;) {
        longword = *long_ptr++;

        // Check for NULL (EOS)
        // (x - 0x01) & ~x & 0x80
        magic_zero = (longword - lomagic) & ~longword & himagic;

        // Check for Char
        // XOR with mask creates 0x00 where char matches
        u32 xor_word = longword ^ char_mask;
        magic_char = (xor_word - lomagic) & ~xor_word & himagic;

        // If either logic found a "zero-like" byte
        if ((magic_zero | magic_char) != 0) {
            // Revert to byte check on the original string
            const char *cp = (const char *)(long_ptr - 1);
            
            // We check 4 bytes manually now
            if (cp[0] == c) return (char *)&cp[0];
            if (cp[0] == EOS) return NULL;
            
            if (cp[1] == c) return (char *)&cp[1];
            if (cp[1] == EOS) return NULL;
            
            if (cp[2] == c) return (char *)&cp[2];
            if (cp[2] == EOS) return NULL;
            
            if (cp[3] == c) return (char *)&cp[3];
            return NULL; // Should technically catch EOS at 4th byte if aligned logic holds
        }
    }
}


/*
 * strrchr - Find last occurrence of character
 * @str: String to search
 * @ch: Character to find
 * * Optimization:
 * Search backwards from the end of the string.
 * * FIX:
 * Fixed potential pointer underflow UB where p could become (str - 1).
 */
char *strrchr(const char *str, int ch) {
    // 1. Find the end of the string using optimized strlen
    const char *p = str + strlen(str);

    // 2. Scan backwards including the null terminator
    while (1) {
        if (*p == (char)ch) {
            return (char *)p; // Found
        }

        // Check boundary BEFORE decrementing to avoid UB
        if (p == str) {
            break;
        }
        
        p--;
    }

    return NULL; // Not found
}


/*
 * memcmp - Compare memory blocks
 * @lhs: Left buffer
 * @rhs: Right buffer
 * @count: Number of bytes to compare
 * * Optimization:
 * Use x86 hardware instruction `repz cmpsb`.
 */
int memcmp(const void *lhs, const void *rhs, size_t count) {
    int res = 0;
    int d0, d1, d2;

    if (count == 0)
        return 0;
    
    __asm__ __volatile__(
        /*
            cmpsb: compare [esi] and [edi] byte
            repz: repeat while equal(zero flag=1) and ecx!=0
            if not equal or ecx=0, stop
        */
        "repz ; cmpsb \n\t"     /* Compare bytes, ecx-- */
        
        "je 1f \n\t"            /* ecx==0 (equal), jmp 1 */
        
        "sbbl %0, %0 \n\t"      /* Trick: sbb (subtract with borrow) */
                                /* If lhs < rhs (Carry=1), res = -1 */
                                /* If lhs > rhs (Carry=0), res = 0 */
                                
        "orl $1, %0 \n\t"       /* Convert 0 to 1. Result is now 1 or -1 */
        "1:"
        
        : "=&a"(res), "=&c"(d0), "=&S"(d1), "=&D"(d2)
        :   "0"(0),             /* res = 0 initially */
            "1"(count),         /* count to ecx */
            "2"(lhs),           /* lhs to esi */
            "3"(rhs)            /* rhs to edi */
        : "memory", "cc"
    );

    return res;
}


/*
 * memset - Fill memory with a constant byte
 * @dest: Destination pointer
 * @ch: Character pattern
 * @count: Number of bytes
 * * Optimization:
 * Arithmetic moved to ASM to save registers.
 * Uses `rep stosl` for 4-byte bulk fill.
 * * FIX: Use EDX as scratch register to preserve EAX (which holds val).
 */
void *memset(void *dest, int ch, size_t count) {
    int d0, d1, d2; // d2 maps to edx

    // exp byte to 32-bit word
    // ch = 0xAB -> val = 0xABABABAB
    u32 val = (u8)ch;
    val = (val << 24) | (val << 16) | (val << 8) | val;

    __asm__ __volatile__(
        /* Logic:
           1. Save original count to EDX (Don't touch EAX, it holds val!)
           2. ecx = count / 4
           3. Fill 4 bytes at a time using stosl (uses EAX value)
           4. edx = original_count % 4
           5. Fill remaining bytes using stosb (uses AL value)
        */
        "movl %%ecx, %%edx \n\t"    /* Backup count to edx */
        "shrl $2, %%ecx \n\t"       /* ecx = count >> 2 */
        
        "rep ; stosl \n\t"          /* Fill 32-bit words. [edi] = eax, ecx-- */
        
        "andl $3, %%edx \n\t"       /* edx = count & 3 */
        "jz 1f \n\t"                /* If remainder is 0, jump to end */
        
        "movl %%edx, %%ecx \n\t"    /* ecx = remainder */
        "rep ; stosb \n\t"          /* Fill remaining bytes. [edi] = al, ecx-- */
        "1:"
        
        : "=&c"(d0), "=&D"(d1), "=&d"(d2) /* Output: d2 maps to edx now */
        :   "a"(val),           /* Input: val MUST be in eax for stosl */
            "0"(count),         /* Input: count in ecx */
            "1"((long)dest)     /* Input: dest in edi */
        : "memory"
    );

    return dest;
}


/*
 * memcpy - Copy memory area
 * @dest: Destination buffer
 * @src: Source buffer
 * @count: Number of bytes
 * * Optimization:
 * 1. Arithmetic moved to ASM to reduce register pressure.
 * 2. Uses `rep movsl` for hardware-accelerated block copy.
 */
void *memcpy(void *dest, const void *src, size_t count) {
    int d0, d1, d2;
    u32 tmp; // Scratch register for eax

    /* 1. first copy (count / 4) bytes */
    /* 2. then copy (count % 4) byte */
    __asm__ __volatile__(
        /*
            rep: repeat the following instruction
            movsl: Move String Long (4 bytes at a time)
            [esi] -> [edi], esi+=4, edi+=4, ecx--
        */
        
        // Optimization: Calculate /4 and %4 inside ASM
        "movl %%ecx, %%eax \n\t"    /* Backup count to eax */
        "shrl $2, %%ecx \n\t"       /* ecx = count >> 2 (/4) */
        
        "rep ; movsl \n\t"          /* Block copy 4 bytes */

        /* handle tail, count % 4 */   
        "andl $3, %%eax \n\t"       /* eax = count & 3 (%4) */
        "jz 1f \n\t"                /* ecx==0, jmp 1 */
        
        "movl %%eax, %%ecx \n\t"    /* Restore remainder to ecx */
        "rep ; movsb \n\t"          /* copy byte by byte */
        "1:"

        /* output operands */
        : "=&c"(d0), "=&D"(d1), "=&S"(d2), "=&a"(tmp)
        /* d0 -> ecx, d1 -> edi, d2 -> esi, tmp -> eax */

        /* input operands */
        : "0"(count),           /* %0 -> ecx (Pass Full Count) */
          "1"((long)dest),      /* %1 -> edi */
          "2"((long)src)        /* %2 -> esi */

        : "memory"
    );
    return dest;
}


/*
 * memchr - Find a character in a memory block
 * @ptr: Pointer to memory block
 * @ch: Character to find
 * @count: Size to search
 * * Optimization:
 * Uses hardware "repne scasb" instruction.
 * Scans memory at hardware speed until byte matches or count reaches 0.
 */
void *memchr(const void *ptr, int ch, size_t count) {
    if (count == 0) return NULL;

    void *result;
    
    __asm__ __volatile__(
        "repne ; scasb \n\t"  // Scan string byte: compare AL with [EDI], increment EDI, decrement ECX
                              // Stops if ECX==0 or ZF=1 (Equal)
        
        "jne 1f \n\t"         // If ZF=0 (Not Equal) after loop, jump to not found
        
        "movl %%edi, %0 \n\t" // Found: EDI points to byte AFTER match
        "decl %0 \n\t"        // Adjust pointer back by 1
        "jmp 2f \n\t"
        
        "1: \n\t"
        "xorl %0, %0 \n\t"    // Not found: return NULL (0)
        "2:"

        : "=r"(result)        // Output
        : "D"(ptr),           // Input: EDI = ptr
          "a"(ch),            // Input: EAX (AL) = character to find
          "c"(count)          // Input: ECX = count
        : "cc", "memory"      // Clobbers flags
    );

    return result;
}