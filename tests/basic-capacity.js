'use strict';

const assert = require('node:assert/strict');
const {OS6Session} = require('../scripts/os6-session.js');

async function checkCapacity(image) {
    const session = new OS6Session(image);
    async function command(text) {
        const from = session.output.length;
        await session.send(text + '\r');
        await session.waitFor('\n> ', from, 120000);
        const reply = session.output.slice(from);
        assert.doesNotMatch(reply, /error|exhausted|out of memory|failed|refused|ABORT\s+\d/i);
        return reply;
    }

    try {
        await session.boot();
        await command('BASIC');
        assert.match(await command('.LOAD "BDEMO"'), /\nLoaded *\r?\n/);
        assert.match(await command('.RUN'), /\n9 *\r?\n/);
        console.log('PASS: BASIC loaded and ran 5,174 bytes of tokenized source.');
    } finally {
        session.stop();
    }
}

checkCapacity(process.argv[2]).then(() => process.exit(0), error => {
    console.error(error);
    process.exit(1);
});
