#include "Highlight.hpp"

#include "Logs.hpp"
#include "globals.h"

#include <cstddef>
#include <fstream>
#include <queue>
#include <unordered_set>
#include <vector>

namespace Highlight {

namespace {

bool s_active = false;
size_t s_rootIdx = 0;
std::unordered_set<size_t> s_chainMsgs;
std::unordered_set<ActorIdx> s_chainActors;
std::unordered_set<size_t> s_ancestors;
std::unordered_set<size_t> s_descendants;

std::vector<Si64> s_parentOf;

void BuildParentIndex() {
  const auto& msgs = Logs::GetLogMessages();
  s_parentOf.assign(msgs.size(), -1);
  for (size_t i = 0; i < msgs.size(); ++i) {
    for (Si64 child : msgs[i].child_msg_idxs) {
      if (child >= 0 && (size_t)child < s_parentOf.size()) {
        if (s_parentOf[child] < 0) {
          s_parentOf[child] = (Si64)i;
        }
      }
    }
  }
}

}  // namespace

void Init() {
  BuildParentIndex();
  Clear();
}

bool IsActive() { return s_active; }
bool IsMsgInChain(size_t msgIdx) { return s_chainMsgs.count(msgIdx) > 0; }
bool IsActorInChain(ActorIdx id) { return s_chainActors.count(id) > 0; }
bool IsAncestor(size_t msgIdx) { return s_ancestors.count(msgIdx) > 0; }
bool IsDescendant(size_t msgIdx) { return s_descendants.count(msgIdx) > 0; }
bool IsRoot(size_t msgIdx) { return s_active && msgIdx == s_rootIdx; }

size_t ChainMsgCount() { return s_chainMsgs.size(); }
size_t ChainActorCount() { return s_chainActors.size(); }
size_t RootMsgIdx() { return s_rootIdx; }

const std::unordered_set<size_t>& ChainMsgs() { return s_chainMsgs; }
const std::unordered_set<ActorIdx>& ChainActors() { return s_chainActors; }

void Clear() {
  s_active = false;
  s_chainMsgs.clear();
  s_chainActors.clear();
  s_ancestors.clear();
  s_descendants.clear();
  s_rootIdx = 0;
}

void SelectChain(size_t msgIdx) {
  const auto& msgs = Logs::GetLogMessages();
  if (msgIdx >= msgs.size()) return;

  s_chainMsgs.clear();
  s_chainActors.clear();
  s_ancestors.clear();
  s_descendants.clear();

  s_chainMsgs.insert(msgIdx);
  s_chainActors.insert(msgs[msgIdx].from);
  s_chainActors.insert(msgs[msgIdx].to);

  std::queue<size_t> qd;
  qd.push(msgIdx);
  while (!qd.empty()) {
    size_t cur = qd.front();
    qd.pop();
    for (Si64 child : msgs[cur].child_msg_idxs) {
      if (child < 0) continue;
      size_t c = (size_t)child;
      if (c >= msgs.size()) continue;
      if (s_chainMsgs.insert(c).second) {
        s_descendants.insert(c);
        s_chainActors.insert(msgs[c].from);
        s_chainActors.insert(msgs[c].to);
        qd.push(c);
      }
    }
  }

  std::queue<size_t> qp;
  qp.push(msgIdx);
  while (!qp.empty()) {
    size_t cur = qp.front();
    qp.pop();
    if (cur >= s_parentOf.size()) continue;
    Si64 p = s_parentOf[cur];
    if (p < 0) continue;
    size_t pu = (size_t)p;
    if (s_chainMsgs.insert(pu).second) {
      s_ancestors.insert(pu);
      s_chainActors.insert(msgs[pu].from);
      s_chainActors.insert(msgs[pu].to);
      qp.push(pu);
    }
  }

  s_rootIdx = msgIdx;
  s_active = true;

  std::ofstream dbg("/tmp/actor_debug_log.txt", std::ios::app);
  dbg << "\n=== CHAIN SELECTED ===\n"
      << "root msg_idx=" << msgIdx
      << " chain_msgs=" << s_chainMsgs.size()
      << " ancestors=" << s_ancestors.size()
      << " descendants=" << s_descendants.size()
      << " chain_actors=" << s_chainActors.size()
      << std::endl;
}

bool TrySelectFromNearest() {
  if (g_mouse_nearest_message_idx < 0) return false;
  if (g_distance_sq_to_nearest_message < 0 || g_distance_sq_to_nearest_message > 100.0) return false;
  SelectChain((size_t)g_mouse_nearest_message_idx);
  return true;
}

}  // namespace Highlight
