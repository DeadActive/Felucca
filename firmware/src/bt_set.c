/* SPDX-License-Identifier: GPL-3.0-only
 * BLE MIDI: 2026 DEADACTIVE
 * FM-Link: 2026 DEADACTIVE */
/* FELUCCA_BLE: what the Bluetooth page keeps over power-off, in the settings record (settings_persist.c: persist_t,
 * its magic and size unchanged): MODE and the remembered device in favorites.factory[13][24..31],
 *   [24..29] the device's address, [30] bits 0-1 MODE, bit 2 the address type (0 public, 1 random), bit 3
 *   AUTOSTART (Bluetooth on by itself after a clean boot, bt_page.c bt_page_auto), bit 4 ROLE (0 HOST, 1 DEVICE),
 *   bit 5 retired (ROLE HOST + LINK, until the saved devices: bt_devs.c; written 0, read and ignored). The address
 *   bytes [24..29] and bit 2 are no longer used either (the saved devices: bt_devs.c): left as saved.
 *   [31] the XOR of the seven bytes before it, XOR 0xB7.
 * Row 13 is the SLICE engine's favourites, a bit per factory preset; bytes 24..31 would be its presets 192..255,
 * which do not exist. A record without the bytes (all zero: every older setting) or with a wrong check byte is no
 * device, MODE AUTO. The plain build never reads them and keeps them as saved, as every byte of that table it does
 * not use. (persist_t's own fields cannot hold them: lowcut is read % 3, zoom and bold are tagged values.)
 * Main loop only. */
enum { BT_MODE_AUTO, BT_MODE_SMC, BT_MODE_MIDI, BT_MODE_LINK };   /* LINK: FM-Link only, no MIDI either way */
typedef struct { uint8_t addr[6], type, mode, has, autostart, role, link; } bt_set_t;   /* has: a device is
                                                                  * remembered; role: 1 DEVICE; link: HOST + LINK */
#define bt_set_bytes (&favorites.factory[13][24])

static uint8_t bt_set_sum(const uint8_t *b)
{
    uint32_t i, x = 0xB7u;
    for (i = 0; i < 7u; i++)
        x ^= b[i];
    return (uint8_t)x;
}

static void bt_set_get(bt_set_t *s)
{
    const uint8_t *b = bt_set_bytes;
    uint32_t i;
    memset(s, 0, sizeof *s);
    if (b[7] != bt_set_sum(b) || (b[6] & ~63u))
        return;
    for (i = 0; i < 6u; i++) {
        s->addr[i] = b[i];
        s->has |= b[i] != 0u;
    }
    s->type = (b[6] >> 2) & 1u;
    s->mode = b[6] & 3u;
    s->autostart = (b[6] >> 3) & 1u;
    s->role = (b[6] >> 4) & 1u;
    s->link = (b[6] >> 5) & 1u;
}

/* the bytes written and the settings saved (project.c settings_save: at once, or when the transport next stops) */
static void bt_set_put(const bt_set_t *s)
{
    uint8_t *b = bt_set_bytes;
    uint32_t i;
    for (i = 0; i < 6u; i++)
        b[i] = s->addr[i];
    b[6] = (uint8_t)((s->mode <= BT_MODE_LINK ? s->mode : BT_MODE_AUTO) | (s->type ? 4u : 0u) | (s->autostart ? 8u : 0u) |
                     (s->role ? 16u : 0u) | (s->link ? 32u : 0u));
    b[7] = bt_set_sum(b);
    settings_save();
}
