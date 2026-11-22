'use strict';

const fs = require('node:fs');
const path = require('node:path');
const {OS6Session} = require('../scripts/os6-session.js');

const config = JSON.parse(fs.readFileSync(process.argv[2]));
const ready = /\n> $/;
const input = /\? $/;
const centos = /CRT0 READY/;
const runError = /Bytecode error[^\r\n]*|(?:ABORT|\bJX)\s*\d[^\r\n]*/;
const progress = /\n> $|\? $|CRT0 READY|(?:ABORT|\bJX)\s*\d/;

async function scan() {
    const session = new OS6Session(config.image);
    async function enter(text, marker, timeout) {
        const from = session.output.length;
        await session.send(text + '\r');
        await session.waitFor(marker, from, timeout);
        return session.output.slice(from);
    }

    async function probe(name) {
        const bytes = fs.readFileSync(path.join(config.work, name));
        // OS6Session owns a private disk buffer. The guest opens and reads
        // each fixture through its normal type A file and lexer paths.
        for (let i = 0; i < bytes.length; i += config.payload) {
            session.disk.image.data.set(bytes.subarray(i, i + config.payload),
                config.sectors[i / config.payload] * config.stride);
        }
        const load = await enter('.LOAD "BDEMO"', ready, 45000);
        if (!/\nLoaded *\r?\n/.test(load)) return {name, status: 'load-error', load};
        const from = session.output.length;
        await session.send('.RUN\r');
        let timedOut = false;
        try {
            await session.waitFor(progress, from, 20000);
        } catch {
            timedOut = true;
        }
        const transcript = session.output.slice(from);
        const diagnostic = transcript.match(runError)?.[0]?.trim();
        let status = 'returned-to-centos';
        if (diagnostic) status = 'run-error';
        else if (ready.test(transcript)) status = 'finished';
        else if (input.test(transcript)) status = 'input';
        else if (timedOut) status = 'run-timeout';
        if (!ready.test(transcript)) {
            if (!centos.test(transcript)) {
                const stop = session.output.length;
                await session.send('\x03');
                await session.waitFor(centos, stop);
            }
            await enter('BASIC', ready);
        }
        return {name, status, diagnostic, transcript};
    }

    try {
        await session.boot();
        await enter('BASIC', ready);
        for (const name of config.names) {
            let result;
            try {
                result = await probe(name);
            } catch (error) {
                result = {name, status: 'harness-error', error: String(error)};
            }
            fs.appendFileSync(config.output, JSON.stringify(result) + '\n');
            if (result.status === 'harness-error') {
                session.stop();
                await session.boot();
                await enter('BASIC', ready);
            }
        }
    } finally {
        session.stop();
    }
}

scan().then(() => process.exit(0), error => {
    console.error(error);
    process.exit(1);
});
