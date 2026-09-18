#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "core/EventBus.h"

using multibus::EventBus;
using multibus::EventRecord;
using multibus::EventSeverity;

static void testIndependentConsumers() {
    EventBus<4> bus;
    assert(bus.emit("modbus", "poll-failure", EventSeverity::Warning, 100, "slot=1"));
    assert(bus.emit("rules", "trigger", EventSeverity::Info, 101, "rule=2"));

    uint64_t cursorA = 0;
    uint64_t cursorB = 0;
    EventRecord event;

    assert(bus.next(cursorA, event));
    assert(event.sequence == 1);
    assert(strcmp(event.source, "modbus") == 0);

    assert(bus.next(cursorA, event));
    assert(event.sequence == 2);

    assert(bus.next(cursorB, event));
    assert(event.sequence == 1);
    assert(bus.next(cursorB, event));
    assert(event.sequence == 2);
    assert(!bus.next(cursorB, event));
}

static void testOverwriteReportsMissedEvents() {
    EventBus<2> bus;
    assert(bus.emit("a", "one", EventSeverity::Info, 1));
    assert(bus.emit("a", "two", EventSeverity::Info, 2));
    assert(bus.emit("a", "three", EventSeverity::Error, 3));

    assert(bus.size() == 2);
    assert(bus.oldestSequence() == 2);
    assert(bus.latestSequence() == 3);
    assert(bus.missedSince(0) == 1);

    uint64_t cursor = 0;
    EventRecord event;
    assert(bus.next(cursor, event));
    assert(event.sequence == 2);
    assert(strcmp(event.type, "two") == 0);
    assert(bus.next(cursor, event));
    assert(event.sequence == 3);
    assert(event.severity == EventSeverity::Error);
}

static void testRejectsOversizedMetadata() {
    EventBus<2> bus;
    char source[40];
    memset(source, 'x', sizeof(source));
    source[sizeof(source) - 1] = '\0';
    assert(!bus.emit(source, "type", EventSeverity::Info, 1));
    assert(bus.size() == 0);
}

int main() {
    testIndependentConsumers();
    testOverwriteReportsMissedEvents();
    testRejectsOversizedMetadata();
    return 0;
}
