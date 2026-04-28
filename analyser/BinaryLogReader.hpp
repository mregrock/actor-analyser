#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <cstring>

static constexpr uint32_t YTRA_MAGIC = 0x41525459;

static constexpr uint32_t YTRA_VERSION_MIN = 4;
static constexpr uint32_t YTRA_VERSION_MAX = 4;

struct BinaryFileHeader {
  uint32_t magic;
  uint32_t version;
  uint32_t nodeId;
  uint32_t headerSize;
  uint64_t eventCount;
  uint64_t startTimestampUs;
};

static_assert(sizeof(BinaryFileHeader) == 32);

struct BinaryEvent {
  uint64_t Sender;        // offset 0
  uint64_t Recipient;     // offset 8
  uint32_t HandleHash;    // offset 16
  uint32_t DeltaUs;       // offset 20
  uint32_t MessageType;   // offset 24
  uint16_t ActivityIndex; // offset 28
  uint8_t  Type;          // offset 30
  uint8_t  ThreadIdx;     // offset 31
};

static_assert(sizeof(BinaryEvent) == 32);

static constexpr size_t kBinaryEventSizeV4 = 32;

enum BinaryEventType : uint8_t {
  SendLocal    = 0,
  ReceiveLocal = 1,
  New          = 2,
  Die          = 3,
  ForwardLocal = 4,
};

class BinaryLogReader {
public:
  static bool IsBinaryFormat(const std::vector<uint8_t>& data) {
    if (data.size() < 4) return false;
    uint32_t magic;
    std::memcpy(&magic, data.data(), sizeof(magic));
    return magic == YTRA_MAGIC;
  }

  static void Parse(const std::vector<uint8_t>& data) {
    if (data.size() < sizeof(BinaryFileHeader)) {
      throw std::runtime_error("BinaryLogReader: file too small for header");
    }

    std::memcpy(&header_, data.data(), sizeof(BinaryFileHeader));
    if (header_.magic != YTRA_MAGIC) {
      throw std::runtime_error("BinaryLogReader: bad magic");
    }

    if (header_.version < YTRA_VERSION_MIN || header_.version > YTRA_VERSION_MAX) {
      std::ostringstream oss;
      oss << "BinaryLogReader: unsupported trace version " << header_.version
          << " (supported: " << YTRA_VERSION_MIN;
      if (YTRA_VERSION_MIN != YTRA_VERSION_MAX) {
        oss << ".." << YTRA_VERSION_MAX;
      }
      oss << "). Legacy v1/v2/v3 traces are no longer supported.";
      throw std::runtime_error(oss.str());
    }

    size_t offset = sizeof(BinaryFileHeader);
    size_t dictEnd = offset + header_.headerSize;
    if (dictEnd > data.size()) {
      throw std::runtime_error("BinaryLogReader: truncated dictionaries section");
    }

    activityDict_.clear();
    eventNamesDict_.clear();
    threadPoolDict_.clear();
    events_.clear();
    stringStorage_.clear();

    ParseActivityDict(data, offset);
    ParseEventNamesDict(data, offset);
    if (offset < dictEnd) {
      ParseThreadPoolDict(data, offset);
    }

    size_t eventsOffset = sizeof(BinaryFileHeader) + header_.headerSize;
    size_t eventsEnd = eventsOffset + header_.eventCount * kBinaryEventSizeV4;
    if (eventsEnd > data.size()) {
      throw std::runtime_error("BinaryLogReader: truncated events section");
    }

    events_.assign(header_.eventCount, BinaryEvent{});
    std::memcpy(events_.data(), data.data() + eventsOffset,
                header_.eventCount * kBinaryEventSizeV4);
  }

  static const BinaryFileHeader& GetHeader() { return header_; }
  static const std::vector<BinaryEvent>& GetEvents() { return events_; }
  static const std::map<uint32_t, std::string>& GetActivityDict() { return activityDict_; }
  static const std::map<uint32_t, std::string>& GetEventNamesDict() { return eventNamesDict_; }
  static const std::map<uint32_t, std::string>& GetThreadPoolDict() { return threadPoolDict_; }

  static uint64_t AbsTimestampUs(const BinaryEvent& ev) {
    return header_.startTimestampUs + static_cast<uint64_t>(ev.DeltaUs);
  }

  static std::string ActorIdToHex(uint64_t id) {
    std::ostringstream oss;
    oss << "0x" << std::hex << id;
    return oss.str();
  }

private:
  static uint32_t ReadU32(const std::vector<uint8_t>& data, size_t& offset) {
    if (offset + 4 > data.size()) {
      throw std::runtime_error("BinaryLogReader: unexpected end of data reading u32");
    }
    uint32_t val;
    std::memcpy(&val, data.data() + offset, sizeof(val));
    offset += 4;
    return val;
  }

  static std::string ReadString(const std::vector<uint8_t>& data, size_t& offset) {
    uint32_t len = ReadU32(data, offset);
    if (offset + len > data.size()) {
      throw std::runtime_error("BinaryLogReader: unexpected end of data reading string");
    }
    std::string s(reinterpret_cast<const char*>(data.data() + offset), len);
    offset += len;
    return s;
  }

  static void ParseActivityDict(const std::vector<uint8_t>& data, size_t& offset) {
    uint32_t count = ReadU32(data, offset);
    for (uint32_t i = 0; i < count; ++i) {
      uint32_t index = ReadU32(data, offset);
      std::string name = ReadString(data, offset);
      activityDict_[index] = std::move(name);
    }
  }

  static void ParseEventNamesDict(const std::vector<uint8_t>& data, size_t& offset) {
    uint32_t count = ReadU32(data, offset);
    for (uint32_t i = 0; i < count; ++i) {
      uint32_t typeId = ReadU32(data, offset);
      std::string name = ReadString(data, offset);
      eventNamesDict_[typeId] = std::move(name);
    }
  }

  static void ParseThreadPoolDict(const std::vector<uint8_t>& data, size_t& offset) {
    uint32_t count = ReadU32(data, offset);
    for (uint32_t i = 0; i < count; ++i) {
      uint32_t threadIdx = ReadU32(data, offset);
      std::string name = ReadString(data, offset);
      threadPoolDict_[threadIdx] = std::move(name);
    }
  }

  static BinaryFileHeader header_;
  static std::vector<BinaryEvent> events_;
  static std::map<uint32_t, std::string> activityDict_;
  static std::map<uint32_t, std::string> eventNamesDict_;
  static std::map<uint32_t, std::string> threadPoolDict_;
  static std::vector<std::string> stringStorage_;
};
