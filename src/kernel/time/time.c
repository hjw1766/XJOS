#include <xjos/time.h>
#include <xjos/debug.h>
#include <xjos/stdlib.h>
#include <hardware/io.h>
#include <xjos/rtc.h>


#define LOGK(fmt, args...) DEBUGK(fmt, ##args)


#define CMOS_ADDR 0x70
#define CMOS_DATA 0x71

#define CMOS_SECOND 0x00    // Seconds (0-59)
#define CMOS_MINUTE 0x02    // Minutes (0-59)
#define CMOS_HOUR   0x04    // Hours (0-23)
#define CMOS_WEEKDAY 0x06    // Day of week (1-7, 1=Sunday)
#define CMOS_DAY    0x07    // Day of month (1-31)
#define CMOS_MONTH  0x08    // Month (1-12)
#define CMOS_YEAR   0x09    // Year (0-99)
#define CMOS_CENTURY 0x32    // Century (0-99)
#define CMOS_NMI 0x80

#define MINUTE 60         // 1 minute in seconds
#define HOUR (60 * MINUTE) // 1 hour in seconds
#define DAY (24 * HOUR)    // 1 day in seconds
#define YEAR (365 * DAY)   // 1 year in seconds


// days per month in non-leap year
static int month[13] = {
    0,
    31,
    28,
    31,
    30,
    31,
    30,
    31,
    31,
    30,
    31,
    30,
    31
};


time_t startup_time;
int century;

static bool is_leap_year(int year);
static int elapsed_leap_years(int year);


int get_yday(tm *time) {
    int res = 0;

    // add days for previous months
    for (int i = 1; i < time->tm_mon; i++) {
        res += month[i];
    }

    // add days for current month
    res += time->tm_mday;

    // handle leap year
    int full_year = time->tm_year + 1900;
    if (is_leap_year(full_year) && (time->tm_mon > 2)) {
        res += 1;
    }

    return res;
}

static bool is_leap_year(int year) {
    return ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
}

// Count leap years from 1970 to year-1
static int elapsed_leap_years(int year) {
    // Count leap years from 1 to year-1
    int count_to_year = (year - 1) / 4 - (year - 1) / 100 + (year - 1) / 400;
    // Count leap years from 1 to 1969
    int count_to_1970 = 1969 / 4 - 1969 / 100 + 1969 / 400;
    return count_to_year - count_to_1970;
}

// Convert time_t to tm structure
void localtime(time_t stamp, tm *time) {
    time->tm_sec = stamp % 60;
    time_t remain = stamp / 60; // total minutes

    time->tm_min = remain % 60;
    remain = remain / 60;       // total hours

    time->tm_hour = remain % 24;
    time_t days = remain / 24;  // total days since 1970-01-01

    time->tm_wday = (days + 4) % 7; // Jan 1, 1970 was a Thursday

    // Calculate year
    int year = 1970;
    while (1) {
        int days_in_year = is_leap_year(year) ? 366 : 365;
        if (days < days_in_year)
            break;
        days -= days_in_year;
        year++;
    }
    time->tm_year = year - 1900;
    time->tm_yday = days + 1;  // day of year (1-366)

    // Calculate month and day
    int mon = 1;
    while (mon <= 12) {
        int days_in_month = month[mon];
        if (mon == 2 && is_leap_year(year))
            days_in_month = 29;
        if (days < days_in_month)
            break;
        days -= days_in_month;
        mon++;
    }

    time->tm_mon = mon;
    time->tm_mday = days + 1; // day of month starts from 1
}

time_t mktime(tm *time) {
    time_t res;
    int year = time->tm_year + 1900;

    // sum years in seconds
    res = (year - 1970) * YEAR;
    res += elapsed_leap_years(year) * DAY;

    // sum months in seconds
    for (int i = 1; i < time->tm_mon; i++) {
        res += month[i] * DAY;
    }

    // handle leap year for current year (add Feb 29)
    if (time->tm_mon > 2 && is_leap_year(year)) {
        res += DAY;
    }

    // sum days, hours, minutes, seconds
    res += DAY * (time->tm_mday - 1);
    res += HOUR * time->tm_hour;
    res += MINUTE * time->tm_min;
    res += time->tm_sec;

    return res;
}


void time_read_bcd(tm *time) {
    do
    {
        time->tm_sec = cmos_read(CMOS_SECOND);
        time->tm_min = cmos_read(CMOS_MINUTE);
        time->tm_hour = cmos_read(CMOS_HOUR);
        time->tm_mday = cmos_read(CMOS_DAY);
        time->tm_mon = cmos_read(CMOS_MONTH);
        time->tm_year = cmos_read(CMOS_YEAR);
        time->tm_wday = cmos_read(CMOS_WEEKDAY);
        // time->tm_yday = get_yday(time);
        century = cmos_read(CMOS_CENTURY);
    } while (time->tm_sec != cmos_read(CMOS_SECOND));
}


void time_read(tm *time) {
    time_read_bcd(time);    // time in BCD format

    time->tm_sec = bcd_to_bin(time->tm_sec);
    time->tm_min = bcd_to_bin(time->tm_min);
    time->tm_hour = bcd_to_bin(time->tm_hour);
    time->tm_mday = bcd_to_bin(time->tm_mday);
    time->tm_mon = bcd_to_bin(time->tm_mon);
    // time->tm_year = bcd_to_bin(time->tm_year);
    time->tm_wday = bcd_to_bin(time->tm_wday);
    time->tm_yday = get_yday(time);
    time->tm_isdst = -1;

    century = bcd_to_bin(century);
    int year_2digit = bcd_to_bin(time->tm_year);

    // 20 * 100 + 25 - 1900 = 125
    time->tm_year = (century * 100 + year_2digit) - 1900;
    time->tm_yday = get_yday(time);
}


void time_init() {
    tm time;
    time_read(&time);       // read CMOS time

    startup_time = mktime(&time);

    LOGK("startup time: %d-%02d-%02d %02d:%02d:%02d\n",
         time.tm_year + 1900, 
         time.tm_mon, 
         time.tm_mday, 
         time.tm_hour, 
         time.tm_min, 
         time.tm_sec
        );
}