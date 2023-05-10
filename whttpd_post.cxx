/**
 * @file
 * HTTPD example for simple POST
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

#include "lwip/opt.h"

#include "whttpd.h"
#include "hardware/flash.h"
#include "lwip/def.h"
#include "lwip/mem.h"
#include "pico/critical_section.h"

#include <stdio.h>
#include <string.h>

extern uint8_t __flash_ota_start, __flash_ota_end;
static void *current_connection;
static void *valid_connection;
static uint8_t page_buffer[FLASH_SECTOR_SIZE];
static int page_offset = 0;
static uint32_t content_offset = 0;
static int content_length = 0;

static critical_section cs;

err_t
whttpd_post_begin(void *connection, const char *uri, const char *http_request,
                 u16_t http_request_len, int content_len, char *response_uri,
                 u16_t response_uri_len, u8_t *post_auto_wnd)
{
  LWIP_UNUSED_ARG(connection);
  LWIP_UNUSED_ARG(http_request);
  LWIP_UNUSED_ARG(http_request_len);
  LWIP_UNUSED_ARG(content_len);
  LWIP_UNUSED_ARG(post_auto_wnd);
  if (!critical_section_is_initialized(&cs))
  {
    critical_section_init(&cs);
  }
  printf("POST %s %d\n", uri, content_len);
  if (memcmp(uri, "/post_update", 5) == 0 ) {
      current_connection = connection;
      valid_connection = NULL;
      *post_auto_wnd = 1;
      content_length = content_len;
      content_offset = 0;
      page_offset = 0;
      return ERR_OK;
  }
  return ERR_VAL;
}

static void flash_page()
{
  uint32_t ota_offset = (uint32_t)&__flash_ota_start - XIP_BASE;
  printf("flash a page @%u\n", content_offset);
  critical_section_enter_blocking(&cs);
  flash_range_erase(ota_offset + content_offset, FLASH_SECTOR_SIZE);
  flash_range_program(ota_offset + content_offset, page_buffer, FLASH_SECTOR_SIZE);
  critical_section_exit(&cs);
  content_offset += sizeof(page_buffer);
  page_offset = 0;
}

err_t
whttpd_post_receive_data(void *connection, struct pbuf *p)
{
  err_t ret;

  LWIP_ASSERT("NULL pbuf", p != NULL);

  printf("POST receive data %u %p %p\n", p->len, current_connection, connection);
  if (current_connection == connection) {
    /* not returning ERR_OK aborts the connection, so return ERR_OK unless the
       connection is unknown */
    uint16_t left = p->len;
    while (left > 0)
    {
      uint32_t to_copy = left;
      if (to_copy > sizeof(page_buffer) - page_offset)
      {
        to_copy = sizeof(page_buffer) - page_offset;
      }
      printf("copy %u\n", to_copy);
      memcpy(page_buffer + page_offset, p->payload, to_copy);
      left -= to_copy;
      page_offset += to_copy;
      if (page_offset == sizeof(page_buffer))
      {
        flash_page();
      }
    }
    ret = ERR_OK;
  } else {
    ret = ERR_VAL;
  }

  /* this function must ALWAYS free the pbuf it is passed or it will leak memory */
  pbuf_free(p);

  return ret;
}

void
whttpd_post_finished(void *connection, char *response_uri, u16_t response_uri_len)
{
  /* default page is "login failed" */
  printf("post done offset %u of %u\n", content_offset, content_length);
  if (page_offset > 0)
  {
    memset(page_buffer + page_offset, 0, sizeof(page_buffer) - page_offset);
    flash_page();
  }
  printf("last flash done offset %u of %u\n", content_offset, content_length);
  snprintf(response_uri, response_uri_len, "/loginfail.html");
  if (current_connection == connection) {
    if (valid_connection == connection) {
      /* login succeeded */
      snprintf(response_uri, response_uri_len, "/session.html");
    }
    current_connection = NULL;
    valid_connection = NULL;
  }
}
