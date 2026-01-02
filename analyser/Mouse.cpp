#include "Mouse.hpp"
#include "Camera.hpp"
#include "engine/easy_input.h"
#include "engine/vec2si32.h"
#include <memory>

#include <iostream>
#include <optional>

void Mouse::Listen() {
  leftMouse_ =  IsKeyDown( KeyCode::kKeyMouseLeft);
  leftMouseDownward_ =  IsKeyDownward( KeyCode::kKeyMouseLeft);
  
  offset_ =  MousePos();
}
