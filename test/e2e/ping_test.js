#!/usr/bin/env node

'use strict';

require('../../lib/icmp-ping').probe('google-public-dns-a.google.com')
  .once('startup', (stats) => {
    const msg = `PING ${stats.target} (${stats.ip}) ${stats.payloadBytes}(${stats.totalBytes}) bytes of data.\n`;
    process.stdout.write(msg);
  })
  .on('receipt', (stats) => {
    const msg = `${stats.receivedBytes} bytes from ${stats.target} (${stats.ip}): icmp_seq=${stats.received} time=${stats.triptime.toFixed(1)} ms\n`;
    process.stdout.write(msg);
  })
  .once('complete', (err, stats) => {
    if (err) {
      process.stdout.write(`${err.stack}\n`);
      return;
    }

    let packetLossMessage = '0% packet loss';

    if (stats.transmitted) {
      if (stats.received > stats.transmitted) {
        packetLossMessage = '-- somebody\'s printing up packets!';
      } else {
        const packetLoss = ((stats.transmitted - stats.received) * 100) / stats.transmitted;
        packetLossMessage = `${packetLoss}% packet loss`;
      }
    }

    const msg = `\n--- ${stats.target} ping statistics ---
${stats.transmitted} packets transmitted, ${stats.received} received, ${packetLossMessage}, time ${stats.runtime}ms
rtt min/avg/max = ${stats.tmin.toFixed(3)}/${(stats.tsum / stats.received).toFixed(3)}/${stats.tmax.toFixed(3)} ms\n`;
    process.stdout.write(msg);
  });
