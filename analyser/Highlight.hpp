#pragma once

#include "globals.h"

#include <cstddef>
#include <unordered_set>
#include <vector>

namespace Highlight {

void Init();
bool IsActive();
bool IsMsgInChain(size_t msgIdx);
bool IsActorInChain(ActorIdx id);
bool IsAncestor(size_t msgIdx);
bool IsDescendant(size_t msgIdx);
bool IsRoot(size_t msgIdx);

size_t ChainMsgCount();
size_t ChainActorCount();
size_t RootMsgIdx();

const std::unordered_set<size_t>& ChainMsgs();
const std::unordered_set<ActorIdx>& ChainActors();

void SelectChain(size_t msgIdx);
void Clear();
bool TrySelectFromNearest();

}  // namespace Highlight
