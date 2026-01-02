#include "TraceScreen.hpp"
#include "arctic/engine/vec2si32.h"
#include "engine/easy_drawing.h"
#include "engine/easy_input.h"
#include "engine/miniz.h"
#include "Logs.hpp"
#include "VisualisationHelper.hpp"
#include <_types/_uint64_t.h>
#include <cstdint>
#include "globals.h"

Sprite TraceScreen::GetDrawSprite() const { return GetFrameSprite(); }
void TraceScreen::SetDrawSprite( Sprite sprite) { SetSprite(sprite); }

const Window* TraceScreen::GetWindow() const { return this; }

Si32 TraceScreen::Draw_(uint64_t msg_idx, Si32 delta) const {
  Si64 x1 = (Si32)(g_messages[msg_idx].start - curMinStartTime_);
  Si64 x2 = (Si32)(g_messages[msg_idx].end - curMinStartTime_);
  x1 = x1 * 1920 / (curMaxEndTime_-curMinStartTime_);
  x2 = x2 * 1920 / (curMaxEndTime_-curMinStartTime_);
  Vec2F ll = g_camera.WorldToScreen(Vec2F(x1, delta-30));
  Vec2F ur = g_camera.WorldToScreen(Vec2F(x2, delta));
  DrawRectangle(GetDrawSprite(), Vec2Si32(ll), Vec2Si32(ur), Rgba(40, 50, 110));
  std::string messageType = (std::string)g_messages[msg_idx].messageType;
  if (VisualisationHelper::IsShortMessageTypeActivate()) {
    messageType = Logs::GetShortLogMessageType(messageType);
  }
  g_large_font.Draw(GetEngine()->GetBackbuffer(), messageType.c_str(), Vec2Si32(ll).x, Vec2Si32(ll).y,
                    kTextOriginBottom,
                    kTextAlignmentLeft,
                    kDrawBlendingModeColorize,
                    kFilterNearest,
                    Rgba(255,255,255));
  Si32 lastDelta = delta - 40;
  for (size_t nextMessageInd : Logs::GetLogMessages()[msg_idx].child_msg_idxs) {
    lastDelta = Draw_(nextMessageInd, lastDelta) - 10;
  }
  return lastDelta;
}

void TraceScreen::Draw() const {
  DrawRectangle(GetDrawSprite(),  Vec2Si32(0, 0), GetDrawSprite().Size(), Rgba(0,0,0));
  Draw_(messageTraces_[0], ScreenSize().y - 40);
}

void TraceScreen::Listen()  {
  if (IsKeyDownward(kKeyN)) {
    VisualisationHelper::SwitchTraceMode();
  }
  MWindow::Listen();
  
  if (IsMouseIn() && GetMouse()->IsLeftDown()) {
    if (GetMouse()->GetFlag()) {
      GetMouse()->Listen();
      g_camera.SetOffset(g_camera.GetOffset() -
                         Vec2Si32( Vec2D(GetMouse()->GetOffset() - GetMouse()->GetSafeOffset()) /
                                  g_camera.GetScaleFactor()));
    }
    
    GetMouse()->SetFlag(true);
    GetMouse()->SafeOffset();
  } else {
    GetMouse()->SetFlag(false);
  }
  
  if (IsMouseIn()) {
    g_camera.SetScaleFactor(g_camera.GetScaleFactor() - MouseWheelDelta() * 0.001);
  }
}

void TraceScreen::SetBackgroundColor(Rgba color) {
  backgroundColor_ = color;
}

void TraceScreen::CreateMessageTraces(uint64_t messageId) {
  messageTraces_.clear();
  curMinStartTime_ = UINT64_MAX;
  curMaxEndTime_ = 0;
  std::queue<uint64_t> messageIdQueue;
  messageIdQueue.push(messageId);
  messageTraces_.emplace_back(messageId);
  while (!messageIdQueue.empty()) {
    uint64_t curMessageId = messageIdQueue.front();
    messageIdQueue.pop();
    for (uint64_t it_idx : Logs::GetLogMessages()[curMessageId].child_msg_idxs) {
      messageTraces_.emplace_back(it_idx);
      messageIdQueue.push(it_idx);
    }
  }
  for (const Ui64 idx : messageTraces_) {
    curMinStartTime_ = std::min(curMinStartTime_, (Ui64)g_messages[idx].start);
    curMaxEndTime_ = std::max(curMaxEndTime_, (Ui64)g_messages[idx].end);
  }
}
