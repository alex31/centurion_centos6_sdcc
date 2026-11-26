#!/usr/bin/env python3
"""Boot the native emulator through its TCP listeners on loopback."""

import argparse
import contextlib
import os
from pathlib import Path
import re
import socket
import subprocess
import sys
import tempfile
import time


ROOT = Path(__file__).resolve().parents[1]
ANSI = re.compile(rb'\x1b\[[0-?]*[ -/]*[@-~]')
sys.path.insert(0, str(ROOT / 'scripts'))
from centos_disk import allocated_sectors, root_entry


def await_text(connection, marker, timeout=12):
    end = time.monotonic() + timeout
    collected = bytearray()
    while time.monotonic() < end:
        connection.settimeout(max(0.01, end - time.monotonic()))
        try:
            fragment = connection.recv(16384)
        except TimeoutError:
            break
        if not fragment:
            break
        collected.extend(fragment)
        if marker in ANSI.sub(b'', collected):
            return
    raise AssertionError(f'Missing {marker!r} in {ANSI.sub(b"", collected)[-1200:]!r}')


def connect(port, process):
    for _ in range(100):
        if process.poll() is not None:
            raise RuntimeError(f'Native emulator exited ({process.returncode})')
        try:
            return socket.create_connection(('127.0.0.1', port), timeout=0.2)
        except ConnectionRefusedError:
            time.sleep(0.05)
    raise TimeoutError(f'Telnet listener did not start on {port}')


def make_sender(work):
    sender, payload = (work / name for name in ('e-kermit-send', 'hello.exf'))
    source = ROOT / 'swext/ekermit'
    subprocess.run([
        'cc', '-O2', '-DNODEBUG', '-DNO_CTRLC', '-DNO_SSW', '-DNO_AT', '-DNO_SCAN',
        '-DSTATIC=static', '-DP_PKTLEN=512', '-DFN_MAX=16', '-I', source,
        ROOT / 'tests/cpp-kermit-sender.c', source / 'kermit.c', '-o', sender,
    ], check=True)
    subprocess.run([
        ROOT / 'run/cpu6-os6-sdcc', '--gc-sections', ROOT / 'examples/hello_os6.c', '-o', payload,
    ], cwd=ROOT, check=True)
    return sender, payload


def transfer_executable(crt, sender, payload, serial_port):
    count, remainder = divmod(payload.stat().st_size, 400)
    assert count and not remainder
    crt.sendall(f".NEW XUPLOAD ON 1 'E' {count}S\r".encode())
    await_text(crt, b'DONE')
    crt.sendall(b'.USE CRT1 FOR SYS000\r.USE XUPLOAD ON 1 FOR SYS001\r.RUN SDCC.XKERMIT\r')
    await_text(crt, b'READY KERMIT CRT1')
    with socket.create_connection(('127.0.0.1', serial_port), timeout=2) as serial:
        serial.setblocking(True)
        subprocess.run([sender, payload], stdin=serial, stdout=serial, check=True, timeout=60)
    await_text(crt, f'RECEIVED {count} SECTORS'.encode())
    crt.sendall(b'.RUN XUPLOAD\r')
    await_text(crt, b'HELLO FROM C ON CENTURION CPU6')


def smoke(first_port, image, kermit=False, format_data=False):
    with tempfile.TemporaryDirectory(prefix='centurion-cpp-telnet-') as work:
        directory = Path(work)
        if kermit:
            sender, payload = make_sender(directory)
        disks = directory / 'disks'
        disks.mkdir()
        with (disks / 'data.img').open('wb') as disk:
            disk.truncate(6_651_904)
        environment = {**os.environ, 'CENTURION_TELNET_BASE': str(first_port),
                       'CENTURION_WEB_PORT': str(first_port + 5), 'CENTURION_BIND': '127.0.0.1'}
        with (directory / 'server.log').open('w+') as log, subprocess.Popen(
            [ROOT / 'run/centurion-system', image], cwd=directory, env=environment,
            stdin=subprocess.PIPE, stdout=log, stderr=log,
        ) as process:
            try:
                with contextlib.ExitStack() as stack:
                    panel = stack.enter_context(connect(first_port, process))
                    crt = stack.enter_context(connect(first_port + 1, process))
                    manager = stack.enter_context(connect(first_port + 3, process))
                    await_text(panel, b'Control Panel')
                    panel.sendall(b'\xff\xfa\x1f')
                    panel.sendall(b'\x00\x64\x00\x28\xff')
                    panel.sendall(b'\xf0\xff\xfa\x18\x00XTERM\xff\xf0?')
                    await_text(panel, b'xterm (100 columns by 40 rows)')
                    panel.sendall(b'm')
                    await_text(panel, b'MICROCODE STATE')
                    panel.sendall(b'g')
                    await_text(panel, b'DIAG ROMs ON')
                    panel.sendall(b'g')
                    await_text(panel, b'DIAG ROMs OFF')
                    await_text(crt, b'D=')
                    await_text(manager, b'Disk Management')
                    crt.sendall(b'H1')
                    await_text(crt, b'SYSTEM DISK (S)= 1')
                    crt.sendall(b'\r')
                    await_text(crt, b'ENTER NEW SYSTEM DATE')
                    crt.sendall(b'112226\r\n')
                    await_text(crt, b'ENTER SYSTEM TIME')
                    crt.sendall(b'120000\r')
                    await_text(crt, b'CRT0 READY')
                    crt.sendall(b'.DIR 1 (BASIC)\r')
                    await_text(crt, b'BASIC      A')
                    manager.sendall(b'l')
                    await_text(manager, b'data.img')
                    manager.sendall(b'0')
                    await_text(manager, b'Mounted data.img to DSK2 Unit 0')
                    if format_data:
                        crt.sendall(b'.NOGUARD 0\r.FORMAT 0\r')
                        await_text(crt, b'DONE', timeout=75)
                        crt.sendall(b".CLR 0 VOL=DATA\r.NEW DATATEST ON 0 'A' 1S\r.DIR 0 (DATATEST)\r")
                        await_text(crt, b'DATATEST   A')
                        print('PASS: formatted the second Hawk and created a file on unit 0')
                    with connect(first_port + 4, process) as raw:
                        with connect(first_port + 2, process) as exclusive:
                            assert exclusive.recv(1) == b'', 'CRT1 remained open while raw serial was in use'
                        # The raw endpoint must not send Telnet negotiations or a terminal screen.
                        raw.settimeout(0.1)
                        try:
                            packet = raw.recv(1024)
                        except TimeoutError:
                            pass
                        else:
                            assert not packet.startswith(b'\x1b[2J') and b'\xff\xfb' not in packet
                    with connect(first_port + 2, process) as secondary:
                        await_text(secondary, b'\xff\xfb\x01')
                    if kermit:
                        transfer_executable(crt, sender, payload, first_port + 4)
                    process.stdin.write(b'save snapshot.img\nq\n')
                    process.stdin.flush()
                    assert process.wait(timeout=10) == 0
                    assert (directory / 'snapshot.img').stat().st_size == image.stat().st_size
                    if kermit:
                        disk = (directory / 'snapshot.img').read_bytes()
                        entry = root_entry(disk, 'XUPLOAD')
                        assert entry is not None and entry.kind == 4
                        received = b''.join(disk[sector * 512: sector * 512 + 400]
                                            for sector in allocated_sectors(disk, entry))
                        assert received[:payload.stat().st_size] == payload.read_bytes()
                        print(f'PASS: Kermit transferred and executed {payload.stat().st_size} bytes')
            except BaseException:
                log.seek(0)
                print(log.read())
                raise
            finally:
                if process.poll() is None:
                    process.terminate()
                    process.wait(timeout=5)
    print('PASS: Telnet panel, CRT0 boot and directory, CRT1/raw exclusivity, disk manager and save')


if __name__ == '__main__':
    command = argparse.ArgumentParser(description=__doc__)
    command.add_argument('--base', type=int, default=32323)
    command.add_argument('--image', type=Path, default=ROOT / 'build/out/image/centos-apps.img')
    command.add_argument('--kermit', action='store_true', help='transfer and execute a CPU6 program')
    command.add_argument('--format-data', action='store_true', help='format the second Hawk from CENTOS')
    args = command.parse_args()
    smoke(args.base, args.image.resolve(), args.kermit, args.format_data)
