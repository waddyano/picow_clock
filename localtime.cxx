#include <stdlib.h>
#include "localtime.h"
#include "timegm.h"
#include "zones.h"
#include "hardware/rtc.h"

static const char *zone = "";

extern const char *localtime_get_zone_name()
{
    return zone;
}

extern bool localtime_set_zone_name(const char *name)
{
    const char *posix_str = micro_tz_db_get_posix_str(name);
    if (posix_str == nullptr)
    {
        return false;
    }
    zone = name;
    setenv("TZ", posix_str, 1);
    tzset();
    return true;
}

extern bool localtime_get_time(struct tm *buf)
{
    datetime_t t;
    rtc_get_datetime(&t);
    buf->tm_mday = t.day;
    buf->tm_mon = t.month - 1;
    buf->tm_year = t.year - 1900;
    buf->tm_hour = t.hour;
    buf->tm_min = t.min;
    buf->tm_sec = t.sec;
    time_t tt = timegm(buf);
    localtime_r(&tt, buf);
    return true;
}

