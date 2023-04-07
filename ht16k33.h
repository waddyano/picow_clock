#pragma once

void ht16k33_init();
void ht16k33_display_string(const char *str);
void ht16k33_scroll_string(const char *str, int interval_ms);
void ht16k33_set_brightness(int bright);
void ht16k33_set_blink(int blink);
void ht16k33_display_char(int position, char ch);
void ht16k33_display_set(int position, uint16_t bin);



