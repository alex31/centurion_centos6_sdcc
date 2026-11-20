'use strict';

const assert = require('node:assert/strict');
const fs = require('node:fs');
const {OS6Session} = require('../scripts/os6-session.js');

function expectedBoards() {
    let seed = 1;
    let board = Array(69).fill(false);
    for (let cell = 2; cell < board.length - 2; cell++) {
        seed = (17 * seed + 43) % 256;
        board[cell] = seed < 128;
    }
    const rows = [];
    for (let generation = 0; generation <= 26; generation++) {
        rows.push('|' + board.map(alive => alive ? '*' : ' ').join('') + '|');
        board = board.map((alive, cell) => {
            if (cell < 2 || cell >= board.length - 2) return false;
            const neighbors = [cell - 2, cell - 1, cell + 1, cell + 2]
                .filter(index => board[index]).length;
            return neighbors === 2 || (alive ? neighbors === 4 : neighbors === 3);
        });
    }
    return rows;
}

async function main() {
    const [image, count = '1', report] = process.argv.slice(2);
    assert.ok(image && /^[1-9]\d*$/.test(count), 'Usage: basic-life.js IMAGE [REPEATS] [REPORT]');
    const session = new OS6Session(image);
    const expected = expectedBoards();
    const cpu = global.window.cpu;
    const step = cpu.hsstep.bind(cpu);
    const write = session.port.write_data.bind(session.port);
    let measured, ticks = 0;
    cpu.hsstep = () => {
        if (measured) ticks++;
        return step();
    };
    session.port.write_data = value => {
        const result = write(value);
        if (measured && (value & 127) === 10) {
            const lineStart = session.output.lastIndexOf('\n', session.output.length - 2) + 1;
            const line = session.output.slice(lineStart).replace(/\r?\n$/, '');
            const row = line.match(/^(\|[ *]{69}\|) *$/);
            if (line.trim() === '1D LIFE - B23/S24') measured.start = ticks;
            if (row) {
                measured.rows.push(row[1]);
                measured.last = ticks;
            }
        }
        return result;
    };
    async function command(text) {
        const from = session.output.length;
        await session.send(text + '\r');
        await session.waitFor('\n> ', from, 180000);
        const output = session.output.slice(from);
        assert.doesNotMatch(output, /ABORT\s+\d|Bytecode error|Syntax Error|File Error/);
        return output;
    }

    try {
        await session.boot();
        const startup = await command('BASIC');
        assert.match(startup, /\.LOAD BASIC SBASIC\.M[0-9A-F]{8}\b/);
        const loaded = await command('.LOAD "LIFE1D"');
        assert.match(loaded, /Loaded/);
        const samples = [];
        for (let iteration = 0; iteration < Number(count); iteration++) {
            measured = {rows: []};
            const output = await command('.RUN');
            assert.match(output, /DONE/);
            assert.deepEqual(measured.rows, expected, 'LIFE1D differs from the Life oracle');
            assert.ok(measured.start !== undefined && measured.last > measured.start);
            const cpuSteps = measured.last - measured.start;
            samples.push(cpuSteps);
            console.log(`PASS LIFE1D · 27 matching rows · ${cpuSteps} microsteps`);
            measured = undefined;
        }
        const sorted = [...samples].sort((a, b) => a - b);
        const mid = Math.floor(sorted.length / 2);
        const medianMicrosteps = sorted.length % 2 ? sorted[mid] : (sorted[mid - 1] + sorted[mid]) / 2;
        const result = {program: 'LIFE1D', rows: expected.length, columns: 69, samples, medianMicrosteps};
        if (report) fs.writeFileSync(report, JSON.stringify(result, null, 2) + '\n');
        console.log(JSON.stringify(result));
    } finally {
        session.stop();
    }
}

main().then(() => process.exit(0), error => {
    console.error(error);
    process.exit(1);
});
