# AGENTS.md — fm1-base

Guidance for AI agents (and humans) working in this repo. Read this first, then
`README.md`, `DESIGN.md`, `LICENSING.md` + `CREDITS.md`, `include/plat.h`, and the
docstring at the top of `tools/build.py` — they are the source of truth, this file
is the rules. Do not invent APIs, build flags, or behaviour; verify against the
code above.

## What this repo is

A base for M-VAVE FM-1 firmware in C, on [felucca-bsp](https://github.com/hugelton/felucca-bsp)
(`bsp/`, a pinned git submodule): the platform layer above the hardware (screen,
drawing/fonts, panel/keys/LEDs, audio, MIDI over USB/TRS/Bluetooth, storage, the
screen reader), a host simulator, a browser emulator, and the tools to build,
package, install, and publish a firmware. Status: beta — `include/plat.h` and
`fm1.toml` settings will change before 1.0.

Everything here is MPL-2.0, file by file (`LICENSE`): an app built on fm1-base may
use any licence. See `LICENSING.md` for where each piece came from and what may
not be added.

## Layout

| Path | What |
| --- | --- |
| `include/plat.h` | The platform interface — the only thing an app sees. Also `fastmath.h`, `midi_rel.h`, `fm1b_config.h`. |
| `device/` | Device implementation: `plat_fm1.c`, `main_fm1.c`, `unity.c`, `panel.c`, `lcd.c`, `gfx.c`, `storage.c`, `midi_uart.c`, `announce.c`, `radio.c`, `bt_own*.c`, storage/linker scripts (`app.ld`, `app_packed.ld`, `app_wifi.ld`), etc. |
| `host/` | `plat_host.c`: the scripted simulator half of the interface; `rfm1.c` (WASM host-app half). |
| `web/` | Browser installer (`installer.html`, `fm1ota.js`, `fm1pkg.js`, `fm1ports.js`), `emu/` (in-browser emulator), `backup/`, `mirror/`. |
| `tools/` | `build.py` (one build for every firmware) and the `fm1_*.py` install/remote/backup/mirror/debug tools, `gen_font.py`, `gen_words.py`, `make_site.py`, `provenance.py`. |
| `lib/` | App libraries: `touch.h/.c` (velocity/pressure), `btdev.h/.c` (Bluetooth MIDI devices page). |
| `ble/` | Bluetooth/WiFi on JieLi's AC79 SDK libraries: the shim and MIDI service. |
| `bt/` | fm1-base's own Bluetooth stack (BLE MIDI, pairing, central, A2DP audio; from Optimist, dual GPL-3.0/MPL-2.0, used here MPL-2.0). |
| `template/` | The smallest complete firmware — **copy this to start a new firmware**. `app.h`, `app.c`, `fm1.toml`, `words.tsv`, `tests/`, `docs/MANUAL.md`. |
| `examples/{ble_own,wifi}/` | Single-issue examples: the template on the own Bluetooth stack with every option; the WiFi + lwIP shape. |
| `tests/` | Host tests (`run_tests.sh`), incl. `*_test.c`, `*_test.py`, `*_test.mjs`, scripted scenarios (`template/tests/*.txt`). |
| `skill/felucca-to-fm1-base/` | Claude skill (also plain instructions) for moving a Felucca-based firmware onto fm1-base. See "Porting" below. |
| `assets/fonts/`, `docs/`, `LICENSES/` | Fonts (SIL OFL 1.1), `docs/bt/`, `docs/FIRMWARE-JSON.md`, third-party licence texts. |

## The contract

- The app sees **only `plat.h`**. `device/` (hardware) and `host/` (simulator)
  implement it, so the same app, unchanged, runs on the FM-1, in the host
  simulator (tests, screenshots), and in the browser. Any new `plat_*` function
  needs both implementations (plus an emu stub where relevant) and a test.
- The platform calls the app: `app_init()`, `app_ui_init()`, `app_frame()`
  (~60/s), `app_input_only()`, `app_render(lr, n, master_q12)` (audio interrupt),
  `app_tick()` (1 kHz, optional), `app_say(a, b)` (screen reader), `app_panic()`
  (silence before flash is rewritten). See `template/app.h` for the full list.
- The app declares its settings in `app.h`: `APP_NAME`, `APP_BLOCK`, `APP_SPLASH_RGB`,
  optional `#define APP_*` toggles (`APP_TOUCH`, `APP_BTDEV`, `APP_BLE_AT_BOOT`,
  `APP_CANVAS_PX`, `APP_BACKUP_*`, `APP_USB_CMD`, …), and the store: `OBJ_*` enum +
  `APP_STORE_MAP` flash offsets. Keep a firmware's `id_prefix` and store offsets
  stable across updates so players' saved data survives.
- The build declares its settings in `fm1.toml` (`name`, `id_prefix`, USB names,
  `o2`, `words`, and the optional parts: `ble`, `ble_stack`, `ble_central`,
  `ble_pairing`, `bt_audio`, `wifi`, `net`, `usb_audio_in/out`, `mirror`, `backup`,
  `cpu2`, `ram_layout`, …). A firmware has only the parts its `fm1.toml` asks for;
  every build prints the parts it has. The full part table is in `README.md`.

## BLE vs WiFi (mutually exclusive)

A firmware has Bluetooth **or** WiFi, never both — each fills most of what the FM-1
has (no external RAM).

| | Bluetooth | WiFi |
| --- | --- | --- |
| `fm1.toml` | `ble = true`; own stack: `ble_stack = "own"` (+ `ble_pairing`, `ble_central`, `bt_audio`, `ble_midi_time`) | `wifi = true` (not with `ble`), `net = [...]`, `usb_audio_in = false`, `usb_audio_out = false` |
| `app.h` | `OBJ_BLE` (+ offsets); own stack may use `APP_BLE_STORE` + `app_ble_load/save` instead | `OBJ_BLE` + `OBJ_WIFI`; smaller canvas: `#define APP_CANVAS_PX (240u * 48u)` (draw in ≤48-row strips) |
| App code | `TO_BLE`/`PORT_BLE`, `plat_ble_*`, `plat_ble_enable`; central: `plat_ble_scan…`; devices page: `APP_BTDEV` + `lib/btdev.h` | net files (listed in `net`, include `lwip/sockets.h`) run in their own task via `plat_net_task` — a socket call waits, the main loop must not; `plat_wifi_*` (passwords arrive over USB: `tools/fm1_remote.py wifi join SSID PASSWORD`) |
| Cost (measured, `README.md`) | JieLi: +124 KB flash / +45 RAM / +24 pool. Own stack: +41–46 / +10 / +1; with pairing+central+audio +57–62 / +8 / +30 | ≈ +300 flash / +100 RAM / +276 pool; leaves an app ~170 KB flash. In a WiFi build `#define APP_BIG_FONT FONT_M` saves 20 KB flash |
| SDK | JieLi stack: AC79 SDK binary libs (copied into `build/`, never committed). Own stack: links nothing of JieLi's — the build checks it | JieLi WiFi libs + lwIP 2.1.3 sources from the SDK; never committed |

Bluetooth on JieLi's stack is the default (`ble/` shim); `ble_stack = "own"`
(`bt/`, from Optimist, MPL-2.0) is smaller, does more (pairing, central, A2DP),
but one BLE MIDI link at a time and its own bond keys. Details: `README.md`
"Bluetooth and WiFi", `docs/bt/README.md`. Example firmwares: `examples/ble_own`,
`examples/wifi`.

Budgets, all modes: 581 KB flash slot; 96 KB RAM for `.data`/`.bss` (128 KB with
WiFi); 336 KB `.pool` (304 KB with WiFi) — or one 464 KB packed SRAM from the
bottom with `ram_layout = "packed"`. Respect `pool_spare` (8 KB of the pool/SRAM).
An app's own fonts can replace fm1-base's via `APP_SYS_TEXT` (saves ~18 KB).

## Build, test, install

From the fm1-base directory (the JieLi toolchain + AC79 SDK via
`tools/get_toolchain.sh`, `AC79_SDK` env or `$HOME/fw-AC79_AIoT_SDK`; `emcc`
for `--web`/`--rfm1`; `node` for the JS tests):

```sh
tools/build.py template                     # template/build/template.fwsc
tools/build.py template --host              # then: template/build/host/template_host template/tests/basic.txt template/build/host
tools/build.py template --web               # template/build/emu/ (browser emulator: panel, screen, sound, screen reader)
tools/build.py template --rfm1              # .rfm1 for host apps (RemoteFM1); --release X.Y beside the .fwsc
tools/build.py template --release 0.1       # build/NAME-0.1.fwsc, identity FM-1_11XXYYZZ; runs the release checks
tools/build.py template --no-ble            # same firmware without Bluetooth (NAME-usb.fwsc)
python3 tools/fm1_install.py template/build/template.fwsc
tests/run_tests.sh                          # logs: template/build/tests/*.log (needs template built first for some tests)
```

- On-device debugging: `tools/fm1_remote.py` drives an FM-1 like a player
  (buttons, keys, encoders, MASTER, screen text, crash record, radio log, WiFi).
  Before installing anything on a real FM-1, say what it overwrites and back up
  (firmwares share the settings sectors). A successful build/install/test is
  only claimed when it was actually run and observed.
- The build enforces, and will fail on: `double`, hardware/MMIO access outside
  BSP + `device/`, 64-bit multiplies inlined beside a load, calls into the chip
  ROM, RAM-code (`PLAT_RAM_CODE`) calling XIP, and static stack depth over its
  budget. Fix the code — add a skip to `fm1.toml` (`mmio_skip`, `rom_check_skip`,
  `ram_code_allow`, …) only when the app genuinely vouches for it, and say why.
- Unity build: the platform + app are built as one unit, at `-Os` (or `-Oz` with
  `unity_opt = "z"`) except units listed in `o2 = [...]`, built at `-O2`.

## Porting (skill/felucca-to-fm1-base)

`skill/felucca-to-fm1-base/SKILL.md` (+ `PORTING.md`, `RULES.md`) is the workflow
for moving a Felucca-based (GPL-3.0) firmware onto felucca-bsp + fm1-base. Use it
whenever asked to port/move a Felucca fork, relicense to MPL-2.0, or check whether
a firmware still contains Felucca code. Key points, already in the skill:

- Inventory first (platform vs app vs library vs third-party), show the user the
  table, then port. **The move does not change the firmware's licence** — MPL-2.0
  files may sit inside a GPL work, file by file. Relicensing to MPL-2.0 is a
  separate, optional step, needed only to link JieLi's Bluetooth/WiFi libs, and
  only the code's authors can agree to it, on the record. Never assume it.
- Keep the firmware's `id_prefix` (package identity `FM-1_`+digits), USB names,
  `APP_STORE_MAP` offsets, published screen-reader words, and any external data
  format, so the port installs over the old firmware as an update.
- Prove the port: byte-for-byte comparison of the old simulator's scenarios
  against fm1-base's (screens, audio, MIDI, words, saved files), then a real FM-1
  (backup first). Start from `template/`, never by copying Felucca files.
- To fetch the skill into a firmware repo:
  `npx degit --force charlesvestal/fm1-base/skill/felucca-to-fm1-base .claude/skills/felucca-to-fm1-base`.
  The skill is also readable as plain instructions without Claude.

## Rules

- **Licensing/provenance** (`LICENSING.md`, `CREDITS.md`, `tools/provenance.py`):
  every source file carries `SPDX-License-Identifier: MPL-2.0` and the
  `Copyright (C) 2026 …` line. Never commit JieLi SDK binaries (`$AC79_SDK`
  files are copied into `build/` at build time only), fonts (SIL OFL), or
  third-party sources under their own licences (SBC Apache-2.0, micro-ecc
  BSD-2-Clause) without keeping their headers and listing them. No GPL code and
  no other Felucca contributors' code; check provenance of anything imported.
  Changes to fm1-base files are published under MPL-2.0; an app's own files are
  the app's choice.
- **`bsp/` is a submodule** (felucca-bsp): do not edit it casually; bump with
  `git submodule update`. A fresh clone needs `--recursive` or the submodule
  stays empty (observed in this checkout).
- **New firmware:** copy `template/`, then set `APP_NAME` + store objects in
  `app.h`; `name`, an own `id_prefix`, USB names, tagline, source, licence,
  credits, author, description, tags, install page, and a square ≥240×240
  screenshot in `fm1.toml`; the `# url:` line in `words.tsv`; rewrite
  `docs/MANUAL.md`, `site/landing.md`, and the tests. The build writes
  `firmware.json` (format: `docs/FIRMWARE-JSON.md`) — commit it at the repo root;
  a release build refuses until the required fields are set.
- **Sizes:** report flash/RAM/pool deltas for a part like `README.md` does, and
  keep `firmware.json` and `.rfm1` metadata honest (`source_url` at the commit
  actually built — commit and push what you build).
- **Style:** plain-sentence, US-spelling; comments explain *why*, not what; C99,
  `-Wall -Wextra -Werror` clean; platform names in `plat.h`/`B_*`/`EN_*`/`PORT_*`
  stay stable so moving a firmware is a change of include paths, not of code.
  Match the surrounding code's structure, naming, and density; keep changes
  small and focused.
- **Verification:** run the relevant checks instead of asserting. Distinguish
  facts you verified (code, doc, command output) from assumptions, and say when
  evidence is incomplete or conflicting.

## Where to add things

- New platform function: `include/plat.h` + `device/plat_fm1.c` + `host/plat_host.c`
  (+ emu) + a host test in `tests/` registered in `tests/run_tests.sh`.
- New optional part: an `fm1.toml` key + `tools/build.py` + `device/` code +
  an entry in `README.md`'s optional-parts table + a test.
- New tool or web page: `tools/` / `web/` + a `tests/*_test.{c,py,mjs}` + an entry
  in `tests/run_tests.sh`.
- New screen-reader text: `words.tsv` (per firmware) / `tools/gen_words.py` +
  `device/announce.c` (fm1-base holds the one copy; its protocol is fm1-reader's).
- New app: copy `template/`, move `.github/workflows/release.yml` to the repo
  root and adapt the `# <- yours` lines. Signed releases come from the shared
  builder (`zvenson/fm1-build` at `v1`), which runs `./build.sh --release X.Y`
  and `tests/run_tests.sh` (a failing test stops the release).
