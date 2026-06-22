/*
 * TC4x reset entry. Sets the stack pointer A10 and the small-data base
 * registers A0 and A1, then jumps to the C startup _cstart. Pure assembly so
 * no prologue runs before the stack exists.
 *
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

	.section .text._start,"ax",@progbits
	.global _start
	.type _start,@function
_start:
	movh.a	%a10, hi:__stack_top
	lea	%a10, [%a10]lo:__stack_top
	movh.a	%a0,  hi:_SMALL_DATA_
	lea	%a0,  [%a0]lo:_SMALL_DATA_
	movh.a	%a1,  hi:_SMALL_DATA2_
	lea	%a1,  [%a1]lo:_SMALL_DATA2_
	dsync
	isync
	j	_cstart
	.size _start,.-_start
