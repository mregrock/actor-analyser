#pragma once

#include "engine/arctic_types.h"
#include "engine/vec2si32.h"
#include "Logs.hpp"
#include "LogReader.hpp"
#include <vector>
#include "RectWinDraw.hpp"
#include <set>

#include "ActorTable.hpp"
#include "engine/easy_input.h"
#include "IListener.hpp"

using namespace arctic;

class GreedSeet : public IListener {
public:
  GreedSeet(std::pair<uint64_t, uint64_t> windowSize, uint64_t actorRadius);
  Vec2Si32 GetCoord(ActorIdx id);

  
  bool HaveCoord(ActorIdx id) const {
    return coordedId_.count(id);
  }
  void ReadConfig(std::string fileName);
  void PrepareTables();
  
  void Listen() override {
  }
  
  void Clear() {
    actorTables_.clear();
    tables_.clear();
    coords_.clear();
    coordedId_.clear();
  }
  

  std::vector<ActorTable> actorTables_;
  std::vector<std::multimap<Ui64, ActorIdx>> tables_;
  std::vector< Vec2Si32> coords_;
  
  std::map<std::string, size_t> actorNameSeet_;
  std::set<ActorIdx> coordedId_;
  
  Vec2Si32 windowSize_;
  uint64_t actorRadius_;

};
