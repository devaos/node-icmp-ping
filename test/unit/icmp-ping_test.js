/**
 * Copyright (c) 2017 Ari Aosved
 * http://github.com/devaos/node-icmp-ping/blob/master/LICENSE
 */

'use strict';

// =============================================================================

const ping = require('../../lib/icmp-ping');

// =============================================================================

/* eslint-env and, mocha */

describe('ping', () => {
  describe('#probe()', () => {
    it('should return an error with bad parameters', (done) => {
      ping.probe(null, (err) => {
        if (err) done();
        else done(new Error('No error'));
      });
    });
  });
});
