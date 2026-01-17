#include <xjos/rtc.h>
#include <xjos/debug.h>
#include <hardware/io.h>
#include <xjos/interrupt.h>
#include <xjos/time.h>
#include <xjos/assert.h>
#include <xjos/stdlib.h>


#define LOGK(fmt, args...) DEBUGK(fmt, ##args)



#define CMOS_ADDR 0x70  // cmos address
#define CMOS_DATA 0x71  // cmos data


#define CMOS_SECOND 0x01
#define CMOS_MINUTE 0x03
#define CMOS_HOUR   0x05

#define CMOS_A 0x0a
#define CMOS_B 0x0b
#define CMOS_C 0x0c
#define CMOS_D 0x0d
#define CMOS_NMI 0x80



u8 cmos_read(u8 addr) {
    outb(CMOS_ADDR, CMOS_NMI | addr);
    return inb(CMOS_DATA);
}


void cmos_write(u8 addr, u8 data) {
    outb(CMOS_ADDR, CMOS_NMI | addr);
    outb(CMOS_DATA, data);
}


extern void start_beep();


void rtc_handler(int vector) {
    // rtc int
    assert(vector == 0x28);

    send_eoi(vector);

    // read reg C, allow cmos produces interrupts
    cmos_read(CMOS_C);

    set_alarm(1);

    start_beep();
}


// int set times
void set_alarm(u32 secs) {
    tm time;
    time_read(&time);

    u8 sec = secs % 60;
    secs /= 60;

    u8 min = secs % 60;
    secs /= 60;

    u32 hour = secs;
    

    time.tm_sec += sec;
    // +1
    if (time.tm_sec >= 60) {
        time.tm_sec %= 60;
        time.tm_min += 1;
    }

    time.tm_min += min;
    if (time.tm_min >= 60) {
        time.tm_min %= 60;
        time.tm_hour += 1;
    }

    time.tm_hour += hour;
    if (time.tm_hour >= 24) {
        time.tm_hour %= 24;
        // Todo: add 1 day, and month, year
    }

    cmos_write(CMOS_HOUR, bin_to_bcd(time.tm_hour));
    cmos_write(CMOS_MINUTE, bin_to_bcd(time.tm_min));
    cmos_write(CMOS_SECOND, bin_to_bcd(time.tm_sec));

    cmos_write(CMOS_B, 0b00100010);
    cmos_read(CMOS_C);
}


void rtc_init() {

    // open alarm int
    // cmos_write(CMOS_B, 0b01000010);

    // set rtc hz
    // outb(CMOS_A, (inb(CMOS_A) & 0xf) | 0b1110);

    set_interrupt_handler(IRQ_RTC, rtc_handler);
    set_interrupt_mask(IRQ_RTC, true);
    set_interrupt_mask(IRQ_CASCADE, true);
}