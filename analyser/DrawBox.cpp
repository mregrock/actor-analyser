#include "DrawBox.hpp"
#include "IDrawElement.hpp"
#include "arctic/engine/vec2si32.h"
#include "engine/arctic_types.h"
#include "engine/easy_drawing.h"
#include "DrawBoxOptions.hpp"

#include <utility>

#include <iostream>

void DrawBox::DrawWithOption() const {
  if (options_.background_color.a != 0) {
    DrawRectangle(
                  space_, 
                  Vec2Si32(0, 0), 
                  GetDrawSprite().Size(),
                  options_.background_color);
  }
  
  if (drawElement_) {
    if (IDrawElement::IsWindowed(drawElement_->GetDrawElementType())) {
      Window* window = dynamic_cast<Window*>(drawElement_);
      
      
      Sprite sprite = Sprite();
      Vec2Si32 oldSize = GetDrawSprite().Size();
      
      Vec2Si32 size = GetDrawSprite().Size();
      Vec2Si32 point = Vec2Si32(0, 0);
      
      size.x -= oldSize.x * options_.padding_right;
      size.y -= oldSize.y * options_.padding_top;
      
      point.x += oldSize.x * options_.padding_left;
      size.x -= oldSize.x * options_.padding_left;
      
      point.y += oldSize.y * options_.padding_bottom;
      size.y -= oldSize.y * options_.padding_bottom;
      
      sprite.Reference(GetDrawSprite(), point, size);
      window->SetSprite(sprite);
    }
    
    drawElement_->Draw(this);
    return;
  }
  
  if (drawers_.empty()) {
    return;
  }
  
  DrawBoxOptions curOptions = options_;
  
  if (curOptions.flex_list.empty()) {
    size_t countDrawers = drawers_.size();
    double samePart = 1.0 / countDrawers;
    
    for (size_t i = 0; i < countDrawers; ++i) {
      curOptions.flex_list.push_back(samePart);
    }
  }
  
  Sprite curSpace = space_;
  Si32 width = curSpace.Width();
  Si32 height = curSpace.Height();
  
  std::vector<std::pair<Si32, Si32>> borders;
  borders.reserve(curOptions.flex_list.size());
  
  std::vector<Sprite> drawerSprites;
  drawerSprites.resize(curOptions.flex_list.size());
  
  if (curOptions.flex_type == "row") {
    borders.emplace_back(0, (Si32)(width * curOptions.flex_list[0]));        
  } else if (curOptions.flex_type == "column") {
    borders.emplace_back(0, (Si32)(height * curOptions.flex_list[0]));
  }
  
  for (size_t curBorderNum = 1; 
       curBorderNum < curOptions.flex_list.size();
       ++curBorderNum) {
    
    if (curOptions.flex_type == "row") {
      borders.emplace_back(borders.back().second,
                           borders.back().second + width * curOptions.flex_list[curBorderNum]);
      
      if (curBorderNum == curOptions.flex_list.size() - 1) {
        borders.back().second = width;
      }
    } else if (curOptions.flex_type == "column") {
      borders.emplace_back(borders.back().second,
                           borders.back().second + height*curOptions.flex_list[curBorderNum]);
      
      if (curBorderNum == curOptions.flex_list.size() - 1) {
        borders.back().second = height;
      }
    }
  }
  
  struct SpritePointSize {
    Vec2Si32 point;
    Vec2Si32 size;
  };
  
  std::vector<SpritePointSize> forSprites(drawerSprites.size());
  for (size_t curBorderNum = 0;
       curBorderNum < curOptions.flex_list.size();
       ++curBorderNum) {
    
    if (curOptions.flex_type == "row") {
      forSprites[curBorderNum].point = Vec2Si32(borders[curBorderNum].first, 0);
      forSprites[curBorderNum].size = Vec2Si32(borders[curBorderNum].second - borders[curBorderNum].first, curSpace.Height());
    } else if (curOptions.flex_type == "column") {
      forSprites[curBorderNum].point = Vec2Si32(0, borders[curBorderNum].first);
      forSprites[curBorderNum].size = Vec2Si32(curSpace.Width(), borders[curBorderNum].second - borders[curBorderNum].first);
    }
  }
  
  for (size_t spriteNum = 0; spriteNum < forSprites.size(); ++spriteNum) {
    Vec2Si32 size = forSprites[spriteNum].size;
    
    forSprites[spriteNum].size.x -= size.x * curOptions.padding_right;
    
    forSprites[spriteNum].size.y -= size.y * curOptions.padding_top;
    
    forSprites[spriteNum].point.x += size.x * (curOptions.padding_left);
    forSprites[spriteNum].size.x -= size.x * (curOptions.padding_left);
    
    forSprites[spriteNum].point.y += size.y * (curOptions.padding_bottom);
    forSprites[spriteNum].size.y -= size.y * (curOptions.padding_bottom);
  }
  
  for (size_t curBorderNum = 0;
       curBorderNum < curOptions.flex_list.size();
       ++curBorderNum) {
    
    drawerSprites[curBorderNum].Reference(curSpace, forSprites[curBorderNum].point, forSprites[curBorderNum].size);
    drawers_[curBorderNum]->SetDrawSprite(drawerSprites[curBorderNum]);
  }
  
  for (const Drawer* drawer : drawers_) {
    drawer->Draw();
  }
}
