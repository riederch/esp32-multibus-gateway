#include "core/PasswordKdfPolicy.h"

#include <cassert>
#include <cstring>

using multibus::PasswordKdfPolicy;

int main() {
    assert(std::strcmp(PasswordKdfPolicy::kAlgorithm, "pbkdf2-sha256") == 0);
    assert(PasswordKdfPolicy::kCurrentIterations == 120000);
    assert(PasswordKdfPolicy::storedOrLegacyIterations(0) == PasswordKdfPolicy::kLegacyIterations);
    assert(PasswordKdfPolicy::storedOrLegacyIterations(150000) == 150000);

    assert(std::strcmp(PasswordKdfPolicy::storedOrLegacyAlgorithm(nullptr),
                       PasswordKdfPolicy::kAlgorithm) == 0);
    assert(std::strcmp(PasswordKdfPolicy::storedOrLegacyAlgorithm(""),
                       PasswordKdfPolicy::kAlgorithm) == 0);
    assert(std::strcmp(PasswordKdfPolicy::storedOrLegacyAlgorithm("pbkdf2-sha256"),
                       PasswordKdfPolicy::kAlgorithm) == 0);

    assert(PasswordKdfPolicy::isSupported("pbkdf2-sha256", 120000));
    assert(PasswordKdfPolicy::isSupported("pbkdf2-sha256",
                                          PasswordKdfPolicy::kMinAcceptedIterations));
    assert(PasswordKdfPolicy::isSupported("pbkdf2-sha256",
                                          PasswordKdfPolicy::kMaxAcceptedIterations));
    assert(!PasswordKdfPolicy::isSupported("pbkdf2-sha1", 120000));
    assert(!PasswordKdfPolicy::isSupported("pbkdf2-sha256",
                                           PasswordKdfPolicy::kMinAcceptedIterations - 1));
    assert(!PasswordKdfPolicy::isSupported("pbkdf2-sha256",
                                           PasswordKdfPolicy::kMaxAcceptedIterations + 1));

    return 0;
}
