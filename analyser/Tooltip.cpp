#include "Tooltip.hpp"

#include "Logs.hpp"
#include "globals.h"

#include "engine/easy.h"
#include "engine/easy_drawing.h"
#include "engine/easy_input.h"
#include "engine/easy_sprite.h"
#include "engine/easy_util.h"
#include "engine/font.h"
#include "engine/rgba.h"
#include "engine/vec2si32.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

using namespace arctic;

namespace Tooltip {

namespace {

std::unordered_map<ActorIdx, size_t> s_sentCount;
std::unordered_map<ActorIdx, size_t> s_recvCount;

void DrawBox(Vec2Si32 mouse, const std::vector<std::string>& lines, Rgba borderColor) {
  if (lines.empty()) return;
  Sprite sprite = GetEngine()->GetBackbuffer();
  Vec2Si32 screen = ScreenSize();

  int lineH = 10;
  int padX = 6;
  int padY = 4;

  int maxW = 0;
  for (const auto& l : lines) {
    Vec2Si32 sz = g_font.EvaluateSize(l.c_str(), false);
    maxW = std::max(maxW, (int)sz.x);
  }
  int boxW = maxW + padX * 2;
  int boxH = (int)lines.size() * lineH + padY * 2;

  int x0 = mouse.x + 14;
  int y1 = mouse.y - 14;
  if (x0 + boxW > screen.x - 4) x0 = mouse.x - 14 - boxW;
  if (x0 < 4) x0 = 4;
  if (y1 > screen.y - 4) y1 = screen.y - 4;
  if (y1 - boxH < 4) y1 = boxH + 4;
  int y0 = y1 - boxH;

  DrawRectangle(sprite, Vec2Si32(x0, y0), Vec2Si32(x0 + boxW, y1), Rgba(0, 0, 0, 220));
  DrawLine(sprite, Vec2Si32(x0, y0), Vec2Si32(x0 + boxW, y0), borderColor);
  DrawLine(sprite, Vec2Si32(x0 + boxW, y0), Vec2Si32(x0 + boxW, y1), borderColor);
  DrawLine(sprite, Vec2Si32(x0 + boxW, y1), Vec2Si32(x0, y1), borderColor);
  DrawLine(sprite, Vec2Si32(x0, y1), Vec2Si32(x0, y0), borderColor);

  int yText = y1 - padY;
  for (const auto& l : lines) {
    g_font.Draw(sprite, l.c_str(), x0 + padX, yText,
                kTextOriginTop, kTextAlignmentLeft,
                kDrawBlendingModeColorize, kFilterNearest,
                Rgba(255, 255, 255));
    yText -= lineH;
  }
}

std::string FormatTime(VisualisationTime t) {
  char buf[64];
  if (t < 1000) snprintf(buf, sizeof(buf), "%lldus", (long long)t);
  else if (t < 1000000) snprintf(buf, sizeof(buf), "%.2fms", (double)t / 1000.0);
  else snprintf(buf, sizeof(buf), "%.3fs", (double)t / 1000000.0);
  return buf;
}

}  // namespace

void Init() {
  s_sentCount.clear();
  s_recvCount.clear();
  const auto& msgs = Logs::GetLogMessages();
  for (const auto& m : msgs) {
    s_sentCount[m.from]++;
    s_recvCount[m.to]++;
  }
}

void DrawActorTooltip(ActorIdx id, VisualisationTime curTime) {
  std::vector<std::string> lines;
  char buf[256];

  std::string_view typeName = "?";
  const auto& idToType = Logs::GetActorIdToActorType();
  auto tIt = idToType.find(id);
  if (tIt != idToType.end()) typeName = tIt->second;

  std::string_view rawId = "?";
  auto rIt = Logs::actorNumIdToRealActorId_.find(id);
  if (rIt != Logs::actorNumIdToRealActorId_.end()) rawId = rIt->second;

  snprintf(buf, sizeof(buf), "actor #%lld  %.*s",
           (long long)id, (int)typeName.size(), typeName.data());
  lines.push_back(buf);

  snprintf(buf, sizeof(buf), "raw id: %.*s",
           (int)rawId.size(), rawId.data());
  lines.push_back(buf);

  auto ltIt = Logs::lifeTime_.find(id);
  if (ltIt != Logs::lifeTime_.end()) {
    VisualisationTime birth = ltIt->second.first;
    VisualisationTime death = ltIt->second.second;
    bool alive = curTime >= birth && curTime <= death;
    snprintf(buf, sizeof(buf), "life: %s .. %s  (%s)",
             FormatTime(birth).c_str(),
             FormatTime(death).c_str(),
             alive ? "alive" : "not alive");
    lines.push_back(buf);
  }

  size_t sent = 0, recv = 0;
  auto sIt = s_sentCount.find(id);
  if (sIt != s_sentCount.end()) sent = sIt->second;
  auto rcIt = s_recvCount.find(id);
  if (rcIt != s_recvCount.end()) recv = rcIt->second;
  snprintf(buf, sizeof(buf), "sent: %zu   recv: %zu", sent, recv);
  lines.push_back(buf);

  DrawBox(MousePos(), lines, Rgba(120, 200, 255));
}

void DrawMessageTooltip(size_t msgIdx) {
  const auto& msgs = Logs::GetLogMessages();
  if (msgIdx >= msgs.size()) return;
  const Logs::LogMessage& m = msgs[msgIdx];

  std::vector<std::string> lines;
  char buf[256];

  std::string_view msgType = m.messageType;
  snprintf(buf, sizeof(buf), "%.*s",
           (int)msgType.size(), msgType.data());
  lines.push_back(buf);

  snprintf(buf, sizeof(buf), "from #%lld -> to #%lld",
           (long long)m.from, (long long)m.to);
  lines.push_back(buf);

  VisualisationTime latency = (m.end >= m.start) ? m.end - m.start : 0;
  snprintf(buf, sizeof(buf), "latency: %s  (start=%s, end=%s)",
           FormatTime(latency).c_str(),
           FormatTime(m.start).c_str(),
           FormatTime(m.end).c_str());
  lines.push_back(buf);

  if (m.handlePtr != 0) {
    snprintf(buf, sizeof(buf), "handlePtr: 0x%llx", (unsigned long long)m.handlePtr);
    lines.push_back(buf);
  }

  if (!m.forwardHops.empty()) {
    snprintf(buf, sizeof(buf), "forward hops: %zu", m.forwardHops.size());
    lines.push_back(buf);
  }

  if (!m.child_msg_idxs.empty()) {
    snprintf(buf, sizeof(buf), "caused: %zu child messages", m.child_msg_idxs.size());
    lines.push_back(buf);
  }

  DrawBox(MousePos(), lines, Rgba(255, 200, 120));
}

}  // namespace Tooltip
