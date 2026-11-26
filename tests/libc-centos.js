'use strict';

const assert = require('node:assert/strict');
const {OS6Session} = require('../scripts/os6-session.js');

async function main() {
    const session = new OS6Session(process.argv[2]);
    try {
        await session.boot();
        const start = session.output.length;
        await session.send('LIBCTEST\r');
        await session.waitFor('LIBC INPUT', start);
        const values = session.output.slice(start).match(/TIME (\d+) (\d+) (\d+)/);
        assert.ok(values, session.output.slice(start));
        const [value, saved, later] = values.slice(1).map(Number);
        const noon = Date.UTC(2026, 9, 28, 12) / 1000;
        assert.equal(value, saved);
        assert.ok(noon <= value && value <= later && later < noon + 120, values[0]);
        await session.send('A B\r\rZ\r');
        await session.waitFor('LIBC LONG INPUT', start);
        for (const [character, length] of [['X', 74], ['Y', 75], ['Z', 151]])
            await session.send(character.repeat(length) + '\r');
        await session.waitFor('LIBC BYTES', start);
        await session.send('A\x00\tB  \r');
        await session.waitFor('LIBC EDITED LINE', start);
        await session.send('A\bB\rA\x7fB\r');
        await session.waitFor('LIBC PRINTABLE', start);
        const printable = Array.from({length: 95}, (_, index) => String.fromCharCode(32 + index)).join('');
        await session.send(printable + '\r');
        await session.waitFor('LIBC DISCARD', start);
        await session.send('A' + '9'.repeat(200) + '\r');
        await session.waitFor('LIBC NEXT LINE', start);
        await session.send('Z\r');
        await session.waitFor('LIBC CHECKED INPUT', start);
        for (const input of ['12\x00tail', '9'.repeat(225), '2147483648', '23x', '42', ''])
            await session.send(input + '\r');
        await session.waitFor('LIBC SINGLE KEY', start);
        await session.send('x');
        await session.waitFor('LIBC CURSOR KEYS', start);
        await session.send('\x1bOA\x1bOB\x1b[C\x1b[D\x1b[5~\x1b[6~');
        await session.waitFor('LIBC ADDS KEYS', start);
        await session.send('\x1a\x0a\x06\x15\b');
        await session.waitFor('PASS stdlib', start);
        await session.waitFor('FLUSH AT RETURN', start);
        await session.waitFor('HELLO FROM C ON CENTURION CPU6', start);
        await session.waitFor('FOR SYSRDR', start);
        assert.doesNotMatch(session.output.slice(start), /FAIL|ABORT\s+\d|JX\s*\d/);
        await session.command('.STA', 'STATUS DISPLAY');
        await session.command('.RUN XLIBC', 'LIBC INPUT');
        const afterPrompt = session.output.length;
        await session.send('\x03');
        await session.waitFor('CRT0 READY', afterPrompt);
        await session.command('.STA', 'STATUS DISPLAY');
        console.log('PASS: clock, stdlib parsing, long and checked console lines, discard and exit status');
    } finally {
        session.stop();
    }
}

main().then(() => process.exit(0), error => {
    console.error(error);
    process.exit(1);
});
