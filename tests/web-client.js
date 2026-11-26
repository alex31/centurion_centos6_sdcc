'use strict';

const vm = require('node:vm');

// Execute the shipped client with the DOM and transport operations it uses.
// The requests and WebSocket messages still go through the real server parsers.
class Element {
    constructor() {
        this.children = [];
        this.dataset = {};
        this.events = new Map();
        this.classList = {add() {}, toggle() {}};
        this.text = '';
        this.clicks = 0;
    }
    get textContent() { return this.text + this.children.map(child => child.textContent).join(''); }
    set textContent(text) { this.text = text; this.children = []; }
    append(...children) { this.children.push(...children); }
    replaceChildren(...children) { this.children = children; this.text = ''; }
    addEventListener(event, listener) { this.events.set(event, listener); }
    dispatch(event, data) { return this.events.get(event)?.(data); }
    click() {
        if (this.disabled) throw new Error('Clicked a disabled console control');
        this.clicks++;
        return this.dispatch('click');
    }
    focus() {}
}

function openConsole({html, script, host, request, openSocket}) {
    const ids = new Map(), actions = new Map(), boot = [];
    for (const [tag] of html.matchAll(/<(?:button|input|div|output|dd|ul)\b[^>]*>/g)) {
        const element = new Element();
        const id = /\bid="([^"]+)"/.exec(tag);
        const action = /\bdata-action="([^"]+)"/.exec(tag);
        const port = /\bdata-port="([01])"/.exec(tag);
        const classes = /\bclass="([^"]+)"/.exec(tag)?.[1].split(/\s+/) ?? [];
        if (id) ids.set(id[1], element);
        if (action) {
            element.dataset.action = action[1];
            actions.set(action[1], element);
        }
        if (port && classes.includes('boot')) boot[Number(port[1])] = element;
        element.disabled = /\sdisabled[\s>]/.test(tag);
    }
    const id = name => ids.get(name);
    let download, client;
    class ClientSocket {
        static OPEN = 1;
        constructor(url) {
            this.socket = openSocket(url);
            this.socket.on('message', data => this.onmessage?.({data: data.toString()}));
            this.socket.on('close', () => this.onclose?.());
            client = this;
        }
        get readyState() { return this.socket.readyState; }
        send(data) { this.socket.send(data); }
    }
    class LocalDate extends Date {
        constructor() { super(2026, 10, 19, 14, 23, 45); }
    }
    const createElement = () => new Element();
    const document = Object.assign(new Element(), {
        getElementById: id,
        querySelectorAll: selector => selector === '[data-action]' ? [...actions.values()] : [],
        querySelector: selector => boot[Number(selector.match(/^\.boot\[data-port="([01])"\]$/)?.[1])],
        createDocumentFragment: createElement,
        createElement,
        createTextNode: text => Object.assign(new Element(), {text}),
    });
    const window = Object.assign(new Element(), {confirm: () => true});
    vm.runInNewContext(script, {
        document,
        location: {host, assign: route => { download = request(route); }},
        window,
        WebSocket: ClientSocket,
        Date: LocalDate, URLSearchParams, setTimeout, clearTimeout,
        fetch: async (route, {method, body}) => {
            const result = await request(route, method, body?.data, {Origin: 'http://' + host});
            return {ok: result.status >= 200 && result.status < 300, text: async () => result.body.toString()};
        },
    }, {filename: 'console.js'});
    return {
        id, boot, actions,
        async chooseFile(name, data) {
            const picker = id('image-file');
            picker.files = [{name, size: data.length, data}];
            await picker.dispatch('change');
        },
        paste(port, text) {
            return id('crt' + port).dispatch('paste', {
                preventDefault() {}, clipboardData: {getData: () => text},
            });
        },
        press(port, key) {
            return id('crt' + port).dispatch('keydown', {key, preventDefault() {}});
        },
        save() {
            id('save-image').click();
            return download;
        },
        close() {
            client.onclose = null;
            client.socket.close();
        },
    };
}

module.exports = {openConsole};
