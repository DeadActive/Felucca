/* SPDX-License-Identifier: GPL-3.0-only
 * BLE MIDI: 2026 DEADACTIVE (after Hortator's bt_page.c)
 * FM-Link: 2026 DEADACTIVE */
/* FELUCCA_BLE (felucca.c includes it after ble/ble_core.h, bt_set.c, bt_midi.c and bt_devs.c; ui_menu.c declares its
 * entry points): the Bluetooth page, MENU > SYSTEM > BLUETOOTH, ui.menu BT_MENU (docs/superpowers/specs/
 * 2026-10-22-bt-devices-design.md 5.1, mockup A). A state line (and the last error), the rows BLUETOOTH, AUTOSTART,
 * ROLE (HOST, DEVICE) and MODE, then (HOST) the saved devices, a line each (connected or not, its name, CTRL or FM-1,
 * ON / NEAR / AWAY), four in view, and + ADD A DEVICE. On a device line a knob left asks FORGET <name>? on the state
 * line (OCT+ yes, OCT- no) and OCT+ uses it now (the first of its kind); + ADD A DEVICE opens the scan list of nearby
 * devices not saved (BLE-MIDI ones first; the page scans while it is open): OCT+ saves and connects one, OCT- goes
 * back. ROLE DEVICE: the FM-1 is the BLE-MIDI device a host connects to; no device lines then. As in the menu:
 * PRESETS moves the cursor (it wraps), any of KNOB 1..4 changes the row, OCT+ acts on it, OCT- goes back to the
 * menu's rows, HOME closes the menu.
 * BLUETOOTH ON is the only call of ble_user_start in the core (gate 3: tools/check_ble_boot.py TRIGGERS has
 * bt_page_on); Bluetooth is off at every power-on and ON is not saved, unless AUTOSTART is ON: then bt_page_auto
 * turns it on once, BT_AUTO_MS after the main loop began, after a clean boot only. AUTOSTART, ROLE, MODE and the saved
 * devices are saved at once (bt_set.c, bt_devs.c). Main loop only. */
#define BT_SET_ROWS 4u                              /* BLUETOOTH AUTOSTART ROLE MODE, then (HOST) the devices, ADD */
#define BT_SHOW 8u                                  /* the ADD list's lines (the scan keeps 12) */
#define BT_VIS 4u                                   /* device lines in view (the cursor scrolls them) */
#define BT_SY 27                                    /* the state line's panel; the last error under it */
#define BT_SH 20
#define BT_EY 48
#define BT_RY 67                                    /* the rows, 17 px, 1 px apart (the ADD list's too) */
#define BT_RH 17
#define BT_SPLIT 121                                /* the two bands meet in the gap above MODE */
#define BT_DY 146                                   /* the device lines, 17 px, 1 px apart */
static const char *const BT_MODE_N[4] = {"AUTO", "SMC DAW", "MIDI", "FM-LINK"};
static uint32_t bt_role(void) { bt_set_t s; bt_set_get(&s); return s.role; }   /* 1: ROLE DEVICE */
static struct {
    uint8_t on;                                     /* BLUETOOTH ON (not saved) */
    uint8_t sel, top;                               /* the cursor (a row), the first device line in view */
    uint8_t confirm;                                /* FORGET asked for saved device confirm - 1 (0: none) */
    uint8_t add, asel, an;                          /* the ADD list open, its cursor, its devices */
    ble_ui_dev_t adev[BT_SHOW];                     /* the ADD list as drawn: a pick takes these */
} btp __attribute__((section(".pool")));

/* the top bar's Bluetooth icon (ui_draw.c draw_head; design 5.6): 0 none (Bluetooth off), 1 struck (on, nothing linked:
 * searching, reconnecting, the device role waiting), 2 lit (a link up) */
static uint32_t bt_head_icon(void)
{
    ble_ui_slot_t x;
    uint32_t i, st = ble_ui_state();
    if (!btp.on)
        return 0;
    if (st == BLE_ST_SUBSCRIBED || st == BLE_ST_RECEIVING)
        return 2;
    for (i = 0; i < 2u; i++)
        if (ble_ui_slot(i, &x) && x.st == BLE_LS_UP)
            return 2;
    return 1;
}

static uint32_t bt_page_rows(void) { return bt_role() ? BT_SET_ROWS : BT_SET_ROWS + bt_devs_n() + 1u; }

/* a name as the screen can show it: the font's characters only (a device's name is any bytes) */
static void bt_name(char *o, const char *s, uint32_t n)
{
    uint32_t i;
    for (i = 0; i + 1u < n && s[i]; i++)
        o[i] = s[i] >= 0x20 && s[i] < 0x7F ? s[i] : '?';
    o[i] = 0;
}
static char *bt_hex2(char *o, uint8_t v)
{
    static const char H[] = "0123456789ABCDEF";
    *o++ = H[v >> 4];
    *o++ = H[v & 15u];
    return o;
}
/* a saved device's line text (24 B): its name; an FM-1 (they all say Felucca FM-1) FM-1 and its address' last four
 * hex digits, most significant first, as `ble list` shows the address */
static void bt_dev_label(const bt_dev_t *d, char *o)
{
    if (d->kind == BT_KIND_FM1) {
        str_cpy(o, "FM-1 ", 24);
        *bt_hex2(bt_hex2(o + 5, d->addr[1]), d->addr[0]) = 0;
    } else {
        bt_name(o, d->name[0] ? d->name : "?", 24);
    }
}
static const char *bt_dev_value(uint32_t i)        /* a device line's value: ON (linked), NEAR (around), AWAY */
{
    int st = bt_devs_state(i);
    return st == 2 ? "ON" : st ? "NEAR" : "AWAY";
}

static void bt_state_text(char *o)                  /* the state line (40 B) */
{
    uint32_t st = ble_ui_state(), code, n, i;
    const char *e = ble_ui_err(&code);
    char t[24];
    bt_dev_t d;
    if (btp.confirm && bt_devs_get(btp.confirm - 1u, &d)) {
        bt_dev_label(&d, t);
        str_cpy(o, "FORGET ", 40);
        str_cpy(o + 7, t, 32);
        str_cpy(o + str_len(o), "?", 40 - str_len(o));
        return;
    }
    if (st == BLE_ST_FAILED) {
        str_cpy(o, "FAILED ", 40);
        str_cpy(o + 7, e, 33);
    } else if (st == BLE_ST_STARTING) {
        str_cpy(o, "STARTING", 40);
    } else if (!btp.on || st == BLE_ST_OFF) {
        str_cpy(o, "OFF", 40);
    } else if (st == BLE_ST_ADVERTISING) {
        str_cpy(o, "WAITING FOR A HOST", 40);
    } else if (st == BLE_ST_SUBSCRIBED || st == BLE_ST_RECEIVING) {   /* the device role: its host */
        str_cpy(o, "CONNECTED ", 40);
        if (ble_ui_conn_name(t))
            t[23] = 0, bt_name(o + 10, t, 30);
    } else if (btp.add) {
        str_cpy(o, "ADD: PICK A DEVICE", 40);
    } else if (!(n = bt_devs_n())) {
        str_cpy(o, "ON, NO DEVICE SAVED", 40);
    } else {
        for (i = 0, n = 0; i < 2u; i++) {
            ble_ui_slot_t x;
            n += ble_ui_slot(i, &x) && x.st == BLE_LS_UP;
        }
        if (n) {
            str_cpy(o, "ON, ", 40);
            fmt_int(o + 4, (int32_t)n);
            str_cpy(o + str_len(o), " CONNECTED", 40 - str_len(o));
        } else {
            str_cpy(o, "ON, SEARCHING", 40);
        }
    }
}

/* the ADD list: the BLE unit's scan list without the saved devices, BLE-MIDI devices first, each part by signal, the
 * first BT_SHOW; the cursor kept on the device it was on (the list re-sorts as signals change) */
static int bt_saved(const uint8_t a[6])
{
    bt_dev_t d;
    uint32_t i;
    for (i = 0; bt_devs_get(i, &d); i++)
        if (!memcmp(d.addr, a, 6))
            return 1;
    return 0;
}
static void bt_add_list(void)
{
    ble_ui_dev_t d;
    uint8_t was[6];
    uint32_t i, k, pass, n = ble_ui_devs(), had = btp.asel < btp.an;
    if (had)
        memcpy(was, btp.adev[btp.asel].addr, 6);
    btp.an = 0;
    for (pass = 0; pass < 2u; pass++)
        for (i = 0; i < n; i++) {
            if (!ble_ui_dev(i, &d) || (d.midi != 0) != (pass == 0u) || bt_saved(d.addr) || btp.an >= BT_SHOW)
                continue;
            d.name[sizeof d.name - 1u] = 0;
            btp.adev[btp.an++] = d;
        }
    for (k = 0; had && k < btp.an; k++)
        if (!memcmp(btp.adev[k].addr, was, 6))
            btp.asel = (uint8_t)k;
    if (btp.asel >= btp.an)
        btp.asel = btp.an ? (uint8_t)(btp.an - 1u) : 0u;
}
static void bt_add_close(void) { btp.add = 0; }

/* BLUETOOTH ON: links wanted, the role handed over, the stack started if it is not (the user's trigger: the only
 * call of ble_user_start in the core, gate 3); the saved devices are the targets (bt_devs_frame) */
__attribute__((noinline)) static void bt_page_on(void)
{
    if (ble_ui_state() == BLE_ST_FAILED) {          /* the stack cannot stop: only a power cycle starts it again */
        ui_message("POWER OFF AND ON TO RETRY");
        return;
    }
    if (btp.on)
        return;
    btp.on = 1;
    ble_ui_role((int)bt_role());                    /* (applied once the stack is up and idle) */
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

static void bt_page_open(void)                      /* MENU > SYSTEM > BLUETOOTH, OCT+ */
{
    ui.menu = BT_MENU;
    btp.sel = btp.top = btp.confirm = 0;
    bt_add_close();
    bt_devs_page(!bt_role());                       /* (HOST: scan while the page is open: its ON / NEAR / AWAY) */
    ui.force = 1;
}

static void bt_page_close(void)                     /* OCT- or HOME: the page's scan and list end */
{
    btp.confirm = 0;
    bt_add_close();
    bt_devs_page(0);
}

static void bt_page_input(uint32_t oct)             /* oct: ui_input.c oct_taps (bit 0 OCT-, bit 1 OCT+) */
{
    uint32_t ok = (oct >> 1) & 1u, rows = bt_page_rows(), st = ble_ui_state(), k, n = bt_devs_n();
    int32_t s;
    bt_set_t set;
    if (btp.add) {                                  /* the ADD list: PRESETS moves, OCT+ adds, OCT- back */
        if ((s = panel_enc(EN_PRESET)) != 0 && btp.an)
            btp.asel = (uint8_t)((btp.asel + (s > 0 ? 1u : btp.an - 1u)) % btp.an);
        if (oct & 1u) {
            bt_add_close();
        } else if (ok && btp.asel < btp.an) {
            const ble_ui_dev_t *a = &btp.adev[btp.asel];
            bt_dev_t d;
            memset(&d, 0, sizeof d);
            memcpy(d.addr, a->addr, 6);
            d.type = a->type & 1u;
            d.kind = (uint8_t)bt_dev_kind(a->name);
            str_cpy(d.name, a->name, sizeof d.name);
            bt_devs_add(&d);                        /* (first of its kind: bt_devs_frame connects it) */
            bt_add_close();
            btp.sel = BT_SET_ROWS;                  /* on its line */
        }
        enc_drop();
        return;
    }
    if (btp.confirm) {                              /* FORGET <name>?: OCT+ yes, OCT- no; anything else no */
        for (s = panel_enc(EN_PRESET), k = 0; k < 4u; k++)
            s |= panel_enc(EN_K1 + k);
        if (!oct && !s)
            return;                                 /* (called every pass: waits for an answer) */
        k = btp.confirm - 1u;
        btp.confirm = 0;
        if (ok)
            bt_devs_forget(k);
        if (btp.sel >= bt_page_rows())
            btp.sel = (uint8_t)(bt_page_rows() - 1u);
        enc_drop();
        return;
    }
    if (oct & 1u) {
        bt_page_close();
        ui.menu = 1;
        ui.force = 1;
        enc_drop();
        return;
    }
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
            set.link = 0;
            bt_set_put(&set);
            ble_ui_role((int)k);                    /* (the BLE unit ends its links, a scan or advertising, then switches) */
            btp.top = 0;
        }
        break;
    case 3:                                         /* MODE: a knob steps and stops at the ends; OCT+ wraps */
        bt_set_get(&set);
        k = s > 0 ? (set.mode < BT_MODE_LINK ? set.mode + 1u : BT_MODE_LINK)
          : s < 0 ? (set.mode ? set.mode - 1u : 0u) : ok ? (set.mode + 1u) % 4u : set.mode;
        if (k != set.mode) {
            set.mode = (uint8_t)k;
            bt_set_put(&set);
        }
        break;
    default:
        k = btp.sel - BT_SET_ROWS;
        if (k == n) {                               /* + ADD A DEVICE */
            if (ok && st == BLE_ST_FAILED)
                ui_message("POWER OFF AND ON TO RETRY");
            else if (ok && !btp.on)
                ui_message("BLUETOOTH IS OFF");
            else if (ok) {
                btp.add = 1;
                btp.asel = btp.an = 0;
            }
        } else if (s < 0) {                         /* a device: a knob left asks FORGET */
            btp.confirm = (uint8_t)(k + 1u);
        } else if (ok) {                            /* OCT+: use it now (around, not linked) */
            int at = bt_devs_state(k);
            if (at == 1)
                bt_devs_use(k);
            else
                ui_message(at ? "CONNECTED" : "NOT AROUND");
        }
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

/* one line at y: its label (and a tag), its value at the right; the cursor's line in the theme colour; dot: 2 a
 * connected device's dot, 1 a saved one's ring, 0 none */
static void bt_line(int32_t y, int sel, int first, const char *label, const char *tag, const char *value, int lit,
                    int dot)
{
    int32_t vy = y + CAP_IN(M, BT_RH), sy = y + CAP_IN(S, BT_RH), lx = MR_LX;
    uint16_t bg = sel ? T_THEME : T_SURF, fg = sel ? T_INK : T_TEXT, val = sel ? T_INK : lit ? T_THEME : T_MID;
    if (sel)                                        /* (over the 1 px gap below: a name's descenders; the line below
                                                     * draws no rule under the cursor's) */
        cv_rrect(MR_X, y, MR_W, BT_RH + 1, 6, T_THEME, T_SURF);
    else if (!first)                                /* a rule from the line above */
        cv_rect(MR_X + 6, y - 1, MR_W - 12, 1, ux.style ? T_RULE : T_LINE);
    if (dot) {
        cv_rrect(lx, y + 5, 7, 7, 3, sel ? T_INK : dot > 1 ? T_THEME : T_MID, bg);
        if (dot == 1)
            cv_rrect(lx + 2, y + 7, 3, 3, 1, bg, sel ? T_INK : T_MID);
        lx += 11;
    }
    GFX_HOOK_ALIGN(0, y, 0, y + BT_RH, AL_V, "bluetooth line name centred up/down");
    cv_free_text(lx, vy, &AF_M, label, fg, bg, (tag ? 134 : 150) - lx);
    if (tag) {
        GFX_HOOK_ALIGN(0, y, 0, y + BT_RH, AL_V, "bluetooth line tag centred up/down");
        cv_text_on(140, sy, &AF_S, tag, sel ? T_INK : T_MID, bg);
    }
    if (value[0]) {
        GFX_HOOK_ALIGN(0, y, 0, y + BT_RH, AL_V, "bluetooth line value centred up/down");
        bt_text_r(MR_VR, vy, &AF_M, value, val, bg, tag ? 44 : MR_VR - 104);
    }
}
static void bt_row(uint32_t k, const char *label, const char *value)   /* one of the four rows */
{
    bt_line(BT_RY + (int32_t)k * (BT_RH + 1), btp.sel == k, !k || btp.sel == k - 1u, label, 0, value, 1, 0);
}

/* the page; drawn again when its signature changes: the state line, the last error, the rows, the saved devices'
 * lines and states, the ADD list, the cursor, the palette and style */
static void bt_page_draw(void)
{
    static const khint_t KEYS[3] = {{KC_PRESETS, "ROW"}, {KC_K14, "VALUE"}, {KC_OCTUP, "OK"}};
    static const khint_t DKEYS[3] = {{KC_PRESETS, "ROW"}, {KC_K14, "FORGET"}, {KC_OCTUP, "USE"}};
    static const khint_t AKEYS[3] = {{KC_PRESETS, "ROW"}, {KC_OCTDN, "BACK"}, {KC_OCTUP, "ADD"}};
    char st[40], lb[24];
    const char *err;
    uint32_t code, sig, pass, v, on, n, rows;
    int32_t y;
    bt_set_t set;
    bt_dev_t d;
    uint8_t ds[BT_NDEV];
    const khint_t *keys = KEYS;
    if (btp.add)
        bt_add_list();
    bt_set_get(&set);
    n = set.role ? 0u : bt_devs_n();
    rows = bt_page_rows();
    if (btp.sel >= rows)
        btp.sel = (uint8_t)(rows - 1u);
    if (btp.sel < BT_SET_ROWS)                      /* the device line under the cursor in view */
        btp.top = 0;
    else if (btp.sel - BT_SET_ROWS < btp.top)
        btp.top = (uint8_t)(btp.sel - BT_SET_ROWS);
    else if (btp.sel - BT_SET_ROWS >= btp.top + BT_VIS)
        btp.top = (uint8_t)(btp.sel - BT_SET_ROWS - BT_VIS + 1u);
    for (v = 0; v < n; v++)
        ds[v] = (uint8_t)bt_devs_state(v);
    bt_state_text(st);
    err = ble_ui_state() == BLE_ST_FAILED ? "" : ble_ui_err(&code);
    sig = bt_hash(2166136261u, st, str_len(st));
    sig = bt_hash(sig, err, str_len(err));
    sig = bt_hash(sig, &btp, sizeof btp);
    sig = bt_hash(sig, &set, sizeof set);
    sig = bt_hash(sig, ds, n);
    for (v = 0; v < BT_NDEV; v++)                   /* the saved devices' bytes */
        sig = bt_hash(sig, &favorites.factory[v][8], 24);
    sig = (sig ^ n ^ palette_cur() << 4) * 16777619u;
    sig = (sig ^ (ui_prefs | (uint32_t)ui_style << 8 | (uint32_t)song.rec << 16)) * 16777619u;
    if (!ui.force && sig == ui.menu_sig)
        return;
    ui.menu_sig = sig;
    menu_head();
    if (btp.add)
        keys = AKEYS, on = 7u;
    else if (btp.sel >= BT_SET_ROWS && btp.sel - BT_SET_ROWS < n)
        keys = DKEYS, on = 7u;
    else
        on = 1u | (btp.sel < BT_SET_ROWS ? 2u : 0u) | 4u;   /* (VALUE: the four rows; OK: every row) */
    for (pass = 0; pass < 2u; pass++) {
        int32_t top_y = pass ? BT_SPLIT : H_HEAD;
        cv_begin(240, pass ? 240u - BT_SPLIT : BT_SPLIT - H_HEAD, T_BG);
        cv_oy = -top_y;                             /* drawn in screen rows */
        if (!pass) {
            if (!ux.style)                          /* FLAT: the state line's panel */
                cv_rrect(4, BT_SY, 232, BT_SH, 8, T_SURF, T_BG);
            GFX_HOOK_ALIGN(0, BT_SY, 0, BT_SY + BT_SH, AL_V, "bluetooth state centred up/down");
            cv_free_text(MR_LX, BT_SY + CAP_IN(M, BT_SH), &AF_M, st, btp.confirm ? T_ACCENT : T_THEME, T_SURF,
                         MR_VR - MR_LX);
            if (err[0])
                cv_free_text(MR_LX, BT_EY, &AF_S, err, T_MID, T_BG, MR_VR - MR_LX);
        }
        if (btp.add) {                              /* the ADD list over the rows: a device a line, BLE-MIDI tagged */
            if (!ux.style && btp.an)
                cv_rrect(4, BT_RY - MP_PAD, 232, (int32_t)btp.an * (BT_RH + 1) - 1 + 2 * MP_PAD, 8, T_SURF, T_BG);
            for (v = 0; v < btp.an; v++) {
                char r[8];
                const ble_ui_dev_t *a = &btp.adev[v];
                y = BT_RY + (int32_t)v * (BT_RH + 1);
                if (a->name[0])
                    bt_name(lb, a->name, sizeof lb);
                else
                    *bt_hex2(bt_hex2(bt_hex2(lb, a->addr[5]), a->addr[4]), a->addr[3]) = 0;
                fmt_int(r, a->rssi);
                bt_line(y, btp.asel == v, !v || btp.asel == v - 1u, lb, a->midi ? "MIDI" : 0, r, 0, 0);
            }
            if (!btp.an && pass)
                cv_free_text(MR_LX, BT_RY + 60, &AF_M, "LOOKING FOR DEVICES...", T_MID, T_BG, MR_VR - MR_LX);
            if (pass)
                cv_key_row(8, 232, MK_Y, keys, 3, on, T_BG);
            cv_oy = 0;
            cv_blit(0, (uint32_t)top_y);
            continue;
        }
        if (!ux.style)                              /* the rows' panel, a part in each band */
            cv_rrect(4, BT_RY - MP_PAD, 232, (int32_t)BT_SET_ROWS * (BT_RH + 1) - 1 + 2 * MP_PAD, 8, T_SURF, T_BG);
        if (!pass) {
            bt_row(0, "BLUETOOTH", btp.on ? "ON" : "OFF");
            bt_row(1, "AUTOSTART", set.autostart ? "ON" : "OFF");
            bt_row(2, "ROLE", set.role ? "DEVICE" : "HOST");
        } else {
            bt_row(3, "MODE", BT_MODE_N[set.mode]);
            if (!set.role) {                        /* the saved devices and ADD, BT_VIS lines in view */
                uint32_t lines = n + 1u - btp.top < BT_VIS ? n + 1u - btp.top : BT_VIS;
                if (!ux.style)
                    cv_rrect(4, BT_DY - MP_PAD, 232, (int32_t)lines * (BT_RH + 1) - 1 + 2 * MP_PAD, 8, T_SURF, T_BG);
                for (v = 0; v < lines; v++) {
                    uint32_t k = btp.top + v;
                    int sel = btp.sel == BT_SET_ROWS + k, first = !v || btp.sel == BT_SET_ROWS + k - 1u;
                    y = BT_DY + (int32_t)v * (BT_RH + 1);
                    if (k < n && bt_devs_get(k, &d)) {
                        bt_dev_label(&d, lb);
                        bt_line(y, sel, first, lb, d.kind == BT_KIND_FM1 ? "FM-1" : "CTRL", bt_dev_value(k), ds[k] == 2,
                                ds[k] == 2 ? 2 : 1);
                    } else {
                        bt_line(y, sel, first, "+ ADD A DEVICE", 0, "SCAN", btp.on, 0);
                    }
                }
            }
            cv_key_row(8, 232, MK_Y, keys, 3, on, T_BG);
        }
        cv_oy = 0;
        cv_blit(0, (uint32_t)top_y);
    }
}
