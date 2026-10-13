# The Bluetooth build: does it fit?

BLE MIDI: 2026 DEADACTIVE.

Felucca 1.6 with the BLE stack of Hortator 0.16.0 (`FELUCCA_BLE=1 ./build.sh`), measured against the app slot
(`APP_SLOT` = 581,564 B). The rule (the user, 2026-10-13): the Bluetooth build leaves out the fewest built-in SAMPLE
sets for which the image keeps at least 32,768 B of the slot free. The pool keeps 8,192 B spare, `.data + .bss`
4,096 B, as in every build. `FELUCCA_BLE=1 BLE_MEASURE=1 FELUCCA_SMP_DROP=… ./build.sh` measures a candidate (the
regions widened; it never makes a package).

## The stack linked into 1.6 (task 2: no page, no MIDI routing yet)

| | image B | slot free B | `.data + .bss` B (of 73,728) | pool B (of 368,640) |
| --- | ---: | ---: | ---: | ---: |
| Felucca 1.6, plain | 508,108 | 73,456 | 55,300 | 328,896 |
| + the BLE stack, every set | 636,844 | −55,280 | 67,612 (6,116 spare) | 352,640 (16,000 spare) |

The stack costs 128,736 B of flash, 12,312 B of `.data + .bss` and 23,744 B of pool (a 13,824 B heap and four task
slots), the same as in Hortator.

## The built-in SAMPLE sets

| set | zones | ADPCM B |
| --- | ---: | ---: |
| PIANO (also sets 1 and 4, its aliases, and SLICE's PIANO) | 5 | 41,345 |
| FLUTE | 3 | 31,422 |
| SAX | 3 | 31,422 |
| SLICE's BREAK (not a SAMPLE set; kept) | | 22,055 |

| left out | image B | slot free B | |
| --- | ---: | ---: | --- |
| FLUTE, SAX | 573,892 | 7,672 | fits the slot, not the margin |
| PIANO, FLUTE | 562,944 | 18,620 | not the margin |
| **PIANO, FLUTE, SAX** | **531,376** | **50,188** | **the margin, with room for the page and the SMC layout** |

So the Bluetooth build leaves out all three (`tools/build.py` `BLE_SMP_DROP`). In it the SAMPLE and GRAIN engines
play the user slots USR1-3; a sound that selects PIANO, FLUTE or SAX plays a sine at the note's pitch and the screen
says NO SAMPLE once; SLICE keeps BREAK and the user slots and has no PIANO source. The plain build has every set.

## The whole Bluetooth build (the page, MIDI mode, the SMC layout)

| | image B | slot free B | `.data + .bss` B (of 73,728) | pool B (of 368,640) |
| --- | ---: | ---: | ---: | ---: |
| the stack alone, the three sets left out | 531,376 | 50,188 | 67,612 | 352,640 |
| + MIDI mode, the settings bytes | 532,008 | 49,556 | 67,756 | 352,640 |
| + the Bluetooth page | 535,272 | 46,292 | 67,756 | 352,936 |
| + the SMC-Mixer layout | 536,936 | 44,628 | 67,756 | 352,992 |
| **after the review's fixes (the build as it is)** | **537,192** | **44,372** | **67,756** (5,972 spare, 4,096 kept) | **352,992** (15,648 spare, 8,192 kept) |

The core's side costs 5,816 B of flash and 144 B of RAM; the page's and the layout's state is in the pool.

Keeping one sustained set (FLUTE or SAX) would leave 18,620 B before the page and the SMC layout, about 13 KB after
them: possible if the margin is given up. That is the user's call:
`FELUCCA_SMP_DROP=PIANO,FLUTE FELUCCA_BLE=1 ./build.sh` builds that variant, and the build then stops on the 32 KB
check until the check is lowered.
