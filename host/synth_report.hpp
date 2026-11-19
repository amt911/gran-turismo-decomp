// What a synthetic-case child process tells its parent through the pipe (host/diff_harness.cpp), parsed apart so it can be tested.
//
// Format (generator v2 off: exactly what it always was):
//   <compared> <equal> <mismatched> <skipped> <informative>\n <first mismatch>\n <first skip>\n [mismatch case lines | [corpus] lines]...
// With v2 cases the child sends the v1 block first, then the line `[v1-end]`, runs the v2 cases, and sends
//   [v2] <compared> <equal> <mismatched> <skipped> <informative>\n  (+ optional `[v2-mismatch] text` / `[v2-skip] text` lines)
// A child killed during the v2 cases therefore loses only the v2 cases: the v1 block is complete once `[v1-end]` was read.
#pragma once

#include <string>
#include <vector>

namespace gt {

struct ChildReport {
    bool ok{};              // the v1 block was read
    bool v2_lost{};         // killed (or cut) after [v1-end] without a [v2] line
    unsigned compared{}, equal{}, mismatched{}, skipped{}, informative{};
    unsigned v2_compared{}, v2_equal{};  // of the totals above
    std::string first_mismatch, first_skip;
    std::vector<std::string> other_lines;  // mismatch case lines and [corpus] lines, as sent
};

ChildReport parse_child_report(const std::string &output, bool killed, bool with_v2);

} // namespace gt
