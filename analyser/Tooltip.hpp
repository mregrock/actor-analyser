#pragma once

#include "globals.h"

namespace Tooltip {

void Init();
void DrawActorTooltip(ActorIdx id, VisualisationTime curTime);
void DrawMessageTooltip(size_t msgIdx);

}  // namespace Tooltip
