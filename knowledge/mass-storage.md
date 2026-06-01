# USB Mass Storage

Accessed from **HID > USB Mass Storage**. ESP32-S3 boards with an SD card only.

UniGeek exposes its storage to a host computer as a removable USB drive (USB Mass Storage Class). Plug the device in, open the screen, and the SD card mounts on the host like a card reader — drag files on and off, no extra software. BACK ejects the drive and hands the card back to the firmware.

## How to use

1. Plug the device into a host computer over USB
2. Open **HID > USB Mass Storage**
3. The screen reads **Mounted** and shows the card capacity. A removable drive appears on the host.
4. Read or write files from the host. The screen's **R / W** counters tick up as the host reads and writes sectors.
5. Press **BACK** to eject. The drive disappears from the host and the firmware can use the card again.

## What gets exposed

The drive is whatever **Storage** the board loaded at boot:

- **SD card present** → the card's real FAT volume is exposed sector-for-sector, exactly as a USB card reader would. The host sees the same files the firmware's File Manager sees.
- **No SD card** (running from internal LittleFS) → the screen shows **No SD card**. LittleFS is not a FAT block device, so it cannot be presented to a host as a drive.

> [!note]
> While the drive is mounted the **host owns the card**. The firmware does not touch the filesystem during this time, so other screens that read the SD (File Manager, Ducky Script, NFC dumps…) should not be used until you eject. Eject with BACK — or "Safely Remove" / unmount on the host — before pulling the card or leaving the screen.

## Single-claim USB profile

Mass Storage is a single-claim USB profile, like USB MouseKeyboard and WebAuthn. The USB drive interface has to be registered before the USB stack starts, and only the **first** USB feature opened in a session gets to register. If you opened **USB MouseKeyboard** or **USB Web Authn** earlier this boot, the Mass Storage screen shows **USB busy** and asks you to reboot and open Mass Storage first.

| You opened first this boot | Mass Storage result |
|---|---|
| Nothing (Mass Storage is first) | Mounts normally |
| USB MouseKeyboard | USB busy — reboot, open Mass Storage first |
| USB Web Authn | USB busy — reboot, open Mass Storage first |

After ejecting you can re-open Mass Storage in the same session without a reboot — the interface stays registered, it just re-presents the media.

## Where it works

| OS | Status | Notes |
|---|---|---|
| Windows 10/11 | Full | Drive appears under "This PC" as a removable disk |
| macOS | Full | Mounts on the desktop; use "Eject" (or the device BACK button) before unplugging |
| Linux | Full | Auto-mounts under most desktops; otherwise `mount /dev/sdX1` |

The host reads the FAT filesystem already on the card, so formatting, file names, and free space all match what the device sees. The card must be FAT/FAT32/exFAT-formatted (the same requirement as normal SD use on the device).

## Implementation notes

- Built on arduino-esp32's `USBMSC` (TinyUSB MSC, `CONFIG_TINYUSB_MSC_ENABLED`). 512-byte sectors.
- `UsbMscUtil` bridges the MSC read/write callbacks to `IStorage::readBlocks()` / `writeBlocks()`. Only a backend that reports `isBlockDevice()` (the SD backend) can be exposed; LittleFS returns false.
- Raw sector I/O on the SD backend goes through the same `MisoDcGuard` as every other SD op, so it stays correct on CoreS3-style boards that share the DC/MISO pin (GPIO35) and stays serialised against the render loop.
- The MSC callbacks run on the TinyUSB task; the screen only reads counters, so there is no concurrent filesystem access from the firmware while mounted.

## Safety

> [!warn]
> The host has full read/write access to the card while mounted, including deleting or formatting it. Eject cleanly (host "Safely Remove" / unmount, or device BACK) before pulling the card to avoid corrupting the FAT volume mid-write.
