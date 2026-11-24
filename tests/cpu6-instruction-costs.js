'use strict';

// Survey one complete instruction at a time on the microcoded CPU6.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const {CoreEmulator} = require('../build/out/emulator/server/src/core/emulator');
const destination = process.argv[2];
if (!destination)
    throw new Error('Usage: node tests/cpu6-instruction-costs.js OUTPUT.tsv');

new CoreEmulator();
const {cpu, bpl, __cpu6: disassembler} = global.window;
global.window.__rtcGuard = true;
const pc = 0x0200;
const word = (value) => [value >> 8, value & 255];
const hex = (value) => value.toString(16).padStart(2, '0');
const rows = [];

function stepInstruction(limit) {
    let leftBoundary = false;
    for (let ticks = 1; ticks <= limit; ticks++) {
        cpu.hsstep();
        if (!cpu.at_boundry) leftBoundary = true;
        else if (leftBoundary) return ticks;
    }
    return null;
}

function sample(op, tail = [], condition = '', flags = 0, registers = {}) {
    // The reset microprogram stops at the alternate fetch entry. Execute one
    // instruction so measurements start at the ordinary instruction boundary.
    cpu.reset();
    cpu.hspre();
    let resetSteps = 0;
    while (!cpu.at_boundry && ++resetSteps < 1000) cpu.hsstep();
    assert.ok(cpu.at_boundry);
    cpu.map = 0;
    cpu.level = 0;
    for (let i = 0; i < 32; i++) cpu.page_ram[i] = i;
    // BIE also needs the interrupt-enable latch in each state.
    bpl.writebyte(pc - 1, op === 0x1e && flags ? 0x04 : 0x01);
    cpu.set_pc(pc - 1);
    assert.ok(stepInstruction(20));
    assert.equal(cpu.pc, pc);
    cpu.cc = flags;
    global.window.__senseSwitch = flags ? 15 : 0;
    cpu.registers[1].ah = flags ? 1 : 0; // Clock status tested by BCK.
    Object.assign(cpu.reg, {a: 0x0301, b: 0x0340, x: 0x0380, y: 0x03c0,
        z: 0x0400, s: 0x0480, c: 0x04a0, p: 0x04c0}, registers);
    // These fixtures cover ordinary RAM, stack, direct, indirect and PC-relative
    // operands. Initialize on every trial because stores can change them.
    for (let i = 0x240; i < 0x500; i++)
        bpl.writebyte(i, (i & 1) ? 0x20 : 0x03);
    for (let i = 0; i < 32; i++) bpl.writebyte(pc + i, 0);
    bpl.writebyte(pc, op);
    tail.forEach((value, i) => bpl.writebyte(pc + 1 + i, value));
    const decoded = {};
    disassembler.map = 0;
    disassembler.disassembly(pc, decoded);
    const size = decoded.l;
    // The bundled assembler and independent C emulator identify RIM at 0B.
    const instruction = op === 0x0b ? 'RIM' : (decoded.ins + (decoded.par || '')).trim();
    assert.ok(Number.isInteger(size) && size >= 1 && size <= tail.length + 1,
        'Incomplete fixture for ' + hex(op) + ': ' + instruction + ' length ' + size);
    const bytes = [op, ...tail].slice(0, size).map(hex).join(' ');
    let ticks = null, outcome = '';
    if (op === 0) outcome = 'halt';
    else if (/!ill|<invalid|<#invalid/i.test(instruction)) outcome = 'reserved in decoder';
    else {
        ticks = stepInstruction(1000000);
        if (ticks === null) outcome = 'no next boundary within limit';
        else if (op >= 0x10 && op <= 0x1f)
            outcome = (cpu.pc === pc + size) ? 'fall through' : 'taken';
        else if (op === 0x2e && cpu.pc !== pc + size)
            outcome = 'control transfer PC=' + hex(cpu.pc);
        else {
            const control = [0x09, 0x0a, 0x0b, 0x0f, 0x66].includes(op) ||
                (op >= 0x71 && op <= 0x75) || (op >= 0x79 && op <= 0x7d);
            if (!control) assert.equal(cpu.pc, pc + size, instruction);
            outcome = 'PC=' + hex(cpu.pc);
        }
    }
    cpu.hsend();
    rows.push([hex(op), bytes, instruction, condition, size, ticks ?? '', outcome]);
}

// Compact addressing modes are encoded in the primary opcode. The explicit
// index byte allows predecrement, postincrement, indirection and displacement.
function address(op) {
    switch (op & 7) {
    case 0: case 1: case 2:
        sample(op, word(0x0380));
        break;
    case 3: case 4:
        sample(op, [0x60]);
        break;
    case 5:
        for (let desc = 0; desc < 256; desc++) {
            // Bit 4 must be zero (word register). Low bits 11 are reserved.
            if (!(desc & 0x10) && (desc & 3) !== 3) sample(op, [desc, 2]);
        }
        break;
    }
}

// Extended operations take two addresses. Each can be absolute, indexed,
// or held in a register. Some operations also accept an inline source.
function extended(op, selector, head, inlineSize, allowInline) {
    const sources = [[0, ...word(0x0380)], [1, 0x40, 2], [1, 0x50, 0, 2], [2, 0x46]];
    if (allowInline) sources.push([3, ...Array(inlineSize).fill(1)]);
    const destinations = [[0, ...word(0x03c0)], [1, 0x60, 2], [1, 0x70, 0, 2], [2, 0x06]];
    for (const [src, ...a] of sources) {
        for (const [dst, ...b] of destinations) {
            // A single byte encodes both registers when both modes are 2.
            const data = src === 2 && dst === 2 ? a : [...a, ...b];
            const format = (selector << 4) | (src << 2) | dst;
            const tail = op === 0x46 ? [...head, format] : [format, ...head];
            sample(op, [...tail, ...data]);
        }
    }
}

for (let op = 0; op <= 255; op++) {
    if (op >= 0x10 && op <= 0x1f) {
        sample(op, [0x10], 'CC=0 switches=0', 0);
        sample(op, [0x10], 'CC=0xf0 switches=15', 0xf0);
    } else if ((op >= 0x20 && op <= 0x27) || (op >= 0x30 && op <= 0x37)) {
        for (let desc = 0; desc < 256; desc++) {
            // Odd operand register selects memory for word operations.
            let operand = [];
            if (op >= 0x30 && (desc & 0x10))
                operand = word((desc >> 4) === 1 ? 0x0380 : 2);
            sample(op, [desc, ...operand]);
        }
    } else if (op === 0x2e) {
        for (let sel = 0; sel < 6; sel++)
            extended(op, sel, [], 1, true);
    } else if (op === 0x2f) {
        for (let sel = 0; sel < 10; sel++) sample(op, [0x40 | sel]);
    } else if (op >= 0x40 && op <= 0x45) {
        for (let pair = 0; pair < 256; pair++) sample(op, [pair]);
    } else if (op === 0x46) {
        for (let sel = 0; sel < 10; sel++) {
            for (const n of [1, 4, 6, 8, 16]) {
                extended(op, sel, [(n - 1) * 17], n, true);
            }
        }
    } else if (op === 0x47 || op === 0x67) {
        const controls = [0, 1, 3, 2, 6, 6, 6, 6, 6, 6];
        for (const [sel, ctl] of controls.entries()) {
            const head = [];
            if (op === 0x47 && (ctl & 2)) head.push(1); // Two data bytes.
            if (op === 0x47 && (ctl & 1)) head.push(3);
            extended(op, sel, head, sel === 9 ? 1 : 2, !!(ctl & 4) && (op === 0x47 || sel === 9));
        }
    } else if ((op >= 0x50 && op <= 0x55) || [0x77, 0x78, 0xd6].includes(op)) {
        for (let pair = 0; pair < 256; pair++) {
            const mode = pair & 0x11;
            sample(op, [pair, ...word(mode === 1 ? 0x0380 : 3)]);
        }
        if (op === 0x77 || op === 0x78) {
            for (const x of [0, 1, 3, 0x7fff, 0x8000, 0xffff]) {
                for (const b of [0, 1, 0x5555, 0x8000, 0xffff])
                    sample(op, [0x42], 'X=' + x + ' B=' + b, 0, {x, b});
            }
        }
    } else if (op === 0x7e || op === 0x7f) {
        for (let first = 0; first < 16; first++)
            for (let count = 0; count < 16 - first; count++)
                sample(op, [first * 16 + count]);
    } else if ((op >= 0x60 && (op & 15) <= 5 && op !== 0x70) ||
               (op >= 0x68 && op <= 0x6d) || (op >= 0x79 && op <= 0x7d)) {
        address(op);
    } else if (op === 0xf6) {
        for (const format of [0x42, 0x52, 0x43, 0x53]) sample(op, [format, 4]);
    } else if (op === 0xf7) {
        for (const count of [1, 2, 16, 256])
            sample(op, [], 'A=' + count, 0, {a:count});
    } else {
        sample(op, [0x03, 0x80]);
    }
}

assert.equal(new Set(rows.map(row => row[0])).size, 256);
for (let op = 0x10; op <= 0x1f; op++) {
    const outcomes = rows.filter(row => row[0] === hex(op)).map(row => row[6]);
    assert.deepEqual(new Set(outcomes), new Set(['fall through', 'taken']), hex(op));
}
const header = ['opcode', 'encoding', 'disassembly', 'condition', 'bytes', 'microcycles', 'outcome'];
fs.writeFileSync(destination, [header, ...rows].map(row => row.join('\t')).join('\n') + '\n');
console.log('Surveyed 256 primary opcodes in ' + rows.length + ' trials.');
process.exit(0);
