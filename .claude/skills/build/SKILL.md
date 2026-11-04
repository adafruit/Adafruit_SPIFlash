---
name: build
description: The Adafruit SPIFlash build contract. Use when a change must be verified by building, when an agent needs the boards and sketches a diff affects, when a HIL run or debug session needs firmware and its build record, or when a workflow asks for "the project's build". Resolves scope to boards, compiles the examples and HIL sketches with arduino-cli against this checkout, reports pass/fail with the first error.
---

# Build contract

`scripts/check_build.py` is the entry point for verifying a change; agents and workflows call it rather than composing
`arduino-cli compile` lines. It is this library's profile for agentrc's `arduino` skill (`arduino_build.py`), loaded
from the installed agentrc (`${AGENTRC_DIR:-$HOME/.claude}`), never fetched; CI fetches the commit
`test/hil/agentrc.json` pins. `.claude/skills/hil-contract/scripts/agentrc_pin.py check` names the one in use. A record
binds that agentrc's commit and file digests: after its files change, build again. That skill's page is the shared
behaviour (compiles, dependencies, the record); this page is what is this library's.

```bash
B=.claude/skills/build/scripts/check_build.py
python3 $B --scope <changed paths or dirs>          # boards and sketches a change affects
python3 $B --base origin/master                      # same, from the branch diff
python3 $B --base origin/master --worktree           # same, plus uncommitted and untracked changes
python3 $B --select-only (--scope ... | --base REF [--worktree] | --diff-file FILE) [--hil-config ROSTER]
python3 $B --board metro_m4 [--board ...] [--example examples/flash_info ...]
python3 $B --board raspberry_pi_pico --shared --record build/hil/raspberry_pi_pico.build.json --hil-config test/hil/ci.json
```

The last stdout line is JSON: `pass`; `firstError`, and `error` on exit 2 naming the cause or remedy; `boards[]` (one
row per board and sketch: `status` `ok`, `failed` or `error`, `log`, `artifacts`, `platform`, `foreignHeaders`,
`libraries`, `removed`); `tests[]`; `paths[]` (each changed path's `rule` and `reason`); `uncovered`; `dependencies`;
`nothingToBuild`; `warnings`; `input` (HEAD, dirty); `narrowed`; `record` when one was written. Exit 0 pass, 1 a build,
test or record check failed, 2 usage or a missing prerequisite, 3 an uncovered path; 2 outranks 1, and 1 outranks 3.
Each build's log is `<buildDir>/check_build.log`.

`--select-only` prints the selection and stops before any compile. With `--hil-config <the host's roster>` it adds
`hil.{boards,full,uncovered}`: the roster boards the change puts under the HIL battery, and each family the change
reaches with no roster board on this host.

## Reporting

An agent that reports `buildOk` and `notes` (agentrc's `code-writer`) maps the JSON this way: `buildOk` is `pass`. A
pass with no build or test run is "nothing to build": say so in `notes`. Exit 3 is `buildOk` false with each
`uncovered` reason quoted verbatim. Exit 1 or 2 quotes `firstError` (and `error`). Each `warnings` entry is quoted in
`notes`.

## Judgment

- **Selection is the script's**, first match per path; `paths[].reason` says why. A flash transport builds its family's
  representatives (`TRANSPORT_FAMILY`, `REPRESENTATIVES`), any other `src/` file and `library.properties` every
  family's; an example builds where ci-arduino's filters allow; a HIL sketch builds on the rig boards and selects the
  battery on every roster board; anything unmatched is uncovered.
- **Boards** (`BOARDS`): CI's matrix with ci-arduino's FQBNs, plus the rig boards named as in tinyusb's HIL farm, each
  on its core's default USB stack so `Serial` is the DUT's own USB CDC. `raspberry_pi_pico` uses
  `flash=2097152_1048576`: the RP2040 transport's window is the core's FS region, and CI's `flash=8388608_0` has
  none.
- **Dependencies**: as agentrc's `arduino` skill says; on samd the core's SPI pulls in `Adafruit_ZeroDMA`, so a
  user-installed copy is recorded as shadowing the core's.
- **A HIL record** (`--board B --shared --record FILE --hil-config ROSTER`, by convention
  `build/hil/<board>.build.json`) is the HIL contract's build receipt (agentrc `arduino` skill). This library's
  `record_check` adds the DUT's commissioned `arduino.vid_pid` (the rp2040 core derives its PID at run time) and, on
  an FS board, the FS window the linker script, the ELF and the board span agree on, holding the roster's region;
  the record keeps it as `storage`.
- **Private dirs for parallel agents.** Each build uses `build/agent-<pid>/<board>/<sketch>`, removed when it passes;
  `--shared` builds into `build/hil/<board>/<sketch>` under `build/hil/.lock` and keeps it for flashing.
- **Printing a command is not verification.** A verdict is the JSON of a run that happened; an agent that cannot run
  the script reports the build as not run.
