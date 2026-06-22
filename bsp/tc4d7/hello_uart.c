/*
 * Hosted demo with printf going out the real UART. Exercises the full C
 * runtime built by open-tricore, printf with formatting, malloc, and
 * recursion, all printed over ASCLIN0 to the serial console. Loops so the
 * host can read it at any time at 115200.
 *
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#include <stdio.h>
#include <stdlib.h>

extern void uart_init(void);

static int fib(int n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); }

int main(void)
{
    uart_init();

    int *a = malloc(8 * sizeof(int));
    for (int i = 0; i < 8; i++) a[i] = i * i;
    int s = 0; for (int i = 0; i < 8; i++) s += a[i];
    free(a);

    unsigned int n = 0;
    for (;;) {
        printf("Hello over UART from TC4D7, built by open-tricore. iter %u\n", n++);
        printf("  sum of squares 0..7 = %d (expect 140), fib(12) = %d (expect 144)\n", s, fib(12));
        for (volatile int d = 0; d < 3000000; d++) { } /* pace the output */
    }
}
