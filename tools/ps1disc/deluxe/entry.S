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

	.globl dlx_kind_site, dlx_avail_site, dlx_records_site

/* 0x8002C398 (j here; nop): music id -> v0 2 stream / 1 XA, a0 = id < 25. */
dlx_kind_site:
	addiu $sp, $sp, -24
	sw $31, 20($sp)
	jal dlx_kind
	sw $4, 16($sp)
	lw $4, 16($sp)
	lw $31, 20($sp)
	andi $4, $4, 0xffff
	sltiu $4, $4, 25
	jr $31
	addiu $sp, $sp, 24

/* 0x80050C40 (j here; nop): v0 = a1 = listed, v1 = the play style. */
dlx_avail_site:
	addiu $sp, $sp, -24
	sw $31, 20($sp)
	jal dlx_avail
	nop
	lui $3, 0x800c
	lb $3, -0x5cd8($3)
	move $5, $2
	lw $31, 20($sp)
	nop
	jr $31
	addiu $sp, $sp, 24

/* RECORDS overlay 0x800E90BC (jal here instead of the LZ decoder): place
 * the slot s0 points to, then decode (a0, a1) as the game would. */
dlx_records_site:
	addiu $sp, $sp, -32
	sw $31, 28($sp)
	sw $4, 16($sp)
	sw $5, 20($sp)
	jal dlx_records_slot
	move $4, $16
	lw $4, 16($sp)
	lw $5, 20($sp)
	lw $31, 28($sp)
	j 0x8002E6F0
	addiu $sp, $sp, 32

	.globl dlx_load_site

/* 0x80023FC0, the READ_DT loader (a0 = its table-of-contents entry), first
 * two words (j here; nop).  The menus' packs that now hold the extra songs'
 * pictures (music select 201-210 with the results, workout results 181,
 * RECORDS 28-32) reach past where the stock ones end, over memory where the
 * game keeps its dancers between songs (loaded at 0x80160000 / 0x8016A000,
 * decoded at 0x8018C000 / 0x801A8000); it skips their load while
 * [0x800BA335] / [0x800BB599] still name them.  Loading one of those packs
 * forgets both, so they load again. */
dlx_load_site:
	lui $24, 0x8009
	ori $24, $24, 0x2b50
	subu $24, $4, $24
	srl $24, $24, 3			/* the entry; huge below the table */
	addiu $25, $24, -28
	sltiu $25, $25, 5
	bnez $25, 1f
	addiu $25, $24, -181
	beqz $25, 1f
	addiu $25, $24, -201
	sltiu $25, $25, 10
	beqz $25, 2f
	nop
1:	lui $25, 0x800c
	addiu $24, $zero, -1
	sb $24, -0x5ccb($25)
	sb $24, -0x4a67($25)
2:	lui $2, 0x8009			/* the loader's own first two words */
	j 0x80023FC8
	lw $2, 0x1dac($2)
