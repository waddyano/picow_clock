
#pragma once

#include <stdint.h>

struct Preferences
{
    uint32_t magic;
    char timezone[40];
    long some_long_int;
};

extern Preferences prefs;

extern bool prefs_load();
extern void prefs_save();