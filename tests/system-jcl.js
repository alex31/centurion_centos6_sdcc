'use strict';

const assert = require('node:assert/strict');
const {execFileSync} = require('node:child_process');
const fs = require('node:fs');
const path = require('node:path');
const {setTimeout: delay} = require('node:timers/promises');
const {OS6Session} = require('../scripts/os6-session');

const root = path.join(__dirname, '..');
const seed = path.resolve(process.argv[2] || path.join(root, 'build/out/image/CENTOS_13.IMG'));
const out = path.join(root, 'build/out/tests');
fs.mkdirSync(out, {recursive: true});
const work = fs.mkdtempSync(path.join(out, 'jcl-'));
const executable = path.join(work, 'client.exf');
const image = path.join(work, 'client.img');
execFileSync(path.join(root, 'run/cpu6-os6-sdcc'),
    ['-O1', '--gc-sections', path.join(__dirname, 'system-jcl.c'), '-o', executable], {stdio: 'inherit'});
execFileSync('python3', [path.join(root, 'scripts/install-os6-bundle.py'), seed,
    'XJCLTEST=' + executable, '--library', 'JCLSDK', '--data-file', 'JCLSDK.REQ:48:A',
    '--data-file', 'JCLSDK.LOG:128:A', '--data-file', 'JCLSDK.TAG0:1:A', '-o', image], {stdio: 'inherit'});

async function waitUntil(condition, description) {
    const deadline = Date.now() + 30000;
    while (!condition()) {
        if (Date.now() > deadline) throw new Error('Timed out waiting for ' + description);
        await delay(20);
    }
}

async function main() {
    const session = new OS6Session(image);
    const ports = global.window.io_mux.muxports;
    const receive = ports[1];
    const transmit = ports[2];
    const queue = [];
    let workerOutput = '';
    let mainStart = 0;
    let connected = false;
    const write = receive.write_data.bind(receive);
    receive.write_data = byte => {
        workerOutput += String.fromCharCode(byte & 127);
        return write(byte);
    };
    // Emulate a one-way serial lead. The guest SDK uses native CENTOS I/O.
    // Pace the feed so the MUX receive register is drained in each slice.
    transmit.bind_dev({emu_linked: true, rts: true, can_receive: () => true,
        receive: byte => { if (connected) queue.push(byte & 127); },
        set_cts() {}, check_send() {},
        get_dev: () => transmit, bind_dev() {}});
    const wire = setInterval(() => {
        if (queue.length && receive.rts && !receive.read_busy) receive.receive(queue.shift());
    }, 90);
    try {
        await session.boot();
        await session.send('.START P1 WITH CRT1\r');
        await waitUntil(() => workerOutput.includes('\r\n'), 'the independent JCL console');
        await session.send('.USE CRT2 FOR SYS001\r');
        await delay(200);
        connected = true;
        mainStart = session.output.length;
        await session.send('.RUN XJCLTEST\r');
        await waitUntil(() => workerOutput.includes('JCL TEST WAITING'), 'the paused job');
        await delay(300);
        assert.doesNotMatch(session.output.slice(mainStart), /PASS full|FAIL |ABORT\s+\d/);
        await waitUntil(() => receive.rts && !receive.read_busy, 'the worker input');
        receive.receive(13);
        await session.waitFor(/PASS zero-capacity JCL output|FAIL [^\r\n]*\r?\n|ABORT\s+\d/, mainStart, 30000);
        const output = session.output.slice(mainStart);
        assert.doesNotMatch(output, /FAIL |ABORT\s+\d/);
        assert.match(output, /PASS full JCL output[\s\S]*PASS truncated JCL output[\s\S]*PASS zero-capacity JCL output/);
        console.log('PASS: native system_jcl waits for SYSLOG, captures .DIR and reports truncation.');
    } finally {
        fs.writeFileSync(path.join(work, 'crt0.log'), session.output);
        fs.writeFileSync(path.join(work, 'crt1.log'), workerOutput);
        clearInterval(wire);
        session.stop();
    }
}

main().then(() => process.exit(0), error => { console.error(error); process.exit(1); });
