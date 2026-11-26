// New batch main-1591 (draft, unverified): twenty-six functions written as structured C by hand from tools.disasm.
// The three re-drafted 0x08AE1xxx writers keep the `lui` base v0 holds across their last call (the callee
// 0x08AE0F64 returns early when its count is <= 0, leaving v0 untouched), 0x08B01070 keeps its batch-18 shape, and
// each body keeps the asm's register value on every exit path and the delay-slot stores before the call or branch
// they belong to.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1591 {
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
// A call with float arguments ($f12..) and a float answer; v0 / v1 / f0 come back as the callee left them.
GuestResult call_float(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                       std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
s32 signed_byte(Runtime &rt, u32 address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }
s32 signed_half(Runtime &rt, u32 address) { return static_cast<std::int16_t>(rt.memory().load16(address)); }
void put_byte(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void put_half(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
// The `lui` half a register holds for an absolute address whose low half goes in as a signed 16-bit immediate (the
// value the recompiler sees: 0x08CB22F8 -> 0x08CB0000, 0x08CFFAAC -> 0x08D00000).
constexpr u32 high_half(u32 address) { return (address + 0x8000u) & 0xFFFF0000u; }
// ll / sc decrement of the word at `address` (nothing else runs here, so the store always succeeds and v0 is 1
// afterwards): true when it was 1.
bool decrement_reaches_zero(Runtime &rt, AllegrexContext &ctx, u32 address) {
    const u32 old = word(rt, address);
    put(rt, address, old - 1u);
    ctx.gpr[2] = 1u;
    return old == 1u;
}

// 0x08ADCDE8 (a0 = self, a1 = row, a2 = column, a3 = three-word out or 0): a name lookup (a1 = 0x08C90FBC); the
// lookup's +0x28 is the row table (8-byte rows, +8 = cell count, +0xC = byte offset), its +0x14 the cell base and the
// cells 12 bytes each. v0 = 1 when a cell was copied out, else 0; a null out still answers 1.
void func_08ADCDE8(Runtime &rt, AllegrexContext &ctx) {
    const s32 row = static_cast<s32>(ctx.gpr[5]);
    const s32 column = static_cast<s32>(ctx.gpr[6]);
    const u32 out = ctx.gpr[7];
    GuestFrame frame(ctx, 0x10u);
    const u32 found = call(rt, ctx, 0x08ADB278u, {ctx.gpr[4], 0x08C90FBCu});
    u32 answer = 0u;
    if (found != 0u) {
        const u32 table = word(rt, found + 0x28u);
        if (table != 0u && row >= 0 && !((static_cast<s32>(word(rt, found + 0x30u)) - 1) < row)) {
            if (column >= 0) {
                const u32 row_entry = table + static_cast<u32>(row) * 8u;
                if (!((static_cast<s32>(word(rt, row_entry + 8u)) - 1) < column)) {
                    const u32 base = word(rt, found + 0x14u);
                    const u32 cell = base + word(rt, row_entry + 0xCu) + static_cast<u32>(column) * 12u;
                    if (out != 0u) {
                        put(rt, out, base + word(rt, cell));
                        put(rt, out + 4u, word(rt, cell + 4u));
                        put(rt, out + 8u, word(rt, cell + 8u));
                    }
                    answer = 1u;
                }
            }
        }
    }
    ctx.gpr[2] = answer;
}

// 0x08B17848 (a0 = self, a1..a3 + t0/t1 to the setup): registers the object with 0x08B34888 (a0 = 0x08D61100), keeps
// self on the frame, sets the vtable 0x08D51100 at +0x24, the byte t1 & 0xFF at +0x28, the span a1 + a2 at +0x1C and
// a1 at +0x14 / +0x18, builds through 0x08B26B28 (self, a3, t0), asks 0x08B7B2E4 about self+0x2C (a zero answer goes to
// 0x08B25968(self)), unregisters 0x08B34B24(0x08D61100) and answers word(self+0x1C) - word(self+0x14).

// 0x0882D758 (a0 = self): base constructor 0x08A2A154, the vtable 0x08CBBBC0 at +0, a 0x10-byte block built by
// 0x08865D1C with 0x08A21F8C(block+8, 0x08CF6ED8) and 0x08835AA8(block+0xC), stored at +0x18; the guarded global at
// 0x08D75160 / 0x08D75168 (vtable 0x08CBE908) is registered once, and 0x08860050(word(self+0x18)) answers into +0x14.

// 0x0886E888 (a0 = self): base constructor 0x0886AD04, the vtable 0x08CBF770 at +0, a 0x3C-byte block built by
// 0x08871908(block), 0x0886FC04(block+0x14), 0x088744FC(block+0x28) and 0x0886F8B4(block+0x14, 1); the block is stored
// at +0x70 with the guarded global 0x08D752E8 (vtable 0x08CBF7A8) at +0x74 and the tail 0x0886AAA4(self, block) runs.
void func_0886E888(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    call(rt, ctx, 0x0886AD04u, {self});
    put(rt, self, 0x08CBF770u);
    const u32 block = call(rt, ctx, 0x08B62344u, {0x3Cu});
    call(rt, ctx, 0x08871908u, {block});
    call(rt, ctx, 0x0886FC04u, {block + 0x14u});
    call(rt, ctx, 0x088744FCu, {block + 0x28u});
    call(rt, ctx, 0x0886F8B4u, {block + 0x14u, 1u});
    put(rt, self + 0x70u, block);
    if (signed_byte(rt, 0x08D752E0u) == 0) {
        if (call(rt, ctx, 0x08B6441Cu, {0x08D752E0u}) != 0u) {
            put(rt, 0x08D752E8u, 0x08CBF7A8u);
            call(rt, ctx, 0x08B64494u, {0x08D752E0u});
        }
    }
    put(rt, self + 0x74u, 0x08D752E8u);
    tail_call(ctx, 0x0886AAA4u, {self, word(rt, self + 0x70u)});
}

// 0x0886F51C (a0 = self, a1 = flag byte, a2 = text, a3 = tag or 0, f16 = value): an empty text or an empty first byte
// tail-calls 0x0886F450(self) unchanged; otherwise the object at self+0x70 gets its +0x90 setup, +0x138 = flag and
// +0x13C = f16, then 0x08B945C4(self+0x70 + 0x130, text, 0x08B35F60(text)), and finally the same pair on the
// +0x134 member with `tag` (0 stands for 0x08C89158).
void func_0886F51C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 flag = ctx.gpr[5] & 0xFFu;
    const u32 text = ctx.gpr[6];
    const u32 tag_in = ctx.gpr[7];
    const float value = ctx.fpr[16];
    GuestFrame frame(ctx, 0x30u);
    if (text == 0u || signed_byte(rt, text) == 0) {
        tail_call(ctx, 0x0886F450u, {self});
        return;
    }
    call(rt, ctx, 0x08AF322Cu, {word(rt, self + 0x70u) + 0x90u});
    put_byte(rt, word(rt, self + 0x70u) + 0x138u, flag);
    put_float(rt, word(rt, self + 0x70u) + 0x13Cu, value);
    const u32 made = call(rt, ctx, 0x08B35F60u, {text});
    call(rt, ctx, 0x08B945C4u, {word(rt, self + 0x70u) + 0x130u, text, made});
    const u32 tag = tag_in != 0u ? tag_in : 0x08C89158u;
    const u32 made2 = call(rt, ctx, 0x08B35F60u, {tag});
    tail_call(ctx, 0x08B945C4u, {word(rt, self + 0x70u) + 0x134u, tag, made2});
}

// 0x0888A6AC / 0x0888A78C (a0 = out): a 0x120-byte block, vtable 0x08CC1A10 at +0, +4 = 0, 0x0888A600(block+8), three
// 0x50-byte records from +0x10 with their +0x40 / +0x44 / +0x48 zeroed, 0x08B1C2BC(block+0x100), +0x110..+0x11C zero,
// *out = block; the guarded global at 0x08D75E70 / 0x08D75E80 (vtable 0x08CC1B18) goes to out+4.
void make_block_120(Runtime &rt, AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const u32 block = call(rt, ctx, 0x08B62344u, {0x120u});
    put(rt, block, 0x08CC1A10u);
    put(rt, block + 4u, 0u);
    call(rt, ctx, 0x0888A600u, {block + 8u});
    for (u32 i = 0u; i < 3u; ++i) {
        const u32 record = block + 0x10u + i * 0x50u;
        put(rt, record + 0x40u, 0u);
        put(rt, record + 0x44u, 0u);
        put(rt, record + 0x48u, 0u);
    }
    call(rt, ctx, 0x08B1C2BCu, {block + 0x100u});
    put(rt, block + 0x110u, 0u);
    put(rt, block + 0x114u, 0u);
    put(rt, block + 0x118u, 0u);
    put(rt, block + 0x11Cu, 0u);
    put(rt, out, block);
    u32 answer = high_half(0x08D75E80u);
    if (signed_byte(rt, 0x08D75E70u) == 0) {
        answer = call(rt, ctx, 0x08B6441Cu, {0x08D75E70u});
        if (answer != 0u) {
            put(rt, 0x08D75E80u, 0x08CC1B18u);
            answer = call(rt, ctx, 0x08B64494u, {0x08D75E70u});
        }
    }
    put(rt, out + 4u, 0x08D75E80u);
    ctx.gpr[2] = answer;
}
void func_0888A6AC(Runtime &rt, AllegrexContext &ctx) { make_block_120(rt, ctx); }
void func_0888A78C(Runtime &rt, AllegrexContext &ctx) { make_block_120(rt, ctx); }

// 0x089EFC90 (a0 = self): base constructor 0x089E20D0, vtable 0x08CBF080 at +0 and 0x08CBF2BC at +0x38, 2.0f at +0x234,
// 1 at +0x22C, the guarded object 0x08D74FDC at +0x238 / +0x23C, 0 at +0x230, two 0x089E4C14 members at +0x240 / +0x244
// (both with 0x08CF6ED8), floats 1.0f / -1.0f / 1.0f at +0x254 / +0x258 / +0x24C, zeros through +0x25C, three zero bytes,
// a 0x100-step delay spin, then +0x264 / +0xA68 / +0xE68 / +0xE6C zeroed; v0 is -1 at exit (the spin's last delay).

// 0x08AE11A0 / 0x08AE1338 / 0x08AE160C (a0 = object, f12 = value, f13 = second float): enter 0x08AEB808(object), a
// 4 * word(0x08D4069C)-byte scratch that `collect` fills with (scratch, object, 0), leave 0x08AEB82C(scratch), then the
// value clamped to [low, high] (high when it is above), scaled and truncated to the integer 0x08AE0F64(scratch, count,
// integer, f12 = the second float) takes. v0 is the `lui` base of the scale constant on the path where that call
// returns early (count <= 0 leaves v0 alone).
void scaled_parameter(Runtime &rt, AllegrexContext &ctx, u32 collect, u32 high_at, u32 low_at, u32 scale_at) {
    const u32 self = ctx.gpr[4];
    const float value_in = ctx.fpr[12];
    const float second = ctx.fpr[13];
    GuestFrame frame(ctx, 0x30u);
    call_float(rt, ctx, 0x08AEB808u, {self}, {value_in});
    const u32 count = word(rt, 0x08D4069Cu);
    GuestFrame scratch(ctx, (count * 4u + 0xFu) & ~0xFu);
    const u32 collected = call(rt, ctx, collect, {scratch.at(0u), self, 0u});
    call(rt, ctx, 0x08AEB82Cu, {scratch.at(0u)});
    const float high = float_at(rt, high_at);
    float value = high;
    if (value_in < high) {
        const float low = float_at(rt, low_at);
        value = value_in < low ? low : value_in;
    }
    const u32 integer = ctx.fpu_float_to_word(psp_mul(value, float_at(rt, scale_at)), 1u);
    ctx.gpr[2] = high_half(scale_at);
    call_float(rt, ctx, 0x08AE0F64u, {scratch.at(0u), collected, integer}, {second});
}
void func_08AE11A0(Runtime &rt, AllegrexContext &ctx) {
    scaled_parameter(rt, ctx, 0x08ADE328u, 0x08CB60C4u, 0x08CB60C8u, 0x08CB60CCu);
}
void func_08AE1338(Runtime &rt, AllegrexContext &ctx) {
    scaled_parameter(rt, ctx, 0x08ADE3A8u, 0x08CB60D0u, 0x08CB60D4u, 0x08CB60D8u);
}
void func_08AE160C(Runtime &rt, AllegrexContext &ctx) {
    scaled_parameter(rt, ctx, 0x08ADE4ACu, 0x08CB60E8u, 0x08CB60ECu, 0x08CB60F0u);
}

// 0x08B29CBC (a0 = state, a1 = out, a2 = in, a3 = length, t0/t1 = key halves): sets the cipher up with
// 0x08B2933C(state, out, (t1 << 26) | (t0 >> 6), t1 >> 6), fills a 0x40-byte keystream with 0x08B2936C(state, buffer) and
// XORs `length` bytes of `in` into `out`, starting at t0 & 0x3F and regenerating the stream (0x08B29348) when it wraps.
// v0 = 0 with no bytes, else buffer + the keystream index.
void func_08B29CBC(Runtime &rt, AllegrexContext &ctx) {
    const u32 state = ctx.gpr[4];
    const u32 out = ctx.gpr[5];
    const u32 in = ctx.gpr[6];
    const u32 length = ctx.gpr[7];
    const u32 t0 = ctx.gpr[8];
    const u32 t1 = ctx.gpr[9];
    GuestFrame frame(ctx, 0x60u);
    call(rt, ctx, 0x08B2933Cu, {state, out, (t1 << 26) | (t0 >> 6), t1 >> 6});
    call(rt, ctx, 0x08B2936Cu, {state, frame.at(0u)});
    const u32 end = in + length;
    u32 index = t0 & 0x3Fu;
    if (in < end) {
        u32 source = in;
        u32 dest = out;
        for (;;) {
            const u32 key = rt.memory().load8(frame.at(0u) + index);
            const u32 data = rt.memory().load8(source);
            ++index;
            const bool more = index < 0x40u;
            put_byte(rt, dest, data ^ key);
            ++source;
            if (!more) {
                call(rt, ctx, 0x08B29348u, {state});
                index = 0u;
                call(rt, ctx, 0x08B2936Cu, {state, frame.at(0u)});
            }
            ++dest;
            if (source == end) break;
        }
        ctx.gpr[2] = frame.at(0u) + index;
    } else {
        ctx.gpr[2] = 0u;
    }
}

// 0x08BE0DD8 (a0 = self): the base destructor: vtable 0x08CC6888, then the holder at +0x2C (use count +4, weak count
// +8; its virtuals +8 / +0xC run when each count reaches zero), then the holder at +0x24 (only its +8 count and virtual
// +0xC), and the vtable becomes 0x08CC3000.

// 0x088DECFC (a0 = self, a1 = a, a2 = count (signed), a3 = mode, f12 = scale): 0x088DA588(self, a1, a2, a3) makes an
// object; if one comes back its +0x10 takes the count and its handler (self+0x134), when non-zero, runs its virtual
// +0x90 with (handler, a1, a2, count, mode) and the scale * 100 truncated as the third word. With a positive count the
// table at word(self+0xC0) + 760 * a1 gets 0x088E8A0C(entry, 0 below mode 3, 1 below 5, 2 at 5).
void func_088DECFC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 count_bits = ctx.gpr[6];
    const u32 mode = ctx.gpr[7];
    const float scale = ctx.fpr[12];
    GuestFrame frame(ctx, 0x30u);
    const u32 object = call(rt, ctx, 0x088DA588u, {self, first, count_bits, mode});
    if (object != 0u) {
        const u32 scaled = ctx.fpu_float_to_word(psp_mul(scale, 100.0f), 1u);
        put(rt, object + 0x10u, count_bits);
        const u32 handler = word(rt, self + 0x134u);
        if (handler != 0u) {
            call_indirect(rt, ctx, word(rt, word(rt, handler) + 0x90u), {handler, first, count_bits, scaled, mode});
        }
    }
    if (static_cast<s32>(count_bits) > 0) {
        u32 selection = 0u;
        if (static_cast<s32>(mode) >= 3) selection = static_cast<s32>(mode) < 5 ? 1u : (mode == 5u ? 2u : 0u);
        const u32 entry = word(rt, self + 0xC0u) + static_cast<u32>(first) * 760u;
        call(rt, ctx, 0x088E8A0Cu, {entry, selection});
    }
    ctx.gpr[2] = 1u;
}

// 0x08917D38 (a0 = self, a1 = out): when the object at self+0xC has its +0x1FC7 flag and its +0x1FCD byte is 1, the
// 0x089277A4(object, 3) answer's +0xC0 byte decides: non-zero queues through 0x08917C2C(self, out, 0, 0x1A) into
// out+8 and sets the object's +0x1FCC to 1, else 0x089277BC's answer 0 goes the 0x08917C2C(self, out, 3, 0) way and
// +0x1FCC clears. Then 0x089274C8(object, out) runs and +0x1FCD clears.
void func_08917D38(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 out = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    const u32 object = word(rt, self + 0xCu);
    u32 answer = 0u;
    if (rt.memory().load8(object + 0x1FC7u) != 0u) {
        const u32 flag = rt.memory().load8(object + 0x1FCDu);
        if (flag != 1u) {
            ctx.gpr[2] = 1u;
            return;
        }
        const u32 queued = call(rt, ctx, 0x089277A4u, {object, 3u});
        if (rt.memory().load8(queued + 0xC0u) == 0u) {
            ctx.gpr[2] = queued;
            return;
        }
        if (call(rt, ctx, 0x089277BCu, {word(rt, self + 0xCu)}) == 3u) {
            put(rt, out + 8u, call(rt, ctx, 0x08917C2Cu, {self, out, 0u, 0x1Au}));
            put_byte(rt, word(rt, self + 0xCu) + 0x1FCCu, flag);
        } else {
            if (call(rt, ctx, 0x089277BCu, {word(rt, self + 0xCu)}) == 0u) {
                put(rt, out + 8u, call(rt, ctx, 0x08917C2Cu, {self, out, 3u, 0u}));
                put_byte(rt, word(rt, self + 0xCu) + 0x1FCCu, 0u);
            }
        }
        call(rt, ctx, 0x089274C8u, {word(rt, self + 0xCu), out});
        put_byte(rt, word(rt, self + 0xCu) + 0x1FCDu, 0u);
        answer = word(rt, self + 0xCu);
    }
    ctx.gpr[2] = answer;
}

// 0x089DE0D8 / 0x089DE240 (a0 = self, a1 = array of float pairs, a2 = count): the member selected by bit 7 (0x089DE0D8,
// slot start +0xE0 / data +0xEC) or bit 8 (0x089DE240, +0x100 / +0x104) of word(self+0x140) is grown (0x08BF1080 in
// place while it has room, else 0x08BF1768) and the pairs are copied in as two floats each.
void copy_pairs(Runtime &rt, AllegrexContext &ctx, u32 slot, u32 data, u32 shift) {
    const u32 self = ctx.gpr[4];
    const u32 source_in = ctx.gpr[5];
    const u32 count = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    const u32 member = self + ((word(rt, self + 0x140u) >> shift) & 1u) * 12u;
    const u32 begin = word(rt, member + data);
    const u32 end = word(rt, member + data + 4u);
    const u32 room = static_cast<u32>(static_cast<s32>(end - begin) >> 3);
    u32 answer;
    if (count < room) {
        answer = call(rt, ctx, 0x08BF1080u, {member + data, begin + count * 8u});
    } else {
        answer = call(rt, ctx, 0x08BF1768u, {member + data, end, count - room, frame.at(0u)});
    }
    if (count != 0u) {
        u32 source = source_in;
        for (u32 i = 0u; i < count; ++i) {
            const u32 target = word(rt, member + slot) + i * 8u;
            put_float(rt, target, float_at(rt, source));
            put_float(rt, target + 4u, float_at(rt, source + 4u));
            source += 8u;
            answer = member;
        }
    }
    ctx.gpr[2] = answer;
}

// 0x08AA6824 (a0 = self): base constructor 0x08A71E28; the list head at +0x14 / +0x10 points at self+8, +8 / +0xC /
// +0x18 zero, +0x1C / +0x20 / +0x24 zero, then a 4-word block from 0x08A5BC20(0, 4) is stored at +0x1C and also at +0x20
// / +0x24 and 0x08C30E0C(block, 0, &0x08D74FDC, &byte) runs; unless the 0x08C30E0C answer is the 0x08D74FD0 sentinel,
// 0x08B60804(word-4, -1) and (when it is not positive) 0x08B8542C(word-0xC) follow. +0x28 = 0xC and v0 = word(self+0x1C),
// which also lands at +0x20.

// 0x08B01070 (a0 = self, a1 = container): walks the container's 84-byte records from the signed half at self+4, each
// record's signed half at +0x52 (negative = none) naming the next, and 0x08B014BC(container, record, 84 * next, next)
// rewriting it. Running out sets self+4 = -1 and v0 = -1; an index below zero answers that index and writes nothing.
void func_08B01070(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 container = ctx.gpr[5];
    GuestFrame frame(ctx, 0x50u);
    const s32 first = signed_half(rt, self + 4u);
    if (first < 0) {
        ctx.gpr[2] = static_cast<u32>(first);
        return;
    }
    u32 record = word(rt, container + 0x60u) + 84u * static_cast<u32>(first);
    while (record != 0u) {
        const s32 next = signed_half(rt, record + 0x52u);
        u32 following = 0u;
        if (next >= 0) following = word(rt, container + 0x60u) + 84u * static_cast<u32>(next);
        call(rt, ctx, 0x08B014BCu, {container, record, 84u * static_cast<u32>(next), static_cast<u32>(next)});
        record = following;
    }
    ctx.gpr[2] = 0xFFFFFFFFu;
    put_half(rt, self + 4u, 0xFFFFFFFFu);
}

// 0x0886057C (a0 = self, a1 = a, a2 = 80-byte stride index, a3 = a): the entry at word(self) + 0x4120 + 80 * a2, when its
// +0x18 byte is clear, takes `a1` at +0x2C (before the call) and is handed to 0x088975C8(56 * its +0x1E half), whose
// answer sizes a stack buffer that 0x088975D0(a3, buffer) fills; 0x088D7658(word(word(self)+0x5AC), a1, a2, buffer, that
// answer) then runs.
void func_0886057C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 stride = ctx.gpr[6];
    const u32 a3 = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    const u32 owner = word(rt, self);
    const u32 entry = owner + 0x4120u + stride * 80u;
    if (rt.memory().load8(entry + 0x18u) != 0u) {
        ctx.gpr[2] = entry;
        return;
    }
    const s32 index = signed_half(rt, entry + 0x1Eu);
    const u32 record = word(rt, word(rt, owner + 0x3F50u)) + 392u * static_cast<u32>(index);
    put(rt, record + 0x2Cu, a1);
    const u32 made = call(rt, ctx, 0x088975C8u, {56u * static_cast<u32>(index)});
    GuestFrame scratch(ctx, (made + 0x1Eu) & ~0xFu);
    call(rt, ctx, 0x088975D0u, {a3, scratch.at(0u)});
    const u32 holder = word(rt, word(rt, self) + 0x5ACu);
    ctx.gpr[2] = call(rt, ctx, 0x088D7658u, {holder, a1, stride, scratch.at(0u), made});
}

// 0x0886AC1C (a0 = self, a1 = text, a2 = value): vtable 0x08CBF710 at +0, 0x08B93A24(self+4, self), the value at +0x1C,
// 0x0887C7A0(self+0x64, self), byte +0x20 = 0, three zero words from +0x10 and eight from +0x24; a non-empty text goes to
// 0x08B7C830(self+0x44, text, 0x1F) with byte +0x63 = 0, else 0x08B37640(self+0x44, 0x08C9F65C, the global at 0x08CE2E28)
// runs and that global counts up.
void func_0886AC1C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 text = ctx.gpr[5];
    const u32 value = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    put(rt, self, 0x08CBF710u);
    call(rt, ctx, 0x08B93A24u, {self + 4u, self});
    put(rt, self + 0x1Cu, value);
    put_byte(rt, self + 0x20u, 0u);
    call(rt, ctx, 0x0887C7A0u, {self + 0x64u, self});
    for (u32 i = 0u; i < 3u; ++i) put(rt, self + 0x10u + i * 4u, 0u);
    for (u32 i = 0u; i < 8u; ++i) put(rt, self + 0x24u + i * 4u, 0u);
    if (text != 0u && signed_byte(rt, text) != 0) {
        ctx.gpr[2] = call(rt, ctx, 0x08B7C830u, {self + 0x44u, text, 0x1Fu});
        put_byte(rt, self + 0x63u, 0u);
    } else {
        ctx.gpr[2] = call(rt, ctx, 0x08B37640u, {self + 0x44u, 0x08C9F65Cu, word(rt, 0x08CE2E28u)});
        put(rt, 0x08CE2E28u, word(rt, 0x08CE2E28u) + 1u);
    }
}

// 0x088F9888 (a0 = self, a1 = value): vtable 0x08CC3F08 at +0, +4 = 0, a 0x18-byte block (vtable 0x08CC3FD8 +0, 0x08CC4004
// +4, zeros through +0x14) at +8; the guarded global 0x08D76140 / 0x08D7614C (vtable 0x08CC4090) goes to word(self+8)+4
// and `value` to word(self+8)+0x10. v0 = word(self+8).

// 0x08ACF920 (a0 = self): base constructor 0x08ABCEB4 (registers as they arrive), vtable 0x08CD6B38 at +0, two 5-word
// records zeroed from +0x8C and +0xA4, 0x08B1AFB8(self+0xB8), two 0x08AFB4B4 records from self+0xFC and two more from
// self+0x2A4 (0xD4 apart), 0x08AFB4B4(self+0x44C), 0x08B1BC98(self+0x528) and 0x08B1BC98(self+0x5F8); +0xC = 0x8000,
// +0xF0 = 0x3E80, +0xF4 = 0, +0xF8 = 0. v0 = 0x3E80.
void func_08ACF920(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    call(rt, ctx, 0x08ABCEB4u);
    put(rt, self, 0x08CD6B38u);
    for (u32 i = 0u; i < 2u; ++i) {
        const u32 record = self + 0x8Cu + i * 0x18u;
        for (u32 j = 0u; j < 5u; ++j) put(rt, record + j * 4u, 0u);
    }
    call(rt, ctx, 0x08B1AFB8u, {self + 0xB8u});
    for (u32 i = 0u; i < 2u; ++i) call(rt, ctx, 0x08AFB4B4u, {self + 0xFCu + i * 0xD4u});
    for (u32 i = 0u; i < 2u; ++i) call(rt, ctx, 0x08AFB4B4u, {self + 0x2A4u + i * 0xD4u});
    call(rt, ctx, 0x08AFB4B4u, {self + 0x44Cu});
    call(rt, ctx, 0x08B1BC98u, {self + 0x528u});
    call(rt, ctx, 0x08B1BC98u, {self + 0x5F8u});
    put(rt, self + 0xCu, 0x8000u);
    put(rt, self + 0xF0u, 0x3E80u);
    put(rt, self + 0xF4u, 0u);
    put(rt, self + 0xF8u, 0u);
    ctx.gpr[2] = 0x3E80u;
}

// 0x08895C9C (a0 = self): a 0x44-byte block with 0x0892B440(block) and 0x0892B440(block+0x20), +0x40 = 0, stored at
// +0x1C; the guarded global 0x08D75ED8 / 0x08D75EE0 (vtable 0x08CC1F30) goes to +0x20; 0x08B1C920(self+0x28) runs and then
// +0x14 / +4 / +0 / +8 / +0xC / +0x24 / byte +0x18 zero; word(self+0x28) is copied to self+0x2C, +0x34 = 0, byte +0x30 = 0.
// v0 = word(self+0x28).
void func_08895C9C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const u32 block = call(rt, ctx, 0x08B62344u, {0x44u});
    call(rt, ctx, 0x0892B440u, {block});
    call(rt, ctx, 0x0892B440u, {block + 0x20u});
    put(rt, block + 0x40u, 0u);
    put(rt, self + 0x1Cu, block);
    if (signed_byte(rt, 0x08D75ED8u) == 0) {
        if (call(rt, ctx, 0x08B6441Cu, {0x08D75ED8u}) != 0u) {
            put(rt, 0x08D75EE0u, 0x08CC1F30u);
            call(rt, ctx, 0x08B64494u, {0x08D75ED8u});
        }
    }
    put(rt, self + 0x20u, 0x08D75EE0u);
    call(rt, ctx, 0x08B1C920u, {self + 0x28u});
    const u32 head = word(rt, self + 0x28u);
    put(rt, self + 0x14u, 0u);
    put(rt, self + 4u, 0u);
    put(rt, self, 0u);
    put(rt, self + 8u, 0u);
    put(rt, self + 0xCu, 0u);
    put(rt, self + 0x24u, 0u);
    put_byte(rt, self + 0x18u, 0u);
    put(rt, self + 0x2Cu, head);
    put(rt, self + 0x34u, 0u);
    put_byte(rt, self + 0x30u, 0u);
    ctx.gpr[2] = head;
}

// 0x08B2A9D0 / 0x08B2AEC4 (a0 = out, a1 = range holder): copies the holder's range (its +0 / +4 / +8, count =
// (word(+8) - word(+4)) >> 2) into a fresh block: out+4 / +8 = the block, out+0xC its end, out+0 the holder's first word,
// 0x08C503D4(block, count, &0, &byte) runs and the words are copied; v0 = the word the copy's delay slot last read (one
// past the copied range).
void copy_range(Runtime &rt, AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    put(rt, out, word(rt, source));
    put(rt, out + 4u, 0u);
    put(rt, out + 8u, 0u);
    put(rt, out + 0xCu, 0u);
    const u32 begin = word(rt, source + 4u);
    const u32 end = word(rt, source + 8u);
    const u32 count = static_cast<u32>(static_cast<s32>(end - begin) >> 2);
    if (0x3FFFFFFFu < count) call(rt, ctx, 0x08B5A9C8u); // length_error: never returns
    const u32 bytes = count << 2;
    const u32 block = call(rt, ctx, 0x08B62344u, {bytes});
    put(rt, out + 0xCu, block + bytes);
    put(rt, out + 4u, block);
    put(rt, out + 8u, block);
    put(rt, frame.at(4u), 0u);
    call(rt, ctx, 0x08C503D4u, {block, count, frame.at(4u), frame.at(0u)});
    u32 dest = word(rt, out + 4u);
    u32 src = word(rt, source + 4u);
    const u32 last = dest + bytes - 4u;
    while (!(last < dest)) {
        const u32 value = word(rt, src);
        src += 4u;
        put(rt, dest, value);
        dest += 4u;
    }
    ctx.gpr[2] = word(rt, src);
}

} // namespace
} // namespace new_batch_main_1591
} // namespace gt::game

#define NEW1591(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1591::name, gt::ReturnKind::kind)
NEW1591(0x08ADCDE8u, func_08ADCDE8, Word);
NEW1591(0x0886E888u, func_0886E888, Word);
NEW1591(0x0886F51Cu, func_0886F51C, Word);
NEW1591(0x0888A6ACu, func_0888A6AC, Word);
NEW1591(0x0888A78Cu, func_0888A78C, Word);
NEW1591(0x08AE11A0u, func_08AE11A0, Word);
NEW1591(0x08AE1338u, func_08AE1338, Word);
NEW1591(0x08AE160Cu, func_08AE160C, Word);
NEW1591(0x08B29CBCu, func_08B29CBC, Word);
NEW1591(0x088DECFCu, func_088DECFC, Word);
NEW1591(0x08917D38u, func_08917D38, Word);
NEW1591(0x08B01070u, func_08B01070, Word);
NEW1591(0x0886057Cu, func_0886057C, Word);
NEW1591(0x0886AC1Cu, func_0886AC1C, Word);
NEW1591(0x08ACF920u, func_08ACF920, Word);
NEW1591(0x08895C9Cu, func_08895C9C, Word);
