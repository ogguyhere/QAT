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

# Exercise 3 — Callee-saved registers across a nested call

![[Pasted image 20260720175940.png]]

-> lsl = logical shift left 
-> why it is doing push{fp,lr} and pop {fp,lr} in main is bcz it is non leaf function wen it will branch to the caller lr will change to save it we are pushing it on stack right- 
-> cool tech of logical shift right and one add to calculate x mul 3. 
-> fp is a fixed reference point at the boundary between "my own local storage" (below it) and "stuff above me, saved fp and caller's stacked args" (above it), and it stays put for the whole function so every offset from it is constant.  

# Exercise 4 — 64-bit values and register pairs
![[Pasted image 20260720202854.png]]

1. `a` → r0 (plain 32-bit arg, first slot).
2. `b` (long long, 64-bit) → r2:r3 pair via LDRD, r1 skipped and left dead because AAPCS forces 64-bit args to start on an even register, and r0 was already taken so it had to jump to r2.
3. `c` → no register left, computed then spilled to stack at `[sp,#0]`, direct proof of the alignment-gap rule from the guide.
# Exercise 5 — Pointer walking / memcpy- style code 
```
[kaysaurus@archlinux arm-exercises]$ arm-none-linux-gnueabihf-objdump -d copy_arm02 | sed -n '278,310p'
000103d8 <my_copy>:
   103d8:	e3520000 	cmp	r2, #0
   103dc:	d12fff1e 	bxle	lr
   103e0:	e2400004 	sub	r0, r0, #4
   103e4:	e3a03000 	mov	r3, #0
   103e8:	e491c004 	ldr	ip, [r1], #4
   103ec:	e2833001 	add	r3, r3, #1
   103f0:	e1520003 	cmp	r2, r3
   103f4:	e5a0c004 	str	ip, [r0, #4]!
   103f8:	1afffffa 	bne	103e8 <my_copy+0x10>
   103fc:	e12fff1e 	bx	lr

00010400 <call_fini>:
   10400:	b538      	push	{r3, r4, r5, lr}
   10402:	4d08      	ldr	r5, [pc, #32]	@ (10424 <call_fini+0x24>)
   10404:	4c08      	ldr	r4, [pc, #32]	@ (10428 <call_fini+0x28>)
   10406:	447d      	add	r5, pc
   10408:	447c      	add	r4, pc
   1040a:	1b2c      	subs	r4, r5, r4
   1040c:	10a4      	asrs	r4, r4, #2
   1040e:	d004      	beq.n	1041a <call_fini+0x1a>
   10410:	f855 3d04 	ldr.w	r3, [r5, #-4]!
   10414:	4798      	blx	r3
   10416:	3c01      	subs	r4, #1
   10418:	d1fa      	bne.n	10410 <call_fini+0x10>
   1041a:	e8bd 4038 	ldmia.w	sp!, {r3, r4, r5, lr}
   1041e:	f034 bfdb 	b.w	453d8 <___fini_from_thumb>
   10422:	bf00      	nop
   10424:	0004d9ce 	.word	0x0004d9ce
   10428:	0004d9c8 	.word	0x0004d9c8

0001042c <__libc_start_call_main>:
   1042c:	b500      	push	{lr}
[kaysaurus@archlinux arm-exercises]$ 
```

The crux: at `-O2`, the compiler replaces explicit pointer-increment arithmetic with auto-indexing addressing modes built into the load/store instructions themselves, `LDR ip, [r1], #4` (post-indexed, read then bump) and `STR ip, [r0, #4]!` (pre-indexed, bump then write), so one instruction does both the memory access and the pointer advance. This is the exact shape of a buffer-copy/memory-dump loop, and it's what you should instantly recognize in firehose code that's reading or writing memory, not a separate `add` next to every `LDR`/`STR`, but the increment folded directly into the addressing mode.

# Exercise 6 — Conditional execution / IT blocks (Thumb-2 specific)

```
000103d4 <abs_diff>:
   103d4:	4288      	cmp	r0, r1
   103d6:	bfcc      	ite	gt
   103d8:	1a40      	subgt	r0, r0, r1
   103da:	1a08      	suble	r0, r1, r0
   103dc:	4770      	bx	lr
   103de:	bf00      	nop
```

# Exercise 7 — Live debugging under QEMU + gdb-multiarch

```
 qemu-arm -g 1234 ./hello_arm &
gdb ./hello_arm
[1] 25886
GNU gdb (GDB) 17.2
Copyright (C) 2025 Free Software Foundation, Inc.
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>
This is free software: you are free to change and redistribute it.
There is NO WARRANTY, to the extent permitted by law.
Type "show copying" and "show warranty" for details.
This GDB was configured as "x86_64-pc-linux-gnu".
Type "show configuration" for configuration details.
For bug reporting instructions, please see:
<https://www.gnu.org/software/gdb/bugs/>.
Find the GDB manual and other documentation resources online at:
    <http://www.gnu.org/software/gdb/documentation/>.

For help, type "help".
Type "apropos word" to search for commands related to "word"...
Reading symbols from ./hello_arm...
(gdb) target remote localhost:!234
❌️ localhost:!234: cannot resolve name: Servname not supported for ai_socktype

(gdb) target remote localhost:1234
Remote debugging using localhost:1234

This GDB supports auto-downloading debuginfo from the following URLs:
  <https://debuginfod.archlinux.org>
Enable debuginfod for this session? (y or [n]) y
Debuginfod has been enabled.
To make this setting permanent, add 'set debuginfod enabled on' to .gdbinit.
_start () at ../sysdeps/arm/start.S:79
⚠️ warning: 79	../sysdeps/arm/start.S: No such file or directory
(gdb) break main
Breakpoint 1 at 0x103d8
(gdb) continue
Continuing.

Breakpoint 1, 0x000103d8 in main ()
(gdb) stepi
0x000103dc in main ()
(gdb) stepi
0x000103e0 in main ()
(gdb) stepi
_IO_puts (str=0x4560c "hello arm") at ioputs.c:35
⚠️ warning: 35	ioputs.c: No such file or directory
(gdb) stepi
0x00010cbc	35	in ioputs.c
(gdb) stepi
0x00010cbe	35	in ioputs.c
(gdb) info registers
r0             0x4560c             284172
r1             0x408006f4          1082132212
r2             0x408006fc          1082132220
r3             0x103d0             66512
r4             0x2                 2
r5             0x5ddc8             384456
r6             0x1                 1
r7             0x4560c             284172
r8             0x408006fc          1082132220
r9             0x0                 0
r10            0x2                 2
r11            0x4080059c          1082131868
r12            0x40800618          1082131992
sp             0x40800580          0x40800580
lr             0x103e4             66532
pc             0x10cbe             0x10cbe <_IO_puts+6>
cpsr           0x400f0030          1074724912
fpscr          0x0                 0
fpsid          0x41034070          1090732144
fpexc          0x40000000          1073741824
ID_MMFR1       0x40000000          1073741824
ACTLR_EL2      0x0                 0
ID_MMFR2       0x1260000           19267584
--Type <RET> for more, q to quit, c to continue without paging--q
❌️ Quit
(gdb) stepi
0x00010cc0	35	in ioputs.c
(gdb) x/5i $pc
=> 0x10cc0 <_IO_puts+8>:	sub	sp, #8
   0x10cc2 <_IO_puts+10>:	bl	0x19680 <strlen>
   0x10cc6 <_IO_puts+14>:	
    ldr	r3, [pc, #368]	@ (0x10e38 <_IO_puts+384>)
   0x10cc8 <_IO_puts+16>:	add	r6, pc
   0x10cca <_IO_puts+18>:	mov	r4, r0
(gdb) continue
Continuing.
hello arm
[Inferior 1 (process 25886) exited normally]
(gdb) exit
[1]+  Done                       qemu-arm -g 1234 ./hello_arm
[kaysaurus@archlinux arm-exercises]$ 

```


