#pragma once

#include "Drawer.hpp"
#include "TraceScreen.hpp"

#include "RectangleWindow.hpp"
#include "MWindow.hpp"

#include "engine/vec2si32.h"
#include "LogReader.hpp"
#include <memory>

using namespace arctic;

class GreedSeet;

class RectWinDraw : public RectangleWindow, public Drawer {
public:
  RectWinDraw( Sprite sprite, Mouse *mouse)
  : RectWinDraw(sprite) {
    SetMouse(mouse);
  }
    
  RectWinDraw( Sprite sprite)
    : Window(sprite), Drawer(), RectangleWindow(sprite) {}
  
  RectWinDraw() {}
  
  Sprite GetDrawSprite() const override;
  
  void SetDrawSprite( Sprite sprite) override;
  
  void Listen() override;
  
  const Window *GetWindow() const override;
  
  void SetGreedSeet(GreedSeet* gs) {
    gs_ = gs;
  }
  
  GreedSeet* GetGreedSeet() {
    return *gs_;
  }
  
  void Draw() const override;
  
  void SetBackgroundColor(Rgba color);
  
  void SetTraceScreen(TraceScreen* traceScreen) {
    ts_ = traceScreen;
  }
  
private:
  std::optional<GreedSeet*> gs_;
  std::optional<TraceScreen*> ts_;
  
  mutable Rgba backgroundColor_=Rgba(0, 0, 0);
  
};
