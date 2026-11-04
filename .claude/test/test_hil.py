#!/usr/bin/env python3
"""The HIL profile: roster checks, the host side of the HIL sketches' serial protocol against a simulated DUT, and
the profile bound to agentrc's arduino_hil.py. The simulation proves the host's parsing and verdicts only;
what the firmware does is a farm run's evidence."""
import json
import sys
import tempfile
import time
import unittest
import zlib
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / '.claude' / 'skills' / 'hil-contract' / 'scripts'))
import hil_test as ht  # noqa: E402

E = ht.engine()
STORAGE = {'transport': 'qspi', 'jedec': 'c84015', 'window': 2097152, 'offset': 2093056, 'length': 4096}


class FakeDut:
    """spiflash_region's protocol over a byte buffer standing in for the flash window (spiflash_region.ino)."""

    def __init__(self, transport='qspi', jedec='c84015', window=2097152, read_only=False, broken=None):
        self.transport, self.jedec, self.window, self.read_only = transport, jedec, window, read_only
        self.flash = bytearray(b'\x5a' * window)
        self.out, self.writes, self.broken = b'', [], broken or set()
        self.write_timeout = None
        self.overlong_runs = False   # the framing bug: an overlong line's tail ran as a command
        self.closed = False

    def reset_input_buffer(self):
        pass

    def close(self):
        self.closed = True

    @property
    def in_waiting(self):
        return min(len(self.out), 17)   # replies arrive in fragments

    def read(self, n):
        chunk, self.out = self.out[:n], self.out[n:]
        return chunk

    def write(self, data):
        for line in data.decode().splitlines():
            if len(line) < 96 or self.overlong_runs:   # the sketch's line buffer
                self.out += (self.reply(line.split(None, 1)[-1] if len(line) >= 96 else line) + '\r\n').encode()

    def region(self, cmd, nonce, a, b):
        try:
            off, n = int(a), int(b)
        except (TypeError, ValueError):
            return None, 'malformed'
        if n == 0:
            return None, 'empty'
        if off % 4096 or n % 4096:
            return None, 'misaligned'
        if off >= self.window or n > self.window - off:
            return None, 'outside-window'
        return (off, n), None

    def reply(self, line):
        w = line.split()
        cmd, nonce = w[0], w[1]
        if cmd == 'INFO':
            return f'HIL_INFO {nonce} ok=1 transport={self.transport} jedec={self.jedec} size={self.window} sector=4096'
        op = {'CHECK': 'check', 'RUN': 'run', 'ERASE': 'erase'}.get(cmd)
        if op is None or (self.read_only and op != 'check'):
            return f'HIL_RESULT {nonce} ? REJECT why={"read-only" if self.read_only else "unknown-command"}'
        reg, why = self.region(cmd, nonce, *(w[2:4] + [None, None])[:2])
        if why:
            return f'HIL_RESULT {nonce} {op} REJECT why={why}'
        off, n = reg
        if op in ('run', 'erase') and op not in self.broken:
            self.writes.append((op, off, n))
            self.flash[off:off + n] = b'\xff' * n
        if op == 'run' and 'run' not in self.broken:
            seed = zlib.crc32(nonce.encode())
            self.flash[off:off + n] = bytes(((seed >> ((i & 3) * 8)) ^ (i * 7) ^ (i >> 8)) & 0xff for i in range(n))
        data = bytes(self.flash[off:off + n])
        erased = int(data == b'\xff' * n)
        return f'HIL_RESULT {nonce} {op} PASS off={off} len={n} crc={zlib.crc32(data):08x} erased={erased}'


class Protocol(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.board = {'arduino': {'storage': dict(STORAGE)}}

    def run_test(self, fn, dut, timeout=5):
        with mock.patch.object(ht, 'open_console', return_value=('/dev/ttyACM0', dut)):
            return fn({'dir': '/x'}, 'abcd1234', time.monotonic() + timeout, None, self.tmp.name, self.board)

    def test_region_passes_and_leaves_the_region_erased(self):
        dut = FakeDut()
        ok, detail = self.run_test(ht.test_region, dut)
        self.assertTrue(ok, detail)
        self.assertIn('6 refusals and an overlong RUN left the pattern', detail)
        self.assertEqual(dut.flash[2093056:], b'\xff' * 4096)
        self.assertEqual([op for op, *_ in dut.writes], ['run', 'erase'])

    def log(self):
        return (Path(self.tmp.name) / 'spiflash-abcd1234.log').read_text()

    def test_region_fails_on_a_wrong_jedec(self):
        dut = FakeDut(jedec='ef4015')
        ok, detail = self.run_test(ht.test_region, dut)
        self.assertFalse(ok)
        self.assertIn('jedec', detail)
        self.assertEqual(dut.writes, [])   # no RUN yet: nothing to erase

    def test_region_does_not_erase_after_an_info_timeout(self):
        dut = FakeDut()
        dut.reply = lambda line: 'HIL_INFO stale ok=1'
        with self.assertRaisesRegex(TimeoutError, 'no reply'):
            self.run_test(ht.test_region, dut, timeout=0.3)
        self.assertEqual(dut.writes, [])
        self.assertNotIn('ERASE', self.log())

    def test_region_fails_when_the_pattern_is_not_written(self):
        dut = FakeDut(broken={'run'})
        ok, detail = self.run_test(ht.test_region, dut)
        self.assertFalse(ok)
        self.assertIn('RUN', detail)
        self.assertEqual([op for op, *_ in dut.writes], ['erase'])   # a failed RUN is erased after too

    def test_region_fails_when_a_request_is_not_refused(self):
        dut = FakeDut()
        dut.region = lambda *a: ((2093056, 4096), None)
        ok, detail = self.run_test(ht.test_region, dut)
        self.assertFalse(ok)
        self.assertIn('want REJECT', detail)
        self.assertEqual(dut.flash[2093056:], b'\xff' * 4096)   # the RUN it did not refuse is erased again
        self.assertEqual(dut.writes[-1], ('erase', 2093056, 4096))

    def test_region_erases_after_a_reply_timeout(self):
        dut = FakeDut()
        reply = dut.reply
        dut.reply = lambda line: 'HIL_RESULT stale' if line.startswith('RUN abcd1234x0 ') else reply(line)
        with self.assertRaisesRegex(TimeoutError, 'no reply'):
            self.run_test(ht.test_region, dut, timeout=0.5)
        self.assertEqual(dut.flash[2093056:], b'\xff' * 4096)
        self.assertEqual([op for op, *_ in dut.writes], ['run', 'erase'])

    def test_a_failed_cleanup_erase_keeps_the_original_failure(self):
        for name, broken, stale in (('failing return', {'run'}, ('ERASE abcd1234z ',)),
                                    ('timeout', set(), ('RUN abcd1234x0 ', 'ERASE abcd1234z '))):
            dut = FakeDut(broken=broken)
            reply = dut.reply
            dut.reply = lambda line, reply=reply, stale=stale: \
                'HIL_RESULT stale' if line.startswith(stale) else reply(line)
            with self.subTest(name), mock.patch.object(ht, 'CLEANUP_WAIT', 0.2):
                if broken:
                    ok, detail = self.run_test(ht.test_region, dut, timeout=0.5)
                    self.assertFalse(ok)
                    self.assertIn('RUN', detail)
                else:
                    with self.assertRaisesRegex(TimeoutError, 'RUN abcd1234x0'):
                        self.run_test(ht.test_region, dut, timeout=0.3)
                self.assertTrue(dut.closed)
                self.assertIn("! cleanup ERASE: TimeoutError: no reply to 'ERASE abcd1234z", self.log())

    def test_region_does_not_erase_on_an_abort(self):
        dut = FakeDut()
        reply = dut.reply

        def interrupted(line):
            if line.startswith('RUN abcd1234x0 '):
                raise KeyboardInterrupt
            return reply(line)
        dut.reply = interrupted
        with self.assertRaises(KeyboardInterrupt):
            self.run_test(ht.test_region, dut)
        self.assertEqual([op for op, *_ in dut.writes], ['run'])   # the engine holds the board: no cleanup ERASE
        self.assertTrue(dut.closed)
        self.assertIn('> RUN abcd1234r', self.log())

    def test_an_abort_during_the_cleanup_still_closes_and_logs(self):
        dut = FakeDut(broken={'run'})
        reply = dut.reply

        def interrupted(line):
            if line.startswith('ERASE abcd1234z '):
                raise KeyboardInterrupt
            return reply(line)
        dut.reply = interrupted
        with self.assertRaises(KeyboardInterrupt):
            self.run_test(ht.test_region, dut)
        self.assertTrue(dut.closed)
        self.assertIn('> RUN abcd1234r', self.log())

    def test_region_fails_when_an_overlong_line_runs_its_tail(self):
        dut = FakeDut()
        dut.overlong_runs = True
        ok, detail = self.run_test(ht.test_region, dut)
        self.assertFalse(ok)
        self.assertIn('overlong RUN CHECK', detail)

    def test_a_reply_for_another_region_or_operation_fails(self):
        for name, bend in (('region', lambda r: r.replace('off=2093056', 'off=0')),
                           ('operation', lambda r: r.replace(' erase PASS', ' check PASS'))):
            dut = FakeDut()
            reply = dut.reply
            dut.reply = lambda line, reply=reply, bend=bend: bend(reply(line))
            with self.subTest(name):
                ok, detail = self.run_test(ht.test_region, dut)
                self.assertFalse(ok)
                self.assertIn('want', detail)
        dut = FakeDut(read_only=True)
        dut.flash[2093056:] = b'\xff' * 4096
        reply = dut.reply
        dut.reply = lambda line: reply(line).replace('len=4096', 'len=8192')
        ok, detail = self.run_test(ht.test_restore, dut)
        self.assertFalse(ok)

    def test_info_needs_the_sector_size(self):
        dut = FakeDut()
        reply = dut.reply
        dut.reply = lambda line: reply(line).replace('sector=4096', 'sector=512')
        ok, detail = self.run_test(ht.test_region, dut)
        self.assertFalse(ok)
        self.assertIn('sector', detail)

    def test_restore_needs_the_region_erased(self):
        dut = FakeDut(read_only=True)
        ok, detail = self.run_test(ht.test_restore, dut)
        self.assertFalse(ok)
        self.assertIn('erased=0', detail)
        dut.flash[2093056:] = b'\xff' * 4096
        ok, detail = self.run_test(ht.test_restore, dut)
        self.assertTrue(ok, detail)

    def test_a_reply_with_another_nonce_times_out(self):
        dut = FakeDut()
        dut.reply = lambda line: 'HIL_INFO stale ok=1'
        with mock.patch.object(ht, 'open_console', return_value=('/dev/ttyACM0', dut)), \
                self.assertRaisesRegex(TimeoutError, 'no reply'):   # arduino_hil.run_test makes it the test's fail
            ht.test_restore({'dir': '/x'}, 'n', time.monotonic() + 0.3, None, self.tmp.name, self.board)

    def test_pattern_crc_matches_the_sketch_formula(self):
        seed = zlib.crc32(b'n1')
        data = bytes(((seed >> ((i & 3) * 8)) ^ (i * 7) ^ (i >> 8)) & 0xff for i in range(8192))
        self.assertEqual(ht.pattern_crc('n1', 8192), zlib.crc32(data))


class Rosters(unittest.TestCase):
    def check(self, arduino):
        ht.check_board('b', {'arduino': arduino}, 'r.json')

    def test_checked_in_rosters_load(self):
        roster = E.load_roster(ROOT / 'test/hil/ci.json')
        self.assertEqual(sorted(roster), ['metro_m4_express', 'raspberry_pi_pico'])
        for b in roster.values():
            self.assertEqual(b['arduino']['tests'], ['spiflash_region'])

    def test_storage_refusals(self):
        good = {'vid_pid': '239a:8020', 'storage': dict(STORAGE)}
        self.check(good)
        for bad in ({**good, 'vid_pid': '239A-8020'}, {**good, 'storage': {**STORAGE, 'offset': 2093057}},
                    {**good, 'storage': {**STORAGE, 'length': 8192}}, {**good, 'storage': {**STORAGE, 'length': 0}},
                    {**good, 'storage': {**STORAGE, 'jedec': 'xyz'}},
                    {**good, 'storage': {k: v for k, v in STORAGE.items() if k != 'window'}}):
            with self.assertRaises(E.Refused, msg=json.dumps(bad)):
                self.check(bad)

    def test_example_roster_is_a_template(self):
        with self.assertRaises(E.Refused):
            E.load_roster(ROOT / 'test/hil/local.example.json')   # its jedec is a placeholder


class Profile(unittest.TestCase):
    def test_tests_and_sketches_agree(self):
        sketches = {Path(s).name for s in ht.BUILD.HIL_SKETCHES['device']}
        self.assertEqual(set(ht.TESTS.values()), sketches)   # the engine's bind() checks the restore and the roles

    def test_engine_is_bound_to_this_profile(self):
        self.assertIs(E.P, ht)
        self.assertIs(E.B.P, ht.BUILD)


if __name__ == '__main__':
    unittest.main()
