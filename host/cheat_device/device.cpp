#include "cheat_device/device.hpp"

#include <algorithm>
#include <bit>
#include <cctype>

namespace gt::cheat_device {
namespace {

std::string fit(std::string text, std::size_t width) {
    if (text.size() > width) text.resize(width);
    return text;
}

// "<title padded> <value>" in kWidth characters; the title gives way to the value.
std::string row_text(const Row &row) {
    std::string value = row.submenu ? ">" : row.value;
    if (value.empty()) return fit(row.title, kWidth);
    if (value.size() >= kWidth - 2) value.resize(kWidth - 2);
    std::string title = row.title;
    const std::size_t room = kWidth - 1 - value.size();
    if (title.size() > room) title.resize(room);
    title.append(room - title.size(), ' ');
    return title + " " + value;
}

std::string sentence_case(std::string text) {
    for (char &c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (!text.empty()) text[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(text[0])));
    return text;
}

std::string or_dash(const std::string &text) { return text.empty() ? "-" : text; }

}  // namespace

Device::Device(ModRows mods) : mods_(std::move(mods)) {
    std::string error;
    bindings_.add_action(kOpenAction, "Open the cheat device", KeyBinding{"F7", 0}, PadBinding{kPadSelect | kPadStart, 0}, true, error);
}

bool Device::add_action(const std::string &id, const std::string &label, const std::string &category, KeyBinding key, PadBinding pad,
                        bool required, std::string &error) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (id.empty() || bindings_.find(id) != nullptr) return bindings_.add_action(id, label, key, pad, required, error);
    std::string clash;
    if (!bindings_.add_action(id, label, key, pad, required, clash)) {
        // A default another action holds: declared unbound (never "last one wins"), the reason goes back to the caller.
        if (!bindings_.add_action(id, label, KeyBinding{}, PadBinding{}, required, error)) return false;
        error = label + " declared without hotkeys: " + clash;
    }
    action_category_.emplace_back(id, category);
    Entry entry{id, EntryKind::Action, label, {}, [this, id](int) {
                    if (fire_) fire_(id);
                },
                {}};
    entries_.emplace_back(category, std::move(entry));
    return true;
}

void Device::add_entry(const std::string &category, Entry entry) {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.emplace_back(category, std::move(entry));
}

void Device::set_log(std::function<void(const std::string &)> log) {
    std::lock_guard<std::mutex> lock(mutex_);
    log_ = std::move(log);
}

void Device::set_fire(std::function<void(const std::string &)> fire) {
    std::lock_guard<std::mutex> lock(mutex_);
    fire_ = std::move(fire);
}

std::vector<std::string> Device::use_bindings_file(const std::filesystem::path &path) {
    std::lock_guard<std::mutex> lock(mutex_);
    bindings_path_ = path;
    std::vector<std::string> warnings;
    bindings_.load_file(path, warnings);
    for (std::string &w : bindings_.take_warnings()) warnings.push_back(std::move(w));
    return warnings;
}

void Device::say_locked(std::string message, std::uint64_t now, bool error) {
    message_ = std::move(message);
    message_error_ = error;
    message_until_ = now + kMessageVblanks;
}

void Device::changed_locked(std::uint64_t now) {
    if (bindings_path_.empty()) return;
    std::string error;
    if (!bindings_.save_file(bindings_path_, error)) say_locked("NOT SAVED: " + error, now);
}

Entry Device::controls_locked() {
    Entry controls{"Controls", EntryKind::Submenu, "Controls", {}, {}, {}};
    for (const Action &a : bindings_.actions()) {
        const std::string id = a.id;
        Entry page{id, EntryKind::Submenu, a.label, {}, {}, {}};
        page.children.push_back(Entry{"key", EntryKind::Action, "Keyboard",
                                      [this, id] { return capture_id_ == id && !capture_pad_ ? std::string("PRESS A KEY")
                                                                                             : or_dash(print_key(bindings_.find(id)->key)); },
                                      [this, id](int) {
                                          capture_id_ = id;
                                          capture_pad_ = false;
                                      },
                                      {}});
        page.children.push_back(Entry{"pad", EntryKind::Action, "Pad",
                                      [this, id] { return capture_id_ == id && capture_pad_ ? std::string("PRESS BUTTONS")
                                                                                            : or_dash(print_pad(bindings_.find(id)->pad)); },
                                      [this, id](int) {
                                          capture_id_ = id;
                                          capture_pad_ = true;
                                          pad_capture_.emplace();
                                          capture_fresh_ = true;
                                          cancel_held_ = 0;
                                      },
                                      {}});
        page.children.push_back(Entry{"clear", EntryKind::Action, "Clear", {}, [this, id](int) {
                                          std::string error;
                                          const bool k = bindings_.set_key(id, KeyBinding{}, error);
                                          const bool p = k && bindings_.set_pad(id, PadBinding{}, error);
                                          if (!k || !p) {
                                              if (k) bindings_.set_key(id, bindings_.find(id)->default_key, error);
                                              say_locked(error, now_);
                                          }
                                          changed_locked(now_);
                                      },
                                      {}});
        page.children.push_back(Entry{"default", EntryKind::Action, "Reset to default", {}, [this, id](int) {
                                          bindings_.reset(id);
                                          changed_locked(now_);
                                      },
                                      {}});
        controls.children.push_back(std::move(page));
    }
    controls.children.push_back(Entry{"all", EntryKind::Action, "Reset all to default", {}, [this](int) {
                                          bindings_.reset_all();
                                          changed_locked(now_);
                                      },
                                      {}});
    return controls;
}

void Device::rebuild_locked() {
    Entry root{"root", EntryKind::Submenu, "Cheats", {}, {}, {}};
    for (const char *category : kCategories) {
        Entry page{category, EntryKind::Submenu, category, {}, {}, {}};
        if (std::string(category) == "Mods") {
            const std::size_t n = mods_.count ? mods_.count() : 0;
            for (std::size_t i = 0; i < n; ++i)
                page.children.push_back(Entry{"mod" + std::to_string(i), EntryKind::Row, "", [this, i] { return mods_.row(i); },
                                              [this, i](int direction) { mods_.adjust(i, direction); }, {}});
        } else if (std::string(category) == "Controls") {
            page = controls_locked();
        } else {
            for (const auto &[where, entry] : entries_)
                if (where == category) page.children.push_back(entry);
            if (page.children.empty()) continue;
        }
        root.children.push_back(std::move(page));
    }
    nav_.set_root(std::move(root));
}

void Device::open() {
    std::lock_guard<std::mutex> lock(mutex_);
    open_locked();
}

void Device::open_locked() {
    rebuild_locked();
    if (!opened_once_) {
        opened_once_ = true;
        nav_.go_to("Mods");
    }
}

bool Device::handle_key(MenuKey key, std::uint64_t now) {
    std::lock_guard<std::mutex> lock(mutex_);
    now_ = now;
    if (!capture_id_.empty()) {  // a capture takes the keys; Close (Escape / Circle tap) cancels it
        if (key == MenuKey::Close) {
            capture_id_.clear();
            pad_capture_.reset();
        }
        return true;
    }
    switch (key) {
    case MenuKey::Up: nav_.up(); break;
    case MenuKey::Down: nav_.down(); break;
    case MenuKey::Left: nav_.left(); break;
    case MenuKey::Right: nav_.right(); break;
    case MenuKey::Activate: nav_.enter(); break;
    case MenuKey::Close: return nav_.back();
    case MenuKey::TogglePage: break;
    }
    return true;
}

// One styled row from a navigator row: the kind says how it is drawn.
ui::Row Device::styled_row_locked(const Row &r) const {
    ui::Row out;
    switch (r.kind) {
    case EntryKind::Toggle:
    case EntryKind::Number:
    case EntryKind::Choice: out = ui::from_value(r.title, r.value, true); break;
    case EntryKind::Submenu:
        out.kind = ui::RowKind::Submenu;
        out.label = r.title;
        break;
    case EntryKind::Action: {
        out.label = r.title;
        std::string chip = r.value;
        if (chip.empty()) {  // a hotkey action: its pad combination, else its key
            if (const Action *a = bindings_.find(r.id)) chip = !print_pad(a->pad).empty() ? print_pad(a->pad) : print_key(a->key);
        }
        if (chip.empty()) {
            // a plain action (Clear, Reset): nothing on the right, or the hotkey action's "not bound" below
            if (bindings_.find(r.id) != nullptr && r.value.empty()) out.value = "\xE2\x80\x94";
        } else if (chip == "-") {
            out.value = "\xE2\x80\x94";  // em dash: not bound
        } else if (chip.rfind("PRESS", 0) == 0) {
            out.kind = ui::RowKind::Chip;
            out.value = sentence_case(chip);
        } else if (!r.value.empty() || bindings_.find(r.id) != nullptr) {
            out.kind = ui::RowKind::Chip;
            out.value = ui::pretty_binding(chip);
        } else {
            out.value = chip;
        }
        break;
    }
    case EntryKind::Info:
        out.label = r.title;
        out.value = r.value;
        break;
    case EntryKind::Row:  // a mod's v4 row: all its text is in the value
        out = r.title.empty() ? ui::from_text(r.value, true) : ui::from_value(r.title, r.value, true);
        break;
    }
    return out;
}

View Device::view(std::size_t rows, std::uint64_t now) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!opened_once_) open_locked();  // shown by a path that did not call open() (the overlay's own F9)
    View v;
    const bool message = now < message_until_;
    v.title = fit(message ? message_ : nav_.path(), 31);
    v.footer = capture_id_.empty() ? std::string(kFooter) : std::string(kCaptureFooter);
    v.crumb = nav_.path();
    if (message) {
        v.message = message_;
        v.message_is_error = message_error_;
    }
    v.capturing = !capture_id_.empty();
    const std::size_t entries = nav_.page().children.size();
    v.count = static_cast<int>(entries);
    v.position = entries > 0 ? static_cast<int>(nav_.cursor()) + 1 : 0;
    if (const Action *open = bindings_.find(kOpenAction)) {
        const std::string pad = ui::upper_binding(print_pad(open->pad)), key = ui::upper_binding(print_key(open->key));
        v.binding = pad.empty() ? key : key.empty() ? pad : pad + " \xC2\xB7 " + key;
    }
    for (const Row &r : nav_.rows(rows)) {
        if (r.selected) v.selected = static_cast<int>(v.rows.size());
        // A mod row (Row kind, empty title) carries all its text in the value.
        v.rows.push_back(r.title.empty() ? fit(r.value, kWidth) : row_text(r));
        v.items.push_back(styled_row_locked(r));
    }
    return v;
}

bool Device::capturing() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !capture_id_.empty();
}

void Device::poll_pad(std::uint32_t raw, std::uint64_t now) {
    std::lock_guard<std::mutex> lock(mutex_);
    now_ = now;
    if (capture_id_.empty() || !capture_pad_ || !pad_capture_) return;
    cancel_held_ = raw == kPadCircle ? cancel_held_ + 1 : 0;
    if (cancel_held_ >= kCancelHold) {  // CIRCLE held alone: cancel, nothing bound
        capture_id_.clear();
        pad_capture_.reset();
        say_locked("CANCELLED", now, false);
        return;
    }
    if (capture_fresh_) {  // what is down on the first sample started the capture (CROSS): not part of the combination
        capture_ignored_ = raw;
        capture_fresh_ = false;
    }
    capture_ignored_ &= raw;  // a released button counts again
    const PadCapture::State state = pad_capture_->poll(raw & ~capture_ignored_);
    if (state == PadCapture::State::Waiting) return;
    const std::string id = capture_id_;
    capture_id_.clear();
    if (state == PadCapture::State::TimedOut) {
        pad_capture_.reset();
        say_locked("NOTHING PRESSED", now, false);
        return;
    }
    const PadBinding binding = pad_capture_->result();
    pad_capture_.reset();
    std::string error;
    if (!bindings_.set_pad(id, binding, error)) {
        say_locked(error, now);
        if (log_) log_("refused " + id + " pad " + print_pad(binding) + ": " + error);
    } else {
        changed_locked(now);
        if (log_) log_("bound " + id + " pad " + print_pad(binding));
    }
}

bool Device::capture_key(const KeyBinding &key, std::uint64_t now) {
    std::lock_guard<std::mutex> lock(mutex_);
    now_ = now;
    if (capture_id_.empty() || capture_pad_) return false;
    const std::string id = capture_id_;
    capture_id_.clear();
    std::string error;
    if (!bindings_.set_key(id, key, error)) {
        say_locked(error, now);
        if (log_) log_("refused " + id + " key " + print_key(key) + ": " + error);
    } else {
        changed_locked(now);
        if (log_) log_("bound " + id + " key " + print_key(key));
    }
    return true;
}

std::optional<std::string> Device::action_for_key(const KeyBinding &key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (const Action *a = bindings_.by_key(key)) return a->id;
    return std::nullopt;
}

std::vector<std::string> Device::poll_pad_hotkeys(std::uint32_t raw) {
    std::lock_guard<std::mutex> lock(mutex_);
    return hotkeys_.poll(bindings_, raw);
}

std::uint32_t Device::bound_pad_buttons() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::uint32_t buttons = 0;
    for (const Action &a : bindings_.actions()) buttons |= a.pad.buttons;
    return buttons;
}

std::string Device::binding_text(const std::string &id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return binding_text_locked(id);
}

std::vector<PadHotkeyMask> Device::pad_hotkeys() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<PadHotkeyMask> out;
    for (const Action &a : bindings_.actions())
        if (a.id != kOpenAction && a.pad.buttons != 0u) out.push_back(PadHotkeyMask{a.pad.buttons, a.pad.hold});
    return out;
}

PadBinding Device::pad_binding(const std::string &id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const Action *a = bindings_.find(id);
    return a != nullptr ? a->pad : PadBinding{};
}

std::string Device::binding_text_locked(const std::string &id) const {
    const Action *a = bindings_.find(id);
    if (a == nullptr) return "";
    return or_dash(print_key(a->key)) + " / " + or_dash(print_pad(a->pad));
}

}  // namespace gt::cheat_device
