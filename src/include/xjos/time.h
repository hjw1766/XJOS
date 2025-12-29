#ifndef XJOS_TIME_H
#define XJOS_TIME_H


#include <xjos/types.h>


typedef struct {
    int tm_sec;      /* seconds */
    int tm_min;      /* minutes */
    int tm_hour;     /* hours */
    int tm_mday;    /* day of the month */
    int tm_mon;      /* month */
    int tm_year;     /* year */
    int tm_wday;     /* day of the week */
    int tm_yday;     /* day in the year */
    int tm_isdst;   
}tm;


void time_read_bcd(tm *time);
void time_read(tm *time);
time_t mktime(tm *time);
void localtime(time_t stamp, tm *time);


#endif /* XJOS_TIME_H */