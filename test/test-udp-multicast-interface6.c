/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
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

static int sv_send_cb_called;
static int close_cb_called;


static void close_cb(uv_handle_t* handle) {
  CHECK_HANDLE(handle);
  close_cb_called++;
}


static void sv_send_cb(uv_udp_send_t* req, int status) {
  ASSERT_NOT_NULL(req);
  ASSERT(status == 0);
  CHECK_HANDLE(req->handle);

  sv_send_cb_called++;

  uv_close((uv_handle_t*) req->handle, close_cb);
}


TEST_IMPL(udp_multicast_interface6) {
/* TODO(gengjiawen): Fix test on QEMU. */
#if defined(__QEMU__)
  RETURN_SKIP("Test does not currently work in QEMU");
#endif

#ifndef __OS2__
  int r;
  uv_udp_send_t req;
  uv_buf_t buf;
  struct sockaddr_in6 addr;
  struct sockaddr_in6 baddr;
#endif

  if (!can_ipv6())
    RETURN_SKIP("IPv6 not supported");

#ifndef __OS2__
  ASSERT(0 == uv_ip6_addr("::1", TEST_PORT, &addr));

  r = uv_udp_init(uv_default_loop(), &server);
  ASSERT(r == 0);

  ASSERT(0 == uv_ip6_addr("::", 0, &baddr));
  r = uv_udp_bind(&server, (const struct sockaddr*)&baddr, 0);
  ASSERT(r == 0);

#if defined(__APPLE__) || defined(__FreeBSD__)
  r = uv_udp_set_multicast_interface(&server, "::1%lo0");
#else
  r = uv_udp_set_multicast_interface(&server, NULL);
#endif
  ASSERT(r == 0);

  /* server sends "PING" */
  buf = uv_buf_init("PING", 4);
  r = uv_udp_send(&req,
                  &server,
                  &buf,
                  1,
                  (const struct sockaddr*)&addr,
                  sv_send_cb);
  ASSERT(r == 0);

  ASSERT(close_cb_called == 0);
  ASSERT(sv_send_cb_called == 0);

  /* run the loop till all events are processed */
  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT(sv_send_cb_called == 1);
  ASSERT(close_cb_called == 1);

  MAKE_VALGRIND_HAPPY();
#endif
  return 0;
}
