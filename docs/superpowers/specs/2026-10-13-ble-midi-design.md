# Bluetooth MIDI host on Felucca 1.6: design

Date: 2026-10-13. Repository `DeadActive/Felucca` (a fork of `hugelton/Felucca`), branch `ble` from tag `v1.6`
(1727537). BLE MIDI work: 2026 DEADACTIVE.

## 1. Purpose

Hortator 0.16.0 (`DeadActive/hortator`, branch `midi`, f3d7e5a) makes the FM-1 a Bluetooth LE MIDI central: it
scans, connects to a BLE-MIDI device, receives and sends MIDI, and drives an M-VAVE SMC-Mixer in its DAW mode. Its
record is in that repository's `docs/ble/` and its two specs (2026-10-08, 2026-10-10). This design brings the same
into Felucca 1.6, which has no Bluetooth code.

## 2. What the user decided (2026-10-13)

- **Flash room:** the Bluetooth build drops the fewest built-in SAMPLE sets that make it fit with a 32 KB margin.
- **Scope:** the BLE stack and console, the Bluetooth page, BLE MIDI in and out, BLE as a clock source, and the
  SMC-Mixer DAW mode.
- **Fork:** a GitHub fork `DeadActive/Felucca`, cloned locally, work on branch `ble` from `v1.6`.
- **Review:** auto-approve. Every choice below not listed here was made by Claude; section 13 lists them.

## 3. Measured (unmodified v1.6, built here; the app is byte-identical in size to the release's, 508,108 B)

| | Felucca 1.6 | BLE cost in Hortator | after |
| --- | ---: | ---: | ---: |
| image, of the 581,564 B slot | 508,108 (73,456 free) | about 127,000 | about 54,000 over |
| `.data + .bss`, of 73,728 B (4,096 kept spare) | 55,300 | about 12,300 | about 67,600 (6,100 spare) |
| `.pool`, of 368,640 B (8,192 kept spare) | 328,896 | 23,744 | 352,640 (16,000 spare) |

RAM and pool fit. Flash does not: section 5.

The stock firmware's stored radio calibration, which the stack needs, was found on the user's FM-1 in the block at
0xE7E68, so at 0xE8000 or just above. Felucca 1.6 writes its autosave and FM6 voice bank at 0xE5000..0xE7FFF and
nothing at or above 0xE8000, so the record survives running 1.6.

## 4. Hard rules (Hortator's, kept)

1. **The update path always stays reachable.** The update loader is byte-identical to v1.6's, its hash pinned for
   every Bluetooth build. M-UPGRADE, the OCT- + OCT+ 5 s UBOOT key and the SysEx UBOOT request keep working.
   Nothing Bluetooth runs at boot. Every Bluetooth build is gated (section 7).
2. **The plain build does not change.** `./build.sh` without `FELUCCA_BLE` produces the same `felucca.bin` as tag
   `v1.6`, byte for byte. So every change to an existing source file is inside `#if FELUCCA_BLE`, and the tools
   change no default output.
3. **No flash writes from Bluetooth code.** The stock config store is read only. The remembered device is saved by
   Felucca's settings code.
4. **Integer / fixed point only** in project code. The JieLi radio library's soft-float is vendor code.
5. **Bluetooth is off at every power-on.** Only BLUETOOTH ON on the page (or the console's `ble start`) starts it.

## 5. Two builds, and the flash room

- `./build.sh` : Felucca 1.6 as upstream (rule 2).
- `FELUCCA_BLE=1 ./build.sh` : the Bluetooth build, package `build/felucca-ble.fwsc`. Same package identity
  (`FM-1_900`), so either build installs over the other. `FELUCCA_VERSION` is unchanged (the web editor reads
  it); the console's `ver` line, the package name and the BLUETOOTH row tell the builds apart.

**SAMPLE sets.** The three built-in sets with data are PIANO, FLUTE and SAX (126,244 B of ADPCM in all; sets 1
and 4 are aliases of PIANO without data). `tools/gen_samples.py` gains `FELUCCA_SMP_DROP` (a comma list of set
names). A dropped set keeps its index, its name and its presets, and has no zones, exactly as every set has when
the CC0 library is absent: the SAMPLE and GRAIN engines play a sine at the note's pitch and the screen says NO
SAMPLE once. User slots USR1-3 are untouched. Unset, the generated header is the same bytes as now.

The Bluetooth build sets `FELUCCA_SMP_DROP` itself. Which sets: measured in the plan's first task, by this rule:
the smallest total of dropped ADPCM bytes for which the gated image leaves at least 32,768 B of the slot free.
PIANO goes last (SLICE's PIANO source and the two alias sets use it). If dropping all three is not enough, the
work stops and the user decides.

## 6. Code layout

**The BLE unit, `firmware/src/ble/`** : Hortator's, copied as it is: `ble.c` (its own compilation unit),
`ble_os.c`, `ble_port.c`, `ble_central.c`, `ble_midi.c`, `ble_scan.c`, `ble_vm.c`, `ble_sdk.h`,
`ble_sdk_abi.h`, `ble_rf_tables.c`, `ble_lto_stubs.c`, `ble_stub.c`, `sdkcfg/app_config.h`. It reaches the core
only through `ble_core.h`. Changes: none expected beyond the heap comment; any that the port forces is listed in
the commit that makes it. What the device work taught (the stack window, the stored calibration instead of the
trim, the RAM rule, the start-up task, the interrupt guard) is kept as it is.

**HAL** : new files only, `firmware/hal/fm1_ble_hal.h`, `fm1_ctx.h`, `fm1_ctx.S`, as Hortator's. The HAL
symbols they use exist unchanged in 1.6. No existing HAL file changes.

**Core unit** (all included by `felucca.c` under `#if FELUCCA_BLE`):
- `core_ble_api.c` : the `core_*` functions of `ble_core.h`, written against 1.6;
- `bt_midi.c` : MIDI mode in and out (section 8);
- `bt_page.c` : the Bluetooth page (section 9), written against 1.6's menu and drawing code;
- `smc.c` : the SMC-Mixer DAW mode (section 11), written for 1.6's four tracks.

**Guarded edits to existing files:** `felucca.c` (the flag's default, the includes), `editor.c` (`ed_service`
calls `ble_service` and the core's per-pass work), `params.c` (`N_CLOCK` gains `BLE`), `console.c` (the `ble`
word), `menu_items.c` and `ui_menu.c` (the BLUETOOTH row and page), and whatever the SMC actions need. None of
them is an update-path source (gate 2a).

## 7. Build and gates

`tools/build.py` gains Hortator's Bluetooth path, adapted:
- the SDK mount, the pinned libraries (`tools/ble_libs.py`, unchanged: `AC79_BT_SDK` V1.2.0), the SDK config
  sources and the log stubs as bitcode, the gold-plugin link, `--wrap=memcpy`, long calls for `ble.c`;
- `firmware/app_ble.ld` made from 1.6's `app.ld` (RAM 72K, POOL 0x01C1A000 / 0x5A000) plus the libraries'
  sections; the budget checks read the regions from the script in use (`ld_regions`), as 1.6 does;
- 1.6's two-stage compile (`size_fns.py`) stays; the new core files are added to its size list;
- `BLE_MEASURE=1` : measuring only, never packaged.

Gates. Every Bluetooth build stops on any of them:

1. **Loader pin.** `build/loader/ota.bin`'s sha256 equals `LOADER_SHA256` in `tools/ble_base.txt` (v1.6's).
2. **Update path.**
   - **2a:** the update path's sources equal the tag named in `tools/ble_base.txt` (`UPSTREAM v1.6`):
     `firmware/src/usb.c`, `ota.c`, `ota_hw.c`, `storage.c`, `storage_hw.c`, `libc.c`, `main.c`,
     `firmware/crt0.S`, `firmware/app.ld`, `firmware/hal`, `firmware/loader`. Only the three new HAL files may
     be added. After a merge of a later upstream release the file names the new tag and its loader's hash.
   - **2b:** the update path's machine code in the Bluetooth link equals the same core object's linked with
     empty BLE entry points (`ble_stub.c`, `app.ld`). Function patterns as Hortator's, re-checked against 1.6's
     names.
3. **No BT at boot.** No BT start-up function is reachable from `fm1_cstart`, the ISRs or the fault handlers,
   except through `ble_user_start`, which only `ble_console` and the page's `bt_page_on` may reference.
4. **RAM code.** `.ram_text` reaches nothing outside itself; no library RAM-code section lands in XIP.
5. **Plain build.** `tools/check_default_build.sh` : the plain build is byte-identical with every Bluetooth
   file deleted. Once, in this work: the plain build of branch `ble` is compared with the build of tag `v1.6`.

The register-access check (`mmio_check`) also scans `src/ble/` (not the SDK's radio tables).

## 8. BLE MIDI in and out (MIDI mode)

- **In.** Received BLE-MIDI, already parsed to USB-MIDI packets by `ble_midi.c`, goes into the MIDI input ring
  (`usb.c midi_enqueue`) as **source 3**, with `midi_in_event`'s filter, interrupts off around the enqueue (the
  TIMER5 ISR fills the ring too), leaving the ring's last 8 slots to TRS. From there it is ordinary MIDI: notes
  by channel, pitch bend, sustain, CCs, the modulation matrix's sources, and MIDI LEARN, with no further change.
- **Clock.** CLK gains **BLE** (value 3) in the Bluetooth build. A project saved with CLK BLE and opened by the
  plain build follows that build's existing clamp.
- **Out.** In MIDI mode, the channel voice messages Felucca sends to USB MIDI out (the keys, on the track's
  channel) also go to the connected BLE device. Real-time bytes are not sent (24 per beat would fill the
  64-message queue). The callers may run in an interrupt, so the core queues them in a small ring that the main
  loop hands to `ble_midi_send`. In SMC DAW mode only the mixer's LEDs and fader positions are sent.
- **Console.** `ble`, `ble start`, `ble scan`, `ble list`, `ble connect N`, `ble stop`, as Hortator's.

## 9. The Bluetooth page

MENU > SYSTEM gains a row **BLUETOOTH** before INFO (Bluetooth build only); OCT+ opens the page, OCT- goes back.

```
BLUETOOTH
STATE   CONNECTED  SMC-Mixer
BLUETOOTH        ON              KNOB right / OCT+: ON; KNOB left / OCT+ again: OFF
MODE             AUTO            AUTO / SMC DAW / MIDI
DEVICE           SMC-Mixer       the remembered device; KNOB left: FORGET
SCAN                             OCT+: a 10 s scan
 1 SMC-Mixer        MIDI  -59    the devices found (MIDI ones first); OCT+ connects and remembers
```

Rows, states, errors, scanning and reconnecting behave as Hortator's page (its spec 2026-10-10, sections 2 and
3): BLUETOOTH ON starts the stack until power-off; OFF disconnects and stops reconnecting; MODE AUTO means SMC
DAW when the connected device's name starts with `SMC`, else MIDI; a manual scan pauses reconnecting while the
page is open. The cursor and value controls follow 1.6's menu (PRESETS moves, any of KNOB 1-4 or OCT+ / OCT-
changes, as the menu's own rows), and the page is drawn with 1.6's fonts and theme colours in every palette.
Top-bar messages: `BT CONNECTED`, `BT LOST`.

## 10. Settings

MODE and the remembered device are saved with the settings (`persist_t`, magic and size unchanged) in
`favorites.factory[13][24..31]`: six address bytes, a flags byte (bits 0-1 MODE, bit 2 the address type), and a
check byte (the XOR of the other seven, XOR 0xB7; a mismatch reads as no device and MODE AUTO). Row 13 is the
SLICE engine's favourites; bytes 24..31 would be its presets 192..255, which do not exist. The plain build
keeps the bytes as saved and never reads them, as it does with every other byte of that table it does not use.
`persist_t`'s scalar fields cannot hold them: 1.6 reads `lowcut % 3`, and `zoom` and `bold` are tagged values.

## 11. The SMC-Mixer DAW mode

The mixer's messages are as in Hortator's spec (faders = 14-bit pitch bend on channels 1-8, knobs = relative
CCs 16-23 sign-magnitude, buttons = notes on channel 1, LEDs = the same notes back, fader position = pitch bend
back). Felucca has four tracks:

| Control | Felucca |
| --- | --- |
| Fader 1-4 | track n LEVEL, with pickup |
| Fader 5-8 | track n-4 REV (the reverb send), with pickup |
| Master fader | ignored |
| Knob 1-4 | the selected track's four HOME knob parameters, one step per click, clamped, recorded as automation as the FM-1's knobs are |
| Knob 5-8 | nothing |
| MUTE 1-4 | mute / unmute track n (the track's MUTE parameter) |
| SOLO 1-4 | solo / unsolo track n (latched; a mask of the mixer's own, applied where the FX layer's track mutes are) |
| REC 1-4 | select track n and press REC for it |
| SELECT 1-4 | select track n |
| buttons 5-8 of each row | nothing |
| PLAY | stopped: play. STOP: playing: stop. REC: REC for the selected track |
| BANK / CHANNEL left, right | the previous / next track |
| REW, FF, arrows | nothing |

Only presses act. LEDs: MUTE n = muted, SOLO n = soloed, REC n = armed, SELECT n = selected, PLAY = playing,
STOP = stopped, REC = any track armed; only changes are sent, all of them on a new connection. Pickup as
Hortator's: an unpicked fader changes nothing until it comes within 2 of the value or crosses it; a value
changed by any other means unpicks its fader and the new position is sent. Solo and the LED state clear when
the mixer disconnects or MODE leaves SMC DAW.

## 12. Tests and checks

- Host: Hortator's `tests/ble/*.c` (OS, parser and packer, scan, central and auto-connect, VM record) and the
  gates' self-tests join `tests/run_tests.sh`.
- Host, new, against 1.6's test harness: MIDI mode into the ring as source 3 with the filter and the TRS
  reserve; the out ring; the settings bytes round-trip, a record without them, a bad check byte; the SMC decode,
  each action, LED diffing, pickup; the page drawn in each state in every palette (1.6's render checks for
  clipped or overlapping text).
- 1.6's whole existing suite passes on the plain build.
- Both builds, every gate, gate 5, and the one-time comparison with tag `v1.6`.
- **Not possible here: the device.** `docs/ble/DEVICE_STEPS.md` is the check sheet for the user (install, UBOOT
  key with BT off and on, start, scan, connect, MIDI in, clock, MIDI out, the SMC layout, reconnect, power
  cycle, an update with BT on, `audio_late 0`). The package is untested on hardware until that sheet is run.

## 13. Choices made by Claude (auto-approved; listed for the user)

1. The plain build stays byte-identical to `v1.6`, so Bluetooth changes live only in the Bluetooth build. Unlike
   Hortator, the plain build does not round-trip the remembered device through named fields: it keeps the bytes.
2. Solo is a runtime mask of the mixer's own (`smc_solo`, applied in `perform.c` as the FX layer's track mutes),
   not a new `song.solo` and not the GLO layer's mask: that one is recomputed from held keys every pass and is off
   while the menu is open (amended during the work, 2026-10-13).
3. The remembered device is stored in unused favourites bytes (section 10). A later upstream version could claim
   them; a merge would then move them.
4. `FELUCCA_VERSION` is not changed in the Bluetooth build.
5. The loader pin and gate 2a are checked in Bluetooth builds only, against `tools/ble_base.txt`, so merging a
   later upstream release into the plain build needs no gate change.
6. Faders 5-8 control the reverb sends; knobs 5-8 and buttons 5-8 do nothing.
7. Knobs 1-4 edit the selected track's HOME parameters, whatever page the FM-1 shows.
8. MIDI out over BLE carries channel voice messages only, no clock.
9. The scan range for the stored calibration is Hortator's (0xE5000..0xFBFFF).
10. Not in this work: a two-version web installer page, the browser emulator (it is the plain build), MIDI
    learn changes (BLE input reaches it as it is), a pull request to upstream.

## 14. Licensing

The JieLi BT libraries linked into the Bluetooth image come from the AC79 SDK (Apache-2.0), as the SDK files the
package already carries; `LICENSING.md` gets a row. `ble_rf_tables.c` is an Apache-2.0 copy, credited in the
file. The SMC mapping values come from the user's own `smc-seq` project. The BLE files keep their headers (GPL-3.0-only, 2026 DEADACTIVE).

## 15. Amendments made during the work (2026-10-13)

- The Bluetooth build leaves out all three sets, PIANO, FLUTE and SAX: no smaller choice keeps the 32 KB margin
  (`docs/ble/FIT.md`).
- The page's cursor wraps, as the menu's rows; three device rows are in view of up to eight; DEVICE shows the
  address in brief until the name is seen.
- When the link drops in MIDI mode, CC 64 = 0 and CC 123 are put into the input ring on each channel the device
  played.
- The mixer's buttons and knobs do nothing while a dialog, NAME or UPDATE MODE is up; they work with the menu open.
- The previous / next track buttons wrap.
- `tools/build.py` tries a tool again when Docker answers "exec format error".
- **Gate 2c** (after the final review): gate 2b links the same core object twice, so it cannot see what compiling
  the core with `FELUCCA_BLE` changes in an update-path function. Every Bluetooth build now also compiles the core
  as the plain build's and as the Bluetooth build's, both without global merging, and stops unless the update
  path's functions are identical in the two, apart from those listed with their reasons and allowed callees in
  `tools/check_update_path.py` (`fm1_main`, `fm1_fault_c`, `fm1_reboot`).
- On the page, the device rows do not move while the cursor is on one (new devices are added below).
- The release after a lost link is put into the ring as a pair, with interrupts off, and stays owed while the ring
  refuses it.
- **AUTOSTART** (the user, 2026-10-15): a saved setting on the page (flags bit 3 of the settings bytes, default OFF).
  ON: Bluetooth is turned on once per power-on, 3 s after the main loop first served it, as BLUETOOTH ON does; only
  after a clean boot (main.c `boot_clean`: no crash, no WDT hang, no failed boot before this one), not while UPDATE
  MODE is up. This loosens rule 5 by the user's choice; gate 3 allows `bt_page_auto` to reach `bt_page_on` only from
  the main loop's Bluetooth service.
- The plain build's equality with upstream holds for builds of the same day: ABOUT shows `__DATE__`.
- **MIDI LEARN with BLE** (the user, 2026-10-15): in MIDI mode a BLE controller's CCs reach MIDI LEARN as USB's do
  (tested). In SMC DAW mode the mixer's knobs 5-8 (CC 20-23, relative clicks) are MIDI LEARN's: while it waits they
  teach it their CC; learned, a click steps the learned parameter by one value from its current value (as the CC
  value `midi_learned` maps back to it), so nothing jumps; not learned, nothing. Knobs 1-4, faders and buttons stay
  the layout's.
- **Upstream 1.6.1** (merged 2026-10-21): the base is `v1.6.1` (`tools/ble_base.txt`: its loader's hash). The
  Bluetooth build is built with `FELUCCA_CPU2=0`: 1.6.1's second core (stacks and mailbox, 5.7 KB of `.data + .bss`)
  and the BT stack do not both fit the RAM with its 4 KB kept spare, and they are untried together. 2ND CORE is an
  opt-in beta, OFF by default; the plain build keeps it. Gate 2b and 2c also cover the second core's functions.
- **ROLE DEVICE** (the user, 2026-10-21: "add ble midi client mode too, so i can connect"): a saved ROLE (flags bit
  4: HOST / DEVICE) on the page. DEVICE: the FM-1 is a BLE-MIDI peripheral: a GATT server with the BLE-MIDI service
  and characteristic (`firmware/src/ble/ble_gatt.c`, the JieLi SDK's table format, host-tested), advertising as
  "Felucca FM-1"; a host's writes are BLE-MIDI in (as MIDI mode), the out queue goes as notifications once the host
  enabled them; Just Works if a host asks to pair, no bond stored. The role is switched only with no scan, link or
  advertising (ended first), as the SDK's examples switch (`ble_stack_gatt_role`, the handlers registered again); the
  central's tested start is unchanged (the server's table is registered the first time DEVICE is applied). 6,340 B
  of flash, 60 B of RAM: 37,152 B of the slot stay free.
- **Clock out over BLE** (the user, 2026-10-21, "for fast theory check ... connect multiple fm-1 together"): an FM-1 on
  CLK INT, linked in MIDI mode, sends Start (PLAY, GLO + PLAY), 24 pulses a beat from the audio's samples and Stop, from
  `seq.c events_block` (`bt_clock_block`, the audio ISR) through the out ring; none on CLK USB / TRS / BLE (no loop),
  none to the SMC layout. Supersedes "no clock is sent" (section 8, choice 8). No setting yet. Host-tested end to end:
  a master at 97 BPM drives a follower on CLK BLE (it starts, its tempo within 1 %). One link a unit: two FM-1s.
