'use strict';

const assert = require('node:assert/strict');
const {OS6Session} = require('../scripts/os6-session.js');
const {CenturionCRT} = require('../build/out/emulator/server/src/terminal/crt.js');

const ready = /\n> $/;
const input = /\? $/;

const games = {
    G23MTCH: [['4', '1'], /YOU CHEATED![\s\S]*I TOOK\s*[123]/],
    CHEMST: [['-1'], /KRYPTOCYANIC ACID[\s\S]*QUIVERING PROTOPLASM/],
    CHIEF: [['YES', '7', 'NO', '8', 'NO'], /I HOPE YOU BELIEVE ME NOW/],
    DICE: [['5', 'NO'], /TOTAL SPOTS +NUMBER OF TIMES/],
    HILO: [[...Array(6).fill('-1'), 'NO'], /THE NUMBER WAS\s*\d+[\s\S]*SO LONG/],
    HURKLE: [['-1', '-1', '-1', '-1'], /GO NORTHEAST[\s\S]*GUESS #3/],
    LETTER: [['a', 'z'], /LETTER GUESSING GAME[\s\S]*(TOO HIGH|YOU GOT IT)/],
    NICOMA: [['2', '2', '0', 'YES'], /YOUR NUMBER WAS\s*77[\s\S]*HOW ABOUT THAT/],
    ROCKSP: [['3', '1', '2', '3'], /HERE IS THE FINAL SCORE[\s\S]*ENDED IN A TIE/],
    STARS: [['0', ...Array(7).fill('-1')], /\*{1,7}[\s\S]*NUMBER WAS\s*\d+/],
    TRAIN: [['0', '3', 'NO'], /CORRECT ANSWER IS\s*[\d.]+ HOURS/],
};

async function checkGames(image, names) {
    const session = new OS6Session(image);
    const crt = new CenturionCRT();
    const write = session.port.write_data.bind(session.port);
    session.port.write_data = byte => {
        crt.receive(byte & 127);
        return write(byte);
    };

    async function enter(text, marker) {
        const from = session.output.length;
        await session.send(text + '\r');
        await session.waitFor(marker, from, 90000);
        const reply = session.output.slice(from);
        assert.doesNotMatch(reply, /Bytecode error|File Error|ABORT\s+\d|JX\s*\d/);
        return reply;
    }

    try {
        await session.boot();
        await enter('BASIC', ready);
        for (const name of names) {
            const [answers, expected] = games[name];
            assert.match(await enter(`.LOAD "${name}"`, ready), /\nLoaded *\r?\n/);
            let reply = await enter('.RUN', input);
            let transcript = reply;
            for (const answer of answers) {
                assert.match(reply, input, name);
                reply = await enter(answer, /\? $|\n> $/);
                transcript += reply;
            }
            assert.match(transcript, expected, name);
            if (name === 'CHIEF') {
                const rows = Array.from({length: 24}, (_, y) =>
                    Array.from({length: 80}, (_, x) =>
                        String.fromCharCode(crt.getCell(x, y) & 127)).join(''));
                assert.ok(rows.some(row => /^ {10}\* *$/.test(row)), rows.join('\n'));
            }
            if (!ready.test(reply)) {
                const from = session.output.length;
                await session.send('\x03');
                await session.waitFor('CRT0 READY', from);
                await enter('BASIC', ready);
            }
            console.log(`PASS BASIC game ${name}`);
        }
    } finally {
        session.stop();
    }
}

const [, , image, ...names] = process.argv;
checkGames(image, names.length ? names : Object.keys(games)).then(
    () => process.exit(0), error => {
        console.error(error);
        process.exit(1);
    });
