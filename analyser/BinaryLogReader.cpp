#include "BinaryLogReader.hpp"

BinaryFileHeader BinaryLogReader::header_ = {};
std::vector<BinaryEvent> BinaryLogReader::events_ = {};
std::map<uint32_t, std::string> BinaryLogReader::activityDict_ = {};
std::map<uint32_t, std::string> BinaryLogReader::eventNamesDict_ = {};
std::map<uint32_t, std::string> BinaryLogReader::threadPoolDict_ = {};
std::vector<std::string> BinaryLogReader::stringStorage_ = {};
