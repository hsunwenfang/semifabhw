	.section	__TEXT,__text,regular,pure_instructions
	.build_version macos, 26, 0	sdk_version 26, 2
	.globl	__Z13read_volatilev             ; -- Begin function _Z13read_volatilev
	.p2align	2
__Z13read_volatilev:                    ; @_Z13read_volatilev
	.cfi_startproc
; %bb.0:
	sub	sp, sp, #16
	.cfi_def_cfa_offset 16
	mov	w8, #32                         ; =0x20
	str	w8, [sp, #12]
	ldr	w8, [sp, #12]
	ldr	w8, [sp, #12]
	ldr	w8, [sp, #12]
	ldr	w8, [sp, #12]
	ldr	w8, [sp, #12]
	ldr	w8, [sp, #12]
	ldr	w8, [sp, #12]
	ldr	w8, [sp, #12]
	ldr	w8, [sp, #12]
	ldr	w8, [sp, #12]
	add	sp, sp, #16
	ret
	.cfi_endproc
                                        ; -- End function
	.globl	_main                           ; -- Begin function main
	.p2align	2
_main:                                  ; @main
	.cfi_startproc
; %bb.0:
	sub	sp, sp, #16
	.cfi_def_cfa_offset 16
	mov	w8, #32                         ; =0x20
	str	w8, [sp, #12]
	ldr	w8, [sp, #12]
	ldr	w8, [sp, #12]
	ldr	w8, [sp, #12]
	ldr	w8, [sp, #12]
	ldr	w8, [sp, #12]
	ldr	w8, [sp, #12]
	ldr	w8, [sp, #12]
	ldr	w8, [sp, #12]
	ldr	w8, [sp, #12]
	ldr	w8, [sp, #12]
	mov	w0, #0                          ; =0x0
	add	sp, sp, #16
	ret
	.cfi_endproc
                                        ; -- End function
.subsections_via_symbols
