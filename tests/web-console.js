'use strict';

const assert = require('node:assert/strict');
const {randomBytes} = require('node:crypto');
const fs = require('node:fs');
const http = require('node:http');
const net = require('node:net');
const path = require('node:path');
const {Duplex} = require('node:stream');
const {setTimeout: delay} = require('node:timers/promises');
const {openConsole} = require('./web-client');
const built = path.resolve(__dirname, '../build/out/emulator');
const {WebSocket} = require(path.join(built, 'node_modules/ws'));

const host = '127.0.0.1:42646';
const origin = 'http://' + host;
delete process.env.CENTURION_DISK_IMAGE;

// Exercise the distribution entry point and real HTTP/WebSocket parsers using
// two linked byte streams. This also works without permission to open sockets.
let webServer;
const listen = net.Server.prototype.listen;
const exists = fs.existsSync;
try {
    net.Server.prototype.listen = function(port) {
        if (port === 42646) webServer = this;
        return this;
    };
    // Test the empty drive even if the user has staged a default boot disk.
    fs.existsSync = file => !/[\\/]disks[\\/]CENTOS_13\.IMG$/.test(String(file)) && exists(file);
    require(path.join(built, 'server/src/server.js'));
} finally {
    net.Server.prototype.listen = listen;
    fs.existsSync = exists;
}
assert.ok(webServer instanceof http.Server, 'The server must expose an HTTP console');

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

function createConnection() {
    const client = stream();
    const server = stream();
    client.peer = server;
    server.peer = client;
    webServer.emit('connection', server);
    return client;
}

function request(route, method = 'GET', body, headers = {}) {
    if (body) headers['Content-Length'] = body.length;
    return new Promise((resolve, reject) => {
        const req = http.request({
            host: '127.0.0.1', port: 42646, path: route, method, headers,
            createConnection,
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

async function checkConsolePage(html, script, image) {
    const page = openConsole({
        html, script, host, request,
        openSocket: url => new WebSocket(url, {origin, createConnection}),
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
    await waitFor(booted, 'CENTOS boot from the file chooser', 45);
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
    await waitFor(() => maximum.checked && !realtime.checked &&
        page.id('connection').textContent === 'Running · Maximum speed', 'the maximum-speed radio option');
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
    return page;
}

async function main() {
    const page = await request('/');
    assert.equal(page.status, 200);
    assert.match(page.body.toString(), /CRT0[\s\S]*CRT1[\s\S]*Stop server/);
    assert.match(page.headers['content-security-policy'], /frame-ancestors 'none'/);
    const script = await request('/console.js');
    assert.equal(script.status, 200);
    assert.equal((await request('/console.css')).status, 200);
    assert.equal((await request('/api/control?action=shutdown', 'POST', null,
        {Origin: 'http://other.example'})).status, 403);
    assert.equal((await request('/', 'GET', null, {Host: 'other.example'})).status, 403);
    await new Promise((resolve, reject) => {
        const crossOrigin = new WebSocket('ws://' + host + '/console', {
            origin: 'http://other.example', createConnection,
        });
        crossOrigin.on('unexpected-response', (_, res) => {
            res.resume();
            if (res.statusCode === 403) resolve();
            else reject(new Error('Cross-origin WebSocket should be forbidden'));
        });
        crossOrigin.on('error', reject);
        crossOrigin.on('open', () => reject(new Error('Cross-origin WebSocket opened')));
    });

    let state;
    let messageError;
    const ws = new WebSocket('ws://' + host + '/console', {origin, createConnection});
    ws.on('error', error => { messageError = error.message; });
    ws.on('message', data => {
        const message = JSON.parse(data);
        if (message.type === 'state') state = message;
        if (message.type === 'error') messageError = message.message;
    });
    const control = action => request('/api/control?action=' + action, 'POST', null, {Origin: origin});
    await waitFor(() => state || messageError, 'the initial CPU and terminal state');
    assert.ifError(messageError);
    assert.equal(state.terminals.length, 2);
    assert.equal(state.registers.length, 8);
    await waitFor(() => state.terminals[0]?.bootReady, 'the first ROM prompt');
    assert.match(screen(state.terminals[0]), /D=/);

    // BASIC .EDIT requests VT100 application cursor keys.
    const crt0 = global.window.io_mux.muxports[0];
    const receive = crt0.receive;
    const received = [];
    crt0.receive = byte => received.push(byte);
    try {
        for (const [mode, key, expected] of [
            ['', '\x1b[D', [21]],
            ['\x1b[?1h', '\x1b[D', [27, 79, 68]],
            ['', '\x1b[C', [27, 79, 67]],
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

    assert.equal(state.canSave, false);
    ws.send(JSON.stringify({type: 'boot', port: 0, date: '111926', time: '140000'}));
    await waitFor(() => messageError, 'a missing image diagnostic');
    assert.equal(messageError, 'Load a CENTOS disk image first');
    messageError = undefined;
    ws.send(JSON.stringify({type: 'paste', port: 0, text: 'H1'}));
    await waitFor(() => /D=H1[\s\S]*ERROR/.test(screen(state.terminals[0])) && state.terminals[0].bootReady,
        'the ROM error and return to D=');

    // CRT1 is ordinarily idle at boot. Exercise its independent MUX path.
    const crt1 = global.window.io_mux.muxports[1];
    for (const ch of 'CRT1 output') crt1.write_data(ch.charCodeAt(0));
    await waitFor(() => screen(state.terminals[1]).includes('CRT1 output'), 'CRT1 display');
    assert.doesNotMatch(screen(state.terminals[0]), /CRT1 output/);

    assert.equal((await control('step')).status, 409);
    assert.equal((await control('pause')).status, 200);
    await waitFor(() => !state.running, 'CPU pause');
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

    const image = process.argv[2] ? fs.readFileSync(process.argv[2]) : Buffer.alloc(6651904, 0x42);
    const consolePage = process.argv[2] ?
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

    await new Promise((resolve, reject) => {
        const legacy = new WebSocket('ws://' + host + '/', {createConnection});
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
        await waitFor(() => shutdownCode === 0, 'server shutdown');
    } finally {
        process.exit = exit;
        consolePage?.close();
        ws.close();
    }
    console.log('PASS: web page, CPU controls and speed selection, both CRTs, disk round trip and server shutdown.');
}

main().then(() => process.exit(0), error => {
    console.error(error);
    process.exit(1);
});
