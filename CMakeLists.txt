cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(picow_clock C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(PICO_BOARD pico_w)

pico_sdk_init()

add_executable(picow_clock
        picow_clock.cxx
        localtime.cxx
        ht16k33_i2c.cxx
        timegm.c
        zones.c
        whttpd_pages.cxx
        whttpd.c
        wfs.c
        )
add_compile_options(-Wall -fdiagnostics-color=never)

target_include_directories(picow_clock PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/.. # for our common lwipopts
)
target_link_libraries(picow_clock
        hardware_i2c
        hardware_rtc
        pico_cyw43_arch_lwip_threadsafe_background
        pico_stdlib
        pico_lwip_http
        )

pico_enable_stdio_usb(picow_clock 1)
pico_enable_stdio_uart(picow_clock 1)

pico_add_extra_outputs(picow_clock)
        