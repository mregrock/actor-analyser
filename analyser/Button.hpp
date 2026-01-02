#pragma once

#include "ActionHandler.hpp"
#include <functional>
#include "MWindow.hpp"

class VisualizatorButton : virtual public MWindow, public ActionHandler {
public:
  VisualizatorButton( Sprite sprite, std::function<void()> action)
  : Window(sprite), MWindow(sprite), ActionHandler(action) {}
  
  VisualizatorButton( Sprite sprite, Mouse* mouse, std::function<void()> action)
  : VisualizatorButton(sprite, action) {
    SetMouse(mouse);
  }
  
  VisualizatorButton() = default;
};
