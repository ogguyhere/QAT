# Exercise 1 — Hello world,

prologue/epilogue basics

![[Pasted image 20260717042608.png]]
### Thumb
![[Pasted image 20260717042630.png]]

- Differences:
	1. prologue: arm pushed fp and lr altho thumb pushed r7 and lr
	2. later on obv they used their respective pushed regs. 
	3. and epilogue is also diff likewise the epilogue
	4. the initial address is same but later on in the second column is diff

![[Pasted image 20260718032701.png]]

`_IO_puts` is at `0x00010cb9`. Low bit is `1` (odd address). That's your answer, confirmed with your own data, not taken on faith.

That low bit is the Thumb marker exactly like section 1 of the guide described — `_IO_puts` is compiled Thumb inside this statically-linked glibc, even though your `main` in `hello_arm.dis` is ARM mode. So when ARM-mode `main` calls it, it can't use plain `bl` (that stays in the same mode), it has to use `blx` to flip state into Thumb on entry to `puts`. Exactly what you saw in the disassembly.

# Exercise 2 — Argument passing
```
000103d0 <combine>:
   103d0:	e52db004 	push	{fp}		@ (str fp, [sp, #-4]!)
   103d4:	e28db000 	add	fp, sp, #0
   103d8:	e24dd014 	sub	sp, sp, #20
   103dc:	e50b0008 	str	r0, [fp, #-8]
   103e0:	e50b100c 	str	r1, [fp, #-12]
   103e4:	e50b2010 	str	r2, [fp, #-16]
   103e8:	e50b3014 	str	r3, [fp, #-20]	@ 0xffffffec
   103ec:	e51b300c 	ldr	r3, [fp, #-12]
   103f0:	e1a02083 	lsl	r2, r3, #1
   103f4:	e51b3008 	ldr	r3, [fp, #-8]
   103f8:	e0821003 	add	r1, r2, r3
   103fc:	e51b2010 	ldr	r2, [fp, #-16]
   10400:	e1a03002 	mov	r3, r2
   10404:	e1a03083 	lsl	r3, r3, #1
   10408:	e0833002 	add	r3, r3, r2
   1040c:	e0812003 	add	r2, r1, r3
   10410:	e51b3014 	ldr	r3, [fp, #-20]	@ 0xffffffec
   10414:	e1a03103 	lsl	r3, r3, #2
   10418:	e0821003 	add	r1, r2, r3
   1041c:	e59b2004 	ldr	r2, [fp, #4]
   10420:	e1a03002 	mov	r3, r2
   10424:	e1a03103 	lsl	r3, r3, #2
   10428:	e0833002 	add	r3, r3, r2
   1042c:	e0812003 	add	r2, r1, r3
   10430:	e59b3008 	ldr	r3, [fp, #8]
   10434:	e3a01006 	mov	r1, #6
   10438:	e0030391 	mul	r3, r1, r3
   1043c:	e0823003 	add	r3, r2, r3
   10440:	e1a00003 	mov	r0, r3
   10444:	e28bd000 	add	sp, fp, #0
   10448:	e49db004 	pop	{fp}		@ (ldr fp, [sp], #4)
   1044c:	e12fff1e 	bx	lr

00010450 <main>:
   10450:	e92d4800 	push	{fp, lr}
   10454:	e28db004 	add	fp, sp, #4
   10458:	e24dd008 	sub	sp, sp, #8
   1045c:	e3a03006 	mov	r3, #6
   10460:	e58d3004 	str	r3, [sp, #4]
   10464:	e3a03005 	mov	r3, #5
   10468:	e58d3000 	str	r3, [sp]
   1046c:	e3a03004 	mov	r3, #4
   10470:	e3a02003 	mov	r2, #3
   10474:	e3a01002 	mov	r1, #2
   10478:	e3a00001 	mov	r0, #1
   1047c:	ebffffd3 	bl	103d0 <combine>
   10480:	e1a03000 	mov	r3, r0
   10484:	e1a00003 	mov	r0, r3
   10488:	e24bd004 	sub	sp, fp, #4
   1048c:	e8bd8800 	pop	{fp, pc}
```

Args 1-4 go into r0-r3 in main, args 5-6 get str'd onto the stack (sp,#4 and sp,#0) before the bl, then in combine it pushes fp on stack, makes space by doing sub sp, and reads args 5-6 back at fp+4 and fp+8 (not fp+0 since combine only pushed fp not lr, being a leaf function with no calls inside, so no ret address on stack, lr stayed live in the register the whole time); bp/fp is the base pointer, a fixed anchor for addressing locals and args, while lr holds the return address that bl set when main called combine.

