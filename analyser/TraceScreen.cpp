#include "TraceScreen.hpp"

#include "Highlight.hpp"
#include "Logs.hpp"
#include "VisualisationHelper.hpp"
#include "globals.h"

#include "engine/easy.h"
#include "engine/easy_drawing.h"
#include "engine/easy_input.h"
#include "engine/font.h"
#include "engine/rgba.h"
#include "engine/vec2f.h"
#include "engine/vec2si32.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

using namespace arctic;

Sprite TraceScreen::GetDrawSprite() const { return GetFrameSprite(); }
void TraceScreen::SetDrawSprite(Sprite sprite) { SetSprite(sprite); }
const Window* TraceScreen::GetWindow() const { return this; }
void TraceScreen::SetBackgroundColor(Rgba color) { backgroundColor_ = color; }

namespace {

std::string FormatTimeShort(VisualisationTime t) {
  char buf[64];
  if (t < 1000) snprintf(buf, sizeof(buf), "%lldus", (long long)t);
  else if (t < 1000000) snprintf(buf, sizeof(buf), "%.2fms", (double)t / 1000.0);
  else snprintf(buf, sizeof(buf), "%.3fs", (double)t / 1000000.0);
  return buf;
}

}  // namespace

void TraceScreen::CreateMessageTraces(uint64_t messageId) {
  Highlight::SelectChain(messageId);
  rootMsgIdx_ = messageId;
  timeScale_ = 1.0;
  timeOffset_ = 0.0;
  BuildLayoutFromHighlight();
}

void TraceScreen::BuildLayoutFromHighlight() {
  orderedMsgs_.clear();
  laneActors_.clear();
  actorToLane_.clear();

  const auto& msgs = Logs::GetLogMessages();
  const auto& chain = Highlight::ChainMsgs();
  if (chain.empty()) return;

  orderedMsgs_.assign(chain.begin(), chain.end());
  std::sort(orderedMsgs_.begin(), orderedMsgs_.end(),
            [&](size_t a, size_t b) {
              return msgs[a].start < msgs[b].start;
            });

  minTime_ = msgs[orderedMsgs_.front()].start;
  maxTime_ = minTime_;
  for (size_t idx : orderedMsgs_) {
    minTime_ = std::min(minTime_, msgs[idx].start);
    maxTime_ = std::max(maxTime_, msgs[idx].end);
  }
  if (maxTime_ <= minTime_) maxTime_ = minTime_ + 1;

  for (size_t idx : orderedMsgs_) {
    const auto& m = msgs[idx];
    for (ActorIdx a : {m.from, m.to}) {
      if (!actorToLane_.count(a)) {
        actorToLane_[a] = (int)laneActors_.size();
        laneActors_.push_back(a);
      }
    }
  }
}

void TraceScreen::Draw() const {
  Sprite sprite = GetDrawSprite();
  Vec2Si32 size = sprite.Size();
  DrawRectangle(sprite, Vec2Si32(0, 0), size, backgroundColor_);

  if (orderedMsgs_.empty()) {
    const char* hint = "No chain selected. Press C on a message to open chain view.";
    g_large_font.Draw(sprite, hint, size.x / 2 - 300, size.y / 2,
                      kTextOriginTop, kTextAlignmentLeft,
                      kDrawBlendingModeColorize, kFilterNearest,
                      Rgba(200, 200, 200));
    return;
  }

  const int leftPad = 280;
  const int rightPad = 40;
  const int topPad = 60;
  const int bottomPad = 60;

  int areaX0 = leftPad;
  int areaX1 = size.x - rightPad;
  int areaY0 = bottomPad;
  int areaY1 = size.y - topPad;
  int areaW = areaX1 - areaX0;
  int areaH = areaY1 - areaY0;
  if (areaW < 10 || areaH < 10) return;

  int numLanes = (int)laneActors_.size();
  if (numLanes <= 0) return;
  double laneHeight = (double)areaH / (double)numLanes;

  const auto& msgs = Logs::GetLogMessages();
  VisualisationTime span = maxTime_ - minTime_;

  auto timeToX = [&](VisualisationTime t) -> double {
    double norm = (double)(t - minTime_) / (double)span;
    norm = (norm - timeOffset_) * timeScale_;
    return (double)areaX0 + norm * (double)areaW;
  };
  auto laneY = [&](int lane) -> double {
    return (double)areaY1 - (lane + 0.5) * laneHeight;
  };

  DrawRectangle(sprite, Vec2Si32(areaX0, areaY0), Vec2Si32(areaX1, areaY1), Rgba(28, 32, 40));
  for (int i = 0; i < numLanes; ++i) {
    int y = (int)laneY(i);
    Rgba laneColor = (i % 2 == 0) ? Rgba(34, 40, 50) : Rgba(28, 32, 40);
    DrawRectangle(sprite,
                  Vec2Si32(areaX0, (int)(areaY1 - (i + 1) * laneHeight)),
                  Vec2Si32(areaX1, (int)(areaY1 - i * laneHeight)),
                  laneColor);
    DrawLine(sprite, Vec2Si32(areaX0, y), Vec2Si32(areaX1, y), Rgba(60, 70, 90));

    ActorIdx a = laneActors_[i];
    std::string_view typeName = "?";
    const auto& idToType = Logs::GetActorIdToActorType();
    auto it = idToType.find(a);
    if (it != idToType.end()) typeName = it->second;

    char buf[128];
    snprintf(buf, sizeof(buf), "#%lld %.*s",
             (long long)a, (int)typeName.size(), typeName.data());
    g_large_font.Draw(sprite, buf, 8, y + 6,
                      kTextOriginTop, kTextAlignmentLeft,
                      kDrawBlendingModeColorize, kFilterNearest,
                      Rgba(220, 220, 220));
  }

  int numTicks = 6;
  for (int i = 0; i <= numTicks; ++i) {
    double frac = (double)i / (double)numTicks;
    VisualisationTime t = minTime_ + (VisualisationTime)(frac * (double)span);
    int x = (int)timeToX(t);
    if (x < areaX0 - 20 || x > areaX1 + 20) continue;
    DrawLine(sprite, Vec2Si32(x, areaY0), Vec2Si32(x, areaY0 - 6), Rgba(120, 140, 160));
    std::string label = FormatTimeShort(t - minTime_);
    g_large_font.Draw(sprite, label.c_str(), x - 20, areaY0 - 10,
                      kTextOriginTop, kTextAlignmentLeft,
                      kDrawBlendingModeColorize, kFilterNearest,
                      Rgba(180, 200, 220));
  }

  int blockHalfH = std::max(4, (int)(laneHeight * 0.35));

  std::vector<Si64> parentOf;
  {
    parentOf.assign(msgs.size(), -1);
    for (size_t i = 0; i < msgs.size(); ++i) {
      for (Si64 child : msgs[i].child_msg_idxs) {
        if (child >= 0 && (size_t)child < parentOf.size() && parentOf[child] < 0) {
          parentOf[child] = (Si64)i;
        }
      }
    }
  }

  auto colorFor = [&](size_t idx) -> Rgba {
    if (idx == rootMsgIdx_) return Rgba(255, 220, 40);
    if (Highlight::IsAncestor(idx)) return Rgba(90, 170, 255);
    if (Highlight::IsDescendant(idx)) return Rgba(120, 230, 120);
    return Rgba(200, 200, 200);
  };

  for (size_t idx : orderedMsgs_) {
    const auto& m = msgs[idx];
    auto toIt = actorToLane_.find(m.to);
    if (toIt == actorToLane_.end()) continue;
    int toLane = toIt->second;

    Si64 pIdx = (idx < parentOf.size()) ? parentOf[idx] : -1;
    if (pIdx < 0) continue;
    if (!Highlight::IsMsgInChain((size_t)pIdx)) continue;

    const auto& pm = msgs[pIdx];
    auto pToIt = actorToLane_.find(pm.to);
    if (pToIt == actorToLane_.end()) continue;
    int pLane = pToIt->second;

    double px = timeToX(pm.end);
    double cx = timeToX(m.start);
    double py = laneY(pLane);
    double cy = laneY(toLane);

    Rgba linkColor = colorFor(idx);
    linkColor.a = 140;
    DrawLine(sprite, Vec2Si32((int)px, (int)py), Vec2Si32((int)cx, (int)cy), linkColor);
  }

  for (size_t idx : orderedMsgs_) {
    const auto& m = msgs[idx];
    auto toIt = actorToLane_.find(m.to);
    if (toIt == actorToLane_.end()) continue;
    int toLane = toIt->second;

    double x1 = timeToX(m.start);
    double x2 = timeToX(m.end);
    double y = laneY(toLane);

    if (x2 < areaX0 - 40 || x1 > areaX1 + 40) continue;

    Rgba color = colorFor(idx);
    Rgba borderColor = Rgba(
        (uint8_t)std::min(255, (int)color.r + 40),
        (uint8_t)std::min(255, (int)color.g + 40),
        (uint8_t)std::min(255, (int)color.b + 40),
        255);

    int bx1 = (int)x1;
    int bx2 = (int)x2;
    if (bx2 - bx1 < 2) bx2 = bx1 + 2;
    int by1 = (int)y - blockHalfH;
    int by2 = (int)y + blockHalfH;

    DrawRectangle(sprite, Vec2Si32(bx1, by1), Vec2Si32(bx2, by2), color);
    DrawLine(sprite, Vec2Si32(bx1, by1), Vec2Si32(bx2, by1), borderColor);
    DrawLine(sprite, Vec2Si32(bx2, by1), Vec2Si32(bx2, by2), borderColor);
    DrawLine(sprite, Vec2Si32(bx2, by2), Vec2Si32(bx1, by2), borderColor);
    DrawLine(sprite, Vec2Si32(bx1, by2), Vec2Si32(bx1, by1), borderColor);

    std::string mt(m.messageType);
    if (VisualisationHelper::IsShortMessageTypeActivate()) {
      mt = Logs::GetShortLogMessageType(mt);
    }
    Vec2Si32 textSize = g_large_font.EvaluateSize(mt.c_str(), false);
    int blockW = bx2 - bx1;
    int tx;
    int ty = by1 + (by2 - by1 + textSize.y) / 2;
    bool drawInside = textSize.x + 6 <= blockW;
    if (drawInside) {
      tx = bx1 + (blockW - textSize.x) / 2;
      g_large_font.Draw(sprite, mt.c_str(), tx, ty,
                        kTextOriginTop, kTextAlignmentLeft,
                        kDrawBlendingModeColorize, kFilterNearest,
                        Rgba(0, 0, 0));
    } else {
      tx = bx2 + 4;
      DrawRectangle(sprite,
                    Vec2Si32(tx - 2, ty - textSize.y - 1),
                    Vec2Si32(tx + textSize.x + 2, ty + 1),
                    Rgba(0, 0, 0, 180));
      g_large_font.Draw(sprite, mt.c_str(), tx, ty,
                        kTextOriginTop, kTextAlignmentLeft,
                        kDrawBlendingModeColorize, kFilterNearest,
                        color);
    }
  }

  char header[256];
  VisualisationTime rootLatency = msgs[rootMsgIdx_].end - msgs[rootMsgIdx_].start;
  snprintf(header, sizeof(header),
           "CHAIN VIEW  root=%zu  msgs=%zu  actors=%zu  span=%s  root-latency=%s   [N/C] close  [wheel] zoom  [drag] scroll time",
           rootMsgIdx_, orderedMsgs_.size(), laneActors_.size(),
           FormatTimeShort(span).c_str(),
           FormatTimeShort(rootLatency).c_str());
  g_large_font.Draw(sprite, header, 10, size.y - 10,
                    kTextOriginTop, kTextAlignmentLeft,
                    kDrawBlendingModeColorize, kFilterNearest,
                    Rgba(255, 220, 40));
}

void TraceScreen::Listen() {
  if (IsKeyDownward(kKeyN)) {
    VisualisationHelper::SwitchTraceMode();
    return;
  }
  if (IsKeyDownward(kKeyC)) {
    VisualisationHelper::SwitchTraceMode();
    return;
  }
  MWindow::Listen();

  if (IsMouseIn() && GetMouse()->IsLeftDown()) {
    if (GetMouse()->GetFlag()) {
      GetMouse()->Listen();
      Vec2Si32 delta = GetMouse()->GetOffset() - GetMouse()->GetSafeOffset();
      Sprite sprite = GetDrawSprite();
      Vec2Si32 size = sprite.Size();
      int areaW = size.x - 280 - 40;
      if (areaW > 0 && timeScale_ > 0) {
        timeOffset_ -= (double)delta.x / (double)areaW / timeScale_;
      }
    }
    GetMouse()->SetFlag(true);
    GetMouse()->SafeOffset();
  } else {
    GetMouse()->SetFlag(false);
  }

  if (IsMouseIn()) {
    double wheel = MouseWheelDelta();
    if (wheel != 0.0) {
      Sprite sprite = GetDrawSprite();
      Vec2Si32 size = sprite.Size();
      int areaX0 = 280;
      int areaW = size.x - 280 - 40;
      if (areaW > 0) {
        Vec2Si32 m = MousePos();
        double cursorFrac = (double)(m.x - areaX0) / (double)areaW;
        double cursorAbs = timeOffset_ + cursorFrac / timeScale_;
        double factor = (wheel > 0) ? 1.15 : 1.0 / 1.15;
        timeScale_ *= factor;
        if (timeScale_ < 0.05) timeScale_ = 0.05;
        if (timeScale_ > 2000.0) timeScale_ = 2000.0;
        timeOffset_ = cursorAbs - cursorFrac / timeScale_;
      }
    }
  }
}
