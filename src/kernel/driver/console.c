#include <drivers/console.h>
#include <hardware/io.h>
#include <xjos/interrupt.h>
#include <libc/string.h>

static void get_screen();
static void set_screen();
static void get_cursor();
static void set_cursor();


// command hanlder
static void command_bs();
static void command_lf();
static void command_cr();


static void srcoll_up();


// static variables -> oop private
static u32 screen; // current screen buffer(memory)

static u32 pos; // current cursor position

static u32 x, y;

static u8 attr = 7; // attribute byte
static u16 erase = 0x0720; // erase byte


static void get_screen() {
    outb(CRT_ADDR_REG, CRT_START_ADDR_H);
    screen = inb(CRT_DATA_REG) << 8;
    outb(CRT_ADDR_REG, CRT_START_ADDR_L);
    screen |= inb(CRT_DATA_REG);

    screen <<= 1; // double the size of memory
    screen += MEM_BASE; // add base address
}


static void set_screen() {
    outb(CRT_ADDR_REG, CRT_START_ADDR_H);
    // >> 9 == >>1(byte / 2) + >>8(set high byte)
    outb(CRT_DATA_REG, ((screen - MEM_BASE) >> 9) & 0xff);
    outb(CRT_ADDR_REG, CRT_START_ADDR_L);
    outb(CRT_DATA_REG, ((screen - MEM_BASE) >> 1) & 0xff);
}


static void get_cursor() {
    outb(CRT_ADDR_REG, CRT_CURSOR_H);
    pos = inb(CRT_DATA_REG) << 8;
    outb(CRT_ADDR_REG, CRT_CURSOR_L);
    pos |= inb(CRT_DATA_REG);

    pos <<= 1; // double the size of memory
    pos += MEM_BASE; // add base address

    u32 delta = (pos - screen) >> 1; // calculate offset
    x = delta % WIDTH;
    y = delta / WIDTH;
}


static void set_cursor() {
    outb(CRT_ADDR_REG,  CRT_CURSOR_H);
    // >> 9 == >>1(byte / 2) + >>8(set high byte)
    outb(CRT_DATA_REG, ((pos - MEM_BASE) >> 9) & 0xff);
    outb(CRT_ADDR_REG, CRT_CURSOR_L);
    outb(CRT_DATA_REG, ((pos - MEM_BASE) >> 1) & 0xff);
}


void console_init() {
    console_clear();
}


void console_clear() {
    screen = MEM_BASE;
    pos = MEM_BASE;
    x = y = 0;
    
    u16 *ptr = (u16 *)MEM_BASE;
    while (ptr < (u16 *)MEM_END) {
        *ptr++ = erase;
    }

    set_cursor();
    set_screen();
}


static void command_bs() {
    if (x) {
        x--;
        pos -= 2;
        *(u16 *)pos = erase;
    }
}


static void command_lf() {
    if (y + 1 < HEIGHT) {
        y++;
        pos += ROW_SIZE;

    } else {
        srcoll_up();
    }
}


static void command_cr() {
    pos -= (x << 1);
    x = 0;
}


static void srcoll_up() {
    if (screen + SCR_SIZE + ROW_SIZE < MEM_END) {
        u16 *ptr = (u16 *)(screen + SCR_SIZE);
        for (size_t i = 0; i < WIDTH; i++) {
            *ptr++ = erase;     // empty
        }

        screen += ROW_SIZE;
        pos += ROW_SIZE;
    } else {
        pos -= (screen - MEM_BASE);     // ! Concurrency bug 

        // copy 2-25 -> 1-24
        memcpy((void*)MEM_BASE, (void*)(screen + ROW_SIZE), SCR_SIZE - ROW_SIZE);

        // clear last row
        u16 *ptr = (u16 *)(MEM_BASE + SCR_SIZE - ROW_SIZE);
        for (size_t i = 0; i < WIDTH; i++) {
            *ptr++ = erase;
        }
        // pos = mem_base + (y * row_size + x);
        // new pos = pos - offset(screen - mem_base)
        screen = MEM_BASE;
    }

    set_screen();
}


extern void start_beep();
 

int32 console_write(const char *buf, u32 count) {
    bool intr = interrupt_disable();            // should turn off interrupt

    char ch;
    int32 nr = 0;
    while (nr++ < count) {
        ch = *buf++;
        switch (ch) {
            case NUL:       // null character
                break;
            case ENQ:       
                break;
            case ESC:       // esc
                break;
            case BEL:       // \a
                start_beep();
                break;
            case BS:        // \b
                command_bs();
                break;      
            case HT:        // \t   
                break;
            case LF:        // \n 
                command_lf();
                command_cr();
                break; 
            case DEL:
                command_bs();
                break;
            default:
            if (x >= WIDTH) {
                x -= WIDTH;     // x = 0
                pos -= ROW_SIZE;
                command_lf();
            }
            
            *(u16 *)pos = (attr << 8) | ch; 

            pos += 2;
            x++;

            break;
        }
    }

    set_cursor();

    set_interrupt_state(intr);
    return nr;
}