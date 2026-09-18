#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "modbus/RtuPassiveFrameCollector.h"

using multibus::modbus::PassiveFrameResult;
using multibus::modbus::RtuPassiveFrameCollector;

static void testCompletesAfterIdleGap() {
    RtuPassiveFrameCollector<8> collector;
    assert(collector.feed(0x01, 1000));
    assert(collector.feed(0x03, 1100));
    assert(collector.feed(0x02, 1200));

    uint8_t output[8] = {0};
    size_t written = 0;
    assert(collector.poll(2000, 1000, output, sizeof(output), written) ==
           PassiveFrameResult::Pending);
    assert(written == 0);

    assert(collector.poll(2200, 1000, output, sizeof(output), written) ==
           PassiveFrameResult::FrameReady);
    const uint8_t expected[] = {0x01, 0x03, 0x02};
    assert(written == sizeof(expected));
    assert(memcmp(output, expected, sizeof(expected)) == 0);
    assert(!collector.pending());
}

static void testOverflowDropsWholeFrame() {
    RtuPassiveFrameCollector<2> collector;
    assert(collector.feed(0xaa, 100));
    assert(collector.feed(0xbb, 200));
    assert(!collector.feed(0xcc, 300));

    uint8_t output[4] = {0};
    size_t written = 0;
    assert(collector.poll(2000, 1000, output, sizeof(output), written) ==
           PassiveFrameResult::Overflow);
    assert(written == 0);
    assert(!collector.pending());
}

static void testOutputTooSmallDropsFrame() {
    RtuPassiveFrameCollector<4> collector;
    assert(collector.feed(0x10, 100));
    assert(collector.feed(0x20, 200));

    uint8_t output[1] = {0};
    size_t written = 0;
    assert(collector.poll(2000, 1000, output, sizeof(output), written) ==
           PassiveFrameResult::Overflow);
    assert(written == 0);
    assert(!collector.pending());
}

int main() {
    testCompletesAfterIdleGap();
    testOverflowDropsWholeFrame();
    testOutputTooSmallDropsFrame();
    return 0;
}
