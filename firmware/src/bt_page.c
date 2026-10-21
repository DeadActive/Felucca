/* SPDX-License-Identifier: GPL-3.0-only
 * BLE MIDI: 2026 DEADACTIVE (after Hortator's bt_page.c) */
/* FELUCCA_BLE (felucca.c includes it after ble/ble_core.h, bt_set.c and bt_midi.c; ui_menu.c declares its entry
 * points): the Bluetooth page, MENU > SYSTEM > BLUETOOTH, ui.menu BT_MENU. A state line (and the last error), then
 * the rows BLUETOOTH, AUTOSTART, ROLE, MODE, DEVICE, SCAN and up to 8 devices found (BLE-MIDI ones first), two in
 * view. ROLE DEVICE: the FM-1 is the BLE-MIDI device a host (a computer, a tablet) connects to; DEVICE, SCAN and the
 * list are not shown then. As in the menu: PRESETS moves the cursor (it wraps), any of KNOB 1..4 changes the row, OCT+ acts on it, OCT- goes back to
 * the menu's rows, HOME closes the menu.
 * BLUETOOTH ON is the only call of ble_user_start in the core (gate 3: tools/check_ble_boot.py TRIGGERS has
 * bt_page_on); Bluetooth is off at every power-on and ON is not saved, unless AUTOSTART is ON: then bt_page_auto
 * turns it on once, BT_AUTO_MS after the main loop began, after a clean boot only. AUTOSTART, ROLE, MODE and the
 * remembered device are saved at once (bt_set.c). Main loop only. */
#define BT_ROWS 6u                                  /* BLUETOOTH AUTOSTART ROLE MODE DEVICE SCAN, then the devices */
#define BT_DEV_ROWS 4u                              /* ROLE DEVICE: BLUETOOTH AUTOSTART ROLE MODE only */
#define BT_SHOW 8u                                  /* devices listed (the scan keeps 16) */
#define BT_VIS 2u                                   /* device rows in view (the cursor scrolls them) */
#define BT_SY 27                                    /* the state line's panel; the last error under it */
#define BT_SH 20
#define BT_EY 48
#define BT_RY 67                                    /* the five rows, 17 px, 1 px apart */
#define BT_RH 17
#define BT_SPLIT 121                                /* the two bands meet in the gap above DEVICE */
#define BT_DY 182                                   /* the device rows, 17 px, 1 px apart */
#define BT_DH 17
static const char *const BT_MODE_N[3] = {"AUTO", "SMC DAW", "MIDI"};
static uint32_t bt_role(void) { bt_set_t s; bt_set_get(&s); return s.role; }   /* 1: ROLE DEVICE */
static uint32_t bt_rows(void) { return bt_role() ? BT_DEV_ROWS : BT_ROWS; }   /* the rows above the devices */
static struct {
    uint8_t on;                                     /* BLUETOOTH ON (not saved) */
    uint8_t sel, top, n;                            /* the cursor, the first device row in view, the devices listed */
    char dname[24];                                 /* the remembered device's name, once seen this power-on */
    ble_ui_dev_t dev[BT_SHOW];                      /* the devices as drawn: a pick takes these */
} btp __attribute__((section(".pool")));

/* a name as the screen can show it: the font's characters only (a device's name is any bytes) */
static void bt_name(char *o, const char *s, uint32_t n)
{
    uint32_t i;
    for (i = 0; i + 1u < n && s[i]; i++)
        o[i] = s[i] >= 0x20 && s[i] < 0x7F ? s[i] : '?';
    o[i] = 0;
}

/* AA:BB:CC:DD:EE:FF, the most significant byte first (ble_central.c put_dev); AA:BB:..:FF when brief */
static void bt_addr_txt(char *o, const uint8_t a[6], int brief)
{
    static const char H[] = "0123456789ABCDEF";
    uint32_t i;
    for (i = 0; i < 6u; i++) {
        o[i * 3u] = H[a[5u - i] >> 4];
        o[i * 3u + 1u] = H[a[5u - i] & 15u];
        o[i * 3u + 2u] = ':';
    }
    o[17] = 0;
    if (brief)
        o[6] = '.', o[7] = '.', o[8] = ':', o[9] = o[15], o[10] = o[16], o[11] = 0;
}

static void bt_dev_text(char *o)                    /* DEVICE: the name once seen, else the address in brief; NONE (24 B) */
{
    bt_set_t s;
    bt_set_get(&s);
    if (!s.has)
        str_cpy(o, "NONE", 24);
    else if (btp.dname[0])
        bt_name(o, btp.dname, 24);
    else
        bt_addr_txt(o, s.addr, 1);
}

static void bt_state_text(char *o)                  /* the state line (40 B) */
{
    static const char *const ST[] = {"OFF", "STARTING", "IDLE", "SCANNING ", "CONNECTING", "CONNECTING",
                                     "CONNECTED ", "CONNECTED ", "FAILED ", "WAITING FOR A HOST"};
    uint32_t st = ble_ui_state(), code, n;
    const char *e = ble_ui_err(&code);
    char t[24];
    str_cpy(o, st <= BLE_ST_ADVERTISING ? ST[st] : "?", 40);
    if (!btp.on && (st == BLE_ST_IDLE || st == BLE_ST_OFF))
        str_cpy(o, "OFF", 40);
    n = str_len(o);
    if (st == BLE_ST_SCANNING) {
        fmt_int(o + n, (int32_t)ble_ui_scan_left());
        str_cpy(o + str_len(o), " S", 4);
    } else if (st == BLE_ST_FAILED) {
        str_cpy(o + n, e, 40 - n);
    } else if (st == BLE_ST_SUBSCRIBED || st == BLE_ST_RECEIVING) {
        char raw[24];
        if (ble_ui_conn_name(raw) && raw[0])
            raw[23] = 0, bt_name(t, raw, sizeof t);
        else
            bt_dev_text(t);
        str_cpy(o + n, t, 40 - n);
    }
}

/* the scan list as the page shows it. With the cursor off the device rows: the BLE unit's list, BLE-MIDI devices
 * first, each part in the unit's order (by signal), the first 8 kept. With the cursor on a device row the rows do
 * not move under it (a scan starting empties the unit's list, a signal changing re-sorts it, and a press would land
 * on another device than the one aimed at): each row is brought up to date in place when its device is still
 * listed, kept as it is when not, and devices seen since are added below. The remembered device's name is noted when
 * it is listed; the cursor is kept on a row and its row in view. bt_page_draw calls it; the input acts on its rows */
static int bt_same(const ble_ui_dev_t *a, const ble_ui_dev_t *b)
{
    return (a->type & 1u) == (b->type & 1u) && !memcmp(a->addr, b->addr, 6);
}
static void bt_list(void)
{
    ble_ui_dev_t d;
    bt_set_t s;
    uint32_t i, k, pass, n = ble_ui_devs(), hold = btp.sel >= BT_ROWS && btp.sel < BT_ROWS + btp.n;
    char t[24];
    bt_set_get(&s);
    if (s.role) {                                   /* ROLE DEVICE: no list, the cursor on the four rows */
        btp.n = 0;
        btp.top = 0;
        if (btp.sel >= BT_DEV_ROWS)
            btp.sel = BT_DEV_ROWS - 1u;
        return;
    }
    if (!hold)
        btp.n = 0;
    for (pass = 0; pass < 2u; pass++)
        for (i = 0; i < n && i < 16u; i++) {
            if (!ble_ui_dev(i, &d) || (d.midi != 0) != (pass == 0u))
                continue;
            d.name[sizeof d.name - 1u] = 0;
            if (s.has && d.name[0] && (d.type & 1u) == s.type && !memcmp(d.addr, s.addr, 6))
                str_cpy(btp.dname, d.name, sizeof btp.dname);
            for (k = 0; hold && k < btp.n && !bt_same(&btp.dev[k], &d); k++)
                ;
            if (hold && k < btp.n)
                btp.dev[k] = d;                     /* (in place: its signal, its name once it is known) */
            else if (btp.n < BT_SHOW)
                btp.dev[btp.n++] = d;
        }
    if (s.has && !btp.dname[0] && ble_ui_conn_name(t) && t[0]) {   /* (linked: the target) */
        t[23] = 0;
        str_cpy(btp.dname, t, sizeof btp.dname);
    }
    if (btp.sel >= BT_ROWS + btp.n)
        btp.sel = (uint8_t)(BT_ROWS - 1u);
    if (btp.sel < BT_ROWS)
        btp.top = 0;
    else if (btp.sel - BT_ROWS < btp.top)
        btp.top = (uint8_t)(btp.sel - BT_ROWS);
    else if (btp.sel - BT_ROWS >= btp.top + BT_VIS)
        btp.top = (uint8_t)(btp.sel - BT_ROWS - BT_VIS + 1u);
}

/* BLUETOOTH ON: the remembered device the target, reconnecting wanted, the stack started if it is not (the user's
 * trigger: the only call of ble_user_start in the core, gate 3) */
__attribute__((noinline)) static void bt_page_on(void)
{
    bt_set_t s;
    if (ble_ui_state() == BLE_ST_FAILED) {          /* the stack cannot stop: only a power cycle starts it again */
        ui_message("POWER OFF AND ON TO RETRY");
        return;
    }
    if (btp.on)
        return;
    btp.on = 1;
    bt_set_get(&s);
    ble_ui_role(s.role);                            /* (applied once the stack is up and idle) */
    ble_ui_target(s.addr, s.type);
    ble_ui_want(1);
    if (ble_ui_state() == BLE_ST_OFF)
        ble_user_start();
}

/* AUTOSTART (core_ble_api.c bt_service, every main-loop pass): once per power-on, BT_AUTO_MS after the main loop
 * first served Bluetooth, BLUETOOTH ON as from the page, when AUTOSTART is ON and the boot was clean (main.c
 * boot_clean: no crash, no WDT hang and no failed boot before this one). So a Bluetooth start that crashes or hangs
 * the FM-1 is not tried again on the next power-on: the way to UBOOT and the updater stays clear. Not while UPDATE
 * MODE is up, nor later in that power-on. Gate 3 (tools/check_ble_boot.py) allows it only from the main loop's
 * service */
#define BT_AUTO_MS 3000u
static struct { uint8_t armed, done; uint32_t t0; } bt_auto;
static uint8_t boot_clean;                          /* main.c (included after this file): its tentative definition */
__attribute__((noinline)) static void bt_page_auto(void)
{
    bt_set_t s;
    if (bt_auto.done)
        return;
    if (!bt_auto.armed) {
        bt_auto.armed = 1;
        bt_auto.t0 = fm1_ms;
        return;
    }
    if (ui.uboot) {
        bt_auto.done = 1;
        return;
    }
    if (fm1_ms - bt_auto.t0 < BT_AUTO_MS)
        return;
    bt_auto.done = 1;
    bt_set_get(&s);
    if (!s.autostart)
        return;
    if (!boot_clean) {
        ui_message("BT AUTOSTART SKIPPED");          /* (after a crash, a hang or a failed boot) */
        return;
    }
    bt_page_on();
}

static void bt_pick(const ble_ui_dev_t *d)          /* a device row, OCT+: remembered (saved) and connected */
{
    bt_set_t s;
    bt_set_get(&s);
    memcpy(s.addr, d->addr, 6);
    s.type = d->type & 1u;
    bt_set_put(&s);
    str_cpy(btp.dname, d->name, sizeof btp.dname);
    btp.on = 1;
    ble_ui_target(s.addr, s.type);
    ble_ui_want(1);
}

static void bt_forget(void)                         /* DEVICE, a knob left: none (saved); a link drops */
{
    bt_set_t s;
    bt_set_get(&s);
    memset(s.addr, 0, sizeof s.addr);
    s.type = 0;
    bt_set_put(&s);
    btp.dname[0] = 0;
    ble_ui_target(s.addr, 0);
}

static void bt_page_open(void)                      /* MENU > SYSTEM > BLUETOOTH, OCT+ */
{
    ui.menu = BT_MENU;
    btp.sel = btp.top = 0;
    ui.force = 1;
}

static void bt_page_close(void)                     /* OCT- or HOME: reconnecting goes on */
{
    ble_ui_pause(0);
}

static void bt_page_input(uint32_t oct)             /* oct: ui_input.c oct_taps (bit 0 OCT-, bit 1 OCT+) */
{
    uint32_t ok = (oct >> 1) & 1u, rows, st = ble_ui_state(), k;
    int32_t s;
    bt_set_t set;
    if (oct & 1u) {
        bt_page_close();
        ui.menu = 1;
        ui.force = 1;
        enc_drop();
        return;
    }
    rows = bt_rows() + btp.n;                       /* the rows as drawn (bt_list: they do not move under the cursor) */
    if ((s = panel_enc(EN_PRESET)) != 0)            /* one row per detent, wrapping (as the menu's rows) */
        btp.sel = (uint8_t)((btp.sel + (s > 0 ? 1u : rows - 1u)) % rows);
    for (s = 0, k = 0; k < 4u; k++)                 /* KNOB 1..4, any of them */
        s += panel_enc(EN_K1 + k);
    switch (btp.sel) {
    case 0:                                         /* BLUETOOTH: a knob right ON, left OFF; OCT+ toggles */
        if (s > 0 || (ok && !btp.on)) {
            bt_page_on();
        } else if ((s < 0 || ok) && btp.on) {
            btp.on = 0;
            ble_ui_want(0);
        }
        break;
    case 1:                                         /* AUTOSTART: a knob right ON, left OFF; OCT+ toggles; saved */
        bt_set_get(&set);
        k = s > 0 ? 1u : s < 0 ? 0u : ok ? !set.autostart : set.autostart;
        if (k != set.autostart) {
            set.autostart = (uint8_t)k;
            bt_set_put(&set);
        }
        break;
    case 2:                                         /* ROLE: a knob right DEVICE, left HOST; OCT+ toggles; saved */
        bt_set_get(&set);
        k = s > 0 ? 1u : s < 0 ? 0u : ok ? !set.role : set.role;
        if (k != set.role) {
            set.role = (uint8_t)k;
            bt_set_put(&set);
            ble_ui_role((int)k);                    /* (the BLE unit ends a scan, a link or advertising, then switches) */
            btp.n = btp.top = 0;
        }
        break;
    case 3:                                         /* MODE: a knob steps and stops at the ends; OCT+ wraps */
        bt_set_get(&set);
        k = s > 0 ? (set.mode < BT_MODE_MIDI ? set.mode + 1u : BT_MODE_MIDI)
          : s < 0 ? (set.mode ? set.mode - 1u : 0u) : ok ? (set.mode + 1u) % 3u : set.mode;
        if (k != set.mode) {
            set.mode = (uint8_t)k;
            bt_set_put(&set);
        }
        break;
    case 4:                                         /* DEVICE: a knob left forgets it */
        bt_set_get(&set);
        if (s < 0 && set.has)
            bt_forget();
        break;
    case 5:                                         /* SCAN (BLUETOOTH ON only) */
        if (ok && st == BLE_ST_FAILED)
            ui_message("POWER OFF AND ON TO RETRY");
        else if (ok && !btp.on)
            ui_message("BLUETOOTH IS OFF");
        else if (ok)
            ble_ui_scan();
        break;
    default:
        if (ok && !bt_role() && btp.sel - BT_ROWS < btp.n && st != BLE_ST_FAILED)
            bt_pick(&btp.dev[btp.sel - BT_ROWS]);
        break;
    }
    enc_drop();                                     /* swallow the rest while the page is up */
}

static uint32_t bt_hash(uint32_t h, const void *p, uint32_t n)
{
    const uint8_t *b = p;
    while (n--)
        h = (h ^ *b++) * 16777619u;
    return h;
}

/* a free text (a name, a message) ending at xr, at most maxw px wide (ellipsised when longer) */
static void bt_text_r(int32_t xr, int32_t y, const aafont_t *f, const char *s, uint16_t fg, uint16_t bg, int32_t maxw)
{
    char b[48];
    int cut = text_fit(b, sizeof b, s, f, maxw);
    cv_text_flags(xr - text_w(f, b), y, f, b, fg, bg, cut ? 9u : 8u);
}

/* one of the four rows: its label, its value at the right */
static void bt_row(uint32_t k, const char *label, const char *value, int avail)
{
    int32_t y = BT_RY + (int32_t)k * (BT_RH + 1), vy = y + CAP_IN(M, BT_RH);
    int sel = btp.sel == k;
    uint16_t bg = sel ? T_THEME : T_SURF, fg = sel ? T_INK : avail ? T_TEXT : T_MID, val = sel ? T_INK : T_THEME;
    if (sel)
        cv_rrect(MR_X, y, MR_W, BT_RH, 6, T_THEME, T_SURF);
    else if (k && btp.sel != k - 1u)                /* a rule from the row above (none beside the selected one) */
        cv_rect(MR_X + 6, y - 1, MR_W - 12, 1, ux.style ? T_RULE : T_LINE);
    GFX_HOOK_ALIGN(0, y, 0, y + BT_RH, AL_V, "bluetooth row name centred up/down");
    cv_text_on(MR_LX, vy, &AF_M, label, fg, bg);
    if (value[0]) {
        GFX_HOOK_ALIGN(0, y, 0, y + BT_RH, AL_V, "bluetooth row value centred up/down");
        bt_text_r(MR_VR, vy, &AF_M, value, val, bg, MR_VR - 104);
    }
}

/* a device row: its name (or its address), MIDI when it advertises BLE-MIDI, its signal */
static void bt_dev_row(uint32_t v, const ble_ui_dev_t *d, int sel)
{
    int32_t y = BT_DY + (int32_t)v * (BT_DH + 1), vy = y + CAP_IN(M, BT_DH), sy = y + CAP_IN(S, BT_DH);
    uint16_t bg = sel ? T_THEME : T_SURF, fg = sel ? T_INK : T_TEXT, dim = sel ? T_INK : T_MID;
    char t[24], r[8];
    if (d->name[0])
        bt_name(t, d->name, sizeof t);
    else
        bt_addr_txt(t, d->addr, 1);
    fmt_int(r, d->rssi);
    if (sel)
        cv_rrect(MR_X, y, MR_W, BT_DH, 6, T_THEME, T_SURF);
    GFX_HOOK_ALIGN(0, y, 0, y + BT_DH, AL_V, "bluetooth device name centred up/down");
    cv_free_text(MR_LX, vy, &AF_M, t, fg, bg, 132);
    if (d->midi) {
        GFX_HOOK_ALIGN(0, y, 0, y + BT_DH, AL_V, "bluetooth device tag centred up/down");
        cv_text_on(154, sy, &AF_S, "MIDI", sel ? T_INK : T_THEME, bg);
    }
    GFX_HOOK_ALIGN(0, y, 0, y + BT_DH, AL_V, "bluetooth device signal centred up/down");
    cv_text_r(MR_VR, sy, &AF_S, r, dim, bg);
}

/* the page; drawn again when its signature changes: the state line (state, seconds left, the connected name, the
 * reason), the last error, the list, the cursor, BLUETOOTH, MODE, the remembered device, the palette and style */
static void bt_page_draw(void)
{
    static const khint_t KEYS[3] = {{KC_PRESETS, "ROW"}, {KC_K14, "VALUE"}, {KC_OCTUP, "OK"}};
    char st[40], dv[24];
    const char *err;
    uint32_t code, sig, pass, v, on;
    bt_set_t set;
    bt_list();
    bt_set_get(&set);
    bt_state_text(st);
    bt_dev_text(dv);
    err = ble_ui_state() == BLE_ST_FAILED ? "" : ble_ui_err(&code);
    sig = bt_hash(2166136261u, st, str_len(st));
    sig = bt_hash(sig, err, str_len(err));
    sig = bt_hash(sig, dv, str_len(dv));
    sig = bt_hash(sig, &btp, sizeof btp);
    sig = bt_hash(sig, &set, sizeof set);
    sig = (sig ^ palette_cur()) * 16777619u;
    sig = (sig ^ (ui_prefs | (uint32_t)ui_style << 8 | (uint32_t)song.rec << 16)) * 16777619u;
    if (!ui.force && sig == ui.menu_sig)
        return;
    ui.menu_sig = sig;
    menu_head();
    on = 1u | (btp.sel <= 4u ? 2u : 0u) | (btp.sel != 4u ? 4u : 0u);   /* (VALUE: the rows above SCAN; OK: not DEVICE) */
    for (pass = 0; pass < 2u; pass++) {
        int32_t top_y = pass ? BT_SPLIT : H_HEAD;
        cv_begin(240, pass ? 240u - BT_SPLIT : BT_SPLIT - H_HEAD, T_BG);
        cv_oy = -top_y;                             /* drawn in screen rows */
        if (!pass) {
            if (!ux.style)                          /* FLAT: the state line's panel, the rows' panel (to the split) */
                cv_rrect(4, BT_SY, 232, BT_SH, 8, T_SURF, T_BG);
            GFX_HOOK_ALIGN(0, BT_SY, 0, BT_SY + BT_SH, AL_V, "bluetooth state centred up/down");
            cv_free_text(MR_LX, BT_SY + CAP_IN(M, BT_SH), &AF_M, st, T_THEME, T_SURF, MR_VR - MR_LX);
            if (err[0])
                cv_free_text(MR_LX, BT_EY, &AF_S, err, T_MID, T_BG, MR_VR - MR_LX);
        }
        if (!ux.style)                              /* the rows' panel, a part in each band */
            cv_rrect(4, BT_RY - MP_PAD, 232, (int32_t)bt_rows() * (BT_RH + 1) - 1 + 2 * MP_PAD, 8, T_SURF, T_BG);
        if (!pass) {
            bt_row(0, "BLUETOOTH", btp.on ? "ON" : "OFF", 1);
            bt_row(1, "AUTOSTART", set.autostart ? "ON" : "OFF", 1);
            bt_row(2, "ROLE", set.role ? "DEVICE" : "HOST", 1);
        } else {
            bt_row(3, "MODE", BT_MODE_N[set.mode], 1);
            if (!set.role) {
                bt_row(4, "DEVICE", dv, 1);
                bt_row(5, "SCAN", "", btp.on);
            }
            if (btp.n && !ux.style)
                cv_rrect(4, BT_DY - MP_PAD, 232, (int32_t)(btp.n < BT_VIS ? btp.n : BT_VIS) * (BT_DH + 1) - 1 + 2 * MP_PAD,
                         8, T_SURF, T_BG);
            for (v = 0; v < BT_VIS && btp.top + v < btp.n; v++)
                bt_dev_row(v, &btp.dev[btp.top + v], btp.sel == BT_ROWS + btp.top + v);
            cv_key_row(8, 232, MK_Y, KEYS, 3, on, T_BG);
        }
        cv_oy = 0;
        cv_blit(0, (uint32_t)top_y);
    }
}
