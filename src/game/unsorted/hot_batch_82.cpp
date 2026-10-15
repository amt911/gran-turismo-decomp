// Hot batch 82: reached functions that no earlier batch has written, picked from the call probe of batch 78 (every function of up to 70 instructions not yet decompiled, run on the five routes with
// --decomp off; batch 81 = PR #202 excluded with --exclude-ref origin/decomp/hot-batch-81, and no address that #202 writes, registers or leaves unregistered is taken here). Before reading any
// function its address was searched for in src/ and in the branch of batch 81. The candidates are the reached singles called by all five routes, ordered by calls, read by hand word by word
// with `tools/disasm.py` of #202 (since merged; branch targets and relocations resolved by the tool, never counted by hand; every delay slot is part of its branch's path).
// The descriptive words (ceilf, strcat, string compare, ...) rest on the behaviour each comment derives from the asm; the symbols keep
// their func_XXXXXXXX names and nothing goes into config/symbol_addrs.txt until a caller or string confirms them.
//
//  - holders, smart pointers and virtual getters of the engine (func_08C20838, func_08C21C84, func_08C20224, func_08C218D0, func_08C25CCC, func_08C34520, func_08C31FB4, func_088FC214, ...);
//  - soft-float and libm: int to double func_08B71CE0 (DoubleWord), ceilf func_08B661E8 (Float), a generator step func_08B3A37C, a float settings update func_089455E0;
//  - strings and libc: strcat func_08B7B9AC, string compare func_08C1CDCC, the representation allocator func_08B8543C, deleting destructors, string-keyed tree insert / operator[] (func_08C1BE44,
//    func_08C338D4, func_08C1CFD0), string range destruction func_08C1DA0C;
//  - ring buffer reader / writer func_08B2440C / func_08B2432C, queue helpers (func_08B34CA0), counters, constructors and small initialisers.
//
// Every call_guest / tail_call passes the argument registers the original sets before the jal (a0-a3, t0-t3, f12-f14): the ones it leaves alone keep the caller's value, which call_guest
// copies from ctx. Every call goes through `call_keep` (the callee's v0, v1 and f0 stay in the registers like the original's; a callee that sets no v0 hands the previous one on).
//
// Written but not registered (the commented HOT82 lines say why): func_08B59A24, func_08ACF390, func_08B33E50, func_08B0EBF0, func_08AD083C and func_08ABCE2C reach imports that stop the case (the
// vblank wait, sceIoRead, a thread start, sceUtility): every case was not comparable; func_08B7B2E4 (setjmp) stores ra, which inside a harness case is the case sentinel, so the first longjmp stopped
// the run; func_08B44B24, func_08BE834C, func_08C15E8C and func_08BE6674 write memory but no compared case changed it (the gate cannot see their stores).

#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cmath>
#include <cstdint>

namespace gt::game {
namespace hot_batch_82 {
namespace {

using namespace gt::game::hot;

constexpr std::uint32_t kStringEmpty = 0x08D74FD0u;  // the shared empty representation of the 1-byte-character string (its characters follow at +0xC)

inline void take(psprecomp::AllegrexContext &ctx, const GuestResult &result) {
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}

// A guest call whose results stay in the registers like the original's: v0 / v1 / f0 of the callee are what the next instruction sees, and a callee that sets
// no v0 hands the previous one on to the code after it (the caller's own v0 is what call_guest starts the callee with).
inline GuestResult call_keep(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments,
                             std::initializer_list<float> float_arguments) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, float_arguments);
    take(ctx, result);
    return result;
}
inline std::uint32_t call_keep(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments) {
    return call_keep(rt, ctx, address, arguments, {}).v0;
}

inline float fbits(std::uint32_t bits) { return std::bit_cast<float>(bits); }
inline std::uint32_t bits_of(float value) { return std::bit_cast<std::uint32_t>(value); }
// cvt.s.w: the word read as a signed integer.
inline float cvt_sw(std::uint32_t word_bits) { return static_cast<float>(static_cast<std::int32_t>(word_bits)); }

// func_08B59A24(count, base, wait): the graphics library waits until its vblank counter (kGuState + 0x20) is `base + count` frames past the old one. For count > 0 the wanted
// distance is count - (wait == 0): while (counter - base) < that, the sceDisplay vblank wait runs (func_08C7AC18, a0 = kGuState on entry). A zero `wait` then waits once more. v0 = the counter,
// v1 = kGuState as the instructions leave them.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): count <= 0 (the `blez` goes to the wait-flag test with a0 unchanged).
void func_08B59A24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t count = ctx.gpr[4], base = ctx.gpr[5], wait = ctx.gpr[6] & 0xFFu;
    GuestFrame frame(ctx, 0x20u);
    if (static_cast<std::int32_t>(count) > 0) {
        ctx.gpr[4] = kGuState;
        const std::uint32_t wanted = count - (wait == 0u ? 1u : 0u);
        if (word(rt, kGuState + 0x20u) - base < wanted) {
            do {
                (void)call_keep(rt, ctx, 0x08C7AC18u, {kGuState});
            } while (word(rt, kGuState + 0x20u) - base < wanted);
        }
    }
    if (wait == 0u) (void)call_keep(rt, ctx, 0x08C7AC18u, {ctx.gpr[4]});
    ctx.gpr[3] = kGuState;
    ctx.gpr[2] = word(rt, kGuState + 0x20u);
}

// func_08ACF390(this): swaps this[0xF8] (a flag, xor 1) around a priority change: func_08AF4F24(this + 0xFC, flag), the current thread priority (import 0x94AA61EE) is read and
// ChangeThreadPriority(0, 0x10) runs; func_08B59A24(this[0x18], D_08D00414, 1) waits for the vblank counter and its result becomes D_08D00414. func_08B1B1E4(this + 0xB8) and
// func_08B1B198(this + 0xB8) fill this[0xF0] (v0) and this[0xF4] (v1), the flag is toggled and written to this[0xF8] (func_08AF4F90(this + 0xFC, flag)), virtual slot 2 of the object runs,
// when it answers non-zero func_08B58E84(0) runs; func_08B2E0A4(func_08B2E314(6, 0)) and func_08AB978C(this[0xF0] as a float * 1e-6f) follow, and the old priority is restored with a tail call
// (ChangeThreadPriority(0, priority)).
void func_08ACF390(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    constexpr std::uint32_t kLastWait = 0x08D00414u;
    std::uint32_t priority;
    {
        GuestFrame frame(ctx, 0x20u);
        const std::uint32_t area = self + 0xFCu;
        const std::uint32_t block = self + 0xB8u;
        (void)call_keep(rt, ctx, 0x08AF4F24u, {area, word(rt, self + 0xF8u)});
        (void)call_keep(rt, ctx, 0x08C7AA88u, {});
        priority = ctx.gpr[2];
        (void)call_keep(rt, ctx, 0x08C7AA50u, {0u, 0x10u});
        const std::uint32_t waited = call_keep(rt, ctx, 0x08B59A24u, {word(rt, self + 0x18u), word(rt, kLastWait), 1u});
        put(rt, kLastWait, waited);
        (void)call_keep(rt, ctx, 0x08B1B1E4u, {block});
        (void)call_keep(rt, ctx, 0x08B1B198u, {block});
        put(rt, self + 0xF0u, ctx.gpr[2]);
        const std::uint32_t flag = word(rt, self + 0xF8u) ^ 1u;
        put(rt, self + 0xF4u, ctx.gpr[3]);
        put(rt, self + 0xF8u, flag);
        (void)call_keep(rt, ctx, 0x08AF4F90u, {area, flag, flag});
        const std::uint32_t vtable = word(rt, self);
        if (call_keep(rt, ctx, word(rt, vtable + 8u), {self}) != 0u) (void)call_keep(rt, ctx, 0x08B58E84u, {0u});
        (void)call_keep(rt, ctx, 0x08B2E314u, {6u, 0u});
        (void)call_keep(rt, ctx, 0x08B2E0A4u, {ctx.gpr[2]});
        const float seconds = psp_mul(cvt_sw(word(rt, self + 0xF0u)), fbits(0x358637BDu));
        ctx.fpr[12] = seconds;
        (void)call_keep(rt, ctx, 0x08AB978Cu, {}, {seconds});
    }
    tail_call(ctx, 0x08C7AA50u, {0u, priority});
}

// func_088FC214(this, object): the owner this->slot 0xC (virtual slot 0x30 of this) answers an entry; none gives 0. Otherwise the object's slot 2 answers a name (s2) and the
// entry's list at entry + 0x540 takes the object by func_089613A8(entry + 0x540, object, flag): flag 0 when the entry's type word [[entry + 4] + 0x2C] is zero and func_08906398(entry + 0x34) answers
// non-zero, or (a non-zero word, or a zero answer) func_08B35FD8(name, D_08CA0DBC) answers non-zero; flag 1 after the object's slot 3 ran with (object, [[[entry + 0x18] + 0x48] + 0x18]). v0 = 1 then.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): every compared case leaves at the first test (no entry from the owner's slot 0xC: v0 = 0, a0 = the object). Everything after it is read from the asm only: the object's slot 2 call, the entry's type word +0x2C (non-zero goes to the func_08B35FD8 test, zero asks func_08906398 first, an answer of 0 there also goes to func_08B35FD8, the `lui a1` of the delay slot restoring a1 = D_08CA0DBC), the flag-0 insertion when func_08906398 or func_08B35FD8 answers non-zero, the slot-3 call with (object, [[[entry + 0x18] + 0x48] + 0x18], 0) and the flag-1 insertion, v0 = 1.
void func_088FC214(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], object = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x20u);
        const std::uint32_t entry = call_keep(rt, ctx, word(rt, word(rt, self) + 0x30u), {self});
        ctx.gpr[4] = object;
        if (entry == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
        const std::uint32_t list = entry + 0x540u;
        const std::uint32_t name = call_keep(rt, ctx, word(rt, word(rt, object) + 8u), {object});
        ctx.gpr[5] = 0x08CA0DBCu;
        bool direct = false;  // true: func_089613A8 with flag 0
        if (word(rt, word(rt, entry + 4u) + 0x2Cu) == 0u) {
            ctx.gpr[4] = entry + 0x34u;
            direct = call_keep(rt, ctx, 0x08906398u, {entry + 0x34u}) != 0u;
        }
        std::uint32_t flag = 0u;
        if (!direct) {
            ctx.gpr[5] = 0x08CA0DBCu;
            if (call_keep(rt, ctx, 0x08B35FD8u, {name, 0x08CA0DBCu}) == 0u) {
                const std::uint32_t handle = word(rt, entry + 0x18u);
                ctx.gpr[5] = handle;
                const std::uint32_t slot = word(rt, word(rt, object) + 0xCu);
                const std::uint32_t second = word(rt, word(rt, handle + 0x48u) + 0x18u);
                (void)call_keep(rt, ctx, slot, {object, second, 0u});
                flag = 1u;
            }
        }
        (void)call_keep(rt, ctx, 0x089613A8u, {list, object, flag});
        ctx.gpr[2] = 1u;
    }
}

// func_08B44B24(this): when the byte this[0xAD] is set and this[0xB0] (an object) is non-null: func_08B344E0(this + 0x12C) runs; with this[0xB0] still non-null the virtual slot 0x23 of this answers
// a 64-bit value stored to this[0x928] / this[0x92C] and passed to func_08B44948(this, -, v0, v1); func_08B43A38(this) runs, and when virtual slot 0x2F of this answers non-zero
// func_08B43B3C(this, -, this[0x928], this[0x92C]); then virtual slot 4 of this[0xB0] runs. func_08B084F0(this) follows, this[0xC4] is incremented and func_08B3457C(this + 0x12C) is
// the tail call (v0 = the new count). Anything else leaves with v0 = 0.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): this[0xB0] reset to zero by func_08B344E0 (the second `beqz` goes straight to func_08B084F0).
void func_08B44B24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t list = self + 0x12Cu;
    std::uint32_t count;
    {
        GuestFrame frame(ctx, 0x10u);
        const std::uint32_t active = rt.memory().load8(self + 0xADu);
        if (active == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
        std::uint32_t member = word(rt, self + 0xB0u);
        ctx.gpr[4] = list;
        if (member == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
        (void)call_keep(rt, ctx, 0x08B344E0u, {list});
        member = word(rt, self + 0xB0u);
        ctx.gpr[5] = 0u;
        ctx.gpr[4] = self;
        if (member != 0u) {
            const GuestResult wide = call_keep(rt, ctx, word(rt, word(rt, self) + 0x8Cu), {self}, {});
            put(rt, self + 0x928u, wide.v0);
            put(rt, self + 0x92Cu, wide.v1);
            (void)call_keep(rt, ctx, 0x08B44948u, {self, ctx.gpr[5], wide.v0, wide.v1});
            (void)call_keep(rt, ctx, 0x08B43A38u, {self});
            if (call_keep(rt, ctx, word(rt, word(rt, self) + 0xBCu), {self}) != 0u) {
                (void)call_keep(rt, ctx, 0x08B43B3Cu, {self, ctx.gpr[5], word(rt, self + 0x928u), word(rt, self + 0x92Cu)});
            }
            const std::uint32_t inner = word(rt, self + 0xB0u);
            (void)call_keep(rt, ctx, word(rt, word(rt, inner) + 0x10u), {inner});
        }
        (void)call_keep(rt, ctx, 0x08B084F0u, {self});
        count = word(rt, self + 0xC4u) + 1u;
        put(rt, self + 0xC4u, count);
    }
    ctx.gpr[2] = count;
    tail_call(ctx, 0x08B3457Cu, {list});
}

// func_08C20838(this, object): this + 8 is a key; func_08A6158C(object) answers a table whose first word is the owner (none: func_08A5BB7C(D_08CB0284) first, the library's assertion);
// func_08A5D438(&a), func_08A77148(owner, &a, key), func_08A759E0(&b, key, 0), func_08A7B0E8(&c, &b, table, &a), func_08A61584(object, &c) and the destructors func_08A5B9EC(&c),
// func_08A69D68(&b), func_08A5B9EC(&a) on the frame's three temporaries.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): an owner of 0 (func_08A5BB7C(D_08CB0284) first; the `bnez` skips it).
void func_08C20838(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], object = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t key = self + 8u;
    const std::uint32_t temp_c = frame.at(4u), temp_a = frame.at(8u), temp_b = frame.at(0u);
    ctx.gpr[4] = object;
    const std::uint32_t table = call_keep(rt, ctx, 0x08A6158Cu, {object});
    (void)call_keep(rt, ctx, 0x08A5D438u, {temp_a});
    const std::uint32_t owner = word(rt, table);
    if (owner == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CB0284u});
    (void)call_keep(rt, ctx, 0x08A77148u, {owner, temp_a, key});
    (void)call_keep(rt, ctx, 0x08A759E0u, {temp_b, key, 0u});
    (void)call_keep(rt, ctx, 0x08A7B0E8u, {temp_c, temp_b, table, temp_a});
    (void)call_keep(rt, ctx, 0x08A61584u, {object, temp_c});
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {temp_c});
    (void)call_keep(rt, ctx, 0x08A69D68u, {temp_b});
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {temp_a});
}

// func_08B71CE0(value): a signed 32-bit integer to the library's soft-float double: a record {kind, sign, exponent, mantissa lo, mantissa hi} on the frame goes to func_08B72030.
// Zero: kind 2 (no other field). Otherwise kind 3, sign = bit 31, exponent 0x3C, mantissa = |value| (0x80000000 answers the constant double D_08CB6948/4C directly), shifted left
// until its high word exceeds 0x0FFFFFFF with the exponent decreasing by one per shift. v0 / v1 = the double.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): negative values (the sign `beqz` is not taken: negu, the sign-extended high word 0), the value 0x80000000 (the `beq` to the constant double D_08CB6948 / D_08CB694C, v0 / v1 loaded, no call) and a high word above 0x0FFFFFFF (the `bnez` straight to the call; unreachable for a 32-bit value).
void func_08B71CE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t value = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t sp = frame.at(0u);
    put(rt, sp, 3u);
    put(rt, sp + 4u, value >> 31);
    if (value == 0u) {
        put(rt, sp, 2u);
        (void)call_keep(rt, ctx, 0x08B72030u, {sp});
        return;
    }
    std::uint32_t exponent = 0x3Cu;
    put(rt, sp + 8u, exponent);
    std::uint32_t low, high;
    if ((value >> 31) != 0u) {
        if (value == 0x80000000u) {
            ctx.gpr[3] = word(rt, 0x08CB694Cu);
            ctx.gpr[2] = word(rt, 0x08CB6948u);
            return;
        }
        low = 0u - value;
        high = static_cast<std::uint32_t>(static_cast<std::int32_t>(low) >> 31);
        put(rt, sp + 0x10u, low);
        put(rt, sp + 0x14u, high);
    } else {
        low = value;
        high = static_cast<std::uint32_t>(static_cast<std::int32_t>(value) >> 31);
        put(rt, sp + 0x10u, value);
        put(rt, sp + 0x14u, high);
    }
    high = word(rt, sp + 0x14u);
    if (!(0x0FFFFFFFu < high)) {
        low = word(rt, sp + 0x10u);
        exponent = word(rt, sp + 8u);
        do {
            const std::uint32_t carry = low >> 31;
            const std::uint32_t next_high = carry | (high << 1);
            const std::uint32_t next_low = low << 1;
            --exponent;
            low = next_low;
            high = next_high;
            put(rt, sp + 0x10u, low);
            put(rt, sp + 0x14u, high);
            put(rt, sp + 8u, exponent);
        } while (!(0x0FFFFFFFu < high));
        ctx.gpr[5] = exponent;
        ctx.gpr[6] = low;
        ctx.gpr[7] = high;
    } else {
        ctx.gpr[5] = word(rt, sp + 8u);
        ctx.gpr[6] = word(rt, sp + 0x10u);
        ctx.gpr[7] = high;
    }
    (void)call_keep(rt, ctx, 0x08B72030u, {sp, ctx.gpr[5], ctx.gpr[6], ctx.gpr[7]});
}

// func_0895BFC4(this): for the four indices i = 0..3: s0 = func_08955300(this, i) (the result is kept) and func_089554A8(this, i) answers w. With t = [[this + 0xC] + 0x270 + (i >> 1) * 4 + 0xA8]
// the record s0 gets [s0 + 0x9C] = -t * ([s0 + 0x94] + [s0 + 0x98]) (stored once), then + t * ([w + 4] * [s0 + 0x48] - [w] * [s0 + 0x44]) (stored again); when [s0 + 0x34] is not zero the value
// loses [s0 + 0x90] if [s0 + 0x34] > 0 and gains it otherwise (a NaN takes the second way: both compares are false). v0 = w and v1 = the last table address as the instructions leave them.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a non-zero [s0 + 0x34] that is not above 0 (the `bc1f` taken: [s0 + 0x9C] = sum + [s0 + 0x90], the delay slot of the `bc1f` having loaded it); the routes only take the zero and the positive ones.
void func_0895BFC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t table = word(rt, self + 0xCu) + 0x270u;
    std::uint32_t entry_w = 0u;
    for (std::uint32_t i = 0; i != 4u; ++i) {
        const std::uint32_t s0 = call_keep(rt, ctx, 0x08955300u, {self, i});
        entry_w = call_keep(rt, ctx, 0x089554A8u, {self, i});
        const std::uint32_t row = ((i >> 1) << 2) + table;
        const float t = float_at(rt, row + 0xA8u);
        const float sum = float_at(rt, s0 + 0x94u) + float_at(rt, s0 + 0x98u);
        const float neg_t = fbits(bits_of(t) ^ 0x80000000u);
        float result = psp_mul(neg_t, sum);
        const float factor = float_at(rt, s0 + 0x34u);
        put_float(rt, s0 + 0x9Cu, result);
        const float cross = psp_mul(float_at(rt, entry_w + 4u), float_at(rt, s0 + 0x48u)) - psp_mul(float_at(rt, entry_w), float_at(rt, s0 + 0x44u));
        result = result + psp_mul(t, cross);
        put_float(rt, s0 + 0x9Cu, result);
        if (!(factor == 0.0f)) {
            if (0.0f < factor) put_float(rt, s0 + 0x9Cu, result - float_at(rt, s0 + 0x90u));
            else put_float(rt, s0 + 0x9Cu, result + float_at(rt, s0 + 0x90u));
        }
        ctx.gpr[3] = row;
    }
    ctx.gpr[2] = entry_w;
}

// func_08A430A8(this, a1, source): with *source == 0 the answer is func_08C1C5E0(this + 0x2C). Otherwise a temporary string (frame + 0xC, constructed by func_08B88F44) and a second object
// (frame + 0x10, func_08A40C24(&it, source)) feed func_08C1C1A8(this + 0x2C, &string); the answer (v0, and the low byte of v1) is kept in the frame, the second object is destroyed
// (func_08A5B9EC) and the temporary string releases its representation unless it is the shared empty one (func_08B60804(data - 4, -1) <= 0 frees it with func_08B8542C).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a zero *source (the `bnez` not taken: func_08C1C5E0(this + 0x2C) and the epilogue), the shared empty string (the `beq` to the end), and a release that frees the string (func_08B60804 answering 0 or less: func_08B8542C(rep, &frame)).
void func_08A430A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], source = ctx.gpr[6];
    GuestFrame frame(ctx, 0x50u);
    const std::uint32_t text = frame.at(0xCu), object = frame.at(0x10u), list = self + 0x2Cu;
    ctx.gpr[4] = text;
    if (word(rt, source) == 0u) {
        (void)call_keep(rt, ctx, 0x08C1C5E0u, {list});
        return;
    }
    (void)call_keep(rt, ctx, 0x08B88F44u, {text});
    (void)call_keep(rt, ctx, 0x08A40C24u, {object, source});
    (void)call_keep(rt, ctx, 0x08C1C1A8u, {list, text});
    const std::uint32_t low = ctx.gpr[2], high = ctx.gpr[3];
    put(rt, frame.at(0x24u), high);
    put(rt, frame.at(4u), low);
    rt.memory().store8(frame.at(8u), rt.memory().load8(frame.at(0x24u)));
    put(rt, frame.at(0x20u), low);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {object});
    const std::uint32_t chars = word(rt, text);
    const std::uint32_t rep = chars - 0xCu;
    ctx.gpr[2] = kStringEmpty;
    ctx.gpr[5] = 0xFFFFFFFFu;
    if (rep == kStringEmpty) return;
    const std::uint32_t count = call_keep(rt, ctx, 0x08B60804u, {chars - 4u, 0xFFFFFFFFu});
    if (static_cast<std::int32_t>(count) > 0) return;
    (void)call_keep(rt, ctx, 0x08B8542Cu, {rep, frame.at(0u)});
}

// func_089A5614(this, a1, a2, a3, t0): v0 = 1 unless the flags this[0x138] & 5 are exactly 1, a2 (as a byte) is zero, a3 and t0 (bytes) are non-zero and func_089A4250(this) answers 0; then a holder
// on the frame is filled by func_089E4C2C(&holder, this) (none: func_08A5BB7C(D_08CA4F08), the library's assertion), its virtual slot 6 runs with a1 and, when it answers non-zero, func_089A54A0(this, a1, &holder);
// the holder is destroyed with func_08A5B9EC and v0 is that call's answer. A non-zero answer of func_089A4250 is returned as is.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the three early exits after the flags test (a2 non-zero, a3 zero, t0 zero: the `bnez` / `beql` to the epilogue with v0 = 1), a null holder (func_08A5BB7C(D_08CA4F08)) and a virtual slot 6 that answers 0 (func_089A54A0 skipped); a flags test that fails (v0 = 1) and a non-zero answer of func_089A4250 are taken.
void func_089A5614(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], target = ctx.gpr[5];
    const std::uint32_t a2 = ctx.gpr[6] & 0xFFu, a3 = ctx.gpr[7] & 0xFFu, t0 = ctx.gpr[8] & 0xFFu;
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[6] = a2;
    ctx.gpr[7] = a3;
    ctx.gpr[8] = t0;
    ctx.gpr[2] = 1u;
    ctx.gpr[3] = word(rt, self + 0x138u) & 5u;
    if (ctx.gpr[3] != 1u) return;
    if (a2 != 0u || a3 == 0u || t0 == 0u) return;
    if (call_keep(rt, ctx, 0x089A4250u, {self}) != 0u) return;
    const std::uint32_t holder = frame.at(0u);
    (void)call_keep(rt, ctx, 0x089E4C2Cu, {holder, self});
    const std::uint32_t object = word(rt, holder);
    if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CA4F08u});
    if (call_keep(rt, ctx, word(rt, word(rt, object) + 0x18u), {object, target}) != 0u) (void)call_keep(rt, ctx, 0x089A54A0u, {self, target, holder});
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {holder});
}

// func_08B33E50(a0, a1, a2): a retry loop around two kernel-style calls. s0:s1 = func_08B33B30(a0, a1, 0, 0, 1); a negative high word returns the low word. Otherwise
// func_08B2DDE8() is read (s3), func_08B33D3C(a0, a1, a2) answers s2 and func_08B2DDE8() is read again: a negative s2 returns it; when the two readings are equal func_08B2DE00(a0) must answer
// non-zero to go on (zero returns s2). Then func_08B33B30(a0, -, s0, s1, 0) is asked again: a negative high word returns its low word, an answer equal to s0:s1 repeats the loop, any other returns 0x80020130.
void func_08B33E50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4], second = ctx.gpr[5], third = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[8] = 1u;
    ctx.gpr[7] = 0u;
    ctx.gpr[6] = 0u;
    GuestResult wide = call_keep(rt, ctx, 0x08B33B30u, {object, second, 0u, 0u, 1u}, {});
    const std::uint32_t low = wide.v0, high = wide.v1;
    std::uint32_t result = low;
    if (static_cast<std::int32_t>(high) < 0) {
        ctx.gpr[2] = result;
        return;
    }
    for (;;) {
        const std::uint32_t before = call_keep(rt, ctx, 0x08B2DDE8u, {});
        result = call_keep(rt, ctx, 0x08B33D3Cu, {object, second, third});
        const std::uint32_t after = call_keep(rt, ctx, 0x08B2DDE8u, {});
        if (static_cast<std::int32_t>(result) < 0) {
            ctx.gpr[2] = result;
            return;
        }
        if (before == after) {
            ctx.gpr[4] = object;
            if (call_keep(rt, ctx, 0x08B2DE00u, {object}) == 0u) {
                ctx.gpr[2] = result;
                return;
            }
        }
        ctx.gpr[4] = object;
        wide = call_keep(rt, ctx, 0x08B33B30u, {object, ctx.gpr[5], low, high, 0u}, {});
        if (static_cast<std::int32_t>(wide.v1) < 0) {
            ctx.gpr[2] = wide.v0;
            return;
        }
        ctx.gpr[2] = 0x80020000u;
        if (wide.v0 == low && wide.v1 == high) continue;
        ctx.gpr[2] = 0x80020130u;
        return;
    }
}

// func_08C21C84(this, object): func_08A76AA0(&a); the owner of the object's table [[object + 0x34] + 0x30] must exist (none: func_08A5BB7C(D_08CB02A4)); when its word +0x34 is 7 or more and
// func_08A6158C(object) answers something other than &a, func_08A5BAD8(&a, *answer) copies it. func_08A61624(object) answers s0; func_08A76ABC(&b, &a), func_08C203C8(s0, this + 8, &b) and the
// destructors func_08A5B9EC(&b) / func_08A5B9EC(&a) follow.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): an owner of 0 (assert), an owner word +0x34 below 7 (func_08A6158C is skipped) and func_08A6158C answering &a itself (the copy func_08A5BAD8 skipped); the routes always take the 7-or-more path with another table.
void func_08C21C84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], object = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t first = frame.at(4u), second = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08A76AA0u, {first});
    const std::uint32_t owner = word(rt, word(rt, object + 0x34u) + 0x30u);
    ctx.gpr[4] = 0x08CB02A4u;
    if (owner == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CB02A4u});
    if (!(static_cast<std::int32_t>(word(rt, owner + 0x34u)) < 7)) {
        const std::uint32_t found = call_keep(rt, ctx, 0x08A6158Cu, {object});
        if (found != first) (void)call_keep(rt, ctx, 0x08A5BAD8u, {first, word(rt, found)});
    }
    const std::uint32_t result = call_keep(rt, ctx, 0x08A61624u, {object});
    (void)call_keep(rt, ctx, 0x08A76ABCu, {second, first});
    (void)call_keep(rt, ctx, 0x08C203C8u, {result, self + 8u, second});
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {second});
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {first});
}

// func_08B661E8(x): ceilf for |x| below 2^23 and the pass-through of the larger ones (infinities and NaNs double: x + x). The "inexact" guard adds the huge constant D_08CB62AC to x and rounds
// only when the sum is positive; a fraction in (-1, 0) answers -0.0, one in (0, 1) answers 1.0, a zero stays. The result is in f0 (and f12).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): everything but the ceiling of a positive non-integer below 2^23: an exponent of 23 or more (pass-through; above 0x7F7FFFFF the `add.s f12, f12, f12` doubling of infinities and NaNs), a fraction that is zero (the integer returns at once), a probe sum x + D_08CB62AC that is not above 0, negative values (no 0x800000 >> e added, only the mask), and |x| < 1 (exponent below 0): -0.0 for a negative x, 1.0 for a positive one, the zero stays.
void func_08B661E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    float x = ctx.fpr[12];
    const std::uint32_t raw = bits_of(x);
    const std::uint32_t magnitude = raw & 0x7FFFFFFFu;
    const std::int32_t exponent = static_cast<std::int32_t>(magnitude >> 23) - 0x7F;
    if (exponent >= 0x17) {
        if (0x7F7FFFFFu < magnitude) x = x + x;
    } else if (exponent >= 0) {
        const std::uint32_t fraction_mask = 0x7FFFFFu >> exponent;
        if ((raw & fraction_mask) != 0u) {
            const float probe = x + float_at(rt, 0x08CB62ACu);
            if (0.0f < probe) {
                std::uint32_t result = raw;
                if (static_cast<std::int32_t>(result) > 0) result += 0x800000u >> exponent;
                result &= ~fraction_mask;
                x = fbits(result);
            }
        }
    } else {
        const float probe = x + float_at(rt, 0x08CB62ACu);
        if (0.0f < probe) {
            if (static_cast<std::int32_t>(raw) < 0) x = fbits(0x80000000u);
            else if (magnitude != 0u) x = fbits(0x3F800000u);
        }
    }
    ctx.fpr[12] = x;
    ctx.fpr[0] = x;
}

// func_08ACCE04(this): 0 when this[0x304] is zero. Otherwise the table of this[0x328] entries (stride 0xEC, flag byte at +0x415) and then the table of this[0x32C] entries (stride 0xF0, flag byte at
// +0x4E9) must have every flag byte non-zero for the answer 1; the first zero flag answers 0. An empty table counts as all set.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): everything past the first test: every compared case has this[0x304] == 0 and answers 0. The tables are read from the asm only: the 0xEC-stride flag bytes at +0x415 (count this[0x328]; an empty table jumps to the second count), the 0xF0-stride flag bytes at +0x4E9 (count this[0x32C]) — the first zero byte answers 0, an empty second table or all set answers 1 (the `addiu v0, 1` of the `blez` delay slot).
void func_08ACCE04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    if (rt.memory().load8(self + 0x304u) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const std::int32_t first_count = static_cast<std::int32_t>(word(rt, self + 0x328u));
    if (first_count > 0) {
        for (std::int32_t i = 0; i < first_count; ++i) {
            if (rt.memory().load8(self + 0x415u + 0xECu * static_cast<std::uint32_t>(i)) == 0u) {
                ctx.gpr[2] = 0u;
                return;
            }
        }
    }
    const std::int32_t second_count = static_cast<std::int32_t>(word(rt, self + 0x32Cu));
    if (second_count > 0) {
        for (std::int32_t i = 0; i < second_count; ++i) {
            if (rt.memory().load8(self + 0x4E9u + 0xF0u * static_cast<std::uint32_t>(i)) == 0u) {
                ctx.gpr[2] = 0u;
                return;
            }
        }
    }
    ctx.gpr[2] = 1u;
}

inline std::uint32_t bit_reverse(std::uint32_t value) {
    std::uint32_t result = 0u;
    for (unsigned i = 0; i < 32u; ++i) result |= ((value >> i) & 1u) << (31u - i);
    return result;
}
inline std::uint32_t byte_swap(std::uint32_t value) {
    return (value << 24) | ((value & 0xFF00u) << 8) | ((value >> 8) & 0xFF00u) | (value >> 24);
}

// func_08B3A37C(state): a generator step. The state word becomes next = state * 17 + 17; the answer mixes the byte-swapped bit reversal r of next: r ^ rotr(r * 257 + 0x101, 16)
// (r * 257 is (r << 8) + r). v1 = r as the instructions leave it.
void func_08B3A37C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t state = ctx.gpr[4];
    const std::uint32_t current = word(rt, state);
    const std::uint32_t next = (current << 4) + current + 0x11u;
    const std::uint32_t r = byte_swap(bit_reverse(next));
    std::uint32_t mixed = (r << 8) + r + 0x101u;
    mixed = (mixed >> 16) | (mixed << 16);
    ctx.gpr[3] = r;
    ctx.gpr[2] = r ^ mixed;
    put(rt, state, next);
}

// func_08BE6674(dest, count, source): count records of 0x30 bytes from dest (a null dest pointer is skipped, the pointer still advances) each get the same 0x30-byte record copy of source:
// words 4, 8, 0xC .. 0x1C, then the vtable pointer D_08CCDE00 at +0, then words 0x20, 0x24, 0x28, 0x2C (loads and stores in the asm's order).
// v0 = the high half of the vtable's address (0x08CD0000, the delay slot of the first test) and, once a record was copied, the words [source + 4] / [source + 8] (v0 / v1).
void func_08BE6674(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t check = ctx.gpr[4], dest = ctx.gpr[4];
    const std::uint32_t count = ctx.gpr[5], source = ctx.gpr[6];
    ctx.gpr[2] = 0x08CD0000u;  // the `lui v0` in the delay slot of the first `beqz` runs on every path
    if (count == 0u) return;
    for (std::uint32_t i = 1; i <= count; ++i) {
        if (check != 0u) {
            const std::uint32_t w0C = word(rt, source + 0xCu), w10 = word(rt, source + 0x10u);
            ctx.gpr[2] = word(rt, source + 4u);
            ctx.gpr[3] = word(rt, source + 8u);
            const std::uint32_t w14 = word(rt, source + 0x14u), w18 = word(rt, source + 0x18u), w1C = word(rt, source + 0x1Cu);
            put(rt, dest + 4u, ctx.gpr[2]);
            put(rt, dest + 8u, ctx.gpr[3]);
            put(rt, dest + 0xCu, w0C);
            put(rt, dest + 0x10u, w10);
            put(rt, dest + 0x14u, w14);
            put(rt, dest + 0x18u, w18);
            put(rt, dest + 0x1Cu, w1C);
            put(rt, dest, 0x08CCDE00u);
            put(rt, dest + 0x20u, word(rt, source + 0x20u));
            put(rt, dest + 0x24u, word(rt, source + 0x24u));
            put(rt, dest + 0x28u, word(rt, source + 0x28u));
            put(rt, dest + 0x2Cu, word(rt, source + 0x2Cu));
        }
        check += 0x30u;
        dest += 0x30u;
    }
}

// func_08C21048(this, object): a 0x18-byte block from func_08A5BC20(0x18, 4) (initialised by func_08A76ACC) gets the vtable D_08CB6B88; func_08A6972C(block + 0x14, this + 8) fills its key;
// the frame holder {block} is passed to func_08A76AB4(&copy, &holder), func_08A61584(object, &copy) takes it and func_08A5B9EC(&copy) destroys the copy.
void func_08C21048(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], object = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t copy = frame.at(4u), holder = frame.at(0u);
    const std::uint32_t block = call_keep(rt, ctx, 0x08A5BC20u, {0x18u, 4u});
    (void)call_keep(rt, ctx, 0x08A76ACCu, {block});
    put(rt, block, 0x08CB6B88u);
    (void)call_keep(rt, ctx, 0x08A6972Cu, {block + 0x14u, self + 8u});
    put(rt, holder, block);
    (void)call_keep(rt, ctx, 0x08A76AB4u, {copy, holder});
    (void)call_keep(rt, ctx, 0x08A61584u, {object, copy});
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {copy});
}

// func_08B7B9AC(dest): strcat. The end of the string at dest (the first zero byte; the asm tests four bytes at a time once dest is aligned, which finds the same byte) is passed to
// func_08B7BB6C(end, src) (a1 stays the caller's); the answer is dest.
void func_08B7B9AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t dest = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    std::uint32_t end = dest;
    while (rt.memory().load8(end) != 0u) ++end;
    (void)call_keep(rt, ctx, 0x08B7BB6Cu, {end});
    ctx.gpr[2] = dest;
}

// func_08C1CDCC(this, other): string compare. s2 = this[0] (the character data; its length is the word at data - 0xC), n = func_08B35F60(other) the other's length; r = func_08B36170(data, other, min(length, n)).
// A non-zero r is the answer, otherwise length - n.
void func_08C1CDCC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], other = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t data = word(rt, self);
    const std::uint32_t length = word(rt, data - 0xCu);
    ctx.gpr[4] = other;
    put(rt, frame.at(0u), length);
    const std::uint32_t other_length = call_keep(rt, ctx, 0x08B35F60u, {other});
    put(rt, frame.at(4u), other_length);
    const std::uint32_t shorter = word(rt, other_length < length ? frame.at(4u) : frame.at(0u));
    const std::uint32_t difference = length - other_length;
    const std::uint32_t result = call_keep(rt, ctx, 0x08B36170u, {data, other, shorter});
    ctx.gpr[2] = result != 0u ? result : difference;
}

// func_08C217F4(this, object): the owner word [func_08A6158C(object)] must exist (none: func_08A5BB7C(D_08CB0284)); when its virtual slot 0xC answers non-zero the tail call
// func_08A61DD4(object, this[2]) runs, otherwise v0 = 0.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): an owner of 0 (func_08A5BB7C(D_08CB0284) first).
void func_08C217F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], object = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        const std::uint32_t table = call_keep(rt, ctx, 0x08A6158Cu, {object});
        const std::uint32_t owner = word(rt, table);
        ctx.gpr[4] = 0x08CB0284u;
        if (owner == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CB0284u});
        const std::uint32_t slot = word(rt, word(rt, owner) + 0x30u);
        if (call_keep(rt, ctx, slot, {owner}) == 0u) return;
    }
    tail_call(ctx, 0x08A61DD4u, {object, word(rt, self + 8u)});
}

// func_08B8543C(wanted, old): the string representation allocator (basic_string::_Rep::_S_create): more than 0x3FFFFFFC characters throw (func_08B5ADDC(D_08C97B8C)); when old < wanted
// the capacity grows to twice old if that is more; sizes of a page or more (0x1001 with the header) that grow beyond old are rounded up to a page boundary (capped at 0x3FFFFFFC, size 0x40000009).
// func_08A5BC20(size + 0xD, 4) allocates and the capacity is stored at +4. v0 = the block.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a wanted size above 0x3FFFFFFC (the throw through func_08B5ADDC) and every size of a page or more (padded >= 0x1001 with old below the capacity: the page rounding and its cap at 0x3FFFFFFC with the size 0x40000009); the routes only allocate small blocks.
void func_08B8543C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t wanted = ctx.gpr[4], old = ctx.gpr[5];
    constexpr std::uint32_t kLimit = 0x3FFFFFFCu;
    GuestFrame frame(ctx, 0x10u);
    std::uint32_t capacity = wanted;
    ctx.gpr[6] = kLimit;
    if (kLimit < wanted) (void)call_keep(rt, ctx, 0x08B5ADDCu, {0x08C97B8Cu});
    std::uint32_t size = capacity + 0xDu;
    std::uint32_t padded = capacity + 0x1Du;
    if (old < ctx.gpr[4]) {
        const std::uint32_t doubled = old << 1;
        if (ctx.gpr[4] < doubled) capacity = doubled;
        padded = capacity + 0x1Du;
        size = capacity + 0xDu;
    }
    if (!(padded < 0x1001u) && old < capacity) {
        capacity += 0x1000u - (padded & 0xFFFu);
        size = capacity + 0xDu;
        if (kLimit < capacity) {
            capacity = kLimit;
            size = 0x40000009u;
        }
    }
    const std::uint32_t block = call_keep(rt, ctx, 0x08A5BC20u, {size, 4u});
    put(rt, block + 4u, capacity);
}

// func_08BE834C(dest, count, source): count records of 0x40 bytes are copy-constructed from source (not advanced: every record gets the same copy): words 4, 8, 0xC .. 0x1C, the vtable D_08CCFC90 at +0
// and the sub-objects at +0x20 and +0x30 by func_089FA280(dest + k, source + k). A null dest pointer is skipped (the pointer still advances).
void func_08BE834C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t dest = ctx.gpr[4];
    const std::uint32_t count = ctx.gpr[5], source = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    if (count == 0u) return;
    for (std::uint32_t i = 1; i <= count; ++i, dest += 0x40u) {
        if (dest == 0u) continue;
        const std::uint32_t w4 = word(rt, source + 4u), w8 = word(rt, source + 8u), wC = word(rt, source + 0xCu), w10 = word(rt, source + 0x10u);
        const std::uint32_t w14 = word(rt, source + 0x14u), w18 = word(rt, source + 0x18u), w1C = word(rt, source + 0x1Cu);
        ctx.gpr[2] = w4;
        ctx.gpr[3] = w8;
        put(rt, dest + 4u, w4);
        put(rt, dest + 8u, w8);
        put(rt, dest + 0xCu, wC);
        put(rt, dest + 0x10u, w10);
        put(rt, dest + 0x14u, w14);
        put(rt, dest + 0x18u, w18);
        put(rt, dest + 0x1Cu, w1C);
        put(rt, dest, 0x08CCFC90u);
        (void)call_keep(rt, ctx, 0x089FA280u, {dest + 0x20u, source + 0x20u});
        (void)call_keep(rt, ctx, 0x089FA280u, {dest + 0x30u, source + 0x30u});
    }
}

// func_08B7B2E4(buffer): setjmp. The callee-saved registers s0-s7, sp, fp, ra and f20-f31 go to buffer + 0x00 .. 0x58 in that order (the float registers as their bits); v0 = 0.
void func_08B7B2E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t buffer = ctx.gpr[4];
    for (std::uint32_t i = 0; i < 8u; ++i) put(rt, buffer + 4u * i, ctx.gpr[16u + i]);
    put(rt, buffer + 0x20u, ctx.gpr[29]);
    put(rt, buffer + 0x24u, ctx.gpr[30]);
    put(rt, buffer + 0x28u, ctx.gpr[31]);
    for (std::uint32_t i = 0; i < 12u; ++i) put(rt, buffer + 0x2Cu + 4u * i, bits_of(ctx.fpr[20u + i]));
    ctx.gpr[2] = 0u;
}

// func_08B18088(this): block = this + 0x182C. func_08B34888(block) runs; when func_08B1A844() answers non-zero this[0x1858] = 0 and func_08B34CA0(block) runs; otherwise a non-zero
// this[0x1858] is decremented (stored before the call) and func_08B34C1C(block) runs. The tail call is func_08B34B24(block).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): func_08B1A844() answering non-zero (this[0x1858] = 0 and func_08B34CA0) and a zero counter this[0x1858] (nothing decremented, no func_08B34C1C); the routes take the answer 0 with a non-zero counter.
void func_08B18088(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t block = self + 0x182Cu;
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[4] = block;
        (void)call_keep(rt, ctx, 0x08B34888u, {block});
        if (call_keep(rt, ctx, 0x08B1A844u, {}) != 0u) {
            put(rt, self + 0x1858u, 0u);
            (void)call_keep(rt, ctx, 0x08B34CA0u, {block});
        } else {
            const std::uint32_t count = word(rt, self + 0x1858u);
            ctx.gpr[2] = count;
            if (count != 0u) {
                put(rt, self + 0x1858u, count - 1u);
                (void)call_keep(rt, ctx, 0x08B34C1Cu, {block});
            }
        }
    }
    tail_call(ctx, 0x08B34B24u, {block});
}

// func_08B34CA0(queue): interrupts are suspended (import sceKernelCpuSuspendIntr, flags kept). When the word queue[8] is set and the current thread func_08B2E6DC() is the owner queue[6]:
// the words queue[2] (head) and queue[3] (tail) are cleared; a non-null head is linked after the last node queue[5] (queue[4] = head when there was none) and queue[5] = the old tail.
// The interrupts resume with a tail call to sceKernelCpuResumeIntr(flags).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the word +0x20 being zero (v0 = 0 to the tail call), a current thread different from the owner at +0x18 (v0 = that word) and a non-null last-node pointer +0x14 (the head stored through it: put(last, head)); the routes always find it null and store the head to +0x10.
void func_08B34CA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t queue = ctx.gpr[4];
    std::uint32_t flags;
    {
        GuestFrame frame(ctx, 0x10u);
        const std::uint32_t current = call_keep(rt, ctx, 0x08B2E6DCu, {});
        flags = call_keep(rt, ctx, 0x08C7A888u, {});
        // v0 at the tail call is whatever the last load left (the import stub does not set it): the word +0x20, then +0x18, then +0x14.
        ctx.gpr[2] = word(rt, queue + 0x20u);
        if (ctx.gpr[2] != 0u) {
            ctx.gpr[2] = word(rt, queue + 0x18u);
            if (current == ctx.gpr[2]) {
                const std::uint32_t head = word(rt, queue + 8u);
                const std::uint32_t tail = word(rt, queue + 0xCu);
                put(rt, queue + 8u, 0u);
                put(rt, queue + 0xCu, 0u);
                if (head != 0u) {
                    const std::uint32_t last = word(rt, queue + 0x14u);
                    ctx.gpr[2] = last;
                    if (last == 0u) put(rt, queue + 0x10u, head);
                    else put(rt, last, head);
                    put(rt, queue + 0x14u, tail);
                }
            }
        }
    }
    tail_call(ctx, 0x08C7A8A8u, {flags});
}

// func_08C297C8(this): func_08A98C1C(&holder, 2) fills a holder on the frame; for three string-like members the target takes the holder's value unless it already is it:
// func_08A5BAD8(holder[0x14], this[0x14][0x10]) when holder[0x14] differs from this[0x14] + 0x10; then func_08A5BAD8(holder[0x14] + 4, this[0x14][0x14]) (unless equal to this[0x14] + 0x14)
// and func_08A5BAD8(this + 4, holder[0]) (unless this + 4 is the holder itself). this[0x14] = func_08B5F16C(this[0x14]) and func_08A5B9EC(&holder) ends.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the equal cases: the holder's target already being this[0x14] + 0x10, its +4 word already being this[0x14] + 0x14, and this + 4 being the holder itself (each skips its func_08A5BAD8 call).
void func_08C297C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t holder = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08A98C1Cu, {holder, 2u});
    std::uint32_t outer = word(rt, holder);
    const std::uint32_t member = word(rt, self + 0x14u);
    std::uint32_t target = word(rt, outer + 0x14u);
    if (target != member + 0x10u) {
        (void)call_keep(rt, ctx, 0x08A5BAD8u, {target, word(rt, member + 0x10u)});
        outer = word(rt, holder);
        target = word(rt, outer + 0x14u);
    }
    const std::uint32_t second = word(rt, self + 0x14u);
    target += 4u;
    if (target != second + 0x14u) (void)call_keep(rt, ctx, 0x08A5BAD8u, {target, word(rt, second + 0x14u)});
    if (self + 4u != holder) (void)call_keep(rt, ctx, 0x08A5BAD8u, {self + 4u, word(rt, holder)});
    const std::uint32_t renewed = call_keep(rt, ctx, 0x08B5F16Cu, {word(rt, self + 0x14u)});
    put(rt, self + 0x14u, renewed);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {holder});
}

// func_08B2432C(this, buffer, length): drains a buffer source. Each round locks (func_08B344E0(this + 0x1C)); this[0x9C] (a stop flag) set or length == 0 ends it. Otherwise
// n = func_08B24B4C(this, buffer, length) is taken: length -= n, total += n, buffer += n * this[0x14] (the byte stride); with the stop flag set after the call or n == 0 the
// func_08B28C74(this + 0x70) wake-up runs; func_08B3457C(this + 0x1C) unlocks and func_08B28C44(this + 0x44) signals before the next round. v0 = the total.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): everything after the first round: every compared case drains the whole length at once (remaining == 0 after func_08B24B4C, the `beqz` to the unlock) or sees the stop flag. The continuing round is read from the asm only: buffer += n * this[0x14] (mtlo / madd / mflo, the low 32 bits), the stop flag re-read after the call (set: the wake-up only when n == 0), func_08B3457C, func_08B28C74(this + 0x70) and func_08B28C44(this + 0x44), then the jump back.
void func_08B2432C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t cursor = ctx.gpr[5], remaining = ctx.gpr[6], total = 0u;
    const std::uint32_t lock = self + 0x1Cu;
    GuestFrame frame(ctx, 0x20u);
    for (;;) {
        (void)call_keep(rt, ctx, 0x08B344E0u, {lock});
        std::uint32_t stopped = rt.memory().load8(self + 0x9Cu);
        std::uint32_t wake = 1u;
        if (stopped != 0u) break;
        const std::uint32_t taken = call_keep(rt, ctx, 0x08B24B4Cu, {self, cursor, remaining});
        remaining -= taken;
        total += taken;
        if (remaining == 0u) break;
        const std::uint32_t stride = word(rt, self + 0x14u);
        stopped = rt.memory().load8(self + 0x9Cu);
        cursor += taken * stride;
        if (stopped != 0u) wake = taken == 0u ? 1u : 0u;
        (void)call_keep(rt, ctx, 0x08B3457Cu, {lock});
        if (wake != 0u) (void)call_keep(rt, ctx, 0x08B28C74u, {self + 0x70u});
        (void)call_keep(rt, ctx, 0x08B28C44u, {self + 0x44u});
    }
    (void)call_keep(rt, ctx, 0x08B3457Cu, {lock});
    ctx.gpr[2] = total;
}

// func_08B2440C(this, buffer, length): the reading twin of func_08B2432C. this[0x9C] (a stop flag) set answers 0 at once. Each round locks (func_08B344E0(this + 0x1C)); with the flag set the round
// unlocks and ends. Otherwise n = func_08B24C1C(this, buffer, length): length -= n, total += n; when nothing is left it unlocks and signals func_08B28C74(this + 0x44) (only if n != 0); else
// buffer += n * this[0x14], unlock, func_08B28C74(this + 0x44), func_08B28C44(this + 0x70) and the next round, or the answer when the flag has been set meanwhile. v0 = the total.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the stop flag being set at entry (v0 = 0) or inside a round, a round that leaves length over (remaining != 0: buffer += n * this[0x14], unlock, func_08B28C74(this + 0x44), func_08B28C44(this + 0x70), the next round or the exit when the flag was set meanwhile) and n == 0 (the signal skipped); every compared case drains the length at once with n != 0.
void func_08B2440C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t cursor = ctx.gpr[5], remaining = ctx.gpr[6], total = 0u;
    const std::uint32_t lock = self + 0x1Cu;
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[3] = 0u;
    if (rt.memory().load8(self + 0x9Cu) != 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    for (;;) {
        (void)call_keep(rt, ctx, 0x08B344E0u, {lock});
        std::uint32_t signal = 0u;
        if (rt.memory().load8(self + 0x9Cu) == 0u) {
            const std::uint32_t taken = call_keep(rt, ctx, 0x08B24C1Cu, {self, cursor, remaining});
            ctx.gpr[3] = taken;
            remaining -= taken;
            total += taken;
            if (remaining != 0u) {
                const std::uint32_t stride = word(rt, self + 0x14u);
                cursor += taken * stride;
                (void)call_keep(rt, ctx, 0x08B3457Cu, {lock});
                (void)call_keep(rt, ctx, 0x08B28C74u, {self + 0x44u});
                (void)call_keep(rt, ctx, 0x08B28C44u, {self + 0x70u});
                ctx.gpr[3] = total;
                if (rt.memory().load8(self + 0x9Cu) == 0u) continue;
                ctx.gpr[2] = total;
                return;
            }
            signal = taken != 0u ? 1u : 0u;
        }
        (void)call_keep(rt, ctx, 0x08B3457Cu, {lock});
        ctx.gpr[3] = total;
        if (signal != 0u) {
            (void)call_keep(rt, ctx, 0x08B28C74u, {self + 0x44u});
            ctx.gpr[3] = total;
        }
        ctx.gpr[2] = total;
        return;
    }
}

// func_08B13278(this, name, object): object (or, when it is 0, func_08B17B84()'s answer; none answers 0) gets object[0xE] = this after func_08B17A20(object) when it was given;
// n = func_08B35F60(name) + 2 bytes from func_08B17A88 are attached with func_08B17AE4(object, buffer); the frame record {func_08B17A98(buffer), n, 0} gets its first byte cleared,
// func_08B23928(&record, func_08B237D8(this[0x6B], name)) names it, func_08B17AA8(buffer) releases and func_08B10BB4(this, object) registers. v0 = object.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the object argument being zero (func_08B17B84() allocates it; its null answer returns 0). The routes always pass an object.
void func_08B13278(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], name = ctx.gpr[5];
    std::uint32_t object = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    ctx.gpr[4] = object;
    if (object != 0u) {
        (void)call_keep(rt, ctx, 0x08B17A20u, {object});
    } else {
        object = call_keep(rt, ctx, 0x08B17B84u, {});
        if (object == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
    }
    put(rt, object + 0x38u, self);
    const std::uint32_t length = call_keep(rt, ctx, 0x08B35F60u, {name});
    const std::uint32_t size = length + 2u;
    const std::uint32_t buffer = call_keep(rt, ctx, 0x08B17A88u, {size});
    (void)call_keep(rt, ctx, 0x08B17AE4u, {object, buffer});
    const std::uint32_t text = call_keep(rt, ctx, 0x08B17A98u, {buffer});
    put(rt, frame.at(0u), text);
    put(rt, frame.at(4u), size);
    put(rt, frame.at(8u), 0u);
    rt.memory().store8(text, 0u);
    const std::uint32_t found = call_keep(rt, ctx, 0x08B237D8u, {word(rt, self + 0x1ACu), name});
    (void)call_keep(rt, ctx, 0x08B23928u, {frame.at(0u), found});
    (void)call_keep(rt, ctx, 0x08B17AA8u, {buffer});
    (void)call_keep(rt, ctx, 0x08B10BB4u, {self, object});
    ctx.gpr[2] = object;
}

// func_08C20224(this, object): func_08A6159C(object, 2) answers s1 and func_08A76ABC(&a, s1) fills the frame holder a; its object (none: func_08A5BB7C(D_08CB0284)) runs virtual slot 0x30
// with s1. func_08A76AA0(&b), func_08A7858C(&b, this + 8, 2, s1) and the object of a (again checked) virtual slot 0x31 with &b follow; func_08A61584(object, &b) and the destructors
// func_08A5B9EC(&b) / func_08A5B9EC(&a) end it.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): an owner of 0 in either of the two checks (func_08A5BB7C(D_08CB0284) first).
void func_08C20224(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], object = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t first = frame.at(0u), second = frame.at(4u);
    const std::uint32_t found = call_keep(rt, ctx, 0x08A6159Cu, {object, 2u});
    (void)call_keep(rt, ctx, 0x08A76ABCu, {first, found});
    std::uint32_t owner = word(rt, first);
    ctx.gpr[4] = 0x08CB0284u;
    if (owner == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CB0284u});
    (void)call_keep(rt, ctx, word(rt, word(rt, owner) + 0xC0u), {owner, found});
    (void)call_keep(rt, ctx, 0x08A76AA0u, {second});
    (void)call_keep(rt, ctx, 0x08A7858Cu, {second, self + 8u, 2u, found});
    owner = word(rt, first);
    ctx.gpr[4] = 0x08CB0284u;
    if (owner == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CB0284u});
    (void)call_keep(rt, ctx, word(rt, word(rt, owner) + 0xC4u), {owner, second});
    (void)call_keep(rt, ctx, 0x08A61584u, {object, second});
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {second});
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {first});
}

// Release of a temporary string (data at [string]; representation at data - 0xC): unless it is the shared empty one the count at data - 4 loses a reference
// (func_08B60804) and a representation left with none is freed (func_08B8542C(rep, &frame)). The three functions below end with this same tail.
inline void release_string(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t chars, std::uint32_t frame_address) {
    const std::uint32_t rep = chars - 0xCu;
    ctx.gpr[2] = kStringEmpty;
    ctx.gpr[5] = 0xFFFFFFFFu;
    if (rep == kStringEmpty) return;
    const std::uint32_t count = call_keep(rt, ctx, 0x08B60804u, {chars - 4u, 0xFFFFFFFFu});
    if (static_cast<std::int32_t>(count) > 0) return;
    (void)call_keep(rt, ctx, 0x08B8542Cu, {rep, frame_address});
}

// func_08C1BE44(map, force_left, hint, key): inserts a node into the red-black tree whose header is map + 4. The node goes left (flag 1) when force_left is set, the hint is the header, or the key string
// compares below the hint's key (func_08B36170 on the lengths' minimum, the length difference breaking a tie); a node of 0x18 bytes is allocated (func_08A5BC20(0x18, 4)), its key copy-constructed
// (func_08B88F44(node + 0x10, key), func_08A40C44(node + 0x14, key + 4)), func_08B5F2B8(flag, node, hint, header) links and rebalances it, map[5] is incremented. v0 = the node.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): force_left set or the hint being the header (the node goes left without a comparison, s6 = 1), a comparison that answers 0 (the length difference decides) and a null allocation (the key copy is skipped).
void func_08C1BE44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t map = ctx.gpr[4], force_left = ctx.gpr[5], hint = ctx.gpr[6], key = ctx.gpr[7];
    const std::uint32_t header = map + 4u;
    GuestFrame frame(ctx, 0x30u);
    std::uint32_t left = 1u;
    if (force_left == 0u && hint != header) {
        left = 0u;
        const std::uint32_t data = word(rt, key);
        const std::uint32_t hint_key = word(rt, hint + 0x10u);
        std::uint32_t difference = word(rt, data - 0xCu);
        put(rt, frame.at(0u), difference);
        const std::uint32_t hint_length = word(rt, hint_key - 0xCu);
        const std::uint32_t shorter = hint_length < difference ? hint_length : difference;
        put(rt, frame.at(4u), hint_length);
        difference -= hint_length;
        const std::uint32_t result = call_keep(rt, ctx, 0x08B36170u, {data, hint_key, shorter});
        if (result != 0u) difference = result;
        if (static_cast<std::int32_t>(difference) < 0) left = 1u;
    }
    const std::uint32_t node = call_keep(rt, ctx, 0x08A5BC20u, {0x18u, 4u});
    if (node + 0x10u != 0u) {
        (void)call_keep(rt, ctx, 0x08B88F44u, {node + 0x10u, key});
        (void)call_keep(rt, ctx, 0x08A40C44u, {node + 0x14u, key + 4u});
    }
    (void)call_keep(rt, ctx, 0x08B5F2B8u, {left, node, hint, header});
    put(rt, map + 0x14u, word(rt, map + 0x14u) + 1u);
    ctx.gpr[2] = node;
}

// func_08C338D4(map, key): map operator[] for a string key. func_08C31D6C() answers the lower bound node; when it is not the header (map + 4) and the key does not compare below that node's key
// (func_08B36170 on the lengths' minimum, the length difference breaking a tie) the answer is node + 0x14. Otherwise a temporary key string (func_08B88F44(&temp, key), value word 0) is
// inserted by func_08C335FC(map, node, &temp), the temporary is released and the answer is the new node + 0x14.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the shared empty temporary string (release_string returns at once) and a release that frees the string (func_08B60804 answering 0 or less: func_08B8542C(rep, &frame)).
void func_08C338D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t map = ctx.gpr[4], key = ctx.gpr[5];
    GuestFrame frame(ctx, 0x40u);
    const std::uint32_t temp = frame.at(0xCu);
    std::uint32_t node = call_keep(rt, ctx, 0x08C31D6Cu, {});
    const std::uint32_t found = node;
    if (map + 4u != found) {
        const std::uint32_t data = word(rt, key);
        std::uint32_t difference = word(rt, data - 0xCu);
        put(rt, frame.at(4u), difference);
        const std::uint32_t node_key = word(rt, found + 0x10u);
        const std::uint32_t node_length = word(rt, node_key - 0xCu);
        put(rt, frame.at(8u), node_length);
        const std::uint32_t shorter = word(rt, node_length < difference ? frame.at(8u) : frame.at(4u));
        difference -= node_length;
        const std::uint32_t result = call_keep(rt, ctx, 0x08B36170u, {word(rt, key), word(rt, found + 0x10u), shorter});
        if (result != 0u) difference = result;
        if (static_cast<std::int32_t>(difference) >= 0) {
            ctx.gpr[2] = found + 0x14u;
            return;
        }
    }
    (void)call_keep(rt, ctx, 0x08B88F44u, {temp, key});
    put(rt, frame.at(0x10u), 0u);
    node = call_keep(rt, ctx, 0x08C335FCu, {map, found, temp});
    release_string(rt, ctx, word(rt, temp), frame.at(0u));
    ctx.gpr[2] = node + 0x14u;
}

// func_08C33F44(out, a1, source): source[0] must be set (none: func_08A5BB7C(D_08CB30B8)); its virtual slot 3 fills the frame string (&temp, object), func_08C33F1C(a1, &temp) consumes it
// and the temporary string is released. v0 as the last call leaves it.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a null source object (func_08A5BB7C(D_08CB30B8) first), the shared empty temporary string and a release that frees it (func_08B8542C).
void func_08C33F44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t second = ctx.gpr[5], source = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t temp = frame.at(4u);
    const std::uint32_t object = word(rt, source);
    ctx.gpr[4] = 0x08CB30B8u;
    if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CB30B8u});
    (void)call_keep(rt, ctx, word(rt, word(rt, object) + 0xCu), {temp, object});
    (void)call_keep(rt, ctx, 0x08C33F1Cu, {second, temp});
    release_string(rt, ctx, word(rt, temp), frame.at(0u));
}

// func_08C218D0(this, object): func_08A76AA0(&a); the owner func_08A61624(object) and func_08A7DBB0(owner, &a, this + 8), func_08A76AB4(&b, D_08CF6ED8) prepare a second holder b; when
// func_08A7C28C(&b, &a) answers non-zero func_08A7E99C(owner, &b, this + 0x14, this + 0x18) runs; the destructors func_08A5B9EC(&b) / func_08A5B9EC(&a) end it.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): func_08A7C28C answering 0 (func_08A7E99C skipped).
void func_08C218D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], object = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t first = frame.at(0u), second = frame.at(4u);
    (void)call_keep(rt, ctx, 0x08A76AA0u, {first});
    const std::uint32_t owner = call_keep(rt, ctx, 0x08A61624u, {object});
    (void)call_keep(rt, ctx, 0x08A7DBB0u, {owner, first, self + 8u});
    (void)call_keep(rt, ctx, 0x08A76AB4u, {second, 0x08CF6ED8u});
    const std::uint32_t answer = call_keep(rt, ctx, 0x08A7C28Cu, {second, first});
    if (answer != 0u) (void)call_keep(rt, ctx, 0x08A7E99Cu, {owner, second, self + 0x14u, self + 0x18u});
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {second});
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {first});
}

// func_08933FB8(this): asks func_089368DC(func_089363FC(), -, this[0x36], this[0x37]); when it answers 0 the result is 0. Otherwise func_08936584(func_089363FC(), -, this[0x36], this[0x37], &frame)
// fills five byte pairs on the frame and four func_089324EC(this, 1, high, low) calls (bytes 7/4, 0xD/0xA, 0x12/0xF, 0x18/0x15) store their answers as bytes to this[0x14F .. 0x152]; v0 = 1.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): func_089368DC answering 0 (v0 = v1 = 0, the byte stores skipped).
void func_08933FB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t first = word(rt, self + 0xD8u);
    const std::uint32_t second = word(rt, self + 0xDCu);
    std::uint32_t source = call_keep(rt, ctx, 0x089363FCu, {self});
    if (call_keep(rt, ctx, 0x089368DCu, {source, ctx.gpr[5], first, second}) == 0u) {
        ctx.gpr[3] = 0u;
        ctx.gpr[2] = 0u;
        return;
    }
    source = call_keep(rt, ctx, 0x089363FCu, {self});
    (void)call_keep(rt, ctx, 0x08936584u, {source, ctx.gpr[5], first, second, frame.at(0u)});
    const auto byte_of = [&](std::uint32_t offset) { return static_cast<std::uint32_t>(rt.memory().load8(frame.at(offset))); };
    ctx.gpr[6] = byte_of(7u);
    ctx.gpr[7] = byte_of(4u);
    std::uint32_t answer = call_keep(rt, ctx, 0x089324ECu, {self, 1u, ctx.gpr[6], ctx.gpr[7]});
    ctx.gpr[6] = byte_of(0xDu);
    ctx.gpr[7] = byte_of(0xAu);
    rt.memory().store8(self + 0x14Fu, static_cast<std::uint8_t>(answer));
    answer = call_keep(rt, ctx, 0x089324ECu, {self, 1u, ctx.gpr[6], ctx.gpr[7]});
    ctx.gpr[6] = byte_of(0x12u);
    ctx.gpr[7] = byte_of(0xFu);
    rt.memory().store8(self + 0x150u, static_cast<std::uint8_t>(answer));
    answer = call_keep(rt, ctx, 0x089324ECu, {self, 1u, ctx.gpr[6], ctx.gpr[7]});
    ctx.gpr[6] = byte_of(0x18u);
    ctx.gpr[7] = byte_of(0x15u);
    rt.memory().store8(self + 0x151u, static_cast<std::uint8_t>(answer));
    answer = call_keep(rt, ctx, 0x089324ECu, {self, 1u, ctx.gpr[6], ctx.gpr[7]});
    ctx.gpr[3] = 1u;
    rt.memory().store8(self + 0x152u, static_cast<std::uint8_t>(answer));
    ctx.gpr[2] = 1u;
}

// func_08C15E8C(vector, first, last): erase for 0x2C-byte elements. The elements from last to the end (count = ((end - last) >> 2) * 0xBA2E8BA3, the inverse of 11) move down to first; each
// element copies words 0 and 1, words 2..5 and 6..9 (skipped when source and destination are the same), then word 10. func_08C15E7C(new end, end, &frame) destroys the tail and the end
// pointer vector[1] loses (last - first) & ~3 bytes. v0 = first.
void func_08C15E8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t vector = ctx.gpr[4], first = ctx.gpr[5], last = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t end_field = vector + 4u;
    const std::int32_t count = static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::int32_t>(word(rt, end_field) - last) >> 2) * 0xBA2E8BA3u);
    std::uint32_t destination = first, source = last;
    for (std::int32_t i = 1; i <= count; ++i) {
        const std::uint32_t w0 = word(rt, source), w1 = word(rt, source + 4u);
        put(rt, destination, w0);
        put(rt, destination + 4u, w1);
        if (destination + 8u != source + 8u) {
            const std::uint32_t w5 = word(rt, source + 0x14u), w2 = word(rt, source + 8u), w3 = word(rt, source + 0xCu), w4 = word(rt, source + 0x10u);
            put(rt, destination + 0x14u, w5);
            put(rt, destination + 8u, w2);
            put(rt, destination + 0xCu, w3);
            put(rt, destination + 0x10u, w4);
        }
        if (destination + 0x18u != source + 0x18u) {
            const std::uint32_t w9 = word(rt, source + 0x24u), w6 = word(rt, source + 0x18u), w7 = word(rt, source + 0x1Cu), w8 = word(rt, source + 0x20u);
            put(rt, destination + 0x24u, w9);
            put(rt, destination + 0x18u, w6);
            put(rt, destination + 0x1Cu, w7);
            put(rt, destination + 0x20u, w8);
        }
        const std::uint32_t tail = word(rt, source + 0x28u);
        source += 0x2Cu;
        put(rt, destination + 0x28u, tail);
        destination += 0x2Cu;
    }
    (void)call_keep(rt, ctx, 0x08C15E7Cu, {destination, word(rt, end_field), frame.at(0u)});
    const std::uint32_t removed = (last - first) & ~3u;
    put(rt, end_field, word(rt, end_field) - removed);
    ctx.gpr[2] = first;
}

// func_08BEDE74(list, key, extra): walks the circular list whose head node is `list` (empty: v0 = list). For each node s (its next pointer is read before the call) func_089FD6E0(key, s[2]) is
// asked; when it answers non-zero a node of 0xC bytes is allocated (func_08A5BC20(0xC, 4)), its payload at +8 copy-constructed from s + 8 (func_089FDAC8), func_08B5C078(node, extra)
// links it and func_08BEDE24(list, s) removes s. v0 = the head again.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a null node address from the allocation (the payload copy func_089FDAC8 skipped).
void func_08BEDE74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t list = ctx.gpr[4], key = ctx.gpr[5], extra = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    std::uint32_t next = word(rt, list);
    ctx.gpr[2] = next;
    if (next == list) return;
    std::uint32_t node = next;
    for (;;) {
        next = word(rt, node);
        ctx.gpr[4] = key;
        if (call_keep(rt, ctx, 0x089FD6E0u, {key, word(rt, node + 8u)}) != 0u) {
            const std::uint32_t created = call_keep(rt, ctx, 0x08A5BC20u, {0xCu, 4u});
            if (created + 8u != 0u) (void)call_keep(rt, ctx, 0x089FDAC8u, {created + 8u, node + 8u});
            (void)call_keep(rt, ctx, 0x08B5C078u, {created, extra});
            (void)call_keep(rt, ctx, 0x08BEDE24u, {list, node});
        }
        node = next;
        ctx.gpr[2] = next;
        if (list == next) return;
    }
}

// func_08B89DD4(counter, n): a ring counter of 100: counter[1] = (counter[1] + n) wrapped by 100 once (a sum of 100 or more loses 100), counter[3] += n. v0 = the new counter[1]; v1 = sum - 100,
// a2 = counter[3], a3 = (sum < 100) as the instructions leave them.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a sum of 100 or more (the counter wraps: v0 = sum - 100).
void func_08B89DD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t counter = ctx.gpr[4], n = ctx.gpr[5];
    const std::uint32_t sum = word(rt, counter + 4u) + n;
    std::uint32_t total = word(rt, counter + 0xCu);
    ctx.gpr[7] = sum < 0x64u ? 1u : 0u;
    ctx.gpr[3] = sum - 0x64u;
    ctx.gpr[2] = sum < 0x64u ? sum : sum - 0x64u;
    total += n;
    ctx.gpr[6] = total;
    put(rt, counter + 4u, ctx.gpr[2]);
    put(rt, counter + 0xCu, total);
}

// func_08B89CD4(object): the vtable D_08CBE300 and three zero words: object[0] = vtable, [3] = 0, [1] = 0, [2] = 0 (that order). v0 = the vtable.
void func_08B89CD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4];
    ctx.gpr[2] = 0x08CBE300u;
    put(rt, object, 0x08CBE300u);
    put(rt, object + 0xCu, 0u);
    put(rt, object + 4u, 0u);
    put(rt, object + 8u, 0u);
}

// func_08B7684C(a0, a1): the tail call func_08B767D0(a0, a1, 0, 0).
void func_08B7684C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    (void)rt;
    ctx.gpr[6] = 0u;
    ctx.gpr[7] = 0u;
    tail_call(ctx, 0x08B767D0u, {ctx.gpr[4], ctx.gpr[5], 0u, 0u});
}

// func_08B3F75C(object, a1): clears a record in the asm's order (words 4 and 8 and 0xC of the sub-record at +0x10, the bytes at +0x20 .. +0x23, the words 0..0xC and 0x28) and sets
// object[9] = 4, object[0xB] = a1; the tail call is func_08B3EBFC(object, a1, object + 0x10, object + 0x20).
void func_08B3F75C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4], second = ctx.gpr[5];
    ctx.gpr[6] = object + 0x10u;
    ctx.gpr[7] = object + 0x20u;
    ctx.gpr[3] = 4u;
    put(rt, object + 0x10u, 0u);
    rt.memory().store8(object + 0x20u, 0u);
    put(rt, object + 0x1Cu, 0u);
    rt.memory().store8(object + 0x23u, 0u);
    put(rt, object + 0x14u, 0u);
    put(rt, object + 0x18u, 0u);
    rt.memory().store8(object + 0x21u, 0u);
    rt.memory().store8(object + 0x22u, 0u);
    put(rt, object + 0x24u, 4u);
    put(rt, object + 0x2Cu, second);
    put(rt, object, 0u);
    put(rt, object + 4u, 0u);
    put(rt, object + 8u, 0u);
    put(rt, object + 0xCu, 0u);
    put(rt, object + 0x28u, 0u);
    tail_call(ctx, 0x08B3EBFCu, {object, second, object + 0x10u, object + 0x20u});
}

// func_089455E0(): two settings bytes of D_08CDE508 (+0x18F and +0x18E) become D_08CF2E9C (the float byte@0x18F * 0.01f + 1.0f) and D_08CF2EA0 (the word byte@0x18E). v0 = the high half of
// the address (0x08CF0000), v1 = the second byte, f0 = 1.0f and f1 = the float, as the instructions leave them.
void func_089455E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const float scale = cvt_sw(rt.memory().load8(0x08CDE508u + 0x18Fu));
    const std::uint32_t second = rt.memory().load8(0x08CDE508u + 0x18Eu);
    put(rt, 0x08CF2EA0u, second);
    const float result = psp_mul(scale, fbits(0x3C23D70Au)) + fbits(0x3F800000u);
    ctx.fpr[0] = fbits(0x3F800000u);
    ctx.fpr[1] = result;
    put_float(rt, 0x08CF2E9Cu, result);
    ctx.gpr[3] = second;
    ctx.gpr[2] = 0x08CF0000u;
}

// func_08B64C80(object): the item [func_08B5C2B0(D_08D721D4) * 4 + object[0][1]] of the array (object[0] is a descriptor whose word 1 points to the table). v0 = that item.
void func_08B64C80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t index = call_keep(rt, ctx, 0x08B5C2B0u, {0x08D721D4u});
    const std::uint32_t descriptor = word(rt, object);
    const std::uint32_t table = word(rt, descriptor + 4u);
    ctx.gpr[4] = descriptor;
    ctx.gpr[3] = table;
    ctx.gpr[2] = word(rt, (index << 2) + table);
}

// func_08C3CCE0(object, a1, size): func_08B24954(object, a1, 0x2000, size) and the vtable D_08CD6790 into object[0]. v0 = the vtable.
void func_08C3CCE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4], size = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[7] = size;
    ctx.gpr[6] = 0x2000u;
    (void)call_keep(rt, ctx, 0x08B24954u, {object, ctx.gpr[5], 0x2000u, size});
    ctx.gpr[2] = 0x08CD6790u;
    put(rt, object, 0x08CD6790u);
}

// func_08C19CEC(object, a1, flag): func_08C19CAC(object, a1, flag) then object[0] = the vtable D_08CD0E30 and the byte object[0x10] = flag & 0xFF. v0 = the vtable.
void func_08C19CEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4], flag = ctx.gpr[6] & 0xFFu;
    GuestFrame frame(ctx, 0x10u);
    (void)call_keep(rt, ctx, 0x08C19CACu, {object});
    ctx.gpr[2] = 0x08CD0E30u;
    rt.memory().store8(object + 0x10u, static_cast<std::uint8_t>(flag));
    put(rt, object, 0x08CD0E30u);
}

// func_08ABCE2C(): func_08ABC650(), D_08CFFAA8 = func_08ABC318(D_08CB438C, 1), func_08ABBC10() and the tail call func_08ABCDE0().
void func_08ABCE2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_keep(rt, ctx, 0x08ABC650u, {});
        const std::uint32_t result = call_keep(rt, ctx, 0x08ABC318u, {0x08CB438Cu, 1u});
        ctx.gpr[3] = 0x08D00000u;
        put(rt, 0x08CFFAA8u, result);
        (void)call_keep(rt, ctx, 0x08ABBC10u, {});
    }
    tail_call(ctx, 0x08ABCDE0u, {});
}

// func_08B0EBF0(): the five argument-less calls func_08B17F20(), func_08B17EF0(), func_08B10EF0(), func_08B17EF8(), func_08B17F28() and the tail call func_08B0E9DC().
void func_08B0EBF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_keep(rt, ctx, 0x08B17F20u, {});
        (void)call_keep(rt, ctx, 0x08B17EF0u, {});
        (void)call_keep(rt, ctx, 0x08B10EF0u, {});
        (void)call_keep(rt, ctx, 0x08B17EF8u, {});
        (void)call_keep(rt, ctx, 0x08B17F28u, {});
    }
    tail_call(ctx, 0x08B0E9DCu, {});
}

// func_08AD083C(): func_08B32228(D_08D69170, 0), then the tail call func_08B319B8(D_08D69170, func_08ABDC6C()).
void func_08AD083C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_keep(rt, ctx, 0x08B32228u, {0x08D69170u, 0u});
        (void)call_keep(rt, ctx, 0x08ABDC6Cu, {});
    }
    const std::uint32_t result = ctx.gpr[2];
    tail_call(ctx, 0x08B319B8u, {0x08D69170u, result});
}

// func_08BDAFB4(object, a1, value): func_08BDAF74(object, a1, value), then the vtable D_08CC56A8 into object[0] and the sign-extended byte of value at object[0x10]. v0 = the vtable.
void func_08BDAFB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4];
    const std::uint32_t value = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(ctx.gpr[6] & 0xFFu)));
    GuestFrame frame(ctx, 0x10u);
    (void)call_keep(rt, ctx, 0x08BDAF74u, {object});
    ctx.gpr[2] = 0x08CC56A8u;
    rt.memory().store8(object + 0x10u, static_cast<std::uint8_t>(value));
    put(rt, object, 0x08CC56A8u);
}

// func_08C19D2C(source): a 0x14-byte object from func_08B62344(0x14) is built by func_08C19CEC(object, source[3], 0). v0 = the object.
void func_08C19D2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t source = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t member = word(rt, source + 0xCu);
    const std::uint32_t object = call_keep(rt, ctx, 0x08B62344u, {0x14u});
    (void)call_keep(rt, ctx, 0x08C19CECu, {object, member, 0u});
    ctx.gpr[6] = 0u;
    ctx.gpr[2] = object;
}

// func_08C3C784(object): object[0] = D_08CD64B8, object[0x3E] = D_08CD64E8, func_08B0C23C(object + 0xF8), func_08B0DDDC(object) and the tail call func_08B64654(object).
void func_08C3C784(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        put(rt, object, 0x08CD64B8u);
        put(rt, object + 0xF8u, 0x08CD64E8u);
        ctx.gpr[2] = 0x08CD64E8u;
        (void)call_keep(rt, ctx, 0x08B0C23Cu, {object + 0xF8u});
        (void)call_keep(rt, ctx, 0x08B0DDDCu, {object});
    }
    tail_call(ctx, 0x08B64654u, {object});
}

// func_08B5D34C(handle): releases a reference: func_08B60804(*handle, -1); when that answered 1 and *handle is not null, func_08B5CC1C(*handle) runs and the tail call is func_08B64654(*handle).
// Anything else leaves with the answer of func_08B60804.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a reference count that reaches 1 (func_08B60804 answering 1: with a non-null target func_08B5CC1C(target) and the tail call func_08B64654(target); a null target returns 1). Every compared case gets another answer and returns it.
void func_08B5D34C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t handle = ctx.gpr[4];
    const std::uint32_t target = word(rt, handle);
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[5] = 0xFFFFFFFFu;
        const std::uint32_t count = call_keep(rt, ctx, 0x08B60804u, {target, 0xFFFFFFFFu});
        ctx.gpr[3] = 1u;
        if (count != 1u || target == 0u) return;
        (void)call_keep(rt, ctx, 0x08B5CC1Cu, {target});
    }
    tail_call(ctx, 0x08B64654u, {target});
}

// func_089F9F7C(key): the node func_08C0DA48(func_089F9E54(), key) is looked up; when it is not the end (func_089F9E54() + 4) v0 = node[5], otherwise 0.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the lookup answering the end node (v0 = 0).
void func_089F9F7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t key = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t root = call_keep(rt, ctx, 0x089F9E54u, {});
    const std::uint32_t node = call_keep(rt, ctx, 0x08C0DA48u, {root, key});
    const std::uint32_t end = call_keep(rt, ctx, 0x089F9E54u, {}) + 4u;
    std::uint32_t value = 0u;
    ctx.gpr[2] = end;
    if (end != node) value = word(rt, node + 0x14u);
    ctx.gpr[3] = value;
    ctx.gpr[2] = value;
}

// func_08BE3FB4(object, a1, a2): object[1] = 0, object[2] = 0, func_08B28DCC(object + 0xC) and the tail call func_08BE3F0C(object, a1, a2).
void func_08BE3FB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4], second = ctx.gpr[5], third = ctx.gpr[6];
    {
        GuestFrame frame(ctx, 0x10u);
        put(rt, object + 4u, 0u);
        ctx.gpr[4] = object + 0xCu;
        put(rt, object + 8u, 0u);
        (void)call_keep(rt, ctx, 0x08B28DCCu, {object + 0xCu});
        ctx.gpr[4] = object;
    }
    tail_call(ctx, 0x08BE3F0Cu, {object, second, third});
}

// func_08C5DC90(object, a1, a2): object[0] = the vtable D_08CDB7F0, object[1] = (a2 != 0), object[2] = a1, object[4] = func_08B5C274() and func_08B610C0(object, 0). v0 as the last call leaves it.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a2 = 0 (the flag object[1] = 0).
void func_08C5DC90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4], second = ctx.gpr[5];
    const std::uint32_t flag = ctx.gpr[6] != 0u ? 1u : 0u;
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[6] = flag;
    ctx.gpr[2] = 0x08CDB7F0u;
    put(rt, object + 4u, flag);
    put(rt, object, 0x08CDB7F0u);
    put(rt, object + 8u, second);
    const std::uint32_t made = call_keep(rt, ctx, 0x08B5C274u, {});
    put(rt, object + 0x10u, made);
    ctx.gpr[5] = 0u;
    (void)call_keep(rt, ctx, 0x08B610C0u, {object, 0u});
}

// func_08B86F3C(target, a1, a2): a holder on the frame is filled (func_08A69B3C(&holder)), a 0x5C-byte object from func_08A5BC20(0x5C, 4) is built by func_08A54F3C(object, &holder, a2), its
// address is stored in a second frame word and func_08A52AB0(target, &that) takes it; func_08A69D68(&holder) destroys the holder.
void func_08B86F3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t target = ctx.gpr[4], third = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t holder = frame.at(0u), slot = frame.at(4u);
    (void)call_keep(rt, ctx, 0x08A69B3Cu, {holder});
    const std::uint32_t object = call_keep(rt, ctx, 0x08A5BC20u, {0x5Cu, 4u});
    (void)call_keep(rt, ctx, 0x08A54F3Cu, {object, holder, third});
    put(rt, slot, object);
    (void)call_keep(rt, ctx, 0x08A52AB0u, {target, slot});
    (void)call_keep(rt, ctx, 0x08A69D68u, {holder});
}

// func_08B86BE8(a0, a1, a2, a3, t0): func_08A69B3C(&holder); func_08A535A4(a0, &holder, a2, a3, t0); func_08A69D68(&holder).
void func_08B86BE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t first = ctx.gpr[4], third = ctx.gpr[6], fourth = ctx.gpr[7], fifth = ctx.gpr[8];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t holder = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08A69B3Cu, {holder});
    (void)call_keep(rt, ctx, 0x08A535A4u, {first, holder, third, fourth, fifth});
    (void)call_keep(rt, ctx, 0x08A69D68u, {holder});
}

// func_08B882FC(object): a deleting destructor. The vtable D_08CBAF98 goes into object[0]; the members at +0x34, +0x30, +0x2C, +0x28, +0x24, +0x20 and +0x14 are destroyed by func_08A5B9EC,
// func_08A780B0(object) follows and the tail call is func_08A5BC54(object, 0x3C, 4) (the free).
void func_08B882FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = 0x08CBAF98u;
        put(rt, object, 0x08CBAF98u);
        for (const std::uint32_t offset : {0x34u, 0x30u, 0x2Cu, 0x28u, 0x24u, 0x20u, 0x14u}) (void)call_keep(rt, ctx, 0x08A5B9ECu, {object + offset});
        (void)call_keep(rt, ctx, 0x08A780B0u, {object});
    }
    tail_call(ctx, 0x08A5BC54u, {object, 0x3Cu, 4u});
}

// func_08BE764C(object): a deleting destructor. The vtable D_08CCBA90 goes into object[0]; the members at +0x3C and +0x38 are destroyed (func_08A5B9EC), func_08B856B0(object[0xB], object[0xC], &frame)
// destroys the elements of the vector at +0x2C and its storage (object[0xB], size (object[0xD] - object[0xB]) & ~3) is freed with func_08A5BC54(storage, size, 4) when it exists;
// func_08974124(object) runs and func_08A5BC54(object, 0x48, 4) frees the object itself.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a null vector storage object[0xB] (the `beqz` skips the free).
void func_08BE764C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t vector = object + 0x2Cu;
    ctx.gpr[2] = 0x08CCBA90u;
    put(rt, object, 0x08CCBA90u);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {object + 0x3Cu});
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {object + 0x38u});
    (void)call_keep(rt, ctx, 0x08B856B0u, {word(rt, object + 0x2Cu), word(rt, vector + 4u), frame.at(0u)});
    const std::uint32_t storage = word(rt, object + 0x2Cu);
    const std::uint32_t size = static_cast<std::uint32_t>(static_cast<std::int32_t>(word(rt, vector + 8u) - storage) >> 2) << 2;
    ctx.gpr[2] = storage;
    ctx.gpr[6] = 4u;
    if (storage != 0u) (void)call_keep(rt, ctx, 0x08A5BC54u, {storage, size, 4u});
    (void)call_keep(rt, ctx, 0x08974124u, {object});
    (void)call_keep(rt, ctx, 0x08A5BC54u, {object, 0x48u, 4u});
}

// func_08C25CCC(target, a1, a2, key): two holders on the frame are built from key (func_08A76ABC(&a, key), func_08A76ABC(&b, key + 4)); the target's virtual slot 5 runs with
// (a1, &a, &b) and the holders are destroyed (func_08A5B9EC(&b), func_08A5B9EC(&a)).
void func_08C25CCC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t target = ctx.gpr[4], second = ctx.gpr[5], key = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t first = frame.at(4u), other = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08A76ABCu, {first, key});
    (void)call_keep(rt, ctx, 0x08A76ABCu, {other, key + 4u});
    (void)call_keep(rt, ctx, word(rt, target + 0x14u), {second, first, other});
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {other});
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {first});
}

// func_08C40EBC(a0, array, count): func_08C40E54(a0) runs; with *array zero v0 = 0. Otherwise func_08C40DEC(a0, *array + 4 * i) is called for i = 0 .. count - 1 (the pointer is read again
// after each call). v0 as the last call leaves it.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): everything past the first test: every compared case has *array == 0 (v0 = 0, the `bnez` not taken). The loop func_08C40DEC(a0, *array + 4 * i) is read from the asm only (an empty count returns the array pointer; the pointer is reloaded by the `bnel` delay slot only when the loop goes on, so the last iteration's v0 is the call's answer).
void func_08C40EBC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t first = ctx.gpr[4], array = ctx.gpr[5], count = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    (void)call_keep(rt, ctx, 0x08C40E54u, {first});
    std::uint32_t base = word(rt, array);
    ctx.gpr[2] = base;
    if (base == 0u || count == 0u) return;
    for (std::uint32_t i = 0;;) {
        (void)call_keep(rt, ctx, 0x08C40DECu, {first, base + 4u * i});
        ++i;
        if (i == count) break;
        base = word(rt, array);  // the delay slot of the `bnel` reloads it only when the loop goes on
        ctx.gpr[2] = base;
    }
}

// func_08C41BE4(a0, array, count): the twin of func_08C40EBC. func_08C4130C(a0) runs; with *array zero it ends (v0 = that call's answer). Otherwise for i = 0 .. count - 1:
// func_08C412A4(a0, *array + 4 * i) and func_08AF98C0((*array)[i], a0), the pointer read again after the first call. v0 as the last call leaves it.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): everything past the first test: every compared case has *array == 0. The loop (func_08C412A4(a0, *array + 4 * i), then func_08AF98C0((*array)[i], a0)) is read from the asm only; a count of 0 leaves v0 as func_08C4130C answered (a1 holds the array pointer), the last iteration returns the call's answer (the `bnel` delay slot reloads the pointer only when it continues).
void func_08C41BE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t first = ctx.gpr[4], array = ctx.gpr[5], count = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    (void)call_keep(rt, ctx, 0x08C4130Cu, {first});
    std::uint32_t base = word(rt, array);
    ctx.gpr[5] = base;
    if (base == 0u || count == 0u) return;
    for (std::uint32_t i = 0;;) {
        (void)call_keep(rt, ctx, 0x08C412A4u, {first, base + 4u * i});
        const std::uint32_t element = word(rt, word(rt, array) + 4u * i);
        ++i;
        ctx.gpr[5] = first;
        (void)call_keep(rt, ctx, 0x08AF98C0u, {element, first});
        if (i == count) break;
        base = word(rt, array);  // the delay slot of the `bnel` reloads it only when the loop goes on
        ctx.gpr[5] = base;
    }
}

// func_08B85D54(object): a deleting destructor. The vtable D_08CB6FE0 goes into object[0]; the 26 pointers from object[0x28] down to object[0xF] (offsets 0xA0 .. 0x3C) are released with
// func_08A5B930 when non-zero (the asm walks them downwards); object[0xE] is released the same way, func_08A7D4E4(object) follows and the tail call is func_08A5BC54(object, 0xA4, 4).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the loop's pointer slots all being zero (func_08A5B930 is never called for them: the asm skips a zero pointer with the `beqz`), a non-zero object[0xE] (released with func_08A5B930) and the two guards at the top (a null object + 0x3C, and the header equal to object + 0xA4, both go straight to the object[0xE] release).
void func_08B85D54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = 0x08CB6FE0u;
        ctx.gpr[3] = object + 0x3Cu;
        put(rt, object, 0x08CB6FE0u);
        if (object + 0x3Cu != 0u && object + 0x3Cu != object + 0xA4u) {
            std::uint32_t cursor = object + 0xA0u;
            for (std::uint32_t remaining = 0x1Au; remaining != 0u; cursor -= 4u) {
                const std::uint32_t pointer = word(rt, cursor);
                --remaining;
                if (pointer != 0u) (void)call_keep(rt, ctx, 0x08A5B930u, {pointer});
            }
        }
        const std::uint32_t last = word(rt, object + 0x38u);
        ctx.gpr[4] = last;
        if (last != 0u) (void)call_keep(rt, ctx, 0x08A5B930u, {last});
        (void)call_keep(rt, ctx, 0x08A7D4E4u, {object});
    }
    tail_call(ctx, 0x08A5BC54u, {object, 0xA4u, 4u});
}

// func_08C34520(target, a1, source): source[0] must be set (none: func_08A5BB7C(D_08CB30B8)); its virtual slot 0x3C fills the frame holder b (&b, object); func_08A76ABC(&a, &b) copies it,
// func_08C34248(a1, &a) consumes it and the holders are destroyed (func_08A5B9EC(&a), func_08A5B9EC(&b)).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a null source object (func_08A5BB7C(D_08CB30B8) first).
void func_08C34520(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t second = ctx.gpr[5], source = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t copy = frame.at(0u), holder = frame.at(4u);
    const std::uint32_t object = word(rt, source);
    ctx.gpr[4] = 0x08CB30B8u;
    if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CB30B8u});
    (void)call_keep(rt, ctx, word(rt, word(rt, object) + 0xF0u), {holder, object});
    (void)call_keep(rt, ctx, 0x08A76ABCu, {copy, holder});
    (void)call_keep(rt, ctx, 0x08C34248u, {second, copy});
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {copy});
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {holder});
}

// func_08C1DA0C(first, last): destroys the 8-byte entries [first, last) (nothing when they are equal: v0 stays): the holder at +4 (func_08A5B9EC) and the string at +0, released
// unless it is the shared empty one (func_08B60804(data - 4, -1) <= 0 frees it with func_08B8542C(rep, &frame)).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): an empty range (v0 stays), the shared empty string, and a release that frees the string (func_08B60804 answering 0 or less: func_08B8542C(rep, &frame)).
void func_08C1DA0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t entry = ctx.gpr[4];
    const std::uint32_t last = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    if (entry == last) return;
    do {
        (void)call_keep(rt, ctx, 0x08A5B9ECu, {entry + 4u});
        const std::uint32_t chars = word(rt, entry);
        ctx.gpr[2] = chars;
        ctx.gpr[5] = 0xFFFFFFFFu;
        entry += 8u;
        const std::uint32_t rep = chars - 0xCu;
        if (rep != kStringEmpty) {
            const std::uint32_t count = call_keep(rt, ctx, 0x08B60804u, {chars - 4u, 0xFFFFFFFFu});
            if (!(static_cast<std::int32_t>(count) > 0)) (void)call_keep(rt, ctx, 0x08B8542Cu, {rep, frame.at(0u)});
        }
    } while (last != entry);
}

// func_08BE6098(target, a1, a2, a3): a holder on the frame (func_08A69B3C), a 0x1C-byte object from func_08A5BC20(0x1C, 4) built by func_08A725A0(object, &holder) gets the vtable D_08CCD5D8,
// object[5] = a2 and object[6] = a3; the holder is destroyed (func_08A69D68), the object goes into a smart pointer (func_08A5D414(&p, &object)), func_08A7C5C8(target, &p) takes it and
// func_08A5B9EC(&p) ends.
void func_08BE6098(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t target = ctx.gpr[4], third = ctx.gpr[6], fourth = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t holder = frame.at(8u), pointer = frame.at(4u), slot = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08A69B3Cu, {holder});
    const std::uint32_t object = call_keep(rt, ctx, 0x08A5BC20u, {0x1Cu, 4u});
    (void)call_keep(rt, ctx, 0x08A725A0u, {object, holder});
    put(rt, object, 0x08CCD5D8u);
    put(rt, object + 0x14u, third);
    put(rt, object + 0x18u, fourth);
    (void)call_keep(rt, ctx, 0x08A69D68u, {holder});
    put(rt, slot, object);
    (void)call_keep(rt, ctx, 0x08A5D414u, {pointer, slot});
    (void)call_keep(rt, ctx, 0x08A7C5C8u, {target, pointer});
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {pointer});
}

// func_08C31FB4(a0, target, source): source[0] must be set (none: func_08A5BB7C(D_08CB30B8)); its virtual slot 0x36 answers n, func_08C31A00(target, n) runs, source[0] (checked again) slot 0x37 answers m
// (called with a1 = 0), and target[1] (none: func_08A5BB7C(D_08CB30B0)) takes the tail call through its virtual slot 0x43 with (target[1], m, n).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the three null-object asserts (func_08A5BB7C with D_08CB30B8 for the source twice and D_08CB30B0 for the target).
void func_08C31FB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t target = ctx.gpr[5], source = ctx.gpr[6];
    std::uint32_t destination, method;
    std::uint32_t second;
    {
        GuestFrame frame(ctx, 0x20u);
        std::uint32_t object = word(rt, source);
        ctx.gpr[4] = 0x08CB30B8u;
        if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CB30B8u});
        const std::uint32_t first = call_keep(rt, ctx, word(rt, word(rt, object) + 0xD8u), {object});
        ctx.gpr[5] = first;
        (void)call_keep(rt, ctx, 0x08C31A00u, {target, first});
        object = word(rt, source);
        ctx.gpr[4] = 0x08CB30B8u;
        if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CB30B8u});
        second = call_keep(rt, ctx, word(rt, word(rt, object) + 0xDCu), {object, 0u});
        destination = word(rt, target + 4u);
        ctx.gpr[4] = 0x08CB30B0u;
        if (destination == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CB30B0u});
        method = word(rt, word(rt, destination) + 0x10Cu);
        ctx.gpr[6] = first;
    }
    ctx.gpr[2] = method;
    ctx.gpr[25] = method;
    tail_call(ctx, method, {destination, second, ctx.gpr[6]});
}

// func_08BC7744(out, a1, source): a counted-reference pair. A 0xC-byte object (func_08B62344(0xC)) gets the vtable D_08CC5270, [1] = a1, [2] = *source and is stored to out[0]; a 0x10-byte control
// block (func_08B62344(0x10)) gets the vtable D_08CC52A0, counts 1 and 1 and the object at +0xC and is stored to out[1]; func_08B8E380(out + 4, object, object) links them. v0 = out.
void func_08BC7744(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], value = ctx.gpr[5], source = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t object = call_keep(rt, ctx, 0x08B62344u, {0xCu});
    put(rt, object + 4u, value);
    put(rt, object, 0x08CC5270u);
    put(rt, object + 8u, word(rt, source));
    put(rt, out, object);
    const std::uint32_t block = call_keep(rt, ctx, 0x08B62344u, {0x10u});
    ctx.gpr[7] = 1u;
    put(rt, block + 4u, 1u);
    put(rt, block + 0xCu, object);
    put(rt, block, 0x08CC52A0u);
    put(rt, block + 8u, 1u);
    put(rt, out + 4u, block);
    (void)call_keep(rt, ctx, 0x08B8E380u, {out + 4u, object, object});
    ctx.gpr[2] = out;
}

// func_08C1CFD0(map, key): insert_unique for the string-keyed tree (header map + 4, root map[2], leftmost map[3], key at node[4]). The key's characters ([[key]]) walk down from the root with
// func_08B35FD8 (a negative answer goes left, node[2], otherwise right, node[3]) to the last node s1. Going left from a node that is not the leftmost moves to its predecessor
// (func_08B5F174); when the node's key then compares below the key's (func_08B35FD8(node chars, key chars) < 0) or the tree is empty / the leftmost was reached,
// func_08C1CF0C(map, 0, s1, key) inserts and the answer is {node, 1}; otherwise {node, 0}. The bool is a byte stored to the frame and read back as a word (v1 = that word, the three upper bytes
// being what the frame held before).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): going left from a node that is not the leftmost (the predecessor func_08B5F174 and the comparison) and an equal key (the comparison not below 0: {node, 0}, no insertion); the compared cases insert after going right and at the leftmost node.
void func_08C1CFD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t map = ctx.gpr[4], key = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    std::uint32_t node = word(rt, map + 8u);
    std::uint32_t parent = map + 4u;
    bool went_left = true;  // the empty tree continues at the leftmost test with the header as the parent
    if (node != 0u) {
        const std::uint32_t chars = word(rt, word(rt, key));
        for (;;) {
            const std::uint32_t node_chars = word(rt, word(rt, node + 0x10u));
            const std::uint32_t order = call_keep(rt, ctx, 0x08B35FD8u, {chars, node_chars});
            went_left = (order >> 31) != 0u;
            const std::uint32_t child = word(rt, node + (went_left ? 8u : 0xCu));
            if (child == 0u) break;
            node = child;
        }
        parent = node;
    }
    bool insert = true;
    if (!went_left || parent != word(rt, map + 0xCu)) {
        if (went_left) node = call_keep(rt, ctx, 0x08B5F174u, {parent});
        const std::uint32_t order = call_keep(rt, ctx, 0x08B35FD8u, {word(rt, word(rt, node + 0x10u)), word(rt, word(rt, key))});
        insert = static_cast<std::int32_t>(order) < 0;
    }
    std::uint32_t flag = 0u;
    if (insert) {
        node = call_keep(rt, ctx, 0x08C1CF0Cu, {map, 0u, parent, key});
        flag = 1u;
    }
    rt.memory().store8(frame.at(4u), static_cast<std::uint8_t>(flag));
    ctx.gpr[3] = word(rt, frame.at(4u));
    ctx.gpr[2] = node;
}

}  // namespace
}  // namespace hot_batch_82
}  // namespace gt::game

#define HOT82(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_82::name, gt::ReturnKind::kind)
// Not registered (4 of its 5 cases stop at the sceDisplay vblank import (not comparable) and the run stops at the diff harness sentinel after them): HOT82(0x08B59A24u, func_08B59A24, Word);
// Not registered (its cases stop at the sceDisplay vblank import (not comparable)): HOT82(0x08ACF390u, func_08ACF390, Word);
HOT82(0x088FC214u, func_088FC214, Word);
// Not registered (no compared case changed memory (every case left at the inactive byte), so the gate cannot see its stores): HOT82(0x08B44B24u, func_08B44B24, Word);
HOT82(0x08C20838u, func_08C20838, Word);
HOT82(0x08B71CE0u, func_08B71CE0, DoubleWord);
HOT82(0x0895BFC4u, func_0895BFC4, Word);
HOT82(0x08A430A8u, func_08A430A8, Word);
HOT82(0x089A5614u, func_089A5614, Word);
// Not registered (every compared case stopped at the sceIoRead import (not comparable)): HOT82(0x08B33E50u, func_08B33E50, Word);
HOT82(0x08C21C84u, func_08C21C84, Word);
HOT82(0x08B661E8u, func_08B661E8, Float);
HOT82(0x08ACCE04u, func_08ACCE04, Word);
HOT82(0x08B3A37Cu, func_08B3A37C, Word);
// Not registered (no compared case changed memory (every compared case has a count of 0), so the gate cannot see its stores): HOT82(0x08BE6674u, func_08BE6674, Word);
HOT82(0x08C21048u, func_08C21048, Word);
HOT82(0x08B7B9ACu, func_08B7B9AC, Word);
HOT82(0x08C1CDCCu, func_08C1CDCC, Word);
HOT82(0x08C217F4u, func_08C217F4, Word);
HOT82(0x08B8543Cu, func_08B8543C, Word);
// Not registered (no compared case changed memory (the copies were already equal), so the gate cannot see its stores): HOT82(0x08BE834Cu, func_08BE834C, Word);
// Not registered (setjmp stores ra, which inside a harness case is the case sentinel: the later longjmp would return to it and stop the run): HOT82(0x08B7B2E4u, func_08B7B2E4, Word);
HOT82(0x08B18088u, func_08B18088, Word);
HOT82(0x08B34CA0u, func_08B34CA0, Word);
HOT82(0x08C297C8u, func_08C297C8, Word);
HOT82(0x08B2432Cu, func_08B2432C, Word);
HOT82(0x08B2440Cu, func_08B2440C, Word);
HOT82(0x08B13278u, func_08B13278, Word);
HOT82(0x08C20224u, func_08C20224, Word);
HOT82(0x08C1BE44u, func_08C1BE44, Word);
HOT82(0x08C338D4u, func_08C338D4, Word);
HOT82(0x08C33F44u, func_08C33F44, Word);
HOT82(0x08C218D0u, func_08C218D0, Word);
HOT82(0x08933FB8u, func_08933FB8, Word);
// Not registered (no compared case changed memory (the erased range was empty or moved onto itself), so the gate cannot see its stores): HOT82(0x08C15E8Cu, func_08C15E8C, Word);
HOT82(0x08BEDE74u, func_08BEDE74, Word);
HOT82(0x08B89DD4u, func_08B89DD4, Word);
HOT82(0x08B89CD4u, func_08B89CD4, Word);
HOT82(0x08B7684Cu, func_08B7684C, Word);
HOT82(0x08B3F75Cu, func_08B3F75C, Word);
HOT82(0x089455E0u, func_089455E0, Word);
HOT82(0x08B64C80u, func_08B64C80, Word);
HOT82(0x08C3CCE0u, func_08C3CCE0, Word);
HOT82(0x08C19CECu, func_08C19CEC, Word);
// Not registered (all 5 cases stop at imports (not comparable)): HOT82(0x08ABCE2Cu, func_08ABCE2C, Word);
// Not registered (its case stops at a thread start that would preempt (not comparable)): HOT82(0x08B0EBF0u, func_08B0EBF0, Word);
// Not registered (its case stops at an sceUtility import (not comparable)): HOT82(0x08AD083Cu, func_08AD083C, Word);
HOT82(0x08BDAFB4u, func_08BDAFB4, Word);
HOT82(0x08C19D2Cu, func_08C19D2C, Word);
HOT82(0x08C3C784u, func_08C3C784, Word);
HOT82(0x08B5D34Cu, func_08B5D34C, Word);
HOT82(0x089F9F7Cu, func_089F9F7C, Word);
HOT82(0x08BE3FB4u, func_08BE3FB4, Word);
HOT82(0x08C5DC90u, func_08C5DC90, Word);
HOT82(0x08B86F3Cu, func_08B86F3C, Word);
HOT82(0x08B86BE8u, func_08B86BE8, Word);
HOT82(0x08B882FCu, func_08B882FC, Word);
HOT82(0x08BE764Cu, func_08BE764C, Word);
HOT82(0x08C25CCCu, func_08C25CCC, Word);
HOT82(0x08C40EBCu, func_08C40EBC, Word);
HOT82(0x08C41BE4u, func_08C41BE4, Word);
HOT82(0x08B85D54u, func_08B85D54, Word);
HOT82(0x08C34520u, func_08C34520, Word);
HOT82(0x08C1DA0Cu, func_08C1DA0C, Word);
HOT82(0x08BE6098u, func_08BE6098, Word);
HOT82(0x08C31FB4u, func_08C31FB4, Word);
HOT82(0x08BC7744u, func_08BC7744, Word);
HOT82(0x08C1CFD0u, func_08C1CFD0, DoubleWord);
