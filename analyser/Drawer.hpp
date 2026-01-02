#pragma once

#include "IDrawElement.hpp"
#include "Camera.hpp"
#include "Window.hpp"
#include <map>
#include "engine/easy_sprite.h"

#include <memory>

class Drawer {
public:
  virtual void Draw() const;
  void AddDrawElement(IDrawElement *drawElement);
  
  virtual  Sprite GetDrawSprite() const = 0;
  virtual void SetDrawSprite( Sprite sprite) = 0;
  
  virtual const Window* GetWindow() const = 0;

  const std::map<uint64_t, size_t>& GetActorIdToStorageInd() const { return actorIdToStorageInd_; }
  
  ~Drawer() = default;
  Drawer() = default;
  
protected:
  Drawer(const Drawer &) = delete;
  
  std::vector<IDrawElement *> drawStorage_;
  
  std::map<uint64_t, size_t> actorIdToStorageInd_;
};
