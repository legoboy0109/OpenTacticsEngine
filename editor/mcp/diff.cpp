#include "diff.h"

#include <algorithm>
#include <vector>

namespace ote_mcp {

namespace {
std::vector<std::string> lines(const std::string& s) {
  std::vector<std::string> out;
  size_t start = 0;
  while (start < s.size()) {
    const size_t nl = s.find('\n', start);
    if (nl == std::string::npos) { out.push_back(s.substr(start)); break; }
    out.push_back(s.substr(start, nl - start));
    start = nl + 1;
  }
  return out;
}

struct Op { char c; size_t a, b; };  // ' ' keep, '-' drop a[a], '+' add b[b]
}  // namespace

DiffResult unifiedDiff(const std::string& before, const std::string& after, int context, int maxLines) {
  DiffResult r;
  if (before == after) return r;
  const std::vector<std::string> a = lines(before), b = lines(after);
  // Trim what both ends share, then an LCS table on the middle (capped: a rewrite of a huge middle is shown as one block).
  size_t pre = 0;
  while (pre < a.size() && pre < b.size() && a[pre] == b[pre]) ++pre;
  size_t suf = 0;
  while (suf < a.size() - pre && suf < b.size() - pre && a[a.size() - 1 - suf] == b[b.size() - 1 - suf]) ++suf;
  const size_t na = a.size() - pre - suf, nb = b.size() - pre - suf;
  std::vector<Op> ops;
  for (size_t i = 0; i < pre; ++i) ops.push_back({' ', i, i});
  if (na * nb <= 4000000) {
    std::vector<std::vector<unsigned>> lcs(na + 1, std::vector<unsigned>(nb + 1, 0));
    for (size_t i = na; i-- > 0;)
      for (size_t j = nb; j-- > 0;)
        lcs[i][j] = a[pre + i] == b[pre + j] ? lcs[i + 1][j + 1] + 1 : std::max(lcs[i + 1][j], lcs[i][j + 1]);
    size_t i = 0, j = 0;
    while (i < na || j < nb) {
      if (i < na && j < nb && a[pre + i] == b[pre + j]) { ops.push_back({' ', pre + i, pre + j}); ++i; ++j; }
      else if (i < na && (j == nb || lcs[i + 1][j] >= lcs[i][j + 1])) { ops.push_back({'-', pre + i, 0}); ++i; }  // removals first, as usual
      else { ops.push_back({'+', 0, pre + j}); ++j; }
    }
  } else {
    for (size_t i = 0; i < na; ++i) ops.push_back({'-', pre + i, 0});
    for (size_t j = 0; j < nb; ++j) ops.push_back({'+', 0, pre + j});
  }
  for (size_t i = 0; i < suf; ++i) ops.push_back({' ', a.size() - suf + i, b.size() - suf + i});

  for (const Op& o : ops) { if (o.c == '+') ++r.added; else if (o.c == '-') ++r.removed; }
  // Hunks: every change with `context` lines around it; overlapping ranges merge.
  std::vector<bool> show(ops.size(), false);
  for (size_t i = 0; i < ops.size(); ++i)
    if (ops[i].c != ' ')
      for (size_t k = i > (size_t)context ? i - (size_t)context : 0; k <= std::min(ops.size() - 1, i + (size_t)context); ++k) show[k] = true;
  int emitted = 0;
  for (size_t i = 0; i < ops.size();) {
    if (!show[i]) { ++i; continue; }
    size_t j = i;
    while (j < ops.size() && show[j]) ++j;
    size_t la = 0, lb = 0, startA = 0, startB = 0;
    bool first = true;
    for (size_t k = i; k < j; ++k) {
      if (ops[k].c != '+') { if (first) startA = ops[k].a; ++la; }
      if (ops[k].c != '-') { if (first) startB = ops[k].b; ++lb; }
      if (first && ops[k].c == '+') startA = k > 0 ? ops[k - 1].a + 1 : 0;
      if (first && ops[k].c == '-') startB = k > 0 ? ops[k - 1].b + 1 : 0;
      first = false;
    }
    r.text += "@@ -" + std::to_string(startA + 1) + "," + std::to_string(la) + " +" + std::to_string(startB + 1) + "," + std::to_string(lb) + " @@\n";
    for (size_t k = i; k < j; ++k) {
      if (emitted >= maxLines) { r.truncated = true; break; }
      const std::string& text = ops[k].c == '+' ? b[ops[k].b] : a[ops[k].a];
      r.text += std::string(1, ops[k].c) + text + "\n";
      ++emitted;
    }
    if (r.truncated) break;
    i = j;
  }
  if (r.truncated) r.text += "... (diff cut after " + std::to_string(maxLines) + " lines; " + std::to_string(r.added) + " added, " + std::to_string(r.removed) + " removed in all)\n";
  return r;
}

}  // namespace ote_mcp
