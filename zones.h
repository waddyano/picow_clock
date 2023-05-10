#pragma once

/**
 * Looks up the POSIX string corresponding to the given tz database name
 * @param[in]   name   the tz database name for the timezone in question
 * @return             the POSIX string for the timezone in question
 **/
#ifdef __cplusplus
extern "C" {
#endif


const char * micro_tz_db_get_posix_str(const char * name);

const char * micro_tz_db_get_safe_name(const char * name);

int micro_tz_db_get_zone_count();

const char *micro_tz_db_get_zone(int index);

#ifdef __cplusplus
}
#endif