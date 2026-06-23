; fastio.s, a PPU (ARC EV71) compute kernel that demonstrates the fast
; shared-memory result path. It reads a 16-word input vector the TriCore placed
; in the shared LMU, doubles each element, and writes the 16-word result back to
; the shared LMU, where the TriCore reads it directly, no run-state handshake.
;
; This works because the host first grants the ARC's master tag write access to
; the LMU region through the LMU access-protection unit (see fastio_demo.c). Out
; of reset the LMU APU lets every master read but only the CPU write, which is
; why an ungranted ARC store is silently dropped (it is an access-protection
; drop, not a cache effect).
;
; Address-vectored reset image, linked at the PPU code base 0xB0000000. Build:
;   arc-linux-gnu-as -mcpu=archs fastio.s -o fastio.o
;   arc-linux-gnu-ld -Ttext=0xB0000000 -e _start fastio.o -o fastio.elf
;   arc-linux-gnu-objcopy -O binary fastio.elf fastio.bin
;
; Copyright 2026 Syuma Labs. Apache-2.0.

	.text
	.global _start
_start:
	.long entry              ; vector[0] = entry address (ARCv2 address-vectored reset)
entry:
	mov   r4, 0xB0400100     ; input vector base (shared LMU, TriCore-written)
	mov   r5, 0xB0400200     ; output vector base (shared LMU, TriCore reads it)
	mov   r6, 16             ; element count
.Lloop:
	ld    r0, [r4]
	add   r0, r0, r0         ; out = in * 2
	st    r0, [r5]
	add   r4, r4, 4
	add   r5, r5, 4
	sub.f r6, r6, 1
	bne   .Lloop
	sleep                    ; done, run state goes to sleep so the host can tell
0:	b 0b
