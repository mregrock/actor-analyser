#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <tuple>
#include <functional>
#include "engine/vec2f.h"
#include "engine/scalar_math.h"

using namespace arctic;

using ActorIdx = int64_t;
using VisualisationTime = int64_t;

struct TupleHash {
    
    template <class T>
    void hash_combine(size_t& seed, const T& val) const {
        std::hash<T> hasher;
        seed ^= hasher(val) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }

    size_t operator()(const std::tuple<int64_t, int64_t>& key) const {
        size_t seed = 0;
        hash_combine(seed, get<0>(key));
        hash_combine(seed, get<1>(key));
        return seed;
    }
};


struct Camera;
namespace arctic {
  class Font;
}

struct ActorRec {
  Vec2Si32 offset_;
  Vec2F visual_size;
  std::string text;
  
  uint64_t id_;
  bool visible_;
  bool active_;
};

struct MessageRec {
  ActorIdx from;
  ActorIdx to;
  bool is_visible = false;
  VisualisationTime start;
  VisualisationTime end;
  std::string_view message;
  std::string_view messageType;
  size_t id;
};

struct ArrowPoint {
  double length_part;
  Vec2F position;
};

struct ArrowRec {
  ActorIdx from = 0;
  ActorIdx to = 0;
  Ui64 last_update_frame = 0;
  Ui64 last_draw_frame = 0;
  std::vector<ArrowPoint> points;
  
  Vec2F PositionAtPart(double part) {
    if (points.empty()) {
      return Vec2F(0, 0);
    }
    size_t end_idx = 0;
    while (end_idx < points.size()-1 && part > points[end_idx].length_part) {
      ++end_idx;
    }
    if (end_idx == 0) {
      return points[0].position;
    }
    ArrowPoint &p1 = points[end_idx - 1];
    ArrowPoint &p2 = points[end_idx];
    double intervalPart = Clamp((part - p1.length_part) / (p2.length_part - p1.length_part), 0.0, 1.0);
    return Vec2F(Lerp(p1.position.x, p2.position.x, (float)intervalPart), Lerp(p1.position.y, p2.position.y, (float)intervalPart));
  }
};

enum TimeMode {
  kTimeNormal,
  kTimeAdaptive
};

extern Camera g_camera;
extern arctic::Font g_font;
extern arctic::Font g_large_font;

extern std::vector<ActorRec> g_actors;
extern std::vector<MessageRec> g_messages;
extern std::unordered_map<std::tuple<ActorIdx, ActorIdx>, ArrowRec, TupleHash> g_arrows;
extern Ui64 g_update_frame;
extern double g_dt;
extern double g_speed;
extern double g_is_pause;
extern TimeMode g_time_mode;
extern std::vector<std::string> g_layer_names;

extern std::stringstream g_log;
