#include "ui_row.hpp"

#include <algorithm>
#include <cctype>
#include <regex>

namespace gt::ui {
namespace {

std::string trim(std::string s) {
    const auto not_space = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
    return s;
}

std::string upper(std::string s) {
    for (char &c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

bool is_on_off(const std::string &v, bool &on) {
    const std::string u = upper(v);
    if (u != "ON" && u != "OFF") return false;
    on = u == "ON";
    return true;
}

bool is_percent(const std::string &v, int &percent) {
    static const std::regex re(R"(^(\d{1,3})\s*%$)");
    std::smatch m;
    if (!std::regex_match(v, m, re)) return false;
    percent = std::min(100, std::stoi(m[1]));
    return true;
}

}  // namespace

Row from_value(const std::string &label, const std::string &value, bool adjustable) {
    Row row;
    row.label = label;
    row.value = value;
    if (is_on_off(value, row.on)) {
        row.kind = RowKind::Toggle;
        row.value.clear();
    } else if (is_percent(value, row.percent)) {
        row.kind = RowKind::Percent;
    } else if (!value.empty() && adjustable) {
        row.kind = RowKind::Choice;
    }
    return row;
}

Row from_text(const std::string &raw, bool adjustable) {
    const std::string text = trim(raw);
    std::smatch m;
    // "<name> ON|OFF [SAVE FAILED]": a mod's switch
    static const std::regex on_off(R"(^(.*?)\s+(ON|OFF)(\s+SAVE FAILED)?$)", std::regex::icase);
    if (std::regex_match(text, m, on_off)) {
        Row row;
        row.kind = RowKind::Toggle;
        row.on = upper(m[2]) == "ON";
        row.label = trim(m[1]);
        if (m[3].matched) row.hint = "Could not save";
        return row;
    }
    // "<name> ERROR [reason]": a mod the loader refused
    static const std::regex error(R"(^(.*?)\s+ERROR(?:\s+(.*))?$)");
    if (std::regex_match(text, m, error)) {
        Row row;
        row.kind = RowKind::Pill;
        row.tone = Tone::Red;
        row.label = trim(m[1]);
        row.value = "ERROR";
        row.hint = m[2].matched ? trim(m[2]) : "";
        return row;
    }
    // "<name> <version> <state>": a downloadable mod (host/mod_store.cpp)
    static const std::regex store(R"(^(.*?)\s+(\S+)\s+(INSTALL|UP TO DATE|RESTART TO LOAD|DOWNLOADING|UPDATE\s+\S+)$)");
    if (std::regex_match(text, m, store)) {
        Row row;
        row.kind = RowKind::Pill;
        row.label = trim(m[1]);
        const std::string state = m[3];
        row.hint = "v" + std::string(m[2]);
        if (state == "INSTALL") {
            row.value = "INSTALL";
            row.tone = Tone::Amber;
        } else if (state == "UP TO DATE") {
            row.value = "UP TO DATE";
            row.tone = Tone::Green;
        } else if (state == "RESTART TO LOAD") {
            row.value = "RESTART";
            row.tone = Tone::Amber;
        } else if (state == "DOWNLOADING") {
            row.value = "DOWNLOADING";
        } else {  // UPDATE <installed version>
            row.value = "UPDATE";
            row.tone = Tone::Amber;
            row.hint += ", installed " + trim(state.substr(6));
        }
        return row;
    }
    // "<name> NN%": a percentage
    static const std::regex percent(R"(^(.*?)\s+(\d{1,3}\s*%)$)");
    if (std::regex_match(text, m, percent)) return from_value(trim(m[1]), m[2], adjustable);
    // "<label>  <value>": two or more spaces split the label from its value
    const std::size_t gap = text.find("  ");
    if (gap == std::string::npos) {
        Row row;
        row.label = text;
        return row;
    }
    const std::string label = trim(text.substr(0, gap)), value = trim(text.substr(gap));
    if (static const std::regex store_error(R"(^ERROR(?:\s+(.*))?$)"); std::regex_match(value, m, store_error)) {
        Row row;
        row.kind = RowKind::Pill;
        row.tone = Tone::Red;
        row.label = label;
        row.value = "ERROR";
        row.hint = m[1].matched ? trim(m[1]) : "";
        return row;
    }
    return from_value(label, value, adjustable);
}

std::string pretty_binding(const std::string &whole) {
    // "L+TRIANGLE@30" holds the combination 30 vblanks
    const std::size_t at = whole.find('@');
    const std::string text = whole.substr(0, at);
    std::string out, token;
    const auto flush = [&] {
        if (token.empty()) return;
        if (!out.empty()) out += " + ";
        if (token.size() > 1 && std::all_of(token.begin(), token.end(), [](unsigned char c) { return std::isupper(c) != 0; })) {
            for (std::size_t i = 1; i < token.size(); ++i) token[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(token[i])));
        }
        out += token;
        token.clear();
    };
    for (const char c : text) {
        if (c == '+') flush();
        else token += c;
    }
    flush();
    if (at != std::string::npos && at + 1 < whole.size()) out += " (hold)";
    return out;
}

std::string upper_binding(const std::string &text) { return upper(pretty_binding(text)); }

}  // namespace gt::ui
