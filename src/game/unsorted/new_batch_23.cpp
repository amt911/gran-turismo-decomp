// New batch 23: 15 of the 80 candidate functions (400-440 bytes each), written by hand from `tools/disasm`. Every body is structured C:
// branches and loops as they read, named typed locals, guest memory through the batch helpers, and a call only at the original's
// `jal` (the argument registers the asm sets, a delay slot's work done before the call). Callee-saved registers are set where the
// original sets them (a callee may read them) and put back at the end; `v0` ends as every exit path of the asm leaves it, including
// the value a void callee returns untouched. No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_23 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call; afterwards `v0` holds its result, as after the original's `jal`.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// The same for a callee that takes its arguments in $f12... ($f0 comes back and is kept in $f0, as the asm leaves it).
GuestResult call_float(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                       std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
s32 signed_byte_at(Runtime &rt, u32 address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }
s32 signed_half_at(Runtime &rt, u32 address) { return static_cast<std::int16_t>(rt.memory().load16(address)); }
// Allegrex `trunc.w.s`: toward zero; NaN and out of range give 0x7FFFFFFF.
s32 trunc_w_s(float value) {
    if (!(value >= -2147483648.0f && value < 2147483648.0f)) return 0x7FFFFFFF;
    return static_cast<s32>(value);
}

// s0-s7 and fp are callee-saved: the body sets them like the original (a callee may read them) and the caller's values come back.
class SavedRegisters {
public:
    explicit SavedRegisters(AllegrexContext &ctx) : ctx_(ctx) {
        for (unsigned i = 0u; i < 8u; ++i) saved_[i] = ctx.gpr[16u + i];
        saved_[8] = ctx.gpr[30];
    }
    ~SavedRegisters() {
        for (unsigned i = 0u; i < 8u; ++i) ctx_.gpr[16u + i] = saved_[i];
        ctx_.gpr[30] = saved_[8];
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    AllegrexContext &ctx_;
    u32 saved_[9];
};
// The callee-saved float registers a few of these functions keep their work in.
class SavedFloats {
public:
    SavedFloats(AllegrexContext &ctx, unsigned first, unsigned count) : ctx_(ctx), first_(first), count_(count) {
        for (unsigned i = 0u; i < count_; ++i) saved_[i] = ctx.fpr[first_ + i];
    }
    ~SavedFloats() {
        for (unsigned i = 0u; i < count_; ++i) ctx_.fpr[first_ + i] = saved_[i];
    }
    SavedFloats(const SavedFloats &) = delete;
    SavedFloats &operator=(const SavedFloats &) = delete;

private:
    AllegrexContext &ctx_;
    unsigned first_;
    unsigned count_;
    float saved_[12] = {};
};
// --- a list of per-entry values (a0 = this, a1 = a key) ----------------------------------------------------------------
// 0x08BD48FC switches on the word at +0x10: 3 hands the float at +0x18 with (a1, this) to 0x08AF2978 and returns; anything but 2
// goes to 0x08AF213C(this). For 2 it reads the table held at word(this + 4) (word +0x30 entries, u16 +0x10 of 0x30-byte records),
// asks 0x089063A8 whether the key matches one record (the signed byte at +0x14 when the word at +0x0C is negative, or its value as
// an index) and then formats that record's float at +4 through 0x08906304 into 0x08AF2978(a1, record). The split when the word at
// +0x0C is negative is "every record" (0x08BD2EB4 supplies the index each time) against one record.
void func_08BD48FC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x50u);
    const u32 kind = word(rt, self + 0x10u);
    if (kind == 3u) {
        call_float(rt, ctx, 0x08AF2978u, {argument, self}, {float_at(rt, self + 0x18u)});
        return;
    }
    if (kind != 2u) {
        call(rt, ctx, 0x08AF213Cu, {self});
        return;
    }
    const u32 found = call(rt, ctx, 0x08BD2E04u, {frame.at(4u), argument, 0u});
    const u32 key = call(rt, ctx, 0x08BD2DDCu, {argument, static_cast<u32>(signed_byte_at(rt, self + 0x14u))});
    const u32 index = word(rt, self + 0xCu);
    const u32 object = word(rt, word(rt, self + 4u));
    const u32 entries = word(rt, object + 0x30u);
    if (static_cast<std::int32_t>(index) >= 0) {
        const u32 record = entries + index * 48u;
        if (call(rt, ctx, 0x089063A8u, {object, key, index}) == 0u) return;
        const GuestResult formatted = call_float(rt, ctx, 0x08906304u, {found, record + 4u}, {});
        call_float(rt, ctx, 0x08AF2978u, {argument, self}, {formatted.f0});
        return;
    }
    const u32 span = static_cast<u32>(rt.memory().load16(object + 0x10u)) * 48u;
    u32 record = entries;
    const u32 end = entries + span;
    u32 entry_index = 0u;
    ctx.gpr[2] = span;  // `subu v0, count << 6, count << 4`, what the epilogue returns when the loop is empty
    if (record == end) return;
    do {
        if (call(rt, ctx, 0x089063A8u, {object, key, entry_index}) != 0u) {
            put(rt, frame.at(0u), entry_index);
            const u32 value = call(rt, ctx, 0x08BD2EB4u, {self, frame.at(0u), entry_index});
            const GuestResult formatted = call_float(rt, ctx, 0x08906304u, {found, record + 4u}, {});
            call_float(rt, ctx, 0x08AF2978u, {argument, value}, {formatted.f0});
        }
        record += 0x30u;
        ++entry_index;
    } while (record != end);
}

// --- three entries of one object's update path (a0 = this) -------------------------------------------------------------
// 0x08C70FE8, 0x08C71D00, 0x08C71544, 0x08C717C8 and 0x08C71A7C hold the same body, differing in the vtable slot called
// (0x8, 0x24, 0x18, 0x1C, 0x20) and in whether it gets a1 (masked to a byte for 0x08C70FE8) or a2 and a3. 0x08C70314 fills a local guard; when it says the object is alive, the object derived from the vptr
// (`this + word(vptr - 0xC)`) hands its word +0x80 to the virtual with (this + 8 as a {count, byte} local, this, the byte
// at +0x74, the argument); when the virtual's v1 is nonzero the derived object's flags at +0x14 get bit 0. Then the derived
// object's word +0x0C is tested for 0x2000: with it set, 0x08B62318 asks whether to go on and the object at +0x78 (if any) is
// asked with its virtual +0x18; -1 from that virtual raises the flags again. v0 = this on every path that returns.
void update_through_vtable(Runtime &rt, AllegrexContext &ctx, u32 slot, bool single_argument, u32 first_extra,
                           u32 second_extra) {
    const u32 self = ctx.gpr[4];
    const u32 argument = first_extra;
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x50u);
    call(rt, ctx, 0x08C70314u, {frame.at(0x10u), self});
    if (rt.memory().load8(frame.at(0x10u)) != 0u) {
        const u32 derived = self + word(rt, word(rt, self) - 0xCu);
        const u32 host = word(rt, derived + 0x80u);
        if (host == 0u) {
            call(rt, ctx, 0x08B5A9FCu);
            return;
        }
        const u32 count = word(rt, derived + 0x78u);
        const u32 empty = count < 1u ? 1u : 0u;  // `sltiu s4, s3, 1`
        if (rt.memory().load8(derived + 0x75u) == 0u) {
            rt.memory().store8(derived + 0x74u, call(rt, ctx, 0x08C5477Cu, {derived, 0x20u}));
            rt.memory().store8(derived + 0x75u, 1u);
        }
        const u32 local = frame.at(8u);  // {word count; byte empty} handed to the virtual
        put(rt, local, count);
        rt.memory().store8(local + 4u, empty);
        const s32 first = static_cast<std::int8_t>(rt.memory().load8(derived + 0x74u));
        const u32 function = word(rt, word(rt, host) + slot);
        const GuestResult result =
            single_argument
                ? call_indirect(rt, ctx, function, {host, local, derived, static_cast<u32>(first), argument}, {})
                : call_indirect(rt, ctx, function, {host, local, derived, static_cast<u32>(first), argument, second_extra}, {});
        put(rt, frame.at(0x20u), result.v0);
        if ((result.v1 & 0xFFu) != 0u) {
            const u32 owner = self + word(rt, word(rt, self) - 0xCu);
            call(rt, ctx, 0x08C54CFCu, {owner, word(rt, owner + 0x14u) | 1u});
        }
    }
    const u32 guard = word(rt, frame.at(0x14u));
    const u32 derived = guard + word(rt, word(rt, guard) - 0xCu);
    if ((word(rt, derived + 0xCu) & 0x2000u) == 0u) {
        ctx.gpr[2] = self;
        return;
    }
    if (call(rt, ctx, 0x08B62318u) != 0u) {
        ctx.gpr[2] = self;
        return;
    }
    const u32 guard_again = word(rt, frame.at(0x14u));
    const u32 inner = guard_again + word(rt, word(rt, guard_again) - 0xCu);
    const u32 held = word(rt, inner + 0x78u);
    if (held == 0u) {
        ctx.gpr[2] = self;
        return;
    }
    if (call_indirect(rt, ctx, word(rt, word(rt, held) + 0x18u), {held}) != 0xFFFFFFFFu) {
        ctx.gpr[2] = self;
        return;
    }
    const u32 object = word(rt, frame.at(0x14u));
    const u32 base = object + word(rt, word(rt, object) - 0xCu);
    call(rt, ctx, 0x08C54CFCu, {base, word(rt, base + 0x14u) | 1u});
    ctx.gpr[2] = self;
}
void func_08C70FE8(Runtime &rt, AllegrexContext &ctx) {
    update_through_vtable(rt, ctx, 0x8u, true, ctx.gpr[5] & 0xFFu, 0u);
}
void func_08C71D00(Runtime &rt, AllegrexContext &ctx) {
    update_through_vtable(rt, ctx, 0x24u, true, ctx.gpr[5], 0u);
}
void func_08C71544(Runtime &rt, AllegrexContext &ctx) {
    update_through_vtable(rt, ctx, 0x18u, false, ctx.gpr[6], ctx.gpr[7]);
}
void func_08C717C8(Runtime &rt, AllegrexContext &ctx) {
    update_through_vtable(rt, ctx, 0x1Cu, false, ctx.gpr[6], ctx.gpr[7]);
}
void func_08C71A7C(Runtime &rt, AllegrexContext &ctx) {
    update_through_vtable(rt, ctx, 0x20u, false, ctx.gpr[6], ctx.gpr[7]);
}

// --- two constructors of one object (a0 = this) ------------------------------------------------------------------------
// 0x08C77004 and 0x08C771D4 hold the same code: two vtable words are stored in turn, the members at +0x1C and +0x24 are set up
// (0x08B5E0EC, 0x08B61528), the fields from +0x2C to +0xA0 are cleared with a size word 0x400 at +0x7C, and then the name
// 0x08D721C8 is looked up twice (0x08B5C2B0) in the table at word(this + 0x1C): {word +8 = count, word +4 = entries}. A missing
// entry leaves the second lookup to report it (0x08B5A9FC); a found one is kept at +0x90.
void construct_with_lookup(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    put(rt, self, 0x08CDBDF0u);
    for (u32 offset = 4u; offset <= 0x18u; offset += 4u) put(rt, self + offset, 0u);
    call(rt, ctx, 0x08B5E0ECu, {self + 0x1Cu});
    put(rt, self, 0x08CDC298u);
    put(rt, self + 0x20u, 0u);
    call(rt, ctx, 0x08B61528u, {self + 0x24u, self + 0x20u});
    put(rt, self + 0x7Cu, 0x400u);
    for (u32 offset = 0x2Cu; offset <= 0x78u; offset += 4u) put(rt, self + offset, 0u);
    for (u32 offset = 0x80u; offset <= 0x83u; offset += 1u) rt.memory().store8(self + offset, 0u);
    put(rt, self + 0x84u, 0u);
    put(rt, self + 0x88u, 0u);
    rt.memory().store8(self + 0x8Cu, 0u);
    for (u32 offset = 0x90u; offset <= 0x9Cu; offset += 4u) put(rt, self + offset, 0u);
    put(rt, self + 0xA0u, 0u);
    const u32 index = call(rt, ctx, 0x08B5C2B0u, {0x08D721C8u});
    const u32 table = word(rt, self + 0x1Cu);
    const u32 entries = word(rt, table + 4u);
    if (index >= word(rt, table + 8u)) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 slot = entries + index * 4u;
    if (word(rt, slot) == 0u) {
        ctx.gpr[2] = slot;
        return;
    }
    const u32 index_again = call(rt, ctx, 0x08B5C2B0u, {0x08D721C8u});
    const u32 table_again = word(rt, self + 0x1Cu);
    if (index_again < word(rt, table_again + 8u)) {
        const u32 value = word(rt, word(rt, table_again + 4u) + index_again * 4u);
        if (value != 0u) {
            put(rt, self + 0x90u, value);
            ctx.gpr[2] = value;
            return;
        }
    }
    call(rt, ctx, 0x08B5A9FCu);
}
void func_08C77004(Runtime &rt, AllegrexContext &ctx) { construct_with_lookup(rt, ctx); }
void func_08C771D4(Runtime &rt, AllegrexContext &ctx) { construct_with_lookup(rt, ctx); }

// --- reading three named values out of a registry (a0 = this, a1 = the registry owner) -----------------------------------
// 0x08C548C4 looks each name up (0x08B5C2B0) in the table whose head is word(a1) ({word +8 = count, word +4 = entries}):
// 0x08D721D4 opens the path (the found value is passed to 0x08B64C80 and the answer kept at +0x7C); 0x08D7747C at +0x80 and
// 0x08D77480 at +0x84 are looked up a second time (the compiler could not keep the first answer) and a vanished entry is an
// error (0x08B5A9FC). A name that resolves to nothing stores 0.
bool registry_value(Runtime &rt, AllegrexContext &ctx, u32 owner, u32 name, u32 &value) {
    const u32 index = call(rt, ctx, 0x08B5C2B0u, {name});
    const u32 table = word(rt, owner);
    const u32 entries = word(rt, table + 4u);
    if (index >= word(rt, table + 8u)) return false;
    const u32 entry = word(rt, entries + index * 4u);
    if (entry == 0u) return false;
    value = entry;
    return true;
}
void func_08C548C4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 owner = ctx.gpr[5];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    u32 value = 0u;
    if (registry_value(rt, ctx, owner, 0x08D721D4u, value)) value = call(rt, ctx, 0x08B64C80u, {owner});
    put(rt, self + 0x7Cu, value);
    for (u32 slot = 0x80u; slot <= 0x84u; slot += 4u) {
        const u32 name = slot == 0x80u ? 0x08D7747Cu : 0x08D77480u;
        if (!registry_value(rt, ctx, owner, name, value)) {
            put(rt, self + slot, 0u);
            ctx.gpr[2] = 0u;
            continue;
        }
        if (!registry_value(rt, ctx, owner, name, value)) {
            call(rt, ctx, 0x08B5A9FCu);
            return;
        }
        put(rt, self + slot, value);
        ctx.gpr[2] = value;
    }
}
// --- a 4x4 matrix built from four axis floats (a0 = this, a1 = 4 floats) ------------------------------------------------
// 0x08AD17C4 fills four rows of a local matrix with the caller's four floats, hands each row to 0x08B4A6A0 with its component
// scaled by -2 (a rotation row), adds 1 to the three diagonal terms, zeroes the three top-right terms and the bottom row's first
// three, and gives that matrix with the record at this + 0x80 + this[0x1C0] * 80 to 0x08B4E994.
void func_08AD17C4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    SavedRegisters saved(ctx);
    SavedFloats floats(ctx, 20, 1);
    GuestFrame frame(ctx, 0x60u);
    const float w = float_at(rt, source);
    const float x = float_at(rt, source + 4u);
    const float y = float_at(rt, source + 8u);
    const float z = float_at(rt, source + 0xCu);
    ctx.fpr[20] = -2.0f;  // `lui $at, 0xC000; mtc1`
    for (u32 row = 0u; row < 0x40u; row += 0x10u) {
        put_float(rt, frame.at(row), w);
        put_float(rt, frame.at(row + 4u), x);
        put_float(rt, frame.at(row + 8u), y);
        put_float(rt, frame.at(row + 0xCu), z);
    }
    call_float(rt, ctx, 0x08B4A6A0u, {frame.at(0u)}, {psp_mul(w, ctx.fpr[20])});
    call_float(rt, ctx, 0x08B4A6A0u, {frame.at(0x10u)}, {psp_mul(x, ctx.fpr[20])});
    call_float(rt, ctx, 0x08B4A6A0u, {frame.at(0x20u)}, {psp_mul(y, ctx.fpr[20])});
    call_float(rt, ctx, 0x08B4A6A0u, {frame.at(0x30u)}, {psp_mul(z, ctx.fpr[20])});
    const u32 matrix = self + 0x40u;
    put_float(rt, matrix + 0x00u, float_at(rt, frame.at(0x00u)) + 1.0f);
    put_float(rt, matrix + 0x04u, float_at(rt, frame.at(0x04u)));
    put_float(rt, matrix + 0x08u, float_at(rt, frame.at(0x08u)));
    put_float(rt, matrix + 0x0Cu, 0.0f);
    put_float(rt, matrix + 0x10u, float_at(rt, frame.at(0x10u)));
    put_float(rt, matrix + 0x14u, float_at(rt, frame.at(0x14u)) + 1.0f);
    put_float(rt, matrix + 0x18u, float_at(rt, frame.at(0x18u)));
    put_float(rt, matrix + 0x1Cu, 0.0f);
    put_float(rt, matrix + 0x20u, float_at(rt, frame.at(0x20u)));
    put_float(rt, matrix + 0x24u, float_at(rt, frame.at(0x24u)));
    put_float(rt, matrix + 0x28u, float_at(rt, frame.at(0x28u)) + 1.0f);
    put_float(rt, matrix + 0x2Cu, 0.0f);
    put_float(rt, matrix + 0x30u, float_at(rt, frame.at(0x30u)));
    put_float(rt, matrix + 0x34u, float_at(rt, frame.at(0x34u)));
    put_float(rt, matrix + 0x38u, float_at(rt, frame.at(0x38u)));
    put_float(rt, matrix + 0x3Cu, 1.0f);
    const u32 index = word(rt, self + 0x1C0u);
    call(rt, ctx, 0x08B4E994u, {self, matrix, self + index * 80u + 0x80u});
}

// --- the graphics library's state block set up (no arguments) -----------------------------------------------------------
// 0x08B5A344: a flag at 0x08D6E0F0 and the block at 0x08D6CE00 cleared with 0x08B36018, the fields 0x400/density/table of the
// display list filled, a loop writing 0xC0..0xCB as bytes 0xC0..0xCB of 0x08D6D048 (an index table at +0x3C of each 4-byte
// word), four more commands (0x08C7AC00, 0x08C7AC40, 0x08C7AC30, 0x08C7AC50, 0x08C7AAB0, 0x08B84B5C, 0x08C7AC38), and two
// buffers handed to 0x08B56B70 and 0x08B59C90 with the second one's word +0x2C rounded up to a multiple of 0x2000.
void func_08B5A344(Runtime &rt, AllegrexContext &ctx) {
    constexpr u32 kState = 0x08D6CE00u;
    constexpr u32 kTable = 0x08D6D048u;
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    rt.memory().store8(0x08D6E0F0u, 1u);
    call(rt, ctx, 0x08C7A9B0u, {0u, 0x4000u});
    call(rt, ctx, 0x08B36018u, {kState, 0u, 0x12F0u});
    const float density = float_at(rt, 0x08CB620Cu);
    put(rt, kState + 0x70u, 0x20u);
    put(rt, kState + 0xC0u, 4u);
    put_float(rt, kState + 0x13Cu, density);
    put(rt, kState + 0x78u, 2u);
    put(rt, kState + 0xF8u, 1u);
    put(rt, kState + 0xFCu, 0u);
    put(rt, kState + 0x64u, 0u);
    put_float(rt, kState + 0xC4u, density);
    put(rt, kState + 0x138u, 0u);
    put(rt, kState + 0x74u, 0u);
    for (u32 index = 0xC0u; index != 0xCCu; ++index) put(rt, kTable + (index - 0xC0u) * 4u, index << 24);
    rt.memory().store8(kState + 0x25Du, static_cast<std::uint8_t>(-1));  // `addiu $v1, $zero, -1`
    rt.memory().store8(kState + 0x255u, 1u);
    put(rt, kState + 0x278u, 0u);
    put(rt, kState + 0x27Cu, 0u);
    put(rt, kState + 0x12E0u, 0u);
    put(rt, kState + 0x12E4u, 0u);
    call(rt, ctx, 0x08C7AC00u, {0u, 0x1E0u, 0x110u});
    call(rt, ctx, 0x08C7AC40u, {0x08C95980u, 0u, 0xFFFFFFFFu, 0u});
    put(rt, kState + 0x2Cu, call(rt, ctx, 0x08C7AC30u));
    put(rt, kState + 0x28u, call(rt, ctx, 0x08C7AC50u));
    put(rt, kState + 0x4Cu, 1u);
    call(rt, ctx, 0x08C7AAB0u, {kState, 0x08CB5774u, 0x300u, 0u, 0u});
    put(rt, kState + 0x20u, 0u);
    put(rt, kState + 0x24u, 0u);
    call(rt, ctx, 0x08B84B5Cu, {0u, 0x08B5A330u, kState});
    put(rt, frame.at(0u), 0x08B5A2F8u);
    put(rt, frame.at(4u), kState);
    put(rt, frame.at(8u), 0x08B5A314u);
    put(rt, frame.at(0xCu), kState);
    put(rt, kState + 0x30u, call(rt, ctx, 0x08C7AC38u, {frame.at(0u)}));
    call(rt, ctx, 0x08B56B70u, {0x08D6D0E0u});
    s32 span = static_cast<s32>(word(rt, kState + 0x2Cu));
    if (span < 0) span += 0x1FFFu;  // `slti` + `movn`
    call(rt, ctx, 0x08B59C90u, {0x08D6DE70u, word(rt, kState + 0x28u), static_cast<u32>(span >> 13) - 1u});
}

// --- one object's teardown (a0 = this) ----------------------------------------------------------------------------------
// 0x089AE3C4: the vtable word 0x08CBB598, two members released (0x08B35254 at +0xB0 and +0x58), the container at +0x178
// destroyed (0x08BF8B64) and the vector at +0x12C disposed of (0x08BF919C with its {begin, end} words, then 0x08A5BC54). Two
// more containers are released (0x08A5B9EC at +0xB8 and +0x68), six held objects at +0x54..+0x40 are released in turn
// (0x08A5B930), the vectors at +0x30, +0x24 and +0x18 are disposed of (0x08BF8B48 + 0x08A5BC54) and the base (0x08A780B0)
// plus the object's own block finally go.
void release_if_held(Runtime &rt, AllegrexContext &ctx, u32 slot) {
    const u32 held = word(rt, slot);
    if (held != 0u) call(rt, ctx, 0x08A5B930u, {held});
}
void dispose_vector(Runtime &rt, AllegrexContext &ctx, u32 vector, u32 scratch) {
    call(rt, ctx, 0x08BF8B48u, {word(rt, vector), word(rt, vector + 4u), scratch});
    const u32 begin = word(rt, vector);
    const s32 count = static_cast<s32>(word(rt, vector + 8u) - begin) >> 2;
    if (begin != 0u) call(rt, ctx, 0x08A5BC54u, {begin, static_cast<u32>(count) << 2, 4u});
}
void func_089AE3C4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    put(rt, self, 0x08CBB598u);
    call(rt, ctx, 0x08B35254u, {word(rt, self + 0xB0u)});
    call(rt, ctx, 0x08B35254u, {word(rt, self + 0x58u)});
    call(rt, ctx, 0x08BF8B64u, {self + 0x178u});
    const u32 vector = self + 0x12Cu;  // s0
    call(rt, ctx, 0x08BF919Cu, {word(rt, vector), word(rt, vector + 4u), frame.at(3u)});
    const u32 begin = word(rt, vector);
    const s32 count = static_cast<s32>(word(rt, vector + 8u) - begin) >> 2;
    if (begin != 0u) call(rt, ctx, 0x08A5BC54u, {begin, static_cast<u32>(count) << 2, 4u});
    call(rt, ctx, 0x08A5B9ECu, {self + 0xB8u});
    call(rt, ctx, 0x08A5B9ECu, {self + 0x68u});
    for (u32 offset = 0x54u; offset >= 0x40u; offset -= 4u) release_if_held(rt, ctx, self + offset);
    dispose_vector(rt, ctx, self + 0x30u, frame.at(2u));
    dispose_vector(rt, ctx, self + 0x24u, frame.at(1u));
    dispose_vector(rt, ctx, self + 0x18u, frame.at(0u));
    call(rt, ctx, 0x08A780B0u, {self});
    call(rt, ctx, 0x08A5BC54u, {self, 0x180u, 4u});
}

// --- the flag words of a pad object written from its two virtuals (a0 = this, a1 = object, a2 = a byte) ------------------
// 0x08916AC0 asks the object's virtual +0x5C for a status word: bit 0x400 or a zero byte gate the writes, and a second virtual
// +0x50 supplies the bits 1, 4, 8, 0x10, 0x20 that become the five flag values 2, 7, 3, 0xA, 0xB of the writer 0x0890B030;
// 0x0890AEAC then gets (byte != 0, bit 0) and the answer of 0x0890B014(state, 2) goes to this + 0x15.
void write_pad_flag(Runtime &rt, AllegrexContext &ctx, u32 state, u32 index, bool enabled, u32 flags, u32 mask) {
    call(rt, ctx, 0x0890B030u, {state, index, (enabled && (flags & mask) != 0u) ? 1u : 0u});
}
void func_08916AC0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 object = ctx.gpr[5];
    const u32 byte = ctx.gpr[6] & 0xFFu;
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    const u32 state = self + 4u;
    ctx.gpr[17] = state;  // s1
    ctx.gpr[21] = self;   // s5
    ctx.gpr[18] = byte;   // s2
    call(rt, ctx, 0x0890AEC4u, {state});
    const u32 status = call_indirect(rt, ctx, word(rt, word(rt, object) + 0x5Cu), {object});
    const bool enabled = (status & 0x400u) == 0u && byte != 0u;
    ctx.gpr[22] = status & 1u;  // s6, the delay slot of the test above
    ctx.gpr[19] = status;       // s0
    ctx.gpr[16] = enabled ? 1u : 0u;  // s3
    ctx.gpr[20] = enabled ? 1u : 0u;  // s4
    const u32 flags = call_indirect(rt, ctx, word(rt, word(rt, object) + 0x50u), {object});
    call(rt, ctx, 0x0890B030u, {state, 0u, (flags >> 1) & 1u});
    call(rt, ctx, 0x0890B030u, {state, 4u, enabled ? 1u : 0u});
    write_pad_flag(rt, ctx, state, 2u, enabled, flags, 1u);
    write_pad_flag(rt, ctx, state, 7u, enabled, flags, 4u);
    write_pad_flag(rt, ctx, state, 3u, enabled, flags, 8u);
    write_pad_flag(rt, ctx, state, 0xAu, enabled, flags, 0x10u);
    write_pad_flag(rt, ctx, state, 0xBu, enabled, flags, 0x20u);
    call(rt, ctx, 0x0890AEACu, {state, enabled ? 1u : 0u, status & 1u});
    rt.memory().store8(self + 0x15u, call(rt, ctx, 0x0890B014u, {state, 2u}));
}
// --- a placement object rebuilt in place (a0 = this) ----------------------------------------------------------------------
// 0x08A4037C asks 0x08B85EB8 for the item named 0x08CADA50 and calls the virtual at word(vptr + 0x108) with it; the block that
// came back is released (0x08B60804 + 0x08B8542C, skipping 0x08D74FD0), a replacement from 0x08A76A94 takes the place of the
// object held at +0x38 (0x08A5BA08 / 0x08A5B930) and eight handlers are registered on it.
void func_08A4037C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    const u32 handler = word(rt, word(rt, self) + 0x108u);
    call(rt, ctx, 0x08B85EB8u, {frame.at(4u), 0x08CADA50u, frame.at(1u)});
    call_indirect(rt, ctx, handler, {self, word(rt, frame.at(4u))});
    const u32 block = word(rt, frame.at(4u));
    const u32 object = block - 0xCu;
    if (object != 0x08D74FD0u && call(rt, ctx, 0x08B60804u, {block - 4u, 0xFFFFFFFFu}) <= 0) {
        call(rt, ctx, 0x08B8542Cu, {object, frame.at(0u)});
    }
    const u32 replacement = call(rt, ctx, 0x08A76A94u);
    if (replacement != 0u) call(rt, ctx, 0x08A5BA08u, {replacement});
    const u32 held = word(rt, self + 0x38u);
    if (held != 0u) call(rt, ctx, 0x08A5B930u, {held});
    put(rt, self + 0x38u, replacement);
    call(rt, ctx, 0x08B8850Cu, {self, 0x08CF6F14u, 0x08A40294u});
    call(rt, ctx, 0x08B87108u, {self, 0x08CADA94u, 0x08A3F7ACu});
    call(rt, ctx, 0x08B878E8u, {self, 0x08CADA9Cu, 0x08A3FC30u, 0x08A40A54u});
    call(rt, ctx, 0x08B8D51Cu, {self, 0x08CADAA8u, 0x08A3FCB0u, 0u});
    call(rt, ctx, 0x08B8D51Cu, {self, 0x08CADAB0u, 0x08A3FD58u, 0u});
    call(rt, ctx, 0x08B8D51Cu, {self, 0x08CADAB8u, 0x08A3FE08u, 0u});
    call(rt, ctx, 0x08B86FB0u, {self, 0x08CADAC0u, 0x08A3F6C4u});
    call(rt, ctx, 0x08B86FB0u, {self, 0x08CADAD8u, 0x08A3F258u});
}

// --- a curved value and its scale from a table entry (a0 = this, a1 = key, a2, a3 = two output floats) ---------------------
// 0x0893B160 looks the entry up (0x08955300), takes the float at table[0x328 + 4 * (byte at +0x10C)] and interpolates the word at
// +0x34..+0x38 of 0x08CF2990, then builds a scale from the entry's |+0x34| (relative to 100, squared, times +0x3C) interpolated
// over +0x44..+0x4C above +0x40 and capped at +0x50. A byte at +0x10C of zero gives 0x08CF29D8 and 0 to the two outputs.
void func_0893B160(Runtime &rt, AllegrexContext &ctx) {
    constexpr u32 kCurve = 0x08CF2990u;
    constexpr u32 kFlat = 0x08CF29D8u;
    const float kLowerEdge = std::bit_cast<float>(0x3E0A3D71u);
    const float kUpperEdge = std::bit_cast<float>(0x3EB5C290u);
    const float kEdgeSpan = std::bit_cast<float>(0x3E6147AFu);
    const float kHundred = std::bit_cast<float>(0x42C80000u);
    const u32 self = ctx.gpr[4];
    const u32 out_scale = ctx.gpr[6];
    const u32 out_value = ctx.gpr[7];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    const u32 table = word(rt, self + 0xCu);
    const u32 entry = call(rt, ctx, 0x08955300u, {self});
    const u32 mode = rt.memory().load8(entry + 0x10Cu);
    if (mode == 0u) {
        put_float(rt, out_value, float_at(rt, kFlat));
        put(rt, out_scale, 0u);
        return;
    }
    ctx.gpr[2] = mode << 2;  // `sll v0, mode, 2` (v0 is not the result of this call)
    const float input = float_at(rt, table + mode * 4u + 0x328u);
    const float low = float_at(rt, kCurve + 0x34u);
    const float high = float_at(rt, kCurve + 0x38u);
    float axis;
    if (input < kLowerEdge) {
        axis = low;
    } else if (kUpperEdge < input) {
        axis = high;
    } else {
        axis = low + ((input - kLowerEdge) * (high - low)) / kEdgeSpan;
    }
    const float distance = std::fabs(float_at(rt, entry + 0x34u));
    const float normalized = distance / kHundred;
    const float gain = axis * ((normalized * normalized) * float_at(rt, kCurve + 0x3Cu));
    const float upper = float_at(rt, kCurve + 0x40u);
    float value;
    if (distance < upper) {
        value = float_at(rt, kCurve + 0x48u);
    } else {
        const float lower = float_at(rt, kCurve + 0x44u);
        if (lower < distance) {
            value = float_at(rt, kCurve + 0x4Cu);
        } else {
            const float start = float_at(rt, kCurve + 0x48u);
            const float end = float_at(rt, kCurve + 0x4Cu);
            value = start + ((distance - upper) * (end - start)) / (lower - upper);
        }
    }
    const float cap = float_at(rt, kCurve + 0x50u);
    float scale = gain;
    if (cap < gain) scale = cap;
    put_float(rt, out_scale, scale);
    put_float(rt, out_value, value);
}

} // namespace
} // namespace new_batch_23
} // namespace gt::game

#define NEW23(address, name, kind) GT_DECOMP(address, gt::game::new_batch_23::name, gt::ReturnKind::kind)
NEW23(0x08BD48FCu, func_08BD48FC, Word);
NEW23(0x08C70FE8u, func_08C70FE8, Word);
NEW23(0x08C71D00u, func_08C71D00, Word);
NEW23(0x08C71544u, func_08C71544, Word);
NEW23(0x08C717C8u, func_08C717C8, Word);
NEW23(0x08C71A7Cu, func_08C71A7C, Word);
NEW23(0x08C77004u, func_08C77004, Word);
NEW23(0x08C771D4u, func_08C771D4, Word);
NEW23(0x08C548C4u, func_08C548C4, Word);
NEW23(0x08AD17C4u, func_08AD17C4, Word);
NEW23(0x08B5A344u, func_08B5A344, Word);
NEW23(0x089AE3C4u, func_089AE3C4, Word);
NEW23(0x08916AC0u, func_08916AC0, Word);
NEW23(0x08A4037Cu, func_08A4037C, Word);
NEW23(0x0893B160u, func_0893B160, Word);
