#pragma once

#include "globals.h"

namespace FilterPanel {

void Init();
void Draw();
void HandleInput();
bool IsActorTypeVisibleForId(ActorIdx id);

}  // namespace FilterPanel
