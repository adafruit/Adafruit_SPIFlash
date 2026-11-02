#!/usr/bin/env python3
"""The build contract's profile: the board table against CI's matrix, source classification and selection through
agentrc's arduino_build.py at the pin, the roster VID:PID adapter and the RP2040 FS-window record check. arduino-cli
is not run; a real build is evidence only from a run of the script itself."""
import json
import re
import struct
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / '.claude' / 'skills' / 'build' / 'scripts'))
import check_build as cb  # noqa: E402

E = cb.engine()


def elf32_with_symbols(symbols):
    """A minimal ELF32 holding only a .symtab with `symbols` ({name: value}) and its .strtab."""
    strtab = b'\0' + b''.join(n.encode() + b'\0' for n in symbols)
    offsets, pos = [], 1
    for n in symbols:
        offsets.append(pos)
        pos += len(n) + 1
    symtab = bytes(16) + b''.join(struct.pack('<IIIBBH', o, v, 0, 0, 0, 0) for o, v in zip(offsets, symbols.values()))
    shoff = 52 + len(symtab) + len(strtab)
    head = bytearray(52)
    head[:6] = b'\x7fELF\x01\x01'
    struct.pack_into('<II', head, 28, 0, shoff)
    struct.pack_into('<HHHH', head, 42, 32, 0, 40, 3)
    sections = struct.pack('<10I', *[0] * 10)
    sections += struct.pack('<10I', 0, 2, 0, 0, 52, len(symtab), 2, 0, 4, 16)
    sections += struct.pack('<10I', 0, 3, 0, 0, 52 + len(symtab), len(strtab), 0, 0, 1, 0)
    return bytes(head) + symtab + strtab + sections


class Tables(unittest.TestCase):
    def test_ci_matrix_is_in_the_table(self):
        text = (ROOT / '.github/workflows/githubci.yml').read_text()
        matrix = re.findall(r"^\s+- '([^']+)'", text.split('arduino-platform:')[1].split('runs-on')[0], re.M)
        self.assertTrue(matrix)
        self.assertEqual(sorted(m for m in matrix if m not in cb.BOARDS), [])

    def test_representatives_and_rig_boards(self):
        for family, boards in cb.REPRESENTATIVES.items():
            for b in boards:
                self.assertEqual(cb.BOARDS[b]['family'], family)
        rig = {b for b, v in cb.BOARDS.items() if 'span' in v}
        self.assertEqual(rig, {'metro_m4_express', 'raspberry_pi_pico'})
        for b in rig:
            self.assertIn(cb.BOARDS[b]['filters'], cb.BOARDS)
            self.assertIn(b, [r[0] for r in cb.REPRESENTATIVES.values()])

    def test_pico_fqbn_reserves_an_fs_and_pins_the_sdk_stack(self):
        fqbn = cb.BOARDS['raspberry_pi_pico']['fqbn']
        self.assertIn('flash=2097152_1048576', fqbn)
        self.assertIn('usbstack=picosdk', fqbn)
        self.assertIn('usbstack=arduino', cb.BOARDS['metro_m4_express']['fqbn'])

    def test_hil_sketches_exist(self):
        for sketch in cb.HIL_SKETCHES['device']:
            self.assertTrue((ROOT / sketch / f'{Path(sketch).name}.ino').is_file(), sketch)

    def test_contract_files_exist(self):
        for f in cb.CONTRACT_FILES:
            self.assertTrue((ROOT / f).is_file(), f)


class Selection(unittest.TestCase):
    def test_transport_rules(self):
        cases = {'src/qspi/Adafruit_FlashTransport_QSPI_SAMD.cpp': ('family', 'samd'),
                 'src/qspi/Adafruit_FlashTransport_QSPI_NRF.cpp': ('family', 'nrf'),
                 'src/rp2040/Adafruit_FlashTransport_RP2040.cpp': ('family', 'rp2040'),
                 'src/esp32/Adafruit_FlashTransport_ESP32.cpp': ('family', 'esp32'),
                 'src/qspi/Adafruit_FlashTransport_QSPI.h': ('shared', None),
                 'src/spi/Adafruit_FlashTransport_SPI.cpp': ('shared', None),
                 'src/Adafruit_SPIFlashBase.cpp': ('shared', None),
                 'examples/flash_info/flash_info.ino': ('example', 'examples/flash_info'),
                 'test/hil/spiflash_region/spiflash_region.ino': ('hil_sketch', 'test/hil/spiflash_region'),
                 'test/hil/ci.json': ('hil', None),
                 '.claude/skills/build/scripts/check_build.py': ('tests', 'build'),
                 'README.md': ('nothing', None), 'platformio.ini': ('nothing', None),
                 'weird/file': ('uncovered', None)}
        for path, want in cases.items():
            self.assertEqual(E.classify(path)[:2], want, path)

    def test_family_change_builds_its_representatives(self):
        sel = E.select(['src/rp2040/Adafruit_FlashTransport_RP2040.cpp'], roster=['raspberry_pi_pico',
                                                                                 'metro_m4_express'])
        self.assertEqual(sorted(sel['builds']), ['feather_rp2040', 'raspberry_pi_pico'])
        self.assertEqual(sel['hil']['boards'], ['raspberry_pi_pico'])

    def test_hil_sketch_builds_only_on_rig_boards(self):
        sel = E.select(['test/hil/spiflash_region/spiflash_region.ino'], roster=['raspberry_pi_pico'])
        self.assertEqual(sorted(sel['builds']), ['metro_m4_express', 'raspberry_pi_pico'])
        self.assertTrue(sel['hil']['full'])

    def test_shared_change_names_families_the_roster_lacks(self):
        sel = E.select(['src/Adafruit_SPIFlash.cpp'], roster=['raspberry_pi_pico', 'metro_m4_express'])
        self.assertEqual([u['family'] for u in sel['hil']['uncovered']], ['avr', 'esp32', 'nrf'])


class Identity(unittest.TestCase):
    def test_vid_pid_comes_from_the_roster(self):
        self.assertEqual(cb.vid_pid('x', 'raspberry_pi_pico', {'arduino': {'vid_pid': '2e8a:000a'}}), '2e8a:000a')
        self.assertIsNone(cb.vid_pid('x', 'raspberry_pi_pico', None))
        self.assertIsNone(cb.vid_pid('x', 'raspberry_pi_pico', {'arduino': {'vid_pid': '2E8A-000A'}}))


class FsWindow(unittest.TestCase):
    LD = ('MEMORY\n{\n    FLASH(rx) : ORIGIN = 0x10000000, LENGTH = 1044480\n}\n'
          'PROVIDE ( _FS_start     = 269479936 );\nPROVIDE ( _FS_end       = 270528512 );\n')
    ROSTER = {'arduino': {'storage': {'window': 1048576, 'offset': 1044480, 'length': 4096}}}

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.dir = Path(self.tmp.name)

    def write(self, ld=LD, syms=None):
        (self.dir / 'memmap_default.ld').write_text(ld)
        (self.dir / 'x.ino.elf').write_bytes(elf32_with_symbols(syms or {'_FS_start': 269479936,
                                                                          '_FS_end': 270528512}))

    def check(self, spans=((0x10000000, 0x10012000),), roster=ROSTER):
        return cb.record_check('raspberry_pi_pico', {'loadSpans': list(spans)}, self.dir, roster)

    def test_window_recorded_transport_relative(self):
        self.write()
        self.assertEqual(self.check(), ({'storage': {'fsStart': 0xff000, 'fsEnd': 0x1ff000}}, ''))

    def test_hex_values_parsed_whole(self):
        self.write(ld=self.LD.replace('1044480', '0xff000').replace('269479936', '0x100ff000'))
        self.assertEqual(cb.fs_window(self.dir), (0x100ff000, 0x100ff000, 270528512))

    def test_board_without_fs_adds_nothing(self):
        self.assertEqual(cb.record_check('metro_m4_express', {'loadSpans': []}, self.dir, {}), ({}, ''))

    def test_symbols_must_match_the_linker_script(self):
        self.write(syms={'_FS_start': 269479936, '_FS_end': 270528511})
        self.assertIn('disagree', self.check()[1])

    def test_empty_window_refused(self):
        self.write(ld=self.LD.replace('270528512', '269479936'), syms={'_FS_start': 269479936, '_FS_end': 269479936})
        self.assertIn('empty FS window', self.check()[1])

    def test_image_reaching_the_fs_refused(self):
        self.write()
        self.assertIn('reaches the FS', self.check(spans=[(0x10000000, 0x10100000)])[1])

    def test_roster_region_outside_the_window_refused(self):
        self.write()
        for storage in ({'window': 1048576, 'offset': 1044480, 'length': 8192},
                        {'window': 2097152, 'offset': 1044480, 'length': 4096}):
            with self.subTest(storage=storage):
                _, err = self.check(roster={'arduino': {'storage': storage}})
                self.assertIn('not a region inside the FS window', err)

    def test_window_past_the_board_span_refused(self):
        self.write(ld=self.LD.replace('269479936', str(0x10200000)).replace('270528512', str(0x10300000)),
                   syms={'_FS_start': 0x10200000, '_FS_end': 0x10300000})
        self.assertIn('not inside the board span', self.check()[1])

    def test_missing_linker_script_refused(self):
        self.assertIn('memmap_default.ld', self.check()[1])


class Rosters(unittest.TestCase):
    def test_every_roster_board_has_a_rig_profile(self):
        for name in ('ci.json', 'local.example.json'):
            boards = json.loads((ROOT / 'test/hil' / name).read_text())['boards']
            for b in boards:
                self.assertIn('span', cb.BOARDS[b['name']], f'{name}: {b["name"]}')


if __name__ == '__main__':
    unittest.main()
