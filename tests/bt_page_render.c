/* SPDX-License-Identifier: GPL-3.0-only
 * BLE MIDI: 2026 DEADACTIVE */
/* The Bluetooth page (firmware/src/bt_page.c) drawn in each of its states, in every palette, FLAT and LINE, through
 * tests/ui_render.c's layout lint (a text off the screen, cut by its canvas, overlapping another, spilling out of its
 * cell, drawn in its background's colour), its GREY / MONO checks and its alignment check. Built with FELUCCA_BLE=1.
 *   bt_page_render OUTDIR      OUTDIR/ppm/<PALETTE>_<state>.ppm for GREY MONO GREEN PAPER NIGHT, OUTDIR/report.txt
 * (tests/ui_render.py OUTDIR makes the PNGs). Exit 1 on a finding. */
#include <sys/stat.h>
#define UI_RENDER_NO_MAIN 1
#include "ui_render.c"
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

enum { BS_OFF, BS_STARTING, BS_NONE, BS_DEVICES, BS_SCROLLED, BS_FORGET, BS_ADD, BS_ADD_EMPTY, BS_FAILED, BS_ERROR, BS_REC,
       BS_DEV_WAIT, BS_DEV_HOST, BS_COUNT };
static const char *const BS_NAME[BS_COUNT] = {"bt_off", "bt_starting", "bt_none", "bt_devices", "bt_scrolled", "bt_forget",
                                            "bt_add", "bt_add_empty", "bt_failed", "bt_error", "bt_rec",
                                            "bt_device_waiting", "bt_device_host"};
/* 23 bytes, some above 0x7F, some below 0x20: a name no label is like */
static const char ODD[] = "W\xC3\x9CRFEL\x01KEYBOARD MK2 WWWWW";
static void dev(uint32_t i, uint8_t a0, uint32_t type, int midi, int rssi, const char *name)   /* seen now */
{
    ble_ui_dev_t *d = &bs.dev[i];
    memset(d, 0, sizeof *d);
    d->addr[0] = a0, d->addr[1] = 0x12, d->addr[2] = 0x34, d->addr[3] = 0x56, d->addr[4] = 0xAB, d->addr[5] = 0xC4;
    d->type = (uint8_t)type;
    d->midi = (uint8_t)midi;
    d->rssi = (int8_t)rssi;
    memcpy(d->name, name, strlen(name) < 23u ? strlen(name) : 23u);
    d->seen_ms = fm1_ms;
    if (bs.ndev <= i)
        bs.ndev = i + 1u;
}
static void save(uint8_t a0, const char *name)       /* a saved device (to the top) */
{
    bt_dev_t d;
    memset(&d, 0, sizeof d);
    d.addr[0] = a0, d.addr[1] = 0x12, d.addr[2] = 0x34, d.addr[3] = 0x56, d.addr[4] = 0xAB, d.addr[5] = 0xC4;
    d.kind = (uint8_t)bt_dev_kind(name);
    memcpy(d.name, name, strlen(name) < 16u ? strlen(name) : 16u);
    bt_devs_add(&d);
}
static void up(uint32_t slot, uint8_t a0)
{
    memset(&bh.slot[slot], 0, sizeof bh.slot[slot]);
    bh.slot[slot].st = BLE_LS_UP;
    bh.slot[slot].addr[0] = a0, bh.slot[slot].addr[1] = 0x12, bh.slot[slot].addr[2] = 0x34;
    bh.slot[slot].addr[3] = 0x56, bh.slot[slot].addr[4] = 0xAB, bh.slot[slot].addr[5] = 0xC4;
}
static void four(void)                               /* four saved: the mixer and an FM-1 linked, one near, one away */
{
    save(0x22, ODD);
    save(0x33, "Xkey Air 25");
    save(0xF1, "Felucca FM-1");
    save(0x11, "SMC-Mixer");
    up(BLE_SLOT_CTRL, 0x11);
    up(BLE_SLOT_FM1, 0xF1);
    dev(0, 0x33, 0, 1, -61, "Xkey Air 25");
}
static void bt_setup(int s)
{
    state();
    memset(&favorites, 0, sizeof favorites);
    memset(&bs, 0, sizeof bs);
    memset(&bh, 0, sizeof bh);
    memset(&btp, 0, sizeof btp);
    memset(&bt_dv, 0, sizeof bt_dv);
    memset(conn_name, 0, sizeof conn_name);
    conn_up = 0;
    fm1_ms = 100000u;
    song.rec = 0;
    ui.menu = BT_MENU;
    ui.menu_sel = MI_BT;
    switch (s) {
    case BS_OFF: break;
    case BS_STARTING: btp.on = 1; bs.state = BLE_ST_STARTING; btp.sel = 3; break;
    case BS_NONE: btp.on = 1; bs.state = BLE_ST_IDLE; btp.sel = 4; break;   /* nothing saved: the cursor on ADD */
    case BS_DEVICES: btp.on = 1; bs.state = BLE_ST_IDLE; four(); btp.sel = 4; break;
    case BS_SCROLLED:                                /* six saved, the cursor on ADD: the lines scrolled */
        btp.on = 1; bs.state = BLE_ST_IDLE; four();
        save(0x44, "nanoKEY Studio");
        save(0x55, "LPD8 MK2");
        btp.sel = 4 + 6;
        break;
    case BS_FORGET: btp.on = 1; bs.state = BLE_ST_IDLE; four(); btp.sel = 6; btp.confirm = 3; break;
    case BS_ADD:                                     /* the ADD list: BLE-MIDI first, a nameless one, the odd name */
        btp.on = 1; bs.state = BLE_ST_IDLE; save(0x11, "SMC-Mixer"); btp.sel = 5; btp.add = 1; btp.asel = 2;
        dev(0, 0x11, 0, 1, -40, "SMC-Mixer");        /* (saved: not listed) */
        dev(1, 0x9A, 1, 0, -40, "");
        dev(2, 0xF2, 0, 1, -59, "Felucca FM-1");
        dev(3, 0x22, 0, 1, -127, ODD);
        dev(4, 0x44, 1, 0, -72, "Galaxy Buds2 Pro");
        dev(5, 0x66, 1, 0, -81, "TV");
        dev(6, 0x77, 1, 0, -90, "");
        dev(7, 0x88, 1, 0, -100, "Printer");
        dev(8, 0x99, 1, 0, -101, "NINTH");
        dev(9, 0xAA, 1, 0, -102, "TENTH (NOT LISTED)");
        break;
    case BS_ADD_EMPTY: btp.on = 1; bs.state = BLE_ST_IDLE; btp.sel = 4; btp.add = 1; break;
    case BS_FAILED: bs.state = BLE_ST_FAILED; bs.err = "NO RF TRIM IN FLASH"; four(); break;
    case BS_ERROR:
        btp.on = 1; bs.state = BLE_ST_IDLE; bs.err = "DISCONNECTED 0x08"; bs.code = 8; four(); btp.sel = 7;
        break;
    case BS_REC: btp.on = 1; bs.state = BLE_ST_IDLE; song.rec = 1; break;   /* (the head: the REC mark beside BACK) */
    case BS_DEV_WAIT: {                              /* ROLE DEVICE: advertising, the cursor on ROLE */
        bt_set_t d = {{0}, 0, BT_MODE_AUTO, 0, 1, 1};
        bt_set_put(&d);
        btp.on = 1; bs.state = BLE_ST_ADVERTISING; btp.sel = 2;
        save(0x11, "SMC-Mixer");                     /* (saved devices the device role does not show) */
        break;
    }
    case BS_DEV_HOST: {                              /* ROLE DEVICE: a host connected (its address as the name), MODE
                                                      * FM-LINK (the longest value) under the cursor */
        bt_set_t d = {{0}, 0, BT_MODE_LINK, 0, 0, 1};
        bt_set_put(&d);
        btp.on = 1; bs.state = BLE_ST_RECEIVING; btp.sel = 3; conn_up = 1;
        strcpy(conn_name, "5A:3C:91:0E:77:C2");
        break;
    }
    }
}
static void bt_draw(void)
{
    nscr = npend = nrules = 0;
    ntight = 0;
    tight[0] = 0;
    memset(host_screen, 0, sizeof host_screen);
    ui.force = 1;
    ui_draw();
}

int main(int argc, char **argv)
{
    static const char *const SHOW[] = {"GREY", "MONO", "GREEN", "PAPER", "NIGHT"};
    const char *out = argc > 1 ? argv[1] : "build/ui_bt";
    char path[600];
    uint32_t p, s, st, k, shots = 0;
    mkdir(out, 0777);
    snprintf(path, sizeof path, "%s/ppm", out);
    mkdir(path, 0777);
    snprintf(path, sizeof path, "%s/report.txt", out);
    rep = fopen(path, "w");
    if (!rep) { fprintf(stderr, "cannot write %s\n", path); return 1; }
    for (st = 0; st < 2u; st++)
        for (p = 0; p < NPALETTES; p++)
            for (s = 0; s < BS_COUNT; s++) {
                char name[64];
                snprintf(name, sizeof name, "%s%s/%s", st ? "LINE_" : "", UI_PALETTES[p].name, BS_NAME[s]);
                cur_name = name;
                bt_setup((int)s);
                pal(p);
                ui_style = (uint8_t)st;
                style_apply();
                bt_draw();
                if (ui.menu != BT_MENU || ux.style != st) { fprintf(stderr, "bt_page_render: %s not drawn as set up\n", name); return 1; }
                lint();
                mono_check();
                shots++;
                for (k = 0; !st && k < sizeof SHOW / sizeof SHOW[0]; k++)
                    if (!strcmp(UI_PALETTES[p].name, SHOW[k])) write_ppm(out, SHOW[k], BS_NAME[s]);
            }
    al_flush();
    {   /* what the page must show: the state, the rows, a device's tag and signal (GREY's last screen is not kept:
         * draw one and look for the words among the boxes the lint saw) */
        static const char *const WANT[] = {"ON, 2 CONNECTED", "BLUETOOTH", "AUTOSTART", "ROLE", "HOST", "MODE", "SMC-Mixer",
                                           "CTRL", "FM-1 12F1", "FM-1", "Xkey Air 25", "NEAR", "ON", "AUTO"};
        uint32_t i, j, miss = 0;
        bt_setup(BS_DEVICES);
        pal(UI_GREY_INDEX);
        ui_style = 0;
        style_apply();
        bt_draw();
        for (i = 0; i < sizeof WANT / sizeof WANT[0]; i++) {
            for (j = 0; j < nscr && strcmp(scr[j].s, WANT[i]); j++) ;
            if (j == nscr) { fprintf(rep, "FIND bt_devices does not show '%s'\n", WANT[i]); miss++; }
        }
        nfind += miss;
    }
    fprintf(rep, "\n%u screens (%u states x %u palettes x FLAT / LINE): %u findings, %u ink, %u GREY / MONO pixels off, %u alignments off\n",
            shots, (unsigned)BS_COUNT, (unsigned)NPALETTES, nfind, nink, mono_bad, al_fail);
    fclose(rep);
    printf("bt_page_render: %u screens, %u findings, %u invisible inks, %u GREY / MONO pixels off, %u alignments 1 px or more off (%s/report.txt)\n",
           shots, nfind, nink, mono_bad, al_fail, out);
    return nfind || nink || mono_bad || al_fail ? 1 : 0;
}
