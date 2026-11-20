// New batch 12: 80 small functions (52-64 bytes) written by hand from `tools.disasm`, smallest first, for a draft that the
// differential gate has not seen yet (the caller verifies). Most are one of four shapes: the Adhoc "as class" forwarder
// (copy the shared empty holder into `out`, then jump to the class's checked cast: 24 of them), a destructor (store a vtable
// word, release one or two member holders, then jump to the base's destructor), a virtual/indirect one-line forwarder, or a
// small setter/getter with a branch or a loop.
// Delay slots are read as "before the branch / call". The callee-saved registers the original keeps values in (only s0 and s1
// appear) are set where the asm sets them, because a callee may read them, and put back at the end; `v0` (or `f0` for the
// float ones, `v1` for the one 64-bit result) ends as the asm leaves it on every exit path. Calls go through `call_set` /
// `call_keep`, which write the argument registers like the original sets them before the `jal` (so `tools.arg_reads` can check
// them). The `GuestFrame` sits on the signature line on purpose: `tools.frame_audit` reads a body's declared frame only from
// there, and a frame on its own line would make the audit skip the function silently. No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_12 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// The registers a callee's results stay in after a call, as the original's `jal` leaves them.
void take(AllegrexContext &ctx, const GuestResult &result) {
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}

// A guest call with a0-a3 / t0-t3 set like the original sets them before the `jal`; v0 / v1 / f0 afterwards are the
// callee's, and a callee that sets no v0 hands the caller's previous one on (call_guest starts it with the caller's).
GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    take(ctx, result);
    return result;
}
u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    u32 reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, target, arguments).v0;
}
// `lw v0, something; jalr target`: the callee starts with the v0 the caller already held (not the target).
u32 call_keeping_v0(Runtime &rt, AllegrexContext &ctx, u32 v0_before, u32 target, std::initializer_list<u32> arguments) {
    ctx.gpr[2] = v0_before;
    return call_set(rt, ctx, target, arguments);
}

u32 byte_at(Runtime &rt, u32 address) { return rt.memory().load8(address); }
s32 signed_byte_at(Runtime &rt, u32 address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }
void put_byte(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void put_half(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
// The Allegrex `wsbh`: the bytes of each halfword swapped.
u32 bswap16(u32 value) { return ((value & 0xFFu) << 8) | ((value >> 8) & 0xFFu); }
u32 sign_extend_byte(u32 value) { return static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(value))); }
u32 sign_extend_half(u32 value) { return static_cast<u32>(static_cast<s32>(static_cast<std::int16_t>(value))); }
// A constant record size (index * 384) built by the original as `(i << 9) - (i << 7)`.
u32 record_offset(Runtime &rt, u32 holder) { return static_cast<u32>(signed_byte_at(rt, holder + 8u)) * 384u; }

// s0-s1 are callee-saved: the body sets them like the original and the caller's values come back.
class SavedRegisters {
public:
    explicit SavedRegisters(AllegrexContext &ctx) : ctx_(ctx), s0_(ctx.gpr[16]), s1_(ctx.gpr[17]) {}
    ~SavedRegisters() {
        ctx_.gpr[16] = s0_;
        ctx_.gpr[17] = s1_;
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    AllegrexContext &ctx_;
    u32 s0_;
    u32 s1_;
};

// --- 52 bytes ---

// 0x08C766B4 (a0 = an output word, a1-a3 = arguments): calls 0x08C765E8 and stores its result at *a0.
void func_08C766B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(ctx);
    const u32 out = ctx.gpr[4];
    ctx.gpr[16] = out;
    const u32 result = call_set(rt, ctx, 0x08C765E8u, {ctx.gpr[5], ctx.gpr[6], ctx.gpr[7]});
    put(rt, out, result);
}

// --- 56 bytes ---

// 0x088910E4 (a0 = this): 0x088DAD00(this->0x8C, 0), then the object at this->0x94's virtual +0x20 (a tail call).
void func_088910E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    {
        SavedRegisters saved(ctx);
        ctx.gpr[16] = self;
        call_set(rt, ctx, 0x088DAD00u, {word(rt, self + 0x8Cu), 0u});
    }
    const u32 object = word(rt, self + 0x94u);
    const u32 vtable = word(rt, object);
    ctx.gpr[2] = vtable;
    tail_call(ctx, word(rt, vtable + 0x20u), {object});
}

// 0x08899E50 (a0 = an output float, a1 = a holder): the object at holder->0xB04's virtual +0x64 (this + 0) into the output.
void func_08899E50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(ctx);
    const u32 out = ctx.gpr[4];
    ctx.gpr[16] = out;
    const u32 holder = word(rt, ctx.gpr[5] + 0xB04u);
    const u32 object = word(rt, holder);
    call_indirect(rt, ctx, word(rt, object + 0x64u), {holder, object});
    put_float(rt, out, ctx.fpr[0]);
}

// 0x088DC1B8 (a0 = this, a1 = a byte value): stores the value at this + 0x30 (unchanged when it is the same) and its low
// byte at this->0x130 + 1; when this->0x640 holds something, 0x088E649C(that, the byte at this + 0x30) finishes. v0 = the
// pointer at this + 0x130 on both paths.
void func_088DC1B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    const u32 current = word(rt, self + 0x30u);
    if (current != value) put(rt, self + 0x30u, value);
    const u32 status = word(rt, self + 0x130u);
    put_byte(rt, status + 1u, value);
    const u32 pending = word(rt, self + 0x640u);
    ctx.gpr[2] = status;
    if (pending != 0u) tail_call(ctx, 0x088E649Cu, {pending, byte_at(rt, self + 0x30u)});
}

// 0x08927A88 (a0 = this, a1 = source): 0x08927A0C(a1, this) copies the source in, then 0x08B507B8(this, ...) finishes. That
// second call sets no a1-a3 of its own: it runs with the a1 the first callee left and the caller's a2/a3 (0x08B507B8 is a
// jump into a matrix copy that writes a2/a3 before reading them). v0 = this.
void func_08927A88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(ctx);
    const u32 self = ctx.gpr[4];
    ctx.gpr[16] = self;
    const GuestResult first = call_keep(rt, ctx, 0x08927A0Cu, {ctx.gpr[5], self});
    call_set(rt, ctx, 0x08B507B8u, {self, first.a1, ctx.gpr[6], ctx.gpr[7]});
    ctx.gpr[2] = self;
}

// 0x08AD22D4 (a0 = this, a1 = a value): 0x08AD2060(this), then 0x08C3DDB8(this + 0x2A4, value) finishes.
void func_08AD22D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    {
        SavedRegisters saved(ctx);
        ctx.gpr[16] = self;
        ctx.gpr[17] = value;
        call_set(rt, ctx, 0x08AD2060u, {self, value});
    }
    tail_call(ctx, 0x08C3DDB8u, {self + 0x2A4u, value});
}

// 0x08B0DD50 (a0 = this, a1, a2): the object at this + 0x38's virtual +0x74, called with (object, this, a1, a2).
void func_08B0DD50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    const u32 object = word(rt, self + 0x38u);
    call_indirect(rt, ctx, word(rt, word(rt, object) + 0x74u), {object, self, first, second});
}

// 0x08B19FF4 (a0 = this): when the handler word at this + 0x18 is set it runs with this->0x1C; then 0x08B19FEC(this + 0x10)
// finishes. v0 = this + 0x18 when no handler ran, else the handler's v0.
void func_08B19FF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    {
        SavedRegisters saved(ctx);
        ctx.gpr[16] = self;
        const u32 slot = self + 0x18u;
        const u32 handler = word(rt, slot);
        ctx.gpr[2] = slot;
        if (handler != 0u) call_keeping_v0(rt, ctx, slot, handler, {word(rt, slot + 4u)});
    }
    tail_call(ctx, 0x08B19FECu, {self + 0x10u});
}

// 0x08B49688 (a0 = a category): 2 for 14 and above, 0 for the categories 0, 10, 11 and 12 (the mask 0x1C01), 1 otherwise.
void func_08B49688(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    const u32 category = ctx.gpr[4];
    u32 result = 2u;
    if (category < 14u) result = (category < 13u && ((1u << category) & 0x1C01u) != 0u) ? 0u : 1u;
    ctx.gpr[2] = result;
}

// 0x08B93AE8 (a0 = a tree header, a1 = a pointer to the key): the lower-bound walk of a binary tree (the key at node + 0x10,
// children at +8 and +0xC) and the address of the first node not below the key (a0 + 4 when the tree is empty). v0 = that.
void func_08B93AE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    u32 position = self + 4u;
    u32 node = word(rt, self + 8u);
    if (node != 0u) {
        const s32 key = static_cast<s32>(word(rt, ctx.gpr[5]));
        while (node != 0u) {
            if (static_cast<s32>(word(rt, node + 0x10u)) < key) {
                node = word(rt, node + 0xCu);
            } else {
                position = node;
                node = word(rt, node + 8u);
            }
        }
    }
    ctx.gpr[2] = position;
}

// --- 60 bytes ---

// 0x088757B0 (a0 = an object, a1 = a value): examines 0x088756F0(a0, 0x08C9FF90, a1); a nonzero answer goes to
// 0x0886A9A0(answer, 2) as a tail call, zero is returned.
void func_088757B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    u32 found;
    {
        found = call_set(rt, ctx, 0x088756F0u, {ctx.gpr[4], 0x08C9FF90u, ctx.gpr[5]});
    }
    if (found != 0u) tail_call(ctx, 0x0886A9A0u, {found, 2u});
}

// 0x0887BC90 (a0 = this, a1 = a value): 0x0887BB3C(this->4), then 0x0887B768(this->4, value) finishes with the member
// re-read after the first call.
void func_0887BC90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    u32 member;
    {
        SavedRegisters saved(ctx);
        ctx.gpr[16] = self;
        ctx.gpr[17] = value;
        call_set(rt, ctx, 0x0887BB3Cu, {word(rt, self + 4u)});
        member = word(rt, self + 4u);
    }
    tail_call(ctx, 0x0887B768u, {member, value});
}

// 0x0889A054: as 0x0889968C with the virtual +0x74 called with 0.
void func_0889A054(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(ctx);
    const u32 out = ctx.gpr[4];
    ctx.gpr[16] = out;
    const u32 holder = word(rt, ctx.gpr[5] + 0xB04u);
    const u32 object = word(rt, holder);
    call_indirect(rt, ctx, word(rt, object + 0x74u), {holder, object, 0u});
    put_float(rt, out, ctx.fpr[0]);
}

// 0x088A6364 (a0 = a pointer to a holder, a1 = a byte): records the byte in the holder's current record (holder->8 signed
// byte index * 384) at +0x15A, then 0x0892ED70(record + 0x38) finishes. v0 = the index << 7 the original leaves.
void func_088A6364(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 holder = word(rt, self);
    put_byte(rt, holder + record_offset(rt, holder) + 0x15Au, ctx.gpr[5]);
    const u32 next = word(rt, self);
    const s32 next_index = signed_byte_at(rt, next + 8u);
    ctx.gpr[2] = static_cast<u32>(next_index) << 7;
    tail_call(ctx, 0x0892ED70u, {next + static_cast<u32>(next_index * 384) + 0x38u});
}

// 0x088D5A34 (a0 = a pointer to a holder, a1 = a value): stores the value's halfword bytes swapped at holder + 0xC and, at
// holder + 8, 0x0200 when the value is zero and 0 otherwise (`wsbh` of the halfword 2 is 0x0200). v0 = the holder.
void func_088D5A34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    const u32 holder = word(rt, self);
    put_half(rt, holder + 0xCu, bswap16(value));
    const u32 reloaded = word(rt, self);
    ctx.gpr[2] = reloaded;
    put_half(rt, reloaded + 8u, value != 0u ? 0u : bswap16(2u));
}

// 0x088DC784 (a0 = this, a1 = a bit count): sets bits 0..count-1 of the byte at this->0x130 + 0xE (an `sllv`, so only the low
// five bits of the count index a bit). v0 = the byte it wrote last.
void func_088DC784(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const s32 count = static_cast<s32>(ctx.gpr[5]);
    if (count < 0) return;
    const u32 self = ctx.gpr[4];
    u32 result = ctx.gpr[2];
    for (s32 bit = 0; bit <= count; ++bit) {
        const u32 holder = word(rt, self + 0x130u);
        const u32 mask = (1u << (static_cast<u32>(bit) & 31u)) | byte_at(rt, holder + 0xEu);
        result = mask;
        put_byte(rt, holder + 0xEu, mask);
    }
    ctx.gpr[2] = result;
}

// 0x08918B48 (a0 = this): this's virtual +0x44, then the byte at this + 0x3FC set to 1 and the words at +0x404 and +0x40C
// cleared. v0 = 1.
void func_08918B48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(ctx);
    const u32 self = ctx.gpr[4];
    ctx.gpr[16] = self;
    const u32 vtable = word(rt, self);
    call_keeping_v0(rt, ctx, vtable, word(rt, vtable + 0x44u), {self});
    put_byte(rt, self + 0x3FCu, 1u);
    put(rt, self + 0x404u, 0u);
    put(rt, self + 0x40Cu, 0u);
    ctx.gpr[2] = 1u;
}

// 0x0891C724 (a0 = this): 0x08926C2C(this), then this + 0x224 = -1, +0x22C (byte) = 0 and +0x218, +0x21C, +0x228 = 0.
void func_0891C724(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(ctx);
    const u32 self = ctx.gpr[4];
    ctx.gpr[16] = self;
    call_set(rt, ctx, 0x08926C2Cu, {self});
    put(rt, self + 0x224u, 0xFFFFFFFFu);
    put_byte(rt, self + 0x22Cu, 0u);
    put(rt, self + 0x218u, 0u);
    put(rt, self + 0x21Cu, 0u);
    put(rt, self + 0x228u, 0u);
    ctx.gpr[2] = 0xFFFFFFFFu;
}

// 0x08AD083C: 0x08B32228(0x08D69170, 0), 0x08ABDC6C(...) and 0x08B319B8(0x08D69170, its result) finishing. The `lui s0, 0x9`
// that both 0x08D69170 arguments come from holds the .data base (0x08D69170 + 0x3390); the middle call sets no a0, so it runs
// with the caller's.
void func_08AD083C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    constexpr u32 kTable = 0x08D69170u;
    SavedRegisters saved(ctx);
    ctx.gpr[16] = kTable + 0x3390u;
    call_set(rt, ctx, 0x08B32228u, {kTable, 0u});
    const u32 found = call_set(rt, ctx, 0x08ABDC6Cu, {ctx.gpr[4]});
    tail_call(ctx, 0x08B319B8u, {kTable, found});
}

// 0x08B2E250: a two-word local {0x08B2E230, 0} handed to 0x08B1B96C(&local, 0, 1); its result is stored at 0x08D68D84 and
// returned.
void func_08B2E250(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 local = frame.at(0u);
    put(rt, local, 0x08B2E230u);
    put(rt, local + 4u, 0u);
    const u32 handle = call_set(rt, ctx, 0x08B1B96Cu, {local, 0u, 1u});
    put(rt, 0x08D68D84u, handle);
}

// 0x08B388FC (a0 = a pointer to a string block): steps the end pointer at +8 down over continuation bytes (a negative signed
// byte with its top two bits at 0, i.e. 0x80-0xBF) until it reaches the start at +4 or a non-continuation byte. v0 = that byte
// sign-extended, or the start when the loop ran out.
void func_08B388FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 limit = word(rt, self + 4u);
    u32 result = word(rt, self + 8u);
    for (;;) {
        const u32 current = word(rt, self + 8u);
        if (current == limit) {
            result = current;
            break;
        }
        const u32 previous = current - 1u;
        put(rt, self + 8u, previous);
        const u32 byte = byte_at(rt, previous);
        const u32 sign_extended = sign_extend_byte(byte);
        result = sign_extended;
        if (static_cast<s32>(sign_extended) >= 0 || (byte & 0xC0u) != 0u) break;
    }
    ctx.gpr[2] = result;
}

// 0x08B41F70 (a0 = this, a1 = a 16-bit value): 0x08B41E58(this, (s16)value), then 0x08B3F4D0(this + 0x8DC, value without
// sign extension) finishing.
void func_08B41F70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 raw = ctx.gpr[5] & 0xFFFFu;
    {
        SavedRegisters saved(ctx);
        ctx.gpr[16] = self;
        ctx.gpr[17] = raw;
        call_set(rt, ctx, 0x08B41E58u, {self, sign_extend_half(raw)});
    }
    tail_call(ctx, 0x08B3F4D0u, {self + 0x8DCu, raw});
}

// 0x08B56384 (a0 = this): ORs the pairs at +0xD18/+0xD20 and +0xD1C/+0xD24 back into +0xD18/+0xD1C, takes the record at
// this + 0xD00 + (this->0xD10) * 4 and hands it to 0x08B57F2C(record, record). v0 = the +0xD18 word.
void func_08B56384(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 index = word(rt, self + 0xD10u);
    const u32 first = word(rt, self + 0xD18u) | word(rt, self + 0xD20u);
    const u32 second = word(rt, self + 0xD1Cu) | word(rt, self + 0xD24u);
    const u32 record = word(rt, self + (index << 2) + 0xD00u);
    put(rt, self + 0xD18u, first);
    put(rt, self + 0xD1Cu, second);
    ctx.gpr[2] = first;
    tail_call(ctx, 0x08B57F2Cu, {record, record});
}

// 0x08B5C018 (a0 = first, a1 = other, a2 = last): splices the (a0, a2) chain into the list next to a1 (the +0/+4 links),
// doing nothing when the chain is empty. v0 = the other node's next link on the splice path.
void func_08B5C018(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 first = ctx.gpr[4];
    const u32 other = ctx.gpr[5];
    const u32 last = ctx.gpr[6];
    if (first == last) return;
    const u32 last_next = word(rt, last + 4u);
    const u32 other_next = word(rt, other + 4u);
    const u32 first_next = word(rt, first + 4u);
    put(rt, first + 4u, last_next);
    put(rt, last_next, first);
    put(rt, last + 4u, other_next);
    put(rt, other_next, last);
    put(rt, other + 4u, first_next);
    put(rt, first_next, other);
    ctx.gpr[2] = other_next;
}

// --- 64 bytes ---

// The Adhoc "as class" forwarder (a0 = the output holder, a1 = the source holder): the copy `holder_copy` resets the output
// to the shared empty holder 0x08CF6ED8, then the class's checked cast(out, source) fills it in as a tail call. The instances
// differ only in the copy and the cast (src/game/adhoc/as_class.cpp carries the same pairs in its table).

// 0x0880AD24: copy 0x08969190, cast 0x0880AC18.
void func_0880AD24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 out = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    {
        SavedRegisters saved(ctx);
        ctx.gpr[16] = source;
        ctx.gpr[17] = out;
        call_set(rt, ctx, 0x08969190u, {out, 0x08CF6ED8u});
    }
    tail_call(ctx, 0x0880AC18u, {out, source});
}

// 0x088354AC: copy 0x08A0FD20, cast 0x08835334.
void func_088354AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 out = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    {
        SavedRegisters saved(ctx);
        ctx.gpr[16] = source;
        ctx.gpr[17] = out;
        call_set(rt, ctx, 0x08A0FD20u, {out, 0x08CF6ED8u});
    }
    tail_call(ctx, 0x08835334u, {out, source});
}

// 0x088418B4: copy 0x08A76AB4, cast 0x08841808.
void func_088418B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 out = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    {
        SavedRegisters saved(ctx);
        ctx.gpr[16] = source;
        ctx.gpr[17] = out;
        call_set(rt, ctx, 0x08A76AB4u, {out, 0x08CF6ED8u});
    }
    tail_call(ctx, 0x08841808u, {out, source});
}

// 0x08858830: copy 0x08A76AB4, cast 0x08858784.
void func_08858830(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 out = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    {
        SavedRegisters saved(ctx);
        ctx.gpr[16] = source;
        ctx.gpr[17] = out;
        call_set(rt, ctx, 0x08A76AB4u, {out, 0x08CF6ED8u});
    }
    tail_call(ctx, 0x08858784u, {out, source});
}

// 0x089C2238: copy 0x08A0FD20, cast 0x089C218C.
void func_089C2238(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 out = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    {
        SavedRegisters saved(ctx);
        ctx.gpr[16] = source;
        ctx.gpr[17] = out;
        call_set(rt, ctx, 0x08A0FD20u, {out, 0x08CF6ED8u});
    }
    tail_call(ctx, 0x089C218Cu, {out, source});
}

// 0x089D57AC: copy 0x089D5E8C, cast 0x089D55E4.
void func_089D57AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 out = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    {
        SavedRegisters saved(ctx);
        ctx.gpr[16] = source;
        ctx.gpr[17] = out;
        call_set(rt, ctx, 0x089D5E8Cu, {out, 0x08CF6ED8u});
    }
    tail_call(ctx, 0x089D55E4u, {out, source});
}

// 0x08A05104: copy 0x089D8A60, cast 0x08A05058.
void func_08A05104(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 out = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    {
        SavedRegisters saved(ctx);
        ctx.gpr[16] = source;
        ctx.gpr[17] = out;
        call_set(rt, ctx, 0x089D8A60u, {out, 0x08CF6ED8u});
    }
    tail_call(ctx, 0x08A05058u, {out, source});
}

// 0x08A12150: copy 0x08A76AB4, cast 0x08A120A4.
void func_08A12150(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 out = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    {
        SavedRegisters saved(ctx);
        ctx.gpr[16] = source;
        ctx.gpr[17] = out;
        call_set(rt, ctx, 0x08A76AB4u, {out, 0x08CF6ED8u});
    }
    tail_call(ctx, 0x08A120A4u, {out, source});
}

// 0x08A21818: copy 0x08A76AB4, cast 0x08A2176C.
void func_08A21818(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 out = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    {
        SavedRegisters saved(ctx);
        ctx.gpr[16] = source;
        ctx.gpr[17] = out;
        call_set(rt, ctx, 0x08A76AB4u, {out, 0x08CF6ED8u});
    }
    tail_call(ctx, 0x08A2176Cu, {out, source});
}

// 0x08A2D338: copy 0x08A76AB4, cast 0x08A2D22C.
void func_08A2D338(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 out = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    {
        SavedRegisters saved(ctx);
        ctx.gpr[16] = source;
        ctx.gpr[17] = out;
        call_set(rt, ctx, 0x08A76AB4u, {out, 0x08CF6ED8u});
    }
    tail_call(ctx, 0x08A2D22Cu, {out, source});
}

// 0x08A33604: copy 0x08A76AB4, cast 0x08A33558.
void func_08A33604(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 out = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    {
        SavedRegisters saved(ctx);
        ctx.gpr[16] = source;
        ctx.gpr[17] = out;
        call_set(rt, ctx, 0x08A76AB4u, {out, 0x08CF6ED8u});
    }
    tail_call(ctx, 0x08A33558u, {out, source});
}

// 0x08A3D8B0: copy 0x08A76AB4, cast 0x08A3D804.
void func_08A3D8B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 out = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    {
        SavedRegisters saved(ctx);
        ctx.gpr[16] = source;
        ctx.gpr[17] = out;
        call_set(rt, ctx, 0x08A76AB4u, {out, 0x08CF6ED8u});
    }
    tail_call(ctx, 0x08A3D804u, {out, source});
}

// 0x08A3F684: copy 0x08A76AB4, cast 0x08A3F1AC.
void func_08A3F684(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 out = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    {
        SavedRegisters saved(ctx);
        ctx.gpr[16] = source;
        ctx.gpr[17] = out;
        call_set(rt, ctx, 0x08A76AB4u, {out, 0x08CF6ED8u});
    }
    tail_call(ctx, 0x08A3F1ACu, {out, source});
}

// 0x08A3F76C: copy 0x08A76AB4, cast 0x08A3F320.
void func_08A3F76C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 out = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    {
        SavedRegisters saved(ctx);
        ctx.gpr[16] = source;
        ctx.gpr[17] = out;
        call_set(rt, ctx, 0x08A76AB4u, {out, 0x08CF6ED8u});
    }
    tail_call(ctx, 0x08A3F320u, {out, source});
}

// 0x08A5DCF8: copy 0x08A76AB4, cast 0x08A5DC48.
void func_08A5DCF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 out = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    {
        SavedRegisters saved(ctx);
        ctx.gpr[16] = source;
        ctx.gpr[17] = out;
        call_set(rt, ctx, 0x08A76AB4u, {out, 0x08CF6ED8u});
    }
    tail_call(ctx, 0x08A5DC48u, {out, source});
}

// 0x08A68F54: copy 0x08A7026C, cast 0x08A68EA8.
void func_08A68F54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 out = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    {
        SavedRegisters saved(ctx);
        ctx.gpr[16] = source;
        ctx.gpr[17] = out;
        call_set(rt, ctx, 0x08A7026Cu, {out, 0x08CF6ED8u});
    }
    tail_call(ctx, 0x08A68EA8u, {out, source});
}

// 0x08A89AA0: copy 0x08A76AB4, cast 0x08A899F4.
void func_08A89AA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 out = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    {
        SavedRegisters saved(ctx);
        ctx.gpr[16] = source;
        ctx.gpr[17] = out;
        call_set(rt, ctx, 0x08A76AB4u, {out, 0x08CF6ED8u});
    }
    tail_call(ctx, 0x08A899F4u, {out, source});
}

// 0x08A8AE70: copy 0x08A76AB4, cast 0x08A8ADC0.
void func_08A8AE70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 out = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    {
        SavedRegisters saved(ctx);
        ctx.gpr[16] = source;
        ctx.gpr[17] = out;
        call_set(rt, ctx, 0x08A76AB4u, {out, 0x08CF6ED8u});
    }
    tail_call(ctx, 0x08A8ADC0u, {out, source});
}

// 0x08AB2D80: copy 0x08A70EC8, cast 0x08AB2CD4.
void func_08AB2D80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 out = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    {
        SavedRegisters saved(ctx);
        ctx.gpr[16] = source;
        ctx.gpr[17] = out;
        call_set(rt, ctx, 0x08A70EC8u, {out, 0x08CF6ED8u});
    }
    tail_call(ctx, 0x08AB2CD4u, {out, source});
}

// 0x08AB3FC0: copy 0x08A76AB4, cast 0x08AB3EB4.
void func_08AB3FC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 out = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    {
        SavedRegisters saved(ctx);
        ctx.gpr[16] = source;
        ctx.gpr[17] = out;
        call_set(rt, ctx, 0x08A76AB4u, {out, 0x08CF6ED8u});
    }
    tail_call(ctx, 0x08AB3EB4u, {out, source});
}

// 0x08885B50 (a0 = this, f12 = a scale): 0x08885994(this->0xC, this->8), then 0x08884C80(this->0xC) with f12 = the scale
// finishing.
void func_08885B50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const float scale = ctx.fpr[12];
    const float saved_f20 = ctx.fpr[20];
    ctx.fpr[20] = scale;
    const u32 self = ctx.gpr[4];
    {
        SavedRegisters registers(ctx);
        ctx.gpr[16] = self;
        call_set(rt, ctx, 0x08885994u, {word(rt, self + 0xCu), word(rt, self + 8u)});
    }
    ctx.fpr[12] = scale;
    ctx.fpr[20] = saved_f20;
    tail_call(ctx, 0x08884C80u, {word(rt, self + 0xCu)});
}

// 0x088A6028 (a0 = a pointer to a holder): 1 when the current record (index * 384, as 0x088A6364 reads it) holds two -1
// words at +8 and +0xC, else 0.
void func_088A6028(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 holder = word(rt, ctx.gpr[4]);
    const u32 record = holder + record_offset(rt, holder) + 0x30u;
    u32 vacant = 0u;
    if (word(rt, record + 8u) == 0xFFFFFFFFu && word(rt, record + 0xCu) == 0xFFFFFFFFu) vacant = 1u;
    ctx.gpr[2] = vacant;
}

// 0x089BF170 (a0 = this): 0x08B344E0(this + 0x2B0), 0x08B3457C(this + 0x2B0). v0 = this + 0x264.
void func_089BF170(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(ctx);
    const u32 self = ctx.gpr[4];
    const u32 member = self + 0x2B0u;
    ctx.gpr[17] = self;
    ctx.gpr[16] = member;
    call_set(rt, ctx, 0x08B344E0u, {member});
    call_set(rt, ctx, 0x08B3457Cu, {member});
    ctx.gpr[2] = self + 0x264u;
}

// 0x08A8A690 (a0 = this): stores the vtable word 0x08CD3438, releases the holders at this + 0x1C and + 0x18 and
// 0x08A5D354(this) finishes.
void func_08A8A690(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    {
        SavedRegisters saved(ctx);
        ctx.gpr[16] = self;
        put(rt, self, 0x08CD3438u);
        call_set(rt, ctx, 0x08A5B9ECu, {self + 0x1Cu});
        call_set(rt, ctx, 0x08A5B9ECu, {self + 0x18u});
    }
    tail_call(ctx, 0x08A5D354u, {self});
}

} // namespace
} // namespace new_batch_12
} // namespace gt::game

#define NEW12(address, name, kind) GT_DECOMP(address, gt::game::new_batch_12::name, gt::ReturnKind::kind)
NEW12(0x08C766B4u, func_08C766B4, Word);
NEW12(0x088910E4u, func_088910E4, Word);
NEW12(0x08899E50u, func_08899E50, Word);
NEW12(0x088DC1B8u, func_088DC1B8, Word);
NEW12(0x08927A88u, func_08927A88, Word);
NEW12(0x08AD22D4u, func_08AD22D4, Word);
NEW12(0x08B0DD50u, func_08B0DD50, Word);
NEW12(0x08B19FF4u, func_08B19FF4, Word);
NEW12(0x08B49688u, func_08B49688, Word);
NEW12(0x08B93AE8u, func_08B93AE8, Word);
NEW12(0x088757B0u, func_088757B0, Word);
NEW12(0x0887BC90u, func_0887BC90, Word);
NEW12(0x0889A054u, func_0889A054, Word);
NEW12(0x088A6364u, func_088A6364, Word);
NEW12(0x088D5A34u, func_088D5A34, Word);
NEW12(0x088DC784u, func_088DC784, Word);
NEW12(0x08918B48u, func_08918B48, Word);
NEW12(0x0891C724u, func_0891C724, Word);
NEW12(0x08AD083Cu, func_08AD083C, Word);
NEW12(0x08B2E250u, func_08B2E250, Word);
NEW12(0x08B388FCu, func_08B388FC, Word);
NEW12(0x08B41F70u, func_08B41F70, Word);
NEW12(0x08B56384u, func_08B56384, Word);
NEW12(0x08B5C018u, func_08B5C018, Word);
NEW12(0x0880AD24u, func_0880AD24, Void);
NEW12(0x088354ACu, func_088354AC, Void);
NEW12(0x088418B4u, func_088418B4, Void);
NEW12(0x08858830u, func_08858830, Void);
NEW12(0x08885B50u, func_08885B50, Word);
NEW12(0x088A6028u, func_088A6028, Word);
NEW12(0x089BF170u, func_089BF170, Word);
NEW12(0x089C2238u, func_089C2238, Void);
NEW12(0x089D57ACu, func_089D57AC, Void);
NEW12(0x08A05104u, func_08A05104, Void);
NEW12(0x08A12150u, func_08A12150, Void);
NEW12(0x08A21818u, func_08A21818, Void);
NEW12(0x08A2D338u, func_08A2D338, Void);
NEW12(0x08A33604u, func_08A33604, Void);
NEW12(0x08A3D8B0u, func_08A3D8B0, Void);
NEW12(0x08A3F684u, func_08A3F684, Void);
NEW12(0x08A3F76Cu, func_08A3F76C, Void);
NEW12(0x08A5DCF8u, func_08A5DCF8, Void);
NEW12(0x08A68F54u, func_08A68F54, Void);
NEW12(0x08A89AA0u, func_08A89AA0, Void);
NEW12(0x08A8A690u, func_08A8A690, Word);
NEW12(0x08A8AE70u, func_08A8AE70, Void);
NEW12(0x08AB2D80u, func_08AB2D80, Void);
NEW12(0x08AB3FC0u, func_08AB3FC0, Void);
