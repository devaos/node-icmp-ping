/*
 * Copyright (c) 2017 Ari Aosved
 * http://github.com/devaos/node-icmp-ping/blob/master/LICENSE
 *
 * Based on ping.c by Mike Muuss
 *  U.S. Army Ballistic Research Laboratory
 *  December, 1983
 */

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <uv.h>
#include "ping.h"

/* ========================================================================== */

extern int errno;
static int instances = 0;
ping_options_t ping_default_options = {NULL, 5, 6000, 1000, NULL, NULL, NULL, NULL};

/* ========================================================================== */
/*
static void WorkAsync(uv_work_t *req) {
  printf("WTF9?\n");
  //ping_state_t* context = (ping_state_t*)req->data;
  //uv_run(uv_default_loop(), UV_RUN_DEFAULT);
  printf("WTF10?\n");
}

static void WorkAsyncComplete(uv_work_t *req, int status) {
  printf("WTF11?\n");
}
*/
ping_state_t* ping(const char *target, ping_options_t *options) {
  int rc;
  ping_state_t *context = calloc(1, sizeof(ping_state_t));
  uv_getaddrinfo_t *resolver = NULL;

  if (!context) {
    if (options && options->cb_complete) {
      options->cb_complete(NULL, 0);
    }
    return NULL;
  }

  resolver = malloc(sizeof(uv_getaddrinfo_t));
  context->hints = malloc(sizeof(struct addrinfo));
  context->options = malloc(sizeof(ping_options_t));

  if (!resolver || !context->hints || !context->options) {
    context->err = -1;
    snprintf(context->errmsg, MAXERRMSG, "calloc() failure [%d]\n", errno);
    if (resolver) {
      free(resolver);
    }
    cleanup(context);
    return NULL;
  }

  if (options) {
    context->options = memcpy(context->options, options, sizeof(ping_options_t));
  } else {
    context->options = memcpy(context->options, &ping_default_options, sizeof(ping_options_t));
  }

  gettimeofday(&context->started, NULL);
  context->pid = getpid() & 0xFFFF;
  context->instance = ++instances;
  strncpy(context->target, target, sizeof(context->target)-1);
  context->osize = 64;
  context->tmin = 999999999;
  context->loop_handle = uv_default_loop();

  context->hints->ai_family = AF_INET;
  context->hints->ai_socktype = SOCK_RAW;
  context->hints->ai_protocol = IPPROTO_ICMP;
  context->hints->ai_flags = 0;

  resolver->data = context;

  rc = uv_getaddrinfo(context->loop_handle, resolver, on_target_resolved, context->target, NULL, context->hints);

  if (rc) {
    context->err = -1;
    snprintf(context->errmsg, MAXERRMSG, "uv_getaddrinfo() failure [%s]", uv_err_name(rc));
    cleanup(context);
    return NULL;
  }

  return context;
}

/* ========================================================================== */

void cleanup(ping_state_t *context) {
  int runtime;
  struct timeval tv;

  if (context->socket) {
    close(context->socket);
    context->socket = 0;
    uv_poll_stop(&context->poll_handle);
    uv_timer_stop(&context->interval_handle);
    uv_timer_stop(&context->timeout_handle);
  }

  gettimeofday(&context->ended, NULL);
  memcpy(&tv, &context->ended, sizeof(tv));
  tvsubtract(&tv, &context->started);
  runtime = tv.tv_sec * 1000 + (tv.tv_usec / 1000);

  if (context->options->cb_complete) {
    context->options->cb_complete(context, runtime);
  }

  if (context->to) {
    uv_freeaddrinfo(context->to);
    context->to = NULL;
  }

  if (context->hints) {
    free(context->hints);
    context->hints = NULL;
  }

  if (context->options) {
    free(context->options);
    context->options = NULL;
  }

  free(context);
}

/* ========================================================================== */

void on_target_resolved(uv_getaddrinfo_t *resolver, int status, struct addrinfo *res) {
  ping_state_t *context = (ping_state_t *)resolver->data;
  free(resolver);

  if (status < 0) {
    context->err = -1;
    snprintf(context->errmsg, MAXERRMSG, "uv_getaddrinfo() failure [%s]", uv_err_name(status));
    cleanup(context);
    return;
  }

  context->to = res;
  uv_ip4_name((struct sockaddr_in*) res->ai_addr, context->toip, 16);
  begin_probing(context);
}

/* ========================================================================== */

void begin_probing(ping_state_t *context) {
  int rc, on = 1;
  struct addrinfo *p;

  if (getuid() != 0) {
    context->err = -1;
    strcpy(context->errmsg, "implementation failure [Raw sockets for ICMP require root]");
    cleanup(context);
    return;
  }

  for (p = context->to; p; p = p->ai_next) {
    context->socket = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (context->socket == -1)
      continue;
    break;
  }

  if (!p) {
    context->socket = 0;
    context->err = errno;
    snprintf(context->errmsg, MAXERRMSG, "socket() failure [%s]", strerror(errno));
    cleanup(context);
    return;
  }

  uv_timer_init(context->loop_handle, &context->interval_handle);
  uv_timer_init(context->loop_handle, &context->timeout_handle);
  uv_poll_init_socket(context->loop_handle, &context->poll_handle, context->socket);
  context->poll_handle.data = context;
  uv_poll_start(&context->poll_handle, UV_READABLE | UV_DISCONNECT, on_socket_poll);
  context->timeout_handle.data = context;
  uv_timer_start(&context->timeout_handle, on_timeout, context->options->timeout, 0);

  rc = setsockopt(context->socket, SOL_SOCKET,  SO_REUSEADDR, (char *)&on, sizeof(on));
  if (rc < 0) {
    context->err = errno;
    snprintf(context->errmsg, MAXERRMSG, "setsockopt() failure [%s]", strerror(errno));
    cleanup(context);
    return;
  }

  rc = connect(context->socket, context->to->ai_addr, sizeof(struct sockaddr));
  if (rc < 0) {
    context->err = errno;
    snprintf(context->errmsg, MAXERRMSG, "connect() failure [%s]", strerror(errno));
    cleanup(context);
    return;
  }

  if (context->options->cb_startup) {
    context->options->cb_startup(context);
  }

  context->interval_handle.data = context;
  on_interval(&context->interval_handle);
  uv_timer_start(&context->interval_handle, on_interval, context->options->interval, context->options->interval);
}

/* ========================================================================== */

void on_timeout(uv_timer_t *timeout) {
  cleanup((ping_state_t *)timeout->data);
}

/* ========================================================================== */

void on_interval(uv_timer_t *timeout) {
  int i;
  ping_state_t *context = (ping_state_t *) timeout->data;
  struct icmp *icp = (struct icmp *) context->opacket;
  ping_header_t *header = (ping_header_t *) &context->opacket[8];
  u_char *data = &context->opacket[8+sizeof(ping_header_t)];

  icp->icmp_type = ICMP_ECHO;
  icp->icmp_code = 0;
  icp->icmp_cksum = 0;
  icp->icmp_seq = ++context->transmitted;
  icp->icmp_id = context->pid;

  header->instance = context->instance;
  gettimeofday(&header->sent, NULL);

  for (i = 8+sizeof(ping_header_t); context->transmitted == 1 && i < context->osize; i++)
    *data++ = i;

  icp->icmp_cksum = cksum(icp, context->osize);

  context->optr = context->opacket;
  context->oleft = context->osize;
  uv_poll_stop(&context->poll_handle);
  uv_poll_start(&context->poll_handle, UV_WRITABLE | UV_READABLE | UV_DISCONNECT, on_socket_poll);
}

/* ========================================================================== */

u_short cksum(struct icmp *addr, int len) {
  int nleft = len, sum = 0;
  u_short *w = (u_short *) addr, answer;

  while (nleft > 1)  {
    sum += *w++;
    nleft -= 2;
  }

  if (nleft == 1) {
    u_short u = 0;
    *(u_char *)(&u) = *(u_char *)w;
    sum += u;
  }

  sum = (sum >> 16) + (sum & 0xffff);
  sum += (sum >> 16);
  answer = ~sum;
  return (answer);
}

/* ========================================================================== */

void on_socket_poll(uv_poll_t *poll_handle, int status, int events) {
  int sent, received;
  struct sockaddr_in from;
  ping_state_t *context = (ping_state_t *) poll_handle->data;

  if (status < 0) {
    context->err = -1;
    snprintf(context->errmsg, MAXERRMSG, "uv_poll_start() failure [%s]", uv_err_name(status));
    cleanup(context);
    return;
  }

  if (!(events & (UV_WRITABLE | UV_READABLE))) {
    context->err = errno;
    snprintf(context->errmsg, MAXERRMSG, "socket closed prematurely [%s]", strerror(errno));
    cleanup(context);
    return;
  }

  if ((events & UV_WRITABLE) && context->oleft > 0) {
    sent = send(context->socket, context->optr, context->oleft, 0);

    if (sent < 0) {
      context->err = errno;
      snprintf(context->errmsg, MAXERRMSG, "send() failed [%s]", strerror(errno));
      cleanup(context);
      return;
    }

    if (sent != context->oleft)  {
      context->optr += sent;
      context->oleft -= sent;
      return;
    }

    context->oleft -= sent;
    context->iptr = context->ipacket;
    context->isize = context->osize;
    if (context->ileft == 0) {
      context->ileft = context->isize;
    }
    uv_poll_stop(&context->poll_handle);
    uv_poll_start(&context->poll_handle, UV_READABLE | UV_DISCONNECT, on_socket_poll);
  }

  if ((events & UV_READABLE) && context->ileft > 0) {
    received = recv(context->socket, context->iptr, context->ileft, 0);

    if (received < 0) {
      if (errno == EINTR) {
        return;
      }

      context->err = errno;
      snprintf(context->errmsg, MAXERRMSG, "recv failed [%s]", strerror(errno));
      cleanup(context);
      return;
    }

    if (received != context->ileft)  {
      context->iptr += received;
      context->ileft -= received;
      return;
    }

    context->ileft = context->isize;
    unpack(context, &from);

    if (context->options->probes > 0 && context->received >= context->options->probes) {
      cleanup(context);
    }
  }
}

/* ========================================================================== */

void unpack(ping_state_t *context, struct sockaddr_in *from) {
  int hlen;
  float triptime;
  struct ip *ip;
  struct icmp *icp;
  ping_header_t *header;
  struct timeval received;

  from->sin_addr.s_addr = ntohl(from->sin_addr.s_addr);
  gettimeofday(&received, NULL);

  ip = (struct ip *) context->ipacket;
  hlen = ip->ip_hl << 2;

  if (context->isize < hlen + ICMP_MINLEN) {
    return;
  }

  icp = (struct icmp *)(context->ipacket + hlen);
  if (icp->icmp_id != context->pid) {
    return;
  }

  if (icp->icmp_type != ICMP_ECHOREPLY) {
    return;
  }

  header = (ping_header_t *) &icp->icmp_data[0];
  if (header->instance != context->instance) {
    return;
  }

  tvsubtract(&received, &header->sent);
  triptime = (float)(received.tv_sec * 1000000 + (received.tv_usec)) / 1000;
  context->tsum += triptime;

  if (triptime < context->tmin) {
    context->tmin = triptime;
  }
  if (triptime > context->tmax) {
    context->tmax = triptime;
  }

  context->received++;

  if (context->options->cb_receipt) {
    context->options->cb_receipt(context, triptime, header->sent, received);
  }
}

/* ========================================================================== */

void tvsubtract(struct timeval *out, struct timeval *in) {
  if ((out->tv_usec -= in->tv_usec) < 0) {
    out->tv_sec--;
    out->tv_usec += 1000000;
  }

  out->tv_sec -= in->tv_sec;
}
