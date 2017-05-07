#!/usr/bin/env node

'use strict';

const ping = require('../../lib/icmp-ping');

const targets = [
  'google-public-dns-a.google.com',
  'google.com',
  'youtube.com',
  'facebook.com',
  'baidu.com',
  'wikipedia.org',
  'yahoo.com',
  'reddit.com',
  'google.co.in',
  'qq.com',
  'taobao.com',
  'amazon.com',
  'tmall.com',
  'twitter.com',
  'google.co.jp',
  'sohu.com',
  'live.com',
  'vk.com',
  'instagram.com',
  'sina.com.cn',
  '360.cn',
];

let done = 0;

function run() {
  process.stdout.write('------\n');

  const report = (err, stats) => {
    if (err) {
      process.stdout.write(`${err.stack}\n`);
      return;
    }

    let packetLossMessage = '0% packet loss';

    if (stats.transmitted) {
      if (stats.received > stats.transmitted) {
        packetLossMessage = 'packet loss assertion failure';
      } else {
        const packetLoss = ((stats.transmitted - stats.received) * 100) / stats.transmitted;
        packetLossMessage = `${packetLoss}% packet loss`;
      }
    }

    const msg = `${stats.target} -- ${stats.transmitted} transmitted, ${stats.received} received, ${packetLossMessage}, ${(stats.tsum / stats.received).toFixed(3)} avg ttl, time ${stats.runtime}ms\n`;
    process.stdout.write(msg);

    done += 1;
    if (done === targets.length) {
      run();
    }
  };

  done = 0;
  for (let i = 0; i < targets.length; i += 1) {
    ping.probe({ target: targets[i], probes: 5, timeout: 6000 }, report);
  }
}

run();
