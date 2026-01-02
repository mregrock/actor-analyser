#include "MWindow.hpp"
#include "engine/vec2si32.h"

Vec2Si32 MWindow::GetMouseOffset() const {
  Mouse* mouse = *mouse_;
  
  Vec2Si32 spritePos = GetFrameSprite().RefPos();
  Vec2Si32 mousePos = mouse->GetOffset();
  
  return mousePos - spritePos;
}
