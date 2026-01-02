#pragma once

#include "engine/easy_drawing.h"
#include "engine/easy_sprite.h"
#include "engine/rgba.h"
#include "engine/vec2si32.h"

using namespace arctic;

const  Sprite kNoneSprite =  Sprite();

class Window {
public:
  virtual void Fill( Rgba color) const {
    if (color.a != 0) {
      DrawRectangle(frameSprite_,  Vec2Si32(0, 0),
                    frameSprite_.Size(), color);
    }
  }
  
  Sprite GetFrameSprite() const { return frameSprite_; }
  
  Window( Sprite frameSprite) : frameSprite_(frameSprite) {}
  
  const std::vector<Window*>& GetSubWindows() const { return subWindows_; }
  
  virtual ~Window() {
    for (Window* window : subWindows_) {
      delete window;
    }
  }
  
  Window() { frameSprite_ = kNoneSprite; }
  
  virtual void SetSprite( Sprite sprite) { frameSprite_ = sprite; }
  
private:
  Window(const Window &) = delete;
  Window &operator=(const Window &) = delete;
  
  Sprite frameSprite_;
  std::vector<Window *> subWindows_;
};
