# Android OS Architecture

## for Physical Memory Acquisition

**QAT — Qualcomm Acquisition Tool**

[Kay] · [FYP-I / 10th September 2026]

---

# Roadmap

1. The Android stack
2. How Android boots
3. Partition layout
4. The security model
5. Encryption & key custody
6. Where EDL fits in
7. Where the project stands

---
# Every security check Android makes...

## ...depends on Android actually running.

Emergency Download Mode (EDL) operates **before that's true.**

<!-- This is the thesis of the whole talk. No kernel, no sandboxing, no encryption enforcement is active at the point EDL runs — that's not a bypass, it's a mode that predates all of it. Everything after this slide builds toward proving that claim precisely. -->

---

# The Android Stack

```
 Apps
   ↑
 Framework        (Activity / Package / Window Manager)
   ↑
 Android Runtime — ART       (per-app process)
   ↑
 Native daemons   (init, vold, surfaceflinger…)
   ↑
 HAL — Hardware Abstraction Layer  (vendor code, Binder)
   ↑
 Linux Kernel      (+ Binder, SELinux hooks)
```

<!-- Kernel: hardware, processes, memory — Android adds Binder (IPC) and SELinux hooks. Doesn't exist in EDL. HAL: boundary for chip-specific code; separate process since Project Treble (Android 8), talks over Binder via HIDL/AIDL. Why: lets Google update Android without the OEM re-certifying vendor code every release. ART = Android Runtime: DEX bytecode, AOT (dex2oat) + JIT hybrid, one instance per app, forked from zygote via copy-on-write. Framework: system_server hosts AMS/PMS/WMS as one process — single choke point for permission checks. -->

---

# Boot Chain (Qualcomm)

```
Power on
  ↓
PBL — Primary Bootloader (boot ROM, root of trust)
  ├── EDL triggered here → Sahara → Firehose   (Slide 13)
  ↓
XBL — Extensible Bootloader (DDR init, loads TrustZone)
  ↓
TrustZone / QSEE starts
  ↓
ABL — Android Bootloader (verifies boot image via AVB)
  ↓
Linux kernel → init → zygote → system_server + apps
```

**EDL branches off at the very first stage — before DDR is even set up.**

<!-- PBL = Primary Bootloader, immutable ROM code, root key hash in QFPROM fuses. XBL = Extensible Bootloader, formerly SBL (Secondary Bootloader). QSEE = Qualcomm Secure Execution Environment, TrustZone's secure-world OS. ABL built on Little Kernel. AVB = Android Verified Boot. init runs twice (loads SELinux policy, switches root, re-execs). zygote forks app processes via copy-on-write, then drops privileges/sets UID/SELinux context/seccomp filter. -->

---

# Partition Layout

- **Bootloader chain:** xbl, tz, abl, keymaster…
- **Radio:** modem, modemst1/2
- **Verified boot:** vbmeta, dtbo
- **OS (inside `super`):** boot, system, vendor…
- **User data:** userdata (encrypted), metadata, misc

**Physical RAM is not on this list.** It's a separate address space, not a partition.

<!-- Names vary by OEM/chip generation — this is representative. The RAM point is the one to land hard: don't let a slide imply memory acquisition = reading a partition. Firehose reads storage with read/program; RAM is peek/poke, a completely different command set (Slide 13). -->

---

# Security Model (1/2)

**Two independent layers, stacked:**

- **DAC** — Discretionary Access Control → one Linux user ID per app
- **SELinux** — Mandatory Access Control → security labels + a strict allow-list → fully enforcing since Android 5.0

<!-- DAC = ordinary Unix permissions, app UIDs like u0_a123. SELinux constrains even root — nothing allowed by default, everything must be explicitly permitted. zygote assigns each app's SELinux label at fork time (setcon). -->

---

# Security Model (2/2) — Verified Boot

```
QFPROM (fused OEM pubkey hash)
      ↓ verifies
     XBL → ABL → boot / vbmeta
                     ↓
            dm-verity (runtime block check)
```

States: `LOCKED+GREEN` · `LOCKED+YELLOW` · `UNLOCKED+ORANGE` · `RED`

<!-- QFPROM = Qualcomm Fuse PROM, one-time-programmable fuses, the hardware root of trust. AVB = Android Verified Boot. dm-verity checks system/vendor block-by-block continuously at runtime, not just once at boot. -->

---

<!-- _class: lead -->

# EDL doesn't defeat this stack.

## It runs before any of it exists.

No kernel → no SELinux → no zygote-assigned UID → no dm-verity check.

**The only real gate: the PBL's signature check during Sahara.**

<!-- This slide is the payoff of slides 4–8. Make sure the audience feels the "before, not around" distinction — it's the difference between a bypass and a predecessor. -->

---

# Encryption: Two Approaches

||FDE|FBE|
|---|---|---|
|Scope|whole `/data`|per file|
|Pre-unlock|nothing works|Direct Boot files OK|
|Mechanism|`dm-crypt`|`fscrypt`|
|Status|legacy (pre-Android 7)|mandatory (Android 10+)|

<!-- FDE = Full Disk Encryption. FBE = File-Based Encryption. Direct Boot = the subset of files (alarms, calls, accessibility) readable before unlock, protected by the DE key on the next slide. -->

---

# Where the Keys Actually Live

```
Hardware key (fused, usable only inside the TEE)
       ↓
Keymaster / KeyMint  (a Trusted Application in TrustZone)
       ↓
   ┌──────────────┬───────────────────┐
   DE key            CE key
 (no credential      (+ credential, via
  needed)             Gatekeeper + RPMB
                       attempt counter)
```

**Keys never leave the TEE.**

<!-- TEE = Trusted Execution Environment (TrustZone, here). DE = Device Encrypted, CE = Credential Encrypted. RPMB = Replay Protected Memory Block — attempt counter can't be reset by cloning storage, since writes need a handshake with the storage controller. -->

---
<!-- _class: lead -->

# Why memory, not storage?

**Locked phone, storage image:** FBE ciphertext. Unreadable. Dead end.

**Unlocked, running phone, memory image:** Decrypted files. Unwrapped keys. Live sessions.

### That's the target.

<!-- This is the actual thesis of the entire project — say it slowly. Keys never leaving the TEE is *why* a storage dump is a dead end; running memory is the one place FBE's protection doesn't reach. -->

---

# Where EDL Cuts In

```
QAT (libusb) — device 0x05c6, product 0x9008
       ↓
Sahara (inside the PBL) — auth + upload
       ↓            signed loader required
Firehose (runs from DRAM)
   ├── read / program → storage
   └── peek / poke    → raw physical memory
          ↑ often disabled on production devices
```

<!-- Ties directly back to current work: libusb handles the transport (this slide's top line), Sahara is the next layer up, Firehose is the actual command channel. peek/poke being restricted on production, secure-boot-locked loaders is the real technical constraint — say this plainly, don't imply it's guaranteed to work. -->

---

# Where the Project Stands

- Transport layer: raw USB bulk transfers via **libusb** — in progress
- Next: Sahara handshake, then the Firehose command channel
- **Open question:** does the target's Firehose loader expose `peek`/`poke` at all?
- Also open: DRAM retention across a warm EDL reset — being characterized empirically, not assumed

<!-- Honesty here reads as maturity, not weakness. Naming the real open engineering question is stronger than implying the tool already works end-to-end. -->

---

# Four Things to Remember

1. RAM is not a partition — a separate address space
2. EDL predates the entire security stack, not bypasses it
3. Storage is encrypted at rest; running memory isn't
4. `peek`/`poke` availability is the real open question

<!-- If time runs short, these four lines are the whole talk compressed. Good place to land if a question derails the flow. -->

--- 
