#pragma once

#include "engine/arctic_types.h"
#include "engine/vec2si32.h"
#include <map>
#include <span>
#include <string>
#include <sys/types.h>
#include <vector>

class LogReader {
public:
  static void ReadFile(const std::string_view fileName);
  static const std::vector<uint8_t>& GetData();
  
  static const std::vector<std::string_view>& GetLogLines();
  
private:
  static void ReadFile_(const std::string_view fileName, std::vector<uint8_t>* data);
  
  static std::vector<uint8_t> data_;
  static std::vector<std::string_view> logLines_;
  
  friend class Logs;
};
