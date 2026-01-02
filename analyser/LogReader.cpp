#include "LogReader.hpp"

#include "engine/easy_util.h"
#include <algorithm>
#include <fstream>
#include <map>
#include <queue>
#include <set>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string_view>

std::vector<uint8_t> LogReader::data_ = {};
std::vector<std::string_view> LogReader::logLines_ = {};

void LogReader::ReadFile(const std::string_view fileName) {
  data_.clear();
  ReadFile_(fileName, &data_);

  size_t cntLogLines = 0;
  for (uint8_t chr : data_) {
    if (chr == '\n') {
      cntLogLines++;
    }
  }

  logLines_.clear();
  logLines_.reserve(cntLogLines);

  size_t lastLogLineStart = 0;
  for (size_t charIndex = 0; charIndex < data_.size(); ++charIndex) {
    if (data_[charIndex] == '\n') {
      char* startLogLine = reinterpret_cast<char*>(data_.data() + lastLogLineStart);
      char* endLogLine = reinterpret_cast<char*>(data_.data() + charIndex);

      if (std::string_view(startLogLine, startLogLine + 3) != "Sen" &&
          std::string_view(startLogLine, startLogLine + 3) != "Rec" &&
          std::string_view(startLogLine, startLogLine + 3) != "New" && 
          std::string_view(startLogLine, startLogLine + 3) != "Die" &&
          std::string_view(startLogLine, startLogLine + 3) != "Tab" &&
          std::string_view(startLogLine, startLogLine + 3) != "Pip" &&
          std::string_view(startLogLine, startLogLine + 3) != "2Ta") {
        lastLogLineStart = charIndex + 1;
        continue;
      }

      logLines_.emplace_back(startLogLine, endLogLine);
      lastLogLineStart = charIndex + 1;
    }
  }
}

const std::vector<uint8_t>& LogReader::GetData() {
  return data_;
}

const std::vector<std::string_view>& LogReader::GetLogLines() {
  return logLines_;
}

void LogReader::ReadFile_(const std::string_view fileName, std::vector<uint8_t>* data) {
  std::ifstream in(std::string(fileName), std::ios_base::in | std::ios_base::binary);
  
  if (in.rdstate() & std::ios_base::failbit) {
    throw std::runtime_error("Error in ReadFile. Can't open the file");
  }
  
  in.seekg(0, std::ios_base::end);
  
  if (in.rdstate() & std::ios_base::failbit) {
    throw std::runtime_error("Error in ReadFile. Can't seek to the end");
  }
  
  std::streampos endPos = in.tellg();
  
  if (endPos == std::streampos(EOF)) {
    throw std::runtime_error("Error in ReadFile. Can't determine file size via tellg");
  }
  
  in.seekg(0, std::ios_base::beg);
  
  if (in.rdstate() & std::ios_base::failbit) {
    throw std::runtime_error("Error in ReadFile. Can't seek to the beg");
  }
  
  // std::vector<uint8_t>* std::vector<uint8_t>;
  if (static_cast<size_t>(endPos) > 0) {
    data->resize(endPos);
    in.read(reinterpret_cast<char*>(data->data()), endPos);
    
    if ((in.rdstate() & (std::ios_base::failbit | std::ios_base::eofbit))
        == (std::ios_base::failbit | std::ios_base::eofbit)) {
      throw std::runtime_error("Error in ReadFile. Can't read the data, eofbit is set");
    }
    
    if (in.rdstate() & std::ios_base::badbit) {
      throw std::runtime_error("Error in ReadFile. Can't read the data, badbit is set");
    }
    
    if (in.rdstate() != std::ios_base::goodbit) {
      throw std::runtime_error("Error in ReadFile. Can't read the data, non-goodbit");
    }
  }
  
  in.close();
  if (in.rdstate() & std::ios_base::failbit) {
    throw std::runtime_error("Error in ReadFile. Can't close the file");
  }
}
