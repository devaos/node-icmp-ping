/*
 * Copyright (c) 2017 Ari Aosved
 * http://github.com/devaos/node-icmp-ping/blob/master/LICENSE
 *
 * Based on ping.c by Mike Muuss
 *  U.S. Army Ballistic Research Laboratory
 *  December, 1983
 */

#ifndef PING_H
#define PING_H

#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <uv.h>

/* ========================================================================== */

#define MAXPACKET 65
#define MAXERRMSG 1024

/* ========================================================================== */

typedef struct ping_options_s ping_options_t;
typedef struct ping_state_s ping_state_t;
typedef struct ping_header_s ping_header_t;

typedef void (*cb_startup_t)(ping_state_t *);
typedef void (*cb_receipt_t)(ping_state_t *, float, struct timeval, struct timeval);
typedef void (*cb_complete_t)(ping_state_t *, int);

struct ping_options_s {
  /* who to ping */
  char* target;

  /* how many packets to send; default 5 */
  int probes;

  /* maximum amount of time (ms) to wait for all replies; default 6000ms */
  int timeout;

  /* when probes > 1, how long to wait (ms) between sending each probe; default 1000ms */
  int interval;

  /* function to call after resolving the host and right before the first probe is sent */
  cb_startup_t cb_startup;

  /* function to call on every probe response */
  cb_receipt_t cb_receipt;

  /* function to on any error call, after all probes have been sent and received, or after timeout has been exceeded */
  cb_complete_t cb_complete;

  /* use for pass through contextual data, this is untouched by ping routines */
  char *data;
};

struct ping_state_s {
  /* options */
  ping_options_t *options;

  /* who to ping */
  char target[255];
  struct addrinfo *to;
  char toip[17];
  int socket;

  /* outbound */
  u_char opacket[MAXPACKET];
  int osize;
  u_char *optr;
  int oleft;
  int transmitted;

  /* inbound */
  u_char ipacket[MAXPACKET];
  int isize;
  u_char *iptr;
  int ileft;
  int received;

  /* state */
  struct addrinfo *hints;
  struct timeval started;
  struct timeval ended;
  int err;
  char errmsg[MAXERRMSG];
  int pid;
  u_char instance;
  float tmin;
  float tmax;
  float tsum;

  /* internally used libuv constructs */
  uv_loop_t *loop_handle;
  uv_timer_t interval_handle;
  uv_timer_t timeout_handle;
  uv_poll_t poll_handle;
};

struct ping_header_s {
  u_long instance;
  struct timeval sent;
};

/* ========================================================================== */

ping_state_t * ping(const char *, ping_options_t *);

/* ========================================================================== */

void cleanup(ping_state_t *);
void on_target_resolved(uv_getaddrinfo_t *, int, struct addrinfo *);
void begin_probing(ping_state_t *);
void on_timeout(uv_timer_t *);
void on_interval(uv_timer_t *);
u_short cksum(struct icmp *, int);
void on_socket_poll(uv_poll_t *req, int, int);
void unpack(ping_state_t *, struct sockaddr_in *);
void tvsubtract(struct timeval *, struct timeval *);

/* ========================================================================== */

#endif