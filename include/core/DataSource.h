#pragma once

#include <Arduino.h>
#include <stdint.h>

namespace multibus {

enum class DataType : uint8_t {
    Boolean,
    Int64,
    UInt64,
    Float64,
    Text,
};

struct DataValue {
    DataType type = DataType::Float64;
    bool valid = false;
    bool booleanValue = false;
    int64_t intValue = 0;
    uint64_t uintValue = 0;
    double floatValue = 0.0;
    String textValue;
};

struct DataPointDescriptor {
    String id;
    String unit;
    DataType type = DataType::Float64;
    bool readable = true;
    bool writable = false;
};

class DataSource {
public:
    virtual ~DataSource() = default;
    virtual const char* sourceId() const = 0;
    virtual bool online() const = 0;
    virtual size_t pointCount() const = 0;
    virtual bool describePoint(size_t index, DataPointDescriptor& descriptor) const = 0;
    virtual bool readPoint(const String& pointId, DataValue& value) = 0;
    virtual bool writePoint(const String& pointId, const DataValue& value) = 0;
};

struct ChannelBinding {
    uint16_t channelId = 0;
    String sourceId;
    String pointId;
    String unit;
    double scale = 1.0;
    double offset = 0.0;
    bool enabled = true;
    bool writable = false;
};

} // namespace multibus
