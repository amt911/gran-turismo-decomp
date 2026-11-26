// Main-session batch 268: functions earlier batches (and their gate) never ran with useful inputs.
// Written by hand as structured C from `tools.disasm`; every exit leaves v0 as the asm does.
//
// 0x08B73ED0 is written here: the bytecode evaluator of the script engine (a stack machine of 64 words).
// Skipped, with the reason:
//   0x08C65724 and 0x08C5E4B0 (398 and 409 insns) are the two halves of the engine's formatted-value I/O over its
//   own stream/string classes: sentry objects (0x08B64C80), a record looked up by name (0x08B5C2B0) and then read
//   through a 7-word descriptor of the record's vtable (+0x48..+0x60), virtual get/eof calls (+0x24 / +0x28) on
//   four cursors, copy-on-write string reps mutated in place (length at data-0xC, capacity at data-8, count at
//   data-4) through 0x08C75840 / 0x08C75AF8 / 0x08C7598C / 0x08C74FAC / 0x08C74DD8, and, in 0x08C65724, a stack
//   frame lowered by a length read at run time (`subu $sp, $sp, $v0`) whose 4-byte unaligned temporary is then
//   walked byte by byte through a five-entry jump table. Neither body has a structure a hand draft could state as
//   straightforward ifs and loops: writing one anyway would be a register-level emitter body (forbidden) whose
//   mismatches a later gate could not localise. They need a reader who knows the classes (or the original source),
//   not another draft pass.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_268 {
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
// A word at an address of any alignment: the `lwl 3(addr)` / `lwr 0(addr)` pair, which on this little-endian
// target is the four bytes in ascending order.
u32 unaligned_word(Runtime &rt, u32 address) {
    return static_cast<u32>(rt.memory().load8(address)) |
           static_cast<u32>(rt.memory().load8(address + 1u)) << 8u |
           static_cast<u32>(rt.memory().load8(address + 2u)) << 16u |
           static_cast<u32>(rt.memory().load8(address + 3u)) << 24u;
}
// The 16-bit operand of the branch opcodes: the byte at `address` is the low half, the next one the high half.
s32 signed_operand(Runtime &rt, u32 address) {
    return static_cast<std::int16_t>(static_cast<std::uint16_t>(rt.memory().load8(address)) |
                                     static_cast<std::uint16_t>(rt.memory().load8(address + 1u)) << 8u);
}

// The bytecode's opcodes. The jump table at 0x08C968BC is indexed by opcode - 3 and the one at 0x08C96B0C by
// opcode - 0x1A (the binary ones), so the numbers themselves live in that table rather than in an instruction.
enum Opcode : u32 {
    kPushWord = 0x03,
    kPushWord2 = 0x0C,  // the same handler as kPushWord
    kPushWord3 = 0x0D,
    kLoadWord = 0x06,
    kPushByte = 0x08,
    kPushSignedByte = 0x09,
    kPushHalf = 0x0A,
    kPushSignedHalf = 0x0B,
    kPushWideWord = 0x0E,  // 8 bytes are skipped
    kPushWideWord2 = 0x0F,  // the same handler as kPushWideWord
    kPushVarint = 0x10,
    kPushSignedVarint = 0x11,
    kDup = 0x12,
    kDrop = 0x13,
    kOver = 0x14,
    kPick = 0x15,
    kRotate = 0x17,
    kAbs = 0x19,
    kAnd = 0x1A,
    kDiv = 0x1B,
    kSub = 0x1C,
    kMod = 0x1D,
    kMul = 0x1E,
    kNegate = 0x1F,
    kComplement = 0x20,
    kOr = 0x21,
    kAdd = 0x22,
    kAddVarint = 0x23,
    kShl = 0x24,
    kShr = 0x25,
    kSar = 0x26,
    kXor = 0x27,
    kJumpIfZero = 0x28,
    kEq = 0x29,
    kLe = 0x2A,
    kLt = 0x2B,
    kGe = 0x2C,
    kGt = 0x2D,
    kNe = 0x2E,
    kJump = 0x2F,
    kRecord = 0x90,
    kRecordPlus = 0x92,
    kLoadSized = 0x94,
    kNop = 0x96,
};
// The opcodes handled by ranges rather than one by one.
constexpr u32 kSmallBase = 0x30u;      // 0x30..0x4F push opcode - 0x30
constexpr u32 kSmallEnd = 0x4Fu;
constexpr u32 kRecordBase = 0x50u;     // 0x50..0x6F push record[opcode - 0x50]
constexpr u32 kRecordEnd = 0x6Fu;
constexpr u32 kRecordPlusBase = 0x70u;  // 0x70..0x8F push record[opcode - 0x70] + the signed varint
constexpr u32 kRecordPlusEnd = 0x8Fu;

// --- 0x08B73ED0 ---
// The script engine's bytecode evaluator. a0 = the first byte of the program, a1 = one past its end, a2 = the table
// of records an operand names (307 entries; 0x08D72BB4 holds one type byte per entry and an entry of type 4 is an
// object whose first word is the value it stands for), a3 = the value the stack starts with.
//
// The machine: an operand stack of 64 words lives in this frame at sp+0x14, sp+0x14 being a3; sp+0 is the word a
// push takes and sp+4 / +8 / +0xC / +0x10 the out-parameters of the two varint decoders 0x08B727A4 (unsigned) and
// 0x08B727D8 (signed), which write through the pointer they get in a1 and answer the cursor after the operand.
// A push that finds the stack full, an operand that needs more words than the stack holds, and an opcode with no
// handler are reported by 0x08B77B04, which does not return; the evaluation then ends like a normal one.
// v0 = the top of the stack once the cursor reaches a1 (a3 when the program is empty).
void func_08B73ED0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x140u);
    const u32 table = ctx.gpr[6];
    const u32 end = ctx.gpr[5];
    u32 pc = ctx.gpr[4];
    s32 depth = 1;
    put(rt, frame.at(0x14u), ctx.gpr[7]);
    const u32 value_address = frame.at(0u);
    const u32 parse_a = frame.at(4u);
    const u32 parse_b = frame.at(8u);
    const u32 parse_c = frame.at(0xCu);
    const u32 parse_d = frame.at(0x10u);
    const auto stack_at = [&frame](s32 index) { return frame.at(0x14u + 4u * static_cast<u32>(index)); };
    // 0x08B77B04: the engine's error report.
    const auto report = [&rt, &ctx] { call(rt, ctx, 0x08B77B04u, {}); };
    // 0x08B741A4 / 0x08B74098: the value a table index stands for; an index past 306 or an entry whose type byte is
    // not 4 is the report path.
    const auto record = [&rt, &table](s32 index, u32 &out) {
        if (index < 0 || index >= 0x133) return false;
        if (rt.memory().load8(0x08D72BB4u + static_cast<u32>(index)) != 4u) return false;
        out = word(rt, word(rt, table + 4u * static_cast<u32>(index)));
        return true;
    };
    for (;;) {
        if (pc >= end) break;
        const u32 opcode = rt.memory().load8(pc);
        u32 next = pc + 1u;
        u32 value = 0u;
        bool pushes = true;
        bool invalid = false;
        switch (opcode) {
        case kPushWord:
        case kPushWord2:
        case kPushWord3:  // 0x08B73F74: the 4-byte word after the opcode
            value = unaligned_word(rt, pc + 1u);
            next = pc + 5u;
            break;
        case kLoadWord: {  // 0x08B74468: the top is an address; the word there replaces it
            if (depth - 1 < 0) { invalid = true; break; }
            --depth;
            value = unaligned_word(rt, word(rt, stack_at(depth)));
            break;
        }
        case kPushByte:  // 0x08B73FCC: an unsigned byte
            value = rt.memory().load8(pc + 1u);
            next = pc + 2u;
            break;
        case kPushSignedByte:  // 0x08B740CC: a signed byte
            value = static_cast<u32>(static_cast<std::int8_t>(rt.memory().load8(pc + 1u)));
            next = pc + 2u;
            break;
        case kPushHalf:  // 0x08B73FFC: an unsigned 16-bit operand
            value = static_cast<u32>(rt.memory().load8(pc + 1u)) | static_cast<u32>(rt.memory().load8(pc + 2u)) << 8u;
            next = pc + 3u;
            break;
        case kPushSignedHalf:  // 0x08B73FDC: a signed 16-bit operand
            value = static_cast<u32>(signed_operand(rt, pc + 1u));
            next = pc + 3u;
            break;
        case kPushWideWord:
        case kPushWideWord2:  // 0x08B73FB4: the first word of an 8-byte operand
            value = unaligned_word(rt, pc + 1u);
            next = pc + 9u;
            break;
        case kPushVarint:  // 0x08B740F8: an unsigned varint, which is also the next cursor
            next = call(rt, ctx, 0x08B727A4u, {pc + 1u, value_address});
            value = word(rt, value_address);
            break;
        case kPushSignedVarint:  // 0x08B740DC: a signed varint, which is also the next cursor
            next = call(rt, ctx, 0x08B727D8u, {pc + 1u, parse_d});
            value = word(rt, parse_d);
            break;
        case kDup:  // 0x08B7416C: duplicate the top
            if (depth <= 0) { invalid = true; break; }
            value = word(rt, stack_at(depth - 1));
            break;
        case kDrop:  // 0x08B74158: drop the top
            --depth;
            if (depth < 0) { invalid = true; break; }
            pushes = false;
            break;
        case kOver:  // 0x08B7413C: push a copy of the word below the top
            if (depth < 2) { invalid = true; break; }
            value = word(rt, stack_at(depth - 2));
            break;
        case kPick: {  // 0x08B7410C: push a copy of the word the operand counts down to
            const u32 pick = rt.memory().load8(pc + 1u);
            next = pc + 2u;
            if (static_cast<s32>(pick) >= depth - 1) { invalid = true; break; }
            value = word(rt, stack_at(depth - static_cast<s32>(pick) - 1));
            break;
        }
        case kRotate: {  // 0x08B742C4: x y z -> y z x
            if (depth < 3) { invalid = true; break; }
            const u32 top = word(rt, stack_at(depth - 1));
            const u32 middle = word(rt, stack_at(depth - 2));
            put(rt, stack_at(depth - 1), middle);
            put(rt, stack_at(depth - 2), word(rt, stack_at(depth - 3)));
            put(rt, stack_at(depth - 3), top);
            pushes = false;
            break;
        }
        case kAbs: {  // 0x08B74058: the absolute value of the top
            if (depth - 1 < 0) { invalid = true; break; }
            --depth;
            const u32 top = word(rt, stack_at(depth));
            value = static_cast<s32>(top) < 0 ? 0u - top : top;
            break;
        }
        case kNegate: {  // 0x08B74060: -top
            if (depth - 1 < 0) { invalid = true; break; }
            --depth;
            value = 0u - word(rt, stack_at(depth));
            break;
        }
        case kComplement: {  // 0x08B7445C: ~top
            if (depth - 1 < 0) { invalid = true; break; }
            --depth;
            value = ~word(rt, stack_at(depth));
            break;
        }
        case kAddVarint: {  // 0x08B74478: the top plus the unsigned varint after the opcode
            if (depth - 1 < 0) { invalid = true; break; }
            --depth;
            next = call(rt, ctx, 0x08B727A4u, {pc + 1u, parse_b});
            value = word(rt, stack_at(depth)) + word(rt, parse_b);
            break;
        }
        // 0x08B74240: the binary opcodes; the older word is at depth-2, the top at depth-1.
        case kAnd:
            if (depth < 2) { invalid = true; break; }
            depth -= 2;
            value = word(rt, stack_at(depth)) & word(rt, stack_at(depth + 1));
            break;
        case kDiv: {  // a zero divisor raises the PSP's break, modelled as the older word here
            if (depth < 2) { invalid = true; break; }
            depth -= 2;
            const s32 left = static_cast<s32>(word(rt, stack_at(depth)));
            const s32 right = static_cast<s32>(word(rt, stack_at(depth + 1)));
            value = right == 0 ? static_cast<u32>(left) : static_cast<u32>(left / right);
            break;
        }
        case kSub:
            if (depth < 2) { invalid = true; break; }
            depth -= 2;
            value = word(rt, stack_at(depth)) - word(rt, stack_at(depth + 1));
            break;
        case kMod: {  // the zero divisor as in div
            if (depth < 2) { invalid = true; break; }
            depth -= 2;
            const s32 left = static_cast<s32>(word(rt, stack_at(depth)));
            const s32 right = static_cast<s32>(word(rt, stack_at(depth + 1)));
            value = right == 0 ? static_cast<u32>(left) : static_cast<u32>(left % right);
            break;
        }
        case kMul: {  // mult + mflo: the low 32 bits of the signed product
            if (depth < 2) { invalid = true; break; }
            depth -= 2;
            const std::int64_t product = static_cast<std::int64_t>(static_cast<s32>(word(rt, stack_at(depth)))) *
                                         static_cast<std::int64_t>(static_cast<s32>(word(rt, stack_at(depth + 1))));
            value = static_cast<u32>(product);
            break;
        }
        case kOr:
            if (depth < 2) { invalid = true; break; }
            depth -= 2;
            value = word(rt, stack_at(depth)) | word(rt, stack_at(depth + 1));
            break;
        case kAdd:
            if (depth < 2) { invalid = true; break; }
            depth -= 2;
            value = word(rt, stack_at(depth)) + word(rt, stack_at(depth + 1));
            break;
        case kShl:  // sllv: the low five bits of the top
            if (depth < 2) { invalid = true; break; }
            depth -= 2;
            value = word(rt, stack_at(depth)) << (word(rt, stack_at(depth + 1)) & 31u);
            break;
        case kShr:  // srlv
            if (depth < 2) { invalid = true; break; }
            depth -= 2;
            value = word(rt, stack_at(depth)) >> (word(rt, stack_at(depth + 1)) & 31u);
            break;
        case kSar:  // srav
            if (depth < 2) { invalid = true; break; }
            depth -= 2;
            value = static_cast<u32>(static_cast<s32>(word(rt, stack_at(depth))) >>
                                     (word(rt, stack_at(depth + 1)) & 31u));
            break;
        case kXor:
            if (depth < 2) { invalid = true; break; }
            depth -= 2;
            value = word(rt, stack_at(depth)) ^ word(rt, stack_at(depth + 1));
            break;
        case kJumpIfZero: {  // 0x08B74288: drop the top; a zero sends execution past the 16-bit operand
            --depth;
            if (depth < 0) { invalid = true; break; }
            const s32 offset = signed_operand(rt, pc + 1u);
            next = word(rt, stack_at(depth)) == 0u ? pc + 3u + static_cast<u32>(offset) : pc + 3u;
            pushes = false;
            break;
        }
        case kEq:
            if (depth < 2) { invalid = true; break; }
            depth -= 2;
            value = word(rt, stack_at(depth)) == word(rt, stack_at(depth + 1)) ? 1u : 0u;
            break;
        case kLe:  // !(top < older): the older one is less than or equal, signed
            if (depth < 2) { invalid = true; break; }
            depth -= 2;
            value = static_cast<s32>(word(rt, stack_at(depth + 1))) < static_cast<s32>(word(rt, stack_at(depth))) ? 0u : 1u;
            break;
        case kLt:
            if (depth < 2) { invalid = true; break; }
            depth -= 2;
            value = static_cast<s32>(word(rt, stack_at(depth))) < static_cast<s32>(word(rt, stack_at(depth + 1))) ? 1u : 0u;
            break;
        case kGe:  // !(older < top)
            if (depth < 2) { invalid = true; break; }
            depth -= 2;
            value = static_cast<s32>(word(rt, stack_at(depth))) < static_cast<s32>(word(rt, stack_at(depth + 1))) ? 0u : 1u;
            break;
        case kGt:  // top < older
            if (depth < 2) { invalid = true; break; }
            depth -= 2;
            value = static_cast<s32>(word(rt, stack_at(depth + 1))) < static_cast<s32>(word(rt, stack_at(depth))) ? 1u : 0u;
            break;
        case kNe:
            if (depth < 2) { invalid = true; break; }
            depth -= 2;
            value = word(rt, stack_at(depth)) != word(rt, stack_at(depth + 1)) ? 1u : 0u;
            break;
        case kJump: {  // 0x08B7421C: jump by the 16-bit operand
            const s32 offset = signed_operand(rt, pc + 1u);
            next = pc + 3u + static_cast<u32>(offset);
            pushes = false;
            break;
        }
        case kRecord: {  // 0x08B74184: record[an unsigned varint].first
            next = call(rt, ctx, 0x08B727A4u, {pc + 1u, parse_a});
            if (!record(static_cast<s32>(word(rt, parse_a)), value)) invalid = true;
            break;
        }
        case kRecordPlus: {  // 0x08B7406C: record[an unsigned varint].first + the signed varint after it
            next = call(rt, ctx, 0x08B727A4u, {pc + 1u, parse_a});
            const u32 second = call(rt, ctx, 0x08B727D8u, {next, parse_c});
            if (!record(static_cast<s32>(word(rt, parse_a)), value)) {
                invalid = true;
                break;
            }
            value += word(rt, parse_c);
            next = second;
            break;
        }
        case kLoadSized: {  // 0x08B7449C: the top is an address; a load of the size after the opcode replaces it
            if (depth - 1 < 0) { invalid = true; break; }
            const u32 size = rt.memory().load8(pc + 1u);
            const u32 address = word(rt, stack_at(depth - 1));
            next = pc + 2u;
            switch (size) {
            case 1u: value = rt.memory().load8(address); break;
            case 2u:
                value = static_cast<u32>(rt.memory().load8(address)) |
                        static_cast<u32>(rt.memory().load8(address + 1u)) << 8u;
                break;
            case 4u:
            case 8u:  // an 8-byte field: the same first word
                value = unaligned_word(rt, address);
                break;
            default: invalid = true; break;
            }
            if (invalid) break;
            --depth;
            break;
        }
        case kNop:  // 0x08B73FA0: nothing at all
            pushes = false;
            break;
        default:
            if (opcode >= kSmallBase && opcode <= kSmallEnd) {  // 0x08B74210: the opcode's own small value
                value = opcode - kSmallBase;
            } else if (opcode >= kRecordBase && opcode <= kRecordEnd) {  // 0x08B741F8
                if (!record(static_cast<s32>(opcode - kRecordBase), value)) invalid = true;
            } else if (opcode >= kRecordPlusBase && opcode <= kRecordPlusEnd) {  // 0x08B741D0
                next = call(rt, ctx, 0x08B727D8u, {pc + 1u, parse_c});
                if (!record(static_cast<s32>(opcode - kRecordPlusBase), value)) {
                    invalid = true;
                } else {
                    value += word(rt, parse_c);
                }
            } else {
                invalid = true;
                pushes = false;
            }
            break;
        }
        if (invalid) {
            report();  // 0x08B73F1C: report and then end like a normal run (0x08B77B04 does not return)
            break;
        }
        if (pushes) {
            if (depth >= 0x40) {  // 0x08B73F88: the push path reports a full stack too
                report();
                break;
            }
            put(rt, stack_at(depth), value);
            ++depth;
        }
        pc = next;
    }
    --depth;
    if (depth < 0) {
        // The asm loops back into the report while the depth is negative; 0x08B77B04 does not return.
        report();
    }
    ctx.gpr[2] = word(rt, stack_at(depth));
}

} // namespace
} // namespace new_batch_main_268
} // namespace gt::game

#define MAIN268(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_268::name, gt::ReturnKind::kind)
MAIN268(0x08B73ED0u, func_08B73ED0, Word);
