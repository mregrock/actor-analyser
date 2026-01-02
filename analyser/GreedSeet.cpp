#include "GreedSeet.hpp"

#include "arctic/engine/vec2si32.h"
#include <algorithm>
#include <fstream>
#include <set>

#include "ActorTable.hpp"
#include "Logs.hpp"
#include "VisualisationHelper.hpp"
#include "globals.h"

using namespace arctic;


void GreedSeet::ReadConfig(std::string filename) {
  std::ifstream file(filename);
  arctic::Check(file.is_open(), (char*)u8"Не удалось открыть файл: ", filename.c_str());
  std::string line;
  size_t currentLayerIndex = (size_t)-1;
  while (std::getline(file, line)) {
    line.erase(0, line.find_first_not_of(" \t"));
    line.erase(line.find_last_not_of(" \t") + 1);
    if (!line.empty()) {
      if (line[0] == '#') {
        std::string layerName = line.substr(1);
        currentLayerIndex = g_layer_names.size();
        g_layer_names.push_back(layerName);
      } else {
        Check(currentLayerIndex != (size_t)-1, "No layer for actor", line.c_str());
        actorNameSeet_[line] = currentLayerIndex;
      }
    }
  }
  file.close();
  
  std::reverse(g_layer_names.begin(), g_layer_names.end());
  for (auto it = actorNameSeet_.begin(); it != actorNameSeet_.end(); ++it) {
    it->second = g_layer_names.size() - 1 - it->second;
  }
}

void GreedSeet::PrepareTables() {
  ReadConfig("data/seet.config");

  const std::map<std::string_view, std::vector<ActorIdx>>& name_id = Logs::GetActorTypeToActorId();
  const std::map<ActorIdx, std::string_view>& id_name = Logs::GetActorIdToActorType();

  for (std::pair<std::string, size_t> it : actorNameSeet_) {
    if (it.second >= tables_.size()) {
      tables_.resize(it.second + 1);
    }

    if (name_id.count(it.first)) {
      for (ActorIdx actor_idx : name_id.at(it.first)) {
        if (!VisualisationHelper::IsBirthActor(actor_idx)) {
          continue;
        }
        auto life_it = Logs::lifeTime_.find(actor_idx);
        Ui64 start_time = 0;
        if (life_it != Logs::lifeTime_.end()) {
          start_time = life_it->second.first;
        }
        tables_[it.second].insert({start_time, actor_idx});
      }
    }
  }

  for (std::pair<ActorIdx, std::string_view> it : id_name) {
    ActorIdx actor_idx = it.first;
    if (!actorNameSeet_.count(std::string(it.second))) {
      if (!VisualisationHelper::IsBirthActor(actor_idx)) {
        continue;
      }
      if (tables_.size() == 0) {
        tables_.resize(1);
      }
      auto life_it = Logs::lifeTime_.find(actor_idx);
      Ui64 start_time = 0;
      if (life_it != Logs::lifeTime_.end()) {
        start_time = life_it->second.first;
      }
      tables_[0].insert({start_time, actor_idx});
    }
  }
  
  Si32 table_width = windowSize_.x;

  actorTables_.resize(tables_.size());
  for (size_t i = 0; i < tables_.size(); ++i) {
    actorTables_[i].SetLineLength(table_width);
  }

  for (size_t tableNum = 0; tableNum < tables_.size(); ++tableNum) {
    for (const auto &[time, actorId] : tables_[tableNum]) {
      actorTables_[tableNum].AddActor(actorId);
    }
  }

  actorTables_[0].SetYAdd(0);
  for (size_t tableNum = 1; tableNum < tables_.size(); ++tableNum) {
      actorTables_[tableNum].SetYAdd(actorTables_[tableNum-1].GetY() + 20);
  }
  
  coords_.resize(Logs::GetMaxActorId() + 1);


  for (size_t i = 0; i < tables_.size(); ++i) {
    for (const auto &[time, actorId] : tables_[i]) {
      coords_[actorId] = actorTables_[i].GetLeftDownCornerPosition(actorId);
      coordedId_.insert(actorId);
    }
  }
}

GreedSeet::GreedSeet(std::pair<uint64_t, uint64_t> windowSize, uint64_t actorRadius) {
  windowSize_.x = (Si32)windowSize.first;
  windowSize_.y = (Si32)windowSize.second;
  actorRadius_ = actorRadius;
}

 Vec2Si32 GreedSeet::GetCoord(ActorIdx id) {
  return coords_[id];
}


