/* SPDX-License-Identifier: GPL-3.0-only
 * BLE MIDI: 2026 DEADACTIVE */
/* The SMC-Mixer DAW layout (firmware/src/smc.c; docs/superpowers/specs/2026-10-13-ble-midi-design.md section 11) on
 * the host, built with FELUCCA_BLE=1 and the UI harness: the decode (pitch bend faders, sign-magnitude knobs, the
 * note rows), each action on Felucca's four tracks, the fader pickup, the LEDs (only changes, everything on a new
 * link), solo, and what happens when the mixer goes or MODE leaves SMC DAW. The BLE unit's entry points are stubs. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#if !FELUCCA_BLE
#error "build with -DFELUCCA_BLE=1"
#endif
#include "ble_ui_stub.h"
static uint8_t sent[128][3];
static uint32_t nsent, room, refused;
int ble_midi_send(const uint8_t *m, uint32_t n)
{
    if (!room || nsent >= 128u || n != 3u) {
        refused++;
        return 0;
    }
    memcpy(sent[nsent++], m, 3);
    room--;
    return 1;
}
uint32_t ble_midi_room(void) { return room; }
#include "../firmware/src/bt_set.c"
#include "../firmware/src/smc.c"
#include "../firmware/src/bt_midi.c"
#include "../firmware/src/bt_page.c"

static int fails;
static void ok(const char *what, int good)
{
    printf("%s  %s\n", good ? "ok  " : "FAIL", what);
    if (!good)
        fails++;
}
static uint32_t note(uint32_t n, uint32_t vel) { return 0x09u | 0x90u << 8 | n << 16 | vel << 24; }
static uint32_t bend(uint32_t ch, uint32_t v7) { return 0x0Eu | (0xE0u | ch) << 8 | (v7 & 1u ? 0x7Fu : 0u) << 16 | v7 << 24; }
static uint32_t cc(uint32_t n, uint32_t v) { return 0x0Bu | 0xB0u << 8 | n << 16 | v << 24; }
static void drain(void) { room = 1000; smc_frame(); }
static void set_mode(uint32_t mode)
{
    bt_set_t s;
    bt_set_get(&s);
    s.mode = (uint8_t)mode;
    bt_set_put(&s);
}
static void link_up(const char *name)
{
    memset(conn_name, 0, sizeof conn_name);
    str_cpy(conn_name, name, sizeof conn_name);
    conn_up = 1;
    bt_link(1);
    smc_link(1);
}
static void link_down(void)
{
    conn_up = 0;
    bt_link(0);
    smc_link(0);
}
static void fresh(void)                              /* power-on, the SMC-Mixer linked, its LEDs and positions sent */
{
    ui_power_on();
    memset(&favorites, 0, sizeof favorites);
    link_down();
    smc_frame();
    set_mode(BT_MODE_AUTO);
    link_up("SMC-Mixer");
    drain();
    nsent = refused = 0;
}
static int sent_is(uint32_t i, uint32_t a, uint32_t b, uint32_t c) { return i < nsent && sent[i][0] == a && sent[i][1] == b && sent[i][2] == c; }
static int sent_has(uint32_t a, uint32_t b, uint32_t c)
{
    uint32_t i;
    for (i = 0; i < nsent; i++)
        if (sent_is(i, a, b, c))
            return 1;
    return 0;
}
static uint32_t knob_id(uint32_t k) { return ENGINES[TSEL->eng_req % NENGINES]->knob[k]; }

static void test_decode_faders(void)
{
    uint32_t t;
    fresh();
    trk[0].p[P_LEVEL] = 127;
    drain();
    smc_in(0x0Eu | 0xE0u << 8 | 0x7Fu << 16 | 0x7Fu << 24);
    smc_in(0x0Eu | 0xE0u << 8 | 0x00u << 16 | 0x40u << 24);
    ok("a fader's 14-bit pitch bend is its top 7 bits: 8192 -> 64", trk[0].p[P_LEVEL] == 64);
    smc_in(0x0Eu | 0xE0u << 8);
    ok("pitch bend 0 -> 0", trk[0].p[P_LEVEL] == 0);
    smc_in(0x0Eu | 0xE0u << 8 | 0x7Fu << 16 | 0x7Fu << 24);
    ok("pitch bend 16383 -> 127", trk[0].p[P_LEVEL] == 127);
    for (t = 0; t < 4u; t++) {
        fresh();
        smc_in(bend(t, 104));                        /* at the track's LEVEL (104 at power-on): picked up */
        smc_in(bend(t, 60 + t));
        ok("fader 1..4 sets its track's LEVEL", trk[t].p[P_LEVEL] == (int16_t)(60 + t));
        smc_in(bend(4u + t, (uint32_t)trk[t].p[P_REV]));   /* at the track's REV send: picked up */
        smc_in(bend(4u + t, 100 + t));
        ok("fader 5..8 sets track 1..4's REV send", trk[t].p[P_REV] == (int16_t)(100 + t) && trk[t].p[P_LEVEL] == (int16_t)(60 + t));
    }
    fresh();
    {
        static track_t before[NTRK];
        memcpy(before, trk, sizeof before);
        smc_in(bend(8, 0));
        smc_in(bend(9, 0));
        smc_in(bend(15, 0));
        drain();
        ok("the master fader (channel 9) and the channels above do nothing", !memcmp(before, trk, sizeof before) && !nsent);
    }
}

static void test_pickup(void)
{
    fresh();
    smc_in(bend(0, 50));
    ok("pickup: a fader away from the LEVEL changes nothing", trk[0].p[P_LEVEL] == 104);
    smc_in(bend(0, 101));
    ok("pickup: 3 away is still away", trk[0].p[P_LEVEL] == 104);
    smc_in(bend(0, 102));
    ok("pickup: within 2 it is picked up and the LEVEL follows", trk[0].p[P_LEVEL] == 102);
    smc_in(bend(0, 20));
    ok("pickup: a picked-up fader follows anywhere", trk[0].p[P_LEVEL] == 20);
    fresh();
    smc_in(bend(1, 20));
    smc_in(bend(1, 120));                            /* from below the LEVEL to above it, never within 2 */
    ok("pickup: crossing the LEVEL picks it up", trk[1].p[P_LEVEL] == 120);
    fresh();
    smc_in(bend(1, 120));
    ok("pickup: the first report cannot cross (nothing to cross from)", trk[1].p[P_LEVEL] == 104);
    /* the LEVEL changed by other means: its fader is unpicked and the new position is sent */
    fresh();
    smc_in(bend(0, 104));
    smc_in(bend(0, 90));
    drain();
    nsent = 0;
    trk[0].p[P_LEVEL] = 30;                          /* (the FM-1's knob, a load, automation) */
    drain();
    ok("a LEVEL changed elsewhere: its fader's new position is sent (E0 v v)", nsent == 1u && sent_is(0, 0xE0, 30, 30));
    smc_in(bend(0, 80));
    ok("a LEVEL changed elsewhere: its fader is unpicked", trk[0].p[P_LEVEL] == 30);
    smc_in(bend(0, 29));
    ok("... and picks up again near the new LEVEL", trk[0].p[P_LEVEL] == 29);
    drain();                                         /* (fader 1's own echo) */
    nsent = 0;
    trk[2].p[P_REV] = 77;
    drain();
    ok("a REV changed elsewhere: fader 7's position is sent (E6 v v)", nsent == 1u && sent_is(0, 0xE6, 77, 77));
    nsent = 0;
    smc_in(bend(0, 29));
    drain();
    ok("a fader report that changes nothing sends nothing", !nsent);
}

static void test_knobs(void)
{
    uint32_t k, id;
    int16_t v0;
    track_t other;
    fresh();
    track_select(1);
    other = trk[0];
    for (k = 0; k < 4u; k++) {
        id = knob_id(k);
        v0 = trk[1].p[id];
        smc_in(cc(16u + k, 1));
        smc_in(cc(16u + k, 1));
        smc_in(cc(16u + k, 0x41));
        ok("knob 1..4: a click steps the selected track's HOME parameter (bit 6: counter-clockwise)",
           trk[1].p[id] != v0 && trk[1].p[id] == (int16_t)param_turn(track_desc(&trk[1], id), v0, 1));
    }
    id = knob_id(0);
    for (k = 0; k < 40u; k++)
        smc_in(cc(16, 0x3F));
    ok("a knob stops at the parameter's maximum", trk[1].p[id] == track_desc(&trk[1], id)->max);
    for (k = 0; k < 40u; k++)
        smc_in(cc(16, 0x7F));
    ok("... and at its minimum", trk[1].p[id] == track_desc(&trk[1], id)->min);
    v0 = trk[1].p[id];
    smc_in(cc(16, 0));
    smc_in(cc(16, 0x40));
    ok("a knob message of no clicks does nothing", trk[1].p[id] == v0);
    {
        track_t before = trk[1];
        for (k = 20; k < 24u; k++)
            smc_in(cc(k, 1));                        /* (knobs 5..8, nothing learned) */
        smc_in(cc(7, 100));
        smc_in(0x0Bu | 0xB1u << 8 | 16u << 16 | 1u << 24);   /* CC 16 on channel 2 */
        ok("knobs 5..8 with nothing learned, other CCs and other channels do nothing", !memcmp(before.p, trk[1].p, sizeof before.p));
    }
    ok("the other tracks' parameters are untouched", !memcmp(other.p, trk[0].p, sizeof other.p));
}

static void test_learn(void)
{
    int16_t v;
    uint32_t i;
    fresh();
    ml_clear_all();
    ui.ml.on = 1;
    ml_knob(&trk[2], P_REV);                         /* MIDI LEARN: REV of track 3 picked */
    smc_in(cc(20, 1));                               /* knob 5 turned a click */
    ml_poll();
    ok("SMC: knob 5 teaches MIDI LEARN its CC (20)", ml_cc_of(2, P_REV) == 21u);
    ok("... and the click itself changes nothing while it was learning", trk[2].p[P_REV] == 20);
    ui.ml.on = 0;
    v = trk[2].p[P_REV];
    smc_in(cc(20, 1));
    ok("SMC: knob 5, learned: a click steps its parameter by one value, from where it is", trk[2].p[P_REV] == v + 1);
    smc_in(cc(20, 3));
    ok("three clicks: three values", trk[2].p[P_REV] == v + 4);
    smc_in(cc(20, 0x42));
    ok("counter-clockwise: down", trk[2].p[P_REV] == v + 2);
    trk[2].p[P_REV] = 90;                            /* changed on the FM-1: the knob goes on from there, no jump */
    smc_in(cc(20, 1));
    ok("a value changed elsewhere: the knob goes on from it", trk[2].p[P_REV] == 91);
    for (i = 0; i < 30u; i++)
        smc_in(cc(20, 0x3F));
    ok("a learned knob stops at the parameter's maximum", trk[2].p[P_REV] == TP[P_REV].max);
    ok("the other tracks are untouched", trk[1].p[P_REV] == 70 && trk[3].p[P_REV] == 20);
    /* a two-valued parameter: a click is a step, not 1/127 of the range */
    ui.ml.on = 1;
    ml_knob(&trk[0], P_MUTE);
    smc_in(cc(23, 1));
    ml_poll();
    ui.ml.on = 0;
    smc_in(cc(23, 1));
    ok("SMC: knob 8 learned to MUTE: a click right mutes", trk[0].p[P_MUTE] == 1);
    smc_in(cc(23, 0x41));
    ok("... a click left unmutes", trk[0].p[P_MUTE] == 0);
    /* a learned CC from the mixer's knob 1..4 is not taken from the layout: they stay the HOME parameters */
    ui.ml.on = 1;
    ml_knob(&trk[0], P_PAN);
    smc_in(cc(16, 1));
    ml_poll();
    ui.ml.on = 0;
    ok("SMC: knobs 1..4 are the layout's, MIDI LEARN does not take them", ml_cc_of(0, P_PAN) == 0u);
    ui.confirm = 1;
    v = trk[2].p[P_REV];
    smc_in(cc(20, 0x41));
    ok("a learned knob waits while a dialog is open", trk[2].p[P_REV] == v);
    ui.confirm = 0;
    ml_clear_all();
}

static void test_buttons(void)
{
    uint32_t n;
    fresh();
    for (n = 0; n < 4u; n++) {
        smc_in(note(0x10u + n, 127));
        ok("MUTE n mutes track n", trk[n].p[P_MUTE] == 1);
        smc_in(note(0x10u + n, 0));
        ok("the release does nothing", trk[n].p[P_MUTE] == 1);
        smc_in(note(0x10u + n, 127));
        ok("MUTE n again unmutes it", trk[n].p[P_MUTE] == 0);
        smc_in(note(0x18u + n, 127));
        ok("SELECT n selects track n", song.sel == n);
    }
    smc_in(note(0x08u + 2u, 127));
    ok("SOLO 3 solos track 3", smc_solo == 4u);
    smc_in(note(0x08u + 0u, 127));
    ok("SOLO 1 too: both soloed", smc_solo == 5u);
    smc_in(note(0x08u + 2u, 127));
    ok("SOLO 3 again: only track 1", smc_solo == 1u);
    smc_in(note(0x08u, 127));
    ok("SOLO 1 again: none", smc_solo == 0u);
    /* REC n: the track selected and armed; armed while stopped starts the transport, as REC on the FM-1 */
    fresh();
    smc_in(note(0x00u + 2u, 127));
    ok("REC 3 selects track 3, arms it and starts the transport", song.sel == 2u && song.rec == 4u && transport_req == 1u);
    transport_req = 0;
    smc_in(note(0x00u + 2u, 127));
    ok("REC 3 again disarms it", song.rec == 0u);
    smc_in(note(0x5Fu, 127));
    ok("REC (transport) arms the selected track", song.rec == 4u);
    smc_in(note(0x5Fu, 127));
    ok("REC again disarms it", song.rec == 0u);
    /* the transport */
    fresh();
    smc_in(note(0x5Du, 127));
    ok("STOP while stopped does nothing", transport_req == 0u);
    smc_in(note(0x5Eu, 127));
    ok("PLAY while stopped starts", transport_req == 1u);
    events_block(256);
    ok("... and the transport runs", song.playing && transport_req == 0u);
    smc_in(note(0x5Eu, 127));
    ok("PLAY while playing does nothing", transport_req == 0u);
    smc_in(note(0x5Du, 127));
    ok("STOP while playing stops", transport_req == 2u);
    events_block(256);
    ok("... and the transport is stopped", !song.playing);
    /* the previous / next track, wrapping */
    fresh();
    smc_in(note(0x2Fu, 127));
    ok("BANK > is the next track", song.sel == 1u);
    smc_in(note(0x31u, 127));
    ok("CHANNEL > is the next track", song.sel == 2u);
    smc_in(note(0x2Eu, 127));
    smc_in(note(0x30u, 127));
    ok("BANK < and CHANNEL < are the previous track", song.sel == 0u);
    smc_in(note(0x2Eu, 127));
    ok("the previous track from the first is the last", song.sel == 3u);
    smc_in(note(0x2Fu, 127));
    ok("the next track from the last is the first", song.sel == 0u);
    /* what does nothing */
    fresh();
    {
        static const uint8_t NOP[] = {0x04, 0x07, 0x0C, 0x0F, 0x14, 0x17, 0x1C, 0x1F, 0x5B, 0x5C, 0x60, 0x61, 0x62, 0x63, 0x20, 0x7F};
        uint32_t i, same = 1;
        for (i = 0; i < sizeof NOP; i++) {
            smc_in(note(NOP[i], 127));
            same &= song.sel == 0u && !song.rec && !smc_solo && !transport_req && !trk[0].p[P_MUTE] && !trk[3].p[P_MUTE];
        }
        ok("strips 5..8's buttons, REW, FF, the arrows and unknown notes do nothing", same);
        smc_in(0x09u | 0x91u << 8 | 0x10u << 16 | 127u << 24);
        smc_in(0x08u | 0x80u << 8 | 0x10u << 16 | 127u << 24);
        ok("a note on another channel and a note-off do nothing", !trk[0].p[P_MUTE]);
    }
    fresh();
    ui.confirm = 1;
    smc_in(note(0x10u, 127));
    smc_in(note(0x5Eu, 127));
    smc_in(note(0x1Au, 127));
    smc_in(cc(16, 1));
    ok("while a dialog is open the buttons and knobs do nothing", !trk[0].p[P_MUTE] && !transport_req && song.sel == 0u);
    ui.confirm = 0;
}

static void test_leds(void)
{
    uint32_t i, off = 0, pos = 0;
    ui_power_on();
    memset(&favorites, 0, sizeof favorites);
    link_down();
    smc_frame();
    set_mode(BT_MODE_AUTO);
    nsent = 0;
    link_up("SMC-Mixer");
    drain();
    for (i = 0; i < nsent; i++) {
        off += sent[i][0] == 0x90u && sent[i][2] == 0u;
        pos += (sent[i][0] & 0xF0u) == 0xE0u;
    }
    ok("a new link: every LED is sent (32 strip buttons, STOP PLAY REC) and all 8 fader positions", nsent == 43u && pos == 8u);
    ok("a new link: SELECT 1 and STOP lit, the rest dark",
       sent_has(0x90, 0x18, 127) && sent_has(0x90, 0x5D, 127) && sent_has(0x90, 0x5E, 0) && sent_has(0x90, 0x5F, 0) && off == 33u);
    ok("a new link: the positions are the tracks' LEVELs and REV sends",
       sent_has(0xE0, 104, 104) && sent_has(0xE3, 104, 104) &&
       sent_has(0xE4, (uint32_t)trk[0].p[P_REV], (uint32_t)trk[0].p[P_REV]) &&
       sent_has(0xE7, (uint32_t)trk[3].p[P_REV], (uint32_t)trk[3].p[P_REV]) && trk[1].p[P_REV] != trk[0].p[P_REV]);
    nsent = 0;
    drain();
    drain();
    ok("nothing changed: nothing is sent", !nsent);
    trk[1].p[P_MUTE] = 1;                            /* the FM-1's own MUTE */
    drain();
    ok("a mute on the FM-1: exactly MUTE 2's LED", nsent == 1u && sent_is(0, 0x90, 0x11, 127));
    nsent = 0;
    track_select(2);
    drain();
    ok("another track selected: SELECT 1 off, SELECT 3 on", nsent == 2u && sent_has(0x90, 0x18, 0) && sent_has(0x90, 0x1A, 127));
    nsent = 0;
    smc_in(note(0x08u + 3u, 127));
    drain();
    ok("SOLO 4: its LED", nsent == 1u && sent_is(0, 0x90, 0x0B, 127));
    nsent = 0;
    song.rec = 1u << 2;
    song.playing = 1;
    drain();
    ok("armed and playing: REC 3, PLAY and REC lit, STOP dark",
       nsent == 4u && sent_has(0x90, 0x02, 127) && sent_has(0x90, 0x5E, 127) && sent_has(0x90, 0x5F, 127) && sent_has(0x90, 0x5D, 0));
    song.playing = 0;
    song.rec = 0;
    drain();
    /* no room: nothing sent, nothing lost */
    nsent = refused = 0;
    trk[0].p[P_MUTE] = 1;
    trk[3].p[P_LEVEL] = 9;
    room = 0;
    smc_frame();
    smc_frame();
    ok("no room in the BLE queue: nothing is sent and nothing is refused", !nsent && !refused);
    room = 1;
    smc_frame();
    ok("room for one: one message", nsent == 1u);
    drain();
    ok("room again: the rest follows, nothing lost", nsent == 2u && sent_has(0x90, 0x10, 127) && sent_has(0xE3, 9, 9));
}

static void test_solo_sound(void)
{
    fresh();
    smc_in(note(0x08u + 1u, 127));                   /* SOLO 2 */
    perf_kill = 0;
    perf_begin(256);
    ok("solo: the other tracks are muted in the mix (as a black key in the FX layer)", ((pf.act >> PF_M1) & 15u) == 0xDu);
    perf_kill = 1;                                   /* the menu open: the GLO layer's own solo is off then */
    perf_begin(256);
    ok("solo: it holds with the menu open", ((pf.act >> PF_M1) & 15u) == 0xDu);
    smc_in(note(0x08u + 1u, 127));
    perf_begin(256);
    ok("unsolo: every track sounds again", ((pf.act >> PF_M1) & 15u) == 0u);
    perf_kill = 0;
    ok("solo leaves the tracks' MUTE alone", !trk[0].p[P_MUTE] && !trk[2].p[P_MUTE] && !trk[3].p[P_MUTE]);
}

static void test_gone(void)
{
    uint32_t i, zero = 0;
    fresh();
    smc_in(note(0x08u, 127));
    link_down();
    smc_frame();
    ok("the mixer gone: solo is cleared", smc_solo == 0u);
    nsent = 0;
    drain();
    ok("the mixer gone: nothing is sent", !nsent);
    link_up("SMC-Mixer");
    drain();
    ok("the mixer back: everything is sent again", nsent == 43u);
    /* MODE leaves SMC DAW while linked: solo off, the LEDs turned off once */
    fresh();
    smc_in(note(0x08u, 127));
    drain();
    nsent = 0;
    set_mode(BT_MODE_MIDI);
    drain();
    for (i = 0; i < nsent; i++)
        zero += sent[i][0] == 0x90u && sent[i][2] == 0u;
    ok("MODE MIDI while linked: solo is cleared and all 35 LEDs are turned off", smc_solo == 0u && nsent == 35u && zero == 35u);
    nsent = 0;
    drain();
    ok("... once", !nsent);
    mi_r = mi_w;
    bt_midi_in(note(0x10u, 127));
    ok("in MIDI mode a note goes to the input ring, not to the layout", !trk[0].p[P_MUTE] && mi_w - mi_r == 1u);
    mi_r = mi_w;
    set_mode(BT_MODE_SMC);
    drain();
    ok("MODE SMC DAW again: everything is sent again", nsent == 43u);
    /* a device that is not the mixer, MODE AUTO: the layout sends nothing */
    fresh();
    link_down();
    smc_frame();
    link_up("Xkey Air");
    nsent = 0;
    drain();
    ok("another device in MODE AUTO: nothing is sent to it", !nsent);
}

int main(void)
{
    test_decode_faders();
    test_pickup();
    test_knobs();
    test_learn();
    test_buttons();
    test_leds();
    test_solo_sound();
    test_gone();
    printf(fails ? "smc: %d FAILED\n" : "smc: all checks passed\n", fails);
    return fails != 0;
}
