#ifndef XJOS_CONSOLE_H
#define XJOS_CONSOLE_H

#include <xjos/types.h>

#define CRT_ADDR_REG 0x3D4 // CRT(6845)
#define CRT_DATA_REG 0x3D5

#define CRT_START_ADDR_H 0xC // video memory start address - high byte
#define CRT_START_ADDR_L 0xD // video memory start address - low byte
#define CRT_CURSOR_H 0xE     // currsor position - high byte
#define CRT_CURSOR_L 0xF     // currsor position - low byte

#define MEM_BASE 0xB8000              // video memory start address
#define MEM_SIZE 0x4000               // video memory size
#define MEM_END (MEM_BASE + MEM_SIZE) // video memory end address
#define WIDTH 80                      // screen width
#define HEIGHT 25                     // screen height
#define ROW_SIZE (WIDTH * 2)          // bytes per row
#define SCR_SIZE (ROW_SIZE * HEIGHT)  // total screen size

#define NUL 0x00
#define ENQ 0x05
#define ESC 0x1B // ESC
#define BEL 0x07 // \a
#define BS 0x08  // \b
#define HT 0x09  // \t
#define LF 0x0A  // \n
#define VT 0x0B  // \v
#define FF 0x0C  // \f
#define CR 0x0D  // \r
#define DEL 0x7F

enum ST {
    ST_NORMAL = 0,
    ST_BOLD = 1,
    ST_BLINK = 5,
    ST_REVERSE = 7,
};

#define STYLE 7
#define BLINK 0x80
#define BOLD 0x0F
#define UNDER 0x0F

enum COLOR {
    BLACK = 0,
    BLUE = 1,
    GREEN = 2,
    CYAN = 3,
    RED = 4,
    MAGENTA = 5,
    YELLOW = 6,
    WHITE = 7,
};

#define ERASE 0x0720
#define ARG_NR 16

enum state {
    STATE_NOR,
    STATE_ESC,
    STATE_QUE,
    STATE_ARG,
    STATE_CSI,
};

typedef struct console_t {
    u32 mem_base;   // 内存基地址
    u32 mem_size;   // 内存大小
    u32 mem_end;    // 内存结束地址

    u32 screen;     // 当前屏幕起始地址
    u32 scr_size;   // 屏幕大小

    union {
        u32 pos;    // 当前光标位置
        char *ptr;  // 位置指针
    };

    u32 x;     // 当前光标坐标
    u32 y;
    u32 saved_x; // 保存的光标坐标
    u32 saved_y;
    u32 width; // 屏幕宽高
    u32 height;
    u32 row_size;   // 每行字节数

    u8 state;   // 转义序列状态
    u32 args[ARG_NR]; // 转义序列参数
    u32 argc;   // 转义序列参数数量
    u32 ques;

    u16 erase;  // 清屏字符
    u8 style;   // 当前样式
} console_t;


void console_init();
void console_clear(console_t *con);









#endif /* XJOS_CONSOLE_H */