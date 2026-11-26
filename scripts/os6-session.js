'use strict';

// Headless OS6 interaction. The supplied disk is read into an independent
// in-memory buffer; saving it is always an explicit operation to a new file.
const fs = require('node:fs');
const path = require('node:path');
const root = path.resolve(__dirname, '..');
const core = path.join(root, 'build/out/emulator/server/src/core');
const sleep = ms => new Promise(resolve => setTimeout(resolve, ms));
const nextTurn = () => new Promise(resolve => setImmediate(resolve));

class Session {
    constructor() {
        this.transcript = '';
    }

    get output() {
        if (this.failure) throw this.failure;
        return this.transcript;
    }

    record(text, cycles) {
        if (!this.onLine) {
            this.transcript += text;
        } else {
            let start = 0;
            for (const cycle of cycles) {
                const end = text.indexOf('\n', start) + 1;
                this.transcript += text.slice(start, end);
                const line = this.transcript.lastIndexOf('\n', this.transcript.length - 2) + 1;
                this.onLine(this.transcript.slice(line), cycle);
                start = end;
            }
            this.transcript += text.slice(start);
        }
        if (process.env.OS6_TRACE_CONSOLE === '1') process.stdout.write(text);
    }

    async waitFor(marker, from = 0, timeoutMs = 45000) {
        const matches = () => {
            const output = this.output.slice(from);
            return typeof marker === 'string' ? output.includes(marker) : output.search(marker) !== -1;
        };
        const deadline = Date.now() + timeoutMs;
        while (!matches() && Date.now() < deadline) await this.waitTurn();
        if (!matches()) {
            const description = typeof marker === 'string' ? JSON.stringify(marker) : String(marker);
            throw new Error(`Timeout waiting for ${description}; output:\n${this.output.slice(from)}`);
        }
    }

    async boot() {
        const from = this.output.length;
        this.start();
        await this.waitFor('D=', from);
        await this.send('H1');
        await this.waitFor('SYSTEM DISK (S)= 1', from);
        await this.send('\r');
        await this.waitFor('ENTER NEW SYSTEM DATE', from);
        await this.send('102826\r');
        await this.waitFor('ENTER SYSTEM TIME', from);
        await this.send('120000\r');
        await this.waitFor('CRT0 READY', from);
    }

    async command(command, completion = 'CRT0 READY') {
        const from = this.output.length;
        await this.send(command + '\r');
        await this.waitFor(completion, from);
        await this.settle();
        return this.output.slice(from);
    }
}

class JavaScriptSession extends Session {
    constructor(diskPath) {
        super();
        const imagePath = path.resolve(diskPath);
        if (fs.statSync(imagePath).size !== 6651904) throw new Error('Expected a 6 651 904 byte Hawk image');
        const {CoreEmulator} = require(path.join(core, 'emulator.js'));
        const bridge = require(path.join(core, 'cpu-real.js'));
        const previousImage = process.env.CENTURION_DISK_IMAGE;
        process.env.CENTURION_DISK_IMAGE = imagePath;
        try {
            this.machine = new CoreEmulator();
        } finally {
            if (previousImage === undefined) delete process.env.CENTURION_DISK_IMAGE;
            else process.env.CENTURION_DISK_IMAGE = previousImage;
        }
        if (!bridge.isRealCPULoaded()) throw new Error('CPU core is a stub');
        this.port = global.window.io_mux.muxports[0];
        this.disk = global.window.io_dsk2.units[1];
        this.cellOffset = 0;
        const originalWrite = this.port.write_data.bind(this.port);
        this.port.write_data = value => {
            const byte = value & 127;
            this.record(String.fromCharCode(byte), byte === 10 ? [global.window.cpu.executedMicrosteps] : []);
            return originalWrite(value);
        };
    }

    start() {
        this.machine.reset();
        this.machine.start();
    }

    waitTurn() { return sleep(25); }
    settle() { return sleep(200); }

    async waitInputReady(ch) {
        const ready = () => !this.port.read_busy &&
            (ch === undefined || ch === '\x03' || this.port.rts !== false);
        const deadline = Date.now() + 5000;
        while (!ready() && Date.now() < deadline) await sleep(10);
        if (!ready()) throw new Error('Terminal input remained busy');
    }

    async send(text) {
        for (const ch of text) {
            await this.waitInputReady(ch);
            this.port.receive(ch.charCodeAt(0));
        }
        if (text.length) await this.waitInputReady();
    }

    cell(x, y) {
        if (!this.crt) {
            const {CenturionCRT} = require(path.join(root, 'build/out/emulator/server/src/terminal/crt.js'));
            this.crt = new CenturionCRT();
        }
        while (this.cellOffset < this.output.length)
            this.crt.receive(this.output.charCodeAt(this.cellOffset++));
        return this.crt.getCell(x, y);
    }

    saveNew(pathname) {
        fs.writeFileSync(pathname, Buffer.from(this.disk.image.data), {flag: 'wx'});
    }

    stop() { this.machine.stop(); }
}

class NativeSession extends Session {
    constructor(diskPath) {
        super();
        this.binding = require(path.join(root, 'build/out/emulator-cpp/centurion-headless.node'));
        this.handle = this.binding.open(
            process.env.CENTURION_FIRMWARE || path.join(root, 'build/inputs/firmware.json'),
            path.join(root, 'swext/centurion-js/src/firmware-spec.json'), path.resolve(diskPath));
    }

    advance(slices) {
        const [text, lineCycles] = this.binding.advance(this.handle, slices);
        this.record(text, lineCycles);
    }

    start() {
        clearImmediate(this.scheduled);
        this.binding.reset(this.handle);
        const pump = () => {
            try {
                this.advance(10);
                this.scheduled = setImmediate(pump);
            } catch (error) {
                this.failure = error;
            }
        };
        this.scheduled = setImmediate(pump);
    }

    waitTurn() { return nextTurn(); }
    settle() { this.advance(20); }

    async send(text) {
        if (this.failure) throw this.failure;
        const sendByte = byte => {
            const deadline = Date.now() + 5000;
            while (!this.binding.input(this.handle, byte)) {
                if (Date.now() >= deadline) throw new Error('Terminal input remained busy');
                this.advance(1);
            }
        };
        for (const ch of text) sendByte(ch.charCodeAt(0) & 255);
        if (text.length) sendByte(256); // Wait until the guest has consumed the last byte.
    }

    cell(x, y) { return this.binding.cell(this.handle, x, y); }
    saveNew(pathname) { this.binding.save(this.handle, pathname); }

    stop() {
        clearImmediate(this.scheduled);
        this.handle = null;
    }
}

const backend = process.env.CENTURION_OS6_BACKEND || 'javascript';
if (backend !== 'cpp' && backend !== 'javascript')
    throw new Error('CENTURION_OS6_BACKEND must be cpp or javascript');
const OS6Session = backend === 'cpp' ? NativeSession : JavaScriptSession;
module.exports = {OS6Session, JavaScriptSession};
