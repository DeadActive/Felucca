# The Bluetooth build on the FM-1: the check sheet

BLE MIDI: 2026 DEADACTIVE.

The build: `FELUCCA_BLE=1 ./build.sh` on branch `ble`, the package `build/felucca-ble.fwsc`. It is Felucca 1.6 plus
[Bluetooth MIDI](BLUETOOTH.md), without the built-in SAMPLE sets. Bluetooth is off at every power-on.

Install: `python3 tools/fm1_install.py build/felucca-ble.fwsc` (or the web installer with that file). Going back:
install `build/felucca.fwsc` or the released 1.6 the same way; if the FM-1 does not start, see the README's "If the
FM-1 does not start".

The console: MENU > SYSTEM > USB SERIAL ON, then `screen /dev/cu.usbmodem* 115200` (leave with Ctrl-A, K, y).
`ble` prints the state and the counters; `crash` the last crash record. After any crash or unexpected reset: power
on, type `ble` and `crash`, and copy both (they keep the start-up record, `fatal` and the heap figures over the
reboot: `fatal` 1 a library assert, 2 a library reset request, 3 a BT task's stack overflow).

## The steps

1. **Install.** Expected: the FM-1 restarts as Felucca 1.6; play, change pages, save a project. MENU > SYSTEM shows
   **BLUETOOTH** between CALIBRATION and INFO. `ble` shows `state OFF`. Copy the `ble` output (its `loop last / max`
   is the reference for step 9).
2. **Way back, Bluetooth off.** Hold OCT- + OCT+ for 5 s. Expected: UPDATE MODE / UBOOT. Leave it by switching off
   and on. This proves the way back before anything riskier.
3. **On.** MENU > SYSTEM > BLUETOOTH, OCT+. Expected: the state `OFF`, BLUETOOTH `OFF`, MODE `AUTO`, DEVICE `NONE`,
   SCAN dim. A knob right on BLUETOOTH. Expected: `ON`, the state `STARTING`, then `IDLE` within 5 s; no tone, no
   crash, no reset. `ble` shows `rf trim stored`.
   - `FAILED NO RF TRIM IN FLASH` or `FAILED BT START TIMEOUT`: copy `ble` and `crash`, power off and on, stop here.
4. **Scan and pick.** Switch the SMC-Mixer on in its DAW mode. PRESETS to SCAN, OCT+. Expected: `SCANNING 10 S`
   counting down; the SMC-Mixer listed first with `MIDI` and its signal. PRESETS to it, OCT+. Expected:
   `CONNECTING`, then `CONNECTED SMC-Mixer`, `BT CONNECTED` on the screen, DEVICE `SMC-Mixer`. On the mixer: SELECT 1
   and STOP lit; the fader LEDs blink where a fader is away from its value. HOME closes the menu.
5. **Faders.** Move fader 1. Expected: nothing until it comes within 2 of track 1's LEVEL or passes it; then the LED
   is steady and LEVEL follows (MIXER page). Fader 5: track 1's REV. Turn LEVEL on the FM-1: the fader's LED blinks
   again.
6. **Knobs and buttons.** Knob 1-4: the selected track's HOME parameters, the screen says `T1 … value`. MUTE n,
   SOLO n (the others go silent, their MUTE stays), SELECT n, BANK / CHANNEL left and right, PLAY, STOP. REC n
   while stopped: the track is selected and armed and the transport starts; play keys, stop: the notes are
   recorded. The LEDs follow each of these, and changes made on the FM-1 too. MIDI LEARN: GLO + D4, turn the FM-1's
   knob for a parameter (REV on MIXER, say), turn mixer knob 5: the screen says `CC20 -> ...`; leave LEARN; knob 5 now
   moves that parameter a step a click.
7. **MIDI mode.** MODE `MIDI` (or connect a BLE-MIDI keyboard with MODE `AUTO`). Expected: its notes play the
   tracks by channel; pitch bend and sustain work; MIDI LEARN (GLO + D4) learns one of its CCs. CLOCK page (tap HOME twice) > CLK
   `BLE` with a device that sends clock: Start / Stop / tempo follow it. The FM-1's keys reach the device (a BLE
   MIDI monitor shows the notes). Switch the device off while holding a note: `BT LOST`, the note stops.
8. **Reconnect.** Switch the device off and on: it reconnects by itself within a few seconds. Power-cycle the FM-1,
   BLUETOOTH ON: it reconnects to the remembered device without a scan. DEVICE, a knob left: `NONE`, the link drops.
8a. **AUTOSTART.** On the page, AUTOSTART ON (with a remembered device). Power off and on. Expected: about 3 s after
    the start the state goes STARTING, IDLE and connects by itself. Then hold OCT- + OCT+ for 5 s: UBOOT still comes.
    AUTOSTART OFF again if you prefer Bluetooth off at power-on.
8b. **ROLE DEVICE.** On the page, ROLE DEVICE. Expected: DEVICE, SCAN and the list go; with BLUETOOTH ON the state
    line says `WAITING FOR A HOST`. On a Mac: Audio MIDI Setup > MIDI Studio > Bluetooth: **Felucca FM-1** is listed;
    Connect. Expected: `CONNECTED` and an address on the FM-1; a MIDI monitor or a DAW on the Mac sees a "Felucca FM-1"
    port: the FM-1's keys come in, notes from the Mac play the FM-1 (MIDI IN CH1-4: channel 1 plays track 1); with CLK
    BLE the Mac's clock runs the transport. Disconnect on the Mac: `WAITING FOR A HOST` again. ROLE HOST: scanning and
    the SMC-Mixer work as before (step 4). `ble` shows `state ADVERTISING` while waiting.
8c. **Two FM-1s in sync** (two units with this build). B: ROLE DEVICE, BLUETOOTH ON, CLK BLE (CLOCK page: tap HOME
    twice). A: ROLE HOST, BLUETOOTH ON, SCAN, pick Felucca FM-1, CLK INT. Expected: PLAY on A starts B; B follows A's
    BPM (turn it); STOP on A stops B; after a few minutes they are still together (by ear, a pattern of 16ths on
    both). `ble` on A shows `out` counting up and `out_drops 0`.
9. **Under load.** A pattern plays while connected with faders and knobs moving. Expected: no audible glitch;
   `ble` shows `audio_late 0`, `errs 0`, `out_drops 0`, `in_drops 0`. Copy the `ble` output (`loop last / max`,
   `longest BT task run`, `heap high`, each task's `stack free`, `irq_lowered 0`).
10. **Saving with Bluetooth on.** Stop; save a project and a user preset; power off and on: both are there, the
    settings too (MODE, DEVICE).
11. **Way back, Bluetooth on and connected.** Hold OCT- + OCT+ for 5 s. Expected: UPDATE MODE / UBOOT. Switch off
    and on.
12. **An update with Bluetooth on and connected.** Install a package (this one again, or the plain one). Expected:
    the update completes and the FM-1 restarts on it.
    - If it stops midway and Felucca comes back: `ble` and `crash`, copy both, install again with Bluetooth off.
    - If Felucca does not come back: run no other tool on it; note what the screen shows.

## Results

| Step | Date | Result | Notes |
| --- | --- | --- | --- |
| 1 install | | | |
| 2 UBOOT, BT off | | | |
| 3 on | | | |
| 4 scan, pick | | | |
| 5 faders | | | |
| 6 knobs, buttons | | | |
| 7 MIDI mode | | | |
| 8 reconnect | | | |
| 8a autostart | | | |
| 8b device role | | | |
| 8c two FM-1s | | | |
| 9 under load | | | |
| 10 saving | | | |
| 11 UBOOT, BT on | | | |
| 12 update, BT on | | | |
