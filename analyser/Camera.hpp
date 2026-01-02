#pragma once

#include "engine/vec2si32.h"
#include "engine/easy.h"

using namespace arctic;

struct Camera {
  Vec2Si32 GetOffset() { return offset_; }
  double GetScaleFactor() { return scaleFactor_; }
  
  void SetOffset( Vec2Si32 offset) { offset_ = offset; }
  void SetScaleFactor(double scaleFactor) { scaleFactor_ = scaleFactor; }
  
  Vec2Si32 offset_ = Vec2Si32(0, 0);
  double scaleFactor_ = 1.0;
  
  Vec2F WorldToScreen(Vec2F world) {
    return (world - Vec2F(offset_)) * float(scaleFactor_) + Vec2F(ScreenSize()) * 0.5f;
  }
};
