#include <assert.h>
#include <string.h>

#include "core/CommandBus.h"

using multibus::ChannelWriteCommand;
using multibus::CommandBus;
using multibus::CommandValueType;

static ChannelWriteCommand makeCommand(uint16_t channel, const char* origin, double value) {
    ChannelWriteCommand command;
    command.channelId = channel;
    command.valueType = CommandValueType::Float64;
    command.floatValue = value;
    assert(multibus::setCommandText(command.origin, sizeof(command.origin), origin));
    return command;
}

static void testFifoAndSequences() {
    CommandBus<3> bus;
    assert(bus.enqueue(makeCommand(1, "mqtt", 1.5)));
    assert(bus.enqueue(makeCommand(2, "lorawan", 2.5)));
    assert(bus.size() == 2);

    ChannelWriteCommand command;
    assert(bus.dequeue(command));
    assert(command.sequence == 1);
    assert(command.channelId == 1);
    assert(strcmp(command.origin, "mqtt") == 0);

    assert(bus.dequeue(command));
    assert(command.sequence == 2);
    assert(command.channelId == 2);
    assert(bus.empty());
}

static void testCapacityIsStrict() {
    CommandBus<2> bus;
    assert(bus.enqueue(makeCommand(1, "mqtt", 1)));
    assert(bus.enqueue(makeCommand(2, "mqtt", 2)));
    assert(bus.full());
    assert(!bus.enqueue(makeCommand(3, "mqtt", 3)));

    ChannelWriteCommand command;
    assert(bus.dequeue(command));
    assert(bus.enqueue(makeCommand(3, "mqtt", 3)));
    assert(bus.dequeue(command));
    assert(command.channelId == 2);
    assert(bus.dequeue(command));
    assert(command.channelId == 3);
}

static void testRejectsInvalidMetadata() {
    CommandBus<2> bus;
    ChannelWriteCommand invalid;
    invalid.channelId = 0;
    assert(multibus::setCommandText(invalid.origin, sizeof(invalid.origin), "mqtt"));
    assert(!bus.enqueue(invalid));

    ChannelWriteCommand missingOrigin;
    missingOrigin.channelId = 1;
    assert(!bus.enqueue(missingOrigin));
}

int main() {
    testFifoAndSequences();
    testCapacityIsStrict();
    testRejectsInvalidMetadata();
    return 0;
}
