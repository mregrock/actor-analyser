#pragma once

#include "Window.hpp"
#include "DrawBoxOptions.hpp"

#include "IDrawElement.hpp"
#include "Drawer.hpp"
#include "engine/arctic_types.h"
#include "engine/easy_sprite.h"

#include <vector>

class DrawBox : public Drawer {
public:
  void SetDrawOptions(DrawBoxOptions options) {
    options_ = options;
  }
  
  void Draw() const override {
    DrawWithOption();
  }
  
  void DrawWithOption() const;
  
  Sprite GetDrawSprite() const override {
    return space_;
  }
  
  void SetDrawSprite( Sprite sprite) override {
    space_ = sprite;
  }
  
  const Window *GetWindow() const override { return nullptr; }
  
  void AddDrawer(Drawer* drawer) {
    drawers_.push_back(drawer);
  }
  
  void SetDrawElement(IDrawElement* drawElement) {
    drawElement_ = drawElement;
  }
  
private:
  
  Sprite space_;
  
  std::vector<Drawer*> drawers_;
  
  IDrawElement* drawElement_ = nullptr;
  
  DrawBoxOptions options_;
};
