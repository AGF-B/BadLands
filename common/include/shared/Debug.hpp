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

namespace Debug {
    static inline constexpr bool DEBUG_USB_OVERRIDE     = false;
    static inline constexpr bool DEBUG_USB_ERRORS       = false || DEBUG_USB_OVERRIDE;
    static inline constexpr bool DEBUG_USB_SOFT_ERRORS  = false || DEBUG_USB_OVERRIDE;
    static inline constexpr bool DEBUG_USB_WARNINGS     = false || DEBUG_USB_OVERRIDE;
    static inline constexpr bool DEBUG_USB_INFO         = false || DEBUG_USB_OVERRIDE;

    static inline constexpr bool DEBUG_HID_OVERRIDE     = false;
    static inline constexpr bool DEBUG_HID_ERRORS       = false || DEBUG_HID_OVERRIDE;
    static inline constexpr bool DEBUG_HID_INFO         = false || DEBUG_HID_OVERRIDE;
}
