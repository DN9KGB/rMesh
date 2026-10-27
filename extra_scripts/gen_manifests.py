#!/usr/bin/env python3
"""
Generates per-device esp-web-tools manifest JSON files for GitHub Releases.
Device list and chip config is read from devices.json in the repo root.
Optionally also writes <device>_flash.txt with the matching esptool command
(for the release ZIPs, so manual flashing always uses the correct offsets).
Usage: python gen_manifests.py <tag> <output_dir> [flash_txt_dir]
"""
import json
import os
import sys

tag        = sys.argv[1]
output_dir = sys.argv[2]
flash_dir  = sys.argv[3] if len(sys.argv) > 3 else None
base_url   = f"https://github.com/DN9KGB/rMesh/releases/download/{tag}"

with open('devices.json', encoding='utf-8') as f:
    devices = json.load(f)

os.makedirs(output_dir, exist_ok=True)
if flash_dir:
    os.makedirs(flash_dir, exist_ok=True)

FLASH_TXT = """rMesh {tag} - {device} ({chip})

Flash all four files with esptool (https://github.com/espressif/esptool).
Replace PORT with your serial port (e.g. /dev/ttyUSB0 or COM3).

1) Optional, wipes everything incl. settings:
   esptool --chip {esptool_chip} -p PORT erase-flash

2) Flash:
   esptool --chip {esptool_chip} -p PORT -b 460800 write-flash \\
     0x{boot:x} {device}_bootloader.bin \\
     0x8000 {device}_partitions.bin \\
     0x10000 {device}_firmware.bin \\
     0x{lfs:x} {device}_littlefs.bin

Only update the WebUI/filesystem (settings are kept):
   esptool --chip {esptool_chip} -p PORT write-flash 0x{lfs:x} {device}_littlefs.bin

The filesystem offset 0x{lfs:x} is board specific. If littlefs.bin is written
to a wrong address, the node runs but the WebUI shows "Not Found" and the
console 'help' is missing. Fix: console command 'updf 1' or reflash as above.
(esptool < v4.8 uses write_flash / erase_flash with underscores.)
"""

for dev in devices:
    device      = dev['name']
    chip        = dev['chip']

    # esp-web-tools manifests only apply to ESP32 chips
    if not chip.startswith('ESP32'):
        print(f'  SKIP {device} (not ESP32)')
        continue

    lfs_offset  = dev['lfs_offset']
    is_s3       = chip == 'ESP32-S3'
    boot_offset = 0x0000 if is_s3 else 0x1000

    manifest = {
        'name':    f'rMesh \u2013 {device}',
        'version': tag,
        'builds': [{
            'chipFamily': chip,
            'parts': [
                {'path': f'{base_url}/{device}_bootloader.bin', 'offset': boot_offset},
                {'path': f'{base_url}/{device}_partitions.bin',  'offset': 0x8000},
                {'path': f'{base_url}/{device}_firmware.bin',    'offset': 0x10000},
                {'path': f'{base_url}/{device}_littlefs.bin',    'offset': lfs_offset},
            ]
        }]
    }

    out_path = os.path.join(output_dir, f'{device}_manifest.json')
    with open(out_path, 'w') as f:
        json.dump(manifest, f, indent=2)
    print(f'  {out_path}')

    if flash_dir:
        txt = FLASH_TXT.format(tag=tag, device=device, chip=chip,
                               esptool_chip='esp32s3' if is_s3 else 'esp32',
                               boot=boot_offset, lfs=lfs_offset)
        txt_path = os.path.join(flash_dir, f'{device}_flash.txt')
        with open(txt_path, 'w', newline='\r\n') as f:
            f.write(txt)
        print(f'  {txt_path}')
