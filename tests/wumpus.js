'use strict';

const assert = require('node:assert/strict');
const {OS6Session} = require('../scripts/os6-session.js');

const instructions = 'Instructions? (y-n) ';
const turn = 'Move or shoot (m-s, q to quit) ';
const again = 'Another game? (y-n) ';
const sameCave = 'Same room setup? (y-n) ';

function location(output) {
    const room = output.match(/You are in room (\d+)/);
    const exits = output.match(/There are tunnels to (\d+) (\d+) (\d+)/);
    assert.ok(room && exits, output);
    return {room: Number(room[1]), exits: exits.slice(1).map(Number)};
}

async function test(image, program) {
    const session = new OS6Session(image);
    function reply(text, marker) {
        return session.command(text, marker);
    }
    async function badAction(text) {
        const output = await reply(text, turn);
        assert.match(output, /Invalid input\. Try again\./, output);
        assert.doesNotMatch(output, /You are in room |You hit the wall|last shot/, output);
    }

    try {
        await session.boot();
        await reply('.RUN ' + program, instructions);
        for (const bad of ['', 'Y garbage', 'y\x00n', 'y' + ' '.repeat(225)])
            assert.match(await reply(bad, instructions), /Enter Y, N or Q\./);
        const opening = await reply(' \tY ', turn);
        assert.match(opening, /The Wumpus lives in a cave of 20 rooms/);
        assert.match(opening, /Each room has 3 tunnels/);
        const start = location(opening);
        const tunnel = start.exits[0];

        for (const bad of [
            '', ' \t ', '\x1b[A', Buffer.from('é').toString('latin1'),
            'm 1x', 'm -1', 'm 21', 'm 4294967297', 'm 2147483648',
            'm 999999999999999999999999', 'm 0', 'm 1 2', 'm 1\x00x',
            's 0', 's 1 0 2', 's 1 2 3 4 5 6', 's 1 2 x', 'q x',
            'q ' + ' '.repeat(225),
        ]) await badAction(bad);

        assert.match(await reply('m ' + start.room, turn), /You hit the wall/);
        await reply('M', 'Which room? ');
        await badAction('1x');
        await reply('s', 'Rooms (0 to finish)? ');
        await reply(String(tunnel), 'Rooms (0 to finish)? ');
        await badAction('0 x');

        await reply('S ' + tunnel, 'Rooms (0 to finish)? ');
        const outcome = await reply(start.room + ' 0', again);
        assert.match(outcome, /You (?:shot yourself|slew the wumpus)/, outcome);
        assert.match(await reply('x', again), /Enter Y, N or Q\./);
        await reply('y', sameCave);
        assert.match(await reply('n\x00y', sameCave), /Enter Y, N or Q\./);

        const replay = location(await reply('y', turn));
        await reply('M', 'Which room? ');
        const afterMove = await reply(String(replay.exits[0]), /Move or shoot \(m-s, q to quit\) |Another game\? \(y-n\) /);
        assert.match(afterMove, new RegExp('You are in room ' + replay.exits[0] + '\\D'));
        const beforeExit = session.output.length;
        await session.send(' \tQ \r');
        await reply('.STA', 'STATUS DISPLAY');
        assert.doesNotMatch(session.output.slice(beforeExit), /Invalid input|JX\d+|ABORT\s+\d/);
        await reply('.RUN ' + program, instructions);
        const interrupted = session.output.length;
        await session.send('\x03');
        await session.waitFor('CRT0 READY', interrupted);
        await reply('.STA', 'STATUS DISPLAY');
        console.log('PASS: Wumpus rejects invalid input, plays a shot and a move, replays and returns to CENTOS.');
    } finally {
        session.stop();
    }
}

const [, , image, program = 'SDCC.XWUMPUS'] = process.argv;
test(image, program).then(() => process.exit(0), error => {
    console.error(error);
    process.exit(1);
});
