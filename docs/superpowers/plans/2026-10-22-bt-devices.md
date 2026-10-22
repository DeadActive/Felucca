# Saved Bluetooth Devices Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** In the host role the FM-1 keeps a saved list of up to 8 Bluetooth devices (controllers and FM-1s), connects
one of each kind by the user's pick order, reconnects by itself, scans without dropping links, and shows each device
on the Bluetooth page (mockup A), with a top-bar Bluetooth icon.

**Architecture:** The BLE unit (`firmware/src/ble/`, its own compilation unit) gets two symmetric host link slots
(CTRL, FM1), each driven to a target address the core sets, and one scanner independent of the links; a pure
`ble_host_step` decides one action per pass. The core (`bt_devs.c`) owns the saved list (stored in spare bytes of
the settings' favourites table), chooses each slot's target by pick order with a 30 s grace for a lost device, and
the page (`bt_page.c`) shows and edits the list.

**Tech Stack:** C (JieLi pi32v2 clang for the firmware, host cc for tests), the JieLi BLE libraries through
`ble_core.h` / `ble_sdk.h`, Python 3 build tools.

**Spec:** `docs/superpowers/specs/2026-10-22-bt-devices-design.md`

## Global Constraints

- Bluetooth build only (`#if FELUCCA_BLE`, or files only it compiles); the plain build stays byte-identical to upstream (gate 5, `cmp` with upstream built the same day).
- `persist_t` (settings record) format, magic and size unchanged.
- App slot: keep ≥ 32,768 B free if possible; 30,720 B is the floor; if it does not fit, Task 8 (the icon) is dropped first.
- RAM `.data+.bss` ≥ 4,096 B spare; `.pool` ≥ 8,192 B spare (build gates).
- At most two host links: one controller (CTRL) and one FM-1 (FM1). FM1 link: FM-Link only, no MIDI.
- Saved list: up to 8; order = the user's picks only (ADD, or OCT+ "use now"), most recent first.
- Grace for a lost device: 30 s (`BT_GRACE_MS 30000u`). "Around": seen by a scan in the last 20 s (`BT_AROUND_MS 20000u`), or connected.
- Kind: a device whose scanned name starts with "Felucca FM-1" is an FM-1; any other is a controller.
- The BLE unit reaches the core only through `ble_core.h`; core code calls BLE entry points only from the main loop.
- Every commit ends with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`; push to `origin ble`, never to main.

## Review Focus

1. A controller drops and returns within 30 s while another saved controller is on: the FM-1 reconnects the first, never the stand-in (Task 4 test `choose_grace`).
2. Both links up, the user opens + ADD A DEVICE: the scan runs and neither link drops (Task 3 test `scan_beside_links`).
3. Forgetting the connected device: it disconnects, and the next saved device of its kind that is around connects (Task 4 `forget_connected`, Task 6 `forget_row`).
4. Power-on with saved devices none of which is around: background scans in windows with rests, the page shows AWAY, no busy loop (Task 3 `scan_rest`).
5. Older settings (zero bytes) and a damaged entry (bad check byte): no device, never a garbage line (Task 4 `load_zero`, `load_bad_check`).

---

### Task 1: The serial logger as a tool

**Files:**
- Create: `tools/fm1_serial_log.py`
- Modify: `docs/ble/DEVICE_STEPS.md` (how to run it)

**Interfaces:**
- Produces: `python3 tools/fm1_serial_log.py LOGFILE` (pyserial): logs every console line with a timestamp; reconnects across reboots (first `/dev/cu.usbmodem*`, or `--port`); on each connect sends `crash`, `ble`, `fml`; `ble` every 30 s.

- [ ] **Step 1:** Copy the job's logger (`~/.claude/jobs/21001013/tmp/fm1_log.py`) into `tools/fm1_serial_log.py` with an argparse `--port` / `--period` and the SPDX header of the repo's tools.
- [ ] **Step 2:** Run `python3 tools/fm1_serial_log.py --help`. Expected: usage text, exit 0.
- [ ] **Step 3:** Add a "Logging a session" paragraph to `docs/ble/DEVICE_STEPS.md` (pip install pyserial; run; what to send back).
- [ ] **Step 4:** Commit `tools: the FM-1 serial logger (reconnects across reboots; crash / ble / fml)`.

### Task 2: Scan entries that age

**Files:**
- Modify: `firmware/src/ble/ble_scan.h`, `firmware/src/ble/ble_scan.c`
- Test: `tests/ble/ble_scan_test.c`

**Interfaces:**
- Produces: `ble_dev_t` gains `uint32_t seen_ms`; `void ble_scan_report(ble_scan_t *s, const uint8_t addr[6], uint8_t addr_type, int8_t rssi, const uint8_t *ad, uint32_t ad_len, uint32_t now_ms)` (new last parameter, sets `seen_ms`); `void ble_scan_expire(ble_scan_t *s, uint32_t now_ms, uint32_t max_age_ms)` frees entries not seen for longer than `max_age_ms`. The list is no longer cleared at a scan's start.

- [ ] **Step 1: failing tests** in `ble_scan_test.c`: `expire_old` (report A at 0 ms, B at 25,000 ms; `ble_scan_expire(s, 30000, 20000)` → A gone, B kept, the name of B kept); `report_refreshes` (A at 0, A again at 15,000; expire at 30,000 / 20,000 → A kept, `seen_ms == 15000`).
- [ ] **Step 2:** `cc -o /tmp/st tests/ble/ble_scan_test.c && /tmp/st` → FAIL (signature).
- [ ] **Step 3:** Implement; update every caller of `ble_scan_report` (`ble_central.c`).
- [ ] **Step 4:** Run → `ble_scan: all checks passed`.
- [ ] **Step 5:** Commit `ble: scan entries remember when they were seen and age out`.

### Task 3: Two host link slots and one scanner (BLE unit)

The largest task: `ble_central.c`'s host side is rebuilt. The device role (advertising, one host, `ble_dev_step`), the stack start, `ble_out_drain` (MIDI out), FM-Link's queue (`fq`) and the console's text helpers stay. Removed: `ble_auto_t` / `ble_auto_step` / `ble_sight` / `ble_watch`'s host part, `ble_lnk_t` / `ble_lnk_step` / `ble_lnk`, `ble_ui_target`, `ble_ui_scan`, `ble_ui_pause`, `ble_ui_link`, `ble_ui_link_state`, `conn_addr` / `conn_name` single-link copies.

**Files:**
- Modify: `firmware/src/ble/ble_central.c`, `firmware/src/ble/ble.c`, `firmware/src/ble/ble_core.h`, `firmware/src/ble/ble_stub.c`, `tests/ble_ui_stub.h`
- Test: `tests/ble/ble_central_test.c` (the removed functions' checks go; new ones below)

**Interfaces:**
- Produces in `ble_core.h`:
  ```c
  enum { BLE_SLOT_CTRL, BLE_SLOT_FM1, BLE_SLOTS };
  enum { BLE_LS_IDLE, BLE_LS_CONNECTING, BLE_LS_DISCOVERING, BLE_LS_UP };
  typedef struct { uint8_t st, addr[6], type; uint8_t last_reason; uint16_t drops; char name[24]; } ble_ui_slot_t;
  void ble_ui_slot_target(uint32_t slot, const uint8_t addr[6], uint32_t type); /* all-zero addr: none; another
                                                                                 * address than the linked one drops it */
  int ble_ui_slot(uint32_t slot, ble_ui_slot_t *out);  /* 1 always (HOST role); its state, its address, its name */
  void ble_ui_scan_want(int on);                       /* the core wants scanning (the page, a slot not on its first
                                                        * choice); the unit also scans by itself for a target not listed */
  void core_ble_mac(uint8_t mac[6]);                   /* the FM-1's fixed address (Task 5) */
  ```
  `ble_ui_dev_t` gains `uint32_t seen_ms`. Kept: `ble_ui_state` (the stack's / device role's state), `ble_ui_err`, `ble_ui_devs`, `ble_ui_dev`, `ble_ui_want`, `ble_ui_role`, `ble_ui_role_now`, `ble_ui_conn_name` (CTRL slot, or the device role's host), `ble_fml_send`, `ble_fml_ready` (FM1 slot up, or the device role linked), `core_ble_link` (CTRL slot / the device role's link up and down).
- Pure, in the host-tested part of `ble_central.c`:
  ```c
  enum { BH_NONE, BH_SCAN_ON, BH_SCAN_OFF, BH_CONNECT, BH_CANCEL, BH_DROP };
  typedef struct { uint8_t st, target, listed, mismatch, tried; uint32_t rest_ms, deadline_ms; } ble_hslot_t;
  typedef struct { ble_hslot_t s[2]; uint8_t want, scan_on, scan_want; uint32_t scan_until, scan_rest; } ble_host_t;
  uint32_t ble_host_step(const ble_host_t *h, uint32_t now_ms, uint32_t *slot);
  ```
  (`target`: a target is set; `listed`: the target is in the scan list, seen in the last 20 s; `mismatch`: the slot is
  linked / connecting to another address than its target, or has no target while not idle; `tried`: a connect to this
  target failed since it was last sighted.) Rules, in order: a CONNECTING slot past its deadline → CANCEL; a
  DISCOVERING one → DROP; a `mismatch` slot (or `!want` and not idle) → DROP or CANCEL; while a slot is CONNECTING or
  DISCOVERING nothing else (a running scan is stopped first: SCAN_OFF); an IDLE slot with `target && listed && !tried`
  and its rest (2 s) over → SCAN_OFF first if scanning, then CONNECT; scanning is wanted while `want && (scan_want ||
  some IDLE slot has a target not listed)`: SCAN_ON in 10 s windows (`scan_until`) with 2 s rests (`scan_rest`)
  unless `scan_want` (the page: continuous); not wanted → SCAN_OFF.
- Scan parameters: continuous (48/48) with no link, 8 ms in 24 (38/13) beside a link (as now). Connection interval
  15 ms, supervision 4 s, connect deadline 5 s, discovery deadline 10 s (as now). CTRL: client index 0, FM1: client
  index 1 (`ble_op_multi_att_send_conn_handle`). Routing of connection-complete by the address each slot initiated
  (`memcmp(&packet[8], addr, 6)`), disconnects by handle, notifications by `d->conn_handle`, discovery by the one slot
  DISCOVERING.
- CTRL discovery requires the BLE-MIDI characteristic (as now: `NO BLE-MIDI SERVICE` drops it); FM1 discovery requires
  the FM-Link characteristic.
- `ble` console output gains, per slot: state, target, linked address, `last_reason` (HCI reason of the last
  disconnect), `drops`, connect attempts.

- [ ] **Step 1: failing tests** in `ble_central_test.c` (host-step): `connect_listed` (IDLE, target listed, not tried, rested → CONNECT slot 0); `scan_for_unlisted` (target not listed, scan off, rested → SCAN_ON); `scan_window` (scanning past `scan_until`, no `scan_want` → SCAN_OFF, then nothing for 2 s, then SCAN_ON); `scan_beside_links` (both slots UP, `scan_want` → SCAN_ON, never DROP); `scan_rest` (no target listed, both IDLE: within one 12 s period exactly one SCAN_ON and one SCAN_OFF); `one_at_a_time` (slot 0 CONNECTING: slot 1 listed → NONE; scanning → SCAN_OFF); `connect_deadline` (→ CANCEL); `discovery_deadline` (→ DROP); `mismatch_drops` (UP with mismatch → DROP); `bt_off` (`!want`: UP → DROP, CONNECTING → CANCEL, scanning → SCAN_OFF); `tried_waits` (tried → no CONNECT until listed again).
- [ ] **Step 2:** `cc -o /tmp/bct tests/ble/ble_central_test.c && /tmp/bct` → FAIL.
- [ ] **Step 3:** Implement `ble_host_step` and the slots' glue (connect, cancel, drop, the handler's routing, discovery per slot, sends per slot, `ble_ui_slot*`, `ble_ui_scan_want`, the console), and the stubs (`ble_stub.c`, `tests/ble_ui_stub.h`: `ble_ui_slot_target` records per slot, `ble_ui_slot` returns a test-set `bs.slot[2]`, `ble_ui_scan_want` records).
- [ ] **Step 4:** Run `ble_central_test` → all passed; build the core tests that use the stub (`bt_midi_test`, `bt_fml_test`, `smc_test` as `tests/run_tests.sh` does) → they compile (the page's test is Task 6's).
- [ ] **Step 5:** Commit `ble: two host link slots driven to targets, one scanner beside them`.

### Task 4: The saved list and the choice of targets (core)

**Files:**
- Create: `firmware/src/bt_devs.c` (included by `felucca.c` after `bt_set.c`, under `FELUCCA_BLE`)
- Test: `tests/bt_devs_test.c` (UI harness, `-DFELUCCA_BLE=1`, as `bt_midi_test.c`), added to `tests/run_tests.sh`

**Interfaces:**
- Consumes: Task 3's `ble_ui_slot_target`, `ble_ui_slot`, `ble_ui_scan_want`, `ble_ui_devs`, `ble_ui_dev` (with `seen_ms`); `settings_save()`; `favorites`.
- Produces:
  ```c
  #define BT_NDEV 8u
  enum { BT_KIND_CTRL, BT_KIND_FM1 };
  typedef struct { uint8_t addr[6], type, kind; char name[17]; } bt_dev_t;   /* name: up to 16 + NUL */
  static uint32_t bt_devs_n(void);
  static int bt_devs_get(uint32_t i, bt_dev_t *d);      /* i in pick order, 0 = most recent */
  static void bt_devs_add(const bt_dev_t *d);           /* to the top; the same address replaced; full: the least
                                                         * recently picked of its kind goes, else the last */
  static void bt_devs_forget(uint32_t i);
  static void bt_devs_use(uint32_t i);                  /* to the top */
  static uint32_t bt_dev_kind(const char *name);        /* "Felucca FM-1" prefix: BT_KIND_FM1 */
  static int bt_devs_choose(uint32_t kind, const uint8_t *around, int linked, uint32_t now_ms);  /* list index or -1 */
  static void bt_devs_frame(void);                      /* main loop (core_ble_api.c bt_service): targets, scan want */
  static int bt_devs_state(uint32_t i);                 /* 2 connected, 1 around, 0 away (the page) */
  ```
  Storage: entry `i` in `favorites.factory[i][8..31]`: `[0..5]` address, `[6]` flags (bit 0 random address, bit 1 FM-1,
  bit 7 used), `[7]` check = XOR of the other 23 bytes ^ 0x5A, `[8..23]` name (NUL-padded). A changed list calls
  `settings_save()`. Choice: the first entry of `kind` in list order that is `around` or `linked`; a device lost from
  its slot keeps the slot for `BT_GRACE_MS` (no other of its kind is chosen meanwhile) unless the user forgets it or
  picks another; a higher entry around again takes the slot. `bt_devs_frame` sets each slot's target to the choice
  (none: all-zero), and `ble_ui_scan_want(page open || a slot not on the first entry of its kind that exists)`.

- [ ] **Step 1: failing tests:** `load_zero` (all-zero favourites → `bt_devs_n() == 0`); `load_bad_check` (one entry's check wrong → skipped, the others kept); `add_order` (add A, B, C → order C B A; add A again → A C B, no duplicate); `evict` (8 controllers then a ninth → the oldest controller gone; 7 controllers + 1 FM-1 full, add an FM-1 → the old FM-1 goes); `forget` (forget index 1 → the rest close up, saved); `kind` (`bt_dev_kind("Felucca FM-1") == BT_KIND_FM1`, `"SMC-Mixer"` → CTRL); `choose_order` (two controllers around → the first); `choose_grace` (the first controller lost at t, the second around: at t + 29 s → -1 (wait), at t + 31 s → the second; the first back at t + 40 s → the first); `forget_connected` (forgetting the linked one → the next around is chosen at once, no grace); `targets` (`bt_devs_frame` with a stubbed scan list → `ble_ui_slot_target` called with the chosen addresses per slot); `name_cut` (a 23-character name → stored 16).
- [ ] **Step 2:** Build and run `tests/bt_devs_test.c` → FAIL.
- [ ] **Step 3:** Implement `bt_devs.c`; call `bt_devs_frame()` from `core_ble_api.c bt_service`.
- [ ] **Step 4:** Run → `bt_devs: all checks passed`.
- [ ] **Step 5:** Commit `bt: the saved device list (favourites' spare bytes) and the choice of each link's device`.

### Task 5: A fixed address for each FM-1

**Files:**
- Modify: `firmware/src/bt_devs.c` (or `bt_set.c`), `firmware/src/core_ble_api.c`, `firmware/src/ble/ble_central.c` (`ble_start`)
- Test: `tests/bt_devs_test.c`

**Interfaces:**
- Produces: `void core_ble_mac(uint8_t mac[6])` (core): the address in `favorites.factory[13][16..21]`, `[22] == 0xA5`, `[23]` check (XOR of 16..22 ^ 0x3C); missing or bad: a new random static address (`fm1_ticks()` mixed, top two bits of the most significant byte set, as `ble_start` does now), stored, `settings_save()`. `ble_start` takes its address from `core_ble_mac` instead of making one.
- [ ] **Step 1: failing tests:** `mac_kept` (two calls → the same 6 bytes; a fresh harness with the bytes in place → the same); `mac_valid` (random static: `mac[5] & 0xC0 == 0xC0` and `mac[0] & 0xC0 == 0xC0` as now); `mac_made_once` (zero bytes → made and stored with a good check).
- [ ] **Step 2:** Run → FAIL. **Step 3:** Implement. **Step 4:** Run → pass.
- [ ] **Step 5:** Commit `bt: each FM-1 keeps one Bluetooth address (made once, saved)`.

### Task 6: The page (mockup A)

**Files:**
- Modify: `firmware/src/bt_page.c` (rewritten rows), `firmware/src/bt_set.c` (ROLE: HOST / DEVICE; bit 5 kept as saved, unused; the old device bytes 24..29 left as saved, unused), `firmware/src/ui_input.c` / `ui_menu.c` only where the page's hooks change
- Test: `tests/bt_page_test.c` (rewritten for the new rows), `tests/bt_page_render.c` (new states)

**Interfaces:**
- Consumes: Task 4's `bt_devs_*`, Task 3's `ble_ui_*`.
- Rows (HOST): `BLUETOOTH`, `AUTOSTART`, `ROLE` (HOST, DEVICE), `MODE`, then the panel `DEVICES`: one line per saved device (`●`/`○`, name — an FM-1 shown as `FM-1 ` + the last 4 hex digits of its address, most significant first —, the kind tag `CTRL` / `FM-1`, `ON` / `AWAY`), then `+ ADD A DEVICE` (`SCAN`). ROLE DEVICE: the DEVICES panel is not shown; the state line shows the host, as now. A failure reason (`ble_ui_err`) shows on the state line.
- A knob left on a device line: the confirmation `FORGET <name>?` (the menu's confirm, OCT+ yes, OCT- no) → `bt_devs_forget`. OCT+ on a line `AROUND` but not connected: `bt_devs_use`. OCT+ on `+ ADD A DEVICE`: the ADD list (a sub-view of the page: the scan list without saved devices, BLE-MIDI first, two... as many lines as fit); OCT+ on one: `bt_devs_add` (kind by name), back to the page; OCT- back. While the ADD list is open the page asks for scanning (`bt_devs_frame`'s scan want).

- [ ] **Step 1: failing tests** (`bt_page_test.c`): `rows_host` (rows in order, cursor wraps); `device_lines` (two saved, one connected → `●`/`ON` and `○`/`AWAY`); `fm1_name` (an FM-1 at …:3F:2A shows `FM-1 3F2A`); `forget_row` (knob left → confirm text `FORGET SMC-Mixer?`; OCT+ → gone and saved; OCT- → kept); `use_now` (OCT+ on an AROUND line → it is first); `add_flow` (OCT+ on ADD → the list without saved ones; OCT+ on a device → saved first, back on the page); `role_two` (ROLE knob right → DEVICE, stops; the DEVICES panel gone); `no_drop_on_add` (opening ADD never calls a drop: `ble_ui_slot_target` unchanged).
- [ ] **Step 2:** Run → FAIL. **Step 3:** Implement. **Step 4:** Run `bt_page_test` → pass; run `bt_page_render` → `0 findings`; look at the PNGs (GREEN) for the device lines and the ADD list.
- [ ] **Step 5:** Commit `bt page: saved devices, each on its line; ADD A DEVICE; FORGET`.

### Task 7: Wire-up, room, docs

**Files:**
- Modify: `firmware/src/bt_midi.c` (the CTRL slot is "the device"; MODE as now), `firmware/src/bt_fml.c` (unchanged API; check), `firmware/src/core_ble_api.c`, `docs/ble/BLUETOOTH.md`, `docs/ble/DEVICE_STEPS.md` (new step 8f: add both, drop each many times, the logger), `docs/superpowers/specs/2026-10-21-fm-link-design.md` (HOST + LINK replaced)
- Test: the whole suite

- [ ] **Step 1:** `FELUCCA_BLE=1 ./build.sh` → all gates ok; note the image's free bytes (≥ 32,768 B wanted, ≥ 30,720 B the floor); if under the floor, trim (the BLE unit's console code is already minsize) and, failing that, Task 8 is not done.
- [ ] **Step 2:** `./build.sh` (plain) and `cmp build/felucca.bin <upstream built today>/build/felucca.bin` → identical.
- [ ] **Step 3:** `GATE5=1 ./tests/run_tests.sh` → only `perform_test` failing (the known host-timing check).
- [ ] **Step 4:** Docs; commit `bt: saved devices wired in; docs`; push; rebuild `build/site-ble` with the package (`python3 web/make_site.py <fwsc> 1.6.1-ble-devs build/site-ble`) and check the installer serves it.

### Task 8: The top-bar Bluetooth icon (dropped if it does not fit)

**Files:**
- Modify: `firmware/src/ui_draw.c` (`draw_head`, under `FELUCCA_BLE`), the icon data (a hand-drawn 16 px Bluetooth rune and its struck form, as a small 1-bit or anti-aliased bitmap beside the generated icons, or drawn with `gfx.c` primitives)
- Test: `tests/bt_page_render.c` or `tests/ui_render.c` (top bar in the three states), `tests/bt_page_test.c` (`icon_state`)

**Interfaces:**
- Consumes: `ble_ui_state`, `ble_ui_slot`, `ble_ui_role_now`.
- Produces: `static uint32_t bt_head_icon(void)`: 0 none (Bluetooth off), 1 struck (on, nothing linked: no slot UP and no device-role link), 2 lit (a link up). Drawn left of the battery (`HEAD_BAT_X`), theme colour when lit, `T_MID` struck; part of the header's signature.
- [ ] **Step 1: failing test** `icon_state` (off → 0; on, nothing → 1; CTRL UP → 2; device role linked → 2).
- [ ] **Step 2–4:** Implement; render the head in the three states through the lint → `0 findings`; check the PNG.
- [ ] **Step 5:** Build; if the image's free bytes fall under 30,720 B, revert this task (the user: "if not fit, drop bluetooth icon"). Else commit `ui: the Bluetooth icon on the top bar (lit, struck, none)`, push, rebuild the installer site.
