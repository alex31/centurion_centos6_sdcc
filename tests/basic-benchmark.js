'use strict';

const assert = require('node:assert/strict');
const fs = require('node:fs');
const {OS6Session} = require('../scripts/os6-session.js');

const increment = ['400 K=0', '500 K=K+1'];
const repeat = '600 IF K<1000 THEN 500';
const arithmetic = '510 A=K/2*3+4-5';
const call = '520 GOSUB 820';
const subroutine = '820 RETURN';
const array = '100 DIM M(5)';
const loop = '530 FOR L=1 TO 5';
const next = '540 NEXT L';
const benchmarks = {
    BM1: {
        body: ['400 FOR K=1 TO 1000', '500 NEXT K'],
        expected: [1001],
        c64Seconds: 1.4,
    },
    BM2: {
        body: [...increment, repeat],
        expected: [1000],
        c64Seconds: 10.5,
    },
    BM3: {
        body: [...increment, '510 A=K/K*K+K-K', repeat],
        variables: ['K', 'A'],
        expected: [1000, 1000],
        c64Seconds: 19.2,
    },
    BM4: {
        body: [...increment, arithmetic, repeat],
        variables: ['K', 'A'],
        expected: [1000, 1499],
        c64Seconds: 20.0,
    },
    BM5: {
        body: [...increment, arithmetic, call, repeat, subroutine],
        variables: ['K', 'A'],
        expected: [1000, 1499],
        c64Seconds: 21.0,
    },
    BM6: {
        body: [array, ...increment, arithmetic, call, loop, next, repeat, subroutine],
        variables: ['K', 'A', 'L'],
        expected: [1000, 1499, 6],
        c64Seconds: 32.2,
    },
    BM7: {
        body: [array, ...increment, arithmetic, call, loop, '535 M(L)=A', next, repeat, subroutine],
        variables: ['K', 'A', 'M(1)', 'M(2)', 'M(3)', 'M(4)', 'M(5)'],
        expected: [1000, 1499, 1499, 1499, 1499, 1499, 1499],
        c64Seconds: 51.6,
    },
    BM8: {
        body: [...increment, '530 A=K^2', '540 B=LOG(K)', '550 C=SIN(K)', repeat],
        variables: ['K', 'A', 'B', 'C'],
        expected: [1000, 1_000_000, Math.log(1000), Math.sin(1000)],
        tolerance: [0, 0, 0.00002, 0.00002],
        c64Seconds: 116.0,
    },
};

async function main() {
    const [image, report, selection = 'BM1,BM2'] = process.argv.slice(2);
    assert.ok(image, 'Usage: basic-benchmark.js IMAGE [REPORT] [BM1,BM2,...|all]');
    const selected = selection === 'all' ? Object.keys(benchmarks) : selection.split(',');
    assert.ok(selected.every(name => Object.hasOwn(benchmarks, name)), 'Unknown BASIC benchmark');
    const session = new OS6Session(image);
    const cpu = global.window.cpu;
    const step = cpu.hsstep.bind(cpu);
    const write = session.port.write_data.bind(session.port);
    let start, end, steps = 0;
    cpu.hsstep = () => {
        if (start !== undefined && end === undefined) ++steps;
        return step();
    };
    session.port.write_data = value => {
        const result = write(value);
        if ((value & 127) === 10) {
            const from = session.output.lastIndexOf('\n', session.output.length - 2) + 1;
            const line = session.output.slice(from).trim();
            if (line === 'S') start = steps;
            if (line === 'E' && start !== undefined) end = steps;
        }
        return result;
    };

    async function command(text) {
        const from = session.output.length;
        await session.send(text + '\r');
        await session.waitFor(/\n> |ABORT\s+\d/, from, 650000);
        const output = session.output.slice(from);
        assert.doesNotMatch(output, /Bytecode error|Syntax Error|File Error|ABORT\s+\d/);
        return output;
    }

    try {
        await session.boot();
        await command('STARTBASIC');
        const results = {};
        for (const name of selected) {
            const {body, variables = ['K'], expected, tolerance = [], c64Seconds} = benchmarks[name];
            await command('.NEW');
            // Checking the final values takes place after the end marker.
            const print = '710 PRINT "RESULT ";' + variables.join(';",";');
            for (const line of ['300 PRINT "S"', ...body, '700 PRINT "E"', print, '800 END'])
                await command(line);
            start = end = undefined;
            steps = 0;
            const output = await command('.RUN');
            const match = /\nRESULT ([^\r\n]+)\r?\n/.exec(output);
            assert.ok(match, output);
            const values = match[1].split(',').map(Number);
            assert.equal(values.length, expected.length, output);
            expected.forEach((value, i) => {
                assert.ok(Math.abs(values[i] - value) <= (tolerance[i] ?? 0), output);
            });
            assert.ok(start !== undefined && end > start, output);
            const microcycles = end - start;
            const secondsAt5MHz = microcycles / 5_000_000;
            results[name] = {microcycles, secondsAt5MHz, c64Seconds};
            console.log(name, secondsAt5MHz.toFixed(3) + ' s at 5 MHz');
        }
        const result = {image: fs.realpathSync(image), results};
        if (report) fs.writeFileSync(report, JSON.stringify(result, null, 2) + '\n');
    } finally {
        session.stop();
    }
}

main().then(() => process.exit(0), error => { console.error(error); process.exit(1); });
