#pragma once

#include "MWindow.hpp"
#include "Button.hpp"
#include <functional>

class CButton : public VisualizatorButton {
public:
  CButton( Sprite sprite, std::function<void()> action)
  : Window(sprite), VisualizatorButton(sprite, action) {}
  
  CButton( Sprite sprite, Mouse* mouse, std::function<void()> action)
  : Window(sprite), VisualizatorButton(sprite, mouse, action) {}
  
  CButton() = default;
  
  void Listen() override;
};
