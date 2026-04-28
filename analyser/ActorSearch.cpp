#include "ActorSearch.hpp"

#include "Camera.hpp"
#include "GreedSeet.hpp"
#include "Logs.hpp"
#include "globals.h"

#include "engine/easy.h"
#include "engine/easy_drawing.h"
#include "engine/easy_input.h"
#include "engine/font.h"
#include "engine/rgba.h"
#include "engine/vec2f.h"
#include "engine/vec2si32.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

using namespace arctic;

extern GreedSeet* g_pgseet;

namespace ActorSearch {

namespace {

struct Match {
  ActorIdx id = -1;
  std::string name;
  int score = 100;
  bool visible_now = false;
};

std::string s_query;
std::string s_status = "Ctrl+F actor search";
std::vector<Match> s_matches;
int s_current = 0;
bool s_active = false;
bool s_dirty = true;

bool IsControlDown() {
  return IsKeyDown(kKeyControl) || IsKeyDown(kKeyLeftControl) || IsKeyDown(kKeyRightControl);
}

bool IsShiftDown() {
  return IsKeyDown(kKeyShift) || IsKeyDown(kKeyLeftShift) || IsKeyDown(kKeyRightShift);
}

std::string ToLower(std::string_view value) {
  std::string out;
  out.reserve(value.size());
  for (char c : value) {
    out.push_back((char)std::tolower((unsigned char)c));
  }
  return out;
}

bool StartsWith(std::string_view value, std::string_view prefix) {
  return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

std::string Truncate(std::string value, size_t max_len) {
  if (value.size() <= max_len) return value;
  if (max_len <= 1) return "~";
  value.resize(max_len - 1);
  value.push_back('~');
  return value;
}

int MatchScore(const std::string& name_lower, ActorIdx id, const std::string& query_lower) {
  if (query_lower.empty()) return 100;

  std::string id_str = std::to_string(id);
  std::string hash_id = "#" + id_str;
  if (hash_id == query_lower || id_str == query_lower) return 0;
  if (StartsWith(hash_id, query_lower) || StartsWith(id_str, query_lower)) return 1;
  if (name_lower == query_lower) return 2;
  if (StartsWith(name_lower, query_lower)) return 3;
  if (name_lower.find(query_lower) != std::string::npos) return 4;
  return 100;
}

void RebuildMatches() {
  s_matches.clear();
  s_current = 0;

  std::string query_lower = ToLower(s_query);
  if (query_lower.empty()) {
    s_status = "type actor name or #id";
    s_dirty = false;
    return;
  }

  const auto& id_to_type = Logs::GetActorIdToActorType();
  for (ActorIdx id = 0; id < (ActorIdx)g_actors.size(); ++id) {
    if (!g_pgseet || !g_pgseet->HaveCoord(id)) continue;

    auto it = id_to_type.find(id);
    std::string name = (it != id_to_type.end()) ? std::string(it->second) : std::string("UNKNOWN");
    int score = MatchScore(ToLower(name), id, query_lower);
    if (score >= 100) continue;

    bool visible_now = id >= 0 && id < (ActorIdx)g_actors.size() && g_actors[id].visible_;
    s_matches.push_back(Match{id, std::move(name), score, visible_now});
  }

  std::sort(s_matches.begin(), s_matches.end(), [](const Match& lhs, const Match& rhs) {
    if (lhs.score != rhs.score) return lhs.score < rhs.score;
    if (lhs.name != rhs.name) return lhs.name < rhs.name;
    return lhs.id < rhs.id;
  });

  if (s_matches.empty()) {
    s_status = "no actors found";
  } else if (s_matches.size() == 1) {
    s_status = "1 match: Enter center";
  } else {
    s_status = std::to_string(s_matches.size()) + " matches: Tab/arrows choose, Enter center";
  }
  s_dirty = false;
}

void StepSelection(int delta) {
  if (s_dirty) RebuildMatches();
  if (s_matches.empty()) return;
  int count = (int)s_matches.size();
  s_current = (s_current + delta) % count;
  if (s_current < 0) s_current += count;
}

void CenterCurrent() {
  if (s_dirty) RebuildMatches();
  if (s_matches.empty()) return;

  const Match& match = s_matches[s_current];
  if (match.id < 0 || match.id >= (ActorIdx)g_actors.size()) return;

  const ActorRec& actor = g_actors[match.id];
  Vec2F actor_center = Vec2F(actor.offset_) + actor.visual_size * 0.5f;
  g_camera.SetOffset(Vec2Si32(actor_center));

  char buf[256];
  snprintf(buf, sizeof(buf), "centered #%lld %s (%d/%zu)",
           (long long)match.id, match.name.c_str(), s_current + 1, s_matches.size());
  s_status = buf;
}

bool AppendTextInput() {
  bool changed = false;
  for (Si32 i = 0; i < InputMessageCount(); ++i) {
    const InputMessage& msg = GetInputMessage(i);
    if (msg.kind != InputMessage::kKeyboard) continue;
    if (msg.keyboard.key_state != 1) continue;
    if (!msg.keyboard.characters[0]) continue;

    for (const char* p = msg.keyboard.characters; *p; ++p) {
      unsigned char c = (unsigned char)*p;
      if (c < 32 || c > 126) continue;
      s_query.push_back((char)c);
      changed = true;
    }
  }
  return changed;
}

void DrawSelectionMarker(Sprite sprite) {
  if (s_dirty) RebuildMatches();
  if (s_matches.empty()) return;

  ActorIdx id = s_matches[s_current].id;
  if (id < 0 || id >= (ActorIdx)g_actors.size()) return;

  const ActorRec& actor = g_actors[id];
  Vec2F center = g_camera.WorldToScreen(Vec2F(actor.offset_) + actor.visual_size * 0.5f);
  Vec2F half_size = actor.visual_size * (float)g_camera.GetScaleFactor() * 0.5f;
  int pad = std::max(4, (int)(4.0 * g_camera.GetScaleFactor()));

  Vec2Si32 a((int)(center.x - half_size.x) - pad, (int)(center.y - half_size.y) - pad);
  Vec2Si32 b((int)(center.x + half_size.x) + pad, (int)(center.y + half_size.y) + pad);
  Rgba color(255, 220, 40);
  DrawLine(sprite, Vec2Si32(a.x, a.y), Vec2Si32(b.x, a.y), color);
  DrawLine(sprite, Vec2Si32(b.x, a.y), Vec2Si32(b.x, b.y), color);
  DrawLine(sprite, Vec2Si32(b.x, b.y), Vec2Si32(a.x, b.y), color);
  DrawLine(sprite, Vec2Si32(a.x, b.y), Vec2Si32(a.x, a.y), color);
}

}  // namespace

bool IsActive() {
  return s_active;
}

void Reset() {
  s_query.clear();
  s_status = "Ctrl+F actor search";
  s_matches.clear();
  s_current = 0;
  s_active = false;
  s_dirty = true;
}

void HandleInput() {
  if (IsControlDown() && IsKeyDownward(kKeyF)) {
    s_active = true;
    s_dirty = true;
    return;
  }

  if (!s_active) return;

  if (IsKeyDownward(kKeyEscape)) {
    s_active = false;
    return;
  }

  bool changed = false;
  if (IsKeyDownward(kKeyBackspace) && !s_query.empty()) {
    s_query.pop_back();
    changed = true;
  }
  if (IsKeyDownward(kKeyDelete) && !s_query.empty()) {
    s_query.clear();
    changed = true;
  }

  if (!IsControlDown() && AppendTextInput()) {
    changed = true;
  }

  if (changed) {
    s_dirty = true;
    RebuildMatches();
  } else if (s_dirty) {
    RebuildMatches();
  }

  if (IsKeyDownward(kKeyTab)) {
    StepSelection(IsShiftDown() ? -1 : 1);
  }
  if (IsKeyDownward(kKeyDown)) {
    StepSelection(1);
  }
  if (IsKeyDownward(kKeyUp)) {
    StepSelection(-1);
  }
  if (IsKeyDownward(kKeyEnter)) {
    CenterCurrent();
  }
}

void Draw() {
  if (!s_active) return;
  if (s_dirty) RebuildMatches();

  Sprite sprite = GetEngine()->GetBackbuffer();
  Vec2Si32 screen = ScreenSize();
  const int x0 = 12;
  const int y1 = screen.y - 54;
  const int width = 640;
  const int row_h = 14;
  const int max_rows = 8;
  int visible_rows = std::min(max_rows, (int)s_matches.size());
  int height = 62 + visible_rows * row_h;
  int y0 = y1 - height;

  DrawRectangle(sprite, Vec2Si32(x0, y0), Vec2Si32(x0 + width, y1), Rgba(0, 0, 0, 220));

  std::string title = "Actor search: " + s_query + "_";
  g_font.Draw(sprite, title.c_str(), x0 + 10, y1 - 8,
              kTextOriginTop, kTextAlignmentLeft,
              kDrawBlendingModeColorize, kFilterNearest,
              Rgba(255, 255, 255));

  g_font.Draw(sprite, s_status.c_str(), x0 + 10, y1 - 22,
              kTextOriginTop, kTextAlignmentLeft,
              kDrawBlendingModeColorize, kFilterNearest,
              Rgba(180, 220, 255));

  const char* hint = "Esc close | Enter center | Tab/Shift+Tab or arrows choose | Delete clear";
  g_font.Draw(sprite, hint, x0 + 10, y1 - 36,
              kTextOriginTop, kTextAlignmentLeft,
              kDrawBlendingModeColorize, kFilterNearest,
              Rgba(170, 170, 170));

  if (!s_matches.empty()) {
    int start = std::max(0, s_current - max_rows / 2);
    start = std::min(start, std::max(0, (int)s_matches.size() - max_rows));
    int y = y1 - 54;
    for (int row = 0; row < visible_rows; ++row) {
      int idx = start + row;
      const Match& match = s_matches[idx];
      std::string line = (idx == s_current ? "> " : "  ");
      line += "#" + std::to_string(match.id) + "  " + match.name;
      if (!match.visible_now) line += "  (hidden now)";
      line = Truncate(line, 92);

      Rgba color = (idx == s_current) ? Rgba(255, 220, 40)
                                      : (match.visible_now ? Rgba(220, 220, 220) : Rgba(120, 120, 120));
      g_font.Draw(sprite, line.c_str(), x0 + 10, y,
                  kTextOriginTop, kTextAlignmentLeft,
                  kDrawBlendingModeColorize, kFilterNearest,
                  color);
      y -= row_h;
    }
  }

  DrawSelectionMarker(sprite);
}

}  // namespace ActorSearch
