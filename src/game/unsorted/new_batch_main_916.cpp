// New batch, main session 916: functions the extra seeds made emittable, written by hand as structured C from `tools.disasm`.
// Three real bodies (a tree walk that keeps two intrusive lists in step, the command-text tokeniser and a piecewise curve
// evaluator) and the two-instruction tail thunks around them. Draft only: the differential gate decides what lands.
// 0x08BB1534 is not here: its whole body is `j 0x08BB1534; nop` (a jump to itself), so a case could only hang.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <array>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_916 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u8 = std::uint8_t;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call; afterwards `v0` holds its result, as after the original's `jal`.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// `lw v1, slot(v0); jalr v1`: the callee starts with v0 = its own address.
u32 call_indirect(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    ctx.gpr[2] = target;
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}

// A `jalr` whose v0 was loaded just before the slot (`lw v0, 0(a0); lw v1, slot(v0); jalr v1`): the callee starts
// with v0 still holding that loaded word, not its own address.
u32 call_keeping_v0(Runtime &rt, AllegrexContext &ctx, u32 v0, u32 target,
                    std::initializer_list<u32> arguments) {
    ctx.gpr[2] = v0;
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}

// A callee that takes its float in $f12 and answers in $f0.
float call_float(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                 std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.f0;
}

// The `ins v0, zero, 0, 4` before the original's alloca: bytes with the low four cleared.
u32 rounded_block(u32 bytes) { return bytes & ~0xFu; }

u8 byte_at(Runtime &rt, u32 address) { return rt.memory().load8(address); }
s32 signed_byte(Runtime &rt, u32 address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }
void put_byte(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<u8>(value)); }

// s0-s7 are callee-saved: the original keeps values in them and restores the caller's on every exit.
struct SavedCallee {
    AllegrexContext &ctx;
    std::array<u32, 8> saved;
    explicit SavedCallee(AllegrexContext &c) : ctx(c) {
        for (unsigned i = 0; i < 8u; ++i) saved[i] = c.gpr[16u + i];
    }
    ~SavedCallee() {
        for (unsigned i = 0; i < 8u; ++i) ctx.gpr[16u + i] = saved[i];
    }
    SavedCallee(const SavedCallee &) = delete;
    SavedCallee &operator=(const SavedCallee &) = delete;
};

// 0x089BB55C (a0 / a1 = two objects with a list at +0x44): for every pair of nodes (walked together through +0x18,
// then +0x54) recurses on the nodes' +0xC members; then takes the reference at 0x08CF6ED8 into the two local holders
// at sp+8 / sp+0xC and passes the second one to a1's virtual slot 0x10C (with the descriptor at 0x08CF6F30); the
// holder at sp+0xC, when non-null, has the caller (a0) copied into sp+4 (that copy's vtable slot 0x1C gets the object,
// the holder at sp+8 and the copy), and finally the holder at sp+8 is passed to another virtual slot 0xCC. Each holder
// is released (0x08A5B9EC) after use. v0 is the last release's.

// 0x08B518F4 (a0 = command record: text at +0x20, lengths at +0x1C / +0x14, kind at +0x18, context at +0x10, the two
// callbacks at +4 / +8): tokenises the text with 0x08B52238 / 0x08B51F38, building the argument table at fp+0x28, then
// copies the tokens into a prefix at the lowered sp and a table of C strings, and hands them to the callback the kind
// selects (1: +4 with the table, 2: +8, 3: both, 0/other: none). v0 = 1 when a callback handles it, 0 for kind 0.
void func_08B518F4(Runtime &rt, AllegrexContext &ctx) {
    const u32 context = ctx.gpr[4];
    SavedCallee saved(ctx);
    GuestFrame frame(ctx, 0x880u);
    const u32 fp = frame.at(0);
    u32 stack = fp;
    // Every alloca lowers sp, as the original's `ins`-rounded `subu sp` does: the callees' frames start below the
    // strings and the table, not on top of them.
    const auto alloca_block = [&](u32 bytes) {
        stack -= rounded_block(bytes);
        ctx.gpr[29] = stack;
        return stack;
    };
    u32 records = 0u;
    u32 previous = 0u;
    u32 state = 0u;
    u32 counter = 0xFFFFFFFFu;
    u32 limit = 0u;
    u32 cursor = fp + 0x28u;
    u32 quoted = 0u;
    put(rt, fp + 0x834u, context + 0x20u);  // the `sw v0` in the call's delay slot: the text, not the return value
    call(rt, ctx, 0x08B52238u, {fp, context + 0x20u, word(rt, context + 0x1Cu), word(rt, context + 0x14u)});
    const u32 flag = word(rt, context + 0x18u) == 2u ? 1u : 0u;
    put(rt, fp + 0x830u, flag);
    limit = word(rt, context + 0x1Cu) - 1u;
    for (;;) {
        const u32 token = call(rt, ctx, 0x08B51F38u, {fp});
        if (token == 0u) break;
        const u32 text = word(rt, fp + 0x20u);
        s32 first = 0;
        if (byte_at(rt, token + 0xCu) != 0u) first = signed_byte(rt, token + 0x04u);
        const u32 toggle = quoted ^ 1u;
        if (state == 1u && first == 0x3D) {  // '=' right after the separator
            put(rt, cursor + 4u, previous);
            cursor += 8u;
            records += 1u;
            put(rt, cursor, text);
            state = 2u;
        }
        if ((static_cast<u32>(first) ^ 0x22u) == 0u) quoted = toggle;  // a quote toggles the quoted state
        if (quoted != 0u) {
            counter += 1u;
            previous = text;
            continue;
        }
        if (first == 0x20 || first == 0x09 || first == 0x0D || first == 0x0A) {
            const bool closing = state < 2u;
            if (limit == word(rt, context + 0x1Cu) - 1u) limit = counter;
            if (!closing) {
                put(rt, cursor + 4u, previous);
                records += 1u;
                cursor += 8u;
            }
            put(rt, cursor, text);
            state = 1u;
        }
        counter += 1u;
        previous = text;
    }
    if (state >= 2u) {
        put(rt, fp + 0x28u + records * 8u + 4u, previous);
        records += 1u;
    }
    const u32 length = limit - flag;
    const u32 prefix = alloca_block(length + 0x20u);
    call(rt, ctx, 0x08B36018u, {prefix, 0u, length + 2u});
    call(rt, ctx, 0x08B79A88u, {prefix, word(rt, fp + 0x834u) + flag, length + 1u});
    const u32 words = records * 4u;
    const u32 table = alloca_block(words + 0x22u);
    put(rt, table + words, 0u);  // the terminator: stored unconditionally, it sits in the loop test's delay slot
    u32 pointer_cursor = table;
    u32 record_cursor = fp + 0x2Cu;
    for (u32 i = 0u; i < records; ++i) {
        const u32 start = word(rt, record_cursor - 4u);
        const u32 end = word(rt, record_cursor);
        const u32 span = end - start;
        const u32 entry = alloca_block(span + 0x1Fu);
        put(rt, pointer_cursor, entry);
        u32 copy_length = span;
        u32 source = start;
        if (signed_byte(rt, start) == 0x22) {  // strip the surrounding quotes
            copy_length = span - 2u;
            source = start + 1u;
        }
        call(rt, ctx, 0x08B79A88u, {entry, source, copy_length});
        record_cursor += 8u;
        pointer_cursor += 4u;
        put_byte(rt, entry + copy_length, 0u);
    }
    const u32 kind = word(rt, context + 0x18u);
    const u32 base = word(rt, context + 0x10u);
    const u32 first_callback = word(rt, context + 0x04u);
    const u32 second_callback = word(rt, context + 0x08u);
    u32 answer;
    if (kind == 1u) {
        if (first_callback != 0u) call_indirect(rt, ctx, first_callback, {base, prefix, table});
        answer = 1u;
    } else if (kind == 2u) {
        if (second_callback != 0u) call_indirect(rt, ctx, second_callback, {base, prefix});
        answer = 1u;
    } else if (kind == 3u) {
        if (first_callback != 0u) call_indirect(rt, ctx, first_callback, {base, prefix, table});
        if (second_callback != 0u) call_indirect(rt, ctx, second_callback, {base, prefix, table});
        answer = 1u;
    } else {
        answer = kind == 0u ? 0u : 1u;
    }
    call(rt, ctx, 0x08B52284u, {fp});
    ctx.gpr[2] = answer;
}

// 0x0893D6A4 (a0 = a curve block with the breakpoints at +0x18..+0x34, a1 = a table index, f12 = x): maps x through
// the block. The byte at 0x08CDE655 selects the plain piecewise form; otherwise the shape is eased through the curve
// at 0x08B49D7C and blended with the weights at 0x08CF2A28..0x08CF2A40 when the byte at 0x08CF2A48 is set.
// v0 = f0 = the mapped value.
void func_0893D6A4(Runtime &rt, AllegrexContext &ctx) {
    const u32 block = ctx.gpr[4];
    const u32 index = ctx.gpr[5];
    const float input = ctx.fpr[12];
    SavedCallee saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    const float a18 = float_at(rt, block + 0x18u);
    const float a1c = float_at(rt, block + 0x1Cu);
    const float a20 = float_at(rt, block + 0x20u);
    const float a24 = float_at(rt, block + 0x24u);
    const float a28 = float_at(rt, block + 0x28u);
    const float a2c = float_at(rt, block + 0x2Cu);
    const float a30 = float_at(rt, block + 0x30u);
    const float a34 = float_at(rt, block + 0x34u);
    const auto curve = [&](float x) { return call_float(rt, ctx, 0x08B49D7Cu, {}, {x}); };
    float result;
    if (byte_at(rt, 0x08CDE655u) != 0u) {
        if (input < 0.0f) {
            if (input <= -a20) {
                result = a24;
            } else if (-a18 <= input) {
                result = psp_mul(-input, a1c) / a18;
            } else {
                result = a1c + psp_mul(-input - a18, a24 - a1c) / (a20 - a18);
            }
        } else if (!(0.0f < input)) {
            result = 0.0f;
        } else if (a30 <= input) {
            result = -a34;
        } else if (input <= a28) {
            result = psp_mul(-input, a2c) / a28;
        } else {
            result = -(a2c + psp_mul(input - a28, a34 - a2c) / (a30 - a28));
        }
    } else if (input < 0.0f) {
        float shaped = input;
        float base = -input;
        if (input < -1.0f) {
            shaped = -1.0f;
            base = 1.0f;
        }
        const float plain = a1c + psp_mul(a20, curve(base - a18));
        if (byte_at(rt, 0x08CF2A48u) == 0u) {
            result = plain;
        } else {
            const u32 offset = index * 4u;
            const float entry = float_at(rt, 0x08CF2A40u + offset);
            const float edge = -entry;
            if (!(edge < shaped)) {
                result = plain;
            } else {
                const float weight = float_at(rt, 0x08CF2A38u + offset);
                const float scaled = psp_mul(psp_mul(a1c + psp_mul(a20, curve(entry - a18)), psp_mul(shaped, shaped)) /
                                                 psp_mul(edge, edge),
                                             weight);
                result = psp_mul(plain, 1.0f - weight) + scaled;
            }
        }
    } else {
        const float shaped = 1.0f < input ? 1.0f : input;
        const float plain = -a2c - psp_mul(a30, curve(shaped - a28));
        if (byte_at(rt, 0x08CF2A48u) == 0u) {
            result = plain;
        } else {
            const u32 offset = index * 4u;
            const float edge = float_at(rt, 0x08CF2A30u + offset);
            if (!(shaped < edge)) {
                result = plain;
            } else {
                const float weight = float_at(rt, 0x08CF2A28u + offset);
                const float scaled = psp_mul(psp_mul(-a2c - psp_mul(a30, curve(edge - a28)), psp_mul(shaped, shaped)) /
                                                 psp_mul(edge, edge),
                                             weight);
                result = psp_mul(plain, 1.0f - weight) + scaled;
            }
        }
    }
    ctx.fpr[0] = result;
}

// 0x088057A0: j 0x08A4D37C; nop
// 0x0885C0D4: j 0x08864A74; lw a0, 4(a0)
// 0x0885F428: j 0x0885F3FC; lw a0, 0(a0)
// 0x08891560: j 0x088910E4; lw a0, 0(a0)
// 0x088A3710: j 0x088A5E5C; addiu a0, a0, 8
// 0x089F0AF4: j 0x08B0B8CC; addiu a0, a0, 0xC
// 0x08A8DB8C: j 0x08A8DAB4; addiu a2, zero, 5
// 0x08ACC0EC: j 0x08ACBC10; lw a0, 0(a0)
// 0x08AD096C: j 0x08B2E9F8; nop
// 0x08B19FEC: j 0x08B19FB0; lw a0, 0(a0)
// 0x08B2E2B8: j 0x08C7A988; nop  (0x08C7A988 is a `.sceStub.text` entry, ThreadManForUser import 0xCEADEB47)
// 0x08B2EFF0: j 0x08B599E0; addiu a0, zero, 1
// 0x08B3027C: j 0x08B301C4; nop
// 0x08B35444: j 0x08C7A928; nop  (0x08C7A928 is a `.sceStub.text` entry, SysMemUserForUser import 0xF919F628)
// 0x08BBFF7C: j 0x08BBFE68; addiu a0, a0, 0x98
// 0x08C501D8: j 0x08C50034; nop
// 0x0885E83C: lw v0, 0(a0); j 0x088D80D4; lw a0, 0x5AC(v0)
// 0x0886A020: lw a0, 4(a0); j 0x0886B9A4; addiu a0, a0, 0x4238
// 0x08892468: lw a0, 0(a0); j 0x088921E0; addiu t2, zero, 1
// 0x089B17AC: addiu a2, a0, 0x18; j 0x089B14F0; addiu a1, a0, 0x30
// 0x08ABFA0C: addu a1, a0, zero; j 0x08B1FA64; lw a0, 0xC(a0)
// 0x08ABFE64: addu a1, a0, zero; j 0x08ABFDD0; lw a0, 0xC(a0)
// 0x08AC0B5C: lui a0, 2; j 0x08B0E234; addiu a0, a0, 0x3680 (the relocated 0x08CFFB80)
// 0x08AD0830: lui a0, 9; j 0x08B3252C; addiu a0, a0, -0x3390 (the relocated 0x08D69170)
// 0x08B0DF0C: addu a1, a0, zero; j 0x08B10C74; lw a0, 0x38(a0)
// 0x08B2E224: lw a0, 0(a0); j 0x08C7A9D8; addu a1, zero, zero  (the target is a `.sceStub.text` import entry)

} // namespace
} // namespace new_batch_main_916
} // namespace gt::game

#define NEW916(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_916::name, gt::ReturnKind::kind)
NEW916(0x08B518F4u, func_08B518F4, Word);
NEW916(0x0893D6A4u, func_0893D6A4, Float);
