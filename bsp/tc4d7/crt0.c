/*
 * Minimal TC4x C runtime startup. Sets the stack and small-data base
 * registers, builds the Context Save Area free list, initializes data and bss,
 * then calls main. The CSA list build follows the iLLD initCSA algorithm and
 * must run before any function call, so it is inlined here in _cstart, which is
 * entered by a jump, not a call.
 *
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

extern unsigned int __stack_top[], __csa_begin[], __csa_end[];
extern unsigned int __data_load[], __data_start[], __data_end[];
extern unsigned int __bss_start[], __bss_end[];
extern unsigned int _SMALL_DATA_[], _SMALL_DATA2_[];
extern int main(void);

/* _start lives in crt0.S and jumps here after setting A10, A0, A1. */

static inline void mtcr(unsigned int id, unsigned int v) {
    __asm__ volatile("mtcr %0,%1\n\tisync" :: "i"(id), "d"(v) : "memory");
}

#define CPU_FCX 0xFE38u
#define CPU_LCX 0xFE3Cu

__attribute__((used, section(".text._cstart")))
void _cstart(void)
{
    /* Build the CSA free list, inline, no calls before this completes. */
    unsigned int *b = __csa_begin, *e = __csa_end;
    unsigned int num = ((unsigned int)e - (unsigned int)b) / 64u;
    unsigned int *prv = 0, *nxt = b, lv = 0;
    for (unsigned int k = 0; k < num; k++) {
        lv = (((unsigned int)nxt & (0xFu << 28)) >> 12) |
             (((unsigned int)nxt & (0xFFFFu << 6)) >> 6);
        if (k == 0) mtcr(CPU_FCX, lv); else *prv = lv;
        if (k == num - 3) mtcr(CPU_LCX, lv);
        prv = nxt; nxt += 16; /* 16 words = 64 bytes per CSA */
    }
    *prv = 0;
    __asm__ volatile("dsync\n\tisync" ::: "memory");

    /* Copy initialized data from its load image, then clear bss. */
    unsigned int *s = __data_load, *d = __data_start;
    while (d < __data_end) *d++ = *s++;
    unsigned int *z = __bss_start;
    while (z < __bss_end) *z++ = 0;

    main();
    for (;;) { }
}
