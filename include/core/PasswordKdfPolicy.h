#pragma once

#include <cstdint>
#include <cstring>

namespace multibus {

struct PasswordKdfPolicy {
    static constexpr const char* kAlgorithm = "pbkdf2-sha256";
    static constexpr uint32_t kCurrentIterations = 120000;
    static constexpr uint32_t kLegacyIterations = 120000;
    static constexpr uint32_t kMinAcceptedIterations = 10000;
    static constexpr uint32_t kMaxAcceptedIterations = 500000;

    static bool isSupported(const char* algorithm, uint32_t iterations) {
        return algorithm != nullptr &&
               std::strcmp(algorithm, kAlgorithm) == 0 &&
               iterations >= kMinAcceptedIterations &&
               iterations <= kMaxAcceptedIterations;
    }

    static uint32_t storedOrLegacyIterations(uint32_t storedIterations) {
        return storedIterations == 0 ? kLegacyIterations : storedIterations;
    }

    static const char* storedOrLegacyAlgorithm(const char* storedAlgorithm) {
        return (storedAlgorithm == nullptr || storedAlgorithm[0] == '\0')
                   ? kAlgorithm
                   : storedAlgorithm;
    }
};

} // namespace multibus
