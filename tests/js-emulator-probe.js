'use strict';

const fs = require('node:fs');
const {performance} = require('node:perf_hooks');
const {CoreEmulator} = require('../build/out/emulator/server/src/core/emulator');

const [program, count, mode] = process.argv.slice(2);
const cycles = Number(count);
if (!program || !Number.isSafeInteger(cycles) || cycles < 1 ||
    !['trace', 'bench', 'realtime'].includes(mode))
    throw new Error('Usage: node tests/js-emulator-probe.js PROGRAM CYCLES trace|bench|realtime');
const trace = mode === 'trace';
const code = fs.readFileSync(program);
new CoreEmulator();
const {cpu, bpl, __runHSHW} = global.window;
// Compare CPU and memory with the same set of devices as the native probe.
if (process.env.CENTURION_PROBE_TICK_DEVICES !== '1')
    __runHSHW.length = 0;

function hash(bytes) {
    let result = 2166136261;
    for (const byte of bytes)
        result = Math.imul(result ^ byte, 16777619);
    return result >>> 0;
}

function state() {
    const registers = new Uint8Array(256);
    const names = ['a', 'b', 'x', 'y', 'z', 's', 'c', 'p'];
    for (const [level, record] of cpu.registers.entries()) {
        for (const [index, name] of names.entries()) {
            registers[level * 16 + index * 2] = record[name] >> 8;
            registers[level * 16 + index * 2 + 1] = record[name];
        }
    }
    const fields = [cpu.executedMicrosteps, cpu.pc, +cpu.at_boundry, cpu.level, cpu.map,
        cpu.cc, cpu.memaddr, cpu.physaddr, hash(registers), hash(cpu.page_ram)];
    if (!trace) {
        const memory = Uint8Array.from({length: 0x3f000}, (_, address) => bpl.readmeta(address) & 255);
        fields.push(hash(memory));
    }
    console.log('state', ...fields);
}

cpu.reset();
cpu.hspre();
for (let steps = 0; steps < 10000 && !cpu.at_boundry; ++steps) {
    cpu.hsstep();
    if (trace) state();
}
if (!cpu.at_boundry)
    throw new Error('Reset did not reach an instruction boundary');
for (const [index, byte] of code.entries())
    bpl.writebyte(0x2000 + index, byte);
cpu.set_pc(0x2000);

if (mode === 'realtime') {
    const tickInterval = 20;
    const stepsPerMillisecond = 5000;
    const hardwareBatch = 100;
    const begin = performance.now();
    const cpuStart = process.cpuUsage();
    const latency = [];
    let previous = begin;
    let remaining = cycles;
    let credit = 0;
    let deadline = begin + tickInterval;
    const tick = () => {
        const started = performance.now();
        latency.push(started - deadline);
        credit = Math.min(credit + (started - previous) * stepsPerMillisecond,
                          2 * tickInterval * stepsPerMillisecond);
        const count = Math.min(remaining, Math.floor(credit / hardwareBatch) * hardwareBatch);
        credit -= count;
        previous = started;
        for (let step = 0; step < count; ++step)
            cpu.hsstep();
        remaining -= count;
        if (remaining) {
            deadline = started + tickInterval;
            setTimeout(tick, Math.max(0, deadline - performance.now()));
            return;
        }
        const seconds = (performance.now() - begin) / 1000;
        const usage = process.cpuUsage(cpuStart);
        latency.sort((a, b) => a - b);
        const p95 = latency[Math.ceil(latency.length * 0.95) - 1];
        console.log('realtime', seconds, (usage.user + usage.system) / 1_000_000, p95);
        state();
        cpu.hsend();
        process.exit(0);
    };
    setTimeout(tick, tickInterval);
} else if (trace) {
    for (let step = 0; step < cycles; ++step) {
        cpu.hsstep();
        state();
    }
} else {
    const begin = performance.now();
    for (let step = 0; step < cycles; ++step)
        cpu.hsstep();
    console.log('seconds', (performance.now() - begin) / 1000);
    state();
}
if (mode !== 'realtime') {
    cpu.hsend();
    process.exit(0);
}
