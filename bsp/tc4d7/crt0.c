/*
 * Minimal TC4x C runtime startup. Disables the watchdogs, sets the stack and
 * small-data base registers, builds the Context Save Area free list,
 * initializes data and bss, then calls main. The CSA list build follows the
 * iLLD initCSA algorithm and must run before any function call, so it is
 * inlined here in _cstart, which is entered by a jump, not a call.
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

/* WTU watchdog control registers. The CPU0 and system watchdogs are live out
   of reset with a ~10ms timeout and reset us before main if not handled. */
#define WDTCPU0_CTRLA ((volatile unsigned int *)0xF000003Cu)
#define WDTCPU0_CTRLB ((volatile unsigned int *)0xF0000040u)
#define WDTSYS_CTRLA  ((volatile unsigned int *)0xF00001A8u)
#define WDTSYS_CTRLB  ((volatile unsigned int *)0xF00001ACu)

/* Disable one watchdog. Password is CTRLA.PW (bits 15:1) XOR 0x7F, per the iLLD
   SSW. Unlock by writing CTRLA with LCK=0 and the password, set CTRLB.DR, then
   re-lock. Always inlined so there is no call before the CSA exists. */
static inline __attribute__((always_inline))
void wdt_off(volatile unsigned int *ctrla, volatile unsigned int *ctrlb)
{
    unsigned int a = *ctrla;
    unsigned int pw = ((a >> 1) & 0x7FFFu) ^ 0x007Fu;
    a = (a & ~0x1u & ~(0x7FFFu << 1)) | (pw << 1); /* LCK=0, PW=password */
    *ctrla = a;                                     /* unlock */
    *ctrlb = *ctrlb | 0x1u;                         /* DR=1, disable request */
    *ctrla = a | 0x1u;                              /* LCK=1, re-lock */
}

__attribute__((used, section(".text._cstart")))
void _cstart(void)
{
    /* Silence the watchdogs first, before anything can time out. */
    wdt_off(WDTCPU0_CTRLA, WDTCPU0_CTRLB);
    wdt_off(WDTSYS_CTRLA, WDTSYS_CTRLB);

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
