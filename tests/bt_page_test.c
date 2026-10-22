/* SPDX-License-Identifier: GPL-3.0-only
 * BLE MIDI: 2026 DEADACTIVE */
/* The Bluetooth page (firmware/src/bt_page.c; docs/superpowers/specs/2026-10-13-ble-midi-design.md section 9) on the
 * host, built with FELUCCA_BLE=1 and the UI harness, driven through the real input (ui_input, menu_input): the row in
 * MENU > SYSTEM, the rows BLUETOOTH AUTOSTART ROLE MODE, the saved devices' lines (docs/superpowers/specs/
 * 2026-10-22-bt-devices-design.md 5.1: each on its line, FORGET, use now), + ADD A DEVICE, the way back, the state line.
 * The BLE unit's entry points are stubs (ble_ui_stub.h: `bs`, the slots `bh`); the saved devices are bt_devs.c's. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#if !FELUCCA_BLE
#error "build with -DFELUCCA_BLE=1"
#endif
#include "ble_ui_stub.h"
int ble_midi_send(const uint8_t *m, uint32_t n) { (void)m; (void)n; return 0; }
uint32_t ble_midi_room(void) { return 0; }
#include "../firmware/src/bt_set.c"
#ifdef BT_TEST_SMC
#include "../firmware/src/smc.c"
#else
static void smc_in(uint32_t pkt) { (void)pkt; }
#endif
#include "../firmware/src/bt_midi.c"
#include "../firmware/src/bt_fml.c"
#include "../firmware/src/bt_devs.c"
#include "../firmware/src/bt_page.c"

static int fails;
static void ok(const char *what, int good)
{
    printf("%s  %s\n", good ? "ok  " : "FAIL", what);
    if (!good)
        fails++;
}
static const uint8_t A1[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0xC6}, A2[6] = {0x9A, 0x00, 0x00, 0x00, 0x12, 0xC4};
static const uint8_t F1[6] = {0x2A, 0x3F, 0x10, 0x20, 0x30, 0xC5};   /* an FM-1: its line FM-1 3F2A */
static void dev(uint32_t i, const uint8_t a[6], uint32_t type, int midi, int rssi, const char *name)   /* seen now */
{
    ble_ui_dev_t *d = &bs.dev[i];
    memset(d, 0, sizeof *d);
    memcpy(d->addr, a, 6);
    d->type = (uint8_t)type;
    d->midi = (uint8_t)midi;
    d->rssi = (int8_t)rssi;
    str_cpy(d->name, name, sizeof d->name);
    d->seen_ms = fm1_ms;
    if (bs.ndev <= i)
        bs.ndev = i + 1u;
}
static void save(const uint8_t a[6], const char *name)   /* a saved device (to the top) */
{
    bt_dev_t d;
    memset(&d, 0, sizeof d);
    memcpy(d.addr, a, 6);
    d.kind = (uint8_t)bt_dev_kind(name);
    str_cpy(d.name, name, sizeof d.name);
    bt_devs_add(&d);
}
static void up(uint32_t slot, const uint8_t a[6])   /* the slot linked to a (0: down) */
{
    memset(&bh.slot[slot], 0, sizeof bh.slot[slot]);
    if (a) {
        bh.slot[slot].st = BLE_LS_UP;
        memcpy(bh.slot[slot].addr, a, 6);
    }
}
static void fresh(void)                              /* power-on: Bluetooth off, nothing remembered, the page open */
{
    ui_power_on();
    memset(&favorites, 0, sizeof favorites);
    memset(&bs, 0, sizeof bs);
    memset(&bh, 0, sizeof bh);
    memset(&btp, 0, sizeof btp);
    memset(&bt_dv, 0, sizeof bt_dv);
    fm1_ms = 100000u;
    conn_up = 0;
    bt_link(0);
    ui.menu = 1;
    ui.menu_sel = MI_BT;
    frame();
    press(B_OCTUP);
}
static void row(uint32_t r)                          /* the cursor to row r, a detent at a time */
{
    uint32_t guard = 32;
    while (btp.sel != r && guard--) {
        turn(EN_PRESET, 1);
        frame();
    }
}
static void k1(int32_t s) { turn(EN_K1, s); frame(); }
static bt_set_t saved(void) { bt_set_t s; bt_set_get(&s); return s; }

static void test_menu_row(void)
{
    uint32_t i, n = 0, at = 99;
    for (i = 0; i < MI_COUNT; i++)
        if (MI_TAB[i] == MTAB_SYSTEM) {
            if (i == MI_BT)
                at = n;
            n++;
        }
    ok("MENU > SYSTEM has BLUETOOTH right after CALIBRATION, before INFO, within the tab's rows",
       MI_BT == MI_PANEL + 1u && MI_BT + 1u == MI_INFO && at == n - 3u && str_eq(MI_NAME[MI_BT], "BLUETOOTH") &&
       MI_TAB[MI_BT] == MTAB_SYSTEM && n <= MTAB_ROWS);
    ok("BLUETOOTH is an action row (no value), as INFO and ABOUT", !menu_valued(MI_BT) && MI_BT >= MI_VALUES);
    fresh();
    ok("OCT+ on the row opens the page", ui.menu == BT_MENU && btp.sel == 0u);
    ok("page_scan: the page open (HOST) wants a scan", bt_dv.page == 1u);
    press(B_OCTDN);
    ok("OCT- goes back to the menu's rows, on BLUETOOTH", ui.menu == 1u && ui.menu_sel == MI_BT);
    ok("page_scan: closed -> no more", bt_dv.page == 0u);
    press(B_OCTUP);
    press(B_OCTDN);
    ui.menu_sel = MI_INFO;
    press(B_OCTUP);
    ok("INFO still opens from its row", ui.menu == 3u);
    press(B_OCTDN);
    ui.menu_sel = MI_ABOUT;
    press(B_OCTUP);
    ok("ABOUT still opens from its row", ui.menu == 2u);
}

static void test_rows_host(void)
{
    fresh();
    turn(EN_PRESET, 1); frame();
    ok("rows_host: PRESETS moves the cursor a row", btp.sel == 1u);
    turn(EN_PRESET, -1); frame();
    turn(EN_PRESET, -1); frame();
    ok("rows_host: no device saved: the cursor wraps from BLUETOOTH to + ADD A DEVICE (the 5th row)", btp.sel == 4u);
    save(A1, "SMC-Mixer");
    save(F1, "Felucca FM-1");
    frame();
    row(6);
    ok("rows_host: two saved: their lines are rows 5 and 6, + ADD A DEVICE the 7th", btp.sel == 6u && bt_page_rows() == 7u);
    turn(EN_PRESET, 1); frame();
    ok("rows_host: and it wraps to BLUETOOTH", btp.sel == 0u);
}

static void test_device_lines(void)
{
    char t[24];
    bt_dev_t d;
    fresh();
    save(A2, "Xkey Air 25");
    save(A1, "SMC-Mixer");                          /* SMC-Mixer first */
    up(BLE_SLOT_CTRL, A1);
    ok("device_lines: the linked one -> connected (ON)", bt_devs_state(0) == 2);
    ok("device_lines: one never seen -> AWAY", bt_devs_state(1) == 0 && str_eq(bt_dev_value(1), "AWAY"));
    dev(0, A2, 1, 1, -60, "Xkey Air 25");
    ok("device_lines: seen lately, not linked -> NEAR", str_eq(bt_dev_value(1), "NEAR"));
    ok("device_lines: linked -> ON", str_eq(bt_dev_value(0), "ON"));
    save(F1, "Felucca FM-1");
    bt_devs_get(0, &d);
    bt_dev_label(&d, t);
    ok("fm1_name: an FM-1's line: FM-1 and its address' last 4 hex digits", str_eq(t, "FM-1 3F2A"));
    bt_devs_get(1, &d);
    bt_dev_label(&d, t);
    ok("device_lines: a controller's line: its name", str_eq(t, "SMC-Mixer"));
}

static void test_forget_row(void)
{
    char t[40];
    fresh();
    save(A2, "Xkey Air 25");
    save(A1, "SMC-Mixer");
    frame();
    row(4);                                         /* SMC-Mixer's line */
    k1(-1);
    bt_state_text(t);
    ok("forget_row: a knob left asks first", str_eq(t, "FORGET SMC-Mixer?") && bt_devs_n() == 2u);
    press(B_OCTDN);
    ok("forget_row: OCT- -> kept (the page stays)", bt_devs_n() == 2u && ui.menu == BT_MENU && !btp.confirm);
    k1(-1);
    press(B_OCTUP);
    ok("forget_row: OCT+ -> forgotten, the other kept", bt_devs_n() == 1u && bt_devs_state(0) == 0 &&
       favorites.factory[0][8] == A2[0]);
}

static void test_use_now(void)
{
    bt_dev_t d;
    fresh();
    save(A2, "Xkey Air 25");
    save(A1, "SMC-Mixer");                          /* SMC-Mixer first, Xkey second */
    up(BLE_SLOT_CTRL, A1);
    dev(0, A2, 1, 1, -60, "Xkey Air 25");           /* Xkey around */
    frame();
    row(5);
    press(B_OCTUP);
    ok("use_now: OCT+ on a device around -> it is first", bt_devs_get(0, &d) && !memcmp(d.addr, A2, 6));
}

static void test_add_flow(void)
{
    bt_dev_t d;
    uint32_t targets;
    fresh();
    k1(1);                                          /* BLUETOOTH ON */
    bs.state = BLE_ST_IDLE;
    save(A1, "SMC-Mixer");
    up(BLE_SLOT_CTRL, A1);
    dev(0, A1, 0, 1, -40, "SMC-Mixer");             /* saved: not in the ADD list */
    dev(1, F1, 0, 1, -70, "Felucca FM-1");
    dev(2, A2, 1, 0, -50, "TV");
    frame();
    row(5);                                         /* + ADD A DEVICE */
    targets = bh.targets[0] + bh.targets[1];
    press(B_OCTUP);
    ok("add_flow: OCT+ on ADD -> the list of nearby devices not saved, BLE-MIDI first", btp.add && btp.an == 2u &&
       !memcmp(btp.adev[0].addr, F1, 6) && !memcmp(btp.adev[1].addr, A2, 6));
    ok("add_flow: the page (and its list) want a scan", bt_dv.page == 1u);
    ok("no_drop_on_add: opening it touches no link's target", bh.targets[0] + bh.targets[1] == targets &&
       bh.slot[0].st == BLE_LS_UP);
    press(B_OCTUP);                                 /* the FM-1, first */
    ok("add_flow: OCT+ on one -> saved first (an FM-1 by its name), back on the page", !btp.add &&
       bt_devs_get(0, &d) && !memcmp(d.addr, F1, 6) && d.kind == BT_KIND_FM1 && bt_devs_n() == 2u);
    ok("page_scan: back on the page its scan goes on (ON / NEAR / AWAY true there)", bt_dv.page == 1u);
    row(bt_page_rows() - 1u);
    press(B_OCTUP);
    press(B_OCTDN);
    ok("add_flow: OCT- in the list -> back on the page, nothing added", !btp.add && ui.menu == BT_MENU && bt_devs_n() == 2u);
}

static void test_add_freeze(void)                    /* the user: the list kept changing; an 8 s scan, then it stays */
{
    char t[40];
    fresh();
    k1(1);
    bs.state = BLE_ST_IDLE;
    dev(0, A1, 0, 1, -60, "SMC-Mixer");
    frame();
    row(4);                                         /* + ADD A DEVICE (nothing saved) */
    press(B_OCTUP);
    bt_state_text(t);
    ok("add_freeze: ADD opens an 8 s scan, counted down on the state line", str_eq(t, "SCANNING 8 S") && btp.an == 1u);
    fm1_ms += 2500u;
    dev(1, A2, 1, 1, -30, "Xkey Air 25");           /* found meanwhile, stronger */
    frame();
    bt_state_text(t);
    ok("add_freeze: during the scan the list takes new devices", str_eq(t, "SCANNING 6 S") && btp.an == 2u);
    fm1_ms += 5600u;                                /* 8.1 s */
    frame();
    bt_state_text(t);
    ok("add_freeze: then the scan's end: ADD: PICK A DEVICE", str_eq(t, "ADD: PICK A DEVICE"));
    {
        ble_ui_dev_t was[2];
        memcpy(was, btp.adev, sizeof was);
        dev(2, F1, 0, 1, -20, "Felucca FM-1");      /* found after the end, strongest */
        bs.dev[0].rssi = -10;                       /* a signal changes */
        bs.dev[1].midi = 0;                         /* (it would sort elsewhere now) */
        frame();
        ok("add_freeze: after it the list stays as it was (no new device, no re-sort, no change)", btp.an == 2u &&
           !memcmp(btp.adev, was, sizeof was));
    }
    fm1_ms += 60000u;                               /* (the scan list ages its entries out meanwhile) */
    bs.ndev = 0;
    frame();
    ok("add_freeze: and stays while the scan list ages out", btp.an == 2u);
    press(B_OCTUP);                                 /* pick the first */
    ok("add_freeze: a pick from the kept list saves it", bt_devs_n() == 1u && !btp.add);
    fresh();
    k1(1);
    bs.state = BLE_ST_IDLE;
    frame();
    row(4);
    press(B_OCTUP);
    fm1_ms += 9000u;
    frame();
    bt_state_text(t);
    ok("add_freeze: nothing found -> NOTHING FOUND", str_eq(t, "NOTHING FOUND") && btp.an == 0u);
    dev(0, A1, 0, 1, -60, "SMC-Mixer");
    press(B_OCTUP);
    bt_state_text(t);
    ok("add_freeze: OCT+ then scans again", str_eq(t, "SCANNING 8 S") && btp.an == 1u);
}

static void test_on_off(void)
{
    fresh();
    k1(-1);
    ok("BLUETOOTH, a knob left while OFF: nothing", !btp.on && !bs.starts && !bs.wants);
    k1(1);
    ok("BLUETOOTH, a knob right: ON, the stack started once, links wanted",
       btp.on && bs.starts == 1u && bs.want == 1 && bs.state == BLE_ST_STARTING);
    k1(1);
    ok("ON again does not start it twice", bs.starts == 1u);
    bs.state = BLE_ST_IDLE;
    k1(-1);
    ok("a knob left: OFF, links no longer wanted (the stack stays started)", !btp.on && bs.want == 0);
    press(B_OCTUP);
    ok("OCT+ toggles: ON (no second start: the stack runs until power-off)", btp.on && bs.starts == 1u && bs.want == 1);
    press(B_OCTUP);
    ok("OCT+ toggles: OFF", !btp.on && bs.want == 0);
    turn(EN_K1 + 2u, 1); frame();
    ok("any of KNOB 1..4 changes the row, as in the menu", btp.on);
    fresh();
    bs.state = BLE_ST_FAILED;
    k1(1);
    ok("after a failed start ON does nothing but say so (only a power cycle starts the stack again)",
       !btp.on && !bs.starts);
}

static void test_mode(void)
{
    fresh();
    row(3);
    k1(1);
    ok("MODE, a knob right: SMC DAW, saved", saved().mode == BT_MODE_SMC);
    k1(1);
    ok("MODE: MIDI", saved().mode == BT_MODE_MIDI);
    k1(1);
    k1(1);
    ok("MODE stops at FM-LINK", saved().mode == BT_MODE_LINK);
    k1(-1);
    k1(-1);
    k1(-1);
    k1(-1);
    ok("MODE stops at AUTO", saved().mode == BT_MODE_AUTO);
    press(B_OCTUP);
    press(B_OCTUP);
    press(B_OCTUP);
    press(B_OCTUP);
    ok("OCT+ steps MODE and wraps", saved().mode == BT_MODE_AUTO);
}

static void text_is_(const char *what, const char *want);
static void test_role(void)
{
    fresh();
    k1(1);
    ok("ROLE: HOST by default, and BLUETOOTH ON hands the BLE unit the saved role", !saved().role && bs_role == 0);
    bs.state = BLE_ST_IDLE;
    save(A1, "SMC-Mixer");
    frame();
    row(2);
    k1(1);
    ok("role_two: a knob right -> DEVICE, saved, handed over", saved().role == 1u && bs_role == 1);
    k1(1);
    ok("role_two: it stops at DEVICE", saved().role == 1u);
    ok("role_two: ROLE DEVICE -> no device lines, no ADD (four rows)", bt_page_rows() == 4u);
    turn(EN_PRESET, 1); frame();
    turn(EN_PRESET, 1); frame();
    ok("role_two: the cursor wraps after MODE", btp.sel == 0u);
    bs.state = BLE_ST_ADVERTISING;
    text_is_("ROLE DEVICE: the state line while advertising", "WAITING FOR A HOST");
    row(2);
    press(B_OCTUP);
    ok("role_two: OCT+ toggles -> HOST, saved, handed over; the lines are back", !saved().role && bs_role == 0 &&
       bt_page_rows() == 6u);
}

static void test_autostart_row(void)
{
    bt_set_t s;
    fresh();
    row(1);
    ok("the second row is AUTOSTART, OFF by default", btp.sel == 1u && !saved().autostart);
    k1(1);
    s = saved();
    ok("AUTOSTART, a knob right: ON, saved; Bluetooth itself not started by it", s.autostart && !bs.starts && !btp.on);
    k1(-1);
    ok("a knob left: OFF, saved", !saved().autostart);
    press(B_OCTUP);
    ok("OCT+ toggles: ON", saved().autostart);
    row(3);
    press(B_OCTUP);
    s = saved();
    ok("AUTOSTART is kept when MODE changes", s.autostart && s.mode == BT_MODE_SMC);
}

/* the autostart: once per power-on, BT_AUTO_MS after the main loop first served Bluetooth, only with AUTOSTART ON and
 * after a clean boot (main.c boot_clean: no crash, no WDT hang, no failed boot before this one) */
static void autostart_power_on(int on, int clean)
{
    bt_set_t s = {{0}, 0, BT_MODE_AUTO, 0, (uint8_t)on};
    ui_power_on();
    memset(&bs, 0, sizeof bs);
    memset(&btp, 0, sizeof btp);
    memset(&bt_auto, 0, sizeof bt_auto);
    bt_set_put(&s);
    boot_clean = (uint8_t)clean;
    fm1_ms = 1234;
}
static void test_autostart(void)
{
    uint32_t i;
    autostart_power_on(1, 1);
    bt_page_auto();
    fm1_ms += BT_AUTO_MS - 16u;
    bt_page_auto();
    ok("autostart: not before BT_AUTO_MS of the main loop", !bs.starts && !btp.on);
    fm1_ms += 16u;
    bt_page_auto();
    ok("autostart: then Bluetooth starts as BLUETOOTH ON does (started, links wanted)",
       bs.starts == 1u && btp.on && bs.want == 1);
    bs.state = BLE_ST_IDLE;
    btp.on = 0;                                      /* the user turns it OFF on the page */
    ble_ui_want(0);
    for (i = 0; i < 100u; i++) {
        fm1_ms += 1000u;
        bt_page_auto();
    }
    ok("autostart: once per power-on (OFF on the page stays OFF)", bs.starts == 1u && !btp.on && bs.want == 0);
    autostart_power_on(0, 1);
    for (i = 0; i < 20u; i++) {
        bt_page_auto();
        fm1_ms += 1000u;
    }
    ok("AUTOSTART OFF: nothing starts", !bs.starts && !btp.on && !bs.wants);
    autostart_power_on(1, 0);
    for (i = 0; i < 20u; i++) {
        bt_page_auto();
        fm1_ms += 1000u;
    }
    ok("after a crash, a hang or a failed boot: no autostart (the way to UBOOT and the updater stays clear)",
       !bs.starts && !btp.on && !bs.wants);
    autostart_power_on(1, 1);
    ui.uboot = 1;                                    /* UPDATE MODE on the screen */
    for (i = 0; i < 20u; i++) {
        bt_page_auto();
        fm1_ms += 1000u;
    }
    ok("autostart: not while UPDATE MODE is up", !bs.starts);
    ui.uboot = 0;
    bt_page_auto();
    ok("... and not later in that power-on either", !bs.starts);
    autostart_power_on(1, 1);
    fm1_ms = 0xFFFFFFFFu - 1000u;                    /* the ms counter wrapping meanwhile */
    bt_page_auto();
    fm1_ms += BT_AUTO_MS;
    bt_page_auto();
    ok("autostart: the delay survives the ms counter wrapping", bs.starts == 1u);
}

static void test_close(void)
{
    fresh();
    k1(1);
    bs.state = BLE_ST_IDLE;
    row(4);
    press(B_OCTUP);                                 /* ADD (no saved device: the 5th row) */
    fm1_in.buttons |= 1u << panel.btn[B_HOME];       /* HOME pressed: the menu closes, from the page too */
    host_pressed |= 1u << panel.btn[B_HOME];
    frame();
    fm1_in.buttons = 0;
    frame();
    ok("HOME closes the menu from the page", !ui.menu);
    ok("closing ends the page's scan and its list; Bluetooth stays ON", bt_dv.page == 0u && !btp.add && btp.on &&
       bs.want == 1);
}

static void text_is(const char *what, const char *want)
{
    char t[40];
    bt_state_text(t);
    if (!str_eq(t, want))
        printf("      (the state line is '%s', not '%s')\n", t, want);
    ok(what, str_eq(t, want));
}
static void text_is_(const char *what, const char *want) { text_is(what, want); }
static void test_state_text(void)
{
    fresh();
    text_is("state: OFF at power-on", "OFF");
    k1(1);
    text_is("state: STARTING", "STARTING");
    bs.state = BLE_ST_IDLE;
    text_is("state: on, nothing saved", "ON, NO DEVICE SAVED");
    save(A1, "SMC-Mixer");
    text_is("state: on, saved, none linked", "ON, SEARCHING");
    up(BLE_SLOT_CTRL, A1);
    text_is("state: one linked", "ON, 1 CONNECTED");
    up(BLE_SLOT_FM1, F1);
    text_is("state: two linked", "ON, 2 CONNECTED");
    bs.state = BLE_ST_FAILED;
    bs.err = "NO RF TRIM IN FLASH";
    text_is("state: FAILED and the reason", "FAILED NO RF TRIM IN FLASH");
    bs.state = BLE_ST_IDLE;
    k1(-1);
    text_is("state: OFF once turned off (the stack idles)", "OFF");
}

static void test_icon_state(void)                    /* the top bar's Bluetooth icon (design 5.6) */
{
    fresh();
    ok("icon_state: Bluetooth off -> none", bt_head_icon() == 0u);
    k1(1);
    bs.state = BLE_ST_IDLE;
    ok("icon_state: on, nothing linked -> struck", bt_head_icon() == 1u);
    up(BLE_SLOT_FM1, F1);
    ok("icon_state: a link up -> lit", bt_head_icon() == 2u);
    up(BLE_SLOT_FM1, 0);
    bs.state = BLE_ST_RECEIVING;                     /* the device role: its host linked */
    ok("icon_state: the device role linked -> lit", bt_head_icon() == 2u);
    bs.state = BLE_ST_ADVERTISING;
    ok("icon_state: the device role waiting -> struck", bt_head_icon() == 1u);
}

int main(void)
{
    test_icon_state();
    test_menu_row();
    test_rows_host();
    test_device_lines();
    test_forget_row();
    test_use_now();
    test_add_flow();
    test_add_freeze();
    test_on_off();
    test_mode();
    test_role();
    test_autostart_row();
    test_autostart();
    test_close();
    test_state_text();
    printf(fails ? "bt_page: %d FAILED\n" : "bt_page: all checks passed\n", fails);
    return fails != 0;
}
