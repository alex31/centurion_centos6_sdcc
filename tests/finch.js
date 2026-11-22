'use strict';

const assert = require('node:assert/strict');
const fs = require('node:fs');
const {CoreEmulator} = require('../build/out/emulator/server/src/core/emulator');
const {FINCH_PLAT, FINCH_TRACK} = require('../build/out/emulator/shared/interfaces');

const origin = 0x2000;
const program = fs.readFileSync(process.argv[2]);
assert.equal(program[0x17], 0x79); // The bare-metal startup calls main.
assert.equal(program[0x1a], 0x00); // Return from main reaches HLT.

const machine = new CoreEmulator();
const {cpu, bpl, io_ffc: controller} = global.window;
assert.ok(controller, 'The simulator must load the microcoded Finch controller');
const disk = new Uint8Array(FINCH_PLAT * 2);
if (process.argv[4] === '--late-mount') {
    machine.reset();
    for (let cycle = 0; cycle < 107593; ++cycle) machine.step();
}
controller.unit_1.set_disk({
    type: 'finch', filename: 'scratch.ffi', stride: FINCH_TRACK,
    protect: false, data: disk, backing_data: disk.buffer,
});

machine.reset();
for (let i = 0; i < 10_000 && !cpu.at_boundry; ++i)
    machine.step();
assert.ok(cpu.at_boundry, 'CPU reset did not reach an instruction boundary');
for (let i = 0; i < program.length; ++i)
    bpl.writebyte(origin + i, program[i]);
cpu.set_pc(origin);

let returned = false;
for (let ticks = 0; ticks < 20_000_000; ++ticks) {
    if (cpu.at_boundry && cpu.pc === origin + 0x1a) {
        returned = true;
        break;
    }
    machine.step();
}
assert.ok(returned, `Finch test timed out at PC 0x${cpu.pc.toString(16)}`);
assert.equal(cpu.reg.b, 0, `Finch test returned 0x${cpu.reg.b.toString(16)}`);

const trackStart = FINCH_PLAT + FINCH_TRACK; // Head 1, cylinder 1.
assert.ok(disk.subarray(trackStart, trackStart + FINCH_TRACK).includes(0xa5));
assert.ok(disk.subarray(0, trackStart).every(byte => byte === 0));
assert.ok(disk.subarray(trackStart + FINCH_TRACK).every(byte => byte === 0));
if (process.argv[3]) fs.writeFileSync(process.argv[3], disk);
console.log('PASS: Finch track format, sector read/write, and unchanged neighbouring data. Microsteps',
            cpu.executedMicrosteps);
process.exit(0);
