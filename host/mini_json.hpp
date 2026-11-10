// A small JSON reader for the mod index (host/mod_index.hpp): objects, arrays, strings (with \uXXXX escapes, BMP and
// surrogate pairs), numbers, true/false/null. Read-only, no streaming; the index is a few kilobytes.
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace gt::json {

struct Value {
    enum class Kind { Null, Bool, Number, String, Array, Object };
    Kind kind = Kind::Null;
    bool boolean = false;
    double number = 0.0;
    std::string string;
    std::vector<Value> array;
    std::vector<std::string> keys;  // an object's members in document order: keys[i] names values[i]
    std::vector<Value> values;

    // The member `key` of an object, or nullptr (also when this is not an object).
    const Value *get(std::string_view key) const;
};

// The whole text must be one value (whitespace around it is allowed). On failure: nullopt and a reason with the byte offset.
std::optional<Value> parse(std::string_view text, std::string &error);

} // namespace gt::json
