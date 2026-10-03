# Learning Android Architecture in 3 Days

### A study plan for QAT (Qualcomm Acquisition Tool)

Your phone's operating system spends a lot of effort protecting your data: sandboxing every app, encrypting everything at rest, checking signatures at every step of starting up. All of that protection depends on one thing — the operating system actually being awake and running. There's a mode built directly into the phone's chip that operates before any of that wakes up, before the operating system exists at all. That's the mode your tool talks to. By the end of these three days, you'll understand exactly why that matters, and exactly where it fits.

## How to use this

This is split across three days, and each one builds on the last.

- **Day 1** covers what Android is actually built from, and exactly how it starts up when you turn a phone on.
- **Day 2** covers how Android protects itself once it's running, and why that protection is exactly what makes running memory, not stored files, the valuable target for your tool.
- **Day 3** covers where things are physically stored on the device, then walks through precisely where your tool's approach fits into everything from the first two days.

Nothing here needs a shorthand memorized — every technical term is written out by its real name, every time, on purpose.

---

# Day 1 — What Android is built from, and how it starts up

## The pieces Android is made of

Picture Android as layers stacked on top of each other, each one depending on the layer below it:

```
       Apps you open and use
              ↑
   The application framework
      (the rulebook every app follows)
              ↑
   The Android Runtime
      (runs each app's actual code)
              ↑
   Core system programs
      (storage, screen, networking, and
       the messaging system that connects
       every part of Android to every other part)
              ↑
   The Hardware Abstraction Layer
      (translates between Android and
       this specific phone's chip)
              ↑
   The Linux kernel
      (talks to the hardware directly,
       manages memory and processes)
```

**The Linux kernel** is the foundation. It manages hardware access, processes, memory, and scheduling — this part isn't unique to Android, it's the same kernel that runs on desktop Linux machines and servers, just configured differently. Android adds a few things on top: a messaging system called Binder that lets every part of Android talk to every other part, some shared-memory tools, a program that kills processes when memory runs low, and hooks for a much stricter permission system called Security-Enhanced Linux, covered on Day 2. Android uses a real, full Linux kernel rather than something custom-built, for one big reason: it comes with an entire existing world of hardware drivers and tools already built, and — more importantly for your project — a mature, already-proven way of keeping processes and their memory separated from each other, which the rest of Android's protections are built on top of. **This whole layer doesn't exist while a phone is in Emergency Download Mode.** No kernel is running at that point. That's the entire reason your tool's approach works the way it does — more on this on Day 3.

**The Hardware Abstraction Layer** is the boundary between code written specifically for this phone's chip, and the rest of Android, which doesn't care which chip it's running on. Before Android version 8, this chip-specific code ran as a library loaded directly into whatever program needed it. Since Android 8's "Project Treble," it runs as its own separate program, talking to the rest of the system over Binder, using an interface language originally called the HAL Interface Definition Language — later replaced by the Android Interface Definition Language, the same one apps themselves use, unifying the whole system on one convention. The reason for this separation: it lets Google update the core of Android without needing the phone's manufacturer to update and re-approve all their chip-specific code at the exact same time. This split is also why phones have a separate storage partition just for the manufacturer's chip-specific code.

**Core system programs** are the everyday C and C++ programs running directly on top of the kernel: the very first program that starts (more on this below), a program that manages storage and encryption (this is the one that actually asks for your data's decryption keys when you unlock your phone — directly relevant to Day 2), a program that draws what's on your screen, a program that handles networking, and a directory service that keeps track of every program on the system that can be reached over Binder. This layer exists below the Android Runtime specifically so that performance-critical, hardware-adjacent code can stay in fast, low-overhead C and C++, rather than in the managed, slightly slower environment apps run in.

**The Android Runtime** is what actually executes an app's code. Apps are written in Java or Kotlin, then compiled down into an intermediate format called Dalvik Executable before they can run. Some of that compiling happens once, at install time — called ahead-of-time compilation, done by a tool named `dex2oat` — and the rest happens on the fly while the app is actually running, called just-in-time compilation. Every app you open gets its own separate copy of the Android Runtime, in its own process — new processes are created quickly by duplicating a template process called zygote that's kept ready and waiting (more on this below), rather than starting completely from scratch each time. The point of a dedicated runtime like this, instead of just running raw native code: memory safety, and complete control — Google fully owns this execution environment, rather than trusting arbitrary code straight from an app developer. _If you ever move from just copying raw memory into actually making sense of what's inside a memory copy, this is the layer that matters most — the internal way the Android Runtime lays out objects in memory is exactly what memory-analysis tools rely on to reconstruct live app data from a raw dump._

**The application framework** is the actual set of tools apps are written against — the Activity Manager, the Package Manager, the Window Manager, and others — plus one especially important program, called `system_server`, that hosts all of those core services as one single program, rather than as separate ones. The reason: every one of those services has to check permissions before doing anything, and keeping them together gives Android one single choke point to enforce that consistently. Apps never get to touch a hardware driver directly — they always go through this layer first, over Binder.

## How it starts up

Here's what actually happens, in order, from the moment power is applied — using Qualcomm's real internal stage names, since generic Android write-ups often use slightly different, less precise language:

```
Power is applied
    ↓
The Primary Bootloader runs — permanent code physically
built into the chip, which can never be changed. It checks
the next stage against a stored, unchangeable copy of the
manufacturer's public key before trusting it. This check is
the hardware "root of trust" that everything else in this
whole chain ultimately depends on.
    ↓
    ├── If Emergency Download Mode is triggered here (by a
    │   test point being shorted, a specific command, or a
    │   signature check simply failing) → a small
    │   authentication exchange called the Sahara protocol
    │   runs, using only a tiny amount of memory built into
    │   the chip itself — the phone's main memory chip
    │   hasn't been set up yet at this exact point. This
    │   exchange authenticates and loads a small program,
    │   called a Firehose loader, which then sets up the
    │   main memory chip itself and takes over from there.
    │                                    (Day 3 — you are here)
    ↓
The Extensible Bootloader runs — the current name for what
used to be called the Secondary Bootloader. It sets up the
phone's main memory chip and internal clocks, starts
TrustZone (below), and checks the next stage before handing
off to it.
    ↓
TrustZone, a separate, isolated "secure world," starts
running alongside the rest of the system from this point
forward. This is where the sensitive operations covered
tomorrow actually happen: generating and using encryption
keys, and checking your unlock code. On this chip, this
secure environment is specifically called the Qualcomm
Secure Execution Environment.
    ↓
The Android Bootloader takes over — built on an earlier,
lightweight bootloader codebase called Little Kernel. It
checks the signature of the actual startup image using the
verified-boot system covered tomorrow, sets up the
instructions the kernel will start with, and hands off to
the kernel.
    ↓
The Linux kernel starts, mounts the system's files as its
root filesystem, and launches `init` — the very first
Android-level program, and the one every other program
traces back to.
    ↓
`init` actually runs in two stages. The first, brief pass
loads the stricter permission system's rulebook, then
switches over to the real filesystem and restarts itself.
From there, it reads its full configuration, mounts the
rest of the partitions, starts every core system program in
the right order, and starts zygote.
    ↓
zygote preloads everything a typical app is going to need,
then sits and waits for requests to create new app
processes. For each one, it duplicates itself using a
copy-on-write fork (new pages of memory are only actually
created once something tries to change them, which is what
makes this fast), then narrows the new copy down: it drops
extra privileges, assigns the new process its own unique
user account, applies its security label, and restricts
which low-level operations it's allowed to perform.
    ├──→ `system_server`, hosting the framework services
    │     mentioned above
    └──→ one process for every app you're running
```

Everything from the Extensible Bootloader onward is what Emergency Download Mode lets your tool skip past entirely.

## Before moving on

By the end of today, you should be able to explain, out loud and without notes: what each of the five layers in the stack actually does, why Android bothers separating chip-specific code from the rest of the system, and the order the boot process goes through from power-on to the first app launching. If any of that feels shaky, re-read it before starting Day 2 — everything from here builds on this.

---

# Day 2 — How Android protects itself, and why that matters for your tool

## The security model

Android layers two completely independent forms of access control on top of each other, plus a separate chain of integrity checks at every boot.

**The first layer** is ordinary Unix-style permissions: every app gets assigned its own unique user account the moment it's installed. This is the same basic permission system that's existed on Unix and Linux for decades — the same way two separate user accounts on a shared computer can't see into each other's personal folders, two apps on the same phone can't see into each other's private data, simply because they're different "users" as far as the operating system is concerned.

**The second layer**, stacked on top of the first, is a much stricter permission system called Security-Enhanced Linux. Where ordinary Unix permissions still let a program with high enough privileges (like `root`) do essentially anything, Security-Enhanced Linux constrains every single process, including ones running with those elevated privileges. Every process, file, and network connection gets a security label, and a single central rulebook explicitly lists what's allowed to interact with what. Nothing is allowed by default — anything not explicitly written into that rulebook is denied. Android has fully enforced this system, actually blocking anything not permitted rather than just logging it, since Android version 5. As covered yesterday, it's zygote that assigns each new app process its label, at the exact moment that process is created.

**The integrity chain**, called Android Verified Boot, is a sequence of digital signature checks, each stage verifying the one after it, starting from yesterday's hardware root of trust:

```
The manufacturer's public key, permanently stored in the
chip via a block of fuses called QFPROM (short for
Qualcomm Fuse PROM) — the one piece of this whole chain
that's physically unchangeable
      ↓ checks the signature of
The Extensible Bootloader
      ↓ checks the signature of
The Android Bootloader
      ↓ checks the signature of
The startup image itself, along with a stored map of
checksums covering the rest of the system's files. That
map gets checked continuously, piece by piece, the entire
time the phone is running, not just once at boot.

The possible results of this whole chain:
  Locked, fully verified          → the full, unmodified
                                     manufacturer chain
  Locked, verified with a
    different key                 → the full chain, but
                                     using a key the phone's
                                     owner deliberately added
  Unlocked                        → the chain is
                                     deliberately broken,
                                     and the owner explicitly
                                     agreed to that
  Failed                          → the phone refuses to
                                     start, or clearly warns
                                     that something doesn't
                                     match
```

**Here's why this section matters for your presentation specifically.** Emergency Download Mode doesn't break or bypass Security-Enhanced Linux, the per-app account separation, or Android Verified Boot. It doesn't need to — **none of those things exist yet at the exact point where Emergency Download Mode runs.** There's no Android running, no user account assigned by zygote, no security label, no ongoing integrity check happening, because none of that code has executed yet. The only actual gate that applies to you at that point is a single signature check performed by the Primary Bootloader, during the Sahara protocol's authentication exchange from yesterday — that's the real constraint your tool has to satisfy, not this entire operating-system security stack.

## Encryption — and why memory is the real target

Android has used two different approaches to encrypting a user's data over the years: an older, simpler approach, and the current, more capable one.

||The older approach: Full Disk Encryption|The current approach: File-Based Encryption|
|---|---|---|
|What gets encrypted|The entire user-data partition, under one single key|Individual files, under two different categories of key|
|Before you unlock the phone|Completely inaccessible — the phone can't even receive a call or ring an alarm|Some specific files are still readable (see below)|
|How it actually works|Encryption applied underneath the whole filesystem, as one block|Encryption built directly into the filesystem itself, file by file|
|Where it stands today|Legacy — this is how it worked before Android 7|Required on every phone that launched with Android 10 or later|

That "some files still readable before unlock" feature is called Direct Boot — it's why your alarm still goes off, and you can still receive a phone call, before you've entered your unlock code.

Here's where the actual encryption keys live, and how they're organized:

```
A key unique to this specific chip, permanently burned in —
and it never, ever leaves an isolated, protected environment
inside the chip called the Trusted Execution Environment (on
this phone's chip, that's TrustZone, from yesterday)
      ↓
A program called Keymaster (or its newer replacement,
KeyMint) — a small program of a type called a Trusted
Application, running inside that protected environment. It
generates keys and performs every actual encrypt/decrypt
operation INSIDE the environment — it never hands the
actual key material back out to the regular operating
system.
      ↓
      ├── One category, called the Device Encrypted key,
      │     needs no unlock code at all, just the chip's
      │     own permanent key → protects the specific files
      │     needed before you've unlocked the phone: alarms,
      │     incoming calls, accessibility settings
      │
      └── The other category, called the Credential
            Encrypted key, needs the chip's permanent key
            AND your actual unlock code → protects the bulk
            of your real data, and only becomes available
            after you unlock the phone. Checking that unlock
            code happens through another Trusted Application
            called Gatekeeper, which enforces a
            failed-attempt counter stored in a specially
            protected part of the storage chip called the
            Replay Protected Memory Block — meaning even
            someone holding a raw, complete copy of the
            storage chip's contents can't just reset that
            counter and brute-force the unlock code offline,
            because writing to that protected part of storage
            requires proving you're the legitimate storage
            controller, not just overwriting bytes.
                ↓
          Each individual file's real, working key comes
          from combining whichever of the two keys above
          applies, with a unique number generated
          specifically for that one file.
```

**This is the actual argument for your entire project.** The keys Keymaster or KeyMint generates never leave the Trusted Execution Environment. If you copy the user-data partition off a locked, otherwise-healthy phone, what you get is properly encrypted, unreadable data — and nothing on that phone will hand you the key to read it, short of somehow compromising the Trusted Execution Environment itself, which is a dramatically harder problem than anything Emergency Download Mode gives you access to. But once someone has actually unlocked their phone and it's running normally, the picture completely flips: decrypted file contents, unlocked keys, login sessions, and app secrets all exist as plain, unprotected data sitting in the phone's running memory. That's the real forensic case for physical memory acquisition specifically — it isn't trying to compete with breaking storage encryption, it's going after the one place that protection simply doesn't reach: memory that's actively being used right now.

## Before moving on

By the end of today, you should be able to explain: the two separate layers of app isolation and how they differ, what Android Verified Boot is actually checking at each stage, the real difference between Full Disk Encryption and File-Based Encryption, and — most importantly — why a locked phone's stored data is essentially useless to you, while an unlocked, running phone's memory is not. That last point is the actual thesis of your whole project, so by the time you move on it should feel completely natural to say out loud.

---

# Day 3 — Where things live, and exactly where your tool fits

## Where things are actually stored

Exact naming varies by phone manufacturer and by which generation of chip is involved, but this is the representative layout on most modern Qualcomm-based phones. Each of the following is a distinct, separately-addressable region of the storage chip:

**The bootloader and secure-world chain**, matching yesterday's boot sequence: the Extensible Bootloader and its configuration data, the TrustZone image, a hypervisor on chips that have one, the Android Bootloader, device configuration data, the Keymaster/KeyMint program from yesterday, and a couple of shared libraries used inside the Trusted Execution Environment.

**The radio**: the cellular modem's own firmware — this runs on a completely separate processor, with its own separate operating system, almost entirely isolated from the rest of the phone — plus the modem's own persistent settings.

**Verified-boot bookkeeping**: the signed metadata from yesterday's integrity chain, kept as its own separate region for the main system and for the phone-specific vendor code, plus hardware configuration data.

**The operating system itself**: stored as flexible, resizable regions inside one larger container partition, rather than as fixed-size partitions — this lets modern phones resize these without repartitioning the entire disk. This includes: the kernel and a small startup filesystem, the system partition itself, the phone-specific vendor code from Day 1, and a couple of further partitions used to split the system up more finely on newer phones.

**Everything user-facing**: the actual user-data partition (this is the encrypted one from yesterday, and normally the largest partition on the phone), a small partition holding key-wrapping information needed before user data can even begin being decrypted, a persistent settings area, a small area recording which copy of the system to boot into next (most modern phones keep two full copies of most of the system, so an update can install to the unused copy while the phone keeps running, then switch over on the next restart — if an update fails, the phone automatically falls back to the other copy), and a small area tracking factory-reset protection status.

**The important thing for your framing specifically: physical memory is not on this list at all.** It isn't a partition, it isn't part of this storage layout, and it isn't read using the same method used to read a partition. It's a completely separate address space — the phone's actual main memory chip — reached using different commands than the ones used for storage, as you'll see below. Don't let a slide imply memory acquisition means "reading a partition" — it doesn't, and it's an easy point for someone in the audience to catch if it's stated imprecisely.

## Exactly where your tool's mode fits in

Here's the complete picture, starting from your current work and building upward:

```
Your tool, written in C, using a library called libusb for
direct, low-level access to devices connected over a
Universal Serial Bus — you are here right now, exchanging
raw data over that connection with a phone that identifies
itself as device 0x05c6, product 0x9008
   ↓
The Sahara protocol — a small authentication exchange that
runs inside the Primary Bootloader itself, straight out of
the boot chip's built-in code. Authenticates and uploads a
Firehose loader — but only if that program is correctly
signed. This signature check is the one real gate standing
between your tool and everything below it.
   ↓
The Firehose protocol takes over: the uploaded loader
starts running, now from the phone's actual main memory
chip, which it sets up itself. It exchanges structured text
commands back and forth over the same connection. Two
categories of command matter here:
    - one category reads and writes actual storage
      partitions
    - the other category reads and writes raw physical
      memory addresses directly — and this second category
      is frequently DISABLED on production phones with
      secure boot locked down. This is a real, common
      restriction, not a hypothetical one.
```

To directly answer "where exactly does it intercept normal startup": the Primary Bootloader branches into the Sahara protocol's authentication exchange **before the Extensible Bootloader even runs — before the phone's main memory chip has been set up by the normal sequence at all.** It isn't "early in the process," it's the earliest possible point in the entire chip, running out of a tiny amount of built-in memory before any memory controller has even been touched. Nothing downstream of that point — TrustZone, the Android Bootloader, the Linux kernel, Security-Enhanced Linux, Android Verified Boot — has had any chance to run yet.

One honest nuance worth including in your talk rather than glossing over: whether data that was sitting in memory **before** you triggered Emergency Download Mode survives into your acquisition window depends entirely on exactly how the phone entered it. If the processor resets but the memory chip itself keeps its power and refresh cycle running, earlier contents likely persist, at least until the Firehose loader's own memory setup happens to overwrite those specific regions. If it's a full power cycle instead, that data is gone completely. This depends on the specific chip involved and exactly how Emergency Download Mode is triggered on it, and it's genuinely worth testing directly on your actual target device rather than confidently asserting an answer either way. Saying "I'm characterizing this empirically" is a stronger position in front of an examiner than a guess.

## Bringing it all together

If you only remember four things walking into your presentation:

1. **Physical memory is not a partition.** It's a separate address space, reached with entirely different commands than the ones used to read a partition.
2. **Emergency Download Mode runs before the entire security stack exists, not around it.** It doesn't defeat Security-Enhanced Linux, app isolation, or Android Verified Boot — none of them have executed yet. The one real gate is a single signature check, during the Sahara protocol's authentication exchange right at the start.
3. **Why memory, and not storage:** encryption keys never leave the Trusted Execution Environment, so a copy of storage taken from a locked phone is unreadable, encrypted data. An unlocked, running phone's memory holds decrypted data and active keys instead — that's the actual target.
4. **Your real, current open question** is whether the Firehose loader on your target device actually allows raw memory reads at all. That's exactly what your current work is building toward — say so plainly if you're asked, rather than implying it's already solved.

## Before moving on

By the end of today, you should be able to walk someone through the entire path: from plugging in a cable, through the two protocol layers involved, to the exact moment in the chip's own boot code where everything gets intercepted — and explain, without hesitating, why that's a fundamentally different problem than reading an encrypted partition. That's the whole presentation, in one clean line of reasoning.