/*
 * Copyright (c) 2017 Ari Aosved
 * http://github.com/devaos/node-icmp-ping/blob/master/LICENSE
 *
 * Based on ping.c by Mike Muuss
 *  U.S. Army Ballistic Research Laboratory
 *  December, 1983
 */

#include <stdlib.h>
#include "ping.h"

/* ========================================================================== */

extern ping_options_t ping_default_options;

/* ========================================================================== */

void on_startup(ping_state_t *context) {
  printf("PING %s (%s) %d(%d) bytes of data.\n",
    context->target, context->toip, context->osize - 8, context->osize);
}

/* ========================================================================== */

void on_receipt(ping_state_t *context, float triptime, struct timeval sent, struct timeval received) {
  printf("%d bytes from %s (%s): icmp_seq=%d time=%.1f ms\n",
    context->isize, context->target, context->toip, context->received, triptime);
}

/* ========================================================================== */

void on_complete(ping_state_t *context, int runtime) {
  if (!context) {
    fprintf(stderr, "ping failed\n");
    exit(-1);
  }

  if (context->err) {
    fprintf(stderr, "ping failed - %s\n", context->errmsg);
    exit(-1);
  }

  printf("\n--- %s ping statistics ---\n", context->target);
  printf("%d packets transmitted, ", context->transmitted);
  printf("%d received, ", context->received);
  if (context->transmitted) {
    if(context->received > context->transmitted) {
      printf("-- somebody's printing up packets!");
    }
    else {
      printf("%d%% packet loss", (int)(((context->transmitted - context->received) * 100) / context->transmitted));
    }
  } else {
    printf("0%% packet loss");
  }
  printf(", time %dms\n", runtime);
  if (context->received) {
    printf("rtt min/avg/max = %.3f/%.3f/%.3f ms\n", context->tmin, context->tsum / context->received, context->tmax);
  }
}

/* ========================================================================== */

int main(int argc, char *argv[])
{
  ping_state_t *context;
  ping_options_t options = {
    .probes = ping_default_options.probes,
    .timeout = ping_default_options.timeout,
    .interval = ping_default_options.interval,
    .cb_startup = on_startup,
    .cb_receipt = on_receipt,
    .cb_complete = on_complete,
    .data = NULL
  };

  if (argc >= 2) {
    context = ping(argv[1], &options);
  } else {
    context = ping("google-public-dns-a.google.com", &options);
  }

  if (!context) {
    fprintf(stderr, "calloc() failure [%d]\n", errno);
    exit(-1);
  }

  uv_run(context->loop_handle, UV_RUN_DEFAULT);
  return 0;
}