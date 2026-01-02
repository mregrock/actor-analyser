#include "Logs.hpp"

#include <map>
#include <set>

std::vector<std::string_view> Logs::logLines_ = {};
std::map<ActorIdx, std::string_view> Logs::actorNumIdToRealActorId_ = {};
std::map<std::string_view, ActorIdx> Logs::realActorIdToActorNumId_ = {};
std::vector<Logs::RealLogLine> Logs::realLogLines_ = {};
std::vector<Logs::ParsedLogLine> Logs::parsedLogLines_ = {};
std::map<std::string_view, std::vector<std::size_t>> Logs::messageToParsedLineInd_ = {};
std::vector<Logs::LogMessage> Logs::logMessages_ = {};
std::map<std::string_view, std::vector<ActorIdx>> Logs::actorTypeToActorId_ = {};
std::map<ActorIdx, std::string_view> Logs::actorIdToActorType_ = {};
std::vector<Logs::NewDieLogLine> Logs::newDieLogLines_ = {};
std::map<ActorIdx, std::pair<VisualisationTime, VisualisationTime>> Logs::lifeTime_ = {};
ActorIdx Logs::maxActorId_ = 0;
VisualisationTime Logs::oldMinTime_ = 0;
std::map<std::string_view, std::string_view> Logs::actorTypesMap_ = {};
std::vector<uint8_t> Logs::actorTypesMapConfigData_ = {};

std::map<ActorIdx, std::map<VisualisationTime, VisualisationTime>> Logs::actorActivityTime_ = {};

std::vector<std::pair<ActorIdx, std::string_view>> Logs::threadActorIds_ = {};

std::set<ActorIdx> Logs::tabletIds_ = {};

std::ostream& operator<<(std::ostream& os, const Logs::LogMessage& lm) {
  return os << "[" 
    << lm.to << ","
    << lm.from << ","
    << lm.start << ","
    << lm.end << ","
    << lm.message
    << "]";
}
