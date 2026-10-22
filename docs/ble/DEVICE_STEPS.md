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

## Logging a session

With the FM-1 on USB and MENU > SYSTEM > USB SERIAL ON, `tools/fm1_serial_log.py session.log` (needs `pip install
pyserial`) writes every console line with the time. It waits for the FM-1 across resets and asks `crash`, `ble` and
`fml` each time it comes back, and `ble` every 30 s. Send the log file back with what you did and when.

## The steps

1. **Install.** Expected: the FM-1 restarts as Felucca 1.6; play, change pages, save a project. MENU > SYSTEM shows
   **BLUETOOTH** between CALIBRATION and INFO. `ble` shows `state OFF`. Copy the `ble` output (its `loop last / max`
   is the reference for step 9).
2. **Way back, Bluetooth off.** Hold OCT- + OCT+ for 5 s. Expected: UPDATE MODE / UBOOT. Leave it by switching off
   and on. This proves the way back before anything riskier.
3. **On.** MENU > SYSTEM > BLUETOOTH, OCT+. Expected: the state `OFF`, BLUETOOTH `OFF`, MODE `AUTO`, then
   `+ ADD A DEVICE`. A knob right on BLUETOOTH. Expected: `ON`, the state `STARTING`, then `ON, NO DEVICE SAVED` within
   5 s; no tone, no crash, no reset. `ble` shows `rf trim stored`.
   - `FAILED NO RF TRIM IN FLASH` or `FAILED BT START TIMEOUT`: copy `ble` and `crash`, power off and on, stop here.
4. **Add a device.** Switch the SMC-Mixer on in its DAW mode. PRESETS to `+ ADD A DEVICE`, OCT+. Expected:
   `SCANNING 8 S` counting down, the SMC-Mixer listed first with `MIDI` and its signal; after 8 s `ADD: PICK A
   DEVICE` and the list stays still. PRESETS to it, OCT+. Expected: back on
   the page, its line `SMC-Mixer CTRL`, then `ON` with a dot, the state `ON, 1 CONNECTED`, `BT CONNECTED` on the
   screen. On the mixer: SELECT 1 and STOP lit; the fader LEDs blink where a fader is away from its value. HOME closes
   the menu.
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
8. **Reconnect.** Switch the device off and on: it reconnects by itself within a few seconds (its line `AWAY`, then
   `ON`). Power-cycle the FM-1, BLUETOOTH ON: it reconnects to the saved device, its name shown from the start. On its
   line, a knob left: `FORGET SMC-Mixer?`; OCT- keeps it; again and OCT+: the line goes, the link drops.
8a. **AUTOSTART.** On the page, AUTOSTART ON (with a saved device). Power off and on. Expected: about 3 s after
    the start the state goes STARTING, IDLE and connects by itself. Then hold OCT- + OCT+ for 5 s: UBOOT still comes.
    AUTOSTART OFF again if you prefer Bluetooth off at power-on.
8b. **ROLE DEVICE.** On the page, ROLE DEVICE. Expected: the device lines and ADD go; with BLUETOOTH ON the state
    line says `WAITING FOR A HOST`. On a Mac: Audio MIDI Setup > MIDI Studio > Bluetooth: **Felucca FM-1** is listed;
    Connect. Expected: `CONNECTED` and an address on the FM-1; a MIDI monitor or a DAW on the Mac sees a "Felucca FM-1"
    port: the FM-1's keys come in, notes from the Mac play the FM-1 (MIDI IN CH1-4: channel 1 plays track 1); with CLK
    BLE the Mac's clock runs the transport; with CLK INT on the FM-1 the Mac follows its clock. Disconnect on the Mac:
    `WAITING FOR A HOST` again. ROLE HOST: the saved devices work as before (step 4). `ble` shows `state ADVERTISING` while waiting.
8c. *(Retired with the saved devices: an FM-1's link to another FM-1 carries FM-Link, not MIDI clock. Two FM-1s: 8d.)*
8d. **FM-Link** (two units with this build). B: ROLE DEVICE, BLUETOOTH ON. A: ROLE HOST, `+ ADD A DEVICE`, pick
    `Felucca FM-1`: its line shows `FM-1` and four characters of B's address, then `ON`. B: CLK `LINK`. Expected: within a few seconds
    `FM-LINK: FOLLOWING` on B and `FM-LINK: LEADING` on A; `fml` on B shows `fml peer 1`, `fml offset_ok 1`. PLAY on A:
    both start together (about 80 ms after the press); by ear the same 16ths on both, no flamming, for several minutes;
    turn A's BPM: B follows. PLAY on B: `FOLLOWING LINK`, nothing else. STOP on A stops both. With A playing, switch B
    off and on and link again: B starts on A's next bar. Copy `fml` and `ble` from both (`fm-link peer`, `in`, `out`,
    `lost`).
8e. **Two links, saved devices** (two FM-1s with this build and the SMC-Mixer). B: ROLE DEVICE, BLUETOOTH ON, CLK
    LINK. A: ROLE HOST, both added (step 4, step 8d), CLK INT. Run `tools/fm1_serial_log.py` on A. Expected: A's
    state `ON, 2 CONNECTED`, both lines `ON`; the mixer works on A (steps 5-6) while A and B play in time (8d). Add in
    either order: same result. Open `+ ADD A DEVICE` with both connected: neither drops. Switch the mixer off for 10 s
    and on: it comes back by itself, the FM-1 link stays. Switch B off and on: A finds it again. Many times, in any
    order. Power-cycle A: both come back without a pick, their names shown. With a second controller saved and on,
    switch the mixer off for 10 s: the second does not take its place (30 s); after 40 s it does; switch the mixer on:
    it takes its place back. Send the log (`ctrl` / `fm1` lines: `reason`, `drops`, `tries`; `heap high`,
    `alloc fails 0`, `fatal 0`).
9. **Under load.** A pattern plays while connected with faders and knobs moving. Expected: no audible glitch;
   `ble` shows `audio_late 0`, `errs 0`, `out_drops 0`, `in_drops 0`. Copy the `ble` output (`loop last / max`,
   `task max` (the longest BT task run), `heap high`, each task's `stack free`, `irq_lowered 0`).
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
| 8d FM-Link | | | |
| 8e two links, saved devices | | | |
| 9 under load | | | |
| 10 saving | | | |
| 11 UBOOT, BT on | | | |
| 12 update, BT on | | | |
