'use strict';

const assert = require('node:assert/strict');
const fs = require('node:fs');
const http = require('node:http');
const net = require('node:net');
const path = require('node:path');
const {Duplex} = require('node:stream');
const {setTimeout: delay} = require('node:timers/promises');
const built = path.resolve(__dirname, '../build/out/emulator');
const {WebSocket} = require(path.join(built, 'node_modules/ws'));

const host = '127.0.0.1:42646';
const origin = 'http://' + host;
delete process.env.CENTURION_DISK_IMAGE;

// Exercise the distribution entry point and real HTTP/WebSocket parsers using
// two linked byte streams. This also works without permission to open sockets.
let webServer;
const listen = net.Server.prototype.listen;
try {
    net.Server.prototype.listen = function(port) {
        if (port === 42646) webServer = this;
        return this;
    };
    require(path.join(built, 'server/src/server.js'));
} finally {
    net.Server.prototype.listen = listen;
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

async function main() {
    const page = await request('/');
    assert.equal(page.status, 200);
    assert.match(page.body.toString(), /CRT0[\s\S]*CRT1[\s\S]*Stop server/);
    assert.match(page.headers['content-security-policy'], /frame-ancestors 'none'/);
    assert.equal((await request('/console.js')).status, 200);
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

    // CRT1 is ordinarily idle at boot. Exercise its independent MUX path.
    const crt1 = global.window.io_mux.muxports[1];
    for (const ch of 'CRT1 output') crt1.write_data(ch.charCodeAt(0));
    await waitFor(() => screen(state.terminals[1]).includes('CRT1 output'), 'CRT1 display');
    assert.doesNotMatch(screen(state.terminals[0]), /CRT1 output/);

    assert.equal((await control('step')).status, 409);
    assert.equal((await control('pause')).status, 200);
    await waitFor(() => !state.running, 'CPU pause');
    assert.equal((await control('step')).status, 200);
    assert.equal((await control('run')).status, 200);
    await waitFor(() => state.running, 'CPU resume');

    const compact = Buffer.alloc(5196800, 0xa5);
    assert.equal((await request('/api/disk?name=compact.img', 'POST', compact, {Origin: origin})).status, 200);
    await waitFor(() => state.disks.some(unit => unit.filename === 'compact.img' && unit.stride === 400),
        'a compact Hawk image');
    assert.ok((await request('/api/disk')).body.equals(compact));
    const image = process.argv[2] ? fs.readFileSync(process.argv[2]) : Buffer.alloc(6651904, 0x42);
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

    if (process.argv[2]) {
        await waitFor(() => state.terminals[0]?.bootReady, 'the ROM prompt after mounting the disk');
        ws.send(JSON.stringify({type: 'boot', port: 0, date: '111926', time: '140000'}));
        await waitFor(() => state.terminals[0].bootStatus === 'CENTOS is ready' || messageError, 'CENTOS boot');
        assert.ifError(messageError);
        assert.match(screen(state.terminals[0]), /CRT0 READY/);
        ws.send(JSON.stringify({type: 'paste', port: 0, text: '.STA\n'}));
        await waitFor(() => {
            const text = screen(state.terminals[0]);
            return /STATUS DISPLAY/.test(text) && /SYSTEM DATE:\s*11\/19\/26/.test(text);
        }, 'the keyboard command and date set by the boot shortcut');
        console.log('PASS: the browser terminal booted CENTOS and accepted a command.');
    }

    const exit = process.exit;
    let shutdownCode;
    try {
        process.exit = code => { shutdownCode = code; };
        assert.equal((await control('shutdown')).body.toString(), 'Server stopped');
        await waitFor(() => shutdownCode === 0, 'server shutdown');
    } finally {
        process.exit = exit;
        ws.close();
    }
    console.log('PASS: web page, CPU controls, both CRTs, disk round trip and server shutdown.');
}

main().then(() => process.exit(0), error => {
    console.error(error);
    process.exit(1);
});
