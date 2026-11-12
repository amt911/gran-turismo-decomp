#include "cheat_device/menu.hpp"

#include <algorithm>

namespace gt::cheat_device {

const Entry &Navigator::page() const {
    const Entry *e = &root_;
    for (const std::size_t i : stack_) e = &e->children[i];
    return *e;
}

Entry &Navigator::page_mut() { return const_cast<Entry &>(page()); }

std::string Navigator::key() const {
    std::string k = root_.id;
    const Entry *e = &root_;
    for (const std::size_t i : stack_) {
        e = &e->children[i];
        k += "/" + e->id;
    }
    return k;
}

std::size_t Navigator::cursor() const {
    const auto it = cursors_.find(key());
    const std::size_t n = page().children.size();
    return it == cursors_.end() || n == 0 ? 0u : std::min(it->second, n - 1);
}

void Navigator::up() {
    const std::size_t n = page().children.size();
    if (n != 0) cursors_[key()] = (cursor() + n - 1) % n;
}

void Navigator::down() {
    const std::size_t n = page().children.size();
    if (n != 0) cursors_[key()] = (cursor() + 1) % n;
}

void Navigator::left() {
    Entry &page = page_mut();
    if (page.children.empty()) return;
    Entry &e = page.children[cursor()];
    if ((e.kind == EntryKind::Toggle || e.kind == EntryKind::Choice || e.kind == EntryKind::Number) && e.change) e.change(-1);
}

void Navigator::right() {
    Entry &page = page_mut();
    if (page.children.empty()) return;
    Entry &e = page.children[cursor()];
    if ((e.kind == EntryKind::Toggle || e.kind == EntryKind::Choice || e.kind == EntryKind::Number) && e.change) e.change(+1);
}

void Navigator::enter() {
    Entry &page = page_mut();
    if (page.children.empty()) return;
    const std::size_t at = cursor();
    Entry &e = page.children[at];
    if (e.kind == EntryKind::Submenu) {
        stack_.push_back(at);
        return;
    }
    if (e.kind == EntryKind::Info || !e.change) return;
    e.change(e.kind == EntryKind::Action ? 0 : +1);  // Enter fires an action and steps a value forward
}

bool Navigator::back() {
    if (stack_.empty()) return false;
    stack_.pop_back();
    return true;
}

std::vector<Row> Navigator::rows(std::size_t capacity) const {
    const Entry &p = page();
    std::vector<Row> out;
    if (capacity == 0 || p.children.empty()) return out;
    const std::size_t at = cursor();
    const std::size_t first = at < capacity ? 0 : at - capacity + 1;
    for (std::size_t i = first; i < p.children.size() && out.size() < capacity; ++i) {
        const Entry &e = p.children[i];
        out.push_back(Row{e.title, e.value ? e.value() : "", i == at, e.kind == EntryKind::Submenu});
    }
    return out;
}

std::string Navigator::path() const {
    std::string out = root_.title;
    const Entry *e = &root_;
    for (const std::size_t i : stack_) {
        e = &e->children[i];
        out += " > " + e->title;
    }
    return out;
}

}  // namespace gt::cheat_device
