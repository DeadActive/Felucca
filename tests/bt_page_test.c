/* SPDX-License-Identifier: GPL-3.0-only
 * BLE MIDI: 2026 DEADACTIVE */
/* The Bluetooth page (firmware/src/bt_page.c; docs/superpowers/specs/2026-10-13-ble-midi-design.md section 9) on the
 * host, built with FELUCCA_BLE=1 and the UI harness, driven through the real input (ui_input, menu_input): the row in
 * MENU > SYSTEM, the cursor, BLUETOOTH ON / OFF, MODE, DEVICE, SCAN, picking a device, the way back, the state line.
 * The BLE unit's entry points are stubs (ble_ui_stub.h: `bs`). */
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
#include "../firmware/src/bt_page.c"

static int fails;
static void ok(const char *what, int good)
{
    printf("%s  %s\n", good ? "ok  " : "FAIL", what);
    if (!good)
        fails++;
}
static const uint8_t A1[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0xC6}, A2[6] = {0x9A, 0x00, 0x00, 0x00, 0x12, 0xC4};
static void dev(uint32_t i, const uint8_t a[6], uint32_t type, int midi, int rssi, const char *name)
{
    ble_ui_dev_t *d = &bs.dev[i];
    memset(d, 0, sizeof *d);
    memcpy(d->addr, a, 6);
    d->type = (uint8_t)type;
    d->midi = (uint8_t)midi;
    d->rssi = (int8_t)rssi;
    str_cpy(d->name, name, sizeof d->name);
    if (bs.ndev <= i)
        bs.ndev = i + 1u;
}
static void fresh(void)                              /* power-on: Bluetooth off, nothing remembered, the page open */
{
    ui_power_on();
    memset(&favorites, 0, sizeof favorites);
    memset(&bs, 0, sizeof bs);
    memset(&btp, 0, sizeof btp);
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
    ok("MENU > SYSTEM has seven rows, BLUETOOTH the fifth, before INFO",
       n == 7u && at == 4u && MI_BT + 1u == MI_INFO && str_eq(MI_NAME[MI_BT], "BLUETOOTH") && n <= MTAB_ROWS);
    ok("BLUETOOTH is an action row (no value), as INFO and ABOUT", !menu_valued(MI_BT) && MI_BT >= MI_VALUES);
    fresh();
    ok("OCT+ on the row opens the page", ui.menu == BT_MENU && btp.sel == 0u);
    press(B_OCTDN);
    ok("OCT- goes back to the menu's rows, on BLUETOOTH", ui.menu == 1u && ui.menu_sel == MI_BT);
    ok("leaving the page ends its pause of reconnecting", bs.pauses >= 1u && bs.pause == 0);
    press(B_OCTUP);
    ok("INFO and ABOUT still open from their rows", ui.menu == BT_MENU);
    press(B_OCTDN);
    ui.menu_sel = MI_INFO;
    press(B_OCTUP);
    ok("INFO still opens from its row", ui.menu == 3u);
    press(B_OCTDN);
    ui.menu_sel = MI_ABOUT;
    press(B_OCTUP);
    ok("ABOUT still opens from its row", ui.menu == 2u);
}

static void test_cursor(void)
{
    fresh();
    turn(EN_PRESET, 1); frame();
    ok("PRESETS moves the cursor a row", btp.sel == 1u);
    turn(EN_PRESET, -1); frame();
    turn(EN_PRESET, -1); frame();
    ok("the cursor wraps past the first row to the last (SCAN, no devices)", btp.sel == 3u);
    turn(EN_PRESET, 1); frame();
    ok("the cursor wraps past the last row to the first", btp.sel == 0u);
    bs.state = BLE_ST_IDLE;
    dev(0, A1, 0, 1, -59, "SMC-Mixer");
    dev(1, A2, 1, 0, -80, "");
    frame();
    row(5);
    ok("the devices found are rows after SCAN", btp.sel == 5u && btp.n == 2u);
    bs.ndev = 0;                                     /* a new scan emptied the list */
    frame();
    ok("the cursor comes back onto a row when the list shrinks", btp.sel <= 3u);
}

static void test_on_off(void)
{
    fresh();
    k1(-1);
    ok("BLUETOOTH, a knob left while OFF: nothing", !btp.on && !bs.starts && !bs.wants);
    k1(1);
    ok("BLUETOOTH, a knob right: ON, the stack started once, reconnecting wanted",
       btp.on && bs.starts == 1u && bs.want == 1 && bs.state == BLE_ST_STARTING);
    k1(1);
    ok("ON again does not start it twice", bs.starts == 1u);
    bs.state = BLE_ST_IDLE;
    k1(-1);
    ok("a knob left: OFF, reconnecting no longer wanted (the stack stays started)", !btp.on && bs.want == 0);
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
    /* the remembered device becomes the target when Bluetooth is turned on */
    fresh();
    {
        bt_set_t s = {{0}, 1, BT_MODE_AUTO, 1};
        memcpy(s.addr, A2, 6);
        bt_set_put(&s);
    }
    k1(1);
    ok("ON hands the remembered device to the BLE unit as the target",
       bs.targets == 1u && !memcmp(bs.taddr, A2, 6) && bs.ttype == 1u);
}

static void test_mode(void)
{
    fresh();
    row(1);
    k1(1);
    ok("MODE, a knob right: SMC DAW, saved", saved().mode == BT_MODE_SMC);
    k1(1);
    k1(1);
    ok("MODE stops at MIDI", saved().mode == BT_MODE_MIDI);
    k1(-1);
    k1(-1);
    k1(-1);
    ok("MODE stops at AUTO", saved().mode == BT_MODE_AUTO);
    press(B_OCTUP);
    press(B_OCTUP);
    press(B_OCTUP);
    ok("OCT+ steps MODE and wraps", saved().mode == BT_MODE_AUTO);
    press(B_OCTUP);
    ok("OCT+ steps MODE", saved().mode == BT_MODE_SMC);
}

static void test_scan_pick_forget(void)
{
    bt_set_t s;
    fresh();
    row(3);
    press(B_OCTUP);
    ok("SCAN while Bluetooth is OFF: no scan", !bs.scans);
    row(0);
    k1(1);
    bs.state = BLE_ST_IDLE;
    row(3);
    press(B_OCTUP);
    ok("SCAN, OCT+: a scan", bs.scans == 1u);
    bs.state = BLE_ST_SCANNING;
    bs.left = 7;
    dev(0, A2, 1, 0, -40, "Phone");                  /* the BLE unit's order is by signal: the MIDI one is listed first */
    dev(1, A1, 0, 1, -59, "SMC-Mixer");
    frame();
    ok("the list: BLE-MIDI devices first", btp.n == 2u && !memcmp(btp.dev[0].addr, A1, 6) && !memcmp(btp.dev[1].addr, A2, 6));
    row(4);
    bs.targets = 0;
    press(B_OCTUP);
    s = saved();
    ok("a device row, OCT+: remembered (saved), the target, reconnecting wanted",
       s.has && !memcmp(s.addr, A1, 6) && s.type == 0u && bs.targets == 1u && !memcmp(bs.taddr, A1, 6) && bs.want == 1);
    ok("picking a device keeps MODE", s.mode == BT_MODE_AUTO);
    row(5);
    press(B_OCTUP);
    s = saved();
    ok("another device row: it replaces the remembered one (a random address keeps its type)",
       !memcmp(s.addr, A2, 6) && s.type == 1u && bs.ttype == 1u);
    row(2);
    k1(1);
    ok("DEVICE, a knob right: nothing", saved().has);
    bs.targets = 0;
    k1(-1);
    s = saved();
    ok("DEVICE, a knob left: forgotten (saved), no target", !s.has && bs.targets == 1u && !bs.taddr[0] && !bs.taddr[5]);
    fresh();
    bs.state = BLE_ST_FAILED;
    dev(0, A1, 0, 1, -59, "SMC-Mixer");
    frame();
    row(4);
    press(B_OCTUP);
    ok("after a failed start a device row does nothing", !saved().has && !bs.targets);
}

static void test_close(void)
{
    fresh();
    k1(1);
    bs.pause = 1;
    fm1_in.buttons |= 1u << panel.btn[B_HOME];       /* HOME pressed: the menu closes, from the page too */
    host_pressed |= 1u << panel.btn[B_HOME];
    frame();
    fm1_in.buttons = 0;
    frame();
    ok("HOME closes the menu from the page", !ui.menu);
    ok("closing ends the pause; Bluetooth stays ON", bs.pause == 0 && btp.on && bs.want == 1);
}

static void text_is(const char *what, const char *want)
{
    char t[40];
    bt_state_text(t);
    if (!str_eq(t, want))
        printf("      (the state line is '%s', not '%s')\n", t, want);
    ok(what, str_eq(t, want));
}
static void test_state_text(void)
{
    char t[24];
    fresh();
    text_is("state: OFF at power-on", "OFF");
    k1(1);
    text_is("state: STARTING", "STARTING");
    bs.state = BLE_ST_IDLE;
    text_is("state: IDLE", "IDLE");
    bs.state = BLE_ST_SCANNING;
    bs.left = 10;
    text_is("state: SCANNING with the seconds left", "SCANNING 10 S");
    bs.state = BLE_ST_CONNECTING;
    text_is("state: CONNECTING", "CONNECTING");
    bs.state = BLE_ST_DISCOVERING;
    text_is("state: CONNECTING while discovering", "CONNECTING");
    bs.state = BLE_ST_RECEIVING;
    memset(conn_name, 0, sizeof conn_name);
    str_cpy(conn_name, "SMC-Mixer", sizeof conn_name);
    conn_up = 1;
    text_is("state: CONNECTED and the device's name", "CONNECTED SMC-Mixer");
    bs.state = BLE_ST_FAILED;
    bs.err = "NO RF TRIM IN FLASH";
    text_is("state: FAILED and the reason", "FAILED NO RF TRIM IN FLASH");
    bs.state = BLE_ST_IDLE;
    k1(-1);
    text_is("state: OFF once turned off (the stack idles)", "OFF");
    /* DEVICE's text: NONE, the address until the name was seen, then the name */
    fresh();
    bt_dev_text(t);
    ok("DEVICE: NONE", str_eq(t, "NONE"));
    {
        bt_set_t s = {{0}, 0, BT_MODE_AUTO, 1};
        memcpy(s.addr, A1, 6);
        bt_set_put(&s);
    }
    bt_dev_text(t);
    ok("DEVICE: the address in brief (its first two bytes and its last), most significant first, until the device is seen",
       str_eq(t, "C6:55:..:11"));
    bs.state = BLE_ST_IDLE;
    dev(0, A1, 0, 1, -59, "SMC-Mixer");
    frame();
    bt_dev_text(t);
    ok("DEVICE: the name once the device was seen in a scan", str_eq(t, "SMC-Mixer"));
}

int main(void)
{
    test_menu_row();
    test_cursor();
    test_on_off();
    test_mode();
    test_scan_pick_forget();
    test_close();
    test_state_text();
    printf(fails ? "bt_page: %d FAILED\n" : "bt_page: all checks passed\n", fails);
    return fails != 0;
}
