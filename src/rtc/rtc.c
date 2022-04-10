#include "rtc.h"
#include "io/io.h"

// Note: Check if RTC is updating 
static int is_updating()
{
    outb(0x70, 0x0A); 
    return insb(0x71) & 0x80;
}

// Note: read from IO
static unsigned char read(int reg)
{
    while (is_updating())
        ;
    outb(0x70, reg);
    
    return insb(0x71);
}

unsigned char rtc_get_seconds()
{
    unsigned char seconds = read(0);
    unsigned char second = (seconds & 0x0F) + ((seconds / 16) * 10);
    return second;
}

unsigned char rtc_get_minutes()
{
    unsigned char minutes = read(0x2);
    unsigned char minute = (minutes & 0x0F) + ((minutes / 16) * 10);
    return minute;
}

unsigned char rtc_get_hours()
{
    unsigned char hours = read(0x4);
    unsigned char hour = ((hours & 0x0F) + (((hours & 0x70) / 16) * 10)) | (hours & 0x80);
    return hour;
}

time rtc_get_time()
{
    time time;
    time.hour = rtc_get_hours();
    time.minute = rtc_get_minutes();
    time.second = rtc_get_seconds();
    return time;
}

datetime rtc_get_date_time()
{
    datetime date_time;

    date_time.day = read(0x7);
    date_time.month = read(0x8);
    date_time.year = read(0x9);

    date_time.time = rtc_get_time();

    return date_time;
}
