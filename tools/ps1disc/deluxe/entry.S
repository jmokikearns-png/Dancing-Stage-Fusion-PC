/* entry.S -- trampolines from the patched game code into payload.c.
 * Each site's original instructions run here, in their order. */
	.set noreorder
	.set noat
	.text
	.globl dlx_note_site, dlx_build1_site, dlx_build2_site, dlx_step_mark_site

/* Every register a C call may change, and hi / lo. */
	.macro SAVE
	addiu $sp, $sp, -88
	sw $1, 16($sp);  sw $2, 20($sp);  sw $3, 24($sp)
	sw $4, 28($sp);  sw $5, 32($sp);  sw $6, 36($sp);  sw $7, 40($sp)
	sw $8, 44($sp);  sw $9, 48($sp);  sw $10, 52($sp); sw $11, 56($sp)
	sw $12, 60($sp); sw $13, 64($sp); sw $14, 68($sp); sw $15, 72($sp)
	sw $24, 76($sp); sw $25, 80($sp); sw $31, 84($sp)
	mfhi $8
	mflo $9
	sw $8, 0($sp)
	sw $9, 4($sp)
	.endm
	.macro RESTORE
	lw $8, 0($sp)
	lw $9, 4($sp)
	nop
	mthi $8
	mtlo $9
	lw $1, 16($sp);  lw $2, 20($sp);  lw $3, 24($sp)
	lw $4, 28($sp);  lw $5, 32($sp);  lw $6, 36($sp);  lw $7, 40($sp)
	lw $8, 44($sp);  lw $9, 48($sp);  lw $10, 52($sp); lw $11, 56($sp)
	lw $12, 60($sp); lw $13, 64($sp); lw $14, 68($sp); lw $15, 72($sp)
	lw $24, 76($sp); lw $25, 80($sp); lw $31, 84($sp)
	addiu $sp, $sp, 88
	.endm

/* 0x800519F0: j here, delay slot nop (was lui t3, 0x800c; addiu t3, t3, -0x5cf0). */
dlx_note_site:
	SAVE
	jal dlx_note_palette
	addiu $4, $sp, 88
	RESTORE
	lui $11, 0x800c
	j 0x800519F8
	addiu $11, $11, -0x5cf0

/* 0x8005517C: was j 0x80055470 (its delay slot runs before we get here). */
dlx_build1_site:
	SAVE
	jal dlx_build1
	nop
	RESTORE
	j 0x80055470
	nop

/* 0x80055A7C: was j 0x80055c8c. */
dlx_build2_site:
	SAVE
	jal dlx_build2
	nop
	RESTORE
	j 0x80055C8C
	nop

/* 0x800EA168 (options overlay): jal here instead of the string lookup;
 * s0 is the value shown. */
dlx_step_mark_site:
	j dlx_step_mark_text
	move $5, $16
