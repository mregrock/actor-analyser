#include "ActorTable.hpp"
#include "engine/vec2si32.h"
#include "engine/font.h"
#include "globals.h"


using namespace arctic;

void ActorTable::AddActor(ActorIdx id) {
  Vec2Si32 curBlockSize = g_font.EvaluateSize(std::string(Logs::GetActorIdToActorType().at(id)).c_str(), false);
  blocksSize_.push_back(curBlockSize);
  idToPositionInd_[id] = positions_.size();
  
  Si32 actor_width = curBlockSize.x + kXDelta;
  auto it = Logs::lifeTime_.find(id);
  Ui64 start_time = 0;
  Ui64 end_time = UINT64_MAX;
  if (it != Logs::lifeTime_.end()) {
    start_time = it->second.first;
    end_time = it->second.second;
  }
  Si32 line = -1;
  Si32 pos_x = -1;
  while (pos_x < 0) {
    ++line;
    if (managers.size() <= line) {
      managers.emplace_back();
      managers.back().Reset(lineLength_);
    }
    SegmentManager &man = managers[line];
    pos_x = man.addSegment(actor_width, start_time, end_time);
  }
  Vec2Si32 urc = Vec2Si32(curBlockSize + Vec2Si32(pos_x, line * (curBlockSize.y + kYDelta)));
  positions_.push_back(urc);
  
  max_urc.x = std::max(max_urc.x, urc.x);
  max_urc.y = std::max(max_urc.y, urc.y);
}
