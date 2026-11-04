#!/usr/bin/env python3
"""Build this library's example and HIL sketches with arduino-cli: the project's build contract (SKILL.md).

  check_build.py (--scope PATH... | --base REF [--worktree] | --diff-file FILE) [--example E]... [-j N]
  check_build.py --board B... [--example E]... [--shared] [--build-property K=V]...
  check_build.py --board B --shared --record FILE --hil-config ROSTER [--experimental --build-property K=V...]
  check_build.py --select-only (--scope ... | --base REF [--worktree] | --diff-file FILE) [--hil-config ROSTER]

stdout ends with one JSON line. Exit 0 pass, 1 a build or test failed, 2 usage or a missing
prerequisite ("error" names the remedy), 3 an uncovered path. 2 outranks 1, 1 outranks 3.

This file is the library's profile for agentrc's arduino_build.py (the `arduino` skill), loaded from the agentrc
commit test/hil/agentrc.json pins; the mechanics are there.
"""

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / '.claude' / 'skills' / 'hil-contract' / 'scripts'))
import agentrc_pin  # noqa: E402

LIBRARY_NAME = 'Adafruit SPIFlash'

# CI's matrix (.github/workflows/githubci.yml), FQBNs from adafruit/ci-arduino all_platforms.py, plus the rig boards
# named as in tinyusb's HIL farm. `filters` is the ci-arduino platform whose .test.only/.skip files apply.
BOARDS = {
    'cpx_ada': {'fqbn': 'adafruit:samd:adafruit_circuitplayground_m0', 'family': 'samd'},
    'feather_m0_express': {'fqbn': 'adafruit:samd:adafruit_feather_m0_express', 'family': 'samd'},
    'metro_m0': {'fqbn': 'adafruit:samd:adafruit_metro_m0', 'family': 'samd'},
    'feather_m4_express': {'fqbn': 'adafruit:samd:adafruit_feather_m4:speed=120', 'family': 'samd'},
    'metro_m4': {'fqbn': 'adafruit:samd:adafruit_metro_m4:speed=120', 'family': 'samd'},
    'grand_central': {'fqbn': 'adafruit:samd:adafruit_grandcentral_m4:speed=120', 'family': 'samd'},
    'neotrellis_m4': {'fqbn': 'adafruit:samd:adafruit_trellis_m4:speed=120', 'family': 'samd'},
    'pyportal': {'fqbn': 'adafruit:samd:adafruit_pyportal_m4:speed=120', 'family': 'samd'},
    'pyportal_titano': {'fqbn': 'adafruit:samd:adafruit_pyportal_m4_titano:speed=120', 'family': 'samd'},
    'matrixportal': {'fqbn': 'adafruit:samd:adafruit_matrixportal_m4:speed=120', 'family': 'samd'},
    'pybadge': {'fqbn': 'adafruit:samd:adafruit_pybadge_m4:speed=120', 'family': 'samd'},
    'metroesp32s2': {'fqbn': 'esp32:esp32:adafruit_metro_esp32s2', 'family': 'esp32'},
    'nrf52840': {'fqbn': 'adafruit:nrf52:feather52840:softdevice=s140v6,debug=l0', 'family': 'nrf'},
    'cpb': {'fqbn': 'adafruit:nrf52:cplaynrf52840:softdevice=s140v6,debug=l0', 'family': 'nrf'},
    'clue': {'fqbn': 'adafruit:nrf52:cluenrf52840:softdevice=s140v6,debug=l0', 'family': 'nrf'},
    'feather_rp2040': {'fqbn': 'rp2040:rp2040:adafruit_feather:freq=125,flash=8388608_0', 'family': 'rp2040'},
    'uno': {'fqbn': 'arduino:avr:uno', 'family': 'avr'},
    # rig boards: `span` is where a HIL image may load (Metro M4: above the UF2 bootloader, below the SAMD51J19's
    # 512 KiB; Pico: XIP base, 2 MiB). Each core's default USB stack: Serial is the DUT's own USB CDC.
    'metro_m4_express': {'fqbn': 'adafruit:samd:adafruit_metro_m4:speed=120,usbstack=arduino', 'family': 'samd',
                         'filters': 'metro_m4', 'span': (0x4000, 0x80000)},
    # `fs`: the SPIFlash RP2040 transport's window is the core's FS region, so the flash menu reserves 1 MiB of it
    'raspberry_pi_pico': {'fqbn': 'rp2040:rp2040:rpipico:flash=2097152_1048576,freq=125,dbgport=Disabled,'
                                  'dbglvl=None,usbstack=picosdk', 'family': 'rp2040', 'filters': 'feather_rp2040',
                          'span': (0x10000000, 0x10200000), 'fs': True},
}
# what a shared-code change builds: rig boards first, then each distinct transport or core (SAMD21 SPI beside the
# SAMD51 QSPI, nRF QSPI, ESP32 partition, AVR SPI)
REPRESENTATIVES = {
    'samd': ['metro_m4_express', 'metro_m0'],
    'rp2040': ['raspberry_pi_pico', 'feather_rp2040'],
    'nrf': ['nrf52840'],
    'esp32': ['metroesp32s2'],
    'avr': ['uno'],
}
TRANSPORT_FAMILY = {
    'src/qspi/Adafruit_FlashTransport_QSPI_SAMD.cpp': 'samd', 'src/qspi/Adafruit_FlashTransport_QSPI_NRF.cpp': 'nrf',
    'src/rp2040/': 'rp2040', 'src/esp32/': 'esp32',
}
# the sketches a HIL record carries per board role (hil_test.py's tests run them), the first also the restoration
# image: it neither erases nor writes the flash
HIL_SKETCHES = {'device': ('test/hil/spiflash_restore', 'test/hil/spiflash_region')}
CONTRACT_TESTS = {'build': '.claude/test/test_build.py', 'hil': '.claude/test/test_hil.py',
                  'hil_remote': '.claude/test/test_hil_remote.py'}
SMOKE_BOARDS = ('raspberry_pi_pico', 'metro_m4_express')   # one per image kind
NOTHING = ('.claude/', '.github/', '.codespell', '.codespellrc', 'tools/', '.agents', '.gitignore',
           '.pre-commit-config.yaml', '.coderabbit.yaml', '.PVS-Studio', 'platformio.ini', 'LICENSE')
EXTRA_DEPENDENCIES = ()
CONTRACT_FILES = ('.claude/skills/build/scripts/check_build.py', '.claude/skills/hil-contract/scripts/hil_test.py',
                  '.claude/skills/hil-contract/scripts/hil_remote.py',
                  '.claude/skills/hil-contract/scripts/agentrc_pin.py', 'test/hil/agentrc.json')
AGENTRC = None   # set by engine(): agentrc_pin.identity()
XIP_BASE = 0x10000000


def classify_source(path):
    """The library's own source rules (arduino_build.classify falls through to the common ones on None)."""
    for prefix, family in TRANSPORT_FAMILY.items():
        if path.startswith(prefix):
            return 'family', family, f'{family} flash transport'
    return None


def vid_pid(build_dir, board=None, roster_entry=None):
    """The VID:PID the roster commissioned for the board's DUT under this FQBN (`arduino.vid_pid`; the rp2040 core
    derives its PID at runtime from the interfaces a sketch registers, cores/rp2040/USB.cpp), checked on the device
    at every run. None without a roster entry."""
    value = ((roster_entry or {}).get('arduino') or {}).get('vid_pid')
    return value if isinstance(value, str) and re.fullmatch(r'[0-9a-f]{4}:[0-9a-f]{4}', value) else None


def fs_window(build_dir):
    """(flash length, FS start, FS end) the rp2040 core substituted into the build's linker script
    (platform.txt's prelink hook writes memmap_default.ld into the build path), or None."""
    try:
        text = (Path(build_dir) / 'memmap_default.ld').read_text()
    except OSError:
        return None
    flash = re.search(r'FLASH\(rx\)\s*:\s*ORIGIN\s*=\s*(0x[0-9a-fA-F]+),\s*LENGTH\s*=\s*(0x[0-9a-fA-F]+|\d+)', text)
    start = re.search(r'_FS_start\s*=\s*(0x[0-9a-fA-F]+|\d+)\s*\)', text)
    end = re.search(r'_FS_end\s*=\s*(0x[0-9a-fA-F]+|\d+)\s*\)', text)
    if not (flash and start and end):
        return None
    return int(flash.group(1), 0) + int(flash.group(2), 0), int(start.group(1), 0), int(end.group(1), 0)


def record_check(board, row, build_dir, roster_entry=None):
    """For an `fs` board: the FS window the linker script declares, the ELF's _FS_start/_FS_end, and every load span
    must agree (spans end at or below FS start, a nonempty window inside the board's span), and the roster's storage
    window must be the FS window's size with its test region inside. Returns ({'storage': ...} or {}, error or '')."""
    if not BOARDS[board].get('fs'):
        return {}, ''
    window = fs_window(build_dir)
    if not window:
        return None, 'memmap_default.ld in the build dir names no FLASH length and FS window'
    flash_end, fs_start, fs_end = window
    elf = next(Path(build_dir).glob('*.ino.elf'))
    syms = E.elf_symbols(elf, ('_FS_start', '_FS_end'))
    if (syms.get('_FS_start'), syms.get('_FS_end')) != (fs_start, fs_end):
        return None, f'the ELF\'s FS symbols {syms} disagree with the linker script [{fs_start:#x}, {fs_end:#x})'
    if fs_end <= fs_start:
        return None, f'empty FS window [{fs_start:#x}, {fs_end:#x}): the FQBN needs a flash menu with an FS'
    span = BOARDS[board]['span']
    if not span[0] <= fs_start < fs_end <= span[1]:
        return None, f'the FS window [{fs_start:#x}, {fs_end:#x}) is not inside the board span ' \
                     f'[{span[0]:#x}, {span[1]:#x})'
    if flash_end > fs_start or any(e > fs_start for _, e in row['loadSpans']):
        return None, f'the image ({row["loadSpans"]}, FLASH to {flash_end:#x}) reaches the FS at {fs_start:#x}'
    storage = ((roster_entry or {}).get('arduino') or {}).get('storage') or {}
    off, length = storage.get('offset'), storage.get('length')
    if roster_entry is not None and not (storage.get('window') == fs_end - fs_start and isinstance(off, int) and
                                         isinstance(length, int) and 0 <= off and
                                         0 < length <= (fs_end - fs_start) - off):
        return None, f'the roster storage {storage} is not a region inside the FS window of {fs_end - fs_start} bytes'
    return {'storage': {'fsStart': fs_start - XIP_BASE, 'fsEnd': fs_end - XIP_BASE}}, ''


E = None   # agentrc's arduino_build, bound to this profile by engine()


def engine():
    """agentrc's arduino_build bound to this profile."""
    global AGENTRC, E
    if E is None:
        AGENTRC = agentrc_pin.identity()
        E = agentrc_pin.load('skills/arduino/scripts/arduino_build.py').bind(sys.modules[__name__])
    return E


def main(argv=None):
    try:
        eng = engine()
    except agentrc_pin.PinError as e:
        print(json.dumps({'version': 2, 'pass': False, 'error': str(e), 'remedy': e.remedy}))
        return 2
    return eng.main(argv)


if __name__ == '__main__':
    sys.modules.setdefault('check_build', sys.modules[__name__])
    sys.exit(main())
