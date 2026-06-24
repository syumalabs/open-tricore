/*
 * I2C demo and self-test. Brings up I2C0 as a master on the TC4D7 Lite Kit's
 * onboard bus and probes the board's 24AA02E48 EEPROM at 0x50: the EEPROM
 * acknowledges (a real device on real SCL/SDA), and an unused address NACKs.
 * Both outcomes together prove the master end to end with no external wiring.
 *
 *   tricore-elf-gcc -mtc18 -O2 -nostartfiles -T bsp/tc4d7/hosted.ld \
 *     bsp/tc4d7/crt0.S bsp/tc4d7/crt0.c bsp/tc4d7/clock.c bsp/tc4d7/i2c.c \
 *     bsp/tc4d7/i2c_demo.c -I bsp/tc4d7 -o i2c_demo.elf
 *   tricore-elf-objcopy -O binary i2c_demo.elf i2c_demo.bin
 *   tc-load run i2c_demo.bin 0x70100000      # heartbeat = 0x12C0AC on pass
 *
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#include "clock.h"
#include "i2c.h"

#define R(a) (*(volatile unsigned int *)(a))

#define EEPROM_ADDR  0x50u   /* onboard 24AA02E48 */
#define ABSENT_ADDR  0x7Eu   /* nothing here -> must NACK */

int main(void)
{
    clock_init_pll();
    clock_enable_i2c(8);     /* fI2C = fsource2 / 8 */
    i2c_init();

    int eeprom = i2c_probe(EEPROM_ADDR);   /* expect 0 (ACK) */
    int absent = i2c_probe(ABSENT_ADDR);   /* expect 1 (NACK) */

    /* data-path check: write only the EEPROM word-address pointer (no data cell
       is altered, so this is non-destructive) and confirm both bytes ACK. */
    uint8_t word_addr = 0x00;
    int wr = i2c_write(EEPROM_ADDR, &word_addr, 1);   /* expect 0 (ACK) */

    unsigned ok = (eeprom == 0) && (absent == 1) && (wr == 0);

    R(0x70000004u) = (unsigned)(eeprom & 0xFF) | ((unsigned)(absent & 0xFF) << 8)
                     | ((unsigned)(wr & 0xFF) << 16); /* got: eeprom|absent<<8|wr<<16 (want 0x000100) */
    R(0x7000000Cu) = 0xEEu;                  /* marker */
    for (;;)
        R(0x70000000u) = ok ? 0x12C0ACu : 0xBADu;  /* heartbeat = 0x12C0AC on pass */
}
