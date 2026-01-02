#pragma once

#include <vector>

#include "IDrawElement.hpp"
#include "MWindow.hpp"

#include "engine/vec2si32.h"

class RectangleWindow : virtual public MWindow {
public:
  RectangleWindow( Sprite sprite) : Window(sprite) {}
  
  RectangleWindow() = default;
  
  Vec2Si32 GetWindowSize() const { return GetFrameSprite().Size(); }
  
  bool IsMouseIn() const override;
};
