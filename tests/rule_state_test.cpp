#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "rules/RuleState.h"

using multibus::rules::RuleState;

static void testMaskOperations() {
    RuleState state;
    assert(state.applyMask(0x8001, 1));
    assert(state.rule(1)->enabled);
    assert(state.rule(16)->enabled);

    assert(state.applyMask(0x0001, 2));
    assert(!state.rule(1)->enabled);
    assert(state.rule(16)->enabled);
}

static void testDeleteClearsFrames() {
    RuleState state;
    auto* rule = state.rule(2);
    assert(rule != nullptr);
    const uint8_t frame[] = {0xf9, 0x7d, 0x82, 0x16};
    assert(rule->frames[0].set(frame, sizeof(frame)));
    rule->enabled = true;
    assert(rule->configured());

    assert(state.applyMask(0x0002, 3));
    assert(!rule->enabled);
    assert(!rule->configured());
}

int main() {
    testMaskOperations();
    testDeleteClearsFrames();
    return 0;
}
