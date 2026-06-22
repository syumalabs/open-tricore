/*
 * UART demo for the TC4D7. Initializes ASCLIN0 and prints a line forever so
 * the host can read it on /dev/ttyUSB0 and calibrate the baud rate.
 *
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

extern void uart_init(void);
extern void uart_puts(const char *);

int main(void)
{
    uart_init();
    for (;;) {
        uart_puts("open-tricore UART on TC4D7 works\r\n"); /* continuous, no delay */
    }
}
