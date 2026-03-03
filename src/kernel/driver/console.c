#include <drivers/console.h>
#include <hardware/io.h>
#include <xjos/interrupt.h>
#include <xjos/string.h>
#include <drivers/device.h>



// Forward declarations (avoid implicit declarations / order dependency)
static void scroll_up(console_t *con);
static void scroll_down(console_t *con);

static _inline void lf(console_t *con);
static _inline void cr(console_t *con);
static _inline void tab(console_t *con);
static _inline void bs(console_t *con);
static _inline void del(console_t *con);
static _inline void chr(console_t *con, char ch);


static console_t console;

static _inline void set_screen(console_t *con) {
    outb(CRT_ADDR_REG, CRT_START_ADDR_H);
    outb(CRT_DATA_REG, ((con->screen - con->mem_base) >> 9) & 0xff);
    outb(CRT_ADDR_REG, CRT_START_ADDR_L);
    outb(CRT_DATA_REG, ((con->screen - con->mem_base) >> 1) & 0xff);
}


static _inline void set_cursor(console_t *con) {
    outb(CRT_ADDR_REG,  CRT_CURSOR_H);
    // >> 9 == >>1(byte / 2) + >>8(set high byte)
    outb(CRT_DATA_REG, ((con->pos - con->mem_base) >> 9) & 0xff);
    outb(CRT_ADDR_REG, CRT_CURSOR_L);
    outb(CRT_DATA_REG, ((con->pos - con->mem_base) >> 1) & 0xff);
}


static void console_memset_16(u16 *dest, u16 val, size_t count) {
    // e.g. 0x0720 -> 0x07200720
    u32 val32 = (val << 16) | val;

    size_t count32 = count / 2;

    asm volatile(
        "cld\n"         // edi++
        "rep stosl\n"   // [edi] = eax, edi += 4
        : "+D"(dest), "+c"(count32)
        : "a"(val32)
        : "memory"
    );

    if (count % 2) {      // e.g. count = 3
        *dest = val;
    }
}


static _inline void set_xy(console_t *con, u32 x, u32 y) {
    // 越界检查
    if ((int)x < 0)
        x = 0;
    else if (x >= con->width)
        x = con->width - 1;

    if ((int)y < 0)
        y = 0;
    else if (y >= con->height)
        y = con->height - 1;

    con->x = x;
    con->y = y;
    con->pos = con->screen + y * con->row_size + (x << 1);
}


static _inline void save_cursor(console_t *con) {
    con->saved_x = con->x;
    con->saved_y = con->y;
}


static _inline void erase_screen(console_t *con, u16 *start, u32 count) {
    console_memset_16(start, con->erase, count);
}


void console_clear(console_t *con) {
    con->screen = con->mem_base;
    con->pos = con->mem_base;
    con->x = 0;
    con->y = 0;

    set_cursor(con);
    set_screen(con);
    erase_screen(con, (u16 *)con->mem_base, con->mem_size >> 1);
}


static _inline void tab(console_t *con) {
    int offset = 8 - (con->x & 7);  // & 7 = % 8, 8 - (x & 7) = next tab stop
    con->x += offset;
    con->pos += offset << 1;   // offset * 2
    if (con->x >= con->width) {
        con->x -= con->width;     // x = 0
        con->pos -= con->row_size;
        lf(con);
    }
}


static _inline void lf(console_t *con) {
    if (con->y + 1 < con->height) {
        con->y++;
        con->pos += con->row_size;
        return;
    }

    scroll_up(con);
}


static _inline void cr(console_t *con) {
    con->pos -= (con->x << 1);    // x * 2
    con->x = 0;
}


static _inline void bs(console_t *con) {
    if (!con->x)
        return;
    con->x--;
    con->pos -= 2;
    *(u16 *)con->pos = con->erase;
}


static _inline void del(console_t *con) {
    *(u16 *)con->pos = con->erase;
}

// 输出字符
static _inline void chr(console_t *con, char ch) {
    if (con->x >= con->width) {
        con->x -= con->width;
        con->pos -= con->row_size;
        lf(con);
    }

    *(u16 *)con->pos = (con->style << 8) | ch;
    con->pos += 2;
    con->x++;
}


static void scroll_up(console_t *con) {
    if ((con->screen + con->scr_size + con->row_size) >= con->mem_end) {
        memcpy((void *)con->mem_base, (void *)con->screen, con->scr_size);
        con->pos -= (con->screen - con->mem_base);
        con->screen = con->mem_base;
    }

    u16 *ptr = (u16 *)(con->screen + con->scr_size);
    erase_screen(con, ptr, con->width);

    con->screen += con->row_size;
    con->pos += con->row_size;
    set_screen(con);
}


static void scroll_down(console_t *con) {
    con->screen -= con->row_size;
    if (con->screen < con->mem_base) {
        con->screen = con->mem_base;
    }
    set_screen(con);
}


extern void start_beep();
 
// 正常状态
static _inline void state_normal(console_t *con, char ch) {
    switch (ch) {
        case NUL:
            break;
        case BEL:
            start_beep();
            break;
        case BS:
            bs(con);
            break;
        case HT:
            tab(con);
            break;
        case LF:
            lf(con);
            cr(con);
            break;
        case VT:
        case FF:
            lf(con);
            break;
        case CR:
            cr(con);
            break;
        case DEL:
            del(con);
            break;
        case ESC:
            con->state = STATE_ESC;
            break;
        default:
            chr(con, ch);
            break;
    }
}

// ESC状态
static _inline void state_esc(console_t *con, char ch) {
    switch (ch) {
        case '[':
            con->state = STATE_QUE;
            break;
        case 'E':
            lf(con);
            cr(con);
            break;
        case 'M':
            // go up
            break;
        case 'D':
            lf(con);
            break;
        case 'Z':
            // respond
            break;
        case '7':
            save_cursor(con);
            break;
        case '8':
            set_xy(con, con->saved_x, con->saved_y);
            break;
        default:
            break;
    }
}

// 参数状态
static _inline bool state_arg(console_t *con, char ch) {
    if (con->argc >= ARG_NR)
        return false;
    if (ch == ';') {
        con->argc++;
        return false;
    }

    if (ch >= '0' && ch <= '9') {
        con->args[con->argc] = con->args[con->argc] * 10 + (ch - '0');
        return false;
    }

    con->argc++;
    con->state = STATE_CSI;

    return true;
}

// 清屏
static _inline void csi_J(console_t *con) {
    int count = 0;
    int start = 0;

    switch (con->args[0]) {
        case 0:     // 从光标位置到屏幕末尾
            count = (con->screen + con->scr_size - con->pos) >> 1;
            start = con->pos;
            break;
        case 1:     // 从屏幕开始到光标位置
            count = (con->pos - con->screen) >> 1;
            start = con->screen;
            break;
        case 2:     // 整个屏幕
            count = con->scr_size >> 1;
            start = con->screen;
            break;
        default:
            return;
    }

    erase_screen(con, (u16 *)start, count);
}

// 删除行
static _inline void csi_K(console_t *con) {
    int count = 0;
    int start = 0;
    switch (con->args[0]) {
        case 0:
            count = con->width - con->x;
            start = con->pos;
            break;
        case 1:
            count = con->x;
            start = con->pos - (con->x << 1);
            break;
        case 2:
            count = con->width;
            start = con->pos - (con->x << 1);
            break;
        default:
            return;
    }

    erase_screen(con, (u16 *)start, count);
}

// 插入一行
static _inline void insert_line(console_t *con) {
    u16 *start = (u16 *)(con->screen + con->y * con->row_size);
    for (size_t i = 2; true; i++) {
        void *src = (void *)(con->screen + con->scr_size - (i * con->row_size));
        if (src < (void *)start)
            break;

        memcpy(src + con->row_size, src, con->row_size);
    }
    erase_screen(con, (u16 *)(con->screen + (con->y) * con->row_size), con->width);
}

// 插入多行
static _inline void csi_L(console_t *con) {
    int nr = con->args[0];
    if (nr > con->height)
        nr = con->height;
    else if (!nr)
        nr = 1;
    while (nr--) {
        insert_line(con);
    }
}

// 删除一行
static _inline void delete_line(console_t *con) {
    u16 *start = (u16 *)(con->screen + con->y * con->row_size);
    for (size_t i = 1; true; i++) {
        void *src = start + (i * con->row_size);
        if (src >= (void *)(con->screen + con->scr_size))
            break;

        memcpy(src - con->row_size, src, con->row_size);
    }
    erase_screen(con, (u16 *)(con->screen + con->scr_size - con->row_size), con->width);
}

// 删除多行
static _inline void csi_M(console_t *con) {
    int nr = con->args[0];
    if (nr > con->height)
        nr = con->height;
    else if (!nr)
        nr = 1;
    while (nr--) {
        delete_line(con);
    }
}

// 删除当前字符
static _inline void delete_char(console_t *con) {
    u16 *ptr = (u16 *)con->pos;
    u32 i = con->x;
    
    // 从当前位置开始，把后面的字符往前拉
    while (++i < con->width) {
        *ptr = *(ptr + 1);
        ptr++;
    }
    // 最后一个格子填空
    *ptr = con->erase;
}

// 删除多个字符
static _inline void csi_P(console_t *con) {
    int nr = con->args[0];
    
    int max = con->width - con->x;
    if (nr > max)
        nr = max;
    else if (!nr)
        nr = 1;
    while (nr--) {
        delete_char(con);
    }
}

// 插入字符
static _inline void insert_char(console_t *con) {
    // 1. 统一使用 u16 指针，指向当前光标所在的物理地址
    u16 *cursor_ptr = (u16 *)con->pos;
    
    // 2. 找到当前行最后一个字符的地址
    // 偏移量就是 (剩余宽度 - 1)
    u16 *last_ptr = cursor_ptr + (con->width - con->x - 1);

    // 3. 从后往前挪动，腾出当前光标位置
    while (last_ptr > cursor_ptr) {
        *last_ptr = *(last_ptr - 1);
        last_ptr--;
    }

    // 4. 在当前光标处插入“擦除字符”（通常是空格）
    // 必须强转为 u16 写入，保证颜色属性 0x07 也写进去
    *cursor_ptr = con->erase;
}

// 插入多个字符
static _inline void csi_at(console_t *con) {
    int nr = con->args[0];
    
    int max = con->width - con->x;
    if (nr > max)        
        nr = max;
    else if (!nr)
        nr = 1;
    while (nr--) {
        insert_char(con);
    }
}

// 修改样式
static _inline void csi_m(console_t *con) {
    con->style = 0;
    for (size_t i = 0; i < con->argc; i++) {
        if (con->args[i] == ST_NORMAL)
            con->style = STYLE;

        else if (con->args[i] == ST_BOLD)
            con->style = BOLD;

        else if (con->args[i] == BLINK)
            con->style |= BLINK;

        else if (con->args[i] == ST_REVERSE)
            con->style = (con->style >> 4) | (con->style << 4);

        else if (con->args[i] >= 30 && con->args[i] <= 37)
            con->style = con->style & 0xF8 | (con->args[i] - 30);

        else if (con->args[i] >= 40 && con->args[i] <= 47)
            con->style = con->style & 0x8F | ((con->args[i] - 40) << 4);
    }
    con->erase = (con->style << 8) | 0x20;
}

// CSI 状态
static _inline void state_csi(console_t *con, char ch) {
    con->state = STATE_NOR;
    switch (ch) {
        case 'G':
        case '`':
            if (con->args[0])
                con->args[0]--;
            set_xy(con, con->args[0], con->y);
            break;
        case 'A': // 光标上移一行或 n 行
            if (!con->args[0])
                con->args[0]++;
            set_xy(con, con->x, con->y - con->args[0]);
            break;
        case 'B':
        case 'e': // 光标下移一行或 n 行
            if (!con->args[0])
                con->args[0]++;
            set_xy(con, con->x, con->y + con->args[0]);
            break;
        case 'C':
        case 'a': // 光标右移一列或 n 列
            if (!con->args[0])
                con->args[0]++;
            set_xy(con, con->x + con->args[0], con->y);
            break;
        case 'D': // 光标左移一列或 n 列
            if (!con->args[0])
                con->args[0]++;
            set_xy(con, con->x - con->args[0], con->y);
            break;
        case 'E': // 光标下移一行或 n 行，并回到 0 列
            if (!con->args[0])
                con->args[0]++;
            set_xy(con, 0, con->y + con->args[0]);
            break;
        case 'F': // 光标上移一行或 n 行，并回到 0 列
            if (!con->args[0])
                con->args[0]++;
            set_xy(con, 0, con->y - con->args[0]);
            break;
        case 'd': // 设置行号
            if (con->args[0])
                con->args[0]--;
            set_xy(con, con->x, con->args[0]);
            break;
        case 'H': // 设置行号和列号
        case 'f':
            if (con->args[0])
                con->args[0]--;
            if (con->args[1])
                con->args[1]--;
            set_xy(con, con->args[1], con->args[0]);
            break;
        case 'J': // 清屏
            csi_J(con);
            break;
        case 'K': // 行删除
            csi_K(con);
            break;
        case 'L': // 插入行
            csi_L(con);
            break;
        case 'M': // 删除行
            csi_M(con);
            break;
        case 'P': // 删除字符
            csi_P(con);
            break;
        case '@': // 插入字符
            csi_at(con);
            break;
        case 'm': // 修改样式
            csi_m(con);
            break;
        case 'r': // 设置起始行号和终止行号
            break;
        case 's':
            save_cursor(con);
            break;
        case 'u':
            set_xy(con, con->saved_x, con->saved_y);
            break;
        default:
            break;
    }
}

int console_write(console_t *con, char *buf, u32 count) {
    bool intr = interrupt_disable(); // 禁止中断

    // console_t *con = &console;
    char ch;
    int nr = 0;
    while (nr++ < count) {
        ch = *buf++;
        switch (con->state) {
            case STATE_NOR:
                state_normal(con, ch);
                break;
            case STATE_ESC:
                state_esc(con, ch);
                break;
            case STATE_QUE:
                memset(con->args, 0, sizeof(con->args));
                con->argc = 0;
                con->ques = (ch == '?');
                con->state = STATE_ARG;
                break;
            case STATE_ARG:
                if (!state_arg(con, ch))
                    break;
            case STATE_CSI:
                state_csi(con, ch);
                break;
            default:
                break;
        }
    }
    set_cursor(con);
    // 恢复中断
    set_interrupt_state(intr);
    return nr;
}


int console_write_wrapper(void *dev, void *buf, size_t count, idx_t idx, int flags) {
    (void)idx;
    (void)flags;

    // 1. 还原上下文：将 void * 转换回 console_t *
    console_t *con = (console_t *)dev;
    
    // 2. 转换缓冲区指针
    char *buffer = (char *)buf;

    // 3. 完美匹配 console_write 的参数签名
    return (int)console_write(con, buffer, count);
}


void console_init() {
    console_t *con = &console;
    con->mem_base = MEM_BASE;
    con->mem_size = (MEM_SIZE / ROW_SIZE) * ROW_SIZE;
    con->mem_end = con->mem_base + con->mem_size;
    con->width = WIDTH;
    con->height = HEIGHT;
    con->row_size = con->width * 2;
    con->scr_size = con->width * con->height * 2;

    con->erase = ERASE;
    con->style = STYLE;
    console_clear(con);

    device_install(
        DEV_CHAR, DEV_CONSOLE,
        con, "console", 0,
        NULL, NULL, console_write_wrapper);
}