'use strict';

const assert = require('node:assert/strict');
const {OS6Session} = require('../scripts/os6-session.js');

async function checkImage() {
    const session = new OS6Session(process.argv[2]);
    try {
        await session.boot();
        const position = session.output.length;
        await session.send('.RUN XSIEVE\r');
        await session.waitFor('C_SIEVE;', position, 120000);
        assert.match(session.output.slice(position), /Count:\s*1229\b/);
        console.log('PASS: XSIEVE reported 1,229 primes under CENTOS.');
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
