/* Copyright libuv project and contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "uv.h"
#include "task.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK_HANDLE(handle) \
  ASSERT((uv_udp_t*)(handle) == &server || (uv_udp_t*)(handle) == &client)

static uv_udp_t server;
static uv_udp_t client;
static uv_buf_t buf;
#ifndef __OS2__
static struct sockaddr_in6 lo_addr;
#endif

static int cl_send_cb_called;
static int sv_recv_cb_called;

static int close_cb_called;


#ifndef __OS2__
static void alloc_cb(uv_handle_t* handle,
                     size_t suggested_size,
                     uv_buf_t* buf) {
  static char slab[65536];
  CHECK_HANDLE(handle);
  ASSERT_LE(suggested_size, sizeof(slab));
  buf->base = slab;
  buf->len = sizeof(slab);
}


static void close_cb(uv_handle_t* handle) {
  CHECK_HANDLE(handle);
  ASSERT(uv_is_closing(handle));
  close_cb_called++;
}


static void cl_send_cb(uv_udp_send_t* req, int status) {
  int r;

  ASSERT_NOT_NULL(req);
  ASSERT_EQ(status, 0);
  CHECK_HANDLE(req->handle);
  if (++cl_send_cb_called == 1) {
    uv_udp_connect(&client, NULL);
    r = uv_udp_send(req, &client, &buf, 1, NULL, cl_send_cb);
    ASSERT_EQ(r, UV_EDESTADDRREQ);
    r = uv_udp_send(req,
                    &client,
                    &buf,
                    1,
                    (const struct sockaddr*) &lo_addr,
                    cl_send_cb);
    ASSERT_EQ(r, 0);
  }

}


static void sv_recv_cb(uv_udp_t* handle,
                       ssize_t nread,
                       const uv_buf_t* rcvbuf,
                       const struct sockaddr* addr,
                       unsigned flags) {
  if (nread > 0) {
    ASSERT_EQ(nread, 4);
    ASSERT_NOT_NULL(addr);
    ASSERT_EQ(memcmp("EXIT", rcvbuf->base, nread), 0);
    if (++sv_recv_cb_called == 4) {
      uv_close((uv_handle_t*) &server, close_cb);
      uv_close((uv_handle_t*) &client, close_cb);
    }
  }
}
#endif


TEST_IMPL(udp_connect6) {
#if defined(__OpenBSD__)
  RETURN_SKIP("Test does not currently work in OpenBSD");
#endif
#ifndef __OS2__
  uv_udp_send_t req;
  struct sockaddr_in6 ext_addr;
  struct sockaddr_in6 tmp_addr;
  int r;
  int addrlen;
#endif

  if (!can_ipv6())
    RETURN_SKIP("IPv6 not supported");

#ifndef __OS2__
  ASSERT_EQ(0, uv_ip6_addr("::", TEST_PORT, &lo_addr));

  r = uv_udp_init(uv_default_loop(), &server);
  ASSERT_EQ(r, 0);

  r = uv_udp_bind(&server, (const struct sockaddr*) &lo_addr, 0);
  ASSERT_EQ(r, 0);

  r = uv_udp_recv_start(&server, alloc_cb, sv_recv_cb);
  ASSERT_EQ(r, 0);

  r = uv_udp_init(uv_default_loop(), &client);
  ASSERT_EQ(r, 0);

  buf = uv_buf_init("EXIT", 4);

  /* connect() to INADDR_ANY fails on Windows wih WSAEADDRNOTAVAIL */
  ASSERT_EQ(0, uv_ip6_addr("::", TEST_PORT, &tmp_addr));
  r = uv_udp_connect(&client, (const struct sockaddr*) &tmp_addr);
#ifdef _WIN32
  ASSERT_EQ(r, UV_EADDRNOTAVAIL);
#else
  ASSERT_EQ(r, 0);
  r = uv_udp_connect(&client, NULL);
  ASSERT_EQ(r, 0);
#endif

  ASSERT_EQ(0, uv_ip6_addr("2001:4860:4860::8888", TEST_PORT, &ext_addr));
  ASSERT_EQ(0, uv_ip6_addr("::1", TEST_PORT, &lo_addr));

  r = uv_udp_connect(&client, (const struct sockaddr*) &lo_addr);
  ASSERT_EQ(r, 0);
  r = uv_udp_connect(&client, (const struct sockaddr*) &ext_addr);
  ASSERT_EQ(r, UV_EISCONN);

  addrlen = sizeof(tmp_addr);
  r = uv_udp_getpeername(&client, (struct sockaddr*) &tmp_addr, &addrlen);
  ASSERT_EQ(r, 0);

  /* To send messages in connected UDP sockets addr must be NULL */
  r = uv_udp_try_send(&client, &buf, 1, (const struct sockaddr*) &lo_addr);
  ASSERT_EQ(r, UV_EISCONN);
  r = uv_udp_try_send(&client, &buf, 1, NULL);
  ASSERT_EQ(r, 4);
  r = uv_udp_try_send(&client, &buf, 1, (const struct sockaddr*) &ext_addr);
  ASSERT_EQ(r, UV_EISCONN);

  r = uv_udp_connect(&client, NULL);
  ASSERT_EQ(r, 0);
  r = uv_udp_connect(&client, NULL);
  ASSERT_EQ(r, UV_ENOTCONN);

  addrlen = sizeof(tmp_addr);
  r = uv_udp_getpeername(&client, (struct sockaddr*) &tmp_addr, &addrlen);
  ASSERT_EQ(r, UV_ENOTCONN);

  /* To send messages in disconnected UDP sockets addr must be set */
  r = uv_udp_try_send(&client, &buf, 1, (const struct sockaddr*) &lo_addr);
  ASSERT_EQ(r, 4);
  r = uv_udp_try_send(&client, &buf, 1, NULL);
  ASSERT_EQ(r, UV_EDESTADDRREQ);


  r = uv_udp_connect(&client, (const struct sockaddr*) &lo_addr);
  ASSERT_EQ(r, 0);
  r = uv_udp_send(&req,
                  &client,
                  &buf,
                  1,
                  (const struct sockaddr*) &lo_addr,
                  cl_send_cb);
  ASSERT_EQ(r, UV_EISCONN);
  r = uv_udp_send(&req, &client, &buf, 1, NULL, cl_send_cb);
  ASSERT_EQ(r, 0);

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT_EQ(close_cb_called, 2);
  ASSERT_EQ(sv_recv_cb_called, 4);
  ASSERT_EQ(cl_send_cb_called, 2);

  ASSERT_EQ(client.send_queue_size, 0);
  ASSERT_EQ(server.send_queue_size, 0);

  MAKE_VALGRIND_HAPPY();
#endif
  return 0;
}
