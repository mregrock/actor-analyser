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
#include "FilterPanel.hpp"
#include "DebugHud.hpp"
#include "WorldRenderer.hpp"
#include "InputController.hpp"
#include "Tooltip.hpp"
#include "Highlight.hpp"
#include "ActorSearch.hpp"

#include <iostream>
#include <memory>

#include <fstream>
#include <set>
#include <algorithm>
#include "globals.h"

using namespace arctic;

Rgba g_arrow_color(255, 255, 255);

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

VisualisationTime g_min_msg_display_duration = 50000;
VisualisationTime g_min_actor_display_duration = 50000;

std::stringstream g_log;

TraceScreen g_trace_screen;
TimeLine g_time_line;

std::vector<std::string> g_layer_names;

void EasyMain() {
  ResizeScreen(1920, 1080);
  g_large_font.Load("data/arctic_one_bmf.fnt");
  g_font.LoadLetterBits(g_tiny_font_letters, 8, 8);

  // Logs::ReadLogs("data/actors_trace_single.bin");
  // Logs::ReadLogs("data/actors_trace_single_pointer.bin");
  Logs::ReadLogs("data/actors_trace_single_v4.bin");
  // // Logs::ReadLogs("data/actors_trace.bin");
  // Logs::ReadLogs("data/storage_start_err.log");
  VisualisationHelper::RecalcMessagesColor();

  FilterPanel::Init();
  Tooltip::Init();
  Highlight::Init();

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
      a.visible_ = Logs::IsAlife(i, g_time_line.GetTime(), g_min_actor_display_duration);
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

    {
      size_t withBirth = 0;
      for (ActorIdx id : g_pgseet->coordedId_) {
        if (Logs::lifeTime_.count(id) && Logs::lifeTime_.at(id).first > 0) {
          withBirth++;
        }
      }
      dbg << "coordedId actors with birth > 0: " << withBirth << std::endl;
      dbg << "coordedId actors with birth = 0 (always visible): "
          << (g_pgseet->coordedId_.size() - withBirth) << std::endl;
      dbg << "Sample coordedId actors with birth > 0:" << std::endl;
      int cnt = 0;
      for (ActorIdx id : g_pgseet->coordedId_) {
        if (Logs::lifeTime_.count(id) && Logs::lifeTime_.at(id).first > 0 && cnt < 10) {
          dbg << "  actor " << id << " birth=" << Logs::lifeTime_.at(id).first
              << " death=" << Logs::lifeTime_.at(id).second << std::endl;
          cnt++;
        }
      }
    }

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


  while (true) {
    if (IsKeyDownward(kKeyEscape) && !ActorSearch::IsActive()) {
      break;
    }

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
      ActorSearch::HandleInput();

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
      WorldRenderer::DrawLayers(g_pgseet);
      {
        static bool first_birth_logged = false;
        static std::set<ActorIdx> prev_visible;
        for (ActorIdx i = 0; i < (ActorIdx)g_actors.size(); ++i) {
          if (!g_pgseet->HaveCoord(i)) continue;
          bool now_visible = Logs::IsAlife(i, g_time_line.GetTime(), g_min_actor_display_duration);
          if (now_visible && !prev_visible.count(i) && Logs::lifeTime_.count(i) && Logs::lifeTime_.at(i).first > 0) {
            Vec2F screenPos = g_camera.WorldToScreen(Vec2F(g_actors[i].offset_));
            std::ofstream dbg("/tmp/actor_births.txt", std::ios::app);
            dbg << "BIRTH: actor " << i << " at time=" << g_time_line.GetTime()
                << " world=(" << g_actors[i].offset_.x << "," << g_actors[i].offset_.y << ")"
                << " screen=(" << (int)screenPos.x << "," << (int)screenPos.y << ")"
                << " birth=" << Logs::lifeTime_.at(i).first
                << " death=" << Logs::lifeTime_.at(i).second << std::endl;
            first_birth_logged = true;
          }
          if (now_visible) prev_visible.insert(i);
          else prev_visible.erase(i);
        }
      }
      for (ActorIdx i = 0; i < (ActorIdx)g_actors.size(); ++i) {
        if (!g_pgseet->HaveCoord(i)) {
          continue;
        }
        ActorRec &a = g_actors[i];
        a.visible_ = Logs::IsAlife(i, g_time_line.GetTime(), g_min_actor_display_duration)
                     && FilterPanel::IsActorVisible(i, g_time_line.GetTime());
        a.active_ = Logs::CheckActorActivity(i, g_time_line.GetTime());
        a.offset_ = g_pgseet->GetCoord(i);

        WorldRenderer::DrawActor(a);
      }
      ssize_t logMessageNum = -1;
      for (size_t i = 0; i < Logs::GetLogMessages().size(); ++i) {
        MessageRec &m = g_messages[i];
        logMessageNum++;
        if (!g_pgseet->HaveCoord(m.from) || !g_pgseet->HaveCoord(m.to)) {
          continue;
        }
        if (!FilterPanel::IsActorVisible(m.from, g_time_line.GetTime()) ||
            !FilterPanel::IsActorVisible(m.to, g_time_line.GetTime())) {
          continue;
        }

        if (VisualisationHelper::IsOnlyBirthActive() && VisualisationHelper::GetMessageColor(m.id) == Rgba(255, 255, 0)) {
          continue;
        }

        WorldRenderer::DrawArrowAndMessage(m);
      }

      screen.Draw();

      if (g_mouse_nearest_actor_idx >= 0) {
        WorldRenderer::DrawActor(g_actors[g_mouse_nearest_actor_idx]);
      }
      if (g_mouse_nearest_message_idx > 0) {
        WorldRenderer::DrawArrowAndMessage(g_messages[g_mouse_nearest_message_idx]);
      }
      if (g_mouse_nearest_actor_idx >= 0 && g_distance_sq_to_nearest_actor == 0.0) {
        WorldRenderer::DrawActor(g_actors[g_mouse_nearest_actor_idx]);
      }

      if (!ActorSearch::IsActive()) {
        ppp.Listen();
        g_time_line.Listen();
        InputController::UpdateCamera();
        InputController::UpdateTime();
        mainFrame.Listen();
        VisualisationHelper::Listen(*g_pgseet, &mainFrame);
        FilterPanel::HandleInput();
      }
      FilterPanel::Draw();
      ActorSearch::Draw();

      VisualisationTime curTime = g_time_line.GetTime();
      std::string time = std::to_string(curTime / 1'000'000) + '.' +
      std::to_string(curTime % 1'000'000) + "s";
      Vec2Si32 timeSize = g_large_font.EvaluateSize(time.c_str(), false);
      g_large_font.Draw(mainFrame.GetDrawSprite(), time.c_str(),
                  mainFrame.GetDrawSprite().Size().x - timeSize.x, mainFrame.GetDrawSprite().Size().y - timeSize.y,
                  kTextOriginTop,  kTextAlignmentLeft,
                  kDrawBlendingModeColorize,  kFilterNearest,
                  Rgba(255, 0, 0));

      if (g_mouse_nearest_actor_idx >= 0 && g_distance_sq_to_nearest_actor == 0.0) {
        Tooltip::DrawActorTooltip(g_mouse_nearest_actor_idx, curTime);
      } else if (g_mouse_nearest_message_idx >= 0 && g_distance_sq_to_nearest_message >= 0 &&
                 g_distance_sq_to_nearest_message < 100.0) {
        Tooltip::DrawMessageTooltip((size_t)g_mouse_nearest_message_idx);
      }
    }

    if (!VisualisationHelper::IsTraceMode()) {
      DebugHud::Draw();
    }
    ShowFrame();
  }
}
