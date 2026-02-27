#pragma once

#include <cstdint>
#include "LogReader.hpp"
#include "BinaryLogReader.hpp"
#include "engine/arctic_platform_fatal.h"

#include <sstream>
#include <stdexcept>
#include <string_view>
#include <chrono>
#include <iomanip>
#include <string>
#include <algorithm>
#include <set>
#include <queue>
#include <deque>
#include <fstream>

#include <iostream>
#include <unordered_map>
#include <vector>

#include "globals.h"

using namespace arctic;

class Logs {
public:
  
  struct RealLogLine {
    std::string_view type;
    std::string_view to;
    std::string_view from;
    std::string_view message;
    std::string_view time;
    std::optional<std::string_view> actorType;
    std::optional<std::string_view> messageType;
    std::optional<std::string_view> threadId;
    std::optional<std::string_view> threadName;
    
    RealLogLine() = default;
    RealLogLine(const RealLogLine&) = default;
    ~RealLogLine() = default;
    RealLogLine& operator=(const RealLogLine&) = default;
    
    bool operator==(const RealLogLine&) const = default;
    
    RealLogLine(std::string_view type,
                std::string_view to,
                std::string_view from,
                std::string_view message,
                std::string_view time) :
    type(type), to(to), from(from), message(message), time(time) {}
    
    RealLogLine(std::string_view type,
                std::string_view to,
                std::string_view from,
                std::string_view message,
                std::string_view time,
                std::string_view argActorType) :
    RealLogLine(type, to, from, message, time)
    { actorType = argActorType;}
  };
  
  struct NewDieLogLine {
    std::string_view type;
    std::string_view id;
    std::string_view time;
    std::string_view threadId;
    
    NewDieLogLine() = default;
    NewDieLogLine(const NewDieLogLine&) = default;
    ~NewDieLogLine() = default;
    NewDieLogLine& operator=(const NewDieLogLine&) = default;
    
    bool operator==(const NewDieLogLine&) const = default;    
  };
  
  struct ParsedLogLine {
    std::string_view type;
    ActorIdx to;
    ActorIdx from;
    std::string_view message;
    VisualisationTime time;
    std::optional<std::string_view> actorType;
    std::optional<std::string_view> messageType;
    std::string_view threadId;
    std::string_view threadName;
    
    // FK for the message array
    Si64 message_idx = -1;
    
    ParsedLogLine() = default;
    ParsedLogLine(const ParsedLogLine&) = default;
    ~ParsedLogLine() = default;
    ParsedLogLine& operator=(const ParsedLogLine&) = default;
    
    bool operator==(const ParsedLogLine&) const = default;
    
    ParsedLogLine(std::string_view type,
                  ActorIdx to,
                  ActorIdx from,
                  std::string_view message,
                  VisualisationTime time) :
    type(type), to(to), from(from), message(message), time(time) {}
    
    ParsedLogLine(std::string_view type,
                  ActorIdx to,
                  ActorIdx from,
                  std::string_view message,
                  VisualisationTime time,
                  std::string_view argActorType) :
    ParsedLogLine(type, to, from, message, time)
    {actorType = argActorType;}
  };
  
  struct LogMessage {
    ActorIdx to;
    ActorIdx from;
    VisualisationTime start;
    VisualisationTime end;
    std::string_view message;
    std::string_view messageType;
    size_t message_idx;
    
    // FKs to child messages
    std::vector<Si64> child_msg_idxs;
    
    LogMessage() = default;
    LogMessage(const LogMessage&) = default;
    ~LogMessage() = default;
    LogMessage& operator=(const LogMessage&) = default;
    
    bool operator==(const LogMessage&) const = default;
    auto operator<=>(const LogMessage&) const = default;
    
    LogMessage(ActorIdx to, ActorIdx from, VisualisationTime start, VisualisationTime end, std::string_view message, size_t id) :
    to(to), from(from), start(start), end(end), message(message), message_idx(id) {}
  };
  
  static void CompressTimelineGaps() {
    if (logMessages_.size() < 2) return;

    std::vector<VisualisationTime> starts;
    starts.reserve(logMessages_.size());
    for (auto& m : logMessages_) starts.push_back(m.start);
    std::sort(starts.begin(), starts.end());

    VisualisationTime maxGap = 0;
    size_t gapIdx = 0;
    for (size_t i = 1; i < starts.size(); ++i) {
      VisualisationTime gap = starts[i] - starts[i - 1];
      if (gap > maxGap) {
        maxGap = gap;
        gapIdx = i;
      }
    }

    VisualisationTime totalRange = starts.back() - starts.front();
    if (totalRange == 0 || maxGap <= totalRange / 2) return;

    VisualisationTime clusterStart = starts[gapIdx];
    VisualisationTime buffer = 1000000;
    VisualisationTime shift = (clusterStart > buffer) ? clusterStart - buffer : 0;
    if (shift == 0) return;

    {
      std::ofstream dbg("/tmp/actor_debug_log.txt", std::ios::app);
      dbg << std::endl << "=== COMPRESS GAPS ===" << std::endl;
      dbg << "Biggest gap: " << maxGap << " us at index " << gapIdx << std::endl;
      dbg << "Total range: " << totalRange << " us" << std::endl;
      dbg << "Cluster starts at: " << clusterStart << ", shift=" << shift << std::endl;
      dbg << "Range before: 0 .. " << GetMaxTime() << std::endl;
      dbg.close();
    }

    for (auto& m : logMessages_) {
      m.start = std::max((VisualisationTime)0, m.start - shift);
      m.end = std::max((VisualisationTime)0, m.end - shift);
    }

    for (auto& [id, lt] : lifeTime_) {
      lt.first = std::max((VisualisationTime)0, lt.first - shift);
      lt.second = std::max((VisualisationTime)0, lt.second - shift);
    }

    {
      std::ofstream dbg("/tmp/actor_debug_log.txt", std::ios::app);
      dbg << "Range after: 0 .. " << GetMaxTime() << std::endl;
      dbg.close();
    }
  }

  static void ReadLogs(const std::string_view file) {
    LogReader::ReadFile_(file, &rawFileData_);

    if (BinaryLogReader::IsBinaryFormat(rawFileData_)) {
      ReadLogsBinary();
    } else {
      ReadLogsText(file);
    }
  }

  static void ReadLogsText(const std::string_view file) {
    LogReader::ReadFile(file);
    logLines_ = LogReader::GetLogLines();

    LogReader::ReadFile_("data/actor_types_map.config", &actorTypesMapConfigData_);
    SetNewActorTypes(actorTypesMapConfigData_);

    CreateRealLogLines();
    ParseRealLogLines();
    SetMessageToParsedLineInd();

    CreateLogMessages();
    NormalizeLogMessagesTime();

    CreateActorTypeToActorId();
    CreateActorIdToActorType();

    GetActorLifeInfo();
    SetActorLifeTime();

    SetActorThreadActive();
  }

  static void ReadLogsBinary() {
    LogReader::ReadFile_("data/actor_types_map.config", &actorTypesMapConfigData_);
    SetNewActorTypes(actorTypesMapConfigData_);

    BinaryLogReader::Parse(rawFileData_);

    const auto& events = BinaryLogReader::GetEvents();
    const auto& activityDict = BinaryLogReader::GetActivityDict();
    const auto& eventNamesDict = BinaryLogReader::GetEventNamesDict();

    binStrings_.clear();

    for (auto& [idx, name] : activityDict) {
      binStrings_.push_back(name);
    }
    for (auto& [idx, name] : eventNamesDict) {
      binStrings_.push_back(name);
    }

    ActorIdx curUnusedActorId = 0;

    auto getOrCreateActorIdx = [&](uint64_t rawId) -> ActorIdx {
      std::string hexStr = BinaryLogReader::ActorIdToHex(rawId);
      auto it = binActorIdMap_.find(rawId);
      if (it != binActorIdMap_.end()) {
        return it->second;
      }
      ActorIdx idx = curUnusedActorId++;
      binActorIdMap_[rawId] = idx;
      binStrings_.push_back(hexStr);
      std::string_view sv = binStrings_.back();
      actorNumIdToRealActorId_[idx] = sv;
      realActorIdToActorNumId_[sv] = idx;
      return idx;
    };

    static const std::string sendStr = "Send";
    static const std::string receiveStr = "Receive";
    static const std::string newStr = "New";
    static const std::string dieStr = "Die";
    static const std::string emptyStr = "";

    parsedLogLines_.reserve(events.size());

    for (size_t i = 0; i < events.size(); ++i) {
      const BinaryEvent& ev = events[i];

      if (ev.type == BinaryEventType::SendLocal || ev.type == BinaryEventType::ReceiveLocal) {
        ParsedLogLine pll;
        pll.type = (ev.type == BinaryEventType::SendLocal) ? std::string_view(sendStr) : std::string_view(receiveStr);

        ActorIdx fromIdx = getOrCreateActorIdx(ev.actor1);
        ActorIdx toIdx = getOrCreateActorIdx(ev.actor2);
        pll.from = fromIdx;
        pll.to = toIdx;

        pll.time = static_cast<VisualisationTime>(ev.timestamp);

        pll.message = emptyStr;

        if (ev.type == BinaryEventType::ReceiveLocal) {
          auto actIt = activityDict.find(ev.extra);
          if (actIt != activityDict.end()) {
            pll.actorType = std::string_view(actIt->second);
          }
        }

        auto msgIt = eventNamesDict.find(ev.aux);
        if (msgIt != eventNamesDict.end()) {
          pll.messageType = std::string_view(msgIt->second);
        }

        pll.threadId = emptyStr;
        pll.threadName = emptyStr;

        parsedLogLines_.push_back(pll);

      } else if (ev.type == BinaryEventType::New || ev.type == BinaryEventType::Die) {
        ActorIdx actorIdx = getOrCreateActorIdx(ev.actor1);

        NewDieLogLine ndl;
        ndl.type = (ev.type == BinaryEventType::New) ? std::string_view(newStr) : std::string_view(dieStr);
        ndl.id = actorNumIdToRealActorId_[actorIdx];

        binStrings_.push_back(std::to_string(ev.timestamp));
        ndl.time = binStrings_.back();
        ndl.threadId = emptyStr;

        newDieLogLines_.push_back(ndl);
      }
    }

    maxActorId_ = curUnusedActorId;

    std::sort(parsedLogLines_.begin(), parsedLogLines_.end(),
              [](const ParsedLogLine& a, const ParsedLogLine& b) {
                return a.time < b.time;
              });

    CreateLogMessagesBinary();

    {
      std::ofstream dbg("/tmp/actor_debug_log.txt");
      dbg << "=== BINARY LOAD DEBUG ===" << std::endl;
      dbg << "Total binary events: " << events.size() << std::endl;

      size_t sendCount = 0, recvCount = 0;
      for (auto& pll : parsedLogLines_) {
        if (pll.type == "Send") sendCount++;
        else if (pll.type == "Receive") recvCount++;
      }
      dbg << "ParsedLogLines: " << parsedLogLines_.size()
          << " (Send=" << sendCount << ", Receive=" << recvCount << ")" << std::endl;
      dbg << "NewDieLogLines: " << newDieLogLines_.size() << std::endl;
      dbg << "LogMessages (correlated): " << logMessages_.size() << std::endl;
      dbg << "Unique actors: " << maxActorId_ << std::endl;

      if (!logMessages_.empty()) {
        VisualisationTime minStart = logMessages_[0].start, maxEnd = logMessages_[0].end;
        for (auto& m : logMessages_) {
          minStart = std::min(minStart, m.start);
          maxEnd = std::max(maxEnd, m.end);
        }
        dbg << "Message time range (before normalization): "
            << minStart << " .. " << maxEnd << std::endl;

        size_t withCoordCount = 0;
        for (auto& m : logMessages_) {
          bool fromOk = actorIdToActorType_.count(m.from) || actorNumIdToRealActorId_.count(m.from);
          bool toOk = actorIdToActorType_.count(m.to) || actorNumIdToRealActorId_.count(m.to);
          if (fromOk && toOk) withCoordCount++;
        }
        dbg << "Messages where both actors have type info: " << withCoordCount << std::endl;
      } else {
        dbg << "NO MESSAGES CORRELATED!" << std::endl;
      }

      if (!parsedLogLines_.empty()) {
        dbg << "First 10 parsed lines:" << std::endl;
        for (size_t i = 0; i < std::min((size_t)10, parsedLogLines_.size()); ++i) {
          auto& p = parsedLogLines_[i];
          dbg << "  [" << i << "] type=" << p.type
              << " from=" << p.from << " to=" << p.to
              << " time=" << p.time
              << " msg=" << p.message << std::endl;
        }
        dbg << "Last 10 parsed lines:" << std::endl;
        size_t startIdx = parsedLogLines_.size() > 10 ? parsedLogLines_.size() - 10 : 0;
        for (size_t i = startIdx; i < parsedLogLines_.size(); ++i) {
          auto& p = parsedLogLines_[i];
          dbg << "  [" << i << "] type=" << p.type
              << " from=" << p.from << " to=" << p.to
              << " time=" << p.time
              << " msg=" << p.message << std::endl;
        }
      }

      if (!logMessages_.empty()) {
        dbg << "First 10 correlated messages:" << std::endl;
        for (size_t i = 0; i < std::min((size_t)10, logMessages_.size()); ++i) {
          auto& m = logMessages_[i];
          dbg << "  [" << i << "] from=" << m.from << " to=" << m.to
              << " start=" << m.start << " end=" << m.end
              << " msg=" << m.message
              << " msgType=" << m.messageType << std::endl;
        }
      }
      dbg.close();
    }

    NormalizeLogMessagesTime();

    {
      std::ofstream dbg("/tmp/actor_debug_log.txt", std::ios::app);
      dbg << std::endl << "=== AFTER NORMALIZATION ===" << std::endl;
      dbg << "oldMinTime_: " << oldMinTime_ << std::endl;
      if (!logMessages_.empty()) {
        VisualisationTime minStart = logMessages_[0].start, maxEnd = logMessages_[0].end;
        for (auto& m : logMessages_) {
          minStart = std::min(minStart, m.start);
          maxEnd = std::max(maxEnd, m.end);
        }
        dbg << "Normalized message time range: " << minStart << " .. " << maxEnd << std::endl;
        dbg << "MaxTime: " << GetMaxTime() << std::endl;
      }
      dbg.close();
    }

    CreateActorTypeToActorIdBinary();
    CreateActorIdToActorTypeBinary();

    for (auto& [rawId, actorIdx] : binActorIdMap_) {
      if (!actorIdToActorType_.count(actorIdx)) {
        binStrings_.push_back("SYSTEM");
        std::string_view sv = binStrings_.back();
        actorIdToActorType_[actorIdx] = sv;
        actorTypeToActorId_[sv].push_back(actorIdx);
      }
    }

    SetActorLifeTimeBinary();
    CompressTimelineGaps();

    {
      std::ofstream dbg("/tmp/actor_debug_log.txt", std::ios::app);
      dbg << std::endl << "=== ACTOR TYPES ===" << std::endl;
      dbg << "actorTypeToActorId_ entries: " << actorTypeToActorId_.size() << std::endl;
      dbg << "actorIdToActorType_ entries: " << actorIdToActorType_.size() << std::endl;
      dbg << "actorTypesMap_ entries: " << actorTypesMap_.size() << std::endl;
      for (auto& [type, ids] : actorTypeToActorId_) {
        dbg << "  type='" << type << "' actors=" << ids.size() << std::endl;
      }
      dbg.close();
    }
  }

  static void CreateLogMessagesBinary() {
    using PairKey = std::pair<ActorIdx, ActorIdx>;
    std::map<PairKey, std::queue<size_t>> sendQueues;

    for (size_t i = 0; i < parsedLogLines_.size(); ++i) {
      ParsedLogLine& pll = parsedLogLines_[i];
      PairKey key = {pll.from, pll.to};

      if (pll.type == "Send") {
        sendQueues[key].push(i);
      } else if (pll.type == "Receive") {
        PairKey senderKey = {pll.from, pll.to};
        auto it = sendQueues.find(senderKey);
        if (it != sendQueues.end() && !it->second.empty()) {
          size_t sendIdx = it->second.front();
          it->second.pop();

          ParsedLogLine& sendLine = parsedLogLines_[sendIdx];

          LogMessage msg;
          msg.from = sendLine.from;
          msg.to = sendLine.to;
          msg.start = sendLine.time;
          msg.end = pll.time;
          msg.message = sendLine.message;
          static const std::string emptyMsgType;
          msg.messageType = pll.messageType.value_or(std::string_view(emptyMsgType));
          msg.message_idx = logMessages_.size();

          sendLine.message_idx = msg.message_idx;
          pll.message_idx = msg.message_idx;

          logMessages_.push_back(msg);
        }
      }
    }
  }

  static void CreateActorTypeToActorIdBinary() {
    const auto& activityDict = BinaryLogReader::GetActivityDict();
    std::set<ActorIdx> added;

    for (const ParsedLogLine& pll : parsedLogLines_) {
      if (!pll.actorType) continue;
      if (added.count(pll.to)) continue;

      std::string_view actorType = *pll.actorType;
      if (actorTypesMap_.count(actorType)) {
        actorType = actorTypesMap_[actorType];
      }
      actorTypeToActorId_[actorType].push_back(pll.to);
      added.insert(pll.to);
    }
  }

  static void CreateActorIdToActorTypeBinary() {
    for (const ParsedLogLine& pll : parsedLogLines_) {
      if (!pll.actorType) continue;

      std::string_view actorType = *pll.actorType;
      if (actorTypesMap_.count(actorType)) {
        actorType = actorTypesMap_[actorType];
      }
      actorIdToActorType_[pll.to] = actorType;
    }
  }

  static void SetActorLifeTimeBinary() {
    std::map<ActorIdx, VisualisationTime> newActors;
    std::map<ActorIdx, VisualisationTime> dieActors;

    for (const NewDieLogLine& ndl : newDieLogLines_) {
      if (!realActorIdToActorNumId_.count(ndl.id)) continue;
      ActorIdx actorId = realActorIdToActorNumId_.at(ndl.id);
      VisualisationTime t = std::stoll(std::string(ndl.time));

      if (std::string(ndl.type) == "New") {
        newActors[actorId] = t;
      } else if (std::string(ndl.type) == "Die") {
        dieActors[actorId] = t;
      }
    }

    NormalizeLifeTimesFromMicroseconds(newActors, dieActors);
  }

  static void NormalizeLifeTimesFromMicroseconds(
      std::map<ActorIdx, VisualisationTime>& newActors,
      std::map<ActorIdx, VisualisationTime>& dieActors) {
    VisualisationTime maxTime = GetMaxTime();

    for (auto& [id, t] : newActors) {
      t = (t < oldMinTime_) ? 0 : t - oldMinTime_;
    }
    for (auto& [id, t] : dieActors) {
      t = (t < oldMinTime_) ? 0 : t - oldMinTime_;
    }

    for (ActorIdx id = 0; id <= maxActorId_; ++id) {
      lifeTime_[id].first = newActors.count(id) ? newActors[id] : 0;
      lifeTime_[id].second = dieActors.count(id) ? dieActors[id] : maxTime;
    }
  }
  
  static bool IsTablet(ActorIdx id) {
    return tabletIds_.count(id);
  }
  
  static Si64 GetMessageProcessingEnd(ActorIdx id, VisualisationTime time) {
    if (!actorActivityTime_.count(id)) {
      return -1;
    }
    auto &map = actorActivityTime_[id];
    auto it = map.upper_bound(time);
    while (true) {
      if (it->first <= time && it->second >= time) {
        return it->second;
      }
      if (it->second <= time) {
        return -1;
      }
      if (it == map.begin()) {
        return -1;
      }
      --it;
    }
    return -1;
  }
  
  static bool CheckActorActivity(ActorIdx id, VisualisationTime time) {
    Si64 endTime = GetMessageProcessingEnd(id, time);
    if (endTime < 0) {
      return false;
    }
    return true;
  }
  
  struct ThreadState {
    ActorIdx cur_actor_idx = -1;
    Si64 cur_message_idx = -1;
  };
  
  static void SetActorThreadActive() {
    VisualisationTime maxTime = Logs::GetMaxTime();
    std::map<std::string_view, ThreadState> usedThreads;
    for (size_t line_idx = 0; line_idx < parsedLogLines_.size(); line_idx++) {
      ParsedLogLine &parsedLogLine = parsedLogLines_[line_idx];
      if (parsedLogLine.type == "Send") {
        std::string_view curThreadId = parsedLogLine.threadId;
        auto thr_it = usedThreads.find(curThreadId);
        if (thr_it != usedThreads.end()) {
          ThreadState &tstate = thr_it->second;
          Si64 msg_idx = tstate.cur_message_idx;
          if (msg_idx >= 0) {
            if (parsedLogLine.message_idx >= 0) {
              logMessages_[msg_idx].child_msg_idxs.push_back(parsedLogLine.message_idx);
            }
          }
        }
      }
      if (parsedLogLine.type == "Receive") {
        ActorIdx curActorId = parsedLogLine.to;
        std::string_view curThreadId = parsedLogLine.threadId;
        auto thr_it = usedThreads.find(curThreadId);
        if (thr_it != usedThreads.end()) {
          actorActivityTime_[thr_it->second.cur_actor_idx].rbegin()->second = parsedLogLine.time - oldMinTime_;
        }
        ThreadState &tstate = usedThreads[curThreadId];
        tstate.cur_actor_idx = curActorId;
        tstate.cur_message_idx = parsedLogLine.message_idx;
      
        actorActivityTime_[curActorId][parsedLogLine.time - oldMinTime_] = maxTime - oldMinTime_;
      }
    }
  }
  
  static void SetNewActorTypes(const std::vector<uint8_t>& actorTypesMapConfigData) {
    size_t lastWordStart = 0;
    std::string_view lastWord;
    
    
    for (size_t curCharNum = 0; curCharNum < actorTypesMapConfigData.size(); ++curCharNum) {
      if (actorTypesMapConfigData[curCharNum] == '=') {
        lastWord = std::string_view(
                                    reinterpret_cast<const char*>(actorTypesMapConfigData.data() + lastWordStart), 
                                    reinterpret_cast<const char*>(actorTypesMapConfigData.data() + curCharNum)
                                    );
        lastWordStart = curCharNum+1;
      } else if (actorTypesMapConfigData[curCharNum] == '\n') {
        std::string_view oldLastWord = lastWord;
        lastWord = std::string_view(
                                    reinterpret_cast<const char*>(actorTypesMapConfigData.data() + lastWordStart), 
                                    reinterpret_cast<const char*>(actorTypesMapConfigData.data() + curCharNum)
                                    );
        lastWordStart = curCharNum+1;
        
        actorTypesMap_[oldLastWord] = lastWord;  
      }
    }
  }
  
  static const std::vector<RealLogLine>& GetRealLogLines() {
    return realLogLines_;
  }
  
  static const std::vector<ParsedLogLine>& GetParsedLogLines() {
    return parsedLogLines_;
  }
  
  static const std::vector<std::string_view>& GetLogLines() {
    return logLines_;
  }
  
  static const std::vector<LogMessage>& GetLogMessages() {
    return logMessages_;
  }
  
  static const std::map<std::string_view, std::vector<ActorIdx>>& GetActorTypeToActorId() {
    return actorTypeToActorId_;
  }
  
  static const std::map<ActorIdx, std::string_view>& GetActorIdToActorType() {
    return actorIdToActorType_;
  }
  
  static const std::map<std::string_view, std::vector<size_t>>& GetMessageToParsedLineInd() {
    return messageToParsedLineInd_;
  }
  
  static VisualisationTime GetMaxTime() {
    if (logMessages_.empty()) {
      return 0;
    }
    
    VisualisationTime maxTime = logMessages_.front().end;
    for (const LogMessage& logMessage : logMessages_) {
      maxTime = std::max(maxTime, logMessage.end);
    }
    
    return maxTime;
  }
  
  static ActorIdx GetMaxActorId() {
    return maxActorId_;
  }
  
  static void Clear() {
    logLines_.clear();
    realLogLines_.clear();
    parsedLogLines_.clear();

    realActorIdToActorNumId_.clear();
    actorNumIdToRealActorId_.clear();
    messageToParsedLineInd_.clear();

    logMessages_.clear();
    actorTypeToActorId_.clear();
    actorIdToActorType_.clear();

    newDieLogLines_.clear();
    lifeTime_.clear();
    actorActivityTime_.clear();
    threadActorIds_.clear();

    rawFileData_.clear();
    binStrings_.clear();
    binActorIdMap_.clear();
  }
  
  static bool IsAlife(ActorIdx id, VisualisationTime time) {
    if (!lifeTime_.count(id)) {
      return true;
    }
    if (lifeTime_[id].first <= time && lifeTime_[id].second >= time) {
      return true;
    }
    return false;
  }
  
  static std::string GetShortLogMessageType(const std::string& logMessageType) {
    std::string shortMessageType;
    
    for (ssize_t i = logMessageType.size() - 1; i >= 0; --i) {
      char let = logMessageType[i];
      if (let == ':') {
        break;
      }
      
      shortMessageType += let;
    }
    
    std::reverse(shortMessageType.begin(), shortMessageType.end());
    return shortMessageType;
  }

  
  static void SetActorLifeTime() {
    std::map<ActorIdx, VisualisationTime> newActors;
    std::map<ActorIdx, VisualisationTime> dieActors; 
    
    for (const NewDieLogLine& newDieLogLine : newDieLogLines_) {
      if (!realActorIdToActorNumId_.count(newDieLogLine.id)) {
        continue;
      }
      ActorIdx actor_id = realActorIdToActorNumId_.at(newDieLogLine.id);
      
      if (std::string(newDieLogLine.type) == std::string("New")) {
        if (newActors.count(actor_id)) {
          throw std::runtime_error("actor created yet");
        }
        arctic::Ui64 t = fromTimestempToTime(std::string(newDieLogLine.time));
        if (t < oldMinTime_) {
          t = oldMinTime_;
        }
        newActors[actor_id] = t - oldMinTime_;
      } else if (std::string(newDieLogLine.type) == std::string("Die")) {
        if (dieActors.count(actor_id)) {
          throw std::runtime_error("actor die yet");
        }
        arctic::Ui64 t = fromTimestempToTime(std::string(newDieLogLine.time));
        if (t < oldMinTime_) {
          t = oldMinTime_;
        }
        dieActors[actor_id] = t - oldMinTime_;
      }
    }
    
    VisualisationTime maxTime = GetMaxTime();
    
    for (ActorIdx id = 0; id <= maxActorId_; ++id) {
      lifeTime_[id].first = (newActors.count(id) ? newActors[id] : 0);
      lifeTime_[id].second = (dieActors.count(id) ? dieActors[id] : maxTime);
    }
  }
  
  static void GetActorLifeInfo() {
    for (std::string_view logLine : logLines_) {
      size_t charInd = 0;
      size_t lastCharInd = 0;
      size_t curWordNum = 0;
      
      if (logLine[0] != 'N' && logLine[0] != 'D') {
        continue;
      }
      
      NewDieLogLine newDieLogLine;
      
      for (; charInd < logLine.size(); ++charInd) {
        if (logLine[charInd] == ' ') {
          switch (curWordNum) {
            case 0: {
              newDieLogLine.type =
                std::string_view(logLine.begin() + lastCharInd, logLine.begin() + charInd);
              break;
            }
              
            case 1 : {
              newDieLogLine.threadId = std::string_view(logLine.begin() + lastCharInd, logLine.begin() + charInd);
              break;
            }
              
            case 2: {
              newDieLogLine.id =
                std::string_view(logLine.begin() + lastCharInd, logLine.begin() + charInd);
              break;
            }
          }
          
          lastCharInd = charInd + 1;
          curWordNum++;
        }
      }
      
      newDieLogLine.time = std::string_view(logLine.begin() + lastCharInd, logLine.begin() + charInd);
      
      newDieLogLines_.push_back(newDieLogLine);
    }
  }
  
  
  static void CreateRealLogLines() {
    for (std::string_view logLine : logLines_) {
      size_t charInd = 0, lastCharInd = 0;
      size_t curWordNum = 0;
      
      if (logLine[0] != 'R' && logLine[0] != 'S') {
        continue;
      }
      
      RealLogLine curRealLogLine;
      
      for (; charInd <= logLine.size(); ++charInd) {
        if (charInd == logLine.size() || logLine[charInd] == ' ') {
          switch (curWordNum) {
            case 0 : {
              curRealLogLine.type =  std::string_view(logLine.begin() + lastCharInd, logLine.begin() + charInd);
              break;
            }
            case 1 : {
              curRealLogLine.threadId = std::string_view(logLine.begin() + lastCharInd, logLine.begin() + charInd);
              arctic::Check((bool)curRealLogLine.threadId, "No thread id");
              break;
            }
            case 2 : {
              curRealLogLine.to = std::string_view(logLine.begin() + lastCharInd, logLine.begin() + charInd);
              break;
            }
            case 3 : {
              curRealLogLine.from = std::string_view(logLine.begin() + lastCharInd, logLine.begin() + charInd);
              break;
            }
            case 4 : {
              curRealLogLine.message = std::string_view(logLine.begin() + lastCharInd, logLine.begin() + charInd);
              break;
            }
            case 5 : {
              curRealLogLine.time = std::string_view(logLine.begin() + lastCharInd, logLine.begin() + charInd);
              break;
            }
            case 6 : {
              curRealLogLine.threadName = std::string_view(logLine.begin() + lastCharInd, logLine.begin() + charInd);
              break;
            }
            case 7 : {
              curRealLogLine.actorType = std::string_view(logLine.begin() + lastCharInd, logLine.begin() + charInd);
              break;
            }
            case 8: {
              curRealLogLine.messageType = std::string_view(logLine.begin() + lastCharInd, logLine.begin() + charInd);
              break;
            }
          }
          lastCharInd = charInd + 1;
          curWordNum++;
        }
      }
      
      arctic::Check((bool)curRealLogLine.threadId, "No thread id!");

      realLogLines_.push_back(curRealLogLine);
    }
  }
  
  static void ParseRealLogLines() {
    // realActorIdToActorNumId_["[0:0:0]"] = 0;
    // actorNumIdToRealActorId_[0] = "[0:0:0]";
    
    ActorIdx curUnusedActorId = 0;
    
    parsedLogLines_.reserve(realLogLines_.size());
    
    for (const RealLogLine& realLogLine : realLogLines_) {
      ParsedLogLine curParsedLogLine;
      curParsedLogLine.type = realLogLine.type;
      
      if (realLogLine.to == "[0:0:0]") {
        throw std::invalid_argument("[0:0:0] to actor");
      }
      
      if (!realActorIdToActorNumId_.count(realLogLine.to)) {
        realActorIdToActorNumId_[realLogLine.to] = curUnusedActorId;
        actorNumIdToRealActorId_[curUnusedActorId++] = realLogLine.to;
      }
      curParsedLogLine.to = realActorIdToActorNumId_[realLogLine.to];
      
      if (realLogLine.from == "[0:0:0]") {
        if (!realActorIdToActorNumId_.count(*realLogLine.threadName)) {
          threadActorIds_.push_back(std::pair(curUnusedActorId, *realLogLine.threadName));
          realActorIdToActorNumId_[*realLogLine.threadName] = curUnusedActorId;
          actorNumIdToRealActorId_[curUnusedActorId++] = *realLogLine.threadName;
        }
        
        curParsedLogLine.from = realActorIdToActorNumId_[*realLogLine.threadName];
      } else {
        if (!realActorIdToActorNumId_.count(realLogLine.from)) {
          realActorIdToActorNumId_[realLogLine.from] = curUnusedActorId;
          actorNumIdToRealActorId_[curUnusedActorId++] = realLogLine.from;        
        }
        
        curParsedLogLine.from = realActorIdToActorNumId_[realLogLine.from];
      }
      
      curParsedLogLine.message = realLogLine.message;
      
      curParsedLogLine.time = fromTimestempToTime(std::string(realLogLine.time));
      
      if (curParsedLogLine.type == "Receive") {
        curParsedLogLine.actorType = realLogLine.actorType;
        curParsedLogLine.messageType = *realLogLine.messageType;
      }
      curParsedLogLine.threadId = *realLogLine.threadId;
      curParsedLogLine.threadName = *realLogLine.threadName;
      
      parsedLogLines_.push_back(curParsedLogLine);
    }
    
    maxActorId_ = curUnusedActorId;
  }
  
  static VisualisationTime fromTimestempToTime(std::string timeStemp) {
    std::string tailWithMcS = timeStemp.substr(timeStemp.length() - 8);
    timeStemp.resize(timeStemp.length() - 8);
    tailWithMcS.pop_back();
    tailWithMcS.erase(tailWithMcS.begin());
    
    
    std::tm tm = {};
    timeStemp[timeStemp.size()-9] = ' ';
    std::stringstream tsStream(timeStemp);
    tsStream >> std::get_time(&tm, "%Y-%m-%d %T");
    auto tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    
    VisualisationTime cur =
    std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch())
      .count();
    cur *= 1'000'000;
    cur += std::stoi(tailWithMcS);
    
    return cur;
  }
  
  static void SetMessageToParsedLineInd() {
    for (size_t lineInd = 0; lineInd < parsedLogLines_.size(); ++lineInd) {
      messageToParsedLineInd_[parsedLogLines_[lineInd].message].push_back(lineInd);
    }
    
    for (auto it = messageToParsedLineInd_.begin(); it != messageToParsedLineInd_.end(); ++it) {
      std::sort(it->second.begin(), it->second.end(),
                [](size_t lhs, size_t rhs) {
        return parsedLogLines_[lhs].time < parsedLogLines_[rhs].time;
      }
                );
    }
  }
  
  static void CreateLogMessages() {
    size_t curMessageId = 0;
    for (auto it = messageToParsedLineInd_.begin(); it != messageToParsedLineInd_.end(); ++it) {
      for (size_t i = 0; i < it->second.size(); ++i) {
        size_t parsedInd = it->second[i];
        ParsedLogLine& parsedLogLineSend = parsedLogLines_[parsedInd];
        if (parsedLogLineSend.type != "Send") {
          continue;
        }
        
        LogMessage cur;
        cur.message = it->first;
        cur.from = parsedLogLineSend.from;
        cur.to = parsedLogLineSend.to;
        cur.start = parsedLogLineSend.time;
        
        bool found = false;
        for (size_t j = i + 1; j < it->second.size(); ++j) {
          ParsedLogLine &parsedLogLineReceive = parsedLogLines_[it->second[j]];
          if (parsedLogLineReceive.type != "Receive") {
            continue;
          }
          
          if (parsedLogLineReceive.from != cur.from ||
              parsedLogLineReceive.to != cur.to) {
            continue;
          }
          
          cur.end = parsedLogLineReceive.time;
          cur.messageType = *parsedLogLineReceive.messageType;
          found = true;
          
          cur.message_idx = curMessageId++;
          logMessages_.push_back(cur);
          parsedLogLineSend.message_idx = cur.message_idx;
          parsedLogLineReceive.message_idx = cur.message_idx;
          break;
        }
      
      }
    }
  }
  
  static void NormalizeLogMessagesTime() {
    if (logMessages_.size() == 0) {
      return;
    }
    
    VisualisationTime minTime = logMessages_.front().start;
    for (LogMessage& message : logMessages_) {
      minTime = std::min(minTime, message.start);
    }
    
    oldMinTime_ = minTime;
    
    for (LogMessage& message: logMessages_) {
      message.start -= minTime;
      message.end -= minTime;
    }
  }
  
  static void CreateActorTypeToActorId() {
    std::set<ActorIdx> added;
    
    // added.insert(0);
    // actorTypeToActorId_["UNIVERSE"] = {0};
    
    for (size_t i = 0; i < threadActorIds_.size(); ++i) {
      added.insert(threadActorIds_[i].first);
      actorTypeToActorId_[threadActorIds_[i].second].push_back(threadActorIds_[i].first);
    }
    
    for (const ParsedLogLine& parsedLogLine: parsedLogLines_) {
      if (!parsedLogLine.actorType) {
        continue;
      }
      
      if (!added.count(parsedLogLine.to)) {
        std::string_view actorType = *parsedLogLine.actorType;
        if (actorTypesMap_.count(actorType)) {
          actorType = actorTypesMap_[actorType];
        }
        
        actorTypeToActorId_[actorType].push_back(parsedLogLine.to);
        added.insert(parsedLogLine.to);
      }
    }
  }
  
  static void CreateActorIdToActorType() {
    // actorIdToActorType_[0] = "UNIVERSE";
    
    for (size_t i = 0; i < threadActorIds_.size(); ++i) {
      actorIdToActorType_[threadActorIds_[i].first] = threadActorIds_[i].second;
    }
    
    for (const ParsedLogLine& parsedLogLine: parsedLogLines_) {
      if (!parsedLogLine.actorType) {
        continue;
      }
      
      std::string_view actorType = *parsedLogLine.actorType;
      if (actorTypesMap_.count(actorType)) {
        actorType = actorTypesMap_[actorType];
      }
      
      actorIdToActorType_[parsedLogLine.to] = actorType;
    }
  }
  
  static std::vector<std::string_view> logLines_;
  static std::vector<RealLogLine> realLogLines_;
  static std::vector<ParsedLogLine> parsedLogLines_;
  
  static std::map<ActorIdx, std::string_view> actorNumIdToRealActorId_;
  static std::map<std::string_view, ActorIdx> realActorIdToActorNumId_;
  
  static std::map<std::string_view, std::vector<size_t>> messageToParsedLineInd_;
  
  static std::vector<LogMessage> logMessages_;
  
  static std::map<std::string_view, std::vector<ActorIdx>> actorTypeToActorId_;
  static std::map<ActorIdx, std::string_view> actorIdToActorType_;
  
  static std::vector<NewDieLogLine> newDieLogLines_;
  static std::map<ActorIdx, std::pair<VisualisationTime, VisualisationTime>> lifeTime_;
  
  static ActorIdx maxActorId_;
  
  
  static std::set<ActorIdx> tabletIds_;
  
  static VisualisationTime oldMinTime_;
  
  static std::map<std::string_view, std::string_view> actorTypesMap_;
  
  static std::vector<uint8_t> actorTypesMapConfigData_;
  
  static std::map<ActorIdx, std::map<VisualisationTime, VisualisationTime>> actorActivityTime_;
  
  static std::vector<std::pair<ActorIdx, std::string_view>> threadActorIds_;

  static std::vector<uint8_t> rawFileData_;
  static std::deque<std::string> binStrings_;
  static std::map<uint64_t, ActorIdx> binActorIdMap_;
};

std::ostream& operator<<(std::ostream& os, const Logs::LogMessage& lm);
