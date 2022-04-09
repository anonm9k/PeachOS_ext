#ifndef RTC_H
#define RTC_H
#include <stdint.h>

typedef struct
{
    uint32_t hour;
    uint32_t second;
    uint32_t minute;

} time;

typedef struct
{
  uint32_t day;
  uint32_t month;
  uint32_t year;
  time time;
} datetime;

void rtc_init();
unsigned char rtc_get_seconds();
unsigned char rtc_get_minutes();
unsigned char rtc_get_hours();
time rtc_get_time();
datetime rtc_get_date_time();
#endif
