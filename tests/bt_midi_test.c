/* SPDX-License-Identifier: GPL-3.0-only
 * BLE MIDI: 2026 DEADACTIVE */
/* The core's side of Bluetooth MIDI (firmware/src/bt_set.c, bt_midi.c; docs/superpowers/specs/2026-10-13-ble-midi-design.md
 * sections 8 and 10) on the host, built with FELUCCA_BLE=1 and the UI harness: the settings bytes, MIDI mode into the
 * input ring as source 3 with the USB filter and the room kept for TRS, CLK BLE, the mode (MODE, the connected name),
 * MIDI out to the device, the release of what the device held when the link drops. The BLE unit's entry points are
 * stubs here. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#if !FELUCCA_BLE
#error "build with -DFELUCCA_BLE=1"
#endif
#include "ble_ui_stub.h"                            /* ble_core.h, the connected name (conn_name / conn_up) */
static uint8_t sent[64][3];
static uint32_t sent_n[64], nsent, room;
int ble_midi_send(const uint8_t *m, uint32_t n)
{
    if (!room || nsent >= 64u)
        return 0;
    memcpy(sent[nsent], m, n);
    sent_n[nsent++] = n;
    room--;
    return 1;
}
uint32_t ble_midi_room(void) { return room; }
#include "../firmware/src/bt_set.c"
#ifdef BT_TEST_SMC                                  /* (once smc.c is in: run_tests.sh) */
#include "../firmware/src/smc.c"
#else
static uint32_t smc_seen;
static void smc_in(uint32_t pkt) { (void)pkt; smc_seen++; }
#endif
#include "../firmware/src/bt_midi.c"
#ifdef BT_TEST_PAGE                                 /* (ui_menu.c's Bluetooth row needs the page) */
#include "../firmware/src/bt_page.c"
#endif

static int fails;
static void ok(const char *what, int good)
{
    printf("%s  %s\n", good ? "ok  " : "FAIL", what);
    if (!good)
        fails++;
}
static uint32_t msg(uint32_t st, uint32_t d1, uint32_t d2) { return (st >> 4) | st << 8 | d1 << 16 | d2 << 24; }
static int gate_note(const track_t *t, uint32_t note)
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].gate && t->v[i].note == note)
            return 1;
    return 0;
}
static void link_to(const char *name)
{
    memset(conn_name, 0, sizeof conn_name);
    str_cpy(conn_name, name, sizeof conn_name);
    conn_up = 1;
    bt_link(1);                                      /* (ble_service: core_ble_link(1) once linked) */
}
static void fresh(uint32_t mode)                     /* power-on, MODE, no device, nothing connected */
{
    bt_set_t s;
    ui_power_on();
    memset(midi_ch, 0, sizeof midi_ch);
    memset(midi_notes, 0, sizeof midi_notes);
    memset(midi_owners, 0, sizeof midi_owners);
    memset(&midi_clock, 0, sizeof midi_clock);
    memset(midi_in_source, 0, sizeof midi_in_source);
    mi_r = mi_w = 0;
    midi_in_overflow = 0;
    fm1_ms = 0;
    memset(&favorites, 0, sizeof favorites);
    memset(&s, 0, sizeof s);
    s.mode = (uint8_t)mode;
    bt_set_put(&s);
    conn_up = 0;
    bt_link(0);
    memset(&bt_in, 0, sizeof bt_in);
    nsent = 0;
    room = 0;
    events_block(256);
}

static void test_settings(void)
{
    static const uint8_t A[6] = {0xC4, 0x12, 0x00, 0x9A, 0xFF, 0x01};
    bt_set_t s, g;
    favorites_t before;
    uint32_t m, t, i, same = 1;
    memset(&favorites, 0, sizeof favorites);
    bt_set_get(&g);
    ok("settings: an older record (all zero) is no device, MODE AUTO", !g.has && g.mode == BT_MODE_AUTO);
    for (m = 0; m < 3u; m++)
        for (t = 0; t < 2u; t++) {
            memset(&s, 0, sizeof s);
            memcpy(s.addr, A, 6);
            s.type = (uint8_t)t;
            s.mode = (uint8_t)m;
            bt_set_put(&s);
            bt_set_get(&g);
            same &= g.has && !memcmp(g.addr, A, 6) && g.type == t && g.mode == m;
        }
    ok("settings: the device (address, type) and MODE round-trip for every mode and both address types", same);
    memset(&s, 0, sizeof s);
    memcpy(s.addr, A, 6);
    s.mode = BT_MODE_SMC;
    s.autostart = 1;
    bt_set_put(&s);
    bt_set_get(&g);
    ok("settings: AUTOSTART round-trips (flags bit 3)", g.autostart && g.has && g.mode == BT_MODE_SMC && favorites.factory[13][30] == 0x09u);
    favorites.factory[13][30] |= 0x10u;              /* a flag this firmware does not know */
    favorites.factory[13][31] = bt_set_sum(&favorites.factory[13][24]);
    bt_set_get(&g);
    ok("settings: an unknown flag bit is no device, MODE AUTO, AUTOSTART OFF", !g.has && g.mode == BT_MODE_AUTO && !g.autostart);
    memset(&s, 0, sizeof s);
    s.mode = BT_MODE_MIDI;
    bt_set_put(&s);
    bt_set_get(&g);
    ok("settings: MODE is kept with no device remembered", !g.has && g.mode == BT_MODE_MIDI);
    memcpy(s.addr, A, 6);
    bt_set_put(&s);
    favorites.factory[13][26] ^= 0x40u;              /* a byte of ours changed by something else */
    bt_set_get(&g);
    ok("settings: a wrong check byte is no device, MODE AUTO", !g.has && g.mode == BT_MODE_AUTO);
    for (i = 0; i < sizeof favorites; i++)
        ((uint8_t *)&favorites)[i] = (uint8_t)(i * 7u + 3u);
    before = favorites;
    bt_set_put(&s);
    for (i = 0, same = 1; i < sizeof favorites; i++) {
        uint32_t ours = i >= 13u * 32u + 24u && i < 14u * 32u;
        if (!ours && ((uint8_t *)&favorites)[i] != ((uint8_t *)&before)[i])
            same = 0;
    }
    ok("settings: only favorites.factory[13][24..31] is written", same);
    memset(&favorites, 0, sizeof favorites);
    favorites.factory[13][0] = 0xFFu;                /* SLICE's first eight favourites */
    bt_set_put(&s);
    ok("settings: SLICE's favourites keep their bits", favorites.factory[13][0] == 0xFFu && favorite_has(13, 3));
}

static void test_filter(void)
{
    uint32_t st, good = 1;
    for (st = 0xF8u; st <= 0xFFu; st++) {
        uint32_t want = st == 0xF8u || st == 0xFAu || st == 0xFBu || st == 0xFCu ? 0xFu | st << 8 : 0u;
        good &= bt_midi_filter(0xFu | st << 8 | 0x12340000u) == want;
    }
    ok("filter: F8 FA FB FC pass as their status alone, the other realtime bytes are dropped", good);
    ok("filter: a note-on passes unchanged", bt_midi_filter(msg(0x93, 60, 100)) == msg(0x93, 60, 100));
    ok("filter: a data byte above 0x7F is dropped",
       !bt_midi_filter(msg(0x90, 0x80, 100)) && !bt_midi_filter(msg(0x90, 60, 0x80)));
    ok("filter: a program change keeps its one data byte", bt_midi_filter(0x0Cu | 0xC1u << 8 | 5u << 16) != 0u);
    ok("filter: SysEx packets (CIN 4..7) are dropped",
       !bt_midi_filter(0x04u | 0xF0u << 8) && !bt_midi_filter(0x05u | 0xF7u << 8) && !bt_midi_filter(0x07u | 0x10u << 8));
    ok("filter: a CIN that is not its status' is dropped", !bt_midi_filter(0x09u | 0x80u << 8 | 60u << 16));
}

static void test_midi_mode_in(void)
{
    uint32_t at, i, w;
    fresh(BT_MODE_MIDI);
    link_to("Xkey Air");
    bt_midi_in(msg(0x90, 60, 100));
    at = mi_r % MQ;
    ok("MIDI mode: a note-on lands in the input ring as source 3",
       mi_w - mi_r == 1u && midi_in_source[at] == 3u && midi_in_q[at] == msg(0x90, 60, 100));
    events_block(256);
    ok("MIDI mode: channel 1 plays track 1", gate_note(&trk[0], 60));
    bt_midi_in(msg(0x80, 60, 0));
    events_block(256);
    ok("MIDI mode: the note-off releases it", !gate_note(&trk[0], 60));
    ok("MIDI mode: nothing goes to the SMC layout", !bt_mode_now());
    bt_midi_in(0x04u | 0xF0u << 8);
    ok("MIDI mode: a dropped packet does not reach the ring and is not a drop", mi_w == mi_r && !bt_in.drops);
    /* the room kept for TRS: with 8 slots left a BLE packet is refused and counted, and never sets the overflow */
    for (i = 0; i < MQ - 8u; i++)
        midi_enqueue(msg(0xB0, 7, 100), 1u);
    w = mi_w;
    bt_midi_in(msg(0x90, 61, 100));
    bt_midi_in(msg(0x90, 62, 100));
    ok("a full ring: BLE packets are refused, counted, and TRS keeps its 8 slots",
       mi_w == w && bt_in.drops == 2u && !midi_in_overflow);
    events_block(256);
    bt_midi_in(msg(0x90, 61, 100));
    ok("a full ring: BLE is taken again once the ring has room", mi_w - mi_r == 1u && bt_in.drops == 2u);
}

static void test_clock(void)
{
    uint32_t i;
    ok("CLK: the clock sources end with BLE (value 3)",
       GP[G_CLOCK].max == 3 && str_eq(GP[G_CLOCK].names[3], "BLE"));
    fresh(BT_MODE_MIDI);
    link_to("Xkey Air");
    song.g[G_CLOCK] = 3;
    events_block(256);
    fm1_ms = 2;
    bt_midi_in(0x0Fu | 0xFAu << 8);
    events_block(256);
    for (i = 0; i < 24u; i++) {
        fm1_ms = 2u + (i * 125u) / 6u;
        bt_midi_in(0x0Fu | 0xF8u << 8);
        events_block(256);
    }
    ok("CLK BLE: Start and clock pulses from the BLE device run the transport", song.playing);
    bt_midi_in(0x0Fu | 0xFCu << 8);
    events_block(256);
    ok("CLK BLE: Stop stops it", !song.playing);
    song.g[G_CLOCK] = 1;                             /* CLK USB: the BLE device's realtime bytes are not the clock */
    events_block(256);
    fm1_ms += 10u;
    bt_midi_in(0x0Fu | 0xFAu << 8);
    bt_midi_in(0x0Fu | 0xF8u << 8);
    events_block(256);
    ok("CLK USB: realtime bytes from BLE do nothing", !song.playing);
}

static void test_mode(void)
{
    fresh(BT_MODE_AUTO);
    ok("AUTO, nothing connected: MIDI", !bt_mode_now());
    link_to("SMC-Mixer");
    ok("AUTO, a device named SMC...: the SMC layout", bt_mode_now() == 1u);
    bt_midi_in(msg(0x90, 0x5E, 127));
    ok("SMC layout: received MIDI goes to the layout, not the ring", mi_w == mi_r);
    fresh(BT_MODE_AUTO);
    link_to("Xkey Air");
    ok("AUTO, any other device: MIDI", !bt_mode_now());
    fresh(BT_MODE_MIDI);
    link_to("SMC-Mixer");
    ok("MODE MIDI overrides the name", !bt_mode_now());
    fresh(BT_MODE_SMC);
    link_to("Xkey Air");
    ok("MODE SMC DAW overrides the name", bt_mode_now() == 1u);
    conn_up = 0;
    bt_link(0);
    fresh(BT_MODE_AUTO);
    ok("the link down: AUTO is MIDI again", !bt_mode_now());
}

static void test_out(void)
{
    uint32_t i;
    fresh(BT_MODE_MIDI);
    bt_midi_out_isr(msg(0x90, 60, 100));
    room = 8;
    bt_midi_frame();
    ok("MIDI out: nothing is sent while no device is connected", !nsent && !bt_in.out_drops);
    link_to("Xkey Air");
    room = 8;
    bt_midi_out_isr(msg(0x91, 60, 100));
    bt_midi_out_isr(0x0Cu | 0xC2u << 8 | 5u << 16);
    bt_midi_out_isr(0x0Fu | 0xF8u << 8);             /* realtime: not sent */
    bt_midi_out_isr(0x04u | 0xF0u << 8);             /* SysEx: not sent */
    bt_midi_frame();
    ok("MIDI out: channel voice messages reach the device in order, with their length",
       nsent == 2u && sent_n[0] == 3u && sent[0][0] == 0x91u && sent[0][1] == 60u && sent[0][2] == 100u &&
       sent_n[1] == 2u && sent[1][0] == 0xC2u && sent[1][1] == 5u);
    nsent = 0;
    room = 1;
    bt_midi_out_isr(msg(0x80, 60, 0));
    bt_midi_out_isr(msg(0x80, 61, 0));
    bt_midi_frame();
    ok("MIDI out: with room for one, one is sent and the other waits", nsent == 1u && sent[0][1] == 60u);
    room = 4;
    bt_midi_frame();
    ok("MIDI out: the waiting message is sent when room returns", nsent == 2u && sent[1][1] == 61u);
    nsent = 0;
    room = 0;
    for (i = 0; i < 40u; i++)
        bt_midi_out_isr(msg(0x90, i, 1));
    ok("MIDI out: a full queue drops and counts", bt_in.out_drops == 40u - BT_OUT_Q);
    fresh(BT_MODE_AUTO);
    link_to("SMC-Mixer");
    room = 8;
    bt_midi_out_isr(msg(0x90, 60, 100));
    bt_midi_frame();
    ok("MIDI out: in the SMC layout the keys are not sent to the mixer", !nsent);
    /* the keys: a key down on the FM-1 sends its note to the device (seq.c key_on) */
    fresh(BT_MODE_MIDI);
    link_to("Xkey Air");
    room = 8;
    fm1_in.notes = 1u << 7;
    events_block(256);
    bt_midi_frame();
    ok("MIDI out: an FM-1 key down sends a note-on on the track's channel",
       nsent == 1u && sent_n[0] == 3u && (sent[0][0] & 0xF0u) == 0x90u && sent[0][2] > 0u);
    fm1_in.notes = 0;
    events_block(256);
    bt_midi_frame();
    ok("MIDI out: the key up sends its note-off", nsent == 2u && (sent[1][0] & 0xF0u) == 0x80u && sent[1][1] == sent[0][1]);
}

static void test_link_drop(void)
{
    fresh(BT_MODE_MIDI);
    link_to("Xkey Air");
    bt_midi_in(msg(0x90, 60, 100));
    bt_midi_in(msg(0x91, 64, 100));
    bt_midi_in(msg(0xB0, 64, 127));                  /* the pedal down on channel 1 */
    events_block(256);
    ok("held from BLE: both notes sound", gate_note(&trk[0], 60) && gate_note(&trk[1], 64));
    conn_up = 0;
    bt_link(0);                                      /* the device is switched off */
    bt_midi_frame();
    events_block(256);
    ok("the link drops: the notes the device held are released, the pedal too",
       !gate_note(&trk[0], 60) && !gate_note(&trk[1], 64));
    ok("the link drops: nothing is left to release", !bt_in.release);
    /* with the ring full the release waits, and happens when there is room */
    fresh(BT_MODE_MIDI);
    link_to("Xkey Air");
    bt_midi_in(msg(0x90, 60, 100));
    events_block(256);
    while (MQ - (mi_w - mi_r) > 8u)
        midi_enqueue(msg(0xB0, 7, 100), 1u);
    conn_up = 0;
    bt_link(0);
    bt_midi_frame();
    ok("the link drops with the ring full: the release waits, no overflow", bt_in.release && !midi_in_overflow);
    events_block(256);
    bt_midi_frame();
    events_block(256);
    ok("the link drops with the ring full: released once there is room", !gate_note(&trk[0], 60) && !bt_in.release);
    /* the ring refusing (USB / TRS overflowed it: midi_enqueue takes nothing until the audio side has recovered):
     * the release is still owed, never half sent and forgotten */
    fresh(BT_MODE_MIDI);
    link_to("Xkey Air");
    bt_midi_in(msg(0x92, 67, 100));
    events_block(256);
    midi_in_overflow = 1;
    conn_up = 0;
    bt_link(0);
    bt_midi_frame();
    ok("the link drops while the ring refuses input: the release stays owed", bt_in.release == (1u << 2) && mi_w == mi_r);
    midi_in_overflow = 0;
    bt_midi_frame();
    ok("... and is sent whole once the ring takes input: the pedal up, then All Notes Off",
       !bt_in.release && mi_w - mi_r == 2u && midi_in_q[mi_r % MQ] == (0x0Bu | 0xB2u << 8 | 64u << 16) &&
       midi_in_q[(mi_r + 1u) % MQ] == (0x0Bu | 0xB2u << 8 | 123u << 16));
}

int main(void)
{
    test_settings();
    test_filter();
    test_midi_mode_in();
    test_clock();
    test_mode();
    test_out();
    test_link_drop();
    printf(fails ? "bt_midi: %d FAILED\n" : "bt_midi: all checks passed\n", fails);
    return fails != 0;
}
