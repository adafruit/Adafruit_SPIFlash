#!/usr/bin/env python3
"""The Adafruit SPIFlash HIL harness (SKILL.md beside this directory). Linux only.

  hil_test.py run [-b BOARD]... [-t TEST]... [--record-dir DIR] [--run-dir DIR] [--staged MANIFEST] ROSTER
  hil_test.py commission -b BOARD [--record-dir DIR] ROSTER
  hil_test.py flash -b BOARD --build-record R --sketch S --under-hold REASON [--staged M] ROSTER
  hil_test.py restore -b BOARD --build-record R --under-hold REASON [--report PATH] [--release] [--staged M] ROSTER
  hil_test.py gdb -b BOARD --build-record R --sketch S --under-hold REASON [--ex CMD]... [--timeout S] [--staged M]
                 ROSTER
  hil_test.py exec -b BOARD --under-hold REASON [--timeout S] [--staged M] ROSTER -- ARGV...
  hil_test.py console -b BOARD --under-hold REASON [--seconds S] [--baud N] [--reset] [--staged M] ROSTER
  hil_test.py check -b BOARD --build-record R --under-hold REASON [--test T] [--staged M] ROSTER
  hil_test.py report RUN_DIR [-b BOARD]...
  hil_test.py lock (hold|release|status) ...        the pinned agentrc hil_lock.py

flash, restore, gdb and check take --bundle DIR, a record bundle `hil_remote.py session stage` staged,
--build-record then inside it. restore --release passes only when the hold was released too.

The last stdout line of every command but lock is JSON. Exits: 0 pass, 1 a board failed or did not run (cleaned up
or untouched), 2 refused before any hardware access, 4 a cleanup did not finish (board held).

This file is the library's HIL profile for agentrc's arduino_hil.py (the `arduino` skill): its tests over the HIL
sketches' serial protocol (test/hil/spiflash_region/spiflash_region.ino) and its roster checks; the mechanics are
there, loaded from the agentrc commit test/hil/agentrc.json pins.
"""

import json
import re
import sys
import time
import zlib
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(ROOT / '.claude' / 'skills' / 'build' / 'scripts'))
import agentrc_pin  # noqa: E402
import check_build as BUILD  # noqa: E402

TESTS = {'spiflash_region': 'spiflash_region', 'spiflash_restore': 'spiflash_restore'}
ROLE_TESTS = {'device': ('spiflash_region', 'spiflash_restore')}
RESTORE_TEST = {'device': 'spiflash_restore'}
REPORT_SCHEMA = 'adafruit-spiflash-hil'
# hil_remote.py (agentrc arduino_remote.py)
REMOTE_BASE = '~/hil-remote/adafruit-spiflash'
REMOTE_ROSTER = 'test/hil/ci.json'
HIL = '.claude/skills/hil-contract/scripts/hil_test.py'
SECTOR = 4096
STORAGE_KEYS = ('transport', 'jedec', 'window', 'offset', 'length')

E = None   # agentrc's arduino_hil, bound to this profile by engine()


def check_board(name, b, path):
    """The roster's `arduino` section: the DUT's commissioned vid_pid and its flash storage, a region of whole
    sectors inside the transport window."""
    a = b.get('arduino', {})
    if not re.fullmatch(r'[0-9a-f]{4}:[0-9a-f]{4}', str(a.get('vid_pid', ''))):
        raise E.Refused(f'roster {path}: {name}: arduino.vid_pid "vvvv:pppp" (the DUT\'s, commissioned) is required')
    st = a.get('storage')
    if not isinstance(st, dict) or any(k not in st for k in STORAGE_KEYS):
        raise E.Refused(f'roster {path}: {name}: arduino.storage needs {", ".join(STORAGE_KEYS)}')
    ints = [st[k] for k in ('window', 'offset', 'length')]
    if not all(isinstance(v, int) and v >= 0 for v in ints) or not re.fullmatch(r'[0-9a-f]{6}', str(st['jedec'])):
        raise E.Refused(f'roster {path}: {name}: arduino.storage window/offset/length are byte counts, jedec 6 hex')
    window, off, length = ints
    if not (length > 0 and off % SECTOR == 0 and length % SECTOR == 0 and length <= window - off):
        raise E.Refused(f'roster {path}: {name}: arduino.storage region [{off}, +{length}) is not whole {SECTOR}-byte '
                        f'sectors inside the {window}-byte window')


def pattern_crc(nonce, length):
    """The crc32 spiflash_region's RUN leaves in a region of `length` bytes (its pattern(), seeded by crc32(nonce))."""
    seed = zlib.crc32(nonce.encode())
    data = bytes(((seed >> ((i & 3) * 8)) ^ (i * 7) ^ (i >> 8)) & 0xff for i in range(length))
    return zlib.crc32(data)


class Console:
    """The DUT's USB CDC tty: one command line out, its reply line (the one carrying the command's nonce) back."""

    def __init__(self, port, deadline, log):
        self.port, self.deadline, self.log = port, deadline, log

    def send(self, line):
        import serial
        self.port.write_timeout = max(0.1, self.deadline - time.monotonic())
        try:
            self.port.write(line.encode() + b'\n')
        except serial.SerialTimeoutException:
            raise TimeoutError(f'write of {line!r} timed out')
        self.log.append(f'> {line}')

    def ask(self, line, nonce):
        self.send(line)
        buf = b''
        while time.monotonic() < self.deadline:
            buf += self.port.read(self.port.in_waiting or 1)   # read(n) blocks for n bytes or the timeout
            while b'\n' in buf:
                raw, buf = buf.split(b'\n', 1)
                text = raw.decode(errors='replace').strip()
                if text:
                    self.log.append(f'< {text}')
                words = text.split()
                if len(words) >= 2 and words[0] in ('HIL_INFO', 'HIL_RESULT') and words[1] == nonce:
                    return words
        raise TimeoutError(f'no reply to {line!r} within the test timeout')


def fields(words):
    return dict(w.split('=', 1) for w in words if '=' in w)


def open_console(dev, deadline):
    import serial
    tty = E.node(dev, '*:*/tty/tty*', deadline)
    port = serial.Serial(tty, 115200, timeout=0.2)
    port.reset_input_buffer()
    return tty, port


def info_problem(console, nonce, st):
    """'' when INFO reports the roster's transport, JEDEC id and window, else why not."""
    got = fields(console.ask(f'INFO {nonce}', nonce))
    want = {'ok': '1', 'transport': st['transport'], 'jedec': st['jedec'], 'size': str(st['window']),
            'sector': str(SECTOR)}
    wrong = {k: got.get(k) for k, v in want.items() if got.get(k) != v}
    return f'INFO {wrong} != roster {want}' if wrong else ''


def ask_op(console, cmd, nonce, off, length):
    """The reply's state (PASS, FAIL, REJECT; WRONG when it answers another operation or region) and its fields,
    the reply itself as `reply`."""
    words = console.ask(f'{cmd} {nonce} {off} {length}', nonce)
    got = fields(words)
    state = words[3] if len(words) > 3 else '?'
    region = (got.get('off'), got.get('len')) == (str(off), str(length))
    if words[2:3] != [cmd.lower()] or (state != 'REJECT' and not region):
        state = 'WRONG'
    got['reply'] = ' '.join(words[2:])
    return state, got


def test_region(dev, nonce, deadline, tracker, run_dir, board):
    """spiflash_region on the roster's region: INFO matches the roster; RUN erases, writes the nonce's pattern and
    reads it back; CHECK sees that pattern; malformed, misaligned, empty and out-of-window requests are refused with
    the pattern intact (no write or erase happened); ERASE leaves the region erased."""
    st = board['arduino']['storage']
    off, length, window = st['offset'], st['length'], st['window']
    tty, port = open_console(dev, deadline)
    log, steps = [], []
    console = Console(port, deadline, log)
    try:
        why = info_problem(console, nonce + 'i', st)
        if why:
            return False, f'{tty}: {why}'
        state, got = ask_op(console, 'RUN', nonce + 'r', off, length)
        want_crc = f'{pattern_crc(nonce + "r", length):08x}'
        if state != 'PASS' or got.get('crc') != want_crc:
            return False, f'{tty}: RUN {got["reply"]} (want PASS off={off} len={length} crc={want_crc})'
        steps.append('run')
        rejects = [('RUN', off + 1, length, 'misaligned'), ('RUN', off, 0, 'empty'),
                   ('ERASE', off, window - off + SECTOR, 'outside-window'), ('RUN', window, SECTOR, 'outside-window'),
                   ('ERASE', 2 ** 32 - SECTOR, 2 * SECTOR, 'outside-window'), ('RUN', f'{off}x', length, 'malformed')]
        for i, (cmd, o, n, reason) in enumerate(rejects):
            sub = f'{nonce}x{i}'
            state, got = ask_op(console, cmd, sub, o, n)
            if state != 'REJECT' or got.get('why') != reason:
                return False, f'{tty}: {cmd} {o} {n}: {got["reply"]} (want REJECT why={reason})'
        # longer than the sketch's 96-byte line buffer, so dropped whole: the RUN at its tail must not run
        console.send('x' * 96 + f' RUN {nonce}o {off} {length}')
        state, got = ask_op(console, 'CHECK', nonce + 'c', off, length)
        if state != 'PASS' or got.get('crc') != want_crc or got.get('erased') != '0':
            return False, f'{tty}: after the refusals and an overlong RUN CHECK {got["reply"]} (want PASS off={off} ' \
                          f'len={length} crc={want_crc})'
        steps.append(f'{len(rejects)} refusals and an overlong RUN left the pattern')
        erase, egot = ask_op(console, 'ERASE', nonce + 'e', off, length)
        state, got = ask_op(console, 'CHECK', nonce + 'k', off, length)
        if erase != 'PASS' or state != 'PASS' or got.get('erased') != '1':
            return False, f'{tty}: ERASE {egot["reply"]}, then CHECK {got["reply"]} (want PASS off={off} ' \
                          f'len={length} erased=1)'
        steps.append('erased')
        return True, f'{tty}: [{off:#x}, +{length:#x}) {", ".join(steps)}'
    finally:
        port.close()
        (Path(run_dir) / f'spiflash-{nonce}.log').write_text('\n'.join(log) + '\n')


def test_restore(dev, nonce, deadline, tracker, run_dir, board):
    """spiflash_restore: INFO matches the roster and the region reads back erased (the state the battery leaves)."""
    st = board['arduino']['storage']
    tty, port = open_console(dev, deadline)
    log = []
    console = Console(port, deadline, log)
    try:
        why = info_problem(console, nonce + 'i', st)
        if why:
            return False, f'{tty}: {why}'
        state, got = ask_op(console, 'CHECK', nonce + 'c', st['offset'], st['length'])
        ok = state == 'PASS' and got.get('erased') == '1'
        return ok, f'{tty}: region [{st["offset"]:#x}, +{st["length"]:#x}) CHECK {got["reply"]}'
    finally:
        port.close()
        (Path(run_dir) / f'spiflash-{nonce}.log').write_text('\n'.join(log) + '\n')


TEST_FUNCS = {'spiflash_region': test_region, 'spiflash_restore': test_restore}


def engine():
    """agentrc's arduino_hil bound to this profile (and its build profile)."""
    global E
    if E is None:
        BUILD.engine()
        E = agentrc_pin.load('skills/arduino/scripts/arduino_hil.py').bind(sys.modules[__name__])
    return E


def main(argv=None):
    try:
        eng = engine()
    except agentrc_pin.PinError as e:
        print(json.dumps({'pass': False, 'error': str(e)}), flush=True)
        return 2
    return eng.main(argv)


if __name__ == '__main__':
    sys.modules.setdefault('hil_test', sys.modules[__name__])
    sys.exit(main())
