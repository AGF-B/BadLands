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

#pragma once

#include <efi/efi_datatypes.h>

struct EFI_SYSTEM_CONFIGURATION {
    VOID    *ACPI_10;
    VOID    *ACPI_20;
    VOID    *SAL_SYSTEM;
    VOID    *SMBIOS;
    VOID    *SMBIOS3;
    VOID    *MPS;
    VOID    *JSON_CONFIG_DATA;
    VOID    *JSON_CAPSULE_DATA;
    VOID    *JSON_CAPSULE_RESULT;
    VOID    *DTB;
    VOID    *RT_PROPERTIES;
    VOID    *MEMORY_ATTRIBUTES;
    VOID    *CONFORMANCE_PROFILES;
    VOID    *MEMORY_RANGE_CAPSULE;
    VOID    *DEBUG_IMAGE_INFO;
    VOID    *SYSTEM_RESOURCE;
    VOID    *IMAGE_SECURITY_DATABASE;
};

namespace Loader {
    void DetectSystemConfiguration(EFI_SYSTEM_CONFIGURATION* sysconfig);
}
