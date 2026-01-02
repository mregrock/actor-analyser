#pragma once

#include <string_view>
#include <sys/types.h>
#include <vector>

#include "engine/rgba.h"

using namespace arctic;

struct DrawBoxOptions {
  double padding_top = 0;
  double padding_bottom = 0;
  double padding_left = 0;
  double padding_right = 0;
  
  std::string_view flex_type = "column";
  std::vector<double> flex_list;
  
  Rgba background_color =  Rgba(0, 0, 0, 0);
};
