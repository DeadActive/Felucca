# Saved Bluetooth devices: a list to manage, two links that just work — design

Date: 2026-10-22. Branch `ble` (the Bluetooth build of Felucca, `FELUCCA_BLE=1`). Bluetooth MIDI / FM-Link: 2026
DEADACTIVE. Talked through with the user in chat on 2026-10-22; the page layout was picked from mockups (option A).
Approved by the user on 2026-10-22 ("implement plan, if not fit, drop bluetooth icon").

## 1. Why

The FM-1 (host role) can hold two Bluetooth links at once since ROLE HOST + LINK (a controller such as the M-VAVE
SMC-Mixer, and another FM-1 for FM-Link). It works, but the user finds it a headache:

- **Order matters.** "To connect both devices you need to first connect another FM-1, and then the mixer; the other
  way, the mixer disconnects while Bluetooth scans." Two causes in the code: the page's SCAN disconnects the device's
  link first (`ble_ui_scan` → `ble_drop`, a rule from the one-link days), and the FM-1 search never scans by itself
  while a controller is remembered (`ble_lnk_step`'s `main_scans`: it waits for the controller link's reconnect scans,
  which only happen while that link is down).
- **Two devices are confusing on the page.** One DEVICE row for the controller, the FM-1 found by name and shown only
  on the state line.
- **Reconnecting fails.** "After some Bluetooth losses, the FM-1 host forgets the device and shows the saved device's
  MAC address"; the SMC-Mixer then has to be reconnected by hand. Two parts: (a) only the address is saved, so after
  a power-on the page shows a MAC until the device is seen again (display); (b) a real reconnect failure, cause not
  yet known (to be found, see 7).
- **FM-1s cannot be saved.** An FM-1 takes a new random Bluetooth address at each Bluetooth start (`ble_start`), so a
  remembered FM-1 is never found again.

What the user wants: "a solid way to save and manage connected devices", "a more obvious UI for each device
connected", and a way to forget each device on its own.

## 2. Goals

1. A short list of saved devices, controllers and FM-1s, kept over power-off, each with its name.
2. Connecting happens by itself: at most one controller and one FM-1 at a time (the two links the FM-1 can hold);
   of the saved devices that are around, the **last used** controller and the **last used** FM-1. After a lost link
   it reconnects by itself, every time, without a manual pick.
3. Scanning never disturbs what is connected; the order in which devices are added does not matter.
4. The Bluetooth page shows every saved device on its own line with its state; each can be forgotten on its own;
   adding a device is one obvious action.
5. A Bluetooth icon on the top bar: connected, on but not connected (struck through), off (no icon).

## 3. Non-goals

- More than two links at once (memory: the BT heap is 23,552 B and was measured at 17,960 B with two links).
- MIDI between two FM-1s on the FM-1 link (it stays FM-Link only; see 5.4).
- Changes to the device role (ROLE DEVICE) beyond what the fixed address needs.
- Changes to FM-Link's timing, the SMC-Mixer layout, or MODE's meaning.

## 4. Anti-goals (a failure even if it works)

- Anything that makes the plain build (no `FELUCCA_BLE`) differ from upstream Felucca, or changes the settings
  record's format (`persist_t`, its magic and size): the plain build must still read every setting as before.
- A connected device dropped because the user scanned, opened the page, or added another device of the other kind.
- A saved device shown as a bare MAC address.
- A reboot (the Bluetooth heap running out: `ble`'s heap high must stay well under its size with two links,
  reconnects included).

## 5. What was decided

### 5.1 The page (mockup A, chosen by the user)

MENU > SYSTEM > BLUETOOTH. Same controls as every menu page: PRESETS moves the cursor, any of KNOB 1-4 changes the
row, OCT+ acts, OCT- goes back.

```
 ⓘ BLUETOOTH                 OCT- BACK
 ┌──────────────────────────────────────┐
 │ BLUETOOTH                         ON │
 │ AUTOSTART                         ON │
 │ ROLE                            HOST │
 │ MODE                         SMC DAW │
 └──────────────────────────────────────┘
 ┌ DEVICES ─────────────────────────────┐
 │ ● SMC-Mixer      CTRL             ON │
 │ ● Felucca FM-1   FM-1             ON │
 │ ○ Xkey Air 25    CTRL           AWAY │
 │ ○ FM-1 Studio    FM-1           AWAY │
 │ + ADD A DEVICE                  SCAN │
 └──────────────────────────────────────┘
 PRESETS ROW      K1-4 FORGET     OCT+ OK
```

- The top four rows as today; ROLE goes back to two values, **HOST** and **DEVICE** (HOST + LINK is gone: HOST
  uses the saved list).
- **DEVICES:** each saved device on its own line: ● connected / ○ not, its name, its kind (CTRL or FM-1), and ON
  (connected) or AWAY. An FM-1's line shows "FM-1" and the last 4 hex digits of its fixed address (e.g. `FM-1 3F2A`):
  every FM-1 advertises the same name, "Felucca FM-1", which stays as it is for computers and DAWs *(the designer's
  call)*. Order: the user's picks, most recent first, within each kind (the builder picks whether the kinds are grouped
  or interleaved by recency; the mockup interleaves).
  - **A knob left on a device:** a confirmation "FORGET <name>?", OCT+ confirms (the menu's existing confirmation
    style). A connected one disconnects; the next saved device of its kind that is around takes its place.
  - **OCT+ on a device that is around but not connected:** use it now: it becomes the last used of its kind and
    replaces the one connected.
- **+ ADD A DEVICE:** OCT+ opens a scan list of nearby devices not yet saved (BLE-MIDI devices first, by signal, as
  the list today). Picking one saves it, connects it (replacing the connected one of its kind) and returns to the
  page. Scanning never drops a link.
- **ROLE DEVICE:** the DEVICES block is not shown; the page shows the host connected to it, as today.
- The state line and the error line of today's page: the builder decides what remains (the device lines carry the
  states now); a reason for a failure (e.g. `FAILED NO RF TRIM IN FLASH`) must still be shown somewhere.

### 5.2 The saved list

- Up to **8** devices (the user: "a short list, several controllers and FM-1s"; 8 is the designer's call).
- Each: address, address type, kind (controller or FM-1), name (the scanned name, cut to what the line shows, at
  least 16 characters), last-used order.
- **Kind:** an FM-1 is a device whose scanned name is "Felucca FM-1" or that offers the FM-Link characteristic
  (`ble_gatt.c`); everything else is a controller.
- Full list (8): adding a device drops the least recently picked device of the same kind (or, if none of that kind,
  the least recently picked overall).
- Kept over power-off. No migration: the device remembered by today's build (`favorites.factory[13][24..29]`, an
  address without a name or kind) is not carried over; the user adds devices once on the new page *(the designer's
  call: a migrated entry would show a placeholder or an address until seen, which the anti-goals rule out)*. The old
  bytes are left as they are.

### 5.3 Connecting and reconnecting

- HOST role: two links at most, one per kind. **Order = the user's picks**: only a pick on the page (ADD A DEVICE, or
  OCT+ "use now") moves a device to the top of its kind; connecting or reconnecting by itself never changes the order.
- For each kind the FM-1 wants the highest device in that order that is around. The user, asked "the SMC-Mixer drops
  for a moment while another saved controller is on: what happens?", answered **"connect to mixer"**: after a lost link
  the FM-1 goes back to that device and does not take another one in its place while it may still come back. The
  designer's reading, for the builder: a dropped device keeps its claim for a grace period (about 30 s) before a lower
  one of its kind may connect; whenever a higher one is around again, the FM-1 switches back to it.
- "Around" means seen by a recent scan. The FM-1 scans in the background while a kind has no link (duty-cycled beside
  a link, as now); with both links up it scans only while the Bluetooth page is open (so ON / AWAY is true there,
  without radio time taken from FM-Link while playing) *(the designer's call)*; never while the ADD list's own scan runs.
- After a lost link: reconnect by itself, indefinitely, while Bluetooth is ON. The user must never have to re-pick a
  saved device.
- BLUETOOTH OFF / AUTOSTART: as today.

### 5.4 What each link carries

- **Controller link:** as today: MODE applies (AUTO, SMC DAW, MIDI; FM-LINK = nothing but FM-Link, for a controller
  that is effectively nothing).
- **FM-1 link:** FM-Link only, as HOST + LINK does now. *(The designer's call: the user was asked twice whether keys
  on one FM-1 should play the other and did not answer; sync-only is how it works today.)*
- An FM-1 saved and used as the controller-kind device: not possible any more (an FM-1 is always the FM-1 kind).

### 5.5 A fixed address for each FM-1 (the designer's call)

Each FM-1's Bluetooth address stays the same from one Bluetooth start to the next (made once per unit and kept, or
derived from the chip), still a valid random static address, different between units. Without it an FM-1 cannot be
saved.

### 5.6 The top bar icon

On the top bar of every screen that has it (`ui_draw.c draw_head`), left of the battery:

- Bluetooth on and at least one device connected: the Bluetooth icon, lit (theme colour).
- Bluetooth on, nothing connected (searching, reconnecting, or the device role waiting for a host): the Bluetooth
  icon **struck through with a diagonal line** (the user's choice over a dim icon).
- Bluetooth off: no icon.
- One icon for all of Bluetooth (the designer's call: the user did not ask for one per device).
- The icon font (`web/fukiai.ttf`, 293 glyphs) has no Bluetooth glyph: one is drawn for the 16 px icon size, with its
  struck-through form; it must read clearly at 16 px in every palette, FLAT and LINE, and pass the UI render lint
  (`tests/ui_render.c`).
- The plain build's top bar stays exactly as upstream (the icon exists only with `FELUCCA_BLE`).

## 6. Constraints

- Gates of the Bluetooth build as now (`tools/build.py`): the loader pin, the update path (gates 2a-2c), no BT
  start-up at boot (gate 3), RAM code, the plain build byte-identical without the Bluetooth files (gate 5).
- Room: 33,076 B of the app slot free (keep ≥ 32 KB, the user's rule; asked whether it may go to 30 KB if the feature
  does not fit otherwise, the user said **"try"**: fit within 32 KB if possible, 30 KB is the floor; the user, on
  approving this design: **"if not fit, drop bluetooth icon"**: the top bar icon (5.6) is the first thing to go); RAM .data+.bss 69,484 of 73,728 B (keep
  ≥ 4 KB spare: upstream's gate); `.pool` 360,240 of 368,640 B (keep ≥ 8 KB spare: upstream's gate). The saved list
  must fit in these.
- The saved list cannot live in the settings record (`persist_t`): its spare bytes are nearly all taken and its format
  must not change. It needs its own small place in flash, written only from the main loop (never with the audio
  running into a flash wait it cannot afford, as the existing stores do) and robust to power loss during a write
  (an interrupted write must leave the previous list readable).
- The Bluetooth unit stays its own compilation unit reaching the core only through `ble_core.h` (gate 5).
- Host tests for the new logic (which device to connect, the list's order and its full-list rule, the page's
  rows), as the existing `tests/ble/*` and `tests/bt_*` do, and the page rendered through the UI lint.
- Untested on hardware until the user runs it: a device-step entry in `docs/ble/DEVICE_STEPS.md`. The hardware runs
  are the user's (two FM-1s, the SMC-Mixer, USB serial); the builder works from host tests and the user's logs.

## 7. The reconnect failure

The SMC-Mixer sometimes does not come back after a lost link (seen on the hardware with an FM-1 linked as well;
the user had to pick it again). Not explained by reading the code. The rework of connecting replaces the code
involved, but the cause should still be found so that it is not carried over: the serial logger used for the heap
crash (it reconnects across reboots and sends `crash` / `ble` / `fml` at once and `ble` every 30 s) goes into
`tools/` for the user's hardware runs, and the `ble` output gains what a drop needs (each link's state, the reason
of the last disconnect, reconnect attempts). The user then checks that a dropped controller and a dropped FM-1 both
come back by themselves, many times, in any order (DEVICE_STEPS).

## 8. Left to the builder

- Where in flash the saved list lives, its format, and how it survives a power cut mid-write.
- How the fixed FM-1 address is made.
- The exact drawing of the Bluetooth glyph and its strike.
- What remains of the state / error lines on the page; grouping vs interleaving of the kinds.
- How "around" is tracked (scan freshness) and the background scan's timing.
- The console's `ble` output for the list.
