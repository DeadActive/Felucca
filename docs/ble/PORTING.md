# Porting Bluetooth to another FM-1 firmware

BLE MIDI: 2026 DEADACTIVE.

This repository (DeadActive/Felucca, `FELUCCA_BLE=1`) is the **reference**: its Bluetooth is the newest, and every
other firmware takes it from here. The rules below come from three ports (Hortator, Skiff, SLOOP) of the same code:
what is copied unchanged, what each firmware must adapt, and what stops a port.

## 1. The five layers

| layer | files (here) | in another firmware |
| --- | --- | --- |
| 1. BLE unit | `firmware/src/ble/*`, `firmware/hal/fm1_ble_hal.h`, `fm1_ctx.S/.h`, `tools/ble_libs.py` | **copied unchanged**. Talks to the core only through `ble/ble_core.h` |
| 2. Core glue | `firmware/src/core_ble_api.c` | one-liners over the firmware's own globals; rewrite each |
| 3. Features | `bt_set.c` (settings), `bt_devs.c` (saved devices, own address, the RF copy), `bt_midi.c` (BLE-MIDI in/out, clock), `bt_fml.c` + `fmlink.c` (FM-Link), `smc.c` (SMC-Mixer layout) | adapt to the firmware's storage, MIDI input path, sequencer and parameters |
| 4. UI | `bt_page.c` (MENU > SYSTEM > BLUETOOTH), the MENU row (`menu_items.c`, `ui_menu.c`), the top-bar icon (`ui_draw.c`) | adapt to the firmware's menu model, canvas (`cv_*`), fonts, palette |
| 5. Build and gates | `tools/build.py` (the `FELUCCA_BLE` link), `firmware/app_ble.ld`, `tools/ble_base.txt`, `tools/check_update_path.py`, `tools/check_ble_boot.py`, `tools/check_default_build.sh`, `tests/run_tests.sh` | re-pin to the firmware's base; regenerate the linker script from its `app.ld` |

## 2. Rules (every port)

1. **The plain build does not change.** Everything is under `#if FELUCCA_BLE`. Gate 5
   (`tools/check_default_build.sh`): the plain build is byte-identical with the Bluetooth files removed. Build the
   plain firmware before the port and `cmp` its `.bin` after.
2. **Measure first.** Build the firmware as it is and record: image size and slot free, `.data + .bss`, pool, RAM code.
   The Bluetooth build costs about **141 KB of flash** (stack 128.7 KB, core side 5.8 KB, device role 6.3 KB),
   **12.3 KB of `.data + .bss`** and **23.7 KB of pool** (the heap, `BLE_HEAP_BYTES` 23,552 B, and four task slots).
   The build keeps 4 KB of `.data + .bss` and 8 KB of pool spare.
3. **What the Bluetooth build leaves out is the user's decision**, and only in that build: built-in sample sets
   (`BLE_SMP_DROP` in `build.py`, `gen_samples.py`), the second core (`FELUCCA_CPU2=0`, about 6 KB of RAM), optional
   UI assets. Bring the measured numbers and a recommendation; never cut an engine or a feature without asking.
4. **The radio calibration must survive.** The radio uses the stock firmware's stored trim (the JieLi config store's
   item 187, 68 bytes, somewhere in flash 0xE5000..0xFBFFF; `ble/ble_vm.c`). Running the trim instead hangs. Any
   firmware that writes there (Felucca's autosave 0xE5000, Skiff's pattern bank 0xE7000, SLOOP's USR4 0xE7000..)
   can erase it. The BLE unit therefore hands the record to the firmware once at each power-on and at each start
   (`core_rf_keep_put`) and uses the kept copy when the store has lost it (`core_rf_keep_get`). Every port implements
   these two hooks over **72 bytes of its own settings**.
5. **Settings storage needs about 300 bytes**, never by growing an existing record (an older firmware would then
   refuse it and lose every setting): MODE, AUTOSTART, ROLE, RECONNECT (one byte + check), 8 saved devices (8 x 24
   B), this FM-1's address (8 B), the RF copy (72 B). Use spare bytes the firmware provably never uses (Felucca: the
   favourites table's rows past each engine's presets, checked by a test against `ENGINES[e]->npresets`) or a new
   storage object in a free flash sector, two copies, its own magic and check.
6. **Nothing Bluetooth starts at boot** except through the user (BLUETOOTH ON, or AUTOSTART after a clean boot):
   gate 3 (`tools/check_ble_boot.py`). A flash scan (the RF copy) is not a start.
7. **The update path is untouched.** Gates 1, 2a-2c (`tools/check_update_path.py`, `tools/ble_base.txt`): the loader,
   the update path's sources and machine code equal the firmware's own base release. Re-pin `ble_base.txt` to that
   release (its tag and loader SHA-256) and list in `KNOWN_2C` the functions `FELUCCA_BLE` legitimately changes.
8. **Tests come with the code.** Copy `tests/ble/*` unchanged. Port `tests/ble_ui_stub.h`, `bt_devs_test.c`,
   `bt_midi_test.c`, `bt_fml_test.c`, `fmlink_test.c`, `bt_page_test.c`, `bt_page_render.c`, `smc_test.c` to the
   firmware's harness. The firmware's own suite stays green.
9. **Docker on Apple silicon:** `tools/build.py`'s `tc()` retries a tool that fails with `exec format error` (Docker's
   amd64 emulation refuses now and then). Add the retry if the firmware lacks it.

## 3. The core hooks (`ble/ble_core.h`), with the reference implementation

| hook | Felucca (`core_ble_api.c`) | needs from the firmware |
| --- | --- | --- |
| `core_ms`, `core_wdt_feed`, `core_reboot` | `fm1_ms`, the watchdog, the reset | its time base |
| `core_audio_stop` | stops the audio DMA before a reboot | |
| `core_irq_attach/mask/unmask/is_masked` | `fm1_irq_*` (hal) | unchanged hal |
| `core_ble_stack_window` | `fm1_guard_stack_window` | |
| `core_midi_out` | USB MIDI out | its MIDI out |
| `core_flash_read` | `st_read` within 0x93000..0xFFFFF | its flash reader |
| `core_audio_late`, `core_audio_gap_us` | `felucca_dbg.late`; `audio.c audio_gap_max` (longest gap between two audio half interrupts) | a counter in its audio ISR |
| `core_ui_frames` | `felucca_dbg.ui_frames` (once per main-loop pass) | |
| `core_ble_slow` | `bt_set_slow(song.playing)` (RECONNECT) | its "transport plays" flag |
| `core_ble_mac` | `bt_devs.c bt_mac` (made once, kept in the settings) | storage |
| `core_rf_keep_get/put` | `bt_devs.c bt_rf_io` (favorites rows 9..11) | storage (rule 4) |
| `core_con_puts` | the USB serial console (CDC) | a console, or a no-op |
| `core_ble_midi_in`, `core_ble_link` | `bt_midi.c` (MIDI mode into the input ring as source 3; SMC DAW mode to `smc.c`) | its MIDI input path |
| `core_fml_rx` | `bt_fml.c` (FM-Link messages in) | its transport and tempo |
| `core_ble_in_drops` | `bt_midi.c` | |

The main loop calls `bt_service()` (Felucca: from `editor.c ed_service`, before any return) every pass. The
console's `ble` word calls `ble_console(args)`.

## 4. Core changes outside the Bluetooth files (Felucca's, as a checklist)

- `felucca.c`: include `ble/ble_core.h`, `bt_set.c`, `bt_midi.c`, `bt_fml.c`, `bt_devs.c`, `bt_page.c`, `smc.c`,
  `fmlink.c`, `core_ble_api.c` under `FELUCCA_BLE`.
- `seq.c`: MIDI out from the keys to `bt_midi_out_isr`; `CLK_LINK` (FM-Link's follower) and `bt_fml_block` in
  `events_block`; `bt_clock_block` (MIDI clock out to the BLE device).
- `params.c`: `N_CLOCK` gains `BLE` and `LINK` (append-only).
- `perform.c`: `smc_solo` (the SMC-Mixer's SOLO) in `perf_begin`.
- `audio.c`: `audio_gap_max` in the audio ISR.
- `console.c`: the `ble` word.
- `menu_items.c`, `ui_menu.c`, `ui_input.c`: the BLUETOOTH row and page (`ui.menu` BT_MENU).
- `ui_draw.c`: the top-bar icon (`bt_head_icon`: none, struck, lit).
- `settings_persist.c`: keep the Bluetooth bytes on a settings import.

## 5. Order of work

1. Fork, branch `ble`, build the plain firmware, record its sizes and its `.bin`.
2. Copy layer 1 and the tools; add the `FELUCCA_BLE` link to `build.py` and `app_ble.ld` from the firmware's
   `app.ld` (keep its MEMORY block equal). Link the stack alone (no features) and measure (`BLE_MEASURE=1`).
3. Decide the cuts with the user (rule 3) and implement them in the Bluetooth build only.
4. Layers 2-4, each with its tests; then the gates (layer 5).
5. Full suite, the plain `cmp`, the Bluetooth build with every gate. Commit on `ble`, push, build the package
   (`*.fwsc`) for a device test with `tools/fm1_serial_log.py` (`ble`, `status` every 2 s) and `docs/ble/DEVICE_STEPS.md`.

## 6. Known differences between firmwares (2026-10-23)

| | Felucca 1.6.1 | Hortator 0.23 | Skiff 1.4 | SLOOP 2.8 |
| --- | --- | --- | --- | --- |
| base | 1.6.1 | a Felucca fork with its own releases (its gates pin drum-v0.23.2) | Felucca 1.1.5 + 1.6.1 merges | Felucca ~1.0, hand-ported |
| settings | favorites table rows | packed bits; the devices in a new object (planned: 0x9F000 / 0xFE000) | favorites table rows | multi-part settings object |
| plain image / free | 511 KB / 70 KB | 328 KB / 253 KB | 581 KB / 0.2 KB | 567 KB / 14 KB |
| Bluetooth build cuts | PIANO, FLUTE, SAX; one core | none | PIANO, FLUTE, SAX, BREAK, pixel fonts; one core | BASS, VIBES, HORNS, FLUTE |
| RF trim at risk from | autosave 0xE5000 | nothing | pattern bank 0xE7000 | USR4 0xE7000 |
