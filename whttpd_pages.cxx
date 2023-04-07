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

#include "timegm.h"
#include "localtime.h"
#include "zones.h"

#include "lwip/opt.h"

#include "wfs.h"
#include "whttpd_structs.h"
#include "lwip/def.h"
#include "lwip/mem.h"

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

static const char generated_html[] = R"!(<html>
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
.center { max-width: 100%; max-height: 100vh; margin: auto; }
button { border: 0; border-radius: 0.3rem; background:#1fa3ec; color:#ffffff; line-height:2.4rem; font-size:1.2rem; width:180px;
-webkit-transition-duration:0.4s;transition-duration:0.4s;cursor:pointer;}
button:hover{background:#0b73aa;}
#state { line-height:2.4rem; font-size:1.2rem; text-align: center; }
.cb { border: 0; border-radius: 0.3rem; font-family: arial, sans-serif; color: black; line-height:2.4rem; font-size:1.2rem;}
        </style>
	</head>
	<body">
		<h1>Clock</h1>
        <table class="tabcenter">
            <tr><td id="state"><b id="zoneval"></b></td></tr>
            <tr><td id="state"><b id="timeval"></b></td></tr>
        </table>
	</body>
</html>)!";

int wfs_open_custom(struct wfs_file *file, const char *name, int n_params, char **params, char **values)
{
    printf("HHTPD get fs %s\n", name);
    /* this example only provides one file */
    if (strcmp(name, "/index.html") == 0)
    {
        /* initialize fs_file correctly */
        memset(file, 0, sizeof(struct wfs_file));
        file->pextension = malloc(sizeof(generated_html));
        if (file->pextension != nullptr)
        {
            /* instead of doing memcpy, you would generate e.g. a JSON here */
            memcpy(file->pextension, generated_html, sizeof(generated_html));
            file->data = (const char *)file->pextension;
            file->len = sizeof(generated_html) - 1; /* don't send the trailing 0 */
            file->index = file->len;
            /* allow persisteng connections */
            file->flags = FS_FILE_FLAGS_HEADER_PERSISTENT;
            file->content_type = HTTP_HDR_HTML;
            return 1;
        } 
    }
    else if (strcmp(name, "/time") == 0)
    {
        memset(file, 0, sizeof(struct wfs_file));
        file->pextension = malloc(128);
        if (file->pextension != nullptr)
        {
            struct tm tmbuf;
            localtime_get_time(&tmbuf);
            snprintf((char *)file->pextension, 128, "{ \"time\": \"%02d/%02d/%04d %02d:%02d:%02d\", \"zone\": \"%s\"}\n", 
                tmbuf.tm_mday, tmbuf.tm_mon + 1, tmbuf.tm_year + 1900, tmbuf.tm_hour, tmbuf.tm_min, tmbuf.tm_sec, localtime_get_zone_name());
            file->data = (const char *)file->pextension;
            file->len = strlen(file->data);
            file->index = file->len;
            file->flags = FS_FILE_FLAGS_HEADER_PERSISTENT;
            file->content_type = HTTP_HDR_JSON;
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
        memset(file, 0, sizeof(struct wfs_file));
        file->pextension = malloc(sz + 5);
        if (file->pextension == nullptr)
            printf("out of mem %zd\n", sz + 5);
        if (file->pextension != nullptr)
        {
            char *ptr = (char *)file->pextension;
            *ptr++ = '[';
            for (int i = 0; i < n; ++i)
            {
                const char *z = micro_tz_db_get_zone(i);
                if (i > 0)
                {
                    *ptr++ = ',';
                }
                *ptr++ = '"';
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
        printf("setzone\n");
        for (int i = 0; i < n_params; ++i)
        {
            printf(" %d %s %s\n", i, params[i], values[i]);
        }
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