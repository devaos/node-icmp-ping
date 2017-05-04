/**
 * Copyright (c) 2017 Ari Aosved
 * http://github.com/devaos/node-icmp-ping/blob/master/LICENSE
 */

'use strict';

// =============================================================================

const EventEmitter = require('events');
const bindings = require('bindings')('icmp_ping.node');

// =============================================================================

class PingEmitter extends EventEmitter {}

// =============================================================================

exports.probe = (options, callback) => {
  let target = options;
  let probes = 5;
  let timeout = 6000;
  let interval = 1000;
  const context = new PingEmitter();

  if (options && typeof options === 'object') {
    if (probes in options) {
      probes = options.probes;
    }
    if (timeout in options) {
      timeout = options.timeout;
    }
    if (interval in options) {
      interval = options.interval;
    }
    target = options.target;
  }

  bindings.ping(target, probes, timeout, interval,
    // startup callback
    (stats) => {
      context.emit('startup', stats);
    },
    // receipt callback
    (stats) => {
      context.emit('receipt', stats);
    },
    // complete callback
    (err, stats) => {
      context.emit('complete', err, stats);
      if (callback) {
        callback(err, stats);
      }
    });

  return context;
};
