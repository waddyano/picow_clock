#pragma once

#include <time.h>

extern const char *localtime_get_zone_name();
extern bool localtime_set_zone_name(const char *name);

extern bool localtime_get_time(struct tm *buf);

