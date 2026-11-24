// New batch 36 (main session): ten long functions the extra seeds made emittable, hand-written as structured C from `tools.disasm`
// (no emitter body, no recompiler output). They are 254-258 instructions each: one tag dispatcher over the animation node format,
// four functions of the counted-object (shared_ptr-shaped) protocol with its `ll`/`sc` refcount bumps, one node/string vector
// insert, a node-list constructor, two float geometry passes, an audio mixing loop and a key-sorted node list.
// Note: every delay slot of a `jal` runs before its call, so a value moved there is the *previous* call's result; `v0` is left at
// every exit with what the asm leaves there (a callee's result, or the constant the compare built).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_36 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call with the argument registers set like the original sets them before the jal (a0-a3, t0-t3 in order): what the callee
// does not take stays as the caller had it; v0 / v1 / f0 afterwards are the callee's.
u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}

// A `jalr $v0` through a vtable slot: the callee starts with v0 = its own address, as after the original's `lw v0, slot(vtable)`.
u32 call_at(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    ctx.gpr[2] = target;
    return call_set(rt, ctx, target, arguments);
}

// The same with float arguments in $f12-$f15; v0 afterwards is the callee's, f0 its float result.
u32 call_setf(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
              std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}

// Where the prologue of a function stores a callee-saved register: `reg` 0-31 is a general register, 32-63 a floating one (32 + its number).
struct SavedSlot {
    unsigned reg;
    u32 offset;
};

// The callee-saved registers (s0-s7, fp, f20-f31) a function keeps values in, and the stores of its prologue: the rewrite stores them
// where the asm does (a callee can read the stack below its caller's frame), sets the registers as the asm does because callees can
// read them too, and gives the caller's values back when it returns.
class SavedRegisters {
public:
    SavedRegisters(Runtime &rt, AllegrexContext &ctx, GuestFrame &frame, std::initializer_list<SavedSlot> slots) : ctx_(ctx) {
        for (unsigned i = 0; i < 8; ++i) gpr_[i] = ctx.gpr[16 + i];
        fp_ = ctx.gpr[30];
        for (unsigned i = 0; i < 12; ++i) fpr_[i] = ctx.fpr[20 + i];
        for (const SavedSlot &slot : slots) {
            if (slot.reg < 32u) put(rt, frame.at(slot.offset), ctx.gpr[slot.reg]);
            else put_float(rt, frame.at(slot.offset), ctx.fpr[slot.reg - 32u]);
        }
    }
    ~SavedRegisters() {
        for (unsigned i = 0; i < 8; ++i) ctx_.gpr[16 + i] = gpr_[i];
        ctx_.gpr[30] = fp_;
        for (unsigned i = 0; i < 12; ++i) ctx_.fpr[20 + i] = fpr_[i];
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    AllegrexContext &ctx_;
    u32 gpr_[8];
    u32 fp_;
    float fpr_[12];
};

std::uint32_t sext(std::uint32_t value, unsigned bits) {
    const unsigned shift = 32u - bits;
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(value << shift) >> shift);
}
// The `lui` half of an address whose low half is added as a signed 16-bit immediate.
constexpr std::uint32_t high_half(std::uint32_t address) { return (address + 0x8000u) & 0xFFFF0000u; }
// `ext rt, rs, position, size`: the `size` bits of rs from `position`.
constexpr std::uint32_t ext(std::uint32_t value, unsigned position, unsigned size) {
    return (value >> position) & (size >= 32u ? 0xFFFFFFFFu : (1u << size) - 1u);
}
// `ins rt, rs, position, size`: rt's `size` bits from `position` replaced by rs's low bits.
constexpr std::uint32_t ins(std::uint32_t value, std::uint32_t source, unsigned position, unsigned size) {
    const std::uint32_t mask = (size >= 32u ? 0xFFFFFFFFu : (1u << size) - 1u) << position;
    return (value & ~mask) | ((source << position) & mask);
}
std::uint32_t sra(std::uint32_t value, unsigned bits) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(value) >> bits); }
// The four-character tags the original builds from a `lui` / `ori` pair, big-endian in the word ('LITD' -> 0x4C495444).
constexpr std::uint32_t tag_of(char a, char b, char c, char d) {
    return (static_cast<std::uint32_t>(a) << 24) | (static_cast<std::uint32_t>(b) << 16) |
           (static_cast<std::uint32_t>(c) << 8) | static_cast<std::uint32_t>(d);
}
float float_bits(std::uint32_t bits) { return std::bit_cast<float>(bits); }

// The counted objects of this batch (PSP libstdc++'s shared_ptr control block): two counts at +4 (strong) and +8 (weak), the object
// below, and the vtable's +8 / +0xC slots (dispose, destroy). The original bumps a count with a `ll`/`sc` retry loop; the harness'
// runs are single-threaded, so a plain read / write leaves the same word.
void retain_counted(Runtime &rt, u32 object) {
    if (object == 0u) return;
    put(rt, object + 4u, word(rt, object + 4u) + 1u);
}
// A bare `ll` / `sc` bump of the word at `address`.
void count_up(Runtime &rt, u32 address) { put(rt, address, word(rt, address) + 1u); }
// Drops the strong count and returns the value it had: 1 means it is now zero and the object must be disposed of.
u32 drop_strong_count(Runtime &rt, u32 object) {
    const u32 before = word(rt, object + 4u);
    put(rt, object + 4u, before - 1u);
    return before;
}
u32 drop_weak_count(Runtime &rt, u32 object) {
    const u32 before = word(rt, object + 8u);
    put(rt, object + 8u, before - 1u);
    return before;
}
// `if (--*strong == 0) { vtable[8](this); if (--*weak == 0) vtable[0xC](this); }`, the vtable reloaded for the second call as the asm does.
void release_counted(Runtime &rt, AllegrexContext &ctx, u32 object) {
    if (object == 0u) return;
    if (drop_strong_count(rt, object) != 1u) return;
    call_at(rt, ctx, word(rt, word(rt, object) + 8u), {object});
    if (drop_weak_count(rt, object) != 1u) return;
    call_at(rt, ctx, word(rt, word(rt, object) + 0xCu), {object});
}

// 0x0886E0E0 (a0 = the object, a1, a2 = a node): the object's +0x1C is decoded first (0x0886B1CC; a non-zero answer ends the
// function), then 0x0886AFA8 gives a cursor four bytes into its result. The node's tag (0x08AC2690) picks the work:
// 'LITD' / 'LITS' / 'LITV' / 'LITA' read the node's four children -- 0x08AC2730 selects one, 0x08AC28B8 gives child 0's value and
// 0x08AC28E8 children 1-3's floats, which go into the frame quad at +0x40 (0x08AC26DC releases each element). With child 0 zero the
// quad goes to 0x0887CAA0 / 0x0887CAC8 / 0x0887CA78 / 0x0887C90C; 'LITV' calls 0x0887CA78 or 0x0887C930 by child 0 instead. 'ANIM'
// asks 0x0886A9A0(object, 5) for an entry, writes 2 into bits 5-7 of the word at frame +0x50 and runs 0x0886F854 / 0x0886F894 with it.
// Any other tag ends the function with the constant the compare built in v0.
void func_0886E0E0(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 node = ctx.gpr[6];
    GuestFrame frame(ctx, 0xB0u);
    SavedRegisters saved(rt, ctx, frame, {{19u, 0x9Cu}, {17u, 0x94u}, {16u, 0x90u}, {31u, 0xA4u}, {20u, 0xA0u}, {18u, 0x98u}});
    const u32 quad = frame.at(0x40u);
    ctx.gpr[19] = node;
    ctx.gpr[17] = a1;
    ctx.gpr[16] = object;
    if (call_set(rt, ctx, 0x0886B1CCu, {word(rt, object + 0x1Cu), a1, node}) != 0u) return;
    const u32 vector = call_set(rt, ctx, 0x0886AFA8u, {word(rt, object + 0x1Cu), a1, node}) + 4u;
    ctx.gpr[20] = vector;
    put(rt, frame.at(0x4Cu), 0u);
    const u32 tag = call_set(rt, ctx, 0x08AC2690u, {node, a1, node});
    ctx.gpr[3] = tag;
    ctx.gpr[2] = tag_of('L', 'I', 'T', 'D');
    const bool above_litd = tag > tag_of('L', 'I', 'T', 'D');
    ctx.gpr[2] = above_litd ? 1u : 0u;
    // The four children of the node: child 0's value as the selector, children 1-3's floats into the quad at +0x40.
    auto read_children = [&](u32 first_element) {
        call_set(rt, ctx, 0x08AC2730u, {first_element, node, 0u});
        const u32 selector = call_set(rt, ctx, 0x08AC28B8u, {first_element});
        call_set(rt, ctx, 0x08AC26DCu, {first_element});
        for (unsigned index = 1u; index < 4u; ++index) {
            const u32 element = first_element - 4u * index;
            call_set(rt, ctx, 0x08AC2730u, {element, node, index});
            call_set(rt, ctx, 0x08AC28E8u, {element});
            put_float(rt, quad + 4u * (index - 1u), ctx.fpr[0]);
            call_set(rt, ctx, 0x08AC26DCu, {element});
        }
        return selector;
    };
    if (tag == tag_of('L', 'I', 'T', 'D')) {
        if (read_children(frame.at(0x1Cu)) == 0u) call_set(rt, ctx, 0x0887CAA0u, {vector, quad});
    } else if (above_litd) {
        ctx.gpr[2] = tag_of('L', 'I', 'T', 'S');
        if (tag == tag_of('L', 'I', 'T', 'S')) {
            if (read_children(frame.at(0x2Cu)) == 0u) call_set(rt, ctx, 0x0887CAC8u, {vector, quad});
        } else if (tag == tag_of('L', 'I', 'T', 'V')) {
            if (read_children(frame.at(0x3Cu)) == 0u) call_set(rt, ctx, 0x0887CA78u, {vector, quad});
            else call_set(rt, ctx, 0x0887C930u, {vector, quad});
        } else {
            ctx.gpr[2] = tag_of('L', 'I', 'T', 'V');
        }
    } else {
        if (tag == tag_of('A', 'N', 'I', 'M')) {
            ctx.gpr[2] = tag_of('L', 'I', 'T', 'A');
            const u32 entry = call_set(rt, ctx, 0x0886A9A0u, {object, 5u, node});
            if (entry == 0u) return;
            put(rt, frame.at(0x50u), ins(word(rt, frame.at(0x50u)), 2u, 5u, 3u));
            call_set(rt, ctx, 0x0886F854u, {frame.at(0x50u), a1, node});
            call_set(rt, ctx, 0x0886F894u, {entry, frame.at(0x50u), node});
        } else if (tag == tag_of('L', 'I', 'T', 'A')) {
            if (read_children(frame.at(0x0Cu)) == 0u) call_set(rt, ctx, 0x0887C90Cu, {vector, quad});
        } else {
            ctx.gpr[2] = tag_of('L', 'I', 'T', 'A');
        }
    }
}

// 0x088F20E4 (a0 = the renderer, a1 = a settings block, f12 = a scale): the point at settings +0/+4/+8 is moved back along the
// direction at settings +0x1C/+0x20/+0x24 by `scale` (the direction, normalised through 0x08B4A720 / sqrt / 0x08B4A5E8, stays at
// frame +0xC); f22 = 1 + byte(+0x42) * 0xBB808081. Then for each of byte(+0x40) slots: the phase (halfword +0x46 - 40) / 30 clamped
// to 0..1 is compared with 0x08BB9130(renderer +0x840, 0, 1) and, when the gate is at or below it, two more 0x08BB9130 reads scale
// 150 and 0.001 * +0x44 give the two distances (f27, f26). With byte(+0x41) set, 0x08B1ED60 / 0x08B1EFF8 give a turn and a radius:
// 0x08B4C0F8 builds a rotation into the frame matrix, 0x08B4BA68 turns the frame point by it, 0x08B4A720 / sqrt / 0x08B4A5E8
// normalise that point and 0x08B4FE74 / 0x08B4BAA8 put it back. Both 0x08BB9130 reads with -1 then scale settings +0x28/+0x2C/+0x30
// and +0x34/+0x38/+0x3C onto the point, which 0x088F5D20 hands to the renderer's +0x844 with (f27, f26) and settings +0xC.
// v0 = byte(+0x40) when no slot is to be done, 0 after the loop.

// 0x088D390C (a0 = p, a1 = self, a2 = out, a3 = mode, f12 = a, f13 = b): self's and out's three words are cleared; when
// 0x088DB5C4(p->+0) fails the function ends there. Two 0x088D2BC8(p) reads at (a + 2) + b and a - 2, and two 0x088D32E0(p, mode)
// vectors at frame +0x54 and +0x48: the first is the axis (kept at +0x90 / +0x88 ...), the second is copied to +0x3C / +0x9C, and
// 0x08B4AA68 gives their cross product at +0x3C. Its length (0x08B4A720, sqrt, 0x08B4A5E8) normalises it at +0x84: 0x08B66E88 of
// -y and 0x08B66FBC of (x, z) give out +0 and out +4. 0x08B4A9D0(+0x60, +0x6C = (0, 1, 0), +0x84) normalised to +0x24 / +0x6C,
// 0x08B4AA68(+0x30 = +0x90, +0x24) with +30 on y is +0xA8, 0x08B4A5E8(+0x0C) and 0x08B4AA34(+0x18 = +0x90, +0x0C) with +30 on y is
// +0xB4. 0x088FAB84(0x088DB5C4(p->+0), +0xC0, +0xA8) then (+0xB4) fill +0xC0, copied to +0x00 / +0xB4; 0x08B4AA68(+0x00, +0xA8)
// normalised to +0x6C and 0x08B4A9D0(+0x60, +0x84, +0x6C); 0x08B66FBC of (x, z) of +0x60 gives out +8, and self takes +0x90's three
// floats. The frame pair at +0xF0 / +0xF4 (0, -1) is written as the original does.

// 0x089B7574 (a0 = the object, a1 = a text): 0x08A36C3C builds a text token at frame +8 (an assert of 0x08CA5910 when it is
// empty), 0x08A34DA8 stores it under 0x08CA5918 and the token is released. With bit 2 of the object's +0x60 and the holder at
// +0x12C: three 0x08A76AA0 / 0x08A76AAC copies at frame +0xC / +0x10 / +4, the +0x10 one assigned from the +4 copy and both
// released, then the holder's vtable +0xCC virtual with (holder, frame +0xC, 1, frame +0x10) and both released. 0x0896A35C feeds
// frame +0x18, 0x089B7378 the object's +0x154, 0x0896A9C4 (+0x124, then +0x120) the frame pair at +0x14 / +0x18 with 0x0896A4E0
// between them. 0x089B5E10's answer, when non-null, is turned by 0x089BD4DC / 0x08969B98 into the frame quad +0x24..+0x30:
// the object's +0x164 / +0x160 take (y + z) - z/2 and z/2 + y, 0x0896A35C / 0x0896A9C4 (of the negated z) then 0x08969B98 (of
// +0x120). The advance (halfword +0x46 - 40) / 30 * -2 compared with +0x154 either latches the state at +0x168..+0x174 and byte
// +0x159 = 1 or clamps the frame +0x18 and runs 0x08969D2C. With +0x128 set, 0x0896A35C and 0x08B70F28 / 0x08B71328 (2.5) /
// 0x08B712B0 (2.9375) / 0x08B71EC8 (integer float words) give a value that 0x0896A9C4 takes with frame +0x34..+0x40. Finally
// byte +0x158 = 1, +0x15C = +0x138, 0x08967538 (+0x120, 0), 0x089AB114, 0x089C4558(1) and 0x089B601C(object, a1).

// 0x08C5086C (a0 = the container, a1 = the position, a2 = the element to insert): a vector of 0x10-byte elements
// {counted text buffer +0, byte length +4, two flag bytes +8 / +9, word +0xC}. With room, one element is appended from the last one
// (its buffer released and rebuilt through 0x08B6461C, the bytes copied one by one), a copy of a2 is built at frame +4 and the
// elements between the position and the new end are shifted up by one (0x08B61FC0 frees the target's buffer), then the position takes
// the frame copy's buffer / bytes / word and the frame copy's buffer is freed unless the position was the frame copy itself. Full, a
// block of 2 x the element count (1 when empty, 0x08B35DA0) is allocated, 0x08C50794 places the new element in it and moves the
// rest (flag bytes at frame +1 / +2), 0x08C50744 drops the old range and 0x08B35254 frees the old block; the members then hold
// the new block / end / capacity.
void func_08C5086C(Runtime &rt, AllegrexContext &ctx) {
    const u32 container = ctx.gpr[4];
    const u32 position = ctx.gpr[5];
    const u32 source = ctx.gpr[6];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{21u, 0x34u}, {20u, 0x30u}, {19u, 0x2Cu}, {18u, 0x28u}, {31u, 0x3Cu}, {22u, 0x38u},
                                           {17u, 0x24u}, {16u, 0x20u}});
    const u32 temporary = frame.at(4u);
    ctx.gpr[18] = source;
    ctx.gpr[21] = position;
    ctx.gpr[20] = position;
    ctx.gpr[19] = container;
    // `*dest = *source` for one element: with release_first the target's buffer is freed first, otherwise its words are zeroed.
    auto copy_element = [&](u32 dest, u32 source_element, bool release_first) {
        if (release_first) {
            const u32 old = word(rt, dest);
            if (old != 0u) call_set(rt, ctx, 0x08B61FC0u, {old});
            put(rt, dest, 0u);
        } else {
            put(rt, dest + 4u, 0u);
            put(rt, dest, 0u);
        }
        const u32 length = word(rt, source_element + 4u);
        put(rt, dest + 4u, length);
        if (static_cast<s32>(length) > 0) {
            const u32 data = call_set(rt, ctx, 0x08B6461Cu, {length});
            put(rt, dest, data);
            if (static_cast<s32>(word(rt, dest + 4u)) > 0) {
                for (u32 index = 0u; static_cast<s32>(index) < static_cast<s32>(word(rt, dest + 4u)); ++index) {
                    rt.memory().store8(word(rt, dest) + index, rt.memory().load8(word(rt, source_element) + index));
                }
            }
        }
        put(rt, dest + 0xCu, word(rt, source_element + 0xCu));
        rt.memory().store8(dest + 8u, rt.memory().load8(source_element + 8u));
        rt.memory().store8(dest + 9u, rt.memory().load8(source_element + 9u));
    };
    u32 end = word(rt, container + 4u);
    if (end == word(rt, container + 8u)) {  // full: a new block of twice the elements
        const u32 begin = word(rt, container);
        const u32 elements = sra(end - begin, 4);
        if (elements == 0x0FFFFFFFu) call_set(rt, ctx, 0x08B5ADDCu, {0x08CB5550u});
        u32 block;
        u32 size;  // s6: the block's bytes
        if (elements == 0u) {
            size = 1u << 4;
            block = call_set(rt, ctx, 0x08B35DA0u, {size});
        } else {
            const u32 doubled = elements << 1;
            if (doubled < elements) {
                size = 0xFFFFFFF0u;
                block = call_set(rt, ctx, 0x08B35DA0u, {size});
            } else if (doubled == 0u) {
                block = 0u;
                size = 0u;
            } else {
                size = doubled << 4;
                block = call_set(rt, ctx, 0x08B35DA0u, {size});
            }
        }
        const u32 inserted = call_set(rt, ctx, 0x08C50794u, {begin, position, block, frame.at(2u)});
        ctx.gpr[16] = inserted;
        if (inserted != 0u) copy_element(inserted, source, false);
        call_set(rt, ctx, 0x08C50794u, {position, word(rt, container + 4u), inserted + 0x10u, frame.at(1u)});
        call_set(rt, ctx, 0x08C50744u, {word(rt, container), word(rt, container + 4u), frame.at(0u)});
        if (word(rt, container) != 0u) call_set(rt, ctx, 0x08B35254u, {word(rt, container)});
        put(rt, container + 8u, block + size);
        put(rt, container + 4u, inserted);
        put(rt, container, block);
        ctx.gpr[2] = block + size;
        return;
    }
    if (end != 0u) {
        copy_element(end, end - 0x10u, false);
        end = word(rt, container + 4u);
    }
    end += 0x10u;
    put(rt, container + 4u, end);
    copy_element(temporary, source, false);
    const u32 from = end - 0x20u;
    const u32 count = sra(from - position, 4);
    rt.memory().store8(frame.at(0xCu), rt.memory().load8(source + 8u));
    rt.memory().store8(frame.at(0xDu), rt.memory().load8(source + 9u));
    put(rt, frame.at(0x10u), word(rt, source + 0xCu));
    if (static_cast<s32>(count) > 0) {
        u32 target = end - 0x10u;
        u32 current = from;
        u32 flags = end - 0x28u;
        u32 destination = end - 0x18u;
        for (u32 done = 0u;;) {
            target -= 0x10u;
            current -= 0x10u;
            if (target != current) copy_element(target, current, true);
            const u32 first_byte = rt.memory().load8(flags);
            const u32 second_byte = rt.memory().load8(flags + 1u);
            const u32 trailing = word(rt, flags + 4u);
            ++done;
            rt.memory().store8(destination, static_cast<std::uint8_t>(first_byte));
            flags -= 0x10u;
            rt.memory().store8(destination + 1u, static_cast<std::uint8_t>(second_byte));
            put(rt, destination + 4u, trailing);
            destination -= 0x10u;
            if (done == count) break;
        }
    }
    if (position != temporary) {
        const u32 old = word(rt, position);
        if (old != 0u) call_set(rt, ctx, 0x08B61FC0u, {old});
        put(rt, position, 0u);
        const u32 length = word(rt, temporary + 4u);
        put(rt, position + 4u, length);
        if (static_cast<s32>(length) > 0) {
            const u32 data = call_set(rt, ctx, 0x08B6461Cu, {length});
            put(rt, position, data);
            if (static_cast<s32>(word(rt, position + 4u)) > 0) {
                for (u32 index = 0u; static_cast<s32>(index) < static_cast<s32>(word(rt, position + 4u)); ++index) {
                    rt.memory().store8(word(rt, position) + index, rt.memory().load8(word(rt, temporary) + index));
                }
            }
        }
    }
    rt.memory().store8(position + 8u, rt.memory().load8(frame.at(0xCu)));
    put(rt, position + 0xCu, word(rt, frame.at(0x10u)));
    const u32 ninth = rt.memory().load8(frame.at(0xDu));
    ctx.gpr[2] = ninth;
    rt.memory().store8(position + 9u, static_cast<std::uint8_t>(ninth));
    if (word(rt, temporary) != 0u) call_set(rt, ctx, 0x08B61FC0u, {word(rt, temporary)});
}

// 0x08AEDD28 (a0 = the output, a1 = the frame count, a2 = a flag): mixes every active channel of 0x08D41AD0 (+0x47C channels,
// +0x478 the array, 0x60B0 bytes each). A channel with byte +0x38 set and byte +0x3B clear asks 0x08AECF9C for its samples (byte
// +0x39 = 1, the count / 4 kept): the frames to do are a1, or from the rate halfword +0x60A4 and the position +0x60A6 with a1 - 1
// scaled by 12 bits. 0x08AECFA4 / 0x08AECFE4 / 0x08B79A88 fill the table at 0x08D42040 piece by piece (a short piece zeroes the
// rest, 0x08B36018), or 0x08AECFD4 fills it in one go when the rate is 0x1000. A rate other than 0x1000 resamples the table into
// 0x08D46040 (two halfwords per frame, linear with the 12-bit fraction +0x60A6), then the callback at 0x08D41AD0 + 0x490 may run
// (0x08AE0968 of the channel's +8 odd) with (channel +8, source, frames). The mix stage scales the two halfwords by the channel's
// +0x40 / +0x42: with the flag set it overwrites the output, otherwise it adds and clamps to -0x8000..0x7FFF. 0x08AED010(channel,
// min(count, frames)) ends the channel and the flag is cleared for the next one.
void func_08AEDD28(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x50u);
    SavedRegisters saved(rt, ctx, frame, {{23u, 0x3Cu}, {30u, 0x40u}, {22u, 0x38u}, {21u, 0x34u}, {20u, 0x30u}, {19u, 0x2Cu},
                                           {18u, 0x28u}, {17u, 0x24u}, {16u, 0x20u}, {31u, 0x44u}});
    const u32 output = frame.at(0u);
    const u32 table = 0x08D42040u;
    const u32 mix_buffer = 0x08D46040u;
    put(rt, output, ctx.gpr[4]);
    put(rt, frame.at(4u), ctx.gpr[5]);
    put(rt, frame.at(8u), ctx.gpr[6]);
    if (static_cast<s32>(word(rt, 0x08D41AD0u + 0x47Cu)) <= 0) {
        ctx.gpr[2] = word(rt, frame.at(8u));
        return;
    }
    put(rt, frame.at(0x10u), ctx.gpr[5] - 1u);
    u32 limit = word(rt, 0x08D41AD0u + 0x47Cu);
    for (u32 index = 0u, offset = 0u; index < limit; ++index, offset += 0x60B0u) {
        const u32 channel = word(rt, word(rt, 0x08D41AD0u + 0x478u) + offset);
        if (rt.memory().load8(channel + 0x38u) == 0u) {
            limit = word(rt, 0x08D41AD0u + 0x47Cu);
            continue;
        }
        if (rt.memory().load8(channel + 0x3Bu) != 0u) {
            limit = word(rt, 0x08D41AD0u + 0x47Cu);
            continue;
        }
        const u32 samples = call_set(rt, ctx, 0x08AECF9Cu, {channel});
        put(rt, frame.at(0xCu), samples);
        if (static_cast<s32>(samples) <= 0) {
            rt.memory().store8(channel + 0x39u, 0u);
            limit = word(rt, 0x08D41AD0u + 0x47Cu);
            continue;
        }
        rt.memory().store8(channel + 0x39u, 1u);
        const u32 count = static_cast<u32>(static_cast<s32>(samples) >> 2);
        put(rt, frame.at(0xCu), count);
        u32 frames = word(rt, frame.at(4u));
        if (word(rt, channel + 0x60A4u) != 0x1000u) {
            const s32 rate = static_cast<std::int16_t>(rt.memory().load16(channel + 0x60A4u));
            const s32 position = static_cast<std::int16_t>(rt.memory().load16(channel + 0x60A6u));
            const s32 scaled = position + static_cast<s32>(word(rt, frame.at(0x10u))) * rate;
            frames = static_cast<u32>((scaled + 0xFFF) >> 12) + 1u;
        }
        const u32 available = call_set(rt, ctx, 0x08AECFA4u, {channel, 0u});
        u32 filled = 0u;
        if (static_cast<s32>(available) >= static_cast<s32>(frames << 2) && word(rt, channel + 0x60A4u) == 0x1000u) {
            const u32 value = call_set(rt, ctx, 0x08AECFD4u, {channel});
            ctx.gpr[16] = value;
        } else {
            if (static_cast<s32>(frames) > 0) {
                for (;;) {
                    const u32 byte_offset = filled << 2;
                    const u32 window = static_cast<u32>(static_cast<s32>(call_set(rt, ctx, 0x08AECFA4u, {channel, byte_offset})) >> 2);
                    const u32 want = frames - filled;
                    u32 take;
                    if (static_cast<s32>(want) < static_cast<s32>(window)) {
                        take = want;
                    } else {
                        take = static_cast<u32>(static_cast<s32>(call_set(rt, ctx, 0x08AECFA4u, {channel, byte_offset})) >> 2);
                    }
                    if (static_cast<s32>(take) <= 0) break;
                    filled += take;
                    const u32 piece = call_set(rt, ctx, 0x08AECFE4u, {channel, byte_offset});
                    call_set(rt, ctx, 0x08B79A88u, {table + byte_offset, piece, take << 2});
                    if (filled >= frames) break;
                }
            }
            if (filled < frames) call_set(rt, ctx, 0x08B36018u, {table + (filled << 2), 0u, (frames - filled) << 2});
        }
        u32 source = table;
        if (word(rt, channel + 0x60A4u) != 0x1000u) {
            const u32 rate = static_cast<u32>(static_cast<std::int32_t>(static_cast<std::int16_t>(rt.memory().load16(channel + 0x60A4u))));
            u32 position = static_cast<u32>(static_cast<std::int32_t>(static_cast<std::int16_t>(rt.memory().load16(channel + 0x60A6u))));
            const u32 step = rate;
            const u32 destination = mix_buffer;
            u32 remaining = word(rt, frame.at(4u));
            if (static_cast<s32>(remaining) > 0) {
                u32 out_at = destination;
                do {
                    const u32 whole = sra(position, 12);
                    const u32 fraction = position & 0xFFFu;
                    const u32 at = table + (static_cast<s32>(whole) << 2);
                    const s32 near_left = static_cast<std::int16_t>(rt.memory().load16(at));
                    const s32 near_right = static_cast<std::int16_t>(rt.memory().load16(at + 2u));
                    const s32 far_left = static_cast<std::int16_t>(rt.memory().load16(at + 4u));
                    const s32 far_right = static_cast<std::int16_t>(rt.memory().load16(at + 6u));
                    const s32 complement = static_cast<s32>(0x1000u - fraction);
                    rt.memory().store16(out_at, static_cast<std::uint16_t>((near_left * complement + far_left * static_cast<s32>(fraction)) >> 12));
                    --remaining;
                    position += step;
                    out_at += 2u;
                    rt.memory().store16(out_at, static_cast<std::uint16_t>((near_right * complement + far_right * static_cast<s32>(fraction)) >> 12));
                    out_at += 2u;
                } while (static_cast<s32>(remaining) > 0);
            }
            rt.memory().store16(channel + 0x60A6u, static_cast<std::uint16_t>(position & 0xFFFu));
            source = mix_buffer;
            frames = sra(position, 12);
        }
        if (word(rt, 0x08D41AD0u + 0x490u) != 0u && (call_set(rt, ctx, 0x08AE0968u, {word(rt, channel + 8u)}) & 1u) != 0u) {
            call_at(rt, ctx, word(rt, 0x08D41AD0u + 0x490u), {word(rt, channel + 8u), source, word(rt, frame.at(4u))});
        }
        const s32 left = static_cast<std::int16_t>(rt.memory().load16(channel + 0x40u));
        const s32 right = static_cast<std::int16_t>(rt.memory().load16(channel + 0x42u));
        u32 out_at = word(rt, output);
        u32 remaining = word(rt, frame.at(4u));
        if (word(rt, frame.at(8u)) == 0u) {
            while (static_cast<s32>(remaining) > 0) {
                const s32 in_left = static_cast<std::int16_t>(rt.memory().load16(source));
                const s32 in_right = static_cast<std::int16_t>(rt.memory().load16(source + 2u));
                source += 4u;
                --remaining;
                s32 mixed = static_cast<std::int16_t>(rt.memory().load16(out_at)) + ((in_left * left) >> 12);
                mixed = mixed < -0x8000 ? -0x8000 : mixed;
                mixed = mixed > 0x7FFF ? 0x7FFF : mixed;
                rt.memory().store16(out_at, static_cast<std::uint16_t>(mixed));
                out_at += 2u;
                mixed = static_cast<std::int16_t>(rt.memory().load16(out_at)) + ((in_right * right) >> 12);
                mixed = mixed < -0x8000 ? -0x8000 : mixed;
                mixed = mixed > 0x7FFF ? 0x7FFF : mixed;
                rt.memory().store16(out_at, static_cast<std::uint16_t>(mixed));
                out_at += 2u;
            }
        } else {
            while (static_cast<s32>(remaining) > 0) {
                const s32 in_left = static_cast<std::int16_t>(rt.memory().load16(source));
                const s32 in_right = static_cast<std::int16_t>(rt.memory().load16(source + 2u));
                source += 4u;
                --remaining;
                rt.memory().store16(out_at, static_cast<std::uint16_t>((in_left * left) >> 12));
                out_at += 2u;
                rt.memory().store16(out_at, static_cast<std::uint16_t>((in_right * right) >> 12));
                out_at += 2u;
            }
        }
        u32 finished = word(rt, frame.at(0xCu));
        if (static_cast<s32>(frames) < static_cast<s32>(finished)) finished = frames;
        call_set(rt, ctx, 0x08AED010u, {channel, finished});
        put(rt, frame.at(8u), 0u);
        limit = word(rt, 0x08D41AD0u + 0x47Cu);
    }
}

// 0x08BD837C (a0 = out, a1 = a {object, counted block} pair, a2 = an argument, t0 = another): out's two words are cleared, and so
// they stay when the pair's object is null or 0x08BD5D54(object, a2) answers object + 0x98. Otherwise, under object +0x6C's lock
// (0x08B34888 / 0x08B34B24), 0x08B2CB18(object + 8, 0x10, t0) gives a handle h: a null answer releases h through 0x08BBFF7C and
// 0x08BBCFA0. With h, its four words are cleared, the pair's block's use count rises by 4 and falls by 3 around a 0x18-byte control
// block {vtable 0x08CC55E0, counts 1 / 1, h, object, block} (0x08B62344); when that allocation failed, 0x08909D98(h), 0x08BBD3B4
// (object, h) and 0x08BBEA18(object + 0x98, block) run. 0x08B8E380(frame + 4, h, h) links the block, the object's +0xC takes h,
// the block's weak count rises and the handle object's +0x10 slot drops its old block's weak count (its vtable +0xC when it reaches
// zero) before taking the new one. out then takes the handle and the block (use count up), the temporary block is released and
// 0x08BBCFA0(pair) ends it. v0 = out.

// 0x08860080 (a0 = the object, a1 = flags, a2 = an index array): the record the flags select (word +0x3E78 or its complement) is
// object + that * 7224 + 0x608, its +0x1C32 the count and the handle at object +0x5AC's +0x59C (0x088D9F80) the number of entries.
// The object's +0x3E7C gets four -1 halfwords. When byte +0x1C34 is clear the entries are only listed (0x088DC90C(handle, i) == 1
// stores i at +0x3E7C); otherwise a 2 x entries + 0x1E dynamic array goes on the stack (+8 frame) and is cleared, the index list at
// +0x1C20 (count +0x1C28, flag byte +0x102A per entry*168) or the caller's array (count +0x1C32) fills +0x3E7C while a byte is not
// set at +0x102A of the entry's record, the +0x1C35 direction byte drives a selection sort on the records' +0x20 / +0x24 words, and
// the entries whose array slot is still zero are appended. With flag bit 1 the list is reversed in place (s3 / 2 swaps).
void func_08860080(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 flags = ctx.gpr[5];
    const u32 other = ctx.gpr[6];
    GuestFrame frame(ctx, 0x50u);
    SavedRegisters saved(rt, ctx, frame, {{30u, 0x40u}, {17u, 0x24u}, {18u, 0x28u}, {31u, 0x44u}, {23u, 0x3Cu}, {22u, 0x38u},
                                           {21u, 0x34u}, {20u, 0x30u}, {19u, 0x2Cu}, {16u, 0x20u}});
    const u32 base = frame.at(0u);
    ctx.gpr[30] = base;
    const u32 odd = (flags ^ 1u) & 1u;
    const u32 reverse = ext(flags, 1u, 1u);
    put(rt, base + 4u, reverse);
    ctx.gpr[18] = object;
    ctx.gpr[17] = odd;
    const u32 selected = odd != 0u ? word(rt, object + 0x3E78u) : (word(rt, object + 0x3E78u) ^ 1u);
    const u32 handle = word(rt, object + 0x5ACu);
    const u32 record = object + ((selected << 6) - (selected << 3)) * 129u + 0x608u;
    const u32 entries = call_set(rt, ctx, 0x088D9F80u, {handle});
    ctx.gpr[20] = entries;
    const u32 count = static_cast<u32>(static_cast<std::int32_t>(static_cast<std::int16_t>(rt.memory().load16(record + 0x1C32u))));
    ctx.gpr[22] = count;
    for (u32 index = 0u, at = object; index < 4u; ++index, at += 2u) rt.memory().store16(at + 0x3E7Cu, static_cast<std::uint16_t>(-1));
    const u32 dynamic = base + 8u;
    u32 listed = 0u;  // s3
    if (rt.memory().load8(record + 0x1C34u) != 0u) {
        const u32 amount = ins((entries << 1) + 0x1Eu, 0u, 0u, 4u);
        ctx.gpr[29] -= amount;
        put(rt, dynamic, ctx.gpr[29]);
        if (static_cast<s32>(entries) > 0) {
            for (u32 index = 0u, at = word(rt, dynamic); index != entries; ++index, at += 2u) rt.memory().store16(at, 0u);
        }
        if (odd != 0u) {
            const u32 list = record + 0x1C20u;
            const u32 length = static_cast<u32>(static_cast<std::int32_t>(static_cast<std::int16_t>(rt.memory().load16(record + 0x1C28u))));
            put(rt, base, length);
            if (static_cast<s32>(length) > 0) {
                for (u32 step = 0u, slot = object;; ++step, slot += 2u) {
                    const u32 index = static_cast<u32>(static_cast<std::int32_t>(static_cast<std::int16_t>(rt.memory().load16(list + (step << 1)))));
                    const u32 answer = call_set(rt, ctx, 0x088DC90Cu, {handle, index, record + 0x1C2Au});
                    if (answer == 1u && rt.memory().load8(record + index * 168u + 0x102Au) != 0u) {
                        rt.memory().store16(word(rt, dynamic) + (index << 1), 1u);
                        ++listed;
                        rt.memory().store16(slot + 0x3E7Cu, static_cast<std::uint16_t>(index));
                    }
                    if (step + 1u == word(rt, base)) break;
                }
            }
            // The selection sort: the direction byte takes the larger record instead of the smaller when it is set.
            const s32 sortable = static_cast<s32>(listed) - 1;
            const u32 direction = rt.memory().load8(record + 0x1C35u);
            for (s32 outer = 0; outer < sortable; ++outer) {
                const u32 next = static_cast<u32>(outer) + 1u;
                u32 chosen = static_cast<u32>(static_cast<std::int32_t>(static_cast<std::int16_t>(rt.memory().load16(object + 0x3E7Cu + (static_cast<u32>(outer) << 1)))));
                u32 chosen_record = record + chosen * 168u + 0x1018u;
                if (static_cast<s32>(next) < sortable) {
                    u32 candidate_at = object + 0x3E7Cu + (next << 1);
                    for (s32 step = 0; step != static_cast<s32>(listed) - static_cast<s32>(next); ++step, candidate_at += 2u) {
                        const u32 candidate = static_cast<u32>(static_cast<std::int32_t>(static_cast<std::int16_t>(rt.memory().load16(candidate_at))));
                        const u32 chosen_high = word(rt, chosen_record + 0x24u);
                        const u32 chosen_low = word(rt, chosen_record + 0x20u);
                        const u32 candidate_record = record + candidate * 168u + 0x1018u;
                        const u32 candidate_high = word(rt, candidate_record + 0x24u);
                        const u32 candidate_low = word(rt, candidate_record + 0x20u);
                        u32 select = 0u;
                        if (candidate_low != chosen_low || candidate_high != chosen_high) {
                            if (candidate_high < chosen_high || (candidate_high == chosen_high && candidate_low < chosen_low)) select = 1u;
                        }
                        if (select != direction) {
                            rt.memory().store16(candidate_at, static_cast<std::uint16_t>(chosen));
                            chosen_record = candidate_record;
                            chosen = candidate;
                            rt.memory().store16(object + 0x3E7Cu + (static_cast<u32>(outer) << 1), static_cast<std::uint16_t>(candidate));
                        }
                    }
                }
            }
        } else {
            const u32 length = count;
            if (static_cast<s32>(length) > 0) {
                for (u32 step = 0u, slot = 0u;; ++step, slot += 2u) {
                    const u32 index = static_cast<u32>(static_cast<std::int32_t>(static_cast<std::int16_t>(rt.memory().load16(other + (step << 1)))));
                    const u32 answer = call_set(rt, ctx, 0x088DC90Cu, {handle, index, record + 0x1C2Au});
                    if (answer == 1u) {
                        rt.memory().store16(word(rt, dynamic) + (index << 1), 1u);
                        ++listed;
                        rt.memory().store16(object + (listed << 1) + 0x3E7Cu, static_cast<std::uint16_t>(index));
                    }
                    if (step + 1u == length) break;
                }
            }
        }
        if (static_cast<s32>(entries) > 0) {
            u32 array_at = word(rt, dynamic);
            u32 slot = object + 0x3E7Cu + (listed << 1);
            for (u32 index = 0u;; ++index, array_at += 2u) {
                if (call_set(rt, ctx, 0x088DC90Cu, {handle, index, record + 0x1C2Au}) == 1u &&
                    static_cast<std::int16_t>(rt.memory().load16(array_at)) == 0) {
                    rt.memory().store16(slot, static_cast<std::uint16_t>(index));
                    ++listed;
                    slot += 2u;
                }
                if (index + 1u == entries) break;
            }
        }
    } else if (static_cast<s32>(entries) > 0) {
        u32 slot = object + 0x3E7Cu;
        for (u32 index = 0u;; ++index) {
            if (call_set(rt, ctx, 0x088DC90Cu, {handle, index, record + 0x1C2Au}) == 1u) {
                rt.memory().store16(slot, static_cast<std::uint16_t>(index));
                ++listed;
                slot += 2u;
            }
            if (index + 1u == entries) break;
        }
    }
    if (reverse != 0u) {
        const u32 half = sra((listed >> 31) + listed, 1);
        ctx.gpr[2] = (listed >> 31) + listed;
        if (static_cast<s32>(half) > 0) {
            u32 low = object;
            u32 high = object + 0x3E7Cu + (listed << 1) - 2u;
            for (u32 step = 0u; step != half; ++step) {
                const u32 low_value = rt.memory().load16(low + 0x3E7Cu);
                const u32 high_value = rt.memory().load16(high);
                rt.memory().store16(high, static_cast<std::uint16_t>(low_value));
                high -= 2u;
                rt.memory().store16(low + 0x3E7Cu, static_cast<std::uint16_t>(high_value));
                low += 2u;
                ctx.gpr[2] = low_value;
            }
        }
    } else {
        ctx.gpr[2] = listed;
    }
}
} // namespace
} // namespace new_batch_main_36
} // namespace gt::game

#define NEWMAIN36(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_36::name, gt::ReturnKind::kind)
NEWMAIN36(0x08860080u, func_08860080, Word);
NEWMAIN36(0x0886E0E0u, func_0886E0E0, Word);
NEWMAIN36(0x08AEDD28u, func_08AEDD28, Word);
NEWMAIN36(0x08C5086Cu, func_08C5086C, Word);
