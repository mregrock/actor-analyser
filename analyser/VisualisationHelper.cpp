#include "VisualisationHelper.hpp"
#include "Logs.hpp"
#include <cstdint>
#include <unordered_map>

std::set<size_t> VisualisationHelper::selectedMessages_ = {};
std::map<size_t, Rgba> VisualisationHelper::selectedMessageColor_ = {};
std::unordered_map<size_t, Rgba> VisualisationHelper::messageColor_ = {};
bool VisualisationHelper::shortMessageNamesActivate_ = true;
bool VisualisationHelper::onlyBirth_ = false;
std::set<ActorIdx> VisualisationHelper::birthActors_ = {};

int64_t VisualisationHelper::normalisationCoef_ = 0;
uint64_t VisualisationHelper::lastSelectedMessageId_ = 0;

bool VisualisationHelper::isTraceMode_ = false;
