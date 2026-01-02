#pragma once

#include <cstdint>
#include "LogReader.hpp"
#include "engine/arctic_platform_fatal.h"

#include <sstream>
#include <stdexcept>
#include <string_view>
#include <chrono>
#include <iomanip>
#include <string>
#include <algorithm>
#include <set>

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
  
  static void ReadLogs(const std::string_view file) {
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
};

std::ostream& operator<<(std::ostream& os, const Logs::LogMessage& lm);
