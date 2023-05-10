#include <stdio.h>
#include <string.h>
#include "localtime.h"
#include "preferences.h"
#include "pico/stdlib.h"
#include "hardware/flash.h" // for the flash erasing and writing
#include "hardware/sync.h" // for the interrupts

extern "C" uint8_t __prefs_start[4096], __flash_binary_start;
Preferences prefs;

static const uint32_t MAGIC = 0xdeafc0de;

void prefs_save()
{
    int myDataSize = sizeof(prefs);
    prefs.magic = MAGIC;
    printf("set magic to %x\n", MAGIC);

    strcpy(prefs.timezone, localtime_get_zone_name());
    
    int writeSize = (myDataSize / FLASH_PAGE_SIZE) + 1; // how many flash pages we're gonna need to write
    int sectorCount = ((writeSize * FLASH_PAGE_SIZE) / FLASH_SECTOR_SIZE) + 1; // how many flash sectors we're gonna need to erase

    uint8_t page[FLASH_PAGE_SIZE];
    memcpy(page, &prefs, sizeof(prefs));
    size_t offset = &__prefs_start[0] - &__flash_binary_start;
    printf("Programming flash target region... write size %d sector count %d\n", writeSize, sectorCount);

    uint32_t interrupts = save_and_disable_interrupts();
    flash_range_erase(offset, FLASH_SECTOR_SIZE * sectorCount);
    flash_range_program(offset, page, FLASH_PAGE_SIZE * writeSize);
    restore_interrupts(interrupts);

    printf("Done.\n");

    prefs_load();
}

bool prefs_load()
{
    memcpy(&prefs, __prefs_start, sizeof(Preferences));
    if (prefs.magic == MAGIC)
    {
        printf("zone: %s\n", prefs.timezone);
        return true;
    }

    return false;
}