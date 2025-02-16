cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(picow_clock C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(PICOWOTA_WIFI_SSID "BAREVA")
set(PICOWOTA_WIFI_PASS "cambridgea")

set(PICO_BOARD pico_w)

pico_sdk_init()

add_executable(picow_clock
        picow_clock.cxx
        localtime.cxx
        ht16k33_i2c.cxx
        preferences.cxx
        ota.cxx
        timegm.c
        zones.cxx
        whttpd_pages.cxx
        whttpd_post.cxx
        whttpd.cxx
        wfs.cxx
        )

target_compile_options(picow_clock PUBLIC $<$<COMPILE_LANGUAGE:C,CXX>:-Wall -fdiagnostics-color=never -include hostname_config.h -dD -D "CYW43_HOST_NAME=get_net_hostname()" >)

target_include_directories(picow_clock PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/.. # for our common lwipopts
)
target_link_libraries(picow_clock
        hardware_i2c
        hardware_rtc
        pico_cyw43_arch_lwip_threadsafe_background
        pico_stdlib
        )

pico_enable_stdio_usb(picow_clock 1)
pico_enable_stdio_uart(picow_clock 0)

pico_set_linker_script(picow_clock ${CMAKE_CURRENT_LIST_DIR}/memmap.ld)
pico_add_extra_outputs(picow_clock)
#picowota_build_standalone(picow_clock)
#picowota_build_combined(picow_clock)        