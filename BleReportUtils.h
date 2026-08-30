#ifndef BLE_REPORT_UTILS_H
#define BLE_REPORT_UTILS_H

#include <cstddef>
#include <cstdint>

// Strips the leading Report ID byte that some hosts (e.g. macOS BLE HID bridge)
// prepend to Output/Feature Report writes, even though the GATT characteristic
// already identifies the report.  If the received payload is exactly
// expectedLen + 1 bytes, the first byte is skipped.
//
// Returns a pointer to the payload start (after optional Report ID) and writes
// the effective payload length to outLen.
static inline const uint8_t *stripReportIdIfPresent(
    const uint8_t *data, size_t len, size_t expectedLen, size_t &outLen)
{
    outLen = len;
    if (len == expectedLen + 1)
    {
        data++;
        outLen--;
    }
    return data;
}

#endif // BLE_REPORT_UTILS_H
