# Bluetooth MIDI on Felucca (the Bluetooth build)

BLE MIDI: 2026 DEADACTIVE. From Hortator 0.16.0 (<https://github.com/DeadActive/hortator>), on Felucca 1.6.1.

`FELUCCA_BLE=1 ./build.sh` builds Felucca with Bluetooth LE MIDI in two roles: **HOST**, the FM-1 keeps a list of
saved devices and connects to them by itself, two at once: a BLE-MIDI controller (a keyboard, an M-VAVE SMC-Mixer),
whose MIDI it takes and sends, and another FM-1, for FM-Link (the two in time); **DEVICE**, the FM-1 is itself a
BLE-MIDI device that a computer, a tablet, a phone or another FM-1 connects to. The package is
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
| MODE | AUTO, SMC DAW, MIDI, FM-LINK (stops at the ends; saved) | the next one (wraps) |
| a saved device (ROLE HOST) | left: `FORGET <name>?` on the state line, OCT+ yes, OCT- no | NEAR: use it now |
| + ADD A DEVICE (ROLE HOST) | - | an 8 s scan for nearby devices not saved, then the list stays put |

- **Bluetooth is off at every power-on** unless AUTOSTART is ON. ON starts it; it then stays started until power-off
  (OFF drops the links and stops connecting). ON itself is not saved.
- **AUTOSTART ON** (default OFF): about 3 s after the FM-1 has started, Bluetooth turns on by itself, once, as ON on
  the page does, and connects to the saved devices. It is skipped after a crash, a hang (watchdog reset) or a failed
  start-up the power-on before (the screen says `BT AUTOSTART SKIPPED`), so a Bluetooth start that went wrong is
  never tried again by itself and the way to UBOOT and the updater stays clear; turn it on from the page then. Not
  while UPDATE MODE is up.
- The state line: OFF, STARTING, `ON, NO DEVICE SAVED`, `ON, SEARCHING`, `ON, n CONNECTED`, FAILED + the reason
  (`NO RF TRIM IN FLASH`: this FM-1 has no stored radio calibration and Bluetooth cannot start on it;
  `BT START TIMEOUT`); in ROLE DEVICE `WAITING FOR A HOST` or `CONNECTED` and the host. After FAILED only a power
  cycle starts it again. Under it: the last error.
- The screen says `BT CONNECTED` and `BT LOST` (the controller's link, or the device role's).
- The top bar (left, after the REC mark) shows the Bluetooth rune: lit while a link is up, struck through while
  Bluetooth is on and nothing is linked, nothing while it is off.
- AUTOSTART, ROLE, MODE and the saved devices are saved with the settings.

## Saved devices (ROLE HOST)

The FM-1 keeps up to 8 saved devices, controllers and FM-1s, each on its own line: a dot when connected, a ring when
not, its name (an FM-1 as `FM-1` and the last four characters of its address, since they are all called Felucca
FM-1), `CTRL` or `FM-1`, and `ON` (connected), `NEAR` (switched on, seen lately) or `AWAY`.

- **+ ADD A DEVICE** scans for 8 s (`SCANNING 8 S` counting down) and lists the devices nearby that are not saved
  (BLE-MIDI ones first); then the list stays as it is, nothing moves under the cursor. OCT+ on one saves it at the top
  of its kind and connects it; OCT- and ADD again scan again (`NOTHING FOUND`: OCT+ scans again). Nothing connected
  drops while the list is open.
- **Two links at once:** one controller and one FM-1. For each kind the FM-1 connects the first saved device that is
  around, in the order you picked them (ADD, or OCT+ on a NEAR line: "use it now", to the top). Connecting by itself
  never changes the order.
- **After a lost link** it reconnects by itself, as long as Bluetooth is ON. A device that dropped keeps its link for
  30 s: no other device of its kind takes its place meanwhile, so a short drop of the mixer brings the mixer back,
  not a stand-in. After that the next one around is used, and the first one takes its place again when it is back.
- **FORGET** (a knob left on a line, then OCT+) removes it; if it was connected the next one of its kind around takes
  its place.
- **The controller's link** is MIDI as MODE says (AUTO, SMC DAW, MIDI). **The FM-1's link** carries FM-Link only
  (below), no MIDI.
- Each FM-1 has one Bluetooth address for good (made the first time Bluetooth starts, then kept), so a saved FM-1 is
  found again after it restarts.
- The connecting happens in the background: scans beside a link use 8 ms in 24, one connect at a time. `ble` on the
  console shows each link (`ctrl`, `fm1`): its state (0 idle, 1 connecting, 2 discovering, 3 up), its target, the
  last disconnect's reason, drops and connects tried.

## ROLE DEVICE: a computer or a tablet connects to the FM-1

With ROLE DEVICE and BLUETOOTH ON (or AUTOSTART), the FM-1 advertises the standard BLE-MIDI service as
**Felucca FM-1** and the state line says `WAITING FOR A HOST`. The saved devices are not shown in this role. Connect from:

- **macOS:** Audio MIDI Setup > Window > Show MIDI Studio > the Bluetooth button (Bluetooth Configuration), then
  **Felucca FM-1** > Connect. It then appears as a MIDI port in every app.
- **iOS / iPadOS:** an app's Bluetooth MIDI screen (GarageBand: Settings > Advanced > Bluetooth MIDI Devices; many
  synth and DAW apps have one), then **Felucca FM-1**.
- **Android, Windows:** an app that connects BLE-MIDI devices (Windows has no built-in pairing for them).

Once connected the state line says `CONNECTED` and the host's address (another FM-1 in ROLE HOST connects to it the
same way, as its FM-1: FM-Link). Then it is MIDI as in MIDI mode, both ways: the
host's notes, CCs, MIDI LEARN and clock (CLK BLE) play the FM-1; the FM-1's keys and mod wheel go to the host once it
listens (it subscribes on its own). No pairing is needed; a host that asks for it gets Just Works, kept until
power-off. When the host disconnects the FM-1 advertises again. Changing ROLE ends a scan, a link or the advertising
first, then switches.

## Clock out (to a computer)

While an FM-1 runs on its own tempo (CLK INT) and is linked over Bluetooth in MIDI mode, it sends MIDI clock to the
other end: Start on PLAY (and on GLO + PLAY, from the top), 24 clock pulses a beat counted from its own audio samples
(they never drift from its steps), Stop on STOP. A computer or a DAW connected to the FM-1 (ROLE DEVICE, above)
follows it; nothing to turn on. An FM-1 that follows a clock itself (CLK USB, TRS or BLE) sends none, so clock never
loops. Two FM-1s keep time with FM-Link instead (below): an FM-1's link to another FM-1 carries no MIDI.

## Two FM-1s in time: FM-Link

FM-Link sends no clock pulses. The two FM-1s measure the difference between their clocks (each way's fastest
packets over the last few seconds, so Bluetooth's bursts do not count) and share a timeline: the leader says when it
started, at what BPM. The follower runs its own steady clock at that BPM and keeps its beats on the leader's by a
sample more or less a block. In the simulations (`tests/fmlink_test.c`, `tests/bt_fml_test.c`: crystals 55 ppm
apart, 15 ms connection events, lost packets) the two stay within about 1 ms, for minutes. Not yet measured on two
FM-1s.

1. Link the two: one in ROLE DEVICE with BLUETOOTH ON; on the other (ROLE HOST), + ADD A DEVICE and pick it
   (`Felucca FM-1`; its line then shows `FM-1` and four characters of its address). Either may lead. The host FM-1
   keeps its controller link beside it.
2. **The leader:** CLK **INT**. **The follower:** CLK **LINK** (CLOCK page: tap HOME twice; KNOB 3). Once they hear
   each other the screen says `FM-LINK: LEADING` / `FM-LINK: FOLLOWING`.
3. PLAY on the leader starts both together, about 80 ms after the press. If the leader is already playing when the
   follower joins (connects, reconnects or is set to LINK), the follower starts on the leader's next bar. The
   follower always plays from its pattern's first step, so patterns whose lengths divide a bar stay in step.
4. The leader's BPM is the follower's (it follows a change at once); STOP on the leader stops both. PLAY / STOP on a
   linked follower do nothing but say `FOLLOWING LINK`.
5. If the link drops, the follower plays on at the last BPM; when it comes back it pulls back into time (or, if it
   drifted more than 50 ms, rejoins on the next bar). CLK LINK with no FM-Link peer plays as INT.

The host FM-1's link to the other FM-1 carries FM-Link only, whatever MODE says (MODE is the controller's). Only clock
and transport travel for now, not notes.

## MODE

- **MIDI:** the device is a third MIDI input beside USB and TRS: notes by channel (MENU > MIDI > MIDI IN), pitch
  bend, sustain, CCs, the modulation matrix's sources, MIDI LEARN (GLO + D4, turn a knob, move the controller's CC). The CLOCK page's CLK (tap HOME twice from HOME: MIXER, then CLOCK; KNOB 3) has a fourth source, **BLE**:
  the device's Start / Stop / clock run the transport. The FM-1's keys (on the track's channel) and the mod wheel
  (SELECT as MODW) are sent to the device too, and, while the FM-1 runs on its own tempo (CLK INT), its clock:
  Start on PLAY, 24 pulses a beat, Stop on STOP (below). When the link drops, the notes and the pedal
  the device held are released.
- **SMC DAW:** the M-VAVE SMC-Mixer in its DAW (Mackie Control) mode, below.
- **FM-LINK:** no MIDI either way on the controller's link (ROLE HOST) or the host's link (ROLE DEVICE): what the
  other end sends as MIDI is ignored, nothing is sent. FM-Link goes on beside it.
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

The USB serial console (MENU > SYSTEM > USB SERIAL ON) has `ble` (state, counters, each host link, the heap, the
loop period, each task's free stack, the scan list, FM-Link's packets), and `ble start`, `ble scan`, `ble list`,
`ble stop`. `fml` shows FM-Link: the role, the peer, the clock offset and whether it is known, the leader's state and BPM,
the follower's beat.

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
