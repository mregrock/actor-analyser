#include "RectangleWindow.hpp"

#include <iostream>

bool RectangleWindow::IsMouseIn() const {
  if (!GetMouse()) {
    return false;
  }

  Mouse* mouse = GetMouse();

   Vec2Si32 spritePos = GetFrameSprite().RefPos();
   Vec2Si32 mousePos = mouse->GetOffset();
   Vec2Si32 spriteSize = GetFrameSprite().Size();


  return mousePos.x > spritePos.x && 
         mousePos.y > spritePos.y &&
         mousePos.x < spritePos.x + spriteSize.x &&
         mousePos.y < spritePos.y + spriteSize.y;
}
