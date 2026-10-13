/* SPDX-License-Identifier: GPL-3.0-only
 * BLE MIDI: 2026 DEADACTIVE */
/* The plain build (no FELUCCA_BLE) opening a project the Bluetooth build saved with CLK BLE (G_CLOCK 3, a value the
 * plain build has no name for): the load clamps it to the plain build's last source (TRS), never past the names. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#if FELUCCA_BLE
#error "the plain build's test: build without FELUCCA_BLE"
#endif
static project_t pj;
int main(void)
{
    int bad = 0;
    ui_power_on();
    project_capture(&pj);
    pj.g[G_CLOCK] = 3;
    pj.sum = proj_sum(&pj);                           /* (as the Bluetooth build stored it) */
    bad += check("a project with CLK BLE loads in the plain build", project_restore_runtime(&pj) == 0);
    bad += check("its clock source is within the plain build's names (TRS)",
                 song.g[G_CLOCK] == GP[G_CLOCK].max && GP[G_CLOCK].max == 2 && str_eq(GP[G_CLOCK].names[song.g[G_CLOCK]], "TRS"));
    printf(bad ? "clk_ble_plain: FAILED\n" : "clk_ble_plain: all checks passed\n");
    return bad != 0;
}
