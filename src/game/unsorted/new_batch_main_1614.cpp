// New batch main-1614 (main-session batch, unverified draft). Structured C from tools.disasm, one function per address.
// No earlier draft exists for these addresses (drafts/fix_info.txt has no entry for them), so each is written from the asm.
// Frames: GuestFrame sizes are the prologue's addiu sp. Calls and tail calls take the argument registers the asm sets.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1614 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;

u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// jal first(this, 0x08CF6ED8) inside a 16-byte frame, then j tail(this, a1): the shape of the one-argument forwarders.
void call_then_tail(Runtime &rt, AllegrexContext &ctx, u32 first, u32 tail) {
    const u32 self = ctx.gpr[4];
    const u32 arg = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        call(rt, ctx, first, {self, 0x08CF6ED8u});
    }
    tail_call(ctx, tail, {self, arg});
}

// jal 0x08A76ACC(this, a1), then the byte or halfword field at +0x14 of this, then the vtable at +0; v0 = the vtable.
enum class Field { Half, Byte };
void call_then_stamp(Runtime &rt, AllegrexContext &ctx, Field kind, u32 field_value, u32 vtable) {
    const u32 self = ctx.gpr[4];
    const u32 arg = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        call(rt, ctx, 0x08A76ACCu, {self, arg});
    }
    if (kind == Field::Half) {
        rt.memory().store16(self + 0x14u, static_cast<std::uint16_t>(field_value));
    } else {
        rt.memory().store8(self + 0x14u, static_cast<std::uint8_t>(field_value));
    }
    put(rt, self, vtable);
    ctx.gpr[2] = vtable;
}

// Copies the word at +0x14 of a1 into +0x14 of a0 after jal 0x08A782D8(a0, a1); v0 = the vtable 0x08CBDFD0.
void call_copy_field_and_stamp(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 src = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        call(rt, ctx, 0x08A782D8u, {self, src});
    }
    put(rt, self + 0x14u, word(rt, src + 0x14u));
    put(rt, self, 0x08CBDFD0u);
    ctx.gpr[2] = 0x08CBDFD0u;
}

// Vtable stamp with a byte at +4 (the `sb`, then the word at +4 masked to its low byte is stored back by the jal's delay slot),
// the call on this+8, then the word at +0x14 cleared.
void stamp_and_forward(Runtime &rt, AllegrexContext &ctx, u32 byte, u32 vtable) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    rt.memory().store8(self + 4u, static_cast<std::uint8_t>(byte));
    const u32 low = word(rt, self + 4u) & 0xFFu;
    put(rt, self, vtable);
    put(rt, self + 4u, low);
    call(rt, ctx, 0x08A75D04u, {self + 8u});
    put(rt, self + 0x14u, 0u);
}

} // namespace

void func_08AAFBB8(Runtime &rt, AllegrexContext &ctx) {
    call_then_stamp(rt, ctx, Field::Half, ctx.gpr[5] & 0xFFFFu, 0x08CD3838u);
}

void func_08AB00A8(Runtime &rt, AllegrexContext &ctx) { call_then_tail(rt, ctx, 0x08A76AB4u, 0x08AAFFB8u); }

void func_08AB133C(Runtime &rt, AllegrexContext &ctx) {
    call_then_stamp(rt, ctx, Field::Byte, ctx.gpr[5] & 0xFFu, 0x08CD3728u);
}





// Returns the callee's v0; the 6-word block at sp holds zeros and a copy of the 8 bytes at a1.
void func_08AC08FC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 w0 = word(rt, ctx.gpr[5]);
    const u32 w1 = word(rt, ctx.gpr[5] + 4u);
    GuestFrame frame(ctx, 0x30u);
    const u32 block = frame.at(0u);
    put(rt, block, 0u);
    put(rt, block + 4u, 0u);
    put(rt, block + 8u, w0);
    put(rt, block + 0xCu, w1);
    put(rt, block + 0x10u, 0u);
    put(rt, block + 0x14u, 0u);
    call(rt, ctx, 0x08AC0880u, {self, block, block + 8u});
}

// As 08AC08FC, with the copy of a1's 8 bytes at block +0x10.
void func_08AC093C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 w0 = word(rt, ctx.gpr[5]);
    const u32 w1 = word(rt, ctx.gpr[5] + 4u);
    GuestFrame frame(ctx, 0x30u);
    const u32 block = frame.at(0u);
    put(rt, block, 0u);
    put(rt, block + 4u, 0u);
    put(rt, block + 8u, 0u);
    put(rt, block + 0xCu, 0u);
    put(rt, block + 0x10u, w0);
    put(rt, block + 0x14u, w1);
    call(rt, ctx, 0x08AC0880u, {self, block, block + 8u});
}

// a1 is masked to a byte and passed as the fourth argument; the block at sp is zeroed.
void func_08AC0D80(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 byte = ctx.gpr[5] & 0xFFu;
    GuestFrame frame(ctx, 0x30u);
    const u32 block = frame.at(0u);
    put(rt, block, 0u);
    put(rt, block + 4u, 0u);
    put(rt, block + 8u, 0u);
    put(rt, block + 0xCu, 0u);
    put(rt, block + 0x10u, 0u);
    put(rt, block + 0x14u, 0u);
    call(rt, ctx, 0x08AC0AACu, {self, block, block + 8u, byte});
}

// a2 is masked to a byte and passed as the fifth argument (t0); a1 passes through unchanged.
void func_08AC0DF4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 arg = ctx.gpr[5];
    const u32 byte = ctx.gpr[6] & 0xFFu;
    GuestFrame frame(ctx, 0x30u);
    const u32 block = frame.at(0u);
    put(rt, block, 0u);
    put(rt, block + 4u, 0u);
    put(rt, block + 8u, 0u);
    put(rt, block + 0xCu, 0u);
    put(rt, block + 0x10u, 0u);
    put(rt, block + 0x14u, 0u);
    call(rt, ctx, 0x08AC0A14u, {self, arg, block, block + 8u, byte});
}

// jal 0x08AB9BF4(this, a1), then a1 at +0x18 of this and the vtable 0x08CD67A0 at +0; v0 = the vtable.
void func_08AC854C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 arg = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        call(rt, ctx, 0x08AB9BF4u, {self, arg});
    }
    put(rt, self + 0x18u, arg);
    put(rt, self, 0x08CD67A0u);
    ctx.gpr[2] = 0x08CD67A0u;
}

// Two calls (the second's result is dropped); v0 is the first call's result, the one made with 0x08D40634 as a0.
void func_08ADDBA8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    u32 result;
    {
        GuestFrame frame(ctx, 0x10u);
        call(rt, ctx, 0x08AEB808u, {self});
        result = call(rt, ctx, 0x08AD8C5Cu, {0x08D40634u, self});
        call(rt, ctx, 0x08AEB82Cu, {0x08D40634u});
    }
    ctx.gpr[2] = result;
}

// Floats 0..2 of the vector at a2 are copied to a local (+3 is 1.0); the call gets that local.

// vtable store, then the call (inside the frame) with this, a second call on this+0xF8, then zero words at +0x104 and +0x108.
void func_08B0E89C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        call(rt, ctx, 0x08B0E008u, {self});
        put(rt, self, 0x08CD7C68u);
        call(rt, ctx, 0x08C4C82Cu, {self + 0xF8u});
    }
    put(rt, self + 0x108u, 0u);
    put(rt, self + 0x104u, 0u);
}

// The member at this+0xC is passed to the first call and tail-called; this is passed to the second.
void func_08B1F928(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 member = self + 0xCu;
    {
        GuestFrame frame(ctx, 0x10u);
        call(rt, ctx, 0x08B344E0u, {member});
        call(rt, ctx, 0x08B1FDA4u, {self});
    }
    tail_call(ctx, 0x08B3457Cu, {member});
}

// Zeroes +0x2C and +0x30 after the call, then tail-calls with this and a1.
void func_08B2BD78(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 arg = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        call(rt, ctx, 0x08B28DCCu, {self, arg});
        put(rt, self + 0x2Cu, 0u);
        put(rt, self + 0x30u, 0u);
    }
    tail_call(ctx, 0x08B2B918u, {self, arg});
}

// jal 0x08B36290(this, a1), then +0x20 = a1 and the vtable 0x08CDA898 at +0; v0 = the vtable.
void func_08B372D4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 arg = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        call(rt, ctx, 0x08B36290u, {self, arg});
    }
    put(rt, self + 0x20u, arg);
    put(rt, self, 0x08CDA898u);
    ctx.gpr[2] = 0x08CDA898u;
}

void func_08A5F438(Runtime &rt, AllegrexContext &ctx) { call_copy_field_and_stamp(rt, ctx); }

void func_08A5F47C(Runtime &rt, AllegrexContext &ctx) { call_copy_field_and_stamp(rt, ctx); }

// Vtable 0x08CB7328 at +0, v0 = it, then the tail call on this+0x14 with a1.
void func_08A6D24C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 arg = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        call(rt, ctx, 0x08A76ACCu, {self, arg});
    }
    put(rt, self, 0x08CB7328u);
    ctx.gpr[2] = 0x08CB7328u;
    tail_call(ctx, 0x08B88F44u, {self + 0x14u, arg});
}

// Forwards a0, a1 and a pointer to the six argument words (a2..t3) to the vtable slot 0x10 of this; the pointer is also stack arg 0.
void func_08ABDEEC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 vtable = word(rt, self);
    GuestFrame frame(ctx, 0x40u);
    const u32 args = frame.at(0x28u);
    put(rt, frame.at(0x28u), ctx.gpr[6]);
    put(rt, frame.at(0x2Cu), ctx.gpr[7]);
    put(rt, frame.at(0x30u), ctx.gpr[8]);
    put(rt, frame.at(0x34u), ctx.gpr[9]);
    put(rt, frame.at(0x38u), ctx.gpr[10]);
    put(rt, frame.at(0x3Cu), ctx.gpr[11]);
    put(rt, frame.at(0u), args);
    call_indirect(rt, ctx, word(rt, vtable + 0x10u), {self, ctx.gpr[5], args});
}

// As 08AC08FC/08AC0DF4: the 8 bytes at a2 are copied into the block; a3 = the block, t0 = the copy's address, the byte in t1.

// a3 is the source of the 8 bytes; a3 = the block, t0 = its copy's address, t1 = the byte (a0..a2 pass through).

// Copies the 16 bytes the callee writes into a local out to this (+0, +4, +8, +0xC); v0 is the callee's.
void func_08B4EAD4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const u32 out = frame.at(0u);
    call(rt, ctx, 0x08B4CFD8u, {out});
    const u32 f0 = word(rt, out);
    const u32 f1 = word(rt, out + 0xCu);
    const u32 f2 = word(rt, out + 8u);
    const u32 f3 = word(rt, out + 4u);
    put(rt, self + 8u, f2);
    put(rt, self, f0);
    put(rt, self + 0xCu, f1);
    put(rt, self + 4u, f3);
}



// Float 8 of this is moved by the callee's float result (f0), with the argument vector (f12 - f20, f13 - f20); v0 = this.
void func_08B28ED4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const float f20 = ctx.fpr[12];
    const float x = float_at(rt, self + 8u) - f20;
    const float y = ctx.fpr[13] - f20;
    GuestFrame frame(ctx, 0x20u);
    const GuestResult r = call_indirect(rt, ctx, 0x08B28E04u, {1u}, {x, y});
    const float updated = f20 + r.f0;
    put_float(rt, self + 8u, updated);
    ctx.gpr[2] = self;
}

// The first call's v0 is the third argument of the tail call; a1 is passed to it twice (the callee reads a0 = a1).

// Allocates 0xA0 bytes (4 aligned), passes the block to the second call and stores the first result at sp+0 for the third.
void func_089B5598(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const u32 first = call(rt, ctx, 0x08A5BC20u, {0xA0u, 4u});
    call(rt, ctx, 0x089B5048u, {first});
    put(rt, frame.at(0u), first);
    call(rt, ctx, 0x089FFC94u, {self, frame.at(0u)});
}

} // namespace new_batch_main_1614
} // namespace gt::game

#define NEW1614(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1614::name, gt::ReturnKind::kind)
NEW1614(0x08AAFBB8u, func_08AAFBB8, Word);
NEW1614(0x08AB00A8u, func_08AB00A8, Word);
NEW1614(0x08AB133Cu, func_08AB133C, Word);
NEW1614(0x08AC08FCu, func_08AC08FC, Word);
NEW1614(0x08AC093Cu, func_08AC093C, Word);
NEW1614(0x08AC0D80u, func_08AC0D80, Word);
NEW1614(0x08AC0DF4u, func_08AC0DF4, Word);
NEW1614(0x08AC854Cu, func_08AC854C, Word);
NEW1614(0x08ADDBA8u, func_08ADDBA8, Word);
NEW1614(0x08B0E89Cu, func_08B0E89C, Word);
NEW1614(0x08B1F928u, func_08B1F928, Word);
NEW1614(0x08B2BD78u, func_08B2BD78, Word);
NEW1614(0x08B372D4u, func_08B372D4, Word);
NEW1614(0x08A5F438u, func_08A5F438, Word);
NEW1614(0x08A5F47Cu, func_08A5F47C, Word);
NEW1614(0x08A6D24Cu, func_08A6D24C, Word);
NEW1614(0x08ABDEECu, func_08ABDEEC, Word);
NEW1614(0x08B4EAD4u, func_08B4EAD4, Word);
NEW1614(0x08B28ED4u, func_08B28ED4, Word);
NEW1614(0x089B5598u, func_089B5598, Word);
