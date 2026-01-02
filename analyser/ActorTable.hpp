#pragma once

#include "arctic/engine/vec2si32.h"
#include "engine/arctic_types.h"
#include <vector>
#include "Logs.hpp"
#include <map>

using namespace arctic;


class SegmentManager {
private:
    Si32 containerWidth;
    Ui64 currentTime;
    std::map<Si32, Si32> free_intervals_end_for_begin;
    std::multimap<Ui64, std::pair<Si32, Si32>> releases_pos_width_for_time;

  void subtractInterval(Si32 start, Si32 end) {
    auto iter = free_intervals_end_for_begin.lower_bound(start);
    if (iter != free_intervals_end_for_begin.begin()) {
      --iter;
      if (iter->second <= start) {
        ++iter;
      }
    }
    while (iter != free_intervals_end_for_begin.end() && iter->first < end) {
      Si32 intervalStart = iter->first;
      Si32 intervalEnd = iter->second;
      iter = free_intervals_end_for_begin.erase(iter);
      if (intervalStart < start) {
        free_intervals_end_for_begin[intervalStart] = start;
      }
      if (intervalEnd > end) {
        free_intervals_end_for_begin[end] = intervalEnd;
      }
    }
  }

  void addInterval(Si32 start, Si32 end) {
    if (start >= end) {
      return;
    }
    auto nextIter = free_intervals_end_for_begin.lower_bound(start);
    auto prevIter = nextIter;
    if (prevIter != free_intervals_end_for_begin.begin()) {
      --prevIter;
      if (prevIter->second < start) {
        prevIter = nextIter;
      }
    } else {
      prevIter = nextIter;
    }
    Si32 mergedStart = start;
    Si32 mergedEnd = end;
    if (prevIter != free_intervals_end_for_begin.end() && prevIter != nextIter && prevIter->second >= start) {
      mergedStart = std::min(mergedStart, prevIter->first);
      mergedEnd = std::max(mergedEnd, prevIter->second);
      nextIter = free_intervals_end_for_begin.erase(prevIter);
    }
    while (nextIter != free_intervals_end_for_begin.end() && nextIter->first <= mergedEnd) {
      mergedEnd = std::max(mergedEnd, nextIter->second);
      nextIter = free_intervals_end_for_begin.erase(nextIter);
    }
    free_intervals_end_for_begin[mergedStart] = mergedEnd;
  }
    
  Si32 findPositionForSegment(Si32 width) {
    for (const auto& [start, end] : free_intervals_end_for_begin) {
      Si32 intervalSize = end - start;
      if (intervalSize >= width) {
        return start;
      }
    }
    return -1;
  }
    
  void processReleaseEventsUntil(Ui64 time) {
    auto iter = releases_pos_width_for_time.begin();
    while (iter != releases_pos_width_for_time.end() && iter->first <= time) {
      Ui64 releaseTime = iter->first;
      Si32 position = iter->second.first;
      Si32 width = iter->second.second;
      addInterval(position, position + width);
      iter = releases_pos_width_for_time.erase(iter);
    }
    currentTime = time;
  }
    
public:
  SegmentManager() {
  }
  
  void Reset(Si32 container_width) {
    containerWidth = container_width;
    releases_pos_width_for_time.clear();
    free_intervals_end_for_begin.clear();
    free_intervals_end_for_begin[0] = containerWidth;
    currentTime = 0;
  }
    
  Si32 addSegment(Si32 width, Ui64 startTime, Ui64 endTime) {
    if (width > containerWidth) {
      return -1;
    }
    Check(startTime >= currentTime, "Segments must be added in increasing order of start time");
    processReleaseEventsUntil(startTime);
    Si32 position = findPositionForSegment(width);
    if (position < 0) {
      return -1;
    }
    subtractInterval(position, position + width);
    releases_pos_width_for_time.emplace(endTime, std::make_pair(position, width));
    return position;
  }
};

class ActorTable {
public:
  const Si32 kXDelta = 26;
  const Si32 kYDelta = 6;
  
  ActorTable() {};
  ActorTable(const ActorTable&) = delete;
  ~ActorTable() = default;
  ActorTable& operator=(const ActorTable&) = delete;
  ActorTable(const ActorTable&& actorTable) {
    idToPositionInd_ = std::move(actorTable.idToPositionInd_);
    blocksSize_ = std::move(actorTable.blocksSize_);
    positions_ = std::move(actorTable.positions_);
    lineLength_ = actorTable.lineLength_;
    yAdd_ = actorTable.yAdd_;
  }
  
  void AddActor(ActorIdx id);
  void SetLineLength( Si32 lineLength) { lineLength_ = lineLength; }
  
  void SetYAdd( Si32 yAdd) { yAdd_ = yAdd; }
  Si32 GetYAdd() const { return yAdd_; }
  
  void SetXAdd( Si32 xAdd) { xAdd_ = xAdd; }
  Si32 GetXAdd() const { return xAdd_; }
  
  Si32 GetY() const {
    return max_urc.y + yAdd_;
  }
  Si32 GetLineLength() const { return lineLength_; }
  
  
  Vec2Si32 GetLeftDownCornerPosition(ActorIdx id) const {
    size_t positionIndex = idToPositionInd_.at(id);
    
    return positions_[positionIndex] - blocksSize_[positionIndex] + Vec2Si32(xAdd_, yAdd_);
  }
  
  
  using RightUpKornerPositon =  Vec2Si32;
  using BlockSize =  Vec2Si32;
private:
  std::map<ActorIdx, size_t> idToPositionInd_;
  
  std::vector<BlockSize> blocksSize_;
  std::vector<RightUpKornerPositon> positions_;
  
  Si32 lineLength_ = 1920;
  
  Si32 yAdd_ = 0;
  Si32 xAdd_ = 0;
  std::deque<SegmentManager> managers;
  Vec2Si32 max_urc = Vec2Si32(0, 0);
};
