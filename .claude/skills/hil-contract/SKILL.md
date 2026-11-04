---
name: hil-contract
description: The Adafruit SPIFlash HIL contract, under agentrc's `hil` rules. Use when running the SPIFlash HIL test on rig boards (Metro M4 Express QSPI flash, Raspberry Pi Pico internal flash), when a HIL run fails, hangs or reports a board locked, when reading its report, or for a manual session (flash, GDB, console, a check, a command) on a rig board from hw-validator or hw-debugger.
---

# HIL contract

Read agentrc's `hil` skill first; this page supplies its contract sections. The mechanics are agentrc's `arduino`
skill (`arduino_hil.py`, `arduino_remote.py`), shared with Adafruit TinyUSB Arduino, whose HIL contract has the same
commands, budgets, session rules and recovery; this page names what is this library's. Manual runs only: no CI job
runs this test. Scripts, all in `scripts/`, each with its usage in its docstring:

- `hil_test.py`: the harness on the host with the boards (this library's profile and tests).
- `hil_remote.py`: drives the harness on ci.lan from the dev PC that built the firmware.
- `agentrc_pin.py`: which agentrc both load their shared scripts from: the installed one
  (`${AGENTRC_DIR:-$HOME/.claude}`) on a dev PC, never fetched; in CI the commit `test/hil/agentrc.json` pins; on the
  rig, the copy `hil_remote.py` staged.

```bash
H=.claude/skills/hil-contract/scripts
python3 $H/agentrc_pin.py check            # the agentrc a run would load, validated; its root
```

## Hosts/rosters

| Host     | How to tell                        | Roster                                                     |
|----------|------------------------------------|------------------------------------------------------------|
| ci.lan   | the HIL farm; reached over ssh     | `test/hil/ci.json` (tracked)                               |
| a dev PC | a board on this PC's USB           | `test/hil/local.json` (ignored; copy `local.example.json`) |

ci.lan has no arduino-cli and no checkout of this library: firmware is built on the dev PC and staged by
`hil_remote.py`. On a dev PC without `test/hil/local.json`, ask the user for one; an agent that cannot ask returns
each requested board not-run naming the missing file.

A roster entry is the `arduino` skill's (`name`, the board's name in agentrc's board registry and its lock; `uid`, the
DUT's USB serial; `usb_port`; `probe`, its debug probe's serial, whose registry entry supplies the flasher, or a dev
PC's unregistered board's own `flasher`; `debug`) plus this library's `arduino` section:

- `tests`: `["spiflash_region"]`.
- `vid_pid`: the DUT's VID:PID under this FQBN, commissioned (Metro M4 `239a:8020`, Pico `2e8a:000a`).
- `storage`: `transport` (`qspi`, `rp2040`), `jedec` (the id `INFO` reports), `window` (the transport's size in
  bytes: the whole chip on QSPI, the core's FS region on the RP2040), and the test region `offset`/`length`, whole
  4096-byte sectors inside the window.

The regions are disposable by the maintainer's decision (2026-11-03): on the Metro M4 the top 64 KiB of its external
QSPI flash (the region is its last 4 KiB sector, 0x1ff000), on the Pico its FS region (the last 4 KiB sector of the
1 MiB window, 0xff000). Never widen a region or add a board's storage without that decision for the board.

ci.lan's boards are also on tinyusb's roster, tinyuf2's and Adafruit TinyUSB Arduino's. This contract performs no
rig-wide operation. **Farm check**: tinyusb's launcher, `ssh ci.lan 'cd ~/code/tinyusb && python3
test/hil/helper/hil_farm_check.py -b <board>...'`.

## Selection

A fixed test per board, the roster's `arduino.tests`:

- `spiflash_region` (`test/hil/spiflash_region/`): over the DUT's USB CDC, `INFO` must report the roster's
  transport, JEDEC id, window and 4096-byte sector; `RUN` erases the region, checks it erased, writes a pattern seeded
  by the run's nonce (on QSPI split at byte 300: an unaligned, page-crossing write; on the RP2040 whole pages, which
  is all its SDK programs) and reads it back; `CHECK` sees that pattern; six malformed, misaligned, empty and
  out-of-window requests must each be refused before any flash operation, and a line longer than the sketch's
  buffer, ending in a `RUN`, must be dropped whole; a `CHECK` after them must still see the pattern (proof none
  wrote or erased); `ERASE` then leaves the region erased.
- the restoration check (`spiflash_restore`, `test/hil/spiflash_restore/`, a sketch with no erase or write command;
  `flash.begin()` may still set a QSPI chip's quad-enable status bit): `INFO` matches and the region reads back
  erased.

Each reply line carries its command's nonce and must name the operation and region asked; a missing, stale or other
reply fails the test at its deadline. `-b`
narrows the boards. The PR-scoped selector is the build contract's:

```bash
python3 .claude/skills/build/scripts/check_build.py --select-only --base origin/master --hil-config test/hil/ci.json
```

Its `hil.boards` are the roster boards to run, `hil.full` means the harness, a HIL sketch or a roster changed (every
board), and `hil.uncovered` names each family the change reaches with no board on that roster: report those
uncovered, never pass.

## Build

Each run board needs a clean HIL record at HEAD (build skill, "A HIL record"):

```bash
python3 .claude/skills/build/scripts/check_build.py --board <board> --shared --record build/hil/<board>.build.json \
  --hil-config <roster>
```

The harness admits a record only as the `arduino` skill's The HIL build record says. A manual flash or GDB session
also takes an experimental record (`--experimental`).

## Execution

Budgets, the remote run and the manual session commands are Adafruit TinyUSB Arduino's HIL contract's, with this
library's names: the attempt dir is `~/hil-remote/adafruit-spiflash/<id>`, `check --test` takes `spiflash_region`
or `spiflash_restore` (default), `session close` restores `spiflash_restore`.

```bash
python3 $H/hil_remote.py run --run-id <id> [-b BOARD]...
python3 $H/hil_remote.py wait <id> --timeout 570    # repeat while it exits 3
python3 $H/hil_test.py report .hil-remote/<id>/hil-run
python3 $H/hil_remote.py session open --run-id <id> -b <board>     # then session exec/stage/close, as TinyUSB's
```

On the dev PC with the board: `python3 $H/hil_test.py run [-b BOARD]... test/hil/local.json`.

## Reservation

As the `arduino` skill's Locks: the board lock `/tmp/tinyusb-hil-locks/<name>.lock` and the pending kind
`arduino-hil` are shared with tinyusb, tinyuf2 and Adafruit TinyUSB Arduino. `hil_test.py run` and `commission` lock
each board themselves; a manual session holds it under a reason unique to its attempt, `arduino-hil session
<attempt dir> <token>`. Cleanup state is `spiflash_restore` flashed, the region read back erased, every child proven
ended, and the hold released.

## Results

`hil_test.py run` writes `hil_report.json` (schema `adafruit-spiflash-hil`) with the same rows, exits, verdicts and
delegated output (`hil_test.py report`) as Adafruit TinyUSB Arduino's contract. The caller retries a fail once under a
new run id, per the `hil` Caller rules.

## Recovery

Adafruit TinyUSB Arduino's Recovery, run in this library's attempt dir with its roster and records. A board left
with the region in an unknown state is not data loss: the region is disposable; its next `spiflash_region` erases
it.

## Limits

- **Coverage:** the SAMD51 QSPI transport on the Metro M4's external flash and the RP2040 transport on the Pico's FS
  region, through the raw API (erase sector, read, write). Not covered on hardware: the nRF52840 QSPI, ESP32 and SPI
  transports, the RP2350, `Adafruit_FlashCache` and the SdFat block device, and every example; those build only.
- **Commissioning** (2026-11-03, sessions `spif-pico-disc1`, `spif-m4-disc1`): each DUT enumerated anew after a flash
  with the roster's VID:PID and serial, and `INFO` reported Metro M4 `c84015` with a 2 MiB window, Pico `ef4015`
  with the 1 MiB FS window. `hil_test.py commission` repeats that check from a checkout on the board's host.
- **Missing documentation:** the flash parts' datasheets (GD25Q16, `c84015`; W25Q16JV, `ef4015`) are not in the
  reference library, so the 4 KiB sector-erase unit rests on the library's `SFLASH_SECTOR_SIZE` and the run's
  readback, not a datasheet. The Metro M4 region is the chip's top sector, inside the top 64 KiB the maintainer
  authorized whatever the erase unit.
- **RP2040:** a probe reset leaves the USB controller attached, so before each flash and console reset the harness
  clears `SIE_CTRL.PULLUP_EN` through the probe (agentrc's `arduino` skill, "Probe detach").
