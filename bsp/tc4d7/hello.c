/*
 * Hosted demo for the TC4D7, exercising the full C runtime built by the
 * open-tricore toolchain. printf with formatting, malloc and free, recursion
 * (which needs the context save area), and ordinary function calls.
 *
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#include <stdio.h>
#include <stdlib.h>

static int fib(int n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); }

int main(void)
{
    printf("Hello from TC4D7, built by open-tricore.\n");

    int *a = malloc(10 * sizeof(int));
    for (int i = 0; i < 10; i++) a[i] = i * i;
    int s = 0; for (int i = 0; i < 10; i++) s += a[i];
    printf("sum of squares 0..9 = %d (expect 285)\n", s);
    free(a);

    printf("fib(10) = %d (expect 55)\n", fib(10));
    printf("printf, malloc, and recursion all work.\n");
    return 0;
}
