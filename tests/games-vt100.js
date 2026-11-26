'use strict';

const assert = require('node:assert/strict');
const {OS6Session} = require('../scripts/os6-session.js');
const A_REVERSE = 0x1000; // Centurion CRT reverse-video attribute.

const CONNECT_PROMPT = 'Column (1-7), or Q to quit: ';
const CONNECT_BORDER = ' +---+---+---+---+---+---+---+';
const CHOMP_PROMPT = 'Your move (ROW COLUMN, or Q to quit): ';
const CHESS_MENU = '(W/B/2/Q): ';
const TERMINAL_PROBE = '\x1b[c\x1b\x05';
const CHESS_OPENING = [
    'rnbqkbnr', 'pppppppp', '        ', '        ',
    '        ', '        ', 'PPPPPPPP', 'RNBQKBNR',
];

async function checkGames(image) {
    const session = new OS6Session(image);
    const vt100 = process.env.CENTURION_OS6_BACKEND !== 'cpp';
    const terminal = vt100 ? 'VT100' : 'ADDS';
    const chessPrompt = vt100 ? '\x1b[K> ' : '\x1bK> ';
    const clearScreen = vt100 ? /\x1b\[2J/ : /\f/;
    const cell = (row, col) => String.fromCharCode(session.cell(col - 1, row - 1) & 127);
    const rowText = row => Array.from({length: 80}, (_, i) => cell(row, i + 1)).join('');

    // Replace the JS backend's built-in ADDS device. The session records the
    // wire output and we reply to the probe with VT100 device attributes.
    if (vt100) session.port.bind_dev({rts: true, receive() {}, check_send() {}, set_cts() {}});

    function checkConnectBorders() {
        assert.equal(rowText(5).trimEnd(), CONNECT_BORDER, 'Top border');
        assert.equal(rowText(12).trimEnd(), CONNECT_BORDER, 'Bottom border');
    }

    async function enter(command, prompt, detectTerminal = false) {
        const from = session.output.length;
        await session.send(command + '\r');
        if (detectTerminal && vt100) {
            await session.waitFor(TERMINAL_PROBE, from);
            await session.send('\x1b[?1;0c');
        }
        await session.waitFor(prompt, from, 90000);
        const reply = session.output.slice(from);
        assert.doesNotMatch(reply, /ABORT\s+\d|JX\s*\d|Engine error/);
        if (detectTerminal) {
            assert.ok(reply.includes(TERMINAL_PROBE), 'Missing terminal probe');
            assert.doesNotMatch(reply, /Terminal \[V\]|Detecting terminal/);
            assert.match(reply, clearScreen);
        }
        return reply;
    }

    function chessBoard(board, flipped = false) {
        assert.match(rowText(1), /^CHESS - Centurion CPU6/);
        assert.equal(cell(3, 6), flipped ? 'h' : 'a');
        assert.equal(cell(5, 2), flipped ? '1' : '8');
        for (let rank = 0; rank < 8; rank++) {
            for (let file = 0; file < 8; file++) {
                const piece = board[flipped ? 7 - rank : rank][flipped ? 7 - file : file];
                const value = session.cell(5 + 4 * file, 4 + 2 * rank);
                assert.equal(String.fromCharCode(value & 127), piece,
                    `Chess board row ${rank + 1}, file ${file + 1}`);
                assert.equal(Boolean(value & A_REVERSE), (rank + file) % 2 === 0);
            }
        }
    }

    function square(name) {
        const file = name.charCodeAt(0) - 97;
        return {rank: 8 - Number(name[1]), file};
    }

    function play(board, from, to) {
        const a = square(from), b = square(to);
        board[b.rank][b.file] = board[a.rank][a.file];
        board[a.rank][a.file] = ' ';
    }

    try {
        await session.boot();
        const directoryPage = /\(MORE\)|\nAVAILABLE:[^\n]*\n/;
        let page = await enter('.DIR SDCC. 1', directoryPage);
        let directory = page;
        while (page.includes('(MORE)')) {
            page = await enter('', directoryPage);
            directory += page;
        }
        assert.match(directory, /^XCHOMP\s+E\b/m);

        await enter('.RUN SDCC.XCHOMP', 'Number of rows (1-9): ');
        await enter('3', 'Number of columns (1-9): ');
        await enter('3', 'Who starts (1 = you, 2 = computer)? ');
        const initialChomp = await enter('1', CHOMP_PROMPT);
        assert.match(initialChomp, /^1   P \* \*/m);
        assert.match(await enter('9:9', CHOMP_PROMPT), /outside the remaining board/);
        const chompMove = await enter('1 2', CHOMP_PROMPT);
        assert.match(chompMove, /Computer takes row 2, column 1/);
        assert.match(chompMove, /^3   \. \. \./m);
        await enter('11', 'You took the poisoned square. Computer wins.');
        await enter('.STA', 'STATUS DISPLAY');
        console.log('PASS: XCHOMP is listed in SDCC and plays to completion.');

        await enter('.RUN SDCC.XCONNECT4', CONNECT_PROMPT, true);
        assert.match(rowText(1), /^CONNECT FOUR/);
        checkConnectBorders();
        await enter('0', CONNECT_PROMPT);
        assert.match(rowText(14), /^Enter a digit from 1 to 7/);
        await enter('4', CONNECT_PROMPT);
        const reply = /^Computer: column ([1-7])\. Your turn\./.exec(rowText(14));
        assert.ok(reply, rowText(14));
        const board = Array.from({length: 6}, (_, row) =>
            Array.from({length: 7}, (_, col) => cell(row + 6, 4 + col * 4)));
        const computer = Number(reply[1]) - 1;
        const expected = Array.from({length: 6}, () => Array(7).fill(' '));
        expected[5][3] = 'X';
        expected[computer === 3 ? 4 : 5][computer] = 'O';
        assert.deepEqual(board, expected);
        await enter('Q', 'End of Connect Four.');
        await enter('.STA', 'STATUS DISPLAY');
        console.log(`PASS: Connect Four detects ${terminal} silently and displays both moves.`);

        await enter('.RUN SDCC.XCHESS', CHESS_MENU, true);
        await enter('2', chessPrompt);
        const chess = CHESS_OPENING.map(rank => [...rank]);
        chessBoard(chess);
        await enter('HELP', chessPrompt);
        assert.match(rowText(24), /^e2e4 \| Castling/);
        await enter('e2e5', chessPrompt);
        assert.match(rowText(24), /^Illegal move/);
        chessBoard(chess);
        await enter('x'.repeat(75), chessPrompt);
        assert.match(rowText(24), /^Illegal move/);
        chessBoard(chess);

        for (const move of ['e2e4', 'h7h6', 'e4e5', 'd7d5', 'e5d6', 'c7d6',
                            'g1f3', 'g8f6', 'f1e2', 'b8c6', 'e1g1']) {
            const bytes = await enter(move, chessPrompt);
            assert.doesNotMatch(bytes, clearScreen, 'A move cleared the screen');
            play(chess, move.slice(0, 2), move.slice(2));
            if (move === 'e5d6') chess[3][3] = ' '; // En passant removes the pawn on d5.
            if (move === 'e1g1') play(chess, 'h1', 'f1');
            chessBoard(chess);
            assert.match(rowText(24), new RegExp('^Last move: ' + move));
            if (move === 'e2e4') console.log(`Chess e2e4 sends ${bytes.length} terminal bytes.`);
        }
        await enter('Q', 'End of Chess.');
        await enter('.STA', 'STATUS DISPLAY');
        console.log(`PASS: Chess detects ${terminal} silently and displays legal play, en passant and castling.`);

        await enter('.RUN SDCC.XCHESS', CHESS_MENU, true);
        await enter('B', chessPrompt);
        const opening = /^Computer plays ([a-h][12])([a-h][34])/.exec(rowText(24));
        assert.ok(opening, rowText(24));
        const delta = Math.abs(opening[1].charCodeAt(0) - opening[2].charCodeAt(0));
        if (opening[1][1] === '1') {
            assert.match(opening[1], /^[bg]1$/);
            assert.equal(delta, 1);
            assert.equal(opening[2][1], '3');
        } else {
            assert.equal(delta, 0);
        }
        const flipped = CHESS_OPENING.map(rank => [...rank]);
        play(flipped, opening[1], opening[2]);
        chessBoard(flipped, true);
        assert.match(rowText(22), /^Black to move\./);
        await enter('Q', 'End of Chess.');
        await enter('.STA', 'STATUS DISPLAY');
        console.log('PASS: Chess computer plays a legal opening move with the board flipped.');
    } finally {
        session.stop();
    }
}

checkGames(process.argv[2]).then(() => process.exit(0), error => {
    console.error(error);
    process.exit(1);
});
