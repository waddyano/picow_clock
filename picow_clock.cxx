/**
 * Copyright (c) 2022 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/unique_id.h"
#include "hardware/rtc.h"
#include "hardware/watchdog.h"

#include "lwip/dns.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"
#include "whttpd.h"

#include "ht16k33.h"
#include "localtime.h"
#include "preferences.h"
#include "timegm.h"
#include "wifi_details.h"

struct NTP_T
{
    ip_addr_t ntp_server_address;
    bool dns_request_sent;
    struct udp_pcb *ntp_pcb;
    absolute_time_t ntp_test_time;
    alarm_id_t ntp_resend_alarm;
};

#define NTP_SERVER "pool.ntp.org"
#define NTP_MSG_LEN 48
#define NTP_PORT 123
#define NTP_DELTA 2208988800 // seconds between 1 Jan 1900 and 1 Jan 1970
#define NTP_TEST_TIME (30 * 1000)
#define NTP_RESEND_TIME (10 * 1000)

// d 1-31
// m 1-12
static int dotw(int d, int m, int y)
{
    return (d += m < 3 ? y-- : y - 2, 23*m/9 + d + 4 + y/4- y/100 + y/400)%7;
}

static absolute_time_t last_ntp_result_time;

// Called with results of operation
static void ntp_result(NTP_T* state, int status, time_t *result) 
{
    if (status == 0 && result) 
    {
        struct tm *utc = gmtime(result);
        printf("got ntp response: %02d/%02d/%04d %02d:%02d:%02d\n", utc->tm_mday, utc->tm_mon + 1, utc->tm_year + 1900,
               utc->tm_hour, utc->tm_min, utc->tm_sec);

        datetime_t t;
        t.year  = utc->tm_year + 1900;
        t.month = utc->tm_mon + 1;
        t.day   = utc->tm_mday;
        t.dotw  = dotw(t.day, t.month, t.year); // 0 is Sunday, so 5 is Friday
        t.hour  = utc->tm_hour;
        t.min   = utc->tm_min;
        t.sec   = utc->tm_sec;

        last_ntp_result_time = get_absolute_time();

        rtc_set_datetime(&t);
    }

    if (state->ntp_resend_alarm > 0)
    {
        cancel_alarm(state->ntp_resend_alarm);
        state->ntp_resend_alarm = 0;
    }

    state->ntp_test_time = make_timeout_time_ms(NTP_TEST_TIME);
    state->dns_request_sent = false;
}

static int64_t ntp_failed_handler(alarm_id_t id, void *user_data);

// Make an NTP request
static void ntp_request(NTP_T *state) 
{
    // cyw43_arch_lwip_begin/end should be used around calls into lwIP to ensure correct locking.
    // You can omit them if you are in a callback from lwIP. Note that when using pico_cyw_arch_poll
    // these calls are a no-op and can be omitted, but it is a good practice to use them in
    // case you switch the cyw43_arch type later.
    cyw43_arch_lwip_begin();
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, NTP_MSG_LEN, PBUF_RAM);
    uint8_t *req = (uint8_t *) p->payload;
    memset(req, 0, NTP_MSG_LEN);
    req[0] = 0x1b;
    udp_sendto(state->ntp_pcb, p, &state->ntp_server_address, NTP_PORT);
    pbuf_free(p);
    cyw43_arch_lwip_end();
}

static int64_t ntp_failed_handler(alarm_id_t id, void *user_data)
{
    NTP_T* state = (NTP_T*)user_data;
    printf("ntp request failed\n");
    ntp_result(state, -1, NULL);
    return 0;
}

// Call back with a DNS result
static void ntp_dns_found(const char *hostname, const ip_addr_t *ipaddr, void *arg) 
{
    NTP_T *state = (NTP_T*)arg;
    if (ipaddr)
    {
        state->ntp_server_address = *ipaddr;
        printf("ntp address %s\n", ipaddr_ntoa(ipaddr));
        ntp_request(state);
    }
    else
    {
        printf("ntp dns request failed\n");
        ntp_result(state, -1, NULL);
    }
}

// NTP data received
static void ntp_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
    NTP_T *state = (NTP_T*)arg;
    uint8_t mode = pbuf_get_at(p, 0) & 0x7;
    uint8_t stratum = pbuf_get_at(p, 1);

    // Check the result
    if (ip_addr_cmp(addr, &state->ntp_server_address) && port == NTP_PORT && p->tot_len == NTP_MSG_LEN &&
        mode == 0x4 && stratum != 0)
    {
        uint8_t seconds_buf[4] = {0};
        pbuf_copy_partial(p, seconds_buf, sizeof(seconds_buf), 40);
        uint32_t seconds_since_1900 = seconds_buf[0] << 24 | seconds_buf[1] << 16 | seconds_buf[2] << 8 | seconds_buf[3];
        uint32_t seconds_since_1970 = seconds_since_1900 - NTP_DELTA;
        time_t epoch = seconds_since_1970;
        ntp_result(state, 0, &epoch);
    }
    else
    {
        printf("invalid ntp response\n");
        ntp_result(state, -1, NULL);
    }
    pbuf_free(p);
}

// Perform initialisation
static NTP_T* ntp_init(void) 
{
    NTP_T *state = (NTP_T *)calloc(1, sizeof(NTP_T));
    if (!state)
    {
        printf("failed to allocate state\n");
        return NULL;
    }
    state->ntp_pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
    if (!state->ntp_pcb)
    {
        printf("failed to create pcb\n");
        free(state);
        return NULL;
    }
    udp_recv(state->ntp_pcb, ntp_recv, state);
    return state;
}

#if 0 // for debug
static const char* link_status_string(int status)
{
    switch (status) {
    case CYW43_LINK_DOWN:
        return "link down";
    case CYW43_LINK_JOIN:
        return "joining";
    case CYW43_LINK_NOIP:
        return "no ip";
    case CYW43_LINK_UP:
        return "link up";
    case CYW43_LINK_FAIL:
        return "link fail";
    case CYW43_LINK_NONET:
        return "network fail";
    case CYW43_LINK_BADAUTH:
        return "bad auth";
    }
    return "unknown";
}
#endif

static void ntp_loop(void)
{
    NTP_T *state = ntp_init();
    if (!state)
        return;
    bool dots = true;
    int print_tick = 0;
    for (;;)
    {
        if (absolute_time_diff_us(get_absolute_time(), state->ntp_test_time) < 0 && !state->dns_request_sent)
        {
            // Set alarm in case udp requests are lost
            state->ntp_resend_alarm = add_alarm_in_ms(NTP_RESEND_TIME, ntp_failed_handler, state, true);

            // cyw43_arch_lwip_begin/end should be used around calls into lwIP to ensure correct locking.
            // You can omit them if you are in a callback from lwIP. Note that when using pico_cyw_arch_poll
            // these calls are a no-op and can be omitted, but it is a good practice to use them in
            // case you switch the cyw43_arch type later.
            cyw43_arch_lwip_begin();
            int err = dns_gethostbyname(NTP_SERVER, &state->ntp_server_address, ntp_dns_found, state);
            cyw43_arch_lwip_end();

            state->dns_request_sent = true;
            if (err == ERR_OK)
            {
                ntp_request(state); // Cached result
            }
            else if (err != ERR_INPROGRESS)
            { // ERR_INPROGRESS means expect a callback
                printf("dns request failed %d\n", err);
                ntp_result(state, -1, NULL);
            }
        }

        cyw43_arch_lwip_begin();
        int link_status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
        cyw43_arch_lwip_end();
        if (link_status != CYW43_LINK_UP)
        {
            printf("link has gone down %d\n", link_status);
            return;
        }

        sleep_ms(1000);
        datetime_t t;
        rtc_get_datetime(&t);
        struct tm tmbuf;
        localtime_get_time(&tmbuf);
        char dbuf[6];
        snprintf(dbuf, sizeof(dbuf), "%2d %02d", tmbuf.tm_hour, tmbuf.tm_min);
        if (tmbuf.tm_hour < 8 || tmbuf.tm_hour >= 20)
        {
            ht16k33_set_brightness(0);
        }
        else
        {
            ht16k33_set_brightness(6);
        }

        ht16k33_display_string(dbuf);
        ht16k33_display_set(2, dots ? 0xff : 0);
        dots = !dots;

        ++print_tick;
        if (print_tick == 10)        
        {
            auto delta = absolute_time_diff_us(last_ntp_result_time, get_absolute_time());
            printf("UTC time: %02d/%02d/%04d %02d:%02d:%02d - last sync %lld secs ago\n", t.day, t.month, t.year,
                t.hour, t.min, t.sec, delta / 1000000);
            printf("localtime says: %02d/%02d/%04d %02d:%02d:%02d\n", tmbuf.tm_mday, tmbuf.tm_mon + 1, tmbuf.tm_year + 1900,
                tmbuf.tm_hour, tmbuf.tm_min, tmbuf.tm_sec);
            print_tick = 0;
        }
        watchdog_update();
    }
}

static uint16_t small_id;
static char hostname[20];
extern "C" const char *get_net_hostname()
{
    if (hostname[0] == '\0')
    {
        snprintf(hostname, sizeof(hostname), "picow_clock_%u", small_id);
    }

    return hostname;
}

// step must be 0 to 6
static void show_busy(int step)
{
    uint16_t pattern = 1 << step;
    ht16k33_display_set(0, pattern);
    ht16k33_display_set(1, pattern);
    ht16k33_display_set(2, 0);
    ht16k33_display_set(3, pattern);
    ht16k33_display_set(4, pattern);
}

int main() 
{
    stdio_init_all();

    ht16k33_init();
    ht16k33_set_brightness(6);
    ht16k33_display_string("-- --");

    printf("init\n");
    
    pico_unique_board_id_t id;

    pico_get_unique_board_id(&id);

    for (size_t i = 0; i < sizeof(id.id) / 2; ++i)
    {
        uint16_t x = (id.id[i * 2 + 1] << 8) | id.id[i * 2];
        small_id += x;
    }

    if (cyw43_arch_init())
    {
        printf("failed to initialise\n");
        return 1;
    }

    rtc_init();
    if (prefs_load())
    {
        localtime_set_zone_name(prefs.timezone);
    }
    else
    {
        localtime_set_zone_name("America/Los_Angeles");
    }

    cyw43_arch_enable_sta_mode();

    // On startup we have to wait for wifi to get a ntp request in
    int busy_step = 0;
    for (;;)
    {
        int err = cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 10000);
        if (err == 0)
        {
            break;
        }
        show_busy(busy_step);
        if (++busy_step == 6)
        {
            busy_step = 0;
        }
    }

    printf("connected to wifi\n");
    whttpd_init();
    watchdog_enable(3000, 0);

    busy_step = 0;

    for (;;)
    {
        // ntp_loop exits if the link goes down so we wait until it is up again. 
        int link_status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
        if (link_status != CYW43_LINK_UP)
        {
            printf("wifi is down\n");
            show_busy(busy_step);
            for (int i = 0; i < 5; i++)
            {
                sleep_ms(1000);
                watchdog_update();
            }
            if (++busy_step == 6)
            {
                busy_step = 0;
            }
            continue;
        }
        
        ntp_loop();
    }

    cyw43_arch_deinit();
    return 0;
}