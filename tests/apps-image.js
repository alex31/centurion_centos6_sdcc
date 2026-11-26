'use strict';

const assert = require('node:assert/strict');
const {execFile} = require('node:child_process');
const path = require('node:path');
const {performance} = require('node:perf_hooks');
const {setImmediate: yieldNow} = require('node:timers/promises');
const {promisify} = require('node:util');

const commands = [['.RUN SDCC.XSIEVE', 'C_SIEVE;'], ['RUNTESTS', 'FOR SYSRDR']];
const root = path.resolve(__dirname, '..');
const built = path.join(root, 'build/out');
const exec = promisify(execFile);

function expectedPrimes(limit) {
    const primes = [];
    for (let n = 2; n <= limit; n++) {
        if (primes.every(p => p * p > n || n % p !== 0)) primes.push(n);
    }
    return primes;
}

function checkSieve(output) {
    const count = /^Count:\s*(\d+)/m.exec(output);
    const start = output.indexOf('2,3,5,7,');
    assert.ok(count && start >= 0, output);
    const primes = output.slice(start, count.index).replace(/\s+/g, '').split(',').map(Number);
    const expected = expectedPrimes(10000);
    assert.equal(Number(count[1]), expected.length);
    assert.deepEqual(primes, expected);
    console.log('PASS: XSIEVE produced all 1,229 expected primes under CENTOS.');
}

function checkRegressions(report, expected) {
    assert.doesNotMatch(report, /FAIL|ABORT\s+\d|JX\s*\d/);
    assert.equal([...report.matchAll(/\bPASS\b/g)].length, expected, report);
    console.log(`PASS: all ${expected} SDCC regression executables under CENTOS.`);
}

async function nativeJobs(image) {
    const {stdout: result} = await exec(path.join(built, 'emulator-cpp/cpp-hawk-boot'), [
        process.env.CENTURION_FIRMWARE || path.join(root, 'build/inputs/firmware.json'),
        path.join(root, 'swext/centurion-js/src/firmware-spec.json'), image, '--apps',
    ], {encoding: 'utf8', timeout: 120000});
    return parseJobs(result);
}

function parseJobs(output) {
    const jobs = [...output.matchAll(/^@@BEGIN (.+)\n([\s\S]*?)\n@@END (\d+) ([\d.e+-]+)$/gm)]
        .map(([, command, transcript, cycles, time]) => ({command, transcript, cycles: +cycles, time: +time}));
    assert.deepEqual(jobs.map(job => job.command), commands.map(([command]) => command));
    return jobs;
}

async function referenceJobs(image) {
    const {JavaScriptSession} = require('../scripts/os6-session.js');
    const session = new JavaScriptSession(image);
    const {cpu, io_mux, io_ffc, __runHW} = global.window;
    const batch = 100, slice = 100000;
    async function advance() {
        for (const port of io_mux.muxports)
            port.run_tx();
        cpu.hspre();
        for (let i = 0; i < slice / batch; ++i) {
            for (const hw of __runHW)
                hw.run(batch);
            for (let cycle = 0; cycle < batch; ++cycle)
                cpu.hsstep();
        }
        cpu.hsend();
        io_ffc.step(true);
        await yieldNow();
    }
    async function waitFor(marker, start = 0) {
        for (let i = 0; i < 12000 && !session.output.includes(marker, start); ++i)
            await advance();
        assert.ok(session.output.includes(marker, start), 'Missing prompt: ' + marker);
    }
    async function send(input) {
        for (const char of input) {
            for (let i = 0; i < 200 && session.port.read_busy; ++i)
                await advance();
            assert.equal(session.port.read_busy, false);
            session.port.receive(char.charCodeAt(0));
        }
    }
    session.machine.reset();
    for (const [prompt, input] of [['D=', 'H1'], ['SYSTEM DISK (S)= 1', '\r'],
        ['ENTER NEW SYSTEM DATE', '112226\r'], ['ENTER SYSTEM TIME', '120000\r']]) {
        await waitFor(prompt);
        await send(input);
    }
    await waitFor('CRT0 READY');
    const jobs = [];
    for (const [command, marker] of commands) {
        const start = session.output.length, initial = cpu.executedMicrosteps;
        const before = performance.now();
        await send(command + '\r');
        await waitFor(marker, start);
        const duration = (performance.now() - before) / 1000;
        jobs.push({command, transcript: session.output.slice(start),
            cycles: cpu.executedMicrosteps - initial, time: duration});
    }
    return jobs;
}

function verifyJobs(jobs, expected) {
    checkSieve(jobs[0].transcript);
    checkRegressions(jobs[1].transcript, expected);
}

async function checkImage() {
    const [image, count, mode] = process.argv.slice(2);
    const expected = Number(count);
    if (mode === '--native') {
        const jobs = await nativeJobs(image);
        verifyJobs(jobs, expected);
        for (const {command, cycles, time} of jobs)
            console.log(`${command}: ${cycles} microcycles in ${time}s`);
        return;
    }
    if (mode === '--compare') {
        const js = await referenceJobs(image);
        const cpp = await nativeJobs(image);
        verifyJobs(js, expected);
        verifyJobs(cpp, expected);
        for (let index = 0; index < cpp.length; ++index) {
            const a = js[index], b = cpp[index];
            assert.equal(a.cycles, b.cycles, b.command + ' did not run for the same microcycles');
            console.log(`${b.command}: ${b.cycles} microcycles, JS ${a.time.toFixed(3)} s, ` +
                        `C++ ${b.time.toFixed(3)} s, ${(a.time / b.time).toFixed(2)}x`);
        }
        return;
    }
    const {OS6Session} = require('../scripts/os6-session.js');
    const session = new OS6Session(image);
    try {
        await session.boot();
        const position = session.output.length;
        await session.send('.RUN SDCC.XSIEVE\r');
        await session.waitFor('C_SIEVE;', position, 120000);
        checkSieve(session.output.slice(position));
        const from = session.output.length;
        await session.send('RUNTESTS\r');
        await session.waitFor('FOR SYSRDR', from, 120000);
        checkRegressions(session.output.slice(from), expected);
        const {expectedBoards} = require('./basic-life.js');
        let previous;
        for (let run = 0; run < 2; run++) {
            const reply = await session.command('.RUN SDCC.XLIFE1D', 'DONE');
            assert.doesNotMatch(reply, /LIFESEED NOT UPDATED/);
            const seed = /1D LIFE - B23\/S24  SEED (\d+)/.exec(reply);
            assert.ok(seed, reply);
            const value = Number(seed[1]);
            if (previous !== undefined) assert.equal(value, (previous + 1) % 256);
            previous = value;
            const boards = [...reply.matchAll(/^(\|[ *]{69}\|)/gm)].map(match => match[1]);
            assert.deepEqual(boards, expectedBoards(value));
        }
        console.log('PASS: XLIFE1D saves the next seed and matches all 27 rows on both runs.');
    } finally {
        session.stop();
    }
}

checkImage().then(() => process.stdout.write('', () => process.exit(0)), error => {
    console.error(error);
    process.exit(1);
});
