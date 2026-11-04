# Adafruit SPIFlash Agent Instructions

## Working Rules

- An Arduino library with no standalone build: sketches compile against this checkout through the build contract.
  CI's board matrix is in `.github/workflows/githubci.yml`, its FQBNs in adafruit/ci-arduino's `all_platforms.py`;
  CI installs the dependencies from `library.properties` `depends=`.
- Code references — the flash API: `src/Adafruit_SPIFlashBase.*` (raw access: JEDEC id, sectors, pages),
  `src/Adafruit_SPIFlash.*` (SdFat block device), `src/Adafruit_FlashCache.*`; the device table:
  `src/flash_devices.h`; transports implementing `src/Adafruit_FlashTransport.h`: `src/spi/` (any core),
  `src/qspi/` (SAMD51, nRF52840), `src/rp2040/` (the RP2040/RP2350 core's FS region of the program flash),
  `src/esp32/` (the first FAT partition of the program flash); examples: `examples/<sketch>/`, each with a copy of
  `flash_config.h` choosing the transport.

## Code Rules

- `.pre-commit-config.yaml` is authoritative: clang-format v15 on C/C++ (never `examples/SdFat_format` or `tools`),
  codespell with `-w`.
- The internal-flash transports (`rp2040`, `esp32`) share the chip with the running program: their window is what
  the core reserves (the RP2040 core's Flash Size menu, the ESP32 partition table), never an address picked here.

## Skills

- Agents, skills and workflows beyond this repo's `.claude/` come from [agentrc](https://github.com/hathach/agentrc);
  install it before agent work on Adafruit SPIFlash. Its `arduino` skill holds the mechanics both contracts below
  run, shared with Adafruit TinyUSB Arduino.
- Skill script tests live in `.claude/test/test_*.py`; pre-commit runs them against the installed agentrc
  (`${AGENTRC_DIR:-$HOME/.claude}`); CI fetches the commit `test/hil/agentrc.json` pins instead.
- Hardware diagnosis and repair use agentrc's `hw-debugger`; independent hardware claims and committed-fix validation
  use `hw-validator`; routine HIL runs use `hil-operator` — all orchestrated by `chief`.

## Build and Validate

- Build contract: `.claude/skills/build/SKILL.md`. It resolves a change to boards and sketches and builds them with
  arduino-cli against this checkout; `--shared --record` writes the HIL build record and keeps the images HIL flashes.
- HIL contract: `.claude/skills/hil-contract/SKILL.md`. It owns taking and releasing a rig board, names the roster for
  the host you are on, runs the flash test on a dev PC's board or the ci.lan farm, and gives hw-validator and
  hw-debugger their manual sessions. Manual runs only: no CI job runs it. Hardware validation comes from its report,
  never from a build; what no roster board covers is reported pending.
- Before submitting, and again after a rebase: `pre-commit run --all-files`.
- For code changes: `python3 .claude/skills/build/scripts/check_build.py --base origin/master --worktree`.

## PRs and Follow-ups

- Before opening or updating a PR, follow Build and Validate; CI does not run HIL.
- A non-draft PR opened on the human's request, unless they declined babysitting, or their request or yes to babysit
  one, authorizes one headless `chief` to drive its reviews and CI to green: push commits to its branch, reply to and
  resolve its review threads through `pr-babysit` with `autoPush: true` and `markSonar: true`. Launch it in the PR
  worktree through agentrc's `headless-chief` skill (`chief_run.py`), never a bare `claude -p`, and relay its exit as
  that skill directs. Merging needs the human's own words asking to merge that PR. `pr-babysit` args:
  `{"pr": <num>, "protected": "^test/hil/[^/]+\\.json$"}`; reviewers are agentrc's default (`protected` keeps the HIL
  rosters and the agentrc pin out of automated fixes).
