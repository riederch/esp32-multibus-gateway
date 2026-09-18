#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "modbus/PassiveFrameAssembler.h"

using multibus::modbus::PassiveFrameAssembler;
using multibus::modbus::PassiveFrameResult;

static void testFrameCompletesAfterIdleGap() {
    PassiveFrameAssembler<8> assembler;
    assert(assembler.append(0x01, 1000));
    assert(assembler.append(0x03, 1100));
    assert(assembler.append(0xaa, 1200));

    uint8_t output[8] = {0};
    size_t written = 0;
    assert(assembler.poll(2000, 1000, output, sizeof(output), written) ==
           PassiveFrameResult::Pending);
    assert(assembler.poll(2200, 1000, output, sizeof(output), written) ==
           PassiveFrameResult::Ready);
    const uint8_t expected[] = {0x01, 0x03, 0xaa};
    assert(written == sizeof(expected));
    assert(memcmp(output, expected, sizeof(expected)) == 0);
    assert(!assembler.pending());
}

static void testOverflowIsReportedAndCleared() {
    PassiveFrameAssembler<2> assembler;
    assert(assembler.append(1, 100));
    assert(assembler.append(2, 200));
    assert(!assembler.append(3, 300));

    uint8_t output[2] = {0};
    size_t written = 0;
    assert(assembler.poll(1000, 100, output, sizeof(output), written) ==
           PassiveFrameResult::Overflow);
    assert(!assembler.pending());
}

int main() {
    testFrameCompletesAfterIdleGap();
    testOverflowIsReportedAndCleared();
    return 0;
}
