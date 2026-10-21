# FM-Link: two FM-1s in time over Bluetooth, without MIDI clock — design

Date: 2026-10-21. Branch `ble`. FM-Link: 2026 DEADACTIVE. Approved by the user in chat (2026-10-21).

## 1. Why

BLE MIDI clock (24 pulses a beat) between two FM-1s "wobbles in a large range" (the user, on two units). Bluetooth
delivers in connection events (15 ms here): pulses arrive in bunches, several in the same millisecond, then none.
Felucca's MIDI clock follower measures the tempo from the gaps between pulses (in whole milliseconds), so its tempo
and its step timing swing.

FM-Link sends no ticks. The two FM-1s agree on time and share a timeline; the follower runs its own steady clock at
the leader's tempo and keeps its beat position on the leader's.

## 2. What the user decided

- **Fixed leader:** the FM-1 on CLK INT leads; the follower is on the new **CLK LINK**. Only the leader's PLAY, STOP
  and BPM count; on a linked follower PLAY / STOP do nothing but say `FOLLOWING LINK`, and its BPM is the leader's.
- **Start on the next bar** when the follower joins a leader that plays (it connects, reconnects, or is set to LINK).
  A leader's PLAY from stop starts both at the same moment, about 80 ms after the press.
- **Two units** (one Bluetooth link, the existing HOST / DEVICE roles; either may lead).
- Clock and transport first; notes later.

## 3. Time

- Each unit's time is its TIMER4 tick count (24 MHz, the crystal that also clocks the audio: one sample is exactly
  544 ticks, `audio.c DAC_TICKS`). A block of 32 samples is stamped with the tick at which it will be heard
  (`t0 + (HALF_FRAMES + b) * DAC_TICKS`, as `audio.c` already computes for the keys' latency).
- **Offset, one way at a time** (amended while building: the simulation showed a round trip's halves are not equal
  over Bluetooth: a packet waits 0..15 ms for the next connection event, a reply most of one, a ~7 ms bias): the
  follower sends PING (its tick t0) and the leader TIME (its tick as it sends), each every 100..150 ms (random, so
  they are written at every phase of the connection interval; a reply is not: it is written just after the event
  that brought its question, the second bias the simulation showed). The leader answers each PING with PONG (t0, its
  tick when the PING came). Samples: `up = t_leader_rx - t0 = offset + delay up`, `down = t_leader_tx -
  t_follower_rx = offset - delay down`. Of the last 48 each way (about 6 s), the smallest `up` and the largest `down`
  are the packets that waited least; their mean is the estimate, and the offset in use follows it by 1/8 at each
  sample (no jump when the fastest packet leaves the window). Chosen by a sweep over 8 random links: worst cases
  0.94 ms at the start, 1.3 ms over 3 minutes, a BPM change, a lost link (`tests/fmlink_test.c`).

## 4. The timeline

The leader keeps `beats`: its sequencer's position in beats since PLAY (Q16), advanced every block by what the
sequencer advanced (`seq_n` samples at the BPM in force: samples a beat `= FS * 60 / BPM`). It sends:

| Message | Bytes | |
| --- | --- | --- |
| HELLO | `'H' version role` | every 500 ms; role 1 leader, 2 follower; the peer is an FM-Link FM-1 while one came in the last 2 s |
| PING | `'P' n t0[4]` | follower, every 100..150 ms |
| PONG | `'Q' n t0[4] t_leader_rx[4]` | leader, at once |
| TIME | `'T' t_leader_tx[4]` | leader, every 100..150 ms |
| STATE | `'S' n flags bpm t_ref[4] beats_ref[4]` | leader: on PLAY / STOP / a BPM change, and every second. flags bit 0 playing, bit 1 a fresh start (beats_ref 0 at t_ref) |

All little endian, at most 14 bytes: one BLE packet each.

## 5. Behaviour

- **Leader PLAY** (from stop, or GLO + PLAY) with a follower linked: the leader does not start at once; it announces
  STATE(start, `t_ref = now + 80 ms`, beats 0) and starts itself when its block heard at `t_ref` comes, as the
  follower does: both step 0 at the same moment. Without a follower: as now.
- **Follower:** while the leader plays and the offset is known, its expected position at a block heard at local `t`
  is `beats_ref + (t + offset - t_ref) / ticks_per_beat`. Not playing yet: it starts on the block where that reaches
  the start (0 for a fresh start, else the next bar: the next multiple of 4 beats), its own `beats` set to it, and that
  block's advance corrected by the remaining fraction (at most half a block). Playing: each block advances
  `n + c` samples, `c` = the error in samples / 4, at most ±1 a block (32 samples: the tempo held within 3 % while it
  corrects; within 4 samples, 90 µs, nothing). The follower's BPM is the leader's (set from STATE).
- **STOP** on the leader: STATE(stopped): the follower stops when it arrives.
- **BPM change** on the leader: STATE at once, `t_ref` / `beats_ref` at the change: the follower's position is
  measured in beats, so it re-aligns whatever the moment its own BPM changed.
- **Link lost:** the follower plays on at its BPM (CLK LINK without a link behaves as INT); at the next link it joins
  on the next bar if the leader plays.

## 6. Transport

A second GATT characteristic beside BLE-MIDI, its own service (128-bit UUIDs `F3110000-...`), WRITE WITHOUT RESPONSE |
NOTIFY. The DEVICE-role FM-1's table has it; the HOST-role FM-1 finds it in the same service discovery as BLE-MIDI
(optional: a device without it is plain BLE-MIDI). HOST → DEVICE: writes; DEVICE → HOST: notifications. BLE-MIDI keeps
working beside it (a Mac connects to a DEVICE FM-1 as before).

## 7. Code

- `firmware/src/fmlink.c` — the engine, pure (one struct per unit, times passed in): offset estimate, timeline,
  messages in and out, the per-block step. Host-tested by `tests/fmlink_test.c`: two engines, each its own crystal
  (±50 ppm, a random offset), joined by a simulated link (15 ms connection events, bunching, random delays, lost
  packets); checks: a fresh start within 1 ms of each other, the beat positions within 2 ms over minutes, a BPM change,
  a join on the next bar, STOP, a lost link.
- BLE unit: the FM-Link characteristic (`ble_gatt.c`), its discovery and send / receive, packets queued with the tick
  they came at; `ble_core.h`: `ble_fml_send`, `core_fml_rx`, `core_ticks`.
- Core: `bt_fml.c` (the engine's glue: ISR block hook from `seq.c events_block`, the main loop's frame, the UI message),
  `N_CLOCK` gains `LINK` (value 4). `audio.c` records each block's heard tick (guarded). All under `FELUCCA_BLE`: the
  plain build stays byte-identical to upstream.
- Gates as before; gate 2c's lists change only if the update path's code does.

## 8. Not in this step

Notes over FM-Link (later: events stamped a little in the future, played at the same sample on both); more than two
units; a manual offset trim.
