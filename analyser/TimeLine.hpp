#pragma once

#include "engine/arctic_input.h"
#include "engine/easy_input.h"
#include "VisualisationHelper.hpp"
#include "IDrawElement.hpp"
#include "RectangleWindow.hpp"
#include "CButton.hpp"
#include "globals.h"

using VisualisationTime = int64_t;

class TimeLine : public RectangleWindow, public CButton, public IDrawElement {
public:
  TimeLine() {
    time_ = 0;
    d_time_ = 0.0;
    maxTime_ = 0;
  }
  
  void SetMaxTime(VisualisationTime maxTime) { maxTime_ = maxTime; }
  
  void Draw(const Drawer* drawer) const override;
  void Action() override;
  void Listen() override {
    CButton::Listen();
    // YYY
    //VisualisationHelper::GetNormalisationCoef()

  }
  
  VisualisationTime GetTime() const { return time_; }
  
  DrawElementType GetDrawElementType() const override {
    return DrawElementType::TIME_LINE;
  }
  
  int GetSpeed() const {
    return g_speed + (int)(VisualisationHelper::GetNormalisationCoef() * ((g_speed > 0) ? 1ll : -1ll)); }
  void SetSpeed(int speed) { g_speed = speed; }
  

  double d_time_;
  VisualisationTime time_;
  VisualisationTime maxTime_;
  
  double speed_;
};
