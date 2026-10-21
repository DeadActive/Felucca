# Bluetooth MIDI on Felucca (the Bluetooth build)

BLE MIDI: 2026 DEADACTIVE. From Hortator 0.16.0 (<https://github.com/DeadActive/hortator>), on Felucca 1.6.1.

`FELUCCA_BLE=1 ./build.sh` builds Felucca with Bluetooth LE MIDI in two roles: **HOST**, the FM-1 scans for a
BLE-MIDI device (a keyboard, a controller, an M-VAVE SMC-Mixer), connects to it, and takes and sends MIDI over it;
**DEVICE**, the FM-1 is itself a BLE-MIDI device that a computer, a tablet or a phone connects to. The package is
`build/felucca-ble.fwsc`. The plain build (`./build.sh`) is Felucca 1.6 as upstream, byte for byte, and either
installs over the other.

**Not yet run on a device in this form.** The BLE stack is the one Hortator runs on the FM-1; its place in Felucca
1.6 is checked by the build's gates and the host tests only. [DEVICE_STEPS.md](DEVICE_STEPS.md) is the check sheet.

## What the Bluetooth build gives up

**The second core.** It is built on one core (`FELUCCA_CPU2=0`, upstream's own one-core variant): MENU > SYSTEM has
no 2ND CORE row. The second core's stacks and mailbox and the Bluetooth stack do not both fit the RAM with the 4 KB
the build keeps spare, and the two have never run together on an FM-1. 2ND CORE is an opt-in beta (OFF by default) in
the plain build, which keeps it.

**Built-in samples.**
The built-in SAMPLE sets PIANO, FLUTE and SAX (104 KB) are left out to make room ([FIT.md](FIT.md)). The SAMPLE
and GRAIN engines play the user slots USR1-3; a sound that selects a built-in set plays a sine at the note's pitch and
the screen says NO SAMPLE once; SLICE keeps BREAK and the user slots and has no PIANO source. Everything else is
Felucca 1.6.

## The page

MENU (hold HOME) > SYSTEM > **BLUETOOTH**, OCT+. PRESETS moves the cursor, any of KNOB 1-4 changes the row, OCT+
acts on it, OCT- goes back to the menu, HOME closes the menu.

| Row | A knob | OCT+ |
| --- | --- | --- |
| BLUETOOTH | right ON, left OFF | ON / OFF |
| AUTOSTART | right ON, left OFF (saved) | ON / OFF |
| ROLE | right DEVICE, left HOST (saved) | HOST / DEVICE |
| MODE | AUTO, SMC DAW, MIDI (stops at the ends) | the next one (wraps) |
| DEVICE | left: forget the remembered device (a link drops) | - |
| SCAN | - | a 10 s scan (Bluetooth ON only) |
| a device found (BLE-MIDI ones first; up to 8, three in view) | - | remember it and connect to it |

While the cursor is on a device, the rows do not move under it: devices found meanwhile are added below. Move the
cursor up to SCAN or above to see the list in its order again.

- **Bluetooth is off at every power-on** unless AUTOSTART is ON. ON starts it; it then stays started until power-off
  (OFF disconnects and stops reconnecting). ON itself is not saved.
- **AUTOSTART ON** (default OFF): about 3 s after the FM-1 has started, Bluetooth turns on by itself, once, as ON on
  the page does, and reconnects to the remembered device. It is skipped after a crash, a hang (watchdog reset) or a
  failed start-up the power-on before (the screen says `BT AUTOSTART SKIPPED`), so a Bluetooth start that went wrong
  is never tried again by itself and the way to UBOOT and the updater stays clear; turn it on from the page then.
  Not while UPDATE MODE is up.
- With a remembered device, ON reconnects to it when it is seen, and again after the link drops. A scan from the
  page pauses that while the page is open.
- The state line: OFF, STARTING, IDLE, SCANNING n S, CONNECTING, CONNECTED + the device's name, FAILED + the reason
  (`NO RF TRIM IN FLASH`: this FM-1 has no stored radio calibration and Bluetooth cannot start on it;
  `BT START TIMEOUT`). After FAILED only a power cycle starts it again. Under it: the last error.
- The screen says `BT CONNECTED` and `BT LOST`.
- MODE and the remembered device are saved with the settings.

## ROLE DEVICE: a computer or a tablet connects to the FM-1

With ROLE DEVICE and BLUETOOTH ON (or AUTOSTART), the FM-1 advertises the standard BLE-MIDI service as
**Felucca FM-1** and the state line says `WAITING FOR A HOST`. DEVICE, SCAN and the device list are not shown in this
role. Connect from:

- **macOS:** Audio MIDI Setup > Window > Show MIDI Studio > the Bluetooth button (Bluetooth Configuration), then
  **Felucca FM-1** > Connect. It then appears as a MIDI port in every app.
- **iOS / iPadOS:** an app's Bluetooth MIDI screen (GarageBand: Settings > Advanced > Bluetooth MIDI Devices; many
  synth and DAW apps have one), then **Felucca FM-1**.
- **Android, Windows:** an app that connects BLE-MIDI devices (Windows has no built-in pairing for them).

Once connected the state line says `CONNECTED` and the host's address. Then it is MIDI as in MIDI mode, both ways: the
host's notes, CCs, MIDI LEARN and clock (CLK BLE) play the FM-1; the FM-1's keys and mod wheel go to the host once it
listens (it subscribes on its own). No pairing is needed; a host that asks for it gets Just Works, kept until
power-off. When the host disconnects the FM-1 advertises again. Changing ROLE ends a scan, a link or the advertising
first, then switches.

## Two FM-1s in sync (clock out)

While an FM-1 runs on its own tempo (CLK INT) and is linked over Bluetooth in MIDI mode, it sends MIDI clock to the
other end: Start on PLAY (and on GLO + PLAY, from the top), 24 clock pulses a beat counted from its own audio samples
(they never drift from its steps), Stop on STOP. The other end follows with CLK BLE. Nothing to turn on; an FM-1 that
follows a clock itself (CLK USB, TRS or BLE) sends none, so clock never loops.

1. **The follower:** ROLE DEVICE, BLUETOOTH ON; on the CLOCK page (tap HOME twice) CLK **BLE**.
2. **The master:** ROLE HOST, BLUETOOTH ON, SCAN, pick **Felucca FM-1** (it is remembered: AUTOSTART on both and they
   find each other at every power-on). CLK stays INT.
3. PLAY on the master starts both; its BPM is theirs; STOP stops both. The keys of each still play the other's
   tracks by channel (MIDI IN on each decides which).

Each FM-1 holds one Bluetooth link, so this is two units, not a chain. Bluetooth delivers in bursts (a 15 ms
connection interval), so a pulse can land a few ms early or late; the follower steps by counting pulses (no drift)
and smooths its tempo, and it starts one link delay (some tens of ms) after the master.

## MODE

- **MIDI:** the device is a third MIDI input beside USB and TRS: notes by channel (MENU > MIDI > MIDI IN), pitch
  bend, sustain, CCs, the modulation matrix's sources, MIDI LEARN (GLO + D4, turn a knob, move the controller's CC). The CLOCK page's CLK (tap HOME twice from HOME: MIXER, then CLOCK; KNOB 3) has a fourth source, **BLE**:
  the device's Start / Stop / clock run the transport. The FM-1's keys (on the track's channel) and the mod wheel
  (SELECT as MODW) are sent to the device too, and, while the FM-1 runs on its own tempo (CLK INT), its clock:
  Start on PLAY, 24 pulses a beat, Stop on STOP (below). When the link drops, the notes and the pedal
  the device held are released.
- **SMC DAW:** the M-VAVE SMC-Mixer in its DAW (Mackie Control) mode, below.
- **AUTO** (default): SMC DAW when the connected device's name starts with `SMC`, else MIDI.

A project saved with CLK BLE opens in the plain build with CLK TRS (its last source): set CLK to INT there.

## The SMC-Mixer (DAW mode)

| On the mixer | Felucca |
| --- | --- |
| Fader 1-4 | track 1-4 LEVEL |
| Fader 5-8 | track 1-4 REV (the reverb send) |
| Master fader | nothing |
| Knob 1-4 | the selected track's four HOME parameters, a step a click |
| Knob 5-8 | MIDI LEARN's: learned to a parameter, a click steps it by one value from where it is; not learned, nothing |
| MUTE 1-4 | mute / unmute the track |
| SOLO 1-4 | solo / unsolo the track (the others are silenced; their MUTE is untouched) |
| REC 1-4 | select the track and press REC for it (armed while stopped: the transport starts, as on the FM-1) |
| SELECT 1-4 | select the track |
| PLAY / STOP / REC | play when stopped / stop / REC for the selected track |
| BANK or CHANNEL left / right | the previous / next track (wraps) |
| strips 5-8's buttons, REW, FF, the arrows | nothing |

- **LEDs:** MUTE = muted, SOLO = soloed, REC n = armed, SELECT = the selected track, PLAY = playing, STOP = stopped,
  REC = any track armed.
- **Fader pickup:** a fader does nothing until it comes within 2 of the value or passes it; the mixer blinks its LED
  until then. A value changed on the FM-1 (a knob, a load, automation) releases its fader again.
- A fader or knob moved while its track records is recorded as automation, as the FM-1's own knobs are.
- **MIDI LEARN with the mixer:** GLO + D4, turn the FM-1's knob for the parameter, then turn mixer knob 5, 6, 7 or 8:
  it is learned (CC 20-23). The mixer's knobs send clicks, not positions, so a learned one moves its parameter on from
  wherever it is. Knobs 1-4, the faders and the buttons stay the layout's. In MIDI mode any controller's CC is
  learned as from USB.
- Solo ends when the mixer disconnects or MODE leaves SMC DAW.
- While a dialog, NAME or UPDATE MODE is on the screen, the mixer's buttons and knobs do nothing.

## The console

The USB serial console (MENU > SYSTEM > USB SERIAL ON) has `ble` (state, counters, the heap, the loop period, each
task's free stack, the device list), and `ble start`, `ble scan`, `ble list`, `ble connect N`, `ble stop`.

## What keeps the FM-1 safe

Every Bluetooth build stops unless:

1. the update loader is byte-identical to Felucca 1.6's (gate 1);
2. the update path's sources are 1.6's (2a); its machine code in the Bluetooth image equals the same core object's
   linked without the BLE unit, so the link changes nothing in it (2b); and the core compiled for the Bluetooth
   build equals the core compiled for the plain build in every update-path function but three, listed with their
   reasons in `tools/check_update_path.py` and checked by the functions they call (2c: `fm1_main`, which gains the
   console's `ble` word, and `fm1_fault_c` / `fm1_reboot`, where the reboot is called instead of inlined). 2c
   compares both without the compiler's merging of small statics, which moves every function's data offsets; the
   image that ships is compiled with it, as the plain build is;
3. no Bluetooth start-up is reachable at boot, only from the page's ON, AUTOSTART (and that only from the main
   loop's Bluetooth service) and the console's `ble start` (gate 3);
4. the radio library's RAM code reaches nothing in flash (gate 4).

`GATE5=1 sh tests/run_tests.sh` also proves the plain build is the same bytes with every Bluetooth file deleted
(gate 5); the plain build is byte-identical to upstream's built the same day (ABOUT shows the build date). The
gates are checks of the build, not of the device: the OCT- + OCT+ 5 s UBOOT key and the updater are
expected to work with Bluetooth off or on, as they do in Hortator, and [DEVICE_STEPS.md](DEVICE_STEPS.md) steps 2,
11 and 12 are where that is proved on this build. `tools/ble_base.txt` names the upstream release and its loader:
after merging a later upstream release, put its tag and its loader's hash there.

The Bluetooth stack is served from the main loop. CALIBRATION (its own screen loop) and an update session do not
serve it, so a connected device may drop while they run.
