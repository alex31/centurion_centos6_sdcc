'use strict';

const assert = require('node:assert/strict');
const {OS6Session} = require('../scripts/os6-session.js');

function expectedPrimes(limit) {
    const primes = [];
    for (let n = 2; n <= limit; n++) {
        if (primes.every(p => p * p > n || n % p !== 0)) primes.push(n);
    }
    return primes;
}

async function checkImage() {
    const session = new OS6Session(process.argv[2]);
    try {
        await session.boot();
        const position = session.output.length;
        await session.send('.RUN XSIEVE\r');
        await session.waitFor('C_SIEVE;', position, 120000);
        const output = session.output.slice(position);
        const count = /^Count:\s*(\d+)/m.exec(output);
        const start = output.indexOf('2,3,5,7,');
        assert.ok(count && start >= 0, output);
        const primes = output.slice(start, count.index).replace(/\s+/g, '').split(',').map(Number);
        const expectedPrimesThrough10000 = expectedPrimes(10000);
        assert.equal(Number(count[1]), expectedPrimesThrough10000.length);
        assert.deepEqual(primes, expectedPrimesThrough10000);
        console.log('PASS: XSIEVE produced all 1,229 expected primes under CENTOS.');
        const expected = Number(process.argv[3]);
        const from = session.output.length;
        await session.send('RUNTESTS\r');
        await session.waitFor('FOR SYSRDR', from, 120000);
        const report = session.output.slice(from);
        assert.doesNotMatch(report, /FAIL|ABORT\s+\d|JX\s*\d/);
        assert.equal([...report.matchAll(/\bPASS\b/g)].length, expected, report);
        console.log(`PASS: all ${expected} SDCC regression executables under CENTOS.`);
    } finally {
        session.stop();
    }
}

checkImage().then(() => process.exit(0), error => {
    console.error(error);
    process.exit(1);
});
