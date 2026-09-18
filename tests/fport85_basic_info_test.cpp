#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "lorawan/FPort85BasicInfo.h"

using multibus::lorawan::BasicInfo;
using multibus::lorawan::EncodeStatus;
using multibus::lorawan::FPort85BasicInfo;

static void testReferenceVector() {
    BasicInfo info;
    const uint8_t serial[] = {0x64, 0x45, 0xb4, 0x34, 0x11, 0x30, 0x00, 0x01};
    memcpy(info.serialNumber, serial, sizeof(serial));
    info.protocolVersion = 0x01;
    info.tslMajor = 0x02;
    info.tslMinor = 0x01;
    info.hardwareMajor = 0x02;
    info.hardwareMinor = 0x00;
    info.softwareMajor = 0x01;
    info.softwareMinor = 0x01;
    info.deviceType = 0x02;

    uint8_t encoded[40] = {0};
    size_t written = 0;
    assert(FPort85BasicInfo::encode(
        info, false, encoded, sizeof(encoded), written) == EncodeStatus::Ok);

    const uint8_t expected[] = {
        0xff, 0x0b, 0xff,
        0xff, 0x01, 0x01,
        0xff, 0xff, 0x02, 0x01,
        0xff, 0x16, 0x64, 0x45, 0xb4, 0x34, 0x11, 0x30, 0x00, 0x01,
        0xff, 0x09, 0x02, 0x00,
        0xff, 0x0a, 0x01, 0x01,
        0xff, 0x0f, 0x02
    };
    assert(written == sizeof(expected));
    assert(memcmp(encoded, expected, sizeof(expected)) == 0);
}

static void testResetEventAppended() {
    BasicInfo info;
    uint8_t encoded[40] = {0};
    size_t written = 0;
    assert(FPort85BasicInfo::encode(
        info, true, encoded, sizeof(encoded), written) == EncodeStatus::Ok);
    assert(written == FPort85BasicInfo::kBasicInfoWithResetLength);
    assert(encoded[written - 3] == 0xff);
    assert(encoded[written - 2] == 0xfe);
    assert(encoded[written - 1] == 0xff);
}

int main() {
    testReferenceVector();
    testResetEventAppended();
    return 0;
}
