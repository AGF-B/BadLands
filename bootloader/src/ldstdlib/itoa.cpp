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

#include <efi/efi_datatypes.h>

#include <ldstdlib.hpp>

INTN Loader::itoa(INTN x, CHAR16* buffer, INT32 radix) {
    CHAR16 tmp[12];
    CHAR16 *tp = tmp;

    INTN i;
    UINTN v;

    BOOLEAN sign = (radix == 10 && x < 0);

    if (sign) {
        v = -x;
    } else {
        v = static_cast<UINTN>(x);
    }

    while (v || tp == tmp) {
        i = v % radix;
        v /= radix;

        if (i < 10) {
            *tp++ = i + u'0';
        } else {
            *tp++ = i + u'a' - 10;
        }
    }

    INTN len = tp - tmp;
    
    if (sign) {
        *buffer++ = '-';
        ++len;
    }

    while (tp > tmp) {
        *buffer++ = *--tp;
    }
    *buffer++ = '\0';

    return len;
}

INTN Loader::utoa(UINTN x, CHAR16* buffer, INT32 radix) {
    CHAR16 tmp[12];
    CHAR16 *tp = tmp;

    UINTN i;
    UINTN v = x;

    while (v || tp == tmp) {
        i = v % radix;
        v /= radix;

        if (i < 10) {
            *tp++ = i + u'0';
        } else {
            *tp++ = i + u'a' - 10;
        }
    }

    INTN len = tp - tmp;

    while (tp > tmp) {
        *buffer++ = *--tp;
    }
    *buffer++ = '\0';

    return len;
}
