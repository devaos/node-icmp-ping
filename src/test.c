/*
 * Copyright (c) 2017 Ari Aosved
 * http://github.com/devaos/node-icmp-ping/blob/master/LICENSE
 *
 * Based on ping.c by Mike Muuss
 *  U.S. Army Ballistic Research Laboratory
 *  December, 1983
 *
 * BUILD+RUN: gcc -g ping.c test.c -luv -o ping && sudo ./ping
 */

#include <stdlib.h>
#include "ping.h"

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
    fprintf(stderr, "ping failed - calloc() failure\n");
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

  free(context);
}

/* ========================================================================== */

int main(int argc, char *argv[])
{
  ping_state_t *context;
  ping_options_t options = {
    .probes = 5,
    .timeout = 6000,
    .interval = 1000,
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

  return 0;
}
