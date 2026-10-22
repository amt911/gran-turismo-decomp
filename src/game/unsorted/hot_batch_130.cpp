// Hot batch 130: functions of 500+ instructions the 11-route gate reaches, written by hand from `tools.disasm`;
// role words describe access shapes, none is a game name.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>
#include <optional>

namespace gt::game {
namespace hot_batch_130 {
namespace {

using namespace gt::game::hot;

inline void take(psprecomp::AllegrexContext &ctx, const GuestResult &result) {
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}
// A guest call whose results stay in the registers like the original's: v0 / v1 / f0 of the callee are what the next instruction sees.
inline GuestResult call_keep(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments,
                             std::initializer_list<float> float_arguments) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, float_arguments);
    take(ctx, result);
    return result;
}
// A guest call with the argument registers set like the original sets them before the jal (a0-a3, t0-t3 in order): what the callee does not take stays as the caller had it.
inline std::uint32_t call_set(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments) {
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments, {}).v0;
}
// The same with float arguments in f12-f15 as well.
inline GuestResult call_setf(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments,
                             std::initializer_list<float> float_arguments) {
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    unsigned freg = 12u;
    for (const float argument : float_arguments) {
        if (freg <= 15u) ctx.fpr[freg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments, float_arguments);
}
// A virtual call `object->slot(...)`: `lw v1, 0(object); lw v0, slot(v1); jalr v0`, both registers set like the original sets them.
inline std::uint32_t call_virtual_at(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object, std::uint32_t slot,
                                     std::initializer_list<std::uint32_t> arguments) {
    ctx.gpr[3] = word(rt, object);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + slot);
    return call_set(rt, ctx, ctx.gpr[2], arguments);
}
inline std::uint32_t byte_u(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load8(address); }
inline std::uint32_t half_u(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load16(address); }
inline void put8(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
inline void put16(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
inline std::uint32_t sext8(std::uint32_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(value))); }
inline std::uint32_t sext16(std::uint32_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(value))); }
inline std::int32_t signed_of(std::uint32_t value) { return static_cast<std::int32_t>(value); }
// The Allegrex div's quotient: -1 (numerator >= 0) or 1 when dividing by zero, 0x80000000 for 0x80000000 / -1.
inline std::int32_t allegrex_div(std::int32_t numerator, std::int32_t denominator) {
    if (denominator == 0) return numerator >= 0 ? -1 : 1;
    if (numerator == INT32_MIN && denominator == -1) return INT32_MIN;
    return numerator / denominator;
}

// The lock pair several of these wrap their body in: func_08B344E0(lock) before, a tail jump to func_08B3457C(lock) after.
constexpr std::uint32_t kLock = 0x08B344E0u;
constexpr std::uint32_t kUnlock = 0x08B3457Cu;
// The value-object pair of the 0x08A5xxxx library: func_08A5BAD8(dst, src->+0) assigns, func_08A5B9EC(object) destroys.
constexpr std::uint32_t kAssign = 0x08A5BAD8u;
constexpr std::uint32_t kDestroy = 0x08A5B9ECu;

// The end of a function that built a value object in its frame at `temporary`: assigned to `target` (skipped when the
// target is that temporary itself), then the temporary is destroyed.
inline void keep_temporary(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t target, std::uint32_t temporary) {
    if (target != temporary) call_set(rt, ctx, kAssign, {target, word(rt, temporary)});
    call_set(rt, ctx, kDestroy, {temporary});
}

// Releases the shared string whose data pointer sits at `slot` (the libstdc++ copy-on-write layout: the rep 0xC bytes
// before the data, its count at -4): unless it is the empty rep `empty`, func_08B60804(&count, -1) steps the count and
// `release`(rep, frame) frees it when the old count was not positive.
inline void release_string(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t slot, std::uint32_t empty,
                           std::uint32_t release, std::uint32_t frame_base) {
    const std::uint32_t text = word(rt, slot);
    const std::uint32_t rep = text - 0xCu;
    ctx.gpr[4] = text - 4u;
    ctx.gpr[5] = 0xFFFFFFFFu;
    if (rep == empty) return;
    if (signed_of(call_set(rt, ctx, 0x08B60804u, {text - 4u, 0xFFFFFFFFu})) > 0) return;
    call_set(rt, ctx, release, {rep, frame_base});
}

} // namespace


// func_08A9109C(function, stream): loads a script function from a bytecode stream (version at stream+0x28; the code
// object is function+0x30, func_08A5BB7C("...") when null). Version < 8: the code gets that version (+0x34) and its name
// (+0x28, func_08AA69A0), and the parameters (12-byte entries at function+0x18: name, index, value) are read as names
// only (index = position), their count at code+0xC. Version >= 8: a flag byte (+0x38), the real version (+0x34, also
// stream+0x28 until the end), the name (from v9 a stream string when +0x38 is set, before v9 a C string), from v12
// the varargs flag (+0x39), the parameters with their indices, the captures (function+0x24) the same way, then
// code+0xC. Before v11 code+0x14 / +0x10 are read and +0x18 copies +0x14; from v11 +0x10 / +0x14 / +0x18. Then
// func_08A90888(function), the instruction count (vector code+0x1C reserved, func_08C2BFA0) and each instruction: an
// opcode (with a 32-bit line in a 5-byte header when +0x38 is set), func_08AADB10(opcode) makes it, slot 8 takes the
// line, slot 0x18 reads its operands and func_08A907CC(function, instruction) appends it. The stream's version is
// restored and the scratch string released.
void func_08A9109C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t function = ctx.gpr[4];
    const std::uint32_t stream = ctx.gpr[5];
    GuestFrame frame(ctx, 0x100u);
    const auto sp = [&](std::uint32_t offset) { return frame.at(offset); };
    constexpr std::uint32_t kEmpty = 0x08D74FD0u;
    const auto code = [&] {
        const std::uint32_t object = word(rt, function + 0x30u);
        if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB1E08u});
        return object;
    };
    const auto read_u32 = [&](std::uint32_t to) { (void)call_set(rt, ctx, 0x08AA6404u, {stream, to}); };
    const auto read_u8 = [&] { return call_set(rt, ctx, 0x08AA64DCu, {stream}); };
    const auto read_string = [&](std::uint32_t to) { (void)call_set(rt, ctx, 0x08AA69A0u, {stream, to}); };
    const std::uint32_t original_version = word(rt, stream + 0x28u);
    put(rt, sp(0x1Cu), kEmpty + 0xCu);
    // resize the vector at `list` (12-byte entries) to `count` entries, new ones copied from a {"", -1, nil} template
    // built at sp+name / +4 / +8 from a nil value at sp+value
    const auto resize = [&](std::uint32_t list, std::uint32_t count, std::uint32_t name, std::uint32_t value) {
        (void)call_set(rt, ctx, 0x08A76AA0u, {sp(value)});
        (void)call_set(rt, ctx, 0x08A6972Cu, {sp(name), 0x08CF6FDCu});
        put(rt, sp(name + 4u), 0xFFFFFFFFu);
        (void)call_set(rt, ctx, 0x08A76ABCu, {sp(name + 8u), sp(value)});
        const std::uint32_t begin = word(rt, list);
        const std::uint32_t size = ((word(rt, list + 4u) - begin) >> 2) * 0xAAAAAAABu;
        if (count < size) {
            (void)call_set(rt, ctx, 0x08C2C1FCu, {list, begin + count * 12u, word(rt, list + 4u)});
        } else {
            (void)call_set(rt, ctx, 0x08C2C56Cu, {list, word(rt, list + 4u), count - size, sp(name)});
        }
        (void)call_set(rt, ctx, kDestroy, {sp(name + 8u)});
        (void)call_set(rt, ctx, 0x08A69D68u, {sp(name)});
        (void)call_set(rt, ctx, kDestroy, {sp(value)});
    };
    // entry i of the vector at `list` gets the name read from the stream (and, with `indexed`, the index after it)
    const auto read_names = [&](std::uint32_t list, std::uint32_t symbol, bool indexed) {
        for (std::int32_t i = 0; i < signed_of(word(rt, sp(0x20u))); ++i) {
            read_string(sp(0x1Cu));
            const std::uint32_t entry = word(rt, list) + static_cast<std::uint32_t>(i) * 12u;
            (void)call_set(rt, ctx, 0x08A69654u, {sp(symbol), sp(0x1Cu)});
            (void)call_set(rt, ctx, 0x08A6A23Cu, {entry, sp(symbol)});
            (void)call_set(rt, ctx, 0x08A69D68u, {sp(symbol)});
            if (indexed) read_u32(sp(0x24u));
            put(rt, word(rt, list) + static_cast<std::uint32_t>(i) * 12u + 4u, indexed ? word(rt, sp(0x24u)) : static_cast<std::uint32_t>(i));
        }
    };
    if (signed_of(original_version) < 8) {
        put(rt, code() + 0x34u, word(rt, stream + 0x28u));
        read_string(code() + 0x28u);
        read_u32(sp(0x20u));
        resize(function + 0x18u, word(rt, sp(0x20u)), 0x40u, 0xCu);
        read_names(function + 0x18u, 0x18u, false);
        put(rt, code() + 0xCu, word(rt, sp(0x20u)));
    } else {
        {
            const std::uint32_t object = code();
            put8(rt, object + 0x38u, read_u8() == 1u ? 1u : 0u);
        }
        {
            const std::uint32_t object = code();
            put(rt, object + 0x34u, read_u8());
        }
        put(rt, stream + 0x28u, word(rt, code() + 0x34u));
        {
            const std::uint32_t object = code();
            if (signed_of(word(rt, object + 0x34u)) >= 9) {
                if (byte_u(rt, object + 0x38u) != 0u) read_string(code() + 0x28u);
            } else {
                const std::uint32_t target = code();
                const std::uint32_t length = call_set(rt, ctx, 0x08B35F60u, {sp(0x4Cu)});
                (void)call_set(rt, ctx, 0x08B8A4C0u, {target + 0x28u, sp(0x4Cu), length});
            }
        }
        if (signed_of(word(rt, code() + 0x34u)) >= 12) {
            const std::uint32_t object = code();
            put8(rt, object + 0x39u, read_u8() == 1u ? 1u : 0u);
        }
        read_u32(sp(0x20u));
        resize(function + 0x18u, word(rt, sp(0x20u)), 0x34u, 8u);
        read_names(function + 0x18u, 0x14u, true);
        read_u32(sp(0x20u));
        resize(function + 0x24u, word(rt, sp(0x20u)), 0x28u, 4u);
        read_names(function + 0x24u, 0x10u, true);
        read_u32(code() + 0xCu);
    }
    if (signed_of(word(rt, stream + 0x28u)) < 11) {
        read_u32(code() + 0x14u);
        read_u32(code() + 0x10u);
        const std::uint32_t to = code();
        const std::uint32_t from = code();
        put(rt, to + 0x18u, word(rt, from + 0x14u));
    } else {
        read_u32(code() + 0x10u);
        read_u32(code() + 0x14u);
        read_u32(code() + 0x18u);
    }
    read_u32(sp(0x20u));
    (void)call_set(rt, ctx, 0x08A90888u, {function});
    (void)call_set(rt, ctx, 0x08C2BFA0u, {code() + 0x1Cu, word(rt, sp(0x20u))});
    for (std::int32_t i = 0; i < signed_of(word(rt, sp(0x20u))); ++i) {
        std::uint32_t line = 0u;
        std::uint32_t opcode;
        if (byte_u(rt, code() + 0x38u) != 0u) {
            (void)call_set(rt, ctx, 0x08AA61D8u, {stream, sp(0x4Cu), 5u});
            line = byte_u(rt, sp(0x4Cu)) | byte_u(rt, sp(0x4Du)) << 8 | byte_u(rt, sp(0x4Eu)) << 16 | byte_u(rt, sp(0x4Fu)) << 24;
            opcode = byte_u(rt, sp(0x50u));
        } else {
            opcode = read_u8();
        }
        const std::uint32_t instruction = call_set(rt, ctx, 0x08AADB10u, {opcode});
        (void)call_set(rt, ctx, word(rt, word(rt, instruction) + 8u), {instruction, line});
        (void)call_set(rt, ctx, word(rt, word(rt, instruction) + 0x18u), {instruction, stream});
        (void)call_set(rt, ctx, 0x08A907CCu, {function, instruction});
    }
    put(rt, stream + 0x28u, original_version);
    if (word(rt, sp(0x1Cu)) - 0xCu == kEmpty) ctx.gpr[2] = kEmpty;  // the asm's v0 when nothing is released
    release_string(rt, ctx, sp(0x1Cu), kEmpty, 0x08B8542Cu, sp(0u));
}

} // namespace hot_batch_130
} // namespace gt::game

#define HOT130(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_130::name, gt::ReturnKind::kind)
HOT130(0x08A9109Cu, func_08A9109C, Word);
