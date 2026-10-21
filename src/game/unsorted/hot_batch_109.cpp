// Hot batch 109: functions of up to 40 instructions the 11-route gate reaches, written by hand from `tools.disasm`;
// role words describe access shapes, none is a game name.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>

namespace gt::game {
namespace hot_batch_109 {
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
inline std::uint32_t swapped(std::uint32_t value) { return __builtin_bswap32(value); }
inline std::int32_t signed_of(std::uint32_t value) { return static_cast<std::int32_t>(value); }

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

} // namespace

// func_088DA57C(a0) -> tail jump to func_088D9BBC(a0, 0, 0).
void func_088DA57C(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    tail_call(ctx, 0x088D9BBCu, {ctx.gpr[4], 0u, 0u});
}

// func_088936D4(holder) -> tail jump to func_088934C4(holder->+0).
void func_088936D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    tail_call(ctx, 0x088934C4u, {word(rt, ctx.gpr[4])});
}

// func_08891EF8(holder, byte) -> tail jump to func_08891E78(holder->+0, byte & 0xFF).
void func_08891EF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    tail_call(ctx, 0x08891E78u, {word(rt, ctx.gpr[4]), ctx.gpr[5] & 0xFFu});
}

// func_08AC0B50() -> tail jump to func_08B0E6B0(0x08CFFB80), a fixed object in .bss.
void func_08AC0B50(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    tail_call(ctx, 0x08B0E6B0u, {0x08CFFB80u});
}

// func_0888F374(holder) -> func_0888EE48(holder->+0).
void func_0888F374(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    call_set(rt, ctx, 0x0888EE48u, {word(rt, ctx.gpr[4])});
}

// func_08ADF910(...) -> func_08ADF880 with f12 = 0.0 and the other argument registers passed on.
void func_08ADF910(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    call_setf(rt, ctx, 0x08ADF880u, {}, {0.0f});
}

// func_08B31960(...) -> func_08C7B1F8(...) == 1.
void func_08B31960(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t result = call_set(rt, ctx, 0x08C7B1F8u, {});
    ctx.gpr[2] = result == 1u ? 1u : 0u;
}

// func_08AD96D8(a0, f12): func_08AD95A0(a0, f12), then -> func_08AD95EC(0, f12).
void func_08AD96D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const float value = ctx.fpr[12];
    GuestFrame frame(ctx, 0x20u);
    call_setf(rt, ctx, 0x08AD95A0u, {}, {value});
    call_setf(rt, ctx, 0x08AD95ECu, {0u}, {value});
}

// func_08820BD4(): func_08B0ABC0(func_08B0979C(), 0, 0); func_08820A54(0); then a tail jump to func_08820B68(0).
void func_08820BD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t object = call_set(rt, ctx, 0x08B0979Cu, {});
    call_set(rt, ctx, 0x08B0ABC0u, {object, 0u, 0u});
    call_set(rt, ctx, 0x08820A54u, {0u});
    tail_call(ctx, 0x08820B68u, {0u});
}

// func_08843330(target): func_088432E0 builds a value object in the frame, which goes to target.
void func_08843330(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t target = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t temporary = frame.at(0u);
    call_set(rt, ctx, 0x088432E0u, {temporary});
    keep_temporary(rt, ctx, target, temporary);
}

// func_088DE700(self, second): func_088DE24C(self, 1, 0), then a tail jump to func_088DCD28(self, second).
void func_088DE700(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t second = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    call_set(rt, ctx, 0x088DE24Cu, {self, 1u, 0u});
    tail_call(ctx, 0x088DCD28u, {self, second});
}

// func_08A5FB60(result, source) -> result: a value object built in the frame by func_08A5FA8C(temp, source->+0x14) is
// handed to func_08A76ABC(result, temp), then destroyed.
void func_08A5FB60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t source = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t temporary = frame.at(0u);
    call_set(rt, ctx, 0x08A5FA8Cu, {temporary, word(rt, source + 0x14u)});
    call_set(rt, ctx, 0x08A76ABCu, {result, temporary});
    call_set(rt, ctx, kDestroy, {temporary});
    ctx.gpr[2] = result;
}

// func_0892C7C8(first, second) -> the byte at +0x16 of the record func_08936584(func_089363FC(), a1, first, second, record)
// fills in the frame.
void func_0892C7C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t first = ctx.gpr[4];
    const std::uint32_t second = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t object = call_set(rt, ctx, 0x089363FCu, {});
    call_set(rt, ctx, 0x08936584u, {object, ctx.gpr[5], first, second, frame.at(0u)});
    ctx.gpr[2] = byte_u(rt, frame.at(0x16u));
}

// func_08996354(self, f12): under the lock at 0x08CF4598, self->+0x178 = f12.
void func_08996354(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t lock = 0x08CF4598u;
    const std::uint32_t self = ctx.gpr[4];
    const float value = ctx.fpr[12];
    GuestFrame frame(ctx, 0x20u);
    call_set(rt, ctx, kLock, {lock});
    put_float(rt, self + 0x178u, value);
    tail_call(ctx, kUnlock, {lock});
}

// func_08A83248(result, source) -> result: like func_08A5FB60 with func_08A83160(temp, source, source->+0x18, source->+0x1C).
void func_08A83248(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t source = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t temporary = frame.at(0u);
    call_set(rt, ctx, 0x08A83160u, {temporary, source, word(rt, source + 0x18u), word(rt, source + 0x1Cu)});
    call_set(rt, ctx, 0x08A76ABCu, {result, temporary});
    call_set(rt, ctx, kDestroy, {temporary});
    ctx.gpr[2] = result;
}

// func_08A887AC(self, target): func_08A87EEC(temp, -self->+0x14 as a float) builds a value object that goes to target.
void func_08A887AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const float value = -float_at(rt, ctx.gpr[4] + 0x14u);
    const std::uint32_t target = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t temporary = frame.at(0u);
    call_setf(rt, ctx, 0x08A87EECu, {temporary}, {value});
    keep_temporary(rt, ctx, target, temporary);
}

// func_08AABA8C(self, text) -> self: func_08BEED1C(self, text, func_08B35F60(text)).
void func_08AABA8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t text = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t length = call_set(rt, ctx, 0x08B35F60u, {text});
    call_set(rt, ctx, 0x08BEED1Cu, {self, text, length});
    ctx.gpr[2] = self;
}

// func_08AD5524(self, pair) -> 1: func_08AD5394(self, pair); self->+0xC (byte) = 0, self->+4 = pair->+0, self->+8 = pair->+4.
void func_08AD5524(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t pair = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    call_set(rt, ctx, 0x08AD5394u, {});
    const std::uint32_t second = word(rt, pair + 4u);
    const std::uint32_t first = word(rt, pair);
    put8(rt, self + 0xCu, 0u);
    put(rt, self + 4u, first);
    put(rt, self + 8u, second);
    ctx.gpr[2] = 1u;
}

// func_08A4C76C(target): the 64-bit result of func_08B2E05C() goes to func_08A83160(temp, a1, low, high); the value object
// goes to target.
void func_08A4C76C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t target = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t temporary = frame.at(0u);
    call_set(rt, ctx, 0x08B2E05Cu, {});
    call_set(rt, ctx, 0x08A83160u, {temporary, ctx.gpr[5], ctx.gpr[2], ctx.gpr[3]});
    keep_temporary(rt, ctx, target, temporary);
}

// func_089AFF3C(first, second): func_089AFE84(first, second) under the lock at 0x08CF4ACC.
void func_089AFF3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t lock = 0x08CF4ACCu;
    const std::uint32_t first = ctx.gpr[4];
    const std::uint32_t second = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    call_set(rt, ctx, kLock, {lock});
    call_set(rt, ctx, 0x089AFE84u, {first, second});
    tail_call(ctx, kUnlock, {lock});
}

// func_08A30E48(target): func_08A97A6C(temp, func_08B31960(0x08D69170)) builds a value object that goes to target.
void func_08A30E48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t target = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t temporary = frame.at(0u);
    const std::uint32_t flag = call_set(rt, ctx, 0x08B31960u, {0x08D69170u});
    call_set(rt, ctx, 0x08A97A6Cu, {temporary, flag});
    keep_temporary(rt, ctx, target, temporary);
}

// func_08A79A5C(target, left, right): func_08A97A6C(temp, left->+0 != right->+0) builds a value object that goes to target.
void func_08A79A5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t target = ctx.gpr[4];
    const std::uint32_t differ = word(rt, ctx.gpr[5]) != word(rt, ctx.gpr[6]) ? 1u : 0u;
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t temporary = frame.at(0u);
    call_set(rt, ctx, 0x08A97A6Cu, {temporary, differ});
    keep_temporary(rt, ctx, target, temporary);
}

// func_08AAB604(self, value) -> self: func_08B37640 formats value with the pattern at 0x08CB318C into a frame buffer, then
// func_08BEED1C(self, buffer, func_08B35F60(buffer)).
void func_08AAB604(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t value = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t buffer = frame.at(0u);
    call_set(rt, ctx, 0x08B37640u, {buffer, 0x08CB318Cu, value});
    const std::uint32_t length = call_set(rt, ctx, 0x08B35F60u, {buffer});
    call_set(rt, ctx, 0x08BEED1Cu, {self, buffer, length});
    ctx.gpr[2] = self;
}

// func_08862BF8(self) -> 0 when func_08862A90(self) is 0; otherwise e = func_088915D4(self->+0 + 0x3F80),
// func_088DADAC(self->+0->+0x5AC, byte-swapped e->+0xE0), and 1.
void func_08862BF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    if (call_set(rt, ctx, 0x08862A90u, {}) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const std::uint32_t entry = call_set(rt, ctx, 0x088915D4u, {word(rt, self) + 0x3F80u});
    const std::uint32_t value = swapped(word(rt, entry + 0xE0u));
    call_set(rt, ctx, 0x088DADACu, {word(rt, word(rt, self) + 0x5ACu), value});
    ctx.gpr[2] = 1u;
}

// func_0885EC28(self, second) -> func_088DE700(o, a1) + 8, or 0 when that is 0, with o = self->+0->+0x5AC; then
// func_088DA57C(o, second).
void func_0885EC28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t second = ctx.gpr[5];
    const std::uint32_t object = word(rt, word(rt, ctx.gpr[4]) + 0x5ACu);
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t found = call_set(rt, ctx, 0x088DE700u, {object});
    const std::uint32_t result = found != 0u ? found + 8u : 0u;
    call_set(rt, ctx, 0x088DA57Cu, {object, second});
    ctx.gpr[2] = result;
}

// func_08B0E560(self, other): under the lock at self+4, other->+0x58 = self, func_08B1FBEC(self + 0xF8, other + 0x5C),
// self->+0xA4 = 0; the unlock (func_08B34B24) is a tail jump.
void func_08B0E560(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t other = ctx.gpr[5];
    const std::uint32_t lock = self + 4u;
    GuestFrame frame(ctx, 0x10u);
    call_set(rt, ctx, 0x08B34888u, {lock});
    put(rt, other + 0x58u, self);
    call_set(rt, ctx, 0x08B1FBECu, {self + 0xF8u, other + 0x5Cu});
    put(rt, self + 0xA4u, 0u);
    tail_call(ctx, 0x08B34B24u, {lock});
}

// func_089969BC(self, flag): when flag, func_089966DC(self, flag, &x, &y) fills two floats that go (copied, y first in
// the frame's second pair) to func_0896A9C4(self->+0x130, &x', &y', 0, 0).
void func_089969BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t flag = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[6] = frame.at(0u);
    ctx.gpr[7] = frame.at(4u);
    if (flag == 0u) return;
    call_set(rt, ctx, 0x089966DCu, {self, flag, frame.at(0u), frame.at(4u)});
    put(rt, frame.at(0xCu), word(rt, frame.at(0u)));
    put(rt, frame.at(8u), word(rt, frame.at(4u)));
    call_set(rt, ctx, 0x0896A9C4u, {word(rt, self + 0x130u), frame.at(0xCu), frame.at(8u), 0u, 0u});
}

// func_08B8DA64(a0, holder): nothing when holder is 0; when holder->+4 is set its virtual slot 0 runs with holder->+0 and
// both words are cleared; then a tail jump to func_08B64654(holder).
void func_08B8DA64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t holder = ctx.gpr[5];
    if (holder == 0u) return;
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t object = word(rt, holder + 4u);
    if (object != 0u) {
        call_virtual_at(rt, ctx, object, 0u, {object, word(rt, holder)});
        put(rt, holder + 4u, 0u);
        put(rt, holder, 0u);
    }
    tail_call(ctx, 0x08B64654u, {holder});
}

// func_08829B34(): func_088297A8 fills a value object in the frame; when its word is 0 func_08A5BB7C(0x08C9A2F8) runs;
// r = func_088294D0(word); the temporary is destroyed; then func_088936D4(func_0885EE58(r)).
void func_08829B34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t temporary = frame.at(0u);
    call_set(rt, ctx, 0x088297A8u, {temporary});
    const std::uint32_t value = word(rt, temporary);
    ctx.gpr[4] = 0x08C9A2F8u;
    if (value == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9A2F8u});
    const std::uint32_t result = call_set(rt, ctx, 0x088294D0u, {value});
    call_set(rt, ctx, kDestroy, {temporary});
    const std::uint32_t next = call_set(rt, ctx, 0x0885EE58u, {result});
    call_set(rt, ctx, 0x088936D4u, {next});
}

// func_088909BC(self, index) -> base + index x 32, where e = func_0888F550(self, index) and base = swapped(e->+0xDC),
// plus self->+0 when swapped(e->+4) is 0.
void func_088909BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t index = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t entry = call_set(rt, ctx, 0x0888F550u, {});
    std::uint32_t base = swapped(word(rt, entry + 0xDCu));
    if (swapped(word(rt, entry + 4u)) == 0u) base += word(rt, self);
    ctx.gpr[2] = base + (index << 5);
}

// func_08AD2848(self): under the lock at o + 0x40B0 (o = self->+0x78): func_08AD5EC8(o->+0x29C), func_08AD22D4(o, self),
// func_08AD5ED0(o->+0x29C).
void func_08AD2848(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t owner = word(rt, self + 0x78u);
    const std::uint32_t lock = owner + 0x40B0u;
    GuestFrame frame(ctx, 0x10u);
    call_set(rt, ctx, kLock, {lock});
    call_set(rt, ctx, 0x08AD5EC8u, {word(rt, owner + 0x29Cu)});
    call_set(rt, ctx, 0x08AD22D4u, {owner, self});
    call_set(rt, ctx, 0x08AD5ED0u, {word(rt, owner + 0x29Cu)});
    tail_call(ctx, kUnlock, {lock});
}

// func_08BEEAA8(self, first, second): a value object made by func_08A76AA0 in the frame goes to the function at
// self->+0x14 as (temp, first, 1, second), then is destroyed.
void func_08BEEAA8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t first = ctx.gpr[5];
    const std::uint32_t second = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t temporary = frame.at(0u);
    call_set(rt, ctx, 0x08A76AA0u, {temporary});
    ctx.gpr[2] = word(rt, self + 0x14u);
    call_set(rt, ctx, ctx.gpr[2], {temporary, first, 1u, second});
    call_set(rt, ctx, kDestroy, {temporary});
}

// func_08BF8B64(head): frees every node of the circular list at head (next at +0): its value object at +8 is destroyed and
// func_08A5BC54(node, 0xC, 4) releases the node.
void func_08BF8B64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t head = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    std::uint32_t node = word(rt, head);
    while (node != head) {
        const std::uint32_t next = word(rt, node);
        call_set(rt, ctx, kDestroy, {node + 8u});
        call_set(rt, ctx, 0x08A5BC54u, {node, 0xCu, 4u});
        node = next;
    }
}

// func_088988B4(self, base, third) -> base + func_08898874(third), after self->+0x40's virtual slot 0x44 runs with
// func_088F9420(self, base + 0x10).
void func_088988B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t base = ctx.gpr[5];
    const std::uint32_t third = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t value = call_set(rt, ctx, 0x088F9420u, {self, base + 0x10u});
    const std::uint32_t object = word(rt, self + 0x40u);
    call_virtual_at(rt, ctx, object, 0x44u, {object, value});
    ctx.gpr[2] = base + call_set(rt, ctx, 0x08898874u, {third});
}

// func_088F9C80(holder, key) -> the 15-bit signed value of the first (key, value) halfword pair in the array
// [holder->+0->+4, holder->+0->+8) whose signed key equals key, or -1.
void func_088F9C80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t table = word(rt, ctx.gpr[4]);
    const std::uint32_t end = word(rt, table + 8u);
    const std::uint32_t key = ctx.gpr[5];
    for (std::uint32_t entry = word(rt, table + 4u); entry != end; entry += 4u) {
        if (sext16(half_u(rt, entry)) == key) {
            ctx.gpr[2] = static_cast<std::uint32_t>(static_cast<std::int32_t>(half_u(rt, entry + 2u) << 17) >> 17);
            return;
        }
    }
    ctx.gpr[2] = 0xFFFFFFFFu;
}

// func_0888B5CC(self, byte, value): when self->+0x1C is 1, func_08891EF8(self->+0x14, byte); then func_088936A4(self->+0x18,
// message) with a frame message {0xB, byte at +8, value at +0xC}.
void func_0888B5CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t byte = ctx.gpr[5] & 0xFFu;
    const std::uint32_t value = ctx.gpr[6];
    GuestFrame frame(ctx, 0xB0u);
    ctx.gpr[5] = byte;
    if (word(rt, self + 0x1Cu) == 1u) call_set(rt, ctx, 0x08891EF8u, {word(rt, self + 0x14u), byte});
    const std::uint32_t message = frame.at(0u);
    put8(rt, message + 8u, byte);
    put(rt, message + 0xCu, value);
    put(rt, message, 0xBu);
    call_set(rt, ctx, 0x088936A4u, {word(rt, self + 0x18u), message});
}

// func_088BB44C(self, a1, list, a3, t0): func_088BAE08(self, a1, a3, t0), func_088BB3F8(self, list); when
// func_08949A08(list->+0) is nonzero, self->+8 (float) is scaled by 0.95.
void func_088BB44C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t list = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    call_set(rt, ctx, 0x088BAE08u, {self, ctx.gpr[5], ctx.gpr[7], ctx.gpr[8]});
    call_set(rt, ctx, 0x088BB3F8u, {self, list});
    if (call_set(rt, ctx, 0x08949A08u, {word(rt, list)}) != 0u) {
        put_float(rt, self + 8u, psp_mul(float_at(rt, self + 8u), std::bit_cast<float>(0x3F733333u)));
    }
}

// func_088DB0AC(self, index): when self->+0x134 (an object) is set and the signed byte self->+(0x78 + index) is not -1,
// the object's virtual slot 0x44 runs with (object, self + 4, &index as a halfword, 1) and that byte becomes -1.
void func_088DB0AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t index = ctx.gpr[5];
    const std::uint32_t object = word(rt, self + 0x134u);
    if (object == 0u) return;
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t slot = index + self + 0x78u;
    ctx.gpr[2] = sext8(byte_u(rt, slot));
    put16(rt, frame.at(0u), index);
    if (ctx.gpr[2] == 0xFFFFFFFFu) return;
    const std::uint32_t table = word(rt, object);
    ctx.gpr[2] = table;
    ctx.gpr[3] = word(rt, table + 0x44u);
    call_set(rt, ctx, ctx.gpr[3], {object, self + 4u, frame.at(0u), 1u});
    put8(rt, slot, 0xFFu);
}

// func_088E0BE4(self, key) -> func_088F9C80(self->+0x5D4, key), after waiting on the lock at self + 0x5E8 (taken and
// released at once) unless both self->+0x5D0 and self->+0x5D4 are set.
void func_088E0BE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t key = ctx.gpr[5];
    const std::uint32_t lock = self + 0x5E8u;
    GuestFrame frame(ctx, 0x10u);
    if (word(rt, self + 0x5D0u) == 0u || word(rt, self + 0x5D4u) == 0u) {
        call_set(rt, ctx, kLock, {lock});
        call_set(rt, ctx, kUnlock, {lock});
    }
    tail_call(ctx, 0x088F9C80u, {word(rt, self + 0x5D4u), key});
}

// func_08AC45F4(): size = func_08C1A1B8(0x08CFFD10) x 0x9A00; func_08B2B760(0x08CFFCF4, func_08B2B4D8(0x80, size), size,
// 0x9A00); func_08B2EEB8(0x08CFFCB8); then a tail jump to func_08C7ACD0.
void func_08AC45F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t count = call_set(rt, ctx, 0x08C1A1B8u, {0x08CFFD10u});
    const std::uint32_t size = count * 0x9A00u;
    const std::uint32_t block = call_set(rt, ctx, 0x08B2B4D8u, {0x80u, size});
    call_set(rt, ctx, 0x08B2B760u, {0x08CFFCF4u, block, size, 0x9A00u});
    call_set(rt, ctx, 0x08B2EEB8u, {0x08CFFCB8u});
    tail_call(ctx, 0x08C7ACD0u, {});
}

// func_08ADC27C(a0, flag) -> 0 when func_08ADB278(a0, 0x08C90EA4) finds nothing; otherwise, while flag and the found
// record's +0x198 is positive, func_08AD96D8(f12 = 0); the result is (+0x198 > 0) at the end.
void func_08ADC27C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const bool flag = ctx.gpr[5] != 0u;
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t record = call_set(rt, ctx, 0x08ADB278u, {ctx.gpr[4], 0x08C90EA4u});
    if (record == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    for (;;) {
        const bool positive = signed_of(word(rt, record + 0x198u)) > 0;
        ctx.fpr[12] = 0.0f;
        if (!(flag && positive)) {
            ctx.gpr[2] = positive ? 1u : 0u;
            return;
        }
        call_setf(rt, ctx, 0x08AD96D8u, {}, {0.0f});
    }
}

// func_08841A60(self): the destructor that also frees: vtable 0x08CBD9A8; when self->+0x18 is set its virtual slot 0
// runs with self->+0x14 and both words are cleared; func_08A780B0(self); tail jump to func_08A5BC54(self, 0x1C, 4).
void func_08841A60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    put(rt, self, 0x08CBD9A8u);
    const std::uint32_t object = word(rt, self + 0x18u);
    if (object != 0u) {
        call_virtual_at(rt, ctx, object, 0u, {object, word(rt, self + 0x14u)});
        put(rt, self + 0x18u, 0u);
        put(rt, self + 0x14u, 0u);
    }
    call_set(rt, ctx, 0x08A780B0u, {self});
    tail_call(ctx, 0x08A5BC54u, {self, 0x1Cu, 4u});
}

// func_088A7C78(self, value): when self->+0x30 is set and the halfword +0xE0 of func_08B03BB4(self->+8) is 0x1000, a tail
// jump to self->+0x30's virtual slot 8 with (object, self->+0x2C, value).
void func_088A7C78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t value = ctx.gpr[5];
    if (word(rt, self + 0x30u) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t entry = call_set(rt, ctx, 0x08B03BB4u, {word(rt, self + 8u)});
    ctx.gpr[6] = value;
    if (sext16(half_u(rt, entry + 0xE0u)) != 0x1000u) {
        ctx.gpr[2] = 0x1000u;
        return;
    }
    const std::uint32_t object = word(rt, self + 0x30u);
    const std::uint32_t table = word(rt, object);
    ctx.gpr[2] = table;
    ctx.gpr[25] = word(rt, table + 8u);
    tail_call(ctx, ctx.gpr[25], {object, word(rt, self + 0x2Cu), value});
}

// func_088DD870(self) -> how many of the first func_088D9F80(self) records (0x2F8 bytes each from self->+0xC0) have +4 == 1
// and a signed +0x1FC of at least -1.
void func_088DD870(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const std::int32_t count = signed_of(call_set(rt, ctx, 0x088D9F80u, {}));
    std::uint32_t matches = 0u;
    std::uint32_t offset = 0u;
    for (std::int32_t index = 0; index < count; ++index, offset += 0x2F8u) {
        const std::uint32_t record = word(rt, self + 0xC0u) + offset;
        if (word(rt, record + 4u) == 1u && signed_of(word(rt, record + 0x1FCu)) >= -1) ++matches;
    }
    ctx.gpr[2] = matches;
}

// func_0899261C(self, value): func_08A005C0(self, value); when value is self->+0x120: func_0896A824(value, 1),
// func_089663BC(self->+0x120, 1) and a tail jump to func_089662F8(self->+0x120, 1).
void func_0899261C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t value = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    call_set(rt, ctx, 0x08A005C0u, {});
    ctx.gpr[2] = word(rt, self + 0x120u);
    ctx.gpr[4] = value;
    ctx.gpr[5] = 1u;
    if (value != ctx.gpr[2]) return;
    call_set(rt, ctx, 0x0896A824u, {value, 1u});
    call_set(rt, ctx, 0x089663BCu, {word(rt, self + 0x120u), 1u});
    tail_call(ctx, 0x089662F8u, {word(rt, self + 0x120u), 1u});
}

// func_08B34060(): once, while the word at 0x08D69274 is not positive, it becomes func_08B2ECAC(0x08CB54F0, 0x08B34270, 0)
// and func_08C7A820(0x08CB54FC, 0x02415821, &value, 4, 0, 0) gets it.
void func_08B34060(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t kHandle = 0x08D69274u;
    ctx.gpr[2] = word(rt, kHandle);
    if (signed_of(ctx.gpr[2]) > 0) return;
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t handle = call_set(rt, ctx, 0x08B2ECACu, {0x08CB54F0u, 0x08B34270u, 0u});
    put(rt, kHandle, handle);
    put(rt, frame.at(0u), handle);
    call_set(rt, ctx, 0x08C7A820u, {0x08CB54FCu, 0x02415821u, frame.at(0u), 4u, 0u, 0u});
}

// func_08893B0C(text) -> the index of the first of the 10 strings listed at 0x08CDD054 that func_08B35FD8(text, string)
// calls equal (0), or -1.
void func_08893B0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t kStrings = 0x08CDD054u;
    const std::uint32_t text = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    for (std::uint32_t index = 0u; index < 10u; ++index) {
        if (call_set(rt, ctx, 0x08B35FD8u, {text, word(rt, kStrings + index * 4u)}) == 0u) {
            ctx.gpr[2] = index;
            return;
        }
    }
    ctx.gpr[2] = 0xFFFFFFFFu;
}

// func_08A2B5DC(target): p = func_08B09A04(func_08B0979C()); func_08B3027C(p) when p->+4 (byte) is set; then
// func_08A97A6C(temp, func_08B327AC(0x08D69170, 0)) builds a value object that goes to target.
void func_08A2B5DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t target = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t temporary = frame.at(0u);
    const std::uint32_t object = call_set(rt, ctx, 0x08B0979Cu, {});
    const std::uint32_t part = call_set(rt, ctx, 0x08B09A04u, {object});
    ctx.gpr[4] = part;
    if (byte_u(rt, part + 4u) != 0u) call_set(rt, ctx, 0x08B3027Cu, {part});
    const std::uint32_t value = call_set(rt, ctx, 0x08B327ACu, {0x08D69170u, 0u});
    call_set(rt, ctx, 0x08A97A6Cu, {temporary, value});
    keep_temporary(rt, ctx, target, temporary);
}

// func_08AA22BC(result, holder) -> result: o = holder->+0 (func_08A5BB7C(0x08CB2B50) first when it is 0); the float of o's
// virtual slot 0x54 goes through func_08A87EEC into a value object handed to func_08A76ABC(result, temp).
void func_08AA22BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t object = word(rt, ctx.gpr[5]);
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t temporary = frame.at(0u);
    ctx.gpr[4] = 0x08CB2B50u;
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB2B50u});
    call_virtual_at(rt, ctx, object, 0x54u, {object});
    call_setf(rt, ctx, 0x08A87EECu, {temporary}, {ctx.fpr[0]});
    call_set(rt, ctx, 0x08A76ABCu, {result, temporary});
    call_set(rt, ctx, kDestroy, {temporary});
    ctx.gpr[2] = result;
}

// func_08AA2494(result, holder) -> result: like func_08AA22BC with the word of slot 0x3C through func_08A85B1C.
void func_08AA2494(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t object = word(rt, ctx.gpr[5]);
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t temporary = frame.at(0u);
    ctx.gpr[4] = 0x08CB2B50u;
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB2B50u});
    const std::uint32_t value = call_virtual_at(rt, ctx, object, 0x3Cu, {object});
    call_set(rt, ctx, 0x08A85B1Cu, {temporary, value});
    call_set(rt, ctx, 0x08A76ABCu, {result, temporary});
    call_set(rt, ctx, kDestroy, {temporary});
    ctx.gpr[2] = result;
}

// func_08B2DEE4() -> (v0, v1) of func_08B2DE94(&b), or (-1, -1) when func_08C7B008(&a) or func_08C7B010(&b, &c) fails
// (func_08C7B000(&a, &c) between them); a, c and b sit at frame +0, +8 and +0x10.
void func_08B2DEE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t first = frame.at(0u);
    const std::uint32_t middle = frame.at(8u);
    const std::uint32_t last = frame.at(0x10u);
    const auto fail = [&ctx] {
        ctx.gpr[2] = 0xFFFFFFFFu;
        ctx.gpr[3] = 0xFFFFFFFFu;
    };
    if (call_set(rt, ctx, 0x08C7B008u, {first}) != 0u) {
        ctx.gpr[4] = first;
        ctx.gpr[5] = middle;
        return fail();
    }
    call_set(rt, ctx, 0x08C7B000u, {first, middle});
    if (call_set(rt, ctx, 0x08C7B010u, {last, middle}) != 0u) return fail();
    call_set(rt, ctx, 0x08B2DE94u, {last});
}

// func_08829C4C(a0, key): func_08829598(0), func_08829604(0); func_088297A8(temp, key) fills a value object in the frame;
// when its word is 0 func_08A5BB7C(0x08C9A2F8) runs; r = func_088294D0(word); the temporary is destroyed;
// func_0885E758(r, 0, 0).
void func_08829C4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t key = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t temporary = frame.at(0u);
    call_set(rt, ctx, 0x08829598u, {0u});
    call_set(rt, ctx, 0x08829604u, {0u});
    call_set(rt, ctx, 0x088297A8u, {temporary, key});
    const std::uint32_t value = word(rt, temporary);
    ctx.gpr[4] = 0x08C9A2F8u;
    if (value == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9A2F8u});
    const std::uint32_t result = call_set(rt, ctx, 0x088294D0u, {value});
    call_set(rt, ctx, kDestroy, {temporary});
    call_set(rt, ctx, 0x0885E758u, {result, 0u, 0u});
}

// The shared body of func_08894A88 / func_088966A8: func_088F891C(self, flag & 0xFF), then the 8-byte pair at self+pair
// copied to the frame goes to func_088B1658(&copy, flag ? value : 0), or to func_088B1418(&copy, value) when flag is set
// and value is negative.
inline void send_pair(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t pair) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t flag = ctx.gpr[5] & 0xFFu;
    const std::uint32_t value = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    call_set(rt, ctx, 0x088F891Cu, {self, flag});
    const std::uint32_t copy = frame.at(0u);
    put(rt, copy, word(rt, self + pair));
    put(rt, copy + 4u, word(rt, self + pair + 4u));
    if (flag != 0u && signed_of(value) < 0) {
        call_set(rt, ctx, 0x088B1418u, {copy, value});
        return;
    }
    call_set(rt, ctx, 0x088B1658u, {copy, flag != 0u ? value : 0u});
}

// func_08894A88(self, flag, value): send_pair over self+0x54.
void func_08894A88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { send_pair(rt, ctx, 0x54u); }

// func_088966A8(self, flag, value): send_pair over self+0x58.
void func_088966A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { send_pair(rt, ctx, 0x58u); }

// The shared body of func_088FDB9C / func_088FDC78 -> a float of a block tagged 'Gcif' (big-endian at +0): -1.0 unless the
// tag matches and index < func_088FDA60(block); otherwise the byte-swapped word at +field of func_088FDA84(block, index).
inline void tagged_float(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t field) {
    constexpr std::uint32_t kTag = 0x47636966u;
    const std::uint32_t block = ctx.gpr[4];
    const std::uint32_t index = ctx.gpr[5];
    ctx.fpr[0] = -1.0f;
    if (swapped(word(rt, block)) != kTag) {
        ctx.gpr[2] = kTag;
        return;
    }
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t count = call_set(rt, ctx, 0x088FDA60u, {});
    ctx.fpr[0] = -1.0f;
    if (!(signed_of(index) < signed_of(count))) {
        ctx.gpr[2] = 0u;
        return;
    }
    const std::uint32_t entry = call_set(rt, ctx, 0x088FDA84u, {block, index});
    ctx.fpr[0] = std::bit_cast<float>(swapped(word(rt, entry + field)));
}

// func_088FDB9C(block, index) -> tagged_float over +4.
void func_088FDB9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tagged_float(rt, ctx, 4u); }

// func_088FDC78(block, index) -> tagged_float over +0.
void func_088FDC78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tagged_float(rt, ctx, 0u); }

// func_088FAB04(self, out) -> -1 when func_088FABD8(self->+4, record) fails; otherwise func_08BBBE68(out, record + 0x1C),
// out->+0x10 = out->+0x14 = 0.5, out->+0x18 = 0, and the record's +0x30 (or -1 when its +0x34 is negative).
void func_088FAB04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t out = ctx.gpr[5];
    GuestFrame frame(ctx, 0x50u);
    const std::uint32_t record = frame.at(0u);
    put(rt, record + 0x30u, 0u);
    put(rt, record + 0x34u, 0xFFFFFFFFu);
    if (call_set(rt, ctx, 0x088FABD8u, {word(rt, self + 4u), record}) == 0u) {
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }
    call_set(rt, ctx, 0x08BBBE68u, {out, record + 0x1Cu});
    const std::uint32_t high = word(rt, record + 0x34u);
    const std::uint32_t low = word(rt, record + 0x30u);
    put_float(rt, out + 0x14u, 0.5f);
    put(rt, out + 0x18u, 0u);
    put_float(rt, out + 0x10u, 0.5f);
    ctx.gpr[2] = signed_of(high) < 0 ? 0xFFFFFFFFu : low;
}

// func_088FE620(self) -> 0 when self->+0->+0x20 is 0; -1 when its +0x48->+0x18 (a list) is 0; 0 when func_08AF9500(list)
// is 0; otherwise the position of that element: (element - list->+0x30) / 16 divided exactly by 3 (the multiply by the
// inverse 0xAAAAAAAB keeps the low word), halved toward zero.
void func_088FE620(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t owner = word(rt, word(rt, ctx.gpr[4]) + 0x20u);
    if (owner == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const std::uint32_t list = word(rt, word(rt, owner + 0x48u) + 0x18u);
    if (list == 0u) {
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t element = call_set(rt, ctx, 0x08AF9500u, {list});
    if (element == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const std::uint32_t units = static_cast<std::uint32_t>(signed_of(element - word(rt, list + 0x30u)) >> 4);
    const std::uint32_t thirds = units * 0xAAAAAAABu;
    ctx.gpr[2] = static_cast<std::uint32_t>(signed_of(thirds + (thirds >> 31)) >> 1);
}

// func_08994C00(self, value): under the lock at 0x08CF4470, func_08A0EB5C(temp, &value) builds a value object that goes to
// self + 0x118.
void func_08994C00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t lock = 0x08CF4470u;
    const std::uint32_t target = ctx.gpr[4] + 0x118u;
    const std::uint32_t value = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t temporary = frame.at(4u);
    call_set(rt, ctx, kLock, {lock});
    put(rt, frame.at(0u), value);
    call_set(rt, ctx, 0x08A0EB5Cu, {temporary, frame.at(0u)});
    keep_temporary(rt, ctx, target, temporary);
    call_set(rt, ctx, kUnlock, {lock});
}

// func_089B8E5C(a0, a1, object, other): func_08A5BA28(object); func_089B8BB4(a0, a1, other); func_089B8DA0(a0, a1, object,
// other->+0x54 read before); then a tail jump to func_08A5B970(object).
void func_089B8E5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t first = ctx.gpr[4];
    const std::uint32_t second = ctx.gpr[5];
    const std::uint32_t object = ctx.gpr[6];
    const std::uint32_t other = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    call_set(rt, ctx, 0x08A5BA28u, {object});
    const std::uint32_t field = word(rt, other + 0x54u);
    call_set(rt, ctx, 0x089B8BB4u, {first, second, other});
    call_set(rt, ctx, 0x089B8DA0u, {first, second, object, field});
    tail_call(ctx, 0x08A5B970u, {object});
}

// func_08A6FBD0(target): func_08A6C410 fills a value object at frame +0 (func_08A5BB7C(0x08CB0754) when its word is 0);
// func_08A6FA6C(temp, word) builds a second one at +4 that goes to target; both are destroyed.
void func_08A6FBD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t target = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t source = frame.at(0u);
    const std::uint32_t temporary = frame.at(4u);
    call_set(rt, ctx, 0x08A6C410u, {source});
    const std::uint32_t value = word(rt, source);
    ctx.gpr[4] = 0x08CB0754u;
    if (value == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB0754u});
    call_set(rt, ctx, 0x08A6FA6Cu, {temporary, value});
    keep_temporary(rt, ctx, target, temporary);
    call_set(rt, ctx, kDestroy, {source});
}

// func_08895FBC(self): nothing unless self's virtual slot 0x24 answers nonzero; then self->+0xA9 (byte) = 0 and, with
// part = self + 0x70: a tail jump to func_08894E10(part) when self->+0xA8 is set, otherwise func_08894EC4(part) and a
// tail jump to func_08894E8C(part, self->+0xAC).
void func_08895FBC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t part = self + 0x70u;
    GuestFrame frame(ctx, 0x10u);
    if (call_virtual_at(rt, ctx, self, 0x24u, {self}) == 0u) return;
    ctx.gpr[2] = byte_u(rt, self + 0xA8u);
    put8(rt, self + 0xA9u, 0u);
    if (ctx.gpr[2] != 0u) return tail_call(ctx, 0x08894E10u, {part});
    call_set(rt, ctx, 0x08894EC4u, {part});
    tail_call(ctx, 0x08894E8Cu, {part, word(rt, self + 0xACu)});
}

// func_08A600E4(self, target, a2, holder): o = holder->+0 (func_08A5BB7C(0x08CB00F8) first when it is 0);
// func_08A97A6C(temp, self->+0x14 == o's virtual slot 0x4C) builds a value object that goes to target.
void func_08A600E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[7]);
    const std::uint32_t target = ctx.gpr[5];
    const std::uint32_t expected = word(rt, ctx.gpr[4] + 0x14u);
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t temporary = frame.at(0u);
    ctx.gpr[4] = 0x08CB00F8u;
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB00F8u});
    const std::uint32_t value = call_virtual_at(rt, ctx, object, 0x4Cu, {object});
    call_set(rt, ctx, 0x08A97A6Cu, {temporary, expected == value ? 1u : 0u});
    keep_temporary(rt, ctx, target, temporary);
}

// func_08894D88(self) -> a signed halfword from three reads of the bit reader at self + 0x28 (func_08B1C958(reader, bits)):
// k = 3 bits, sign = 1 bit, then table[k] + (2k + 1 bits), with table the halfwords at 0x08CDD07C; negated when sign.
void func_08894D88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t kTable = 0x08CDD07Cu;
    const std::uint32_t reader = ctx.gpr[4] + 0x28u;
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t twice = call_set(rt, ctx, 0x08B1C958u, {reader, 3u}) << 1;
    const std::uint32_t negative = call_set(rt, ctx, 0x08B1C958u, {reader, 1u});
    const std::uint32_t low = call_set(rt, ctx, 0x08B1C958u, {reader, twice + 1u});
    std::uint32_t value = sext16(low + half_u(rt, kTable + twice));
    if (negative != 0u) value = sext16(0u - value);
    ctx.gpr[2] = value;
}

// func_08ADD674(self): with the counters at 0x08D40628: +0x40 goes down when self->+0x78 is set; when self->+0x70 is set,
// +0x48 goes down and func_08ADD504(self) runs; then, when neither +0x40 nor +0x44 is positive, func_08ADEB40(0).
void func_08ADD674(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t kCounters = 0x08D40628u;
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    if (word(rt, self + 0x78u) != 0u) put(rt, kCounters + 0x40u, word(rt, kCounters + 0x40u) - 1u);
    if (word(rt, self + 0x70u) != 0u) {
        put(rt, kCounters + 0x48u, word(rt, kCounters + 0x48u) - 1u);
        call_set(rt, ctx, 0x08ADD504u, {self});
    }
    ctx.gpr[2] = word(rt, kCounters + 0x40u);
    if (signed_of(ctx.gpr[2]) > 0) return;
    ctx.gpr[2] = word(rt, kCounters + 0x44u);
    ctx.gpr[4] = 0u;
    if (signed_of(ctx.gpr[2]) > 0) return;
    call_set(rt, ctx, 0x08ADEB40u, {0u});
}

// func_08B02FD4(list, link, other, item) -> link (a leaf): nothing when link is 0. When item->+0xE2 has bit 0x4000, a tail
// jump to func_08B02FA4(list, other, item). Otherwise item gets that bit, item->+0xEA = other->+0xEA and item->+0xEE =
// link->+0 (halfwords); link->+0 = item's index in list->+0 (0xF0-byte items: / 16, then exactly / 15 as the low word of
// the multiply by the inverse 0xEEEEEEEF), and the low 14 bits of link->+2 go up by one.
void func_08B02FD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t list = ctx.gpr[4];
    const std::uint32_t link = ctx.gpr[5];
    const std::uint32_t other = ctx.gpr[6];
    const std::uint32_t item = ctx.gpr[7];
    ctx.gpr[2] = link;
    if (link == 0u) return;
    const std::uint32_t flags = half_u(rt, item + 0xE2u);
    if ((flags & 0x4000u) != 0u) return tail_call(ctx, 0x08B02FA4u, {list, other, item});
    put16(rt, item + 0xE2u, flags | 0x4000u);
    put16(rt, item + 0xEAu, half_u(rt, other + 0xEAu));
    put16(rt, item + 0xEEu, half_u(rt, link));
    const std::uint32_t units = static_cast<std::uint32_t>(signed_of(item - word(rt, list)) >> 4);
    const std::uint32_t count = half_u(rt, link + 2u);
    const std::uint32_t bumped = static_cast<std::uint32_t>(signed_of(count << 18) >> 18) + 1u;
    put16(rt, link, units * 0xEEEEEEEFu);
    put16(rt, link + 2u, (count & ~0x3FFFu) | (bumped & 0x3FFFu));
}

// func_088FE528(self, index): when self->+0->+0x28 (a table, count byte-swapped at +0x18) has index < count:
// r = func_08BCF6F4(func_089076F0(table, index), func_08907694(table, 1)) is stored at +0x64 of self->+0->+0x20->+0x48.
void func_088FE528(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t index = ctx.gpr[5];
    const std::uint32_t base = word(rt, self);
    const std::uint32_t table = word(rt, base + 0x28u);
    ctx.gpr[2] = 0u;
    if (table == 0u || !(signed_of(index) < signed_of(swapped(word(rt, table + 0x18u))))) return;
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t owner = word(rt, base + 0x20u);
    const std::uint32_t target = owner != 0u ? word(rt, owner + 0x48u) : 0u;
    const std::uint32_t first = call_set(rt, ctx, 0x089076F0u, {table, index});
    const std::uint32_t second = call_set(rt, ctx, 0x08907694u, {word(rt, word(rt, self) + 0x28u), 1u});
    put(rt, target + 0x64u, call_set(rt, ctx, 0x08BCF6F4u, {first, second}));
}

// func_088D3D10(self, holder) -> self: func_088D2F9C(self, holder); when e = func_088DA4B0(holder->+0) is set:
// self->+0x18 = func_088FDD68(e) (a float), func_088D390C(holder->+0->+0x61C, self, self + 0xC, 6; f12 = that, f13 = 0),
// self->+0x1C = func_088FD9D0(e) and self->+0x20 (byte) = 1.
void func_088D3D10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t holder = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    call_set(rt, ctx, 0x088D2F9Cu, {});
    const std::uint32_t entry = call_set(rt, ctx, 0x088DA4B0u, {word(rt, holder)});
    if (entry != 0u) {
        call_set(rt, ctx, 0x088FDD68u, {entry});
        const float value = ctx.fpr[0];
        put_float(rt, self + 0x18u, value);
        call_setf(rt, ctx, 0x088D390Cu, {word(rt, word(rt, holder) + 0x61Cu), self, self + 0xCu, 6u}, {value, 0.0f});
        const std::uint32_t result = call_set(rt, ctx, 0x088FD9D0u, {entry});
        put8(rt, self + 0x20u, 1u);
        put(rt, self + 0x1Cu, result);
    }
    ctx.gpr[2] = self;
}

// func_088D6114(left, right) -> 1 when the records left->+0 and right->+0 have the same words at +0x28 and +0x2C and the
// same byte at +0x4C, otherwise 0 (a leaf; the right pair is copied through its frame first).
void func_088D6114(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t left = word(rt, ctx.gpr[4]);
    const std::uint32_t right = word(rt, ctx.gpr[5]);
    GuestFrame frame(ctx, 0x10u);
    put(rt, frame.at(0u), word(rt, right + 0x28u));
    put(rt, frame.at(4u), word(rt, right + 0x2Cu));
    const bool same = word(rt, left + 0x2Cu) == word(rt, frame.at(4u)) && word(rt, left + 0x28u) == word(rt, frame.at(0u)) &&
                      byte_u(rt, left + 0x4Cu) == byte_u(rt, right + 0x4Cu);
    ctx.gpr[2] = same ? 1u : 0u;
}

// func_08A57E50(target): like func_08A6FBD0 with func_08A57854 (assert string 0x08CAF7EC) and the word of the object's
// virtual slot 0x7C through func_08A85B1C.
void func_08A57E50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t target = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t source = frame.at(0u);
    const std::uint32_t temporary = frame.at(4u);
    call_set(rt, ctx, 0x08A57854u, {source});
    const std::uint32_t object = word(rt, source);
    ctx.gpr[4] = 0x08CAF7ECu;
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAF7ECu});
    const std::uint32_t value = call_virtual_at(rt, ctx, object, 0x7Cu, {object});
    call_set(rt, ctx, 0x08A85B1Cu, {temporary, value});
    keep_temporary(rt, ctx, target, temporary);
    call_set(rt, ctx, kDestroy, {source});
}

// func_08A885A8(self, target, a2, holder): o = holder->+0 (func_08A5BB7C(0x08CB195C) first when it is 0);
// func_08A97A6C(temp, o's virtual slot 0x54 <= self->+0x14, as floats) builds a value object that goes to target.
void func_08A885A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[7]);
    const std::uint32_t target = ctx.gpr[5];
    const float limit = float_at(rt, ctx.gpr[4] + 0x14u);
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t temporary = frame.at(0u);
    ctx.gpr[4] = 0x08CB195Cu;
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB195Cu});
    call_virtual_at(rt, ctx, object, 0x54u, {object});
    call_set(rt, ctx, 0x08A97A6Cu, {temporary, ctx.fpr[0] <= limit ? 1u : 0u});
    keep_temporary(rt, ctx, target, temporary);
}

// func_08C46854(out, list, it) -> out (a leaf): steps the iterator past it->+0xC: the list's count (+4) goes down; the
// next index n is the signed halfword at it->+8->+6. With a node c = it->+0xC, c->+6 = n, and when n is -1 the list's
// +2 becomes c's index (from list->+8->+0, 8-byte nodes); without one the list's +0 = n, and its +2 = n when n is -1.
// out = {+4: it->+4, +8: the node at index n (0 when n < 0), +0xC: c}.
void func_08C46854(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t list = ctx.gpr[5];
    const std::uint32_t it = ctx.gpr[6];
    const std::uint32_t next = sext16(half_u(rt, word(rt, it + 8u) + 6u));
    const std::uint32_t current = word(rt, it + 0xCu);
    put(rt, list + 4u, word(rt, list + 4u) - 1u);
    if (current != 0u) {
        put16(rt, current + 6u, next);
        if (next == 0xFFFFFFFFu) put16(rt, list + 2u, static_cast<std::uint32_t>(signed_of(current - word(rt, word(rt, list + 8u))) >> 3));
    } else {
        put16(rt, list, next);
        if (next == 0xFFFFFFFFu) put16(rt, list + 2u, next);
    }
    const std::uint32_t node = signed_of(next) < 0 ? 0u : word(rt, word(rt, list + 8u)) + (next << 3);
    const std::uint32_t link = word(rt, it + 4u);
    put(rt, out + 0xCu, current);
    put(rt, out + 4u, link);
    put(rt, out + 8u, node);
    ctx.gpr[2] = out;
}

// func_08833B90(target): func_088297A8 fills a value object at frame +4 (func_08A5BB7C(0x08C9A2F8) when its word is 0);
// r = func_088294D0(word); that object is destroyed; func_08A97A6C(temp, func_08862BF8(r)) builds a value object that
// goes to target.
void func_08833B90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t target = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t temporary = frame.at(0u);
    const std::uint32_t source = frame.at(4u);
    call_set(rt, ctx, 0x088297A8u, {source});
    const std::uint32_t value = word(rt, source);
    ctx.gpr[4] = 0x08C9A2F8u;
    if (value == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9A2F8u});
    const std::uint32_t result = call_set(rt, ctx, 0x088294D0u, {value});
    call_set(rt, ctx, kDestroy, {source});
    const std::uint32_t flag = call_set(rt, ctx, 0x08862BF8u, {result});
    call_set(rt, ctx, 0x08A97A6Cu, {temporary, flag});
    keep_temporary(rt, ctx, target, temporary);
}

// func_08905500(self, a1, a2, a3) -> 0 when func_088FB728(self + 0x20, &a, &b, a2, a3) fails, otherwise
// func_089041C8(self, a1, self + 0x20, &a, &b, 0, self->+0x4C (halfword) - 1, 0); a and b sit at frame +0 and +0xC.
void func_08905500(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t second = ctx.gpr[5];
    const std::uint32_t third = ctx.gpr[6];
    const std::uint32_t fourth = ctx.gpr[7];
    const std::uint32_t part = self + 0x20u;
    GuestFrame frame(ctx, 0x40u);
    const std::uint32_t first_out = frame.at(0u);
    const std::uint32_t second_out = frame.at(0xCu);
    if (call_set(rt, ctx, 0x088FB728u, {part, first_out, second_out, third, fourth}) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    call_set(rt, ctx, 0x089041C8u, {self, second, part, first_out, second_out, 0u, half_u(rt, self + 0x4Cu) - 1u, 0u});
}

// func_089EE408(self): func_089E1ABC(self); then func_089E4C14(temp, 0x08CF6ED8) twice, the first value object going to
// self + 0x240 and the second to self + 0x244.
void func_089EE408(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t kSource = 0x08CF6ED8u;
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    call_set(rt, ctx, 0x089E1ABCu, {});
    call_set(rt, ctx, 0x089E4C14u, {frame.at(4u), kSource});
    keep_temporary(rt, ctx, self + 0x240u, frame.at(4u));
    call_set(rt, ctx, 0x089E4C14u, {frame.at(0u), kSource});
    keep_temporary(rt, ctx, self + 0x244u, frame.at(0u));
}

// func_08A3ACDC(a0, a1, mode, holder): nothing unless mode is 1; then flag = the low byte of holder->+0's virtual slot 0x30
// (func_08A5BB7C(0x08CAD518) first when it is 0); func_08A3A9D0(temp, a1) fills a value object whose word w gets
// func_08A3A4A0(w, flag) (func_08A5BB7C(0x08CAD544) first when it is 0); the temporary is destroyed.
void func_08A3ACDC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t second = ctx.gpr[5];
    ctx.gpr[2] = 1u;
    if (ctx.gpr[6] != 1u) return;
    const std::uint32_t object = word(rt, ctx.gpr[7]);
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t temporary = frame.at(0u);
    ctx.gpr[4] = 0x08CAD518u;
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAD518u});
    const std::uint32_t flag = call_virtual_at(rt, ctx, object, 0x30u, {object}) & 0xFFu;
    call_set(rt, ctx, 0x08A3A9D0u, {temporary, second});
    const std::uint32_t value = word(rt, temporary);
    ctx.gpr[4] = 0x08CAD544u;
    if (value == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAD544u});
    call_set(rt, ctx, 0x08A3A4A0u, {value, flag});
    call_set(rt, ctx, kDestroy, {temporary});
}

// func_08842018(target): func_088418B4 fills a value object at frame +4 (func_08A5BB7C(0x08C9C584) when its word is 0);
// r = func_0884177C(word); that object is destroyed; e = func_0888EB9C(r); func_08A85B1C(temp, swapped(e->+0xE0)) builds
// a value object that goes to target.
void func_08842018(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t target = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t temporary = frame.at(0u);
    const std::uint32_t source = frame.at(4u);
    call_set(rt, ctx, 0x088418B4u, {source});
    const std::uint32_t value = word(rt, source);
    ctx.gpr[4] = 0x08C9C584u;
    if (value == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9C584u});
    const std::uint32_t result = call_set(rt, ctx, 0x0884177Cu, {value});
    call_set(rt, ctx, kDestroy, {source});
    const std::uint32_t entry = call_set(rt, ctx, 0x0888EB9Cu, {result});
    call_set(rt, ctx, 0x08A85B1Cu, {temporary, swapped(word(rt, entry + 0xE0u))});
    keep_temporary(rt, ctx, target, temporary);
}

// func_088DB118(self, key) -> the index of key among the first func_088D9F80(self) signed halfwords at self->+0x140, or -1
// (also when key is negative or not below that count).
void func_088DB118(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t key = ctx.gpr[5];
    ctx.gpr[2] = 0xFFFFFFFFu;
    if (signed_of(key) < 0) return;
    GuestFrame frame(ctx, 0x10u);
    const std::int32_t count = signed_of(call_set(rt, ctx, 0x088D9F80u, {}));
    ctx.gpr[2] = 0xFFFFFFFFu;
    if (!(signed_of(key) < count)) return;
    const std::uint32_t keys = word(rt, self + 0x140u);
    for (std::int32_t index = 0; index < count; ++index) {
        if (sext16(half_u(rt, keys + static_cast<std::uint32_t>(index) * 2u)) == key) {
            ctx.gpr[2] = static_cast<std::uint32_t>(index);
            return;
        }
    }
}

// func_08A833DC(self, target, a2, holder): o = holder->+0 (func_08A5BB7C(0x08CB168C) first when it is 0); the 64-bit
// self->+0x18 plus o's virtual slot 0x40 (v0 / v1) goes to func_08A83160(temp, a1, low, high), a value object that goes
// to target.
void func_08A833DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t target = ctx.gpr[5];
    const std::uint32_t object = word(rt, ctx.gpr[7]);
    const std::uint32_t low = word(rt, ctx.gpr[4] + 0x18u);
    const std::uint32_t high = word(rt, ctx.gpr[4] + 0x1Cu);
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t temporary = frame.at(0u);
    ctx.gpr[4] = 0x08CB168Cu;
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB168Cu});
    call_virtual_at(rt, ctx, object, 0x40u, {object});
    const std::uint32_t sum_low = low + ctx.gpr[2];
    const std::uint32_t sum_high = high + ctx.gpr[3] + (sum_low < low ? 1u : 0u);
    call_set(rt, ctx, 0x08A83160u, {temporary, ctx.gpr[5], sum_low, sum_high});
    keep_temporary(rt, ctx, target, temporary);
}

// func_08B0E284(self): under the lock at self + 4 (func_08B34888 / func_08B34B24): self->+0xA4 becomes 3 (func_08B34CA0
// on the lock when it was not); func_08B0E5B8(self->+0x58, self) when that is set. Then a tail jump to
// func_08B0DEB0(self) when the byte self->+0x68 (read under the lock) is set.
void func_08B0E284(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t lock = self + 4u;
    GuestFrame frame(ctx, 0x10u);
    call_set(rt, ctx, 0x08B34888u, {lock});
    if (word(rt, self + 0xA4u) != 3u) {
        put(rt, self + 0xA4u, 3u);
        call_set(rt, ctx, 0x08B34CA0u, {lock});
    }
    const std::uint32_t other = word(rt, self + 0x58u);
    if (other != 0u) call_set(rt, ctx, 0x08B0E5B8u, {other, self});
    const std::uint32_t flag = byte_u(rt, self + 0x68u);
    call_set(rt, ctx, 0x08B34B24u, {lock});
    if (flag != 0u) tail_call(ctx, 0x08B0DEB0u, {self});
}

// func_08BBBBF0(self, index, out) -> fills out from the index-th 4-byte slot of self->+4 (a signed halfword id, a 15-bit
// signed value): e = func_08B03BB4(self->+0x1C, id); out = {+0: the value, +2: 0, +4: e, +8: e + 0x20, +0xC: e + 0xB0
// when e->+0xE2 has bit 1, else 0}; the result is 1 unless e->+0xE2 has bit 4 and the slot's bit 0x8000 is clear.
void func_08BBBBF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t out = ctx.gpr[6];
    const std::uint32_t slot = word(rt, self + 4u) + (ctx.gpr[5] << 2);
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t entry = call_set(rt, ctx, 0x08B03BB4u, {word(rt, self + 0x1Cu), sext16(half_u(rt, slot))});
    const std::uint32_t value = static_cast<std::uint32_t>(static_cast<std::int32_t>(half_u(rt, slot + 2u) << 17) >> 17);
    put(rt, out + 0xCu, (half_u(rt, entry + 0xE2u) & 1u) != 0u ? entry + 0xB0u : 0u);
    put16(rt, out, value);
    put(rt, out + 8u, entry + 0x20u);
    put16(rt, out + 2u, 0u);
    put(rt, out + 4u, entry);
    const bool flagged = (half_u(rt, entry + 0xE2u) & 4u) != 0u;
    ctx.gpr[2] = flagged && (half_u(rt, slot + 2u) & 0x8000u) == 0u ? 0u : 1u;
}

// func_08A0A398(self, object): object's virtual slot 8 runs with (object, &record, self + 0x11C, vtable), the frame record
// being {0x08CCD9F0, the empty shared string 0x08D74FD0 + 0xC, -1, self + 0x11C}; then the record's string is released:
// unless it is the empty one, its count (at -4) goes down by func_08B60804(count, -1) and func_08B8542C(rep, &record)
// frees it when that answer is not positive.
void func_08A0A398(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t kEmpty = 0x08D74FD0u;
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t object = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t record = frame.at(4u);
    put(rt, frame.at(8u), kEmpty + 0xCu);
    put(rt, frame.at(0xCu), 0xFFFFFFFFu);
    put(rt, frame.at(4u), 0x08CCD9F0u);
    put(rt, frame.at(0x10u), self + 0x11Cu);
    const std::uint32_t table = word(rt, object);
    ctx.gpr[2] = word(rt, table + 8u);
    ctx.gpr[3] = 0xFFFFFFFFu;
    call_set(rt, ctx, ctx.gpr[2], {object, record, self + 0x11Cu, table});
    const std::uint32_t text = word(rt, frame.at(8u));
    put(rt, frame.at(4u), 0x08CCD990u);
    const std::uint32_t rep = text - 0xCu;
    ctx.gpr[4] = text - 4u;
    ctx.gpr[5] = 0xFFFFFFFFu;
    if (rep == kEmpty) return;
    if (signed_of(call_set(rt, ctx, 0x08B60804u, {text - 4u, 0xFFFFFFFFu})) > 0) return;
    call_set(rt, ctx, 0x08B8542Cu, {rep, frame.at(0u)});
}

// func_08B1D860(key, fallback) -> the integer value of the entry func_08B1DEB4(0x08D68A94, key) finds, or fallback when
// there is none or its type (+0) is 6 or more: types 0-3 hold it at +8, type 4 a float at +8 (trunc.w.s), type 5 a 64-bit
// value at +8 / +0xC converted by func_08B71DC8.
void func_08B1D860(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t key = ctx.gpr[4];
    const std::uint32_t fallback = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t found = call_set(rt, ctx, 0x08B1DEB4u, {0x08D68A94u, key});
    ctx.gpr[2] = fallback;
    if (found == 0u) return;
    const std::uint32_t entry = word(rt, found);
    if (entry == 0u || word(rt, entry) >= 6u) return;
    switch (word(rt, entry)) {
    case 4u: ctx.gpr[2] = ctx.fpu_float_to_word(float_at(rt, entry + 8u), 1u); break;
    case 5u: ctx.gpr[2] = call_set(rt, ctx, 0x08B71DC8u, {word(rt, entry + 8u), word(rt, entry + 0xCu)}); break;
    default: ctx.gpr[2] = word(rt, entry + 8u); break;
    }
}

} // namespace hot_batch_109
} // namespace gt::game

#define HOT109(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_109::name, gt::ReturnKind::kind)

HOT109(0x088DA57Cu, func_088DA57C, Word);
HOT109(0x088936D4u, func_088936D4, Word);
HOT109(0x08891EF8u, func_08891EF8, Word);
// HOT109(0x08AC0B50u, func_08AC0B50, Word);  // batch 109: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT109(0x0888F374u, func_0888F374, Word);
// HOT109(0x08ADF910u, func_08ADF910, Word);  // batch 109: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
// HOT109(0x08B31960u, func_08B31960, Word);  // batch 109: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
// HOT109(0x08AD96D8u, func_08AD96D8, Word);  // batch 109: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT109(0x08820BD4u, func_08820BD4, Word);
HOT109(0x08843330u, func_08843330, Word);
HOT109(0x088DE700u, func_088DE700, Word);
HOT109(0x08A5FB60u, func_08A5FB60, Word);
HOT109(0x0892C7C8u, func_0892C7C8, Word);
HOT109(0x08996354u, func_08996354, Word);
HOT109(0x08A83248u, func_08A83248, Word);
HOT109(0x08A887ACu, func_08A887AC, Word);
HOT109(0x08AABA8Cu, func_08AABA8C, Word);
// HOT109(0x08AD5524u, func_08AD5524, Word);  // batch 109: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT109(0x08A4C76Cu, func_08A4C76C, Word);
HOT109(0x089AFF3Cu, func_089AFF3C, Word);
// HOT109(0x08A30E48u, func_08A30E48, Word);  // batch 109: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT109(0x08A79A5Cu, func_08A79A5C, Word);
HOT109(0x08AAB604u, func_08AAB604, Word);
// HOT109(0x08862BF8u, func_08862BF8, Word);  // batch 109: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT109(0x0885EC28u, func_0885EC28, Word);
// HOT109(0x08B0E560u, func_08B0E560, Word);  // batch 109: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT109(0x089969BCu, func_089969BC, Word);
// HOT109(0x08B8DA64u, func_08B8DA64, Word);  // batch 109: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT109(0x08829B34u, func_08829B34, Word);
// HOT109(0x088909BCu, func_088909BC, Word);  // batch 109: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT109(0x08AD2848u, func_08AD2848, Word);
HOT109(0x08BEEAA8u, func_08BEEAA8, Word);
// HOT109(0x08BF8B64u, func_08BF8B64, Word);  // batch 109: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT109(0x088988B4u, func_088988B4, Word);
// HOT109(0x088F9C80u, func_088F9C80, Word);  // batch 109: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
// HOT109(0x0888B5CCu, func_0888B5CC, Word);  // batch 109: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT109(0x088BB44Cu, func_088BB44C, Word);
HOT109(0x088DB0ACu, func_088DB0AC, Word);
// HOT109(0x088E0BE4u, func_088E0BE4, Word);  // batch 109: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
// HOT109(0x08AC45F4u, func_08AC45F4, Word);  // batch 109: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
// HOT109(0x08ADC27Cu, func_08ADC27C, Word);  // batch 109: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT109(0x08841A60u, func_08841A60, Word);
HOT109(0x088A7C78u, func_088A7C78, Word);
// HOT109(0x088DD870u, func_088DD870, Word);  // batch 109: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT109(0x0899261Cu, func_0899261C, Word);
// HOT109(0x08B34060u, func_08B34060, Word);  // batch 109: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT109(0x08893B0Cu, func_08893B0C, Word);
// HOT109(0x08A2B5DCu, func_08A2B5DC, Word);  // batch 109: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
// HOT109(0x08AA22BCu, func_08AA22BC, Word);  // batch 109: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
// HOT109(0x08AA2494u, func_08AA2494, Word);  // batch 109: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
// HOT109(0x08B2DEE4u, func_08B2DEE4, DoubleWord);  // batch 109: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT109(0x08829C4Cu, func_08829C4C, Word);
HOT109(0x08894A88u, func_08894A88, Word);
HOT109(0x088966A8u, func_088966A8, Word);
HOT109(0x088FDB9Cu, func_088FDB9C, Float);
HOT109(0x088FDC78u, func_088FDC78, Float);
HOT109(0x088FAB04u, func_088FAB04, Word);
HOT109(0x088FE620u, func_088FE620, Word);
HOT109(0x08994C00u, func_08994C00, Word);
HOT109(0x089B8E5Cu, func_089B8E5C, Word);
HOT109(0x08A6FBD0u, func_08A6FBD0, Word);
HOT109(0x08895FBCu, func_08895FBC, Word);
HOT109(0x08A600E4u, func_08A600E4, Word);
HOT109(0x08894D88u, func_08894D88, Word);
// HOT109(0x08ADD674u, func_08ADD674, Word);  // batch 109: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT109(0x08B02FD4u, func_08B02FD4, Word);
HOT109(0x088FE528u, func_088FE528, Word);
// HOT109(0x088D3D10u, func_088D3D10, Word);  // batch 109: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
// HOT109(0x088D6114u, func_088D6114, Word);  // batch 109: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT109(0x08A57E50u, func_08A57E50, Word);
HOT109(0x08A885A8u, func_08A885A8, Word);
HOT109(0x08C46854u, func_08C46854, Word);
HOT109(0x08833B90u, func_08833B90, Word);
HOT109(0x08905500u, func_08905500, Word);
HOT109(0x089EE408u, func_089EE408, Word);
HOT109(0x08A3ACDCu, func_08A3ACDC, Word);
HOT109(0x08842018u, func_08842018, Word);
HOT109(0x088DB118u, func_088DB118, Word);
HOT109(0x08A833DCu, func_08A833DC, Word);
HOT109(0x08B0E284u, func_08B0E284, Word);
HOT109(0x08BBBBF0u, func_08BBBBF0, Word);
HOT109(0x08A0A398u, func_08A0A398, Word);
// HOT109(0x08B1D860u, func_08B1D860, Word);  // batch 109: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
