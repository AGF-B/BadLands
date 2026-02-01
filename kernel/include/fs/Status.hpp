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

#include <shared/Response.hpp>

namespace FS {
    enum class Status {
        SUCCESS,
        INVALID_PARAMETER,
        UNSUPPORTED,
        NOT_READY,
        DEVICE_ERROR,
        READ_PROTECTED,
        WRITE_PROTECTED,
        LOCK_PROTECTED,
        VOLUME_CORRUPTED,
        VOLUME_FULL,
        NOT_FOUND,
        ALREADY_EXISTS,
        UNAVAILABLE,
        ACCESS_DENIED,
        TIMEOUT,
        ABORTED,
        OUT_OF_BOUNDS,
        IN_USE
    };

    template<typename V>
    class Response : public ::Response<Status, V> {
    public:
        explicit inline Response(Status status) : ::Response<Status, V>(status) {}
        explicit inline Response(V value) : ::Response<Status, V>(value) {}
    };
}
