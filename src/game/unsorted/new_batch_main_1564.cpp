// Main-session batch 1564: small functions (7-16 instructions) from tools.disasm, hand-written structured C.
// Shapes: value getters, vtable forwarders, destructor-shaped wrappers with a tail call, and three float returns.
// Every body keeps the original's register effects: v0 on every exit path, s0 saved and restored where the asm does,
// delay slots before the branch or call they belong to, and every argument register a callee reads.
// DRAFT ONLY: the differential gate has not run.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1564 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
s32 signed_byte(Runtime &rt, u32 a) { return static_cast<std::int8_t>(rt.memory().load8(a)); }
s32 signed_half(Runtime &rt, u32 a) { return static_cast<std::int16_t>(rt.memory().load16(a)); }
void put8(Runtime &rt, u32 a, u32 v) { rt.memory().store8(a, static_cast<std::uint8_t>(v)); }
void put16(Runtime &rt, u32 a, u32 v) { rt.memory().store16(a, static_cast<std::uint16_t>(v)); }

// A guest call whose second result register ($v1, as the callee left it) is needed by the caller.
GuestResult call_full(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    return result;
}

// Restores $s0 exactly where the original writes it back before returning.
struct SavedS0 {
    AllegrexContext &ctx;
    u32 s0;
    explicit SavedS0(AllegrexContext &c) : ctx(c), s0(c.gpr[16]) {}
    ~SavedS0() { ctx.gpr[16] = s0; }
};

// 0x08B84938: f0 = (t2 != 0) ? -NaN 0xFF80FFFF : -inf 0xFF800000 (both bit patterns as the asm builds them).

// 0x0890B454 (a0 = table, a1 = index): looked up entry's byte +8 (0x0890B428(a0, a1) gives it, 0 when out of range); 0 when null.
void func_0890B454(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 found = call(rt, ctx, 0x0890B428u, {ctx.gpr[4], ctx.gpr[5]});
    u32 value = 0u;
    if (found != 0u) value = rt.memory().load8(found + 8u);
    ctx.gpr[3] = value;
    ctx.gpr[2] = value;
}

// 0x089C7274 (a0 = object, a1 = key): 0x089C7128(a0, a1) answers an entry; v0 = its word +0, 0 when null.
void func_089C7274(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 found = call(rt, ctx, 0x089C7128u, {ctx.gpr[4], ctx.gpr[5]});
    u32 value = 0u;
    if (found != 0u) value = word(rt, found);
    ctx.gpr[3] = value;
    ctx.gpr[2] = value;
}

// 0x08ADDABC (a0 = tagged record): v0 = 0x08ADB314(a0)'s word +8, -1 when the tag does not match.
void func_08ADDABC(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 found = call(rt, ctx, 0x08ADB314u, {ctx.gpr[4]});
    u32 value = 0xFFFFFFFFu;
    if (found != 0u) value = word(rt, found + 8u);
    ctx.gpr[3] = value;
    ctx.gpr[2] = value;
}

// 0x08B1A890 (a0 ignored by both callees): 0x08B1A844() until it answers non-zero, 0x08B2E064() after each zero;
// the packet stub 0x08C7A9E8 inside it receives the static object 0x08D689D0 that 0x08B1A844 leaves in a0.

// 0x08A5BB2C (a0 = destination, a1 = source holder): when they differ, 0x08A5BAD8(a0, *a1); v0 = a0.
void func_08A5BB2C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        SavedS0 saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        if (ctx.gpr[4] != ctx.gpr[5]) call(rt, ctx, 0x08A5BAD8u, {ctx.gpr[4], word(rt, ctx.gpr[5])});
    }
    ctx.gpr[2] = self;
}

// 0x08AB4B48 (a0, a1): when they differ, 0x08A781D4(a0, a1); v0 = a0.

// 0x08ADDAE8 (a0 = tagged record): v0 = 0x08ADB314(a0)'s word +0xC with bit 31 cleared, -1 when the tag does not match.
void func_08ADDAE8(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 found = call(rt, ctx, 0x08ADB314u, {ctx.gpr[4]});
    u32 value = 0xFFFFFFFFu;
    if (found != 0u) value = word(rt, found + 0xCu) & 0x7FFFFFFFu;
    ctx.gpr[3] = value;
    ctx.gpr[2] = value;
}

// 0x08B339FC (a0 = handle): 0x08B33950(a0); non-zero answers with the UMD medium check import 0x08C7B110
// (sceUmdUser 0x6AF9B50A, no arguments), zero returns 0.
void func_08B339FC(Runtime &rt, AllegrexContext &ctx) {
    u32 result;
    {
        GuestFrame frame(ctx, 0x10u);
        result = call(rt, ctx, 0x08B33950u, {ctx.gpr[4]});
    }
    if (result == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    tail_call(ctx, 0x08C7B110u, {});
}

// 0x089557FC (a0 = object): 0x089557AC(a0) gates the signed half at +0x74C of the object's word +8; 0 when false.
void func_089557FC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    u32 value;
    {
        SavedS0 saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        const u32 inner = word(rt, self + 8u);  // loaded in the call's delay slot, before 0x089557AC
        ctx.gpr[16] = inner;
        const u32 found = call(rt, ctx, 0x089557ACu, {self});
        value = 0u;
        if (found != 0u) value = static_cast<u32>(signed_half(rt, inner + 0x74Cu));
    }
    ctx.gpr[3] = value;
    ctx.gpr[2] = value;
}

// 0x08AE1CBC (a0 = table index, a1 = 16-bit value): 0x08ADDCD4(a0, 0x08C913AC) names the record; its half +0xEA = a1.
void func_08AE1CBC(Runtime &rt, AllegrexContext &ctx) {
    const u32 value = ctx.gpr[5];
    u32 found;
    {
        SavedS0 saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = value;
        found = call(rt, ctx, 0x08ADDCD4u, {ctx.gpr[4], 0x08C913ACu});
        if (found != 0u) put16(rt, found + 0xEAu, value);
    }
    ctx.gpr[2] = found;
}

// 0x08AF27F4 (a0 = object): 0x08AF27B4(a0) gives an object whose word +4 has the answer at +0x2B8 via 0x08AF1280;
// a null answer gives v0 = 0.
void func_08AF27F4(Runtime &rt, AllegrexContext &ctx) {
    u32 found;
    {
        GuestFrame frame(ctx, 0x10u);
        found = call(rt, ctx, 0x08AF27B4u, {ctx.gpr[4]});
    }
    if (found == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    tail_call(ctx, 0x08AF1280u, {found});
}

// 0x088DF394 (a0 = object): 0x088D9E58(a0) gates it; when it answers non-zero, the byte +0x5B8 is read and, when
// zero, byte +0x25 is cleared. v0 = that byte (0 when the gate says no).
void func_088DF394(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    u32 value;
    {
        SavedS0 saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        const u32 gate = call(rt, ctx, 0x088D9E58u, {self});
        if (gate == 0u) {
            value = 0u;
        } else {
            value = rt.memory().load8(self + 0x5B8u);
            if (value == 0u) put8(rt, self + 0x25u, 0u);
        }
    }
    ctx.gpr[2] = value;
}

// 0x0892FABC (a0, a1, a2): 0x0892F95C's v0:v1 pair; v0 = 1 unless both are 0xFFFFFFFF (then 0).
void func_0892FABC(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const GuestResult result = call_full(rt, ctx, 0x0892F95Cu, {ctx.gpr[4], ctx.gpr[5], ctx.gpr[6]});
    ctx.gpr[2] = (result.v0 != 0xFFFFFFFFu || result.v1 != 0xFFFFFFFFu) ? 1u : 0u;
}

// 0x089893A4 (a0 = object): its member +0x1E0, reported by 0x08A5BB7C when null, is handed to 0x089CC068.
void func_089893A4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    u32 member;
    {
        SavedS0 saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        member = word(rt, self + 0x1E0u);
        ctx.gpr[16] = member;
        if (member == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA39B4u});
    }
    tail_call(ctx, 0x089CC068u, {member});
}

// 0x08AF5630 (a0 = record, a1 = value): 0x08C3FD78(*[a0+0xA0], u16[a0+0x9A], 4, a1) gives an entry whose word +0 is
// the answer (0 when null).
void func_08AF5630(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 second = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    const u32 table = word(rt, self + 0xA0u);
    const u32 half = rt.memory().load16(self + 0x9Au);
    const u32 found = call(rt, ctx, 0x08C3FD78u, {table, half, 4u, second});
    u32 value = 0u;
    if (found != 0u) value = word(rt, found);
    ctx.gpr[3] = value;
    ctx.gpr[2] = value;
}

// 0x08B49874 (f12 = x): f0 = +0 for a zero magnitude, else copysign(1.0, x) built bit by bit as the asm does.
void func_08B49874(Runtime &, AllegrexContext &ctx) {
    const u32 bits = std::bit_cast<u32>(ctx.fpr[12]);
    if ((bits & 0x7FFFFFFFu) == 0u) {
        ctx.fpr[0] = 0.0f;
    } else {
        ctx.fpr[0] = std::bit_cast<float>((bits & 0x80000000u) | 0x3F800000u);
    }
}

// 0x088A7C20 (a0 = object): 0x08AF569C(a0) gives an address; v0 = (that - word[a0+0x9C]) >> 7 (arithmetic), or -1.
void func_088A7C20(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    u32 value;
    {
        SavedS0 saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        const u32 found = call(rt, ctx, 0x08AF569Cu, {self});
        if (found == 0u) {
            value = 0xFFFFFFFFu;
        } else {
            const s32 difference = static_cast<s32>(found - word(rt, self + 0x9Cu));
            value = static_cast<u32>(difference >> 7);
        }
    }
    ctx.gpr[2] = value;
}

// 0x089327A0 (a0 = object, a1): the signed byte +0x10, when it is not -1, goes to 0x08932798(a0, byte, a1) whose answer
// is v0; a -1 byte answers 0.
void func_089327A0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 second = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    const s32 selector = signed_byte(rt, self + 0x10u);
    ctx.gpr[3] = 0xFFFFFFFFu;  // v1 = -1, as the asm sets it before the test
    u32 value = 0u;
    if (selector != -1) value = call(rt, ctx, 0x08932798u, {self, static_cast<u32>(selector), second});
    ctx.gpr[2] = value;
}

// 0x089556F4 (a0 = object): 0x0894D074(*a0) decides; when it answers 0, 0x08945ECC(a0). f0 = +0 either way.
void func_089556F4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        SavedS0 saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        const u32 answer = call(rt, ctx, 0x0894D074u, {word(rt, self)});
        if (answer == 0u) call(rt, ctx, 0x08945ECCu, {self});
    }
    ctx.fpr[0] = 0.0f;
}

// 0x08A934C4 (a0 = object): *[a0+4]'s member +0x30, reported by 0x08A5BB7C when null; v0 = that member's word +0x30.
void func_08A934C4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    u32 member;
    {
        SavedS0 saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        const u32 owner = word(rt, self + 4u);
        member = word(rt, owner + 0x30u);
        ctx.gpr[16] = member;
        if (member == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB204Cu});
    }
    ctx.gpr[2] = word(rt, member + 0x30u);
}

// 0x08B0B4CC (a0 = holder): with word[+0] zero, 0x08B0B448(word[+4], a1, a2, a3) gives v0; otherwise 0.
void func_08B0B4CC(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    u32 value = 0u;
    if (word(rt, ctx.gpr[4]) == 0u) {
        const u32 inner = word(rt, ctx.gpr[4] + 4u);
        if (inner != 0u) value = call(rt, ctx, 0x08B0B448u, {inner, ctx.gpr[5], ctx.gpr[6], ctx.gpr[7]});
    }
    ctx.gpr[2] = value;
}

// 0x08935F88 (a0 = object, a1): the byte +0x10, sign-extended; -1 clears it and passes 0. 0x08935D44(a0, byte, sp)
// fills the word at sp, which is v0.
void func_08935F88(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    const s32 extended = static_cast<std::int8_t>(rt.memory().load8(self + 0x10u));
    u32 argument = static_cast<u32>(extended);
    if (extended == -1) {
        put8(rt, self + 0x10u, 0u);
        argument = 0u;
    }
    call(rt, ctx, 0x08935D44u, {self, argument, frame.at(0u)});
    ctx.gpr[2] = word(rt, frame.at(0u));
    ctx.gpr[3] = 0u;
}

// 0x08ABBAE0 (a0 = holder): the virtual +0x18 of the object *holder, called with that object; 0 for a null holder or object.

// 0x08ABC9D0 (a0 = holder, a1, a2, a3): inner = *holder; a0 = inner, a1 = holder, a2 = a1, a3 = a2, t0 = a3 & 0xFF;
// then the virtual +0x30 of inner's vtable. A null holder or inner answers 0.
void func_08ABC9D0(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 second = ctx.gpr[5];
    const u32 third = ctx.gpr[6];
    const u32 flag = ctx.gpr[7] & 0xFFu;
    ctx.gpr[3] = second;
    ctx.gpr[9] = third;
    ctx.gpr[8] = flag;
    if (object == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 inner = word(rt, object);
    ctx.gpr[5] = object;
    ctx.gpr[6] = second;
    ctx.gpr[4] = inner;
    ctx.gpr[7] = third;
    if (inner == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 table = word(rt, inner);
    const u32 function = word(rt, table + 0x30u);
    ctx.gpr[2] = table;
    ctx.gpr[25] = function;
    tail_call(ctx, function, {inner, object, second, third, flag});
}

// 0x08ACE9DC (a0 = base, a1 = index, a2 = value): record a0 + a1*236; when its byte +0x41D is clear, set it, store a2
// at +0x3E0 and tail-call 0x08ACE96C(a0, record + 0x334). An already-set record answers v0 = a1.
void func_08ACE9DC(Runtime &rt, AllegrexContext &ctx) {
    const u32 base = ctx.gpr[4];
    const u32 index = ctx.gpr[5];
    const u32 value = ctx.gpr[6];
    const u32 record = base + index * 236u;
    ctx.gpr[5] = record + 0x334u;
    if (rt.memory().load8(record + 0x41Du) != 0u) {
        ctx.gpr[2] = index;
        return;
    }
    ctx.gpr[2] = 1u;
    put8(rt, record + 0x41Du, 1u);
    put(rt, record + 0x3E0u, value);
    // a3 = the record (set before the flag test) goes along with a0/a1/a2, as the original leaves it.
    tail_call(ctx, 0x08ACE96Cu, {base, record + 0x334u, value, record});
}

// 0x08AD1700 (a0 = object): clears its word +0x1C0, 0x08AD1528(a0), then tail-calls 0x08AD1660(a0).
void func_08AD1700(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        SavedS0 saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        put(rt, self + 0x1C0u, 0u);  // the call's delay slot
        call(rt, ctx, 0x08AD1528u, {self});
    }
    tail_call(ctx, 0x08AD1660u, {self});
}

// 0x08AD1CE0 (a0 = object): with the member +0x1C set, 1 when its virtual +0x24 answers non-zero; otherwise 0.
void func_08AD1CE0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 inner = word(rt, ctx.gpr[4] + 0x1Cu);
    u32 value = 0u;
    if (inner != 0u) {
        const u32 vtable = word(rt, inner);
        ctx.gpr[3] = vtable;
        if (call_indirect(rt, ctx, word(rt, vtable + 0x24u), {inner}) != 0u) value = 1u;
    }
    ctx.gpr[2] = value;
}

// 0x08AD384C (a0 = object): 0x08AD3638(a0); with the member +0x30 set, 0x08B64654(that) is tail-called, else v0 = 0.
void func_08AD384C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    u32 member;
    {
        SavedS0 saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        call(rt, ctx, 0x08AD3638u, {self});
        member = word(rt, self + 0x30u);
    }
    if (member == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    ctx.gpr[2] = member;
    tail_call(ctx, 0x08B64654u, {member});
}

// 0x08ADFDE0 (a0 = table index, a1 = value): 0x08ADDCD4(a0, 0x08C91144) names the record; 0x08ADFCEC(record, a1)
// when set. v0 = that answer (0 when the name is not found).
void func_08ADFDE0(Runtime &rt, AllegrexContext &ctx) {
    const u32 value = ctx.gpr[5];
    u32 found;
    {
        SavedS0 saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = value;
        found = call(rt, ctx, 0x08ADDCD4u, {ctx.gpr[4], 0x08C91144u});
        if (found != 0u) found = call(rt, ctx, 0x08ADFCECu, {found, value});
    }
    ctx.gpr[2] = found;
}

} // namespace
} // namespace new_batch_main_1564
} // namespace gt::game

#define NEW1564(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1564::name, gt::ReturnKind::kind)
NEW1564(0x0890B454u, func_0890B454, Word);
NEW1564(0x089C7274u, func_089C7274, Word);
NEW1564(0x08ADDABCu, func_08ADDABC, Word);
NEW1564(0x08A5BB2Cu, func_08A5BB2C, Word);
NEW1564(0x08ADDAE8u, func_08ADDAE8, Word);
NEW1564(0x08B339FCu, func_08B339FC, Word);
NEW1564(0x089557FCu, func_089557FC, Word);
NEW1564(0x08AE1CBCu, func_08AE1CBC, Word);
NEW1564(0x08AF27F4u, func_08AF27F4, Word);
NEW1564(0x088DF394u, func_088DF394, Word);
NEW1564(0x0892FABCu, func_0892FABC, Word);
NEW1564(0x089893A4u, func_089893A4, Word);
NEW1564(0x08AF5630u, func_08AF5630, Word);
NEW1564(0x08B49874u, func_08B49874, Float);
NEW1564(0x088A7C20u, func_088A7C20, Word);
NEW1564(0x089327A0u, func_089327A0, Word);
NEW1564(0x089556F4u, func_089556F4, Float);
NEW1564(0x08A934C4u, func_08A934C4, Word);
NEW1564(0x08B0B4CCu, func_08B0B4CC, Word);
NEW1564(0x08935F88u, func_08935F88, Word);
NEW1564(0x08ABC9D0u, func_08ABC9D0, Word);
NEW1564(0x08ACE9DCu, func_08ACE9DC, Word);
NEW1564(0x08AD1700u, func_08AD1700, Word);
NEW1564(0x08AD1CE0u, func_08AD1CE0, Word);
NEW1564(0x08AD384Cu, func_08AD384C, Word);
NEW1564(0x08ADFDE0u, func_08ADFDE0, Word);
