// Calling conventions shared by GT's Adhoc natives (the script-callable C++ functions): reading an
// argument object's value through its virtual interface, and handing a new value object back
// through the native's result holder. All of it is GT's own code, called through the guest.
#pragma once

#include "guest_call.hpp"

#include <cstdint>

namespace gt::adhoc {

// Virtual slots of an Adhoc object (vtable word index).
constexpr std::uint32_t kToStringSlot = 3u;    // std::string toString(this) (into *out)
constexpr std::uint32_t kToBoolSlot = 12u;     // bool toBool(this)
constexpr std::uint32_t kToIntSlot = 15u;      // int toInt(this)
constexpr std::uint32_t kToULongSlot = 20u;    // u64 toULong(this) (v0:v1)
constexpr std::uint32_t kToFloatSlot = 21u;    // float toFloat(this)
constexpr std::uint32_t kDefinedSlot = 38u;    // bool defined(this, name)

// The address of an object's virtual function `slot`.
std::uint32_t virtual_function(psprecomp::Runtime &rt, std::uint32_t object, std::uint32_t slot);

// *out = object.toString(): a libstdc++ (copy-on-write) std::string, i.e. a pointer to its characters.
void to_string(psprecomp::Runtime &rt, const psprecomp::AllegrexContext &ctx, std::uint32_t object, std::uint32_t out);
// ~std::string for the string whose characters are at `chars`: drops a reference to its rep and
// destroys the rep with the last one (the shared empty rep is never counted).
void string_release(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t chars);
// *result = a new Adhoc string with the std::string at `string` (left as it is).
void return_string(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t result, std::uint32_t string);
// *result = a new Adhoc string with the C string at `text` (through a std::string, as GT does).
void return_c_string(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t result, std::uint32_t text);
std::int32_t to_int(psprecomp::Runtime &rt, const psprecomp::AllegrexContext &ctx, std::uint32_t object);
float to_float(psprecomp::Runtime &rt, const psprecomp::AllegrexContext &ctx, std::uint32_t object);

// *result = new Adhoc integer / float / boolean holding `value`. `scratch` is a guest-stack holder
// (gt::GuestFrame) the value is built in, as the originals do; `result` may be that same holder.
void return_integer(psprecomp::Runtime &rt, const psprecomp::AllegrexContext &ctx, std::uint32_t result,
                    std::uint32_t scratch, std::uint32_t value);
void return_float(psprecomp::Runtime &rt, const psprecomp::AllegrexContext &ctx, std::uint32_t result,
                  std::uint32_t scratch, float value);
void return_bool(psprecomp::Runtime &rt, const psprecomp::AllegrexContext &ctx, std::uint32_t result,
                 std::uint32_t scratch, std::uint32_t value);
// A 64-bit integer, passed as its low and high words (the EABI a2:a3 pair).
void return_long(psprecomp::Runtime &rt, const psprecomp::AllegrexContext &ctx, std::uint32_t result,
                 std::uint32_t scratch, std::uint32_t low, std::uint32_t high);

} // namespace gt::adhoc
