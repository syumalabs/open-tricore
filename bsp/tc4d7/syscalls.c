/*
 * Newlib syscall stubs for the TC4D7 BSP. printf output is captured into a
 * fixed RAM buffer that the host reads back over the debugger, so no UART is
 * needed yet. malloc is backed by a heap region in DSPR0.
 *
 * Output buffer at 0x7003A000:  word 0 is the byte count, bytes follow at +4.
 *
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#include <sys/stat.h>
#include <sys/types.h>

/* _sbrk is provided by the toolchain libos, backed by the __HEAP symbols
   defined in hosted.ld. */

extern void uart_putc(char c);

/* Route stdout/stderr to the ASCLIN0 UART. Define both write and _write so
   these override the libgloss virtio semihosting versions. */
int write(int fd, const char *buf, int len) {
    (void)fd;
    for (int i = 0; i < len; i++) {
        if (buf[i] == '\n') uart_putc('\r');
        uart_putc(buf[i]);
    }
    return len;
}
int _write(int fd, const char *buf, int len) { return write(fd, buf, len); }

int   _read(int fd, char *buf, int len) { (void)fd;(void)buf;(void)len; return 0; }
int   _close(int fd) { (void)fd; return -1; }
off_t _lseek(int fd, off_t off, int dir) { (void)fd;(void)off;(void)dir; return 0; }
int   _isatty(int fd) { (void)fd; return 1; }
int   _fstat(int fd, struct stat *st) { (void)fd; st->st_mode = S_IFCHR; return 0; }
void  _exit(int code) { (void)code; for (;;) { } }
int   _kill(int pid, int sig) { (void)pid;(void)sig; return -1; }
int   _getpid(void) { return 1; }
