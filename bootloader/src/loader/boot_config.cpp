// SPDX-License-Identifier: GPL-3.0-only
//
// Copyright (C) 2026 Alexandre Boissiere
// This file is part of the BadLands operating system.
//
// This program is free software: you can redistribute it and/or modify it under the terms of the
// GNU General Public License as published by the Free Software Foundation, version 3.
// This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
// without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with this program.
// If not, see <https://www.gnu.org/licenses/>. 

#include <cstdint>

#include <efi/efi_fs.hpp>
#include <efi/efi_image_services.hpp>
#include <efi/efi_misc.hpp>

#include <loader/boot_config.hpp>

#include <ldstdio.hpp>
#include <ldstdlib.hpp>

namespace {
    static const CHAR16* config_file_path = u"\\EFI\\BOOT\\boot.cfg\0";
}

namespace Loader {
    static constexpr BOOLEAN isLineEnd(CHAR8 c) {
        return c == '\n' || c == '\r';
    }

    static constexpr UINT8 hexCharToByte(CHAR8 c, BOOLEAN& err) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;

        err = true;

        return 0;
    }

    static BOOT_CONFIGURATION ParseConfiguration(CONST UINT8* data, UINT64 size) {
        BOOT_CONFIGURATION config;

        for (UINT32 i = 0; i < 16; ++i) {
            config.root_partition_uuid[i] = 0;
        }

        BOOLEAN root_found = false;

        UINT64 i = 0;
        UINT64 entry_num = 1;
        
        while (i < size) {
            // Read key
            CHAR8 key[256];
            UINT64 key_len = 0;

            while (i < size && data[i] != '=' && !isLineEnd(data[i])) {
                if (key_len < 255) {
                    key[key_len++] = data[i];
                }
                ++i;
            }

            key[key_len] = '\0';

            if (key_len == 0 && (i >= size || isLineEnd(data[i]))) {
                // Skip empty lines
                while (i < size && (data[i] == '\n' || data[i] == '\r')) {
                    ++i;
                }

                continue;
            }

            if (i >= size || data[i] != '=') {
                Loader::printf(
                    u"Malformed boot configuration for entry %llu: Missing '=' after key\n\r",
                    entry_num
                );

                EFI::Terminate();
            }

            // Skip '='
            ++i;
            
            // Read value
            CHAR8 value[256];
            UINT64 value_len = 0;

            while (i < size && !isLineEnd(data[i])) {
                if (value_len < 255) {
                    value[value_len++] = data[i];
                }
                ++i;
            }

            value[value_len] = '\0';

            if (key_len == 4 && Loader::memcmp(key, "root", 4)) {
                UINT16 byte_idx = 0;
                BOOLEAN hex_err = false;

                for (UINT64 j = 0; j < value_len && byte_idx < 16; ++j) {
                    if (value[j] == '-') {
                        continue;
                    }

                    if (j + 1 < value_len) {
                        config.root_partition_uuid[byte_idx++] = 
                            (hexCharToByte(value[j], hex_err) << 4)
                            | hexCharToByte(value[j + 1], hex_err);
                        
                        ++j; // skip the second hex char
                    }
                }

                if (hex_err || byte_idx != 16) {
                    Loader::printf(
                        u"Malformed boot configuration for entry %llu: Invalid root UUID format\n\r",
                        entry_num
                    );

                    EFI::Terminate();
                }

                root_found = true;
            }
            else {
                Loader::printf(
                    u"Unknown boot configuration key '%s' for entry %llu\n\r",
                    key,
                    entry_num
                );
            }

            ++entry_num;

            // Skip to next line
            while (i < size && isLineEnd(data[i])) {
                ++i;
            }
        }
        
        if (!root_found) {
            Loader::puts(u"Malformed boot configuration: Missing root partition UUID\n\r");
            EFI::Terminate();
        }

        return config;
    }

    BOOT_CONFIGURATION GetBootConfiguration(EFI_HANDLE ImageHandle) {
        EFI_LOADED_IMAGE_PROTOCOL* efi_lip = EFI::getLoadedImageProtocol(ImageHandle);
        EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* efi_sfsp = EFI::getDeviceSFSP(ImageHandle, efi_lip->DeviceHandle);
        EFI_FILE_PROTOCOL* efi_root_fsp = EFI::openDeviceVolume(efi_sfsp);
        EFI_FILE_PROTOCOL* efi_bcfg_fsp = EFI::openReadOnlyFile(efi_root_fsp, const_cast<CHAR16*>(config_file_path));

        if (efi_bcfg_fsp == nullptr) {
            Loader::puts(u"Boot configuration was either not found, or no suitable protocol was found to locate/open it\n\r");
            EFI::Terminate();
        }

        EFI_FILE_INFO* ConfigInfo = EFI::getFileInfo(efi_bcfg_fsp);
        UINT64 ConfigSize = ConfigInfo->FileSize;
        EFI::sys->BootServices->FreePool(ConfigInfo);

        UINT8* ConfigData = nullptr;

        EFI::sys->BootServices->AllocatePool(
            EfiLoaderData,
            ConfigSize,
            reinterpret_cast<VOID**>(&ConfigData)
        );

        if (efi_bcfg_fsp->Read(efi_bcfg_fsp, &ConfigSize, ConfigData) != EFI_SUCCESS) {
            Loader::puts(u"Error reading boot configuration\n\r");
            EFI::Terminate();
        }

        efi_bcfg_fsp->Close(efi_bcfg_fsp);

        BOOT_CONFIGURATION config = ParseConfiguration(ConfigData, ConfigSize);

        EFI::sys->BootServices->FreePool(ConfigData);

        return config;
    }
}