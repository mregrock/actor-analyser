#include "Camera.hpp"
#include "GreedSeet.hpp"
#include "RectWinDraw.hpp"
#include "RectangleWindow.hpp"
#include "LogReader.hpp"
#include "Logs.hpp"
#include "CButton.hpp"
#include "DrawBox.hpp"
#include "Footer.hpp"
#include "TimeLine.hpp"
#include "VisualisationHelper.hpp"

#include "CButton.hpp"
#include "PlayerPausePlay.hpp"

#include "engine/arctic_input.h"
#include "engine/easy.h"
#include "engine/easy_advanced.h"
#include "engine/easy_input.h"
#include "engine/easy_sprite.h"
#include "engine/easy_util.h"
#include "engine/font.h"
#include "engine/rgba.h"
#include "engine/vec2d.h"
#include "engine/vec2si32.h"
#include "TraceScreen.hpp"
#include "DrawBoxOptions.hpp"

#include <iostream>
#include <memory>

#include <fstream>
#include "globals.h"

using namespace arctic;

Rgba g_arrow_color(255,255,255);

GreedSeet *g_pgseet;
Camera g_camera;
arctic::Font g_font;
arctic::Font g_large_font;
std::vector<ActorRec> g_actors;
std::vector<MessageRec> g_messages;
std::unordered_map<std::tuple<ActorIdx, ActorIdx>, ArrowRec, TupleHash> g_arrows;
Ui64 g_update_frame = 1;
double g_dt = 0.0;
double g_speed = 1.0;
double g_is_pause = true;


Si64 g_mouse_nearest_message_idx = -1;
double g_distance_sq_to_nearest_message = -1.0;
Si64 g_mouse_nearest_actor_idx = -1;
double g_distance_sq_to_nearest_actor = -1.0;

TimeMode g_time_mode = kTimeNormal;

VisualisationTime g_min_msg_display_duration = 500000;

std::stringstream g_log;

TraceScreen g_trace_screen;
TimeLine g_time_line;

const std::vector< Rgba> layersColors_ = {
//   Rgba(245/4, 120/4, 145/4),
//   Rgba(0, 0, 253/4),
//   Rgba(0, 130/4, 50/4),
//   Rgba(230/4, 7, 7)
};

std::vector<std::string> g_layer_names;

void UpdateTime() {
  if (!g_is_pause) {
    
    double time_diff = 0.0;
    
    if (g_time_mode == kTimeNormal) {
      time_diff = g_dt * g_speed;
    } else if (g_time_mode == kTimeAdaptive) {
      Si64 shortest_delivery = INT64_MAX;
      Ui64 latest_shortest_delivery_end = 0;
      
      for (size_t i = 0; i < Logs::GetLogMessages().size(); ++i) {
        MessageRec &m = g_messages[i];
        if (!g_pgseet->HaveCoord(m.from) || !g_pgseet->HaveCoord(m.to)) {
          continue;
        }
        if (VisualisationHelper::IsOnlyBirthActive() && VisualisationHelper::GetMessageColor(m.id) == Rgba(255, 255, 0)) {
          continue;
        }
        {
          VisualisationTime dispEnd = std::max(m.end, m.start + g_min_msg_display_duration);
          if (m.start <= g_time_line.GetTime() && dispEnd >= g_time_line.GetTime()) {
            Si64 delivery = dispEnd - m.start;
            if (delivery < shortest_delivery && delivery > 0) {
              shortest_delivery = delivery;
              latest_shortest_delivery_end = dispEnd;
            } else if (delivery == shortest_delivery) {
              latest_shortest_delivery_end = std::max(latest_shortest_delivery_end, (Ui64)dispEnd);
            }
          }
        }
      }
      
      if (shortest_delivery == 0) {
        shortest_delivery = 1;
      }
      if (shortest_delivery > 1000.0) {
        shortest_delivery = 1000.0;
      }
      double desired_duration = 1000.0;
      double duration = shortest_delivery;
      double multiplier = duration/desired_duration;
      time_diff = g_dt * g_speed * multiplier;
      if (latest_shortest_delivery_end > 0) {
        if (g_time_line.d_time_ + time_diff > latest_shortest_delivery_end + 1) {
          time_diff = latest_shortest_delivery_end + 1 - g_time_line.d_time_;
        }
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
  if (IsKeyDownward(kKey1)) {
    idx = 0;
  }
  if (IsKeyDownward(kKey2)) {
    idx = 1;
  }
  if (IsKeyDownward(kKey3)) {
    idx = 2;
  }
  if (IsKeyDownward(kKey4)) {
    idx = 3;
  }
  if (IsKeyDownward(kKey5)) {
    idx = 4;
  }
  if (IsKeyDownward(kKey6)) {
    idx = 5;
  }
  if (IsKeyDownward(kKey7)) {
    idx = 6;
  }
  if (IsKeyDownward(kKey8)) {
    idx = 7;
  }
  if (IsKeyDownward(kKey9)) {
    idx = 8;
  }
  if (IsKeyDownward(kKey0)) {
    idx = 9;
  }
  constexpr std::array<double, 10> kSpeed =
    {5000.0, 10000.0, 25000.0, 50000.0, 100000.0, 250000.0, 500000.0, 1000000.0, 5000000.0, 10000000.0};
  double sign = 1.0;
  if (IsKeyDown(kKeyShift) || IsKeyDown(kKeyLeftShift)) {
    sign = -1.0;
  }
  if (idx >= 0 && idx < kSpeed.size()) {
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
  if (IsKeyDown(KeyCode::kKeyLeft)) { // XXX
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
}

void DrawStatus() {
  static double prev_time = Time();
  static double last_update_at = -10;
  static double last_fps = 0;
  static Si64 last_active_count = 0;
  static Si64 last_visible_count = 0;
  double cur_time = Time();
  char buf[1024];
  g_dt = cur_time - prev_time;
  double fps = 1.0 / (g_dt > 0.0 ? g_dt : 1.0);
  if (cur_time - last_update_at > 0.1) {
    last_fps = fps;
    last_update_at = cur_time;

    last_active_count = 0;
    last_visible_count = 0;
    for (size_t i = 0; i < g_messages.size(); ++i) {
      MessageRec &m = g_messages[i];
      VisualisationTime dispEnd = std::max(m.end, m.start + g_min_msg_display_duration);
      if (m.start <= g_time_line.GetTime() && dispEnd >= g_time_line.GetTime()) {
        last_active_count++;
        if (g_pgseet->HaveCoord(m.from) && g_pgseet->HaveCoord(m.to)) {
          last_visible_count++;
        }
      }
    }
  }
  
  snprintf(buf, sizeof(buf), "Speed: %0.0f ticks/s FPS: %03.1f Offset: %d,%d Zoom: %f",
           g_speed, last_fps, g_camera.offset_.x, g_camera.offset_.y, g_camera.scaleFactor_);
  g_large_font.Draw(GetEngine()->GetBackbuffer(), buf, 10, ScreenSize().y - 10,
              kTextOriginTop,
              kTextAlignmentLeft,
              kDrawBlendingModeColorize,
              kFilterNearest,
              Rgba(255,255,0));

  snprintf(buf, sizeof(buf),
           "Msgs: %zu total | Active: %lld | Visible: %lld | Time: %lld | MaxTime: %lld | Paused: %s",
           g_messages.size(), last_active_count, last_visible_count,
           (long long)g_time_line.GetTime(), (long long)g_time_line.maxTime_,
           g_is_pause ? "YES" : "NO");
  g_large_font.Draw(GetEngine()->GetBackbuffer(), buf, 10, ScreenSize().y - 30,
              kTextOriginTop,
              kTextAlignmentLeft,
              kDrawBlendingModeColorize,
              kFilterNearest,
              Rgba(0,255,0));

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
                kTextOriginTop,
                kTextAlignmentLeft,
                kDrawBlendingModeColorize,
                kFilterNearest,
                Rgba(0,255,0));
  }

  prev_time = cur_time;
}

void DrawActor(const ActorRec &a) {
  if (!a.visible_) {
    return;
  }
  Vec2F leftDownBlockCorner = g_camera.WorldToScreen(Vec2F(a.offset_));
  Vec2F size = a.visual_size * g_camera.scaleFactor_;
  Vec2F textPos = leftDownBlockCorner + Vec2F(2, 2)*g_camera.scaleFactor_;
  Rgba blockColor = Rgba(128/2, 160/2, 190/2);
  if (a.active_) {
    blockColor = Rgba(255, 160/2, 190/2);
  }
  
  Sprite sprite = GetEngine()->GetBackbuffer();
  DrawBlock(sprite,
            leftDownBlockCorner,
            size,
            5.0 * g_camera.scaleFactor_,
            blockColor,
            1.0 * g_camera.scaleFactor_,
            Rgba(0, 0, 0));
  
  g_font.Draw(sprite, a.text.c_str(), textPos.x, textPos.y,
              kTextOriginBottom,  kTextAlignmentLeft,
              kDrawBlendingModeColorize,  kFilterNearest,
              Rgba(255, 255, 255));
  
  Vec2F m = Vec2F(MousePos());
  if (m.x >= leftDownBlockCorner.x
      && m.y >= leftDownBlockCorner.y
      && m.x <= leftDownBlockCorner.x + size.x
      && m.y <= leftDownBlockCorner.y + size.y) {
    g_mouse_nearest_actor_idx = a.id_;
    g_distance_sq_to_nearest_actor = 0;
  } else {
    Vec2F a_center = leftDownBlockCorner + size * 0.5f;
    Vec2F edge = BlockEdgePos(leftDownBlockCorner, a.visual_size, 5.0 * g_camera.scaleFactor_, a_center - Vec2F(MousePos()));
    double distance_sq = LengthSquared(Vec2F(m) - edge);
    if (g_mouse_nearest_actor_idx < 0 ||
        (g_mouse_nearest_message_idx < 0 || distance_sq <= g_distance_sq_to_nearest_message)) {
      if (g_mouse_nearest_actor_idx < 0 || distance_sq <= g_distance_sq_to_nearest_actor) {
        g_mouse_nearest_actor_idx = a.id_;
        g_distance_sq_to_nearest_actor = distance_sq;
      }
    }
  }
}

void UpdateArrow(ArrowRec &arrow) {
  if (arrow.last_update_frame == g_update_frame) {
    return;
  }
  if (arrow.from == arrow.to) {
    const ActorRec &fromActor = g_actors[arrow.from];
    double width = fromActor.visual_size.x;
    double height = fromActor.visual_size.y;
    arrow.points.resize(4);
    arrow.points[0].position = Vec2F(fromActor.offset_) + Vec2F(width / 2 - height / 2, height);
    arrow.points[3].position = arrow.points[0].position + Vec2F(height, 0);
    arrow.points[1].position = arrow.points[0].position + Vec2F(0, 2*height);
    arrow.points[2].position = arrow.points[3].position + Vec2F(0, 2*height);
    arrow.points[0].length_part = 0.0;
    arrow.points[1].length_part = 1.0 / 3.0;
    arrow.points[2].length_part = 2.0 / 3.0;
    arrow.points[3].length_part = 1.0;
  } else {
    const ActorRec &fromActor = g_actors[arrow.from];
    const ActorRec &toActor = g_actors[arrow.to];
    Vec2F toCenter = Vec2F(toActor.offset_) + toActor.visual_size * 0.5f;
    Vec2F fromCenter = Vec2F(fromActor.offset_) + fromActor.visual_size * 0.5f;
    arrow.points.resize(2);
    arrow.points[0].position = BlockEdgePos(Vec2F(fromActor.offset_), fromActor.visual_size, 5.0, Vec2F(toCenter - fromCenter));
    arrow.points[1].position = BlockEdgePos(Vec2F(toActor.offset_), toActor.visual_size, 5.0, Vec2F(fromCenter - toCenter));
    arrow.points[0].length_part = 0.0;
    arrow.points[1].length_part = 1.0;
  }
  arrow.last_update_frame = g_update_frame;
}

void DrawArrow(ArrowRec &arrow) {
  if (arrow.last_draw_frame == g_update_frame) {
    return;
  }
  size_t size = arrow.points.size();
  if (size < 2) {
    return;
  }
  Sprite sprite = GetEngine()->GetBackbuffer();
  Vec2F prevPos = g_camera.WorldToScreen(arrow.points[0].position);
  for (size_t i = 1; i < size-1; ++i) {
    Vec2F pos = g_camera.WorldToScreen(arrow.points[i].position);
    DrawLine(sprite, Vec2Si32(prevPos), Vec2Si32(pos), g_arrow_color);
    prevPos = pos;
  }
  Vec2F pos = g_camera.WorldToScreen(arrow.points[size-1].position);
  DrawArrow(sprite, prevPos, pos, g_camera.scaleFactor_, 7*g_camera.scaleFactor_, 10*g_camera.scaleFactor_, g_arrow_color);
  arrow.last_draw_frame = g_update_frame;
}

void DrawMessage(ArrowRec &arrow, MessageRec &msg, double d_time, Rgba color) {
  VisualisationTime displayEnd = std::max(msg.end, msg.start + g_min_msg_display_duration);
  double duration = (double)(displayEnd - msg.start);
  double part = (d_time - msg.start) / (duration > 0 ? duration : 1.0);
  Vec2F pos = arrow.PositionAtPart(part);
  Vec2Si32 coord = Vec2Si32(g_camera.WorldToScreen(pos));
  
  
  std::string messageType = (std::string)msg.messageType;
  if (VisualisationHelper::IsShortMessageTypeActivate()) {
    messageType = Logs::GetShortLogMessageType(messageType);
  }
  
  Vec2Si32 messageSize = g_font.EvaluateSize(messageType.c_str(), false);
  Vec2Si32 messagePos = Vec2Si32(coord.x, coord.y - messageSize.y);

  Sprite sprite = GetEngine()->GetBackbuffer();
  DrawRectangle(sprite, messagePos, messagePos + messageSize, Rgba(255, 255, 255));
  
  DrawCircle(GetEngine()->GetBackbuffer(), coord,
             4.0 * g_camera.GetScaleFactor(),
             Rgba(0,0,0));
  
  DrawCircle(GetEngine()->GetBackbuffer(), coord,
             3.0 * g_camera.GetScaleFactor(),
             color);
  
  g_font.Draw(sprite, messageType.c_str(), messagePos.x, messagePos.y+1, kTextOriginBottom,  kTextAlignmentLeft,
              kDrawBlendingModeColorize,  kFilterNearest,
              Rgba(0, 0, 0));
  
  
  double distance_sq = LengthSquared(MousePos() - coord);
  if (g_mouse_nearest_message_idx < 0 || distance_sq <= g_distance_sq_to_nearest_message) {
    g_mouse_nearest_message_idx = msg.id;
    g_distance_sq_to_nearest_message = distance_sq;
  }
}

void DrawArrowAndMessage(MessageRec &m) {
  VisualisationTime displayEnd = std::max(m.end, m.start + g_min_msg_display_duration);
  if (m.start <= g_time_line.GetTime() && displayEnd >= g_time_line.GetTime()) {
    std::tuple<ActorIdx, ActorIdx> fromTo(m.from, m.to);
    ArrowRec &arrow = g_arrows[fromTo];
    UpdateArrow(arrow);
    
    DrawArrow(arrow);
    DrawMessage(arrow, m, g_time_line.d_time_, VisualisationHelper::GetMessageColor(m.id));
  }
}

void DrawLayers(GreedSeet *gs) {
  const std::vector<ActorTable>& actorTables = gs->actorTables_;
  for (size_t actorTableNum = 0; actorTableNum < actorTables.size(); ++actorTableNum) {
    Rgba layerColor;
    if (actorTableNum < layersColors_.size()) {
      layerColor = layersColors_[actorTableNum];
    } else {
      layerColor = Rgba(24, 120, 145);
    }
    
    std::string layerName;
    if (actorTableNum < g_layer_names.size()) {
      layerName = g_layer_names[actorTableNum];
    } else {
      Check(false, "Unexpected layer");
    }
    
    Vec2Si32 leftDownCorner(actorTables[actorTableNum].GetXAdd(),
                            actorTables[actorTableNum].GetYAdd()-1);
    
    Vec2Si32 rightUpCorner = Vec2Si32(leftDownCorner.x + actorTables[actorTableNum].GetLineLength(),
                                      actorTables[actorTableNum].GetY() + 8);
    
    Vec2Si32 llc = Vec2Si32(g_camera.WorldToScreen((Vec2F)leftDownCorner));
    Vec2Si32 urc = Vec2Si32(g_camera.WorldToScreen((Vec2F)rightUpCorner));
    
    Sprite sprite = GetEngine()->GetBackbuffer();
    DrawRectangle(sprite, llc, urc, layerColor);
    
    g_font.Draw(sprite, layerName.c_str(),
                llc.x, llc.y,
                kTextOriginTop,  kTextAlignmentLeft,
                kDrawBlendingModeColorize,  kFilterNearest,
                Rgba(255, 255, 255));
  }
}

void EasyMain() {
  ResizeScreen(1920, 1080);
  g_large_font.Load("data/arctic_one_bmf.fnt");
  g_font.LoadLetterBits(g_tiny_font_letters, 8, 8);
  
  Logs::ReadLogs("data/actors_trace.bin");
  VisualisationHelper::RecalcMessagesColor();

  
  
  g_pgseet = new GreedSeet(std::pair(ScreenSize().x, ScreenSize().y),
                  std::max(static_cast<ActorIdx>(1), 9000 / Logs::GetMaxActorId()));
  g_pgseet->PrepareTables();
  
  g_camera.offset_ = ScreenSize()/2;
  g_camera.scaleFactor_ = 1.0;

  Mouse mouse;
  PlayerPausePlay ppp;
  ppp.SetMouse(&mouse);
  
  g_time_line.SetMouse(&mouse);
  g_time_line.SetMaxTime(Logs::GetMaxTime());

  
  ppp.SetAction([](){
    g_is_pause = !g_is_pause;
  });
  
  g_actors.resize(Logs::GetMaxActorId() + 1);
  for (ActorIdx i = 0; i <= Logs::GetMaxActorId(); ++i) {
    ActorRec &a = g_actors[i];
    a.id_ = i;
    if (!g_pgseet->HaveCoord(i)) {
      a.visible_ = false;
    } else {
      a.visible_ = Logs::IsAlife(i, g_time_line.GetTime());
      a.active_ = Logs::CheckActorActivity(i, g_time_line.GetTime());
      a.offset_ = g_pgseet->GetCoord(i);
      const std::map<ActorIdx, std::string_view>& idToType = Logs::GetActorIdToActorType();
      a.text = static_cast<std::string>(idToType.at(a.id_));
      Vec2Si32 typeBlockSize = g_font.EvaluateSize(a.text.c_str(), false);
      a.visual_size = (Vec2F(typeBlockSize) + Vec2F(2, 2));
    }
  }
  
  g_messages.resize(Logs::GetLogMessages().size());
  for (size_t i = 0; i < Logs::GetLogMessages().size(); ++i) {
    const Logs::LogMessage &event = Logs::GetLogMessages()[i];
    MessageRec &m = g_messages[i];
    m.from = event.from;
    m.to = event.to;
    m.start = event.start;
    m.end = event.end;
    m.message = event.message;
    m.messageType = event.messageType;
    m.id = event.message_idx;

    std::tuple<ActorIdx, ActorIdx> fromTo(m.from, m.to);
    auto it = g_arrows.find(fromTo);
    if (it == g_arrows.end()) {
      ArrowRec &arrow = g_arrows[fromTo];
      arrow.from = m.from;
      arrow.to = m.to;
    }
  }

  {
    std::ofstream dbg("/tmp/actor_debug_log.txt", std::ios::app);
    dbg << std::endl << "=== MAIN INIT DEBUG ===" << std::endl;
    dbg << "g_messages.size: " << g_messages.size() << std::endl;
    dbg << "g_actors.size: " << g_actors.size() << std::endl;
    dbg << "g_arrows.size: " << g_arrows.size() << std::endl;
    dbg << "maxTime: " << g_time_line.maxTime_ << std::endl;
    dbg << "coordedId count: " << g_pgseet->coordedId_.size() << std::endl;

    size_t msgsWithCoord = 0;
    for (size_t i = 0; i < g_messages.size(); ++i) {
      if (g_pgseet->HaveCoord(g_messages[i].from) && g_pgseet->HaveCoord(g_messages[i].to)) {
        msgsWithCoord++;
      }
    }
    dbg << "Messages where BOTH actors have coords: " << msgsWithCoord << std::endl;

    if (!g_messages.empty()) {
      dbg << "First 5 messages:" << std::endl;
      for (size_t i = 0; i < std::min((size_t)5, g_messages.size()); ++i) {
        auto& m = g_messages[i];
        dbg << "  [" << i << "] from=" << m.from << "(coord=" << g_pgseet->HaveCoord(m.from) << ")"
            << " to=" << m.to << "(coord=" << g_pgseet->HaveCoord(m.to) << ")"
            << " start=" << m.start << " end=" << m.end << std::endl;
      }
    }
    dbg.close();
  }
  
  
  while (!IsKeyDownward( kKeyEscape)) {
    
    Clear(Rgba(32, 32, 32));
    
    if (VisualisationHelper::IsTraceMode()) {
      DrawBox screen;
      screen.SetDrawSprite(GetEngine()->GetBackbuffer());
      screen.SetDrawOptions(DrawBoxOptions{
        .flex_type="column"
      });
      screen.AddDrawer(&g_trace_screen);
      g_trace_screen.SetBackgroundColor( Rgba(232, 227, 227));
      g_trace_screen.SetMouse(&mouse);
      screen.Draw();
      g_trace_screen.Listen();
    } else {
      DrawBox screen;
      screen.SetDrawSprite( GetEngine()->GetBackbuffer());
      screen.SetDrawOptions(DrawBoxOptions{
        .flex_type="column",
        .flex_list={0.05, 0.95}  
      });
      
      DrawBox footer;
      footer.SetDrawOptions(DrawBoxOptions{
        .flex_type="row",
        .flex_list={0.02, 0.98},
        .background_color= Rgba(131, 131, 131)
      });
      
      DrawBox playerPausePlay;
      DrawBox timeLineBox;
      timeLineBox.SetDrawOptions(DrawBoxOptions{
        .background_color= Rgba(131, 131, 131),
        .padding_right=0.05,
        .padding_left=0.05,
        .padding_top=0.3,
        .padding_bottom=0.3
      });
      
      timeLineBox.SetDrawElement(&g_time_line);
      
      playerPausePlay.SetDrawOptions(DrawBoxOptions{
        .background_color= Rgba(0, 0, 0, 0)
      });
      
      playerPausePlay.SetDrawElement(&ppp);
      
      footer.AddDrawer(&playerPausePlay);
      footer.AddDrawer(&timeLineBox);
      
      RectWinDraw mainFrame;
      screen.AddDrawer(&footer);
      screen.AddDrawer(&mainFrame);
      
      mainFrame.SetBackgroundColor( Rgba(232, 227, 227, 0));
      mainFrame.SetTraceScreen(&g_trace_screen);
      
      mainFrame.SetMouse(&mouse);
      
      mainFrame.SetGreedSeet(g_pgseet);
      
      g_pgseet->Listen();
      
      g_mouse_nearest_message_idx = -1;
      g_mouse_nearest_actor_idx = -1;
      
      screen.Draw();
      ++g_update_frame;
      DrawLayers(g_pgseet);
      for (ActorIdx i = 0; i < g_actors.size(); ++i) {
        if (!g_pgseet->HaveCoord(i)) {
          continue;
        }
        ActorRec &a = g_actors[i];
        a.visible_ = Logs::IsAlife(i, g_time_line.GetTime());
        a.active_ = Logs::CheckActorActivity(i, g_time_line.GetTime());
        a.offset_ = g_pgseet->GetCoord(i);
        
        DrawActor(a);
      }
      ssize_t logMessageNum = -1;
      for (size_t i = 0; i < Logs::GetLogMessages().size(); ++i) {
        MessageRec &m = g_messages[i];
        logMessageNum++;
        if (!g_pgseet->HaveCoord(m.from) || !g_pgseet->HaveCoord(m.to)) {
          continue;
        }
        
        if (VisualisationHelper::IsOnlyBirthActive() && VisualisationHelper::GetMessageColor(m.id) == Rgba(255, 255, 0)) {
          continue;
        }
        
        DrawArrowAndMessage(m);
      }
  
      screen.Draw();
      
      if (g_mouse_nearest_actor_idx >= 0) {
        DrawActor(g_actors[g_mouse_nearest_actor_idx]);
      }
      if (g_mouse_nearest_message_idx > 0) {
        DrawArrowAndMessage(g_messages[g_mouse_nearest_message_idx]);
      }
      if (g_mouse_nearest_actor_idx >= 0 && g_distance_sq_to_nearest_actor == 0.0) {
        DrawActor(g_actors[g_mouse_nearest_actor_idx]);
      }
      
      ppp.Listen();
      g_time_line.Listen();
      UpdateCamera();
      UpdateTime();
      mainFrame.Listen();
      VisualisationHelper::Listen(*g_pgseet, &mainFrame);
      
      VisualisationTime curTime = g_time_line.GetTime();
      std::string time = std::to_string(curTime / 1'000'000) + '.' + 
      std::to_string(curTime % 1'000'000) + "s";
      Vec2Si32 timeSize = g_large_font.EvaluateSize(time.c_str(), false);
      g_large_font.Draw(mainFrame.GetDrawSprite(), time.c_str(),
                  mainFrame.GetDrawSprite().Size().x - timeSize.x, mainFrame.GetDrawSprite().Size().y - timeSize.y,
                  kTextOriginTop,  kTextAlignmentLeft,
                  kDrawBlendingModeColorize,  kFilterNearest,
                  Rgba(255, 0, 0));
    }
  
    DrawStatus();
    ShowFrame();
  }
}
