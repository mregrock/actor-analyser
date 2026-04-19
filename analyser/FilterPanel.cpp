#include "FilterPanel.hpp"

#include "Logs.hpp"

#include "engine/easy.h"
#include "engine/easy_input.h"
#include "engine/rgba.h"
#include "engine/vec2si32.h"
#include "engine/font.h"

#include <algorithm>
#include <map>
#include <string>
#include <string_view>
#include <vector>

using namespace arctic;

extern arctic::Font g_font;

namespace FilterPanel {

namespace {

struct Group {
  std::string_view prefix;
  std::vector<std::string_view> types;
  size_t totalActors = 0;
  bool expanded = false;
};

struct Row {
  int groupIdx = -1;
  int typeIdx = -1;
  Vec2Si32 min;
  Vec2Si32 max;
};

std::map<std::string_view, bool> s_typeVisible;
std::vector<Group> s_groups;
std::vector<Row> s_rows;
bool s_panelVisible = true;

std::string_view ExtractPrefix(std::string_view s) {
  if (s.empty()) return s;
  size_t i = 1;
  for (; i < s.size(); ++i) {
    char c = s[i];
    if (c == '_') break;
    char prev = s[i - 1];
    bool prevLower = (prev >= 'a' && prev <= 'z');
    bool curUpper = (c >= 'A' && c <= 'Z');
    if (prevLower && curUpper) break;
  }
  return s.substr(0, i);
}

bool AllVisible(const Group& g) {
  for (auto t : g.types) {
    auto it = s_typeVisible.find(t);
    if (it != s_typeVisible.end() && !it->second) return false;
  }
  return true;
}

bool AnyVisible(const Group& g) {
  for (auto t : g.types) {
    auto it = s_typeVisible.find(t);
    if (it != s_typeVisible.end() && it->second) return true;
  }
  return false;
}

void SetGroupVisible(Group& g, bool v) {
  for (auto t : g.types) {
    s_typeVisible[t] = v;
  }
}

void ToggleGroup(Group& g) {
  bool anyOn = AnyVisible(g);
  SetGroupVisible(g, !anyOn);
}

}  // namespace

void Init() {
  s_typeVisible.clear();
  s_groups.clear();
  s_rows.clear();

  std::map<std::string_view, Group> byPrefix;
  for (const auto& [type, ids] : Logs::GetActorTypeToActorId()) {
    s_typeVisible[type] = true;
    std::string_view prefix = ExtractPrefix(type);
    Group& g = byPrefix[prefix];
    g.prefix = prefix;
    g.types.push_back(type);
    g.totalActors += ids.size();
  }

  for (auto& [prefix, g] : byPrefix) {
    std::sort(g.types.begin(), g.types.end());
    s_groups.push_back(std::move(g));
  }
  std::sort(s_groups.begin(), s_groups.end(),
            [](const Group& a, const Group& b) {
              if (a.totalActors != b.totalActors) return a.totalActors > b.totalActors;
              return a.prefix < b.prefix;
            });
}

bool IsActorTypeVisibleForId(ActorIdx id) {
  const auto& idToType = Logs::GetActorIdToActorType();
  auto it = idToType.find(id);
  if (it == idToType.end()) return true;
  auto vis = s_typeVisible.find(it->second);
  if (vis == s_typeVisible.end()) return true;
  return vis->second;
}

void Draw() {
  if (!s_panelVisible) return;
  if (s_groups.empty()) return;

  Sprite sprite = GetEngine()->GetBackbuffer();
  Vec2Si32 screen = ScreenSize();

  const int lineHeight = 12;
  const int colWidth = 260;
  const int panelRightPad = 8;
  const int panelTopPad = 8;
  const int headerHeight = 22;
  const int indent = 16;

  s_rows.clear();

  int totalRows = 0;
  for (const Group& g : s_groups) {
    totalRows++;
    if (g.expanded && g.types.size() > 1) totalRows += (int)g.types.size();
  }

  int panelHeight = totalRows * lineHeight + headerHeight + 8;
  int panelWidth = colWidth + 16;

  int x0 = screen.x - panelWidth - panelRightPad;
  int y1 = screen.y - panelTopPad;
  int y0 = y1 - panelHeight;

  DrawRectangle(sprite, Vec2Si32(x0, y0), Vec2Si32(x0 + panelWidth, y1), Rgba(0, 0, 0, 200));

  char buf[160];
  int visibleTypes = 0;
  for (auto& [t, v] : s_typeVisible) if (v) ++visibleTypes;
  snprintf(buf, sizeof(buf),
           "Filter by actor type [%d/%zu]  LMB: toggle  [+/-]: expand  RMB: all/none  H: hide",
           visibleTypes, s_typeVisible.size());
  g_font.Draw(sprite, buf, x0 + 6, y1 - 4,
              kTextOriginTop, kTextAlignmentLeft,
              kDrawBlendingModeColorize, kFilterNearest,
              Rgba(255, 255, 255));

  int cursorY = y1 - headerHeight;
  for (int gi = 0; gi < (int)s_groups.size(); ++gi) {
    Group& g = s_groups[gi];

    bool allOn = AllVisible(g);
    bool anyOn = AnyVisible(g);
    char mark;
    Rgba color;
    if (allOn) {
      mark = 'x';
      color = Rgba(180, 255, 180);
    } else if (anyOn) {
      mark = '~';
      color = Rgba(230, 220, 120);
    } else {
      mark = ' ';
      color = Rgba(130, 130, 130);
    }

    bool multi = g.types.size() > 1;
    const char* expander = multi ? (g.expanded ? "[-]" : "[+]") : "   ";

    std::string prefixStr(g.prefix);
    if (prefixStr.size() > 26) prefixStr = prefixStr.substr(0, 25) + "~";
    if (multi) {
      snprintf(buf, sizeof(buf), "%s [%c] %s  (%zu / %zu types)",
               expander, mark, prefixStr.c_str(), g.totalActors, g.types.size());
    } else {
      snprintf(buf, sizeof(buf), "%s [%c] %s  (%zu)",
               expander, mark, prefixStr.c_str(), g.totalActors);
    }

    Row r;
    r.groupIdx = gi;
    r.min = Vec2Si32(x0 + 4, cursorY - lineHeight);
    r.max = Vec2Si32(x0 + panelWidth - 4, cursorY);
    s_rows.push_back(r);

    g_font.Draw(sprite, buf, x0 + 6, cursorY,
                kTextOriginTop, kTextAlignmentLeft,
                kDrawBlendingModeColorize, kFilterNearest,
                color);
    cursorY -= lineHeight;

    if (g.expanded && multi) {
      for (int ti = 0; ti < (int)g.types.size(); ++ti) {
        std::string_view t = g.types[ti];
        bool on = s_typeVisible[t];
        Rgba tc = on ? Rgba(180, 255, 180) : Rgba(130, 130, 130);
        char tm = on ? 'x' : ' ';

        size_t cnt = 0;
        auto it = Logs::GetActorTypeToActorId().find(t);
        if (it != Logs::GetActorTypeToActorId().end()) cnt = it->second.size();

        std::string ts(t);
        if (ts.size() > 30) ts = ts.substr(0, 29) + "~";
        snprintf(buf, sizeof(buf), "      [%c] %s  (%zu)", tm, ts.c_str(), cnt);

        Row rr;
        rr.groupIdx = gi;
        rr.typeIdx = ti;
        rr.min = Vec2Si32(x0 + 4 + indent, cursorY - lineHeight);
        rr.max = Vec2Si32(x0 + panelWidth - 4, cursorY);
        s_rows.push_back(rr);

        g_font.Draw(sprite, buf, x0 + 6, cursorY,
                    kTextOriginTop, kTextAlignmentLeft,
                    kDrawBlendingModeColorize, kFilterNearest,
                    tc);
        cursorY -= lineHeight;
      }
    }
  }
}

void HandleInput() {
  if (IsKeyDownward(kKeyH)) {
    s_panelVisible = !s_panelVisible;
  }
  if (!s_panelVisible) return;

  Vec2Si32 m = MousePos();
  bool overPanel = false;
  const Row* hitRow = nullptr;
  for (const Row& r : s_rows) {
    if (m.x >= r.min.x && m.x <= r.max.x &&
        m.y >= r.min.y && m.y <= r.max.y) {
      overPanel = true;
      hitRow = &r;
      break;
    }
  }

  bool plain = !IsKeyDown(kKeyShift) && !IsKeyDown(kKeyLeftShift)
             && !IsKeyDown(kKeyControl) && !IsKeyDown(kKeyLeftControl);

  if (hitRow && IsKeyDownward(kKeyMouseLeft) && plain) {
    Group& g = s_groups[hitRow->groupIdx];
    if (hitRow->typeIdx < 0) {
      bool multi = g.types.size() > 1;
      int expanderWidth = 28;
      if (multi && m.x <= hitRow->min.x + expanderWidth) {
        g.expanded = !g.expanded;
      } else {
        ToggleGroup(g);
      }
    } else {
      std::string_view t = g.types[hitRow->typeIdx];
      auto it = s_typeVisible.find(t);
      if (it != s_typeVisible.end()) {
        it->second = !it->second;
      }
    }
  }

  if (overPanel && IsKeyDownward(kKeyMouseRight)) {
    bool allOn = true;
    for (auto& [t, v] : s_typeVisible) { if (!v) { allOn = false; break; } }
    for (auto& [t, v] : s_typeVisible) v = !allOn;
  }
}

}  // namespace FilterPanel
