#include "synth_report.hpp"

#include <sstream>

namespace gt {

ChildReport parse_child_report(const std::string &output, bool killed, bool with_v2) {
    ChildReport report;
    const std::string marker = "\n[v1-end]\n";
    const std::size_t at = output.find(marker);
    const bool v1_complete = with_v2 && at != std::string::npos;
    if (killed && !v1_complete) return report;  // killed during the v1 cases: nothing usable, as it always was
    std::istringstream in(v1_complete ? output.substr(0, at + 1u) : output);
    if (!(in >> report.compared >> report.equal >> report.mismatched >> report.skipped >> report.informative)) return report;
    report.ok = true;
    std::string line;
    std::getline(in, line);
    std::getline(in, report.first_mismatch);
    std::getline(in, report.first_skip);
    while (std::getline(in, line))
        if (!line.empty()) report.other_lines.push_back(line);
    if (!v1_complete) return report;
    std::istringstream tail(output.substr(at + marker.size()));
    bool saw_v2 = false;
    while (std::getline(tail, line)) {
        if (line.rfind("[v2] ", 0) == 0) {
            std::istringstream numbers(line.substr(5));
            unsigned c = 0, e = 0, m = 0, s = 0, i = 0;
            if (!(numbers >> c >> e >> m >> s >> i)) continue;
            saw_v2 = true;
            report.v2_compared = c;
            report.v2_equal = e;
            report.compared += c;
            report.equal += e;
            report.mismatched += m;
            report.skipped += s;
            report.informative += i;
        } else if (line.rfind("[v2-mismatch] ", 0) == 0) {
            if (report.first_mismatch.empty()) report.first_mismatch = line.substr(14);
        } else if (line.rfind("[v2-skip] ", 0) == 0) {
            if (report.first_skip.empty()) report.first_skip = line.substr(10);
        } else if (!line.empty()) {
            report.other_lines.push_back(line);
        }
    }
    report.v2_lost = !saw_v2;
    return report;
}

} // namespace gt
