#pragma once

#include "DrawBox.hpp"
#include "Drawer.hpp"
#include "IListener.hpp"
#include "RectangleWindow.hpp"

#include "engine/easy_sprite.h"

class Footer : public Drawer, public RectangleWindow {
public:
  Footer( Sprite sprite) : Window(sprite), RectangleWindow(sprite) {}
  
  Footer() = default;
  
  void Listen() override {
    RectangleWindow::Listen();
    
    for (Window* window : GetSubWindows()) {
      const Window* elem = dynamic_cast<Drawer *>(window)->GetWindow();
      Window* nonConst = const_cast<Window *>(elem);
      dynamic_cast<IListener *>(nonConst)->Listen();
    }
  }
  
  const Window *GetWindow() const override { return this; }
  
  Sprite GetDrawSprite() const override { return GetFrameSprite(); }
  
  void SetDrawSprite( Sprite sprite) override {
    SetSprite(sprite);
  }
};
