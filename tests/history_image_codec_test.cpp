#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "history/HistoryImageCodec.h"

using multibus::history::HistoryImageCodec;
using multibus::lorawan::HistoricalRecord;
using multibus::lorawan::HistoryRing;

static HistoricalRecord makeRecord(uint32_t timestamp, uint8_t slot) {
    HistoricalRecord record;
    record.payload[0] = 0x21;
    record.payload[1] = 0xce;
    record.payload[2] = static_cast<uint8_t>(timestamp & 0xffU);
    record.payload[3] = static_cast<uint8_t>((timestamp >> 8U) & 0xffU);
    record.payload[4] = static_cast<uint8_t>((timestamp >> 16U) & 0xffU);
    record.payload[5] = static_cast<uint8_t>((timestamp >> 24U) & 0xffU);
    record.payload[6] = slot;
    return record;
}

static void testRoundTripPreservesOrderAndGeneration() {
    HistoryRing<3> ring;
    ring.push(makeRecord(10, 1));
    ring.push(makeRecord(20, 2));
    ring.push(makeRecord(30, 3));
    ring.push(makeRecord(40, 4));

    using Codec = HistoryImageCodec<3>;
    uint8_t image[Codec::kImageLength] = {0};
    size_t written = 0;
    assert(Codec::encode(ring, 17, image, sizeof(image), written));
    assert(written == sizeof(image));

    HistoryRing<3> decoded;
    uint32_t generation = 0;
    assert(Codec::decode(image, sizeof(image), decoded, generation));
    assert(generation == 17);
    assert(decoded.size() == 3);
    assert(decoded.oldest(0)->timestamp() == 20);
    assert(decoded.oldest(1)->timestamp() == 30);
    assert(decoded.oldest(2)->timestamp() == 40);
}

static void testCorruptionIsRejected() {
    HistoryRing<2> ring;
    ring.push(makeRecord(123, 1));

    using Codec = HistoryImageCodec<2>;
    uint8_t image[Codec::kImageLength] = {0};
    size_t written = 0;
    assert(Codec::encode(ring, 2, image, sizeof(image), written));

    image[15] ^= 0x40U;
    HistoryRing<2> decoded;
    uint32_t generation = 0;
    assert(!Codec::decode(image, sizeof(image), decoded, generation));
}

int main() {
    testRoundTripPreservesOrderAndGeneration();
    testCorruptionIsRejected();
    return 0;
}
