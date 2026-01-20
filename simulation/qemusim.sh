# SPDX-License-Identifier: GPL-3.0-only
#
# Copyright (C) 2026 Alexandre Boissiere
# This file is part of the BadLands operating system.
#
# This program is free software: you can redistribute it and/or modify it under the terms of the
# GNU General Public License as published by the Free Software Foundation, version 3.
# This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
# without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with this program.
# If not, see <https://www.gnu.org/licenses/>. 

qemu-system-x86_64 \
-drive file=disk.img,format=raw,media=disk \
-drive if=pflash,format=raw,unit=0,file=OVMF_CODE-pure-efi.fd \
-drive if=pflash,format=raw,unit=1,file=OVMF_VARS-pure-efi.fd \
-m 256M \
-display sdl \
-vga std \
-machine q35 \
-cpu max,host-phys-bits=on,+avx,enforce \
-device nec-usb-xhci \
-device usb-kbd \
-device usb-hub \
-rtc base=localtime \
-net none \
-monitor telnet:127.0.0.1:7777,server,nowait \
-d int,cpu_reset \
-D qemu.log \
-no-reboot