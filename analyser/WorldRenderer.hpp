#pragma once

#include "globals.h"

class GreedSeet;

namespace WorldRenderer {

void DrawActor(const ActorRec& a);
void UpdateArrow(ArrowRec& arrow);
void DrawArrow(ArrowRec& arrow);
void DrawMessage(ArrowRec& arrow, MessageRec& msg, double d_time, arctic::Rgba color);
void DrawArrowAndMessage(MessageRec& m);
void DrawLayers(GreedSeet* gs);

}  // namespace WorldRenderer
