// Hot batch 335: functions no route reaches (loops, several exits, tail calls, calls through registers; integer code only),
// drafted from `tools.disasm` with a register-level emitter (tools/batch_emit.py: one statement per instruction, labels and
// gotos for branches), read against the asm and verified by synthetic cases.
// Fifteen of them (0x08AC8E2C, 0x08B27AE8, 0x08A3ED98, 0x08993304, 0x089306DC, 0x08ABE3A4, 0x08B52F58, 0x08875AD8, 0x08940328, 0x089D7300, 0x08B3772C, 0x08A0D214,
// 0x08A77D80, 0x0884125C, 0x089AF334) are rewritten as structured C from the disassembly (structured rewrites, batch 75); the others keep their emitter bodies.
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <utility>

namespace gt::game {
namespace hot_batch_335 {
namespace {

using namespace gt::game::hot;

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
void jump(psprecomp::AllegrexContext &ctx, std::uint32_t target) { ctx.pc = target; }
// `div`: lo the quotient, hi the remainder, both toward zero; INT_MIN / -1 gives INT_MIN and 0 (no host trap), and a
// zero divisor leaves hi = the dividend and lo = -1 (dividend >= 0) or 1, as Allegrex does.
void divide(psprecomp::AllegrexContext &ctx, std::uint32_t dividend, std::uint32_t divisor) {
    const auto a = static_cast<std::int32_t>(dividend);
    const auto b = static_cast<std::int32_t>(divisor);
    if (b == 0) { ctx.lo = a >= 0 ? 0xFFFFFFFFu : 1u; ctx.hi = dividend; return; }
    if (b == -1) { ctx.lo = static_cast<std::uint32_t>(0u - dividend); ctx.hi = 0u; return; }
    ctx.lo = static_cast<std::uint32_t>(a / b);
    ctx.hi = static_cast<std::uint32_t>(a % b);
}
// A `jal` with the registers as they stand (the caller's frame already open); v0 / v1 / f0 back as the callee left them.
void call_here(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t target) {
    const GuestResult result = call_guest(rt, ctx, target, {}, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}

using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call with the argument registers set like the original sets them before the jal (a0-a3, t0-t3 in order): what the callee
// does not take stays as the caller had it; v0 / v1 / f0 afterwards are the callee's.
u32 call_set(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
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

// The callee-saved registers (s0-s7, fp, f20-f31) a function keeps values in, and the stores of its prologue: the rewrite stores them where the asm
// does (a callee can read the stack below its caller's frame), sets the registers as the asm does because callees can read them too, and gives
// the caller's values back when it returns.
class SavedRegisters {
public:
    SavedRegisters(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, GuestFrame &frame, std::initializer_list<SavedSlot> slots) : ctx_(ctx) {
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
    psprecomp::AllegrexContext &ctx_;
    u32 gpr_[8];
    u32 fp_;
    float fpr_[12];
};

u32 signed_byte(psprecomp::Runtime &rt, u32 address) {
    return static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(rt.memory().load8(address))));
}
u32 signed_half(psprecomp::Runtime &rt, u32 address) {
    return static_cast<u32>(static_cast<s32>(static_cast<std::int16_t>(rt.memory().load16(address))));
}

// 0x08875AD8 (a0 = the object): reads text lines with the tokenizer on the frame (0x08AB85F0 / 0x08AB8430(tokenizer, 1) fill the line buffer at frame + 0x24
// through 0x08B7B9AC until a ';' sits in the tokenizer's byte +0xC or it has no more text) and acts on each non-empty line. A second string on the frame (+0x14,
// 0x08AB85F0 of the line) gives its first character through 0x08AB856C: an empty line or a '\n' ends the pass with status 0, a '#' too; otherwise the lookup
// 0x08875330(object) finds a member-pointer record (word +4 = function or vtable offset, word +8 = adjustment * 2 + the virtual flag): none gives status 1, one is
// called with (object + adjustment, the 0x08AB8594 text or the default 0x08C891EC) and gives status 2. Both strings release their buffer afterwards
// (0x08B60804(buffer - 4, -1) not positive: 0x08B93554(buffer - 0xC)) unless it is the shared empty one (0x08CB6054 for the line, the word
// 0x08D75298 when no line was read). Status 2 reads the next line; status 0 ends with v0 = 1 and status 1 with v0 = 0; the function returns v0 = that.
void func_08875AD8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x860u);
    SavedRegisters saved(rt, ctx, frame, {{20u, 0x840u}, {30u, 0x850u}, {23u, 0x84Cu}, {22u, 0x848u}, {21u, 0x844u}, {18u, 0x838u}, {31u, 0x854u}, {19u, 0x83Cu}, {17u, 0x834u}, {16u, 0x830u}});
    const u32 reader = frame.at(0x4u);
    const u32 text = frame.at(0x14u);
    const u32 line = frame.at(0x24u);
    const auto release = [&](u32 holder, u32 shared, u32 spare) {
        const u32 buffer = word(rt, holder);
        if (buffer - 0xCu == shared) return;
        if (static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {buffer - 4u, 0xFFFFFFFFu})) > 0) return;
        call_set(rt, ctx, 0x08B93554u, {buffer - 0xCu, spare});
    };
    call_set(rt, ctx, 0x08AB85F0u, {reader});
    rt.memory().store8(line, 0u);
    u32 status;
    u32 shared;
    for (;;) {
        for (;;) {
            const u32 piece = call_set(rt, ctx, 0x08AB8430u, {reader, 1u});
            if (piece == 0u) {
                ctx.gpr[4] = line;
                ctx.gpr[5] = piece;
                break;
            }
            call_set(rt, ctx, 0x08B7B9ACu, {line, piece});
            call_set(rt, ctx, 0x08B7B9ACu, {line, 0x08C9FFB0u});
            if (signed_byte(rt, reader + 0xCu) == 0x3Bu) break;
        }
        if (signed_byte(rt, line) == 0u) {
            shared = 0x08D75298u;
            status = 1u;
            break;
        }
        call_set(rt, ctx, 0x08AB85F0u, {text, line});
        const u32 start = call_set(rt, ctx, 0x08AB856Cu, {text});
        const u32 lead = signed_byte(rt, start);
        status = 0u;
        if (lead != 0u && lead != 0xAu && lead != 0x23u) {
            const u32 member = call_set(rt, ctx, 0x08875330u, {object, start});
            status = 1u;
            if (member != 0u) {
                const u32 name = call_set(rt, ctx, 0x08AB8594u, {text});
                const u32 adjustment = word(rt, member + 8u);
                u32 target = word(rt, member + 4u);
                if ((adjustment & 1u) != 0u) target = word(rt, word(rt, object + (static_cast<s32>(adjustment) >> 1)) + target);
                ctx.gpr[5] = name;
                call_indirect(rt, ctx, target, {object + (static_cast<s32>(adjustment) >> 1), name != 0u ? name : 0x08C891ECu});
                status = 2u;
            }
        }
        shared = word(rt, 0x08CB6054u);
        release(text, shared, frame.at(1u));
        if (status == 0u) {
            status = 1u;
            break;
        }
        if (status != 2u) {
            status = 0u;
            break;
        }
        status = 0u;
        rt.memory().store8(line, 0u);
    }
    release(reader, shared, frame.at(0u));
    ctx.gpr[2] = status;
}
// 0x089306DC (a0 = index, a1 = flag; v0 = 1): the table at 0x08CF297C holds, per index (4 bytes), an object created on first use: 0x08B62344(0x64) is handed to
// 0x08938554, stored, and 0x089363FC gives the name source (+0xC); with the name 0x08CDDE38[index] a text buffer (sizes of both 0x08B35F60 lengths + 0x23,
// 16-aligned, on the stack) is formatted with 0x08B3767C(buffer, length + 5, 0x08CA1F20, name, other) and passed to 0x08938BEC(object, buffer). With a flag
// byte of 1 and the table at 0x08CF296C empty for the index, a second object (0x08B62344(0xC) made by 0x08937E04) is stored there, the buffer formatted with
// 0x08CA1F2C and passed to 0x08937EBC(object, buffer).
void func_089306DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 slot_offset = ctx.gpr[4] << 2;
    const u32 flag = ctx.gpr[5] & 0xFFu;
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{18u, 0x18u}, {30u, 0x30u}, {20u, 0x20u}, {17u, 0x14u}, {31u, 0x34u}, {23u, 0x2Cu}, {22u, 0x28u}, {21u, 0x24u}, {19u, 0x1Cu}, {16u, 0x10u}});
    ctx.gpr[2] = 0x1u;
    const u32 primary = slot_offset + 0x08CF297Cu;
    if (word(rt, primary) != 0u) return;
    u32 object = call_set(rt, ctx, 0x08B62344u, {0x64u});
    call_set(rt, ctx, 0x08938554u, {object});
    put(rt, primary, object);
    const u32 source = call_set(rt, ctx, 0x089363FCu, {});
    const u32 name = word(rt, slot_offset + 0x08CDDE38u);
    const u32 other = source + 0xCu;
    call_set(rt, ctx, 0x08B35F60u, {other});
    object = ctx.gpr[2];
    call_set(rt, ctx, 0x08B35F60u, {name});
    const u32 length = object + ctx.gpr[2];
    GuestFrame buffer_frame(ctx, ins(length + 0x23u, 0u, 0u, 4u));
    const u32 buffer = buffer_frame.at(0u);
    call_set(rt, ctx, 0x08B3767Cu, {buffer, length + 5u, 0x08CA1F20u, other, name});
    call_set(rt, ctx, 0x08938BECu, {word(rt, primary), buffer});
    const u32 secondary = slot_offset + 0x08CF296Cu;
    if (flag != 1u || word(rt, secondary) != 0u) {
        ctx.gpr[2] = 1u;
        return;
    }
    const u32 created = call_set(rt, ctx, 0x08B62344u, {0xCu});
    call_set(rt, ctx, 0x08937E04u, {created});
    put(rt, secondary, created);
    call_set(rt, ctx, 0x08B3767Cu, {buffer, length + 5u, 0x08CA1F2Cu, other, name});
    call_set(rt, ctx, 0x08937EBCu, {word(rt, secondary), buffer});
    ctx.gpr[2] = 1u;
}
// 0x08940328 (a0 = the record writer, a1 = the source data, a2 = the owner): reads the signed bytes +6 / +3 / +2 of the source (s6, s4, s5) and the words +8 / +0
// of the writer (the target s3 and the object s2), then s6 (and its copy s7) is cleared when 0x0895B58C(owner) is nonzero, and s4 and s5 (and the flag
// for 0x089448AC argument 3) when 0x0894D050(object) is. Six calls 0x089448AC(writer, n, ...) set the fields 3 (flag), 1 (owner byte +0x1C4), 2 (0x08944418(owner)), 5, 6 and 4
// (owner bytes +0x1AC, +0x1AD, +0x1AE). When the target byte +3 is zero, the target byte +7 is rebuilt: 1 for s5, |4 for s4, |2 by the mode byte at 0x08CDE685
// (2: s4 set, 1: s5 set, 3: always, 4: 0x089499F8(owner) nonzero), then |8 for s6 (|0x48 when it is 1) and |0x10 (|0x50 when it is 1) for the copy s7, which always equals s6. v0 is as the
// original leaves it: the last call's, 1, the mode constants or the last byte stored.
void func_08940328(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 writer = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    const u32 owner = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{23u, 0x1Cu}, {17u, 0x4u}, {16u, 0x0u}, {31u, 0x20u}, {22u, 0x18u}, {21u, 0x14u}, {20u, 0x10u}, {19u, 0xCu}, {18u, 0x8u}});
    u32 byte6 = signed_byte(rt, source + 6u);
    const u32 target = word(rt, writer + 8u);
    const u32 object = word(rt, writer);
    u32 byte3 = signed_byte(rt, source + 3u);
    u32 byte2 = signed_byte(rt, source + 2u);
    if (call_set(rt, ctx, 0x0895B58Cu, {owner}) != 0u) {
        byte6 = 0u;
    }
    u32 flag = byte3 != 0u ? 1u : 0u;
    if (call_set(rt, ctx, 0x0894D050u, {object}) != 0u) {
        flag = 0u;
        byte3 = 0u;
        byte2 = 0u;
    }
    call_set(rt, ctx, 0x089448ACu, {writer, 3u, flag});
    call_set(rt, ctx, 0x089448ACu, {writer, 1u, signed_byte(rt, owner + 0x1C4u)});
    const u32 count = call_set(rt, ctx, 0x08944418u, {owner});
    call_set(rt, ctx, 0x089448ACu, {writer, 2u, count});
    call_set(rt, ctx, 0x089448ACu, {writer, 5u, signed_byte(rt, owner + 0x1ACu)});
    call_set(rt, ctx, 0x089448ACu, {writer, 6u, signed_byte(rt, owner + 0x1ADu)});
    call_set(rt, ctx, 0x089448ACu, {writer, 4u, signed_byte(rt, owner + 0x1AEu)});
    const u32 flags_address = target + 7u;
    const u32 locked = signed_byte(rt, target + 3u);
    rt.memory().store8(flags_address, 0u);
    if (locked != 0u) return;
    ctx.gpr[2] = 1u;
    if (byte2 != 0u) rt.memory().store8(flags_address, 1u);
    if (byte3 != 0u) {
        ctx.gpr[2] = rt.memory().load8(flags_address) | 4u;
        rt.memory().store8(flags_address, static_cast<std::uint8_t>(ctx.gpr[2]));
    }
    const u32 mode = rt.memory().load8(0x08CDE685u);
    bool set_bit = false;
    ctx.gpr[2] = high_half(0x08CDE685u);
    switch (mode) {
    case 2u:
        ctx.gpr[2] = 1u;
        set_bit = byte3 != 0u;
        break;
    case 0u:
    case 1u:
        ctx.gpr[2] = 1u;
        set_bit = mode == 1u && byte2 != 0u;
        break;
    case 3u:
        ctx.gpr[2] = 4u;
        set_bit = true;
        break;
    case 4u:
        ctx.gpr[2] = 4u;
        set_bit = call_set(rt, ctx, 0x089499F8u, {owner}) != 0u;
        break;
    default:
        ctx.gpr[2] = 4u;
        break;
    }
    if (set_bit) {
        ctx.gpr[2] = rt.memory().load8(flags_address) | 2u;
        rt.memory().store8(flags_address, static_cast<std::uint8_t>(ctx.gpr[2]));
    }
    if (byte6 != 0u) {
        const u32 before = rt.memory().load8(flags_address);
        ctx.gpr[2] = before | 8u;
        rt.memory().store8(flags_address, static_cast<std::uint8_t>(ctx.gpr[2]));
        if (byte6 == 1u) {
            ctx.gpr[2] = before | 0x48u;
            rt.memory().store8(flags_address, static_cast<std::uint8_t>(ctx.gpr[2]));
        }
    }
    if (byte6 != 0u) {
        const u32 before = rt.memory().load8(flags_address);
        ctx.gpr[2] = before | 0x10u;
        rt.memory().store8(flags_address, static_cast<std::uint8_t>(ctx.gpr[2]));
        if (byte6 == 1u) {
            ctx.gpr[2] = before | 0x50u;
            rt.memory().store8(flags_address, static_cast<std::uint8_t>(ctx.gpr[2]));
        }
    }
}
// 0x08993304 (a0 = mode, a1 = value): mode 1 with value 0xFFFF registers 0x08A92114(0x08CF43E0, 0x08CA40C0, 0x150, 0x08993248, 0x089940B0, 0x0899326C), runs
// 0x08B28DCC(0x08CF442C) and 0x0897D534(0x08CF43D0, 0x08CA40C0, 0x08994274, 0x08994230), then three 0x08BF0984(name, 0x08CA40xx, pair on the frame, 0x08CF43D0)
// calls whose pairs are copied from the words at 0x08C8C184, 0x08C8C17C and 0x08C8C174 (two words each); v0 is the last call's. Mode 0 with 0xFFFF ends with
// the tail call 0x08B28DC4(0x08CF442C), v0 its result. Other modes leave v0 = 1 (mode 1 with another value: 0xFFFF; mode 0 with another value: 0xFFFF).
void func_08993304(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 mode = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    bool tail = false;
    {
        GuestFrame frame(ctx, 0x30u);
        SavedRegisters saved(rt, ctx, frame, {{31u, 0x28u}, {17u, 0x24u}, {16u, 0x20u}});
        ctx.gpr[2] = 1u;
        if (mode == 1u) {
            ctx.gpr[2] = 0xFFFFu;
            if (value != 0xFFFFu) return;
            call_set(rt, ctx, 0x08A92114u, {0x08CF43E0u, 0x08CA40C0u, 0x150u, 0x08993248u, 0x089940B0u, 0x0899326Cu});
            call_set(rt, ctx, 0x08B28DCCu, {0x08CF442Cu});
            call_set(rt, ctx, 0x0897D534u, {0x08CF43D0u, 0x08CA40C0u, 0x08994274u, 0x08994230u});
            struct Pair {
                u32 source;
                u32 slot;
                u32 name;
                u32 label;
            };
            for (const Pair &pair : {Pair{0x08C8C184u, 0x10u, 0x08CF441Cu, 0x08CA40CCu}, Pair{0x08C8C17Cu, 0x8u, 0x08CF440Cu, 0x08CA40D8u},
                                     Pair{0x08C8C174u, 0x0u, 0x08CF43FCu, 0x08CA40E0u}}) {
                const u32 first = word(rt, pair.source);
                const u32 second = word(rt, pair.source + 4u);
                put(rt, frame.at(pair.slot), first);
                put(rt, frame.at(pair.slot + 4u), second);
                call_set(rt, ctx, 0x08BF0984u, {pair.name, pair.label, frame.at(pair.slot), 0x08CF43D0u, first, second});
            }
            return;
        }
        if (mode != 0u) return;
        ctx.gpr[2] = 0xFFFFu;
        if (value != 0xFFFFu) return;
        tail = true;
    }
    if (tail) call_set(rt, ctx, 0x08B28DC4u, {0x08CF442Cu});
}
// 0x089D7300 (a0 = mode, a1 = value): v0 = 1 for any mode but 1, 0xFFFF for mode 1 with another value. Mode 1 with 0xFFFF registers 0x08B2095C(0x08CF5268, 0x08C8CFE8, 0x29)
// and 0x08B2095C(0x08CF5260, 0x08C8CFD8, 2), 0x0897D534(0x08CF51CC, 0x08CA70D4, 0x089D7D98, 0x089D7D54), then eight 0x08C044A0(name, label, pair on the frame,
// 0x08CF51CC) calls whose pairs are the word pairs at 0x08C8D16C, 0x08C8D164, ... 0x08C8D134 (slot 0x38 down to 0); v0 is the last call's.
void func_089D7300(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 mode = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    GuestFrame frame(ctx, 0x50u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x44u}, {16u, 0x40u}});
    ctx.gpr[2] = 1u;
    if (mode != 1u) return;
    ctx.gpr[2] = 0xFFFFu;
    if (value != 0xFFFFu) return;
    call_set(rt, ctx, 0x08B2095Cu, {0x08CF5268u, 0x08C8CFE8u, 0x29u});
    call_set(rt, ctx, 0x08B2095Cu, {0x08CF5260u, 0x08C8CFD8u, 2u});
    call_set(rt, ctx, 0x0897D534u, {0x08CF51CCu, 0x08CA70D4u, 0x089D7D98u, 0x089D7D54u});
    struct Entry {
        u32 source;
        u32 slot;
        u32 name;
        u32 label;
    };
    const Entry entries[] = {{0x08C8D16Cu, 0x38u, 0x08CF5250u, 0x08CA70ECu}, {0x08C8D164u, 0x30u, 0x08CF5240u, 0x08CA70FCu},
                             {0x08C8D15Cu, 0x28u, 0x08CF5230u, 0x08CA7110u}, {0x08C8D154u, 0x20u, 0x08CF5220u, 0x08CA7124u},
                             {0x08C8D14Cu, 0x18u, 0x08CF5210u, 0x08CA7134u}, {0x08C8D144u, 0x10u, 0x08CF5200u, 0x08CA7144u},
                             {0x08C8D13Cu, 0x08u, 0x08CF51F0u, 0x08CA715Cu}, {0x08C8D134u, 0x00u, 0x08CF51E0u, 0x08CA7164u}};
    for (const Entry &entry : entries) {
        const u32 first = word(rt, entry.source);
        const u32 second = word(rt, entry.source + 4u);
        put(rt, frame.at(entry.slot), first);
        put(rt, frame.at(entry.slot + 4u), second);
        call_set(rt, ctx, 0x08C044A0u, {entry.name, entry.label, frame.at(entry.slot), 0x08CF51CCu, first, second});
    }
}
// 0x08A3ED98 (a0 = mode, a1 = value): v0 = 0xFFFF unless the 0xFFFF value picks an action. Mode 1 with 0xFFFF: four registrations 0x08A92114(target, name, size,
// three function addresses) for the sizes 0x98, 0x38, 0x108 and 0x10C, then 0x08B85EB8(0x08CF6848, 0x08C8E804, frame + 1); v0 is its result. Mode 0 with 0xFFFF:
// the object word 0x08CF6848 - 0xC, unless it is 0x08D74FD0, is released with 0x08B8542C(object, frame) when 0x08B60804(word - 4, -1) is not positive.
void func_08A3ED98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 mode = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x14u}, {16u, 0x10u}});
    ctx.gpr[2] = 0xFFFFu;
    if (value != 0xFFFFu || mode > 1u) return;
    if (mode == 1u) {
        call_set(rt, ctx, 0x08A92114u, {0x08CF682Cu, 0x08CADA04u, 0x98u, 0x08A3E9B0u, 0x08A40940u, 0x08A3E9D4u});
        call_set(rt, ctx, 0x08A92114u, {0x08CF6814u, 0x08CADA18u, 0x38u, 0x08A3EA68u, 0x08A40850u, 0x08A3EA8Cu});
        call_set(rt, ctx, 0x08A92114u, {0x08CF67FCu, 0x08CADA2Cu, 0x108u, 0x08A3EB20u, 0x08A40640u, 0x08A3EB44u});
        call_set(rt, ctx, 0x08A92114u, {0x08CF67E4u, 0x08CADA3Cu, 0x10Cu, 0x08A3EBE0u, 0x08A40518u, 0x08A3EC04u});
        call_set(rt, ctx, 0x08B85EB8u, {0x08CF6848u, 0x08C8E804u, frame.at(1u)});
        return;
    }
    const u32 word_value = word(rt, 0x08CF6848u);
    const u32 object = word_value - 0xCu;
    ctx.gpr[2] = 0x08D74FD0u;
    ctx.gpr[5] = 0xFFFFFFFFu;
    if (object == 0x08D74FD0u) return;
    const u32 count = call_set(rt, ctx, 0x08B60804u, {word_value - 4u, 0xFFFFFFFFu});
    if (static_cast<s32>(count) > 0) return;
    call_set(rt, ctx, 0x08B8542Cu, {object, frame.at(0u)});
}
// 0x08ABE3A4 (a0, a1, a2 = group size, a3 = decimals, t0 = output buffer, t1 = its size): formats three values into the buffer with 0x08B3767C(buffer, size,
// 0x08CB467C, a3, a0, a1), then rewrites the text in a 0x80-byte scratch area on the frame, filling it from its end (position 0x7E down, the byte at 0x7F
// is 0): the characters are copied from the last to the first; before each copy that is not the first one and is not preceded by a '-', a '.' is inserted
// when the digits copied so far equal the decimals (a3, nonzero) or, with a group size (a2) other than 0, a ',' when (1 - decimals + the copy index) is a multiple of
// the group size. A decimals count not below the length then pads with '0' characters and "0." in front; the scratch text from the final position goes
// back to the buffer with 0x08B7C830(buffer, text, size) and v0 is its result.
void func_08ABE3A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 first = ctx.gpr[4];
    const u32 second = ctx.gpr[5];
    const s32 group = static_cast<s32>(ctx.gpr[6]);
    const s32 decimals = static_cast<s32>(ctx.gpr[7]);
    const u32 buffer = ctx.gpr[8];
    const u32 size = ctx.gpr[9];
    GuestFrame frame(ctx, 0xA0u);
    SavedRegisters saved(rt, ctx, frame, {{19u, 0x8Cu}, {18u, 0x88u}, {17u, 0x84u}, {31u, 0x90u}, {16u, 0x80u}});
    call_set(rt, ctx, 0x08B3767Cu, {buffer, size, 0x08CB467Cu, static_cast<u32>(decimals), first, second});
    const s32 length = static_cast<s32>(call_set(rt, ctx, 0x08B35F60u, {buffer}));
    rt.memory().store8(frame.at(0x7Fu), 0u);
    s32 position = 0x7E;
    if (length > 0) {
        u32 source = buffer + static_cast<u32>(length) - 1u;
        s32 index = 1 - decimals;
        for (s32 copied = 1;; ++copied, --source, ++index) {
            rt.memory().store8(frame.at(static_cast<u32>(position)), rt.memory().load8(source));
            s32 next = position - 1;
            if (copied < length && static_cast<std::int8_t>(rt.memory().load8(source - 1u)) != '-') {
                if (decimals != 0 && decimals == copied) {
                    rt.memory().store8(frame.at(static_cast<u32>(next)), '.');
                    next = position - 2;
                } else if (group != 0) {
                    divide(ctx, static_cast<u32>(index), static_cast<u32>(group));
                    if (ctx.hi == 0u) {
                        rt.memory().store8(frame.at(static_cast<u32>(next)), ',');
                        next = position - 2;
                    }
                }
            }
            position = next;
            if (copied == length) break;
        }
    }
    if (decimals >= length) {
        for (s32 zeros = decimals - length; zeros > 0; --zeros) rt.memory().store8(frame.at(static_cast<u32>(position--)), '0');
        rt.memory().store8(frame.at(static_cast<u32>(position - 1)), '0');
        rt.memory().store8(frame.at(static_cast<u32>(position)), '.');
        position -= 2;
    }
    call_set(rt, ctx, 0x08B7C830u, {buffer, frame.at(static_cast<u32>(position + 1)), size});
}
// 0x08AC8E2C (a0 = mode, a1 = value): v0 = 0xFFFF unless the 0xFFFF value picks an action. Mode 1 with 0xFFFF: eight 0x20-byte entries from 0x08D00008 are
// set up with 0x08AB94C4 (each one's byte at 0x08D00018 + 0x20 * i cleared after its call), the words 0x08D00000 = 0x08D00008 and 0x08D00004 = 8 are stored,
// 0x08C3C230(0x08CFFFE4, 0x08CB48F4, 0x10) and 0x08C3CD4C(0x08CFFFC8) run, and the function ends with the tail call 0x08B28DCC(0x08CFFFA0) (made after
// the frame is gone). Mode 0 with 0xFFFF: 0x08B28DC4(0x08CFFFA0), 0x08C3CD7C(0x08CFFFC8), 0x08C3C284(0x08CFFFE4), then 0x08AB9404 on the entries from the last
// (0x08D000E8) down to the first (0x08D00008); v0 is the last call's.
void func_08AC8E2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 mode = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    bool tail = false;
    {
        GuestFrame frame(ctx, 0x20u);
        SavedRegisters saved(rt, ctx, frame, {{31u, 0x10u}, {19u, 0xCu}, {18u, 0x8u}, {17u, 0x4u}, {16u, 0x0u}});
        ctx.gpr[2] = 0xFFFFu;
        if (value != 0xFFFFu || mode > 1u) return;
        if (mode == 1u) {
            for (u32 i = 0; i < 8u; ++i) {
                call_set(rt, ctx, 0x08AB94C4u, {0x08D00008u + 0x20u * i});
                rt.memory().store8(0x08D00018u + 0x20u * i, 0u);
            }
            put(rt, 0x08D00000u + 4u, 8u);
            put(rt, 0x08D00000u, 0x08D00008u);
            call_set(rt, ctx, 0x08C3C230u, {0x08CFFFE4u, 0x08CB48F4u, 0x10u, 0x08D00000u, 0x08D00000u});
            call_set(rt, ctx, 0x08C3CD4Cu, {0x08CFFFC8u});
            tail = true;
        } else {
            call_set(rt, ctx, 0x08B28DC4u, {0x08CFFFA0u});
            call_set(rt, ctx, 0x08C3CD7Cu, {0x08CFFFC8u});
            call_set(rt, ctx, 0x08C3C284u, {0x08CFFFE4u});
            u32 entry = 0x08D00108u - 0x20u;
            for (;;) {
                call_set(rt, ctx, 0x08AB9404u, {entry});
                if (entry == 0x08D00008u) break;
                entry -= 0x20u;
            }
        }
    }
    if (tail) call_set(rt, ctx, 0x08B28DCCu, {0x08CFFFA0u});
}
// 0x08B27AE8 (a0 = the object): walks the halfword stream from self[+0x20] up to self[+0x2C]; v0 = 1 when it ends there, 0 when a check fails. Each step reads
// an index (halfword), runs 0x08B278A0(self, the halfword +0xC, the signed byte +0xE) on the 16-byte record index * 16 of the table self[+0x40] and stops at
// a zero result. An index of 0x101 or more has a flag byte in 0x08C937C0 + index - 0x101: nonzero runs 0x08B278A0(self, the next halfword, the flag as a
// signed byte) and takes the halfword after it as the second index (zero skips that call and takes the next one). The second index is looked up in the
// table self[+0x48] the same way and its flag byte in 0x08C937E0 + (index & 0xFF) gives a last call with the following halfword.
void func_08B27AE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{18u, 0x8u}, {31u, 0x20u}, {23u, 0x1Cu}, {22u, 0x18u}, {21u, 0x14u}, {20u, 0x10u}, {19u, 0xCu}, {17u, 0x4u}, {16u, 0x0u}});
    u32 cursor = word(rt, self + 0x20u);
    const u32 end = word(rt, self + 0x2Cu);
    ctx.gpr[2] = 1u;
    if (cursor == end) return;
    const auto check_record = [&](u32 table, u32 index) {
        const u32 record = word(rt, self + table) + (index << 4);
        return call_set(rt, ctx, 0x08B278A0u, {self, rt.memory().load16(record + 0xCu), signed_byte(rt, record + 0xEu)}) != 0u;
    };
    for (;;) {
        const u32 index = rt.memory().load16(cursor);
        u32 next = cursor + 2u;
        if (!check_record(0x40u, index)) return;
        if (index >= 0x101u) {
            u32 second = cursor + 2u;
            const u32 flag = rt.memory().load8(0x08C937C0u + index - 0x101u);
            if (flag != 0u) {
                if (call_set(rt, ctx, 0x08B278A0u, {self, rt.memory().load16(cursor + 2u), static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(flag)))}) == 0u) return;
                second = cursor + 4u;
            }
            const u32 second_index = rt.memory().load16(second);
            next = second + 2u;
            if (!check_record(0x48u, second_index)) return;
            const u32 second_flag = rt.memory().load8(0x08C937E0u + (second_index & 0xFFu));
            if (second_flag != 0u) {
                if (call_set(rt, ctx, 0x08B278A0u, {self, rt.memory().load16(second + 2u), static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(second_flag)))}) == 0u) return;
                next = second + 4u;
            }
        }
        if (end == next) {
            ctx.gpr[2] = 1u;
            return;
        }
        cursor = next;
    }
}
// 0x08B3772C (a0 = the tree, a1 = a node, a2 = the state; v0 = the best size found): the node's children (the list from the word +0, each child's next link in +0xC,
// its object in +4, its node index in +8) are visited. The state word +0x14 (a running position) is saved first and put back after every child. The child object's
// virtual +8 gives its kind: 2 and 3 recurse into the node tree[+4] + index * 8; when that size beats the best so far (unsigned), the child's virtual +0x1C is asked for a
// table row of the state (word +0xC, 8 bytes a row) and the row word (+0 for kind 2, +4 for kind 3) takes the distance (state[+0x14] - state[+0]) when the
// row's value is below it. Any other kind asks the object's virtual +0x10(object, state) and, when that is nonzero, recurses and keeps the larger size.
// The best starts at the word +0x14 for a node whose byte +4 is nonzero, else 0.
void func_08B3772C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 tree = ctx.gpr[4];
    const u32 node = ctx.gpr[5];
    const u32 state = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{19u, 0xCu}, {17u, 0x4u}, {31u, 0x1Cu}, {22u, 0x18u}, {21u, 0x14u}, {20u, 0x10u}, {18u, 0x8u}, {16u, 0x0u}});
    const u32 saved_position = word(rt, state + 0x14u);
    u32 best = rt.memory().load8(node + 4u) != 0u ? saved_position : 0u;
    const auto virtual_call = [&](u32 object, u32 slot, std::initializer_list<u32> extra) {
        const u32 target = word(rt, word(rt, object) + slot);
        if (extra.size() == 0u) return call_indirect(rt, ctx, target, {object});
        return call_indirect(rt, ctx, target, {object, *extra.begin()});
    };
    const auto recurse = [&](u32 child) {
        const u32 sub_node = word(rt, tree + 4u) + (word(rt, child + 8u) << 3);
        return call_set(rt, ctx, 0x08B3772Cu, {tree, sub_node, state});
    };
    // The kinds 2 and 3: recurse, and when the result beats `best` record the distance in the row's word at `column`.
    const auto account = [&](u32 child, u32 column) {
        const u32 size = recurse(child);
        if (!(best < size)) {
            put(rt, state + 0x14u, saved_position);
            return;
        }
        const u32 object = word(rt, child + 4u);
        const s32 distance = static_cast<s32>(word(rt, state + 0x14u) - word(rt, state));
        best = size;
        const u32 row = virtual_call(object, 0x1Cu, {});
        const u32 row_value = word(rt, word(rt, state + 0xCu) + (row << 3) + column);
        if (!(static_cast<s32>(row_value) < distance)) {
            put(rt, state + 0x14u, saved_position);
            return;
        }
        const u32 row_again = virtual_call(word(rt, child + 4u), 0x1Cu, {});
        put(rt, word(rt, state + 0xCu) + (row_again << 3) + column, static_cast<u32>(distance));
        put(rt, state + 0x14u, saved_position);
    };
    for (u32 child = word(rt, node); child != 0u; child = word(rt, child + 0xCu)) {
        const u32 kind = virtual_call(word(rt, child + 4u), 8u, {});
        if (kind == 2u) {
            account(child, 0u);
            continue;
        }
        const u32 kind_again = virtual_call(word(rt, child + 4u), 8u, {});
        if (kind_again == 3u) {
            account(child, 4u);
            continue;
        }
        if (virtual_call(word(rt, child + 4u), 0x10u, {state}) == 0u) {
            put(rt, state + 0x14u, saved_position);
            continue;
        }
        const u32 size = recurse(child);
        if (best < size) best = size;
        put(rt, state + 0x14u, saved_position);
    }
    ctx.gpr[2] = best;
}
// 0x08B52F58 (a0 = the object, a1 = a delta; v0 as described): the table of eight words at 0x08C955E4 is copied to the frame, then the command words at
// object[+0x10] + delta (opcode = the top byte, ended by opcode 0xB) are scanned: opcodes 0xA0-0xA7 or in the frame word (opcode - 0xA0) their low 24 bits, 0xA8-0xAF
// or the bits 16-23 shifted up by 8 into the word (opcode - 0xA8), 0xB0 / 0xB1 collect the low 24 bits / the bits 16-23 shifted up in an accumulator. Every frame
// word then gets the delta added. With a stream that starts with the end opcode v0 = 0xB and nothing more happens. Otherwise the command words are
// rewritten in a second pass: opcodes 0xA0-0xA7 keep their top byte and take the low 24 bits of the frame word, 0xA8-0xAF keep all but the bits 16-23 and
// take the frame word's bits 8-15 moved to 16-23 (as the original masks them), 0xB0 takes the low 24 bits of (accumulator + delta), 0xB1 the same sum
// shifted into the bits 16-23; other opcodes are left alone. v0 is the last word computed (stored, or for opcodes the pass ignores the 0xB1 candidate).
void func_08B52F58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 delta = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    for (u32 i = 0; i < 8u; ++i) put(rt, frame.at(4u * i), word(rt, 0x08C955E4u + 4u * i));
    const u32 stream = word(rt, object + 0x10u) + delta;
    const u32 first_opcode = word(rt, stream) >> 24;
    u32 accumulator = 0u;
    if (first_opcode != 0xBu) {
        for (u32 cursor = stream;; cursor += 4u) {
            const u32 command = word(rt, cursor);
            const u32 opcode = command >> 24;
            if (opcode == 0xBu) break;
            if (opcode - 0xA0u < 8u) {
                const u32 slot = frame.at(4u * (opcode - 0xA0u));
                put(rt, slot, word(rt, slot) | ins(command, 0u, 24u, 8u));
            } else if (opcode - 0xA8u < 8u) {
                const u32 slot = frame.at(4u * (opcode - 0xA8u));
                put(rt, slot, word(rt, slot) | ((command & 0x00FF0000u) << 8));
            } else if (opcode == 0xB0u) {
                accumulator |= ins(command, 0u, 24u, 8u);
            } else if (opcode == 0xB1u) {
                accumulator |= (command & 0x00FF0000u) << 8;
            }
        }
    }
    for (u32 i = 0; i < 8u; ++i) put(rt, frame.at(4u * i), word(rt, frame.at(4u * i)) + delta);
    ctx.gpr[2] = 0xBu;
    if (first_opcode == 0xBu) return;
    const u32 sum = accumulator + delta;
    const u32 sum_low = ins(sum, 0u, 24u, 8u);
    const u32 sum_high = (sum >> 8) & 0x00FF0000u;
    for (u32 cursor = stream;; cursor += 4u) {
        const u32 command = word(rt, cursor);
        const u32 opcode = command >> 24;
        if (opcode == 0xBu) break;
        u32 result;
        bool store = true;
        if (opcode - 0xA0u < 8u) {
            result = ins(command, 0u, 0u, 24u) | ins(word(rt, frame.at(4u * (opcode - 0xA0u))), 0u, 24u, 8u);
        } else if (opcode - 0xA8u < 8u) {
            result = ins(command, 0u, 16u, 8u) | ((word(rt, frame.at(4u * (opcode - 0xA8u))) >> 8) & 0x00FF0000u);
        } else if (opcode == 0xB0u) {
            result = ins(command, 0u, 0u, 24u) | sum_low;
        } else {
            result = ins(command, 0u, 16u, 8u) | sum_high;
            store = opcode == 0xB1u;
        }
        if (store) put(rt, cursor, result);
        ctx.gpr[2] = result;
    }
}

// 0x08A0D214 (a0 = mode, a1 = value): v0 = 1 for any mode but 1, 0xFFFF for mode 1 with another value. Mode 1 with 0xFFFF registers 0x08A92114(0x08CF6044, 0x08CA948C, 0x120,
// 0x08A0CF94, 0x08A0DF10, 0x08A0CFB8) and 0x0897D534(0x08CF6034, 0x08CA948C, 0x08A0E0D4, 0x08A0E090), then ten 0x08C14738(name, label, pair on the frame, 0x08CF6034)
// calls whose pairs are the word pairs at 0x08C8DFE8, 0x08C8DFE0, ... 0x08C8DFA0 (frame slot 0x48 down to 0); v0 is the last call's.
void func_08A0D214(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 mode = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    GuestFrame frame(ctx, 0x60u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x58u}, {17u, 0x54u}, {16u, 0x50u}});
    ctx.gpr[2] = 1u;
    if (mode != 1u) return;
    ctx.gpr[2] = 0xFFFFu;
    if (value != 0xFFFFu) return;
    call_set(rt, ctx, 0x08A92114u, {0x08CF6044u, 0x08CA948Cu, 0x120u, 0x08A0CF94u, 0x08A0DF10u, 0x08A0CFB8u});
    call_set(rt, ctx, 0x0897D534u, {0x08CF6034u, 0x08CA948Cu, 0x08A0E0D4u, 0x08A0E090u});
    struct Entry {
        u32 source;
        u32 slot;
        u32 name;
        u32 label;
    };
    const Entry entries[] = {{0x08C8DFE8u, 0x48u, 0x08CF60F0u, 0x08CA9494u}, {0x08C8DFE0u, 0x40u, 0x08CF60E0u, 0x08CA94A4u},
                             {0x08C8DFD8u, 0x38u, 0x08CF60D0u, 0x08CA94B4u}, {0x08C8DFD0u, 0x30u, 0x08CF60C0u, 0x08CA94C4u},
                             {0x08C8DFC8u, 0x28u, 0x08CF60B0u, 0x08CA94D8u}, {0x08C8DFC0u, 0x20u, 0x08CF60A0u, 0x08CA94E4u},
                             {0x08C8DFB8u, 0x18u, 0x08CF6090u, 0x08CA94F0u}, {0x08C8DFB0u, 0x10u, 0x08CF6080u, 0x08CA94F8u},
                             {0x08C8DFA8u, 0x08u, 0x08CF6070u, 0x08CA9504u}, {0x08C8DFA0u, 0x00u, 0x08CF6060u, 0x08CA9514u}};
    for (const Entry &entry : entries) {
        const u32 first = word(rt, entry.source);
        const u32 second = word(rt, entry.source + 4u);
        put(rt, frame.at(entry.slot), first);
        put(rt, frame.at(entry.slot + 4u), second);
        call_set(rt, ctx, 0x08C14738u, {entry.name, entry.label, frame.at(entry.slot), 0x08CF6034u, first, second});
    }
}
// 0x08A77D80 (a0 = the list object, a1 = a pointer to the item object pointer): the object's virtual +0x78 picks one of two walks. Nonzero: the item's virtual +0x7C
// gives a count n, the list's virtual +0x80(list, n) prepares it, and for each i below n the item's virtual +0x84(frame + 8, item, i) fills a string-like value that is
// handed to the list's virtual +0x88(list, i, value) and released with 0x08A5B9EC. Zero: when the list's virtual +0x90 is nonzero the item's virtual +0xA8(frame + 0xC, item)
// starts an iteration whose current record (the word at frame + 0xC, its member +0x14 an object) is advanced while that object's virtual +8 is nonzero: its virtual +0xC runs,
// 0x08A76ABC(frame + 0x10, member + 4) builds a second iterator and the list's virtual +0xA0(list, frame + 4, frame) is called with the two values filled by the
// item's virtual +0x84 (flag 0, then 1); the three temporaries are released with 0x08A5B9EC and the first is released at the end. A null item pointer calls
// 0x08A5BB7C(0x08CB0E38) (0x08CB0E90 for the first iterator).
void func_08A77D80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 list = ctx.gpr[4];
    const u32 item_pointer = ctx.gpr[5];
    GuestFrame frame(ctx, 0x50u);
    SavedRegisters saved(rt, ctx, frame, {{22u, 0x38u}, {21u, 0x34u}, {31u, 0x40u}, {23u, 0x3Cu}, {20u, 0x30u}, {19u, 0x2Cu}, {18u, 0x28u}, {17u, 0x24u}, {16u, 0x20u}});
    const auto virtual_call = [&](u32 object, u32 slot, std::initializer_list<u32> arguments) {
        const u32 target = word(rt, word(rt, object) + slot);
        return call_indirect(rt, ctx, target, arguments);
    };
    const auto require = [&](u32 pointer, u32 message) {
        if (pointer == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {message});
    };
    if (virtual_call(list, 0x78u, {list}) != 0u) {
        u32 item = word(rt, item_pointer);
        require(item, 0x08CB0E38u);
        const u32 count = virtual_call(item, 0x7Cu, {item});
        virtual_call(list, 0x80u, {list, count});
        if (count == 0u) return;
        const u32 value = frame.at(8u);
        u32 index = 0u;
        do {
            const u32 store = word(rt, word(rt, list) + 0x88u);
            item = word(rt, item_pointer);
            require(item, 0x08CB0E38u);
            virtual_call(item, 0x84u, {value, item, index});
            call_indirect(rt, ctx, store, {list, index, value});
            ++index;
            call_set(rt, ctx, 0x08A5B9ECu, {value});
        } while (count != index);
        return;
    }
    if (virtual_call(list, 0x90u, {list}) == 0u) return;
    u32 item = word(rt, item_pointer);
    require(item, 0x08CB0E38u);
    const u32 first = frame.at(0xCu);
    const u32 second = frame.at(0x10u);
    const u32 text = frame.at(4u);
    const u32 scratch = frame.at(0u);
    virtual_call(item, 0xA8u, {first, item});
    for (;;) {
        u32 record = word(rt, first);
        require(record, 0x08CB0E90u);
        if (virtual_call(word(rt, record + 0x14u), 8u, {word(rt, record + 0x14u)}) == 0u) {
            call_set(rt, ctx, 0x08A5B9ECu, {first});
            return;
        }
        virtual_call(word(rt, record + 0x14u), 0xCu, {word(rt, record + 0x14u)});
        record = word(rt, first);
        require(record, 0x08CB0E90u);
        call_set(rt, ctx, 0x08A76ABCu, {second, word(rt, record + 0x14u) + 4u});
        const u32 store = word(rt, word(rt, list) + 0xA0u);
        item = word(rt, second);
        require(item, 0x08CB0E38u);
        virtual_call(item, 0x84u, {text, item, 0u});
        item = word(rt, second);
        require(item, 0x08CB0E38u);
        virtual_call(item, 0x84u, {scratch, item, 1u});
        call_indirect(rt, ctx, store, {list, text, scratch});
        call_set(rt, ctx, 0x08A5B9ECu, {scratch});
        call_set(rt, ctx, 0x08A5B9ECu, {text});
        call_set(rt, ctx, 0x08A5B9ECu, {second});
    }
}

// 0x0884125C (a0 = the result list, a1 = the number of entries wanted, a2 = a pointer to the source object; v0 is the entry v0 when a1 is not positive, else the
// last call's): the source's virtual +0xC(frame + 0xC, source) fills a name string; 0x08931090 gives a count n and 0x08A98C1C(frame + 0x10, n) a list. For every index
// below n, 0x0893119C(index, frame + 0x18, 0x40) below zero skips the entry; otherwise 0x08930170(text) gives a pair and 0x08A5DEE8 / 0x0883B7F8 / 0x08B35FD8(name, ...) look it
// up: a nonzero result only releases the temporary, a zero one builds the pair again in frame + 8, stores it with 0x08A5BAD8(slot, frame word +8) into the list
// member +0x14 of the filled list (slot = member[+0x14] + found * 4) and counts it. The result list (member +0x14 .. +0x18, vector pointers) is then resized to the number of
// found entries with 0x08B85798 or 0x08B85F30(list member, +0x18 word, missing count, empty value); the name string is released as a shared buffer
// (0x08B60804(buffer - 4, -1) not positive: 0x08B8542C) unless it is the shared empty one (0x08D74FD0).
void func_0884125C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 result = ctx.gpr[4];
    const s32 wanted = static_cast<s32>(ctx.gpr[5]);
    const u32 source_pointer = ctx.gpr[6];
    GuestFrame frame(ctx, 0xA0u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x94u}, {30u, 0x90u}, {23u, 0x8Cu}, {22u, 0x88u}, {21u, 0x84u}, {20u, 0x80u}, {19u, 0x7Cu}, {18u, 0x78u}, {17u, 0x74u}, {16u, 0x70u}});
    put(rt, frame.at(0x60u), result);
    if (wanted <= 0) return;
    const u32 name = frame.at(0xCu);
    const u32 list = frame.at(0x10u);
    const u32 text = frame.at(0x18u);
    const u32 temporary = frame.at(0x14u);
    const u32 rebuilt = frame.at(8u);
    const u32 empty = frame.at(4u);
    u32 source = word(rt, source_pointer);
    if (source == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9BD48u});
    call_indirect(rt, ctx, word(rt, word(rt, source) + 0xCu), {name, source});
    const u32 entries = call_set(rt, ctx, 0x08931090u, {});
    call_set(rt, ctx, 0x08A98C1Cu, {list, entries});
    u32 holder = word(rt, list);
    u32 found = 0u;
    if (static_cast<s32>(entries) > 0) {
        u32 index = 0u;
        do {
            const u32 status = call_set(rt, ctx, 0x0893119Cu, {index, text, 0x40u});
            ++index;
            if (static_cast<s32>(status) >= 0) {
                const u32 low = call_set(rt, ctx, 0x08930170u, {text});
                const u32 high = ctx.gpr[3];
                call_set(rt, ctx, 0x08A5DEE8u, {temporary, ctx.gpr[5], low, high});
                const u32 match = call_set(rt, ctx, 0x0883B7F8u, {1u, temporary});
                const u32 lookup = call_set(rt, ctx, 0x08B35FD8u, {word(rt, name), match});
                const u32 current = word(rt, list);
                if (lookup == 0u) {
                    if (current == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9C42Cu});
                    call_set(rt, ctx, 0x08A5DEE8u, {rebuilt, ctx.gpr[5], low, high});
                    const u32 slot = word(rt, current + 0x14u) + found * 4u;
                    ++found;
                    if (slot != rebuilt) call_set(rt, ctx, 0x08A5BAD8u, {slot, word(rt, rebuilt)});
                    call_set(rt, ctx, 0x08A5B9ECu, {rebuilt});
                }
                call_set(rt, ctx, 0x08A5B9ECu, {temporary});
            }
        } while (entries != index);
        holder = word(rt, list);
    }
    if (holder == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9C42Cu});
    call_set(rt, ctx, 0x08A76AA0u, {empty});
    const u32 member = holder + 0x14u;
    const u32 first = word(rt, holder + 0x14u);
    const s32 size = static_cast<s32>(word(rt, member + 4u) - first) >> 2;
    if (found < static_cast<u32>(size)) {
        call_set(rt, ctx, 0x08B85798u, {member, first + (found << 2), word(rt, holder + 0x18u)});
    } else {
        const u32 limit = word(rt, holder + 0x18u);
        call_set(rt, ctx, 0x08B85F30u, {member, limit, found - static_cast<u32>(size), empty});
    }
    call_set(rt, ctx, 0x08A5B9ECu, {empty});
    const u32 destination = word(rt, frame.at(0x60u));
    if (destination != list) call_set(rt, ctx, 0x08A5BAD8u, {destination, word(rt, list)});
    call_set(rt, ctx, 0x08A5B9ECu, {list});
    const u32 buffer = word(rt, name);
    const u32 object = buffer - 0xCu;
    if (object == 0x08D74FD0u) return;
    if (static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {buffer - 4u, 0xFFFFFFFFu})) > 0) return;
    call_set(rt, ctx, 0x08B8542Cu, {object, frame.at(0u)});
}

// 0x089AF334 (a0 = the object, a1 = a pointer to the string pointer; v0 as the original leaves it): when the object's word +0xC0 is nonzero, 0x089DFAAC(that, frame + 0x18,
// frame + 0x1C) gives two numbers. The string is compared (0x08B36170 on the shorter length; a nonzero result, else the length difference) with four literals built by
// 0x08B85EB8 (0x08CA510C, 0x08CA511C, 0x08CA5114, 0x08CA5128), each release of a built string being the shared-buffer one (0x08B60804(buffer - 4, -1) not positive: 0x08B8542C
// unless it is the shared empty 0x08D74FD0). The first literal that does not match ends the search: a match of the first gives (0, 0), the second (number0 - 1, 0), the third
// (0, number1 - 1), the fourth (number0 - 1, number1 - 1) and none (0, 0). The pair goes into the words +0xF0 / +0xF4 when either differs, together with the byte +0xFB = byte
// +0xFA xor 1.
void func_089AF334(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 string_pointer = ctx.gpr[5];
    GuestFrame frame(ctx, 0x50u);
    SavedRegisters saved(rt, ctx, frame, {{20u, 0x40u}, {18u, 0x38u}, {31u, 0x48u}, {21u, 0x44u}, {19u, 0x3Cu}, {17u, 0x34u}, {16u, 0x30u}});
    const u32 source = word(rt, object + 0xC0u);
    if (source == 0u) return;
    call_set(rt, ctx, 0x089DFAACu, {source, frame.at(0x18u), frame.at(0x1Cu)});
    struct Literal {
        u32 text;
        u32 slot;
        u32 tag;
        u32 release_offset;
        bool swapped;
    };
    // Compares the string with one built literal: the result of 0x08B36170 when nonzero, else the length difference; the built string is released.
    const auto compare = [&](const Literal &literal) {
        call_set(rt, ctx, 0x08B85EB8u, {frame.at(literal.slot), literal.text, frame.at(literal.tag)});
        const u32 string = word(rt, string_pointer);
        const u32 built = word(rt, frame.at(literal.slot));
        const u32 string_length = word(rt, string - 0xCu);
        const u32 length_slot = frame.at(literal.swapped ? 0x20u : 0x24u);
        const u32 built_slot = frame.at(literal.swapped ? 0x24u : 0x20u);
        put(rt, length_slot, string_length);
        const u32 header = built - 0xCu;
        const u32 built_length = word(rt, header);
        put(rt, built_slot, built_length);
        u32 difference = string_length - built_length;
        const u32 shorter = word(rt, built_length < string_length ? built_slot : length_slot);
        const u32 order = call_set(rt, ctx, 0x08B36170u, {string, built, shorter});
        if (order != 0u) difference = order;
        if (header != 0x08D74FD0u && static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {built - 4u, 0xFFFFFFFFu})) <= 0) {
            call_set(rt, ctx, 0x08B8542Cu, {header, frame.at(literal.release_offset)});
        }
        return difference;
    };
    u32 first;
    u32 second;
    if (compare({0x08CA510Cu, 0x14u, 7u, 6u, false}) == 0u) {
        first = 0u;
        second = 0u;
    } else if (compare({0x08CA511Cu, 0x10u, 5u, 4u, true}) == 0u) {
        first = word(rt, frame.at(0x18u)) - 1u;
        second = 0u;
    } else if (compare({0x08CA5114u, 0x0Cu, 3u, 2u, false}) == 0u) {
        first = 0u;
        second = word(rt, frame.at(0x1Cu)) - 1u;
    } else if (compare({0x08CA5128u, 0x08u, 1u, 0u, true}) == 0u) {
        first = word(rt, frame.at(0x18u)) - 1u;
        second = word(rt, frame.at(0x1Cu)) - 1u;
    } else {
        first = 0u;
        second = 0u;
    }
    ctx.gpr[2] = word(rt, object + 0xF0u);
    if (first == ctx.gpr[2]) {
        ctx.gpr[2] = word(rt, object + 0xF4u);
        if (second == ctx.gpr[2]) return;
    }
    ctx.gpr[2] = rt.memory().load8(object + 0xFAu);
    put(rt, object + 0xF0u, first);
    ctx.gpr[2] ^= 1u;
    rt.memory().store8(object + 0xFBu, static_cast<std::uint8_t>(ctx.gpr[2]));
    put(rt, object + 0xF4u, second);
}

} // namespace

#define G ctx.gpr
#define load8(address) rt.memory().load8(address)
#define load16(address) rt.memory().load16(address)
#define store8(address, value) rt.memory().store8((address), static_cast<std::uint8_t>(value))
#define store16(address, value) rt.memory().store16((address), static_cast<std::uint16_t>(value))

void func_0880410C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x20u); G[2] = 0x05050000u; put(rt, frame.at(0xCu), G[19]); G[19] = G[4] + 0u; G[4] = G[2] | 0x10u; put(rt, frame.at(0x8u), G[18]); G[18] = G[5] + 0u; put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); G[16] = 0x0u; put(rt, frame.at(0x10u), G[31]); G[17] = G[16] + 0x0u; call_here(rt, ctx, 0x08C7A940u); G[2] = 0x30000u; G[4] = G[2] | 0x306u; call_here(rt, ctx, 0x08C7A958u); { const bool tk = G[17] == 0u; G[9] = 0x0u; if (tk) goto L_08804180; } call_here(rt, ctx, 0x08C7A9F8u); G[8] = G[2] << 11u; G[7] = G[2] ^ G[8]; put(rt, G[16] + 0x0u, G[7]); call_here(rt, ctx, 0x08C7A9E8u); G[6] = word(rt, G[16] + 0x0u); G[5] = G[6] ^ G[2]; G[4] = G[5] ^ G[17]; put(rt, G[16] + 0x0u, G[4]); G[9] = 0x0u; L_08804180: ; G[3] = G[9] + 0x0u; { const bool tk = G[3] == 0u; G[4] = G[3] + 0u; if (tk) goto L_08804214; } L_0880418C: ; G[3] = high_half(0x08CDC59Cu); G[11] = 0x08CDC59Cu; { const bool tk = G[11] == 0u; G[6] = 0x20u; if (tk) goto L_088041A0; } G[6] = word(rt, 0x08CDC59Cu); L_088041A0: ; G[3] = high_half(0x08CDC598u); G[12] = 0x08CDC598u; { const bool tk = G[12] == 0u; G[7] = 0x40000u; if (tk) goto L_088041B8; } G[13] = word(rt, 0x08CDC598u); G[7] = G[13] << 10u; L_088041B8: ; G[5] = high_half(0x08CDC594u); G[14] = 0x08CDC594u; { const bool tk = G[14] == 0u; G[8] = 0x80000000u; if (tk) goto L_088041D4; } G[16] = word(rt, 0x08CDC594u); G[15] = 0x80000000u; G[8] = G[16] | G[15]; L_088041D4: ; G[17] = high_half(0x08804220u); G[5] = 0x08804220u; G[9] = 0u + 0u; call_here(rt, ctx, 0x08C7AA18u); G[4] = G[2] + 0u; G[5] = G[19] + 0u; G[6] = G[18] + 0u; call_here(rt, ctx, 0x08C7A9D0u); G[31] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); G[2] = 0u + 0u; goto exit_; L_08804214: ; G[10] = high_half(0x08C87C78u); G[4] = 0x08C87C78u; goto L_0880418C; goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }
// 0x0881F5CC (a0 = a table, a1 = the result or 0, a2 / a3 = two filter flags): walks the table's rows (count +0, rows of 0x22 bytes at +0x30:
// an id halfword, then 32 cells with a kind in the low nibble of their byte +2) and counts the cells that pass the filters on the id's name
// (0x0892C7C8, matched against 0x08C98F20 / 0x08C99284 by 0x08937A60). With a result, each passing cell becomes an entry (0x0884CD28) stored
// in a preallocated array (0x08A98C1C of +8 slots) or appended past it; the array is trimmed (0x08B85798) or padded (0x08B85F30) to the count
// and assigned to the result. v0 = the count.
void func_0881F5CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 table = ctx.gpr[4], result = ctx.gpr[5], want_named = ctx.gpr[6] & 0xFFu, exclude_named = ctx.gpr[7] & 0xFFu;
    GuestFrame frame(ctx, 0x60u);
    SavedRegisters saved(rt, ctx, frame, {{30u, 0x50u}, {22u, 0x48u}, {31u, 0x54u}, {23u, 0x4Cu}, {21u, 0x44u}, {20u, 0x40u}, {19u, 0x3Cu}, {18u, 0x38u}, {17u, 0x34u}, {16u, 0x30u}});
    ctx.gpr[30] = exclude_named;
    ctx.gpr[22] = table;
    const u32 capacity = word(rt, table + 0x8u);
    put(rt, frame.at(0x20u), result);
    put(rt, frame.at(0x1Cu), capacity);
    put(rt, frame.at(0x24u), want_named);
    const u32 array = frame.at(0x4u), entry = frame.at(0x8u);
    call_set(rt, ctx, 0x08A98C1Cu, {array, capacity});
    const u32 rows = word(rt, table + 0x0u);
    put(rt, frame.at(0x18u), 0u);
    put(rt, frame.at(0x14u), rows);
    auto name_matches = [&](u32 pattern, u32 name) { return call_set(rt, ctx, 0x08937A60u, {pattern, name}) != 0u; };
    if (static_cast<s32>(rows) > 0) {
        put(rt, frame.at(0x10u), 0u);
        u32 row_offset = 0u;
        for (u32 row = 0u; row != rows;) {
            ctx.gpr[23] = row_offset;
            for (u32 column = 0u; column != 0x20u; ++column) {
                ctx.gpr[21] = column;
                const u32 row_at = word(rt, table + 0x30u) + row_offset;
                const u32 kind = load8(row_at + column + 0x2u) & 0xFu;
                ctx.gpr[20] = kind;
                if (static_cast<s32>(kind) <= 0) continue;
                const u32 id = load16(row_at + 0x0u);
                ctx.gpr[18] = id;
                ctx.gpr[19] = 0u;
                bool pass = true;
                if (word(rt, frame.at(0x24u)) != 0u) {
                    const u32 name = call_set(rt, ctx, 0x0892C7C8u, {id, 0u});
                    ctx.gpr[16] = name;
                    const bool first = name_matches(0x08C98F20u, name);
                    ctx.gpr[17] = first ? 1u : 0u;
                    const bool second = name_matches(0x08C99284u, name);
                    pass = first && !second;
                }
                ctx.gpr[2] = pass ? 1u : 0u;
                if (exclude_named != 0u) {
                    const u32 name = call_set(rt, ctx, 0x0892C7C8u, {id, 0u});
                    if (name_matches(0x08C98F20u, name)) continue;
                } else if (!pass) {
                    continue;
                }
                if (word(rt, frame.at(0x20u)) != 0u) {
                    call_set(rt, ctx, 0x0884CD28u, {entry, ctx.gpr[5], id, 0u, column, kind});
                    const u32 limit = word(rt, frame.at(0x1Cu)), used = word(rt, frame.at(0x18u));
                    u32 holder = word(rt, frame.at(0x4u));
                    ctx.gpr[16] = holder;
                    if (static_cast<s32>(used) < static_cast<s32>(limit)) {
                        const u32 slot = word(rt, word(rt, frame.at(0x4u)) + 0x14u) + (used << 2);
                        ctx.gpr[2] = entry;
                        if (slot != entry) call_set(rt, ctx, 0x08A5BAD8u, {slot, word(rt, entry)});
                    } else {
                        if (holder == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C99264u});
                        const u32 vec = holder + 0x14u;
                        ctx.gpr[16] = vec;
                        const u32 finish = word(rt, vec + 0x4u);
                        if (finish == word(rt, vec + 0x8u)) {
                            call_set(rt, ctx, 0x08B8A03Cu, {vec, finish, entry});
                        } else {
                            if (finish != 0u) call_set(rt, ctx, 0x08A76ABCu, {finish, entry});
                            put(rt, vec + 0x4u, word(rt, vec + 0x4u) + 0x4u);
                        }
                    }
                    call_set(rt, ctx, 0x08A5B9ECu, {entry});
                }
                put(rt, frame.at(0x18u), word(rt, frame.at(0x18u)) + 1u);
            }
            ctx.gpr[2] = 0x20u;
            ++row;
            row_offset += 0x22u;
            put(rt, frame.at(0x10u), row);
            ctx.gpr[4] = row;
            ctx.gpr[5] = word(rt, frame.at(0x14u));
        }
    }
    const u32 target = word(rt, frame.at(0x20u));
    u32 holder = word(rt, frame.at(0x4u));
    ctx.gpr[16] = holder;
    if (target != 0u) {
        if (holder == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C99264u});
        const u32 vec = holder + 0x14u, filler = frame.at(0u);
        ctx.gpr[17] = vec;
        call_set(rt, ctx, 0x08A76AA0u, {filler});
        const u32 begin = word(rt, holder + 0x14u), finish = word(rt, vec + 0x4u), used = word(rt, frame.at(0x18u));
        const u32 size = sra(finish - begin, 2);
        if (used < size) call_set(rt, ctx, 0x08B85798u, {vec, begin + (used << 2)});
        else call_set(rt, ctx, 0x08B85F30u, {vec, finish, used - size, filler});
        call_set(rt, ctx, 0x08A5B9ECu, {filler});
        if (target != array) call_set(rt, ctx, 0x08A5BAD8u, {target, word(rt, array)});
    }
    call_set(rt, ctx, 0x08A5B9ECu, {array});
    ctx.gpr[2] = word(rt, frame.at(0x18u));
}
void func_0885AEF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x40u); G[5] = high_half(0x08C9F2E8u); put(rt, frame.at(0x2Cu), G[23]); G[5] = 0x08C9F2E8u; G[23] = G[4] + 0u; G[4] = G[29] + 0u; put(rt, frame.at(0x30u), G[31]); put(rt, frame.at(0x28u), G[22]); G[22] = frame.at(0xCu); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x20u), G[20]); G[20] = high_half(0x08C9EFE8u); put(rt, frame.at(0x1Cu), G[19]); G[19] = frame.at(0x8u); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x14u), G[17]); call_here(rt, ctx, 0x08A69B3Cu); G[4] = 0x38u; G[5] = 0x4u; call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2] + 0u; G[5] = G[29] + 0u; G[16] = G[2] + 0u; call_here(rt, ctx, 0x08A7E914u); G[5] = frame.at(0x4u); G[4] = G[22] + 0u; put(rt, frame.at(0x4u), G[16]); call_here(rt, ctx, 0x08A76AB4u); G[4] = G[29] + 0u; call_here(rt, ctx, 0x08A69D68u); G[2] = high_half(0x08CDCEC8u); G[3] = high_half(0x08CDCEE8u); G[18] = 0x08CDCEC8u; G[21] = 0x08CDCEE8u; G[17] = word(rt, frame.at(0xCu)); L_0885AF80: ; { const bool tk = G[17] != 0u; G[4] = G[20] + sext(0x08C9EFE8u, 16u); if (tk) goto L_0885AF90; } call_here(rt, ctx, 0x08A5BB7Cu); L_0885AF90: ; G[16] = word(rt, G[18] + 0x0u); G[5] = word(rt, G[18] + 0x4u); G[4] = G[19] + 0u; G[18] = G[18] + 0x8u; call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[17] + 0u; G[5] = G[16] + 0u; G[6] = G[19] + 0u; call_here(rt, ctx, 0x08B86E98u); G[4] = G[19] + 0u; call_here(rt, ctx, 0x08A5B9ECu); { const bool tk = G[18] != G[21]; G[17] = word(rt, frame.at(0xCu)); if (tk) goto L_0885AF80; } G[16] = word(rt, G[23] + 0x0u); if (G[16] != 0u) { G[4] = G[16] + 0u; goto L_0885AFE0; } G[4] = high_half(0x08C9EFE8u); G[4] = 0x08C9EFE8u; call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16] + 0u; L_0885AFE0: ; G[5] = G[22] + 0u; call_here(rt, ctx, 0x08A7CE50u); G[4] = G[22] + 0u; call_here(rt, ctx, 0x08A5B9ECu); G[31] = word(rt, frame.at(0x30u)); G[23] = word(rt, frame.at(0x2Cu)); G[22] = word(rt, frame.at(0x28u)); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); goto exit_; goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }
void func_08860C80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0xE0u); G[2] = G[5] << 6u; G[3] = G[5] << 4u; G[3] = G[3] + G[2]; put(rt, frame.at(0xCCu), G[23]); G[3] = G[3] + 0x4120u; G[23] = frame.at(0x50u); put(rt, frame.at(0xC0u), G[20]); G[20] = G[5] + 0u; G[5] = 0u + 0u; put(rt, frame.at(0xBCu), G[19]); G[19] = G[4] + 0u; G[4] = G[29] + 0u; put(rt, frame.at(0xD0u), G[30]); G[30] = frame.at(0xB0u); put(rt, frame.at(0xC8u), G[22]); G[22] = G[7] + 0u; put(rt, frame.at(0xB8u), G[18]); G[18] = G[19] + G[3]; put(rt, frame.at(0xB4u), G[17]); G[17] = G[6] + 0u; put(rt, frame.at(0xB0u), G[16]); G[16] = G[23] + 0u; put(rt, frame.at(0xD4u), G[31]); put(rt, frame.at(0xC4u), G[21]); call_here(rt, ctx, 0x088A79ACu); G[4] = G[16] + 0u; L_08860CEC: ; G[16] = G[16] + 0x30u; call_here(rt, ctx, 0x08897B8Cu); { const bool tk = G[30] != G[16]; G[4] = G[16] + 0u; if (tk) goto L_08860CEC; } G[4] = word(rt, G[18] + 0x44u); G[5] = word(rt, G[18] + 0x48u); G[21] = frame.at(0x18u); G[6] = G[21] + 0u; G[7] = G[29] + 0u; G[8] = G[23] + 0u; put(rt, frame.at(0x8u), 0u); put(rt, frame.at(0xCu), 0u); store8(frame.at(0x10u), 0u); store8(frame.at(0x11u), 0u); call_here(rt, ctx, 0x08866510u); { const bool tk = G[17] == 0u; G[5] = G[17] + 0u; if (tk) goto L_08860D38; } G[4] = G[29] + 0u; call_here(rt, ctx, 0x088A6C18u); L_08860D38: ; G[2] = load8(G[18] + 0x18u); if (G[2] == 0u) { G[2] = sext(load16(G[18] + 0x1Eu), 16u); goto L_08860D74; } G[2] = sext(load8(frame.at(0x23u)), 8u); G[3] = G[18] + 0u; G[4] = frame.at(0x38u); put(rt, G[18] + 0x24u, G[2]); G[5] = frame.at(0x3Au); G[2] = sext(load8(G[4] + 0x0u), 8u); L_08860D5C: ; G[4] = G[4] + 0x1u; store8(G[3] + 0x4u, G[2]); { const bool tk = G[4] == G[5]; G[3] = G[3] + 0x1u; if (tk) goto L_08860DDC; } G[2] = sext(load8(G[4] + 0x0u), 8u); goto L_08860D5C; L_08860D74: ; G[4] = word(rt, G[18] + 0x24u); G[5] = word(rt, G[19] + 0x3F50u); G[3] = G[2] << 6u; store8(frame.at(0x23u), G[4]); G[2] = G[2] << 3u; G[3] = G[3] - G[2]; G[4] = word(rt, G[5] + 0x0u); G[2] = G[3] << 3u; G[2] = G[2] - G[3]; G[16] = G[4] + G[2]; G[5] = G[18] + 0u; G[6] = 0u + 0u; G[7] = 0x2u; G[4] = G[16] + 0x44u; L_08860DAC: ; G[3] = sext(load8(G[4] + 0x0u), 8u); G[2] = G[21] + G[6]; G[6] = G[6] + 0x1u; store8(G[2] + 0x20u, G[3]); G[4] = G[4] + 0x4u; store8(G[5] + 0x4u, G[3]); { const bool tk = G[6] != G[7]; G[5] = G[5] + 0x1u; if (tk) goto L_08860DAC; } G[4] = G[16] + 0u; G[5] = G[29] + 0u; call_here(rt, ctx, 0x0885DD1Cu); store8(G[16] + 0x15u, 0u); L_08860DDC: ; G[2] = 0x1u; store8(frame.at(0x20u), G[2]); G[2] = 0xFFFFFFFFu; { const bool tk = G[22] != G[2]; put(rt, frame.at(0x1Cu), 0u); if (tk) goto L_08860E58; } G[8] = sext(load8(frame.at(0x22u)), 8u); G[4] = word(rt, G[18] + 0x44u); G[5] = word(rt, G[18] + 0x48u); G[6] = G[21] + 0u; G[7] = G[29] + 0u; G[9] = G[23] + 0u; call_here(rt, ctx, 0x088661B8u); G[4] = G[19] + 0u; G[5] = G[20] + 0u; call_here(rt, ctx, 0x0885F77Cu); store8(G[18] + 0x1Bu, G[2]); G[4] = G[29] + 0u; G[16] = G[2] + 0u; call_here(rt, ctx, 0x088A5928u); { const bool tk = G[2] == 0u; G[4] = 0u + 0u; if (tk) goto L_08860E4C; } G[3] = word(rt, G[19] + 0x5ACu); G[2] = word(rt, G[3] + 0x638u); G[2] = G[2] << 7u; G[2] = sext(G[2], 8u); { const bool tk = static_cast<std::int32_t>(G[2]) >= 0; G[4] = 0x1u; if (tk) goto L_08860E4C; } G[4] = 0u + 0u; L_08860E4C: ; G[2] = G[16] & G[4]; store8(G[18] + 0x1Bu, G[2]); goto L_08860E5C; L_08860E58: ; store8(G[18] + 0x1Bu, 0u); L_08860E5C: ; G[2] = load8(G[18] + 0x1Bu); G[3] = word(rt, G[18] + 0x28u); G[5] = 0x2u; if (G[2] == 0u) G[5] = 0u; G[4] = G[29] + 0u; put(rt, frame.at(0x8u), G[3]); call_here(rt, ctx, 0x088A5974u); G[4] = G[29] + 0u; G[5] = load8(G[18] + 0x18u); G[21] = 0u + 0u; call_here(rt, ctx, 0x088A5990u); G[4] = word(rt, G[19] + 0x5B0u); G[5] = G[20] + 0u; G[6] = frame.at(0x8u); put(rt, frame.at(0xCu), G[29]); call_here(rt, ctx, 0x088A26E0u); { const bool tk = G[2] == 0u; G[17] = G[2] + 0u; if (tk) goto L_08860F84; } G[16] = word(rt, G[19] + 0x5ACu); G[4] = G[29] + 0u; call_here(rt, ctx, 0x088A5928u); G[7] = G[2] + 0u; G[4] = G[16] + 0u; G[5] = G[20] + 0u; G[6] = G[17] + 0u; call_here(rt, ctx, 0x088D7B04u); { const bool tk = G[2] != 0u; G[16] = G[2] + 0u; if (tk) goto L_08860EE0; } G[4] = word(rt, G[19] + 0x5B0u); G[5] = G[20] + 0u; call_here(rt, ctx, 0x088A23ECu); G[21] = G[16] + 0u; goto L_08860F84; L_08860EE0: ; G[4] = G[29] + 0u; call_here(rt, ctx, 0x088A5928u); if (G[2] == 0u) goto L_08860F10; G[4] = word(rt, G[19] + 0x5ACu); G[5] = G[20] + 0u; call_here(rt, ctx, 0x088DC858u); { const bool tk = G[2] != 0u; G[2] = 0xFFFFFFFFu; if (tk) goto L_08860F10; } { const bool tk = G[22] != G[2]; G[2] = 0x1u; if (tk) goto L_08860F10; } store8(G[18] + 0x1Cu, G[2]); L_08860F10: ; G[4] = G[29] + 0u; call_here(rt, ctx, 0x088A5EE0u); put(rt, G[18] + 0x30u, G[2]); G[2] = 0x1u; put(rt, G[18] + 0x34u, G[3]); store8(G[18] + 0x1Au, G[2]); G[4] = word(rt, G[19] + 0x5ACu); call_here(rt, ctx, 0x088DA0BCu); if (G[2] == 0u) { G[4] = G[19] + 0u; goto L_08860F74; } G[2] = word(rt, G[19] + 0x3E78u); G[8] = word(rt, G[18] + 0x30u); G[9] = word(rt, G[18] + 0x34u); G[4] = G[2] << 6u; G[2] = G[2] << 3u; G[4] = G[4] - G[2]; G[3] = G[4] << 7u; G[4] = G[4] + G[3]; G[4] = G[4] + G[19]; G[5] = G[20] + 0u; G[4] = G[4] + 0x608u; G[6] = G[18] + 0x6u; G[10] = 0u + 0u; call_here(rt, ctx, 0x0888A9B8u); G[4] = G[19] + 0u; L_08860F74: ; G[5] = G[20] + 0u; G[6] = G[22] + 0u; call_here(rt, ctx, 0x0885FAA4u); G[21] = G[16] + 0u; L_08860F84: ; G[16] = G[30] + 0u; G[16] = G[16] - 0x30u; L_08860F8C: ; G[4] = G[16] + 0u; call_here(rt, ctx, 0x088978B4u); { const bool tk = G[16] != G[23]; G[16] = G[16] - 0x30u; if (tk) goto L_08860F8C; } G[4] = G[29] + 0u; call_here(rt, ctx, 0x088A7548u); G[2] = G[21] + 0u; G[31] = word(rt, frame.at(0xD4u)); G[30] = word(rt, frame.at(0xD0u)); G[23] = word(rt, frame.at(0xCCu)); G[22] = word(rt, frame.at(0xC8u)); G[21] = word(rt, frame.at(0xC4u)); G[20] = word(rt, frame.at(0xC0u)); G[19] = word(rt, frame.at(0xBCu)); G[18] = word(rt, frame.at(0xB8u)); G[17] = word(rt, frame.at(0xB4u)); G[16] = word(rt, frame.at(0xB0u)); goto exit_; goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }
// 0x0887FC90 (a0 = the owner, a1 = a data node): reads one tagged chunk (node handles: 0x08AC26D4 make, 0x08AC2730 child, 0x08AC2900 string,
// 0x08AC28B8 integer, 0x08AC26DC release; 0x08AC2690 tag). 'ITMF' (child 0 = a value, child 1 = a name) creates the owner's item (+0x74 -> +0,
// a 0x78-byte object: 0x0887E880, 0x0887E270 name, 0x0887E260 value) if it has none. 'RESF' (child 0 = a kind, child 2 = the payload) by
// kind: 0x08CA03B8 resets the item's +0x28 list (0x08884720) and adds a named entry (0x0887F7C8); 0x08CA03BC adds a named entry with child
// 1's integer (0x0887F7E8); 0x08CA03C0 / 0x08CA03C4 are only compared. Names come from 0x0886C798(owner +0x1C, string) and are released.
void func_0887FC90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 owner = ctx.gpr[4], node = ctx.gpr[5];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{21u, 0x34u}, {20u, 0x30u}, {18u, 0x28u}, {31u, 0x3Cu}, {22u, 0x38u}, {19u, 0x2Cu}, {17u, 0x24u}, {16u, 0x20u}});
    const u32 payload = frame.at(0x18u), name = frame.at(0x1Cu);
    ctx.gpr[21] = payload;
    ctx.gpr[20] = owner;
    ctx.gpr[18] = node;
    call_set(rt, ctx, 0x08AC26D4u, {payload, 0u});
    auto child_string = [&](u32 handle, u32 index) {
        call_set(rt, ctx, 0x08AC2730u, {handle, node, index});
        const u32 text = call_set(rt, ctx, 0x08AC2900u, {handle});
        call_set(rt, ctx, 0x08AC26DCu, {handle});
        return text;
    };
    auto release_name = [&](u32 tag) {
        const u32 data = word(rt, name), header = data - 0xCu;
        ctx.gpr[2] = 0x08D75298u;
        ctx.gpr[16] = header;
        ctx.gpr[4] = data - 0x4u;
        if (header == 0x08D75298u) return;
        const u32 left = call_set(rt, ctx, 0x08B60804u, {data - 0x4u, 0xFFFFFFFFu});
        ctx.gpr[4] = header;
        if (static_cast<s32>(left) > 0) return;
        call_set(rt, ctx, 0x08B93554u, {header, tag});
    };
    const u32 tag = call_set(rt, ctx, 0x08AC2690u, {node});
    ctx.gpr[3] = tag;
    ctx.gpr[2] = tag == 0x49544D46u ? 0x52450000u : 0x52455346u;  // the 'RESF' constant the asm builds while comparing (v0 for other tags)
    if (tag == 0x49544D46u) {  // 'ITMF'
        const u32 value = child_string(frame.at(0x8u), 0u);
        ctx.gpr[22] = value;
        const u32 label = child_string(frame.at(0x4u), 1u);
        ctx.gpr[19] = label;
        if (value != 0u && label != 0u) {
            const u32 slot = word(rt, owner + 0x74u);
            ctx.gpr[18] = slot;
            if (word(rt, slot) == 0u) {
                const u32 context = word(rt, owner + 0x1Cu);
                ctx.gpr[17] = context;
                const u32 item = call_set(rt, ctx, 0x08B62344u, {0x78u});
                ctx.gpr[16] = item;
                call_set(rt, ctx, 0x0887E880u, {item, 0u, context});
                put(rt, slot, item);
                if (word(rt, word(rt, owner + 0x74u)) != 0u) {
                    call_set(rt, ctx, 0x0886C798u, {name, word(rt, owner + 0x1Cu), label});
                    call_set(rt, ctx, 0x0887E270u, {word(rt, word(rt, owner + 0x74u)), word(rt, name)});
                    call_set(rt, ctx, 0x0887E260u, {word(rt, word(rt, owner + 0x74u)), value});
                    release_name(frame.at(0u));
                }
            }
        }
    } else if (tag == 0x52455346u) {  // 'RESF'
        const u32 kind = child_string(frame.at(0x14u), 0u);
        ctx.gpr[19] = kind;
        call_set(rt, ctx, 0x08AC2730u, {frame.at(0x10u), node, 2u});
        put(rt, payload, word(rt, frame.at(0x10u)));
        call_set(rt, ctx, 0x08AC26DCu, {frame.at(0x10u)});
        if (kind != 0u) {
            if (call_set(rt, ctx, 0x08B7B8C4u, {kind, 0x08CA03B8u}) == 0u) {
                call_set(rt, ctx, 0x08884720u, {word(rt, owner + 0x74u) + 0x28u});
                const u32 context = word(rt, owner + 0x1Cu);
                ctx.gpr[16] = context;
                const u32 text = call_set(rt, ctx, 0x08AC2900u, {payload});
                call_set(rt, ctx, 0x0886C798u, {name, context, text});
                call_set(rt, ctx, 0x0887F7C8u, {owner, word(rt, name)});
                release_name(frame.at(0x2u));
            } else if (call_set(rt, ctx, 0x08B7B8C4u, {kind, 0x08CA03BCu}) == 0u) {
                const u32 context = word(rt, owner + 0x1Cu);
                const u32 text = call_set(rt, ctx, 0x08AC2900u, {payload});
                const u32 extra = frame.at(0xCu);
                ctx.gpr[16] = extra;
                call_set(rt, ctx, 0x0886C798u, {name, context, text});
                const u32 entry_name = word(rt, name);
                ctx.gpr[17] = entry_name;
                call_set(rt, ctx, 0x08AC2730u, {extra, node, 1u});
                const u32 number = call_set(rt, ctx, 0x08AC28B8u, {extra});
                call_set(rt, ctx, 0x0887F7E8u, {owner, entry_name, number});
                call_set(rt, ctx, 0x08AC26DCu, {extra});
                release_name(frame.at(0x1u));
            } else if (call_set(rt, ctx, 0x08B7B8C4u, {kind, 0x08CA03C0u}) != 0u) {
                call_set(rt, ctx, 0x08B7B8C4u, {kind, 0x08CA03C4u});
            }
        }
    }
    call_set(rt, ctx, 0x08AC26DCu, {payload});
}
void func_0888ED2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x8u), G[18]); G[18] = G[4] + 0u; G[4] = 0x1D8u; put(rt, frame.at(0xCu), G[31]); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x4u), G[17]); call_here(rt, ctx, 0x08B62344u); G[16] = G[2] + 0u; G[4] = G[2] + 0x188u; call_here(rt, ctx, 0x088D65DCu); G[4] = G[16] + 0x190u; call_here(rt, ctx, 0x08894444u); G[4] = G[16] + 0x198u; call_here(rt, ctx, 0x08893F7Cu); G[4] = G[16] + 0x8u; put(rt, G[16] + 0x0u, 0u); G[5] = 0u + 0u; G[6] = 0x178u; call_here(rt, ctx, 0x08B36018u); put(rt, G[16] + 0x180u, 0u); G[2] = G[16] + 0u; G[3] = 0u + 0u; put(rt, G[16] + 0x184u, 0u); G[4] = 0x4u; L_0888ED90: ; G[3] = G[3] + 0x1u; put(rt, G[2] + 0x1A0u, 0u); { const bool tk = G[3] != G[4]; G[2] = G[2] + 0x4u; if (tk) goto L_0888ED90; } G[5] = 0u + 0u; G[6] = 0x2u; G[7] = 0x4u; G[4] = 0x1B0u; L_0888EDB0: ; G[2] = G[16] + G[4]; G[3] = 0u + 0u; L_0888EDB8: ; G[3] = G[3] + 0x1u; put(rt, G[2] + 0x0u, 0u); { const bool tk = G[3] != G[6]; G[2] = G[2] + 0x4u; if (tk) goto L_0888EDB8; } G[5] = G[5] + 0x1u; { const bool tk = G[5] != G[7]; G[4] = G[4] + 0x8u; if (tk) goto L_0888EDB0; } G[2] = 0xFFFFFFFFu; put(rt, G[16] + 0x1D0u, G[2]); G[3] = high_half(0x08D75E88u); store8(G[16] + 0x1D4u, 0u); put(rt, G[18] + 0x0u, G[16]); G[2] = sext(load8(0x08D75E88u), 8u); { const bool tk = G[2] != 0u; G[17] = 0x08D75E88u; if (tk) goto L_0888EE04; } G[4] = G[17] + 0u; call_here(rt, ctx, 0x08B6441Cu); { const bool tk = G[2] != 0u; G[3] = high_half(0x08D75E90u); if (tk) goto L_0888EE10; } L_0888EE04: ; G[2] = high_half(0x08D75E90u); G[16] = 0x08D75E90u; goto L_0888EE2C; L_0888EE10: ; G[16] = G[3] + sext(0x08D75E90u, 16u); { const bool tk = G[16] == 0u; G[2] = high_half(0x08CC1D18u); if (tk) goto L_0888EE24; } G[2] = 0x08CC1D18u; put(rt, G[3] + sext(0x08D75E90u, 16u), G[2]); L_0888EE24: ; G[4] = G[17] + 0u; call_here(rt, ctx, 0x08B64494u); L_0888EE2C: ; put(rt, G[18] + 0x4u, G[16]); G[31] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); goto exit_; goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }
void func_088A4F94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x30u); G[2] = high_half(0x08CC29A8u); G[2] = 0x08CC29A8u; put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x18u), G[18]); G[18] = G[4] + 0u; put(rt, frame.at(0x10u), G[16]); G[16] = G[18] + 0x318u; put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x14u), G[17]); put(rt, G[4] + 0x0u, G[2]); call_here(rt, ctx, 0x088A3750u); G[2] = high_half(0x08CC2C30u); G[2] = 0x08CC2C30u; put(rt, G[18] + 0x318u, G[2]); G[2] = high_half(0x08CC2C4Cu); G[2] = 0x08CC2C4Cu; G[4] = G[16] + 0u; put(rt, G[16] + 0x4u, G[2]); call_here(rt, ctx, 0x088A8420u); G[6] = frame.at(0x1u); G[4] = word(rt, G[18] + 0x338u); G[5] = word(rt, G[18] + 0x33Cu); call_here(rt, ctx, 0x08BAE078u); G[4] = word(rt, G[18] + 0x338u); if (G[4] == 0u) { G[4] = word(rt, G[18] + 0x32Cu); goto L_088A5010; } call_here(rt, ctx, 0x08B35254u); G[4] = word(rt, G[18] + 0x32Cu); L_088A5010: ; G[5] = word(rt, G[18] + 0x330u); G[6] = G[29] + 0u; call_here(rt, ctx, 0x08BAC238u); G[4] = word(rt, G[18] + 0x32Cu); { const bool tk = G[4] == 0u; G[2] = high_half(0x08CC2A98u); if (tk) goto L_088A5034; } call_here(rt, ctx, 0x08B35254u); G[2] = high_half(0x08CC2A98u); L_088A5034: ; G[2] = G[2] + sext(0x08CC2A98u, 16u); put(rt, G[16] + 0x4u, G[2]); G[2] = high_half(0x08CC2AB0u); G[2] = 0x08CC2AB0u; G[16] = G[18] + 0x270u; put(rt, G[18] + 0x318u, G[2]); G[4] = word(rt, G[16] + 0x4u); { const bool tk = G[4] == 0u; G[2] = high_half(0x08CC2CB8u); if (tk) goto L_088A5074; } G[3] = word(rt, G[4] + 0x0u); G[2] = word(rt, G[3] + 0x0u); { const std::uint32_t callee = G[2]; G[5] = word(rt, G[18] + 0x270u); call_here(rt, ctx, callee); } put(rt, G[16] + 0x4u, 0u); put(rt, G[18] + 0x270u, 0u); G[2] = high_half(0x08CC2CB8u); L_088A5074: ; G[3] = high_half(0x08CC2DDCu); G[20] = G[18] + 0x20u; G[2] = G[2] + sext(0x08CC2CB8u, 16u); G[3] = 0x08CC2DDCu; G[4] = G[18] + 0x134u; put(rt, G[18] + 0x20u, G[2]); { const bool tk = G[4] == 0u; put(rt, G[20] + 0x4u, G[3]); if (tk) goto L_088A50CC; } G[17] = G[18] + 0x164u; { const bool tk = G[4] == G[17]; G[2] = G[20] + 0xA4u; if (tk) goto L_088A50D0; } G[16] = G[18] + 0x158u; G[19] = 0x4u; G[2] = word(rt, G[16] + 0x0u); L_088A50AC: ; G[17] = G[17] - 0xCu; G[19] = G[19] - 0x1u; G[3] = word(rt, G[2] + 0x0u); G[4] = G[17] + 0u; { const std::uint32_t callee = G[3]; G[16] = G[16] - 0xCu; call_here(rt, ctx, callee); } if (G[19] != 0u) { G[2] = word(rt, G[16] + 0x0u); goto L_088A50AC; } L_088A50CC: ; G[2] = G[20] + 0xA4u; L_088A50D0: ; { const bool tk = G[2] == 0u; G[17] = G[20] + 0x114u; if (tk) goto L_088A5108; } { const bool tk = G[2] == G[17]; G[16] = G[20] + 0xF8u; if (tk) goto L_088A5108; } G[19] = 0x4u; G[2] = word(rt, G[16] + 0x0u); L_088A50E8: ; G[17] = G[17] - 0x1Cu; G[19] = G[19] - 0x1u; G[3] = word(rt, G[2] + 0x0u); G[4] = G[17] + 0u; { const std::uint32_t callee = G[3]; G[16] = G[16] - 0x1Cu; call_here(rt, ctx, callee); } if (G[19] != 0u) { G[2] = word(rt, G[16] + 0x0u); goto L_088A50E8; } L_088A5108: ; G[2] = high_half(0x08CBF428u); G[2] = 0x08CBF428u; put(rt, G[20] + 0x0u, G[2]); G[3] = high_half(0x08CC2AD8u); G[2] = high_half(0x08CC16A0u); G[2] = 0x08CC16A0u; G[3] = 0x08CC2AD8u; put(rt, G[20] + 0x4u, G[2]); G[4] = G[18] + 0x8u; put(rt, G[18] + 0x10u, G[3]); call_here(rt, ctx, 0x088A7548u); G[2] = high_half(0x08CC2AE8u); G[2] = 0x08CC2AE8u; put(rt, G[18] + 0x0u, G[2]); G[31] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); goto exit_; goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }
void func_088D63E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x4u), G[17]); G[17] = G[4] + 0u; G[4] = 0x118u; put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0x8u), G[18]); G[18] = G[5] + 0u; put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0xCu), G[19]); call_here(rt, ctx, 0x08B62344u); G[16] = G[2] + 0u; G[4] = G[2] + 0x82u; call_here(rt, ctx, 0x088EF524u); put(rt, G[17] + 0x0u, G[16]); G[3] = high_half(0x08D75FD8u); G[2] = sext(load8(0x08D75FD8u), 8u); { const bool tk = G[2] != 0u; G[16] = 0x08D75FD8u; if (tk) goto L_088D6440; } G[4] = G[16] + 0u; call_here(rt, ctx, 0x08B6441Cu); { const bool tk = G[2] != 0u; G[3] = high_half(0x08D75FE0u); if (tk) goto L_088D644C; } L_088D6440: ; G[2] = high_half(0x08D75FE0u); G[19] = 0x08D75FE0u; goto L_088D6468; L_088D644C: ; G[19] = G[3] + sext(0x08D75FE0u, 16u); { const bool tk = G[19] == 0u; G[2] = high_half(0x08CC36F8u); if (tk) goto L_088D6460; } G[2] = 0x08CC36F8u; put(rt, G[3] + sext(0x08D75FE0u, 16u), G[2]); L_088D6460: ; G[4] = G[16] + 0u; call_here(rt, ctx, 0x08B64494u); L_088D6468: ; G[6] = word(rt, G[18] + 0x0u); G[7] = word(rt, G[17] + 0x0u); put(rt, G[17] + 0x4u, G[19]); G[8] = G[6] + 0x60u; L_088D6478: ; G[2] = word(rt, G[6] + 0x0u); G[3] = word(rt, G[6] + 0x4u); G[4] = word(rt, G[6] + 0x8u); G[5] = word(rt, G[6] + 0xCu); put(rt, G[7] + 0x0u, G[2]); G[6] = G[6] + 0x10u; G[7] = G[7] + 0x10u; put(rt, G[7] - 0xCu, G[3]); put(rt, G[7] - 0x8u, G[4]); { const bool tk = G[6] != G[8]; put(rt, G[7] - 0x4u, G[5]); if (tk) goto L_088D6478; } G[4] = word(rt, G[18] + 0x0u); G[3] = word(rt, G[17] + 0x0u); G[6] = G[4] + 0x60u; G[7] = G[3] + 0x60u; G[2] = G[6] | G[7]; G[2] = G[2] & 0x3u; { const bool tk = G[2] == 0u; G[8] = G[4] + 0x80u; if (tk) goto L_088D64F8; } L_088D64C4: ; G[2] = load16(G[6] + 0x0u); G[3] = load16(G[6] + 0x2u); G[4] = load16(G[6] + 0x4u); G[5] = load16(G[6] + 0x6u); store16(G[7] + 0x0u, G[2]); G[6] = G[6] + 0x8u; G[7] = G[7] + 0x8u; store16(G[7] - 0x6u, G[3]); store16(G[7] - 0x4u, G[4]); { const bool tk = G[6] != G[8]; store16(G[7] - 0x2u, G[5]); if (tk) goto L_088D64C4; } G[2] = load16(G[6] + 0x0u); goto L_088D6528; L_088D64F8: ; G[2] = word(rt, G[6] + 0x0u); G[3] = word(rt, G[6] + 0x4u); G[4] = word(rt, G[6] + 0x8u); G[5] = word(rt, G[6] + 0xCu); put(rt, G[7] + 0x0u, G[2]); G[6] = G[6] + 0x10u; G[7] = G[7] + 0x10u; put(rt, G[7] - 0xCu, G[3]); put(rt, G[7] - 0x8u, G[4]); { const bool tk = G[6] != G[8]; put(rt, G[7] - 0x4u, G[5]); if (tk) goto L_088D64F8; } G[2] = load16(G[6] + 0x0u); L_088D6528: ; store16(G[7] + 0x0u, G[2]); G[5] = word(rt, G[18] + 0x0u); G[3] = word(rt, G[17] + 0x0u); G[6] = G[5] + 0x82u; G[7] = G[3] + 0x82u; G[2] = G[6] | G[7]; G[2] = G[2] & 0x3u; { const bool tk = G[2] == 0u; G[8] = G[5] + 0x112u; if (tk) goto L_088D658C; } L_088D654C: ; G[2] = load16(G[6] + 0x0u); G[3] = load16(G[6] + 0x2u); G[4] = load16(G[6] + 0x4u); G[5] = load16(G[6] + 0x6u); store16(G[7] + 0x0u, G[2]); G[6] = G[6] + 0x8u; G[7] = G[7] + 0x8u; store16(G[7] - 0x6u, G[3]); store16(G[7] - 0x4u, G[4]); { const bool tk = G[6] != G[8]; store16(G[7] - 0x2u, G[5]); if (tk) goto L_088D654C; } G[2] = load16(G[6] + 0x2u); G[3] = load16(G[6] + 0x0u); store16(G[7] + 0x2u, G[2]); store16(G[7] + 0x0u, G[3]); goto L_088D65C0; L_088D658C: ; G[2] = word(rt, G[6] + 0x0u); G[3] = word(rt, G[6] + 0x4u); G[4] = word(rt, G[6] + 0x8u); G[5] = word(rt, G[6] + 0xCu); put(rt, G[7] + 0x0u, G[2]); G[6] = G[6] + 0x10u; G[7] = G[7] + 0x10u; put(rt, G[7] - 0xCu, G[3]); put(rt, G[7] - 0x8u, G[4]); { const bool tk = G[6] != G[8]; put(rt, G[7] - 0x4u, G[5]); if (tk) goto L_088D658C; } G[2] = word(rt, G[6] + 0x0u); put(rt, G[7] + 0x0u, G[2]); L_088D65C0: ; G[31] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); goto exit_; goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }
void func_088E16D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x4u), G[17]); G[17] = G[4] + 0u; put(rt, frame.at(0x0u), G[16]); G[16] = G[5] + 0u; put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0xCu), G[19]); { const bool tk = static_cast<std::int32_t>(G[5]) < 0; put(rt, frame.at(0x8u), G[18]); if (tk) goto L_088E17C8; } call_here(rt, ctx, 0x088D9F80u); G[2] = static_cast<std::int32_t>(G[16]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; { const bool tk = G[2] == 0u; G[2] = 0xFFFFFFFFu; if (tk) goto L_088E17CC; } goto L_088E17D4; L_088E1714: ; G[2] = G[16] << 7u; G[2] = G[2] - G[3]; G[4] = word(rt, G[17] + 0xC0u); G[2] = G[2] - G[16]; G[2] = G[2] << 3u; G[16] = G[4] + G[2]; G[3] = word(rt, G[16] + 0xCu); { const bool tk = G[3] == 0u; G[2] = 0xFFFFFFFFu; if (tk) goto L_088E17CC; } G[3] = word(rt, G[16] + 0x4u); G[2] = 0x1u; { const bool tk = G[3] != G[2]; G[2] = 0xFFFFFFFFu; if (tk) goto L_088E17CC; } G[4] = G[16] + 0u; call_here(rt, ctx, 0x088E85E8u); { const bool tk = G[2] == 0u; G[2] = 0xFFFFFFFFu; if (tk) goto L_088E17CC; } G[4] = G[17] + 0x4u; call_here(rt, ctx, 0x088D5D34u); G[19] = sra(G[2], 31u); { const bool tk = static_cast<std::int32_t>(G[19]) < 0; G[18] = G[2] + 0u; if (tk) goto L_088E17C8; } G[4] = G[17] + 0u; call_here(rt, ctx, 0x088D9C70u); G[6] = G[2] + 0u; G[4] = G[16] + 0u; G[7] = G[3] + 0u; call_here(rt, ctx, 0x088E855Cu); { const bool tk = static_cast<std::int32_t>(G[3]) < 0; G[6] = 0xEA60u; if (tk) goto L_088E17C8; } { const auto product = static_cast<std::uint64_t>(G[18]) * G[6]; ctx.lo = static_cast<std::uint32_t>(product); ctx.hi = static_cast<std::uint32_t>(static_cast<std::uint64_t>(product) >> 32); } G[5] = ctx.hi; G[4] = ctx.lo; ctx.lo = G[5]; G[2] = G[4] - G[2]; G[6] = G[4] < G[2] ? 1u : 0u; G[5] = ctx.lo; G[3] = G[5] - G[3]; G[3] = G[3] - G[6]; { const bool tk = static_cast<std::int32_t>(G[3]) >= 0; G[31] = word(rt, frame.at(0x10u)); if (tk) goto L_088E17F0; } G[2] = 0u + 0u; G[3] = 0u + 0u; goto L_088E17F0; L_088E17C8: ; G[2] = 0xFFFFFFFFu; L_088E17CC: ; G[3] = 0xFFFFFFFFu; goto L_088E17EC; L_088E17D4: ; G[4] = G[17] + 0u; call_here(rt, ctx, 0x088DAA84u); { const bool tk = G[2] == 0u; G[2] = 0xFFFFFFFFu; if (tk) goto L_088E17CC; } G[3] = G[16] << 5u; goto L_088E1714; L_088E17EC: ; G[31] = word(rt, frame.at(0x10u)); L_088E17F0: ; G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); goto exit_; goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }
void func_088F0B90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x30u); G[2] = high_half(0x08CC3A50u); put(rt, frame.at(0x1Cu), G[19]); G[3] = G[4] + 0x4u; G[19] = G[4] + 0u; G[2] = 0x08CC3A50u; put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, G[4] + 0x0u, G[2]); G[2] = 0x3u; G[4] = 0xFFFFFFFFu; put(rt, G[3] + 0x18u, 0u); put(rt, G[3] + 0x4u, 0u); put(rt, G[3] + 0x8u, 0u); put(rt, G[3] + 0xCu, 0u); put(rt, G[3] + 0x10u, 0u); put(rt, G[3] + 0x14u, 0u); put(rt, G[19] + 0x4u, 0u); put(rt, G[19] + 0x20u, 0u); G[2] = G[2] - 0x1u; L_088F0BEC: ; { const bool tk = G[2] != G[4]; G[2] = G[2] - 0x1u; if (tk) goto L_088F0BEC; } G[4] = G[19] + 0xBCu; call_here(rt, ctx, 0x088AFAF0u); G[2] = 0x8u; G[4] = 0x10u; store8(G[19] + 0xC8u, G[2]); G[18] = 0x1u; G[20] = G[19] + 0xCCu; put(rt, G[19] + 0xC4u, 0u); store8(G[19] + 0xCBu, 0u); call_here(rt, ctx, 0x08B62344u); G[4] = 0x54u; put(rt, G[2] + 0x0u, 0u); G[16] = G[2] + 0u; put(rt, G[2] + 0x4u, G[18]); call_here(rt, ctx, 0x08B6461Cu); G[17] = G[2] + 0u; G[4] = G[2] + 0u; call_here(rt, ctx, 0x088F1310u); put(rt, G[16] + 0x0u, G[17]); G[4] = 0x8u; put(rt, G[16] + 0xCu, G[18]); put(rt, G[16] + 0x8u, 0u); call_here(rt, ctx, 0x08B6461Cu); G[17] = G[2] + 0u; G[4] = G[2] + 0u; call_here(rt, ctx, 0x088F0EF4u); put(rt, G[16] + 0x8u, G[17]); G[3] = high_half(0x08D75FF8u); put(rt, G[19] + 0xCCu, G[16]); G[2] = sext(load8(0x08D75FF8u), 8u); { const bool tk = G[2] != 0u; G[17] = 0x08D75FF8u; if (tk) goto L_088F0C84; } G[4] = G[17] + 0u; call_here(rt, ctx, 0x08B6441Cu); { const bool tk = G[2] != 0u; G[3] = high_half(0x08D76000u); if (tk) goto L_088F0C90; } L_088F0C84: ; G[2] = high_half(0x08D76000u); G[18] = 0x08D76000u; goto L_088F0CAC; L_088F0C90: ; G[18] = G[3] + sext(0x08D76000u, 16u); { const bool tk = G[18] == 0u; G[2] = high_half(0x08CC3B00u); if (tk) goto L_088F0CA4; } G[2] = 0x08CC3B00u; put(rt, G[3] + sext(0x08D76000u, 16u), G[2]); L_088F0CA4: ; G[4] = G[17] + 0u; call_here(rt, ctx, 0x08B64494u); L_088F0CAC: ; G[16] = 0x1u; put(rt, G[20] + 0x4u, G[18]); G[4] = 0x1u; G[17] = G[19] + 0xDCu; put(rt, G[19] + 0xD8u, G[16]); put(rt, G[19] + 0xD4u, 0u); call_here(rt, ctx, 0x08B6461Cu); G[4] = 0x2u; put(rt, G[19] + 0xD4u, G[2]); put(rt, G[19] + 0xDCu, 0u); put(rt, G[17] + 0x4u, G[16]); call_here(rt, ctx, 0x08B6461Cu); G[4] = G[2] + 0u; put(rt, G[19] + 0xE8u, 0u); G[6] = G[29] + 0u; store8(G[19] + 0xC9u, 0u); store8(G[19] + 0xCAu, 0u); G[5] = word(rt, G[17] + 0x4u); put(rt, G[19] + 0xDCu, G[2]); G[5] = G[5] << 1u; G[5] = G[2] + G[5]; G[2] = 0xFFFFFFFFu; put(rt, frame.at(0x0u), G[2]); call_here(rt, ctx, 0x08BB8760u); G[2] = word(rt, G[19] + 0xDCu); put(rt, G[19] + 0xE4u, G[2]); G[31] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); goto exit_; goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }
void func_08918844(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x4u), G[17]); G[2] = high_half(0x08CC58E0u); G[17] = G[4] + 0u; put(rt, frame.at(0x8u), G[31]); G[2] = 0x08CC58E0u; put(rt, frame.at(0x0u), G[16]); G[16] = word(rt, G[4] + 0x8u); put(rt, G[17] + 0x0u, G[2]); { const bool tk = G[16] == 0u; G[4] = G[16] + 0x2010u; if (tk) goto L_08918968; } G[2] = high_half(0x08CC5950u); G[2] = 0x08CC5950u; put(rt, G[16] + 0x2174u, G[2]); G[3] = high_half(0x08CC2760u); G[2] = high_half(0x08CC5978u); G[3] = 0x08CC2760u; G[2] = 0x08CC5978u; put(rt, G[16] + 0x216Cu, G[3]); G[3] = high_half(0x08CBF428u); G[3] = 0x08CBF428u; put(rt, G[16] + 0x2164u, G[2]); G[2] = high_half(0x08CC5990u); G[2] = 0x08CC5990u; put(rt, G[16] + 0x2160u, G[3]); put(rt, G[16] + 0x2010u, G[2]); call_here(rt, ctx, 0x08925AE4u); G[4] = G[16] + 0x1FD4u; call_here(rt, ctx, 0x08925B48u); G[4] = G[16] + 0x1D30u; call_here(rt, ctx, 0x0891C6B0u); G[4] = G[16] + 0x1B20u; call_here(rt, ctx, 0x0891A724u); G[2] = high_half(0x08CC5B40u); G[2] = 0x08CC5B40u; put(rt, G[16] + 0x1920u, G[2]); G[4] = G[16] + 0x1920u; call_here(rt, ctx, 0x08925FF8u); G[2] = high_half(0x08CC6020u); G[2] = 0x08CC6020u; put(rt, G[16] + 0x1720u, G[2]); G[4] = G[16] + 0x1720u; call_here(rt, ctx, 0x08925FF8u); G[2] = high_half(0x08CC59D0u); G[2] = 0x08CC59D0u; put(rt, G[16] + 0x1520u, G[2]); G[4] = G[16] + 0x1520u; call_here(rt, ctx, 0x08925FF8u); G[2] = high_half(0x08CC5A28u); G[2] = 0x08CC5A28u; put(rt, G[16] + 0x1320u, G[2]); G[4] = G[16] + 0x1320u; call_here(rt, ctx, 0x08925FF8u); G[4] = G[16] + 0xF00u; call_here(rt, ctx, 0x0891A344u); G[2] = high_half(0x08CC5D18u); G[2] = 0x08CC5D18u; put(rt, G[16] + 0xD00u, G[2]); G[4] = G[16] + 0xD00u; call_here(rt, ctx, 0x08925FF8u); G[4] = G[16] + 0xB00u; call_here(rt, ctx, 0x0891A5B8u); G[4] = G[16] + 0x900u; call_here(rt, ctx, 0x0891A4C4u); G[4] = G[16] + 0x4E0u; call_here(rt, ctx, 0x0891A344u); G[4] = G[16] + 0x290u; call_here(rt, ctx, 0x0891C588u); G[4] = G[16] + 0u; call_here(rt, ctx, 0x0891FDE8u); G[4] = G[16] + 0u; call_here(rt, ctx, 0x08B64654u); put(rt, G[17] + 0x8u, 0u); L_08918968: ; G[2] = high_half(0x08CC5A98u); G[3] = high_half(0x08CC5A80u); G[2] = 0x08CC5A98u; G[3] = 0x08CC5A80u; put(rt, G[17] + 0x0u, G[2]); put(rt, G[17] + 0x1Cu, G[3]); put(rt, G[17] + 0xCu, 0u); G[31] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); goto exit_; goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }
void func_0897F6A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x34u), G[21]); G[21] = high_half(0x08CF4100u); put(rt, frame.at(0x2Cu), G[19]); G[19] = G[4] + 0u; G[4] = 0x08CF4100u; put(rt, frame.at(0x20u), G[16]); G[16] = G[5] + 0u; put(rt, frame.at(0x38u), G[31]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x24u), G[17]); call_here(rt, ctx, 0x08B344E0u); G[2] = G[19] + 0x1E4u; G[5] = word(rt, G[19] + 0x1E4u); { const bool tk = G[5] != G[2]; G[5] = G[5] + 0x8u; if (tk) goto L_0897F6FC; } G[2] = high_half(0x08D74FD0u); G[20] = 0x08D74FD0u; G[3] = G[20] + 0xCu; put(rt, frame.at(0xCu), G[3]); goto L_0897F70C; L_0897F6FC: ; G[4] = frame.at(0xCu); call_here(rt, ctx, 0x08B88F44u); G[2] = high_half(0x08D74FD0u); G[20] = 0x08D74FD0u; L_0897F70C: ; G[5] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[5] - 0xCu); { const bool tk = G[2] != 0u; G[2] = G[20] + 0xCu; if (tk) goto L_0897F788; } G[4] = 0xCu; put(rt, frame.at(0x8u), G[2]); G[5] = 0x4u; G[17] = word(rt, G[19] + 0x1E4u); call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2] + 0x8u; { const bool tk = G[4] == 0u; G[16] = G[2] + 0u; if (tk) goto L_0897F744; } G[5] = frame.at(0x8u); call_here(rt, ctx, 0x08B88F44u); L_0897F744: ; G[4] = G[16] + 0u; G[5] = G[17] + 0u; call_here(rt, ctx, 0x08B5C078u); G[4] = word(rt, frame.at(0x8u)); G[16] = G[4] - 0xCu; if (G[20] == G[16]) { G[8] = word(rt, G[19] + 0x1E4u); goto L_0897F7FC; } G[4] = G[4] - 0x4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) > 0) { G[8] = word(rt, G[19] + 0x1E4u); goto L_0897F7FC; } G[4] = G[16] + 0u; G[5] = frame.at(0x2u); call_here(rt, ctx, 0x08B8542Cu); G[8] = word(rt, G[19] + 0x1E4u); goto L_0897F7FC; L_0897F788: ; G[18] = frame.at(0x4u); G[4] = G[18] + 0u; G[6] = frame.at(0x3u); call_here(rt, ctx, 0x08B85EB8u); G[4] = 0xCu; G[5] = 0x4u; G[17] = word(rt, G[19] + 0x1E4u); call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2] + 0x8u; { const bool tk = G[4] == 0u; G[16] = G[2] + 0u; if (tk) goto L_0897F7BC; } G[5] = G[18] + 0u; call_here(rt, ctx, 0x08B88F44u); L_0897F7BC: ; G[4] = G[16] + 0u; G[5] = G[17] + 0u; call_here(rt, ctx, 0x08B5C078u); G[4] = word(rt, frame.at(0x4u)); G[16] = G[4] - 0xCu; if (G[20] == G[16]) { G[8] = word(rt, G[19] + 0x1E4u); goto L_0897F7FC; } G[4] = G[4] - 0x4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) > 0) { G[8] = word(rt, G[19] + 0x1E4u); goto L_0897F7FC; } G[4] = G[16] + 0u; G[5] = frame.at(0x1u); call_here(rt, ctx, 0x08B8542Cu); G[8] = word(rt, G[19] + 0x1E4u); L_0897F7FC: ; G[17] = word(rt, frame.at(0xCu)); G[7] = frame.at(0x14u); G[2] = word(rt, G[8] + 0x8u); G[5] = G[17] + 0u; G[18] = G[17] - 0xCu; G[4] = word(rt, G[2] - 0xCu); G[2] = frame.at(0x10u); put(rt, frame.at(0x10u), G[4]); G[3] = word(rt, G[17] - 0xCu); G[6] = G[3] < G[4] ? 1u : 0u; put(rt, frame.at(0x14u), G[3]); if (G[6] == 0u) G[7] = G[2]; G[16] = G[4] - G[3]; G[6] = word(rt, G[7] + 0x0u); G[4] = word(rt, G[8] + 0x8u); call_here(rt, ctx, 0x08B36170u); { const bool tk = G[18] == G[20]; if (G[2] != 0u) G[16] = G[2]; if (tk) goto L_0897F860; } G[4] = G[17] - 0x4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[4] = G[18] + 0u; if (tk) goto L_0897F860; } G[5] = G[29] + 0u; call_here(rt, ctx, 0x08B8542Cu); L_0897F860: ; G[4] = G[21] + sext(0x08CF4100u, 16u); call_here(rt, ctx, 0x08B3457Cu); { const bool tk = G[16] == 0u; G[31] = word(rt, frame.at(0x38u)); if (tk) goto L_0897F87C; } G[4] = G[19] + 0u; call_here(rt, ctx, 0x0897E2E0u); G[31] = word(rt, frame.at(0x38u)); L_0897F87C: ; G[21] = word(rt, frame.at(0x34u)); G[20] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); goto exit_; goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }
void func_08988264(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x38u), G[22]); G[22] = G[4] + 0u; G[4] = frame.at(0x8u); put(rt, frame.at(0x34u), G[21]); G[21] = G[5] + 0u; G[5] = G[22] + 0x4u; put(rt, frame.at(0x3Cu), G[31]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x20u), G[16]); call_here(rt, ctx, 0x08B88F44u); G[5] = high_half(0x08CA3960u); G[5] = 0x08CA3960u; G[4] = frame.at(0x4u); G[6] = frame.at(0x3u); call_here(rt, ctx, 0x08B85EB8u); G[17] = word(rt, frame.at(0x8u)); G[16] = word(rt, frame.at(0x4u)); G[2] = frame.at(0xCu); G[7] = word(rt, G[17] - 0xCu); G[8] = frame.at(0x10u); G[4] = G[17] + 0u; put(rt, frame.at(0xCu), G[7]); G[5] = G[16] + 0u; G[19] = G[16] - 0xCu; G[3] = word(rt, G[16] - 0xCu); G[6] = G[3] < G[7] ? 1u : 0u; put(rt, frame.at(0x10u), G[3]); if (G[6] == 0u) G[8] = G[2]; G[18] = G[7] - G[3]; G[6] = word(rt, G[8] + 0x0u); call_here(rt, ctx, 0x08B36170u); G[3] = high_half(0x08D74FD0u); G[20] = 0x08D74FD0u; { const bool tk = G[19] == G[20]; if (G[2] != 0u) G[18] = G[2]; if (tk) goto L_08988324; } G[4] = G[16] - 0x4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) > 0) { G[17] = word(rt, frame.at(0x8u)); goto L_08988324; } G[4] = G[19] + 0u; G[5] = frame.at(0x2u); call_here(rt, ctx, 0x08B8542Cu); G[17] = word(rt, frame.at(0x8u)); L_08988324: ; G[16] = G[17] - 0xCu; { const bool tk = G[20] == G[16]; G[4] = G[17] - 0x4u; if (tk) goto L_08988348; } G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[4] = G[16] + 0u; if (tk) goto L_08988348; } G[5] = frame.at(0x1u); call_here(rt, ctx, 0x08B8542Cu); L_08988348: ; { const bool tk = G[18] != 0u; G[16] = frame.at(0x14u); if (tk) goto L_089883C8; } G[2] = G[20] + 0xCu; G[4] = G[21] + 0u; G[5] = G[16] + 0u; put(rt, frame.at(0x14u), G[2]); call_here(rt, ctx, 0x08987A74u); G[4] = G[21] + 0u; G[5] = G[16] + 0u; call_here(rt, ctx, 0x08987A74u); G[3] = 0x101u; { const bool tk = G[2] != G[3]; G[4] = G[21] + 0u; if (tk) goto L_08988390; } G[4] = word(rt, frame.at(0x14u)); G[16] = word(rt, G[22] + 0xCu); call_here(rt, ctx, 0x08985D80u); store16(G[16] + 0x0u, G[2]); G[4] = G[21] + 0u; L_08988390: ; G[5] = 0x1u; call_here(rt, ctx, 0x0898682Cu); G[4] = word(rt, frame.at(0x14u)); G[16] = G[4] - 0xCu; { const bool tk = G[20] == G[16]; G[31] = word(rt, frame.at(0x3Cu)); if (tk) goto L_089883CC; } G[4] = G[4] - 0x4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[31] = word(rt, frame.at(0x3Cu)); if (tk) goto L_089883CC; } G[4] = G[16] + 0u; G[5] = G[29] + 0u; call_here(rt, ctx, 0x08B8542Cu); L_089883C8: ; G[31] = word(rt, frame.at(0x3Cu)); L_089883CC: ; G[22] = word(rt, frame.at(0x38u)); G[21] = word(rt, frame.at(0x34u)); G[20] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); goto exit_; goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }
void func_089BC7E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x70u); put(rt, frame.at(0x4Cu), G[19]); G[19] = G[7] + 0u; put(rt, frame.at(0x60u), G[30]); G[30] = G[5] + 0u; put(rt, frame.at(0x58u), G[22]); G[22] = G[6] + 0u; put(rt, frame.at(0x54u), G[21]); G[21] = G[4] + 0u; put(rt, frame.at(0x40u), G[16]); put(rt, frame.at(0x64u), G[31]); put(rt, frame.at(0x5Cu), G[23]); put(rt, frame.at(0x50u), G[20]); put(rt, frame.at(0x48u), G[18]); put(rt, frame.at(0x44u), G[17]); call_here(rt, ctx, 0x08A7F3D0u); G[16] = word(rt, G[19] + 0x0u); { const bool tk = G[16] != G[19]; G[2] = high_half(0x08CA5D44u); if (tk) goto L_089BC958; } G[2] = word(rt, G[21] + 0x3Cu); { const bool tk = G[2] == 0u; G[3] = G[21] + 0x3Cu; if (tk) goto L_089BCB10; } G[2] = word(rt, G[2] + 0x4u); { const bool tk = G[2] == 0u; G[31] = word(rt, frame.at(0x64u)); if (tk) goto L_089BCB14; } G[2] = word(rt, G[3] + 0x0u); goto L_089BCAF8; L_089BC850: ; G[5] = high_half(0x08CF6F10u); G[5] = 0x08CF6F10u; G[4] = G[23] + 0u; call_here(rt, ctx, 0x08A6972Cu); G[4] = G[22] + 0u; G[5] = G[23] + 0u; call_here(rt, ctx, 0x08BFE7D0u); G[20] = G[2] + 0u; G[2] = G[22] + 0x4u; { const bool tk = G[20] == G[2]; G[2] = word(rt, frame.at(0x20u)); if (tk) goto L_089BC88C; } G[3] = word(rt, G[20] + 0x10u); G[2] = G[2] < G[3] ? 1u : 0u; { const bool tk = G[2] == 0u; G[18] = G[20] + 0u; if (tk) goto L_089BC8E4; } L_089BC88C: ; G[17] = frame.at(0xCu); G[16] = frame.at(0x2Cu); G[4] = G[17] + 0u; call_here(rt, ctx, 0x08A5D438u); G[19] = frame.at(0x30u); G[4] = G[16] + 0u; G[5] = G[23] + 0u; call_here(rt, ctx, 0x08A6972Cu); G[4] = G[19] + 0u; G[5] = G[17] + 0u; call_here(rt, ctx, 0x08A5D424u); G[5] = G[20] + 0u; G[6] = G[16] + 0u; G[4] = G[22] + 0u; call_here(rt, ctx, 0x08BFEBACu); G[4] = G[19] + 0u; G[18] = G[2] + 0u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16] + 0u; call_here(rt, ctx, 0x08A69D68u); G[4] = G[17] + 0u; call_here(rt, ctx, 0x08A5B9ECu); L_089BC8E4: ; G[4] = word(rt, G[21] + 0x3Cu); { const bool tk = G[4] == 0u; G[18] = G[18] + 0x14u; if (tk) goto L_089BC8FC; } G[2] = word(rt, G[4] + 0x4u); { const bool tk = G[2] != 0u; G[3] = G[4] + 0u; if (tk) goto L_089BC900; } L_089BC8FC: ; G[3] = 0u + 0u; L_089BC900: ; G[17] = frame.at(0x18u); G[4] = G[17] + 0u; G[5] = frame.at(0x14u); G[16] = frame.at(0x1Cu); put(rt, frame.at(0x14u), G[3]); call_here(rt, ctx, 0x08A7C0E0u); G[4] = G[16] + 0u; G[5] = G[23] + 0u; G[6] = G[17] + 0u; call_here(rt, ctx, 0x08A7B544u); { const bool tk = G[18] == G[16]; G[5] = word(rt, frame.at(0x1Cu)); if (tk) goto L_089BC938; } G[4] = G[18] + 0u; call_here(rt, ctx, 0x08A5BAD8u); L_089BC938: ; G[4] = G[16] + 0u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[17] + 0u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[23] + 0u; call_here(rt, ctx, 0x08A69D68u); G[31] = word(rt, frame.at(0x64u)); goto L_089BCB14; L_089BC958: ; G[5] = G[2] + sext(0x08CA5D44u, 16u); G[4] = frame.at(0x8u); G[6] = frame.at(0x3u); call_here(rt, ctx, 0x08B85EB8u); G[2] = word(rt, G[16] + 0x8u); G[18] = word(rt, frame.at(0x8u)); G[7] = frame.at(0x28u); G[6] = word(rt, G[2] - 0xCu); G[2] = frame.at(0x24u); G[5] = G[18] + 0u; put(rt, frame.at(0x24u), G[6]); G[20] = G[18] - 0xCu; G[3] = word(rt, G[18] - 0xCu); G[4] = G[3] < G[6] ? 1u : 0u; put(rt, frame.at(0x28u), G[3]); if (G[4] == 0u) G[7] = G[2]; G[17] = G[6] - G[3]; G[4] = word(rt, G[16] + 0x8u); G[6] = word(rt, G[7] + 0x0u); call_here(rt, ctx, 0x08B36170u); G[3] = high_half(0x08D74FD0u); G[23] = 0x08D74FD0u; { const bool tk = G[20] == G[23]; if (G[2] != 0u) G[17] = G[2]; if (tk) goto L_089BC9D4; } G[4] = G[18] - 0x4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[4] = G[20] + 0u; if (tk) goto L_089BC9D4; } G[5] = frame.at(0x2u); call_here(rt, ctx, 0x08B8542Cu); L_089BC9D4: ; { const bool tk = G[17] != 0u; G[31] = word(rt, frame.at(0x64u)); if (tk) goto L_089BCB14; } G[2] = word(rt, G[21] + 0x3Cu); { const bool tk = G[2] == 0u; G[3] = G[21] + 0x3Cu; if (tk) goto L_089BCB14; } G[2] = word(rt, G[2] + 0x4u); if (G[2] == 0u) { G[30] = word(rt, frame.at(0x60u)); goto L_089BCB18; } G[2] = word(rt, G[3] + 0x0u); goto L_089BCB08; L_089BC9FC: ; G[4] = G[16] + 0u; call_here(rt, ctx, 0x08B5C094u); G[4] = word(rt, G[16] + 0x8u); G[17] = G[4] - 0xCu; if (G[23] == G[17]) { G[4] = G[16] + 0u; goto L_089BCA38; } G[4] = G[4] - 0x4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[4] = G[16] + 0u; if (tk) goto L_089BCA38; } G[4] = G[17] + 0u; G[5] = frame.at(0x1u); call_here(rt, ctx, 0x08B8542Cu); G[4] = G[16] + 0u; L_089BCA38: ; G[5] = 0xCu; G[6] = 0x4u; call_here(rt, ctx, 0x08A5BC54u); G[4] = word(rt, G[21] + 0x3Cu); if (G[4] == 0u) { G[4] = 0u + 0u; goto L_089BCA60; } G[2] = word(rt, G[4] + 0x4u); if (G[2] != 0u) { G[2] = word(rt, G[4] + 0x0u); goto L_089BCA64; } G[4] = 0u + 0u; L_089BCA60: ; G[2] = word(rt, G[4] + 0x0u); L_089BCA64: ; G[7] = G[19] + 0u; G[5] = G[30] + 0u; G[3] = word(rt, G[2] + 0x100u); G[6] = G[22] + 0u; { const std::uint32_t callee = G[3]; G[18] = frame.at(0x10u); call_here(rt, ctx, callee); } G[2] = high_half(0x08CA5D44u); G[5] = 0x08CA5D44u; G[4] = G[18] + 0u; G[6] = frame.at(0x4u); call_here(rt, ctx, 0x08B85EB8u); G[4] = 0xCu; G[5] = 0x4u; G[17] = word(rt, G[19] + 0x0u); call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2] + 0x8u; { const bool tk = G[4] == 0u; G[16] = G[2] + 0u; if (tk) goto L_089BCAB4; } G[5] = G[18] + 0u; call_here(rt, ctx, 0x08B88F44u); L_089BCAB4: ; G[4] = G[16] + 0u; G[5] = G[17] + 0u; call_here(rt, ctx, 0x08B5C078u); G[4] = word(rt, frame.at(0x10u)); G[16] = G[4] - 0xCu; { const bool tk = G[23] == G[16]; G[31] = word(rt, frame.at(0x64u)); if (tk) goto L_089BCB14; } G[4] = G[4] - 0x4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) > 0) { G[31] = word(rt, frame.at(0x64u)); goto L_089BCB14; } G[4] = G[16] + 0u; G[5] = G[29] + 0u; call_here(rt, ctx, 0x08B8542Cu); G[31] = word(rt, frame.at(0x64u)); goto L_089BCB14; L_089BCAF8: ; { const bool tk = G[2] != 0u; G[23] = frame.at(0x20u); if (tk) goto L_089BC850; } G[31] = word(rt, frame.at(0x64u)); goto L_089BCB14; L_089BCB08: ; if (G[2] != 0u) { G[16] = word(rt, G[19] + 0x0u); goto L_089BC9FC; } L_089BCB10: ; G[31] = word(rt, frame.at(0x64u)); L_089BCB14: ; G[30] = word(rt, frame.at(0x60u)); L_089BCB18: ; G[23] = word(rt, frame.at(0x5Cu)); G[22] = word(rt, frame.at(0x58u)); G[21] = word(rt, frame.at(0x54u)); G[20] = word(rt, frame.at(0x50u)); G[19] = word(rt, frame.at(0x4Cu)); G[18] = word(rt, frame.at(0x48u)); G[17] = word(rt, frame.at(0x44u)); G[16] = word(rt, frame.at(0x40u)); goto exit_; goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }
void func_089C970C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0xB0u); put(rt, frame.at(0xA8u), G[22]); G[22] = high_half(0x08CF4E98u); put(rt, frame.at(0xA0u), G[20]); G[20] = G[4] + 0u; G[4] = 0x08CF4E98u; put(rt, frame.at(0xACu), G[31]); put(rt, frame.at(0xA4u), G[21]); G[21] = G[5] + 0u; put(rt, frame.at(0x9Cu), G[19]); G[19] = frame.at(0x4u); put(rt, frame.at(0x98u), G[18]); G[18] = G[7] + 0u; put(rt, frame.at(0x94u), G[17]); G[17] = G[6] + 0u; put(rt, frame.at(0x90u), G[16]); call_here(rt, ctx, 0x08B344E0u); G[2] = high_half(0x08D69524u); G[4] = word(rt, 0x08D69524u); G[16] = frame.at(0x8u); call_here(rt, ctx, 0x08B49634u); G[7] = word(rt, G[17] + 0x0u); G[9] = word(rt, G[18] + 0x0u); G[6] = high_half(0x08CA6664u); G[8] = G[2] + 0u; G[4] = G[16] + 0u; G[6] = 0x08CA6664u; G[5] = 0x80u; call_here(rt, ctx, 0x08B3767Cu); G[5] = G[16] + 0u; G[6] = frame.at(0x1u); G[16] = G[20] + 0x48u; G[4] = G[19] + 0u; call_here(rt, ctx, 0x08B85EB8u); G[4] = G[16] + 0u; G[5] = G[19] + 0u; call_here(rt, ctx, 0x08C00EB8u); G[3] = word(rt, frame.at(0x4u)); G[17] = G[2] + 0u; G[2] = high_half(0x08D74FD0u); G[16] = G[3] - 0xCu; G[2] = 0x08D74FD0u; G[5] = 0xFFFFFFFFu; { const bool tk = G[16] == G[2]; G[4] = G[3] - 0x4u; if (tk) goto L_089C97DC; } call_here(rt, ctx, 0x08B60804u); G[4] = G[16] + 0u; { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[5] = G[29] + 0u; if (tk) goto L_089C97DC; } call_here(rt, ctx, 0x08B8542Cu); L_089C97DC: ; G[2] = G[20] + 0x4Cu; G[3] = G[17] + 0x14u; { const bool tk = G[17] == G[2]; G[16] = 0u + 0u; if (tk) goto L_089C9800; } G[4] = G[21] + 0u; { const bool tk = G[3] == G[21]; G[16] = 0x1u; if (tk) goto L_089C9800; } G[5] = word(rt, G[17] + 0x14u); call_here(rt, ctx, 0x08A5BAD8u); L_089C9800: ; G[4] = G[22] + sext(0x08CF4E98u, 16u); call_here(rt, ctx, 0x08B3457Cu); G[2] = G[16] + 0u; G[31] = word(rt, frame.at(0xACu)); G[22] = word(rt, frame.at(0xA8u)); G[21] = word(rt, frame.at(0xA4u)); G[20] = word(rt, frame.at(0xA0u)); G[19] = word(rt, frame.at(0x9Cu)); G[18] = word(rt, frame.at(0x98u)); G[17] = word(rt, frame.at(0x94u)); G[16] = word(rt, frame.at(0x90u)); goto exit_; goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }
void func_089F11A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x30u), G[31]); put(rt, frame.at(0x2Cu), G[23]); G[23] = G[6] + 0u; put(rt, frame.at(0x28u), G[22]); G[22] = G[4] + 0u; put(rt, frame.at(0x24u), G[21]); G[21] = 0u + 0u; put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); G[19] = G[7] + 0u; G[20] = G[19] + 0u; put(rt, frame.at(0x18u), G[18]); G[18] = G[22] + 0x20u; put(rt, frame.at(0x14u), G[17]); G[17] = G[23] << 2u; put(rt, frame.at(0x10u), G[16]); G[16] = G[5] + 0u; call_here(rt, ctx, 0x089FD978u); G[2] = high_half(0x08CBA5F8u); G[2] = 0x08CBA5F8u; G[5] = G[16] + 0u; put(rt, G[22] + 0x0u, G[2]); G[4] = G[22] + 0x1Cu; call_here(rt, ctx, 0x08A76ABCu); G[4] = G[17] + 0u; G[5] = 0x4u; put(rt, G[22] + 0x20u, 0u); G[16] = frame.at(0x4u); put(rt, G[18] + 0x4u, 0u); put(rt, G[18] + 0x8u, 0u); call_here(rt, ctx, 0x08A5BC20u); G[19] = G[2] + 0u; G[2] = G[2] + G[17]; put(rt, G[18] + 0x8u, G[2]); G[4] = G[16] + 0u; put(rt, G[22] + 0x20u, G[19]); put(rt, G[18] + 0x4u, G[19]); call_here(rt, ctx, 0x08A76AA0u); G[4] = G[19] + 0u; G[5] = G[23] + 0u; G[6] = G[16] + 0u; G[7] = G[29] + 0u; call_here(rt, ctx, 0x08B85AF4u); G[4] = G[16] + 0u; call_here(rt, ctx, 0x08A5B9ECu); G[19] = word(rt, G[22] + 0x20u); G[17] = G[17] + G[19]; { const bool tk = static_cast<std::int32_t>(G[23]) > 0; put(rt, G[18] + 0x4u, G[17]); if (tk) goto L_089F1274; } G[31] = word(rt, frame.at(0x30u)); goto L_089F129C; L_089F1270: ; G[19] = word(rt, G[22] + 0x20u); L_089F1274: ; G[2] = G[21] << 2u; G[2] = G[2] + G[19]; G[21] = G[21] + 0x1u; { const bool tk = G[20] == G[2]; G[4] = G[2] + 0u; if (tk) goto L_089F1290; } G[5] = word(rt, G[20] + 0x0u); call_here(rt, ctx, 0x08A5BAD8u); L_089F1290: ; { const bool tk = G[23] != G[21]; G[20] = G[20] + 0x4u; if (tk) goto L_089F1270; } G[31] = word(rt, frame.at(0x30u)); L_089F129C: ; G[23] = word(rt, frame.at(0x2Cu)); G[22] = word(rt, frame.at(0x28u)); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); goto exit_; goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }
// 0x08A2F8A8 (a0 = the result, a2 = the argument count, a3 = the arguments): fills a request on the frame with its defaults (sizes 0x800 / 0x3000,
// rates 5000000 / 6000000 / 1000000, retries 0x4E20, flags) and takes up to five integer arguments (their virtual +0x3C: the mode, then, for
// mode 1, a second value; a third; a size x 2; a fifth). The global service (0x08CF6648 through 0x08B09A04) gets a callback (0x08B0A1F0) and
// starts the request (0x08B30284); the result is a boolean value (0x08A97A6C, assigned with 0x08A5BAD8): the start's own answer when the mode
// was set, false when it could not start, otherwise the job's outcome after polling every 500 (0x08B2E2B8) until its state (+0xC) fails or its
// progress (+0x12A8) is positive.
void func_08A2F8A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 result = ctx.gpr[4], count = ctx.gpr[6], arguments = ctx.gpr[7];
    GuestFrame frame(ctx, 0x80u);
    SavedRegisters saved(rt, ctx, frame, {{19u, 0x6Cu}, {18u, 0x68u}, {17u, 0x64u}, {31u, 0x70u}, {16u, 0x60u}});
    ctx.gpr[19] = result;
    ctx.gpr[18] = arguments;
    ctx.gpr[17] = count;
    if (static_cast<s32>(count) <= 0) return;
    const u32 request = frame.at(0xCu);
    put(rt, frame.at(0x10u), 4u);
    put(rt, frame.at(0x1Cu), 0x800u);
    put(rt, frame.at(0x20u), 0x004C4B40u);
    put(rt, frame.at(0x24u), 0x005B8D80u);
    put(rt, frame.at(0x2Cu), 0x000F4240u);
    put(rt, frame.at(0x28u), 6u);
    put(rt, frame.at(0x38u), 0x3000u);
    put(rt, frame.at(0x3Cu), 0x11u);
    store8(frame.at(0x4Cu), 1u);
    put(rt, frame.at(0x50u), 0x4E20u);
    put(rt, frame.at(0xCu), 0u);
    put(rt, frame.at(0x14u), 0u);
    store16(frame.at(0x18u), 1u);
    put(rt, frame.at(0x30u), 0u);
    put(rt, frame.at(0x34u), 0x11u);
    put(rt, frame.at(0x40u), 0u);
    put(rt, frame.at(0x44u), 0u);
    put(rt, frame.at(0x48u), 0u);
    auto integer = [&](u32 index) {
        const u32 argument = word(rt, arguments + 4u * index);
        ctx.gpr[16] = argument;
        if (argument == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAC378u});
        const u32 method = word(rt, word(rt, argument) + 0x3Cu);
        ctx.gpr[3] = word(rt, argument);
        ctx.gpr[2] = method;
        return call_set(rt, ctx, method, {argument});
    };
    const s32 n = static_cast<s32>(count);
    const u32 mode = integer(0u);
    put(rt, frame.at(0xCu), mode);
    if (n >= 2 && mode == 1u) put(rt, frame.at(0x10u), integer(1u));
    if (n >= 3) put(rt, frame.at(0x14u), integer(2u));
    if (n >= 4) put(rt, frame.at(0x1Cu), integer(3u) << 1);
    if (n >= 5) put(rt, frame.at(0x30u), integer(4u));
    ctx.gpr[16] = 0x08CF0000u;
    const u32 jobs = call_set(rt, ctx, 0x08B09A04u, {word(rt, 0x08CF6648u)});
    const u32 service = word(rt, 0x08CF6648u);
    put(rt, jobs + 0x14u, 0x08B0A1F0u);
    put(rt, jobs + 0x18u, service);
    const u32 job = call_set(rt, ctx, 0x08B09A04u, {service});
    ctx.gpr[16] = job;
    const u32 started = call_set(rt, ctx, 0x08B30284u, {job, request});
    // the boolean value made into `slot` and assigned to the result
    auto answer = [&](u32 slot, u32 value) {
        ctx.gpr[16] = slot;
        call_set(rt, ctx, 0x08A97A6Cu, {slot, value});
        if (result != slot) call_set(rt, ctx, 0x08A5BAD8u, {result, word(rt, slot)});
        call_set(rt, ctx, 0x08A5B9ECu, {slot});
    };
    if (word(rt, frame.at(0xCu)) != 0u) {
        answer(frame.at(0u), started);
        return;
    }
    if (started == 0u) {
        answer(frame.at(0x8u), 0u);
        return;
    }
    u32 outcome;
    for (;;) {
        if (static_cast<s32>(word(rt, job + 0xCu)) < 0) {
            outcome = 0u;
            break;
        }
        if (static_cast<s32>(word(rt, job + 0x12A8u)) > 0) {
            outcome = 1u;
            break;
        }
        call_set(rt, ctx, 0x08B2E2B8u, {0x1F4u});
    }
    answer(frame.at(0x4u), outcome);
}
// 0x08A4ABE4 (a0 = the result, a1 = the argument count, a2 = the arguments): with an argument, its string (virtual +0xC) is parsed into a
// 0x3C-byte record (0x08B0EC60) and turned into a table (0x08A6BA44) of six named values: integers from the record's +0x0, +0x4, +0x8, +0x18
// (0x08A85B1C), a 64-bit one from +0x10 / +0x14 (0x08A5DEE8) and a zero, each stored at the slot 0x08A6BBE8 returns for its key (0x08CAEFD8,
// 0x08CAEFE0, 0x08CAEFE8, 0x08CAEFF4, 0x08CAF004, 0x08CAF00C). The table becomes the result (0x08A76ABC, 0x08A5BAD8); the string is released.
void func_08A4ABE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 result = ctx.gpr[4], count = ctx.gpr[5], arguments = ctx.gpr[6];
    GuestFrame frame(ctx, 0xD0u);
    SavedRegisters saved(rt, ctx, frame, {{19u, 0xBCu}, {31u, 0xC0u}, {18u, 0xB8u}, {17u, 0xB4u}, {16u, 0xB0u}});
    ctx.gpr[19] = result;
    if (static_cast<s32>(count) <= 0) return;
    const u32 argument = word(rt, arguments);
    ctx.gpr[16] = argument;
    if (argument == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAED38u});
    const u32 text = frame.at(0x20u), parsed = frame.at(0x70u), record = frame.at(0x28u);
    const u32 method = word(rt, word(rt, argument) + 0xCu);
    ctx.gpr[2] = word(rt, argument);
    ctx.gpr[3] = method;
    ctx.gpr[16] = parsed;
    call_set(rt, ctx, method, {text, argument});
    call_set(rt, ctx, 0x08B0EC60u, {parsed, word(rt, text)});
    for (u32 at = 0u; at < 0x30u; at += 0x10u)
        for (u32 w = 0u; w < 0x10u; w += 4u) put(rt, record + at + w, word(rt, parsed + at + w));
    const u32 w8 = word(rt, parsed + 0x38u), w0 = word(rt, parsed + 0x30u), w4 = word(rt, parsed + 0x34u);
    put(rt, record + 0x34u, w4);
    put(rt, record + 0x38u, w8);
    put(rt, record + 0x30u, w0);
    const u32 table = frame.at(0x24u);
    ctx.gpr[18] = table;
    ctx.gpr[17] = frame.at(0x1Cu);
    call_set(rt, ctx, 0x08A6BA44u, {table});
    // slot(key) = a value made into `temp` by `make`, then the temporary destroyed.
    auto store = [&](u32 key, u32 temp, auto make) {
        const u32 slot = call_set(rt, ctx, 0x08A6BBE8u, {table, key});
        ctx.gpr[16] = temp;
        ctx.gpr[17] = slot;
        make(temp);
        if (slot != temp) call_set(rt, ctx, 0x08A5BAD8u, {slot, word(rt, temp)});
        call_set(rt, ctx, 0x08A5B9ECu, {temp});
    };
    auto integer = [&](u32 field) { return [&, field](u32 temp) { call_set(rt, ctx, 0x08A85B1Cu, {temp, field}); }; };
    {  // the first key keeps its slot in s0 and the temporary in s1, the others the other way round
        const u32 slot = call_set(rt, ctx, 0x08A6BBE8u, {table, 0x08CAEFD8u});
        const u32 temp = frame.at(0x1Cu);
        ctx.gpr[16] = slot;
        call_set(rt, ctx, 0x08A85B1Cu, {temp, word(rt, frame.at(0x28u))});
        if (slot != temp) call_set(rt, ctx, 0x08A5BAD8u, {slot, word(rt, temp)});
        call_set(rt, ctx, 0x08A5B9ECu, {temp});
    }
    store(0x08CAEFE0u, frame.at(0x18u), integer(word(rt, frame.at(0x2Cu))));
    store(0x08CAEFE8u, frame.at(0x14u), integer(word(rt, frame.at(0x30u))));
    store(0x08CAEFF4u, frame.at(0x10u), integer(word(rt, frame.at(0x40u))));
    store(0x08CAF004u, frame.at(0xCu), [&](u32 temp) {
        call_set(rt, ctx, 0x08A5DEE8u, {temp, ctx.gpr[5], word(rt, frame.at(0x38u)), word(rt, frame.at(0x3Cu))});
    });
    {
        const u32 slot = call_set(rt, ctx, 0x08A6BBE8u, {table, 0x08CAF00Cu});
        const u32 temp = frame.at(0x8u);
        ctx.gpr[17] = temp;
        ctx.gpr[16] = slot;
        call_set(rt, ctx, 0x08A85B1Cu, {temp, 0u});
        if (slot != temp) call_set(rt, ctx, 0x08A5BAD8u, {slot, word(rt, temp)});
        call_set(rt, ctx, 0x08A5B9ECu, {temp});
    }
    const u32 made = frame.at(0x4u);
    ctx.gpr[16] = made;
    call_set(rt, ctx, 0x08A76ABCu, {made, table});
    if (result != made) call_set(rt, ctx, 0x08A5BAD8u, {result, word(rt, made)});
    call_set(rt, ctx, 0x08A5B9ECu, {made});
    call_set(rt, ctx, 0x08A5B9ECu, {table});
    const u32 data = word(rt, text), header = data - 0xCu;
    ctx.gpr[2] = 0x08D74FD0u;
    ctx.gpr[16] = header;
    ctx.gpr[4] = data;
    if (header == 0x08D74FD0u) return;
    if (static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {data - 0x4u, 0xFFFFFFFFu})) > 0) return;
    call_set(rt, ctx, 0x08B8542Cu, {header, frame.at(0u)});
}
void func_08A5B070(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x3Cu), G[23]); G[23] = G[5] + 0u; put(rt, frame.at(0x38u), G[22]); G[22] = G[4] + 0u; put(rt, frame.at(0x34u), G[21]); put(rt, frame.at(0x44u), G[31]); put(rt, frame.at(0x40u), G[30]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x20u), G[16]); G[16] = word(rt, G[6] + 0x0u); { const bool tk = G[16] != 0u; G[21] = G[6] + 0u; if (tk) goto L_08A5B0BC; } G[4] = high_half(0x08CAFB14u); G[4] = 0x08CAFB14u; call_here(rt, ctx, 0x08A5BB7Cu); L_08A5B0BC: ; G[3] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[3] + 0x78u); { const std::uint32_t callee = G[2]; G[4] = G[16] + 0u; call_here(rt, ctx, callee); } { const bool tk = G[2] == 0u; G[16] = word(rt, G[21] + 0x0u); if (tk) goto L_08A5B208; } if (G[16] != 0u) { G[3] = word(rt, G[16] + 0x0u); goto L_08A5B0EC; } G[4] = high_half(0x08CAFB14u); G[4] = 0x08CAFB14u; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x0u); L_08A5B0EC: ; G[2] = word(rt, G[3] + 0x7Cu); { const std::uint32_t callee = G[2]; G[4] = G[16] + 0u; call_here(rt, ctx, callee); } G[3] = word(rt, G[22] + 0x3Cu); G[20] = G[2] + 0u; G[2] = G[3] < G[2] ? 1u : 0u; if (G[2] != 0u) G[20] = G[3]; { const bool tk = G[20] == 0u; G[31] = word(rt, frame.at(0x44u)); if (tk) goto L_08A5B2A4; } G[2] = G[22] + 0x38u; G[19] = 0u + 0u; put(rt, frame.at(0x10u), G[2]); G[30] = G[23] + 0x18u; G[16] = word(rt, G[22] + 0x38u); L_08A5B124: ; if (G[16] != 0u) { G[3] = word(rt, G[16] + 0x0u); goto L_08A5B13C; } G[2] = high_half(0x08CAFB04u); G[4] = 0x08CAFB04u; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x0u); L_08A5B13C: ; G[2] = word(rt, G[3] + 0x120u); { const std::uint32_t callee = G[2]; G[4] = G[16] + 0u; call_here(rt, ctx, callee); } { const auto product = static_cast<std::int64_t>(static_cast<std::int32_t>(G[19])) * static_cast<std::int32_t>(G[2]); ctx.lo = static_cast<std::uint32_t>(product); ctx.hi = static_cast<std::uint32_t>(static_cast<std::uint64_t>(product) >> 32); } G[2] = word(rt, G[23] + 0x1Cu); G[16] = ctx.lo; { const bool tk = G[2] != 0u; G[7] = G[2] + G[16]; if (tk) goto L_08A5B17C; } G[17] = word(rt, G[23] + 0x18u); if (G[17] != 0u) { G[2] = word(rt, G[17] + 0xCu); goto L_08A5B178; } G[2] = high_half(0x08CAFB0Cu); G[4] = 0x08CAFB0Cu; call_here(rt, ctx, 0x08A5BB7Cu); G[2] = word(rt, G[17] + 0xCu); L_08A5B178: ; G[7] = G[2] + G[16]; L_08A5B17C: ; G[5] = word(rt, frame.at(0x10u)); G[4] = frame.at(0x4u); G[6] = G[30] + 0u; call_here(rt, ctx, 0x08A57B54u); G[17] = word(rt, frame.at(0x4u)); if (G[17] != 0u) { G[2] = word(rt, G[17] + 0x0u); goto L_08A5B1A8; } G[2] = high_half(0x08CAFBB4u); G[4] = 0x08CAFBB4u; call_here(rt, ctx, 0x08A5BB7Cu); G[2] = word(rt, G[17] + 0x0u); L_08A5B1A8: ; G[16] = word(rt, G[21] + 0x0u); { const bool tk = G[16] != 0u; G[18] = word(rt, G[2] + 0xF4u); if (tk) goto L_08A5B1C0; } G[2] = high_half(0x08CAFB14u); G[4] = 0x08CAFB14u; call_here(rt, ctx, 0x08A5BB7Cu); L_08A5B1C0: ; G[2] = word(rt, G[16] + 0x0u); G[6] = G[19] + 0u; G[5] = G[16] + 0u; G[3] = word(rt, G[2] + 0x84u); G[4] = G[29] + 0u; { const std::uint32_t callee = G[3]; G[19] = G[19] + 0x1u; call_here(rt, ctx, callee); } G[4] = G[17] + 0u; { const std::uint32_t callee = G[18]; G[5] = G[29] + 0u; call_here(rt, ctx, callee); } G[4] = G[29] + 0u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = frame.at(0x4u); call_here(rt, ctx, 0x08A5B9ECu); if (G[20] != G[19]) { G[16] = word(rt, G[22] + 0x38u); goto L_08A5B124; } G[31] = word(rt, frame.at(0x44u)); goto L_08A5B2A4; L_08A5B208: ; if (G[16] != 0u) { G[3] = word(rt, G[16] + 0x0u); goto L_08A5B220; } G[4] = high_half(0x08CAFB14u); G[4] = 0x08CAFB14u; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x0u); L_08A5B220: ; G[2] = word(rt, G[3] + 0xD4u); { const std::uint32_t callee = G[2]; G[4] = G[16] + 0u; call_here(rt, ctx, callee); } { const bool tk = G[2] == 0u; G[31] = word(rt, frame.at(0x44u)); if (tk) goto L_08A5B2A4; } G[2] = word(rt, G[23] + 0x0u); G[16] = word(rt, G[21] + 0x0u); { const bool tk = G[16] != 0u; G[18] = word(rt, G[2] + 0xE0u); if (tk) goto L_08A5B250; } G[4] = high_half(0x08CAFB14u); G[4] = 0x08CAFB14u; call_here(rt, ctx, 0x08A5BB7Cu); L_08A5B250: ; G[3] = word(rt, G[16] + 0x0u); G[4] = G[16] + 0u; G[2] = word(rt, G[3] + 0xDCu); { const std::uint32_t callee = G[2]; G[5] = 0u + 0u; call_here(rt, ctx, callee); } G[16] = word(rt, G[21] + 0x0u); { const bool tk = G[16] != 0u; G[17] = G[2] + 0u; if (tk) goto L_08A5B27C; } G[4] = high_half(0x08CAFB14u); G[4] = 0x08CAFB14u; call_here(rt, ctx, 0x08A5BB7Cu); L_08A5B27C: ; G[3] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[3] + 0xD8u); { const std::uint32_t callee = G[2]; G[4] = G[16] + 0u; call_here(rt, ctx, callee); } G[7] = G[2] + 0u; G[4] = G[23] + 0u; G[6] = G[17] + 0u; { const std::uint32_t callee = G[18]; G[5] = 0u + 0u; call_here(rt, ctx, callee); } G[31] = word(rt, frame.at(0x44u)); L_08A5B2A4: ; G[30] = word(rt, frame.at(0x40u)); G[23] = word(rt, frame.at(0x3Cu)); G[22] = word(rt, frame.at(0x38u)); G[21] = word(rt, frame.at(0x34u)); G[20] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); goto exit_; goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }
void func_08A81C60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x24u), G[17]); G[17] = G[4] + 0u; put(rt, frame.at(0x20u), G[16]); put(rt, frame.at(0x44u), G[31]); put(rt, frame.at(0x40u), G[30]); put(rt, frame.at(0x3Cu), G[23]); put(rt, frame.at(0x38u), G[22]); put(rt, frame.at(0x34u), G[21]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x28u), G[18]); G[2] = word(rt, G[5] + 0x0u); { const bool tk = G[2] == 0u; G[16] = G[5] + 0u; if (tk) goto L_08A81E1C; } G[3] = word(rt, G[2] + 0x0u); G[4] = G[2] + 0u; G[2] = word(rt, G[3] + 0x90u); { const std::uint32_t callee = G[2];  call_here(rt, ctx, callee); } { const bool tk = G[2] == 0u; G[31] = word(rt, frame.at(0x44u)); if (tk) goto L_08A81E20; } G[16] = word(rt, G[16] + 0x0u); if (G[16] != 0u) { G[2] = word(rt, G[16] + 0x0u); goto L_08A81CD4; } G[4] = high_half(0x08CB15A0u); G[4] = 0x08CB15A0u; call_here(rt, ctx, 0x08A5BB7Cu); G[2] = word(rt, G[16] + 0x0u); L_08A81CD4: ; G[21] = frame.at(0x8u); G[5] = G[16] + 0u; G[3] = word(rt, G[2] + 0xA8u); G[4] = G[21] + 0u; { const std::uint32_t callee = G[3]; G[23] = high_half(0x08CB1604u); call_here(rt, ctx, callee); } G[22] = frame.at(0xCu); G[30] = high_half(0x08CB15A0u); G[19] = frame.at(0x4u); G[20] = frame.at(0x10u); goto L_08A81DBC; L_08A81D00: ; G[4] = G[23] + sext(0x08CB1604u, 16u); call_here(rt, ctx, 0x08A5BB7Cu); G[5] = word(rt, G[16] + 0x14u); L_08A81D0C: ; G[4] = G[22] + 0u; G[5] = G[5] + 0x4u; call_here(rt, ctx, 0x08A76ABCu); G[16] = word(rt, frame.at(0xCu)); if (G[16] != 0u) { G[3] = word(rt, G[16] + 0x0u); goto L_08A81D30; } G[4] = G[30] + sext(0x08CB15A0u, 16u); call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x0u); L_08A81D30: ; G[5] = G[16] + 0u; G[4] = G[19] + 0u; G[2] = word(rt, G[3] + 0x84u); { const std::uint32_t callee = G[2]; G[6] = 0u + 0u; call_here(rt, ctx, callee); } G[4] = G[20] + 0u; G[5] = G[17] + 0u; G[6] = G[19] + 0u; call_here(rt, ctx, 0x08A818A0u); G[4] = G[19] + 0u; call_here(rt, ctx, 0x08A5B9ECu); G[2] = word(rt, frame.at(0x10u)); { const bool tk = G[2] == 0u; G[16] = word(rt, frame.at(0xCu)); if (tk) goto L_08A81DAC; } G[2] = word(rt, G[17] + 0x0u); { const bool tk = G[16] != 0u; G[18] = word(rt, G[2] + 0xA0u); if (tk) goto L_08A81D7C; } G[4] = G[30] + sext(0x08CB15A0u, 16u); call_here(rt, ctx, 0x08A5BB7Cu); L_08A81D7C: ; G[3] = word(rt, G[16] + 0x0u); G[5] = G[16] + 0u; G[4] = G[29] + 0u; G[2] = word(rt, G[3] + 0x84u); { const std::uint32_t callee = G[2]; G[6] = 0x1u; call_here(rt, ctx, callee); } G[4] = G[17] + 0u; G[5] = G[20] + 0u; { const std::uint32_t callee = G[18]; G[6] = G[29] + 0u; call_here(rt, ctx, callee); } G[4] = G[29] + 0u; call_here(rt, ctx, 0x08A5B9ECu); L_08A81DAC: ; G[4] = G[20] + 0u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[22] + 0u; call_here(rt, ctx, 0x08A5B9ECu); L_08A81DBC: ; G[16] = word(rt, frame.at(0x8u)); if (G[16] != 0u) { G[4] = word(rt, G[16] + 0x14u); goto L_08A81DD4; } G[4] = G[23] + sext(0x08CB1604u, 16u); call_here(rt, ctx, 0x08A5BB7Cu); G[4] = word(rt, G[16] + 0x14u); L_08A81DD4: ; G[3] = word(rt, G[4] + 0x0u); G[2] = word(rt, G[3] + 0x8u); { const std::uint32_t callee = G[2];  call_here(rt, ctx, callee); } if (G[2] == 0u) goto L_08A81E14; G[4] = word(rt, G[16] + 0x14u); G[3] = word(rt, G[4] + 0x0u); G[2] = word(rt, G[3] + 0xCu); { const std::uint32_t callee = G[2];  call_here(rt, ctx, callee); } G[16] = word(rt, frame.at(0x8u)); if (G[16] != 0u) { G[5] = word(rt, G[16] + 0x14u); goto L_08A81D0C; } goto L_08A81D00; L_08A81E14: ; G[4] = G[21] + 0u; call_here(rt, ctx, 0x08A5B9ECu); L_08A81E1C: ; G[31] = word(rt, frame.at(0x44u)); L_08A81E20: ; G[30] = word(rt, frame.at(0x40u)); G[23] = word(rt, frame.at(0x3Cu)); G[22] = word(rt, frame.at(0x38u)); G[21] = word(rt, frame.at(0x34u)); G[20] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); goto exit_; goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }
void func_08A8D308(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x40u), G[30]); G[30] = G[4] + 0u; put(rt, frame.at(0x3Cu), G[23]); G[23] = G[7] + 0u; put(rt, frame.at(0x38u), G[22]); G[22] = G[5] + 0u; put(rt, frame.at(0x30u), G[20]); G[20] = G[6] + 0u; put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x44u), G[31]); put(rt, frame.at(0x34u), G[21]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x20u), G[16]); G[16] = word(rt, G[6] + 0x0u); { const bool tk = G[16] != 0u; G[19] = G[8] + 0u; if (tk) goto L_08A8D35C; } G[4] = high_half(0x08CB1D54u); G[4] = 0x08CB1D54u; call_here(rt, ctx, 0x08A5BB7Cu); L_08A8D35C: ; G[4] = G[16] + 0x24u; G[5] = G[23] + 0u; call_here(rt, ctx, 0x08C2B010u); G[17] = word(rt, G[20] + 0x0u); { const bool tk = G[17] != 0u; G[16] = G[2] + 0u; if (tk) goto L_08A8D380; } G[4] = high_half(0x08CB1D54u); G[4] = 0x08CB1D54u; call_here(rt, ctx, 0x08A5BB7Cu); L_08A8D380: ; G[2] = G[17] + 0x28u; { const bool tk = G[2] == G[16]; G[18] = 0x8u; if (tk) goto L_08A8D3DC; } if (G[19] == G[18]) { G[16] = word(rt, G[20] + 0x0u); goto L_08A8D3BC; } G[17] = word(rt, G[16] + 0x14u); { const bool tk = G[17] != 0u; G[16] = G[16] + 0x14u; if (tk) goto L_08A8D3AC; } G[4] = high_half(0x08CB1D60u); G[4] = 0x08CB1D60u; call_here(rt, ctx, 0x08A5BB7Cu); L_08A8D3AC: ; G[2] = word(rt, G[17] + 0x14u); { const bool tk = G[2] != G[18]; G[5] = G[16] + 0u; if (tk) goto L_08A8D538; } G[16] = word(rt, G[20] + 0x0u); L_08A8D3BC: ; if (G[16] != 0u) { G[4] = G[16] + 0u; goto L_08A8D3D4; } G[4] = high_half(0x08CB1D54u); G[4] = 0x08CB1D54u; call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16] + 0u; L_08A8D3D4: ; G[5] = G[23] + 0u; call_here(rt, ctx, 0x08AAE5F0u); L_08A8D3DC: ; G[21] = frame.at(0x10u); G[5] = high_half(0x08CF6ED8u); G[5] = 0x08CF6ED8u; G[4] = G[21] + 0u; call_here(rt, ctx, 0x08A5BAB8u); G[2] = 0x5u; { const bool tk = G[19] == G[2]; G[2] = 0x1u; if (tk) goto L_08A8D404; } if (G[19] != G[2]) { G[18] = word(rt, G[22] + 0x78u); goto L_08A8D4A0; } L_08A8D404: ; G[17] = word(rt, G[22] + 0x78u); G[4] = 0x48u; G[5] = 0x4u; call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2] + 0u; G[5] = G[20] + 0u; G[6] = G[23] + 0u; G[7] = G[19] + 0u; G[9] = G[17] + 0u; G[8] = G[22] + 0x74u; G[16] = G[2] + 0u; call_here(rt, ctx, 0x08AAEBF0u); G[2] = high_half(0x08CD5618u); G[3] = G[16] + 0x24u; G[6] = G[16] + 0x28u; G[7] = G[16] + 0x3Cu; G[17] = frame.at(0xCu); G[2] = 0x08CD5618u; put(rt, G[16] + 0x0u, G[2]); G[4] = G[17] + 0u; G[5] = frame.at(0x4u); put(rt, G[3] + 0x10u, G[6]); put(rt, G[3] + 0x14u, 0u); put(rt, G[3] + 0x4u, 0u); put(rt, G[3] + 0x8u, 0u); put(rt, G[3] + 0xCu, G[6]); put(rt, G[7] + 0x8u, 0u); put(rt, G[16] + 0x3Cu, 0u); put(rt, G[7] + 0x4u, 0u); put(rt, frame.at(0x4u), G[16]); call_here(rt, ctx, 0x08AAE204u); { const bool tk = G[21] == G[17]; G[5] = word(rt, frame.at(0xCu)); if (tk) goto L_08A8D490; } G[4] = G[21] + 0u; call_here(rt, ctx, 0x08A5BAD8u); L_08A8D490: ; G[4] = G[17] + 0u; call_here(rt, ctx, 0x08A5B9ECu); G[16] = word(rt, G[20] + 0x0u); goto L_08A8D4F8; L_08A8D4A0: ; G[4] = 0x24u; G[5] = 0x4u; call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2] + 0u; G[6] = G[23] + 0u; G[7] = G[19] + 0u; G[8] = G[22] + 0x74u; G[9] = G[18] + 0u; G[5] = G[20] + 0u; G[16] = frame.at(0x8u); G[17] = G[2] + 0u; call_here(rt, ctx, 0x08AAEB30u); G[5] = G[29] + 0u; G[4] = G[16] + 0u; put(rt, frame.at(0x0u), G[17]); call_here(rt, ctx, 0x08A5BAB8u); G[4] = G[21] + 0u; G[5] = word(rt, frame.at(0x8u)); call_here(rt, ctx, 0x08A5BAD8u); G[4] = G[16] + 0u; call_here(rt, ctx, 0x08A5B9ECu); G[16] = word(rt, G[20] + 0x0u); L_08A8D4F8: ; if (G[16] != 0u) { G[4] = G[16] + 0u; goto L_08A8D510; } G[4] = high_half(0x08CB1D54u); G[4] = 0x08CB1D54u; call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16] + 0u; L_08A8D510: ; G[5] = G[23] + 0u; G[6] = G[21] + 0u; call_here(rt, ctx, 0x08AAE678u); G[4] = G[30] + 0u; G[5] = G[21] + 0u; call_here(rt, ctx, 0x08AAE214u); G[4] = G[21] + 0u; call_here(rt, ctx, 0x08A5B9ECu); G[2] = G[30] + 0u; goto L_08A8D544; L_08A8D538: ; G[4] = G[30] + 0u; call_here(rt, ctx, 0x08AAE214u); G[2] = G[30] + 0u; L_08A8D544: ; G[31] = word(rt, frame.at(0x44u)); G[30] = word(rt, frame.at(0x40u)); G[23] = word(rt, frame.at(0x3Cu)); G[22] = word(rt, frame.at(0x38u)); G[21] = word(rt, frame.at(0x34u)); G[20] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); goto exit_; goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }
void func_08A92A9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x70u); put(rt, frame.at(0x4Cu), G[19]); G[19] = G[7] + 0u; put(rt, frame.at(0x60u), G[30]); G[30] = G[5] + 0u; put(rt, frame.at(0x58u), G[22]); G[22] = G[6] + 0u; put(rt, frame.at(0x54u), G[21]); G[21] = G[4] + 0u; put(rt, frame.at(0x40u), G[16]); put(rt, frame.at(0x64u), G[31]); put(rt, frame.at(0x5Cu), G[23]); put(rt, frame.at(0x50u), G[20]); put(rt, frame.at(0x48u), G[18]); put(rt, frame.at(0x44u), G[17]); call_here(rt, ctx, 0x08A7ED20u); G[16] = word(rt, G[19] + 0x0u); { const bool tk = G[16] != G[19]; G[2] = high_half(0x08CB1FD0u); if (tk) goto L_08A92C10; } G[2] = word(rt, G[21] + 0x38u); { const bool tk = G[2] == 0u; G[3] = G[21] + 0x38u; if (tk) goto L_08A92DC8; } G[2] = word(rt, G[2] + 0x4u); { const bool tk = G[2] == 0u; G[31] = word(rt, frame.at(0x64u)); if (tk) goto L_08A92DCC; } G[2] = word(rt, G[3] + 0x0u); goto L_08A92DB0; L_08A92B08: ; G[5] = high_half(0x08CF6F08u); G[5] = 0x08CF6F08u; G[4] = G[23] + 0u; call_here(rt, ctx, 0x08A6972Cu); G[4] = G[22] + 0u; G[5] = G[23] + 0u; call_here(rt, ctx, 0x08BFE7D0u); G[20] = G[2] + 0u; G[2] = G[22] + 0x4u; { const bool tk = G[20] == G[2]; G[2] = word(rt, frame.at(0x20u)); if (tk) goto L_08A92B44; } G[3] = word(rt, G[20] + 0x10u); G[2] = G[2] < G[3] ? 1u : 0u; { const bool tk = G[2] == 0u; G[18] = G[20] + 0u; if (tk) goto L_08A92B9C; } L_08A92B44: ; G[17] = frame.at(0xCu); G[16] = frame.at(0x2Cu); G[4] = G[17] + 0u; call_here(rt, ctx, 0x08A5D438u); G[19] = frame.at(0x30u); G[4] = G[16] + 0u; G[5] = G[23] + 0u; call_here(rt, ctx, 0x08A6972Cu); G[4] = G[19] + 0u; G[5] = G[17] + 0u; call_here(rt, ctx, 0x08A5D424u); G[5] = G[20] + 0u; G[6] = G[16] + 0u; G[4] = G[22] + 0u; call_here(rt, ctx, 0x08BFEBACu); G[4] = G[19] + 0u; G[18] = G[2] + 0u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16] + 0u; call_here(rt, ctx, 0x08A69D68u); G[4] = G[17] + 0u; call_here(rt, ctx, 0x08A5B9ECu); L_08A92B9C: ; G[4] = word(rt, G[21] + 0x38u); { const bool tk = G[4] == 0u; G[18] = G[18] + 0x14u; if (tk) goto L_08A92BB4; } G[2] = word(rt, G[4] + 0x4u); { const bool tk = G[2] != 0u; G[3] = G[4] + 0u; if (tk) goto L_08A92BB8; } L_08A92BB4: ; G[3] = 0u + 0u; L_08A92BB8: ; G[17] = frame.at(0x18u); G[4] = G[17] + 0u; G[5] = frame.at(0x14u); G[16] = frame.at(0x1Cu); put(rt, frame.at(0x14u), G[3]); call_here(rt, ctx, 0x08A924F0u); G[4] = G[16] + 0u; G[5] = G[23] + 0u; G[6] = G[17] + 0u; call_here(rt, ctx, 0x08A91A38u); { const bool tk = G[18] == G[16]; G[5] = word(rt, frame.at(0x1Cu)); if (tk) goto L_08A92BF0; } G[4] = G[18] + 0u; call_here(rt, ctx, 0x08A5BAD8u); L_08A92BF0: ; G[4] = G[16] + 0u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[17] + 0u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[23] + 0u; call_here(rt, ctx, 0x08A69D68u); G[31] = word(rt, frame.at(0x64u)); goto L_08A92DCC; L_08A92C10: ; G[5] = G[2] + sext(0x08CB1FD0u, 16u); G[4] = frame.at(0x8u); G[6] = frame.at(0x3u); call_here(rt, ctx, 0x08B85EB8u); G[2] = word(rt, G[16] + 0x8u); G[18] = word(rt, frame.at(0x8u)); G[7] = frame.at(0x28u); G[6] = word(rt, G[2] - 0xCu); G[2] = frame.at(0x24u); G[5] = G[18] + 0u; put(rt, frame.at(0x24u), G[6]); G[20] = G[18] - 0xCu; G[3] = word(rt, G[18] - 0xCu); G[4] = G[3] < G[6] ? 1u : 0u; put(rt, frame.at(0x28u), G[3]); if (G[4] == 0u) G[7] = G[2]; G[17] = G[6] - G[3]; G[4] = word(rt, G[16] + 0x8u); G[6] = word(rt, G[7] + 0x0u); call_here(rt, ctx, 0x08B36170u); G[3] = high_half(0x08D74FD0u); G[23] = 0x08D74FD0u; { const bool tk = G[20] == G[23]; if (G[2] != 0u) G[17] = G[2]; if (tk) goto L_08A92C8C; } G[4] = G[18] - 0x4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[4] = G[20] + 0u; if (tk) goto L_08A92C8C; } G[5] = frame.at(0x2u); call_here(rt, ctx, 0x08B8542Cu); L_08A92C8C: ; { const bool tk = G[17] != 0u; G[31] = word(rt, frame.at(0x64u)); if (tk) goto L_08A92DCC; } G[2] = word(rt, G[21] + 0x38u); { const bool tk = G[2] == 0u; G[3] = G[21] + 0x38u; if (tk) goto L_08A92DCC; } G[2] = word(rt, G[2] + 0x4u); if (G[2] == 0u) { G[30] = word(rt, frame.at(0x60u)); goto L_08A92DD0; } G[2] = word(rt, G[3] + 0x0u); goto L_08A92DC0; L_08A92CB4: ; G[4] = G[16] + 0u; call_here(rt, ctx, 0x08B5C094u); G[4] = word(rt, G[16] + 0x8u); G[17] = G[4] - 0xCu; if (G[23] == G[17]) { G[4] = G[16] + 0u; goto L_08A92CF0; } G[4] = G[4] - 0x4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[4] = G[16] + 0u; if (tk) goto L_08A92CF0; } G[4] = G[17] + 0u; G[5] = frame.at(0x1u); call_here(rt, ctx, 0x08B8542Cu); G[4] = G[16] + 0u; L_08A92CF0: ; G[5] = 0xCu; G[6] = 0x4u; call_here(rt, ctx, 0x08A5BC54u); G[4] = word(rt, G[21] + 0x38u); if (G[4] == 0u) { G[4] = 0u + 0u; goto L_08A92D18; } G[2] = word(rt, G[4] + 0x4u); if (G[2] != 0u) { G[2] = word(rt, G[4] + 0x0u); goto L_08A92D1C; } G[4] = 0u + 0u; L_08A92D18: ; G[2] = word(rt, G[4] + 0x0u); L_08A92D1C: ; G[7] = G[19] + 0u; G[5] = G[30] + 0u; G[3] = word(rt, G[2] + 0x11Cu); G[6] = G[22] + 0u; { const std::uint32_t callee = G[3]; G[18] = frame.at(0x10u); call_here(rt, ctx, callee); } G[2] = high_half(0x08CB1FD0u); G[5] = 0x08CB1FD0u; G[4] = G[18] + 0u; G[6] = frame.at(0x4u); call_here(rt, ctx, 0x08B85EB8u); G[4] = 0xCu; G[5] = 0x4u; G[17] = word(rt, G[19] + 0x0u); call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2] + 0x8u; { const bool tk = G[4] == 0u; G[16] = G[2] + 0u; if (tk) goto L_08A92D6C; } G[5] = G[18] + 0u; call_here(rt, ctx, 0x08B88F44u); L_08A92D6C: ; G[4] = G[16] + 0u; G[5] = G[17] + 0u; call_here(rt, ctx, 0x08B5C078u); G[4] = word(rt, frame.at(0x10u)); G[16] = G[4] - 0xCu; { const bool tk = G[23] == G[16]; G[31] = word(rt, frame.at(0x64u)); if (tk) goto L_08A92DCC; } G[4] = G[4] - 0x4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) > 0) { G[31] = word(rt, frame.at(0x64u)); goto L_08A92DCC; } G[4] = G[16] + 0u; G[5] = G[29] + 0u; call_here(rt, ctx, 0x08B8542Cu); G[31] = word(rt, frame.at(0x64u)); goto L_08A92DCC; L_08A92DB0: ; { const bool tk = G[2] != 0u; G[23] = frame.at(0x20u); if (tk) goto L_08A92B08; } G[31] = word(rt, frame.at(0x64u)); goto L_08A92DCC; L_08A92DC0: ; if (G[2] != 0u) { G[16] = word(rt, G[19] + 0x0u); goto L_08A92CB4; } L_08A92DC8: ; G[31] = word(rt, frame.at(0x64u)); L_08A92DCC: ; G[30] = word(rt, frame.at(0x60u)); L_08A92DD0: ; G[23] = word(rt, frame.at(0x5Cu)); G[22] = word(rt, frame.at(0x58u)); G[21] = word(rt, frame.at(0x54u)); G[20] = word(rt, frame.at(0x50u)); G[19] = word(rt, frame.at(0x4Cu)); G[18] = word(rt, frame.at(0x48u)); G[17] = word(rt, frame.at(0x44u)); G[16] = word(rt, frame.at(0x40u)); goto exit_; goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }
void func_08A9FE54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x20u), G[16]); G[16] = frame.at(0xCu); G[6] = frame.at(0x1u); put(rt, frame.at(0x30u), G[20]); G[20] = G[4] + 0u; G[4] = G[16] + 0u; put(rt, frame.at(0x38u), G[31]); put(rt, frame.at(0x2Cu), G[19]); G[19] = G[5] + 0u; put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x34u), G[21]); put(rt, frame.at(0x28u), G[18]); call_here(rt, ctx, 0x08B85EB8u); G[4] = G[16] + 0u; call_here(rt, ctx, 0x08A69530u); G[4] = word(rt, frame.at(0xCu)); G[17] = G[2] + 0u; G[2] = high_half(0x08D74FD0u); G[16] = G[4] - 0xCu; G[2] = 0x08D74FD0u; { const bool tk = G[16] == G[2]; G[4] = G[4] - 0x4u; if (tk) goto L_08A9FEC8; } G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[4] = G[16] + 0u; if (tk) goto L_08A9FEC8; } G[5] = G[29] + 0u; call_here(rt, ctx, 0x08B8542Cu); L_08A9FEC8: ; { const bool tk = G[17] == 0u; G[16] = 0u + 0u; if (tk) goto L_08A9FFA8; } G[21] = frame.at(0x10u); G[4] = G[21] + 0u; G[18] = frame.at(0x14u); call_here(rt, ctx, 0x08A9DD14u); G[4] = G[18] + 0u; call_here(rt, ctx, 0x08A76AA0u); G[16] = word(rt, frame.at(0x10u)); { const bool tk = G[16] != 0u; G[4] = high_half(0x08CB277Cu); if (tk) goto L_08A9FEFC; } G[4] = 0x08CB277Cu; call_here(rt, ctx, 0x08A5BB7Cu); L_08A9FEFC: ; G[4] = G[16] + 0u; call_here(rt, ctx, 0x08A9BD30u); G[17] = word(rt, G[2] + 0x0u); { const bool tk = G[17] != 0u; G[16] = frame.at(0x8u); if (tk) goto L_08A9FF20; } G[4] = high_half(0x08CB2784u); G[4] = 0x08CB2784u; call_here(rt, ctx, 0x08A5BB7Cu); G[16] = frame.at(0x8u); L_08A9FF20: ; G[4] = G[16] + 0u; G[5] = G[19] + 0u; call_here(rt, ctx, 0x08A69B3Cu); G[6] = G[16] + 0u; G[4] = G[17] + 0u; G[5] = G[18] + 0u; call_here(rt, ctx, 0x08A7BFD0u); G[4] = G[16] + 0u; call_here(rt, ctx, 0x08A69D68u); G[16] = word(rt, frame.at(0x14u)); if (G[16] == 0u) { G[16] = 0u + 0u; goto L_08A9FF98; } call_here(rt, ctx, 0x08A98330u); G[5] = G[2] + 0u; G[4] = G[16] + 0u; call_here(rt, ctx, 0x08A76F20u); { const bool tk = G[2] == 0u; G[16] = 0u + 0u; if (tk) goto L_08A9FF98; } G[16] = frame.at(0x4u); G[4] = G[16] + 0u; G[5] = G[18] + 0u; call_here(rt, ctx, 0x08A98674u); { const bool tk = G[20] == G[16]; G[5] = word(rt, frame.at(0x4u)); if (tk) goto L_08A9FF8C; } G[4] = G[20] + 0u; call_here(rt, ctx, 0x08A5BAD8u); L_08A9FF8C: ; G[4] = G[16] + 0u; call_here(rt, ctx, 0x08A5B9ECu); G[16] = 0x1u; L_08A9FF98: ; G[4] = G[18] + 0u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[21] + 0u; call_here(rt, ctx, 0x08A5B9ECu); L_08A9FFA8: ; G[2] = G[16] + 0u; G[31] = word(rt, frame.at(0x38u)); G[21] = word(rt, frame.at(0x34u)); G[20] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); goto exit_; goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }
void func_08B16ECC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x90u); put(rt, frame.at(0x84u), G[17]); G[2] = 0x1u; put(rt, frame.at(0x88u), G[31]); put(rt, frame.at(0x80u), G[16]); G[3] = word(rt, G[5] + 0x7Cu); { const bool tk = G[3] != G[2]; G[17] = G[5] + 0u; if (tk) goto L_08B16F5C; } G[4] = G[29] + 0u; call_here(rt, ctx, 0x08B0CE58u); G[2] = 0xFFFFFFFFu; G[3] = 0u + 0u; put(rt, frame.at(0x10u), G[2]); G[6] = G[17] + 0xA8u; G[7] = G[29] + 0u; put(rt, frame.at(0x14u), G[3]); G[8] = frame.at(0x30u); L_08B16F10: ; G[2] = word(rt, G[7] + 0x0u); G[3] = word(rt, G[7] + 0x4u); G[4] = word(rt, G[7] + 0x8u); G[5] = word(rt, G[7] + 0xCu); put(rt, G[6] + 0x0u, G[2]); G[7] = G[7] + 0x10u; G[6] = G[6] + 0x10u; put(rt, G[6] - 0xCu, G[3]); put(rt, G[6] - 0x8u, G[4]); { const bool tk = G[7] != G[8]; put(rt, G[6] - 0x4u, G[5]); if (tk) goto L_08B16F10; } G[2] = word(rt, G[7] + 0x8u); G[3] = word(rt, G[7] + 0x0u); G[4] = word(rt, G[7] + 0x4u); G[5] = 0x1u; put(rt, G[6] + 0x8u, G[2]); put(rt, G[6] + 0x0u, G[3]); put(rt, G[6] + 0x4u, G[4]); goto L_08B1702C; L_08B16F5C: ; G[2] = word(rt, G[4] + 0x0u); G[16] = frame.at(0x40u); G[5] = G[4] + 0u; G[3] = word(rt, G[2] + 0xACu); G[6] = G[17] + 0u; { const std::uint32_t callee = G[3]; G[4] = G[16] + 0u; call_here(rt, ctx, callee); } G[6] = G[29] + 0u; G[7] = frame.at(0x70u); L_08B16F80: ; G[2] = word(rt, G[16] + 0x0u); G[3] = word(rt, G[16] + 0x4u); G[4] = word(rt, G[16] + 0x8u); G[5] = word(rt, G[16] + 0xCu); put(rt, G[6] + 0x0u, G[2]); G[16] = G[16] + 0x10u; G[6] = G[6] + 0x10u; put(rt, G[6] - 0xCu, G[3]); put(rt, G[6] - 0x8u, G[4]); { const bool tk = G[16] != G[7]; put(rt, G[6] - 0x4u, G[5]); if (tk) goto L_08B16F80; } G[2] = word(rt, G[16] + 0x8u); G[3] = word(rt, G[16] + 0x0u); G[4] = word(rt, G[16] + 0x4u); put(rt, G[6] + 0x8u, G[2]); put(rt, G[6] + 0x0u, G[3]); put(rt, G[6] + 0x4u, G[4]); G[2] = word(rt, frame.at(0x0u)); { const bool tk = G[2] == 0u; G[5] = 0u + 0u; if (tk) goto L_08B16FD8; } put(rt, G[17] + 0xA8u, G[2]); goto L_08B1702C; L_08B16FD8: ; G[6] = G[17] + 0xA8u; G[7] = G[29] + 0u; G[8] = frame.at(0x30u); L_08B16FE4: ; G[2] = word(rt, G[7] + 0x0u); G[3] = word(rt, G[7] + 0x4u); G[4] = word(rt, G[7] + 0x8u); G[5] = word(rt, G[7] + 0xCu); put(rt, G[6] + 0x0u, G[2]); G[7] = G[7] + 0x10u; G[6] = G[6] + 0x10u; put(rt, G[6] - 0xCu, G[3]); put(rt, G[6] - 0x8u, G[4]); { const bool tk = G[7] != G[8]; put(rt, G[6] - 0x4u, G[5]); if (tk) goto L_08B16FE4; } G[2] = word(rt, G[7] + 0x8u); G[3] = word(rt, G[7] + 0x0u); G[4] = word(rt, G[7] + 0x4u); G[5] = 0x1u; put(rt, G[6] + 0x8u, G[2]); put(rt, G[6] + 0x0u, G[3]); put(rt, G[6] + 0x4u, G[4]); L_08B1702C: ; G[31] = word(rt, frame.at(0x88u)); G[17] = word(rt, frame.at(0x84u)); G[16] = word(rt, frame.at(0x80u)); G[2] = G[5] + 0u; goto exit_; goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }
void func_08AD2E04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0xCu), G[19]); G[19] = G[4] + 0x40B0u; put(rt, frame.at(0x4u), G[17]); G[17] = G[4] + 0u; G[4] = G[19] + 0u; put(rt, frame.at(0x14u), G[31]); put(rt, frame.at(0x10u), G[20]); G[20] = G[17] + 0x29Cu; put(rt, frame.at(0x8u), G[18]); G[18] = G[17] + 0x2A4u; put(rt, frame.at(0x0u), G[16]); call_here(rt, ctx, 0x08B344E0u); G[4] = word(rt, G[17] + 0x29Cu); call_here(rt, ctx, 0x08AD5EC8u); G[3] = sext(load16(G[17] + 0x2A4u), 16u); { const bool tk = static_cast<std::int32_t>(G[3]) < 0; G[2] = G[3] << 7u; if (tk) goto L_08AD2E88; } G[3] = G[3] << 2u; G[2] = G[2] - G[3]; G[2] = G[18] + G[2]; G[16] = G[2] + 0xCu; L_08AD2E5C: ; { const bool tk = G[16] == 0u; G[5] = G[16] + 0u; if (tk) goto L_08AD2E88; } G[4] = G[17] + 0u; call_here(rt, ctx, 0x08AD22D4u); G[2] = sext(load16(G[16] + 0x6u), 16u); G[3] = G[2] << 7u; G[4] = G[2] << 2u; G[3] = G[3] - G[4]; G[3] = G[18] + G[3]; { const bool tk = static_cast<std::int32_t>(G[2]) >= 0; G[16] = G[3] + 0xCu; if (tk) goto L_08AD2E5C; } L_08AD2E88: ; G[4] = word(rt, G[20] + 0x0u); call_here(rt, ctx, 0x08AD5ED0u); G[4] = G[19] + 0u; call_here(rt, ctx, 0x08B3457Cu); G[4] = G[19] + 0u; call_here(rt, ctx, 0x08B28DC4u); G[2] = G[18] + 0xCu; { const bool tk = G[2] == 0u; G[17] = G[18] + 0x3E0Cu; if (tk) goto L_08AD2EDC; } { const bool tk = G[2] == G[17]; G[16] = G[18] + 0x3D90u; if (tk) goto L_08AD2EDC; } G[18] = 0x80u; G[2] = word(rt, G[16] + 0x0u); L_08AD2EBC: ; G[17] = G[17] - 0x7Cu; G[18] = G[18] - 0x1u; G[3] = word(rt, G[2] + 0x0u); G[4] = G[17] + 0u; { const std::uint32_t callee = G[3]; G[16] = G[16] - 0x7Cu; call_here(rt, ctx, callee); } if (G[18] != 0u) { G[2] = word(rt, G[16] + 0x0u); goto L_08AD2EBC; } L_08AD2EDC: ; G[4] = word(rt, G[20] + 0x4u); { const bool tk = G[4] == 0u; G[31] = word(rt, frame.at(0x14u)); if (tk) goto L_08AD2F04; } G[3] = word(rt, G[4] + 0x0u); G[2] = word(rt, G[3] + 0x0u); { const std::uint32_t callee = G[2]; G[5] = word(rt, G[20] + 0x0u); call_here(rt, ctx, callee); } put(rt, G[20] + 0x0u, 0u); put(rt, G[20] + 0x4u, 0u); G[31] = word(rt, frame.at(0x14u)); L_08AD2F04: ; G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); goto exit_; goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }
void func_08AE70C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x30u), G[30]); G[3] = high_half(0x08D409A4u); G[2] = 0x08D409A4u; put(rt, frame.at(0x24u), G[21]); G[30] = G[4] + 0u; G[21] = G[8] + 0u; put(rt, frame.at(0x20u), G[20]); G[20] = G[7] + 0u; put(rt, frame.at(0x18u), G[18]); G[18] = 0u + 0u; put(rt, frame.at(0x34u), G[31]); put(rt, frame.at(0x2Cu), G[23]); put(rt, frame.at(0x28u), G[22]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); G[3] = word(rt, G[2] + 0x4u); put(rt, frame.at(0x0u), G[5]); G[2] = word(rt, G[3] + 0x8u); G[2] = static_cast<std::int32_t>(G[2]) < 0x0 ? 1u : 0u; if (G[2] == 0u) G[18] = G[3]; { const bool tk = G[18] == 0u; put(rt, frame.at(0x4u), G[6]); if (tk) goto L_08AE73D0; } G[19] = G[18] + 0x38u; G[3] = 0xFFFFFFFFu; G[2] = G[19] + 0u; G[17] = 0x1u; L_08AE7130: ; G[17] = G[17] - 0x1u; store16(G[2] + 0x0u, G[3]); { const bool tk = static_cast<std::int32_t>(G[17]) >= 0; G[2] = G[2] + 0x2u; if (tk) goto L_08AE7130; } G[23] = static_cast<std::int32_t>(0u) < static_cast<std::int32_t>(G[20]) ? 1u : 0u; { const bool tk = G[23] == 0u; G[17] = 0u + 0u; if (tk) goto L_08AE71E8; } G[22] = high_half(0x08AE6EF4u); G[16] = G[19] + 0u; G[5] = G[17] + 0u; L_08AE7158: ; G[4] = G[21] + 0u; call_here(rt, ctx, 0x08AEF0FCu); G[3] = sext(G[2], 16u); store16(G[16] + 0x0u, G[2]); G[17] = G[17] + 0x1u; G[5] = G[22] + sext(0x08AE6EF4u, 16u); G[6] = G[19] + 0u; G[4] = G[3] + 0u; { const bool tk = static_cast<std::int32_t>(G[3]) < 0; G[16] = G[16] + 0x2u; if (tk) goto L_08AE7318; } call_here(rt, ctx, 0x08AE9E94u); G[2] = static_cast<std::int32_t>(G[17]) < static_cast<std::int32_t>(G[20]) ? 1u : 0u; { const bool tk = G[2] != 0u; G[5] = G[17] + 0u; if (tk) goto L_08AE7158; } { const bool tk = G[23] == 0u; G[17] = 0u + 0u; if (tk) goto L_08AE71E8; } G[3] = G[19] + 0u; G[2] = sext(load16(G[3] + 0x0u), 16u); L_08AE71A4: ; G[17] = G[17] + 0x1u; G[4] = static_cast<std::int32_t>(G[17]) < static_cast<std::int32_t>(G[20]) ? 1u : 0u; { const bool tk = static_cast<std::int32_t>(G[2]) < 0; G[3] = G[3] + 0x2u; if (tk) goto L_08AE7304; } if (G[4] != 0u) { G[2] = sext(load16(G[3] + 0x0u), 16u); goto L_08AE71A4; } { const bool tk = G[23] == 0u; G[16] = G[19] + 0u; if (tk) goto L_08AE71E8; } G[17] = G[20] + 0u; G[19] = high_half(0x08AE6F58u); L_08AE71CC: ; G[4] = sext(load16(G[16] + 0x0u), 16u); G[5] = G[19] + sext(0x08AE6F58u, 16u); G[6] = G[18] + 0u; G[17] = G[17] - 0x1u; call_here(rt, ctx, 0x08AE9E94u); { const bool tk = G[17] != 0u; G[16] = G[16] + 0x2u; if (tk) goto L_08AE71CC; } L_08AE71E8: ; G[4] = word(rt, frame.at(0x0u)); G[3] = 0x1000u; G[2] = G[18] + 0x14u; G[5] = 0xCu; G[16] = 0xFFFFFFFFu; put(rt, G[18] + 0x10u, G[30]); put(rt, G[18] + 0x8u, G[4]); G[4] = G[18] + 0x5Cu; put(rt, G[2] + 0xCu, 0u); put(rt, G[2] + 0x4u, G[2]); put(rt, G[2] + 0x8u, G[16]); put(rt, G[18] + 0x14u, G[2]); store8(G[18] + 0x53u, G[5]); store16(G[18] + 0x4Cu, G[3]); store8(G[18] + 0x36u, G[20]); put(rt, G[18] + 0x24u, 0u); put(rt, G[18] + 0x28u, 0u); put(rt, G[18] + 0x2Cu, 0u); put(rt, G[18] + 0x30u, 0u); G[2] = word(rt, frame.at(0x4u)); store8(G[18] + 0x52u, G[5]); store16(G[18] + 0x34u, G[2]); store16(G[18] + 0x40u, G[3]); store16(G[18] + 0x48u, G[3]); store16(G[18] + 0x4Au, G[3]); store16(G[18] + 0x44u, G[3]); store16(G[18] + 0x46u, G[3]); put(rt, G[18] + 0x3Cu, 0u); store16(G[18] + 0x42u, 0u); store8(G[18] + 0x50u, 0u); store8(G[18] + 0x51u, 0u); call_here(rt, ctx, 0x08AE68D8u); store16(G[18] + 0x5Cu, G[16]); G[4] = word(rt, frame.at(0x4u)); put(rt, G[18] + 0xA8u, 0u); put(rt, G[18] + 0xACu, 0u); put(rt, G[18] + 0xB0u, 0u); put(rt, G[18] + 0xB4u, 0u); call_here(rt, ctx, 0x08ADC1CCu); G[4] = word(rt, G[18] + 0x0u); { const bool tk = G[4] == G[18]; G[5] = G[30] + 0x18u; if (tk) goto L_08AE72A8; } G[2] = word(rt, G[18] + 0x4u); put(rt, G[4] + 0x4u, G[2]); G[3] = word(rt, G[18] + 0x4u); put(rt, G[18] + 0x4u, G[18]); put(rt, G[3] + 0x0u, G[4]); put(rt, G[18] + 0x0u, G[18]); L_08AE72A8: ; G[2] = word(rt, G[30] + 0x18u); put(rt, G[18] + 0x4u, G[5]); G[5] = G[18] + 0u; put(rt, G[2] + 0x4u, G[18]); G[3] = word(rt, G[30] + 0xCu); G[4] = word(rt, G[18] + 0x4u); put(rt, G[18] + 0x0u, G[2]); G[3] = G[3] | 0x2u; put(rt, G[30] + 0xCu, G[3]); put(rt, G[4] + 0x0u, G[18]); L_08AE72D0: ; G[31] = word(rt, frame.at(0x34u)); G[30] = word(rt, frame.at(0x30u)); G[23] = word(rt, frame.at(0x2Cu)); G[22] = word(rt, frame.at(0x28u)); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); G[2] = G[5] + 0u; goto exit_; L_08AE7304: ; G[2] = high_half(0x08CDFB3Cu); G[3] = word(rt, 0x08CDFB3Cu); G[3] = G[3] & 0x2u; { const bool tk = G[3] != 0u; G[2] = high_half(0x08D409A4u); if (tk) goto L_08AE73AC; } L_08AE7318: ; if (G[23] == 0u) goto L_08AE735C; G[16] = G[19] + 0u; G[17] = G[20] + 0u; L_08AE7328: ; G[2] = sext(load16(G[16] + 0x0u), 16u); G[5] = 0x3u; G[6] = 0u + 0u; G[7] = 0u + 0u; G[17] = G[17] - 0x1u; { const bool tk = static_cast<std::int32_t>(G[2]) < 0; G[4] = G[2] + 0u; if (tk) goto L_08AE7354; } call_here(rt, ctx, 0x08AEF874u); G[4] = sext(load16(G[16] + 0x0u), 16u); call_here(rt, ctx, 0x08AEF1A8u); L_08AE7354: ; { const bool tk = G[17] != 0u; G[16] = G[16] + 0x2u; if (tk) goto L_08AE7328; } L_08AE735C: ; { const bool tk = G[18] == 0u; G[3] = high_half(0x08D409A4u); if (tk) goto L_08AE73A4; } G[4] = word(rt, G[18] + 0x0u); { const bool tk = G[4] == G[18]; G[5] = 0x08D409A4u; if (tk) goto L_08AE7388; } G[2] = word(rt, G[18] + 0x4u); put(rt, G[4] + 0x4u, G[2]); G[3] = word(rt, G[18] + 0x4u); put(rt, G[18] + 0x4u, G[18]); put(rt, G[3] + 0x0u, G[4]); put(rt, G[18] + 0x0u, G[18]); L_08AE7388: ; G[2] = high_half(0x08D409A4u); G[3] = word(rt, 0x08D409A4u); put(rt, G[18] + 0x4u, G[5]); put(rt, G[3] + 0x4u, G[18]); G[2] = word(rt, G[18] + 0x4u); put(rt, G[18] + 0x0u, G[3]); put(rt, G[2] + 0x0u, G[18]); L_08AE73A4: ; G[5] = 0u + 0u; goto L_08AE72D0; L_08AE73AC: ; G[4] = word(rt, G[2] + sext(0x08D3FB44u, 16u)); G[5] = high_half(0x08C91908u); G[7] = high_half(0x08C919DCu); G[5] = 0x08C91908u; G[7] = 0x08C919DCu; G[6] = 0x39Eu; call_here(rt, ctx, 0x08AD83B8u); goto L_08AE7318; L_08AE73D0: ; G[3] = high_half(0x08CDFB3Cu); G[2] = word(rt, 0x08CDFB3Cu); G[2] = G[2] & 0x2u; if (G[2] == 0u) { G[5] = 0u + 0u; goto L_08AE72D0; } G[2] = high_half(0x08CDFB44u); G[4] = word(rt, 0x08CDFB44u); G[5] = high_half(0x08C91908u); G[7] = high_half(0x08C919F0u); G[5] = 0x08C91908u; G[7] = 0x08C919F0u; G[6] = 0x37Eu; call_here(rt, ctx, 0x08AD83B8u); G[5] = 0u + 0u; goto L_08AE72D0; goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }
void func_08AF1E98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x10u); G[2] = high_half(0x08CD7000u); G[2] = 0x08CD7000u; G[3] = G[4] + 0u; put(rt, frame.at(0x0u), G[16]); G[16] = G[3] + 0x4u; put(rt, frame.at(0xCu), G[31]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); put(rt, G[4] + 0x0u, G[2]); G[4] = 0x330u; call_here(rt, ctx, 0x08B62344u); G[5] = G[2] + 0u; put(rt, G[2] + 0x4u, 0u); G[3] = 0x20u; G[2] = G[2] + 0x18u; put(rt, G[5] + 0x8u, 0u); put(rt, G[5] + 0xCu, 0u); put(rt, G[5] + 0x10u, 0u); put(rt, G[5] + 0x14u, 0u); L_08AF1EE8: ; G[3] = G[3] - 0x1u; put(rt, G[2] + 0x0u, 0u); put(rt, G[2] + 0x4u, 0u); put(rt, G[2] + 0x8u, 0u); { const bool tk = G[3] != 0u; G[2] = G[2] + 0x14u; if (tk) goto L_08AF1EE8; } G[2] = 0xFFFFFFFFu; put(rt, G[5] + 0x2ACu, G[2]); G[2] = high_half(0x08CB4AA8u); G[3] = 0x1u; G[2] = 0x08CB4AA8u; G[4] = high_half(0x08D77270u); store8(G[5] + 0x313u, G[3]); put(rt, G[5] + 0x320u, G[2]); put(rt, G[5] + 0x298u, 0u); put(rt, G[5] + 0x29Cu, 0u); put(rt, G[5] + 0x2A0u, 0u); put(rt, G[5] + 0x2A4u, 0u); put(rt, G[5] + 0x2A8u, 0u); put(rt, G[5] + 0x2B0u, 0u); put(rt, G[5] + 0x2B4u, 0u); put(rt, G[5] + 0x2B8u, 0u); put(rt, G[5] + 0x2BCu, 0u); store8(G[5] + 0x310u, 0u); store8(G[5] + 0x311u, G[3]); store8(G[5] + 0x312u, G[3]); put(rt, G[5] + 0x31Cu, 0u); put(rt, G[5] + 0x318u, 0u); put(rt, G[5] + 0x314u, 0u); put(rt, G[16] + 0x0u, G[5]); G[2] = sext(load8(0x08D77270u), 8u); { const bool tk = G[2] != 0u; G[18] = 0x08D77270u; if (tk) goto L_08AF1F7C; } G[4] = G[18] + 0u; call_here(rt, ctx, 0x08B6441Cu); { const bool tk = G[2] != 0u; G[3] = high_half(0x08D77278u); if (tk) goto L_08AF1F88; } L_08AF1F7C: ; G[2] = high_half(0x08D77278u); G[17] = 0x08D77278u; goto L_08AF1FA4; L_08AF1F88: ; G[17] = G[3] + sext(0x08D77278u, 16u); { const bool tk = G[17] == 0u; G[2] = high_half(0x08CD7018u); if (tk) goto L_08AF1F9C; } G[2] = 0x08CD7018u; put(rt, G[3] + sext(0x08D77278u, 16u), G[2]); L_08AF1F9C: ; G[4] = G[18] + 0u; call_here(rt, ctx, 0x08B64494u); L_08AF1FA4: ; put(rt, G[16] + 0x4u, G[17]); G[31] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); goto exit_; goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }
void func_08B04B40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x40u), G[31]); put(rt, frame.at(0x3Cu), G[19]); G[19] = G[5] + 0u; put(rt, frame.at(0x38u), G[18]); G[18] = G[6] + 0u; put(rt, frame.at(0x34u), G[17]); G[17] = G[4] + 0u; put(rt, frame.at(0x30u), G[16]); call_here(rt, ctx, 0x08B03B98u); G[16] = G[2] + 0u; G[4] = word(rt, G[17] + 0x0u); G[5] = G[2] + 0u; G[6] = G[18] + 0u; call_here(rt, ctx, 0x08B04A40u); G[2] = load16(G[16] + 0xE2u); G[2] = G[2] & 0x4000u; { const bool tk = G[2] == 0u; G[31] = word(rt, frame.at(0x40u)); if (tk) goto L_08B04C60; } G[5] = word(rt, G[17] + 0x0u); G[3] = sext(load16(G[16] + 0xEAu), 16u); G[4] = word(rt, G[5] + 0x7Cu); G[2] = G[3] << 3u; G[3] = G[3] << 1u; G[2] = G[2] - G[3]; G[2] = G[2] + G[4]; G[3] = sext(load16(G[2] + 0x0u), 16u); G[4] = word(rt, G[5] + 0x0u); store8(frame.at(0x0u), G[16]); G[2] = G[3] << 8u; G[3] = G[3] << 4u; G[6] = word(rt, frame.at(0x0u)); G[2] = G[2] - G[3]; G[4] = G[4] + G[2]; put(rt, frame.at(0x20u), G[6]); put(rt, frame.at(0x24u), G[5]); put(rt, frame.at(0x28u), G[4]); put(rt, frame.at(0xCu), 0u); put(rt, frame.at(0x8u), G[4]); put(rt, frame.at(0x4u), G[5]); put(rt, frame.at(0x10u), G[6]); put(rt, frame.at(0x14u), G[5]); put(rt, frame.at(0x18u), G[4]); put(rt, frame.at(0x1Cu), 0u); put(rt, frame.at(0x2Cu), 0u); goto L_08B04C48; L_08B04BF8: ; { const bool tk = G[16] == G[2]; G[4] = G[2] + 0u; if (tk) goto L_08B04C10; } G[4] = word(rt, G[17] + 0x0u); call_here(rt, ctx, 0x08B03E1Cu); G[2] = word(rt, frame.at(0x28u)); G[4] = G[2] + 0u; L_08B04C10: ; G[5] = frame.at(0x20u); G[6] = G[16] + 0u; put(rt, frame.at(0x2Cu), G[2]); call_here(rt, ctx, 0x08C46D74u); G[2] = sext(G[2], 16u); G[3] = G[2] << 4u; G[4] = G[2] << 8u; G[4] = G[4] - G[3]; { const bool tk = static_cast<std::int32_t>(G[2]) < 0; G[3] = 0u + 0u; if (tk) goto L_08B04C44; } G[2] = word(rt, frame.at(0x24u)); G[3] = word(rt, G[2] + 0x0u); G[3] = G[3] + G[4]; L_08B04C44: ; put(rt, frame.at(0x28u), G[3]); L_08B04C48: ; G[2] = word(rt, frame.at(0x28u)); G[5] = G[19] + 0u; G[7] = G[18] + 0u; { const bool tk = G[2] != 0u; G[6] = G[2] + 0u; if (tk) goto L_08B04BF8; } G[31] = word(rt, frame.at(0x40u)); L_08B04C60: ; G[19] = word(rt, frame.at(0x3Cu)); G[18] = word(rt, frame.at(0x38u)); G[17] = word(rt, frame.at(0x34u)); G[16] = word(rt, frame.at(0x30u)); goto exit_; goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }
void func_08B41AF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x1Cu), G[23]); G[23] = G[4] + 0x12Cu; put(rt, frame.at(0x18u), G[22]); G[22] = G[4] + 0u; G[4] = G[23] + 0u; put(rt, frame.at(0x10u), G[20]); G[20] = G[5] + 0u; put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x14u), G[21]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); call_here(rt, ctx, 0x08B344E0u); G[2] = 0xFFFFFFFFu; { const bool tk = G[20] == G[2]; G[18] = G[22] + 0x454u; if (tk) goto L_08B41B98; } G[6] = word(rt, G[18] + 0x480u); { const bool tk = G[6] == 0u; G[5] = 0u + 0u; if (tk) goto L_08B41BE4; } G[3] = G[18] + 0u; G[4] = G[18] + 0u; L_08B41B50: ; G[2] = load8(G[3] + 0x400u); { const bool tk = G[2] == 0u; G[3] = G[3] + 0x1u; if (tk) goto L_08B41B78; } G[2] = word(rt, G[4] + 0x200u); if (G[20] != G[2]) { G[5] = G[5] + 0x1u; goto L_08B41B7C; } { const bool tk = static_cast<std::int32_t>(G[5]) >= 0; G[2] = G[5] << 2u; if (tk) goto L_08B41B8C; } G[5] = 0u + 0u; goto L_08B41BE4; L_08B41B78: ; G[5] = G[5] + 0x1u; L_08B41B7C: ; { const bool tk = G[6] != G[5]; G[4] = G[4] + 0x4u; if (tk) goto L_08B41B50; } G[5] = 0u + 0u; goto L_08B41BE4; L_08B41B8C: ; G[2] = G[2] + G[18]; G[5] = word(rt, G[2] + 0x0u); goto L_08B41BE4; L_08B41B98: ; G[5] = word(rt, G[18] + 0x480u); if (G[5] != 0u) { G[2] = load8(G[18] + 0x400u); goto L_08B41E18; } G[5] = 0u + 0u; goto L_08B41BE4; L_08B41BAC: ; { const bool tk = G[2] == 0u; G[3] = G[3] + 0x1u; if (tk) goto L_08B41BC4; } { const bool tk = static_cast<std::int32_t>(G[4]) >= 0; G[2] = G[4] + 0u; if (tk) goto L_08B41BD8; } G[5] = 0u + 0u; goto L_08B41BE4; L_08B41BC4: ; G[4] = G[4] + 0x1u; if (G[5] != G[4]) { G[2] = load8(G[3] + 0x401u); goto L_08B41BAC; } G[5] = 0u + 0u; goto L_08B41BE4; L_08B41BD8: ; G[2] = G[2] << 2u; G[2] = G[2] + G[18]; G[5] = word(rt, G[2] + 0x0u); L_08B41BE4: ; { const bool tk = G[5] == 0u; G[19] = G[5] + 0u; if (tk) goto L_08B41BF8; } G[17] = word(rt, G[5] + 0x0u); { const bool tk = G[17] != 0u; G[21] = G[22] + 0xCCu; if (tk) goto L_08B41C0C; } L_08B41BF8: ; G[4] = G[23] + 0u; call_here(rt, ctx, 0x08B3457Cu); G[2] = 0u + 0u; goto L_08B41E2C; L_08B41C08: ; G[19] = G[16] + 0u; L_08B41C0C: ; G[5] = sext(load16(G[17] + 0x28u), 16u); if (static_cast<std::int32_t>(G[5]) < 0) { G[7] = word(rt, G[18] + 0x480u); goto L_08B41C44; } G[4] = G[22] + 0x154u; call_here(rt, ctx, 0x08B3EBD0u); { const bool tk = G[2] == 0u; G[4] = G[2] + 0u; if (tk) goto L_08B41C40; } G[2] = word(rt, G[2] + 0x10u); if (G[2] == 0u) { G[7] = word(rt, G[18] + 0x480u); goto L_08B41C44; } G[5] = sext(load16(G[17] + 0x16u), 16u); G[6] = sext(load16(G[17] + 0x0u), 16u); call_here(rt, ctx, 0x08B3ED24u); L_08B41C40: ; G[7] = word(rt, G[18] + 0x480u); L_08B41C44: ; { const bool tk = G[7] == 0u; G[6] = sext(load16(G[17] + 0x0u), 16u); if (tk) goto L_08B41DC8; } G[3] = G[18] + 0u; G[4] = G[18] + 0u; G[5] = 0u + 0u; L_08B41C58: ; G[2] = load8(G[3] + 0x400u); { const bool tk = G[2] == 0u; G[3] = G[3] + 0x1u; if (tk) goto L_08B41C80; } G[2] = word(rt, G[4] + 0x200u); if (G[6] != G[2]) { G[5] = G[5] + 0x1u; goto L_08B41C84; } if (static_cast<std::int32_t>(G[5]) >= 0) { G[2] = word(rt, G[18] + 0x484u); goto L_08B41C9C; } goto L_08B41DC8; L_08B41C80: ; G[5] = G[5] + 0x1u; L_08B41C84: ; { const bool tk = G[7] != G[5]; G[4] = G[4] + 0x4u; if (tk) goto L_08B41C58; } goto L_08B41DC8; L_08B41C94: ; put(rt, G[11] + 0x8u, G[2]); goto L_08B41DC8; L_08B41C9C: ; G[3] = G[5] << 2u; G[3] = G[3] + G[18]; G[2] = G[2] - 0x1u; G[4] = G[5] + G[18]; G[8] = G[5] - 0x1u; put(rt, G[3] + 0x0u, 0u); store8(G[4] + 0x400u, 0u); { const bool tk = static_cast<std::int32_t>(G[8]) < 0; put(rt, G[18] + 0x484u, G[2]); if (tk) goto L_08B41D70; } G[2] = G[8] << 2u; G[3] = G[5] + 0x3FFu; G[6] = G[18] + G[2]; G[3] = G[18] + G[3]; G[4] = 0u + 0u; G[10] = G[5] + 0x401u; G[9] = G[5] + 0u; L_08B41CDC: ; G[2] = G[8] - G[4]; G[2] = static_cast<std::int32_t>(G[2]) < static_cast<std::int32_t>(G[7]) ? 1u : 0u; { const bool tk = G[2] == 0u; G[4] = G[4] + 0x1u; if (tk) goto L_08B41D64; } G[2] = load8(G[3] + 0x0u); if (G[2] == 0u) { G[6] = G[6] - 0x4u; goto L_08B41D68; } G[11] = word(rt, G[6] + 0x0u); { const bool tk = G[11] == 0u; G[6] = G[6] - 0x4u; if (tk) goto L_08B41D68; } G[3] = G[5] + 0x1u; G[2] = G[3] < 0x80u ? 1u : 0u; if (G[2] == 0u) { put(rt, G[11] + 0x8u, 0u); goto L_08B41DC8; } G[2] = G[3] << 2u; G[5] = G[18] + G[2]; G[4] = G[18] + G[10]; G[6] = 0x80u; L_08B41D24: ; G[2] = static_cast<std::int32_t>(G[3]) < static_cast<std::int32_t>(G[7]) ? 1u : 0u; if (G[2] == 0u) { G[3] = G[3] + 0x1u; goto L_08B41D50; } { const bool tk = static_cast<std::int32_t>(G[3]) < 0; G[3] = G[3] + 0x1u; if (tk) goto L_08B41D50; } G[2] = load8(G[4] + 0x0u); if (G[2] == 0u) goto L_08B41D50; G[2] = word(rt, G[5] + 0x0u); if (G[2] != 0u) { put(rt, G[2] + 0x4u, G[11]); goto L_08B41C94; } L_08B41D50: ; G[5] = G[5] + 0x4u; { const bool tk = G[3] != G[6]; G[4] = G[4] + 0x1u; if (tk) goto L_08B41D24; } put(rt, G[11] + 0x8u, 0u); goto L_08B41DC8; L_08B41D64: ; G[6] = G[6] - 0x4u; L_08B41D68: ; { const bool tk = G[9] != G[4]; G[3] = G[3] - 0x1u; if (tk) goto L_08B41CDC; } L_08B41D70: ; G[6] = G[5] + 0x1u; G[2] = G[6] < 0x80u ? 1u : 0u; { const bool tk = G[2] == 0u; G[3] = G[5] + 0x401u; if (tk) goto L_08B41DC8; } G[2] = G[6] << 2u; G[5] = G[18] + G[2]; G[3] = G[18] + G[3]; G[4] = 0x80u; L_08B41D90: ; G[2] = static_cast<std::int32_t>(G[6]) < static_cast<std::int32_t>(G[7]) ? 1u : 0u; if (G[2] == 0u) { G[6] = G[6] + 0x1u; goto L_08B41DBC; } { const bool tk = static_cast<std::int32_t>(G[6]) < 0; G[6] = G[6] + 0x1u; if (tk) goto L_08B41DBC; } G[2] = load8(G[3] + 0x0u); if (G[2] == 0u) goto L_08B41DBC; G[2] = word(rt, G[5] + 0x0u); if (G[2] != 0u) { put(rt, G[2] + 0x4u, 0u); goto L_08B41DC8; } L_08B41DBC: ; G[5] = G[5] + 0x4u; { const bool tk = G[6] != G[4]; G[3] = G[3] + 0x1u; if (tk) goto L_08B41D90; } L_08B41DC8: ; G[4] = G[17] + 0u; call_here(rt, ctx, 0x08B3E5F8u); G[4] = G[21] + 0u; G[5] = G[17] + 0u; call_here(rt, ctx, 0x08B2CE7Cu); G[2] = 0xFFFFFFFFu; { const bool tk = G[20] != G[2]; G[5] = G[19] + 0u; if (tk) goto L_08B41E08; } G[16] = word(rt, G[19] + 0x8u); G[4] = G[21] + 0u; call_here(rt, ctx, 0x08B2CE7Cu); if (G[16] == 0u) goto L_08B41E08; G[2] = word(rt, G[16] + 0x0u); { const bool tk = G[2] != 0u; G[17] = G[2] + 0u; if (tk) goto L_08B41C08; } L_08B41E08: ; G[4] = G[23] + 0u; call_here(rt, ctx, 0x08B3457Cu); G[2] = 0x1u; goto L_08B41E2C; L_08B41E18: ; G[3] = G[18] + 0u; { const bool tk = G[2] == 0u; G[4] = 0u + 0u; if (tk) goto L_08B41BC4; } G[2] = 0u + 0u; goto L_08B41BD8; L_08B41E2C: ; G[31] = word(rt, frame.at(0x20u)); G[23] = word(rt, frame.at(0x1Cu)); G[22] = word(rt, frame.at(0x18u)); G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); goto exit_; goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }
void func_08B45054(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0xCu), G[19]); G[19] = 0xFFFFFFFFu; put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); G[16] = G[4] + 0u; call_here(rt, ctx, 0x08B08840u); G[2] = high_half(0x08CDA9A0u); G[2] = 0x08CDA9A0u; G[18] = G[16] + 0xCCu; put(rt, G[16] + 0x0u, G[2]); G[4] = G[18] + 0u; G[17] = G[16] + 0x454u; put(rt, G[16] + 0xC0u, G[19]); store8(G[16] + 0xADu, 0u); put(rt, G[16] + 0xB0u, 0u); store16(G[16] + 0xB4u, 0u); store8(G[16] + 0xB6u, 0u); store8(G[16] + 0xB7u, 0u); store8(G[16] + 0xB8u, 0u); store8(G[16] + 0xB9u, 0u); store8(G[16] + 0xBAu, 0u); put(rt, G[16] + 0xBCu, 0u); put(rt, G[16] + 0xC4u, 0u); put(rt, G[16] + 0xC8u, 0u); call_here(rt, ctx, 0x08B2C350u); G[4] = G[16] + 0x12Cu; call_here(rt, ctx, 0x08B28DCCu); G[4] = G[16] + 0x154u; call_here(rt, ctx, 0x08B3EB80u); G[4] = G[17] + 0u; G[5] = 0u + 0u; G[6] = 0x200u; call_here(rt, ctx, 0x08B36018u); G[4] = G[16] + 0x654u; G[5] = 0u + 0u; G[6] = 0x200u; call_here(rt, ctx, 0x08B36018u); G[6] = 0x80u; G[4] = G[16] + 0x854u; G[5] = 0u + 0u; call_here(rt, ctx, 0x08B36018u); G[3] = 0x80u; put(rt, G[17] + 0x480u, G[3]); G[5] = G[18] + 0u; G[4] = G[16] + 0x8DCu; put(rt, G[17] + 0x484u, 0u); call_here(rt, ctx, 0x08B3F75Cu); G[2] = 0u + 0u; G[3] = 0u + 0u; put(rt, G[16] + 0x910u, G[2]); G[4] = G[16] + 0xBB8u; put(rt, G[16] + 0x914u, G[3]); put(rt, G[16] + 0x93Cu, G[19]); put(rt, G[16] + 0x918u, G[2]); put(rt, G[16] + 0x91Cu, G[3]); put(rt, G[16] + 0x920u, G[2]); put(rt, G[16] + 0x924u, G[3]); put(rt, G[16] + 0x928u, G[2]); put(rt, G[16] + 0x92Cu, G[3]); put(rt, G[16] + 0x930u, 0u); store16(G[16] + 0x934u, 0u); store8(G[16] + 0x936u, 0u); store16(G[16] + 0x938u, 0u); put(rt, G[16] + 0x940u, 0u); put(rt, G[16] + 0x944u, 0u); put(rt, G[16] + 0x988u, 0u); put(rt, G[16] + 0x98Cu, 0u); call_here(rt, ctx, 0x08B3FA98u); G[2] = high_half(0x08D69500u); G[3] = high_half(0x08D694FCu); put(rt, 0x08D69500u, 0u); G[4] = G[16] + 0u; put(rt, 0x08D694FCu, 0u); G[31] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); tail = 0x08B3FF98u; goto exit_; goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_335
} // namespace gt::game

#define HOT335(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_335::name, gt::ReturnKind::kind)
// HOT335(0x0880410Cu, func_0880410C, Word);  // no comparable synthetic case yet
HOT335(0x0881F5CCu, func_0881F5CC, Word);
HOT335(0x0884125Cu, func_0884125C, Word);
// HOT335(0x0885AEF0u, func_0885AEF0, Word);  // compared equal, but the synthetic sabotage went undetected: not verified
// HOT335(0x08860C80u, func_08860C80, Word);  // no comparable synthetic case yet
HOT335(0x08875AD8u, func_08875AD8, Word);
HOT335(0x0887FC90u, func_0887FC90, Word);
// HOT335(0x0888ED2Cu, func_0888ED2C, Word);  // compared equal, but the synthetic sabotage went undetected: not verified
// HOT335(0x088A4F94u, func_088A4F94, Word);  // compared equal, but the synthetic sabotage went undetected: not verified
// HOT335(0x088D63E8u, func_088D63E8, Word);  // compared equal, but the synthetic sabotage went undetected: not verified
// HOT335(0x088E16D4u, func_088E16D4, Word);  // compared equal, but the synthetic sabotage went undetected: not verified
// HOT335(0x088F0B90u, func_088F0B90, Word);  // compared equal, but the synthetic sabotage went undetected: not verified
// HOT335(0x08918844u, func_08918844, Word);  // compared equal, but the synthetic sabotage went undetected: not verified
HOT335(0x089306DCu, func_089306DC, Word);
HOT335(0x08940328u, func_08940328, Word);
// HOT335(0x0897F6A4u, func_0897F6A4, Word);  // compared equal, but the synthetic sabotage went undetected: not verified
// HOT335(0x08988264u, func_08988264, Word);  // compared equal, but the synthetic sabotage went undetected: not verified
HOT335(0x08993304u, func_08993304, Word);
HOT335(0x089AF334u, func_089AF334, Word);
// HOT335(0x089BC7E4u, func_089BC7E4, Word);  // no comparable synthetic case yet
// HOT335(0x089C970Cu, func_089C970C, Word);  // compared equal, but the synthetic sabotage went undetected: not verified
HOT335(0x089D7300u, func_089D7300, Word);
// HOT335(0x089F11A0u, func_089F11A0, Word);  // compared equal, but the synthetic sabotage went undetected: not verified
HOT335(0x08A0D214u, func_08A0D214, Word);
HOT335(0x08A2F8A8u, func_08A2F8A8, Word);
HOT335(0x08A3ED98u, func_08A3ED98, Word);
HOT335(0x08A4ABE4u, func_08A4ABE4, Word);
// HOT335(0x08A5B070u, func_08A5B070, Word);  // no comparable synthetic case yet
HOT335(0x08A77D80u, func_08A77D80, Word);
// HOT335(0x08A81C60u, func_08A81C60, Word);  // no comparable synthetic case yet
// HOT335(0x08A8D308u, func_08A8D308, Word);  // no comparable synthetic case yet
HOT335(0x08A92A9Cu, func_08A92A9C, Word);
// HOT335(0x08A9FE54u, func_08A9FE54, Word);  // compared equal, but the synthetic sabotage went undetected: not verified
HOT335(0x08ABE3A4u, func_08ABE3A4, Word);
HOT335(0x08AC8E2Cu, func_08AC8E2C, Word);
// HOT335(0x08AD2E04u, func_08AD2E04, Word);  // compared equal, but the synthetic sabotage went undetected: not verified
HOT335(0x08AE70C0u, func_08AE70C0, Word);
// HOT335(0x08AF1E98u, func_08AF1E98, Word);  // compared equal, but the synthetic sabotage went undetected: not verified
// HOT335(0x08B04B40u, func_08B04B40, Word);  // compared equal, but the synthetic sabotage went undetected: not verified
HOT335(0x08B16ECCu, func_08B16ECC, Word);
HOT335(0x08B27AE8u, func_08B27AE8, Word);
HOT335(0x08B3772Cu, func_08B3772C, Word);
HOT335(0x08B41AF4u, func_08B41AF4, Word);
// HOT335(0x08B45054u, func_08B45054, Word);  // compared equal, but the synthetic sabotage went undetected: not verified
HOT335(0x08B52F58u, func_08B52F58, Word);
