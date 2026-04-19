#include "InputController.hpp"

#include "Camera.hpp"
#include "DebugHud.hpp"
#include "GreedSeet.hpp"
#include "Logs.hpp"
#include "TimeLine.hpp"
#include "TraceScreen.hpp"
#include "VisualisationHelper.hpp"
#include "globals.h"

#include "engine/arctic_input.h"
#include "engine/easy.h"
#include "engine/easy_input.h"
#include "engine/rgba.h"
#include "engine/vec2d.h"
#include "engine/vec2si32.h"

#include <algorithm>
#include <array>

using namespace arctic;

extern GreedSeet* g_pgseet;
extern TimeLine g_time_line;
extern TraceScreen g_trace_screen;

namespace InputController {

void UpdateTime() {
  if (!g_is_pause) {
    double time_diff = 0.0;

    if (g_time_mode == kTimeNormal) {
      time_diff = g_dt * g_speed;
    } else if (g_time_mode == kTimeAdaptive) {
      VisualisationTime earliest_active_end = -1;

      for (size_t i = 0; i < Logs::GetLogMessages().size(); ++i) {
        MessageRec& m = g_messages[i];
        if (!g_pgseet->HaveCoord(m.from) || !g_pgseet->HaveCoord(m.to)) {
          continue;
        }
        if (VisualisationHelper::IsOnlyBirthActive() && VisualisationHelper::GetMessageColor(m.id) == Rgba(255, 255, 0)) {
          continue;
        }
        VisualisationTime dispEnd = std::max(m.end, m.start + g_min_msg_display_duration);
        if (m.start <= g_time_line.GetTime() && dispEnd >= g_time_line.GetTime()) {
          if (earliest_active_end < 0 || dispEnd < earliest_active_end) {
            earliest_active_end = dispEnd;
          }
        }
      }

      if (earliest_active_end >= 0) {
        double max_adaptive_speed = (double)g_min_msg_display_duration * 2.0;
        double effective_speed = g_speed;
        if (g_speed > 0) {
          effective_speed = std::min(g_speed, max_adaptive_speed);
        } else if (g_speed < 0) {
          effective_speed = std::max(g_speed, -max_adaptive_speed);
        }
        time_diff = g_dt * effective_speed;
        if (g_speed > 0 && g_time_line.d_time_ + time_diff > earliest_active_end + 1) {
          time_diff = earliest_active_end + 1 - g_time_line.d_time_;
        } else if (g_speed < 0 && g_time_line.d_time_ + time_diff < earliest_active_end - 1) {
          time_diff = earliest_active_end - 1 - g_time_line.d_time_;
        }
      } else {
        time_diff = g_dt * g_speed;
      }
    }

    g_time_line.d_time_ += time_diff;
    if (g_time_line.d_time_ < 0.0) {
      g_time_line.d_time_ = 0.0;
    }
    if (g_time_line.d_time_ >= g_time_line.maxTime_) {
      g_time_line.d_time_ = g_time_line.maxTime_;
    }
    g_time_line.time_ = (Ui64)g_time_line.d_time_;
    if (g_time_line.time_ >= g_time_line.maxTime_) {
      g_time_line.time_ = g_time_line.maxTime_;
    }
  }

  Si32 idx = -1;
  if (IsKeyDownward(kKey1)) idx = 0;
  if (IsKeyDownward(kKey2)) idx = 1;
  if (IsKeyDownward(kKey3)) idx = 2;
  if (IsKeyDownward(kKey4)) idx = 3;
  if (IsKeyDownward(kKey5)) idx = 4;
  if (IsKeyDownward(kKey6)) idx = 5;
  if (IsKeyDownward(kKey7)) idx = 6;
  if (IsKeyDownward(kKey8)) idx = 7;
  if (IsKeyDownward(kKey9)) idx = 8;
  if (IsKeyDownward(kKey0)) idx = 9;

  constexpr std::array<double, 10> kSpeed =
      {5000.0, 10000.0, 25000.0, 50000.0, 100000.0, 250000.0, 500000.0, 1000000.0, 5000000.0, 10000000.0};
  double sign = 1.0;
  if (IsKeyDown(kKeyShift) || IsKeyDown(kKeyLeftShift)) {
    sign = -1.0;
  }
  if (idx >= 0 && idx < (Si32)kSpeed.size()) {
    g_speed = sign * kSpeed[idx];
  }
  if (IsKeyDownward(kKeySectionSign)) {
    g_speed = 0;
  }

  if (IsKeyDownward(kKeyA)) {
    if (g_time_mode == kTimeNormal) {
      g_time_mode = kTimeAdaptive;
    } else {
      g_time_mode = kTimeNormal;
    }
  }
}

void UpdateCamera() {
  if (IsKeyDown(KeyCode::kKeyLeft)) {
    g_camera.offset_.x -= ScreenSize().x * g_dt / g_camera.scaleFactor_;
  }
  if (IsKeyDown(KeyCode::kKeyDown)) {
    g_camera.offset_.y -= ScreenSize().x * g_dt / g_camera.scaleFactor_;
  }
  if (IsKeyDown(KeyCode::kKeyRight)) {
    g_camera.offset_.x += ScreenSize().x * g_dt / g_camera.scaleFactor_;
  }
  if (IsKeyDown(KeyCode::kKeyUp)) {
    g_camera.offset_.y += ScreenSize().x * g_dt / g_camera.scaleFactor_;
  }
  if (IsKeyDown(KeyCode::kKeyEquals)) {
    g_camera.scaleFactor_ += 0.01;
  }
  if (IsKeyDown(KeyCode::kKeyMinus)) {
    g_camera.scaleFactor_ -= 0.01;
  }

  if (IsKeyDown(kKeyMouseLeft) && !IsKeyDownward(kKeyMouseLeft)) {
    g_camera.offset_ -= Vec2Si32(Vec2D(MouseMove()) / g_camera.GetScaleFactor());
  }
  if (MouseWheelDelta()) {
    double new_scale = g_camera.GetScaleFactor() - MouseWheelDelta() * 0.001;
    new_scale = Clamp(new_scale, 0.05, 5.0);
    g_camera.SetScaleFactor(new_scale);
  }

  if (IsKeyDownward(kKeyMouseLeft) && (IsKeyDown(kKeyShift) || IsKeyDown(kKeyLeftShift))) {
    if (g_mouse_nearest_message_idx >= 0 && g_distance_sq_to_nearest_message < 25.0) {
      VisualisationHelper::SelectMessage(Logs::GetLogMessages()[g_mouse_nearest_message_idx]);
      VisualisationHelper::SetSelectedMessageColor(Logs::GetLogMessages()[g_mouse_nearest_message_idx], Rgba(0, 30, 255));
      VisualisationHelper::RecalcMessagesColor();
    }
  }
  if (IsKeyDownward(kKeyMouseLeft) && (IsKeyDown(kKeyControl) || IsKeyDown(kKeyLeftControl))) {
    if (g_mouse_nearest_message_idx >= 0 && g_distance_sq_to_nearest_message < 25.0) {
      g_trace_screen.CreateMessageTraces(g_mouse_nearest_message_idx);
      g_camera.SetOffset(Vec2Si32(0, 0));
      VisualisationHelper::SwitchTraceMode();
    }
  }
  if (IsKeyDownward(kKeyF1)) {
    DebugHud::Toggle();
  }
}

}  // namespace InputController
