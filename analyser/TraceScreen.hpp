#pragma once

#include "Logs.hpp"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include "RectangleWindow.hpp"
#include "Drawer.hpp"

class TraceScreen : public RectangleWindow, public Drawer {
public:
  TraceScreen(Sprite sprite, Mouse* mouse)
    : TraceScreen(sprite) {
    SetMouse(mouse);
  }

  TraceScreen(Sprite sprite)
    : Window(sprite), Drawer(), RectangleWindow(sprite) {}

  TraceScreen() {}

  Sprite GetDrawSprite() const override;
  void SetDrawSprite(Sprite sprite) override;
  void Listen() override;
  const Window* GetWindow() const override;
  void Draw() const override;
  void SetBackgroundColor(Rgba color);

  void CreateMessageTraces(uint64_t messageId);

private:
  void BuildLayoutFromHighlight();

  mutable Rgba backgroundColor_ = Rgba(20, 20, 24);

  std::vector<size_t> orderedMsgs_;
  std::vector<ActorIdx> laneActors_;
  std::unordered_map<ActorIdx, int> actorToLane_;

  VisualisationTime minTime_ = 0;
  VisualisationTime maxTime_ = 0;
  size_t rootMsgIdx_ = 0;

  double timeScale_ = 1.0;
  double timeOffset_ = 0.0;
};
