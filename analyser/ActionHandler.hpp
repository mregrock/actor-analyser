#pragma once

#include <functional>

class ActionHandler {
public:
  ActionHandler(std::function<void()> action) : action_(action) {}
  
  virtual void Action() { action_(); }
  
  void SetAction(std::function<void()> action) { action_ = action; }
  
  ActionHandler() = default;
  
private:
  std::function<void()> action_ = []() {};
};
