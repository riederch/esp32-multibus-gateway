#include <assert.h>
#include <string.h>

#include "mqtt/MqttProtocol.h"

using multibus::mqtt::CommandTopic;
using multibus::mqtt::makeAvailabilityTopic;
using multibus::mqtt::makeChannelCommandTopic;
using multibus::mqtt::makeChannelCommandWildcard;
using multibus::mqtt::makeChannelStateTopic;
using multibus::mqtt::parseChannelCommandTopic;
using multibus::mqtt::validTopicPrefix;

static void testTopics() {
    char topic[128] = {0};

    assert(makeChannelStateTopic("multibus", "mb-a1b2c3", 42, topic, sizeof(topic)));
    assert(strcmp(topic, "multibus/mb-a1b2c3/channels/42/state") == 0);

    assert(makeChannelCommandTopic("site/gateway", "gw_01", 7, topic, sizeof(topic)));
    assert(strcmp(topic, "site/gateway/gw_01/channels/7/set") == 0);

    assert(makeChannelCommandWildcard("multibus", "gw01", topic, sizeof(topic)));
    assert(strcmp(topic, "multibus/gw01/channels/+/set") == 0);

    assert(makeAvailabilityTopic("multibus", "gw01", topic, sizeof(topic)));
    assert(strcmp(topic, "multibus/gw01/status") == 0);
}

static void testCommandParsing() {
    const CommandTopic parsed = parseChannelCommandTopic(
        "multibus", "gw01", "multibus/gw01/channels/65535/set");
    assert(parsed.valid);
    assert(parsed.channelId == 65535);

    assert(!parseChannelCommandTopic(
        "multibus", "gw01", "multibus/gw02/channels/1/set").valid);
    assert(!parseChannelCommandTopic(
        "multibus", "gw01", "multibus/gw01/channels/0/set").valid);
    assert(!parseChannelCommandTopic(
        "multibus", "gw01", "multibus/gw01/channels/1/state").valid);
}

static void testPrefixValidation() {
    assert(validTopicPrefix("multibus"));
    assert(validTopicPrefix("site/gateway"));
    assert(!validTopicPrefix(""));
    assert(!validTopicPrefix("/multibus"));
    assert(!validTopicPrefix("multibus/"));
    assert(!validTopicPrefix("multi//bus"));
    assert(!validTopicPrefix("multi+bus"));
    assert(!validTopicPrefix("multi#bus"));
    assert(!validTopicPrefix("multi bus"));
}

int main() {
    testTopics();
    testCommandParsing();
    testPrefixValidation();
    return 0;
}
