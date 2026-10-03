```c
/*
 * qat_edl_bulk_test.c
 *
 * Minimal libusb-1.0 program: find a Qualcomm EDL device (VID 0x05c6,
 * PID 0x9008), claim its interface, discover bulk IN/OUT endpoints from
 * the descriptor (not hardcoded), and perform one bulk OUT write + bulk
 * IN read.
 *
 * Build:
 *   gcc -o qat_edl_bulk_test qat_edl_bulk_test.c $(pkg-config --libs --cflags libusb-1.0)
 *
 * Run (needs raw USB access — see udev rule note at bottom, or run as root
 * while developing):
 *   ./qat_edl_bulk_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libusb-1.0/libusb.h>

#define EDL_VID 0x05c6
#define EDL_PID 0x9008
#define XFER_TIMEOUT_MS 5000

static int find_bulk_endpoints(libusb_device *dev, int *iface_num,
                                uint8_t *ep_in, uint8_t *ep_out)
{
    struct libusb_config_descriptor *cfg;
    int r = libusb_get_active_config_descriptor(dev, &cfg);
    if (r != 0) {
        fprintf(stderr, "get_active_config_descriptor failed: %s\n",
                libusb_error_name(r));
        return -1;
    }

    int found = 0;
    for (int i = 0; i < cfg->bNumInterfaces && !found; i++) {
        const struct libusb_interface *iface = &cfg->interface[i];
        for (int a = 0; a < iface->num_altsetting && !found; a++) {
            const struct libusb_interface_descriptor *idesc = &iface->altsetting[a];
            uint8_t local_in = 0, local_out = 0;

            for (int e = 0; e < idesc->bNumEndpoints; e++) {
                const struct libusb_endpoint_descriptor *ep = &idesc->endpoint[e];
                uint8_t xfer_type = ep->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK;
                if (xfer_type != LIBUSB_TRANSFER_TYPE_BULK)
                    continue;

                if (ep->bEndpointAddress & LIBUSB_ENDPOINT_IN)
                    local_in = ep->bEndpointAddress;
                else
                    local_out = ep->bEndpointAddress;
            }

            if (local_in && local_out) {
                *iface_num = idesc->bInterfaceNumber;
                *ep_in = local_in;
                *ep_out = local_out;
                found = 1;
            }
        }
    }

    libusb_free_config_descriptor(cfg);
    return found ? 0 : -1;
}

int main(void)
{
    libusb_context *ctx = NULL;
    libusb_device **list = NULL;
    libusb_device_handle *handle = NULL;
    libusb_device *target = NULL;
    int r;

    r = libusb_init(&ctx);
    if (r != 0) {
        fprintf(stderr, "libusb_init failed: %s\n", libusb_error_name(r));
        return EXIT_FAILURE;
    }

    /* Bump this for noisy driver-level debugging; LIBUSB_LOG_LEVEL_WARNING
     * is enough day-to-day. */
    libusb_set_option(ctx, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_WARNING);

    ssize_t cnt = libusb_get_device_list(ctx, &list);
    if (cnt < 0) {
        fprintf(stderr, "libusb_get_device_list failed: %s\n",
                libusb_error_name((int)cnt));
        libusb_exit(ctx);
        return EXIT_FAILURE;
    }

    for (ssize_t i = 0; i < cnt; i++) {
        struct libusb_device_descriptor desc;
        r = libusb_get_device_descriptor(list[i], &desc);
        if (r != 0)
            continue;

        if (desc.idVendor == EDL_VID && desc.idProduct == EDL_PID) {
            target = list[i];
            break;
        }
    }

    if (!target) {
        fprintf(stderr, "No device %04x:%04x found. Is the phone in EDL mode "
                        "and enumerated? Check `lsusb`.\n", EDL_VID, EDL_PID);
        libusb_free_device_list(list, 1);
        libusb_exit(ctx);
        return EXIT_FAILURE;
    }

    r = libusb_open(target, &handle);
    if (r != 0) {
        fprintf(stderr, "libusb_open failed: %s — likely a permissions "
                        "problem (see udev note). Try running as root to "
                        "confirm before fixing perms.\n", libusb_error_name(r));
        libusb_free_device_list(list, 1);
        libusb_exit(ctx);
        return EXIT_FAILURE;
    }

    /* Let libusb detach any kernel driver (e.g. qcserial) automatically
     * before claim, and reattach on release/close. Must be set before
     * claim_interface. Not all backends support this (returns
     * LIBUSB_ERROR_NOT_SUPPORTED on e.g. Windows) — ignore failure there. */
    libusb_set_auto_detach_kernel_driver(handle, 1);

    int iface_num = -1;
    uint8_t ep_in = 0, ep_out = 0;
    if (find_bulk_endpoints(target, &iface_num, &ep_in, &ep_out) != 0) {
        fprintf(stderr, "Could not find a bulk IN/OUT endpoint pair.\n");
        goto cleanup;
    }

    printf("Found interface %d: EP_IN=0x%02x EP_OUT=0x%02x\n",
           iface_num, ep_in, ep_out);

    r = libusb_claim_interface(handle, iface_num);
    if (r != 0) {
        fprintf(stderr, "libusb_claim_interface failed: %s\n",
                libusb_error_name(r));
        goto cleanup;
    }

    /*
     * Sahara protocol hello packet is 0x30 bytes; command 0x01 is
     * SAHARA_HELLO_REQ from device, but here we're just proving the
     * transport works. Replace this buffer with a real Sahara/Firehose
     * frame once you're past transport-layer testing. Reading from an
     * idle EDL device without sending anything first is a common mistake
     * — the device is waiting for host to speak first in some flows,
     * so don't assume IN will ever complete without a prior OUT.
     */
    unsigned char probe[] = { 0x01, 0x00, 0x00, 0x00 };
    int actual = 0;

    r = libusb_bulk_transfer(handle, ep_out, probe, sizeof(probe),
                              &actual, XFER_TIMEOUT_MS);
    if (r != 0) {
        fprintf(stderr, "bulk OUT failed: %s (wrote %d of %zu bytes)\n",
                libusb_error_name(r), actual, sizeof(probe));
        goto cleanup_release;
    }
    printf("Wrote %d bytes OUT\n", actual);

    unsigned char rxbuf[512];
    memset(rxbuf, 0, sizeof(rxbuf));
    r = libusb_bulk_transfer(handle, ep_in, rxbuf, sizeof(rxbuf),
                              &actual, XFER_TIMEOUT_MS);
    if (r != 0) {
        fprintf(stderr, "bulk IN failed: %s\n", libusb_error_name(r));
        goto cleanup_release;
    }

    printf("Read %d bytes IN:\n", actual);
    for (int i = 0; i < actual; i++) {
        printf("%02x ", rxbuf[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    printf("\n");

cleanup_release:
    libusb_release_interface(handle, iface_num);
cleanup:
    if (handle)
        libusb_close(handle);
    libusb_free_device_list(list, 1);
    libusb_exit(ctx);
    return r == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

/*
 * udev rule to avoid needing root (Arch):
 *   /etc/udev/rules.d/51-edl.rules
 *
 *   SUBSYSTEM=="usb", ATTR{idVendor}=="05c6", ATTR{idProduct}=="9008", MODE="0666", GROUP="plugdev"
 *
 * Then: sudo udevadm control --reload-rules && sudo udevadm trigger
 * Make sure your user is in the `plugdev` group (or drop MODE to 0666
 * and skip the group entirely for local dev).
 */
```