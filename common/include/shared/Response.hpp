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

template<class ErrT, typename V>
class Response {
public:
    explicit inline Response(ErrT error) : isError{true}, error{error} {}
    explicit inline Response(V value) : isError{false}, value{value} {}

    inline bool CheckError() const {
        return isError;
    }

    inline ErrT GetError() const {
        return error;
    }

    inline V GetValue() const {
        return value;
    }

private:
    bool isError;
    [[maybe_unused]] ErrT error;
    [[maybe_unused]] V value;
};

template<typename T>
class Optional {
public:
    explicit inline Optional() : hasValue{false} {}
    explicit inline Optional(T value) : hasValue{true}, value{value} {}

    inline bool HasValue() const {
        return hasValue;
    }

    inline T GetValue() const {
        return value;
    }

    inline void ClearValue() {
        hasValue = false;
    }

    inline void SetValue(const T& newValue) {
        value = newValue;
        hasValue = true;
    }

    inline T GetValueAndClear() {
        hasValue = false;
        return value;
    }

private:
    bool hasValue;
    [[maybe_unused]] T value;
};

class Success {
public:
    explicit inline constexpr Success(bool isSuccess = true) : isSuccess{isSuccess} {}
    static inline constexpr Success MakeSuccess() { return Success(true); }
    static inline constexpr Success MakeFailure() { return Success(false); }
    
    inline bool constexpr IsSuccess() const {
        return isSuccess;
    }

private:
    bool isSuccess;
};

static inline constexpr Success Failure() {
    return Success(false);
}

