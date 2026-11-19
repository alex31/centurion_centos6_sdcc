'use strict';

// Headless OS6 interaction. The supplied disk is read into an independent
// in-memory buffer; saving it is always an explicit operation to a new file.
const fs = require('node:fs');
const path = require('node:path');
const root = path.resolve(__dirname, '..');
const core = path.join(root, 'build/out/emulator/server/src/core');
const {CoreEmulator} = require(path.join(core, 'emulator.js'));
const bridge = require(path.join(core, 'cpu-real.js'));
const sleep = ms => new Promise(resolve => setTimeout(resolve, ms));

class OS6Session {
    constructor(diskPath) {
        const bytes = fs.readFileSync(diskPath);
        if (bytes.length !== 6651904) throw new Error('Expected a 6 651 904 byte Hawk image');
        this.machine = new CoreEmulator();
        if (!bridge.isRealCPULoaded()) throw new Error('CPU core is a stub');
        this.port = global.window.io_mux.muxports[0];
        this.disk = global.window.io_dsk2.units[1];
        const data = Uint8Array.from(bytes);
        this.disk.image = {type: 'hawk', filename: path.basename(diskPath), stride: 512,
                           backing_data: data.buffer, protect: false, data};
        this.output = '';
        const originalWrite = this.port.write_data.bind(this.port);
        this.port.write_data = value => {
            this.output += String.fromCharCode(value & 0x7f);
            if (process.env.OS6_TRACE_CONSOLE === '1')
                process.stdout.write(String.fromCharCode(value & 0x7f));
            return originalWrite(value);
        };
    }

    async waitFor(marker, from = 0, timeoutMs = 45000) {
        const deadline = Date.now() + timeoutMs;
        while (!this.output.slice(from).includes(marker) && Date.now() < deadline) await sleep(25);
        if (!this.output.slice(from).includes(marker))
            throw new Error(`Timeout waiting for ${JSON.stringify(marker)}; output:\n${this.output.slice(from)}`);
    }

    async send(text) {
        for (const ch of text) {
            const deadline = Date.now() + 5000;
            while ((this.port.read_busy || (this.port.rts === false && ch !== '\x03')) && Date.now() < deadline) await sleep(10);
            if (this.port.read_busy || (this.port.rts === false && ch !== '\x03')) throw new Error('Terminal input remained busy');
            this.port.receive(ch.charCodeAt(0));
            await sleep(50);
        }
    }

    async boot() {
        this.machine.reset();
        this.machine.start();
        await this.waitFor('D=');
        await this.send('H1');
        await this.waitFor('SYSTEM DISK (S)= 1');
        await this.send('\r');
        await this.waitFor('ENTER NEW SYSTEM DATE');
        await this.send('102826\r');
        await this.waitFor('ENTER SYSTEM TIME');
        await this.send('120000\r');
        await this.waitFor('CRT0 READY');
    }

    async command(command, completion = 'CRT0 READY') {
        const from = this.output.length;
        await this.send(command + '\r');
        await this.waitFor(completion, from);
        await sleep(200);
        return this.output.slice(from);
    }

    saveNew(pathname) {
        fs.writeFileSync(pathname, Buffer.from(this.disk.image.data), {flag: 'wx'});
    }

    stop() { this.machine.stop(); }
}

module.exports = {OS6Session};
