/**
 * @file
 * HTTPD custom file system example for runtime generated files
 *
 * This file demonstrates how to add support for generated files to httpd.
 */
 
 /*
 * Copyright (c) 2017 Simon Goldschmidt
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED 
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 * 
 * Author: Simon Goldschmidt <goldsimon@gmx.de>
 *
 */

#include "ht16k33.h"
#include "timegm.h"
#include "localtime.h"
#include "ota.h"
#include "preferences.h"
#include "zones.h"

#include "lwip/opt.h"

#include "wfs.h"
#include "whttpd_structs.h"
#include "hardware/watchdog.h"
#include "lwip/def.h"
#include "lwip/mem.h"
#include "pico/time.h"

#include <stdio.h>
#include <string.h>

/** define LWIP_HTTPD_EXAMPLE_GENERATEDFILES to 1 to enable this file system */
#ifndef LWIP_HTTPD_EXAMPLE_GENERATEDFILES
#define LWIP_HTTPD_EXAMPLE_GENERATEDFILES 1
#endif

#if LWIP_HTTPD_EXAMPLE_GENERATEDFILES

#if !LWIP_HTTPD_CUSTOM_FILES
#error This needs LWIP_HTTPD_CUSTOM_FILES
#endif
#if !LWIP_HTTPD_DYNAMIC_HEADERS
#error This needs LWIP_HTTPD_DYNAMIC_HEADERS
#endif

static const char index_page[] = R"!(<html>
	<head>
		<meta http-equiv="content-type" content="text/html; charset=utf-8" />
        <!-- <meta name="viewport" content="width=device-width, initial-scale=1.0"/> -->
		<title>Clock</title>
        <script>
function update_state()
{
    var xhr = new XMLHttpRequest();
    xhr.onreadystatechange = () => 
    {
        if (xhr.readyState === 4) 
        {
            let state = JSON.parse(xhr.response);
            timeval.innerHTML = state.time;
            weekday.innerHTML = state.weekday;
            zoneval.innerHTML = state.zone;
        }
    }
    xhr.open("GET", '/time');
    xhr.send();
}
setInterval(update_state, 1000);
        </script>
        <style>
body, textarea, button {font-family: arial, sans-serif;}
h1 { text-align: center; }
.tabcenter { margin-left: auto; margin-right: auto; }
button { border: 0; border-radius: 0.3rem; background:#1fa3ec; color:#ffffff; line-height:2.4rem; font-size:1.2rem; width:180px;
-webkit-transition-duration:0.4s;transition-duration:0.4s;cursor:pointer;}
button:hover{background:#0b73aa;}
#state { line-height:2.4rem; font-size:1.2rem; text-align: center; }
.acenter { text-align: center; }
.cb { border: 0; border-radius: 0.3rem; font-family: arial, sans-serif; color: black; line-height:2.4rem; font-size:1.2rem;}
        </style>
	</head>
	<body>
		<h1>Clock</h1>
        <table class="tabcenter">
            <tr><td id="state"><b id="zoneval"></b></td></tr>
            <tr><td id="state"><b id="weekday"></b></td></tr>
            <tr><td id="state"><b id="timeval"></b></td></tr>
            <tr><td class="acenter"><a href="/settings.html">Settings</a></td></tr>
        </table>
	</body>
</html>)!";

static const char settings_page[] = R"!(
<html>
	<head>
		<meta http-equiv="content-type" content="text/html; charset=utf-8" />
        <!-- <meta name="viewport" content="width=device-width, initial-scale=1.0"/> -->
		<title>Clock Settings</title>
        <script>
function newzone()
{
}
function update_zones()
{
    var xhr = new XMLHttpRequest();
    xhr.onreadystatechange = () => 
    {
        if (xhr.readyState === 4) 
        {
            let zones = JSON.parse(xhr.response);
            let current = "";
            for (let zone of zones)            
            {
                if (zone.startsWith("*"))
                {
                    zone = zone.slice(1);
                    current = zone;
                }
                zoneSelect.add(new Option(zone, zone));
            }
            if (current != "")
            {
                zoneSelect.value = current;
            }
        }
    }
    xhr.open("GET", '/zones');
    xhr.send();
}
        </script>
        <style>
body, textarea, button {font-family: arial, sans-serif;}
h1 { text-align: center; }
.tabcenter { margin-left: auto; margin-right: auto; }
.center { max-width: 100%; max-height: 100vh; margin: auto; }
button { border: 0; border-radius: 0.3rem; background:#1fa3ec; color:#ffffff; line-height:2.4rem; font-size:1.2rem; width:180px;
-webkit-transition-duration:0.4s;transition-duration:0.4s;cursor:pointer;}
button:hover{background:#0b73aa;}
#state { line-height:2.4rem; font-size:1.2rem; text-align: center; }
.cb { border: 0; border-radius: 0.3rem; font-family: arial, sans-serif; color: black; line-height:2.4rem; font-size:1.2rem;}
        </style>
	</head>
	<body onload="update_zones()">
		<h1>Clock Settings</h1>
        <table class="tabcenter">
        <tr>
        <td>
            <select id="zoneSelect" onchange="newzone()">
            </select>
        </td>
        </tr>
        </table>
	</body>
</html>
)!";

static const char restart_page_head[] = R"!(
<html>
<head>
	<title>Rebooting</title>
    <script>
        const id = setInterval(ping, 1000);
        let controller = null;

        function ping()
        {
            if (controller != null)
            {
                controller.abort();
            }
            controller = new AbortController();
            fetch('/', { signal: controller.signal })
                .then(response => response.text())
                .then(function(data) { clearInterval(id); window.location.href = '/'; })
                .catch((error) => { console.error('Error:', error); });
        };

        ping();
    </script>
<style>
body, p {font-family: arial, sans-serif;}
p { line-height:2.4rem; font-size:1.2rem; }
</style>
</head>
<body><p>
    )!";

static const char restart_page_tail[] = "</p></body></html>\n";

static int64_t reset_now(alarm_id_t, void *)
{
    watchdog_reboot(0, 0, 0);
    while(1);
}

static const char *weekday_string(int wday)
{
    static const char *wdays[] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    if (wday < 0 || wday > 6)
    {
        return "";
    }
    return wdays[wday];
}

int wfs_open_custom(struct wfs_file *file, const char *name, int n_params, char **params, char **values)
{
    //printf("HTTPD get fs %s\n", name);
    memset(file, 0, sizeof(struct wfs_file));
    /* this example only provides one file */
    if (strcmp(name, "/index.html") == 0)
    {
        /* initialize fs_file correctly */
        file->pextension = malloc(sizeof(index_page));
        if (file->pextension != nullptr)
        {
            /* instead of doing memcpy, you would generate e.g. a JSON here */
            memcpy(file->pextension, index_page, sizeof(index_page));
            file->data = (const char *)file->pextension;
            file->len = sizeof(index_page) - 1; /* don't send the trailing 0 */
            file->index = file->len;
            /* allow persisteng connections */
            file->flags = FS_FILE_FLAGS_HEADER_PERSISTENT;
            file->content_type = HTTP_HDR_HTML;
            return 1;
        } 
    }
    else if (strcmp(name, "/settings.html") == 0)
    {
        /* initialize fs_file correctly */
        file->pextension = malloc(sizeof(settings_page));
        if (file->pextension != nullptr)
        {
            /* instead of doing memcpy, you would generate e.g. a JSON here */
            memcpy(file->pextension, settings_page, sizeof(settings_page));
            file->data = (const char *)file->pextension;
            file->len = sizeof(settings_page) - 1; /* don't send the trailing 0 */
            file->index = file->len;
            /* allow persisteng connections */
            file->flags = FS_FILE_FLAGS_HEADER_PERSISTENT;
            file->content_type = HTTP_HDR_HTML;
            return 1;
        } 
    }
    else if (strcmp(name, "/update.html") == 0)
    {
        /* initialize fs_file correctly */
        file->pextension = ota_get_update_page("Clock");
        if (file->pextension != nullptr)
        {
            file->data = (const char *)file->pextension;
            file->len = strlen(file->data);
            file->index = file->len;
            /* allow persisteng connections */
            file->flags = FS_FILE_FLAGS_HEADER_PERSISTENT;
            file->content_type = HTTP_HDR_HTML;
            return 1;
        } 
    }
    else if (strcmp(name, "/restart.html") == 0)
    {
        /* initialize fs_file correctly */
        file->pextension = malloc(sizeof(restart_page_head) + sizeof(restart_page_tail) + 32);
        if (file->pextension != nullptr)
        {
            /* instead of doing memcpy, you would generate e.g. a JSON here */
            size_t off = 0;
            memcpy(file->pextension, restart_page_head, sizeof(restart_page_head) - 1);
            off += sizeof(restart_page_head) - 1;
            memcpy((char *)file->pextension + off, "Restarting", 10);
            off += 10;
            memcpy((char *)file->pextension + off, restart_page_tail, sizeof(restart_page_tail) - 1);
            off += sizeof(restart_page_tail) - 1;
            file->data = (const char *)file->pextension;
            file->len = off;
            file->index = file->len;
            /* allow persisteng connections */
            file->flags = FS_FILE_FLAGS_HEADER_PERSISTENT;
            file->content_type = HTTP_HDR_HTML;
            add_alarm_in_ms(1000, reset_now, nullptr, true);
            return 1;
        } 
    }
    else if (strcmp(name, "/time") == 0)
    {
        file->pextension = malloc(128);
        if (file->pextension != nullptr)
        {
            struct tm tmbuf;
            localtime_get_time(&tmbuf);
            snprintf((char *)file->pextension, 128, "{ \"time\": \"%02d/%02d/%04d %02d:%02d:%02d\", \"weekday\": \"%s\", \"zone\": \"%s\"}\n", 
                tmbuf.tm_mon + 1, tmbuf.tm_mday, tmbuf.tm_year + 1900, tmbuf.tm_hour, tmbuf.tm_min, tmbuf.tm_sec, weekday_string(tmbuf.tm_wday), localtime_get_zone_name());
            file->data = (const char *)file->pextension;
            file->len = strlen(file->data);
            file->index = file->len;
            file->flags = FS_FILE_FLAGS_HEADER_PERSISTENT;
            file->content_type = HTTP_HDR_JSON;
            prefs_load();
            return 1;
        }
    }
    else if (strcmp(name, "/zones") == 0)
    {
        int n = micro_tz_db_get_zone_count();
        size_t sz = 0;
        for (int i = 0; i < n; ++i)
        {
            const char *z = micro_tz_db_get_zone(i);
            sz += strlen(z) + 3;
        }
        file->pextension = malloc(sz + 6);
        if (file->pextension == nullptr)
            printf("out of mem %zd\n", sz + 6);
        if (file->pextension != nullptr)
        {
            char *ptr = (char *)file->pextension;
            *ptr++ = '[';
            const char *current = localtime_get_zone_name();
            for (int i = 0; i < n; ++i)
            {
                const char *z = micro_tz_db_get_zone(i);
                if (i > 0)
                {
                    *ptr++ = ',';
                }
                *ptr++ = '"';
                if (strcmp(z, current) == 0)
                {
                    *ptr++ = '*';
                }
                strcpy(ptr, z);
                ptr += strlen(z);
                *ptr++ = '"';
            }
            *ptr++ = ']';
            *ptr++ = '\0';

            file->data = (const char *)file->pextension;
            file->len = strlen(file->data);
            printf("lens %d %zd\n", file->len, sz + 5);
            file->index = file->len;
            file->flags = FS_FILE_FLAGS_HEADER_PERSISTENT;
            file->content_type = HTTP_HDR_JSON;
            return 1;
        }
    }
    else if (strcmp(name, "/setzone") == 0)
    {
        //printf("setzone\n");
        bool ok = false;
        for (int i = 0; i < n_params; ++i)
        {
            printf(" %d %s %s\n", i, params[i], values[i]);
            if (strcmp(params[i], "z") == 0)
            {
                ok = localtime_set_zone_name(values[i]);
                if (ok)
                    prefs_save();
                break;
            }
        }

        file->data = ok ? "OK" : "NOCHANGE";
        file->len = strlen(file->data);
        file->index = file->len;
        file->flags = FS_FILE_FLAGS_HEADER_PERSISTENT;
        file->content_type = HTTP_HDR_TEXT;
        return 1;
    }
    else if (strcmp(name, "/status") == 0)
    {
        extern uint8_t __flash_binary_start;
        extern uint8_t __flash_binary_end;
        printf("base %p start %p end %p\n", XIP_BASE, &__flash_binary_start, &__flash_binary_end);
    }
    else if (strcmp(name, "/brightness") == 0)
    {
        bool ok = false;
        for (int i = 0; i < n_params; ++i)
        {
            if (strcmp(params[i], "v") == 0)
            {
                char *end;
                unsigned long val = strtoul(values[i], &end, 10);
                if (*end == '\0')
                {
                    ok = true;
                    ht16k33_set_brightness(val);
                    printf("set brightness %lu\n", val);
                }
                break;
            }
        }
        file->data = ok ? "OK" : "NOCHANGE";
        file->len = strlen(file->data);
        file->index = file->len;
        file->flags = FS_FILE_FLAGS_HEADER_PERSISTENT;
        file->content_type = HTTP_HDR_TEXT;
        return 1;
    }
    else
    {
        printf("HTTPD unhandled fs %s\n", name);
    }

    return 0;
}

void wfs_close_custom(struct wfs_file *file)
{
    if (file && file->pextension)
    {
        free(file->pextension);
        file->pextension = NULL;
    }
}

#if LWIP_HTTPD_FS_ASYNC_READ
u8_t
wfs_canread_custom(struct wfs_file *file)
{
  LWIP_UNUSED_ARG(file);
  /* This example does not use delayed/async reading */
  return 1;
}

u8_t
wfs_wait_read_custom(struct wfs_file *file, wfs_wait_cb callback_fn, void *callback_arg)
{
  LWIP_ASSERT("not implemented in this example configuration", 0);
  LWIP_UNUSED_ARG(file);
  LWIP_UNUSED_ARG(callback_fn);
  LWIP_UNUSED_ARG(callback_arg);
  /* Return
     - 1 if ready to read (at least one byte)
     - 0 if reading should be delayed (call 'tcpip_callback(callback_fn, callback_arg)' when ready) */
  return 1;
}

int
wfs_read_async_custom(struct wfs_file *file, char *buffer, int count, wfs_wait_cb callback_fn, void *callback_arg)
{
  LWIP_ASSERT("not implemented in this example configuration", 0);
  LWIP_UNUSED_ARG(file);
  LWIP_UNUSED_ARG(buffer);
  LWIP_UNUSED_ARG(count);
  LWIP_UNUSED_ARG(callback_fn);
  LWIP_UNUSED_ARG(callback_arg);
  /* Return
     - FS_READ_EOF if all bytes have been read
     - FS_READ_DELAYED if reading is delayed (call 'tcpip_callback(callback_fn, callback_arg)' when done) */
  /* all bytes read already */
  return FS_READ_EOF;
}

#else /* LWIP_HTTPD_FS_ASYNC_READ */
int wfs_read_custom(struct wfs_file *file, char *buffer, int count)
{
    LWIP_ASSERT("not implemented in this example configuration", 0);
    LWIP_UNUSED_ARG(file);
    LWIP_UNUSED_ARG(buffer);
    LWIP_UNUSED_ARG(count);
    /* Return
     -   FS_READ_EOF if all bytes have been read
     -  FS_READ_DELAYED if reading is delayed (call 'tcpip_callback(callback_fn, callback_arg)' when done) */
    /* all bytes read already */
    return FS_READ_EOF;
}

#endif /* LWIP_HTTPD_FS_ASYNC_READ */

#endif /* LWIP_HTTPD_EXAMPLE_GENERATEDFILES */