 
## 1. USB Bulk Transfer, Conceptually

> [!info] Host-driven USB is host-driven. The phone in EDL mode never initiates anything. Your Linux box is the host and polls. The device only speaks when spoken to (mostly, see the framing question below).

> [!info] Bulk transfer type One of four transfer types: control, bulk, interrupt, isochronous.
> 
> - No guaranteed bandwidth or latency
> - Guaranteed data integrity via hardware-level CRC and retry (unlike isochronous)
> - Unidirectional per endpoint

> [!info] Endpoint addressing 8-bit number, bit 7 = direction.
> 
> - `0x81` = endpoint 1, IN (device to host)
> - `0x01` = endpoint 1, OUT (host to device)
> 
> Do not hardcode these. Always pull them from the interface's endpoint descriptors at runtime, even though `0x81`/`0x01` is the common Qualcomm layout.

> [!warning] Zero-length packets (ZLP) A bulk transfer is framed in packets of `wMaxPacketSize` (512 bytes, USB 2.0 high-speed bulk). If your payload is an exact multiple of `wMaxPacketSize`, USB convention requires a ZLP afterward on OUT transfers so the receiver knows the transfer ended.
> 
> libusb does **not** send this automatically on `libusb_bulk_transfer`. You handle it, or your Sahara handshake hangs on a 512-byte-aligned packet with no obvious error.

- [x] Confirm I understand why a receiver would otherwise wait for more data on an exact-multiple payload

---

## 2. libusb-1.0 API Surface

Only what's needed for EDL transport work:

```c
int libusb_init(libusb_context **ctx);
void libusb_exit(libusb_context *ctx);

ssize_t libusb_get_device_list(libusb_context *ctx, libusb_device ***list);
void libusb_free_device_list(libusb_device **list, int unref_devices);

int libusb_get_device_descriptor(libusb_device *dev, struct libusb_device_descriptor *desc);

libusb_device_handle *libusb_open_device_with_vid_pid(
    libusb_context *ctx, uint16_t vendor_id, uint16_t product_id);

int libusb_open(libusb_device *dev, libusb_device_handle **handle);
void libusb_close(libusb_device_handle *dev_handle);

int libusb_kernel_driver_active(libusb_device_handle *dev, int interface_number);
int libusb_detach_kernel_driver(libusb_device_handle *dev, int interface_number);
int libusb_set_auto_detach_kernel_driver(libusb_device_handle *dev, int enable);

int libusb_claim_interface(libusb_device_handle *dev, int interface_number);
int libusb_release_interface(libusb_device_handle *dev, int interface_number);

int libusb_bulk_transfer(libusb_device_handle *dev_handle,
    unsigned char endpoint, unsigned char *data, int length,
    int *actual_length, unsigned int timeout);
```

> [!danger] Convenience trap `libusb_open_device_with_vid_pid` silently picks the **first** matching device. Fine for one phone plugged in. Dangerous the moment two are connected. Real tooling enumerates manually.

> [!danger] The #1 beginner bug Every function returning `int` returns a `libusb_error` code (≤ 0), not `errno`. Use `libusb_error_name()` / `libusb_strerror()`.
> 
> `libusb_bulk_transfer` returning `0` (success) does **not** mean your full buffer was transferred. `actual_length` is a separate out-parameter and can be less than `length` even on success. Check it independently, every time.

> [!info] Sync vs async `libusb_bulk_transfer` is synchronous, blocks the calling thread. Fine for single request/response (Sahara handshake). Switch to async (`libusb_fill_bulk_transfer` + `libusb_submit_transfer` + `libusb_handle_events`) only once sustained throughput for memory dumps becomes the bottleneck. Don't reach for it early.

> [!warning] Timeout `timeout` is milliseconds. `0` = block forever. Never use `0` in production, an unresponsive device hangs the acquisition tool indefinitely. Use a real timeout and handle `LIBUSB_ERROR_TIMEOUT` explicitly.

> [!warning] Kernel driver conflicts (Linux-specific) If `qcserial` or similar has claimed the interface, `libusb_claim_interface` fails with `LIBUSB_ERROR_BUSY`. Call `libusb_set_auto_detach_kernel_driver(handle, 1)` right after `libusb_open`, before claiming.

- [ ] Trace through my own code and mark every place I check `actual_length` vs just the return code

---

## 3. Program Structure (see `qat_edl_bulk_test.c`)

Flow:

1. `libusb_init` → context
2. `libusb_get_device_list` → enumerate
3. Match `idVendor == 0x05c6 && idProduct == 0x9008` from descriptor
4. `libusb_open`
5. `libusb_set_auto_detach_kernel_driver(handle, 1)`
6. Walk `libusb_get_active_config_descriptor` → interfaces → altsettings → endpoints, find first bulk IN and bulk OUT pair
7. `libusb_claim_interface`
8. `libusb_bulk_transfer` OUT (write probe/command)
9. `libusb_bulk_transfer` IN (read response)
10. `libusb_release_interface`, `libusb_close`, `libusb_free_device_list`, `libusb_exit`

> [!question] Open question to resolve with exercise 1 below Does the EDL target send anything unsolicited before you write? Or does it strictly wait for host-first framing? This determines whether step 8 or step 9 comes first in your real Sahara implementation.

---

## 4. Failure Ladder on Arch (debug in this order)

> [!bug] Device not found `lsusb -d 05c6:9008` first. If nothing shows, it's cable/mode, not code. `dmesg -w` while plugging in, watch for immediate disconnect (flaky cable/hub, use a direct rear port).

> [!bug] `LIBUSB_ERROR_ACCESS` on open Permissions. `ls -l /dev/bus/usb/<bus>/<dev>`. Fix with udev rule (below), don't run as root permanently.

> [!bug] `LIBUSB_ERROR_BUSY` on claim Something else has the interface (`qcserial`, ModemManager). `lsusb -t` shows driver bindings. `systemctl mask ModemManager` while developing saves pain.

> [!bug] `LIBUSB_ERROR_TIMEOUT` on transfer Wrong endpoint address (verify with `lsusb -v -d 05c6:9008` against what your code found), or a protocol-layer wait-order problem, not transport.

> [!bug] `LIBUSB_ERROR_PIPE` Endpoint stalled. Malformed packet or wrong `wMaxPacketSize` assumption. `libusb_clear_halt` and retry once; repeated stalls mean payload framing is wrong.

> [!tip] Ground truth tool: usbmon `sudo modprobe usbmon`, then Wireshark/tshark on the matching `usbmonN` interface. Gives every USB transaction at the wire level, independent of your code. When code and assumptions disagree, this tells you who's lying.

**udev rule** (avoid needing root):

```
# /etc/udev/rules.d/51-edl.rules
SUBSYSTEM=="usb", ATTR{idVendor}=="05c6", ATTR{idProduct}=="9008", MODE="0666", GROUP="plugdev"
```

```
sudo udevadm control --reload-rules && sudo udevadm trigger
```

---

## 5. Exercises

- [ ] **1. Framing direction.** Loop `libusb_bulk_transfer` reads with a short timeout (~200ms) until `LIBUSB_ERROR_TIMEOUT`, before sending anything. Determine if the device sends unsolicited data on connect.
- [ ] **2. Leak audit.** Trace every early-return/`goto` path in the program. Confirm nothing leaks a claimed interface or device list ref under any failure branch. Explain what `libusb_free_device_list(list, 1)` actually does and what breaks if called with `0` while `target` is still referenced.
- [ ] **3. ZLP behavior.** Send a payload that's an exact multiple of `wMaxPacketSize` (check via `libusb_get_max_packet_size()`). Confirm whether a follow-up zero-length transfer is required for your actual device to consider it complete.
- [ ] **4. Async port.** Once sync throughput becomes the bottleneck, port `main()` to `libusb_fill_bulk_transfer` + `libusb_submit_transfer` + completion callback + `libusb_handle_events_completed` loop.



---



