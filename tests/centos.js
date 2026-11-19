'use strict';

const {OS6Session} = require('../scripts/os6-session.js');

async function main() {
    const session = new OS6Session(process.argv[2]);
    try {
        await session.boot();
        await session.command('.RUN XHELLO6', 'HELLO FROM C ON CENTURION CPU6');
        await session.command('.STA', 'STATUS DISPLAY');
        console.log('PASS: the SDCC executable ran and returned to CENTOS.');
    } finally {
        session.stop();
    }
}

main().then(() => process.exit(0), error => {
    console.error(error);
    process.exit(1);
});
