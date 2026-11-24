// New batch 9 (main session): 80 functions the recompiler emitted, written by hand as structured C from `tools.disasm`.
// None is named: no evidence yet (no string, import or binding names them).
//
// Most of them are one of the library's "format a value into a std::string" wrappers: a producer fills an object in the
// frame, a getter reads one value out of it, a value object built from that value is assigned to the caller's string and
// both temporaries are destroyed. The rest are destructors/constructors of the same library (vtable stores plus virtual
// releases), two record copies, a binary-tree insert and a couple of table walkers. Nothing here is copied from the
// recompiler's output: each body was written from its disassembly, with the call order, the delay slots and the v0 of
// every exit path kept as the original has them.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_9 {
namespace {

using namespace gt::game::hot;

inline void take(psprecomp::AllegrexContext &ctx, const GuestResult &result) {
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}
// A guest call whose results stay in the registers like the original's: v0 / v1 / f0 of the callee are what the next
// instruction sees.
inline GuestResult call_keep(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address,
                             std::initializer_list<std::uint32_t> arguments, std::initializer_list<float> float_arguments) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, float_arguments);
    take(ctx, result);
    return result;
}
// A guest call with the argument registers set like the original sets them before the jal (a0-a3, t0-t3 in order): what
// the callee does not take stays as the caller had it.
inline std::uint32_t call_set(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address,
                              std::initializer_list<std::uint32_t> arguments) {
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments, {}).v0;
}
// The same with float arguments in f12-f15 as well.
inline GuestResult call_setf(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address,
                             std::initializer_list<std::uint32_t> arguments, std::initializer_list<float> float_arguments) {
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
// A virtual call `object->slot(...)`: `lw v1, 0(object); lw v0, slot(v1); jalr v0`.
inline std::uint32_t call_virtual_at(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object,
                                     std::uint32_t slot, std::initializer_list<std::uint32_t> arguments) {
    ctx.gpr[3] = word(rt, object);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + slot);
    return call_set(rt, ctx, ctx.gpr[2], arguments);
}
// The other spelling of the same: `lw v0, 0(object); lw v1, slot(v0); jalr v1` — the vtable ends up in v0.
inline std::uint32_t call_vtable_in_v0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object,
                                       std::uint32_t slot, std::initializer_list<std::uint32_t> arguments) {
    ctx.gpr[2] = word(rt, object);
    ctx.gpr[3] = word(rt, ctx.gpr[2] + slot);
    return call_set(rt, ctx, ctx.gpr[3], arguments);
}
inline std::uint32_t byte_u(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load8(address); }
inline void put8(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) {
    rt.memory().store8(address, static_cast<std::uint8_t>(value));
}
inline std::uint32_t sext8(std::uint32_t value) {
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(value)));
}
inline std::uint32_t sext16(std::uint32_t value) {
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(value)));
}
inline std::int32_t signed_of(std::uint32_t value) { return static_cast<std::int32_t>(value); }

// The value-object pair of the 0x08A5xxxx library: func_08A5BAD8(dst, src->+0) assigns, func_08A5B9EC(object) destroys.
constexpr std::uint32_t kAssign = 0x08A5BAD8u;
constexpr std::uint32_t kDestroy = 0x08A5B9ECu;
// Its null-object assert: func_08A5BB7C(message).
constexpr std::uint32_t kAssert = 0x08A5BB7Cu;
// The lock pair func_08B344E0(lock) / func_08B3457C(lock).
constexpr std::uint32_t kLock = 0x08B344E0u;
constexpr std::uint32_t kUnlock = 0x08B3457Cu;

// The end of a function that built a value object in its frame at `temporary`: assigned to `target` (skipped when the
// target is that temporary itself), then the temporary is destroyed.
inline void keep_temporary(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t target,
                           std::uint32_t temporary) {
    if (target != temporary) call_set(rt, ctx, kAssign, {target, word(rt, temporary)});
    call_set(rt, ctx, kDestroy, {temporary});
}
// The same, then the object the value was read from is destroyed too — the tail of most wrappers below.
inline void keep_string(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t target,
                        std::uint32_t temporary, std::uint32_t object_address) {
    keep_temporary(rt, ctx, target, temporary);
    call_set(rt, ctx, kDestroy, {object_address});
}

// `producer(object)` fills a value object at `object_address`; a null one trips the library assert at `message`.
inline std::uint32_t produce(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t producer,
                             std::uint32_t object_address, std::uint32_t message) {
    call_set(rt, ctx, producer, {object_address});
    const std::uint32_t object = word(rt, object_address);
    if (object == 0u) call_set(rt, ctx, kAssert, {message});
    return object;
}

// Wrapper shape 1: the value is the word at `field` of the produced object; it is copied into the frame at `value_offset`
// and func_08A76AAC builds the string at `string_offset` from its address.
void string_of_field(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t producer,
                     std::uint32_t field, std::uint32_t message, const GuestFrame &frame, std::uint32_t object_offset,
                     std::uint32_t value_offset, std::uint32_t string_offset) {
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t object = produce(rt, ctx, producer, frame.at(object_offset), message);
    put(rt, frame.at(value_offset), word(rt, object + field));
    call_set(rt, ctx, 0x08A76AACu, {frame.at(string_offset), frame.at(value_offset)});
    keep_string(rt, ctx, out, frame.at(string_offset), frame.at(object_offset));
}

// Wrapper shape 1b: the value is `getter(object)`.
void string_of_int_getter(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t producer,
                          std::uint32_t getter, std::uint32_t message, const GuestFrame &frame,
                          std::uint32_t object_offset, std::uint32_t value_offset, std::uint32_t string_offset) {
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t object = produce(rt, ctx, producer, frame.at(object_offset), message);
    put(rt, frame.at(value_offset), call_set(rt, ctx, getter, {object}));
    call_set(rt, ctx, 0x08A76AACu, {frame.at(string_offset), frame.at(value_offset)});
    keep_string(rt, ctx, out, frame.at(string_offset), frame.at(object_offset));
}

// Wrapper shape 2: the value is `getter(object)`, handed to func_08A85B1C(string, value).
void string_of_getter(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t producer,
                      std::uint32_t getter, std::uint32_t message, const GuestFrame &frame, std::uint32_t object_offset,
                      std::uint32_t string_offset) {
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t object = produce(rt, ctx, producer, frame.at(object_offset), message);
    const std::uint32_t value = call_set(rt, ctx, getter, {object});
    call_set(rt, ctx, 0x08A85B1Cu, {frame.at(string_offset), value});
    keep_string(rt, ctx, out, frame.at(string_offset), frame.at(object_offset));
}

// Wrapper shape 3: `getter(object)` returns a float in f0; it goes to func_08A87EEC(string, f12).
void string_of_float_getter(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t producer,
                            std::uint32_t getter, std::uint32_t message, const GuestFrame &frame,
                            std::uint32_t object_offset, std::uint32_t string_offset) {
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t object = produce(rt, ctx, producer, frame.at(object_offset), message);
    const GuestResult value = call_keep(rt, ctx, getter, {object}, {});
    call_setf(rt, ctx, 0x08A87EECu, {frame.at(string_offset)}, {value.f0});
    keep_string(rt, ctx, out, frame.at(string_offset), frame.at(object_offset));
}

// Wrapper shape 4: the value is `getter(object)`, handed to func_08A6D2D4(string, value).
void string_of_object_getter(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t producer,
                             std::uint32_t getter, std::uint32_t message, const GuestFrame &frame,
                             std::uint32_t object_offset, std::uint32_t string_offset) {
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t object = produce(rt, ctx, producer, frame.at(object_offset), message);
    const std::uint32_t value = call_set(rt, ctx, getter, {object});
    call_set(rt, ctx, 0x08A6D2D4u, {frame.at(string_offset), value});
    keep_string(rt, ctx, out, frame.at(string_offset), frame.at(object_offset));
}

// Wrapper shape 5: `getter(object)` answers a 64-bit value (v0/v1) that func_08A83160(string, a1, low, high) formats.
void string_of_pair(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t producer,
                    std::uint32_t getter, std::uint32_t message, const GuestFrame &frame, std::uint32_t object_offset,
                    std::uint32_t string_offset) {
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t object = produce(rt, ctx, producer, frame.at(object_offset), message);
    const GuestResult value = call_keep(rt, ctx, getter, {object}, {});
    call_set(rt, ctx, 0x08A83160u, {frame.at(string_offset), ctx.gpr[5], value.v0, value.v1});
    keep_string(rt, ctx, out, frame.at(string_offset), frame.at(object_offset));
}

// Wrapper shape 6: the object is destroyed first, then `field` of `getter(object)` (the getter's answer is kept in a
// register across the destroy) goes through `formatter` into the string.
void string_of_destroyed_field(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t producer,
                               std::uint32_t getter, std::uint32_t field, std::uint32_t formatter, std::uint32_t message,
                               const GuestFrame &frame, std::uint32_t object_offset, std::uint32_t string_offset) {
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t object = produce(rt, ctx, producer, frame.at(object_offset), message);
    const std::uint32_t record = call_set(rt, ctx, getter, {object});
    call_set(rt, ctx, kDestroy, {frame.at(object_offset)});
    call_set(rt, ctx, formatter, {frame.at(string_offset), word(rt, record + field)});
    keep_temporary(rt, ctx, out, frame.at(string_offset));
}

// Wrapper shape 6b: the object is destroyed first, then the word at +0x14 of it is handed to `reader` and the answer goes
// through `formatter` into the string.
void string_of_destroyed_member(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t producer,
                                std::uint32_t reader, std::uint32_t formatter, std::uint32_t message,
                                const GuestFrame &frame, std::uint32_t object_offset, std::uint32_t string_offset) {
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t object = produce(rt, ctx, producer, frame.at(object_offset), message);
    const std::uint32_t member = word(rt, object + 0x14u);
    call_set(rt, ctx, kDestroy, {frame.at(object_offset)});
    const std::uint32_t value = call_set(rt, ctx, reader, {member});
    call_set(rt, ctx, formatter, {frame.at(string_offset), value});
    keep_temporary(rt, ctx, out, frame.at(string_offset));
}

} // namespace

// ---- the std::string wrappers --------------------------------------------------------------------------------------

// func_08843E7C(target): func_088435A8 builds an object in the frame, func_08843404(object) an entry whose byte at +0x80
// goes into func_08A97A6C(string, byte); the string is assigned to target, then both temporaries are destroyed.

// func_08843F04(target): the same wrapper reading the byte at +0x81.










































// ---- destructors, constructors and object plumbing ------------------------------------------------------------------

// func_0885C538(self): under the lock at self+8, func_0885E430(self->+4), then func_0885C3C8(self); self->+0->+0x30 and
// +0x34 are cleared and eight 8-byte entries at +0x38 store -1 in both words. v0 is the last entry's address.

// func_0886C31C(self, value) -> the byte at +0x14 of the record found in the map at self->+4 (+0x1DC .. +0x1E0):
// func_08B93AE8 finds it, the entry at `value` is inserted by func_08B940D8 (with the byte at frame+4 cleared) when the
// value sorts before the found one or the map is at its end.

// func_0886EB48(self, first, second) -> 1, or 0 when func_0886F874(self->+0x70 + 0x64, first, second) is 0: the object at
// +0x70 is set up first (func_0886A9A0(self, 5) == 0 runs func_0886AAA4(self, that + 0x64)) and released by func_0886AA0C.
void func_0886EB48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t first = ctx.gpr[5];
    const std::uint32_t second = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    if (call_set(rt, ctx, 0x0886A9A0u, {self, 5u}) == 0u) {
        call_set(rt, ctx, 0x0886AAA4u, {self, word(rt, self + 0x70u) + 0x64u});
    }
    std::uint32_t result = 1u;
    if (call_set(rt, ctx, 0x0886F874u, {word(rt, self + 0x70u) + 0x64u, first, second}) == 0u) {
        call_set(rt, ctx, 0x0886AA0Cu, {self, 5u});
        result = 0u;
    }
    ctx.gpr[3] = result;
    ctx.gpr[2] = result;
}

// func_0887EE68(self, first, second) -> the same as func_0886EB48 with the object at +0x74 and offset 0x14.
void func_0887EE68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t first = ctx.gpr[5];
    const std::uint32_t second = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    if (call_set(rt, ctx, 0x0886A9A0u, {self, 5u}) == 0u) {
        call_set(rt, ctx, 0x0886AAA4u, {self, word(rt, self + 0x74u) + 0x14u});
    }
    std::uint32_t result = 1u;
    if (call_set(rt, ctx, 0x0886F874u, {word(rt, self + 0x74u) + 0x14u, first, second}) == 0u) {
        call_set(rt, ctx, 0x0886AA0Cu, {self, 5u});
        result = 0u;
    }
    ctx.gpr[3] = result;
    ctx.gpr[2] = result;
}

// func_088733F4(self), a destructor: the two vtable stores at +0xC and +0, the release of the object at +0x60 (through its
// first virtual slot, with self->+0x5C as the argument) and the pointers it owned cleared, then +0xC's vtable and
// func_08AF484C(self+0xC), and the base vtable at +0.

// func_0888D82C(self, first, second, flag): sets bit 1 (flag 0) or bit 0 of the byte at +0x198 of the object at self->+4;
// with flag 0 it also queues a 0x18-tagged record (second at +8, first at +0xC) to func_088936A4(self->+0x18, record).

// func_088A2364(self, index): unlinks the free node `index` of the table at self->+0xC (a signed byte link per node); when
// the node it pointed at is a real one, that node is marked -1 and func_088A2274 hands its 124-byte record (table
// self->+4) back. Then `index` itself is marked and its record handed back — a tail jump to func_088A2274.

// func_088A34BC(self), a destructor: both vtable stores at +0 and +4, func_088A322C(self), the release of the object at
// +0x5C (first virtual slot, self->+0x58 as the argument) with both pointers cleared, then the +4 and +0 vtable stores
// and a tail jump to func_08B64654(self).
void func_088A34BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    put(rt, self, 0x08CC2800u);
    put(rt, self + 4u, 0x08CC2898u);
    call_set(rt, ctx, 0x088A322Cu, {self});
    const std::uint32_t member_slot = self + 0x58u;
    const std::uint32_t member = word(rt, member_slot + 4u);
    if (member != 0u) {
        call_virtual_at(rt, ctx, member, 0u, {member, word(rt, member_slot)});
        put(rt, member_slot + 4u, 0u);
        put(rt, member_slot, 0u);
    }
    put(rt, self + 4u, 0x08CC28E0u);
    put(rt, self, 0x08CC28F8u);
    tail_call(ctx, 0x08B64654u, {self});
}

// func_088BBC70(self, count, ...): func_088C50B8(hash, self->+0x1C, byte at self->+0x24 + 0x10) starts a hash over
// self->+0x24->+0x11 words, func_088C5324 feeds it `count` times and func_088C50C4 finishes it. A negative count skips the
// loop and only finishes.

// func_088DA8DC(self, index): func_088E89CC hands back the 760-byte record at self->+0xC0 + 760*index, then the object
// func_088F8D58(self->+0x14C, index) is locked by func_088F8938, its first virtual slot is called with 0, and slot 0x1C is
// the tail call (0 as well).

// func_088E698C(self, value): when self->+0->+0x52F (signed byte) is not `value`: under the lock at self->+0 + 0xEC, the
// virtual slot 0x50 (with -1) answers the record whose byte +8 and self->+0->+0x52F are set to `value`; the unlock is the
// tail call. An unchanged value leaves the signed byte in v0.
void func_088E698C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t value = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t object = word(rt, self);
    const std::uint32_t current = sext8(byte_u(rt, object + 0x52Fu));
    if (current == value) {
        ctx.gpr[2] = current;
        return;
    }
    call_set(rt, ctx, kLock, {object + 0xECu});
    const std::uint32_t holder = word(rt, self);
    const std::uint32_t target = word(rt, word(rt, holder) + 0x50u);
    const std::uint32_t record = call_set(rt, ctx, target, {holder, 0xFFFFFFFFu});
    if (record != 0u) {
        put8(rt, record + 8u, value);
        ctx.gpr[2] = word(rt, self);
        put8(rt, ctx.gpr[2] + 0x52Fu, value);
    }
    tail_call(ctx, kUnlock, {word(rt, self) + 0xECu});
}

// func_088F1F84(self), a constructor: func_08B28DCC(self+4), the +0x2C word cleared, two slots at +0x30 and +0x438
// (vtable 0x08CC3BA8, +0x404 cleared), the marker 0x12345678 at +0x840, func_088F5AFC(self+0x844), -1 at +0x9C8 and the
// bytes at +0x9CC / +0x9C4, the words at +0x9B8 / +0x9BC cleared. v0 ends at -1.
void func_088F1F84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    put(rt, self, 0x08CC3B30u);
    call_set(rt, ctx, 0x08B28DCCu, {self + 4u});
    put(rt, self + 0x2Cu, 0u);
    std::uint32_t slot = self + 0x30u;
    for (std::uint32_t i = 2u; i != 0u;) {
        --i;
        put(rt, slot, 0x08CC3BA8u);
        put(rt, slot + 0x404u, 0u);
        slot += 0x408u;
    }
    put(rt, self + 0x840u, 0x12345678u);
    call_set(rt, ctx, 0x088F5AFCu, {self + 0x844u});
    put(rt, self + 0x9C8u, 0xFFFFFFFFu);
    put8(rt, self + 0x9CCu, 0u);
    put(rt, self + 0x9B8u, 0u);
    put(rt, self + 0x9BCu, 0u);
    put8(rt, self + 0x9C4u, 0u);
    ctx.gpr[2] = 0xFFFFFFFFu;
}

// func_0891FE68(self), a destructor: the 13 slots +0x250 .. +0x280 each release their object through its second virtual
// slot and are cleared; then func_08925FF8(self) and a tail jump to func_08B64654(self).

// func_08930F8C(self, second) -> 1 when func_08938CB4(func_089363FC(), func_08936578(it, ?, pair.v0, pair.v1), second) is
// non-zero: the pair comes from func_08931900(self), and a pair of -1/-1 answers 0 without any of the three calls.

// func_08A073E0(self): func_089A8B68(self) (its f0 saved aside), func_089A8B80(self), then the virtual slot 0x10C of
// self->+0 with a1 = word(0x08C8CC00), a2 = 2, a3 = the frame record {0, 0, first f0, second f0}, t0 = t1 = 0. The dead
// `bne` self-loop of the original (v0 = 1, then -1, then -2) has no effect on any register the code reads.

// func_08A19BA0(self, value, holder): self->+0x28 = value; the object at *holder answers self->+0x2C through its virtual
// slot 0xDC (with 0) and self->+0x30 is its word at +0x1C.

// func_08A351B4(self, first, second): func_08A3A73C(self->+0x18, first, 0), then a tail jump to
// func_08A3A66C(self->+0x18, second); a null +0x18 trips the library assert at 0x08CACEC8.
void func_08A351B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t first = ctx.gpr[5];
    const std::uint32_t second = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t object = word(rt, self + 0x18u);
    if (object == 0u) call_set(rt, ctx, kAssert, {0x08CACEC8u});
    call_set(rt, ctx, 0x08A3A73Cu, {object, first, 0u});
    const std::uint32_t other = word(rt, self + 0x18u);
    if (other == 0u) call_set(rt, ctx, kAssert, {0x08CACEC8u});
    tail_call(ctx, 0x08A3A66Cu, {other, second});
}

// func_08A3F458(target, member, holder) -> target: the virtual slot 0x3C of *holder goes through func_08B21590(member +
// 0x14, it) and func_08A85B1C(string, it); func_08A76ABC(target, string) hands the string over and it is destroyed.

// func_08A406E4(target): func_08A3F684 builds an object at frame+4 whose null trips the assert at 0x08CADA84, and
// func_08A4064C(string, object, frame+8, 0x80) formats it; the string is assigned to target, both temporaries destroyed.

// func_08AAE5F0(self): func_08C2B010(self+0x24) is compared with self+0x28; when they differ, func_08B5F444(that, self+0x28)
// answers a record whose +0x14 string is destroyed and whose +0x10 is released by func_08A69D68 and func_08A5BC54(record,
// 0x18, 4), and the counter at self+0x24->+0x14 drops by one.
void func_08AAE5F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t found = call_set(rt, ctx, 0x08C2B010u, {self + 0x24u, ctx.gpr[5]});
    if (found != self + 0x28u) {
        const std::uint32_t record = call_set(rt, ctx, 0x08B5F444u, {found, self + 0x28u});
        call_set(rt, ctx, kDestroy, {record + 0x14u});
        call_set(rt, ctx, 0x08A69D68u, {record + 0x10u});
        call_set(rt, ctx, 0x08A5BC54u, {record, 0x18u, 4u});
        put(rt, self + 0x24u + 0x14u, word(rt, self + 0x24u + 0x14u) - 1u);
    }
}

// func_08ADD674(self): the counter at 0x08D40628 + 0x40 drops by one when self->+0x78 is set; while self->+0x70 is set the
// counter at +0x48 drops and func_08ADD504(self) runs; then the counters at +0x40 and +0x44 decide whether
// func_08ADEB40(0) runs. v0 is the counter that ended the walk (or the last call's result).

// func_08B10EF0(): a one-shot table walk: when the flag byte at 0x08D47494 is already set, a tail jump to func_08B2E064.
// Otherwise it is set, func_08B17924 runs, and the lists at 0x08D77350 (next at +0, value at +4 through func_08B10E58) and
// at 0x08D47460 (next at +8, value at +0 through func_08B10598) are walked.

// func_08B14E34(target, -, second, third, fourth, flag): func_08B18534 builds a 0x3C-byte record in the frame from
// (second, third, fourth, flag & 0xFF), which is copied to target: 0x30 bytes as three 16-byte blocks, then three words.
// v0 = target.

// func_08B26D0C(base, first, index, link) -> index or `first`: the tree insert of node `index` of the 16-byte-node array at
// `base` (key at +0 compared as unsigned, the signed 16-bit link at +6). `first` < 0 is the empty tree; the walk follows
// the links until one is negative or the key sorts before a node's, then the parent's link (or the negative entry's own)
// is rewritten with `index`, and node[index]'s link takes the entry that replaced it. v0 is `index` unless the walk
// stopped on a non-negative parent, where it is the original `first`.

// func_08B63A70(self, key, value, bias) -> 1 when a record was matched, 0 when the walk ran out: func_08B63614 decodes
// the variable-length handle at self->+0xC - bias - 1 into frame+0, func_08B63924(class, handle) makes the object and
// func_08B639D8(object, key, &value) tests it; the walk restarts from the decoder's answer until it fails.

// func_08B99E48(self, count, character): std::string-style append of `count` copies of `character`. `self->+0` is the data
// pointer and the word at -0xC its capacity: over 0x3FFFFFFC func_08B5ADDC raises the length error at 0x08C9FFE4; a full
// buffer hands (count - capacity, signed character) to func_08B99D6C, a non-full one (capacity - count, 0) to
// func_08B943A0, both as tail jumps; an exact fit returns 0.
void func_08B99E48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t count = ctx.gpr[5];
    const std::uint32_t character = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t capacity = word(rt, word(rt, ctx.gpr[4]) - 0xCu);
    if (count > 0x3FFFFFFCu) call_set(rt, ctx, 0x08B5ADDCu, {0x08C9FFE4u});
    if (capacity < count) {
        tail_call(ctx, 0x08B99D6Cu, {ctx.gpr[4], count - capacity, sext8(character)});
        return;
    }
    if (capacity > count) {
        tail_call(ctx, 0x08B943A0u, {ctx.gpr[4], count, capacity - count, 0u});
        return;
    }
    ctx.gpr[2] = 0u;
}

// func_08BBEC68(begin, end, out) -> out advanced past the last copy: each 16-byte record of [begin, end) is placed by
// func_08B9432C(out, record) and its words +4, +8, +0xC copied; `out` = 0 skips the copy but still counts the record.

// func_08BBF048(begin, end, out): func_08BBEC68 again, instruction for instruction (two emitted copies of one template).

// func_08BC3C40(unused, holder, value): func_08B62344(8) allocates the node, vtable 0x08CC4F08 at +0, its +4 is placed
// from `value` by func_08B9432C, the node is pushed at *holder and the previous head is released through its second
// virtual slot (a tail jump). v0 = the previous head.
void func_08BC3C40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t holder = ctx.gpr[5];
    const std::uint32_t value = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t node = call_set(rt, ctx, 0x08B62344u, {8u});
    put(rt, node, 0x08CC4F08u);
    call_set(rt, ctx, 0x08B9432Cu, {node + 4u, value});
    const std::uint32_t previous = word(rt, holder);
    put(rt, holder, node);
    ctx.gpr[2] = previous;
    if (previous != 0u) tail_call(ctx, word(rt, word(rt, previous) + 4u), {previous, ctx.gpr[5]});
}

// func_08C676F0(self, source, field, flags): the word at field+0xC keeps its bits except 0x5A/0x405A, which become
// 0x208, while func_08C67404(self, {source->+0, byte source->+4}, field, flags) runs; the old word is restored. v0 is its
// result, v1 the word at frame+4 whose low byte was set from source->+0 (the other three keep the frame's contents).
void func_08C676F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t source = ctx.gpr[5];
    const std::uint32_t field = ctx.gpr[6];
    const std::uint32_t flags = sext8(ctx.gpr[7]);
    GuestFrame frame(ctx, 0x50u);
    const std::uint32_t old = word(rt, field + 0xCu);
    const std::uint32_t first = word(rt, source);
    put(rt, field + 0xCu, (old & 0xFFFFBFA5u) | 0x208u);
    put(rt, frame.at(0x10u), first);
    put8(rt, frame.at(0x14u), byte_u(rt, source + 4u));
    const std::uint32_t result = call_set(rt, ctx, 0x08C67404u, {self, frame.at(0x10u), field, flags, ctx.gpr[8]});
    put(rt, frame.at(0x24u), first);
    put8(rt, frame.at(4u), byte_u(rt, frame.at(0x24u)));
    put(rt, field + 0xCu, old);
    ctx.gpr[2] = result;
    ctx.gpr[3] = word(rt, frame.at(4u));
}

// func_0880C020(self): under the lock at self, func_08A76AAC builds a value object from 0x08CF6ED8 which is assigned to
// the string at self+0x208 and destroyed; self->+0x200 is cleared, func_0886924C(self+0x30, {0, 0}) and
// func_088691D8(self+0x30) run, and the unlock ends the function.

// func_0888B2D4(self, first, second, third): func_08B62344(0x30) allocates a record, func_08897B8C builds it,
// func_088972BC(record, third) fills it, and func_088936A4(self->+0x18, {0x1E, first, second, record}) queues it; then
// func_088978B4(record) and func_08B64654(record) release it.
void func_0888B2D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t first = ctx.gpr[5];
    const std::uint32_t second = ctx.gpr[6];
    const std::uint32_t third = ctx.gpr[7];
    GuestFrame frame(ctx, 0xB0u);
    const std::uint32_t record = call_set(rt, ctx, 0x08B62344u, {0x30u});
    call_set(rt, ctx, 0x08897B8Cu, {record});
    call_set(rt, ctx, 0x088972BCu, {record, third});
    put(rt, frame.at(0u), 0x1Eu);
    put(rt, frame.at(8u), first);
    put(rt, frame.at(0xCu), second);
    put(rt, frame.at(0x10u), record);
    call_set(rt, ctx, 0x088936A4u, {word(rt, self + 0x18u), frame.at(0u)});
    if (record != 0u) {
        call_set(rt, ctx, 0x088978B4u, {record});
        call_set(rt, ctx, 0x08B64654u, {record});
    }
}

// func_088FAD1C(target, source) -> target: a 12-byte value copied into target from a frame temporary that
// func_08900D94 builds from source->+4 (or, when that is null, func_089032FC from source->+0 while source is non-null).

// func_0898E268(self, source): the string at self+0x12C is assigned from a frame temporary, built by func_08A399C0(temp,
// 0x08CF6ED8) when *source is 0 and by func_08A39AF0(temp) otherwise; the temporary is destroyed.
void func_0898E268(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t source = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t member = self + 0x12Cu;
    if (word(rt, source) == 0u) {
        call_set(rt, ctx, 0x08A399C0u, {frame.at(4u), 0x08CF6ED8u});
        if (member != frame.at(4u)) call_set(rt, ctx, kAssign, {member, word(rt, frame.at(4u))});
        call_set(rt, ctx, kDestroy, {frame.at(4u)});
        return;
    }
    call_set(rt, ctx, 0x08A39AF0u, {frame.at(0u), source});
    if (member != frame.at(0u)) call_set(rt, ctx, kAssign, {member, word(rt, frame.at(0u))});
    call_set(rt, ctx, kDestroy, {frame.at(0u)});
}

// func_089FA398(target, range): a buffer of (range->+4 - range->+0) bytes rounded down to 32 is allocated by
// func_08B35840(0x10, size); target gets {buffer, +4 = written, +8 = end, +0xC = -1} and func_08C0D7F8 copies the range in,
// its answer going to +4.

// func_08A79420(target, holder, first, second) -> target: the virtual slot 0xB0 of *holder (with the frame temporary as
// its first argument, the object, `first` and `second`) builds a value object that is assigned to target and destroyed.

// func_089FCFB8(self), a destructor: vtable 0x08CC7808 at +0, 0x08CC79A4 at +0x38, the string at +0x68 destroyed, and the
// reference-counted block at +0x64 (0x08D74FD0 is the shared empty one) released when func_08B60804(block - 4) answers
// non-positive through func_08B8542C(block - 0xC, temporary); func_089BBAC8(self) ends it.
void func_089FCFB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    put(rt, self, 0x08CC7808u);
    put(rt, self + 0x38u, 0x08CC79A4u);
    call_set(rt, ctx, kDestroy, {self + 0x68u});
    const std::uint32_t data = word(rt, self + 0x64u);
    if (data - 0xCu != 0x08D74FD0u) {
        if (signed_of(call_set(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu})) <= 0) {
            call_set(rt, ctx, 0x08B8542Cu, {data - 0xCu, frame.at(0u)});
        }
    }
    call_set(rt, ctx, 0x089BBAC8u, {self});
}

} // namespace new_batch_9
} // namespace gt::game

#define NEW9(address, name, kind) GT_DECOMP(address, gt::game::new_batch_9::name, gt::ReturnKind::kind)
NEW9(0x0886EB48u, func_0886EB48, Word);
NEW9(0x0887EE68u, func_0887EE68, Word);
NEW9(0x0888B2D4u, func_0888B2D4, Word);
NEW9(0x088A34BCu, func_088A34BC, Word);
NEW9(0x088E698Cu, func_088E698C, Word);
NEW9(0x088F1F84u, func_088F1F84, Word);
NEW9(0x0898E268u, func_0898E268, Word);
NEW9(0x089FCFB8u, func_089FCFB8, Word);
NEW9(0x08A351B4u, func_08A351B4, Word);
NEW9(0x08AAE5F0u, func_08AAE5F0, Word);
NEW9(0x08B99E48u, func_08B99E48, Word);
NEW9(0x08BC3C40u, func_08BC3C40, Word);
NEW9(0x08C676F0u, func_08C676F0, Word);
