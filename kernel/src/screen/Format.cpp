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

#include <cstdarg>
#include <cstddef>
#include <cstdint>

#include <screen/Log.hpp>

namespace {
    enum class length_modifier {
        hh,
        h,
        none,
        l,
        ll,
        j,
        z,
        t,
        L
    };

    #define case_fetch_ret(lmodif, type, ret_type) case length_modifier::lmodif: return (ret_type)va_arg(*vlist, type)
    static inline intmax_t fetch_signed_number(
        length_modifier lmodif,
        va_list* vlist,
        bool* success
    ) {
        *success = true;

        switch (lmodif) {
            case_fetch_ret(hh, int, intmax_t);
            case_fetch_ret(h, int, intmax_t);
            case_fetch_ret(none, int, intmax_t);
            case_fetch_ret(l, long, intmax_t);
            case_fetch_ret(ll, long long, intmax_t);
            case_fetch_ret(j, intmax_t, intmax_t);
            case_fetch_ret(z, size_t, intmax_t);
            case_fetch_ret(t, ptrdiff_t, intmax_t);
            default:
                *success = false;
        }

        return 0;
    }

    static inline uintmax_t fetch_unsigned_number(
        length_modifier lmodif,
        va_list* vlist,
        bool* success
    ) {
        *success = true;

        switch (lmodif) {
            case_fetch_ret(hh, unsigned int, uintmax_t);
            case_fetch_ret(h, unsigned int, uintmax_t);
            case_fetch_ret(none, unsigned int, uintmax_t);
            case_fetch_ret(l, unsigned long, uintmax_t);
            case_fetch_ret(ll, unsigned long long, uintmax_t);
            case_fetch_ret(j, uintmax_t, uintmax_t);
            case_fetch_ret(z, size_t, uintmax_t);
            case_fetch_ret(t, ptrdiff_t, uintmax_t);
            default:
                *success = false;
        }

        return 0;
    }
    #undef case_fetch_ret

    static intmax_t itoa(intmax_t x, char *buffer, int radix) {
        static constexpr size_t BUFFER_SIZE = 22;

        char tmp[BUFFER_SIZE];
        char* tp = tmp;

        intmax_t i;
        uintmax_t v;

        bool sign = radix == 10 && x < 0;

        if (sign) {
            v = -x;
        }
        else {
            v = static_cast<uintmax_t>(x);
        }

        while (v || tp == tmp) {
            i = v % radix;
            v /= radix;

            if (i < 10) {
                *tp++ = i + '0';
            }
            else {
                *tp++ = i + 'a' - 10;
            }
        }

        intmax_t len = tp - tmp;

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

    static intmax_t utoa(uintmax_t x, char* buffer, int radix) {
        static constexpr size_t BUFFER_SIZE = 22;

        char tmp[BUFFER_SIZE];
        char* tp = tmp;

        uintmax_t i;
        uintmax_t v = x;

        while (v || tp == tmp) {
            i = v % radix;
            v /= radix;

            if (i < 10) {
                *tp++ = i + u'0';
            } else {
                *tp++ = i + u'a' - 10;
            }
        }

        intmax_t len = tp - tmp;

        while (tp > tmp) {
            *buffer++ = *--tp;
        }
        *buffer++ = '\0';

        return len;
    }

    static inline void precision_fill(
        intmax_t* _len,
        intmax_t precision,
        char* number_buffer
    ) {
        intmax_t len = *_len;

        if (precision > len) {
            for (size_t i = len; i > 0; --i) {
                number_buffer[i - 1 + precision - len] = number_buffer[i - 1];
            }
            for (intmax_t i = 0; i < precision - len; ++i) {
                number_buffer[i] = '0';
            }
            *_len = precision;
        }
    }
}

namespace Log {
    void vprintf(const char* format, va_list vlist) {
        char c;

        while (*format != '\0') {
            c = *format++;

            if (c == '%' && *format != '%') {
                bool left_justify = false,
                    force_sign = false,
                    prepend_space = false,
                    alternative_conv = false,
                    leading_zeroes = false;
                
                bool exit_loop = false;

                while (!exit_loop) {
                    c = *format++;

                    switch (c) {
                        case '-':
                            left_justify = true;
                            leading_zeroes = false;
                            break;
                        case '+':
                            force_sign = true;
                            prepend_space = false;
                            break;
                        case ' ':
                            prepend_space = !force_sign;
                            break;
                        case '#':
                            alternative_conv = true;
                            break;
                        case '0':
                            leading_zeroes = !left_justify;
                            break;
                        default:
                            exit_loop = true;
                            --format;
                            break;
                    }
                }

                intmax_t minimum_field_size = 0;
                c = *format++;

                if (c == '*') {
                    minimum_field_size = va_arg(vlist, int);
                }
                else if (c == '-' || (c >= '0' && c <= '9')) {
                    bool neg = c == '-';

                    if (neg) {
                        c = *format++;
                    }

                    while (c >= '0' && c <= '9') {
                        minimum_field_size *= 10;
                        minimum_field_size += static_cast<intmax_t>(c - '0');
                        c = *format++;
                    }

                    --format;

                    if (neg) {
                        minimum_field_size = -minimum_field_size;
                    }
                }
                else {
                    --format;
                }

                intmax_t precision = -1;

                if (*format == '.') {
                    ++format;
                    c = *format++;

                    if (c == '*') {
                        precision = va_arg(vlist, int);
                    }
                    else {
                        precision = 0;
                        bool neg = c == '-';

                        if (neg) {
                            c = *format++;
                        }

                        if (c >= '0' && c <= '9') {
                            while (c >= '0' && c <= '9') {
                                precision *= 10;
                                precision += static_cast<intmax_t>(c - '0');
                                c = *format++;
                            }

                            --format;

                            if (neg) {
                                precision = -precision;
                            }
                        }
                        else {
                            precision = -1;
                        }
                    }
                }

                length_modifier lmodif = length_modifier::none;
                c = *format++;

                switch (c) {
                    case 'h':
                        if (*format++ == 'h') {
                            lmodif = length_modifier::hh;
                        }
                        else {
                            --format;
                            lmodif = length_modifier::h;
                        }
                        break;
                    case 'l':
                        if (*format++ == 'l') {
                            lmodif = length_modifier::ll;
                        }
                        else {
                            --format;
                            lmodif = length_modifier::l;
                        }
                        break;
                    case 'j':
                        ++format;
                        lmodif = length_modifier::j;
                        break;
                    case 'z':
                        ++format;
                        lmodif = length_modifier::z;
                        break;
                    case 't':
                        ++format;
                        lmodif = length_modifier::t;
                        break;
                    case 'L':
                        ++format;
                        lmodif = length_modifier::L;
                        break;
                    default:
                        --format;
                        lmodif = length_modifier::none;
                        break;
                }
                
                c = *format++;

                switch (c) {
                    case 'c': {
                        char buffer = '\0';
                        
                        switch (lmodif) {
                            case length_modifier::none:
                                buffer = va_arg(vlist, int);
                                break;
                            case length_modifier::l:
                                /// TODO: Add support for Unicode
                                buffer = va_arg(vlist, int);
                                break;
                            default:
                                continue;
                        }

                        if (left_justify) {
                            Log::putc(buffer);
                            
                            for (intmax_t i = 0; i < minimum_field_size - 1; ++i) {
                                Log::putc(' ');
                            }
                        }
                        else {
                            for (intmax_t i = 0; i < minimum_field_size - 1; ++i) {
                                Log::putc(' ');
                            }
                            Log::putc(buffer);
                        }

                        break;
                    }
                    case 's': {
                        intmax_t length = 0;

                        switch (lmodif) {
                            case length_modifier::none: {
                                const char* s = va_arg(vlist, const char*);

                                if (left_justify) {
                                    while (*s != '\0') {
                                        Log::putc(*s++);
                                        ++length;
                                    }
                                    for (intmax_t i = 0; i < minimum_field_size - length; ++i) {
                                        Log::putc(' ');
                                    }
                                }
                                else {
                                    const char* tmp = s;
                                    while (*tmp++ != '\0');
                                    length = static_cast<intmax_t>(tmp - s) - 1;

                                    for (intmax_t i = 0; i < minimum_field_size - length; ++i) {
                                        Log::putc(' ');
                                    }
                                    Log::puts(s);
                                }

                                break;
                            }
                            case length_modifier::l: {
                                /// TODO: Add support for Unicode
                                const char* s = va_arg(vlist, const char*);

                                if (left_justify) {
                                    while (*s != '\0') {
                                        Log::putc(*s++);
                                        ++length;
                                    }
                                    for (intmax_t i = 0; i < minimum_field_size - length; ++i) {
                                        Log::putc(' ');
                                    }
                                }
                                else {
                                    const char* tmp = s;
                                    while (*tmp++ != '\0');
                                    length = tmp - s - 1;

                                    for (intmax_t i = 0; minimum_field_size - length; ++i) {
                                        Log::putc(' ');
                                    }
                                    Log::puts(s);
                                }

                                break;
                            }
                            default:
                                continue;
                        }
                        break;
                    }
                    case 'd':
                    case 'i': {
                        if (precision < 0) {
                            precision = 1;
                        }
                        else if (precision > 32) {
                            /// NOTE: Implement a limit on the precision
                            /// to have buffers with a reasonable size
                            precision = 32;
                        }

                        bool fetch_success = true;

                        intmax_t number = fetch_signed_number(lmodif, &vlist, &fetch_success);

                        if (!fetch_success) {
                            continue;
                        }

                        char number_buffer[precision + 1];
                        number_buffer[precision] = 0;
                        intmax_t len = itoa(number, number_buffer, 10);

                        precision_fill(&len, precision, number_buffer);

                        /// TODO: Add support for variable field length
                        (void)leading_zeroes; // remove warning

                        if (force_sign && number >= 0) {
                            Log::putc('+');
                        }
                        else if (prepend_space && number >= 0) {
                            Log::putc(' ');
                        }

                        Log::puts(number_buffer);

                        break;
                    }
                    case 'o': {
                        if (precision < 0) {
                            precision = 1;
                        }
                        else if (precision > 32) {
                            /// NOTE: Implement a limit on the precision
                            /// to have buffers with a reasonable size
                            precision = 32;
                        }

                        bool fetch_success = true;

                        uintmax_t number = fetch_unsigned_number(lmodif, &vlist, &fetch_success);

                        if (!fetch_success) {
                            continue;
                        }

                        char number_buffer[precision + 1];
                        number_buffer[precision] = 0;
                        intmax_t len = utoa(number, number_buffer, 8);

                        precision_fill(&len, precision, number_buffer);

                        /// TODO: Add support for variable field length

                        if (alternative_conv) {
                            Log::putc('0');
                        }

                        Log::puts(number_buffer);

                        break;
                    }
                    case 'x':
                    case 'X': {
                        if (precision < 0) {
                            precision = 1;
                        }
                        else if (precision > 32) {
                            /// NOTE: Implement a limit on the precision
                            /// to have buffers with a reasonable size
                            precision = 32;
                        }

                        bool fetch_success = true;

                        uintmax_t number = fetch_unsigned_number(lmodif, &vlist, &fetch_success);

                        if (!fetch_success) {
                            continue;
                        }

                        char number_buffer[precision + 1];
                        number_buffer[precision] = 0;
                        intmax_t len = utoa(number, number_buffer, 16);

                        if (c == 'X') {
                            for (size_t i = len; i > 0; --i) {
                                if (number_buffer[i - 1] >= u'a' && number_buffer[i - 1] <= u'f') {
                                    number_buffer[i - 1] -= (u'a' - u'A');
                                }
                            }
                        }

                        precision_fill(&len, precision, number_buffer);

                        /// TODO: Add support for variable field length

                        if (alternative_conv && number != 0) {
                            Log::puts("0x");
                        }

                        Log::puts(number_buffer);

                        break;
                    }
                    case 'u': {
                        /// TODO: Refactor this repetitive piece of code
                        if (precision < 0) {
                            precision = 1;
                        }
                        else if (precision > 32) {
                            /// NOTE: Implement a limit on the precision
                            /// to have buffers with a reasonable size
                            precision = 32;
                        }

                        bool fetch_success = true;

                        uintmax_t number = fetch_unsigned_number(lmodif, &vlist, &fetch_success);

                        if (!fetch_success) {
                            continue;
                        }

                        char number_buffer[precision + 1];
                        number_buffer[precision] = 0;
                        intmax_t len = utoa(number, number_buffer, 10);

                        precision_fill(&len, precision, number_buffer);

                        /// TODO: Add support for variable field length

                        Log::puts(number_buffer);

                        break;
                    }
                }
            }
            else {
                Log::putc(c);
            }
        }
    }
    
    void printf(const char* format, ...) {
        va_list args;
        va_start(args, format);

        vprintf(format, args);

        va_end(args);
    }
}