let ping = require('../../lib/icmp-ping')

var assert = require('assert');
describe('ping', function() {
  describe('#probe()', function() {
    it('should return an error with bad parameters', function(done) {
      ping.probe(null, function(err) {
        if (err) done();
        else done(new Error('No error'));
      });
    });
  });
});
