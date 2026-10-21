// Methods of Adhoc's String class (initializer 0x26B010: toInt, toLong, toULong, upcase, substr, ...)
// and pdistd.AsciiStringHash. A string object keeps a libstdc++ std::string; self is fetched as
// a string (kSelfAsString) or converted with toString (vslot 3).

#include "decomp_registry.hpp"
#include "value.hpp"

namespace gt::adhoc::string {
namespace {

constexpr std::uint32_t kHolderReset = 0x08A5BAD8u;
constexpr std::uint32_t kHolderDestroy = 0x08A5B9ECu;
constexpr std::uint32_t kSelfAsString = 0x08A6C410u;     // (holder *out, [self holder])
constexpr std::uint32_t kStringToIntBase = 0x08A6C228u;  // (string *, base) -> int
constexpr std::uint32_t kStringToLongBase = 0x08A6C248u; // (string *, base) -> s64
constexpr std::uint32_t kStringToULongBase = 0x08A6BFBCu;  // (string *, base) -> u64
constexpr std::uint32_t kNewSignedLong = 0x08A83160u;    // (holder *out, -, s64 in a2:a3)
constexpr std::uint32_t kNewUnsignedLong = 0x08A5DEE8u;  // (holder *out, -, u64 in a2:a3)
constexpr std::uint32_t kStringUpcase = 0x08A6F888u;     // (holder *out, string *)
constexpr std::uint32_t kStdStringSubstr = 0x08B8D96Cu;  // std::string::substr(out, const string *, pos, n)
constexpr std::uint32_t kThrowOutOfRange = 0x08B5AEDCu;  // std::__throw_out_of_range(what): does not return
constexpr std::uint32_t kSubstrWhat = 0x08CB07A8u;       // "basic_string::substr"
constexpr std::uint32_t kAsciiStringHash = 0x08B1CE38u;  // (const char *) -> u64
constexpr std::uint32_t kToLongSlot = 16u;               // s64 toLong(this)

void destroy(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t holder) {
    (void)call_guest(rt, ctx, kHolderDestroy, {holder});
}

// *result = the holder `make(out)` fills.
template <typename Make>
void return_holder(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t result, Make make) {
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t out = frame.at(0u);
    make(out);
    if (result != out) (void)call_guest(rt, ctx, kHolderReset, {result, rt.memory().load32(out)});
    destroy(rt, ctx, out);
}

// The shape of toInt/toLong/toULong: no argument converts self with its own vslot; one argument
// is the base, parsed from self as a string.
template <typename NoBase, typename WithBase>
void convert(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, NoBase no_base, WithBase with_base) {
    const std::uint32_t argc = ctx.gpr[6];
    const std::uint32_t argv = ctx.gpr[7];
    if (argc == 0u) {
        no_base(rt.memory().load32(ctx.gpr[5]));
    } else if (argc == 1u) {
        GuestFrame frame(ctx, 0x10u);
        const std::uint32_t self = frame.at(0u);
        (void)call_guest(rt, ctx, kSelfAsString, {self});
        const auto base = static_cast<std::uint32_t>(to_int(rt, ctx, rt.memory().load32(argv)));
        with_base(rt.memory().load32(self), base);
        destroy(rt, ctx, self);
    }
}

} // namespace

// String.toInt([base]) -> integer.
void toInt(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    convert(rt, ctx,
        [&](std::uint32_t self) {
            GuestFrame frame(ctx, 0x10u);
            return_integer(rt, ctx, result, frame.at(0u), static_cast<std::uint32_t>(to_int(rt, ctx, self)));
        },
        [&](std::uint32_t string, std::uint32_t base) {
            GuestFrame frame(ctx, 0x10u);
            return_integer(rt, ctx, result, frame.at(0u), call_guest(rt, ctx, kStringToIntBase, {string, base}));
        });
}

// String.toLong([base]) / toULong([base]) -> 64-bit integer.
void toLong(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const auto hand_out = [&](GuestResult value) {
        return_holder(rt, ctx, result, [&](std::uint32_t out) { (void)call_guest(rt, ctx, kNewSignedLong, {out, 0u, value.v0, value.v1}); });
    };
    convert(rt, ctx,
        [&](std::uint32_t self) { hand_out(call_guest(rt, ctx, virtual_function(rt, self, kToLongSlot), {self}, {})); },
        [&](std::uint32_t string, std::uint32_t base) { hand_out(call_guest(rt, ctx, kStringToLongBase, {string, base}, {})); });
}
void toULong(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const auto hand_out = [&](GuestResult value) {
        return_holder(rt, ctx, result, [&](std::uint32_t out) { (void)call_guest(rt, ctx, kNewUnsignedLong, {out, 0u, value.v0, value.v1}); });
    };
    convert(rt, ctx,
        [&](std::uint32_t self) { hand_out(call_guest(rt, ctx, virtual_function(rt, self, kToULongSlot), {self}, {})); },
        [&](std::uint32_t string, std::uint32_t base) { hand_out(call_guest(rt, ctx, kStringToULongBase, {string, base}, {})); });
}

// String.upcase() -> a new upper-case string.
void upcase(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t self = frame.at(0u);
    (void)call_guest(rt, ctx, kSelfAsString, {self});
    return_holder(rt, ctx, result, [&](std::uint32_t out) { (void)call_guest(rt, ctx, kStringUpcase, {out, rt.memory().load32(self)}); });
    destroy(rt, ctx, self);
}

// String.substr() -> self; substr(pos) / substr(pos, n) -> the substring (std::string::substr,
// which throws when pos is past the end).
void substr(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t self_holder = ctx.gpr[5];
    const std::uint32_t argc = ctx.gpr[6];
    const std::uint32_t argv = ctx.gpr[7];
    auto &memory = rt.memory();
    if (argc == 0u) {
        GuestFrame frame(ctx, 0x10u);
        const std::uint32_t unused = frame.at(0u);
        (void)call_guest(rt, ctx, kSelfAsString, {unused});
        return_holder(rt, ctx, result, [&](std::uint32_t out) { (void)call_guest(rt, ctx, kSelfAsString, {out, self_holder}); });
        destroy(rt, ctx, unused);
        return;
    }
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t self = frame.at(0x0u);
    const std::uint32_t part = frame.at(0x4u);
    to_string(rt, ctx, memory.load32(self_holder), self);
    const auto pos = static_cast<std::uint32_t>(to_int(rt, ctx, memory.load32(argv)));
    const std::uint32_t count = argc == 1u ? 0xFFFFFFFFu : static_cast<std::uint32_t>(to_int(rt, ctx, memory.load32(argv + 4u)));
    if (memory.load32(memory.load32(self) - 0xCu) < pos) (void)call_guest(rt, ctx, kThrowOutOfRange, {kSubstrWhat});
    (void)call_guest(rt, ctx, kStdStringSubstr, {part, self, pos, count});
    return_string(rt, ctx, result, part);
    string_release(rt, ctx, memory.load32(part));
    string_release(rt, ctx, memory.load32(self));
}

// pdistd.AsciiStringHash(text) -> 64-bit hash. Module function: (result, argc, argv).
void AsciiStringHash(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const auto argc = static_cast<std::int32_t>(ctx.gpr[5]);
    const std::uint32_t argv = ctx.gpr[6];
    if (argc <= 0) return;
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t text = frame.at(0u);
    to_string(rt, ctx, rt.memory().load32(argv), text);
    const GuestResult hash = call_guest(rt, ctx, kAsciiStringHash, {rt.memory().load32(text)}, {});
    return_holder(rt, ctx, result, [&](std::uint32_t out) { (void)call_guest(rt, ctx, kNewUnsignedLong, {out, 0u, hash.v0, hash.v1}); });
    string_release(rt, ctx, rt.memory().load32(text));
}

} // namespace gt::adhoc::string

GT_DECOMP(0x08A6CB30u, gt::adhoc::string::toInt, gt::ReturnKind::Void);
GT_DECOMP(0x08A6CD70u, gt::adhoc::string::toLong, gt::ReturnKind::Void);
GT_DECOMP(0x08A6CE98u, gt::adhoc::string::toULong, gt::ReturnKind::Void);
GT_DECOMP(0x08A6F9ECu, gt::adhoc::string::upcase, gt::ReturnKind::Void);
GT_DECOMP(0x08A6E0DCu, gt::adhoc::string::substr, gt::ReturnKind::Void);
GT_DECOMP(0x08A4C8D8u, gt::adhoc::string::AsciiStringHash, gt::ReturnKind::Void);
