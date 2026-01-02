#pragma once

#include "Logs.hpp"
#include <_types/_uint64_t.h>
#include <cstdint>
#include <unordered_map>
#include "RectangleWindow.hpp"
#include "Drawer.hpp"

class TraceScreen : public RectangleWindow, public Drawer {
public:
  struct MessageTrace {
    MessageTrace(VisualisationTime startTime, 
                 VisualisationTime endTime,
                 uint64_t messageId,
                 const std::vector<size_t>& childrenMessages) :
    startTime(startTime), endTime(endTime), messageId(messageId), childrenMessages(childrenMessages) {}
    
    VisualisationTime startTime, endTime;
    uint64_t messageId;
    std::vector<size_t> childrenMessages;
  };
  
  TraceScreen( Sprite sprite, Mouse *mouse)
  : TraceScreen(sprite) {
    SetMouse(mouse);
  }
  
  TraceScreen( Sprite sprite)
    : Window(sprite), Drawer(), RectangleWindow(sprite) {}
  
  TraceScreen() {}
  
  Sprite GetDrawSprite() const override;
  
  void SetDrawSprite( Sprite sprite) override;
  
  void Listen() override;
  
  const Window *GetWindow() const override;
  
  void Draw() const override;
  
  void SetBackgroundColor( Rgba color);
  
  void CreateMessageTraces(uint64_t messageId);
private:
  Si32 Draw_(uint64_t messageId, Si32 delta) const;
  
  mutable Rgba backgroundColor_=Rgba(0, 0, 0);
  
  std::vector<Ui64> messageTraces_;
  
  uint64_t curMinStartTime_ = UINT64_MAX;
  uint64_t curMaxEndTime_ = 0;
};
