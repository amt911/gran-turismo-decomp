// Cheat device (spec docs/superpowers/specs/2026-11-11-cheat-device-design.md, §1): the menu tree and its navigation. Pure logic,
// drawn by a view (phase 1: the host's text overlay; phase 2: the new UI) that asks for rows(); CTest drives it.
#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace gt::cheat_device {

enum class EntryKind { Toggle, Choice, Number, Action, Submenu, Info };

struct Entry {
    std::string id;
    EntryKind kind = EntryKind::Info;
    std::string title;                       // fixed left part ("Slow motion")
    std::function<std::string()> value;      // live right part ("0.5x"); empty for Action / Submenu
    std::function<void(int)> change;         // Toggle/Choice/Number: -1 left, +1 right, 0 Enter; Action: 0
    std::vector<Entry> children;             // Submenu
};

struct Row {
    std::string title, value;
    bool selected = false, submenu = false;
};

// Up/Down move (wrapping), Left/Right change a value, Enter fires / opens / cycles, Back goes up one level (false at the root:
// the caller closes the device). The cursor of every page is remembered when the player leaves it and comes back.
class Navigator {
public:
    explicit Navigator(Entry root) : root_(std::move(root)) {}
    void up();
    void down();
    void left();
    void right();
    void enter();
    bool back();
    // The visible rows of the current page: `capacity` of them, scrolled so the cursor is visible.
    std::vector<Row> rows(std::size_t capacity) const;
    std::string path() const;  // "Cheats > Race"
    const Entry &page() const;
    std::size_t cursor() const;

private:
    Entry &page_mut();
    Entry root_;
    std::vector<std::size_t> stack_;              // child index taken at each level below the root
    std::map<std::string, std::size_t> cursors_;  // page id path -> cursor
    std::string key() const;
};

}  // namespace gt::cheat_device
