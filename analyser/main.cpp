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
#include <exception>
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

namespace {

constexpr const char* kDefaultTraceFile = "data/actors_trace_single_v4.bin";

std::string g_current_trace_file;
std::string g_trace_status;
bool g_has_trace_loaded = false;

bool IsShortcutDown() {
  return IsKeyDown(kKeyControl) || IsKeyDown(kKeyLeftControl) || IsKeyDown(kKeyRightControl);
}

std::string FileNameOnly(const std::string& path) {
  size_t pos = path.find_last_of("/\\");
  if (pos == std::string::npos) return path;
  return path.substr(pos + 1);
}

bool FileExists(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  return file.good();
}

void FillActorRecords() {
  g_actors.resize(Logs::GetMaxActorId() + 1);
  const std::map<ActorIdx, std::string_view>& idToType = Logs::GetActorIdToActorType();
  for (ActorIdx i = 0; i <= Logs::GetMaxActorId(); ++i) {
    ActorRec &a = g_actors[i];
    a.id_ = i;
    if (!g_pgseet->HaveCoord(i)) {
      a.visible_ = false;
    } else {
      a.visible_ = Logs::IsAlife(i, g_time_line.GetTime(), g_min_actor_display_duration);
      a.active_ = Logs::CheckActorActivity(i, g_time_line.GetTime());
      a.offset_ = g_pgseet->GetCoord(i);
      auto typeIt = idToType.find(a.id_);
      a.text = (typeIt != idToType.end()) ? static_cast<std::string>(typeIt->second) : "UNKNOWN";
      Vec2Si32 typeBlockSize = g_font.EvaluateSize(a.text.c_str(), false);
      a.visual_size = (Vec2F(typeBlockSize) + Vec2F(2, 2));
    }
  }
}

void FillMessageRecords() {
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
}

void WriteTraceLoadDebug() {
  std::ofstream dbg("/tmp/actor_debug_log.txt", std::ios::app);
  dbg << std::endl << "=== TRACE LOAD DEBUG ===" << std::endl;
  dbg << "file: " << g_current_trace_file << std::endl;
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
}

bool TryLoadTraceFile(const std::string& path, std::string* error) {
  if (!FileExists(path)) {
    if (error) *error = "file not found";
    return false;
  }

  try {
    Logs::Clear();
    VisualisationHelper::Reset();
    ActorSearch::Reset();
    g_layer_names.clear();
    g_actors.clear();
    g_messages.clear();
    g_arrows.clear();
    g_has_trace_loaded = false;
    g_mouse_nearest_message_idx = -1;
    g_mouse_nearest_actor_idx = -1;
    g_update_frame = 1;
    if (g_pgseet) {
      delete g_pgseet;
      g_pgseet = nullptr;
    }

    Logs::ReadLogs(path);
    VisualisationHelper::RecalcMessagesColor();
    FilterPanel::Init();
    Tooltip::Init();
    Highlight::Init();

    ActorIdx maxActorId = std::max(static_cast<ActorIdx>(1), Logs::GetMaxActorId());
    g_pgseet = new GreedSeet(std::pair(ScreenSize().x, ScreenSize().y),
                    std::max(static_cast<ActorIdx>(1), 9000 / maxActorId));
    g_pgseet->PrepareTables();

    g_time_line.d_time_ = 0.0;
    g_time_line.time_ = 0;
    g_time_line.SetMaxTime(Logs::GetMaxTime());
    g_camera.offset_ = ScreenSize()/2;
    g_camera.scaleFactor_ = 1.0;

    FillActorRecords();
    FillMessageRecords();

    g_current_trace_file = path;
    g_has_trace_loaded = true;
    g_trace_status = "Trace: " + FileNameOnly(path) + "  |  Cmd+O open";
    WriteTraceLoadDebug();
    return true;
  } catch (const std::exception& e) {
    if (error) *error = e.what();
  } catch (...) {
    if (error) *error = "unknown error";
  }
  return false;
}

bool LoadTraceFile(const std::string& path) {
  std::string previous = g_current_trace_file;
  std::string error;
  if (TryLoadTraceFile(path, &error)) {
    return true;
  }

  if (!previous.empty() && previous != path) {
    std::string restoreError;
    TryLoadTraceFile(previous, &restoreError);
  }
  g_trace_status = "Failed to open: " + FileNameOnly(path) + " (" + error + ")";
  return false;
}

bool HasTraceLoaded() {
  return g_has_trace_loaded && g_pgseet != nullptr;
}

void HandleOpenTraceShortcut() {
  if (!IsShortcutDown() || !IsKeyDownward(kKeyO)) return;

  std::string path = OpenFileDialog("Open actor trace", "bin");
  if (path.empty()) return;

  LoadTraceFile(path);
  ClearKeyStateTransitions();
}

void DrawTraceStatus() {
  if (g_trace_status.empty()) return;
  g_font.Draw(GetEngine()->GetBackbuffer(), g_trace_status.c_str(), 10, ScreenSize().y - 10,
              kTextOriginTop, kTextAlignmentLeft,
              kDrawBlendingModeColorize, kFilterNearest,
              Rgba(210, 230, 255));
}

void DrawEmptyTraceScreen() {
  DrawTraceStatus();

  const char* title = "No trace loaded";
  const char* hint = "Press Cmd+O to open actor trace file";
  Vec2Si32 titleSize = g_large_font.EvaluateSize(title, false);
  Vec2Si32 hintSize = g_font.EvaluateSize(hint, false);
  Vec2Si32 screen = ScreenSize();
  int x = (screen.x - titleSize.x) / 2;
  int y = screen.y / 2 + 24;

  g_large_font.Draw(GetEngine()->GetBackbuffer(), title, x, y,
                    kTextOriginTop, kTextAlignmentLeft,
                    kDrawBlendingModeColorize, kFilterNearest,
                    Rgba(230, 230, 230));
  g_font.Draw(GetEngine()->GetBackbuffer(), hint, (screen.x - hintSize.x) / 2, y - 32,
              kTextOriginTop, kTextAlignmentLeft,
              kDrawBlendingModeColorize, kFilterNearest,
              Rgba(180, 220, 255));
}

}  // namespace

void EasyMain() {
  ResizeScreen(1920, 1080);
  g_large_font.Load("data/arctic_one_bmf.fnt");
  g_font.LoadLetterBits(g_tiny_font_letters, 8, 8);

  if (!LoadTraceFile(kDefaultTraceFile)) {
    g_trace_status = "No trace loaded  |  Cmd+O open";
  }

  Mouse mouse;
  PlayerPausePlay ppp;
  ppp.SetMouse(&mouse);

  g_time_line.SetMouse(&mouse);
  ppp.SetAction([](){
    g_is_pause = !g_is_pause;
  });


  while (true) {
    if (IsKeyDownward(kKeyEscape) && !ActorSearch::IsActive()) {
      break;
    }

    HandleOpenTraceShortcut();
    Clear(Rgba(32, 32, 32));

    if (!HasTraceLoaded()) {
      DrawEmptyTraceScreen();
      DebugHud::Draw();
      ShowFrame();
      continue;
    }

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
      DrawTraceStatus();

      VisualisationTime curTime = g_time_line.GetTime();
      if (DebugHud::IsVisible()) {
        std::string time = std::to_string(curTime / 1'000'000) + '.' +
        std::to_string(curTime % 1'000'000) + "s";
        Vec2Si32 timeSize = g_large_font.EvaluateSize(time.c_str(), false);
        g_large_font.Draw(mainFrame.GetDrawSprite(), time.c_str(),
                    mainFrame.GetDrawSprite().Size().x - timeSize.x, mainFrame.GetDrawSprite().Size().y - timeSize.y,
                    kTextOriginTop,  kTextAlignmentLeft,
                    kDrawBlendingModeColorize,  kFilterNearest,
                    Rgba(255, 0, 0));
      }

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
