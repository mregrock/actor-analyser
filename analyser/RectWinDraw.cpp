#include "RectWinDraw.hpp"

#include "Camera.hpp"
#include "Mouse.hpp"
#include "MWindow.hpp"
#include "RectangleWindow.hpp"
#include "Window.hpp"
#include "globals.h"
#include "engine/arctic_input.h"
#include "engine/easy_drawing.h"
#include "engine/easy_input.h"
#include "engine/rgba.h"
#include "engine/vec2d.h"
#include "engine/vec2f.h"
#include "engine/vec2si32.h"
#include "Logs.hpp"
#include "GreedSeet.hpp"
#include "VisualisationHelper.hpp"

#include <iostream>

Sprite RectWinDraw::GetDrawSprite() const { return GetFrameSprite(); }
void RectWinDraw::SetDrawSprite( Sprite sprite) { SetSprite(sprite); }

const Window *RectWinDraw::GetWindow() const { return this; }

void RectWinDraw::Draw() const {
  if (backgroundColor_.a != 0) {
    DrawRectangle(GetDrawSprite(),  Vec2Si32(0, 0), GetDrawSprite().Size(), backgroundColor_);
    
    Drawer::Draw();
  }
}

void RectWinDraw::Listen() {
  MWindow::Listen();
  
}

void RectWinDraw::SetBackgroundColor( Rgba color) {
  backgroundColor_ = color;
}
