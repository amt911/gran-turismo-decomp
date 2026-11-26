// Main-session batch 1119 (draft, unverified): 30 functions, structured C from tools.disasm.
// Input shapes: gt-ops/shapes/shapes_main1119.txt (one line per address that dereferences an argument register).
// Before registering: run the four audits (frame_audit, literal_check, sp_loads, arg_reads) and the synthetic gate; drop what never compares.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1119 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;

u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }

// The 0x30-byte frames of 08AC0BF0..08AC12E4: the six words at sp+0..+0x14 are zeroed, then the two words at `data`
// are copied to sp+copy_at (0x10 or 0x08). The caller passes sp, sp+8 and the flag to the callee.
void fill_block(Runtime &rt, const GuestFrame &frame, u32 data, u32 copy_at) {
    const u32 lo = word(rt, data);
    const u32 hi = word(rt, data + 4u);
    for (u32 offset = 0u; offset <= 0x14u; offset += 4u) put(rt, frame.at(offset), 0u);
    put(rt, frame.at(copy_at), lo);
    put(rt, frame.at(copy_at + 4u), hi);
}

// 0x08AF49EC family: t = field(a0 + in_off) -/+ 1.0f; s = 0x08AF49B4(a0) (float result); v0 = t * s / field(a0 + div_off).
void float_ratio(Runtime &rt, AllegrexContext &ctx, u32 in_off, bool subtract, u32 div_off) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    float t = float_at(rt, self + in_off);
    t = subtract ? t - 1.0f : t + 1.0f;
    const GuestResult scale = call_guest(rt, ctx, 0x08AF49B4u, {self}, {});
    const float product = psp_mul(t, scale.f0);
    const float divisor = float_at(rt, self + div_off);
    ctx.fpr[1] = divisor;
    ctx.fpr[0] = product / divisor;
}

// 0x08A5D7F0 / 0x08A5D83C / 0x08A82B40: a0 gets a copy of the (+0x18, +0x1C) pair of a1 and a vtable word.
void copy_pair_and_tag(Runtime &rt, AllegrexContext &ctx, u32 tag) {
    GuestFrame frame(ctx, 0x10u);
    const u32 dst = ctx.gpr[4];
    const u32 src = ctx.gpr[5];
    call(rt, ctx, 0x08A782D8u, {dst});
    const u32 first = word(rt, src + 0x18u);
    const u32 second = word(rt, src + 0x1Cu);
    put(rt, dst + 0x18u, first);
    put(rt, dst + 0x1Cu, second);
    put(rt, dst, tag);
    ctx.gpr[2] = first;
}

// 0x08A66D6C / 0x08A67074: the byte at +4 is the tag, the word at +4 keeps only that byte, then the callee gets +8.
void tagged_block(Runtime &rt, AllegrexContext &ctx, u32 tag_byte, u32 vtable) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    put8(rt, self + 4u, tag_byte);
    put(rt, self, vtable);
    const u32 kept = word(rt, self + 4u) & 0xFFu;
    put(rt, self + 4u, kept);
    call(rt, ctx, 0x08A75D04u, {self + 8u});
    put(rt, self + 0x14u, 0u);
}

// 0x08A647F4 / 0x08A64AA0: a0 gets vtable, a delegated call, then tail-calls 0x08A90480 with a0 = +0xC and a1 = a2.
void tail_with_block(Runtime &rt, AllegrexContext &ctx, u32 tag, u32 vtable) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 src = ctx.gpr[5];
    const u32 third = ctx.gpr[6];
    call(rt, ctx, 0x08A644FCu, {self, tag, src});
    put(rt, self, vtable);
    tail_call(ctx, 0x08A90480u, {self + 0xCu, third});
}

} // namespace

// 0x08B3FF00 (68 B): forwards to 0x08B3F838, then 0x08B3FA3C(a0, a2 & 0xFFFF), then tail-calls 0x08B3F7A8(a0, 2).
void func_08B3FF00(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 count = ctx.gpr[6] & 0xFFFFu;
    call(rt, ctx, 0x08B3F838u, {ctx.gpr[4], ctx.gpr[5], ctx.gpr[6], ctx.gpr[7]});
    call(rt, ctx, 0x08B3FA3Cu, {self, count});
    tail_call(ctx, 0x08B3F7A8u, {self, 2u});
}

// 0x08A94074 (72 B): vtable, two sub-objects, zero +0x24, then tail-calls 0x08A93DD4(a0).
void func_08A94074(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08A719F0u, {self});
    put(rt, self, 0x08CD3B28u);
    call(rt, ctx, 0x08A76AA0u, {self + 0x1Cu});
    call(rt, ctx, 0x08A94CE8u, {self + 0x20u});
    put(rt, self + 0x24u, 0u);
    tail_call(ctx, 0x08A93DD4u, {self});
}

// 0x08AB49FC (72 B): base init, sub-object at +0x14, vtable and a pointer +0x24 to 0x08D74FDC (returned).
void func_08AB49FC(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08A76ACCu, {self});
    put(rt, self, 0x08CD4A58u);
    call(rt, ctx, 0x08B1E97Cu, {self + 0x14u});
    const u32 table = 0x08D74FD0u + 0xCu;
    put(rt, self + 0x24u, table);
    ctx.gpr[2] = table;
}

// 0x08ABA43C (72 B): base init, sub-object at +0x14, byte +0x4C and words +0x40..+0x48 cleared; v0 from the last call.
void func_08ABA43C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08ABB250u, {self});
    put(rt, self, 0x08CD6018u);
    call(rt, ctx, 0x08B28D34u, {self + 0x14u});
    put8(rt, self + 0x4Cu, 0u);
    put(rt, self + 0x40u, 0u);
    put(rt, self + 0x44u, 0u);
    put(rt, self + 0x48u, 0u);
}

// 0x08ABB0B8 (72 B): base init, then pushes the object onto the list head at 0x08D77184; v0 = the old head.
void func_08ABB0B8(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08ABCD64u, {self});
    put(rt, self, 0x08CD60C8u);
    const u32 head_slot = 0x08D77184u;
    const u32 old_head = word(rt, head_slot);
    put(rt, self + 0x10u, self);
    put(rt, self + 0xCu, old_head);
    put(rt, head_slot, self + 0xCu);
    ctx.gpr[2] = old_head;
}

// 0x08AC0BF0 (72 B): copies the pair at a1 to sp+0x10, zeroes sp+0..+0xC, calls 0x08AC0AAC(a0, sp, sp+8, a2 & 0xFF).
void func_08AC0BF0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const u32 flag = ctx.gpr[6] & 0xFFu;
    fill_block(rt, frame, ctx.gpr[5], 0x10u);
    call(rt, ctx, 0x08AC0AACu, {ctx.gpr[4], frame.at(0u), frame.at(8u), flag});
}

// 0x08AC0C70 (72 B): copies the pair at a2 to sp+0x10, calls 0x08AC0A14(a0, a1, sp, sp+8, a3 & 0xFF).
void func_08AC0C70(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const u32 flag = ctx.gpr[7] & 0xFFu;
    fill_block(rt, frame, ctx.gpr[6], 0x10u);
    call(rt, ctx, 0x08AC0A14u, {ctx.gpr[4], ctx.gpr[5], frame.at(0u), frame.at(8u), flag});
}

// 0x08AC0CB8 (72 B): copies the pair at a1 to sp+8, calls 0x08AC0AAC(a0, sp, sp+8, a2 & 0xFF).
void func_08AC0CB8(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const u32 flag = ctx.gpr[6] & 0xFFu;
    fill_block(rt, frame, ctx.gpr[5], 0x08u);
    call(rt, ctx, 0x08AC0AACu, {ctx.gpr[4], frame.at(0u), frame.at(8u), flag});
}

// 0x08AC0D38 (72 B): copies the pair at a2 to sp+8, calls 0x08AC0A14(a0, a1, sp, sp+8, a3 & 0xFF).
void func_08AC0D38(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const u32 flag = ctx.gpr[7] & 0xFFu;
    fill_block(rt, frame, ctx.gpr[6], 0x08u);
    call(rt, ctx, 0x08AC0A14u, {ctx.gpr[4], ctx.gpr[5], frame.at(0u), frame.at(8u), flag});
}

// 0x08AC119C (72 B): copies the pair at a2 to sp+0x10, calls 0x08AC1060(a0, a1, sp, sp+8, a3 & 0xFF).
void func_08AC119C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const u32 flag = ctx.gpr[7] & 0xFFu;
    fill_block(rt, frame, ctx.gpr[6], 0x10u);
    call(rt, ctx, 0x08AC1060u, {ctx.gpr[4], ctx.gpr[5], frame.at(0u), frame.at(8u), flag});
}

// 0x08AC121C (72 B): copies the pair at a3 to sp+0x10, calls 0x08AC0F08(a0, a1, a2, sp, sp+8, t0 & 0xFF).
void func_08AC121C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const u32 flag = ctx.gpr[8] & 0xFFu;
    fill_block(rt, frame, ctx.gpr[7], 0x10u);
    call(rt, ctx, 0x08AC0F08u, {ctx.gpr[4], ctx.gpr[5], ctx.gpr[6], frame.at(0u), frame.at(8u), flag});
}

// 0x08AC1264 (72 B): copies the pair at a2 to sp+8, calls 0x08AC1060(a0, a1, sp, sp+8, a3 & 0xFF).

// 0x08AC12E4 (72 B): copies the pair at a3 to sp+8, calls 0x08AC0F08(a0, a1, a2, sp, sp+8, t0 & 0xFF).

// 0x08AF49EC (72 B): float ratio: (field +0x20 - 1.0) * scale(a0) / field +0x00; f0 result.
void func_08AF49EC(Runtime &rt, AllegrexContext &ctx) { float_ratio(rt, ctx, 0x20u, true, 0x00u); }

// 0x08AF4A34 (72 B): float ratio: (field +0x20 + 1.0) * scale(a0) / field +0x00.
void func_08AF4A34(Runtime &rt, AllegrexContext &ctx) { float_ratio(rt, ctx, 0x20u, false, 0x00u); }

// 0x08AF4A7C (72 B): float ratio: (field +0x24 + 1.0) * scale(a0) / field +0x14.
void func_08AF4A7C(Runtime &rt, AllegrexContext &ctx) { float_ratio(rt, ctx, 0x24u, false, 0x14u); }

// 0x08AF4AC4 (72 B): float ratio: (field +0x24 - 1.0) * scale(a0) / field +0x14.
void func_08AF4AC4(Runtime &rt, AllegrexContext &ctx) { float_ratio(rt, ctx, 0x24u, true, 0x14u); }

// 0x08B1B9C4 (72 B): calls 0x08B2E1F8(a0, a1), then stores the byte a1 & 0xFF at +0xC and clears the rest of the record.
void func_08B1B9C4(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 flag = ctx.gpr[5] & 0xFFu;
    call(rt, ctx, 0x08B2E1F8u, {self});
    put8(rt, self + 0xCu, flag);
    put(rt, self + 0x14u, 0u);
    put(rt, self + 0x4u, 0u);
    put(rt, self + 0x8u, 0u);
    put8(rt, self + 0xDu, 0u);
    put(rt, self + 0x10u, 0u);
}

// 0x08B1E43C (72 B): the global record at 0x08D68AE8 is passed to 0x08C4E874 with its word +8; then its links are reset and v0 = 0x08D68AEC.
void func_08B1E43C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 record = 0x08D68AE8u;
    call(rt, ctx, 0x08C4E874u, {record, word(rt, record + 8u)});
    const u32 list = 0x08D68AECu;
    put(rt, record + 0x10u, list);
    put(rt, record + 0x14u, 0u);
    put(rt, record + 0xCu, list);
    put(rt, record + 0x8u, 0u);
    ctx.gpr[2] = list;
}

// 0x08B3DF88 (72 B): calls 0x08B3E0AC(a0, a2, a3), stores a1 at +0x2C and the vtable; v0 = 0x08CDA960.
void func_08B3DF88(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    call(rt, ctx, 0x08B3E0ACu, {self, ctx.gpr[6], ctx.gpr[7]});
    put(rt, self + 0x2Cu, value);
    put(rt, self, 0x08CDA960u);
    ctx.gpr[2] = 0x08CDA960u;
}

// 0x08A5D5EC (76 B): base init, a sub-object at +0xC is set up by 0x08A6972C(a1), word +0x10 cleared; v0 = the callee's.
void func_08A5D5EC(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    call(rt, ctx, 0x08A5B828u, {self});
    put(rt, self, 0x08CD2578u);
    call(rt, ctx, 0x08A6972Cu, {self + 0xCu, value});
    put(rt, self + 0x10u, 0u);
}

// 0x08A5D7F0 (76 B): copies the +0x18/+0x1C pair of a1 into a0 and sets vtable 0x08CB8688; v0 = the copied first word.
void func_08A5D7F0(Runtime &rt, AllegrexContext &ctx) { copy_pair_and_tag(rt, ctx, 0x08CB8688u); }

// 0x08A5D83C (76 B): the same body as 0x08A5D7F0.
void func_08A5D83C(Runtime &rt, AllegrexContext &ctx) { copy_pair_and_tag(rt, ctx, 0x08CB8688u); }

// 0x08A6152C (76 B): base init, a list head at +0x18 that points to itself; v0 = a0 + 0x18.
void func_08A6152C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08A76ACCu, {self});
    put(rt, self, 0x08CD2468u);
    call(rt, ctx, 0x08A61BC0u, {self + 0x14u, 0x08CF6ED8u});
    const u32 head = self + 0x18u;
    put(rt, self + 0x18u, head);
    put(rt, head + 4u, head);
    ctx.gpr[2] = head;
}

// 0x08A647F4 (76 B): see tail_with_block; tag 0x14, vtable 0x08CD2BB0.
void func_08A647F4(Runtime &rt, AllegrexContext &ctx) { tail_with_block(rt, ctx, 0x14u, 0x08CD2BB0u); }

// 0x08A64AA0 (76 B): see tail_with_block; tag 0xA, vtable 0x08CD2A20.
void func_08A64AA0(Runtime &rt, AllegrexContext &ctx) { tail_with_block(rt, ctx, 0x0Au, 0x08CD2A20u); }

// 0x08A66D6C (76 B): tagged block with byte 0x24 at +4 and vtable 0x08CD2E30.

// 0x08A67074 (76 B): tagged block with byte 0x26 at +4 and vtable 0x08CD2E80.

// 0x08A7CB5C (76 B): builds a block at sp, 0x08A7CB20(a0, sp, a2), then 0x08A69D68(sp).
void func_08A7CB5C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 third = ctx.gpr[6];
    call(rt, ctx, 0x08A69B3Cu, {frame.at(0u)});
    call(rt, ctx, 0x08A7CB20u, {self, frame.at(0u), third});
    call(rt, ctx, 0x08A69D68u, {frame.at(0u)});
}

// 0x08A82B40 (76 B): as 0x08A5D7F0 with vtable 0x08CBBD00.
void func_08A82B40(Runtime &rt, AllegrexContext &ctx) { copy_pair_and_tag(rt, ctx, 0x08CBBD00u); }

#define NEWM(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1119::name, gt::ReturnKind::kind)
NEWM(0x08B3FF00u, func_08B3FF00, Word);
NEWM(0x08A94074u, func_08A94074, Word);
NEWM(0x08AB49FCu, func_08AB49FC, Word);
NEWM(0x08ABA43Cu, func_08ABA43C, Word);
NEWM(0x08ABB0B8u, func_08ABB0B8, Word);
NEWM(0x08AC0BF0u, func_08AC0BF0, Word);
NEWM(0x08AC0C70u, func_08AC0C70, Word);
NEWM(0x08AC0CB8u, func_08AC0CB8, Word);
NEWM(0x08AC0D38u, func_08AC0D38, Word);
NEWM(0x08AC119Cu, func_08AC119C, Word);
NEWM(0x08AC121Cu, func_08AC121C, Word);
NEWM(0x08AF49ECu, func_08AF49EC, Float);
NEWM(0x08AF4A34u, func_08AF4A34, Float);
NEWM(0x08AF4A7Cu, func_08AF4A7C, Float);
NEWM(0x08AF4AC4u, func_08AF4AC4, Float);
NEWM(0x08B1B9C4u, func_08B1B9C4, Word);
NEWM(0x08B1E43Cu, func_08B1E43C, Word);
NEWM(0x08B3DF88u, func_08B3DF88, Word);
NEWM(0x08A5D5ECu, func_08A5D5EC, Word);
NEWM(0x08A5D7F0u, func_08A5D7F0, Word);
NEWM(0x08A5D83Cu, func_08A5D83C, Word);
NEWM(0x08A6152Cu, func_08A6152C, Word);
NEWM(0x08A647F4u, func_08A647F4, Word);
NEWM(0x08A64AA0u, func_08A64AA0, Word);
NEWM(0x08A7CB5Cu, func_08A7CB5C, Word);
NEWM(0x08A82B40u, func_08A82B40, Word);
#undef NEWM

} // namespace new_batch_main_1119
} // namespace gt::game
