#pragma once

#include "Mouse.hpp"
#include "IListener.hpp"
#include "Window.hpp"

#include "engine/vec2si32.h"

#include <optional>

const  Vec2Si32 kUndefinedMousePosition =  Vec2Si32(-1, -1);

class MWindow : virtual public Window, public IListener {
public:
  void Listen() override {
    if (mouse_) {
      (*mouse_)->Listen();
    }
  }
  
  virtual bool IsMouseIn() const = 0;
  
  Vec2Si32 GetMouseOffset() const;
  
  void SetMouse(Mouse *mouse) { mouse_ = mouse; }
  
  Mouse *GetMouse() const {
    if (!mouse_) {
      return nullptr;
    }
    
    return *mouse_;
  }
  
  MWindow( Sprite sprite, Mouse *mouse) : MWindow(sprite) {
    mouse_ = mouse;
  }
  
  MWindow( Sprite sprite) : Window(sprite) {}
  
  ~MWindow() = default;
  
  MWindow() = default;
  
private:
  MWindow(const MWindow &) = delete;
  MWindow &operator=(const MWindow &) = delete;
  
  std::optional<Mouse *> mouse_;
};
