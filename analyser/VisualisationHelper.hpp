#pragma once

#include <cstdint>
#include "Logs.hpp"
#include <queue>
#include <unordered_map>
#include "Camera.hpp"
#include "engine/easy.h"
#include "engine/easy_input.h"
#include "GreedSeet.hpp"
#include "globals.h"

using namespace arctic;

class VisualisationHelper {
public:
  static void SelectMessage(Logs::LogMessage logMessage) {
    selectedMessages_.insert(logMessage.message_idx);
    lastSelectedMessageId_ = logMessage.message_idx;   
  }
    
  static void SetSelectedMessageColor(Logs::LogMessage logMessage, Rgba color) {
    selectedMessageColor_[logMessage.message_idx] = color;
  }
  
  static void RecalcMessagesColor() {
    messageColor_.clear();
    for (size_t selectedMessageId : selectedMessages_) {
      CalcColor(selectedMessageId);
    }
    for (const Logs::LogMessage& it : Logs::GetLogMessages()) {
      CalcColor(it.message_idx);
    }
  }
  
  static Rgba GetMessageColor(const Logs::LogMessage& logMessage) {
    return GetMessageColor(logMessage.message_idx);
  }
  
  static Rgba GetMessageColor(size_t logMessageId) {
    return messageColor_[logMessageId];
  }
  
  static void Listen(GreedSeet& gs, const Drawer* drawer) {
    if (IsKeyDownward(kKeyS)) {
      shortMessageNamesActivate_ = !shortMessageNamesActivate_;
    }
    
    
    if (IsKeyDownward(kKeyB)) {
      const std::vector<Logs::LogMessage>& logMessages = Logs::GetLogMessages();
      
      onlyBirth_ = !onlyBirth_;
      SetBirthActors();
      gs.Clear();
      gs.PrepareTables();
      
      VisualisationTime minTime = UINT64_MAX;
      for (ssize_t messageNum = logMessages.size() - 1; messageNum >= 0; --messageNum) {
        if (GetMessageColor(logMessages[messageNum].message_idx) != Rgba(255, 255, 0)) {
          minTime = std::min(minTime, logMessages[messageNum].end - logMessages[messageNum].start);
        }
      }
      
      VisualisationHelper::SetNormalisationCoef(minTime / 1000);
      
      if (GetLastSelectedMessageId() != 0) {
        g_camera.SetOffset(gs.GetCoord(logMessages[GetLastSelectedMessageId()].from));
      }
    }
  }
  
  static bool IsShortMessageTypeActivate() {
    return shortMessageNamesActivate_;
  }
  
  static bool IsBirthActor(ActorIdx id) {
    if (!onlyBirth_) {
      return true;
    }
    
    return birthActors_.count(id);
  }
  
  static bool IsOnlyBirthActive() {
    return onlyBirth_;
  }
  
  static void SetNormalisationCoef(int64_t normCoef) {
    normalisationCoef_ = normCoef;
  }
  
  static int64_t GetNormalisationCoef() {
    return normalisationCoef_;
  }
  
  static uint64_t GetLastSelectedMessageId() {
    return lastSelectedMessageId_;
  }
  
  static void SwitchTraceMode() {
    isTraceMode_ = !isTraceMode_;
  }
  
  static bool IsTraceMode() {
    return isTraceMode_;
  }
  
private:
  
  static void CalcColor(size_t id) {
    if (messageColor_.count(id)) {
      return;
    }
    
    if (selectedMessages_.count(id)) {
      messageColor_[id] = selectedMessageColor_[id];
      return;
    }
    
    if (!messageColor_.count(id)) {
      messageColor_[id] = Rgba(255, 255, 0);
    }
  }
  
  static void SetBirthActors() {
    const std::vector<Logs::LogMessage>& logMessages = Logs::GetLogMessages();
    std::unordered_set<Ui64> next_messages;
    std::unordered_set<Ui64> done_messages;
    for (const Logs::LogMessage& logMessage : logMessages) {
      if (GetMessageColor(logMessage.message_idx) != Rgba(255, 255, 0)) {
        next_messages.insert(logMessage.message_idx);
      }
    }
    while (!next_messages.empty()) {
      Ui64 msg_idx = *next_messages.begin();
      next_messages.erase(next_messages.begin());
      
      const Logs::LogMessage &msg = logMessages[msg_idx];
      messageColor_[msg_idx] = Rgba(250, 250, 0);
      
      birthActors_.insert(msg.from);
      birthActors_.insert(msg.to);
      
      for (Si64 i : msg.child_msg_idxs) {
        if (done_messages.insert(i).second) {
          next_messages.insert(i);
        }
      }
    }
  }
  
  static std::set<size_t> selectedMessages_;
  static std::map<size_t, Rgba> selectedMessageColor_;
  
  static std::unordered_map<size_t, Rgba> messageColor_;
  
  static std::set<ActorIdx> birthActors_;
  
  static bool shortMessageNamesActivate_;
  static bool onlyBirth_;
  
  static int64_t normalisationCoef_;
  
  static uint64_t lastSelectedMessageId_;
  
  static bool isTraceMode_;
};
