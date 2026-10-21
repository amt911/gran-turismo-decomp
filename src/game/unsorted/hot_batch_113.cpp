// Hot batch 113: functions of 141-185 instructions the 11-route gate reaches, written by hand from `tools.disasm`;
// role words describe access shapes, none is a game name.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>

namespace gt::game {
namespace hot_batch_113 {
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

// The reference-count steps of a shared object: `ll/sc` on a word of guest memory (single-threaded here, the host never
// preempts inside one). The decrement leaves a0 = the old value, v1 = the word's address and v0 = 1 (the successful
// `sc`); true when the old value was 1.
inline bool count_down(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address) {
    const std::uint32_t old = word(rt, address);
    ctx.gpr[3] = address;
    ctx.gpr[4] = old;
    ctx.gpr[2] = 1u;
    put(rt, address, old - 1u);
    return old == 1u;
}

// Releases one use of a shared object: its use count (+4) goes down; when that was the last use the virtual slot 8 runs,
// then the weak count (+8) goes down and when that was the last one slot 0xC runs (a0 = the object for both).
inline void release_shared(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object) {
    if (!count_down(rt, ctx, object + 4u)) return;
    call_virtual_at(rt, ctx, object, 8u, {object});
    if (count_down(rt, ctx, object + 8u)) call_virtual_at(rt, ctx, object, 0xCu, {object});
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

// The `ll/sc` increment of a reference count (single-threaded here): v1 = the address, v0 = 1 (the successful `sc`).
inline void count_up(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address) {
    put(rt, address, word(rt, address) + 1u);
    ctx.gpr[3] = address;
    ctx.gpr[2] = 1u;
}

// A null check the way these functions spell it: func_08A5BB7C(message) runs when `object` is zero (it is used anyway).
inline std::uint32_t checked(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object, std::uint32_t message) {
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {message});
    return object;
}

// func_08844924(a0, a1, count, record): nothing when count < 3. Otherwise func_088447E8(temp1) gives an object (checked
// against 0x08C9C948) whose func_08844634 result is kept; record->+4's virtual slot 0x3C goes to a byte at frame +0x18 while
// func_08A98674(temp1, record + 8) builds a value object; its +0x14 table gives three floats (virtual slot 0x54 of entries
// 0..2, each checked) to frame +0xC / +0x10 / +0x14; func_08A98674(temp2, record + 0xC) and the same four slot-0x54 floats
// of temp2's table go to frame +0x1C..+0x28; func_08893D4C(kept, record->+0's slot 0x3C, frame + 0xC) takes them. Both
// value objects are destroyed.
void func_08844924(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t count = ctx.gpr[6];
    const std::uint32_t record = ctx.gpr[7];
    GuestFrame frame(ctx, 0x60u);
    if (signed_of(count) < 3) return;
    const std::uint32_t first = frame.at(0u);
    const std::uint32_t second = frame.at(4u);
    const std::uint32_t third = frame.at(8u);
    const std::uint32_t floats = frame.at(0xCu);
    call_set(rt, ctx, 0x088447E8u, {first});
    call_set(rt, ctx, 0x08844634u, {checked(rt, ctx, word(rt, first), 0x08C9C948u)});
    const std::uint32_t kept = ctx.gpr[2];
    call_set(rt, ctx, kDestroy, {first});
    const std::uint32_t source = word(rt, record + 4u);
    for (std::uint32_t offset = 0xCu; offset < 0x2Cu; offset += 4u) put(rt, frame.at(offset), 0u);
    const std::uint32_t source_object = checked(rt, ctx, source, 0x08C9C908u);
    call_virtual_at(rt, ctx, source_object, 0x3Cu, {source_object});
    put8(rt, frame.at(0x18u), ctx.gpr[2]);
    call_set(rt, ctx, 0x08A98674u, {second, record + 8u});
    for (std::uint32_t index = 0u; index < 3u; ++index) {
        const std::uint32_t entry = checked(rt, ctx, word(rt, word(rt, word(rt, second) + 0x14u) + index * 4u), 0x08C9C908u);
        call_virtual_at(rt, ctx, entry, 0x54u, {entry});
        put_float(rt, floats + index * 4u, ctx.fpr[0]);
    }
    call_set(rt, ctx, 0x08A98674u, {third, record + 0xCu});
    for (std::uint32_t offset = 0u; offset < 16u; offset += 4u) {
        const std::uint32_t entry = checked(rt, ctx, word(rt, word(rt, word(rt, third) + 0x14u) + offset), 0x08C9C908u);
        call_virtual_at(rt, ctx, entry, 0x54u, {entry});
        put_float(rt, floats + offset + 0x10u, ctx.fpr[0]);
    }
    const std::uint32_t selector = checked(rt, ctx, word(rt, record), 0x08C9C908u);
    const std::uint32_t slot = call_virtual_at(rt, ctx, selector, 0x3Cu, {selector});
    call_set(rt, ctx, 0x08893D4Cu, {kept, slot, floats});
    call_set(rt, ctx, kDestroy, {third});
    call_set(rt, ctx, kDestroy, {second});
}
// func_088DEA1C(self, a, b, extra): nothing (returns 0) when self->+0x118 (a handler) is null. The handler is called with
// (-1, frame) over a request {0, self->+0x124, a, b, -1, 0, byte 0} unless a chain of checks (func_088DA4C8(self, 0),
// func_088D9F80 / func_088DA168 compared, func_088DE9CC -> func_088E5E60(self->+0x640, a, b, extra) which can finish the
// call with 1, func_088D4C9C(self + 4), func_088DE5F4) picks a slot index: then the 760-byte record `index` of the table at
// self->+0xC0 is filled by func_088E7E94 (with the same request shape using that slot index), its byte +0x13 cleared and
// +0x1FC set, extra->+0x10 passed to func_088E8A24, the previous slot marked -2 (and slot 2 marked -3 when
// func_088D4D9C(self + 4)), and the handler is called with (index, frame); returns 1 for that path, else 0.
void func_088DEA1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t a = ctx.gpr[5];
    const std::uint32_t b = ctx.gpr[6];
    const std::uint32_t extra = ctx.gpr[7];
    GuestFrame frame(ctx, 0x50u);
    const std::uint32_t request = frame.at(0u);
    std::uint32_t handler = word(rt, self + 0x118u);
    if (handler == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const std::uint32_t inner = self + 4u;
    bool reread = false;   // the paths that jump to the fallback through `j` reload the handler first
    do {
        if (call_set(rt, ctx, 0x088DA4C8u, {self, 0u}) == 0u) break;
        const std::uint32_t before = call_set(rt, ctx, 0x088D9F80u, {self});
        const std::uint32_t after = call_set(rt, ctx, 0x088DA168u, {self});
        if (!(signed_of(after) < signed_of(before))) break;
        if (call_set(rt, ctx, 0x088DE9CCu, {self}) != 0u) {
            if (call_set(rt, ctx, 0x088E5E60u, {word(rt, self + 0x640u), a, b, extra}) != 0u) {
                ctx.gpr[2] = 1u;
                return;
            }
            reread = true;
            break;
        }
        if (call_set(rt, ctx, 0x088D4C9Cu, {inner}) != 0u) {
            reread = true;
            break;
        }
        const std::uint32_t slot = call_set(rt, ctx, 0x088DE5F4u, {self});
        if (slot == 0xFFFFFFFFu) {
            reread = true;
            break;
        }
        std::uint32_t previous = 0xFFFFFFFFu;
        if (call_set(rt, ctx, 0x088D4D9Cu, {inner}) != 0u && slot == 0u) previous = call_set(rt, ctx, 0x088DE5F4u, {self});
        const std::uint32_t offset = slot * 760u;
        const std::uint32_t table = word(rt, self + 0xC0u);
        const std::uint32_t current = word(rt, self + 0x124u);
        put(rt, request, 0u);
        put(rt, request + 4u, current);
        put(rt, request + 8u, a);
        put(rt, request + 0xCu, b);
        put(rt, request + 0x14u, 0u);
        put(rt, request + 0x10u, previous);
        put8(rt, request + 0x18u, 0u);
        call_set(rt, ctx, 0x088E7E94u, {table + offset, a, b});
        std::uint32_t record = word(rt, self + 0xC0u) + offset;
        put8(rt, record + 0x13u, 0u);
        put(rt, record + 0x1FCu, previous);
        if (extra != 0u) {
            const std::uint32_t argument = word(rt, extra + 0x10u);
            call_set(rt, ctx, 0x088E8A24u, {word(rt, self + 0xC0u) + offset, argument});
        }
        if (previous != 0xFFFFFFFFu) {
            const std::uint32_t old_record = word(rt, self + 0xC0u) + previous * 760u;
            put(rt, old_record + 0x1FCu, 0xFFFFFFFEu);
            put8(rt, old_record + 0x13u, 0u);
        }
        if (call_set(rt, ctx, 0x088D4D9Cu, {inner}) != 0u && slot == 2u) {
            record = word(rt, self + 0xC0u) + offset;
            put(rt, record + 0x1FCu, 0xFFFFFFFDu);
            put8(rt, record + 0x13u, 0u);
        }
        handler = word(rt, self + 0x118u);
        ctx.gpr[2] = handler;
        call_set(rt, ctx, handler, {slot, request});
        ctx.gpr[2] = 1u;
        return;
    } while (false);
    if (reread) handler = word(rt, self + 0x118u);
    put(rt, request + 4u, word(rt, self + 0x124u));
    put(rt, request + 8u, a);
    put(rt, request + 0xCu, b);
    put(rt, request + 0x10u, 0xFFFFFFFFu);
    put(rt, request, 0u);
    put(rt, request + 0x14u, 0u);
    put8(rt, request + 0x18u, 0u);
    call_set(rt, ctx, handler, {0xFFFFFFFFu, request});
    ctx.gpr[2] = 0u;
}
// func_08A13BE0(self): a loop that runs until the byte self->+0x41 is set. Each pass: func_08A1369C(self, 0); under the
// lock at self + 0x10 (func_08B34888 / func_08B34B24) the byte +0x1F9 is the pass flag (0, then 1) and +0x40 cleared
// (func_08B28C04(lock)); func_08A13604(self, 0); func_08A19AE4(self + 0x178, self + 0x140); +0x1C0 / +0x1C4 and the three
// words at +0x60.. cleared and +0xC = 1; the byte at +0x18D is saved and cleared and a 0x80-byte frame buffer filled by
// func_08A19A9C. With the sink at self + 0x240 (func_08ACCCA0, func_08ACDEF0, then func_08ACCCA8 and
// func_08A1369C(self, 2) deciding) the work of one frame is done (func_08ACCD2C, func_08ACE9DC, func_08ACCDFC, the
// +0x784 handle calls func_08ACCC60 / func_08ACEB40 / func_08ACCC50, the float at +0x190 to func_08ACD174, func_08A13654 /
// func_08A13604 / func_08B28C44 / func_08ACE760); every pass ends with func_08A1369C(self, 3) and func_08ACDD14(sink).
void func_08A13BE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0xA0u);
    const std::uint32_t lock = self + 0x10u;
    const std::uint32_t block = self + 0x178u;
    const std::uint32_t sink = self + 0x240u;
    const std::uint32_t words = self + 0x5Cu;
    const std::uint32_t buffer = frame.at(0u);
    std::uint32_t flag = 0u;
    for (;;) {
        call_set(rt, ctx, 0x08A1369Cu, {self, 0u});
        call_set(rt, ctx, 0x08B34888u, {lock});
        put8(rt, self + 0x1F9u, flag);
        put8(rt, self + 0x40u, 0u);
        call_set(rt, ctx, 0x08B28C04u, {lock});
        put8(rt, self + 0x1F9u, 0u);
        call_set(rt, ctx, 0x08B34B24u, {lock});
        const std::uint32_t done = byte_u(rt, self + 0x41u);
        ctx.gpr[5] = 0u;
        if (done != 0u) {
            ctx.gpr[2] = done;
            return;
        }
        call_set(rt, ctx, 0x08A13604u, {self, 0u});
        call_set(rt, ctx, 0x08B34888u, {lock});
        call_set(rt, ctx, 0x08A19AE4u, {block, self + 0x140u});
        put(rt, self + 0x1C4u, 0u);
        put(rt, self + 0x1C0u, 0u);
        std::uint32_t source = self + 0x1A0u;
        put(rt, words + 4u, 0u);
        put(rt, words + 8u, 0u);
        put(rt, words + 0xCu, 0u);
        const std::uint32_t saved = byte_u(rt, block + 0x15u);
        put8(rt, block + 0x15u, 0u);
        put(rt, self + 0xCu, 1u);
        call_set(rt, ctx, 0x08B34B24u, {lock});
        call_set(rt, ctx, 0x08A19A9Cu, {block, buffer, 0x80u});
        if (word(rt, self + 0x1A0u) == 0u || word(rt, source + 4u) == 0u) source = 0u;
        call_set(rt, ctx, 0x08ACCCA0u, {sink, byte_u(rt, block + 0x16u)});
        call_set(rt, ctx, 0x08ACDEF0u, {sink, buffer, source});
        bool frame_work = call_set(rt, ctx, 0x08ACCCA8u, {sink}) != 0u;
        if (frame_work) frame_work = call_set(rt, ctx, 0x08A1369Cu, {self, 2u}) != 0u;
        if (frame_work) {
            call_set(rt, ctx, 0x08ACCD2Cu, {sink});
            call_set(rt, ctx, 0x08ACE9DCu, {sink, 0u, words, word(rt, self + 0x1BCu)});
            call_set(rt, ctx, 0x08ACCDFCu, {sink, 0u, word(rt, self + 0x48u), word(rt, self + 0x4Cu)});
            const std::uint32_t handle = word(rt, self + 0x784u);
            const std::uint32_t valid = call_set(rt, ctx, 0x08ACCC60u, {sink, handle});
            ctx.gpr[7] = 0x2000u;
            if (valid != 0u) {
                call_set(rt, ctx, 0x08ACEB40u, {sink, word(rt, self + 0x784u), self + 0xA0u, 0x2000u});
                call_set(rt, ctx, 0x08ACCC50u, {sink, word(rt, self + 0x784u), saved ^ 1u});
                const std::uint32_t state = call_set(rt, ctx, 0x08ACCC60u, {sink, word(rt, self + 0x784u)});
                put8(rt, block + 0x15u, state);
            }
            call_set(rt, ctx, 0x08B34888u, {lock});
            call_setf(rt, ctx, 0x08ACD174u, {sink}, {float_at(rt, block + 0x18u)});
            call_set(rt, ctx, 0x08A13654u, {self, 1u});
            call_set(rt, ctx, 0x08B34B24u, {lock});
            call_set(rt, ctx, 0x08A13604u, {self, 1u});
            call_set(rt, ctx, 0x08B28C44u, {self + 0x200u});
            call_set(rt, ctx, 0x08ACE760u, {sink});
            call_set(rt, ctx, 0x08A13654u, {self, 0u});
        }
        call_set(rt, ctx, 0x08A1369Cu, {self, 3u});
        call_set(rt, ctx, 0x08ACDD14u, {sink});
        flag = 1u;
    }
}
// func_08845CB0(target, a, flag, b): the flag selects the shape. Zero: the object func_088458D0 yields (checked against
// 0x08C9CA90) goes through func_08845798 and func_08894140; func_08A98C1C(temp, 2) gives a record r whose first word is
// kept; func_08A85B1C builds a value object from r->+0 into the table entry (temp->+0x14)->+0x14 (assigned unless it is
// the frame temporary); when r->+0 is 9 the same is done for a second record (func_08A98C1C(temp2, 1) with r->+4) and
// the entry (temp->+0x14)->+0x14 + 4; the result of the first record's temporary is assigned to target and all are
// destroyed. Non-zero: func_08A98674(temp2, b), func_088458D0 / func_08845798 / func_0889414C give a destination d; the
// table of temp2 (+0x14, entry 0, checked against 0x08C9CA54) virtual slot 0x3C gives d->+0 and, when the
// +0x14 table's word +4 is set and that value is 9, entry 0 of a second func_08A98674(temp, table + 4) gives d->+4.
void func_08845CB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t target = ctx.gpr[4];
    const std::uint32_t a = ctx.gpr[5];
    const std::uint32_t flag = ctx.gpr[6];
    const std::uint32_t b = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    const std::uint32_t temp14 = frame.at(0x14u);
    const std::uint32_t temp10 = frame.at(0x10u);
    if (flag == 0u) {
        const std::uint32_t temp0C = frame.at(0xCu);
        call_set(rt, ctx, 0x088458D0u, {temp0C});
        const std::uint32_t checked_object = checked(rt, ctx, word(rt, temp0C), 0x08C9CA90u);
        std::uint32_t current = call_set(rt, ctx, 0x08845798u, {checked_object});
        call_set(rt, ctx, kDestroy, {temp0C});
        const std::uint32_t record = call_set(rt, ctx, 0x08894140u, {current});
        call_set(rt, ctx, 0x08A98C1Cu, {temp14, 2u});
        const std::uint32_t entry = word(rt, word(rt, temp14) + 0x14u);
        const std::uint32_t temp08 = frame.at(8u);
        call_set(rt, ctx, 0x08A85B1Cu, {temp08, word(rt, record)});
        if (entry != temp08) call_set(rt, ctx, kAssign, {entry, word(rt, temp08)});
        call_set(rt, ctx, kDestroy, {temp08});
        if (word(rt, record) == 9u) {
            call_set(rt, ctx, 0x08A98C1Cu, {temp10, 1u});
            const std::uint32_t second = word(rt, word(rt, temp10) + 0x14u);
            const std::uint32_t temp04 = frame.at(4u);
            call_set(rt, ctx, 0x08A85B1Cu, {temp04, word(rt, record + 4u)});
            if (second != temp04) call_set(rt, ctx, kAssign, {second, word(rt, temp04)});
            call_set(rt, ctx, kDestroy, {temp04});
            const std::uint32_t slot = word(rt, word(rt, temp14) + 0x14u) + 4u;
            if (slot != temp10) call_set(rt, ctx, kAssign, {slot, word(rt, temp10)});
            call_set(rt, ctx, kDestroy, {temp10});
        }
        keep_temporary(rt, ctx, target, temp14);
        return;
    }
    call_set(rt, ctx, 0x08A98674u, {temp10, b});
    const std::uint32_t temp00 = frame.at(0u);
    call_set(rt, ctx, 0x088458D0u, {temp00, a});
    const std::uint32_t checked_object = checked(rt, ctx, word(rt, temp00), 0x08C9CA90u);
    const std::uint32_t found = call_set(rt, ctx, 0x08845798u, {checked_object});
    call_set(rt, ctx, kDestroy, {temp00});
    const std::uint32_t destination = call_set(rt, ctx, 0x0889414Cu, {found});
    const std::uint32_t head = checked(rt, ctx, word(rt, word(rt, word(rt, temp10) + 0x14u)), 0x08C9CA54u);
    const std::uint32_t kind = call_virtual_at(rt, ctx, head, 0x3Cu, {head});
    put(rt, destination, kind);
    const std::uint32_t table = word(rt, word(rt, temp10) + 0x14u);
    if (word(rt, table + 4u) != 0u && kind == 9u) {
        call_set(rt, ctx, 0x08A98674u, {temp14, table + 4u});
        const std::uint32_t second = checked(rt, ctx, word(rt, word(rt, word(rt, temp14) + 0x14u)), 0x08C9CA54u);
        const std::uint32_t value = call_virtual_at(rt, ctx, second, 0x3Cu, {second});
        put(rt, destination + 4u, value);
        call_set(rt, ctx, kDestroy, {temp14});
    }
    call_set(rt, ctx, kDestroy, {temp10});
}
// func_0888BD14(self, a, ids, count, flag1, flag2): over the `count` halfwords `ids`. When func_088D4C50(self->+4 + 4) is
// zero, first each id is looked up (func_088DA588(self->+4, id)); a found object gets virtual slot 0x1C(obj, 1) and then
// slot 0x18(obj, 1, 0x50) when flag2 is set, else slot 0x1C(obj, 1) and slot 0x14(obj, 0). Then (and directly when that
// call was non-zero) the ids are walked until one of them takes the "hit" branch: func_088DA788 -> func_088D0918 ->
// func_088D6204(a, id) = 1, or = 0 with func_088DB2C8(self->+4, id) non-zero: func_08917AB0(self->+8), then
// func_088D6FD4(self->+4, func_088D5938(a), flag1 ^ 1, 0x22). Last, every id is sent: func_088DB2C8(self->+4, id) and a
// 0x12-byte request {0x25, -, id, 0x22, byte flag1, byte result} to func_088936A4(self->+0x18, request). Returns 0 /
// flag1 ^ 1 for an empty list, else the last call's result.
void func_0888BD14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t a = ctx.gpr[5];
    const std::uint32_t ids = ctx.gpr[6];
    const std::uint32_t count = ctx.gpr[7];
    const std::uint32_t flag1 = ctx.gpr[8] & 0xFFu;
    const std::uint32_t flag2 = ctx.gpr[9] & 0xFFu;
    GuestFrame frame(ctx, 0xE0u);
    const std::uint32_t scratch = frame.at(0xA0u);
    if (call_set(rt, ctx, 0x088D4C50u, {word(rt, self + 4u) + 4u}) == 0u) {
        if (!(signed_of(count) > 0)) return;
        for (std::uint32_t index = 0u; index != count; ++index) {
            const std::uint32_t object = call_set(rt, ctx, 0x088DA588u, {word(rt, self + 4u), half_u(rt, ids + index * 2u)});
            if (object == 0u) continue;
            if (flag2 != 0u) {
                ctx.gpr[2] = word(rt, object);
                ctx.gpr[3] = word(rt, ctx.gpr[2] + 0x1Cu);
                call_set(rt, ctx, ctx.gpr[3], {object, 1u});
                ctx.gpr[6] = word(rt, object);
                ctx.gpr[2] = word(rt, ctx.gpr[6] + 0x18u);
                call_set(rt, ctx, ctx.gpr[2], {object, 1u, 0x50u});
            } else {
                ctx.gpr[3] = word(rt, object);
                ctx.gpr[2] = word(rt, ctx.gpr[3] + 0x1Cu);
                call_set(rt, ctx, ctx.gpr[2], {object, 1u});
                call_virtual_at(rt, ctx, object, 0x14u, {object, 0u});
            }
        }
    } else if (!(signed_of(count) > 0)) {
        ctx.gpr[2] = flag1 ^ 1u;
        return;
    }
    put(rt, scratch, flag1 ^ 1u);
    bool found = false;
    std::uint32_t seen = 0u;
    for (std::uint32_t index = 0u;; ) {
        const std::uint32_t id = half_u(rt, ids + index * 2u);
        ++index;
        const std::uint32_t entry = call_set(rt, ctx, 0x088DA788u, {word(rt, self + 4u), id});
        call_set(rt, ctx, 0x088D0918u, {entry});
        // a1 is not set before this call in the original: the callee's leftover goes through
        const std::uint32_t result = call_set(rt, ctx, 0x088D6204u, {a, ctx.gpr[5]});
        ctx.gpr[5] = id;
        bool hit = false;
        if (result == 0u) {
            hit = call_set(rt, ctx, 0x088DB2C8u, {word(rt, self + 4u), id}) != 0u;
        } else {
            hit = result == 1u;
        }
        ++seen;
        if (hit) {
            found = true;
            call_set(rt, ctx, 0x08917AB0u, {word(rt, self + 8u)});
            const std::uint32_t target = word(rt, self + 4u);
            const std::uint32_t chosen = call_set(rt, ctx, 0x088D5938u, {a});
            call_set(rt, ctx, 0x088D6FD4u, {target, chosen, word(rt, scratch) & 0xFFu, 0x22u});
        }
        if (seen == count || found) break;
    }
    for (std::uint32_t index = 0u; index != count; ++index) {
        const std::uint32_t id = half_u(rt, ids + index * 2u);
        put(rt, frame.at(0u), 0x25u);
        put(rt, frame.at(8u), id);
        put(rt, frame.at(0xCu), 0x22u);
        put8(rt, frame.at(0x10u), flag1);
        const std::uint32_t state = call_set(rt, ctx, 0x088DB2C8u, {word(rt, self + 4u), id});
        put8(rt, frame.at(0x11u), state);
        call_set(rt, ctx, 0x088936A4u, {word(rt, self + 0x18u), frame.at(0u)});
    }
}
// func_08B71720(a, b, c, d): the soft-float divide on two unpacked values: the four argument words go to frame +0..+0xC,
// func_08B72314(frame, x) and func_08B72314(frame + 8, y) unpack them into 0x18-byte records at +0x10 (x) / +0x28 (y)
// {class, sign, exponent, -, low, high}; the packed result is made by func_08B72030(record). Class below 2 returns that
// operand; class 4 / 2 of x gives x (or the constant at 0x08C966B0 when y has the same class); y of class 4 gives a zero
// record; y of class 2 turns x into class 4. Otherwise the signs are xored, the exponents subtracted and the 64-bit
// mantissas divided by 0x3C shift-and-subtract steps, with the round-to-even fix on the low byte (0x80) at the end.
void func_08B71720(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x50u);
    const std::uint32_t x = frame.at(0x10u);
    const std::uint32_t y = frame.at(0x28u);
    put(rt, frame.at(0u), ctx.gpr[4]);
    put(rt, frame.at(4u), ctx.gpr[5]);
    put(rt, frame.at(8u), ctx.gpr[6]);
    put(rt, frame.at(0xCu), ctx.gpr[7]);
    call_set(rt, ctx, 0x08B72314u, {frame.at(0u), x});
    call_set(rt, ctx, 0x08B72314u, {frame.at(8u), y});
    const std::uint32_t x_class = word(rt, x);
    if (x_class < 2u) {
        call_set(rt, ctx, 0x08B72030u, {x});
        return;
    }
    const std::uint32_t y_class = word(rt, y);
    if (y_class < 2u) {
        call_set(rt, ctx, 0x08B72030u, {y});
        return;
    }
    put(rt, x + 4u, word(rt, x + 4u) ^ word(rt, y + 4u));
    if (x_class == 4u || x_class == 2u) {
        if (y_class != x_class) call_set(rt, ctx, 0x08B72030u, {x});
        else call_set(rt, ctx, 0x08B72030u, {0x08C966B0u});
        return;
    }
    if (y_class == 4u) {
        put(rt, x + 0x10u, 0u);
        put(rt, x + 0x14u, 0u);
        put(rt, x + 8u, 0u);
        call_set(rt, ctx, 0x08B72030u, {x});
        return;
    }
    if (y_class == 2u) {
        put(rt, x, 4u);
        call_set(rt, ctx, 0x08B72030u, {x});
        return;
    }
    std::uint32_t exponent = word(rt, x + 8u) - word(rt, y + 8u);
    std::uint32_t high = word(rt, x + 0x14u);
    std::uint32_t low = word(rt, x + 0x10u);
    const std::uint32_t divisor_high = word(rt, y + 0x14u);
    const std::uint32_t divisor_low = word(rt, y + 0x10u);
    put(rt, x + 8u, exponent);
    bool below = high < divisor_high;
    bool shift_first = below;
    if (!below && high == divisor_high) shift_first = low < divisor_low;
    if (shift_first) {
        high = (low >> 31) | (high << 1);
        low <<= 1;
        put(rt, x + 8u, exponent - 1u);
        below = high < divisor_high;
    }
    std::uint32_t bit_low = 0u;
    std::uint32_t bit_high = 0x10000000u;
    std::uint32_t quotient_low = 0u;
    std::uint32_t quotient_high = 0u;
    for (std::uint32_t steps = 0x3Du - 1u;; --steps) {
        if (!below) {
            const std::uint32_t next_low = low - divisor_low;
            const std::uint32_t borrow = low < next_low ? 1u : 0u;
            const std::uint32_t next_high = high - divisor_high - borrow;
            if (!(high == divisor_high && low < divisor_low)) {
                quotient_low |= bit_low;
                quotient_high |= bit_high;
                low = next_low;
                high = next_high;
            }
        }
        const std::uint32_t new_bit_low = (bit_low >> 1) | (bit_high << 31);
        const std::uint32_t new_bit_high = bit_high >> 1;
        high = (high << 1) | (low >> 31);
        low <<= 1;
        bit_low = new_bit_low;
        bit_high = new_bit_high;
        if (steps == 0u) break;
        below = high < divisor_high;
    }
    if ((quotient_low & 0xFFu) == 0x80u && (quotient_low & 0x100u) == 0u && (low | high) != 0u) {
        const std::uint32_t sum = quotient_low + 0x80u;
        quotient_high += sum < quotient_low ? 1u : 0u;
        quotient_low = sum & ~0xFFu;
    }
    put(rt, x + 0x10u, quotient_low);
    put(rt, x + 0x14u, quotient_high);
    call_set(rt, ctx, 0x08B72030u, {x});
}
// func_08C23158(self, holder): value objects a = frame +0x10, b = +0x14 and c = +0x18 are built (func_08A76AA0, func_08A5D438,
// func_08A7C0D8(c, 0x08CF6ED8)); the object o = func_08A61624(holder)'s virtual slot 0x10C runs with (o, b, self + 8). When
// func_08A5D50C(b) is zero, a record is made: d = func_08A7C0D8(0x08CF6ED8); func_08A7DBB0(o, a, self + 0xC); and when
// func_08A922DC(d, a) is non-zero, two value objects (+0x20 / +0x24) are built, a string from func_08B85EB8(self->+8->+4)
// is assigned to the first, the string released, func_08A9219C() and func_08A77C54(that, c, 0x08CF6FE0, 2, first) run,
// func_08A7CE50(o, c) and the two objects destroyed. Else (b set) o's virtual slot 0x1C(o, a, temp) runs (o checked
// against 0x08CB028C) and func_08A922DC(c, a). Last func_08A62C54(holder, c->+0) and a, b, c destroyed.
void func_08C23158(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t holder = ctx.gpr[5];
    GuestFrame frame(ctx, 0x60u);
    const std::uint32_t a = frame.at(0x10u);
    const std::uint32_t b = frame.at(0x14u);
    const std::uint32_t c = frame.at(0x18u);
    call_set(rt, ctx, 0x08A76AA0u, {a});
    call_set(rt, ctx, 0x08A5D438u, {b});
    call_set(rt, ctx, 0x08A7C0D8u, {c, 0x08CF6ED8u});
    const std::uint32_t object = call_set(rt, ctx, 0x08A61624u, {holder});
    ctx.gpr[3] = word(rt, object);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 0x10Cu);
    call_set(rt, ctx, ctx.gpr[2], {object, b, self + 8u});
    if (call_set(rt, ctx, 0x08A5D50Cu, {b}) != 0u) {
        const std::uint32_t target = checked(rt, ctx, word(rt, b), 0x08CB028Cu);
        const std::uint32_t temporary = frame.at(4u);
        const std::uint32_t vtable = word(rt, target);
        call_set(rt, ctx, 0x08A76AA0u, {temporary});
        ctx.gpr[17] = word(rt, vtable + 0x1Cu);
        call_set(rt, ctx, ctx.gpr[17], {target, a, temporary});
        call_set(rt, ctx, kDestroy, {temporary});
        call_set(rt, ctx, 0x08A922DCu, {c, a});
    } else {
        const std::uint32_t record = frame.at(0x1Cu);
        const std::uint32_t pair = frame.at(0x20u);
        call_set(rt, ctx, 0x08A7C0D8u, {record, 0x08CF6ED8u});
        call_set(rt, ctx, 0x08A7DBB0u, {object, a, self + 0xCu});
        if (call_set(rt, ctx, 0x08A922DCu, {record, a}) != 0u) {
            call_set(rt, ctx, 0x08A76AA0u, {pair});
            call_set(rt, ctx, 0x08A76AA0u, {pair + 4u});
            const std::uint32_t text = frame.at(8u);
            const std::uint32_t copy = frame.at(0xCu);
            call_set(rt, ctx, 0x08B85EB8u, {text, word(rt, word(rt, self + 8u) + 4u), frame.at(1u)});
            call_set(rt, ctx, 0x08A6D2D4u, {copy, text});
            const std::uint32_t assigned = word(rt, copy);
            if (pair != copy) call_set(rt, ctx, kAssign, {pair, assigned});
            call_set(rt, ctx, kDestroy, {copy});
            release_string(rt, ctx, text, 0x08D74FD0u, 0x08B8542Cu, frame.at(0u));
            if (pair + 4u != record) call_set(rt, ctx, kAssign, {pair + 4u, word(rt, record)});
            const std::uint32_t found = call_set(rt, ctx, 0x08A9219Cu, {});
            call_set(rt, ctx, 0x08A77C54u, {found, c, 0x08CF6FE0u, 2u, pair});
            call_set(rt, ctx, 0x08A7CE50u, {object, c});
            call_set(rt, ctx, kDestroy, {pair + 4u});
            call_set(rt, ctx, kDestroy, {pair});
        }
        call_set(rt, ctx, kDestroy, {record});
    }
    call_set(rt, ctx, 0x08A62C54u, {holder, word(rt, c)});
    call_set(rt, ctx, kDestroy, {c});
    call_set(rt, ctx, kDestroy, {b});
    call_set(rt, ctx, kDestroy, {a});
}
// func_08A535A4(self, key, third, count, extra): nothing (returns self->+0x54) when that word is not negative. Otherwise
// value object v = frame +0x10 (func_08A52A98); a non-negative `count` first makes a 0x44-byte block (func_08A5BC20,
// func_08A5B70C(block, v, count)) assigned into v through func_08A52AB0. A 0x18-byte node (func_08A5BC20, func_08A5B828,
// vtable 0x08CD1F48, v copied to +0xC, extra at +0x10, -1 at +0x14) is put in a handle at frame +0x14; the map at
// self + 0x38 looks `key` up (func_08C1D288): a found entry (not the end self + 0x3C) has its +0x14 handle's node copied
// (checked) into the new node's +0x14, else func_08C1D250 gives the lower bound which, unless it holds a key not above
// `key`, is followed by a func_08C1D530 insert of the pair; the handle is assigned to the entry's +0x14.
void func_08A535A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t key = ctx.gpr[5];
    const std::uint32_t third = ctx.gpr[6];
    const std::uint32_t count = ctx.gpr[7];
    const std::uint32_t extra = ctx.gpr[8];
    GuestFrame frame(ctx, 0x60u);
    const std::uint32_t flag = word(rt, self + 0x54u);
    if (signed_of(flag) >= 0) {
        ctx.gpr[2] = flag;
        return;
    }
    const std::uint32_t value = frame.at(0x10u);
    const std::uint32_t handle = frame.at(0x14u);
    call_set(rt, ctx, 0x08A52A98u, {value, third});
    if (signed_of(count) >= 0) {
        const std::uint32_t memory = call_set(rt, ctx, 0x08A5BC20u, {0x44u, 4u});
        call_set(rt, ctx, 0x08A5B70Cu, {memory, value, count});
        const std::uint32_t pointer = frame.at(8u);
        const std::uint32_t temporary = frame.at(0xCu);
        put(rt, pointer, memory);
        call_set(rt, ctx, 0x08A52AB0u, {temporary, pointer});
        if (value != temporary) call_set(rt, ctx, kAssign, {value, word(rt, temporary)});
        call_set(rt, ctx, kDestroy, {temporary});
    }
    const std::uint32_t node = call_set(rt, ctx, 0x08A5BC20u, {0x18u, 4u});
    call_set(rt, ctx, 0x08A5B828u, {node});
    put(rt, node, 0x08CD1F48u);
    call_set(rt, ctx, 0x08A52A98u, {node + 0xCu, value});
    put(rt, node + 0x10u, extra);
    put(rt, node + 0x14u, 0xFFFFFFFFu);
    const std::uint32_t map = self + 0x38u;
    const std::uint32_t map_end = self + 0x3Cu;
    put(rt, frame.at(4u), node);
    call_set(rt, ctx, 0x08A5BAB8u, {handle, frame.at(4u)});
    const std::uint32_t entry = call_set(rt, ctx, 0x08C1D288u, {map, key});
    if (map_end != entry) {
        const std::uint32_t slot = entry + 0x14u;
        const std::uint32_t copy = frame.at(0x18u);
        call_set(rt, ctx, 0x08A590CCu, {copy, slot});
        const std::uint32_t target = checked(rt, ctx, word(rt, handle), 0x08CAF594u);
        const std::uint32_t source = checked(rt, ctx, word(rt, copy), 0x08CAF594u);
        put(rt, target + 0x14u, word(rt, source + 0x14u));
        if (slot != handle) call_set(rt, ctx, kAssign, {slot, word(rt, handle)});
        call_set(rt, ctx, kDestroy, {copy});
    } else {
        const std::uint32_t holder = checked(rt, ctx, word(rt, handle), 0x08CAF594u);
        put(rt, holder + 0x14u, word(rt, map + 0x14u));
        const std::uint32_t lower = call_set(rt, ctx, 0x08C1D250u, {map, key});
        std::uint32_t result = lower;
        if (map_end == lower || word(rt, key) < word(rt, lower + 0x10u)) {
            const std::uint32_t empty = frame.at(0u);
            const std::uint32_t inserted = frame.at(0x1Cu);
            const std::uint32_t text = frame.at(0x20u);
            call_set(rt, ctx, 0x08A5BAB8u, {empty, 0x08CF6ED8u});
            call_set(rt, ctx, 0x08A6972Cu, {inserted, key});
            call_set(rt, ctx, 0x08A590CCu, {text, empty});
            result = call_set(rt, ctx, 0x08C1D530u, {map, lower, inserted});
            call_set(rt, ctx, kDestroy, {text});
            call_set(rt, ctx, 0x08A69D68u, {inserted});
            call_set(rt, ctx, kDestroy, {empty});
        }
        const std::uint32_t slot = result + 0x14u;
        if (slot != handle) call_set(rt, ctx, kAssign, {slot, word(rt, handle)});
    }
    call_set(rt, ctx, kDestroy, {handle});
    call_set(rt, ctx, kDestroy, {value});
}
// func_089A50A0(out, owner, text, flag_a, flag_b, flag_c): returns `out`. Builds the value object in `out`
// (func_089E4C14(out, 0x08CF6ED8)); when flag_c is set and func_089A41A8 / func_089A4284(owner, out, text) give non-zero it
// returns at once. With owner->+0x138 & 2 a block func_08B3544C(-0x10) is kept at frame +0x20. A string
// (func_08B85EB8(0x08CA4EF8)) is compared with `text` (func_08B36170 on their lengths and the shorter length); the
// result picks func_089E4C2C(temp, owner) or func_089F52BC(temp, owner) to assign to `out`; the string is released.
// out->+0 (checked against 0x08CA4F08) gets the bytes flag_a at +0x10 and flag_b at +0x11, then its virtual slot 0x18 runs
// with (obj, text); a zero result resets `out` and flag_c; the block is freed again and, with flag_c, func_089A4EFC(owner,
// text, out) runs.
void func_089A50A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t owner = ctx.gpr[5];
    const std::uint32_t text = ctx.gpr[6];
    const std::uint32_t flag_a = ctx.gpr[7] & 0xFFu;
    const std::uint32_t flag_b = ctx.gpr[8] & 0xFFu;
    std::uint32_t flag_c = ctx.gpr[9] & 0xFFu;
    GuestFrame frame(ctx, 0x60u);
    call_set(rt, ctx, 0x089E4C14u, {out, 0x08CF6ED8u});
    if (flag_c != 0u) {
        if (call_set(rt, ctx, 0x089A41A8u, {owner, out, text}) != 0u) {
            ctx.gpr[2] = out;
            return;
        }
        if (call_set(rt, ctx, 0x089A4284u, {owner, out, text}) != 0u) {
            ctx.gpr[2] = out;
            return;
        }
    }
    put(rt, frame.at(0x20u), 0u);
    if ((word(rt, owner + 0x138u) & 2u) != 0u) put(rt, frame.at(0x20u), call_set(rt, ctx, 0x08B3544Cu, {0xFFFFFFF0u}));
    call_set(rt, ctx, 0x08B85EB8u, {frame.at(4u), 0x08CA4EF8u, frame.at(1u)});
    const std::uint32_t own_text = word(rt, text);
    const std::uint32_t other = word(rt, frame.at(4u));
    const std::uint32_t own_length = word(rt, own_text - 0xCu);
    put(rt, frame.at(0x14u), own_length);
    const std::uint32_t other_rep = other - 0xCu;
    const std::uint32_t other_length = word(rt, other - 0xCu);
    put(rt, frame.at(0x18u), other_length);
    std::uint32_t difference = own_length - other_length;
    const std::uint32_t shorter = word(rt, other_length < own_length ? frame.at(0x18u) : frame.at(0x14u));
    const std::uint32_t compared = call_set(rt, ctx, 0x08B36170u, {own_text, other, shorter});
    if (compared != 0u) difference = compared;
    if (other_rep != 0x08D74FD0u) {
        if (!(signed_of(call_set(rt, ctx, 0x08B60804u, {other - 4u, 0xFFFFFFFFu})) > 0)) call_set(rt, ctx, 0x08B8542Cu, {other_rep, frame.at(0u)});
    }
    std::uint32_t temporary;
    if (difference == 0u) {
        temporary = frame.at(0x10u);
        call_set(rt, ctx, 0x089F52BCu, {temporary, owner});
        if (out != temporary) call_set(rt, ctx, kAssign, {out, word(rt, temporary)});
    } else {
        temporary = frame.at(0xCu);
        call_set(rt, ctx, 0x089E4C2Cu, {temporary, owner});
        if (out != temporary) call_set(rt, ctx, kAssign, {out, word(rt, temporary)});
    }
    call_set(rt, ctx, kDestroy, {temporary});
    std::uint32_t target = checked(rt, ctx, word(rt, out), 0x08CA4F08u);
    put8(rt, target + 0x10u, flag_a);
    target = checked(rt, ctx, word(rt, out), 0x08CA4F08u);
    put8(rt, target + 0x11u, flag_b);
    target = checked(rt, ctx, word(rt, out), 0x08CA4F08u);
    ctx.gpr[3] = word(rt, target);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 0x18u);
    std::uint32_t flags;
    if (call_set(rt, ctx, ctx.gpr[2], {target, text}) != 0u) {
        flags = word(rt, owner + 0x138u);
    } else {
        temporary = frame.at(8u);
        call_set(rt, ctx, 0x089E4C14u, {temporary, 0x08CF6ED8u});
        if (out != temporary) call_set(rt, ctx, kAssign, {out, word(rt, temporary)});
        call_set(rt, ctx, kDestroy, {temporary});
        flag_c = 0u;
        flags = word(rt, owner + 0x138u);
    }
    if ((flags & 2u) != 0u) call_set(rt, ctx, 0x08B3544Cu, {word(rt, frame.at(0x20u))});
    if (flag_c != 0u) call_set(rt, ctx, 0x089A4EFCu, {owner, text, out});
    ctx.gpr[2] = out;
}
// func_0888AF70(self, record): 0 when the byte record->+0x12 is clear. self holds a list of up to 20 entry indices at
// +0xD20 (count at +0xD70, spare index at +0xD6C) over 168-byte entries at self; each entry has a 64-bit key at +0x20.
// func_0888A93C(entry, record) picks the position of the first matching entry; that entry (or a new one at the end, or the
// spare when the list is full) is returned 0 early when its key equals the record's key or when the key order disagrees
// with the byte self->+0x1C35. Otherwise func_0888A878(entry, record) runs and the entry index is moved towards the front
// while the previous entries' keys order wrongly (stopping with 1 on an equal key or the right order). Returns 0, 1 or the
// last compared key's low word.
void func_0888AF70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t record = ctx.gpr[5];
    if (byte_u(rt, record + 0x12u) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const std::uint32_t key_low = word(rt, record + 0x20u);
    const std::uint32_t key_high = word(rt, record + 0x24u);
    const std::uint32_t count = word(rt, self + 0xD70u);
    const std::uint32_t list = self + 0xD20u;
    std::uint32_t position = 0u;
    std::uint32_t entry_index = 0u;
    bool matched = false;
    if (signed_of(count) > 0) {
        for (position = 0u; position != count;) {
            const std::uint32_t index = word(rt, list + position * 4u);
            if (call_set(rt, ctx, 0x0888A93Cu, {self + index * 168u, record}) != 0u) {
                matched = position != 0xFFFFFFFFu;
                break;
            }
            ++position;
        }
        if (position == count) matched = false;
    }
    // Whether the entry's key orders after the record's key the way the order byte wants (false: return 0).
    const auto accepts = [&](std::uint32_t index) {
        const std::uint32_t low = word(rt, self + index * 168u + 0x20u);
        const std::uint32_t high = word(rt, self + index * 168u + 0x24u);
        const std::uint32_t order = byte_u(rt, self + 0x1C35u);
        if (low == key_low && high == key_high) return false;
        const bool before = high > key_high || (high == key_high && low > key_low);
        return order != (before ? 1u : 0u);
    };
    if (matched) {
        entry_index = word(rt, list + position * 4u);
        if (!accepts(entry_index)) {
            ctx.gpr[2] = 0u;
            return;
        }
    } else if (signed_of(count) < 20) {
        entry_index = word(rt, list + count * 4u);
        position = count;
        put(rt, self + 0xD70u, count + 1u);
    } else {
        entry_index = word(rt, self + 0xD6Cu);
        if (!accepts(entry_index)) {
            ctx.gpr[2] = 0u;
            return;
        }
        position = 0x13u;
        put(rt, self + 0xD70u, 0x14u);
    }
    call_set(rt, ctx, 0x0888A878u, {self + entry_index * 168u, record});
    if (signed_of(position) <= 0) {
        ctx.gpr[2] = 1u;
        return;
    }
    const std::uint32_t order = byte_u(rt, self + 0x1C35u);
    std::uint32_t slot = list + position * 4u - 4u;
    for (std::uint32_t step = 1u;; ++step) {
        const std::uint32_t previous = word(rt, slot);
        const std::uint32_t entry = self + previous * 168u;
        const std::uint32_t high = word(rt, entry + 0x24u);
        const std::uint32_t low = word(rt, entry + 0x20u);
        if (low == key_low && high == key_high) {
            ctx.gpr[2] = 1u;
            return;
        }
        const bool less = key_high < high || (key_high == high && key_low < low);
        if (order == (less ? 1u : 0u)) {
            ctx.gpr[2] = 1u;
            return;
        }
        put(rt, slot, entry_index);
        put(rt, slot + 4u, previous);
        slot -= 4u;
        if (position == step) {
            ctx.gpr[2] = low;
            return;
        }
    }
}
// func_08A14D6C(a0): func_08B2E924(a0), then a tail jump to func_08A14B14(a0). (The disassembly of this address runs on into
// the next function, which starts at 0x08A14D94; that one is not part of this entry.)
void func_08A14D6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t argument = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    call_set(rt, ctx, 0x08B2E924u, {argument});
    tail_call(ctx, 0x08A14B14u, {argument});
}
// func_08AFFA70(a, b, c, d, e): b and d are objects with a table pointer at +0xD8 whose count is the halfword at +0x7E
// (b's table gives the outer count, d's the inner one); 0 when b's count is not positive. For every outer index i and inner
// index j: func_08AFF7E0 fills the two work records (frame +0x30 / +0x110: d with b + 0x20 or b with d + 0x20), then
// func_08AFDA58 twice and func_08AFDCD8 twice combine them with the 4-bit field (word >> 8 & 0xF) of the tables' entries
// (0x200 bytes apart from +0x27C), threading the results through the frame (+0x10, +0x14) and the stack arguments; the
// last func_08AFDCD8 result is kept. When it is non-zero its halfword gets bit 0 set and func_08B012D8(it, a) runs. Returns
// whether the kept result is non-zero.
void func_08AFFA70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = ctx.gpr[4];
    const std::uint32_t b = ctx.gpr[5];
    const std::uint32_t c = ctx.gpr[6];
    const std::uint32_t d = ctx.gpr[7];
    const std::uint32_t e = ctx.gpr[8];
    GuestFrame frame(ctx, 0x240u);
    const std::uint32_t b_table = word(rt, b + 0xD8u);
    const std::uint32_t d_table = word(rt, d + 0xD8u);
    put(rt, frame.at(0x1F8u), b_table);
    put(rt, frame.at(0x1F4u), d_table);
    ctx.gpr[2] = 0xFFFFFFFEu;
    ctx.gpr[3] = 0xFFFFFFFFu;
    std::uint32_t kept = 0u;
    if (signed_of(half_u(rt, b_table + 0x7Eu)) > 0) {
        put(rt, frame.at(0x1FCu), d + 0x20u);
        put(rt, frame.at(0x200u), b + 0x20u);
        put(rt, frame.at(0x204u), b_table + 0x27Cu);
        for (std::uint32_t outer = 0u;;) {
            call_set(rt, ctx, 0x08AFF7E0u, {b, frame.at(0x30u), outer, d, word(rt, frame.at(0x1FCu)), frame.at(0x18u)});
            const std::uint32_t outer_field = (word(rt, word(rt, frame.at(0x204u))) >> 8) & 0xFu;
            put(rt, frame.at(0x1F0u), outer_field);
            const std::uint32_t inner_count = half_u(rt, word(rt, frame.at(0x1F4u)) + 0x7Eu);
            if (signed_of(inner_count) > 0) {
                std::uint32_t inner_entry = word(rt, frame.at(0x1F4u)) + 0x27Cu;
                for (std::uint32_t inner = 0u;;) {
                    call_set(rt, ctx, 0x08AFF7E0u, {d, frame.at(0x110u), inner, b, word(rt, frame.at(0x200u)), frame.at(0x24u)});
                    const std::uint32_t inner_field = (word(rt, inner_entry) >> 8) & 0xFu;
                    put(rt, frame.at(0x14u), 0u);
                    put(rt, frame.at(0x10u), 0u);
                    put(rt, frame.at(8u), kept);
                    inner_entry += 0x200u;
                    put(rt, frame.at(4u), inner);
                    put(rt, frame.at(0u), e);
                    put(rt, frame.at(0xCu), frame.at(0x14u));
                    const std::uint32_t first = call_set(rt, ctx, 0x08AFDA58u,
                                                         {b, a, c, outer, frame.at(0x110u), inner_field, frame.at(0x24u), d});
                    put(rt, frame.at(0xCu), frame.at(0x10u));
                    put(rt, frame.at(8u), first);
                    put(rt, frame.at(0u), c);
                    put(rt, frame.at(4u), outer);
                    const std::uint32_t second = call_set(rt, ctx, 0x08AFDA58u,
                                                          {d, a, e, inner, frame.at(0x30u), word(rt, frame.at(0x1F0u)), frame.at(0x18u), b});
                    put(rt, frame.at(4u), second);
                    put(rt, frame.at(8u), word(rt, frame.at(0x14u)));
                    put(rt, frame.at(0u), inner);
                    const std::uint32_t third = call_set(rt, ctx, 0x08AFDCD8u,
                                                         {b, a, c, outer, frame.at(0x110u), frame.at(0x24u), d, e});
                    put(rt, frame.at(4u), third);
                    put(rt, frame.at(8u), word(rt, frame.at(0x10u)));
                    put(rt, frame.at(0u), outer);
                    kept = call_set(rt, ctx, 0x08AFDCD8u, {d, a, e, inner, frame.at(0x30u), frame.at(0x18u), b, c});
                    ++inner;
                    if (!(signed_of(inner) < signed_of(half_u(rt, word(rt, frame.at(0x1F4u)) + 0x7Eu)))) break;
                }
            }
            ++outer;
            put(rt, frame.at(0x204u), word(rt, frame.at(0x204u)) + 0x200u);
            if (!(signed_of(outer) < signed_of(half_u(rt, word(rt, frame.at(0x1F8u)) + 0x7Eu)))) break;
        }
        if (kept != 0u) {
            put16(rt, kept, half_u(rt, kept) | 1u);
            call_set(rt, ctx, 0x08B012D8u, {kept, a});
            ctx.gpr[2] = 1u;
            return;
        }
    }
    ctx.gpr[2] = 0u;
}
// func_08AACAF8(self, key): returns self->+0x80 when the key string (a libstdc++ string slot) is empty or compares equal
// to the string func_08B85EB8(0x08CB3374) makes (length difference, func_08B36170 on the common length; that string
// released). Otherwise under the lock at self + 0x84 (func_08B344E0 / func_08B3457C) the map at self + 0x68 (end self + 0x6C)
// is searched with a copy of the key (func_08B88F44, hash func_08B29E88(0, text, length) at frame +0x14): func_08C34FE8
// finds an entry whose +0x18 is the result; else a 12-byte node (func_08A69520(0xC), key copy at +4, 0x1A at +8) is made and
// func_08C34770 (lower bound) / func_08C34C58 (insert at the hint, with the key copy as frame +0x18) put it into the map
// (the entry's +0x18 = the node; an existing entry with an equal hash and key gets the node directly). The key copy is
// released, the lock freed and the entry's node returned.
void func_08AACAF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t key = ctx.gpr[5];
    GuestFrame frame(ctx, 0x60u);
    constexpr std::uint32_t kEmptyRep = 0x08D74FD0u;
    if (word(rt, word(rt, key) - 0xCu) == 0u) {
        ctx.gpr[2] = word(rt, self + 0x80u);
        return;
    }
    call_set(rt, ctx, 0x08B85EB8u, {frame.at(4u), 0x08CB3374u, frame.at(3u)});
    const std::uint32_t text = word(rt, key);
    const std::uint32_t other = word(rt, frame.at(4u));
    const std::uint32_t text_length = word(rt, text - 0xCu);
    put(rt, frame.at(0xCu), text_length);
    const std::uint32_t other_rep = other - 0xCu;
    const std::uint32_t other_length = word(rt, other_rep);
    put(rt, frame.at(8u), other_length);
    std::uint32_t difference = text_length - other_length;
    const std::uint32_t shorter = word(rt, other_length < text_length ? frame.at(8u) : frame.at(0xCu));
    const std::uint32_t compared = call_set(rt, ctx, 0x08B36170u, {text, other, shorter});
    if (compared != 0u) difference = compared;
    if (other_rep != kEmptyRep) {
        if (!(signed_of(call_set(rt, ctx, 0x08B60804u, {other - 4u, 0xFFFFFFFFu})) > 0)) call_set(rt, ctx, 0x08B8542Cu, {other_rep, frame.at(2u)});
    }
    if (difference == 0u) {
        ctx.gpr[2] = word(rt, self + 0x80u);
        return;
    }
    const std::uint32_t lock = self + 0x84u;
    const std::uint32_t map = self + 0x68u;
    const std::uint32_t end = self + 0x6Cu;
    const std::uint32_t copy = frame.at(0x10u);
    const std::uint32_t hash = frame.at(0x14u);
    const std::uint32_t pending = frame.at(0x18u);
    call_set(rt, ctx, 0x08B344E0u, {lock});
    call_set(rt, ctx, 0x08B88F44u, {copy, key});
    const std::uint32_t copy_text = word(rt, key);
    put(rt, hash, call_set(rt, ctx, 0x08B29E88u, {0u, copy_text, word(rt, copy_text - 0xCu)}));
    const std::uint32_t found = call_set(rt, ctx, 0x08C34FE8u, {map, copy});
    std::uint32_t result;
    if (found != end) {
        result = word(rt, found + 0x18u);
    } else {
        const std::uint32_t memory = call_set(rt, ctx, 0x08A69520u, {0xCu});
        put(rt, memory, 0u);
        call_set(rt, ctx, 0x08B88F44u, {memory + 4u, key});
        result = memory;
        put(rt, result + 8u, 0x1Au);
        const std::uint32_t position = call_set(rt, ctx, 0x08C34770u, {map, copy});
        std::uint32_t entry = 0u;
        bool settled = false;
        bool insert = true;
        if (position != found) {
            const std::uint32_t entry_hash = word(rt, position + 0x14u);
            entry = position;
            if (word(rt, hash) < entry_hash) {
                insert = true;
            } else if (entry_hash < word(rt, hash)) {
                insert = false;
                settled = true;
            } else {
                const std::uint32_t copy_length = word(rt, word(rt, copy) - 0xCu);
                put(rt, frame.at(8u), copy_length);
                const std::uint32_t entry_length = word(rt, word(rt, position + 0x10u) - 0xCu);
                const std::uint32_t pick = entry_length < copy_length ? frame.at(0xCu) : frame.at(8u);
                put(rt, frame.at(0xCu), entry_length);
                std::int32_t order = static_cast<std::int32_t>(copy_length - entry_length);
                const std::uint32_t r = call_set(rt, ctx, 0x08B36170u, {word(rt, copy), word(rt, position + 0x10u), word(rt, pick)});
                if (r != 0u) order = static_cast<std::int32_t>(r);
                insert = order < 0;
                settled = !insert;
            }
        }
        if (settled) {
            put(rt, entry + 0x18u, result);
        } else {
            call_set(rt, ctx, 0x08B88F44u, {pending, copy});
            put(rt, frame.at(0x1Cu), word(rt, hash));
            put(rt, frame.at(0x20u), 0u);
            entry = call_set(rt, ctx, 0x08C34C58u, {map, position, pending});
            const std::uint32_t pending_text = word(rt, pending);
            const std::uint32_t pending_rep = pending_text - 0xCu;
            if (pending_rep != kEmptyRep) {
                if (!(signed_of(call_set(rt, ctx, 0x08B60804u, {pending_text - 4u, 0xFFFFFFFFu})) > 0))
                    call_set(rt, ctx, 0x08B8542Cu, {pending_rep, frame.at(1u)});
            }
            put(rt, entry + 0x18u, result);
        }
    }
    const std::uint32_t copy_final = word(rt, copy);
    const std::uint32_t copy_rep = copy_final - 0xCu;
    if (copy_rep != kEmptyRep) {
        if (!(signed_of(call_set(rt, ctx, 0x08B60804u, {copy_final - 4u, 0xFFFFFFFFu})) > 0)) call_set(rt, ctx, 0x08B8542Cu, {copy_rep, frame.at(0u)});
    }
    call_set(rt, ctx, 0x08B3457Cu, {lock});
    ctx.gpr[2] = result;
}
// func_088A66A4(self, a, b, c, d, e): mode = func_0893069C(b, c); a table of three 0x180-byte records at
// (self->+0)->+0x38 is walked, asking func_08932D40(record) and func_08932DA0(record) for each. Mode 0 stops at the first
// record where either answer equals d or e; modes 2 / 3 want both answers equal to d / e; other modes never stop. A stop
// (mode 0) rebuilds the three records from func_08932558 -> func_08934F78 and the pairs of func_0893389C / func_089338B0
// through func_089348D0; a mismatch in modes 2 / 3 calls func_088A5E54(self) and then re-keys each record with
// func_0892F95C((self->+0)->+0x4C0, 0x19 / 0x1A, key) results through func_089348D0, ending in a tail jump to
// func_088A5CD0(self). Walking all three records returns the last func_08932DA0 answer.
void func_088A66A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t d = ctx.gpr[8];
    const std::uint32_t e = ctx.gpr[9];
    GuestFrame frame(ctx, 0x40u);
    put(rt, frame.at(0u), self);
    const std::uint32_t mode = call_set(rt, ctx, 0x0893069Cu, {ctx.gpr[6], ctx.gpr[7]});
    const std::uint32_t records = word(rt, word(rt, frame.at(0u))) + 0x38u;
    std::uint32_t record = records;
    std::uint32_t walked = 0u;
    std::uint32_t key = 0u;
    std::uint32_t first = 0u;
    bool match = false;
    for (;;) {
        call_set(rt, ctx, 0x08932D40u, {record});
        first = ctx.gpr[2];
        const std::uint32_t second = call_set(rt, ctx, 0x08932DA0u, {record});
        ctx.gpr[3] = 2u;
        if (mode == 0u) {
            key = 0xFFFFFFFFu;
            if (first == d || first == e || second == d || second == e) {
                match = true;
                break;
            }
            ++walked;
        } else if (mode == 2u || mode == 3u) {
            const std::uint32_t wanted = mode == 2u ? d : e;
            key = d;
            if (first == wanted && second == wanted) {
                if (mode == 2u) first = 0xFFFFFFFFu;
                ++walked;
            } else {
                first = 0xFFFFFFFFu;
                ++walked;
                if (key != first) break;
            }
        } else {
            key = 0xFFFFFFFFu;
            first = 0xFFFFFFFFu;
            ++walked;
        }
        if (walked == 3u) return;
        record += 0x180u;
    }
    if (match) {
        record = records;
        call_set(rt, ctx, 0x088A6608u, {self});
        call_set(rt, ctx, 0x08932558u, {ctx.gpr[2]});
        call_set(rt, ctx, 0x08934F78u, {ctx.gpr[2], ctx.gpr[3]});
        call_set(rt, ctx, 0x0893389Cu, {});
        const std::uint32_t low_a = ctx.gpr[2];
        const std::uint32_t high_a = ctx.gpr[3];
        call_set(rt, ctx, 0x089338B0u, {});
        const std::uint32_t low_b = ctx.gpr[2];
        const std::uint32_t high_b = ctx.gpr[3];
        call_set(rt, ctx, 0x0893388Cu, {});
        for (std::uint32_t index = 0u; index != 3u; ++index, record += 0x180u) {
            call_set(rt, ctx, 0x089348D0u, {record, ctx.gpr[5], low_a, high_a});
            call_set(rt, ctx, 0x089348D0u, {record, ctx.gpr[5], low_b, high_b});
        }
        return;
    }
    const auto rekey = [&](std::uint32_t kind) {
        const std::uint32_t table = word(rt, word(rt, frame.at(0u)));
        return call_set(rt, ctx, 0x0892F95Cu, {word(rt, table + 0x4C0u), kind, key});
    };
    call_set(rt, ctx, 0x088A5E54u, {word(rt, frame.at(0u))});
    std::uint32_t low = rekey(0x19u);
    std::uint32_t high = ctx.gpr[3];
    const auto rewrite = [&]() {
        record = records;
        for (std::uint32_t index = 0u; index != 3u; ++index, record += 0x180u) call_set(rt, ctx, 0x089348D0u, {record, ctx.gpr[5], low, high});
    };
    if (!(first == low && high == 0xFFFFFFFFu)) rewrite();
    low = rekey(0x1Au);
    high = ctx.gpr[3];
    if (low != 0xFFFFFFFFu || high != 0xFFFFFFFFu) rewrite();
    tail_call(ctx, 0x088A5CD0u, {word(rt, frame.at(0u))});
}
// func_08BB6964(request): a first-fit allocator over a circular free list of {next, size-in-8-byte-units} blocks. The state
// block at 0x08D75F5C (list head +0x14, growth size +0x18, block vector +4 / +8 / +0xC, in use +0x1C / +0x20, peak +0x24)
// is set up on first use behind the guard byte 0x08D75F50 (func_08B6441C / func_08B64494 / func_08B77B1C). need =
// (request + 7) / 8 + 1 units; the first block big enough is split (the tail returned) or unlinked, the use counters
// updated and the block + 8 returned. Walking the whole ring without a fit grows the pool: a block of
// max(need, growth) units from func_08B2B4D8(0x10, bytes) (0 when that fails) is recorded in the vector (func_08BB13D4 when
// full), growth *= 1.5, and the block is put into the address-ordered ring, merged with the neighbours that touch it.
void func_08BB6964(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t request = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    constexpr std::uint32_t state = 0x08D75F5Cu;
    constexpr std::uint32_t guard = 0x08D75F50u;
    if (sext8(byte_u(rt, guard)) == 0u) {
        if (call_set(rt, ctx, 0x08B6441Cu, {guard}) != 0u) {
            put(rt, state + 0x18u, 0x1001u);
            put(rt, state + 0xCu, 0x08D75F68u);
            put(rt, state + 0x10u, 0u);
            put(rt, state + 4u, 0u);
            put(rt, state + 8u, 0u);
            put(rt, state + 0x14u, 0x08D75F68u);
            put(rt, state + 0x1Cu, 0u);
            put(rt, state + 0x20u, 0u);
            put(rt, state + 0x24u, 0u);
            put(rt, state, 0u);
            call_set(rt, ctx, 0x08B64494u, {guard});
            call_set(rt, ctx, 0x08B77B1Cu, {0x088B8234u});
        }
    }
    const std::uint32_t need = ((request + 7u) >> 3) + 1u;
    std::uint32_t previous = word(rt, state + 0x14u);
    std::uint32_t block = word(rt, previous);
    std::uint32_t size = word(rt, block + 4u);
    for (;;) {
        if (!(size < need)) {
            std::uint32_t chosen = block;
            if (need == size) {
                put(rt, previous, word(rt, block));
            } else {
                const std::uint32_t rest = size - need;
                put(rt, block + 4u, rest);
                chosen = block + (rest << 3);
                put(rt, chosen + 4u, need);
            }
            const std::uint32_t used = need + word(rt, state + 0x20u);
            const std::uint32_t peak = word(rt, state + 0x24u);
            put(rt, state + 0x14u, previous);
            put(rt, state + 0x20u, used);
            if (peak < used) put(rt, state + 0x24u, used);
            ctx.gpr[2] = chosen + 8u;
            return;
        }
        if (word(rt, state + 0x14u) != block) {
            previous = block;
            block = word(rt, previous);
            size = word(rt, block + 4u);
            continue;
        }
        std::uint32_t units = word(rt, state + 0x18u);
        if (!(need < units)) units = need;
        const std::uint32_t memory = call_set(rt, ctx, 0x08B2B4D8u, {0x10u, units << 3});
        put(rt, frame.at(0u), memory);
        if (memory == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
        const std::uint32_t growth = word(rt, state + 0x18u);
        std::uint32_t vector_end = word(rt, state + 4u);
        const std::uint32_t vector_capacity = word(rt, state + 8u);
        put(rt, state + 0x18u, growth + (growth >> 1));
        if (vector_end == vector_capacity) {
            call_set(rt, ctx, 0x08BB13D4u, {state, vector_end, frame.at(0u)});
        } else {
            if (vector_end != 0u) {
                put(rt, vector_end, memory);
                vector_end = word(rt, state + 4u);
            }
            put(rt, state + 4u, vector_end + 4u);
        }
        const std::uint32_t fresh = word(rt, frame.at(0u));
        put(rt, state + 0x1Cu, units + word(rt, state + 0x1Cu));
        put(rt, fresh + 4u, units);
        const std::uint32_t total = units + word(rt, state + 0x20u);
        put(rt, state + 0x20u, total);
        if (fresh + 8u != 0u) {
            std::uint32_t at = word(rt, state + 0x14u);
            for (;;) {
                const bool below = at < fresh;
                const std::uint32_t next = word(rt, at);
                if (below && fresh < next) break;
                const bool ascending = at < next;
                if (ascending) {
                    at = next;
                    continue;
                }
                if (below || fresh < next) break;
                at = next;
            }
            put(rt, state + 0x20u, total - word(rt, fresh + 4u));
            const std::uint32_t fresh_size = word(rt, fresh + 4u);
            const std::uint32_t after = word(rt, at);
            if (fresh + (fresh_size << 3) != after) {
                put(rt, fresh, after);
            } else {
                const std::uint32_t after_size = word(rt, after + 4u);
                const std::uint32_t after_next = word(rt, after);
                put(rt, fresh + 4u, fresh_size + after_size);
                put(rt, fresh, after_next);
            }
            const std::uint32_t at_size = word(rt, at + 4u);
            if (fresh != at + (at_size << 3)) {
                put(rt, at, fresh);
            } else {
                const std::uint32_t merged = word(rt, fresh + 4u);
                const std::uint32_t merged_next = word(rt, fresh);
                put(rt, at + 4u, at_size + merged);
                put(rt, at, merged_next);
            }
            put(rt, state + 0x14u, at);
        }
        previous = word(rt, state + 0x14u);
        if (previous == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
        block = word(rt, previous);
        size = word(rt, block + 4u);
    }
}
// The pool's free step (the allocator of func_08BB6964, state 0x08D75F5C): block `bp` (size in units at +4) is put back
// into the address-ordered ring starting at the head (+0x14), merged with the next / previous block when they touch, the
// in-use counter (+0x20) lowered and the head moved to the block it was inserted after. Leaves the last v0 of the code.
inline void pool_release(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t bp) {
    constexpr std::uint32_t state = 0x08D75F5Cu;
    std::uint32_t at = word(rt, state + 0x14u);
    for (;;) {
        const bool below = at < bp;
        const std::uint32_t next = word(rt, at);
        if (below && bp < next) break;
        if (at < next) {
            at = next;
            continue;
        }
        if (below || bp < next) break;
        at = next;
    }
    put(rt, state + 0x20u, word(rt, state + 0x20u) - word(rt, bp + 4u));
    const std::uint32_t size = word(rt, bp + 4u);
    const std::uint32_t after = word(rt, at);
    std::uint32_t v0 = bp + (size << 3);
    if (v0 != after) {
        put(rt, bp, after);
    } else {
        v0 = size + word(rt, after + 4u);
        put(rt, bp + 4u, v0);
        put(rt, bp, word(rt, after));
    }
    const std::uint32_t at_size = word(rt, at + 4u);
    v0 = (at_size << 3) + at;
    if (bp != v0) {
        put(rt, at, bp);
    } else {
        v0 = at_size + word(rt, bp + 4u);
        put(rt, at + 4u, v0);
        put(rt, at, word(rt, bp));
    }
    put(rt, state + 0x14u, at);
    ctx.gpr[2] = v0;
}

// The pool's first-use set-up behind the guard byte 0x08D75F50 (as in func_08BB6964).
inline void pool_setup(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t state = 0x08D75F5Cu;
    constexpr std::uint32_t guard = 0x08D75F50u;
    if (sext8(byte_u(rt, guard)) != 0u) return;
    if (call_set(rt, ctx, 0x08B6441Cu, {guard}) == 0u) return;
    put(rt, state + 0x18u, 0x1001u);
    put(rt, state, 0u);
    put(rt, state + 4u, 0u);
    put(rt, state + 8u, 0u);
    put(rt, state + 0x14u, 0x08D75F68u);
    put(rt, state + 0x1Cu, 0u);
    put(rt, state + 0x20u, 0u);
    put(rt, state + 0x24u, 0u);
    put(rt, state + 0xCu, 0x08D75F68u);
    put(rt, state + 0x10u, 0u);
    call_set(rt, ctx, 0x08B64494u, {guard});
    call_set(rt, ctx, 0x08B77B1Cu, {0x088B8234u});
}

// Puts `item` (when non-null) on the free chain of the size-class record that func_08BB1F48()'s table gives for `key`
// (looked up, or made with func_08BB1D0C(+0x24, +0x28, &key, 0x08BB1B40) when missing or of another class): the record's
// +0x1C count goes down and the chain head (+0xC) takes the item.
inline void return_to_class(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t key, std::uint32_t item, std::uint32_t key_slot) {
    const std::uint32_t context = call_set(rt, ctx, 0x08BB1F48u, {});
    put(rt, key_slot, key);
    std::uint32_t record = word(rt, context + 0x34u);
    if (record == 0u || word(rt, record + 0x10u) != key) {
        const std::uint32_t found = call_set(rt, ctx, 0x08BB1D0Cu, {word(rt, context + 0x24u), word(rt, context + 0x28u), key_slot, 0x08BB1B40u});
        record = word(rt, found);
        put(rt, context + 0x34u, record);
    }
    if (item != 0u) {
        const std::uint32_t head = word(rt, record + 0xCu);
        put(rt, record + 0x1Cu, word(rt, record + 0x1Cu) - 1u);
        put(rt, item, head);
        put(rt, record + 0xCu, item);
    }
}

// func_08BB2CFC(tree, node): the recursive teardown of a binary tree's nodes: the right subtree (node->+0xC) first by
// recursion, then the payload node->+0x10 (use count at +0x10 lowered; on zero its chain word +0xC goes back to the size
// class 4 pool, func_08BB1FF0(payload) runs with +0xC cleared, and the payload itself goes to the size class 0x14 pool);
// the node's own block (node - 8) is freed into the pool ring (set up on first use); the left subtree (node->+8) loops.
void func_08BB2CFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t tree = ctx.gpr[4];
    std::uint32_t node = ctx.gpr[5];
    GuestFrame frame(ctx, 0x40u);
    if (node == 0u) return;
    for (;;) {
        call_set(rt, ctx, 0x08BB2CFCu, {tree, word(rt, node + 0xCu)});
        const std::uint32_t payload = word(rt, node + 0x10u);
        const std::uint32_t left = word(rt, node + 8u);
        if (payload != 0u) {
            const std::uint32_t count = word(rt, payload + 0x10u) - 1u;
            put(rt, payload + 0x10u, count);
            if (count == 0u) {
                const std::uint32_t chain = word(rt, payload + 0xCu);
                return_to_class(rt, ctx, 4u, chain, frame.at(0u));
                put(rt, payload + 0xCu, 0u);
                call_set(rt, ctx, 0x08BB1FF0u, {payload});
                return_to_class(rt, ctx, 0x14u, payload, frame.at(0u));
            }
        }
        pool_setup(rt, ctx);
        pool_release(rt, ctx, node - 8u);
        if (left == 0u) return;
        node = left;
    }
}
// func_089E1C90(self, flag): three value objects are copied from self + 0x20C / +0x214 / +0x21C into frame +0x18 / +0x1C /
// +0x20 (func_089E4C24). For each of the three strings at self + 0x1A4 / +0x1A8 / +0x1AC, when `flag` is set or the copied
// object's first word is zero: an empty string gives a fresh value (func_089E4C14(temp, 0x08CF6ED8)), a non-empty one
// func_089A50A0(temp, func_089BD4DC(self), &string, bit 3, bit 2, bit 1 of self->+0x1B0); the result is assigned to the
// copy. Then under the lock 0x08CF55FC the three copies are assigned back (skipped when they are the same objects),
// self->+0x224 = 1, and the copies are destroyed.
void func_089E1C90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t flag = ctx.gpr[5] & 0xFFu;
    GuestFrame frame(ctx, 0x60u);
    const std::uint32_t first = frame.at(0x18u);
    const std::uint32_t second = frame.at(0x1Cu);
    const std::uint32_t third = frame.at(0x20u);
    const std::uint32_t lock = 0x08CF55FCu;
    call_set(rt, ctx, 0x089E4C24u, {first, self + 0x20Cu});
    call_set(rt, ctx, 0x089E4C24u, {second, self + 0x214u});
    call_set(rt, ctx, 0x089E4C24u, {third, self + 0x21Cu});
    const auto rebuild = [&](std::uint32_t copy, std::uint32_t string_slot, std::uint32_t full_temporary, std::uint32_t empty_temporary) {
        if (flag == 0u && word(rt, copy) != 0u) return;
        if (word(rt, word(rt, string_slot) - 0xCu) == 0u) {
            call_set(rt, ctx, 0x089E4C14u, {empty_temporary, 0x08CF6ED8u});
            call_set(rt, ctx, kAssign, {copy, word(rt, empty_temporary)});
            call_set(rt, ctx, kDestroy, {empty_temporary});
            return;
        }
        const std::uint32_t owner = call_set(rt, ctx, 0x089BD4DCu, {self});
        const std::uint32_t bits = word(rt, self + 0x1B0u);
        call_set(rt, ctx, 0x089A50A0u, {full_temporary, owner, string_slot, (bits >> 3) & 1u, (bits >> 2) & 1u, (bits >> 1) & 1u});
        call_set(rt, ctx, kAssign, {copy, word(rt, full_temporary)});
        call_set(rt, ctx, kDestroy, {full_temporary});
    };
    rebuild(first, self + 0x1A4u, frame.at(0x14u), frame.at(0x10u));
    rebuild(second, self + 0x1A8u, frame.at(0xCu), frame.at(8u));
    rebuild(third, self + 0x1ACu, frame.at(4u), frame.at(0u));
    call_set(rt, ctx, kLock, {lock});
    if (self + 0x20Cu != first) call_set(rt, ctx, kAssign, {self + 0x20Cu, word(rt, first)});
    if (self + 0x214u != second) call_set(rt, ctx, kAssign, {self + 0x214u, word(rt, second)});
    if (self + 0x21Cu != third) call_set(rt, ctx, kAssign, {self + 0x21Cu, word(rt, third)});
    put(rt, self + 0x224u, 1u);
    call_set(rt, ctx, kUnlock, {lock});
    call_set(rt, ctx, kDestroy, {third});
    call_set(rt, ctx, kDestroy, {second});
    call_set(rt, ctx, kDestroy, {first});
}
// func_08940610(self; f12 = scale): s = min(table->+0x1360, scale), with table = self->+0xC and state = self->+8. Clears
// six words at state +0x6E4..+0x6F8, fills three floats at state +0x6CC from state->+0x1C.. times s, copies the float at
// +0x704 to +0x6D8 and s to +0x6DC, then func_0894020C(self; s), func_0895DCAC(self, func_0895DD80(self, 0; s)). The float
// at +0x590 = s * table->+0x2F0[byte state->+0x5F8] (1.0 at +0x5F0, func_0895BBD8(table + 0x270, state + 0x590, state +
// 0x5F0)) is clamped between table halfwords +0x23E and +0x23C times 0.10472 and converted to a halfword angle at +0x58E
// (x 60 / 6.2831855). With table->+0x26C zero a flag byte +0x5F9 = 1 is all; otherwise +0x588 (and +0x58A when
// table->+0x258 > 0) get the angle and func_0894C240(table + 0x170, angle, angle, frame) gives the float at +0x5A4 beside
// constants at +0x584 / +0x59C / +0x5A0 / +0x58C. Then the four records func_08955300(self, i) get s at +0x30 / +0x34,
// two floats at +0x5FC / +0x600 are s * table-float (+0xB0 + 4k, or by the index byte +2 when the byte +4 is set), and
// func_08954AEC runs when the byte table->+0x52 is 8; last func_08957B30(self), func_0895F41C(self).
void func_08940610(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const float argument = ctx.fpr[12];
    const float saved20 = ctx.fpr[20];
    const float saved21 = ctx.fpr[21];
    GuestFrame frame(ctx, 0x40u);
    const std::uint32_t table = word(rt, self + 0xCu);
    float scale = float_at(rt, table + 0x1360u);
    ctx.fpr[20] = scale;
    const std::uint32_t state = word(rt, self + 8u);
    if (!(scale < argument)) {
        scale = argument;
        ctx.fpr[20] = scale;
    }
    put(rt, state + 0x6F8u, 0u);
    put(rt, state + 0x6F0u, 0u);
    put(rt, state + 0x6F4u, 0u);
    put(rt, state + 0x6E4u, 0u);
    put(rt, state + 0x6E8u, 0u);
    put(rt, state + 0x6ECu, 0u);
    for (std::uint32_t index = 0u; index < 3u; ++index) put_float(rt, state + 0x6CCu + index * 4u, psp_mul(scale, float_at(rt, state + 0x1Cu + index * 4u)));
    const std::uint32_t sink = table + 0x270u;
    put(rt, state + 0x6D8u, word(rt, state + 0x704u));
    ctx.fpr[12] = scale;
    put_float(rt, state + 0x6DCu, scale);
    call_setf(rt, ctx, 0x0894020Cu, {self}, {scale});
    ctx.fpr[12] = scale;
    const std::uint32_t handle = call_setf(rt, ctx, 0x0895DD80u, {self, 0u}, {scale}).v0;
    call_set(rt, ctx, 0x0895DCACu, {self, handle});
    const float one = 1.0f;
    const std::uint32_t kind = byte_u(rt, state + 0x5F8u);
    const float table_value = float_at(rt, table + 0x2F0u + kind * 4u);
    put_float(rt, state + 0x5F0u, one);
    float angle = psp_mul(scale, table_value);
    put_float(rt, state + 0x590u, angle);
    call_set(rt, ctx, 0x0895BBD8u, {sink, state + 0x590u, state + 0x5F0u});
    const float unit = std::bit_cast<float>(0x3DD67750u);
    angle = float_at(rt, state + 0x590u);
    const float minimum = psp_mul(static_cast<float>(signed_of(half_u(rt, table + 0x23Eu))), unit);
    if (angle < minimum) {
        angle = minimum;
        put_float(rt, state + 0x590u, minimum);
    } else {
        const float maximum = psp_mul(static_cast<float>(signed_of(half_u(rt, table + 0x23Cu))), unit);
        if (maximum < angle) {
            angle = maximum;
            put_float(rt, state + 0x590u, maximum);
        }
    }
    const float sixty = std::bit_cast<float>(0x42700000u);
    const float turn = std::bit_cast<float>(0x40C90FDBu);
    put8(rt, state + 0x57Au, 0u);
    const std::uint32_t degrees = ctx.fpu_float_to_word(psp_mul(angle, sixty) / turn, 1u);
    put16(rt, state + 0x58Eu, degrees);
    ctx.gpr[2] = degrees;
    if (float_at(rt, table + 0x26Cu) == 0.0f) {
        ctx.gpr[2] = 1u;
    } else {
        put16(rt, state + 0x588u, degrees);
        if (0.0f < float_at(rt, table + 0x258u)) put16(rt, state + 0x58Au, degrees);
        const std::uint32_t signed_angle = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(half_u(rt, state + 0x58Eu))));
        ctx.gpr[4] = table + 0x170u;
        ctx.gpr[5] = signed_angle;
        ctx.gpr[6] = signed_angle;
        ctx.gpr[7] = frame.at(0u);
        const GuestResult curve = call_keep(rt, ctx, 0x0894C240u, {table + 0x170u, signed_angle, signed_angle, frame.at(0u)}, {});
        const std::uint32_t angle_half = half_u(rt, state + 0x58Eu);
        put_float(rt, state + 0x5A4u, curve.f0);
        put_float(rt, state + 0x584u, one);
        put16(rt, state + 0x58Cu, angle_half);
        put_float(rt, state + 0x5A0u, 0.0f);
        put_float(rt, state + 0x59Cu, one);
        put8(rt, state + 0x57Bu, 0u);
        ctx.gpr[2] = 1u;
    }
    put8(rt, state + 0x5F9u, ctx.gpr[2]);
    put_float(rt, state + 0x5ECu, one);
    put8(rt, state + 0x732u, 0xFFFFFFFFu);
    put_float(rt, state + 0x6C4u, scale);
    put(rt, state + 0x6C8u, 0u);
    put8(rt, state + 0x733u, 0u);
    for (std::uint32_t index = 0u; index < 4u; ++index) {
        const std::uint32_t record = call_set(rt, ctx, 0x08955300u, {self, index});
        put_float(rt, record + 0x30u, scale);
        put_float(rt, record + 0x34u, scale);
    }
    for (std::uint32_t step = 0u; step < 2u; ++step) {
        float value = float_at(rt, sink + 0xB0u + step * 4u);
        if (byte_u(rt, sink + 4u) != 0u) value = float_at(rt, sink + byte_u(rt, sink + 2u) * 4u + 0xB0u);
        put_float(rt, state + 0x5FCu + step * 4u, psp_mul(scale, value));
    }
    if (byte_u(rt, table + 0x52u) == 8u) call_set(rt, ctx, 0x08954AECu, {self});
    call_set(rt, ctx, 0x08957B30u, {self});
    call_set(rt, ctx, 0x0895F41Cu, {self});
    ctx.fpr[20] = saved20;
    ctx.fpr[21] = saved21;
}
// func_08A993B4(self, list, a2, holder): holder->+0's virtual slot 0x78 decides (objects checked against 0x08CB2328).
// Non-zero: n = self's slot 0x7C and m = holder->+0's slot 0x7C; func_08A98C1C(temp, n + m) sizes the list value (assigned to
// `list`, temp destroyed); for each i < n the list->+0 object's slot 0x88(obj, i, temp') runs with temp' built by
// self's slot 0x84(temp', self, i), temp' destroyed; then for each i < m (when m was non-zero) the same with the holder
// object building (slot 0x84(temp', holder obj, i)) and the list object's slot 0x88(obj, n + i, temp'). Zero: n =
// self's slot 0x7C, func_08A98C1C(temp, n + 1) sizes the list, the first loop runs over n, and the list object's slot
// 0x88(obj, n, holder) ends it.
void func_08A993B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t list = ctx.gpr[5];
    const std::uint32_t holder = ctx.gpr[7];
    constexpr std::uint32_t message = 0x08CB2328u;
    GuestFrame frame(ctx, 0x60u);
    put(rt, frame.at(0x20u), holder);
    std::uint32_t object = checked(rt, ctx, word(rt, holder), message);
    if (call_virtual_at(rt, ctx, object, 0x78u, {object}) != 0u) {
        const std::uint32_t own_count = call_virtual_at(rt, ctx, self, 0x7Cu, {self});
        object = checked(rt, ctx, word(rt, word(rt, frame.at(0x20u))), message);
        const std::uint32_t other_count = call_virtual_at(rt, ctx, object, 0x7Cu, {object});
        const std::uint32_t temp = frame.at(0x10u);
        call_set(rt, ctx, 0x08A98C1Cu, {temp, own_count + other_count});
        if (list != temp) call_set(rt, ctx, kAssign, {list, word(rt, temp)});
        call_set(rt, ctx, kDestroy, {temp});
        const std::uint32_t scratch = frame.at(0xCu);
        if (own_count != 0u) {
            std::uint32_t index = 0u;
            do {
                const std::uint32_t target = checked(rt, ctx, word(rt, list), message);
                const std::uint32_t store = word(rt, word(rt, target) + 0x88u);
                call_virtual_at(rt, ctx, self, 0x84u, {scratch, self, index});
                call_set(rt, ctx, store, {target, index, scratch});
                ++index;
                call_set(rt, ctx, kDestroy, {scratch});
            } while (own_count != index);
        }
        if (other_count != 0u) {
            const std::uint32_t built = frame.at(8u);
            std::uint32_t index = 0u;
            do {
                const std::uint32_t target = checked(rt, ctx, word(rt, list), message);
                const std::uint32_t base = index + own_count;
                const std::uint32_t source = word(rt, word(rt, frame.at(0x20u)));
                const std::uint32_t store = word(rt, word(rt, target) + 0x88u);
                const std::uint32_t source_object = checked(rt, ctx, source, message);
                call_virtual_at(rt, ctx, source_object, 0x84u, {built, source_object, index});
                ++index;
                call_set(rt, ctx, store, {target, base, built});
                call_set(rt, ctx, kDestroy, {built});
            } while (other_count != index);
        }
        return;
    }
    const std::uint32_t own_count = call_virtual_at(rt, ctx, self, 0x7Cu, {self});
    const std::uint32_t temp = frame.at(4u);
    call_set(rt, ctx, 0x08A98C1Cu, {temp, own_count + 1u});
    if (list != temp) call_set(rt, ctx, kAssign, {list, word(rt, temp)});
    call_set(rt, ctx, kDestroy, {temp});
    if (own_count != 0u) {
        const std::uint32_t scratch = frame.at(0u);
        std::uint32_t index = 0u;
        do {
            const std::uint32_t target = checked(rt, ctx, word(rt, list), message);
            const std::uint32_t store = word(rt, word(rt, target) + 0x88u);
            call_virtual_at(rt, ctx, self, 0x84u, {scratch, self, index});
            call_set(rt, ctx, store, {target, index, scratch});
            ++index;
            call_set(rt, ctx, kDestroy, {scratch});
        } while (own_count != index);
    }
    object = checked(rt, ctx, word(rt, list), message);
    call_virtual_at(rt, ctx, object, 0x88u, {object, own_count, word(rt, frame.at(0x20u))});
}
// func_08A1AC08(self): on first use (guard byte 0x08CF63E0) the eight words at 0x08CF63E8 are filled with the results of
// func_08A2A110, func_08A2379C, func_08A21E28, func_08A214C8, func_08A20814, func_08A1CAD0, func_08A1C0B0 and
// func_08A1B2DC. While the table words are non-zero, func_08A7CC30(self->+0, word) runs for each. Then eight times: a
// value object is built in the frame (func_08A2982C, func_08A29588, func_08A1AEDC, func_08A278B0, func_08A270AC,
// func_08A25498, func_08A26B98, func_08A20010 at frame +0x1C .. +0), handed to func_08A7CE50(self->+0, object) (the
// target checked against 0x08CA9E50) and destroyed. Last func_08A2A724, func_08A2A824, func_08A2A5F8, func_08A2A950 and
// func_08A1ED68 on self.
void func_08A1AC08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x40u);
    constexpr std::uint32_t guard = 0x08CF63E0u;
    constexpr std::uint32_t table = 0x08CF63E8u;
    constexpr std::uint32_t message = 0x08CA9E50u;
    if (sext8(byte_u(rt, guard)) == 0u && call_set(rt, ctx, 0x08B6441Cu, {guard}) != 0u) {
        static constexpr std::uint32_t kBuilders[8] = {0x08A2A110u, 0x08A2379Cu, 0x08A21E28u, 0x08A214C8u,
                                                       0x08A20814u, 0x08A1CAD0u, 0x08A1C0B0u, 0x08A1B2DCu};
        for (std::uint32_t index = 0u; index < 8u; ++index) put(rt, table + index * 4u, call_set(rt, ctx, kBuilders[index], {}));
        call_set(rt, ctx, 0x08B64494u, {guard});
    }
    ctx.gpr[2] = word(rt, table);
    if (ctx.gpr[2] != 0u) {
        std::uint32_t entry = table;
        do {
            const std::uint32_t target = checked(rt, ctx, word(rt, self), message);
            call_set(rt, ctx, 0x08A7CC30u, {target, word(rt, entry)});
            entry += 4u;
        } while (word(rt, entry) != 0u);
    }
    static constexpr std::uint32_t kMakers[8] = {0x08A2982Cu, 0x08A29588u, 0x08A1AEDCu, 0x08A278B0u,
                                                 0x08A270ACu, 0x08A25498u, 0x08A26B98u, 0x08A20010u};
    for (std::uint32_t index = 0u; index < 8u; ++index) {
        const std::uint32_t temporary = frame.at(0x1Cu - index * 4u);
        const std::uint32_t target = checked(rt, ctx, word(rt, self), message);
        call_set(rt, ctx, kMakers[index], {temporary});
        call_set(rt, ctx, 0x08A7CE50u, {target, temporary});
        call_set(rt, ctx, kDestroy, {temporary});
    }
    call_set(rt, ctx, 0x08A2A724u, {self});
    call_set(rt, ctx, 0x08A2A824u, {self});
    call_set(rt, ctx, 0x08A2A5F8u, {self});
    call_set(rt, ctx, 0x08A2A950u, {self});
    call_set(rt, ctx, 0x08A1ED68u, {self});
}

// Copies one 32-byte element: the seven words at +0..+0x18 and the byte at +0x1C.
inline void copy_element(psprecomp::Runtime &rt, std::uint32_t destination, std::uint32_t source) {
    for (std::uint32_t offset = 0u; offset < 0x1Cu; offset += 4u) put(rt, destination + offset, word(rt, source + offset));
    put8(rt, destination + 0x1Cu, byte_u(rt, source + 0x1Cu));
}

// func_08BAC714(vector, position, count, value): vector::insert(position, count, value) over 32-byte elements
// {begin +0, end +4, capacity end +8}. Nothing for count 0. With room (capacity - end >= count) the value is copied to
// a frame temporary (+0xC); when more than `count` elements follow the position the last `count` move up (func_08BAC354),
// the middle moves up backwards and func_08BAC3C0(position, position + count, temp) fills; otherwise func_08BAC688 fills
// the overflow, func_08BAC618 moves the tail and func_08BAC3C0 fills the rest. Without room a block of size + max(size,
// count) elements (func_08B5ADDC(0x08CA08A4) when over the maximum 0x7FFFFFF; func_08B35DA0 for the bytes) takes the
// prefix (func_08BAC53C), the new elements (func_08BAC5AC) and the suffix (func_08BAC53C); the old range is destroyed
// (func_08BAC238), freed (func_08B35254) and the three pointers replaced.
void func_08BAC714(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t vector = ctx.gpr[4];
    const std::uint32_t position = ctx.gpr[5];
    const std::uint32_t count = ctx.gpr[6];
    const std::uint32_t value = ctx.gpr[7];
    GuestFrame frame(ctx, 0x60u);
    put(rt, frame.at(0x30u), count);
    if (count == 0u) return;
    const std::uint32_t end = word(rt, vector + 4u);
    const std::uint32_t temporary = frame.at(0xCu);
    if (!((static_cast<std::uint32_t>(signed_of(word(rt, vector + 8u) - end) >> 5)) < count)) {
        copy_element(rt, temporary, value);
        const std::uint32_t after = static_cast<std::uint32_t>(signed_of(end - position) >> 5);
        if (count < after) {
            call_set(rt, ctx, 0x08BAC354u, {end - (count << 5), end, end, frame.at(6u)});
            put(rt, vector + 4u, word(rt, vector + 4u) + (count << 5));
            const std::uint32_t middle_end = end - (count << 5);
            const std::int32_t middle = signed_of(middle_end - position) >> 5;
            std::uint32_t source = middle_end;
            std::uint32_t destination = end;
            for (std::int32_t step = 0; step < middle; ++step) {
                source -= 0x20u;
                destination -= 0x20u;
                copy_element(rt, destination, source);
            }
            call_set(rt, ctx, 0x08BAC3C0u, {position, position + (count << 5), temporary});
            return;
        }
        call_set(rt, ctx, 0x08BAC688u, {end, count - after, temporary, frame.at(5u)});
        const std::uint32_t new_end = word(rt, vector + 4u) + ((count - after) << 5);
        put(rt, vector + 4u, new_end);
        call_set(rt, ctx, 0x08BAC618u, {position, end, new_end, frame.at(4u)});
        put(rt, vector + 4u, word(rt, vector + 4u) + (after << 5));
        call_set(rt, ctx, 0x08BAC3C0u, {position, end, temporary});
        return;
    }
    const std::uint32_t begin = word(rt, vector);
    const std::uint32_t size = static_cast<std::uint32_t>(signed_of(end - begin) >> 5);
    put(rt, frame.at(8u), size);
    if (0x07FFFFFFu - size < count) call_set(rt, ctx, 0x08B5ADDCu, {0x08CA08A4u});
    const std::uint32_t larger = size < count ? count : size;
    const std::uint32_t capacity = size + larger;
    std::uint32_t bytes;
    std::uint32_t block = 0u;
    if (capacity < size) bytes = 0xFFFFFFE0u;
    else bytes = capacity == 0u ? 0u : capacity << 5;
    if (capacity >= size && capacity == 0u) {
        block = 0u;
    } else {
        block = call_set(rt, ctx, 0x08B35DA0u, {bytes});
    }
    const std::uint32_t old_begin = word(rt, vector);
    std::uint32_t copied = call_set(rt, ctx, 0x08BAC53Cu, {old_begin, position, block, frame.at(3u)});
    const std::uint32_t filled = call_set(rt, ctx, 0x08BAC5ACu, {copied, word(rt, frame.at(0x30u)), value, frame.at(2u)});
    (void)filled;
    const std::uint32_t tail_target = (word(rt, frame.at(0x30u)) << 5) + copied;
    const std::uint32_t new_end = call_set(rt, ctx, 0x08BAC53Cu, {position, word(rt, vector + 4u), tail_target, frame.at(1u)});
    call_set(rt, ctx, 0x08BAC238u, {word(rt, vector), word(rt, vector + 4u), frame.at(0u)});
    if (word(rt, vector) != 0u) call_set(rt, ctx, 0x08B35254u, {word(rt, vector)});
    ctx.gpr[2] = block + bytes;
    put(rt, vector + 8u, block + bytes);
    put(rt, vector + 4u, new_end);
    put(rt, vector, block);
}
// func_0889E258(self, source): source is a shared pointer {pointer +0, control block +4}. Its use count is raised twice
// (copies for the node and for the slot) and holder = self->+0x24 is locked (func_08B34888 / func_08B34B24). With
// holder->+0x2C zero func_0889DE18(self) runs; else a 16-byte node (func_08B62344(0x10): vtable 0x08CC2530, self, pointer,
// block; one more use count) replaces holder->+0x34 (the old object's virtual slot 0 first, then cleared) and holder->+0x38
// gets the static object at 0x08D75F10 (set up on first use behind the guard byte 0x08D75F08, vtable 0x08CC2560); the
// copy's use count is released. After the unlock the two remaining uses of the block are released, the very last
// weak release being a tail jump to the control block's virtual slot 0xC.
void func_0889E258(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t source = ctx.gpr[5];
    const std::uint32_t block = word(rt, source + 4u);
    const std::uint32_t pointer = word(rt, source);
    if (block != 0u) count_up(rt, ctx, block + 4u);
    const std::uint32_t holder = word(rt, self + 0x24u);
    if (block != 0u) count_up(rt, ctx, block + 4u);
    call_set(rt, ctx, 0x08B34888u, {holder});
    if (word(rt, holder + 0x2Cu) == 0u) {
        call_set(rt, ctx, 0x0889DE18u, {self});
    } else {
        const std::uint32_t slot = holder + 0x34u;
        if (block != 0u) count_up(rt, ctx, block + 4u);
        const std::uint32_t node = call_set(rt, ctx, 0x08B62344u, {0x10u});
        put(rt, node, 0x08CC2530u);
        put(rt, node + 4u, self);
        put(rt, node + 8u, pointer);
        put(rt, node + 0xCu, block);
        if (block != 0u) count_up(rt, ctx, block + 4u);
        if (word(rt, slot) != node) {
            const std::uint32_t previous = word(rt, slot + 4u);
            if (previous == 0u) {
                put(rt, slot, node);
            } else {
                ctx.gpr[3] = word(rt, previous);
                ctx.gpr[2] = word(rt, ctx.gpr[3]);
                call_set(rt, ctx, ctx.gpr[2], {previous});
                put(rt, slot + 4u, 0u);
                put(rt, slot, 0u);
                put(rt, slot, node);
            }
            constexpr std::uint32_t guard = 0x08D75F08u;
            constexpr std::uint32_t object = 0x08D75F10u;
            if (sext8(byte_u(rt, guard)) == 0u && call_set(rt, ctx, 0x08B6441Cu, {guard}) != 0u) {
                put(rt, object, 0x08CC2560u);
                call_set(rt, ctx, 0x08B64494u, {guard});
            }
            put(rt, slot + 4u, object);
        }
        if (block != 0u) release_shared(rt, ctx, block);
    }
    call_set(rt, ctx, 0x08B34B24u, {holder});
    if (block == 0u) return;
    if (count_down(rt, ctx, block + 4u)) {
        call_virtual_at(rt, ctx, block, 8u, {block});
        if (count_down(rt, ctx, block + 8u)) call_virtual_at(rt, ctx, block, 0xCu, {block});
    }
    if (!count_down(rt, ctx, block + 4u)) return;
    call_virtual_at(rt, ctx, block, 8u, {block});
    if (!count_down(rt, ctx, block + 8u)) return;
    ctx.gpr[2] = word(rt, block);
    ctx.gpr[25] = word(rt, ctx.gpr[2] + 0xCu);
    tail_call(ctx, ctx.gpr[25], {block});
}
// func_089348D0(self, a1, low, high): the kind k = func_0893656C(func_089363FC(self), a1, low, high) picks one case of a
// 32-entry table (0 and kinds of 32 and above do nothing; the others return the table slot address 0x08C8B268 + 4k, kinds of 32 and above 0): most store the (low, high) pair into two
// adjacent words of self (case 1 at +0x18, 2 at +0x20, 3 at +0x48, ... 28 at +0x100; offsets below), several then tail
// jump to an update function with self (cases 2, 3, 4, 5, 7, 10, 14, 24, 27 and 11 after func_089338C4(self)); case 31 sets
// the byte self->+0x15E and returns 1.
void func_089348D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t low = ctx.gpr[6];
    const std::uint32_t high = ctx.gpr[7];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t handle = call_set(rt, ctx, 0x089363FCu, {self});
    const std::uint32_t kind = call_set(rt, ctx, 0x0893656Cu, {handle, ctx.gpr[5], low, high});
    struct Case {
        std::uint16_t first;
        std::uint16_t second;
        std::uint32_t tail;
    };
    static constexpr Case kCases[32] = {
        {0, 0, 0},           {0x18, 0x1C, 0},     {0x20, 0x24, 0x08933A64u}, {0x48, 0x4C, 0x08933C5Cu},
        {0xD8, 0xDC, 0x08933FB8u}, {0xE0, 0xE4, 0x08934090u}, {0x28, 0x2C, 0}, {0x78, 0x7C, 0x08933B0Cu},
        {0x70, 0x74, 0},     {0x68, 0x6C, 0},     {0x38, 0x3C, 0x089339D4u}, {0x40, 0x44, 0x0892ED70u},
        {0x30, 0x34, 0},     {0xA0, 0xA4, 0},     {0xA8, 0xAC, 0x08933BB4u}, {0x80, 0x84, 0},
        {0x88, 0x8C, 0},     {0x90, 0x94, 0},     {0x98, 0x9C, 0},     {0xD0, 0xD4, 0},
        {0xC8, 0xCC, 0},     {0xB8, 0xBC, 0},     {0xB0, 0xB4, 0},     {0xC0, 0xC4, 0},
        {0x50, 0x54, 0x08933EB0u}, {0x58, 0x5C, 0}, {0x60, 0x64, 0},   {0xF0, 0xF4, 0x089341A0u},
        {0x100, 0x104, 0},   {0xE8, 0xEC, 0},     {0xF8, 0xFC, 0},     {0, 0, 0}};
    ctx.gpr[2] = kind < 0x20u ? 0x08C8B268u + kind * 4u : 0u;
    if (kind >= 0x20u || kind == 0u) return;
    if (kind == 31u) {
        put8(rt, self + 0x15Eu, 1u);
        ctx.gpr[2] = 1u;
        return;
    }
    const Case &entry = kCases[kind];
    put(rt, self + entry.first, low);
    put(rt, self + entry.second, high);
    if (kind == 11u) call_set(rt, ctx, 0x089338C4u, {self});
    if (entry.tail != 0u) tail_call(ctx, entry.tail, {self});
}
// func_08A2C2B4(self): the same shape as func_08A1AC08 with another table: on first use (guard byte 0x08CF65A0) the 19
// words at 0x08CF65B8 are filled with the results of 19 builder functions (func_08A443E0 .. func_08A36D2C, in the order of
// the code); while the table words are non-zero func_08A7CC30(self->+0, word) runs for each; func_08AA3040(func_08A2C248(),
// self); func_08A4A3A0 / func_08A47388 / func_08A46DCC(self); four value objects (func_08A2C594, func_08A3D394,
// func_08A30AC8, func_08A2D068 at frame +0xC / +8 / +4 / +0) handed to func_08A7CE50(self->+0, object) (target checked
// against 0x08CAC0D0) and destroyed; then eighteen update functions on self (func_08A46340 .. func_08A4565C).
void func_08A2C2B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    constexpr std::uint32_t guard = 0x08CF65A0u;
    constexpr std::uint32_t table = 0x08CF65B8u;
    constexpr std::uint32_t message = 0x08CAC0D0u;
    if (sext8(byte_u(rt, guard)) == 0u && call_set(rt, ctx, 0x08B6441Cu, {guard}) != 0u) {
        static constexpr std::uint32_t kBuilders[19] = {0x08A443E0u, 0x08A43C90u, 0x08A40AD0u, 0x08A40ADCu, 0x08A3E9A4u,
                                                        0x08A3D6CCu, 0x08A3A2CCu, 0x08A398B0u, 0x08A38A2Cu, 0x08A37D0Cu,
                                                        0x08A34D34u, 0x08A3309Cu, 0x08A32264u, 0x08A3CB38u, 0x08A34088u,
                                                        0x08A2DDA0u, 0x08A2D108u, 0x08A31CBCu, 0x08A36D2Cu};
        for (std::uint32_t index = 0u; index < 19u; ++index) put(rt, table + index * 4u, call_set(rt, ctx, kBuilders[index], {}));
        call_set(rt, ctx, 0x08B64494u, {guard});
    }
    ctx.gpr[2] = word(rt, table);
    if (ctx.gpr[2] != 0u) {
        std::uint32_t entry = table;
        do {
            const std::uint32_t target = checked(rt, ctx, word(rt, self), message);
            call_set(rt, ctx, 0x08A7CC30u, {target, word(rt, entry)});
            entry += 4u;
        } while (word(rt, entry) != 0u);
    }
    const std::uint32_t found = call_set(rt, ctx, 0x08A2C248u, {});
    call_set(rt, ctx, 0x08AA3040u, {found, self});
    call_set(rt, ctx, 0x08A4A3A0u, {self});
    call_set(rt, ctx, 0x08A47388u, {self});
    call_set(rt, ctx, 0x08A46DCCu, {self});
    static constexpr std::uint32_t kMakers[4] = {0x08A2C594u, 0x08A3D394u, 0x08A30AC8u, 0x08A2D068u};
    for (std::uint32_t index = 0u; index < 4u; ++index) {
        const std::uint32_t temporary = frame.at(0xCu - index * 4u);
        const std::uint32_t target = checked(rt, ctx, word(rt, self), message);
        call_set(rt, ctx, kMakers[index], {temporary});
        call_set(rt, ctx, 0x08A7CE50u, {target, temporary});
        call_set(rt, ctx, kDestroy, {temporary});
    }
    static constexpr std::uint32_t kUpdates[18] = {0x08A46340u, 0x08A46B74u, 0x08A4691Cu, 0x08A46A48u, 0x08A46CA0u, 0x08A459E0u,
                                                   0x08A45B0Cu, 0x08A45C38u, 0x08A45D64u, 0x08A45E90u, 0x08A45FBCu, 0x08A460E8u,
                                                   0x08A458B4u, 0x08A46214u, 0x08A45404u, 0x08A45788u, 0x08A45530u, 0x08A4565Cu};
    for (const std::uint32_t update : kUpdates) call_set(rt, ctx, update, {self});
}
// func_088D8CA8(self, source): the member-wise copy assignment of four shared-pointer pairs {pointer +0, control block +4}
// at source + 0 / 8 / 0x10 / 0x18 into self + 0x678 + the same offsets: the pointer is copied; when the control block
// differs from the target's (read before the pointer store) its use count goes up, the target's old block is released
// (use count; virtual slot 8, then weak count +8 and slot 0xC on the last) and the new block stored. The rest is a tail
// jump to func_08B90024(target + 0x20, source + 0x20).
void func_088D8CA8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t source = ctx.gpr[5];
    const std::uint32_t target = self + 0x678u;
    for (std::uint32_t offset = 0u; offset < 0x20u; offset += 8u) {
        const std::uint32_t pointer = word(rt, source + offset);
        const std::uint32_t earlier = word(rt, target + offset + 4u);
        put(rt, target + offset, pointer);
        const std::uint32_t block = word(rt, source + offset + 4u);
        if (block == earlier) continue;
        if (block != 0u) count_up(rt, ctx, block + 4u);
        const std::uint32_t old = word(rt, target + offset + 4u);
        if (old != 0u) release_shared(rt, ctx, old);
        put(rt, target + offset + 4u, block);
    }
    tail_call(ctx, 0x08B90024u, {target + 0x20u, source + 0x20u});
}
} // namespace hot_batch_113
} // namespace gt::game

#define HOT113(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_113::name, gt::ReturnKind::kind)

HOT113(0x08844924u, func_08844924, Word);
HOT113(0x088DEA1Cu, func_088DEA1C, Word);
HOT113(0x08A13BE0u, func_08A13BE0, Word);
HOT113(0x08845CB0u, func_08845CB0, Word);
HOT113(0x0888BD14u, func_0888BD14, Word);
HOT113(0x08B71720u, func_08B71720, Word);
HOT113(0x08C23158u, func_08C23158, Word);
HOT113(0x08A535A4u, func_08A535A4, Word);
HOT113(0x089A50A0u, func_089A50A0, Word);
HOT113(0x0888AF70u, func_0888AF70, Word);
HOT113(0x08A14D6Cu, func_08A14D6C, Word);
HOT113(0x08AFFA70u, func_08AFFA70, Word);
HOT113(0x08AACAF8u, func_08AACAF8, Word);
HOT113(0x088A66A4u, func_088A66A4, Word);
HOT113(0x08BB6964u, func_08BB6964, Word);
HOT113(0x08BB2CFCu, func_08BB2CFC, Word);
HOT113(0x089E1C90u, func_089E1C90, Word);
HOT113(0x08940610u, func_08940610, Word);
HOT113(0x08A993B4u, func_08A993B4, Word);
HOT113(0x08A1AC08u, func_08A1AC08, Word);
HOT113(0x08BAC714u, func_08BAC714, Word);
HOT113(0x0889E258u, func_0889E258, Word);
HOT113(0x089348D0u, func_089348D0, Word);
HOT113(0x08A2C2B4u, func_08A2C2B4, Word);
HOT113(0x088D8CA8u, func_088D8CA8, Word);
