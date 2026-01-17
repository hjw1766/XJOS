#include <xjos/stdlib.h>



void delay(u32 count) {
    while(count--)
        asm volatile("nop");
}


void hang() {
    while(1);
}


// exp.   25 -> (00011001), but bcd -> (00100101)
u8 bcd_to_bin(u8 value) {
    return (value & 0x0f) + (value >> 4) * 10;
}


u8 bin_to_bcd(u8 value) {
    return (value / 10) * 0x10 + (value % 10);
}


// num / szie copies
u32 div_round_up(u32 num, u32 size) {
    return (num + size - 1) / size;
}


int atoi(const char *str) {
    if (str == NULL)
        return 0;

    while (*str == ' ' || *str == '\t')
        str++;
    
    int sign = 1;
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }

    // parse number
    int result = 0;
    while (*str >= '0' && *str <= '9') { 
        result = result * 10 + (*str - '0');
        str++;
    }

    return sign * result;
}
