#pragma once

#include "IListener.hpp"

#include "engine/easy_input.h"
#include "engine/vec2si32.h"

#include <optional>

using namespace arctic;

class Mouse : public IListener {
public:
  Vec2Si32 GetOffset() const { return offset_; }
  
  bool IsLeftDown() const { return leftMouse_; }
  
  bool IsLeftDownward() const { return leftMouseDownward_; }
  
  void Listen() override;
  
  void SafeOffset() { safeOffset_ = offset_; }
  
  Vec2Si32 GetSafeOffset() { return safeOffset_; }
  
  bool GetFlag() { return leftMouseFlag_; }
  
  void SetFlag(bool toSet) { leftMouseFlag_ = toSet; }
  
private:
  bool leftMouse_ = false;
  
  Vec2Si32 offset_ = {};
  Vec2Si32 safeOffset_ = {};
  
  bool leftMouseFlag_ = false;
  
  bool leftMouseDownward_ = false;
};
