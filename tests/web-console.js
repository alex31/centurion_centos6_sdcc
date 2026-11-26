'use strict';

const assert = require('node:assert/strict');
const {spawn} = require('node:child_process');
const {randomBytes} = require('node:crypto');
const fs = require('node:fs');
const {once} = require('node:events');
const http = require('node:http');
const net = require('node:net');
const path = require('node:path');
const {Duplex} = require('node:stream');
const {setTimeout: delay} = require('node:timers/promises');
const {openConsole} = require('./web-client');
const built = path.resolve(__dirname, '../build/out/emulator');
const {WebSocket} = require(path.join(built, 'node_modules/ws'));

const native = process.argv.includes('--native');
const sourceImage = process.argv.slice(2).find(arg => arg !== '--native');
const port = native ? Number(process.env.CENTURION_TEST_WEB_PORT || 32646) : 42646;
const host = '127.0.0.1:' + port;
const origin = 'http://' + host;
delete process.env.CENTURION_DISK_IMAGE;
let nativeServer;
let nativeServerError;

// Exercise the distribution entry point and real HTTP/WebSocket parsers using
// two linked byte streams. This also works without permission to open sockets.
let webServer;
let telnetCrt1;
const listen = net.Server.prototype.listen;
const exists = fs.existsSync;
if (!native) {
    try {
        net.Server.prototype.listen = function(port) {
            if (port === 42646) webServer = this;
            if (port === 2325) telnetCrt1 = this;
            return this;
        };
        // Test the empty drive even if the user has staged a default boot disk.
        fs.existsSync = file => !/[\\/]disks[\\/]CENTOS_13\.IMG$/.test(String(file)) && exists(file);
        require(path.join(built, 'server/src/server.js'));
    } finally {
        net.Server.prototype.listen = listen;
        fs.existsSync = exists;
    }
}
if (!native) assert.ok(webServer instanceof http.Server, 'The server must expose an HTTP console');

async function launchNative() {
    const root = path.resolve(__dirname, '..');
    const sources = path.join(root, 'swext/centurion-js');
    nativeServer = spawn(path.join(root, 'build/out/emulator-cpp/centurion-emulator'), [
        process.env.CENTURION_FIRMWARE || path.join(root, 'build/inputs/firmware.json'),
        path.join(sources, 'src/firmware-spec.json'), path.join(sources, 'web'),
    ], {
        env: {...process.env, CENTURION_BIND: '127.0.0.1', CENTURION_WEB_PORT: String(port),
            CENTURION_TELNET_BASE: process.env.CENTURION_TEST_TELNET_BASE || '33323'},
        stdio: ['ignore', 'inherit', 'inherit'],
    });
    nativeServer.on('error', error => { nativeServerError = error; });
    const deadline = Date.now() + 5000;
    for (;;) {
        if (nativeServerError) throw nativeServerError;
        if (nativeServer.exitCode !== null) throw new Error('Native emulator exited during startup');
        try {
            if ((await request('/')).status === 200) return;
        } catch (error) {
            if (error.code !== 'ECONNREFUSED' || Date.now() > deadline) throw error;
        }
        if (Date.now() > deadline) throw new Error('Native Web console did not start');
        await delay(25);
    }
}

function stream() {
    return new Duplex({
        read() {},
        write(data, encoding, callback) {
            this.peer.push(Buffer.from(data));
            callback();
        },
        final(callback) {
            this.peer.push(null);
            callback();
        },
    });
}

function connectToServer(listener) {
    const client = stream();
    const server = stream();
    client.peer = server;
    server.peer = client;
    listener.emit('connection', server);
    return client;
}

function createConnection() {
    return connectToServer(webServer);
}

function request(route, method = 'GET', body, headers = {}) {
    if (body) headers['Content-Length'] = body.length;
    return new Promise((resolve, reject) => {
        const req = http.request({
            host: '127.0.0.1', port, path: route, method, headers,
            createConnection: native ? undefined : createConnection,
        }, res => {
            const chunks = [];
            res.on('data', part => chunks.push(part));
            res.on('end', () => resolve({
                status: res.statusCode, headers: res.headers, body: Buffer.concat(chunks),
            }));
            res.on('error', reject);
        });
        req.on('error', reject);
        const timer = setTimeout(() => req.destroy(new Error('HTTP request timed out')), 5000);
        req.once('close', () => clearTimeout(timer));
        req.end(body);
    });
}

async function waitFor(check, label, seconds = 20) {
    const end = Date.now() + seconds * 1000;
    while (!check()) {
        if (Date.now() > end) throw new Error('Timed out waiting for ' + label);
        await delay(25);
    }
}

function screen(terminal) {
    return terminal ? String.fromCharCode(...terminal.cells.map(cell => cell & 127)) : '';
}

async function checkTerminalStatusBeforeConnect() {
    const crt1 = global.window.io_mux.muxports[1];
    const receive = crt1.receive;
    const received = [];
    const banner = 'Offline CRT';
    const status = [2, 80, 107, 100, 88, 64, 64, 32 + banner.length, 32, 32, 13];
    let client;
    crt1.receive = byte => received.push(byte);
    try {
        for (const ch of banner + '\x1b\x05') crt1.write_data(ch.charCodeAt(0));
        await waitFor(() => received.length >= status.length, 'a CRT status reply without any client', 2);
        assert.deepEqual(received, status);

        client = connectToServer(telnetCrt1);
        let output = '';
        client.on('data', bytes => { output += bytes.toString(); });
        await waitFor(() => output.includes(banner), 'the replayed CRT output on Telnet', 2);
        client.write('K');
        await waitFor(() => received.includes(75), 'Telnet input after replay', 2);
        assert.deepEqual(received, [...status, 75], 'Telnet replay sent another status reply');
    } finally {
        client?.peer.destroy();
        client?.destroy();
        crt1.receive = receive;
    }
}

async function checkConsoleReconnect(html, script) {
    const sockets = [];
    const page = openConsole({
        html, script, host, request,
        openSocket(url) {
            const socket = new WebSocket(url, {origin, createConnection: native ? undefined : createConnection});
            sockets.push(socket);
            return socket;
        },
    });
    const status = () => page.id('connection').textContent;
    const online = () => /^(Running|Paused)/.test(status());
    try {
        assert.equal(status(), 'Connecting');
        await waitFor(online, 'initial console state');
        const closed = once(sockets[0], 'close');
        sockets[0].terminate();
        await closed;
        assert.equal(status(), 'Reconnecting');
        assert.equal(page.actions.get('reset').disabled, true);
        await waitFor(online, 'fast console reconnect', 0.9);
        assert.equal(sockets.length, 2);
        assert.equal(page.actions.get('reset').disabled, false);
    } finally {
        page.close();
    }
}

async function checkConsolePage(html, script, image) {
    const page = openConsole({
        html, script, host, request,
        openSocket: url => new WebSocket(url, {origin, createConnection: native ? undefined : createConnection}),
    });
    const terminal = page.id('crt0');
    const status = page.id('crt0-status');
    const picker = page.id('image-file');
    const ready = () => !page.boot[0].disabled && terminal.textContent.includes('D=');
    const booted = () => status.textContent === 'CENTOS is ready' && terminal.textContent.includes('CRT0 READY');
    await waitFor(ready, 'Boot CENTOS on the empty console');
    assert.equal(page.id('save-image').disabled, true);
    page.boot[0].click();
    assert.equal(picker.clicks, 1);
    await page.chooseFile('from-console.img', image);
    try {
        await waitFor(booted, 'CENTOS boot from the file chooser', 45);
    } catch (error) {
        console.error('Boot status:', status.textContent, page.id('notice')?.textContent);
        console.error('CRT0:', terminal.textContent.match(/.{1,80}/g)?.filter(line => line.trim()).join('\n'));
        throw error;
    }
    const realtime = page.id('speed-realtime');
    const maximum = page.id('speed-maximum');
    assert.equal(maximum.checked, true);
    assert.equal(realtime.disabled, false);
    realtime.checked = true;
    await realtime.dispatch('change');
    await waitFor(() => realtime.checked && !maximum.checked &&
        page.id('connection').textContent === 'Running · Real time', 'the real-time radio option');
    assert.ok(booted(), 'changing speed reset the guest');
    maximum.checked = true;
    await maximum.dispatch('change');
    const speedStatus = /^Running · Maximum speed · \d+[.,]\d×$/;
    const connection = page.id('connection');
    await waitFor(() => maximum.checked && !realtime.checked && speedStatus.test(connection.textContent),
        'the measured maximum-speed factor');
    assert.ok(booted(), 'changing speed reset the guest');
    await page.actions.get('pause').click();
    await waitFor(() => connection.textContent === 'Paused', 'the paused status without a speed factor');
    await page.actions.get('run').click();
    await waitFor(() => speedStatus.test(connection.textContent), 'a fresh speed factor after resuming');
    for (const key of ['.', 'S', 'T', 'A', 'Enter']) page.press(0, key);
    await waitFor(() => /STATUS DISPLAY/.test(terminal.textContent) &&
        /SYSTEM DATE:\s*11\/19\/26/.test(terminal.textContent), 'the console keyboard and boot date');

    const name = 'WVT' + randomBytes(3).toString('hex').toUpperCase();
    page.paste(0, ".NEW " + name + " ON 1 'A' 1S\n");
    await waitFor(() => terminal.textContent.includes('DONE'), 'the guest disk write');
    assert.match(terminal.textContent, new RegExp('14:(?:23|24):\\d{2}\\s+\\.NEW\\s+' + name));
    const saved = await page.save();
    assert.equal(saved.status, 200);
    assert.match(saved.headers['content-disposition'], /from-console-snapshot\.img/);
    assert.equal(saved.body.length, image.length);
    assert.ok(!saved.body.equals(image), 'Save image must include writes made by CENTOS');

    page.id('load-image').click();
    assert.equal(picker.clicks, 2);
    await page.chooseFile('from-console-snapshot.img', saved.body);
    await waitFor(() => ready() && page.id('disk-list').textContent.includes('from-console-snapshot.img'),
        'the reloaded disk at the ROM prompt');
    page.boot[0].click();
    await waitFor(booted, 'CENTOS boot from the reloaded disk', 45);
    page.paste(0, '.DIR 1 (' + name + ')\n');
    const entry = new RegExp(name + '\\s+A\\s+0\\s+1\\s+1S');
    await waitFor(() => entry.test(terminal.textContent), 'the saved disk entry in the CENTOS directory');
    assert.match(terminal.textContent, /14:(?:23|24):\d{2}\s+\.DIR/);
    console.log('PASS: the console page boots, saves guest writes and reloads the saved disk.');

    page.paste(0, 'BASIC\n');
    await waitFor(() => terminal.textContent.includes('> '), 'the BASIC prompt');
    page.paste(0, '20 S=0\n30 PRINT "EDITRESULT";S\n40 END\n.EDIT 20\n');
    await waitFor(() => terminal.textContent.includes('Ctrl-D cancels'), 'the BASIC line editor');
    for (const key of ['ArrowRight', 'ArrowRight', 'ArrowRight', 'Backspace', '5', 'Enter'])
        page.press(0, key);
    page.paste(0, '.RUN\n');
    await waitFor(() => /EDITRESULT\s*5(?:\.0+)?\s/.test(terminal.textContent),
        'the program result after a VT100 cursor edit');
    console.log('PASS: BASIC .EDIT accepts application cursor keys through the Web console.');
    return page;
}

async function main() {
    if (native)
        await launchNative();
    else
        await checkTerminalStatusBeforeConnect();
    const page = await request('/');
    assert.equal(page.status, 200);
    assert.match(page.body.toString(), /CRT0[\s\S]*CRT1[\s\S]*Stop server/);
    assert.match(page.headers['content-security-policy'], /frame-ancestors 'none'/);
    const script = await request('/console.js');
    assert.equal(script.status, 200);
    await checkConsoleReconnect(page.body.toString(), script.body.toString());
    assert.equal((await request('/console.css')).status, 200);
    assert.equal((await request('/api/control?action=shutdown', 'POST', null,
        {Origin: 'http://other.example'})).status, 403);
    assert.equal((await request('/', 'GET', null, {Host: 'other.example'})).status, 403);
    await new Promise((resolve, reject) => {
        const crossOrigin = new WebSocket('ws://' + host + '/console', {
            origin: 'http://other.example', createConnection: native ? undefined : createConnection,
        });
        crossOrigin.on('unexpected-response', (_, res) => {
            res.resume();
            if (res.statusCode === 403) resolve();
            else reject(new Error('Cross-origin WebSocket should be forbidden'));
        });
        crossOrigin.on('error', reject);
        crossOrigin.on('open', () => reject(new Error('Cross-origin WebSocket opened')));
    });

    let state, stateTime;
    let messageError;
    const ws = new WebSocket('ws://' + host + '/console', {
        origin, createConnection: native ? undefined : createConnection,
    });
    ws.on('error', error => { messageError = error.message; });
    ws.on('message', data => {
        const message = JSON.parse(data);
        if (message.type === 'state') {
            state = message;
            stateTime = performance.now();
        }
        if (message.type === 'error') messageError = message.message;
    });
    const control = action => request('/api/control?action=' + action, 'POST', null, {Origin: origin});
    await waitFor(() => state || messageError, 'the initial CPU and terminal state');
    assert.ifError(messageError);
    assert.equal(state.terminals.length, 2);
    assert.equal(state.registers.length, 8);
    await waitFor(() => state.terminals[0]?.bootReady, 'the first ROM prompt');
    assert.match(screen(state.terminals[0]), /D=/);

    // BASIC .EDIT and .LIST request VT100 application cursor keys.
    if (!native) {
        const crt0 = global.window.io_mux.muxports[0];
        const receive = crt0.receive;
        const received = [];
        crt0.receive = byte => received.push(byte);
        try {
            for (const [mode, key, expected] of [
                ['', '\x1b[D', [21]],
                ['\x1b[?1h', '\x1b[D', [27, 79, 68]],
                ['', '\x1b[C', [27, 79, 67]],
                ['', '\x1b[A', [27, 79, 65]],
                ['', '\x1b[B', [27, 79, 66]],
                ['', '\x1b[5~', [27, 91, 53, 126]],
                ['', '\x1b[6~', [27, 91, 54, 126]],
                ['\x1b[?1l', '\x1b[C', [6]],
            ]) {
                for (const ch of mode) crt0.write_data(ch.charCodeAt(0));
                received.length = 0;
                ws.send(JSON.stringify({type: 'key', port: 0, key}));
                await waitFor(() => received.length >= expected.length, 'the cursor key bytes', 2);
                assert.deepEqual(received, expected);
            }
        } finally {
            crt0.receive = receive;
        }
    }

    assert.equal(state.canSave, false);
    ws.send(JSON.stringify({type: 'boot', port: 0, date: '111926', time: '140000'}));
    await waitFor(() => messageError, 'a missing image diagnostic');
    assert.equal(messageError, 'Load a CENTOS disk image first');
    messageError = undefined;
    ws.send(JSON.stringify({type: 'paste', port: 0, text: 'H1'}));
    await waitFor(() => /D=H1[\s\S]*ERROR/.test(screen(state.terminals[0])) && state.terminals[0].bootReady,
        'the ROM error and return to D=');

    // CRT1 is ordinarily idle at boot. Exercise its independent MUX path.
    if (!native) {
        const crt1 = global.window.io_mux.muxports[1];
        for (const ch of 'CRT1 output') crt1.write_data(ch.charCodeAt(0));
        await waitFor(() => screen(state.terminals[1]).includes('CRT1 output'), 'CRT1 display');
        assert.doesNotMatch(screen(state.terminals[0]), /CRT1 output/);
    }

    const hasMeasuredSpeed = () => Number.isFinite(state.speedFactor) && state.speedFactor > 0;
    await waitFor(hasMeasuredSpeed, 'a maximum-speed WebSocket sample');
    assert.equal((await control('speed&mode=realtime')).status, 200);
    await waitFor(() => state.speed === 'realtime' && state.speedFactor === null,
        'the realtime transition to clear the maximum-speed measurement');
    if (native) {
        const start = stateTime, initialCycles = state.microsteps;
        await waitFor(() => stateTime - start >= 1400, 'a running 5 MHz sample');
        const mhz = (state.microsteps - initialCycles) / ((stateTime - start) * 1000);
        assert.ok(mhz > 4.5 && mhz < 5.5, 'Unexpected live clock: ' + mhz + ' MHz');
        console.log('PASS: live Web server real-time clock is ' + mhz.toFixed(2) + ' MHz.');
    }
    assert.equal((await control('speed&mode=maximum')).status, 200);
    await waitFor(() => state.speed === 'maximum' && state.speedFactor === null,
        'the maximum-speed transition to start an empty sample');
    await waitFor(hasMeasuredSpeed, 'a new maximum-speed sample');
    assert.equal((await control('step')).status, 409);
    assert.equal((await control('pause')).status, 200);
    await waitFor(() => !state.running && state.speedFactor === null, 'CPU pause to clear the speed factor');
    assert.equal((await control('run')).status, 200);
    await waitFor(() => state.running && state.speedFactor === null, 'CPU resume to start an empty sample');
    await waitFor(hasMeasuredSpeed, 'an updated factor after CPU resume');
    assert.equal((await control('pause')).status, 200);
    await waitFor(() => !state.running, 'CPU pause for the single-step check');
    const pausedPC = state.pc;
    assert.equal((await control('speed&mode=invalid')).status, 400);
    assert.equal((await control('speed&mode=realtime')).status, 200);
    await waitFor(() => state.speed === 'realtime', 'the real-time speed while paused');
    assert.equal(state.running, false);
    assert.equal(state.pc, pausedPC);
    assert.equal((await control('step')).status, 200);
    assert.equal((await control('run')).status, 200);
    await waitFor(() => state.running, 'CPU resume');
    assert.equal(state.speed, 'realtime');
    assert.equal((await control('speed&mode=maximum')).status, 200);
    await waitFor(() => state.speed === 'maximum' && state.running, 'maximum speed while running');

    const image = sourceImage ? fs.readFileSync(sourceImage) : Buffer.alloc(6651904, 0x42);
    const consolePage = sourceImage ?
        await checkConsolePage(page.body.toString(), script.body.toString(), image) : null;
    const compact = Buffer.alloc(5196800, 0xa5);
    assert.equal((await request('/api/disk?name=compact.img', 'POST', compact, {Origin: origin})).status, 200);
    await waitFor(() => state.disks.some(unit => unit.filename === 'compact.img' && unit.stride === 400),
        'a compact Hawk image');
    assert.ok((await request('/api/disk')).body.equals(compact));
    assert.equal((await request('/api/disk?name=test.img', 'POST', image, {Origin: origin})).status, 200);
    await waitFor(() => state.canSave && state.disks.some(unit => unit.filename === 'test.img'),
        'the uploaded disk');
    const download = await request('/api/disk');
    assert.equal(download.status, 200);
    assert.match(download.headers['content-disposition'], /test-snapshot\.img/);
    assert.ok(download.body.equals(image), 'The image download must match the upload');
    assert.equal((await request('/api/disk', 'POST', Buffer.alloc(10), {Origin: origin})).status, 400);
    assert.ok((await request('/api/disk')).body.equals(image), 'Invalid upload replaced the disk');

    if (native) {
        assert.equal((await control('pause')).status, 200);
        await waitFor(() => !state.running, 'a stopped CPU for the binary debugger');
        const legacy = new WebSocket('ws://' + host + '/');
        try {
            await once(legacy, 'open');
            async function requestLegacy(bytes) {
                const pending = once(legacy, 'message');
                legacy.send(Buffer.from(bytes));
                const [reply, binary] = await pending;
                assert.equal(binary, true);
                return reply;
            }
            const hello = await requestLegacy([0x20, 0]);
            assert.equal(hello.length, 24);
            assert.equal(hello.subarray(4, 10).toString(), 'WebCen');
            assert.deepEqual(await requestLegacy([0x40, 0]), Buffer.from([0x48, 0, 0, 0, 2, 0, 3, 0]));
            assert.deepEqual(await requestLegacy([0, 0x18]), Buffer.from([8, 0x18]));
            assert.deepEqual(await requestLegacy([0, 0x20]), Buffer.from([0, 0x20]));
            const reply = await requestLegacy([10, 0]);
            assert.equal(reply.readUInt16LE(), 0x04a0);
            const fields = [[0, 1], [1, 1], [2, 2], [3, 3], [6, 16], [8, 4], [10, 8]];
            let at = 2;
            for (const [tag, width] of fields) {
                assert.equal(reply[at], tag);
                at += 1 + width;
            }
            for (let level = 0; level < 16; ++level) {
                assert.deepEqual([...reply.subarray(at, at + 2)], [12, level * 16]);
                if (level === state.level) {
                    const registers = Array.from({length: 8}, (_, i) => reply.readUInt16BE(at + 2 + i * 2));
                    assert.deepEqual(registers, state.registers);
                }
                at += 20; // The historical decoder reads work address after each register row.
            }
            const before = state.microsteps;
            assert.equal((await requestLegacy([11, 0])).readUInt16LE(), 0x04a0);
            const previewed = stateTime;
            await waitFor(() => stateTime > previewed, 'a browser snapshot after the binary preview');
            assert.equal(state.microsteps, before);
            assert.equal((await requestLegacy([13, 0, 7])).readUInt16LE(), 0x04a0);
            await waitFor(() => state.microsteps === before + 7, 'seven binary debugger microsteps');
            console.log('PASS: binary debugger reports registers and executes the requested microsteps.');
        } finally {
            legacy.close();
        }
    } else await new Promise((resolve, reject) => {
        const legacy = new WebSocket('ws://' + host + '/', {createConnection: native ? undefined : createConnection});
        legacy.on('open', () => legacy.send(Buffer.from([10, 0])));
        legacy.on('error', reject);
        legacy.once('message', data => {
            try {
                assert.equal(data.readUInt16LE(0), 0x04a0);
                legacy.close();
                resolve();
            } catch (error) { reject(error); }
        });
    });

    const exit = process.exit;
    let shutdownCode;
    try {
        process.exit = code => { shutdownCode = code; };
        if (consolePage) {
            await consolePage.actions.get('shutdown').click();
            assert.equal(consolePage.id('connection').textContent, 'Server stopped');
        } else {
            assert.equal((await control('shutdown')).body.toString(), 'Server stopped');
        }
        await waitFor(() => (native ? nativeServer.exitCode : shutdownCode) === 0, 'server shutdown');
    } finally {
        process.exit = exit;
        consolePage?.close();
        ws.close();
    }
    console.log('PASS: web page, CPU controls and measured speed, both CRTs, disk round trip and server shutdown.');
}

main().then(() => process.exit(0), error => {
    console.error(error);
    nativeServer?.kill();
    process.exit(1);
});
