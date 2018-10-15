#define ALIGN .align
.intel_syntax noprefix
.section .text
.global cnv1_mainloop_sandybridge_asm
.global cnv2_mainloop_ivybridge_asm
.global cnv2_mainloop_ryzen_asm
.global cnv2_double_mainloop_sandybridge_asm

.global cnv1_mainloop_soft_aes_sandybridge_asm
.global cnv2_mainloop_soft_aes_sandybridge_asm

ALIGN 64
cnv1_mainloop_sandybridge_asm:
	sub rsp, 48
	mov rcx, rdi
	#include "cnv1_mainloop_sandybridge.inc"
	add rsp, 48
	ret 0

ALIGN 64
cnv2_mainloop_ivybridge_asm:
	sub rsp, 48
	mov rcx, rdi
	#include "cnv2_main_loop_ivybridge.inc"
	add rsp, 48
	ret 0

ALIGN 64
cnv2_mainloop_ryzen_asm:
	sub rsp, 48
	mov rcx, rdi
	#include "cnv2_main_loop_ryzen.inc"
	add rsp, 48
	ret 0

ALIGN 64
cnv2_double_mainloop_sandybridge_asm:
	sub rsp, 48
	mov rcx, rdi
	mov rdx, rsi
	#include "cnv2_double_main_loop_sandybridge.inc"
	add rsp, 48
	ret 0

ALIGN 64
cnv1_mainloop_soft_aes_sandybridge_asm:
	sub rsp, 48
	mov rcx, rdi
	#include "cnv1_mainloop_soft_aes_sandybridge.inc"
	add rsp, 48
	ret 0

ALIGN 64
cnv2_mainloop_soft_aes_sandybridge_asm:
	sub rsp, 48
	mov rcx, rdi
	#include "cnv2_mainloop_soft_aes_sandybridge.inc"
	add rsp, 48
	ret 0
