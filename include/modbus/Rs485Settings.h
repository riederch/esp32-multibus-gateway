#pragma once

#include <stdint.h>

namespace multibus {
namespace modbus {

enum class Rs485Parity : uint8_t {
    None = 0x00,
    Even = 0x01,
    Odd = 0x02,
};

enum class Rs485StopBits : uint8_t {
    One = 0x01,
    Two = 0x02,
    OnePointFive = 0x03,
};

struct Rs485SerialSettings {
    uint32_t baudRate = 9600;
    uint8_t dataBits = 8;
    Rs485StopBits stopBits = Rs485StopBits::One;
    Rs485Parity parity = Rs485Parity::None;
};

inline bool validRs485BaudRate(uint32_t value) {
    switch (value) {
        case 1200:
        case 2400:
        case 4800:
        case 9600:
        case 19200:
        case 38400:
        case 57600:
        case 115200:
            return true;
        default:
            return false;
    }
}

inline bool validRs485SerialSettings(const Rs485SerialSettings& settings) {
    if (!validRs485BaudRate(settings.baudRate)) return false;
    if (settings.dataBits < 7 || settings.dataBits > 9) return false;
    const uint8_t stop = static_cast<uint8_t>(settings.stopBits);
    if (stop < 1 || stop > 3) return false;
    const uint8_t parity = static_cast<uint8_t>(settings.parity);
    return parity <= static_cast<uint8_t>(Rs485Parity::Odd);
}

inline bool esp32SupportsRs485SerialSettings(const Rs485SerialSettings& settings) {
    if (!validRs485SerialSettings(settings)) return false;
    // ESP32-S3 UART hardware supports up to 8 data bits. The Arduino serial
    // configuration API used by MultiBus exposes 1 or 2 stop bits, not 1.5.
    return (settings.dataBits == 7 || settings.dataBits == 8) &&
           settings.stopBits != Rs485StopBits::OnePointFive;
}

} // namespace modbus
} // namespace multibus
