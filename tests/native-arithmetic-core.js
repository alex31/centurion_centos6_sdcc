'use strict';
// Execute the same binary records as native-arithmetic-host.c, one ISA
// instruction at a time, using the independent microcode implementation.
const fs = require('node:fs'), path = require('node:path');
const assert = require('node:assert/strict');
const root = path.resolve(__dirname, '..');
const {CoreEmulator} = require(root + '/build/out/emulator/server/src/core/emulator');
const machine = new CoreEmulator(), cpu = global.window.cpu, bus = global.window.bpl;
// Use the existing low-memory RTC guard: arithmetic probes must not include
// asynchronous timer microcode that updates the interrupt-level-10 registers.
global.window.__rtcGuard = true;
const source = fs.readFileSync(process.argv[2]);
assert.equal(source.length % 26, 0);
const output = Buffer.alloc(source.length / 26 * 22);
const names = ['a','b','x','y','z','s','c','p'];
machine.reset();
let init = 0;
while (!cpu.at_boundry && init++ < 10000) cpu.step(false);
assert.ok(cpu.at_boundry);
const started = performance.now();
let totalSteps = 0;
for (let pos = 0, dest = 0; pos < source.length; pos += 26, dest += 22) {
    cpu.level = source[pos + 3]; cpu.map = 0;
    for (let r = 0; r < 8; r++) cpu.reg[names[r]] = source.readUInt16BE(pos + 4 + r * 2);
    cpu.cc = source[pos + 2];
    for (let i = 0; i < 2; i++) bus.writebyte(0x0200 + i, source[pos + i]);
    bus.writebyte(0x0202, source[pos + 20]); bus.writebyte(0x0203, source[pos + 21]);
    const address = source.readUInt16BE(pos + 22);
    bus.writebyte(address, source[pos + 24]); bus.writebyte(address + 1, source[pos + 25]);
    cpu.set_pc(0x0200);
    const arithmetic = source[pos] === 0x77 || source[pos] === 0x78;
    const shortByte = source[pos] >= 0x48 && source[pos] <= 0x4a;
    const registerByte = source[pos] >= 0x40 && source[pos] <= 0x45;
    const end = 0x0200 + (shortByte ? 1 : registerByte ? 2 : arithmetic ? (source[pos + 1] & 0x11 ? 4 : 2) :
        (source[pos + 1] & 8 ? 3 : 2)); // Indexed load/store records.
    let steps = 0;
    cpu.hspre();
    do { cpu.hsstep(); steps++; } while (!(cpu.at_boundry && cpu.pc === end) && steps < 10000);
    cpu.hsend();
    assert.ok(steps < 10000, `instruction did not return at case ${pos / 26}, pc=${cpu.pc.toString(16)}`);
    totalSteps += steps;
    for (let r = 0; r < 8; r++) output.writeUInt16BE(cpu.reg[names[r]], dest + r * 2);
    output[dest + 16] = cpu.cc; output[dest + 17] = cpu.level;
    output.writeUInt16BE(cpu.pc, dest + 18);
    output[dest + 20] = bus.readbyte(address); output[dest + 21] = bus.readbyte(address + 1);
    if (pos && pos / 26 % 65536 === 0)
        console.log('PROGRESS', pos / 26, 'cases', Math.round((performance.now() - started) / 1000), 's');
}
fs.writeFileSync(process.argv[3], output);
console.log('DONE', source.length / 26, 'cases', totalSteps, 'microsteps',
            ((performance.now() - started) / 1000).toFixed(3), 's');
process.exit(0);
