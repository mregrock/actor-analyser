#include "DebugHud.hpp"

#include "Logs.hpp"
#include "GreedSeet.hpp"
#include "TimeLine.hpp"
#include "globals.h"

#include "engine/easy.h"
#include "engine/font.h"
#include "engine/rgba.h"

#include <algorithm>
#include <cstdio>

using namespace arctic;

extern GreedSeet* g_pgseet;
extern TimeLine g_time_line;
extern VisualisationTime g_min_msg_display_duration;

namespace DebugHud {

namespace {
bool s_visible = false;
}

void Toggle() { s_visible = !s_visible; }

void Draw() {
  static double prev_time = Time();
  static double last_update_at = -10;
  static double last_fps = 0;
  static Si64 last_active_count = 0;
  static Si64 last_visible_count = 0;
  double cur_time = Time();
  g_dt = cur_time - prev_time;
  prev_time = cur_time;

  char buf[1024];
  double fps = 1.0 / (g_dt > 0.0 ? g_dt : 1.0);
  if (cur_time - last_update_at > 0.1) {
    last_fps = fps;
    last_update_at = cur_time;

    last_active_count = 0;
    last_visible_count = 0;
    for (size_t i = 0; i < g_messages.size(); ++i) {
      MessageRec& m = g_messages[i];
      VisualisationTime dispEnd = std::max(m.end, m.start + g_min_msg_display_duration);
      if (m.start <= g_time_line.GetTime() && dispEnd >= g_time_line.GetTime()) {
        last_active_count++;
        if (g_pgseet->HaveCoord(m.from) && g_pgseet->HaveCoord(m.to)) {
          last_visible_count++;
        }
      }
    }
  }

  snprintf(buf, sizeof(buf),
           "Speed: %0.0f ticks/s FPS: %03.1f Offset: %d,%d Zoom: %f",
           g_speed, last_fps, g_camera.offset_.x, g_camera.offset_.y, g_camera.scaleFactor_);
  g_large_font.Draw(GetEngine()->GetBackbuffer(), buf, 10, ScreenSize().y - 10,
                    kTextOriginTop, kTextAlignmentLeft,
                    kDrawBlendingModeColorize, kFilterNearest,
                    Rgba(255, 255, 0));

  if (!s_visible) return;

  snprintf(buf, sizeof(buf),
           "Msgs: %zu total | Active: %lld | Visible: %lld | Time: %lld | MaxTime: %lld | Paused: %s",
           g_messages.size(), last_active_count, last_visible_count,
           (long long)g_time_line.GetTime(), (long long)g_time_line.maxTime_,
           g_is_pause ? "YES" : "NO");
  g_large_font.Draw(GetEngine()->GetBackbuffer(), buf, 10, ScreenSize().y - 30,
                    kTextOriginTop, kTextAlignmentLeft,
                    kDrawBlendingModeColorize, kFilterNearest,
                    Rgba(0, 255, 0));

  if (!g_messages.empty()) {
    VisualisationTime minStart = g_messages[0].start, maxEnd = g_messages[0].end;
    for (auto& m : g_messages) {
      minStart = std::min(minStart, m.start);
      maxEnd = std::max(maxEnd, m.end);
    }
    snprintf(buf, sizeof(buf),
             "MsgRange: %lld .. %lld | Actors: %lld | WithCoord: %zu",
             (long long)minStart, (long long)maxEnd,
             (long long)Logs::GetMaxActorId(), g_pgseet->coordedId_.size());
    g_large_font.Draw(GetEngine()->GetBackbuffer(), buf, 10, ScreenSize().y - 50,
                      kTextOriginTop, kTextAlignmentLeft,
                      kDrawBlendingModeColorize, kFilterNearest,
                      Rgba(0, 255, 0));
  }
}

}  // namespace DebugHud
