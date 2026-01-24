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
