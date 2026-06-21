/*
 * led-demo drives the TC4D7 Lite Kit user LEDs from Linux over TAS and
 * verifies every transition by reading the pin level back from the chip.
 *
 * LED1 is P03.9 and LED2 is P03.10. Both are low-active, so driving a pin
 * LOW turns its LED ON. Register map comes from the official iLLD TC4Dx
 * headers (Infineon/illd_release_tc4x).
 *
 * Copyright 2026 Syuma Labs.
 * Licensed under the Apache License, Version 2.0. See LICENSE.
 */

#include "tas_client_rw.h"

#include <cstdio>
#include <cstdint>
#include <thread>
#include <chrono>

// TC4Dx Port P03 registers (absolute addresses).
static constexpr uint32_t P03_IN              = 0xF003AC24; // pin level read-back, P9=bit9 P10=bit10
static constexpr uint32_t P03_OMR             = 0xF003AC3C; // PS9=b9 PS10=b10 drive high, PCL9=b25 PCL10=b26 drive low
static constexpr uint32_t P03_PADCFG9_DRVCFG  = 0xF003AF94; // DIR=bit0 (1=out), OD=bit1 (0=push-pull)
static constexpr uint32_t P03_PADCFG10_DRVCFG = 0xF003AFA4;

static constexpr uint32_t OMR_PS9   = 1u << 9;   // drive P03.9 high, LED1 OFF
static constexpr uint32_t OMR_PCL9  = 1u << 25;  // drive P03.9 low, LED1 ON
static constexpr uint32_t OMR_PS10  = 1u << 10;  // drive P03.10 high, LED2 OFF
static constexpr uint32_t OMR_PCL10 = 1u << 26;  // drive P03.10 low, LED2 ON

static constexpr uint32_t DRVCFG_OUTPUT_PUSHPULL = 0x1; // DIR=1, OD=0, MODE=0

static CTasClientRw* g = nullptr;

static uint32_t rd(uint32_t a) {
    uint32_t v = 0;
    if (g->read32(a, &v) != TAS_ERR_NONE) printf("  [READ  0x%08X FAILED %s]\n", a, g->get_error_info());
    return v;
}
static void wr(uint32_t a, uint32_t v) {
    if (g->write32(a, v) != TAS_ERR_NONE) printf("  [WRITE 0x%08X=0x%08X FAILED %s]\n", a, v, g->get_error_info());
}
static void sleep_ms(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

// Report both LEDs' actual electrical level straight from the chip's P03_IN register.
static void report() {
    uint32_t in = rd(P03_IN);
    int l1 = (in >> 9)  & 1;   // P03.9
    int l2 = (in >> 10) & 1;   // P03.10
    printf("    P03_IN=0x%08X  P9=%d (LED1 %s)  P10=%d (LED2 %s)\n",
           in, l1, l1 ? "OFF" : "ON ", l2, l2 ? "OFF" : "ON ");
}

int main() {
    CTasClientRw client("LedDemo");
    g = &client;

    if (client.server_connect("localhost") != TAS_ERR_NONE) {
        printf("Failed to connect to tas_server, %s\n", client.get_error_info());
        return 1;
    }

    const tas_target_info_st* targets = nullptr;
    uint32_t numTargets = 0;
    if (client.get_targets(&targets, &numTargets) != TAS_ERR_NONE || numTargets == 0) {
        printf("No targets found, %s\n", client.get_error_info());
        return 1;
    }
    printf("Target %s\n", targets[0].identifier);

    if (client.session_start(targets[0].identifier, "LedSession") != TAS_ERR_NONE) {
        printf("session_start failed, %s\n", client.get_error_info());
        return 1;
    }
    // Reset and halt the cores so no flash firmware fights us for the GPIO.
    if (client.device_connect(TAS_CLNT_DCO_RESET_AND_HALT) != TAS_ERR_NONE) {
        printf("reset and halt failed, %s\n", client.get_error_info());
        return 1;
    }
    printf("Cores reset and halted, we own the GPIO.\n\n");

    printf("Baseline before config\n");
    printf("    DRVCFG9=0x%08X  DRVCFG10=0x%08X\n", rd(P03_PADCFG9_DRVCFG), rd(P03_PADCFG10_DRVCFG));
    report();

    printf("\nConfiguring P03.9 and P03.10 as push-pull outputs\n");
    wr(P03_PADCFG9_DRVCFG,  DRVCFG_OUTPUT_PUSHPULL);
    wr(P03_PADCFG10_DRVCFG, DRVCFG_OUTPUT_PUSHPULL);
    uint32_t d9  = rd(P03_PADCFG9_DRVCFG);
    uint32_t d10 = rd(P03_PADCFG10_DRVCFG);
    printf("    DRVCFG9=0x%08X (DIR=%d)  DRVCFG10=0x%08X (DIR=%d)\n", d9, d9 & 1, d10, d10 & 1);
    if ((d9 & 1) == 0 || (d10 & 1) == 0) {
        printf("    DIR bit did not stick, writes blocked (access protection?). Aborting blink.\n");
        return 2;
    }
    printf("    Both pins now outputs, DIR=1 read back from silicon.\n");

    wr(P03_OMR, OMR_PS9 | OMR_PS10);   // both OFF
    sleep_ms(300);
    printf("\nBoth LEDs OFF\n"); report();

    printf("\nAlternating blink x6, each step verified via P03_IN read-back\n");
    for (int i = 0; i < 6; ++i) {
        if (i & 1) { wr(P03_OMR, OMR_PCL9 | OMR_PS10);  printf("  step %d, LED1 ON  LED2 OFF\n", i); }
        else       { wr(P03_OMR, OMR_PS9  | OMR_PCL10); printf("  step %d, LED1 OFF LED2 ON\n", i); }
        sleep_ms(500);
        report();
    }

    wr(P03_OMR, OMR_PCL9 | OMR_PS10);  // final, LED1 ON LED2 OFF
    sleep_ms(200);
    printf("\nFinal state set, LED1 ON LED2 OFF\n");
    report();

    printf("\nDone.\n");
    return 0;
}
