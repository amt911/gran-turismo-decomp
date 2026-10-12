// Small leaf functions the race path calls hundreds of thousands of times, picked with a call probe
// (GT_PROBE_SYMBOLS over every undone function). Their classes and subsystems aren't known yet, so
// they keep their addresses as names; each comment says what the code does. Written from the asm.

#include "decomp_registry.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>

namespace gt::game {
namespace {

constexpr std::uint32_t kNestedLockCount = 0x08CF4C54u;  // the nesting depth of func_089B8980
constexpr std::uint32_t kNestedLockMutex = 0x08CF4C2Cu;  // the lock it takes on the first entry
constexpr std::uint32_t kHeapLock = 0x08B344E0u;         // gt::libc::page_heap::heap_lock (mutex *)
constexpr std::uint32_t kHeapUnlock = 0x08B3457Cu;       // gt::libc::page_heap::heap_unlock (mutex *)
constexpr std::uint32_t kAllocateAligned = 0x08B2B4D8u;  // (alignment, size)

std::uint32_t word(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load32(address); }
float float_at(psprecomp::Runtime &rt, std::uint32_t address) { return std::bit_cast<float>(rt.memory().load32(address)); }

} // namespace

// --- element addresses: this->array + index * stride (+ offset) ---

// func_08955300(this, i) -> &this->array8[i].member70 (stride 0x118).
void func_08955300(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = word(rt, ctx.gpr[4] + 0x8u) + ctx.gpr[5] * 0x118u + 0x70u;
}

// func_089554A8(this, i) -> &this->array14[i].member20 (stride 0x24).
void func_089554A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = word(rt, ctx.gpr[4] + 0x14u) + ctx.gpr[5] * 0x24u + 0x20u;
}

// func_089F9B54(this, i) -> &this->array0[i] (stride 0x20).
void func_089F9B54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = word(rt, ctx.gpr[4]) + (ctx.gpr[5] << 5);
}

// func_0893C780(this, i) -> &this->array10[i] (stride 0x38).
void func_0893C780(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = word(rt, ctx.gpr[4] + 0x10u) + ctx.gpr[5] * 0x38u;
}

// func_089FA8A0(vector) -> (end - begin) / 32: the element count of a vector of 0x20-byte items.
void func_089FA8A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t begin = word(rt, ctx.gpr[4]);
    const std::uint32_t end = word(rt, ctx.gpr[4] + 0x4u);
    ctx.gpr[2] = static_cast<std::uint32_t>(static_cast<std::int32_t>(end - begin) >> 5);
}

// The current-entry accessors of one class: entries (0x40 bytes) at +0xB0, the current index at +0xB4.
std::uint32_t current_entry(psprecomp::Runtime &rt, const psprecomp::AllegrexContext &ctx) {
    return word(rt, ctx.gpr[4] + 0xB0u) + (word(rt, ctx.gpr[4] + 0xB4u) << 6);
}
// func_089A8B98(this) -> the current entry.
void func_089A8B98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = current_entry(rt, ctx); }
// func_089A8B20(this) -> &current entry + 0x10.
void func_089A8B20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = current_entry(rt, ctx) + 0x10u; }
// func_089A8B68 / func_089A8B80 / func_089A8BF4(this) -> the float at +0x8 / +0xC / +0x34 of the current entry.
void func_089A8B68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t entry = current_entry(rt, ctx);
    ctx.gpr[2] = entry;
    ctx.fpr[0] = float_at(rt, entry + 0x8u);
}
void func_089A8B80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t entry = current_entry(rt, ctx);
    ctx.gpr[2] = entry;
    ctx.fpr[0] = float_at(rt, entry + 0xCu);
}
void func_089A8BF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t entry = current_entry(rt, ctx);
    ctx.gpr[2] = entry;
    ctx.fpr[0] = float_at(rt, entry + 0x34u);
}

// func_089A8C94(this) -> &this->items58[this->index5C] (stride 0x10).
void func_089A8C94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = word(rt, ctx.gpr[4] + 0x58u) + (word(rt, ctx.gpr[4] + 0x5Cu) << 4);
}

// func_089A8C70(this) -> 1 when the flag byte at +0x60 is set and the index at +0x5C isn't negative.
void func_089A8C70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t valid = 1u;
    if (rt.memory().load8(ctx.gpr[4] + 0x60u) == 0u) {
        valid = 0u;
    } else if (static_cast<std::int32_t>(word(rt, ctx.gpr[4] + 0x5Cu)) < 0) {
        valid = 0u;
    }
    ctx.gpr[2] = valid;
}

// func_089A8C24(this): clears the state bytes and words it owns.
void func_089A8C24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    rt.memory().store8(self + 0x151u, 0u);
    rt.memory().store8(self + 0x118u, 0u);
    rt.memory().store32(self + 0x170u, 0u);
    rt.memory().store32(self + 0x174u, 0u);
    rt.memory().store8(self + 0x152u, 0u);
    rt.memory().store8(self + 0x150u, 0u);
}

// --- flags ---

// func_089673DC(this) -> bit 2 of the flags at +0xE4, which it clears.
void func_089673DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t flags = word(rt, ctx.gpr[4] + 0xE4u);
    rt.memory().store32(ctx.gpr[4] + 0xE4u, flags & ~0x4u);
    ctx.gpr[2] = (flags >> 2) & 1u;
}

// func_08968C38(this) -> a test of the nibbles of the flags at +0xE0: 0 unless bit 14 is set; then,
// with the 0xF00 nibble clear, 1 unless the 0xF0 nibble shares no bit with the low one; otherwise
// whether the 0xF00 nibble shares no bit with (the 0xF0 nibble ^ the low byte).
void func_08968C38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t x = word(rt, ctx.gpr[4] + 0xE0u);
    std::uint32_t result = 0u;
    if ((x & 0x4000u) != 0u) {
        const std::uint32_t mid = (x >> 4) & 0xFu;
        const std::uint32_t high = (x >> 8) & 0xFu;
        result = (((mid ^ x) & high) == 0u) ? 1u : 0u;
        if ((x & 0xF00u) == 0u) {
            result = 1u;
            if ((x & 0xF0u) != 0u) result = ((x & mid) != 0u) ? 1u : 0u;
        }
    }
    ctx.gpr[2] = result;
}

// func_08B21410(set, i) -> whether bit i of a byte-sized bit set is on: count at +4, bits from +5.
void func_08B21410(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const auto i = static_cast<std::int32_t>(ctx.gpr[5]);
    std::uint32_t on = 0u;
    if (i >= 0 && i < static_cast<std::int32_t>(rt.memory().load8(ctx.gpr[4] + 0x4u))) {
        const std::uint32_t bits = rt.memory().load8(ctx.gpr[4] + static_cast<std::uint32_t>(i >> 3) + 0x5u);
        on = (bits & (1u << (i & 7))) != 0u ? 1u : 0u;
    }
    ctx.gpr[2] = on;
}

// func_08B21590(table, i) -> the signed 16-bit entry i (count at +2, entries from +4), 0 out of range.
void func_08B21590(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const auto i = static_cast<std::int32_t>(ctx.gpr[5]);
    std::uint32_t value = 0u;
    if (i >= 0 && i < static_cast<std::int32_t>(rt.memory().load16(ctx.gpr[4] + 0x2u))) {
        value = static_cast<std::uint32_t>(static_cast<std::int32_t>(
            static_cast<std::int16_t>(rt.memory().load16(ctx.gpr[4] + static_cast<std::uint32_t>(i) * 2u + 0x4u))));
    }
    ctx.gpr[2] = value;
}

// --- stores ---

// func_08977368(table, row, column, value in $f12): table->cells[row][column] = value
// (rows of 22 floats from +0x94).
void func_08977368(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t cell = ((ctx.gpr[5] * 22u + ctx.gpr[6]) << 2) + ctx.gpr[4];
    rt.memory().store32(cell + 0x94u, std::bit_cast<std::uint32_t>(ctx.fpr[12]));
    ctx.gpr[2] = cell;
}

// func_088CB910(-, vec3 *from, vec4 *to): to = {from->x, from->y, from->z, 1.0f}.
void func_088CB910(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t from = ctx.gpr[5];
    const std::uint32_t to = ctx.gpr[6];
    const std::uint32_t x = word(rt, from), y = word(rt, from + 4u), z = word(rt, from + 8u);
    rt.memory().store32(to + 0xCu, 0x3F800000u);  // 1.0f
    rt.memory().store32(to, x);
    rt.memory().store32(to + 0x4u, y);
    rt.memory().store32(to + 0x8u, z);
}

// --- text and memory ---

// func_08B1AC1C(const char **cursor) -> the UTF-8 code point at *cursor; *cursor moves past it.
// Continuation bytes are taken while the lead byte has more 1 bits (not checked to be 10xxxxxx);
// a stray continuation byte as lead gives its low 6 bits.
void func_08B1AC1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t cursor = ctx.gpr[4];
    std::uint32_t p = word(rt, cursor);
    std::uint32_t code = rt.memory().load8(p);
    ++p;
    if ((code & 0x80u) != 0u) {
        if ((code & 0x40u) == 0u) {
            code &= 0x3Fu;
        } else {
            std::uint32_t lead = code;
            std::uint32_t mask = 0xFFFFFFC0u;
            bool more;
            do {
                const std::uint32_t next = rt.memory().load8(p);
                lead <<= 1;
                code = (code << 6) | (next & 0x3Fu);
                ++p;
                mask <<= 5;
                more = (lead & 0x40u) != 0u;
            } while (more);
            code &= ~mask;
        }
    }
    rt.memory().store32(cursor, p);
    ctx.gpr[2] = code;
}

// func_08A5BC20(size, alignment): allocates with an alignment of at least 4, 8 or 16 by size (<= 4,
// <= 8, larger) when the requested one is below 16. The object factories call it (src/core/factories.cpp).
void func_08A5BC20(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t size = ctx.gpr[4];
    std::uint32_t alignment = ctx.gpr[5];
    if (alignment < 0x10u) alignment = size < 5u ? 4u : size < 9u ? 8u : 0x10u;
    tail_call(ctx, kAllocateAligned, {alignment, size});
}

// --- a nesting lock ---

// func_089B8980(): enter — takes the lock at 0x08CF4C2C on the outermost entry (depth 0 -> 1).
void func_089B8980(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t depth = word(rt, kNestedLockCount);
    rt.memory().store32(kNestedLockCount, depth + 1u);
    ctx.gpr[2] = 1u;  // the store-conditional's success flag
    if (depth == 0u) tail_call(ctx, kHeapLock, {kNestedLockMutex});
}

// func_089B87B8(): leave — releases it on the outermost exit (depth 1 -> 0).
void func_089B87B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t depth = word(rt, kNestedLockCount);
    rt.memory().store32(kNestedLockCount, depth - 1u);
    ctx.gpr[2] = 1u;
    if (depth == 1u) tail_call(ctx, kHeapUnlock, {kNestedLockMutex});
}

} // namespace gt::game

GT_DECOMP(0x08955300u, gt::game::func_08955300, gt::ReturnKind::Word);
GT_DECOMP(0x089554A8u, gt::game::func_089554A8, gt::ReturnKind::Word);
GT_DECOMP(0x089F9B54u, gt::game::func_089F9B54, gt::ReturnKind::Word);
GT_DECOMP(0x0893C780u, gt::game::func_0893C780, gt::ReturnKind::Word);
GT_DECOMP(0x089FA8A0u, gt::game::func_089FA8A0, gt::ReturnKind::Word);
GT_DECOMP(0x089A8B98u, gt::game::func_089A8B98, gt::ReturnKind::Word);
GT_DECOMP(0x089A8B20u, gt::game::func_089A8B20, gt::ReturnKind::Word);
GT_DECOMP(0x089A8B68u, gt::game::func_089A8B68, gt::ReturnKind::Float);
GT_DECOMP(0x089A8B80u, gt::game::func_089A8B80, gt::ReturnKind::Float);
GT_DECOMP(0x089A8BF4u, gt::game::func_089A8BF4, gt::ReturnKind::Float);
GT_DECOMP(0x089A8C94u, gt::game::func_089A8C94, gt::ReturnKind::Word);
GT_DECOMP(0x089A8C70u, gt::game::func_089A8C70, gt::ReturnKind::Word);
GT_DECOMP(0x089A8C24u, gt::game::func_089A8C24, gt::ReturnKind::Void);
GT_DECOMP(0x089673DCu, gt::game::func_089673DC, gt::ReturnKind::Word);
GT_DECOMP(0x08968C38u, gt::game::func_08968C38, gt::ReturnKind::Word);
GT_DECOMP(0x08B21410u, gt::game::func_08B21410, gt::ReturnKind::Word);
GT_DECOMP(0x08B21590u, gt::game::func_08B21590, gt::ReturnKind::Word);
GT_DECOMP(0x08977368u, gt::game::func_08977368, gt::ReturnKind::Void);
GT_DECOMP(0x088CB910u, gt::game::func_088CB910, gt::ReturnKind::Void);
GT_DECOMP(0x08B1AC1Cu, gt::game::func_08B1AC1C, gt::ReturnKind::Word);
GT_DECOMP(0x08A5BC20u, gt::game::func_08A5BC20, gt::ReturnKind::Word);
GT_DECOMP(0x089B8980u, gt::game::func_089B8980, gt::ReturnKind::Void);
GT_DECOMP(0x089B87B8u, gt::game::func_089B87B8, gt::ReturnKind::Void);

// --- batch 2 ---

namespace gt::game {
namespace {

constexpr std::uint32_t kSlotCount = 0x088D9F80u;       // (this) -> how many 0x2F8-byte slots are in use
constexpr std::uint32_t kSlotLink = 0x088E70B0u;        // func_088E70B0 below
constexpr std::uint32_t kByteAt70 = 0x08899E34u;        // func_08899E34 below
constexpr std::uint32_t kKindTable = 0x08CDE148u;       // u8[256], indexed by the byte at +0x2D
constexpr std::uint32_t kBigRecords = 0x08CE6E10u;      // 0xB60-byte records
constexpr std::uint32_t kEmptyListVtable = 0x08CD2318u;

std::uint32_t slot(psprecomp::Runtime &rt, std::uint32_t self, std::uint32_t i) {
    return word(rt, self + 0xC0u) + i * 0x2F8u;
}

} // namespace

// The 0x2F8-byte slots at this->+0xC0:
// func_088DA788 / func_088DF13C(this, i) -> &slot[i] + 0x30 (two copies).
void func_088DA788(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = slot(rt, ctx.gpr[4], ctx.gpr[5]) + 0x30u; }

// func_088DCD28 / func_088DCD90(this, i) -> slot[i]'s word at +0xC, 0 when i is out of range (two copies).
void func_088DCD28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const auto i = static_cast<std::int32_t>(ctx.gpr[5]);
    std::uint32_t value = 0u;
    if (i >= 0) {
        GuestFrame frame(ctx, 0x10u);
        const auto count = static_cast<std::int32_t>(call_guest(rt, ctx, kSlotCount, {}));
        if (i < count) value = word(rt, slot(rt, self, static_cast<std::uint32_t>(i)) + 0xCu);
    }
    ctx.gpr[2] = value;
}

// func_088DB758(this, i) -> &slot[i] + 0x2E8 after linking it to (i, this, this->+0x5CC); 0 out of range.
void func_088DB758(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const auto i = static_cast<std::int32_t>(ctx.gpr[5]);
    std::uint32_t link = 0u;
    if (i >= 0) {
        GuestFrame frame(ctx, 0x10u);
        const auto count = static_cast<std::int32_t>(call_guest(rt, ctx, kSlotCount, {}));
        if (i < count) {
            link = slot(rt, self, static_cast<std::uint32_t>(i)) + 0x2E8u;
            (void)call_guest(rt, ctx, kSlotLink, {link, static_cast<std::uint32_t>(i), self, word(rt, self + 0x5CCu)});
        }
    }
    ctx.gpr[2] = link;
}

// func_088E70B0(link, index, owner, extra): stores the three (+4, +8, +0xC).
void func_088E70B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store32(ctx.gpr[4] + 0xCu, ctx.gpr[7]);
    rt.memory().store32(ctx.gpr[4] + 0x4u, ctx.gpr[5]);
    rt.memory().store32(ctx.gpr[4] + 0x8u, ctx.gpr[6]);
}

// func_0894CF40(this, i) -> &this->array4[i] (stride 0x2100); func_088C50C4: stride 0x40.
void func_0894CF40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = word(rt, ctx.gpr[4] + 0x4u) + ctx.gpr[5] * 0x2100u; }
void func_088C50C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = word(rt, ctx.gpr[4] + 0x4u) + (ctx.gpr[5] << 6); }

// func_088CB360(i) -> &records[i] (0xB60 bytes each; a madd, so HI changes too).
void func_088CB360(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    const std::uint64_t accumulator = (static_cast<std::uint64_t>(ctx.hi) << 32) | kBigRecords;
    const auto product = static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int32_t>(ctx.gpr[4])) * 0xB60);
    const std::uint64_t sum = accumulator + product;
    ctx.lo = static_cast<std::uint32_t>(sum);
    ctx.hi = static_cast<std::uint32_t>(sum >> 32);
    ctx.gpr[2] = ctx.lo;
}

// func_08954670(this) -> kind_table[this->byte_2D].
void func_08954670(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = rt.memory().load8(kKindTable + rt.memory().load8(ctx.gpr[4] + 0x2Du));
}

// func_08954618(this) -> the byte at +0x2D; with bit 1 of +0x2F set, values 0 and 7 become 0x14,
// 1, 10, 11 and 12 become 0x15 and 9 becomes 0x16.
void func_08954618(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t kind = rt.memory().load8(ctx.gpr[4] + 0x2Du);
    std::uint32_t result = kind;
    if ((rt.memory().load8(ctx.gpr[4] + 0x2Fu) & 2u) != 0u && kind < 13u) {
        const std::uint32_t bit = 1u << kind;
        if ((bit & 0x81u) != 0u) result = 0x14u;
        else if ((bit & 0x1C02u) != 0u) result = 0x15u;
        else if ((bit & 0x200u) != 0u) result = 0x16u;
    }
    ctx.gpr[2] = result;
}

// func_089557BC(this) -> bit 2 of this->ptr8's byte at +0x74B.
void func_089557BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = (rt.memory().load8(word(rt, ctx.gpr[4] + 0x8u) + 0x74Bu) >> 2) & 1u;
}

// func_08955380(this) -> whether this->ptr8's byte at +0xF is non-zero.
void func_08955380(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = rt.memory().load8(word(rt, ctx.gpr[4] + 0x8u) + 0xFu) != 0u ? 1u : 0u;
}

// func_08949A08(this) -> whether the byte at +0x281 is 9.
void func_08949A08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = rt.memory().load8(ctx.gpr[4] + 0x281u) == 9u ? 1u : 0u;
}

// func_0895BBC4(this) -> whether the byte at +0x11 is 1 or 2.
void func_0895BBC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = static_cast<std::uint8_t>(rt.memory().load8(ctx.gpr[4] + 0x11u) - 1u) < 2u ? 1u : 0u;
}

// func_08899E34(holder) -> the byte at +0x70 of the object it points to.
void func_08899E34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = rt.memory().load8(word(rt, ctx.gpr[4]) + 0x70u);
}

// func_0889DD14(this) -> 1 when this->ptr24's word at +0x2C is 0 and the byte at +0x70 of the object
// held at +0x28 is set.
void func_0889DD14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t result = 0u;
    if (word(rt, word(rt, self + 0x24u) + 0x2Cu) == 0u) {
        GuestFrame frame(ctx, 0x10u);
        result = call_guest(rt, ctx, kByteAt70, {self + 0x28u}) != 0u ? 1u : 0u;
    }
    ctx.gpr[2] = result;
}

// func_088E85E8(this) -> the flag byte at +0x2B7, except when the object at +0x2E4 exists and the
// mode at +0x1C is 1: then whether both the flag and that object's byte at +3 are set.
void func_088E85E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t flag = rt.memory().load8(ctx.gpr[4] + 0x2B7u);
    const std::uint32_t other = word(rt, ctx.gpr[4] + 0x2E4u);
    std::uint32_t result = flag;
    if (other != 0u && word(rt, ctx.gpr[4] + 0x1Cu) == 1u) {
        result = (flag != 0u && rt.memory().load8(other + 0x3u) != 0u) ? 1u : 0u;
    }
    ctx.gpr[2] = result;
}

// func_088F8D58(this, i) -> the word picked by the signed byte index at +0x24 + i
// (this->+4 + (2i + index) * 4); 0 when that index is negative, or when i is 0..3 and bit i of the
// mask at +0x2C is set.
void func_088F8D58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const auto i = static_cast<std::int32_t>(ctx.gpr[5]);
    if (i >= 0 && i < 4) {
        const std::uint32_t mask = word(rt, self + 0x2Cu + static_cast<std::uint32_t>(i >> 5) * 4u);
        if (((mask >> (i & 31)) & 1u) != 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
    }
    const auto index = static_cast<std::int32_t>(static_cast<std::int8_t>(rt.memory().load8(self + static_cast<std::uint32_t>(i) + 0x24u)));
    ctx.gpr[2] = index < 0 ? 0u : word(rt, self + static_cast<std::uint32_t>(i * 2 + index) * 4u + 0x4u);
}

// func_088D6218(this, i) -> the big-endian signed 16-bit value i of the table at *this + 0x7A.
void func_088D6218(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t raw = rt.memory().load16(word(rt, ctx.gpr[4]) + ctx.gpr[5] * 2u + 0x7Au);
    const std::uint32_t swapped = ((raw & 0xFFu) << 24) | ((raw >> 8) << 16);  // wsbw of a zero-extended half
    ctx.gpr[2] = static_cast<std::uint32_t>(static_cast<std::int32_t>(swapped) >> 16);
}

// func_08B1EFF8(bits) -> bits / 2^32 as a float in [0, 1) (built from the leading-zero count:
// exponent 0x7E - clz, the bits after the leading 1 as mantissa); 0.0 for 0.
void func_08B1EFF8(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t bits = ctx.gpr[4];
    const std::uint32_t exponent = 0x7Eu - static_cast<std::uint32_t>(std::countl_zero(bits));
    const std::uint32_t mantissa = (bits << ((0u - exponent) & 31u)) >> 9;
    const std::uint32_t result = bits == 0u ? 0u : (exponent << 23) | mantissa;
    ctx.fpr[0] = std::bit_cast<float>(result);
}

// func_08960FA4(this, side) -> this->float18 / 2, negated for sides 0 and 1.
void func_08960FA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    float half = float_at(rt, ctx.gpr[4] + 0x18u) * 0.5f;
    if (static_cast<std::int32_t>(ctx.gpr[5]) < 2) half = -half;
    ctx.fpr[0] = half;
}

// func_08960FD4(this, pair, value in $f13) -> this->rows[pair / 2].float_36C - value (rows of 0x74 bytes).
void func_08960FD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t row = static_cast<std::uint32_t>(static_cast<std::int32_t>(ctx.gpr[5]) >> 1) * 0x74u + ctx.gpr[4];
    ctx.gpr[2] = row;
    ctx.fpr[0] = float_at(rt, row + 0x36Cu) - ctx.fpr[13];
}

// func_08A5B828(list): an empty list — its vtable, +4 and +8 cleared.
void func_08A5B828(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store32(ctx.gpr[4] + 0x4u, 0u);
    rt.memory().store32(ctx.gpr[4], kEmptyListVtable);
    rt.memory().store32(ctx.gpr[4] + 0x8u, 0u);
}

} // namespace gt::game

GT_DECOMP(0x088DA788u, gt::game::func_088DA788, gt::ReturnKind::Word);
GT_DECOMP(0x088DF13Cu, gt::game::func_088DA788, gt::ReturnKind::Word);
GT_DECOMP(0x088DCD28u, gt::game::func_088DCD28, gt::ReturnKind::Word);
GT_DECOMP(0x088DCD90u, gt::game::func_088DCD28, gt::ReturnKind::Word);
GT_DECOMP(0x088DB758u, gt::game::func_088DB758, gt::ReturnKind::Word);
GT_DECOMP(0x088E70B0u, gt::game::func_088E70B0, gt::ReturnKind::Void);
GT_DECOMP(0x0894CF40u, gt::game::func_0894CF40, gt::ReturnKind::Word);
GT_DECOMP(0x088C50C4u, gt::game::func_088C50C4, gt::ReturnKind::Word);
GT_DECOMP(0x088CB360u, gt::game::func_088CB360, gt::ReturnKind::Word);
GT_DECOMP(0x08954670u, gt::game::func_08954670, gt::ReturnKind::Word);
GT_DECOMP(0x08954618u, gt::game::func_08954618, gt::ReturnKind::Word);
GT_DECOMP(0x089557BCu, gt::game::func_089557BC, gt::ReturnKind::Word);
GT_DECOMP(0x08955380u, gt::game::func_08955380, gt::ReturnKind::Word);
GT_DECOMP(0x08949A08u, gt::game::func_08949A08, gt::ReturnKind::Word);
GT_DECOMP(0x0895BBC4u, gt::game::func_0895BBC4, gt::ReturnKind::Word);
GT_DECOMP(0x08899E34u, gt::game::func_08899E34, gt::ReturnKind::Word);
GT_DECOMP(0x0889DD14u, gt::game::func_0889DD14, gt::ReturnKind::Word);
GT_DECOMP(0x088E85E8u, gt::game::func_088E85E8, gt::ReturnKind::Word);
GT_DECOMP(0x088F8D58u, gt::game::func_088F8D58, gt::ReturnKind::Word);
GT_DECOMP(0x088D6218u, gt::game::func_088D6218, gt::ReturnKind::Word);
GT_DECOMP(0x08B1EFF8u, gt::game::func_08B1EFF8, gt::ReturnKind::Float);
GT_DECOMP(0x08960FA4u, gt::game::func_08960FA4, gt::ReturnKind::Float);
GT_DECOMP(0x08960FD4u, gt::game::func_08960FD4, gt::ReturnKind::Float);
GT_DECOMP(0x08A5B828u, gt::game::func_08A5B828, gt::ReturnKind::Void);

// --- batch 3 ---

namespace gt::game {
namespace {

constexpr std::uint32_t kEntryAt = 0x0893C780u;          // func_0893C780 (this, i) -> &this->array10[i]
constexpr std::uint32_t kKindOf = 0x08954670u;           // func_08954670 above
constexpr std::uint32_t kKindTable2 = 0x08CDE130u;       // u8[], the table just before kKindTable
constexpr std::uint32_t kListInit = 0x08A5B828u;         // func_08A5B828 above
constexpr std::uint32_t kListVtable2 = 0x08CD3C58u;
constexpr std::uint32_t kLookup = 0x08AE655Cu;           // (key, 0) -> record or NULL
constexpr std::uint32_t kStride2100Entry = 0x0894CF40u;  // func_0894CF40 above
constexpr std::uint32_t kStreamRead = 0x08AA634Cu;
constexpr std::uint32_t kSharedFind = 0x088884C4u;       // (a, b) -> const shared pointer *
constexpr std::uint32_t kVec3Of = 0x089279B4u;           // (object, vec3 *out)
constexpr std::uint32_t kCrcTable = 0x08CDA288u;         // the IEEE CRC-32 table (0, 0x77073096, ...)

} // namespace

// func_08A5BB5C(object): resets its vtable to the empty list's (a base destructor).
void func_08A5BB5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store32(ctx.gpr[4], kEmptyListVtable);
    ctx.gpr[2] = kEmptyListVtable;
}

// func_08A76ACC(list): an empty list, then the derived vtable and +0xC/+0x10 cleared.
void func_08A76ACC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, kListInit, {self});
    }
    rt.memory().store32(self, kListVtable2);
    rt.memory().store32(self + 0x10u, 0u);
    rt.memory().store32(self + 0xCu, 0u);
    ctx.gpr[2] = kListVtable2;
}

// func_088A3E48(this, i) -> bit 0 of the byte at +0x2F of this->ptr18's entry i; 0 without ptr18.
void func_088A3E48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t owner = word(rt, ctx.gpr[4] + 0x18u);
    std::uint32_t result = 0u;
    if (owner != 0u) {
        GuestFrame frame(ctx, 0x10u);
        const std::uint32_t entry = call_guest(rt, ctx, kEntryAt, {owner});
        result = rt.memory().load8(entry + 0x2Fu) & 1u;
    }
    ctx.gpr[2] = result;
}

// func_088A3E0C(this, i) -> whether that entry's kind (func_08954670) is non-zero; 0 without ptr18.
void func_088A3E0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t owner = word(rt, ctx.gpr[4] + 0x18u);
    std::uint32_t result = 0u;
    if (owner != 0u) {
        GuestFrame frame(ctx, 0x10u);
        const std::uint32_t entry = call_guest(rt, ctx, kEntryAt, {owner});
        result = call_guest(rt, ctx, kKindOf, {entry}) != 0u ? 1u : 0u;
    }
    ctx.gpr[2] = result;
}

// func_088A3844(this) -> 1 when this->ptr18 exists and the byte at +2 of its ptr8 is set.
void func_088A3844(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t owner = word(rt, ctx.gpr[4] + 0x18u);
    ctx.gpr[2] = (owner != 0u && rt.memory().load8(word(rt, owner + 0x8u) + 0x2u) != 0u) ? 1u : 0u;
}

// func_088D09CC(this) -> the word at +0x194 when the flag byte at +0x27 is set, else 0x481907FF.
void func_088D09CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = rt.memory().load8(ctx.gpr[4] + 0x27u) != 0u ? word(rt, ctx.gpr[4] + 0x194u) : 0x481907FFu;
}

// func_08954688(-, i) -> kind_table2[i].
void func_08954688(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = rt.memory().load8(kKindTable2 + ctx.gpr[5]);
}

// func_08903568(a in $f12, b in $f13, period holder, raw) -> a - b, wrapped into
// [-period/2, period/2] (one period at most) unless `raw` (a byte) is set; the period is at +0x14.
void func_08903568(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    float delta = ctx.fpr[12] - ctx.fpr[13];
    if ((ctx.gpr[5] & 0xFFu) == 0u) {
        const float period = float_at(rt, ctx.gpr[4] + 0x14u);
        const float half = period * 0.5f;
        if (half < delta) {
            delta = delta - period;
        } else if (delta < -half) {
            delta = delta + period;
        }
    }
    ctx.fpr[12] = delta;
    ctx.fpr[0] = delta;
}

// func_08968DE4(this): moves bit 3 of the flags at +0xE4 into bit 4 (bit 3 cleared).
void func_08968DE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t flags = word(rt, ctx.gpr[4] + 0xE4u);
    const std::uint32_t bit3 = (flags >> 3) & 1u;
    rt.memory().store32(ctx.gpr[4] + 0xE4u, (flags & ~0x18u) | (bit3 << 4));
    ctx.gpr[2] = bit3;
}

// func_089673F4(this) -> bit 4 of the flags at +0xE4, which it clears.
void func_089673F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t flags = word(rt, ctx.gpr[4] + 0xE4u);
    rt.memory().store32(ctx.gpr[4] + 0xE4u, flags & ~0x10u);
    ctx.gpr[2] = (flags >> 4) & 1u;
}

// func_08AE8658(key) -> the word at +0xC of the record found for `key`, or 0.
void func_08AE8658(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t record = call_guest(rt, ctx, kLookup, {ctx.gpr[4], 0u});
    ctx.gpr[2] = record != 0u ? word(rt, record + 0xCu) : 0u;
}

// func_0894CFF8(this, i) -> &this->array4[i] + 0x2010 (func_0894CF40 + 0x2010).
void func_0894CFF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, kStride2100Entry, {}) + 0x2010u;
}

// func_0895BBB4(this) -> whether the byte at +0x11 is 2.
void func_0895BBB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = rt.memory().load8(ctx.gpr[4] + 0x11u) == 2u ? 1u : 0u;
}

// func_089A8C0C(value in $f12, this): the current entry's float at +0x34 = value.
void func_089A8C0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t entry = current_entry(rt, ctx);
    rt.memory().store32(entry + 0x34u, std::bit_cast<std::uint32_t>(ctx.fpr[12]));
    ctx.gpr[2] = entry;
}

// func_08AA6404(this, out) -> this, after *out = func_08AA634C(this, out).
void func_08AA6404(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t out = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    rt.memory().store32(out, call_guest(rt, ctx, kStreamRead, {}));
    ctx.gpr[2] = self;
}

// func_08888694(out, a, b) -> out: a copy of the shared pointer func_088884C4(a, b) returns (its
// reference count, at +4 of the control block, is incremented).
void func_08888694(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t found = call_guest(rt, ctx, kSharedFind, {ctx.gpr[5], ctx.gpr[6]});
    rt.memory().store32(out, word(rt, found));
    const std::uint32_t control = word(rt, found + 0x4u);
    rt.memory().store32(out + 0x4u, control);
    if (control != 0u) rt.memory().store32(control + 0x4u, word(rt, control + 0x4u) + 1u);
    ctx.gpr[2] = out;
}

// func_08929864(this, vec3 *out): *out = func_089279B4(this->ptr8) (through a stack copy).
void func_08929864(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t tmp = frame.at(0u);
    (void)call_guest(rt, ctx, kVec3Of, {word(rt, ctx.gpr[4] + 0x8u), tmp});
    const std::uint32_t x = word(rt, tmp), y = word(rt, tmp + 4u), z = word(rt, tmp + 8u);
    rt.memory().store32(out + 0x4u, y);
    rt.memory().store32(out, x);
    rt.memory().store32(out + 0x8u, z);
}

// func_08946C58(value in $f12, this, pair) -> value / this->floats_328[pair / 2].
void func_08946C58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t at = static_cast<std::uint32_t>(static_cast<std::int32_t>(ctx.gpr[5]) >> 1) * 4u + ctx.gpr[4];
    ctx.fpr[0] = ctx.fpr[12] / float_at(rt, at + 0x328u);
}

// func_08955358(this, vec3 *out): *out = {v.x, v.z, -v.y} of this->ptr8's vector at +0x6CC.
void func_08955358(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t v = word(rt, ctx.gpr[4] + 0x8u) + 0x6CCu;
    const std::uint32_t out = ctx.gpr[5];
    rt.memory().store32(out, word(rt, v));
    rt.memory().store32(out + 0x4u, word(rt, v + 0x8u));
    rt.memory().store32(out + 0x8u, std::bit_cast<std::uint32_t>(-float_at(rt, v + 0x4u)));
    ctx.gpr[2] = v;
}

// crc32(crc, buffer, length) -> zlib's CRC-32 (IEEE table at 0x08CDA288, ~crc in and out).
// Name evidence: the standard table (0, 0x77073096, 0xEE0E612C, ...) and zlib's signature; GT links
// zlib for pdistd.Deflate/Inflate.
void crc32(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t crc = ~ctx.gpr[4];
    std::uint32_t p = ctx.gpr[5];
    const std::uint32_t end = p + ctx.gpr[6];
    if (ctx.gpr[6] != 0u) {
        do {
            const std::uint32_t byte = rt.memory().load8(p);
            ++p;
            crc = word(rt, kCrcTable + ((byte ^ crc) & 0xFFu) * 4u) ^ (crc >> 8);
        } while (end != p);
    }
    ctx.gpr[2] = ~crc;
}

} // namespace gt::game

GT_DECOMP(0x08A5BB5Cu, gt::game::func_08A5BB5C, gt::ReturnKind::Void);
GT_DECOMP(0x08A76ACCu, gt::game::func_08A76ACC, gt::ReturnKind::Void);
GT_DECOMP(0x088A3E48u, gt::game::func_088A3E48, gt::ReturnKind::Word);
GT_DECOMP(0x088A3E0Cu, gt::game::func_088A3E0C, gt::ReturnKind::Word);
GT_DECOMP(0x088A3844u, gt::game::func_088A3844, gt::ReturnKind::Word);
GT_DECOMP(0x088D09CCu, gt::game::func_088D09CC, gt::ReturnKind::Word);
GT_DECOMP(0x08954688u, gt::game::func_08954688, gt::ReturnKind::Word);
GT_DECOMP(0x08903568u, gt::game::func_08903568, gt::ReturnKind::Float);
GT_DECOMP(0x08968DE4u, gt::game::func_08968DE4, gt::ReturnKind::Void);
GT_DECOMP(0x089673F4u, gt::game::func_089673F4, gt::ReturnKind::Word);
GT_DECOMP(0x08AE8658u, gt::game::func_08AE8658, gt::ReturnKind::Word);
GT_DECOMP(0x0894CFF8u, gt::game::func_0894CFF8, gt::ReturnKind::Word);
GT_DECOMP(0x0895BBB4u, gt::game::func_0895BBB4, gt::ReturnKind::Word);
GT_DECOMP(0x089A8C0Cu, gt::game::func_089A8C0C, gt::ReturnKind::Void);
GT_DECOMP(0x08AA6404u, gt::game::func_08AA6404, gt::ReturnKind::Word);
GT_DECOMP(0x08888694u, gt::game::func_08888694, gt::ReturnKind::Word);
GT_DECOMP(0x08929864u, gt::game::func_08929864, gt::ReturnKind::Void);
GT_DECOMP(0x08946C58u, gt::game::func_08946C58, gt::ReturnKind::Float);
GT_DECOMP(0x08955358u, gt::game::func_08955358, gt::ReturnKind::Void);
GT_DECOMP(0x08B29E88u, gt::game::crc32, gt::ReturnKind::Word);

// --- batch 4 ---

namespace gt::game {
namespace {

constexpr std::uint32_t kElement118 = 0x08955300u;     // func_08955300 (this, i) -> &this->array8[i].member70
constexpr std::uint32_t kCurrentHook = 0x089A8D3Cu;    // (this, other)
constexpr std::uint32_t kSharedRoot = 0x08CFF0E0u;     // a global object pointer
constexpr std::uint32_t kRetain = 0x08A69638u;         // (object)
constexpr std::uint32_t kBufferGrow = 0x08A947F0u;     // (buffer, capacity)
constexpr std::uint32_t kMemcpyGame = 0x08B79A88u;     // gt::libc::memcpy
constexpr std::uint32_t kHolderResetGame = 0x08A5BAD8u; // src/core/refcount.cpp
constexpr std::uint32_t kKeyCopy = 0x08A6A23Cu;        // (key slot *, const key *)
constexpr std::uint32_t kFindCurrent = 0x088A9A0Cu;    // (this) -> object or NULL
constexpr std::uint32_t kSlotCount2 = 0x088D9F80u;     // (this) -> slot count

// Calls func_08955300(owner, i) and returns the element's address.
std::uint32_t element118(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t owner, std::uint32_t i) {
    return call_guest(rt, ctx, kElement118, {owner, i});
}

// The 2048-slot cache at `table`: slots of {key, key's +0x14 word, stamp, holder}.
std::uint32_t cache_slot(psprecomp::Runtime &rt, std::uint32_t table, std::uint32_t key, std::uint32_t stamp_at) {
    const std::uint32_t stamp = word(rt, stamp_at);
    const std::uint32_t hash = (static_cast<std::uint32_t>(static_cast<std::int32_t>(key) >> 3) ^ (stamp >> 4)) & 0x7FFu;
    return table + (hash << 4);
}

} // namespace

// The element of this->ptr18 at index i (a1): 0 / 0.0 without ptr18.
// func_088A3EB8 -> whether its byte at +0x10C is set; func_088A3F40 / func_088A3F10 -> its float at +0x3C / +0x24.
void func_088A3EB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t owner = word(rt, ctx.gpr[4] + 0x18u);
    std::uint32_t result = 0u;
    if (owner != 0u) {
        GuestFrame frame(ctx, 0x10u);
        result = rt.memory().load8(element118(rt, ctx, owner, ctx.gpr[5]) + 0x10Cu) != 0u ? 1u : 0u;
    }
    ctx.gpr[2] = result;
}
void func_088A3F40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t owner = word(rt, ctx.gpr[4] + 0x18u);
    float result = 0.0f;
    if (owner != 0u) {
        GuestFrame frame(ctx, 0x10u);
        result = float_at(rt, element118(rt, ctx, owner, ctx.gpr[5]) + 0x3Cu);
    }
    ctx.fpr[0] = result;
}
void func_088A3F10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t owner = word(rt, ctx.gpr[4] + 0x18u);
    float result = 0.0f;
    if (owner != 0u) {
        GuestFrame frame(ctx, 0x10u);
        result = float_at(rt, element118(rt, ctx, owner, ctx.gpr[5]) + 0x24u);
    }
    ctx.fpr[0] = result;
}

// Through this->ptr8 (its +0xC owner) and the index at this->+4:
// func_088A96FC -> the element's float at +0x54 (0.0 without ptr8).
void func_088A96FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t ptr = word(rt, self + 0x8u);
    float result = 0.0f;
    if (ptr != 0u) {
        GuestFrame frame(ctx, 0x10u);
        result = float_at(rt, element118(rt, ctx, word(rt, ptr + 0xCu), word(rt, self + 0x4u)) + 0x54u);
    }
    ctx.fpr[0] = result;
}
// func_088A97A0(this, float out[4]) -> the element's floats at +0x100, +0x104, +0x108 and +0x30.
void func_088A97A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t out = ctx.gpr[5];
    const std::uint32_t ptr = word(rt, self + 0x8u);
    if (ptr == 0u) return;
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t e = element118(rt, ctx, word(rt, ptr + 0xCu), word(rt, self + 0x4u));
    rt.memory().store32(out, word(rt, e + 0x100u));
    rt.memory().store32(out + 0x4u, word(rt, e + 0x104u));
    rt.memory().store32(out + 0x8u, word(rt, e + 0x108u));
    rt.memory().store32(out + 0xCu, word(rt, e + 0x30u));
}

// func_088FB028(pair) -> the float at +0x14 of pair->second, else of pair->first, else 0.0.
void func_088FB028(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t second = word(rt, ctx.gpr[4] + 0x4u);
    const std::uint32_t first = word(rt, ctx.gpr[4]);
    float result = 0.0f;
    if (second != 0u) result = float_at(rt, second + 0x14u);
    else if (first != 0u) result = float_at(rt, first + 0x14u);
    ctx.fpr[0] = result;
}

// func_088D62FC(this) -> the big-endian signed 16-bit value at *this + 4.
void func_088D62FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t raw = rt.memory().load16(word(rt, ctx.gpr[4]) + 0x4u);
    const std::uint32_t swapped = ((raw & 0xFFu) << 24) | ((raw >> 8) << 16);
    ctx.gpr[2] = static_cast<std::uint32_t>(static_cast<std::int32_t>(swapped) >> 16);
}

// func_088BBB98(this) -> whether this->ptr24's byte at +0x6E is 60 or more.
void func_088BBB98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = rt.memory().load8(word(rt, ctx.gpr[4] + 0x24u) + 0x6Eu) >= 0x3Cu ? 1u : 0u;
}

// func_08955774(this) -> 1 when this->ptr8's byte at +2 is set or its signed half at +0x74C is negative.
void func_08955774(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t q = word(rt, ctx.gpr[4] + 0x8u);
    std::uint32_t result = 1u;
    if (rt.memory().load8(q + 0x2u) == 0u &&
        static_cast<std::int16_t>(rt.memory().load16(q + 0x74Cu)) >= 0) {
        result = 0u;
    }
    ctx.gpr[2] = result;
}

// func_08A696E4(holder *out): *out = the global at 0x08CFF0E0, retained when not null.
void func_08A696E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t root = word(rt, kSharedRoot);
    rt.memory().store32(ctx.gpr[4], root);
    ctx.gpr[2] = root;
    if (root != 0u) tail_call(ctx, kRetain, {root});
}

// func_08A94A28(buffer, data, length): appends `length` bytes, doubling the capacity (from 1) until
// they fit. Buffer: +0x14 bytes, +0x18 capacity, +0x1C size.
void func_08A94A28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t buffer = ctx.gpr[4];
    const std::uint32_t data = ctx.gpr[5];
    const std::uint32_t length = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t needed = word(rt, buffer + 0x1Cu) + length;
    std::uint32_t capacity = word(rt, buffer + 0x18u);
    if (capacity < needed) {
        if (capacity == 0u) capacity = 1u;
        while (capacity < needed) capacity <<= 1;
        (void)call_guest(rt, ctx, kBufferGrow, {buffer, capacity});
    }
    (void)call_guest(rt, ctx, kMemcpyGame, {word(rt, buffer + 0x14u) + word(rt, buffer + 0x1Cu), data, length});
    rt.memory().store32(buffer + 0x1Cu, length + word(rt, buffer + 0x1Cu));
}

// func_089A8D6C(this, other): steps the current index (+0xB4) back by one; when the current entry's
// float at +0x34 is positive it first calls func_089A8D3C(this, other) and, if other's flag 0x2000
// (+0xE0) or 0x80 (+0xE4) is set, also decrements the counter at +0x13C.
void func_089A8D6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t other = ctx.gpr[5];
    std::uint32_t index = word(rt, self + 0xB4u);
    if (0.0f < float_at(rt, current_entry(rt, ctx) + 0x34u)) {
        {
            GuestFrame frame(ctx, 0x10u);
            (void)call_guest(rt, ctx, kCurrentHook, {self, other});
        }
        const bool flagged = (word(rt, other + 0xE0u) & 0x2000u) != 0u || (word(rt, other + 0xE4u) & 0x80u) != 0u;
        if (flagged) {
            const std::uint32_t counter = word(rt, self + 0x13Cu);
            index = word(rt, self + 0xB4u);
            rt.memory().store32(self + 0x13Cu, counter - 1u);
        } else {
            index = word(rt, self + 0xB4u);
        }
    }
    rt.memory().store32(self + 0xB4u, index - 1u);
}

// func_08AAD8BC(table, holder *out, key, const stamp *) -> 1 and *out = the cached holder when the
// slot for (key, stamp) holds exactly that key, key's +0x14 word and stamp; else 0.
void func_08AAD8BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[5];
    const std::uint32_t key = ctx.gpr[6];
    std::uint32_t hit = 0u;
    if (key != 0u) {
        const std::uint32_t slot = cache_slot(rt, ctx.gpr[4], key, ctx.gpr[7]);
        if (word(rt, slot) == key && word(rt, slot + 0x4u) == word(rt, key + 0x14u) &&
            word(rt, slot + 0x8u) == word(rt, ctx.gpr[7])) {
            hit = 1u;
            if (out != slot + 0xCu) {
                GuestFrame frame(ctx, 0x10u);
                (void)call_guest(rt, ctx, kHolderResetGame, {out, word(rt, slot + 0xCu)});
            }
        }
    }
    ctx.gpr[2] = hit;
}

// func_08AAD93C(table, key, const stamp *, const holder *value): stores (key, key's +0x14 word,
// stamp, value) in the key's slot.
void func_08AAD93C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t key = ctx.gpr[5];
    const std::uint32_t stamp = ctx.gpr[6];
    const std::uint32_t value = ctx.gpr[7];
    if (key == 0u) return;
    const std::uint32_t slot = cache_slot(rt, ctx.gpr[4], key, stamp);
    rt.memory().store32(slot, key);
    rt.memory().store32(slot + 0x4u, word(rt, key + 0x14u));
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, kKeyCopy, {slot + 0x8u, stamp});
    }
    if (slot + 0xCu != value) tail_call(ctx, kHolderResetGame, {slot + 0xCu, word(rt, value)});
}

// func_089DDC94(this) -> the item count (8-byte items) of one of two vectors at +0xEC / +0xF8, chosen
// by bit 7 of the word at +0x140.
void func_089DDC94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t which = (word(rt, ctx.gpr[4] + 0x140u) >> 7) & 1u;
    const std::uint32_t base = ctx.gpr[4] + which * 12u + 0xE0u;
    ctx.gpr[2] = static_cast<std::uint32_t>(static_cast<std::int32_t>(word(rt, base + 0x10u) - word(rt, base + 0xCu)) >> 3);
}

// func_088A9A88(this) -> the byte at +0x2D of func_088A9A0C(this)'s object, or 0.
void func_088A9A88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t object = call_guest(rt, ctx, kFindCurrent, {});
    ctx.gpr[2] = object != 0u ? rt.memory().load8(object + 0x2Du) : 0u;
}

// func_088DA1C8(this, id) -> the position of `id` in the u16 list at this->+0x138 (of the slot
// count's length), or -1 (also for a negative or out-of-range id).
void func_088DA1C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const auto id = static_cast<std::int32_t>(ctx.gpr[5]);
    std::int32_t position = -1;
    if (id >= 0) {
        GuestFrame frame(ctx, 0x10u);
        const auto count = static_cast<std::int32_t>(call_guest(rt, ctx, kSlotCount2, {}));
        if (id < count) {
            const std::uint32_t list = word(rt, self + 0x138u);
            for (std::int32_t k = 0; k < count; ++k) {
                if (rt.memory().load16(list + static_cast<std::uint32_t>(k) * 2u) == static_cast<std::uint32_t>(id)) {
                    position = k;
                    break;
                }
            }
        }
    }
    ctx.gpr[2] = static_cast<std::uint32_t>(position);
}

} // namespace gt::game

GT_DECOMP(0x088A3EB8u, gt::game::func_088A3EB8, gt::ReturnKind::Word);
GT_DECOMP(0x088A3F40u, gt::game::func_088A3F40, gt::ReturnKind::Float);
GT_DECOMP(0x088A3F10u, gt::game::func_088A3F10, gt::ReturnKind::Float);
GT_DECOMP(0x088A96FCu, gt::game::func_088A96FC, gt::ReturnKind::Float);
GT_DECOMP(0x088A97A0u, gt::game::func_088A97A0, gt::ReturnKind::Void);
GT_DECOMP(0x088FB028u, gt::game::func_088FB028, gt::ReturnKind::Float);
GT_DECOMP(0x088D62FCu, gt::game::func_088D62FC, gt::ReturnKind::Word);
GT_DECOMP(0x088BBB98u, gt::game::func_088BBB98, gt::ReturnKind::Word);
GT_DECOMP(0x08955774u, gt::game::func_08955774, gt::ReturnKind::Word);
GT_DECOMP(0x08A696E4u, gt::game::func_08A696E4, gt::ReturnKind::Void);
GT_DECOMP(0x08A94A28u, gt::game::func_08A94A28, gt::ReturnKind::Void);
GT_DECOMP(0x089A8D6Cu, gt::game::func_089A8D6C, gt::ReturnKind::Void);
GT_DECOMP(0x08AAD8BCu, gt::game::func_08AAD8BC, gt::ReturnKind::Word);
GT_DECOMP(0x08AAD93Cu, gt::game::func_08AAD93C, gt::ReturnKind::Void);
GT_DECOMP(0x089DDC94u, gt::game::func_089DDC94, gt::ReturnKind::Word);
GT_DECOMP(0x088A9A88u, gt::game::func_088A9A88, gt::ReturnKind::Word);
GT_DECOMP(0x088DA1C8u, gt::game::func_088DA1C8, gt::ReturnKind::Word);

// --- batch 5 ---

namespace gt::game {
namespace {

constexpr std::uint32_t kComputeState = 0x089868A8u;     // (this) -> word stored at +0x34
constexpr std::uint32_t kGlobalVec3 = 0x08D47204u;       // a float[3]
constexpr std::uint32_t kCurrentObject = 0x08D471B8u;    // a global object pointer
constexpr std::uint32_t kObjectUpdate = 0x08AF8434u;     // (object's first word)
constexpr std::uint32_t kAvailable = 0x089588F0u;        // (this) -> bool
constexpr std::uint32_t kDirtyNotify = 0x08AF8EB4u;      // (owner)
constexpr std::uint32_t kBase2174 = 0x08917940u;         // (object) -> base address
constexpr std::uint32_t kSlotValue = 0x088DB5D4u;        // (slots, index) -> word
constexpr std::uint32_t kAngle = 0x0893C54Cu;            // (object) -> angle in radians ($f0)
constexpr std::uint32_t kInit20 = 0x08897788u;           // (member at +0x20)

// The two 8-byte-point vectors of one class (at +0xEC / +0x104, each doubled by bit 7 / 8 of the
// word at +0x140): the vector's data pointer.
std::uint32_t point_data(psprecomp::Runtime &rt, std::uint32_t self, std::uint32_t bit, std::uint32_t base) {
    const std::uint32_t which = (word(rt, self + 0x140u) >> bit) & 1u;
    return word(rt, self + which * 12u + base);
}

} // namespace

// func_08987A74(this): this->+0x34 = func_089868A8(this).
void func_08987A74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    rt.memory().store32(self + 0x34u, call_guest(rt, ctx, kComputeState, {}));
}

// func_08AF7DF4(x in $f12, y in $f13, z in $f14): sets the global vector at 0x08D47204.
void func_08AF7DF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store32(kGlobalVec3 + 0x8u, std::bit_cast<std::uint32_t>(ctx.fpr[14]));
    rt.memory().store32(kGlobalVec3, std::bit_cast<std::uint32_t>(ctx.fpr[12]));
    rt.memory().store32(kGlobalVec3 + 0x4u, std::bit_cast<std::uint32_t>(ctx.fpr[13]));
}

// func_088D4C9C(holder) -> whether the signed byte at +0x4C of the object it points to is non-zero.
void func_088D4C9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = rt.memory().load8(word(rt, ctx.gpr[4]) + 0x4Cu) != 0u ? 1u : 0u;
}

// func_08958904(this) -> when func_089588F0(this) holds, whether this->ptr8's byte at +0x780 is set.
void func_08958904(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    std::uint32_t result = 0u;
    if (call_guest(rt, ctx, kAvailable, {}) != 0u)
        result = rt.memory().load8(word(rt, self + 0x8u) + 0x780u) != 0u ? 1u : 0u;
    ctx.gpr[2] = result;
}

// func_08AF8FF0(this, value): when the signed half at +0x20 differs, stores `value` there and
// notifies func_08AF8EB4(this->first).
void func_08AF8FF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const auto current = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(rt.memory().load16(self + 0x20u))));
    ctx.gpr[2] = current;
    if (current == ctx.gpr[5]) return;
    rt.memory().store16(self + 0x20u, static_cast<std::uint16_t>(ctx.gpr[5]));
    tail_call(ctx, kDirtyNotify, {word(rt, self)});
}

// func_089DB0EC / func_089DAF14(this, i, p0 = ($f12, $f13), p1 = ($f14, $f15)): points i and i + 1 of
// the +0x104 / +0xEC vector = p0, p1.
void store_point_pair(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t bit, std::uint32_t base) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t first = point_data(rt, self, bit, base) + (ctx.gpr[5] << 3);
    rt.memory().store32(first + 0x4u, std::bit_cast<std::uint32_t>(ctx.fpr[13]));
    rt.memory().store32(first, std::bit_cast<std::uint32_t>(ctx.fpr[12]));
    const std::uint32_t second = point_data(rt, self, bit, base) + ((ctx.gpr[5] + 1u) << 3);
    rt.memory().store32(second + 0x4u, std::bit_cast<std::uint32_t>(ctx.fpr[15]));
    rt.memory().store32(second, std::bit_cast<std::uint32_t>(ctx.fpr[14]));
    ctx.gpr[2] = second;
}
void func_089DB0EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_point_pair(rt, ctx, 8u, 0x104u); }
void func_089DAF14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_point_pair(rt, ctx, 7u, 0xECu); }

// func_089DB078(this, i, x, y, w, h): points i and i + 1 of the +0x104 vector = (x + 0, y + 0) and
// (x + w, y + h) — a rectangle's corners (the + 0.0f is the original's, kept for -0.0).
void func_089DB078(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const float x = ctx.fpr[12], y = ctx.fpr[13], w = ctx.fpr[14], h = ctx.fpr[15];
    const std::uint32_t first = point_data(rt, self, 8u, 0x104u) + (ctx.gpr[5] << 3);
    rt.memory().store32(first + 0x4u, std::bit_cast<std::uint32_t>(y + 0.0f));
    rt.memory().store32(first, std::bit_cast<std::uint32_t>(x + 0.0f));
    ctx.fpr[12] = x + w;
    ctx.fpr[13] = y + h;
    ctx.fpr[0] = x + 0.0f;
    ctx.fpr[1] = y + 0.0f;
    const std::uint32_t second = point_data(rt, self, 8u, 0x104u) + ((ctx.gpr[5] + 1u) << 3);
    rt.memory().store32(second + 0x4u, std::bit_cast<std::uint32_t>(ctx.fpr[13]));
    rt.memory().store32(second, std::bit_cast<std::uint32_t>(ctx.fpr[12]));
    ctx.gpr[2] = second;
}

// func_08AF8490(): func_08AF8434(the current object's first word), when there is a current object.
void func_08AF8490(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, kCurrentObject);
    if (object != 0u) tail_call(ctx, kObjectUpdate, {word(rt, object)});
}

// func_08918B0C(this) -> func_08917940(this->ptr4) + 0x2174, or 0 without ptr4.
void func_08918B0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t ptr = word(rt, ctx.gpr[4] + 0x4u);
    std::uint32_t result = 0u;
    if (ptr != 0u) {
        GuestFrame frame(ctx, 0x10u);
        result = call_guest(rt, ctx, kBase2174, {ptr}) + 0x2174u;
    }
    ctx.gpr[2] = result;
}

// func_088E70E4(link) -> func_088DB5D4(link->owner, link->index), or 0 without an owner.
void func_088E70E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t link = ctx.gpr[4];
    const std::uint32_t owner = word(rt, link + 0x8u);
    std::uint32_t result = 0u;
    if (owner != 0u) {
        GuestFrame frame(ctx, 0x10u);
        result = call_guest(rt, ctx, kSlotValue, {owner, word(rt, link + 0x4u)});
    }
    ctx.gpr[2] = result;
}

// func_0894D050(this) -> 1 for mode 2, 2 for mode 3, 0 otherwise (the mode at +0xDC).
void func_0894D050(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t mode = word(rt, ctx.gpr[4] + 0xDCu);
    ctx.gpr[2] = mode == 2u ? 1u : mode == 3u ? 2u : 0u;
}

// func_0894CFB8(this, i) -> &this->array4[i] + 0x14E0.
void func_0894CFB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, kStride2100Entry, {}) + 0x14E0u;
}

// func_08898274(this): func_08897788(&this->member20).
void func_08898274(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, kInit20, {ctx.gpr[4] + 0x20u});
}

// func_0893C60C(a, b) -> angle(a) - angle(b), wrapped into [-pi, pi) by one turn.
void func_0893C60C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr float kPi = std::bit_cast<float>(0x40490FDBu);
    constexpr float kTwoPi = std::bit_cast<float>(0x40C90FDBu);
    const std::uint32_t b = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const float angle_a = call_guest(rt, ctx, kAngle, {}, {}).f0;
    float difference = angle_a - call_guest(rt, ctx, kAngle, {b}, {}).f0;
    if (kPi <= difference) {
        difference = difference - kTwoPi;
    } else if (difference < -kPi) {
        difference = difference + kTwoPi;
    }
    ctx.fpr[0] = difference;
}

} // namespace gt::game

GT_DECOMP(0x08987A74u, gt::game::func_08987A74, gt::ReturnKind::Void);
GT_DECOMP(0x08AF7DF4u, gt::game::func_08AF7DF4, gt::ReturnKind::Void);
GT_DECOMP(0x088D4C9Cu, gt::game::func_088D4C9C, gt::ReturnKind::Word);
GT_DECOMP(0x08958904u, gt::game::func_08958904, gt::ReturnKind::Word);
GT_DECOMP(0x08AF8FF0u, gt::game::func_08AF8FF0, gt::ReturnKind::Void);
GT_DECOMP(0x089DB0ECu, gt::game::func_089DB0EC, gt::ReturnKind::Void);
GT_DECOMP(0x089DAF14u, gt::game::func_089DAF14, gt::ReturnKind::Void);
GT_DECOMP(0x089DB078u, gt::game::func_089DB078, gt::ReturnKind::Void);
GT_DECOMP(0x08AF8490u, gt::game::func_08AF8490, gt::ReturnKind::Void);
GT_DECOMP(0x08918B0Cu, gt::game::func_08918B0C, gt::ReturnKind::Word);
GT_DECOMP(0x088E70E4u, gt::game::func_088E70E4, gt::ReturnKind::Word);
GT_DECOMP(0x0894D050u, gt::game::func_0894D050, gt::ReturnKind::Word);
GT_DECOMP(0x0894CFB8u, gt::game::func_0894CFB8, gt::ReturnKind::Word);
GT_DECOMP(0x08898274u, gt::game::func_08898274, gt::ReturnKind::Void);
GT_DECOMP(0x0893C60Cu, gt::game::func_0893C60C, gt::ReturnKind::Float);

// --- batch 6 ---

namespace gt::game {
namespace {

constexpr std::uint32_t kEnterHook = 0x08D41B04u;        // a function pointer: called by func_08AEB808
constexpr std::uint32_t kLeaveHook = 0x08D41B08u;        // a function pointer: called by func_08AEB82C
constexpr std::uint32_t kEnterSound = 0x08AEB808u;
constexpr std::uint32_t kLeaveSound = 0x08AEB82Cu;
constexpr std::uint32_t kSlotFlag = 0x088E85E8u;         // func_088E85E8 (slot) -> flag
constexpr std::uint32_t kPairLock = 0x08CF4A2Cu;         // mutex guarding func_089AA95C's pair
constexpr std::uint32_t kStackLock = 0x08CF4A54u;        // mutex guarding func_089AAC8C's stack
constexpr std::uint32_t kCopyLock = 0x08CF55FCu;         // mutex guarding func_089E0570's copy
constexpr std::uint32_t kCopyFrom = 0x089E4C24u;         // (out, source)
constexpr std::uint32_t kPendingObject = 0x08D471B8u;    // same global as kCurrentObject
constexpr std::uint32_t kStopTarget = 0x08B537DCu;       // (handle)
constexpr std::uint32_t kResetState = 0x08AFBCC8u;       // (state, 0)
constexpr std::uint32_t kResetStateArg = 0x08D472ACu;
constexpr std::uint32_t kHasThing = 0x088C541Cu;         // (object) -> word
constexpr std::uint32_t kFindActive = 0x08AF26E4u;       // () -> object or NULL
constexpr std::uint32_t kActiveValue = 0x08AF1430u;      // (object) -> word

} // namespace

// func_08AEB808() / func_08AEB82C(): call the function pointers at 0x08D41B04 / 0x08D41B08 (the
// enter/leave pair around every change of a sound parameter block).
void func_08AEB808(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, word(rt, kEnterHook), {});
}
void func_08AEB82C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, word(rt, kLeaveHook), {});
}

// func_08AD8CB0(block, index, value): a sound parameter block — signed 16-bit values from +4 by
// index (2..12), +0 and +2 bit masks of changed / set values. Stores `value` and marks it in both
// masks (between enter and leave) unless it is already set to that value.
void func_08AD8CB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t block = ctx.gpr[4];
    const std::uint32_t index = ctx.gpr[5];
    const std::uint32_t value = ctx.gpr[6];
    if (index - 2u >= 11u) return;
    const auto set_mask = static_cast<std::int32_t>(static_cast<std::int16_t>(rt.memory().load16(block + 0x2u)));
    if (((set_mask >> (index & 31)) & 1) != 0) {
        const auto current = static_cast<std::int32_t>(static_cast<std::int16_t>(rt.memory().load16(block + index * 2u)));
        if (static_cast<std::uint32_t>(current) == value) return;
    }
    GuestFrame frame(ctx, 0x20u);
    (void)call_guest(rt, ctx, kEnterSound, {});
    rt.memory().store16(block + index * 2u, static_cast<std::uint16_t>(value));
    const std::uint32_t bit = 1u << (index & 31);
    const std::uint32_t set = rt.memory().load16(block + 0x2u) | bit;
    const std::uint32_t changed = rt.memory().load16(block) | bit;
    rt.memory().store16(block, static_cast<std::uint16_t>(changed));
    rt.memory().store16(block + 0x2u, static_cast<std::uint16_t>(set));
    (void)call_guest(rt, ctx, kLeaveSound, {});
}

// func_088DB5D4(this, i) -> 1 when slot i is in range, holds something (+0xC), is of type 1 (+4)
// and func_088E85E8(slot) holds; else 0.
void func_088DB5D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const auto i = static_cast<std::int32_t>(ctx.gpr[5]);
    std::uint32_t result = 0u;
    if (i >= 0) {
        GuestFrame frame(ctx, 0x10u);
        const auto count = static_cast<std::int32_t>(call_guest(rt, ctx, kSlotCount2, {}));
        if (i < count) {
            const std::uint32_t slot_at = word(rt, self + 0xC0u) + static_cast<std::uint32_t>(i) * 0x2F8u;
            if (word(rt, slot_at + 0xCu) != 0u && word(rt, slot_at + 0x4u) == 1u)
                result = call_guest(rt, ctx, kSlotFlag, {slot_at}) != 0u ? 1u : 0u;
        }
    }
    ctx.gpr[2] = result;
}

// func_089AA95C(this, float *a, float *b): *a, *b = the floats at +0xE4, +0xE8, under a lock.
void func_089AA95C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t a = ctx.gpr[5];
    const std::uint32_t b = ctx.gpr[6];
    {
        GuestFrame frame(ctx, 0x20u);
        (void)call_guest(rt, ctx, kHeapLock, {kPairLock});
    }
    rt.memory().store32(a, word(rt, self + 0xE4u));
    rt.memory().store32(b, word(rt, self + 0xE8u));
    tail_call(ctx, kHeapUnlock, {kPairLock});
}

// func_089AAC8C(this) -> the topmost element of the stack (+0x24 bottom, +0x28 top) whose flags at
// +0x138 have 0x100 set, or NULL; under a lock.
void func_089AAC8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    (void)call_guest(rt, ctx, kHeapLock, {kStackLock});
    const std::uint32_t bottom = word(rt, self + 0x24u);
    std::uint32_t found = 0u;
    for (std::uint32_t top = word(rt, self + 0x28u); top != bottom; top -= 4u) {
        const std::uint32_t element = word(rt, top - 4u);
        if ((word(rt, element + 0x138u) & 0x100u) != 0u) {
            found = element;
            break;
        }
    }
    (void)call_guest(rt, ctx, kHeapUnlock, {kStackLock});
    ctx.gpr[2] = found;
}

// func_089E0570(out, source) -> out, after func_089E4C24(out, &source->member20C) under a lock.
void func_089E0570(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t source = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    (void)call_guest(rt, ctx, kHeapLock, {kCopyLock});
    (void)call_guest(rt, ctx, kCopyFrom, {out, source + 0x20Cu});
    (void)call_guest(rt, ctx, kHeapUnlock, {kCopyLock});
    ctx.gpr[2] = out;
}

// func_08AF8434(object): when an object is pending (0x08D471B8), clears it, stops the object's handle
// at +0x94 if it has one and its count at +0xC is non-zero, then resets the state at 0x08D472AC.
void func_08AF8434(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4];
    if (word(rt, kPendingObject) == 0u) return;
    rt.memory().store32(kPendingObject, 0u);
    if (rt.memory().load16(object + 0xCu) != 0u) {
        const std::uint32_t handle = word(rt, object + 0x94u);
        if (handle != 0u) {
            GuestFrame frame(ctx, 0x10u);
            (void)call_guest(rt, ctx, kStopTarget, {handle});
        }
    }
    tail_call(ctx, kResetState, {kResetStateArg, 0u});
}

// func_088C5718(this) -> whether func_088C541C(this->ptr4) is non-zero.
void func_088C5718(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, kHasThing, {word(rt, ctx.gpr[4] + 0x4u)}) != 0u ? 1u : 0u;
}

// func_088CE8B4(this) -> whether this->ptr4 has any slot (func_088D9F80).
void func_088CE8B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, kSlotCount2, {word(rt, ctx.gpr[4] + 0x4u)}) != 0u ? 1u : 0u;
}

// func_088FE518(holder) -> the object it points to when that object's word at +0x20 is set, else 0.
void func_088FE518(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4]);
    ctx.gpr[2] = word(rt, object + 0x20u) != 0u ? object : 0u;
}

// func_08AF26F0() -> func_08AF1430(the active object), or 0 when there is none.
void func_08AF26F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t active;
    {
        GuestFrame frame(ctx, 0x10u);
        active = call_guest(rt, ctx, kFindActive, {});
    }
    if (active != 0u) {
        tail_call(ctx, kActiveValue, {active});
        return;
    }
    ctx.gpr[2] = 0u;
}

// func_08AF2570(list, arg): calls every element's virtual at +8 with `arg` (a vector of pointers at
// +4..+8).
void func_08AF2570(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t arg = ctx.gpr[5];
    const std::uint32_t end = word(rt, ctx.gpr[4] + 0x8u);
    GuestFrame frame(ctx, 0x10u);
    for (std::uint32_t p = word(rt, ctx.gpr[4] + 0x4u); p != end; p += 4u) {
        const std::uint32_t element = word(rt, p);
        (void)call_guest(rt, ctx, word(rt, word(rt, element) + 0x8u), {element, arg});
    }
}

// func_08B2ACDC(a, b) -> a, after a &= ~b over their common length (bit sets as word vectors at +4..+8).
void func_08B2ACDC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = ctx.gpr[4];
    const std::uint32_t b = ctx.gpr[5];
    std::uint32_t p = word(rt, a + 0x4u);
    std::uint32_t q = word(rt, b + 0x4u);
    const auto length_a = static_cast<std::uint32_t>(static_cast<std::int32_t>(word(rt, a + 0x8u) - p) >> 2);
    const auto length_b = static_cast<std::uint32_t>(static_cast<std::int32_t>(word(rt, b + 0x8u) - q) >> 2);
    const std::uint32_t end = p + (length_b < length_a ? length_b : length_a) * 4u;
    for (; p < end; p += 4u, q += 4u) rt.memory().store32(p, word(rt, p) & ~word(rt, q));
    ctx.gpr[2] = a;
}

// func_08B2A8E0(set) -> set, with every word of its vector (+4..+8) cleared.
void func_08B2A8E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t last = word(rt, ctx.gpr[4] + 0x8u) - 4u;
    for (std::uint32_t p = word(rt, ctx.gpr[4] + 0x4u); !(last < p); p += 4u) rt.memory().store32(p, 0u);
    ctx.gpr[2] = ctx.gpr[4];
}

} // namespace gt::game

GT_DECOMP(0x08AEB808u, gt::game::func_08AEB808, gt::ReturnKind::Void);
GT_DECOMP(0x08AEB82Cu, gt::game::func_08AEB82C, gt::ReturnKind::Void);
GT_DECOMP(0x08AD8CB0u, gt::game::func_08AD8CB0, gt::ReturnKind::Void);
GT_DECOMP(0x088DB5D4u, gt::game::func_088DB5D4, gt::ReturnKind::Word);
GT_DECOMP(0x089AA95Cu, gt::game::func_089AA95C, gt::ReturnKind::Void);
GT_DECOMP(0x089AAC8Cu, gt::game::func_089AAC8C, gt::ReturnKind::Word);
GT_DECOMP(0x089E0570u, gt::game::func_089E0570, gt::ReturnKind::Word);
GT_DECOMP(0x08AF8434u, gt::game::func_08AF8434, gt::ReturnKind::Void);
GT_DECOMP(0x088C5718u, gt::game::func_088C5718, gt::ReturnKind::Word);
GT_DECOMP(0x088CE8B4u, gt::game::func_088CE8B4, gt::ReturnKind::Word);
GT_DECOMP(0x088FE518u, gt::game::func_088FE518, gt::ReturnKind::Word);
GT_DECOMP(0x08AF26F0u, gt::game::func_08AF26F0, gt::ReturnKind::Word);
GT_DECOMP(0x08AF2570u, gt::game::func_08AF2570, gt::ReturnKind::Void);
GT_DECOMP(0x08B2ACDCu, gt::game::func_08B2ACDC, gt::ReturnKind::Word);
GT_DECOMP(0x08B2A8E0u, gt::game::func_08B2A8E0, gt::ReturnKind::Word);

// --- batch 7 ---

namespace gt::game {
namespace {

constexpr std::uint32_t kDistanceOf = 0x088C3808u;       // (this, other) -> distance ($f0)
constexpr std::uint32_t kVtable72F8 = 0x08CD72F8u;
constexpr std::uint32_t kRecordOf = 0x089277B4u;         // (object) -> record
constexpr std::uint32_t kDrawWith = 0x08A4D79Cu;         // (renderer, a, 0, 0, 0)
constexpr std::uint32_t kRenderer = 0x08CF68E8u;         // a global renderer object pointer
constexpr std::uint32_t kDrawWithExtra = 0x08A4DB18u;    // func_08A4DB18 below
constexpr std::uint32_t kEntryPlus10 = 0x089A8B20u;      // func_089A8B20 (this) -> &current entry + 0x10
constexpr std::uint32_t kCanPop = 0x089A8EA8u;           // (this) -> bool
constexpr std::uint32_t kSmallTable = 0x08C8E8A4u;       // word[9]
constexpr std::uint32_t kChildBusy = 0x088E77B4u;        // (child) -> word
constexpr std::uint32_t kSelfBusy = 0x088D9CC4u;         // (this) -> word
constexpr std::uint32_t kStaticObject = 0x08D689D0u;
constexpr std::uint32_t kStaticInit = 0x08C19DFCu;       // (object)

} // namespace

// func_08977270(this, i) -> the word i of the array at +0x8C.
void func_08977270(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = word(rt, (ctx.gpr[5] << 2) + ctx.gpr[4] + 0x8Cu);
}

// func_088C3858(this, other): keeps the nearest — when func_088C3808(this, other) is below
// this->ptr24's float at +0x98, stores it there and other's id byte (+0x10) at +0xA0.
void func_088C3858(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t other = ctx.gpr[5];
    const std::uint32_t best = word(rt, ctx.gpr[4] + 0x24u);
    GuestFrame frame(ctx, 0x10u);
    const float distance = call_guest(rt, ctx, kDistanceOf, {}, {}).f0;
    if (distance < float_at(rt, best + 0x98u)) {
        rt.memory().store32(best + 0x98u, std::bit_cast<std::uint32_t>(distance));
        rt.memory().store8(best + 0xA0u, static_cast<std::uint8_t>(word(rt, other + 0x10u)));
    }
}

// func_08AF484C(object): sets its vtable (a base destructor or constructor).
void func_08AF484C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store32(ctx.gpr[4], kVtable72F8);
    ctx.gpr[2] = kVtable72F8;
}

// func_089296BC / func_0892969C(this) -> the float at +0x88 / +0x84 of func_089277B4(this->ptr8).
void func_089296BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t record = call_guest(rt, ctx, kRecordOf, {word(rt, ctx.gpr[4] + 0x8u)});
    ctx.gpr[2] = record;
    ctx.fpr[0] = float_at(rt, record + 0x88u);
}
void func_0892969C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t record = call_guest(rt, ctx, kRecordOf, {word(rt, ctx.gpr[4] + 0x8u)});
    ctx.gpr[2] = record;
    ctx.fpr[0] = float_at(rt, record + 0x84u);
}

// func_089099A0(this) -> the u16 that this->ptr10 points to, or 0.
void func_089099A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t p = word(rt, ctx.gpr[4] + 0x10u);
    ctx.gpr[2] = p != 0u ? rt.memory().load16(p) : 0u;
}

// func_08A4DB18(renderer, a, extra): func_08A4D79C(renderer, a, 0, 0, 0) with `extra` in +0x6C
// for the duration of the call.
void func_08A4DB18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t renderer = ctx.gpr[4];
    rt.memory().store32(renderer + 0x6Cu, ctx.gpr[6]);
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, kDrawWith, {renderer, ctx.gpr[5], 0u, 0u, 0u});
    }
    rt.memory().store32(renderer + 0x6Cu, 0u);
}

// func_089A9478(this, holder *h): func_08A4DB18(the global renderer, *h, &current entry + 0x10).
void func_089A9478(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t renderer = word(rt, kRenderer);
    const std::uint32_t held = word(rt, ctx.gpr[5]);
    std::uint32_t extra;
    {
        GuestFrame frame(ctx, 0x10u);
        extra = call_guest(rt, ctx, kEntryPlus10, {});
    }
    tail_call(ctx, kDrawWithExtra, {renderer, held, extra});
}

// func_089A8CE0(this) -> whether the flag byte at +0x138 and the word at +0x13C are both set.
void func_089A8CE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = (rt.memory().load8(ctx.gpr[4] + 0x138u) != 0u && word(rt, ctx.gpr[4] + 0x13Cu) != 0u) ? 1u : 0u;
}

// func_089A8B38 / func_089A8B50(this) -> the current entry's float at +0 / +4.
void func_089A8B38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t entry = current_entry(rt, ctx);
    ctx.gpr[2] = entry;
    ctx.fpr[0] = float_at(rt, entry);
}
void func_089A8B50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t entry = current_entry(rt, ctx);
    ctx.gpr[2] = entry;
    ctx.fpr[0] = float_at(rt, entry + 0x4u);
}

// func_089A8EB0(this): when func_089A8EA8(this) allows it, pops the stack of 16-byte items
// (+0x58 items, +0x5C count): calls the virtual at +0x144 with the new top item, or the one at
// +0x140 when the stack became empty.
void func_089A8EB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        if (call_guest(rt, ctx, kCanPop, {self}) == 0u) return;
    }
    const std::uint32_t count = word(rt, self + 0x5Cu) - 1u;
    rt.memory().store32(self + 0x5Cu, count);
    const std::uint32_t vtable = word(rt, self);
    if (static_cast<std::int32_t>(count) >= 0) {
        tail_call(ctx, word(rt, vtable + 0x144u), {self, word(rt, self + 0x58u) + (count << 4)});
    } else {
        tail_call(ctx, word(rt, vtable + 0x140u), {self});
    }
}

// A ring buffer: +4 read, +8 write, +0xC used, +0x18 capacity.
// func_08B24A8C(ring, n): advances the read position by n (wrapping) and adds n to the used count;
// func_08B24A60(ring, n): advances the write position by n and subtracts n from it.
void func_08B24A8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t ring = ctx.gpr[4];
    const std::uint32_t n = ctx.gpr[5];
    std::uint32_t position = word(rt, ring + 0x4u) + n;
    const std::uint32_t capacity = word(rt, ring + 0x18u);
    if (!(position < capacity)) position -= capacity;
    const std::uint32_t used = word(rt, ring + 0xCu) + n;
    rt.memory().store32(ring + 0x4u, position);
    rt.memory().store32(ring + 0xCu, used);
    ctx.gpr[2] = position;
}
void func_08B24A60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t ring = ctx.gpr[4];
    const std::uint32_t n = ctx.gpr[5];
    std::uint32_t position = word(rt, ring + 0x8u) + n;
    const std::uint32_t capacity = word(rt, ring + 0x18u);
    if (!(position < capacity)) position -= capacity;
    const std::uint32_t used = word(rt, ring + 0xCu) - n;
    rt.memory().store32(ring + 0x8u, position);
    rt.memory().store32(ring + 0xCu, used);
    ctx.gpr[2] = position;
}

// func_08A4CCE8(-, i) -> table[i] for i 0..8, table[4] otherwise.
void func_08A4CCE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t i = ctx.gpr[5] < 9u ? ctx.gpr[5] : 4u;
    ctx.gpr[2] = word(rt, kSmallTable + (i << 2));
}

// func_08983440(this): copies the renderer's floats at +0x34/+0x38 to +0xAC/+0xB0 and clears
// the renderer's flag 0x2000 (+0x58).
void func_08983440(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t renderer = word(rt, kRenderer);
    rt.memory().store32(ctx.gpr[4] + 0xACu, word(rt, renderer + 0x34u));
    rt.memory().store32(ctx.gpr[4] + 0xB0u, word(rt, renderer + 0x38u));
    const std::uint32_t flags = word(rt, renderer + 0x58u) & ~0x2000u;
    rt.memory().store32(renderer + 0x58u, flags);
    ctx.gpr[2] = flags;
}

// func_088D9CCC(this) -> 1 when it is busy: either flag byte +0x5B6 / +0x5B7, func_088D9CC4(this),
// or func_088E77B4 of the child at +0x630.
void func_088D9CCC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    std::uint32_t child_busy = 0u;
    const std::uint32_t child = word(rt, self + 0x630u);
    if (child != 0u) child_busy = call_guest(rt, ctx, kChildBusy, {child});
    std::uint32_t busy = 1u;
    if (rt.memory().load8(self + 0x5B6u) == 0u && rt.memory().load8(self + 0x5B7u) == 0u &&
        call_guest(rt, ctx, kSelfBusy, {self}) == 0u && child_busy == 0u) {
        busy = 0u;
    }
    ctx.gpr[2] = busy;
}

// func_08B1A844(): func_08C19DFC(the static object at 0x08D689D0).
void func_08B1A844(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, kStaticInit, {kStaticObject});
}

} // namespace gt::game

GT_DECOMP(0x08977270u, gt::game::func_08977270, gt::ReturnKind::Word);
GT_DECOMP(0x088C3858u, gt::game::func_088C3858, gt::ReturnKind::Void);
GT_DECOMP(0x08AF484Cu, gt::game::func_08AF484C, gt::ReturnKind::Void);
GT_DECOMP(0x089296BCu, gt::game::func_089296BC, gt::ReturnKind::Float);
GT_DECOMP(0x0892969Cu, gt::game::func_0892969C, gt::ReturnKind::Float);
GT_DECOMP(0x089099A0u, gt::game::func_089099A0, gt::ReturnKind::Word);
GT_DECOMP(0x08A4DB18u, gt::game::func_08A4DB18, gt::ReturnKind::Void);
GT_DECOMP(0x089A9478u, gt::game::func_089A9478, gt::ReturnKind::Void);
GT_DECOMP(0x089A8CE0u, gt::game::func_089A8CE0, gt::ReturnKind::Word);
GT_DECOMP(0x089A8B38u, gt::game::func_089A8B38, gt::ReturnKind::Float);
GT_DECOMP(0x089A8B50u, gt::game::func_089A8B50, gt::ReturnKind::Float);
GT_DECOMP(0x089A8EB0u, gt::game::func_089A8EB0, gt::ReturnKind::Void);
GT_DECOMP(0x08B24A8Cu, gt::game::func_08B24A8C, gt::ReturnKind::Void);
GT_DECOMP(0x08B24A60u, gt::game::func_08B24A60, gt::ReturnKind::Void);
GT_DECOMP(0x08A4CCE8u, gt::game::func_08A4CCE8, gt::ReturnKind::Word);
GT_DECOMP(0x08983440u, gt::game::func_08983440, gt::ReturnKind::Void);
GT_DECOMP(0x088D9CCCu, gt::game::func_088D9CCC, gt::ReturnKind::Word);
GT_DECOMP(0x08B1A844u, gt::game::func_08B1A844, gt::ReturnKind::Void);

// --- batch 8 ---

namespace gt::game {
namespace {

constexpr std::uint32_t kStateLock = 0x08CF4ACCu;        // mutex guarding func_089AB0C8 / func_089AA910's reads
constexpr std::uint32_t kSlotKind = 0x088E7F14u;         // (slot) -> kind
constexpr std::uint32_t kSlotFlagAt = 0x088DA424u;       // func_088DA424 below
constexpr std::uint32_t kGroupCount = 0x08AF6E90u;       // (group) -> count

} // namespace

// func_088DC90C(this, i) -> slot i's type (+4), or 3 when i is out of range.
void func_088DC90C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const auto i = static_cast<std::int32_t>(ctx.gpr[5]);
    std::uint32_t type = 3u;
    if (i >= 0) {
        GuestFrame frame(ctx, 0x10u);
        if (i < static_cast<std::int32_t>(call_guest(rt, ctx, kSlotCount2, {})))
            type = word(rt, slot(rt, self, static_cast<std::uint32_t>(i)) + 0x4u);
    }
    ctx.gpr[2] = type;
}

// func_088DB6E4(this, i) -> func_088E7F14(&slot[i]), or -1 when i is out of range.
void func_088DB6E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const auto i = static_cast<std::int32_t>(ctx.gpr[5]);
    if (i >= 0) {
        std::int32_t count;
        {
            GuestFrame frame(ctx, 0x10u);
            count = static_cast<std::int32_t>(call_guest(rt, ctx, kSlotCount2, {}));
        }
        if (i < count) {
            tail_call(ctx, kSlotKind, {slot(rt, self, static_cast<std::uint32_t>(i))});
            return;
        }
    }
    ctx.gpr[2] = 0xFFFFFFFFu;
}

// func_088DA424(this, i) -> func_088E85E8(&slot[i]) (no range check).
void func_088DA424(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, kSlotFlag, {slot(rt, ctx.gpr[4], ctx.gpr[5])});
}

// func_088E711C(link) -> func_088DA424(link->owner, link->index), or 0 without an owner.
void func_088E711C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t link = ctx.gpr[4];
    const std::uint32_t owner = word(rt, link + 0x8u);
    std::uint32_t result = 0u;
    if (owner != 0u) {
        GuestFrame frame(ctx, 0x10u);
        result = call_guest(rt, ctx, kSlotFlagAt, {owner, word(rt, link + 0x4u)});
    }
    ctx.gpr[2] = result;
}

// func_088DA0BC(this) -> whether the byte at +0x30 isn't 3.
void func_088DA0BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = rt.memory().load8(ctx.gpr[4] + 0x30u) != 3u ? 1u : 0u;
}

// func_089AB0C8(this) -> the word at +0xC4; func_089AA910(this) -> the byte at +0xFC; both read
// under the lock at 0x08CF4ACC (taken and released around the read).
void func_089AB0C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    (void)call_guest(rt, ctx, kHeapLock, {kStateLock});
    const std::uint32_t value = word(rt, self + 0xC4u);
    (void)call_guest(rt, ctx, kHeapUnlock, {kStateLock});
    ctx.gpr[2] = value;
}
void func_089AA910(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    (void)call_guest(rt, ctx, kHeapLock, {kStateLock});
    const std::uint32_t value = rt.memory().load8(self + 0xFCu);
    (void)call_guest(rt, ctx, kHeapUnlock, {kStateLock});
    ctx.gpr[2] = value;
}

// func_08B21C64 / func_08B21894(record) -> record + 4 + the u16 at +4: the address just past its
// length-prefixed block (two copies).
void func_08B21C64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = ctx.gpr[4] + 0x4u + rt.memory().load16(ctx.gpr[4] + 0x4u);
}

// func_08B2AABC(a, b) -> a, after copying b's words into a over their common length and clearing
// the rest of a (bit sets as word vectors at +4..+8).
void func_08B2AABC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = ctx.gpr[4];
    const std::uint32_t b = ctx.gpr[5];
    std::uint32_t p = word(rt, a + 0x4u);
    std::uint32_t q = word(rt, b + 0x4u);
    const std::uint32_t last = word(rt, a + 0x8u) - 4u;
    const auto length_a = static_cast<std::uint32_t>(static_cast<std::int32_t>(word(rt, a + 0x8u) - p) >> 2);
    const auto length_b = static_cast<std::uint32_t>(static_cast<std::int32_t>(word(rt, b + 0x8u) - q) >> 2);
    std::uint32_t copy_end = p + (length_b < length_a ? length_b : length_a) * 4u;
    for (; p < copy_end; p += 4u, q += 4u) rt.memory().store32(p, word(rt, q));
    for (; !(last < copy_end); copy_end += 4u) rt.memory().store32(copy_end, 0u);
    ctx.gpr[2] = a;
}

// func_089F9B34(this, i) -> &this->array0[i] (stride 0x20; a second copy of func_089F9B54).
void func_089F9B34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = word(rt, ctx.gpr[4]) + (ctx.gpr[5] << 5);
}

// func_08906398(holder) -> the byte at +0x158 of the object it points to.
void func_08906398(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = rt.memory().load8(word(rt, ctx.gpr[4]) + 0x158u);
}

// func_08977358(this, i, value): the word i of the array at +0x8C = value.
void func_08977358(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store32((ctx.gpr[5] << 2) + ctx.gpr[4] + 0x8Cu, ctx.gpr[6]);
}

// func_08B03BB4(holder **h, i) -> &(**h)[i] (stride 0xF0).
void func_08B03BB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = word(rt, word(rt, ctx.gpr[4])) + ctx.gpr[5] * 0xF0u;
}

// func_08AF923C(this) -> the larger (signed) of the u16 at +6 of this->ptr84 and
// func_08AF6E90(this->ptr48); each counts as 1 when its pointer is null.
void func_08AF923C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t a = 1u;
    std::uint32_t b = 1u;
    const std::uint32_t first = word(rt, self + 0x84u);
    if (first != 0u) a = rt.memory().load16(first + 0x6u);
    const std::uint32_t group = word(rt, self + 0x48u);
    GuestFrame frame(ctx, 0x20u);
    if (group != 0u) b = call_guest(rt, ctx, kGroupCount, {group});
    ctx.gpr[2] = static_cast<std::int32_t>(a) < static_cast<std::int32_t>(b) ? b : a;
}

} // namespace gt::game

GT_DECOMP(0x088DC90Cu, gt::game::func_088DC90C, gt::ReturnKind::Word);
GT_DECOMP(0x088DB6E4u, gt::game::func_088DB6E4, gt::ReturnKind::Word);
GT_DECOMP(0x088DA424u, gt::game::func_088DA424, gt::ReturnKind::Word);
GT_DECOMP(0x088E711Cu, gt::game::func_088E711C, gt::ReturnKind::Word);
GT_DECOMP(0x088DA0BCu, gt::game::func_088DA0BC, gt::ReturnKind::Word);
GT_DECOMP(0x089AB0C8u, gt::game::func_089AB0C8, gt::ReturnKind::Word);
GT_DECOMP(0x089AA910u, gt::game::func_089AA910, gt::ReturnKind::Word);
GT_DECOMP(0x08B21C64u, gt::game::func_08B21C64, gt::ReturnKind::Word);
GT_DECOMP(0x08B21894u, gt::game::func_08B21C64, gt::ReturnKind::Word);
GT_DECOMP(0x08B2AABCu, gt::game::func_08B2AABC, gt::ReturnKind::Word);
GT_DECOMP(0x08906398u, gt::game::func_08906398, gt::ReturnKind::Word);
GT_DECOMP(0x08977358u, gt::game::func_08977358, gt::ReturnKind::Void);
GT_DECOMP(0x08B03BB4u, gt::game::func_08B03BB4, gt::ReturnKind::Word);
GT_DECOMP(0x08AF923Cu, gt::game::func_08AF923C, gt::ReturnKind::Word);
GT_DECOMP(0x089F9B34u, gt::game::func_089F9B34, gt::ReturnKind::Word);

// --- batch 9 ---

namespace gt::game {
namespace {

constexpr std::uint32_t kStride2000 = 0x0894D018u;       // func_0894D018 below
constexpr std::uint32_t kKindCheck = 0x088A5984u;        // (object) -> kind
constexpr std::uint32_t kApplyEntry = 0x0895F174u;       // (this, entry)
constexpr std::uint32_t kCounterOwner = 0x08959180u;     // (this) -> object with a countdown at +0x78
constexpr std::uint32_t kIsActive = 0x08948170u;         // (this) -> bool
constexpr std::uint32_t kCountMatching = 0x089444F8u;    // (this, 4, 0, 0) -> count

} // namespace

// func_0894D018(this, i) -> &this->array4[i] + 0x2000.
void func_0894D018(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, kStride2100Entry, {}) + 0x2000u;
}

// func_08949AF4(args) -> func_0894D018(args->first, *args->ptr8 as a byte).
void func_08949AF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t args = ctx.gpr[4];
    tail_call(ctx, kStride2000, {word(rt, args), rt.memory().load8(word(rt, args + 0x8u))});
}

// func_088E7FA4(this, const float v[3], const float *w): this->ptr2E4's floats +0x20..+0x28 = v
// and +0x2C = w[1]; nothing without ptr2E4.
void func_088E7FA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t v = ctx.gpr[5];
    if (word(rt, self + 0x2E4u) == 0u) return;
    for (std::uint32_t k = 0u; k < 3u; ++k)
        rt.memory().store32(word(rt, self + 0x2E4u) + k * 4u + 0x20u, word(rt, v + k * 4u));
    rt.memory().store32(word(rt, self + 0x2E4u) + 0x2Cu, word(rt, ctx.gpr[6] + 0x4u));
}

// func_088C37C0(this): resets this->ptr24's tracking state (ids to 0xFF, distances to 1000.0f,
// counters cleared).
void func_088C37C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t q = word(rt, ctx.gpr[4] + 0x24u);
    constexpr std::uint32_t kThousand = 0x447A0000u;  // 1000.0f
    rt.memory().store8(q + 0xA1u, 0xFFu);
    rt.memory().store32(q + 0x9Cu, kThousand);
    rt.memory().store32(q + 0x44u, 0u);
    rt.memory().store16(q + 0x48u, 0u);
    rt.memory().store8(q + 0x4Au, 0u);
    rt.memory().store8(q + 0x4Bu, 0u);
    rt.memory().store8(q + 0x50u, 0u);
    rt.memory().store8(q + 0x51u, 0xFFu);
    rt.memory().store32(q + 0x58u, kThousand);
    rt.memory().store32(q + 0x5Cu, kThousand);
    rt.memory().store32(q + 0x60u, kThousand);
    rt.memory().store32(q + 0x98u, kThousand);
    rt.memory().store8(q + 0xA0u, 0xFFu);
    ctx.gpr[2] = q;
}

// func_088FB000(pair) -> the float at +0x1C of pair->second, else of pair->first, else 0.0.
void func_088FB000(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t second = word(rt, ctx.gpr[4] + 0x4u);
    const std::uint32_t first = word(rt, ctx.gpr[4]);
    float result = 0.0f;
    if (second != 0u) result = float_at(rt, second + 0x1Cu);
    else if (first != 0u) result = float_at(rt, first + 0x1Cu);
    ctx.fpr[0] = result;
}

// func_088E7F1C(this): when the state at +0x2C4 isn't 3, moves it to +4 and sets it to 3 (state 1
// also sets the byte at +0x19).
void func_088E7F1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t state = word(rt, self + 0x2C4u);
    ctx.gpr[2] = 3u;
    if (state == 3u) return;
    rt.memory().store32(self + 0x2C4u, 3u);
    rt.memory().store32(self + 0x4u, state);
    ctx.gpr[2] = 1u;
    if (state == 1u) rt.memory().store8(self + 0x19u, static_cast<std::uint8_t>(state));
}

// func_088A94C8(this) -> 1 when this->ptr10 exists and func_088A5984 of it is 1.
void func_088A94C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t p = word(rt, ctx.gpr[4] + 0x10u);
    std::uint32_t result = 0u;
    if (p != 0u) {
        GuestFrame frame(ctx, 0x10u);
        result = call_guest(rt, ctx, kKindCheck, {p}) == 1u ? 1u : 0u;
    }
    ctx.gpr[2] = result;
}

// func_08B00DF0(element, list): unless the element's flag 4 (+0xE2) is set, walks the chain of
// 8-byte nodes from the element's first index (+0xEC; the nodes at list->+0x44, each with the next
// index at +6, negative to end) clearing bit 0 of every node.
void func_08B00DF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t element = ctx.gpr[4];
    const std::uint32_t nodes_at = ctx.gpr[5] + 0x44u;
    auto index = static_cast<std::int32_t>(static_cast<std::int16_t>(rt.memory().load16(element + 0xECu)));
    if (index < 0 || (rt.memory().load16(element + 0xE2u) & 4u) != 0u) return;
    std::uint32_t node = word(rt, nodes_at) + static_cast<std::uint32_t>(index) * 8u;
    while (node != 0u) {
        rt.memory().store16(node, static_cast<std::uint16_t>(rt.memory().load16(node) & ~1u));
        index = static_cast<std::int16_t>(rt.memory().load16(node + 0x6u));
        if (index < 0) return;
        node = word(rt, nodes_at) + static_cast<std::uint32_t>(index) * 8u;
    }
}

// func_0895F3BC(this): counts this->ptr8's byte at +0x67A down; at 0 it restarts it at 20, steps
// the index at +0x679 (0..4, wrapping), keeps the previous index at +0x678 and applies that entry
// (12-byte entries at +0x618) with func_0895F174.
void func_0895F3BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t q = word(rt, ctx.gpr[4] + 0x8u);
    const std::uint32_t countdown = (rt.memory().load8(q + 0x67Au) - 1u) & 0xFFu;
    rt.memory().store8(q + 0x67Au, static_cast<std::uint8_t>(countdown));
    if (countdown != 0u) return;
    const std::uint32_t index = rt.memory().load8(q + 0x679u);
    rt.memory().store8(q + 0x67Au, 0x14u);
    const std::uint32_t entry = index * 12u + q + 0x618u;
    const std::uint32_t next = (index + 1u) & 0xFFu;
    rt.memory().store8(q + 0x679u, static_cast<std::uint8_t>(next));
    if (next >= 5u) rt.memory().store8(q + 0x679u, 0u);
    rt.memory().store8(q + 0x678u, static_cast<std::uint8_t>(index));
    tail_call(ctx, kApplyEntry, {ctx.gpr[4], entry});
}

// func_089596D4(this): decrements the countdown byte at +0x78 of func_08959180(this), if not 0.
void func_089596D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t owner = call_guest(rt, ctx, kCounterOwner, {});
    const std::uint32_t count = rt.memory().load8(owner + 0x78u);
    if (count != 0u) rt.memory().store8(owner + 0x78u, static_cast<std::uint8_t>(count - 1u));
    ctx.gpr[2] = count;
}

// func_08957154(this): this->ptr8's flag at +0x737 = without flag +0x5F9: +0x5E0 > 0.9 and
// +0x534 <= 0.9; with it: +0x534 > 0.9 and the byte at +0x512 is 11.
void func_08957154(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t q = word(rt, ctx.gpr[4] + 0x8u);
    const float threshold = std::bit_cast<float>(0x3F666666u);  // 0.9f
    std::uint32_t flag;
    if (rt.memory().load8(q + 0x5F9u) == 0u) {
        flag = (float_at(rt, q + 0x5E0u) <= threshold || threshold < float_at(rt, q + 0x534u)) ? 0u : 1u;
    } else {
        flag = (float_at(rt, q + 0x534u) <= threshold || rt.memory().load8(q + 0x512u) != 11u) ? 0u : 1u;
    }
    rt.memory().store8(q + 0x737u, static_cast<std::uint8_t>(flag));
    ctx.gpr[2] = flag;
}

// func_0895711C(this, int *out) -> with that flag set (signed byte 1), the float at +0x1478 of
// this->ptrC, else 0.0; *out = 0.
void func_0895711C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t q = word(rt, self + 0x8u);
    rt.memory().store32(ctx.gpr[5], 0u);
    float result = 0.0f;
    if (static_cast<std::int8_t>(rt.memory().load8(q + 0x737u)) == 1)
        result = float_at(rt, word(rt, self + 0xCu) + 0x1478u);
    ctx.fpr[0] = result;
}

// func_089551E0(this, vec3 *out): *out = {v.x, v.z, -v.y} of this->ptr14's vector at +0x14.
void func_089551E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t v = word(rt, ctx.gpr[4] + 0x14u) + 0x14u;
    const std::uint32_t out = ctx.gpr[5];
    rt.memory().store32(out, word(rt, v));
    rt.memory().store32(out + 0x4u, word(rt, v + 0x8u));
    rt.memory().store32(out + 0x8u, std::bit_cast<std::uint32_t>(-float_at(rt, v + 0x4u)));
    ctx.gpr[2] = v;
}

// func_08948AE4(record): clears its 32 bytes of flags and counters.
void func_08948AE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t r = ctx.gpr[4];
    for (const std::uint32_t at : {0x16u, 0x0u, 0x1Fu, 0x1u, 0x2u, 0x3u}) rt.memory().store8(r + at, 0u);
    rt.memory().store16(r + 0x4u, 0u);
    rt.memory().store8(r + 0x6u, 0u);
    rt.memory().store8(r + 0x7u, 0u);
    for (const std::uint32_t at : {0x8u, 0xAu, 0xCu, 0xEu, 0x10u}) rt.memory().store16(r + at, 0u);
    for (std::uint32_t at = 0x12u; at <= 0x1Eu; ++at) {
        if (at != 0x16u) rt.memory().store8(r + at, 0u);
    }
    ctx.gpr[2] = r + 0x16u;
}

// func_08948184(this): when func_08948170(this) holds, clears this->ptr8's byte at +0x77D.
void func_08948184(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    if (call_guest(rt, ctx, kIsActive, {self}) != 0u) {
        const std::uint32_t q = word(rt, self + 0x8u);
        rt.memory().store8(q + 0x77Du, 0u);
        ctx.gpr[2] = q;
    }
}

// func_08944588(this) -> 1 when this->ptr8's signed byte at +3 is 2 or its byte at +0xE is set,
// else whether func_089444F8(this, 4, 0, 0) is positive.
void func_08944588(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t q = word(rt, self + 0x8u);
    std::uint32_t result = 1u;
    if (static_cast<std::int8_t>(rt.memory().load8(q + 0x3u)) != 2 && rt.memory().load8(q + 0xEu) == 0u) {
        GuestFrame frame(ctx, 0x10u);
        result = static_cast<std::int32_t>(call_guest(rt, ctx, kCountMatching, {self, 4u, 0u, 0u})) > 0 ? 1u : 0u;
    }
    ctx.gpr[2] = result;
}

} // namespace gt::game

GT_DECOMP(0x0894D018u, gt::game::func_0894D018, gt::ReturnKind::Word);
GT_DECOMP(0x08949AF4u, gt::game::func_08949AF4, gt::ReturnKind::Word);
GT_DECOMP(0x088E7FA4u, gt::game::func_088E7FA4, gt::ReturnKind::Void);
GT_DECOMP(0x088C37C0u, gt::game::func_088C37C0, gt::ReturnKind::Void);
GT_DECOMP(0x088FB000u, gt::game::func_088FB000, gt::ReturnKind::Float);
GT_DECOMP(0x088E7F1Cu, gt::game::func_088E7F1C, gt::ReturnKind::Void);
GT_DECOMP(0x088A94C8u, gt::game::func_088A94C8, gt::ReturnKind::Word);
GT_DECOMP(0x08B00DF0u, gt::game::func_08B00DF0, gt::ReturnKind::Void);
GT_DECOMP(0x0895F3BCu, gt::game::func_0895F3BC, gt::ReturnKind::Void);
GT_DECOMP(0x089596D4u, gt::game::func_089596D4, gt::ReturnKind::Void);
GT_DECOMP(0x08957154u, gt::game::func_08957154, gt::ReturnKind::Void);
GT_DECOMP(0x0895711Cu, gt::game::func_0895711C, gt::ReturnKind::Float);
GT_DECOMP(0x089551E0u, gt::game::func_089551E0, gt::ReturnKind::Void);
GT_DECOMP(0x08948AE4u, gt::game::func_08948AE4, gt::ReturnKind::Void);
GT_DECOMP(0x08948184u, gt::game::func_08948184, gt::ReturnKind::Void);
GT_DECOMP(0x08944588u, gt::game::func_08944588, gt::ReturnKind::Word);

// --- batch 10 ---

namespace gt::game {
namespace {

constexpr std::uint32_t kPartnerUpdate = 0x088B75D0u;    // (a, b)
constexpr std::uint32_t kStateReset = 0x088C36E4u;       // (state)
constexpr std::uint32_t kStateRelease = 0x088B5D9Cu;     // (object)
constexpr std::uint32_t kHasSpecial = 0x088A4218u;       // (this) -> bool
constexpr std::uint32_t kFlagAtF = 0x08955380u;          // func_08955380 (owner) -> bool
constexpr std::uint32_t kPickWord = 0x088F8D58u;         // func_088F8D58 (this, i) -> word
constexpr std::uint32_t kInit20b = 0x08896FB8u;          // (member at +0x20)
constexpr std::uint32_t kCopyBytes = 0x08B7C830u;        // (dst, src, n)
constexpr std::uint32_t kBitOf = 0x088D1F88u;            // func_088D1F88 below
constexpr std::uint32_t kSlotState = 0x088E85C4u;        // (slot) -> state
constexpr std::uint32_t kSwapped = 0x0896D850u;          // (b, a)
constexpr std::uint32_t kHolderFrom = 0x08A6972Cu;       // (holder *out, const holder *)
constexpr std::uint32_t kLinkLock = 0x08CF4950u;         // mutex guarding func_089A3130
constexpr std::uint32_t kWorldReady = 0x0895B59Cu;       // (this) -> bool
constexpr std::uint32_t kWorldFlag = 0x08CF35A0u;        // a global flag word

} // namespace

// func_088B4740(this): unless this->ptrC's signed byte at +3 is 4, func_088B75D0(this->+0x2C, this->+0x30).
void func_088B4740(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    if (static_cast<std::int8_t>(rt.memory().load8(word(rt, self + 0xCu) + 0x3u)) == 4) return;
    tail_call(ctx, kPartnerUpdate, {word(rt, self + 0x2Cu), word(rt, self + 0x30u)});
}

// func_088B45BC(this): unless that byte is 4, resets the state at +4 and releases this->+0x2C.
void func_088B45BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    if (static_cast<std::int8_t>(rt.memory().load8(word(rt, self + 0xCu) + 0x3u)) == 4) return;
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, kStateReset, {self + 0x4u});
    }
    tail_call(ctx, kStateRelease, {word(rt, self + 0x2Cu)});
}

// func_088AA6D8(holder) -> the word at +0x10 of the object's +0x4C member, or 0.
void func_088AA6D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t member = word(rt, word(rt, ctx.gpr[4]) + 0x4Cu);
    ctx.gpr[2] = member != 0u ? word(rt, member + 0x10u) : 0u;
}

// func_088A4264(this) -> this->+0x1E4 + 0x400 when func_088A4218(this) holds, else 0.
void func_088A4264(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, kHasSpecial, {self}) != 0u ? word(rt, self + 0x1E4u) + 0x400u : 0u;
}

// func_088A3AF0(this) -> func_08955380(this->ptr18), or 1 without ptr18.
void func_088A3AF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t owner = word(rt, ctx.gpr[4] + 0x18u);
    std::uint32_t result = 1u;
    if (owner != 0u) {
        GuestFrame frame(ctx, 0x10u);
        result = call_guest(rt, ctx, kFlagAtF, {owner});
    }
    ctx.gpr[2] = result;
}

// func_088993E4(this, i) -> the word at +0x10 of func_088F8D58(this->ptrC, i), or 0.
void func_088993E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t picked = call_guest(rt, ctx, kPickWord, {word(rt, ctx.gpr[4] + 0xCu)});
    ctx.gpr[2] = picked != 0u ? word(rt, picked + 0x10u) : 0u;
}

// func_088993A0(this, i) -> 1 when this->ptrC exists and func_088F8D58(ptrC, i)'s byte at +9 is set.
void func_088993A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t owner = word(rt, ctx.gpr[4] + 0xCu);
    std::uint32_t result = 0u;
    if (owner != 0u) {
        GuestFrame frame(ctx, 0x10u);
        const std::uint32_t picked = call_guest(rt, ctx, kPickWord, {owner});
        if (picked != 0u && rt.memory().load8(picked + 0x9u) != 0u) result = 1u;
    }
    ctx.gpr[2] = result;
}

// func_088982A8(this): func_08896FB8(&this->member20).
void func_088982A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, kInit20b, {ctx.gpr[4] + 0x20u});
}

// func_08892BE0(this, i, src) / func_08892BF8: copies 0x80 bytes into the name slot i at +0x588 /
// 4 bytes into the word i at +0x578.
void func_08892BE0(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    tail_call(ctx, kCopyBytes, {(ctx.gpr[5] << 7) + ctx.gpr[4] + 0x588u, ctx.gpr[6], 0x80u});
}
void func_08892BF8(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    tail_call(ctx, kCopyBytes, {(ctx.gpr[5] << 2) + ctx.gpr[4] + 0x578u, ctx.gpr[6], 0x4u});
}

// func_088DC858(this, i) -> slot i's word at +0x1C, or 0 out of range.
void func_088DC858(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const auto i = static_cast<std::int32_t>(ctx.gpr[5]);
    std::uint32_t value = 0u;
    if (i >= 0) {
        GuestFrame frame(ctx, 0x10u);
        if (i < static_cast<std::int32_t>(call_guest(rt, ctx, kSlotCount2, {})))
            value = word(rt, slot(rt, self, static_cast<std::uint32_t>(i)) + 0x1Cu);
    }
    ctx.gpr[2] = value;
}

// func_088DA4DC(this, i) -> func_088E85C4(&slot[i]), or 9 out of range.
void func_088DA4DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const auto i = static_cast<std::int32_t>(ctx.gpr[5]);
    if (i >= 0) {
        std::int32_t count;
        {
            GuestFrame frame(ctx, 0x10u);
            count = static_cast<std::int32_t>(call_guest(rt, ctx, kSlotCount2, {}));
        }
        if (i < count) {
            tail_call(ctx, kSlotState, {slot(rt, self, static_cast<std::uint32_t>(i))});
            return;
        }
    }
    ctx.gpr[2] = 9u;
}

// func_088D0924(this, value) -> the old value of +0x40, which moves to +0x44 (+0x40 = value).
void func_088D0924(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t old = word(rt, ctx.gpr[4] + 0x40u);
    rt.memory().store32(ctx.gpr[4] + 0x40u, ctx.gpr[5]);
    rt.memory().store32(ctx.gpr[4] + 0x44u, old);
    ctx.gpr[2] = old;
}

// func_088F8D0C(this, i, j) -> the word at this->+4 + (2i + j) * 4, or 0 when i is 0..3 and bit i
// of the mask at +0x2C is set.
void func_088F8D0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const auto i = static_cast<std::int32_t>(ctx.gpr[5]);
    if (i >= 0 && i < 4 &&
        ((word(rt, self + 0x2Cu + static_cast<std::uint32_t>(i >> 5) * 4u) >> (i & 31)) & 1u) != 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    ctx.gpr[2] = word(rt, (static_cast<std::uint32_t>(i) * 2u + ctx.gpr[6]) * 4u + self + 0x4u);
}

// func_088D1F88(this, i) -> bit i of the mask at +8 for i 0..3, else 0.
void func_088D1F88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const auto i = static_cast<std::int32_t>(ctx.gpr[5]);
    ctx.gpr[2] = (i >= 0 && i < 4) ? (word(rt, ctx.gpr[4] + 0x8u + static_cast<std::uint32_t>(i >> 5) * 4u) >> (i & 31)) & 1u : 0u;
}

// func_088DB2C8(this, i) -> func_088D1F88(this->+0x618, i).
void func_088DB2C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, kBitOf, {word(rt, ctx.gpr[4] + 0x618u)});
}

// func_089A8978(a, b): func_0896D850(b, a).
void func_089A8978(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = ctx.gpr[4];
    ctx.gpr[2] = a;
    tail_call(ctx, kSwapped, {ctx.gpr[5], a});
}

// func_08A759E0(holder *out, holder **array, i) -> out, after func_08A6972C(out, &(*array)[i]).
void func_08A759E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    (void)call_guest(rt, ctx, kHolderFrom, {out, word(rt, ctx.gpr[5]) + (ctx.gpr[6] << 2)});
    ctx.gpr[2] = out;
}

// func_089A3130(this) -> this->ptr144 when it and its +4 word are set, else NULL; under a lock.
void func_089A3130(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    (void)call_guest(rt, ctx, kHeapLock, {kLinkLock});
    const std::uint32_t link = word(rt, self + 0x144u);
    const std::uint32_t result = (link != 0u && word(rt, link + 0x4u) != 0u) ? link : 0u;
    (void)call_guest(rt, ctx, kHeapUnlock, {kLinkLock});
    ctx.gpr[2] = result;
}

// func_089BAB0C(this) -> this->ptr3C when it and its +4 word are set, else this.
void func_089BAB0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t link = word(rt, self + 0x3Cu);
    std::uint32_t result = self;
    if (link != 0u && word(rt, link + 0x4u) != 0u) result = word(rt, link + 0x4u) != 0u ? link : 0u;
    ctx.gpr[2] = result;
}

// isInstanceOf(object, class) -> whether the object's class (its virtual at +0x24) is `class` or
// derives from it: the chain of parents at +0x38, each with a non-zero +4.
// Name evidence: as_class.cpp / try_cast.cpp call it before every cast (kIsInstanceOf).
void isInstanceOf(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4];
    const std::uint32_t cls = ctx.gpr[5];
    std::uint32_t current;
    {
        GuestFrame frame(ctx, 0x10u);
        current = call_guest(rt, ctx, word(rt, word(rt, object) + 0x24u), {object});
    }
    std::uint32_t result = 0u;
    if (current != 0u) {
        if (current == cls) {
            result = 1u;
        } else {
            for (std::uint32_t parent = word(rt, current + 0x38u); parent != 0u; parent = word(rt, parent + 0x38u)) {
                if (word(rt, parent + 0x4u) == 0u) break;
                if (parent == cls) {
                    result = 1u;
                    break;
                }
            }
        }
    }
    ctx.gpr[2] = result;
}

// func_088BB5A0(this) -> the next value of the 15-bit random generator at +0x6A
// (x = x * 0x41C64E6D + 0x3039, bits 16..30 kept — the ANSI C rand constants).
void func_088BB5A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t seed = rt.memory().load16(ctx.gpr[4] + 0x6Au);
    const std::uint32_t next = ((seed * 0x41C64E6Du + 0x3039u) >> 16) & 0x7FFFu;
    rt.memory().store16(ctx.gpr[4] + 0x6Au, static_cast<std::uint16_t>(next));
    ctx.gpr[2] = next;
}

// func_0895BB08(this) -> 1 when func_0895B59C(this) holds, the global flag at 0x08CF35A0 is set and
// this->ptr8's word at +0x778 is set.
void func_0895BB08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    std::uint32_t result = 0u;
    if (call_guest(rt, ctx, kWorldReady, {self}) != 0u && word(rt, kWorldFlag) != 0u &&
        word(rt, word(rt, self + 0x8u) + 0x778u) != 0u) {
        result = 1u;
    }
    ctx.gpr[2] = result;
}

} // namespace gt::game

GT_DECOMP(0x088B4740u, gt::game::func_088B4740, gt::ReturnKind::Void);
GT_DECOMP(0x088B45BCu, gt::game::func_088B45BC, gt::ReturnKind::Void);
GT_DECOMP(0x088AA6D8u, gt::game::func_088AA6D8, gt::ReturnKind::Word);
GT_DECOMP(0x088A4264u, gt::game::func_088A4264, gt::ReturnKind::Word);
GT_DECOMP(0x088A3AF0u, gt::game::func_088A3AF0, gt::ReturnKind::Word);
GT_DECOMP(0x088993E4u, gt::game::func_088993E4, gt::ReturnKind::Word);
GT_DECOMP(0x088993A0u, gt::game::func_088993A0, gt::ReturnKind::Word);
GT_DECOMP(0x088982A8u, gt::game::func_088982A8, gt::ReturnKind::Void);
GT_DECOMP(0x08892BE0u, gt::game::func_08892BE0, gt::ReturnKind::Word);
GT_DECOMP(0x08892BF8u, gt::game::func_08892BF8, gt::ReturnKind::Word);
GT_DECOMP(0x088DC858u, gt::game::func_088DC858, gt::ReturnKind::Word);
GT_DECOMP(0x088DA4DCu, gt::game::func_088DA4DC, gt::ReturnKind::Word);
GT_DECOMP(0x088D0924u, gt::game::func_088D0924, gt::ReturnKind::Word);
GT_DECOMP(0x088F8D0Cu, gt::game::func_088F8D0C, gt::ReturnKind::Word);
GT_DECOMP(0x088D1F88u, gt::game::func_088D1F88, gt::ReturnKind::Word);
GT_DECOMP(0x088DB2C8u, gt::game::func_088DB2C8, gt::ReturnKind::Word);
GT_DECOMP(0x089A8978u, gt::game::func_089A8978, gt::ReturnKind::Word);
GT_DECOMP(0x08A759E0u, gt::game::func_08A759E0, gt::ReturnKind::Word);
GT_DECOMP(0x089A3130u, gt::game::func_089A3130, gt::ReturnKind::Word);
GT_DECOMP(0x089BAB0Cu, gt::game::func_089BAB0C, gt::ReturnKind::Word);
GT_DECOMP(0x08A76F20u, gt::game::isInstanceOf, gt::ReturnKind::Word);
GT_DECOMP(0x088BB5A0u, gt::game::func_088BB5A0, gt::ReturnKind::Word);
GT_DECOMP(0x0895BB08u, gt::game::func_0895BB08, gt::ReturnKind::Word);

// --- batch 11 ---

namespace gt::game {
namespace {

constexpr std::uint32_t kRawPercent = 0x088C49FCu;       // () -> value
constexpr std::uint32_t kPresenceTable = 0x08CDD20Cu;    // word[]
constexpr std::uint32_t kMemberOf = 0x088AA6D8u;         // func_088AA6D8 above
constexpr std::uint32_t kIsTracked = 0x08966168u;        // (this) -> bool
constexpr std::uint32_t kIsVisible = 0x08965CE8u;        // (a, b) -> bool
constexpr std::uint32_t kTrackedFlags = 0x08968D94u;     // func_08968D94 below
constexpr std::uint32_t kAttachLock = 0x08CFF450u;       // mutex guarding func_08A7BE18
constexpr std::uint32_t kRetainRef = 0x08A5BA08u;        // (object)
constexpr std::uint32_t kReleaseRef = 0x08A5B930u;       // (object)
constexpr std::uint32_t kFindActive2 = 0x08AF27B4u;      // () -> object or NULL
constexpr std::uint32_t kActiveValue2 = 0x08AF1298u;     // (object) -> word
constexpr std::uint32_t kGuSetup = 0x08B56790u;          // (0, value, 0, 0)
constexpr std::uint32_t kAllocate = 0x08A5BC14u;         // (size) -> memory
constexpr std::uint32_t kConstruct18 = 0x08A670C0u;      // (object, arg)

} // namespace

// func_088C4A08() -> func_088C49FC(), at most 100 (unsigned).
void func_088C4A08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t value = call_guest(rt, ctx, kRawPercent, {});
    ctx.gpr[2] = value < 101u ? value : 100u;
}

// func_088CB430(-, i) -> whether the word i of the table at 0x08CDD20C is set.
void func_088CB430(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = word(rt, (ctx.gpr[5] << 2) + kPresenceTable) != 0u ? 1u : 0u;
}

// func_089AA4E8 / func_089AA5A0(this) -> this->ptr50 / ptr44 when it and its +4 word are set, else
// NULL; under the lock at 0x08CF4ACC.
void linked_under_lock(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t offset) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    (void)call_guest(rt, ctx, kHeapLock, {kStateLock});
    const std::uint32_t link = word(rt, self + offset);
    const std::uint32_t result = (link != 0u && word(rt, link + 0x4u) != 0u) ? link : 0u;
    (void)call_guest(rt, ctx, kHeapUnlock, {kStateLock});
    ctx.gpr[2] = result;
}
void func_089AA4E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { linked_under_lock(rt, ctx, 0x50u); }
void func_089AA5A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { linked_under_lock(rt, ctx, 0x44u); }

// func_08A72708(stack) -> &items[--count] (items at +0, count at +0x1C).
void func_08A72708(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t count = word(rt, ctx.gpr[4] + 0x1Cu) - 1u;
    rt.memory().store32(ctx.gpr[4] + 0x1Cu, count);
    ctx.gpr[2] = word(rt, ctx.gpr[4]) + (count << 2);
}

// func_08A72768(stack, i) -> &items[base + i] (base at +0xC).
void func_08A72768(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = word(rt, ctx.gpr[4]) + ((word(rt, ctx.gpr[4] + 0xCu) + ctx.gpr[5]) << 2);
}

// func_089552C8 / func_089552B4(this) -> this->ptrC's float +4 minus / float +0 plus its float +0x70.
void func_089552C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t q = word(rt, ctx.gpr[4] + 0xCu);
    ctx.gpr[2] = q;
    ctx.fpr[0] = float_at(rt, q + 0x4u) - float_at(rt, q + 0x70u);
}
void func_089552B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t q = word(rt, ctx.gpr[4] + 0xCu);
    ctx.gpr[2] = q;
    ctx.fpr[0] = float_at(rt, q) + float_at(rt, q + 0x70u);
}

// func_08893788(holder) -> &object[object->signed byte 1] + 0xFD8 (records of 0x26D0 bytes; a madd,
// so HI changes too).
void func_08893788(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4]);
    const auto index = static_cast<std::int32_t>(static_cast<std::int8_t>(rt.memory().load8(object + 0x1u)));
    const std::uint64_t accumulator = (static_cast<std::uint64_t>(ctx.hi) << 32) | object;
    const std::uint64_t sum = accumulator + static_cast<std::uint64_t>(static_cast<std::int64_t>(index) * 0x26D0);
    ctx.lo = static_cast<std::uint32_t>(sum);
    ctx.hi = static_cast<std::uint32_t>(sum >> 32);
    ctx.gpr[2] = ctx.lo + 0xFD8u;
}

// func_08948170(this) -> whether this->ptrC's byte at +0x270 is 7.
void func_08948170(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = rt.memory().load8(word(rt, ctx.gpr[4] + 0xCu) + 0x270u) == 7u ? 1u : 0u;
}

// func_088AA15C(this) -> the word at +0x38 of func_088AA6D8(&this->ptr8->member270), or 0.
void func_088AA15C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t member = call_guest(rt, ctx, kMemberOf, {word(rt, ctx.gpr[4] + 0x8u) + 0x270u});
    ctx.gpr[2] = member != 0u ? word(rt, member + 0x38u) : 0u;
}

// func_088C3808(a, b) -> |(b.x - a.x) * a.dx + (b.y - a.y) * a.dy|: b's distance along a's direction
// (positions at +0x10/+0x14 and direction at +0x1C/+0x20 of each ptr8).
void func_088C3808(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = word(rt, ctx.gpr[4] + 0x8u);
    const std::uint32_t b = word(rt, ctx.gpr[5] + 0x8u);
    float dx = float_at(rt, b + 0x10u) - float_at(rt, a + 0x10u);
    float dy = float_at(rt, b + 0x14u) - float_at(rt, a + 0x14u);
    dy = dy * float_at(rt, a + 0x20u);
    dx = dx * float_at(rt, a + 0x1Cu);
    float along = dx + dy;
    if (along < 0.0f) along = -along;
    ctx.fpr[0] = along;
}

// func_089DAF74 / func_089DAD9C(this, i, x, y): point i of the +0x104 / +0xEC vector = (x, y).
void store_point(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t bit, std::uint32_t base) {
    const std::uint32_t point = point_data(rt, ctx.gpr[4], bit, base) + (ctx.gpr[5] << 3);
    rt.memory().store32(point + 0x4u, std::bit_cast<std::uint32_t>(ctx.fpr[13]));
    rt.memory().store32(point, std::bit_cast<std::uint32_t>(ctx.fpr[12]));
    ctx.gpr[2] = point;
}
void func_089DAF74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_point(rt, ctx, 8u, 0x104u); }
void func_089DAD9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_point(rt, ctx, 7u, 0xECu); }

// func_08909A38(this) -> the address after the u16-counted array at this->ptr10 (count, then count
// halves), or NULL; func_08909A28(this) -> the array's first element (ptr10 + 2), or NULL.
void func_08909A38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t p = word(rt, ctx.gpr[4] + 0x10u);
    ctx.gpr[2] = p != 0u ? p + (rt.memory().load16(p) << 1) + 0x2u : 0u;
}
void func_08909A28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t p = word(rt, ctx.gpr[4] + 0x10u);
    ctx.gpr[2] = p != 0u ? p + 0x2u : 0u;
}

// func_088D4E84(holder, side) -> side 1: the byte at +0x72 / 100; otherwise the byte at +0x6E / -100.
void func_088D4E84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4]);
    if (ctx.gpr[5] == 1u) {
        ctx.fpr[0] = static_cast<float>(static_cast<std::int32_t>(rt.memory().load8(object + 0x72u))) / 100.0f;
    } else {
        ctx.fpr[0] = static_cast<float>(static_cast<std::int32_t>(rt.memory().load8(object + 0x6Eu))) / -100.0f;
    }
    ctx.gpr[2] = object;
}

// func_088D4F64(holder, side) -> 10 × the byte at +0x74 (side 1) or +0x70.
void func_088D4F64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4]);
    const std::uint32_t at = ctx.gpr[5] == 1u ? 0x74u : 0x70u;
    ctx.fpr[0] = static_cast<float>(static_cast<std::int32_t>(rt.memory().load8(object + at))) * 10.0f;
    ctx.gpr[2] = object;
}

// func_088B5DA4(this) -> 1 for state 2, -1 for state 3, 0 otherwise (the state at +0x18C).
void func_088B5DA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t state = word(rt, ctx.gpr[4] + 0x18Cu);
    ctx.gpr[2] = state == 2u ? 1u : state == 3u ? 0xFFFFFFFFu : 0u;
}

// func_08968D94(this) -> 1 when func_08966168(this) holds and flag 0x4000 or 0x80000 (+0xE0) is set.
void func_08968D94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    std::uint32_t result = 0u;
    if (call_guest(rt, ctx, kIsTracked, {self}) != 0u) {
        const std::uint32_t flags = word(rt, self + 0xE0u);
        if ((flags & 0x4000u) != 0u || (flags & 0x80000u) != 0u) result = 1u;
    }
    ctx.gpr[2] = result;
}

// func_0897D738(this): the 3x3 matrix at +0x14 (rows of 12 bytes) = identity.
void func_0897D738(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t m = ctx.gpr[4] + 0x14u;
    constexpr std::uint32_t kOne = 0x3F800000u;
    rt.memory().store32(m, kOne);
    rt.memory().store32(m + 0x20u, kOne);
    rt.memory().store32(m + 0xCu, 0u);
    rt.memory().store32(m + 0x18u, 0u);
    rt.memory().store32(m + 0x4u, 0u);
    rt.memory().store32(m + 0x10u, kOne);
    rt.memory().store32(m + 0x1Cu, 0u);
    rt.memory().store32(m + 0x8u, 0u);
    rt.memory().store32(m + 0x14u, 0u);
    ctx.gpr[2] = m;
}

// func_08A00448(a, b) -> 1 when func_08965CE8(a, b) and func_08968D94(b) both hold.
void func_08A00448(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t b = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    std::uint32_t result = 0u;
    if (call_guest(rt, ctx, kIsVisible, {}) != 0u && call_guest(rt, ctx, kTrackedFlags, {b}) != 0u) result = 1u;
    ctx.gpr[2] = result;
}

// func_08A7BE18(this, object): this->+0x1C = object, retaining the new one and releasing the old,
// under the lock at 0x08CFF450.
void func_08A7BE18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t object = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, kHeapLock, {kAttachLock});
        if (object != 0u) (void)call_guest(rt, ctx, kRetainRef, {object});
        const std::uint32_t old = word(rt, self + 0x1Cu);
        if (old != 0u) (void)call_guest(rt, ctx, kReleaseRef, {old});
    }
    rt.memory().store32(self + 0x1Cu, object);
    tail_call(ctx, kHeapUnlock, {kAttachLock});
}

// func_08AF27C0() -> func_08AF1298(func_08AF27B4()), or 0 when there is no object.
void func_08AF27C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t object;
    {
        GuestFrame frame(ctx, 0x10u);
        object = call_guest(rt, ctx, kFindActive2, {});
    }
    if (object != 0u) {
        tail_call(ctx, kActiveValue2, {object});
        return;
    }
    ctx.gpr[2] = 0u;
}

// func_08956530(this) -> whether this->ptr8's byte at +2 is set.
void func_08956530(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = rt.memory().load8(word(rt, ctx.gpr[4] + 0x8u) + 0x2u) != 0u ? 1u : 0u;
}

// func_08AF3FA0(value) -> value, after func_08B56790(0, value, 0, 0).
void func_08AF3FA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t value = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    (void)call_guest(rt, ctx, kGuSetup, {0u, value, 0u, 0u});
    ctx.gpr[2] = value;
}

// func_08A6710C(arg) -> a new 0x18-byte object constructed with arg (func_08A670C0).
void func_08A6710C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t arg = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t object = call_guest(rt, ctx, kAllocate, {0x18u});
    (void)call_guest(rt, ctx, kConstruct18, {object, arg});
    ctx.gpr[2] = object;
}

} // namespace gt::game

GT_DECOMP(0x088C4A08u, gt::game::func_088C4A08, gt::ReturnKind::Word);
GT_DECOMP(0x088CB430u, gt::game::func_088CB430, gt::ReturnKind::Word);
GT_DECOMP(0x089AA4E8u, gt::game::func_089AA4E8, gt::ReturnKind::Word);
GT_DECOMP(0x089AA5A0u, gt::game::func_089AA5A0, gt::ReturnKind::Word);
GT_DECOMP(0x08A72708u, gt::game::func_08A72708, gt::ReturnKind::Word);
GT_DECOMP(0x08A72768u, gt::game::func_08A72768, gt::ReturnKind::Word);
GT_DECOMP(0x089552C8u, gt::game::func_089552C8, gt::ReturnKind::Float);
GT_DECOMP(0x089552B4u, gt::game::func_089552B4, gt::ReturnKind::Float);
GT_DECOMP(0x08893788u, gt::game::func_08893788, gt::ReturnKind::Word);
GT_DECOMP(0x08948170u, gt::game::func_08948170, gt::ReturnKind::Word);
GT_DECOMP(0x088AA15Cu, gt::game::func_088AA15C, gt::ReturnKind::Word);
GT_DECOMP(0x088C3808u, gt::game::func_088C3808, gt::ReturnKind::Float);
GT_DECOMP(0x089DAF74u, gt::game::func_089DAF74, gt::ReturnKind::Void);
GT_DECOMP(0x089DAD9Cu, gt::game::func_089DAD9C, gt::ReturnKind::Void);
GT_DECOMP(0x08909A38u, gt::game::func_08909A38, gt::ReturnKind::Word);
GT_DECOMP(0x08909A28u, gt::game::func_08909A28, gt::ReturnKind::Word);
GT_DECOMP(0x088D4E84u, gt::game::func_088D4E84, gt::ReturnKind::Float);
GT_DECOMP(0x088D4F64u, gt::game::func_088D4F64, gt::ReturnKind::Float);
GT_DECOMP(0x088B5DA4u, gt::game::func_088B5DA4, gt::ReturnKind::Word);
GT_DECOMP(0x08968D94u, gt::game::func_08968D94, gt::ReturnKind::Word);
GT_DECOMP(0x0897D738u, gt::game::func_0897D738, gt::ReturnKind::Void);
GT_DECOMP(0x08A00448u, gt::game::func_08A00448, gt::ReturnKind::Word);
GT_DECOMP(0x08A7BE18u, gt::game::func_08A7BE18, gt::ReturnKind::Void);
GT_DECOMP(0x08AF27C0u, gt::game::func_08AF27C0, gt::ReturnKind::Word);
GT_DECOMP(0x08956530u, gt::game::func_08956530, gt::ReturnKind::Word);
GT_DECOMP(0x08AF3FA0u, gt::game::func_08AF3FA0, gt::ReturnKind::Word);
GT_DECOMP(0x08A6710Cu, gt::game::func_08A6710C, gt::ReturnKind::Word);

// --- batch 12 ---

namespace gt::game {
namespace {

constexpr std::uint32_t kOwnerLock = 0x08CF4994u;        // mutex guarding func_089A717C's read
constexpr std::uint32_t kCopyOut = 0x089A6A68u;          // (out, source)
constexpr std::uint32_t kNotifyDisabled = 0x08CFFB70u;   // a global flag byte: notifications off
constexpr std::uint32_t kForEachLocked = 0x08ABFF20u;    // (list, callback)
constexpr std::uint32_t kForEachLocked2 = 0x08ABFFA0u;   // (list, callback)
constexpr std::uint32_t kQueuePost = 0x08B1C820u;        // (queue, item, -1, -1)
constexpr std::uint32_t kSwapGlobal = 0x08D405D8u;       // a global word swapped by func_08AD9784
constexpr std::uint32_t kSetMode = 0x08AD95ECu;          // (mode)
constexpr std::uint32_t kListHeadA = 0x08CFF9C0u, kListLockA = 0x08CFF9C8u;
constexpr std::uint32_t kListHeadB = 0x08CFF98Cu, kListLockB = 0x08CFF994u;
constexpr std::uint32_t kListHeadC = 0x08D4059Cu, kListLockC = 0x08D405A4u;
constexpr std::uint32_t kListHeadD = 0x08D77188u;
constexpr std::uint32_t kListHeadE = 0x08D7717Cu;
constexpr std::uint32_t kDetach = 0x089A7414u;           // (target, this)
constexpr std::uint32_t kOwnerOf = 0x089A717Cu;          // func_089A717C below
constexpr std::uint32_t kAttachTo = 0x089A89A0u;         // (b, owner)
constexpr std::uint32_t kClock = 0x08B1C490u;            // func_08B1C490 below
constexpr std::uint32_t kClockValue = 0x08D68A80u;       // u64

void for_each_callback(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t head, std::uint32_t lock) {
    GuestFrame frame(ctx, 0x10u);
    (void)call_guest(rt, ctx, kHeapLock, {lock});
    for (std::uint32_t node = word(rt, head); node != 0u;) {
        const std::uint32_t entry = word(rt, node);
        const std::uint32_t next = word(rt, node + 0x8u);
        const std::uint32_t function = word(rt, entry);
        if (function != 0u) (void)call_guest(rt, ctx, function, {word(rt, entry + 0x4u)});
        node = next;
    }
}

} // namespace

// func_089A717C(this) -> the word at +0x14, read under the lock at 0x08CF4994.
void func_089A717C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    (void)call_guest(rt, ctx, kHeapLock, {kOwnerLock});
    const std::uint32_t value = word(rt, self + 0x14u);
    (void)call_guest(rt, ctx, kHeapUnlock, {kOwnerLock});
    ctx.gpr[2] = value;
}

// func_08A7BF7C(this) -> the word at +0x34, read under the lock at 0x08CFF450.
void func_08A7BF7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    (void)call_guest(rt, ctx, kHeapLock, {kAttachLock});
    const std::uint32_t value = word(rt, self + 0x34u);
    (void)call_guest(rt, ctx, kHeapUnlock, {kAttachLock});
    ctx.gpr[2] = value;
}

// func_089AA658(out, source) -> out, after func_089A6A68(out, &source->member68) under the lock at 0x08CF4ACC.
void func_089AA658(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t source = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    (void)call_guest(rt, ctx, kHeapLock, {kStateLock});
    (void)call_guest(rt, ctx, kCopyOut, {out, source + 0x68u});
    (void)call_guest(rt, ctx, kHeapUnlock, {kStateLock});
    ctx.gpr[2] = out;
}

// func_08ABF9B4 / ..990 / ..96C / ..948 / ..924(list): unless notifications are off (0x08CFFB70),
// runs the list with one of five callbacks (through func_08ABFF20 or func_08ABFFA0).
void notify(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t walk, std::uint32_t callback) {
    if (rt.memory().load8(kNotifyDisabled) != 0u) return;
    tail_call(ctx, walk, {ctx.gpr[4], callback});
}
void func_08ABF9B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { notify(rt, ctx, kForEachLocked, 0x08ABFC18u); }
void func_08ABF990(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { notify(rt, ctx, kForEachLocked, 0x08ABFC08u); }
void func_08ABF96C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { notify(rt, ctx, kForEachLocked2, 0x08ABFBF8u); }
void func_08ABF948(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { notify(rt, ctx, kForEachLocked2, 0x08ABFBE8u); }
void func_08ABF924(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { notify(rt, ctx, kForEachLocked2, 0x08ABFBD8u); }

// func_08B1C468 / func_08B1C3F8 / func_08B1C374(-, item): posts the item to one of three global
// queues (func_08B1C820(queue, item, -1, -1)).
void func_08B1C468(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, kQueuePost, {0x08D68A70u, ctx.gpr[5], 0xFFFFFFFFu, 0xFFFFFFFFu}); }
void func_08B1C3F8(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, kQueuePost, {0x08D68A50u, ctx.gpr[5], 0xFFFFFFFFu, 0xFFFFFFFFu}); }
void func_08B1C374(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, kQueuePost, {0x08D68A30u, ctx.gpr[5], 0xFFFFFFFFu, 0xFFFFFFFFu}); }

// func_08AF4F24(this, i): the flag byte i at +0x424 = 1.
void func_08AF4F24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store8(ctx.gpr[5] + ctx.gpr[4] + 0x424u, 1u);
    ctx.gpr[2] = 1u;
}

// func_08AD9784(value) -> the previous value of the global at 0x08D405D8, which becomes `value`.
void func_08AD9784(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t old = word(rt, kSwapGlobal);
    rt.memory().store32(kSwapGlobal, ctx.gpr[4]);
    ctx.gpr[2] = old;
}

// func_08AD96A0(): func_08AD95EC(0).
void func_08AD96A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, kSetMode, {0u});
}

// func_08AB9C48 / func_08AB9A9C(): run the callbacks of two global lists ({function, argument}
// entries, next at +8), each under its lock.
void func_08AB9C48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    for_each_callback(rt, ctx, kListHeadA, kListLockA);
    tail_call(ctx, kHeapUnlock, {kListLockA});
}
void func_08AB9A9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    for_each_callback(rt, ctx, kListHeadB, kListLockB);
    tail_call(ctx, kHeapUnlock, {kListLockB});
}

// func_08AC8D20(): calls the virtual at +0x14 of every object in a global list (object at +0, next
// at +8), under its lock.
void func_08AC8D20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, kHeapLock, {kListLockC});
        for (std::uint32_t node = word(rt, kListHeadC); node != 0u; node = word(rt, node + 0x8u)) {
            const std::uint32_t object = word(rt, node);
            (void)call_guest(rt, ctx, word(rt, word(rt, object) + 0x14u), {object});
        }
    }
    tail_call(ctx, kHeapUnlock, {kListLockC});
}

// func_08ABBC64(): calls the virtual at +0x14 of every object in a global list (next at +0, object
// at +4).
void func_08ABBC64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    for (std::uint32_t node = word(rt, kListHeadD); node != 0u; node = word(rt, node)) {
        const std::uint32_t object = word(rt, node + 0x4u);
        (void)call_guest(rt, ctx, word(rt, word(rt, object) + 0x14u), {object});
    }
}

// func_08AB978C(value in $f12): calls the virtual at +0x20 of every object in another global list with
// `value`.
void func_08AB978C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const float value = ctx.fpr[12];
    GuestFrame frame(ctx, 0x20u);
    for (std::uint32_t node = word(rt, kListHeadE); node != 0u; node = word(rt, node)) {
        const std::uint32_t object = word(rt, node + 0x4u);
        (void)call_guest(rt, ctx, word(rt, word(rt, object) + 0x20u), {object}, {value});
    }
}

// func_089AB274(this): func_089A7414(this->ptr68, this) if set, under the lock at 0x08CF4ACC.
void func_089AB274(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, kHeapLock, {kStateLock});
        const std::uint32_t target = word(rt, self + 0x68u);
        if (target != 0u) (void)call_guest(rt, ctx, kDetach, {target, self});
    }
    tail_call(ctx, kHeapUnlock, {kStateLock});
}

// func_089A73B8(this, b): func_089A89A0(b, func_089A717C(this)) when there is an owner and this's
// state (+0x38) isn't 3.
void func_089A73B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t b = ctx.gpr[5];
    std::uint32_t owner;
    {
        GuestFrame frame(ctx, 0x10u);
        owner = call_guest(rt, ctx, kOwnerOf, {self});
    }
    if (owner != 0u && word(rt, self + 0x38u) != 3u) tail_call(ctx, kAttachTo, {b, owner});
}

// func_088D776C(this, flag) -> 1 after storing the flag byte at +0x5B7, unless the child at +0x640
// exists and the flag is set (then 0).
void func_088D776C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t flag = ctx.gpr[5] & 0xFFu;
    std::uint32_t stored = 0u;
    if (word(rt, ctx.gpr[4] + 0x640u) == 0u || flag == 0u) {
        rt.memory().store8(ctx.gpr[4] + 0x5B7u, static_cast<std::uint8_t>(flag));
        stored = 1u;
    }
    ctx.gpr[2] = stored;
}

// func_08B21A4C(this): hands the pending item at +0x6C to the sink at +0x2C (its virtual at +0xC)
// and clears it.
void func_08B21A4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t sink = word(rt, self + 0x2Cu);
    if (sink == 0u) return;
    const std::uint32_t pending = word(rt, self + 0x6Cu);
    if (pending == 0u) return;
    GuestFrame frame(ctx, 0x10u);
    (void)call_guest(rt, ctx, word(rt, word(rt, sink) + 0xCu), {sink, pending});
    rt.memory().store32(self + 0x6Cu, 0u);
}

// func_08B1C490() -> the u64 at 0x08D68A80 (v0:v1).
void func_08B1C490(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = word(rt, kClockValue);
    ctx.gpr[3] = word(rt, kClockValue + 0x4u);
}

// func_088DB4F8(this): this->+0x90 = func_08B1C490() (64 bits).
void func_088DB4F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const GuestResult value = call_guest(rt, ctx, kClock, {}, {});
    rt.memory().store32(self + 0x90u, value.v0);
    rt.memory().store32(self + 0x94u, value.v1);
}

// func_08AD4050(callback): calls callback->function (+0xC) with (callback->argument (+0x10), callback).
void func_08AD4050(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t callback = ctx.gpr[4];
    const std::uint32_t function = word(rt, callback + 0xCu);
    ctx.gpr[5] = callback;
    if (function != 0u) tail_call(ctx, function, {word(rt, callback + 0x10u), callback});
}

// func_0890AF20(-, vec3 *out): *out = {1, 1, 1}.
void func_0890AF20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t kOne = 0x3F800000u;
    rt.memory().store32(ctx.gpr[5] + 0x8u, kOne);
    rt.memory().store32(ctx.gpr[5], kOne);
    rt.memory().store32(ctx.gpr[5] + 0x4u, kOne);
}

// func_088FBB70(this) -> &object->member3C of this's virtual +0x30 result when that object's word at
// +0x18 is set, else NULL.
void func_088FBB70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t object = call_guest(rt, ctx, word(rt, word(rt, self) + 0x30u), {self});
    ctx.gpr[2] = (object != 0u && word(rt, object + 0x18u) != 0u) ? object + 0x3Cu : 0u;
}

// func_088F960C(this): when this->ptr8's flag byte +0x14 is clear and it has a source at +0x10,
// sets the flag to the source's virtual +8 (source, &ptr8->member4).
void func_088F960C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t q = word(rt, ctx.gpr[4] + 0x8u);
    if (rt.memory().load8(q + 0x14u) != 0u) return;
    const std::uint32_t source = word(rt, q + 0x10u);
    if (source == 0u) return;
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t flag = call_guest(rt, ctx, word(rt, word(rt, source) + 0x8u), {source, q + 0x4u});
    rt.memory().store8(q + 0x14u, static_cast<std::uint8_t>(flag));
}

// func_088937A8(holder, i) -> &object->records[object->signed byte 1][i] + 0x38 (rows of 0x14C bytes,
// i of 1000 bytes; a madd, so HI changes too).
void func_088937A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4]);
    const auto row = static_cast<std::int32_t>(static_cast<std::int8_t>(rt.memory().load8(object + 0x1u)));
    const std::uint64_t accumulator = (static_cast<std::uint64_t>(ctx.hi) << 32) | (ctx.gpr[5] * 1000u);
    const std::uint64_t sum = accumulator + static_cast<std::uint64_t>(static_cast<std::int64_t>(row) * 0x14C);
    ctx.lo = static_cast<std::uint32_t>(sum);
    ctx.hi = static_cast<std::uint32_t>(sum >> 32);
    ctx.gpr[2] = ctx.lo + object + 0x38u;
}

// func_088936C0(holder) -> the float at +0x844C of the object it points to.
void func_088936C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t base = word(rt, ctx.gpr[4]) + 0x10000u;
    ctx.gpr[2] = base;
    ctx.fpr[0] = float_at(rt, base - 0x7BB4u);
}

} // namespace gt::game

GT_DECOMP(0x089A717Cu, gt::game::func_089A717C, gt::ReturnKind::Word);
GT_DECOMP(0x08A7BF7Cu, gt::game::func_08A7BF7C, gt::ReturnKind::Word);
GT_DECOMP(0x089AA658u, gt::game::func_089AA658, gt::ReturnKind::Word);
GT_DECOMP(0x08ABF9B4u, gt::game::func_08ABF9B4, gt::ReturnKind::Void);
GT_DECOMP(0x08ABF990u, gt::game::func_08ABF990, gt::ReturnKind::Void);
GT_DECOMP(0x08ABF96Cu, gt::game::func_08ABF96C, gt::ReturnKind::Void);
GT_DECOMP(0x08ABF948u, gt::game::func_08ABF948, gt::ReturnKind::Void);
GT_DECOMP(0x08ABF924u, gt::game::func_08ABF924, gt::ReturnKind::Void);
GT_DECOMP(0x08B1C468u, gt::game::func_08B1C468, gt::ReturnKind::Void);
GT_DECOMP(0x08B1C3F8u, gt::game::func_08B1C3F8, gt::ReturnKind::Void);
GT_DECOMP(0x08B1C374u, gt::game::func_08B1C374, gt::ReturnKind::Void);
GT_DECOMP(0x08AF4F24u, gt::game::func_08AF4F24, gt::ReturnKind::Void);
GT_DECOMP(0x08AD9784u, gt::game::func_08AD9784, gt::ReturnKind::Word);
// Never reached on the three routes: written, registered once one does.
// GT_DECOMP(0x08AD96A0u, gt::game::func_08AD96A0, gt::ReturnKind::Void);
GT_DECOMP(0x08AB9C48u, gt::game::func_08AB9C48, gt::ReturnKind::Void);
GT_DECOMP(0x08AB9A9Cu, gt::game::func_08AB9A9C, gt::ReturnKind::Void);
GT_DECOMP(0x08AC8D20u, gt::game::func_08AC8D20, gt::ReturnKind::Void);
GT_DECOMP(0x08ABBC64u, gt::game::func_08ABBC64, gt::ReturnKind::Void);
GT_DECOMP(0x08AB978Cu, gt::game::func_08AB978C, gt::ReturnKind::Void);
GT_DECOMP(0x089AB274u, gt::game::func_089AB274, gt::ReturnKind::Void);
GT_DECOMP(0x089A73B8u, gt::game::func_089A73B8, gt::ReturnKind::Void);
GT_DECOMP(0x088D776Cu, gt::game::func_088D776C, gt::ReturnKind::Word);
GT_DECOMP(0x08B21A4Cu, gt::game::func_08B21A4C, gt::ReturnKind::Void);
GT_DECOMP(0x08B1C490u, gt::game::func_08B1C490, gt::ReturnKind::DoubleWord);
GT_DECOMP(0x088DB4F8u, gt::game::func_088DB4F8, gt::ReturnKind::Void);
GT_DECOMP(0x08AD4050u, gt::game::func_08AD4050, gt::ReturnKind::Void);
GT_DECOMP(0x0890AF20u, gt::game::func_0890AF20, gt::ReturnKind::Void);
GT_DECOMP(0x088FBB70u, gt::game::func_088FBB70, gt::ReturnKind::Word);
GT_DECOMP(0x088F960Cu, gt::game::func_088F960C, gt::ReturnKind::Void);
GT_DECOMP(0x088937A8u, gt::game::func_088937A8, gt::ReturnKind::Word);
GT_DECOMP(0x088936C0u, gt::game::func_088936C0, gt::ReturnKind::Float);

// --- batch 13 ---

namespace gt::game {
namespace {

constexpr std::uint32_t kSetLights = 0x08AD10F4u;        // (block, value)
constexpr std::uint32_t kRecordOf2 = 0x089277B4u;        // (object) -> record
constexpr std::uint32_t kCountOf = 0x089445D8u;          // (owner) -> count
constexpr std::uint32_t kBusyMember = 0x0889717Cu;       // (member) -> bool
constexpr std::uint32_t kVec4Assign = 0x089641C4u;       // (vec4 *to, const vec4 *from)
constexpr std::uint32_t kApplyMasked = 0x08AF1C40u;      // (state, a, mask)
constexpr std::uint32_t kHasSlots = 0x088D9E58u;         // (owner) -> bool
constexpr std::uint32_t kCurrentCar = 0x088A3DB8u;       // () -> car or NULL
constexpr std::uint32_t kGridValue = 0x0894C178u;        // (this, x, y) -> float

} // namespace

// func_08B2ABE4(a, b) -> a, after a |= b over their common length (bit sets as word vectors at +4..+8).
void func_08B2ABE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = ctx.gpr[4];
    const std::uint32_t b = ctx.gpr[5];
    std::uint32_t p = word(rt, a + 0x4u);
    std::uint32_t q = word(rt, b + 0x4u);
    const auto length_a = static_cast<std::uint32_t>(static_cast<std::int32_t>(word(rt, a + 0x8u) - p) >> 2);
    const auto length_b = static_cast<std::uint32_t>(static_cast<std::int32_t>(word(rt, b + 0x8u) - q) >> 2);
    const std::uint32_t end = p + (length_b < length_a ? length_b : length_a) * 4u;
    for (; p < end; p += 4u, q += 4u) rt.memory().store32(p, word(rt, p) | word(rt, q));
    ctx.gpr[2] = a;
}

// func_08AB8A28 / func_08AB8A04(holder, -, const vec3 *v): copies v to +0x210 / +0x234 of the object.
void copy_vec3_into(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t offset) {
    const std::uint32_t object = word(rt, ctx.gpr[4]);
    const std::uint32_t v = ctx.gpr[6];
    for (std::uint32_t k = 0u; k < 3u; ++k) rt.memory().store32(object + offset + k * 4u, word(rt, v + k * 4u));
    ctx.gpr[2] = object;
}
void func_08AB8A28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { copy_vec3_into(rt, ctx, 0x210u); }
void func_08AB8A04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { copy_vec3_into(rt, ctx, 0x234u); }

// func_08AB897C(holder, -, flag, value in $f12): the object's block at +0x1D0: byte +0x78 = flag, float +0x70 = value.
void func_08AB897C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t block = word(rt, ctx.gpr[4]) + 0x1D0u;
    rt.memory().store8(block + 0x78u, static_cast<std::uint8_t>(ctx.gpr[6]));
    rt.memory().store32(block + 0x70u, std::bit_cast<std::uint32_t>(ctx.fpr[12]));
    ctx.gpr[2] = block;
}

// func_08AB8930(holder, -, value): func_08AD10F4(&object->block1D0, value).
void func_08AB8930(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    tail_call(ctx, kSetLights, {word(rt, ctx.gpr[4]) + 0x1D0u, ctx.gpr[6]});
}

// func_089297D8(this, vec3 *out): *out = the vector at +0x70 of func_089277B4(this->ptr8).
void func_089297D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t v = call_guest(rt, ctx, kRecordOf2, {word(rt, ctx.gpr[4] + 0x8u)}) + 0x70u;
    for (std::uint32_t k = 0u; k < 3u; ++k) rt.memory().store32(out + k * 4u, word(rt, v + k * 4u));
    ctx.gpr[2] = v;
}

// func_08A5D4A0(this, object): this->+0x10 = object, retaining the new one and releasing the old.
void func_08A5D4A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t object = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        if (object != 0u) (void)call_guest(rt, ctx, kRetainRef, {object});
        const std::uint32_t old = word(rt, self + 0x10u);
        if (old != 0u) (void)call_guest(rt, ctx, kReleaseRef, {old});
    }
    rt.memory().store32(self + 0x10u, object);
}

// func_088A3B8C(this) -> func_089445D8(this->ptr18), or -1 without it.
void func_088A3B8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t owner = word(rt, ctx.gpr[4] + 0x18u);
    ctx.gpr[2] = 0xFFFFFFFFu;
    if (owner != 0u) tail_call(ctx, kCountOf, {owner});
}

// func_08895EFC(this) -> 1 when func_0889717C(&this->member20) is clear and the byte at +0xF8 is 1.
void func_08895EFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const bool busy = call_guest(rt, ctx, kBusyMember, {self + 0x20u}) != 0u;
    ctx.gpr[2] = (!busy && rt.memory().load8(self + 0xF8u) == 1u) ? 1u : 0u;
}

// func_089DB204(this, i, const vec4 *v): item i of the vec4 vector at +0x11C (doubled by bit 9 of
// the word at +0x140) = v.
void func_089DB204(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t item = point_data(rt, ctx.gpr[4], 9u, 0x11Cu) + (ctx.gpr[5] << 4);
    tail_call(ctx, kVec4Assign, {item, ctx.gpr[6]});
}

// func_08AF1DE0(this, a, mask): func_08AF1C40(this->ptr4, a, ptr4's mask at +0x2A0 & mask).
void func_08AF1DE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t state = word(rt, ctx.gpr[4] + 0x4u);
    tail_call(ctx, kApplyMasked, {state, ctx.gpr[5], word(rt, state + 0x2A0u) & ctx.gpr[6]});
}

// func_08AF1254(this, value): this->ptr4's +0x2B0 = value and its byte +0x310 cleared.
void func_08AF1254(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store32(word(rt, ctx.gpr[4] + 0x4u) + 0x2B0u, ctx.gpr[5]);
    rt.memory().store8(word(rt, ctx.gpr[4] + 0x4u) + 0x310u, 0u);
}

// func_088CE914(this) -> 1 when func_088D9E58(this->ptr4) holds and ptr4's byte at +0x5B8 is clear.
void func_088CE914(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    std::uint32_t result = 0u;
    if (call_guest(rt, ctx, kHasSlots, {word(rt, self + 0x4u)}) != 0u &&
        rt.memory().load8(word(rt, self + 0x4u) + 0x5B8u) == 0u) {
        result = 1u;
    }
    ctx.gpr[2] = result;
}

// func_088D6358(holder) -> the big-endian signed 16-bit value at +6 of the object.
void func_088D6358(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t raw = rt.memory().load16(word(rt, ctx.gpr[4]) + 0x6u);
    const std::uint32_t swapped = ((raw & 0xFFu) << 24) | ((raw >> 8) << 16);
    ctx.gpr[2] = static_cast<std::uint32_t>(static_cast<std::int32_t>(swapped) >> 16);
}

// func_08AF3FD8(value) -> value, after func_08B56790(1, value, 0, 0).
void func_08AF3FD8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t value = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    (void)call_guest(rt, ctx, kGuSetup, {1u, value, 0u, 0u});
    ctx.gpr[2] = value;
}

// func_08A76E3C(object, a, b): the object's class (its virtual +0x24), then that class's virtual
// +0x10C (class, a, b).
void func_08A76E3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4];
    const std::uint32_t a = ctx.gpr[5];
    const std::uint32_t b = ctx.gpr[6];
    std::uint32_t cls;
    {
        GuestFrame frame(ctx, 0x10u);
        cls = call_guest(rt, ctx, word(rt, word(rt, object) + 0x24u), {object});
    }
    tail_call(ctx, word(rt, word(rt, cls) + 0x10Cu), {cls, a, b});
}

// func_089AA48C(this) -> this->ptr40 when it and its +4 word are set, else NULL; under the lock at 0x08CF4ACC.
void func_089AA48C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { linked_under_lock(rt, ctx, 0x40u); }

// func_088A3DD0() -> the float at +0x1358 of func_088A3DB8()'s car, or 1.0 without one.
void func_088A3DD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t car = call_guest(rt, ctx, kCurrentCar, {});
    ctx.gpr[2] = car;
    ctx.fpr[0] = car != 0u ? float_at(rt, car + 0x1358u) : 1.0f;
}

// func_0894C240(this, x, y) -> func_0894C178(this, (s16) x, (s16) y), at most the float at +0xFC plus 1.
void func_0894C240(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const auto x = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(ctx.gpr[5])));
    const auto y = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(ctx.gpr[6])));
    float value;
    {
        GuestFrame frame(ctx, 0x10u);
        value = call_guest(rt, ctx, kGridValue, {self, x, y}, {}).f0;
    }
    const float limit = float_at(rt, self + 0xFCu) + 1.0f;
    if (limit < value) value = limit;
    ctx.fpr[0] = value;
}

} // namespace gt::game

GT_DECOMP(0x08B2ABE4u, gt::game::func_08B2ABE4, gt::ReturnKind::Word);
GT_DECOMP(0x08AB8A28u, gt::game::func_08AB8A28, gt::ReturnKind::Void);
GT_DECOMP(0x08AB8A04u, gt::game::func_08AB8A04, gt::ReturnKind::Void);
GT_DECOMP(0x08AB897Cu, gt::game::func_08AB897C, gt::ReturnKind::Void);
GT_DECOMP(0x08AB8930u, gt::game::func_08AB8930, gt::ReturnKind::Void);
GT_DECOMP(0x089297D8u, gt::game::func_089297D8, gt::ReturnKind::Void);
GT_DECOMP(0x08A5D4A0u, gt::game::func_08A5D4A0, gt::ReturnKind::Void);
GT_DECOMP(0x088A3B8Cu, gt::game::func_088A3B8C, gt::ReturnKind::Word);
GT_DECOMP(0x08895EFCu, gt::game::func_08895EFC, gt::ReturnKind::Word);
GT_DECOMP(0x089DB204u, gt::game::func_089DB204, gt::ReturnKind::Void);
GT_DECOMP(0x08AF1DE0u, gt::game::func_08AF1DE0, gt::ReturnKind::Void);
GT_DECOMP(0x08AF1254u, gt::game::func_08AF1254, gt::ReturnKind::Void);
GT_DECOMP(0x088CE914u, gt::game::func_088CE914, gt::ReturnKind::Word);
GT_DECOMP(0x088D6358u, gt::game::func_088D6358, gt::ReturnKind::Word);
GT_DECOMP(0x08AF3FD8u, gt::game::func_08AF3FD8, gt::ReturnKind::Word);
GT_DECOMP(0x08A76E3Cu, gt::game::func_08A76E3C, gt::ReturnKind::Void);
GT_DECOMP(0x089AA48Cu, gt::game::func_089AA48C, gt::ReturnKind::Word);
GT_DECOMP(0x088A3DD0u, gt::game::func_088A3DD0, gt::ReturnKind::Float);
GT_DECOMP(0x0894C240u, gt::game::func_0894C240, gt::ReturnKind::Float);

// --- batch 14 ---

namespace gt::game {
namespace {

constexpr std::uint32_t kNetMode = 0x08D69168u;          // a global mode word
constexpr std::uint32_t kVtableE0F8 = 0x08CCE0F8u;
constexpr std::uint32_t kVtableEF10 = 0x08CCEF10u;
constexpr std::uint32_t kVtable2578 = 0x08CD2578u;
constexpr std::uint32_t kFlagsLock = 0x08CF3F18u;        // mutex guarding func_08969330's read
constexpr std::uint32_t kStringAssign = 0x08B88F44u;     // (out, const std::string *)
constexpr std::uint32_t kBaseReset = 0x08A5B8B8u;        // (object)
constexpr std::uint32_t kRetainedRelease = 0x08A69D68u;  // (holder *)
constexpr std::uint32_t kBaseDestroy = 0x08A5BB5Cu;      // func_08A5BB5C
constexpr std::uint32_t kKeyCopy2 = 0x08A6A23Cu;         // (key slot *, const key *)
constexpr std::uint32_t kEmptyKey = 0x08CF6FDCu;         // a global empty key
constexpr std::uint32_t kBlend = 0x08B28FCCu;            // (current, a, b in $f12..$f14) -> float
constexpr std::uint32_t kSlotBehind = 0x088DD0E4u;       // func_088DD0E4 below
constexpr std::uint32_t kPositive538 = 0x08955990u;      // func_08955990 below
constexpr std::uint32_t kIsLive = 0x08A5D4F4u;           // (object) -> bool

} // namespace

// func_08B319A4 / func_08B31990() -> whether the global mode at 0x08D69168 is 1 / 2.
void func_08B319A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = word(rt, kNetMode) == 1u ? 1u : 0u; }
void func_08B31990(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = word(rt, kNetMode) == 2u ? 1u : 0u; }

// func_08968F20(this, const float v[2]): +0xB4/+0xB8 = v and flag 0x100000 set (+0xE0).
void func_08968F20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t v = ctx.gpr[5];
    rt.memory().store32(self + 0xB4u, word(rt, v));
    const std::uint32_t flags = word(rt, self + 0xE0u) | 0x100000u;
    rt.memory().store32(self + 0xE0u, flags);
    rt.memory().store32(self + 0xB8u, word(rt, v + 0x4u));
    ctx.gpr[3] = flags;
}

// func_0897D548 / func_089B8884(object): sets its vtable (base destructors).
void func_0897D548(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store32(ctx.gpr[4], kVtableE0F8);
    ctx.gpr[2] = kVtableE0F8;
}
void func_089B8884(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store32(ctx.gpr[4], kVtableEF10);
    ctx.gpr[2] = kVtableEF10;
}

// func_08969330(this) -> bit 1 of the flags at +0xE0, read under the lock at 0x08CF3F18.
void func_08969330(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    (void)call_guest(rt, ctx, kHeapLock, {kFlagsLock});
    const std::uint32_t flags = word(rt, self + 0xE0u);
    (void)call_guest(rt, ctx, kHeapUnlock, {kFlagsLock});
    ctx.gpr[2] = (flags >> 1) & 1u;
}

// func_08A6C4FC(out, object) -> out, after func_08B88F44(out, &object->name14).
void func_08A6C4FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    (void)call_guest(rt, ctx, kStringAssign, {out, ctx.gpr[5] + 0x14u});
    ctx.gpr[2] = out;
}

// func_08A5D6D0(this): func_08A5B8B8(this), then releases and clears the object at +0x10.
void func_08A5D6D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    (void)call_guest(rt, ctx, kBaseReset, {self});
    const std::uint32_t held = word(rt, self + 0x10u);
    if (held != 0u) (void)call_guest(rt, ctx, kReleaseRef, {held});
    rt.memory().store32(self + 0x10u, 0u);
}

// func_08A5D638(this): its destructor — vtable, release +0x10, destroy the holder at +0xC, base destructor.
void func_08A5D638(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    rt.memory().store32(self, kVtable2578);
    {
        GuestFrame frame(ctx, 0x10u);
        const std::uint32_t held = word(rt, self + 0x10u);
        if (held != 0u) (void)call_guest(rt, ctx, kReleaseRef, {held});
        (void)call_guest(rt, ctx, kRetainedRelease, {self + 0xCu});
    }
    tail_call(ctx, kBaseDestroy, {self});
}

// func_08A61E3C(this, const holder *h): the holder at +0x4C = *h (unless it is that holder).
void func_08A61E3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t mine = ctx.gpr[4] + 0x4Cu;
    if (mine != ctx.gpr[5]) tail_call(ctx, kHolderResetGame, {mine, word(rt, ctx.gpr[5])});
}

// func_08A5D460(this, object): when this->+0x10 (if live) is `object`, resets the key at +0xC to the empty key.
void func_08A5D460(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t link = word(rt, self + 0x10u);
    if (link != 0u && word(rt, link + 0x4u) == 0u) link = 0u;
    if (link == ctx.gpr[5]) tail_call(ctx, kKeyCopy2, {self + 0xCu, kEmptyKey});
}

// func_08B29034(a in $f12, b in $f13, this) -> this, after this->float8 = func_08B28FCC(this->float8, a, b).
void func_08B29034(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const float a = ctx.fpr[12];
    const float b = ctx.fpr[13];
    GuestFrame frame(ctx, 0x10u);
    const float blended = call_guest(rt, ctx, kBlend, {}, {float_at(rt, self + 0x8u), a, b}).f0;
    rt.memory().store32(self + 0x8u, std::bit_cast<std::uint32_t>(blended));
    ctx.gpr[2] = self;
}

// func_08A009BC(this): sets flag 8 in the word at +0xE4.
void func_08A009BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t flags = word(rt, ctx.gpr[4] + 0xE4u) | 0x8u;
    rt.memory().store32(ctx.gpr[4] + 0xE4u, flags);
    ctx.gpr[2] = flags;
}

// func_088DD0E4(this, i) -> whether slot i's word at +0x1FC is below -1 (when there is any slot;
// i isn't range-checked).
void func_088DD0E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t i = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    std::uint32_t result = 0u;
    if (call_guest(rt, ctx, kSlotCount2, {}) != 0u)
        result = static_cast<std::int32_t>(word(rt, slot(rt, self, i) + 0x1FCu)) < -1 ? 1u : 0u;
    ctx.gpr[2] = result;
}

// func_088B44A4(this) -> -1 when func_088DD0E4(this->+0x10, this->+0x14) holds, else whether
// this->ptrC's byte at +0x69E is 4.
void func_088B44A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    std::uint32_t result = 0xFFFFFFFFu;
    if (call_guest(rt, ctx, kSlotBehind, {word(rt, self + 0x10u), word(rt, self + 0x14u)}) == 0u)
        result = rt.memory().load8(word(rt, self + 0xCu) + 0x69Eu) == 4u ? 1u : 0u;
    ctx.gpr[2] = result;
}

// func_088B4108(set, i, value): value 0 clears byte i of the byte array at +0; a positive value sets
// bit i of the word mask at +8 (a negative i addresses words before it, bit 0).
void func_088B4108(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t set = ctx.gpr[4];
    const std::uint32_t i = ctx.gpr[5];
    const auto value = static_cast<std::int32_t>(ctx.gpr[6]);
    if (value == 0) {
        rt.memory().store8(word(rt, set) + i, 0u);
        return;
    }
    if (value <= 0) return;
    std::uint32_t at;
    std::uint32_t bit;
    if (static_cast<std::int32_t>(i) < 0) {
        const std::uint32_t k = (~i) >> 5;
        at = word(rt, set + 0x8u) - (k << 2) - 4u;
        bit = ((k << 5) + 0x20u) & 31u;
    } else {
        at = word(rt, set + 0x8u) + ((i >> 5) << 2);
        bit = i & 0x1Fu;
    }
    rt.memory().store32(at, word(rt, at) | (1u << bit));
}

// func_08A759BC(vector) -> the item count (4-byte items) minus one, at least 1.
void func_08A759BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const auto count = static_cast<std::int32_t>(word(rt, ctx.gpr[4] + 0x4u) - word(rt, ctx.gpr[4])) >> 2;
    ctx.gpr[2] = count < 2 ? 1u : static_cast<std::uint32_t>(count - 1);
}

// func_08955990(this) -> whether this->ptr8's float at +0x538 is positive.
void func_08955990(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = 0.0f < float_at(rt, word(rt, ctx.gpr[4] + 0x8u) + 0x538u) ? 1u : 0u;
}

// func_088A8F34(this) -> 1.0 when this->ptrC exists and func_08955990 holds for it, else 0.0.
void func_088A8F34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t owner = word(rt, ctx.gpr[4] + 0xCu);
    float result = 0.0f;
    if (owner != 0u) {
        GuestFrame frame(ctx, 0x10u);
        result = call_guest(rt, ctx, kPositive538, {owner}) != 0u ? 1.0f : 0.0f;
    }
    ctx.fpr[0] = result;
}

// func_0897D704(sx in $f12, sy in $f13, matrix): scales the 2D part of the matrix at +0x14
// (+0x14/+0x18 by sx, +0x20/+0x24 by sy).
void func_0897D704(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t m = ctx.gpr[4];
    const float sx = ctx.fpr[12], sy = ctx.fpr[13];
    const float m14 = sx * float_at(rt, m + 0x14u);
    const float m18 = sx * float_at(rt, m + 0x18u);
    const float m24 = sy * float_at(rt, m + 0x24u);
    const float m20 = sy * float_at(rt, m + 0x20u);
    rt.memory().store32(m + 0x14u, std::bit_cast<std::uint32_t>(m14));
    rt.memory().store32(m + 0x24u, std::bit_cast<std::uint32_t>(m24));
    rt.memory().store32(m + 0x20u, std::bit_cast<std::uint32_t>(m20));
    rt.memory().store32(m + 0x18u, std::bit_cast<std::uint32_t>(m18));
}

// func_08A5D50C(holder) -> whether the object it holds is live (func_08A5D4F4).
void func_08A5D50C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4]);
    std::uint32_t result = 0u;
    if (object != 0u) {
        GuestFrame frame(ctx, 0x10u);
        result = call_guest(rt, ctx, kIsLive, {object}) != 0u ? 1u : 0u;
    }
    ctx.gpr[2] = result;
}

} // namespace gt::game

GT_DECOMP(0x08B319A4u, gt::game::func_08B319A4, gt::ReturnKind::Word);
GT_DECOMP(0x08B31990u, gt::game::func_08B31990, gt::ReturnKind::Word);
GT_DECOMP(0x08968F20u, gt::game::func_08968F20, gt::ReturnKind::Void);
GT_DECOMP(0x0897D548u, gt::game::func_0897D548, gt::ReturnKind::Void);
GT_DECOMP(0x089B8884u, gt::game::func_089B8884, gt::ReturnKind::Void);
GT_DECOMP(0x08969330u, gt::game::func_08969330, gt::ReturnKind::Word);
GT_DECOMP(0x08A6C4FCu, gt::game::func_08A6C4FC, gt::ReturnKind::Word);
GT_DECOMP(0x08A5D6D0u, gt::game::func_08A5D6D0, gt::ReturnKind::Void);
GT_DECOMP(0x08A5D638u, gt::game::func_08A5D638, gt::ReturnKind::Void);
GT_DECOMP(0x08A61E3Cu, gt::game::func_08A61E3C, gt::ReturnKind::Void);
GT_DECOMP(0x08A5D460u, gt::game::func_08A5D460, gt::ReturnKind::Void);
GT_DECOMP(0x08B29034u, gt::game::func_08B29034, gt::ReturnKind::Word);
GT_DECOMP(0x08A009BCu, gt::game::func_08A009BC, gt::ReturnKind::Void);
GT_DECOMP(0x088DD0E4u, gt::game::func_088DD0E4, gt::ReturnKind::Word);
GT_DECOMP(0x088B44A4u, gt::game::func_088B44A4, gt::ReturnKind::Word);
GT_DECOMP(0x088B4108u, gt::game::func_088B4108, gt::ReturnKind::Void);
GT_DECOMP(0x08A759BCu, gt::game::func_08A759BC, gt::ReturnKind::Word);
GT_DECOMP(0x08955990u, gt::game::func_08955990, gt::ReturnKind::Word);
GT_DECOMP(0x088A8F34u, gt::game::func_088A8F34, gt::ReturnKind::Float);
GT_DECOMP(0x0897D704u, gt::game::func_0897D704, gt::ReturnKind::Void);
GT_DECOMP(0x08A5D50Cu, gt::game::func_08A5D50C, gt::ReturnKind::Word);

// --- batch 15 ---

namespace gt::game {
namespace {

constexpr std::uint32_t kSlotBody = 0x088DF13Cu;         // func_088DF13C (owner, i) -> &slot[i] + 0x30
constexpr std::uint32_t kNew14 = 0x08A66F3Cu;            // (object, arg): constructor of a 0x14-byte object
constexpr std::uint32_t kEntryValue = 0x088F0F68u;       // (entry) -> word
constexpr std::uint32_t kByte4C = 0x088D4C9Cu;           // func_088D4C9C
constexpr std::uint32_t kRecords = 0x088CB360u;          // func_088CB360 (i) -> &records[i]

// The slot body a link points to: func_088DF13C(link->owner (+8), link->index (+4)).
std::uint32_t link_slot(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t link) {
    return call_guest(rt, ctx, kSlotBody, {word(rt, link + 0x8u), word(rt, link + 0x4u)});
}

} // namespace

// Reads through a link (owner at +8, index at +4) into its slot body; 0 without an owner:
// func_088E7568 / func_088E7468 -> the byte at +0x26 / +0x3C, func_088E71B0 / func_088E7178 -> the word
// at +0x74 / +0x14, func_088E73BC / func_088E7380 -> whether the byte at +0x199 / +0x198 is set.
void link_byte(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t offset) {
    const std::uint32_t link = ctx.gpr[4];
    std::uint32_t value = 0u;
    if (word(rt, link + 0x8u) != 0u) {
        GuestFrame frame(ctx, 0x10u);
        value = rt.memory().load8(link_slot(rt, ctx, link) + offset);
    }
    ctx.gpr[2] = value;
}
void link_word(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t offset) {
    const std::uint32_t link = ctx.gpr[4];
    std::uint32_t value = 0u;
    if (word(rt, link + 0x8u) != 0u) {
        GuestFrame frame(ctx, 0x10u);
        value = word(rt, link_slot(rt, ctx, link) + offset);
    }
    ctx.gpr[2] = value;
}
void link_flag(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t offset) {
    link_byte(rt, ctx, offset);
    ctx.gpr[2] = ctx.gpr[2] != 0u ? 1u : 0u;
}
void func_088E7568(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { link_byte(rt, ctx, 0x26u); }
void func_088E7468(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { link_byte(rt, ctx, 0x3Cu); }
void func_088E71B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { link_word(rt, ctx, 0x74u); }
void func_088E7178(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { link_word(rt, ctx, 0x14u); }
void func_088E73BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { link_flag(rt, ctx, 0x199u); }
void func_088E7380(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { link_flag(rt, ctx, 0x198u); }

// func_088E7430(link) -> the virtual +0x3C of the object at +0xC with the link's index, or 0 without it.
void func_088E7430(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t link = ctx.gpr[4];
    const std::uint32_t object = word(rt, link + 0xCu);
    std::uint32_t value = 0u;
    if (object != 0u) {
        GuestFrame frame(ctx, 0x10u);
        value = call_guest(rt, ctx, word(rt, word(rt, object) + 0x3Cu), {object, word(rt, link + 0x4u)});
    }
    ctx.gpr[2] = value;
}

// func_088DF160(owner, i) -> the word at +0x78 of slot i's body.
void func_088DF160(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = word(rt, call_guest(rt, ctx, kSlotBody, {}) + 0x78u);
}

// func_08A729A4(stack, i, const holder *h) -> &items[base + i] after storing *h there (and raising the
// high-water mark at +0x10 to base + i).
void func_08A729A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t stack = ctx.gpr[4];
    const std::uint32_t source = ctx.gpr[6];
    const std::uint32_t index = word(rt, stack + 0xCu) + ctx.gpr[5];
    if (static_cast<std::int32_t>(word(rt, stack + 0x10u)) < static_cast<std::int32_t>(index))
        rt.memory().store32(stack + 0x10u, index);
    const std::uint32_t item = word(rt, stack) + (index << 2);
    if (item != source) {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, kHolderResetGame, {item, word(rt, source)});
    }
    ctx.gpr[2] = item;
}

// func_08A66F68(arg) -> a new 0x14-byte object constructed with arg (func_08A66F3C).
void func_08A66F68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t arg = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t object = call_guest(rt, ctx, kAllocate, {0x14u});
    (void)call_guest(rt, ctx, kNew14, {object, arg});
    ctx.gpr[2] = object;
}

// func_08AB8990(holder, -, int *flag) -> the float at +0x70 of the object's block at +0x1D0, and
// *flag = the block's byte at +0x78 when flag isn't null (func_08AB897C's getter).
void func_08AB8990(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4]);
    const std::uint32_t block = object + 0x1D0u;
    ctx.gpr[2] = object;
    if (ctx.gpr[6] != 0u) {
        ctx.gpr[2] = rt.memory().load8(block + 0x78u);
        rt.memory().store32(ctx.gpr[6], ctx.gpr[2]);
    }
    ctx.fpr[0] = float_at(rt, block + 0x70u);
}

// func_089624FC(pair) -> whether its first or third word is set.
void func_089624FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = (word(rt, ctx.gpr[4]) != 0u || word(rt, ctx.gpr[4] + 0x8u) != 0u) ? 1u : 0u;
}

// func_08903350(this, i) -> the first word of entry i (8-byte entries at +0x58), or 0 when i isn't
// below the u16 count at +0x38.
void func_08903350(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const auto i = static_cast<std::int32_t>(ctx.gpr[5]);
    std::uint32_t value = 0u;
    if (i < static_cast<std::int32_t>(rt.memory().load16(ctx.gpr[4] + 0x38u)))
        value = word(rt, (ctx.gpr[5] << 3) + word(rt, ctx.gpr[4] + 0x58u));
    ctx.gpr[2] = value;
}

// func_088EFD04(this, i) -> func_088F0F68(&table->entries[i]) for 0 <= i < table->count (table at
// +0xCC: entries at +8, count at +0xC), else 0.
void func_088EFD04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const auto i = static_cast<std::int32_t>(ctx.gpr[5]);
    std::uint32_t value = 0u;
    if (i >= 0) {
        const std::uint32_t table = word(rt, ctx.gpr[4] + 0xCCu);
        if (i < static_cast<std::int32_t>(word(rt, table + 0xCu))) {
            GuestFrame frame(ctx, 0x10u);
            value = call_guest(rt, ctx, kEntryValue, {word(rt, table + 0x8u) + (ctx.gpr[5] << 3)});
        }
    }
    ctx.gpr[2] = value;
}

// func_088CE848(this) -> func_088D4C9C(&this->ptr4->member4).
void func_088CE848(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, kByte4C, {word(rt, ctx.gpr[4] + 0x4u) + 0x4u});
}

// func_088CB938 / func_088CB51C() -> &records[0] + 0x61C / + 0x628.
void func_088CB938(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, kRecords, {0u}) + 0x61Cu;
}
void func_088CB51C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, kRecords, {0u}) + 0x628u;
}

} // namespace gt::game

GT_DECOMP(0x088E7568u, gt::game::func_088E7568, gt::ReturnKind::Word);
GT_DECOMP(0x088E7468u, gt::game::func_088E7468, gt::ReturnKind::Word);
GT_DECOMP(0x088E71B0u, gt::game::func_088E71B0, gt::ReturnKind::Word);
GT_DECOMP(0x088E7178u, gt::game::func_088E7178, gt::ReturnKind::Word);
GT_DECOMP(0x088E73BCu, gt::game::func_088E73BC, gt::ReturnKind::Word);
GT_DECOMP(0x088E7380u, gt::game::func_088E7380, gt::ReturnKind::Word);
GT_DECOMP(0x088E7430u, gt::game::func_088E7430, gt::ReturnKind::Word);
GT_DECOMP(0x088DF160u, gt::game::func_088DF160, gt::ReturnKind::Word);
GT_DECOMP(0x08A729A4u, gt::game::func_08A729A4, gt::ReturnKind::Word);
GT_DECOMP(0x08A66F68u, gt::game::func_08A66F68, gt::ReturnKind::Word);
GT_DECOMP(0x08AB8990u, gt::game::func_08AB8990, gt::ReturnKind::Float);
GT_DECOMP(0x089624FCu, gt::game::func_089624FC, gt::ReturnKind::Word);
GT_DECOMP(0x08903350u, gt::game::func_08903350, gt::ReturnKind::Word);
GT_DECOMP(0x088EFD04u, gt::game::func_088EFD04, gt::ReturnKind::Word);
GT_DECOMP(0x088CE848u, gt::game::func_088CE848, gt::ReturnKind::Word);
GT_DECOMP(0x088CB938u, gt::game::func_088CB938, gt::ReturnKind::Word);
GT_DECOMP(0x088CB51Cu, gt::game::func_088CB51C, gt::ReturnKind::Word);

// --- batch 16 ---

namespace gt::game {
namespace {

constexpr std::uint32_t kListItemAt = 0x089F9B54u;      // gt::game::func_089F9B54 (vector, i) -> &item[i]
constexpr std::uint32_t kListCount = 0x089FA8A0u;       // gt::game::func_089FA8A0 (vector) -> count
constexpr std::uint32_t kDrawEntry = 0x08B537CCu;       // (target, entry, 0, f12 = 0.0)
constexpr std::uint32_t kDrawFallback = 0x08AF8500u;    // (self, i, 0, 0, 0, f12 = 0.0)
constexpr std::uint32_t kRefreshFlags = 0x0896AD58u;    // (self)
constexpr std::uint32_t kDefaultVec4 = 0x08C8B5A4u;     // a 16-byte default value
constexpr std::uint32_t kChainNext = 0x08A69D3Cu;       // (node)
constexpr std::uint32_t kCurrentWidth = 0x089A8BF4u;    // gt::game::func_089A8BF4 -> f0
constexpr std::uint32_t kPackColor = 0x08964254u;       // (const float rgba[4], f12 scale) -> packed RGBA8
constexpr std::uint32_t kApplyColor = 0x08B57DE8u;      // (packed)
constexpr std::uint32_t kQueryInto = 0x08AD15FCu;       // (source, out, arg)

void copy_words(psprecomp::Runtime &rt, std::uint32_t to, std::uint32_t from) {
    const std::uint32_t a = word(rt, from), b = word(rt, from + 0x4u), c = word(rt, from + 0x8u), d = word(rt, from + 0xCu);
    rt.memory().store32(to, a);
    rt.memory().store32(to + 0x4u, b);
    rt.memory().store32(to + 0x8u, c);
    rt.memory().store32(to + 0xCu, d);
}

// Forwards (owner, self, arg) to the virtual at `slot` of the delegate at self+`field`; with no
// delegate returns self (the original leaves it in v0).
void forward_to_delegate(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t field, std::uint32_t slot) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t delegate = word(rt, self + field);
    ctx.gpr[2] = self;
    if (delegate != 0u) tail_call(ctx, word(rt, word(rt, delegate) + slot), {delegate, self, ctx.gpr[5]});
}

} // namespace

// func_08965CE8(this) -> 1 when the virtual +0x194 holds and bit 1 of the flags at +0xE4 is set.
void func_08965CE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t holds;
    {
        GuestFrame frame(ctx, 0x10u);
        holds = call_guest(rt, ctx, word(rt, word(rt, self) + 0x194u), {self});
    }
    ctx.gpr[2] = (holds != 0u && (word(rt, self + 0xE4u) & 2u) != 0u) ? 1u : 0u;
}

// func_08966168(this) -> bit 0 of the flags at +0xE4; func_08965BD4(this) -> bits 21-22 of +0xE0.
void func_08966168(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = word(rt, ctx.gpr[4] + 0xE4u) & 1u;
}
void func_08965BD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = (word(rt, ctx.gpr[4] + 0xE0u) >> 21) & 3u;
}

// func_0894268C(this, row, column) -> &this->slots[table[row / 2][column]] (slots of 0x268 bytes at
// +0x4A8; the byte table, rows of 0x18, at +0x1318).
void func_0894268C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const auto row = static_cast<std::uint32_t>(static_cast<std::int32_t>(ctx.gpr[5]) >> 1);
    const std::uint32_t slot = rt.memory().load8(self + row * 0x18u + ctx.gpr[6] + 0x1318u);
    ctx.gpr[2] = self + slot * 0x268u + 0x4A8u;
}

// func_089641C4(vec4 *to, const vec4 *from) -> to, after copying the four words unless to == from.
void func_089641C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    if (ctx.gpr[4] != ctx.gpr[5]) copy_words(rt, ctx.gpr[4], ctx.gpr[5]);
    ctx.gpr[2] = ctx.gpr[4];
}

// func_0896418C(vec4 *to, const vec4 *from): stores the default value, then *from, into *to.
void func_0896418C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    copy_words(rt, ctx.gpr[4], kDefaultVec4);
    copy_words(rt, ctx.gpr[4], ctx.gpr[5]);
    ctx.gpr[2] = kDefaultVec4;
}

// func_08AF8BB4(this, i): draws entry i (0x20 bytes) of the table at +0xA8 into the target at +0x94,
// or falls back to func_08AF8500(this, i) when there is no table.
void func_08AF8BB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t table = word(rt, self + 0xA8u);
    ctx.fpr[12] = 0.0f;
    ctx.gpr[2] = ctx.gpr[5] << 5;
    if (table != 0u) tail_call(ctx, kDrawEntry, {word(rt, self + 0x94u), table + (ctx.gpr[5] << 5), 0u, 0u, 0u});
    else tail_call(ctx, kDrawFallback, {self, ctx.gpr[5], 0u, 0u, 0u});
}

// func_089660A0(this) -> the virtual +0x20C.
void func_089660A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    tail_call(ctx, word(rt, word(rt, ctx.gpr[4]) + 0x20Cu), {ctx.gpr[4]});
}

// func_089A8D0C / func_089A8C40(this, arg): the delegate at +0x64 / +0x11C, virtual +0x8 / +0x24.
void func_089A8D0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_to_delegate(rt, ctx, 0x64u, 0x8u); }
void func_089A8C40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_to_delegate(rt, ctx, 0x11Cu, 0x24u); }

// func_0896B0A8(this) -> the word at +0xA0, after func_0896AD58(this) when bit 20 of +0xE0 is set.
void func_0896B0A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    if ((word(rt, self + 0xE0u) & 0x100000u) != 0u) {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, kRefreshFlags, {self});
    }
    ctx.gpr[2] = word(rt, self + 0xA0u);
}

// func_08A71A68(this) -> the byte the virtual +0x110 reads (this, &byte, 1, -1) when it returns 1;
// -1 when it doesn't, or when +0x14 is set.
void func_08A71A68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t value = 0xFFFFFFFFu;
    if (word(rt, self + 0x14u) == 0u) {
        GuestFrame frame(ctx, 0x20u);
        rt.memory().store8(frame.at(0u), 0u);
        const std::uint32_t read = call_guest(rt, ctx, word(rt, word(rt, self) + 0x110u), {self, frame.at(0u), 1u, 0xFFFFFFFFu});
        if (read == 1u) value = rt.memory().load8(frame.at(0u));
    }
    ctx.gpr[2] = value;
}

// func_08A06E94 / func_08A06EC8(this): the item accessor / the count of the vector at +0x10C.
void func_08A06E94(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, kListItemAt, {ctx.gpr[4] + 0x10Cu, ctx.gpr[5]}); }
void func_08A06EC8(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, kListCount, {ctx.gpr[4] + 0x10Cu}); }

// func_0895549C(this) -> the float at +0x4F4 of the object at +0x8.
void func_0895549C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4] + 0x8u);
    ctx.gpr[2] = object;
    ctx.fpr[0] = float_at(rt, object + 0x4F4u);
}

// func_08A69D68(holder): func_08A69D3C(*holder) when the holder isn't empty.
void func_08A69D68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t node = word(rt, ctx.gpr[4]);
    ctx.gpr[2] = node;
    if (node != 0u) tail_call(ctx, kChainNext, {node});
}

// func_089C46F4(this, const float rgba[4]): packs rgba scaled by the current entry's width, stores
// it at +0x180 and applies it.
void func_089C46F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t rgba = ctx.gpr[5];
    std::uint32_t packed;
    {
        GuestFrame frame(ctx, 0x10u);
        const float width = call_guest(rt, ctx, kCurrentWidth, {self}, {}).f0;
        packed = call_guest(rt, ctx, kPackColor, {rgba}, {width}).v0;
    }
    rt.memory().store32(self + 0x180u, packed);
    tail_call(ctx, kApplyColor, {packed});
}

// func_08AD1C88(this, arg) -> the virtual +0x40 (this, &query) after func_08AD15FC(this->source
// (+0x78), &query, arg) fills a local query.
void func_08AD1C88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    (void)call_guest(rt, ctx, kQueryInto, {word(rt, self + 0x78u), frame.at(0u), ctx.gpr[5]});
    ctx.gpr[2] = call_guest(rt, ctx, word(rt, word(rt, self) + 0x40u), {self, frame.at(0u)});
}

} // namespace gt::game

GT_DECOMP(0x08965CE8u, gt::game::func_08965CE8, gt::ReturnKind::Word);
GT_DECOMP(0x08966168u, gt::game::func_08966168, gt::ReturnKind::Word);
GT_DECOMP(0x08965BD4u, gt::game::func_08965BD4, gt::ReturnKind::Word);
GT_DECOMP(0x0894268Cu, gt::game::func_0894268C, gt::ReturnKind::Word);
GT_DECOMP(0x089641C4u, gt::game::func_089641C4, gt::ReturnKind::Word);
GT_DECOMP(0x0896418Cu, gt::game::func_0896418C, gt::ReturnKind::Word);
GT_DECOMP(0x08AF8BB4u, gt::game::func_08AF8BB4, gt::ReturnKind::Word);
GT_DECOMP(0x089660A0u, gt::game::func_089660A0, gt::ReturnKind::Word);
GT_DECOMP(0x089A8D0Cu, gt::game::func_089A8D0C, gt::ReturnKind::Word);
GT_DECOMP(0x089A8C40u, gt::game::func_089A8C40, gt::ReturnKind::Word);
GT_DECOMP(0x0896B0A8u, gt::game::func_0896B0A8, gt::ReturnKind::Word);
GT_DECOMP(0x08A71A68u, gt::game::func_08A71A68, gt::ReturnKind::Word);
GT_DECOMP(0x08A06E94u, gt::game::func_08A06E94, gt::ReturnKind::Word);
GT_DECOMP(0x08A06EC8u, gt::game::func_08A06EC8, gt::ReturnKind::Word);
GT_DECOMP(0x0895549Cu, gt::game::func_0895549C, gt::ReturnKind::Float);
GT_DECOMP(0x08A69D68u, gt::game::func_08A69D68, gt::ReturnKind::Word);
GT_DECOMP(0x089C46F4u, gt::game::func_089C46F4, gt::ReturnKind::Word);
GT_DECOMP(0x08AD1C88u, gt::game::func_08AD1C88, gt::ReturnKind::Word);

// --- batch 17 ---

namespace gt::game {
namespace {

constexpr std::uint32_t kRetainNode = 0x08A69638u;      // (node): takes a reference
constexpr std::uint32_t kReleaseNode = 0x08A69D3Cu;     // (node): drops a reference
constexpr std::uint32_t kScaleBy = 0x08D47204u;         // float[3] the scale func_08AF7E0C applies
constexpr std::uint32_t kApplyScaled = 0x08B57D6Cu;     // (f12, f13, f14)
constexpr std::uint32_t kByteTable = 0x08CDA288u;       // 256 words, indexed by a byte
constexpr std::uint32_t kBlendOffset = 0x089559C4u;     // (this) -> f0
constexpr std::uint32_t kBlendKeys = 0x089551B0u;       // (this) -> the key block
constexpr std::uint32_t kNextSibling = 0x08968A38u;     // (node) -> the next node or 0

// The FPU's multiply: inf * 0 is the PSP's default NaN (0x7FC00000), not the host's (0xFFC00000).
float psp_mul(float a, float b) {
    if ((std::isinf(a) && b == 0.0f) || (std::isinf(b) && a == 0.0f)) return std::bit_cast<float>(0x7FC00000u);
    return a * b;
}

// The blends of one class: t = func_089559C4(this) + bias; keys = func_089551B0(this). Returns
// t * keys[late] + base * (1 - t), with `base` read by `base_of` after the keys.
template <typename Base>
float blend(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t late, Base base_of) {
    const std::uint32_t self = ctx.gpr[4];
    const float bias = ctx.fpr[12];
    GuestFrame frame(ctx, 0x20u);
    const float t = call_guest(rt, ctx, kBlendOffset, {self}, {}).f0 + bias;
    const std::uint32_t keys = call_guest(rt, ctx, kBlendKeys, {self});
    const float scaled = psp_mul(t, float_at(rt, keys + late));
    return scaled + psp_mul(base_of(keys), 1.0f - t);
}

// Calls `visit(this, arg, node)` for every node of the child list at +0xF4.
void for_each_child(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t visit) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t arg = ctx.gpr[5];
    std::uint32_t node = word(rt, self + 0xF4u);
    if (node == 0u) return;  // v0 untouched, as in the original
    GuestFrame frame(ctx, 0x10u);
    while (node != 0u) {
        (void)call_guest(rt, ctx, visit, {self, arg, node});
        node = call_guest(rt, ctx, kNextSibling, {node});
    }
    ctx.gpr[2] = 0u;
}

} // namespace

// func_0893EB40(this) -> whether the byte at +0x11 is 3.
void func_0893EB40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = rt.memory().load8(ctx.gpr[4] + 0x11u) == 3u ? 1u : 0u;
}

// func_08A6972C(node **to, node *const *from): copies the pointer and takes a reference to it.
void func_08A6972C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t node = word(rt, ctx.gpr[5]);
    rt.memory().store32(ctx.gpr[4], node);
    ctx.gpr[2] = node;
    if (node != 0u) tail_call(ctx, kRetainNode, {node});
}

// func_08A6A23C(node **to, node *const *from) -> to: assignment of a counted pointer (retain the new
// node, release the old one, store) unless to == from.
void func_08A6A23C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t to = ctx.gpr[4];
    const std::uint32_t from = ctx.gpr[5];
    if (to != from) {
        GuestFrame frame(ctx, 0x10u);
        if (const std::uint32_t incoming = word(rt, from); incoming != 0u) (void)call_guest(rt, ctx, kRetainNode, {incoming});
        if (const std::uint32_t outgoing = word(rt, to); outgoing != 0u) (void)call_guest(rt, ctx, kReleaseNode, {outgoing});
        rt.memory().store32(to, word(rt, from));
    }
    ctx.gpr[2] = to;
}

// func_08AF7E0C(x, y, z): func_08B57D6C with each coordinate multiplied by the global scale.
void func_08AF7E0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.fpr[14] = psp_mul(float_at(rt, kScaleBy + 0x8u), ctx.fpr[14]);
    ctx.fpr[12] = psp_mul(float_at(rt, kScaleBy), ctx.fpr[12]);
    ctx.fpr[13] = psp_mul(float_at(rt, kScaleBy + 0x4u), ctx.fpr[13]);
    ctx.gpr[2] = kScaleBy & 0xFFFF0000u;
    ctx.gpr[3] = kScaleBy;
    tail_call(ctx, kApplyScaled, {});
}

// func_08B29E40(value) -> the table-driven hash of its four bytes (low first), starting from 0:
// h = table[(h ^ byte) & 0xFF] ^ (h >> 8), the CRC-32 update step without the initial inversion.
void func_08B29E40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t value = ctx.gpr[4];
    std::uint32_t hash = 0u;
    for (int i = 0; i < 4; ++i, value >>= 8)
        hash = word(rt, kByteTable + (((hash ^ value) & 0xFFu) << 2)) ^ (hash >> 8);
    ctx.gpr[2] = hash;
}

// func_08A0F514(this): the virtual +0x1B8 when the virtual +0x1AC holds.
void func_08A0F514(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t holds;
    {
        GuestFrame frame(ctx, 0x10u);
        holds = call_guest(rt, ctx, word(rt, word(rt, self) + 0x1ACu), {self});
    }
    ctx.gpr[2] = holds;
    if (holds != 0u) tail_call(ctx, word(rt, word(rt, self) + 0x1B8u), {self});
}

// func_08956184 / func_08956098 / func_0895601C(this, bias): component x / y / z of the blend
// between the key block's two vectors (+0x0 and +0x1C); the base is read from a second
// func_089551B0 call, as the original does.
void func_08956184(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    ctx.fpr[0] = blend(rt, ctx, 0x1Cu, [&](std::uint32_t) { return float_at(rt, call_guest(rt, ctx, kBlendKeys, {self})); });
}
void func_08956098(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    ctx.fpr[0] = blend(rt, ctx, 0x20u, [&](std::uint32_t) { return float_at(rt, call_guest(rt, ctx, kBlendKeys, {self}) + 0x4u); });
}
void func_0895601C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    ctx.fpr[0] = blend(rt, ctx, 0x24u, [&](std::uint32_t) { return float_at(rt, call_guest(rt, ctx, kBlendKeys, {self}) + 0x8u); });
}

// func_089562D0 / func_08956268 / func_08956200(this, bias): the same blend from the base vector in
// the object at +0x8 (+0x704 / +0x700 / +0x6FC) to the key block's +0x3C / +0x38 / +0x34.
void func_089562D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    ctx.fpr[0] = blend(rt, ctx, 0x3Cu, [&](std::uint32_t) { return float_at(rt, word(rt, self + 0x8u) + 0x704u); });
}
void func_08956268(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    ctx.fpr[0] = blend(rt, ctx, 0x38u, [&](std::uint32_t) { return float_at(rt, word(rt, self + 0x8u) + 0x700u); });
}
void func_08956200(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    ctx.fpr[0] = blend(rt, ctx, 0x34u, [&](std::uint32_t) { return float_at(rt, word(rt, self + 0x8u) + 0x6FCu); });
}

// func_088A8B64(this, float out[3]): the three blends of the source at +0xC with the bias at +0x34.
void func_088A8B64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t out = ctx.gpr[5];
    if (word(rt, self + 0xCu) == 0u) return;
    GuestFrame frame(ctx, 0x10u);
    constexpr std::uint32_t kComponents[] = {0x08956184u, 0x08956098u, 0x0895601Cu};
    for (std::uint32_t i = 0; i < 3u; ++i) {
        const float value = call_guest(rt, ctx, kComponents[i], {word(rt, self + 0xCu)}, {float_at(rt, self + 0x34u)}).f0;
        rt.memory().store32(out + i * 4u, std::bit_cast<std::uint32_t>(value));
    }
}

// func_08A00AB0 / func_08A01C30(this, arg): func_08A009CC / func_08A01B7C(this, arg, child) for every
// child.
void func_08A00AB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { for_each_child(rt, ctx, 0x08A009CCu); }
void func_08A01C30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { for_each_child(rt, ctx, 0x08A01B7Cu); }

} // namespace gt::game

GT_DECOMP(0x0893EB40u, gt::game::func_0893EB40, gt::ReturnKind::Word);
GT_DECOMP(0x08A6972Cu, gt::game::func_08A6972C, gt::ReturnKind::Word);
GT_DECOMP(0x08A6A23Cu, gt::game::func_08A6A23C, gt::ReturnKind::Word);
GT_DECOMP(0x08AF7E0Cu, gt::game::func_08AF7E0C, gt::ReturnKind::Word);
GT_DECOMP(0x08B29E40u, gt::game::func_08B29E40, gt::ReturnKind::Word);
GT_DECOMP(0x08A0F514u, gt::game::func_08A0F514, gt::ReturnKind::Word);
GT_DECOMP(0x08956184u, gt::game::func_08956184, gt::ReturnKind::Float);
GT_DECOMP(0x08956098u, gt::game::func_08956098, gt::ReturnKind::Float);
GT_DECOMP(0x0895601Cu, gt::game::func_0895601C, gt::ReturnKind::Float);
GT_DECOMP(0x089562D0u, gt::game::func_089562D0, gt::ReturnKind::Float);
GT_DECOMP(0x08956268u, gt::game::func_08956268, gt::ReturnKind::Float);
GT_DECOMP(0x08956200u, gt::game::func_08956200, gt::ReturnKind::Float);
GT_DECOMP(0x088A8B64u, gt::game::func_088A8B64, gt::ReturnKind::Void);
GT_DECOMP(0x08A00AB0u, gt::game::func_08A00AB0, gt::ReturnKind::Word);
GT_DECOMP(0x08A01C30u, gt::game::func_08A01C30, gt::ReturnKind::Word);

// --- batch 18 ---

namespace gt::game {
namespace {

constexpr std::uint32_t kByteHash = 0x08B29E40u;        // gt::game::func_08B29E40 (value) -> hash
constexpr std::uint32_t kLookupIn = 0x08A5BAB8u;        // (key, table)
constexpr std::uint32_t kLookupTable = 0x08CF6ED8u;
constexpr std::uint32_t kSetRange = 0x08B52CDCu;        // (range, begin, end)
constexpr std::uint32_t kFindIn = 0x08B52CECu;          // (range, key, 0) -> count
constexpr std::uint32_t kSlotBase = 0x08955300u;        // gt::game::func_08955300 (this, i) -> &slot[i] + 0x70
constexpr std::uint32_t kApplyPair = 0x08960FA4u;       // (target, i, f12, f13)
constexpr std::uint32_t kWaveOf = 0x08AE0244u;          // (f12) -> f0

} // namespace

// func_08A76AA0(key): func_08A5BAB8(key, the table at 0x08CF6ED8).
void func_08A76AA0(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, kLookupIn, {ctx.gpr[4], kLookupTable}); }

// func_08B1ED60(unsigned *state) -> hash(*state = *state * 17 + 17): a pseudo-random step.
void func_08B1ED60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t next = word(rt, ctx.gpr[4]) * 17u + 17u;
    rt.memory().store32(ctx.gpr[4], next);
    ctx.gpr[2] = next;
    tail_call(ctx, kByteHash, {next});
}

// func_089552F4(this) -> the first byte of the object at +0x8; func_08945ECC(this) -> its +0x4D0.
void func_089552F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = rt.memory().load8(word(rt, ctx.gpr[4] + 0x8u));
}
void func_08945ECC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = word(rt, ctx.gpr[4] + 0x8u) + 0x4D0u;
}

// func_088A595C(this) -> the signed byte at +0x2E of the object at +0x0.
void func_088A595C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(rt.memory().load8(word(rt, ctx.gpr[4]) + 0x2Eu))));
}

// func_08907A44(this, bool on): func_08907910 when on, func_08907874 otherwise.
void func_08907A44(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[5] &= 0xFFu;
    tail_call(ctx, ctx.gpr[5] != 0u ? 0x08907910u : 0x08907874u, {});
}

// func_08AFA5C0(this) -> the virtual +0x38 (this, 0).
void func_08AFA5C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, word(rt, word(rt, self) + 0x38u), {self, 0u});
}

// func_08B1ECFC(scale) -> func_08B1EFF8(func_08B1ECC8()) * scale.
void func_08B1ECFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const float scale = ctx.fpr[12];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t source = call_guest(rt, ctx, 0x08B1ECC8u, {});
    const GuestResult value = call_guest(rt, ctx, 0x08B1EFF8u, {source}, {});
    ctx.gpr[2] = value.v0;
    ctx.fpr[0] = psp_mul(value.f0, scale);
}

// func_08AFAA28(this, key) -> whether key is in the range [+0x4, +0x8) (searched through the range
// object at +0x2C).
void func_08AFAA28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t key = ctx.gpr[5];
    const std::uint32_t range = self + 0x2Cu;
    GuestFrame frame(ctx, 0x10u);
    (void)call_guest(rt, ctx, kSetRange, {range, word(rt, self + 0x4u), word(rt, self + 0x8u)});
    ctx.gpr[2] = call_guest(rt, ctx, kFindIn, {range, key, 0u}) != 0u ? 1u : 0u;
}

// func_089566F0(this, i) -> func_08960FA4(this->target (+0xC), i, the float at +0x520 of the object
// at +0x8, slot i's +0xCC): a float.
void func_089566F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t index = ctx.gpr[5];
    const float first = float_at(rt, word(rt, self + 0x8u) + 0x520u);
    const std::uint32_t target = word(rt, self + 0xCu);
    std::uint32_t slot;
    {
        GuestFrame frame(ctx, 0x20u);
        slot = call_guest(rt, ctx, kSlotBase, {self, index});
    }
    ctx.fpr[13] = float_at(rt, slot + 0xCCu);
    ctx.fpr[12] = first;
    ctx.gpr[2] = slot;  // func_08960FA4 leaves v0 alone: the original returns the slot with its f0
    tail_call(ctx, kApplyPair, {target, index});
}

// func_088FAB84(this, a, b) -> func_08902554(this->second (+0x4), a, b, 0) when set, else
// func_08904A04(this->first, a, b, 0) when set, else 0.
void func_088FAB84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t result = 0u;
    GuestFrame frame(ctx, 0x10u);
    if (const std::uint32_t second = word(rt, self + 0x4u); second != 0u)
        result = call_guest(rt, ctx, 0x08902554u, {second, ctx.gpr[5], ctx.gpr[6], 0u});
    else if (const std::uint32_t first = word(rt, self); first != 0u)
        result = call_guest(rt, ctx, 0x08904A04u, {first, ctx.gpr[5], ctx.gpr[6], 0u});
    ctx.gpr[2] = result;
}

// func_08AC88A0(x) -> func_08AE0244(x folded into [-0.25, 0.25]: -0.5 - x below -0.25, 0.5 - x above
// 0.25).
void func_08AC88A0(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    float x = ctx.fpr[12];
    if (x < -0.25f) {
        ctx.fpr[0] = -0.5f;
        x = -0.5f - x;
    } else {
        ctx.fpr[0] = 0.25f;
        if (0.25f < x) {
            ctx.fpr[0] = 0.5f;
            x = 0.5f - x;
        }
    }
    ctx.fpr[12] = x;
    tail_call(ctx, kWaveOf, {});
}

} // namespace gt::game

GT_DECOMP(0x08A76AA0u, gt::game::func_08A76AA0, gt::ReturnKind::Word);
GT_DECOMP(0x08B1ED60u, gt::game::func_08B1ED60, gt::ReturnKind::Word);
GT_DECOMP(0x089552F4u, gt::game::func_089552F4, gt::ReturnKind::Word);
GT_DECOMP(0x08945ECCu, gt::game::func_08945ECC, gt::ReturnKind::Word);
GT_DECOMP(0x088A595Cu, gt::game::func_088A595C, gt::ReturnKind::Word);
GT_DECOMP(0x08907A44u, gt::game::func_08907A44, gt::ReturnKind::Word);
GT_DECOMP(0x08AFA5C0u, gt::game::func_08AFA5C0, gt::ReturnKind::Word);
GT_DECOMP(0x08B1ECFCu, gt::game::func_08B1ECFC, gt::ReturnKind::Float);
GT_DECOMP(0x08AFAA28u, gt::game::func_08AFAA28, gt::ReturnKind::Word);
GT_DECOMP(0x089566F0u, gt::game::func_089566F0, gt::ReturnKind::Float);
GT_DECOMP(0x088FAB84u, gt::game::func_088FAB84, gt::ReturnKind::Word);
GT_DECOMP(0x08AC88A0u, gt::game::func_08AC88A0, gt::ReturnKind::Float);

// --- batch 19 ---

namespace gt::game {
namespace {

constexpr std::uint32_t kImportAA88 = 0x08C7AA88u;  // an import stub (NID not resolved here) -> a value
constexpr std::uint32_t kImportAA50 = 0x08C7AA50u;  // an import stub (0, value)

// The pair holders of one class: `second` (+0x4) wins over `first` (+0x0); both empty -> 0.
// Tail-calls `on_second` / `on_first` with the chosen object.
void pair_dispatch(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t on_second, std::uint32_t on_first) {
    const std::uint32_t self = ctx.gpr[4];
    if (const std::uint32_t second = word(rt, self + 0x4u); second != 0u) {
        ctx.gpr[2] = second;
        tail_call(ctx, on_second, {second});
        return;
    }
    const std::uint32_t first = word(rt, self);
    ctx.gpr[2] = first;
    ctx.gpr[4] = first;
    if (first != 0u) tail_call(ctx, on_first, {first});
}

// Calls the virtual at `slot` of *holder with (object, holder, arg) when both are set.
void holder_call(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t slot) {
    const std::uint32_t holder = ctx.gpr[4];
    if (holder == 0u) return;  // v0 untouched, as in the original
    const std::uint32_t object = word(rt, holder);
    ctx.gpr[2] = object;
    if (object == 0u) return;
    ctx.gpr[2] = word(rt, object);
    tail_call(ctx, word(rt, word(rt, object) + slot), {object, holder, ctx.gpr[5]});
}

} // namespace

// func_088B1A0C(this) -> the word at +0x2F8 of the object at +0x4.
void func_088B1A0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = word(rt, word(rt, ctx.gpr[4] + 0x4u) + 0x2F8u);
}

// func_08AFA674(this) -> whether func_08B52D08(&this->range (+0x2C)) finds anything, after
// func_08B58960(this, x = +0x20, y = +0x24).
void func_08AFA674(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    (void)call_guest(rt, ctx, 0x08B58960u, {self}, {float_at(rt, self + 0x20u), float_at(rt, self + 0x24u)});
    ctx.gpr[2] = call_guest(rt, ctx, 0x08B52D08u, {self + 0x2Cu}) != 0u ? 1u : 0u;
}

// func_088A8DE0(this) -> the virtual +0x84 of the object at +0x8.
void func_088A8DE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4] + 0x8u);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, word(rt, word(rt, object) + 0x84u), {object});
}

// func_088FB0C8(this) -> func_0890103C(second) or func_08903504(first) (a float; 0.0 with neither).
void func_088FB0C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    if (word(rt, ctx.gpr[4] + 0x4u) == 0u) ctx.fpr[0] = 0.0f;
    pair_dispatch(rt, ctx, 0x0890103Cu, 0x08903504u);
}

// func_088FACE8(this) -> func_089012D4(second) or func_0890379C(first), 0 with neither.
void func_088FACE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_dispatch(rt, ctx, 0x089012D4u, 0x0890379Cu); }

// func_088C50B8(this): clears the words at +0x4 and +0x0, then func_088C5088(this).
void func_088C50B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store32(ctx.gpr[4] + 0x4u, 0u);
    rt.memory().store32(ctx.gpr[4], 0u);
    tail_call(ctx, 0x088C5088u, {ctx.gpr[4]});
}

// func_08A5BC14(size) -> func_08B2B4D8(16, size): 16-byte aligned allocation.
void func_08A5BC14(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, kAllocateAligned, {16u, ctx.gpr[4]}); }

// func_08B2E0A4(state): the import at 0x08C7AA50 (0, state); func_08B2E0B0() -> the import at
// 0x08C7AA88's result, after the 0x08C7AA50 import (0, 16).
void func_08B2E0A4(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, kImportAA50, {0u, ctx.gpr[4]}); }
void func_08B2E0B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t result = call_guest(rt, ctx, kImportAA88, {});
    (void)call_guest(rt, ctx, kImportAA50, {0u, 16u});
    ctx.gpr[2] = result;
}

// func_088E85C4(this) -> the word at +0x38 of the object at +0x2E4 when it is set and +0x1C is 1,
// the word at +0x8 otherwise.
void func_088E85C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t object = word(rt, self + 0x2E4u);
    std::uint32_t value = word(rt, self + 0x8u);
    if (object != 0u && word(rt, self + 0x1Cu) == 1u) value = word(rt, object + 0x38u);
    ctx.gpr[2] = value;
}

// func_088E8474(this) -> 0 when the byte at +0x2B4 is clear; else 1 when the word at +0x1FC is -1 or
// more; else whether the bytes at +0x14 and +0x15 are both set.
void func_088E8474(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t result = 0u;
    if (rt.memory().load8(self + 0x2B4u) != 0u) {
        if (static_cast<std::int32_t>(word(rt, self + 0x1FCu)) >= -1) result = 1u;
        else if (rt.memory().load8(self + 0x14u) != 0u && rt.memory().load8(self + 0x15u) != 0u) result = 1u;
    }
    ctx.gpr[2] = result;
}

// func_088E74A0(link) -> func_088DB67C(link->owner (+8), link->index (+4)), 0 without an owner.
void func_088E74A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t link = ctx.gpr[4];
    const std::uint32_t owner = word(rt, link + 0x8u);
    std::uint32_t value = 0u;
    if (owner != 0u) {
        GuestFrame frame(ctx, 0x10u);
        value = call_guest(rt, ctx, 0x088DB67Cu, {owner, word(rt, link + 0x4u)});
    }
    ctx.gpr[2] = value;
}

// func_08ABB6A4 / func_08ABB57C(holder, arg): the virtual +0x48 / +0x70 of *holder (object, holder, arg).
void func_08ABB6A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_call(rt, ctx, 0x48u); }
void func_08ABB57C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_call(rt, ctx, 0x70u); }

// func_088CE8D4(this) -> func_088D9E58(the object at +0x4).
void func_088CE8D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4] + 0x4u);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, 0x088D9E58u, {object});
}

} // namespace gt::game

GT_DECOMP(0x088B1A0Cu, gt::game::func_088B1A0C, gt::ReturnKind::Word);
GT_DECOMP(0x08AFA674u, gt::game::func_08AFA674, gt::ReturnKind::Word);
GT_DECOMP(0x088A8DE0u, gt::game::func_088A8DE0, gt::ReturnKind::Word);
GT_DECOMP(0x088FB0C8u, gt::game::func_088FB0C8, gt::ReturnKind::Float);
GT_DECOMP(0x088FACE8u, gt::game::func_088FACE8, gt::ReturnKind::Word);
GT_DECOMP(0x088C50B8u, gt::game::func_088C50B8, gt::ReturnKind::Word);
GT_DECOMP(0x08A5BC14u, gt::game::func_08A5BC14, gt::ReturnKind::Word);
GT_DECOMP(0x08B2E0A4u, gt::game::func_08B2E0A4, gt::ReturnKind::Word);
GT_DECOMP(0x08B2E0B0u, gt::game::func_08B2E0B0, gt::ReturnKind::Word);
GT_DECOMP(0x088E85C4u, gt::game::func_088E85C4, gt::ReturnKind::Word);
GT_DECOMP(0x088E8474u, gt::game::func_088E8474, gt::ReturnKind::Word);
GT_DECOMP(0x088E74A0u, gt::game::func_088E74A0, gt::ReturnKind::Word);
GT_DECOMP(0x08ABB6A4u, gt::game::func_08ABB6A4, gt::ReturnKind::Word);
GT_DECOMP(0x08ABB57Cu, gt::game::func_08ABB57C, gt::ReturnKind::Word);
GT_DECOMP(0x088CE8D4u, gt::game::func_088CE8D4, gt::ReturnKind::Word);

// --- batch 20 ---

namespace gt::game {
namespace {

constexpr std::uint32_t kImportA988 = 0x08C7A988u;  // import stubs (NIDs not resolved here)
constexpr std::uint32_t kImportAA08 = 0x08C7AA08u;
constexpr std::uint32_t kImportAA20 = 0x08C7AA20u;

// The float at `offset` of the object at +0x8 (which the original leaves in v0).
void object8_float(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t offset) {
    const std::uint32_t object = word(rt, ctx.gpr[4] + 0x8u);
    ctx.gpr[2] = object;
    ctx.fpr[0] = float_at(rt, object + offset);
}

// Stores the byte `value` at `offset` of the object at +0x0.
void object0_store_byte(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t offset) {
    const std::uint32_t object = word(rt, ctx.gpr[4]);
    ctx.gpr[2] = object;
    rt.memory().store8(object + offset, static_cast<std::uint8_t>(ctx.gpr[5]));
}

} // namespace

// func_08917888(this) -> the byte at +0x18; func_0895BBA4(this) -> whether the byte at +0x11 is 1;
// func_0895EA44(this) -> whether the byte at +0x69 is set.
void func_08917888(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = rt.memory().load8(ctx.gpr[4] + 0x18u); }
void func_0895BBA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = rt.memory().load8(ctx.gpr[4] + 0x11u) == 1u ? 1u : 0u;
}
void func_0895EA44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = rt.memory().load8(ctx.gpr[4] + 0x69u) != 0u ? 1u : 0u;
}

// Forwarders: func_0890E75C(a, b) -> func_0890E41C(a, b, 0); func_08A5D438(key) -> func_08A5BAB8(key,
// the table at 0x08CF6ED8) (as func_08A76AA0); func_0897D7F4(this) -> func_089C3DA8(this + 0x14);
// func_088DAA8C(this) -> func_088D5DA8(this + 4); func_08893674(holder) -> func_08893250(*holder);
// func_08B2E2B8() -> the import at 0x08C7A988.
void func_0890E75C(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.gpr[6] = 0u; tail_call(ctx, 0x0890E41Cu, {}); }
void func_08A5D438(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, kLookupIn, {ctx.gpr[4], kLookupTable}); }
void func_0897D7F4(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x089C3DA8u, {ctx.gpr[4] + 0x14u}); }
void func_088DAA8C(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x088D5DA8u, {ctx.gpr[4] + 0x4u}); }
void func_08893674(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x08893250u, {word(rt, ctx.gpr[4])}); }
void func_08B2E2B8(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, kImportA988, {}); }

// func_08967538(this, x): stores x clamped to [0, 1] (NaN kept) at +0xC8.
void func_08967538(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    float x = ctx.fpr[12];
    ctx.fpr[1] = 0.0f;
    ctx.fpr[0] = 1.0f;
    if (x < 0.0f) x = 0.0f;
    else if (1.0f < x) x = 1.0f;
    ctx.fpr[12] = x;
    rt.memory().store32(ctx.gpr[4] + 0xC8u, std::bit_cast<std::uint32_t>(x));
}

// func_088A9858(this) -> -func_08955CC8(source->blend (+0xC), this->index (+4), source->bias (+0x34))
// for the source at +0x8; 0.0 without one.
void func_088A9858(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t source = word(rt, self + 0x8u);
    ctx.gpr[2] = source;
    ctx.fpr[0] = 0.0f;
    if (source == 0u) return;
    GuestFrame frame(ctx, 0x10u);
    const GuestResult value = call_guest(rt, ctx, 0x08955CC8u, {word(rt, source + 0xCu), word(rt, self + 0x4u)},
                                         {float_at(rt, source + 0x34u)});
    ctx.gpr[2] = value.v0;
    ctx.fpr[0] = std::bit_cast<float>(std::bit_cast<std::uint32_t>(value.f0) ^ 0x80000000u);
}

// func_088A8FE8(this) -> (float)func_08949E8C(the object at +0xC); 0.0 without one.
void func_088A8FE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4] + 0xCu);
    ctx.gpr[2] = object;
    ctx.fpr[0] = 0.0f;
    if (object == 0u) return;
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t value = call_guest(rt, ctx, 0x08949E8Cu, {object});
    ctx.gpr[2] = value;
    ctx.fpr[0] = static_cast<float>(static_cast<std::int32_t>(value));
}

// func_089564F0 / func_089564E4 / func_089564D8 / func_08955334(this) -> the float at +0x6BC / +0x6B4 /
// +0x6B8 / +0x714 of the object at +0x8.
void func_089564F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { object8_float(rt, ctx, 0x6BCu); }
void func_089564E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { object8_float(rt, ctx, 0x6B4u); }
void func_089564D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { object8_float(rt, ctx, 0x6B8u); }
void func_08955334(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { object8_float(rt, ctx, 0x714u); }

// func_08ABFAB8(this): the virtual +0x0 when the byte at +0x5D is set, the virtual +0x34 otherwise.
void func_08ABFAB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t vtable = word(rt, ctx.gpr[4]);
    const std::uint32_t slot = rt.memory().load8(ctx.gpr[4] + 0x5Du) != 0u ? 0x0u : 0x34u;
    ctx.gpr[2] = vtable;
    tail_call(ctx, word(rt, vtable + slot), {ctx.gpr[4]});
}

// func_08AEB930(handle *h): the import at 0x08C7AA08 (*h, 1) when h is set; func_08AEB908(handle *h):
// the import at 0x08C7AA20 (*h, 1, 0) when h is set.
void func_08AEB930(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    if (ctx.gpr[4] == 0u) return;
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, kImportAA08, {word(rt, ctx.gpr[4]), 1u});
}
void func_08AEB908(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    if (ctx.gpr[4] == 0u) return;
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, kImportAA20, {word(rt, ctx.gpr[4]), 1u, 0u});
}

// func_0891126C / func_08911284 / func_08911278(this, value): the byte at +0x2C / +0x2D / +0x2F of the
// object at +0x0.
void func_0891126C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { object0_store_byte(rt, ctx, 0x2Cu); }
void func_08911284(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { object0_store_byte(rt, ctx, 0x2Du); }
void func_08911278(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { object0_store_byte(rt, ctx, 0x2Fu); }

// func_08911290(this) -> x * x + k * y * y for the object at +0x0 (x at +0x114, y at +0x118; k = 4
// when the byte at +0x2C is set, 1 otherwise).
void func_08911290(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4]);
    const float x = float_at(rt, object + 0x114u);
    const float y = float_at(rt, object + 0x118u);
    const std::uint32_t wide = rt.memory().load8(object + 0x2Cu);
    const float k = wide != 0u ? 4.0f : 1.0f;
    ctx.gpr[2] = object;
    ctx.gpr[3] = wide;
    ctx.fpr[0] = psp_mul(x, x) + psp_mul(k, psp_mul(y, y));
}

} // namespace gt::game

GT_DECOMP(0x08917888u, gt::game::func_08917888, gt::ReturnKind::Word);
GT_DECOMP(0x0895BBA4u, gt::game::func_0895BBA4, gt::ReturnKind::Word);
GT_DECOMP(0x0895EA44u, gt::game::func_0895EA44, gt::ReturnKind::Word);
GT_DECOMP(0x0890E75Cu, gt::game::func_0890E75C, gt::ReturnKind::Word);
GT_DECOMP(0x08A5D438u, gt::game::func_08A5D438, gt::ReturnKind::Word);
GT_DECOMP(0x0897D7F4u, gt::game::func_0897D7F4, gt::ReturnKind::Word);
GT_DECOMP(0x088DAA8Cu, gt::game::func_088DAA8C, gt::ReturnKind::Word);
GT_DECOMP(0x08893674u, gt::game::func_08893674, gt::ReturnKind::Word);
// Not registered: every case calls a kernel import the isolated diff run cannot replay ("not
// comparable 22/22" on the race route), so the gate can't check it.
// GT_DECOMP(0x08B2E2B8u, gt::game::func_08B2E2B8, gt::ReturnKind::Word);
GT_DECOMP(0x08967538u, gt::game::func_08967538, gt::ReturnKind::Void);
GT_DECOMP(0x088A9858u, gt::game::func_088A9858, gt::ReturnKind::Float);
GT_DECOMP(0x088A8FE8u, gt::game::func_088A8FE8, gt::ReturnKind::Float);
GT_DECOMP(0x089564F0u, gt::game::func_089564F0, gt::ReturnKind::Float);
GT_DECOMP(0x089564E4u, gt::game::func_089564E4, gt::ReturnKind::Float);
GT_DECOMP(0x089564D8u, gt::game::func_089564D8, gt::ReturnKind::Float);
GT_DECOMP(0x08955334u, gt::game::func_08955334, gt::ReturnKind::Float);
GT_DECOMP(0x08ABFAB8u, gt::game::func_08ABFAB8, gt::ReturnKind::Word);
// Not registered: every case calls a kernel import the isolated diff run cannot replay ("not
// comparable 22/22" on the race route), so the gate can't check it.
// GT_DECOMP(0x08AEB930u, gt::game::func_08AEB930, gt::ReturnKind::Word);
// Not registered: every case calls a kernel import the isolated diff run cannot replay ("not
// comparable 22/22" on the race route), so the gate can't check it.
// GT_DECOMP(0x08AEB908u, gt::game::func_08AEB908, gt::ReturnKind::Word);
GT_DECOMP(0x0891126Cu, gt::game::func_0891126C, gt::ReturnKind::Word);
GT_DECOMP(0x08911284u, gt::game::func_08911284, gt::ReturnKind::Word);
GT_DECOMP(0x08911278u, gt::game::func_08911278, gt::ReturnKind::Word);
GT_DECOMP(0x08911290u, gt::game::func_08911290, gt::ReturnKind::Float);

// --- batch 21 ---

namespace gt::game {
namespace {

constexpr std::uint32_t kImportA9E8 = 0x08C7A9E8u;     // an import stub (NID not resolved here)
constexpr std::uint32_t kByteSlots = 0x08CF205Cu;      // a byte table indexed by func_089161A0
constexpr std::uint32_t kVtable2EE8 = 0x08CC2EE8u;     // a vtable func_088AFAF0 installs
constexpr std::uint32_t kVtable2158 = 0x08CC2158u;     // a vtable func_088AFB14 installs
constexpr float kRefreshRate = std::bit_cast<float>(0x426FC29Fu);   // 59.94 (the PSP's refresh rate)
constexpr float kRefreshPeriod = std::bit_cast<float>(0x3C88AB7Cu); // 0.01668 (1 / 59.94 s)

// The float at `offset` of the object at +0xC (which the original leaves in v0).
void object12_float(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t offset) {
    const std::uint32_t object = word(rt, ctx.gpr[4] + 0xCu);
    ctx.gpr[2] = object;
    ctx.fpr[0] = float_at(rt, object + offset);
}

// func_08B28C74 / func_08B28C44: `body(this)` between func_08B34888(this) and func_08B34B24(this).
void bracketed(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t body) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08B34888u, {self});
        (void)call_guest(rt, ctx, body, {self});
    }
    tail_call(ctx, 0x08B34B24u, {self});
}

} // namespace

// func_088A9570(this, u8 *out) -> the virtual +0xBC of the object at +0x8, also stored as a byte at out.
void func_088A9570(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4] + 0x8u);
    const std::uint32_t out = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t value = call_guest(rt, ctx, word(rt, word(rt, object) + 0xBCu), {object});
    rt.memory().store8(out, static_cast<std::uint8_t>(value));
    ctx.gpr[2] = value;
}

// func_088A4800(this) -> bit 5 of the halfword func_088AA6D8(this + 0x270) points to, 0 for null.
void func_088A4800(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t entry;
    {
        GuestFrame frame(ctx, 0x10u);
        entry = call_guest(rt, ctx, 0x088AA6D8u, {self + 0x270u});
    }
    ctx.gpr[2] = entry != 0u ? (rt.memory().load16(entry) >> 5) & 1u : 0u;
}

// func_0895EA38(this) -> whether the byte at +0x68 is set.
void func_0895EA38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = rt.memory().load8(ctx.gpr[4] + 0x68u) != 0u ? 1u : 0u;
}

// func_0895545C(this): from the rate g at +0x4F0 of the object at +0x8, stores g * (1 / 59.94) at +0x4F4
// and 59.94 / g at +0x4F8; func_08955490(this, g) stores g first.
void func_0895545C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4] + 0x8u);
    const float rate = float_at(rt, object + 0x4F0u);
    const float per_frame = psp_mul(rate, kRefreshPeriod);
    const float frames = kRefreshRate / rate;
    rt.memory().store32(object + 0x4F4u, std::bit_cast<std::uint32_t>(per_frame));
    rt.memory().store32(object + 0x4F8u, std::bit_cast<std::uint32_t>(frames));
    ctx.gpr[2] = object;
    ctx.fpr[0] = per_frame;
    ctx.fpr[1] = kRefreshPeriod;
    ctx.fpr[2] = frames;
}
void func_08955490(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4] + 0x8u);
    ctx.gpr[2] = object;
    rt.memory().store32(object + 0x4F0u, std::bit_cast<std::uint32_t>(ctx.fpr[12]));
    tail_call(ctx, 0x0895545Cu, {ctx.gpr[4]});
}

// func_088A1A0C(this, g): func_08955490(func_0894CFF8(the object at +0x54), g).
void func_088A1A0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const float rate = ctx.fpr[12];
    std::uint32_t target;
    {
        GuestFrame frame(ctx, 0x20u);
        target = call_guest(rt, ctx, 0x0894CFF8u, {word(rt, ctx.gpr[4] + 0x54u)});
    }
    ctx.fpr[12] = rate;
    tail_call(ctx, 0x08955490u, {target});
}

// func_088FB1B4 / func_088FB144(this): func_08900E70 / func_089011E4 (second) or func_089033D8 /
// func_089036AC (first), 0 with neither (the pair holders of func_088FACE8).
void func_088FB1B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_dispatch(rt, ctx, 0x08900E70u, 0x089033D8u); }
void func_088FB144(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_dispatch(rt, ctx, 0x089011E4u, 0x089036ACu); }

// func_0893AEF8(this): counts down the byte at +0x74A of the object at +0x8 when it is set, clearing the
// word at +0x744 when it reaches 0.
void func_0893AEF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4] + 0x8u);
    const std::uint32_t count = rt.memory().load8(object + 0x74Au);
    const std::uint32_t next = (count - 1u) & 0xFFu;
    if (count != 0u) {
        rt.memory().store8(object + 0x74Au, static_cast<std::uint8_t>(next));
        if (next == 0u) rt.memory().store32(object + 0x744u, 0u);
    }
    ctx.gpr[2] = next;
    ctx.gpr[3] = count;
    ctx.gpr[4] = object;
}

// func_0893AE44(this): clears the word at +0x740 of the object at +0x8 unless its signed byte at +0x748
// is 2.
void func_0893AE44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4] + 0x8u);
    if (static_cast<std::int8_t>(rt.memory().load8(object + 0x748u)) != 2) rt.memory().store32(object + 0x740u, 0u);
    ctx.gpr[2] = 2u;
    ctx.gpr[4] = object;
}

// func_089161A0(-, -, i, value): the byte table at 0x08CF205C, entry i + 8.
void func_089161A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store8(ctx.gpr[6] + kByteSlots + 0x8u, static_cast<std::uint8_t>(ctx.gpr[7]));
    ctx.gpr[2] = kByteSlots;
}

// func_088E73F8(link) -> func_088E1B90(link->owner (+8), link->index (+4)), 0 without an owner
// (as func_088E74A0).
void func_088E73F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t link = ctx.gpr[4];
    const std::uint32_t owner = word(rt, link + 0x8u);
    std::uint32_t value = 0u;
    if (owner != 0u) {
        GuestFrame frame(ctx, 0x10u);
        value = call_guest(rt, ctx, 0x088E1B90u, {owner, word(rt, link + 0x4u)});
    }
    ctx.gpr[2] = value;
}

// func_088C57DC / func_088C57BC / func_088C577C(this, arg) -> func_088C55A0 (object, arg, 0) / (object,
// arg, -1) / func_088C557C(object, arg, 0), for the object at +0x4.
void forward_object4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t target, std::uint32_t third) {
    const std::uint32_t object = word(rt, ctx.gpr[4] + 0x4u);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, target, {object, ctx.gpr[5], third});
}
void func_088C57DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_object4(rt, ctx, 0x088C55A0u, 0u); }
void func_088C57BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_object4(rt, ctx, 0x088C55A0u, 0xFFFFFFFFu); }
void func_088C577C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_object4(rt, ctx, 0x088C557Cu, 0u); }

// func_088B16D4(this) -> the word at +0x2E0 of the object at +0x4.
void func_088B16D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = word(rt, word(rt, ctx.gpr[4] + 0x4u) + 0x2E0u);
}

// func_08AFA404(this, x, y): stores the point at +0x20.
void func_08AFA404(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store32(ctx.gpr[4] + 0x24u, std::bit_cast<std::uint32_t>(ctx.fpr[13]));
    rt.memory().store32(ctx.gpr[4] + 0x20u, std::bit_cast<std::uint32_t>(ctx.fpr[12]));
}

// func_089B8BA0(this) -> func_089B8A6C(this, the words at +0xC and +0x10).
void func_089B8BA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    tail_call(ctx, 0x089B8A6Cu, {ctx.gpr[4], word(rt, ctx.gpr[4] + 0xCu), word(rt, ctx.gpr[4] + 0x10u)});
}

// func_089552E8 / func_089552DC(this) -> the float at +0xC / +0x8 of the object at +0xC.
void func_089552E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { object12_float(rt, ctx, 0xCu); }
void func_089552DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { object12_float(rt, ctx, 0x8u); }

// func_089567B4(this, x): stores x at +0x538 of the object at +0x8, then func_08956758(this).
void func_089567B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4] + 0x8u);
    ctx.gpr[2] = object;
    rt.memory().store32(object + 0x538u, std::bit_cast<std::uint32_t>(ctx.fpr[12]));
    tail_call(ctx, 0x08956758u, {ctx.gpr[4]});
}

// func_08AF23C0(this, arg) -> func_08C3F490(&{+0x4, 0}, &{0, +0x8}, arg): two local pairs built from
// this.
void func_08AF23C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t arg = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    rt.memory().store32(frame.at(0x4u), word(rt, self + 0x8u));
    rt.memory().store32(frame.at(0x8u), word(rt, self + 0x4u));
    rt.memory().store32(frame.at(0xCu), 0u);
    rt.memory().store32(frame.at(0x0u), 0u);
    ctx.gpr[2] = call_guest(rt, ctx, 0x08C3F490u, {frame.at(0x8u), frame.at(0x0u), arg});
}

// func_08909B00(this, x) -> func_08909AF0(this, func_08AF26F0(x)).
void func_08909B00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t value;
    {
        GuestFrame frame(ctx, 0x10u);
        value = call_guest(rt, ctx, 0x08AF26F0u, {ctx.gpr[5]});
    }
    tail_call(ctx, 0x08909AF0u, {self, value});
}

void func_08B28C74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bracketed(rt, ctx, 0x08B28BE8u); }
void func_08B28C44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bracketed(rt, ctx, 0x08B28C04u); }

// func_08AEB954() -> the import at 0x08C7A9E8 (arguments passed through).
void func_08AEB954(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, kImportA9E8, {});
}

// func_08A7273C(stack, n) -> &items[top -= n]: pops n words (items at +0x0, top at +0x1C);
// func_08A6159C(this, n): pops from the stack at +0x3C when n > 0, else returns 0.
void func_08A7273C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t stack = ctx.gpr[4];
    const std::uint32_t top = word(rt, stack + 0x1Cu) - ctx.gpr[5];
    rt.memory().store32(stack + 0x1Cu, top);
    ctx.gpr[2] = word(rt, stack) + (top << 2);
}
void func_08A6159C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    if (static_cast<std::int32_t>(ctx.gpr[5]) <= 0) {
        ctx.gpr[2] = 0u;
        return;
    }
    tail_call(ctx, 0x08A7273Cu, {word(rt, ctx.gpr[4] + 0x3Cu), ctx.gpr[5]});
}

// func_088AFAF0 / func_088AFB14(this): base constructors, storing the vtable (and clearing +0x4).
void func_088AFAF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store32(ctx.gpr[4], kVtable2EE8);
    rt.memory().store32(ctx.gpr[4] + 0x4u, 0u);
    ctx.gpr[2] = kVtable2EE8;
}
void func_088AFB14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store32(ctx.gpr[4], kVtable2158);
    ctx.gpr[2] = kVtable2158;
}

} // namespace gt::game

GT_DECOMP(0x088A9570u, gt::game::func_088A9570, gt::ReturnKind::Word);
GT_DECOMP(0x088A4800u, gt::game::func_088A4800, gt::ReturnKind::Word);
GT_DECOMP(0x0895EA38u, gt::game::func_0895EA38, gt::ReturnKind::Word);
GT_DECOMP(0x0895545Cu, gt::game::func_0895545C, gt::ReturnKind::Void);
GT_DECOMP(0x08955490u, gt::game::func_08955490, gt::ReturnKind::Void);
GT_DECOMP(0x088A1A0Cu, gt::game::func_088A1A0C, gt::ReturnKind::Void);
GT_DECOMP(0x088FB1B4u, gt::game::func_088FB1B4, gt::ReturnKind::Word);
GT_DECOMP(0x088FB144u, gt::game::func_088FB144, gt::ReturnKind::Word);
GT_DECOMP(0x0893AEF8u, gt::game::func_0893AEF8, gt::ReturnKind::Void);
GT_DECOMP(0x0893AE44u, gt::game::func_0893AE44, gt::ReturnKind::Void);
GT_DECOMP(0x089161A0u, gt::game::func_089161A0, gt::ReturnKind::Word);
GT_DECOMP(0x088E73F8u, gt::game::func_088E73F8, gt::ReturnKind::Word);
GT_DECOMP(0x088C57DCu, gt::game::func_088C57DC, gt::ReturnKind::Word);
GT_DECOMP(0x088C57BCu, gt::game::func_088C57BC, gt::ReturnKind::Word);
GT_DECOMP(0x088C577Cu, gt::game::func_088C577C, gt::ReturnKind::Word);
GT_DECOMP(0x088B16D4u, gt::game::func_088B16D4, gt::ReturnKind::Word);
GT_DECOMP(0x08AFA404u, gt::game::func_08AFA404, gt::ReturnKind::Void);
GT_DECOMP(0x089B8BA0u, gt::game::func_089B8BA0, gt::ReturnKind::Word);
GT_DECOMP(0x089552E8u, gt::game::func_089552E8, gt::ReturnKind::Float);
GT_DECOMP(0x089552DCu, gt::game::func_089552DC, gt::ReturnKind::Float);
GT_DECOMP(0x089567B4u, gt::game::func_089567B4, gt::ReturnKind::Void);
GT_DECOMP(0x08AF23C0u, gt::game::func_08AF23C0, gt::ReturnKind::Word);
GT_DECOMP(0x08909B00u, gt::game::func_08909B00, gt::ReturnKind::Word);
GT_DECOMP(0x08B28C74u, gt::game::func_08B28C74, gt::ReturnKind::Word);
GT_DECOMP(0x08B28C44u, gt::game::func_08B28C44, gt::ReturnKind::Word);
// Not registered: every case calls a kernel import the isolated diff run cannot replay ("not
// comparable" in all 59 cases), so the gate can't check it.
// GT_DECOMP(0x08AEB954u, gt::game::func_08AEB954, gt::ReturnKind::Word);
GT_DECOMP(0x08A7273Cu, gt::game::func_08A7273C, gt::ReturnKind::Word);
GT_DECOMP(0x08A6159Cu, gt::game::func_08A6159C, gt::ReturnKind::Word);
GT_DECOMP(0x088AFAF0u, gt::game::func_088AFAF0, gt::ReturnKind::Word);
GT_DECOMP(0x088AFB14u, gt::game::func_088AFB14, gt::ReturnKind::Word);

// --- batch 22 ---

namespace gt::game {
namespace {

constexpr std::uint32_t kGlobal72A0 = 0x08D472A0u;  // globals the setters below write
constexpr std::uint32_t kGlobal72A4 = 0x08D472A4u;
constexpr std::uint32_t kGlobal71BC = 0x08D471BCu;
constexpr std::uint32_t kFlagByte = 0x08CE2FA4u;    // the byte func_08892C1C reads
constexpr std::uint32_t kContext9138 = 0x08D69138u; // the object func_08B2EFDC passes first

// Stores a0 into a global; the original leaves the global's high half in v0.
void store_global(psprecomp::AllegrexContext &ctx, psprecomp::Runtime &rt, std::uint32_t address) {
    ctx.gpr[2] = address & 0xFFFF0000u;
    rt.memory().store32(address, ctx.gpr[4]);
}

// Stores a1 at `offset` of the object at `field` (the original leaves the object in v0).
void store_into(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t field, std::uint32_t offset) {
    const std::uint32_t object = word(rt, ctx.gpr[4] + field);
    ctx.gpr[2] = object;
    rt.memory().store32(object + offset, ctx.gpr[5]);
}

// `body(this)` between `enter(this)` and the tail call `leave(this)`.
void bracketed_by(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t enter, std::uint32_t body,
                  std::uint32_t leave) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, enter, {self});
        (void)call_guest(rt, ctx, body, {self});
    }
    tail_call(ctx, leave, {self});
}

} // namespace

// Global setters: func_08AFAA10 / func_08AFA9F8 / func_08AF7DE8(value).
void func_08AFAA10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_global(ctx, rt, kGlobal72A0); }
void func_08AFA9F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_global(ctx, rt, kGlobal72A4); }
void func_08AF7DE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_global(ctx, rt, kGlobal71BC); }

// func_08892C1C() -> the byte at 0x08CE2FA4.
void func_08892C1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[3] = kFlagByte & 0xFFFF0000u;
    ctx.gpr[2] = rt.memory().load8(kFlagByte);
}

// func_08866920(this): func_0888A24C(this + 0x4584), then func_08B52D90(1).
void func_08866920(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x0888A24Cu, {ctx.gpr[4] + 0x4584u});
    }
    tail_call(ctx, 0x08B52D90u, {1u});
}

// func_08A7BCC8(this, from): the counted-pointer assignment (func_08A6A23C) of the member at +0x18.
void func_08A7BCC8(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x08A6A23Cu, {ctx.gpr[4] + 0x18u}); }

// func_0889414C(this) -> the object at +0x0, plus 0x14.
void func_0889414C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = word(rt, ctx.gpr[4]) + 0x14u; }

// Setters through a member pointer: func_08AF12A4 / func_08AF1274 (object at +0x4, +0x2BC / +0x2B4);
// func_08AF2930 / func_08AF2858 / func_08AF284C (object at +0x0, +0x34 / +0x4 / +0x0).
void func_08AF12A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_into(rt, ctx, 0x4u, 0x2BCu); }
void func_08AF1274(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_into(rt, ctx, 0x4u, 0x2B4u); }
void func_08AF2930(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_into(rt, ctx, 0x0u, 0x34u); }
void func_08AF2858(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_into(rt, ctx, 0x0u, 0x4u); }
void func_08AF284C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_into(rt, ctx, 0x0u, 0x0u); }

// func_089773A8(this, row, column) -> the signed byte grid[row][column] (rows of 2 at +0x154);
// func_08977280(this, i) -> the word table[i] at +0x90.
void func_089773A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t address = (ctx.gpr[5] << 1) + ctx.gpr[4] + ctx.gpr[6] + 0x154u;
    ctx.gpr[2] = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(rt.memory().load8(address))));
}
void func_08977280(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = word(rt, (ctx.gpr[5] << 2) + ctx.gpr[4] + 0x90u);
}

// func_08A72724(stack) -> &items[top - 1] (the word stack of func_08A7273C).
void func_08A72724(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = word(rt, ctx.gpr[4]) + (word(rt, ctx.gpr[4] + 0x1Cu) << 2) - 4u;
}

// func_08B28D34(this) -> func_08B3467C(this), after which the bytes at +0x29 and +0x28 are cleared.
void func_08B28D34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t result;
    {
        GuestFrame frame(ctx, 0x10u);
        result = call_guest(rt, ctx, 0x08B3467Cu, {self});
    }
    rt.memory().store8(self + 0x29u, 0u);
    rt.memory().store8(self + 0x28u, 0u);
    ctx.gpr[2] = result;
}

// func_089B883C(this, owner): constructor (vtable, owner at +0x4, five cleared words), then
// func_089BA6B0(owner, this).
void func_089B883C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t owner = ctx.gpr[5];
    rt.memory().store32(self, kVtableEF10);
    rt.memory().store32(self + 0x4u, owner);
    for (std::uint32_t offset = 0x8u; offset <= 0x18u; offset += 4u) rt.memory().store32(self + offset, 0u);
    ctx.gpr[2] = self;
    ctx.gpr[3] = kVtableEF10;
    tail_call(ctx, 0x089BA6B0u, {owner, self, owner});
}

// func_08B2EFF0(-, x) -> func_08B599E0(1, x); func_08B2EFDC(a, b) -> func_08B1C590(the object at
// 0x08D69138, b, a, b); func_08B2DBE0() -> func_08B2DBD8(0).
void func_08B2EFF0(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x08B599E0u, {1u}); }
void func_08B2EFDC(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    tail_call(ctx, 0x08B1C590u, {kContext9138, ctx.gpr[5], ctx.gpr[4], ctx.gpr[5]});
}
void func_08B2DBE0(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x08B2DBD8u, {0u}); }

// func_08B28CD4 / func_08B28D04(this): func_08B28C04 / func_08B28BE8 between func_08B34880 and
// func_08B34B1C (the other lock pair of func_08B28C74).
void func_08B28CD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bracketed_by(rt, ctx, 0x08B34880u, 0x08B28C04u, 0x08B34B1Cu); }
void func_08B28D04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bracketed_by(rt, ctx, 0x08B34880u, 0x08B28BE8u, 0x08B34B1Cu); }

// func_0895541C(this) -> the byte at +0x733 of the object at +0x8.
void func_0895541C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = rt.memory().load8(word(rt, ctx.gpr[4] + 0x8u) + 0x733u);
}

// func_088D5910(holder) -> 0 when the big-endian halfword at +0x8 of *holder is set, else its
// big-endian halfword at +0xC.
void func_088D5910(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4]);
    ctx.gpr[4] = object;
    std::uint32_t value = 0u;
    if (rt.memory().load16(object + 0x8u) == 0u) {
        const std::uint32_t raw = rt.memory().load16(object + 0xCu);
        value = ((raw & 0xFFu) << 8) | (raw >> 8);
    }
    ctx.gpr[2] = value;
}

// func_08A76E88(this) -> whether the word at +0x10 is set.
void func_08A76E88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = word(rt, ctx.gpr[4] + 0x10u) != 0u ? 1u : 0u;
}

// func_08A7D398(this): func_08A77FF8(this), func_08A7D24C(this), then func_08A7BE18(this, 0).
void func_08A7D398(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08A77FF8u, {self});
        (void)call_guest(rt, ctx, 0x08A7D24Cu, {self});
    }
    tail_call(ctx, 0x08A7BE18u, {self, 0u});
}

// func_0890B5F0(this): func_08AF1DF0 on the members at +0x20, +0x10 and +0x28, then func_08AF2D20 on
// the one at +0x0.
void func_0890B5F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        for (const std::uint32_t member : {0x20u, 0x10u, 0x28u}) (void)call_guest(rt, ctx, 0x08AF1DF0u, {word(rt, self + member)});
    }
    tail_call(ctx, 0x08AF2D20u, {word(rt, self)});
}

// func_089675A0(this): func_089BD168(this), then func_089B8BA0(this + 0x84).
void func_089675A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x089BD168u, {self});
    }
    tail_call(ctx, 0x089B8BA0u, {self + 0x84u});
}

} // namespace gt::game

GT_DECOMP(0x08AFAA10u, gt::game::func_08AFAA10, gt::ReturnKind::Word);
GT_DECOMP(0x08AFA9F8u, gt::game::func_08AFA9F8, gt::ReturnKind::Word);
GT_DECOMP(0x08AF7DE8u, gt::game::func_08AF7DE8, gt::ReturnKind::Word);
GT_DECOMP(0x08892C1Cu, gt::game::func_08892C1C, gt::ReturnKind::Word);
GT_DECOMP(0x08866920u, gt::game::func_08866920, gt::ReturnKind::Word);
GT_DECOMP(0x08A7BCC8u, gt::game::func_08A7BCC8, gt::ReturnKind::Word);
GT_DECOMP(0x0889414Cu, gt::game::func_0889414C, gt::ReturnKind::Word);
GT_DECOMP(0x08AF12A4u, gt::game::func_08AF12A4, gt::ReturnKind::Word);
GT_DECOMP(0x08AF1274u, gt::game::func_08AF1274, gt::ReturnKind::Word);
GT_DECOMP(0x08AF2930u, gt::game::func_08AF2930, gt::ReturnKind::Word);
GT_DECOMP(0x08AF2858u, gt::game::func_08AF2858, gt::ReturnKind::Word);
GT_DECOMP(0x08AF284Cu, gt::game::func_08AF284C, gt::ReturnKind::Word);
GT_DECOMP(0x089773A8u, gt::game::func_089773A8, gt::ReturnKind::Word);
GT_DECOMP(0x08977280u, gt::game::func_08977280, gt::ReturnKind::Word);
GT_DECOMP(0x08A72724u, gt::game::func_08A72724, gt::ReturnKind::Word);
GT_DECOMP(0x08B28D34u, gt::game::func_08B28D34, gt::ReturnKind::Word);
GT_DECOMP(0x089B883Cu, gt::game::func_089B883C, gt::ReturnKind::Word);
// Not registered: it reaches a kernel import the isolated diff run cannot replay ("not comparable"
// in all 58 cases), so the gate can't check it.
// GT_DECOMP(0x08B2EFF0u, gt::game::func_08B2EFF0, gt::ReturnKind::Word);
// Not registered: it reaches a kernel import the isolated diff run cannot replay ("not comparable"
// in all 58 cases), so the gate can't check it.
// GT_DECOMP(0x08B2EFDCu, gt::game::func_08B2EFDC, gt::ReturnKind::Word);
// Not registered: it reaches a kernel import the isolated diff run cannot replay ("not comparable"
// in all 48 cases), so the gate can't check it.
// GT_DECOMP(0x08B2DBE0u, gt::game::func_08B2DBE0, gt::ReturnKind::Word);
// Not registered: it reaches a kernel import the isolated diff run cannot replay ("not comparable"
// in all 58 cases), so the gate can't check it.
// GT_DECOMP(0x08B28CD4u, gt::game::func_08B28CD4, gt::ReturnKind::Word);
// Not registered: it reaches a kernel import the isolated diff run cannot replay ("not comparable"
// in all 58 cases), so the gate can't check it.
// GT_DECOMP(0x08B28D04u, gt::game::func_08B28D04, gt::ReturnKind::Word);
GT_DECOMP(0x0895541Cu, gt::game::func_0895541C, gt::ReturnKind::Word);
GT_DECOMP(0x088D5910u, gt::game::func_088D5910, gt::ReturnKind::Word);
GT_DECOMP(0x08A76E88u, gt::game::func_08A76E88, gt::ReturnKind::Word);
GT_DECOMP(0x08A7D398u, gt::game::func_08A7D398, gt::ReturnKind::Word);
GT_DECOMP(0x0890B5F0u, gt::game::func_0890B5F0, gt::ReturnKind::Word);
GT_DECOMP(0x089675A0u, gt::game::func_089675A0, gt::ReturnKind::Word);

// --- batch 23 ---

namespace gt::game {
namespace {

// The byte at `offset` of the object at +0x8 (v1 keeps the object, as in the original).
std::uint32_t object8_byte(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t offset) {
    const std::uint32_t object = word(rt, ctx.gpr[4] + 0x8u);
    ctx.gpr[3] = object;
    return rt.memory().load8(object + offset);
}

// The queries of one class on its source at +0xC: `query(source)`, or `absent` without a source.
enum class SourceResult { Value, NonZero, IsTwo };
void source_query(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t query, std::uint32_t absent,
                  SourceResult shape) {
    const std::uint32_t source = word(rt, ctx.gpr[4] + 0xCu);
    std::uint32_t result = absent;
    if (source != 0u) {
        GuestFrame frame(ctx, 0x10u);
        const std::uint32_t value = call_guest(rt, ctx, query, {source});
        switch (shape) {
        case SourceResult::Value: result = value; break;
        case SourceResult::NonZero: result = value != 0u ? 1u : 0u; break;
        case SourceResult::IsTwo: result = value == 2u ? 1u : 0u; break;
        }
    }
    ctx.gpr[2] = result;
}

} // namespace

// func_088E71E8(link): func_088D09CC(the slot body func_088DF13C(link->owner, link->index)) when the
// link has an owner (+8).
void func_088E71E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t link = ctx.gpr[4];
    const std::uint32_t owner = word(rt, link + 0x8u);
    ctx.gpr[2] = 0u;
    if (owner == 0u) return;
    std::uint32_t body;
    {
        GuestFrame frame(ctx, 0x10u);
        body = call_guest(rt, ctx, kSlotBody, {owner, word(rt, link + 0x4u)});
    }
    tail_call(ctx, 0x088D09CCu, {body});
}

// Readers: func_08AB8940 (*this + 0x210), func_0889410C (*this + 0x4), func_08AB8718 (1),
// func_08917898 (the byte at +0x19), func_088D4FC0 / func_08AB8864 (the byte at +0x75 / +0x278 of
// *this), func_088D4C84 (the signed byte at +0x25 of *this), func_088D4D9C (whether the byte at
// +0x4F of *this is set).
void func_08AB8940(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = word(rt, ctx.gpr[4]) + 0x210u; }
void func_0889410C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = word(rt, ctx.gpr[4]) + 0x4u; }
void func_08AB8718(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = 1u; }
void func_08917898(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = rt.memory().load8(ctx.gpr[4] + 0x19u); }
void func_088D4FC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[3] = word(rt, ctx.gpr[4]);
    ctx.gpr[2] = rt.memory().load8(ctx.gpr[3] + 0x75u);
}
void func_08AB8864(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[3] = word(rt, ctx.gpr[4]);
    ctx.gpr[2] = rt.memory().load8(ctx.gpr[3] + 0x278u);
}
void func_088D4C84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[3] = word(rt, ctx.gpr[4]);
    ctx.gpr[2] = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(rt.memory().load8(ctx.gpr[3] + 0x25u))));
}
void func_088D4D9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[3] = word(rt, ctx.gpr[4]);
    ctx.gpr[2] = rt.memory().load8(ctx.gpr[3] + 0x4Fu) != 0u ? 1u : 0u;
}

// func_088FAE2C(this) -> func_08900DE8(second) or func_08903350(first), 0 with neither.
void func_088FAE2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_dispatch(rt, ctx, 0x08900DE8u, 0x08903350u); }

// func_088CE9A0(this): func_088DCDF8(object, the byte func_088D4FC0(object + 4)) for the object at +0x4.
void func_088CE9A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4] + 0x4u);
    std::uint32_t value;
    {
        GuestFrame frame(ctx, 0x10u);
        value = call_guest(rt, ctx, 0x088D4FC0u, {object + 0x4u});
    }
    tail_call(ctx, 0x088DCDF8u, {object, value});
}

// func_088CB4FC() -> the float at +0x60C of record 0 (func_088CB360(0)).
void func_088CB4FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t record = call_guest(rt, ctx, kRecords, {0u});
    ctx.gpr[2] = record;
    ctx.fpr[0] = float_at(rt, record + 0x60Cu);
}

// Queries on the source at +0xC: func_088A937C (func_08955208, else 0), func_088A9344 / func_088A930C /
// func_088A9264 / func_088A91F4 (whether func_08955894 / func_08955888 / func_0895589C / func_0895541C
// is set, else 0), func_088A92D0 (whether func_0895587C is 2, else 0), func_088A929C (func_08955760,
// else 1), func_088A91A0 (func_089554C8, else 49).
void func_088A937C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { source_query(rt, ctx, 0x08955208u, 0u, SourceResult::Value); }
void func_088A9344(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { source_query(rt, ctx, 0x08955894u, 0u, SourceResult::NonZero); }
void func_088A930C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { source_query(rt, ctx, 0x08955888u, 0u, SourceResult::NonZero); }
void func_088A9264(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { source_query(rt, ctx, 0x0895589Cu, 0u, SourceResult::NonZero); }
void func_088A91F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { source_query(rt, ctx, 0x0895541Cu, 0u, SourceResult::NonZero); }
void func_088A92D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { source_query(rt, ctx, 0x0895587Cu, 0u, SourceResult::IsTwo); }
void func_088A929C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { source_query(rt, ctx, 0x08955760u, 1u, SourceResult::Value); }
void func_088A91A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { source_query(rt, ctx, 0x089554C8u, 49u, SourceResult::Value); }

// func_0890638C(this, x): stores x at +0x154 of *this.
void func_0890638C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4]);
    ctx.gpr[2] = object;
    rt.memory().store32(object + 0x154u, std::bit_cast<std::uint32_t>(ctx.fpr[12]));
}

// func_08906498(this): when func_08906398(this) holds, func_089060C8 on the word at +0x48 of the object
// at +0x14 of *this.
void func_08906498(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t holds;
    {
        GuestFrame frame(ctx, 0x10u);
        holds = call_guest(rt, ctx, 0x08906398u, {self});
    }
    ctx.gpr[2] = holds;
    if (holds == 0u) return;
    const std::uint32_t object = word(rt, self);
    const std::uint32_t inner = word(rt, object + 0x14u);
    ctx.gpr[2] = object;
    ctx.gpr[3] = inner;
    tail_call(ctx, 0x089060C8u, {word(rt, inner + 0x48u)});
}

// func_08B20BA0(u16 header[13]): header = {28, 0 × 12} with header[4] = 1; returns 1.
void func_08B20BA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t header = ctx.gpr[4];
    rt.memory().store16(header, 28u);
    for (std::uint32_t i = 1u; i <= 12u; ++i) rt.memory().store16(header + i * 2u, 0u);
    rt.memory().store16(header + 0x8u, 1u);
    ctx.gpr[2] = 1u;
    ctx.gpr[3] = 12u;
}

// func_0895B59C(this) -> whether the byte at +0x52 of the object at +0xC is 7.
void func_0895B59C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4] + 0xCu);
    ctx.gpr[3] = object;
    ctx.gpr[2] = rt.memory().load8(object + 0x52u) == 7u ? 1u : 0u;
}

// Floats of the object at +0x8: func_089564FC (+0x720), func_08955984 (+0x5A4), func_08955978 (+0x534),
// func_0895596C (+0x538), func_08955960 (+0x530).
void func_089564FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { object8_float(rt, ctx, 0x720u); }
void func_08955984(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { object8_float(rt, ctx, 0x5A4u); }
void func_08955978(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { object8_float(rt, ctx, 0x534u); }
void func_0895596C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { object8_float(rt, ctx, 0x538u); }
void func_08955960(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { object8_float(rt, ctx, 0x530u); }

// func_0895589C(this) -> whether the float at +0x53C of the object at +0x8 is above 0.25 (NaN: no).
void func_0895589C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4] + 0x8u);
    const float value = float_at(rt, object + 0x53Cu);
    ctx.fpr[0] = 0.25f;
    ctx.fpr[1] = value;
    ctx.gpr[2] = 0.25f < value ? 1u : 0u;
    ctx.gpr[3] = ctx.gpr[2];
}

// Bytes of the object at +0x8: func_08955888 (+0x731), func_0895587C (+0x730), func_0895534C (+0x5F8),
// func_08955760 (whether +0x5F9 is 1).
void func_08955888(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = object8_byte(rt, ctx, 0x731u); }
void func_0895587C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = object8_byte(rt, ctx, 0x730u); }
void func_0895534C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = object8_byte(rt, ctx, 0x5F8u); }
void func_08955760(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = object8_byte(rt, ctx, 0x5F9u) == 1u ? 1u : 0u;
}

} // namespace gt::game

GT_DECOMP(0x088E71E8u, gt::game::func_088E71E8, gt::ReturnKind::Word);
GT_DECOMP(0x08AB8940u, gt::game::func_08AB8940, gt::ReturnKind::Word);
GT_DECOMP(0x0889410Cu, gt::game::func_0889410C, gt::ReturnKind::Word);
GT_DECOMP(0x08AB8718u, gt::game::func_08AB8718, gt::ReturnKind::Word);
GT_DECOMP(0x08917898u, gt::game::func_08917898, gt::ReturnKind::Word);
GT_DECOMP(0x088D4FC0u, gt::game::func_088D4FC0, gt::ReturnKind::Word);
GT_DECOMP(0x08AB8864u, gt::game::func_08AB8864, gt::ReturnKind::Word);
GT_DECOMP(0x088D4C84u, gt::game::func_088D4C84, gt::ReturnKind::Word);
GT_DECOMP(0x088D4D9Cu, gt::game::func_088D4D9C, gt::ReturnKind::Word);
GT_DECOMP(0x088FAE2Cu, gt::game::func_088FAE2C, gt::ReturnKind::Word);
GT_DECOMP(0x088CE9A0u, gt::game::func_088CE9A0, gt::ReturnKind::Word);
GT_DECOMP(0x088CB4FCu, gt::game::func_088CB4FC, gt::ReturnKind::Float);
GT_DECOMP(0x088A937Cu, gt::game::func_088A937C, gt::ReturnKind::Word);
GT_DECOMP(0x088A9344u, gt::game::func_088A9344, gt::ReturnKind::Word);
GT_DECOMP(0x088A930Cu, gt::game::func_088A930C, gt::ReturnKind::Word);
GT_DECOMP(0x088A9264u, gt::game::func_088A9264, gt::ReturnKind::Word);
GT_DECOMP(0x088A91F4u, gt::game::func_088A91F4, gt::ReturnKind::Word);
GT_DECOMP(0x088A92D0u, gt::game::func_088A92D0, gt::ReturnKind::Word);
GT_DECOMP(0x088A929Cu, gt::game::func_088A929C, gt::ReturnKind::Word);
GT_DECOMP(0x088A91A0u, gt::game::func_088A91A0, gt::ReturnKind::Word);
GT_DECOMP(0x0890638Cu, gt::game::func_0890638C, gt::ReturnKind::Word);
GT_DECOMP(0x08906498u, gt::game::func_08906498, gt::ReturnKind::Word);
GT_DECOMP(0x08B20BA0u, gt::game::func_08B20BA0, gt::ReturnKind::Word);
GT_DECOMP(0x0895B59Cu, gt::game::func_0895B59C, gt::ReturnKind::Word);
GT_DECOMP(0x089564FCu, gt::game::func_089564FC, gt::ReturnKind::Float);
GT_DECOMP(0x08955984u, gt::game::func_08955984, gt::ReturnKind::Float);
GT_DECOMP(0x08955978u, gt::game::func_08955978, gt::ReturnKind::Float);
GT_DECOMP(0x0895596Cu, gt::game::func_0895596C, gt::ReturnKind::Float);
GT_DECOMP(0x08955960u, gt::game::func_08955960, gt::ReturnKind::Float);
GT_DECOMP(0x0895589Cu, gt::game::func_0895589C, gt::ReturnKind::Word);
GT_DECOMP(0x08955888u, gt::game::func_08955888, gt::ReturnKind::Word);
GT_DECOMP(0x0895587Cu, gt::game::func_0895587C, gt::ReturnKind::Word);
GT_DECOMP(0x0895534Cu, gt::game::func_0895534C, gt::ReturnKind::Word);
GT_DECOMP(0x08955760u, gt::game::func_08955760, gt::ReturnKind::Word);

// --- batch 24 ---

namespace gt::game {
namespace {

constexpr std::uint32_t kGlobal71C0 = 0x08D471C0u;     // the global func_08AF7CB8 sets
constexpr std::uint32_t kGuard6DE0 = 0x08CE6DE0u;      // func_088CA28C runs only when this word is set
constexpr std::uint32_t kTarget3BD0 = 0x08CE3BD0u;     // ... on this object
constexpr std::uint32_t kGuardD208 = 0x08CDD208u;      // func_088C85C0 runs only when this word is set
constexpr std::uint32_t kTarget30D4 = 0x08CE30D4u;     // ... on this object
constexpr std::uint32_t kFiveFloats = 0x08CE2DF4u;     // the five floats func_088669B4 stores

std::uint32_t bswap32(std::uint32_t value) {
    return ((value & 0xFFu) << 24) | ((value & 0xFF00u) << 8) | ((value >> 8) & 0xFF00u) | (value >> 24);
}

// The halfword at `offset` of *holder, byte-swapped (a big-endian field).
void big_endian_half(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t offset) {
    const std::uint32_t object = word(rt, ctx.gpr[4]);
    ctx.gpr[3] = object;
    const std::uint32_t raw = rt.memory().load16(object + offset);
    ctx.gpr[2] = ((raw & 0xFFu) << 8) | (raw >> 8);
}

// Runs `target(object)` when the guard word is set (both are globals).
void guarded_global(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t guard, std::uint32_t object,
                    std::uint32_t target) {
    ctx.gpr[2] = object & 0xFFFF0000u;  // the original's lui, shared by both addresses
    ctx.gpr[3] = word(rt, guard);
    ctx.gpr[4] = object;
    if (ctx.gpr[3] != 0u) tail_call(ctx, target, {object});
}

// Stores x at `offset` and sets bit 20 of the flags at +0xE0.
void store_and_flag(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t offset) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t flags = word(rt, self + 0xE0u) | 0x100000u;
    rt.memory().store32(self + offset, std::bit_cast<std::uint32_t>(ctx.fpr[12]));
    rt.memory().store32(self + 0xE0u, flags);
    ctx.gpr[2] = flags;
    ctx.gpr[3] = 0x100000u;
}

} // namespace

// func_08955208(this) -> 0 when the float at +0x26C of the object at +0xC is 0.0, else 1 (NaN: 1).
void func_08955208(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4] + 0xCu);
    const float value = float_at(rt, object + 0x26Cu);
    ctx.fpr[0] = 0.0f;
    ctx.fpr[1] = value;
    ctx.gpr[2] = value == 0.0f ? 0u : 1u;
    ctx.gpr[3] = ctx.gpr[2];
}

// Readers: func_0894D040 (the byte at +0xC), func_0892B968 (the object at +0x260, plus 0x90),
// func_08AF2B64 (the float at +0x50 of *this), func_08ACF14C (the constant 0x3FE1E1E2, 1.7647...).
void func_0894D040(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = rt.memory().load8(ctx.gpr[4] + 0xCu); }
void func_0892B968(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = word(rt, ctx.gpr[4] + 0x260u) + 0x90u; }
void func_08AF2B64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4]);
    ctx.gpr[2] = object;
    ctx.fpr[0] = float_at(rt, object + 0x50u);
}
void func_08ACF14C(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.fpr[0] = std::bit_cast<float>(0x3FE1E1E2u); }

// func_0893ECA4(this) -> func_0893EB40(&the object at +0xC's +0x270).
void func_0893ECA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t target = word(rt, ctx.gpr[4] + 0xCu) + 0x270u;
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, 0x0893EB40u, {target});
}

// func_0892B538(a, b) -> whether the 32 bytes at a and b are equal (func_08B36170(a, b, 32) == 0).
void func_0892B538(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, 0x08B36170u, {ctx.gpr[4], ctx.gpr[5], 32u}) == 0u ? 1u : 0u;
}

// func_088D7F54(this, i) -> whether entry i (0x2F8 bytes, table at +0xC0) has its word at +0xC and its
// byte at +0x10 set.
void func_088D7F54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t entry = word(rt, ctx.gpr[4] + 0xC0u) + ctx.gpr[5] * 0x2F8u;
    ctx.gpr[2] = (word(rt, entry + 0xCu) != 0u && rt.memory().load8(entry + 0x10u) != 0u) ? 1u : 0u;
}

// func_088D1F68(counter, n): adds n to the word at +0x4 when the byte at +0x0 is set.
void func_088D1F68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t counter = ctx.gpr[4];
    const std::uint32_t enabled = rt.memory().load8(counter);
    ctx.gpr[2] = enabled;
    if (enabled == 0u) return;
    ctx.gpr[2] = word(rt, counter + 0x4u) + ctx.gpr[5];
    rt.memory().store32(counter + 0x4u, ctx.gpr[2]);
}

// func_088A3BAC(this, a, int *x, int *y): func_089444F8(the object at +0x18, a, x, y); without it
// *y = *x = 0 and 0 is returned.
void func_088A3BAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4] + 0x18u);
    if (object != 0u) {
        ctx.gpr[2] = object;
        tail_call(ctx, 0x089444F8u, {object});
        return;
    }
    ctx.gpr[4] = 0u;
    ctx.gpr[2] = 0u;
    rt.memory().store32(ctx.gpr[7], 0u);
    rt.memory().store32(ctx.gpr[6], 0u);
}

// Big-endian fields of *holder: func_088D5D34 (the word at +0x20), func_088D5D98 / func_088D5D88 (the
// halfwords at +0xC / +0xE), func_088D63D4 (the signed halfword at +0x8).
void func_088D5D34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4]);
    ctx.gpr[3] = object;
    ctx.gpr[2] = bswap32(word(rt, object + 0x20u));
}
void func_088D5D98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { big_endian_half(rt, ctx, 0xCu); }
void func_088D5D88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { big_endian_half(rt, ctx, 0xEu); }
void func_088D63D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4]);
    ctx.gpr[3] = object;
    ctx.gpr[2] = static_cast<std::uint32_t>(static_cast<std::int32_t>(bswap32(rt.memory().load16(object + 0x8u))) >> 16);
}

// func_088E74D8(link) -> the first float of the link's slot body (func_088DF13C), 0.0 without an owner.
void func_088E74D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t link = ctx.gpr[4];
    const std::uint32_t owner = word(rt, link + 0x8u);
    ctx.gpr[2] = owner;
    ctx.fpr[0] = 0.0f;
    if (owner == 0u) return;
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t body = call_guest(rt, ctx, kSlotBody, {owner, word(rt, link + 0x4u)});
    ctx.gpr[2] = body;
    ctx.fpr[0] = float_at(rt, body);
}

// func_08AF7CB8(value): the global at 0x08D471C0.
void func_08AF7CB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_global(ctx, rt, kGlobal71C0); }

// func_08AF4054(this, x): +0x8 = x, +0x4 = 0. func_08AF26C0(this, x): +0x50 of *this = x.
void func_08AF4054(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store32(ctx.gpr[4] + 0x8u, ctx.gpr[5]);
    rt.memory().store32(ctx.gpr[4] + 0x4u, 0u);
}
void func_08AF26C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4]);
    ctx.gpr[2] = object;
    rt.memory().store32(object + 0x50u, std::bit_cast<std::uint32_t>(ctx.fpr[12]));
}

// func_08AF2640(this) -> func_08C3F598(this + 4, the words at +0x4 and +0x8).
void func_08AF2640(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    ctx.gpr[2] = self;
    tail_call(ctx, 0x08C3F598u, {self + 0x4u, word(rt, self + 0x4u), word(rt, self + 0x8u)});
}

// func_08AF1308(this, float out[3]): the three floats at +0x314 of the object at +0x4.
void func_08AF1308(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    for (std::uint32_t i = 0; i < 3u; ++i) {
        const std::uint32_t object = word(rt, ctx.gpr[4] + 0x4u);
        ctx.gpr[2] = object;
        ctx.fpr[0] = float_at(rt, object + 0x314u + i * 4u);
        rt.memory().store32(ctx.gpr[5] + i * 4u, std::bit_cast<std::uint32_t>(ctx.fpr[0]));
    }
}

// func_0890B56C(this): func_08AF1490 on the members at +0x20, +0x10 and +0x28.
void func_0890B56C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08AF1490u, {word(rt, self + 0x20u)});
        (void)call_guest(rt, ctx, 0x08AF1490u, {word(rt, self + 0x10u)});
    }
    tail_call(ctx, 0x08AF1490u, {word(rt, self + 0x28u)});
}

// func_088F1B78(this, x): func_088F5EA0(&this->part (+0x844), x, the byte at +0x9CC), then
// func_088F5F08(&this->part, x).
void func_088F1B78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t part = ctx.gpr[4] + 0x844u;
    const std::uint32_t x = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x088F5EA0u, {part, x, rt.memory().load8(ctx.gpr[4] + 0x9CCu)});
    }
    tail_call(ctx, 0x088F5F08u, {part, x});
}

// func_088CA28C / func_088C85C0(): a global object's update, when its guard word is set.
void func_088CA28C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_global(rt, ctx, kGuard6DE0, kTarget3BD0, 0x088C9F48u); }
void func_088C85C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_global(rt, ctx, kGuardD208, kTarget30D4, 0x088C8330u); }

// func_088BBC40(this) -> the virtual +0x18 of the object at +0x1C, with the byte at +0x10 of the
// object at +0x24.
void func_088BBC40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4] + 0x1Cu);
    const std::uint32_t value = rt.memory().load8(word(rt, ctx.gpr[4] + 0x24u) + 0x10u);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, word(rt, word(rt, object) + 0x18u), {object, value});
}

// func_088669B4(a, b, c, d, e): stores the five floats at 0x08CE2DF4.
void func_088669B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    for (std::uint32_t i = 0; i < 5u; ++i)
        rt.memory().store32(kFiveFloats + i * 4u, std::bit_cast<std::uint32_t>(ctx.fpr[12 + i]));
    ctx.gpr[2] = kFiveFloats;
    ctx.gpr[3] = kFiveFloats & 0xFFFF0000u;
}

// func_08893730(holder, i) -> *holder + 1000 * i + 332 * (signed byte at +0x1 of *holder) + 0x38.
void func_08893730(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t base = word(rt, ctx.gpr[4]);
    const auto row = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(rt.memory().load8(base + 0x1u))));
    ctx.gpr[2] = ctx.gpr[5] * 1000u + row * 332u + base + 0x38u;
}

// func_088D9C50(this) -> func_088E5DC4(the object at +0x640), 0 without it.
void func_088D9C50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4] + 0x640u);
    ctx.gpr[2] = 0u;
    ctx.gpr[4] = object;
    if (object != 0u) tail_call(ctx, 0x088E5DC4u, {object});
}

// func_088D2AB0(-, kind) -> 0 for kinds 1, 3, 6 and 9, else 1.
void func_088D2AB0(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t kind = ctx.gpr[5];
    ctx.gpr[2] = (kind == 1u || kind == 3u || kind == 6u || kind == 9u) ? 0u : 1u;
}

// func_08A69520(size) -> func_08A5BC20(size, 4).
void func_08A69520(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x08A5BC20u, {ctx.gpr[4], 4u}); }

// func_08AA64DC(this) -> the byte func_08AA61D8(this, &byte, 1) reads.
void func_08AA64DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    (void)call_guest(rt, ctx, 0x08AA61D8u, {ctx.gpr[4], frame.at(0u), 1u});
    ctx.gpr[2] = rt.memory().load8(frame.at(0u));
}

// func_0897D7FC(this) -> func_089C3DF0(this + 0x14).
void func_0897D7FC(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x089C3DF0u, {ctx.gpr[4] + 0x14u}); }

// func_0896855C / func_08967510(this, x): stores x at +0xA8 / +0xC4 and sets bit 20 of +0xE0.
void func_0896855C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_and_flag(rt, ctx, 0xA8u); }
void func_08967510(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_and_flag(rt, ctx, 0xC4u); }

// func_089BA59C(this): when func_08A76E88(this) is 0, sets the low two bits of +0x60 to 1.
void func_089BA59C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t result;
    {
        GuestFrame frame(ctx, 0x10u);
        result = call_guest(rt, ctx, 0x08A76E88u, {self});
    }
    ctx.gpr[3] = 1u;
    if (result == 0u) {
        result = (word(rt, self + 0x60u) & ~3u) | 1u;
        rt.memory().store32(self + 0x60u, result);
    }
    ctx.gpr[2] = result;
}

// func_08A17C48(this) -> func_08A17B28(this), after which the words at +0x4..+0x14 are cleared.
void func_08A17C48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t result;
    {
        GuestFrame frame(ctx, 0x10u);
        result = call_guest(rt, ctx, 0x08A17B28u, {self});
    }
    for (const std::uint32_t offset : {0x14u, 0x4u, 0x8u, 0xCu, 0x10u}) rt.memory().store32(self + offset, 0u);
    ctx.gpr[2] = result;
}

} // namespace gt::game

GT_DECOMP(0x08955208u, gt::game::func_08955208, gt::ReturnKind::Word);
GT_DECOMP(0x0894D040u, gt::game::func_0894D040, gt::ReturnKind::Word);
GT_DECOMP(0x0892B968u, gt::game::func_0892B968, gt::ReturnKind::Word);
GT_DECOMP(0x08AF2B64u, gt::game::func_08AF2B64, gt::ReturnKind::Float);
GT_DECOMP(0x08ACF14Cu, gt::game::func_08ACF14C, gt::ReturnKind::Float);
GT_DECOMP(0x0893ECA4u, gt::game::func_0893ECA4, gt::ReturnKind::Word);
GT_DECOMP(0x0892B538u, gt::game::func_0892B538, gt::ReturnKind::Word);
GT_DECOMP(0x088D7F54u, gt::game::func_088D7F54, gt::ReturnKind::Word);
GT_DECOMP(0x088D1F68u, gt::game::func_088D1F68, gt::ReturnKind::Word);
GT_DECOMP(0x088A3BACu, gt::game::func_088A3BAC, gt::ReturnKind::Word);
GT_DECOMP(0x088D5D34u, gt::game::func_088D5D34, gt::ReturnKind::Word);
GT_DECOMP(0x088D5D98u, gt::game::func_088D5D98, gt::ReturnKind::Word);
GT_DECOMP(0x088D5D88u, gt::game::func_088D5D88, gt::ReturnKind::Word);
GT_DECOMP(0x088D63D4u, gt::game::func_088D63D4, gt::ReturnKind::Word);
GT_DECOMP(0x088E74D8u, gt::game::func_088E74D8, gt::ReturnKind::Float);
GT_DECOMP(0x08AF7CB8u, gt::game::func_08AF7CB8, gt::ReturnKind::Word);
GT_DECOMP(0x08AF4054u, gt::game::func_08AF4054, gt::ReturnKind::Void);
GT_DECOMP(0x08AF26C0u, gt::game::func_08AF26C0, gt::ReturnKind::Word);
GT_DECOMP(0x08AF2640u, gt::game::func_08AF2640, gt::ReturnKind::Word);
GT_DECOMP(0x08AF1308u, gt::game::func_08AF1308, gt::ReturnKind::Word);
GT_DECOMP(0x0890B56Cu, gt::game::func_0890B56C, gt::ReturnKind::Word);
GT_DECOMP(0x088F1B78u, gt::game::func_088F1B78, gt::ReturnKind::Word);
GT_DECOMP(0x088CA28Cu, gt::game::func_088CA28C, gt::ReturnKind::Word);
GT_DECOMP(0x088C85C0u, gt::game::func_088C85C0, gt::ReturnKind::Word);
GT_DECOMP(0x088BBC40u, gt::game::func_088BBC40, gt::ReturnKind::Word);
GT_DECOMP(0x088669B4u, gt::game::func_088669B4, gt::ReturnKind::Word);
GT_DECOMP(0x08893730u, gt::game::func_08893730, gt::ReturnKind::Word);
GT_DECOMP(0x088D9C50u, gt::game::func_088D9C50, gt::ReturnKind::Word);
GT_DECOMP(0x088D2AB0u, gt::game::func_088D2AB0, gt::ReturnKind::Word);
GT_DECOMP(0x08A69520u, gt::game::func_08A69520, gt::ReturnKind::Word);
GT_DECOMP(0x08AA64DCu, gt::game::func_08AA64DC, gt::ReturnKind::Word);
GT_DECOMP(0x0897D7FCu, gt::game::func_0897D7FC, gt::ReturnKind::Word);
GT_DECOMP(0x0896855Cu, gt::game::func_0896855C, gt::ReturnKind::Word);
GT_DECOMP(0x08967510u, gt::game::func_08967510, gt::ReturnKind::Word);
GT_DECOMP(0x089BA59Cu, gt::game::func_089BA59C, gt::ReturnKind::Word);
GT_DECOMP(0x08A17C48u, gt::game::func_08A17C48, gt::ReturnKind::Word);

// --- batch 25 ---

namespace gt::game {
namespace {

constexpr std::uint32_t kFlags8D5C = 0x08D68D5Cu;    // the word func_08B2DE00 tests
constexpr std::uint32_t kFallbackA87C = 0x08C8A87Cu; // the word func_0891C6F4 falls back to
constexpr std::uint32_t kTable0E58 = 0x08C90E58u;    // the table func_08ADBD58 searches
constexpr std::uint32_t kEntries1B6C = 0x08D41B6Cu;  // the entry array (0x38 bytes each) of func_08AEF1A8

// The style bits of one class: 0 or 1 without func_08966174(this), else bits of the word at +0x30 of
// the object at +0x80.
void style_bits(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t without, unsigned shift, std::uint32_t mask) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t has;
    {
        GuestFrame frame(ctx, 0x10u);
        has = call_guest(rt, ctx, 0x08966174u, {self});
    }
    std::uint32_t result = without;
    if (has != 0u) result = (word(rt, word(rt, self + 0x80u) + 0x30u) >> shift) & mask;
    ctx.gpr[2] = result;
}

// Sets the low two bits of +0x60 to `mode` when func_08A76E88(this) is 0 (as func_089BA59C).
void set_mode(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t mode) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t result;
    {
        GuestFrame frame(ctx, 0x10u);
        result = call_guest(rt, ctx, 0x08A76E88u, {self});
    }
    ctx.gpr[3] = mode;
    if (result == 0u) {
        result = (word(rt, self + 0x60u) & ~3u) | mode;
        rt.memory().store32(self + 0x60u, result);
    }
    ctx.gpr[2] = result;
}

// func_08A7805C(this) when the low two bits of +0x60 are `mode`.
void when_mode(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t mode) {
    ctx.gpr[2] = word(rt, ctx.gpr[4] + 0x60u) & 3u;
    ctx.gpr[3] = mode;
    if (ctx.gpr[2] == mode) tail_call(ctx, 0x08A7805Cu, {ctx.gpr[4]});
}

// func_08AA634C(this, out) stored at *out; returns this.
void store_result(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t out = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    rt.memory().store32(out, call_guest(rt, ctx, 0x08AA634Cu, {self, out}));
    ctx.gpr[2] = self;
}

} // namespace

void func_08967064(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { style_bits(rt, ctx, 1u, 3u, 1u); }
void func_08966FEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { style_bits(rt, ctx, 1u, 1u, 1u); }
void func_08967028(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { style_bits(rt, ctx, 1u, 2u, 1u); }
void func_08966FB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { style_bits(rt, ctx, 1u, 0u, 1u); }
void func_089670A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { style_bits(rt, ctx, 0u, 4u, 3u); }

void func_089BA5F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { set_mode(rt, ctx, 2u); }
void func_089BA654(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { set_mode(rt, ctx, 3u); }
void func_089BA5D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { when_mode(rt, ctx, 1u); }
void func_089BA630(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { when_mode(rt, ctx, 2u); }

// func_08AA63CC / func_08AA6394(this, out) -> this, with *out = func_08AA634C(this, out).
void func_08AA63CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_result(rt, ctx); }
void func_08AA6394(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_result(rt, ctx); }

// Constants and small readers: func_088C4A30 (2.0), func_08A72780(stack, i) (&items[i]),
// func_089A8E94 (the float at +0x110 times 60).
void func_088C4A30(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.fpr[0] = 2.0f; }
void func_08A72780(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = word(rt, ctx.gpr[4]) + (ctx.gpr[5] << 2); }
void func_089A8E94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.fpr[0] = psp_mul(float_at(rt, ctx.gpr[4] + 0x110u), 60.0f);
}

// func_088B5D38(a, b) -> whether a's float at +0x44 is below b's (a sort predicate).
void func_088B5D38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.fpr[1] = float_at(rt, ctx.gpr[4] + 0x44u);
    ctx.fpr[0] = float_at(rt, ctx.gpr[5] + 0x44u);
    ctx.gpr[2] = ctx.fpr[1] < ctx.fpr[0] ? 1u : 0u;
}

// func_08B2DE00() -> whether any of bits 16, 17 and 19 of the word at 0x08D68D5C is set.
void func_08B2DE00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[3] = 0x08D70000u;
    ctx.gpr[4] = 0xB0000u;
    ctx.gpr[2] = (word(rt, kFlags8D5C) & 0xB0000u) != 0u ? 1u : 0u;
}

// func_088D4F00(holder, which) -> the byte at +0x73 (which == 1) or +0x6F of *holder, as a float.
void func_088D4F00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4]);
    const std::uint32_t value = rt.memory().load8(object + (ctx.gpr[5] == 1u ? 0x73u : 0x6Fu));
    ctx.gpr[2] = value;
    ctx.fpr[0] = static_cast<float>(static_cast<std::int32_t>(value));
}

// func_08A4D738(this, a, b): +0x28 = a, +0x2C = the float at +0x24 × 11.5 + b, clearing bit 8 of +0x58.
void func_08A4D738(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const float scaled = psp_mul(float_at(rt, self + 0x24u), 11.5f) + ctx.fpr[13];
    const std::uint32_t flags = word(rt, self + 0x58u) & ~0x100u;
    rt.memory().store32(self + 0x28u, std::bit_cast<std::uint32_t>(ctx.fpr[12]));
    rt.memory().store32(self + 0x58u, flags);
    rt.memory().store32(self + 0x2Cu, std::bit_cast<std::uint32_t>(scaled));
    ctx.gpr[2] = flags;
}

// func_088BB5C4(this, a, b) -> a + (the u16 at +0x68 / 32768) × (b - a): a 1.15 fixed-point blend.
void func_088BB5C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t raw = rt.memory().load16(ctx.gpr[4] + 0x68u);
    const float a = ctx.fpr[12];
    const float span = ctx.fpr[13] - a;
    ctx.gpr[4] = raw;
    ctx.fpr[13] = span;
    ctx.fpr[1] = std::bit_cast<float>(0x38000000u);
    ctx.fpr[0] = a + psp_mul(psp_mul(static_cast<float>(static_cast<std::int32_t>(raw)), std::bit_cast<float>(0x38000000u)), span);
}

// func_08A66060() -> a new 8-byte object constructed by func_08A6603C.
void func_08A66060(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t object = call_guest(rt, ctx, kAllocate, {8u});
    (void)call_guest(rt, ctx, 0x08A6603Cu, {object});
    ctx.gpr[2] = object;
}

// func_089273B4(this) -> 1 when neither byte at +0x1FBC nor +0x1FBD is set.
void func_089273B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    ctx.gpr[2] = (rt.memory().load8(self + 0x1FBCu) == 0u && rt.memory().load8(self + 0x1FBDu) == 0u) ? 1u : 0u;
}

// func_0891C6F4(this) -> 1 when func_089273B4(the object at +0x1E0) holds, else the word at 0x08C8A87C.
void func_0891C6F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t result;
    {
        GuestFrame frame(ctx, 0x10u);
        result = call_guest(rt, ctx, 0x089273B4u, {word(rt, ctx.gpr[4] + 0x1E0u)});
    }
    ctx.gpr[2] = result != 0u ? 1u : word(rt, kFallbackA87C);
}

// func_08B2A908(bitset, i) -> bit i of the word array at +0x4, most significant bit first.
void func_08B2A908(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t i = ctx.gpr[5];
    const std::uint32_t bits = word(rt, word(rt, ctx.gpr[4] + 0x4u) + ((i >> 5) << 2));
    ctx.gpr[2] = (bits & (1u << (~i & 31u))) != 0u ? 1u : 0u;
}

// Forwarders: func_0893656C(-, -, a, b) -> func_089304C0(a, b); func_08A618AC(this, x): func_08AA6404 on
// (x, this + 8) and (x, this + 12); func_08A06F7C(this): func_089FB570(this + 0x10C), then
// func_08A06ED8(this); func_08A004D0(this): func_089675A0(this), then func_089B8BA0(this + 0xE8);
// func_0896A824(this, b): func_0896A7B0(this, b & 0xFF), then func_089689C8(this, b & 0xFF).
void func_0893656C(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x089304C0u, {ctx.gpr[6], ctx.gpr[7]}); }
void func_08A618AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t x = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08AA6404u, {x, self + 0x8u});
    }
    tail_call(ctx, 0x08AA6404u, {x, self + 0xCu});
}
void func_08A06F7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x089FB570u, {self + 0x10Cu});
    }
    tail_call(ctx, 0x08A06ED8u, {self});
}
void func_08A004D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x089675A0u, {self});
    }
    tail_call(ctx, 0x089B8BA0u, {self + 0xE8u});
}
void func_0896A824(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t value = ctx.gpr[5] & 0xFFu;
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x0896A7B0u, {self, value});
    }
    tail_call(ctx, 0x089689C8u, {self, value});
}

// func_08A75A14(this, other) -> this, after copying the counted pointer at other's +0x4 minus 4
// (func_08A6972C).
void func_08A75A14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t from = word(rt, ctx.gpr[5] + 0x4u) - 4u;
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08A6972Cu, {self, from});
    }
    ctx.gpr[2] = self;
}

// func_08A72BD8(array, i, const holder *h) -> &items[i] after storing *h there (func_08A5BAD8),
// unless it is h itself.
void func_08A72BD8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t item = word(rt, ctx.gpr[4]) + (ctx.gpr[5] << 2);
    const std::uint32_t source = ctx.gpr[6];
    if (item != source) {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, kHolderResetGame, {item, word(rt, source)});
    }
    ctx.gpr[2] = item;
}

// func_08966040(this, mode): bits 21-22 of the flags at +0xE0 (the field func_08965BD4 reads).
void func_08966040(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t flags = (word(rt, ctx.gpr[4] + 0xE0u) & ~0x00600000u) | ((ctx.gpr[5] & 3u) << 21);
    rt.memory().store32(ctx.gpr[4] + 0xE0u, flags);
    ctx.gpr[2] = flags;
}

// func_08ADBD58(key) -> the entry func_08ADB278(key, the table at 0x08C90E58) finds, plus 0x19C; 0 when
// not found.
void func_08ADBD58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t found = call_guest(rt, ctx, 0x08ADB278u, {ctx.gpr[4], kTable0E58});
    ctx.gpr[3] = found;
    ctx.gpr[2] = found != 0u ? found + 0x19Cu : 0u;
}

// func_08AEF1A8(i): clears bit 0 of the byte at +0x4 of entry i (0x38 bytes) of the array at 0x08D41B6C.
void func_08AEF1A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t entry = word(rt, kEntries1B6C) + ctx.gpr[4] * 0x38u;
    const std::uint32_t value = rt.memory().load8(entry + 0x4u) & ~1u;
    rt.memory().store8(entry + 0x4u, static_cast<std::uint8_t>(value));
    ctx.gpr[2] = ctx.gpr[4] * 0x38u;
    ctx.gpr[3] = value;
    ctx.gpr[5] = entry;
}

// func_08A6191C(this, x): the word at +0x8 = func_08AA64DC(x).
void func_08A6191C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t value = call_guest(rt, ctx, 0x08AA64DCu, {ctx.gpr[5]});
    rt.memory().store32(self + 0x8u, value);
    ctx.gpr[2] = value;
}

// func_0894BF60(this, x) -> func_089468C0(this, x), after func_08958324 on a local 0x20-byte block.
void func_0894BF60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const float x = ctx.fpr[12];
    GuestFrame frame(ctx, 0x40u);
    (void)call_guest(rt, ctx, 0x08958324u, {frame.at(0u)}, {x});
    const GuestResult result = call_guest(rt, ctx, 0x089468C0u, {self}, {x});
    ctx.gpr[2] = result.v0;
    ctx.fpr[0] = result.f0;
}

// func_08AFAA78(this) -> this + 0x4C, after setting the range object at +0x2C to [+0x4, +0x8).
void func_08AFAA78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    (void)call_guest(rt, ctx, kSetRange, {self + 0x2Cu, word(rt, self + 0x4u), word(rt, self + 0x8u)});
    ctx.gpr[2] = self + 0x4Cu;
}

// func_08A16EA4 / func_08A16CFC(this): clear four / two words.
void func_08A16EA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    for (const std::uint32_t offset : {0xCu, 0x0u, 0x4u, 0x8u}) rt.memory().store32(ctx.gpr[4] + offset, 0u);
}
void func_08A16CFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store32(ctx.gpr[4] + 0x4u, 0u);
    rt.memory().store32(ctx.gpr[4], 0u);
}

} // namespace gt::game

GT_DECOMP(0x08967064u, gt::game::func_08967064, gt::ReturnKind::Word);
GT_DECOMP(0x08966FECu, gt::game::func_08966FEC, gt::ReturnKind::Word);
GT_DECOMP(0x08967028u, gt::game::func_08967028, gt::ReturnKind::Word);
GT_DECOMP(0x08966FB0u, gt::game::func_08966FB0, gt::ReturnKind::Word);
GT_DECOMP(0x089670A0u, gt::game::func_089670A0, gt::ReturnKind::Word);
GT_DECOMP(0x089BA5F8u, gt::game::func_089BA5F8, gt::ReturnKind::Word);
GT_DECOMP(0x089BA654u, gt::game::func_089BA654, gt::ReturnKind::Word);
GT_DECOMP(0x089BA5D4u, gt::game::func_089BA5D4, gt::ReturnKind::Word);
GT_DECOMP(0x089BA630u, gt::game::func_089BA630, gt::ReturnKind::Word);
GT_DECOMP(0x08AA63CCu, gt::game::func_08AA63CC, gt::ReturnKind::Word);
GT_DECOMP(0x08AA6394u, gt::game::func_08AA6394, gt::ReturnKind::Word);
GT_DECOMP(0x088C4A30u, gt::game::func_088C4A30, gt::ReturnKind::Float);
GT_DECOMP(0x08A72780u, gt::game::func_08A72780, gt::ReturnKind::Word);
GT_DECOMP(0x089A8E94u, gt::game::func_089A8E94, gt::ReturnKind::Float);
GT_DECOMP(0x088B5D38u, gt::game::func_088B5D38, gt::ReturnKind::Word);
GT_DECOMP(0x08B2DE00u, gt::game::func_08B2DE00, gt::ReturnKind::Word);
GT_DECOMP(0x088D4F00u, gt::game::func_088D4F00, gt::ReturnKind::Float);
GT_DECOMP(0x08A4D738u, gt::game::func_08A4D738, gt::ReturnKind::Void);
GT_DECOMP(0x088BB5C4u, gt::game::func_088BB5C4, gt::ReturnKind::Float);
GT_DECOMP(0x08A66060u, gt::game::func_08A66060, gt::ReturnKind::Word);
GT_DECOMP(0x089273B4u, gt::game::func_089273B4, gt::ReturnKind::Word);
GT_DECOMP(0x0891C6F4u, gt::game::func_0891C6F4, gt::ReturnKind::Word);
// Not registered: the harness never sees a call to it on the three pad routes ("calls 0"), so it is
// untested.
// GT_DECOMP(0x08B2A908u, gt::game::func_08B2A908, gt::ReturnKind::Word);
GT_DECOMP(0x0893656Cu, gt::game::func_0893656C, gt::ReturnKind::Word);
GT_DECOMP(0x08A618ACu, gt::game::func_08A618AC, gt::ReturnKind::Word);
GT_DECOMP(0x08A06F7Cu, gt::game::func_08A06F7C, gt::ReturnKind::Word);
GT_DECOMP(0x08A004D0u, gt::game::func_08A004D0, gt::ReturnKind::Word);
GT_DECOMP(0x0896A824u, gt::game::func_0896A824, gt::ReturnKind::Word);
GT_DECOMP(0x08A75A14u, gt::game::func_08A75A14, gt::ReturnKind::Word);
GT_DECOMP(0x08A72BD8u, gt::game::func_08A72BD8, gt::ReturnKind::Word);
GT_DECOMP(0x08966040u, gt::game::func_08966040, gt::ReturnKind::Word);
GT_DECOMP(0x08ADBD58u, gt::game::func_08ADBD58, gt::ReturnKind::Word);
GT_DECOMP(0x08AEF1A8u, gt::game::func_08AEF1A8, gt::ReturnKind::Word);
GT_DECOMP(0x08A6191Cu, gt::game::func_08A6191C, gt::ReturnKind::Word);
GT_DECOMP(0x0894BF60u, gt::game::func_0894BF60, gt::ReturnKind::Word);
GT_DECOMP(0x08AFAA78u, gt::game::func_08AFAA78, gt::ReturnKind::Word);
GT_DECOMP(0x08A16EA4u, gt::game::func_08A16EA4, gt::ReturnKind::Void);
GT_DECOMP(0x08A16CFCu, gt::game::func_08A16CFC, gt::ReturnKind::Void);

// --- batch 26 ---

namespace gt::game {
namespace {

constexpr std::uint32_t kVtableAA88 = 0x08CBAA88u;  // the vtables the constructors below install
constexpr std::uint32_t kVtableA830 = 0x08CBA830u;
constexpr std::uint32_t kVtable2318 = 0x08CD2318u;
constexpr std::uint32_t kVtableF440 = 0x08CCF440u;
constexpr std::uint32_t kVtable28E0 = 0x08CD28E0u;

// Releases the handle at +0x98 (via `release`) when it is valid (>= 0) and marks it -1.
void release_handle(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t release) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t handle = word(rt, self + 0x98u);
    ctx.gpr[2] = handle;
    if (static_cast<std::int32_t>(handle) < 0) return;
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, release, {handle});
    }
    rt.memory().store32(self + 0x98u, 0xFFFFFFFFu);
    ctx.gpr[2] = 0xFFFFFFFFu;
}

} // namespace

void func_089BA68C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { when_mode(rt, ctx, 3u); }

// func_08A17BD0(this): func_08A17B28(this), clears +0x4..+0xC, then the words at +0x10 and +0xC of *this.
void func_08A17BD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08A17B28u, {self});
    }
    const std::uint32_t first = word(rt, self);
    for (const std::uint32_t offset : {0x4u, 0x8u, 0xCu}) rt.memory().store32(self + offset, 0u);
    rt.memory().store32(first + 0x10u, 0u);
    const std::uint32_t second = word(rt, self);
    rt.memory().store32(second + 0xCu, 0u);
    ctx.gpr[2] = second;
    ctx.gpr[3] = first;
}

// func_08A17B68(holder, target, a, b): func_08966CF0(target, a, b), with a and b swapped when **holder
// is set.
void func_08A17B68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4]);
    const std::uint32_t inner = word(rt, object);
    const float a = ctx.fpr[12];
    ctx.fpr[1] = a;
    ctx.gpr[2] = object;
    ctx.gpr[3] = inner;
    if (inner != 0u) {
        ctx.fpr[12] = ctx.fpr[13];
        ctx.fpr[13] = a;
    }
    tail_call(ctx, 0x08966CF0u, {ctx.gpr[5]});
}

// func_08A7ADD0(this) -> func_08A5D50C(this + 0x1C).
void func_08A7ADD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t target = ctx.gpr[4] + 0x1Cu;
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, 0x08A5D50Cu, {target});
}

// func_08ABBEF8(holder, u8 value): the virtual +0x78 of *holder (object, holder, value) when both are set.
void func_08ABBEF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t holder = ctx.gpr[4];
    const std::uint32_t value = ctx.gpr[5] & 0xFFu;
    ctx.gpr[6] = value;
    if (holder == 0u) return;
    const std::uint32_t object = word(rt, holder);
    ctx.gpr[2] = object;
    if (object == 0u) return;
    ctx.gpr[2] = word(rt, object);
    tail_call(ctx, word(rt, word(rt, object) + 0x78u), {object, holder, value});
}

// func_089A1628(this): func_089D8EB8(this) unless the record func_08BF4BE8(this) returns has its word
// at +0x14 set.
void func_089A1628(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t record;
    {
        GuestFrame frame(ctx, 0x10u);
        record = call_guest(rt, ctx, 0x08BF4BE8u, {self});
    }
    ctx.gpr[2] = record;
    ctx.gpr[4] = self;
    if (word(rt, record + 0x14u) == 0u) tail_call(ctx, 0x089D8EB8u, {self});
}

// func_08A9A414(this, list, i) -> this, after func_08A76ABC(this, &list->items[i]) (items at +0x14).
void func_08A9A414(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t item = word(rt, ctx.gpr[5] + 0x14u) + (ctx.gpr[6] << 2);
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08A76ABCu, {self, item});
    }
    ctx.gpr[2] = self;
}

// func_08AE2B40 / func_08AE28A4(this): release the handle at +0x98 through func_08AE2FE8 / func_08AE6B34.
void func_08AE2B40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { release_handle(rt, ctx, 0x08AE2FE8u); }
void func_08AE28A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { release_handle(rt, ctx, 0x08AE6B34u); }

// func_08B23778(path) -> path past its leading '/' characters; func_08B237A0(path) -> the next '/' or
// the terminator.
void func_08B23778(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t p = ctx.gpr[4];
    while (rt.memory().load8(p) == '/') ++p;
    ctx.gpr[3] = '/';
    ctx.gpr[2] = p;
}
void func_08B237A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t p = ctx.gpr[4];
    const std::uint32_t first = rt.memory().load8(p);
    ctx.gpr[3] = first == 0u ? 0u : static_cast<std::uint32_t>('/');
    if (first != 0u && first != '/') {
        ++p;
        while (rt.memory().load8(p) != 0u && rt.memory().load8(p) != '/') ++p;
    }
    ctx.gpr[2] = p;
}

// func_08AFAC7C(this): func_08AFAC58(this), then func_08B52C84(this + 0x2C).
void func_08AFAC7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08AFAC58u, {self});
    }
    tail_call(ctx, 0x08B52C84u, {self + 0x2Cu});
}

// Constructors: func_08A0DD40 (vtables at +0x38 and +0x0, then func_08A0298C), func_08A5B860 (vtable,
// two cleared words), func_089D76A8 (vtable, 1.0 at +0x14, six cleared words), func_08A64AEC (vtable,
// func_08A64554, then func_08A5BB84(this, 12)).
void func_08A0DD40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store32(ctx.gpr[4] + 0x38u, kVtableAA88);
    rt.memory().store32(ctx.gpr[4], kVtableA830);
    ctx.gpr[3] = kVtableAA88;
    ctx.gpr[5] = kVtableA830;
    tail_call(ctx, 0x08A0298Cu, {ctx.gpr[4]});
}
void func_08A5B860(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store32(ctx.gpr[4] + 0x4u, 0u);
    rt.memory().store32(ctx.gpr[4], kVtable2318);
    rt.memory().store32(ctx.gpr[4] + 0x8u, 0u);
    ctx.gpr[2] = kVtable2318;
}
void func_089D76A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    rt.memory().store32(self, kVtableF440);
    rt.memory().store32(self + 0x14u, std::bit_cast<std::uint32_t>(1.0f));
    for (const std::uint32_t offset : {0x1Cu, 0x4u, 0x8u, 0xCu, 0x10u, 0x18u}) rt.memory().store32(self + offset, 0u);
    ctx.gpr[2] = kVtableF440;
    ctx.fpr[0] = 1.0f;
}
void func_08A64AEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    rt.memory().store32(self, kVtable28E0);
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08A64554u, {self});
    }
    tail_call(ctx, 0x08A5BB84u, {self, 12u});
}

// func_08AA65A8(this, bool *out) -> this, with *out = (func_08AA64DC(this) != 0).
void func_08AA65A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t out = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    rt.memory().store8(out, call_guest(rt, ctx, 0x08AA64DCu, {self}) != 0u ? 1u : 0u);
    ctx.gpr[2] = self;
}

// func_088FB560(this, n): adds n to the word at +0x8 when it is set.
void func_088FB560(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t value = word(rt, ctx.gpr[4] + 0x8u);
    ctx.gpr[5] += value;
    ctx.gpr[2] = value;
    if (value != 0u) rt.memory().store32(ctx.gpr[4] + 0x8u, ctx.gpr[5]);
}

// func_08AF6CE8(this) -> whether the first word is the tag "3SXt" or "3SXT" (0x74585333 / 0x54585333).
void func_08AF6CE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t tag = word(rt, ctx.gpr[4]);
    ctx.gpr[3] = 0xABA7ACCDu;
    ctx.gpr[5] = (tag + 0xABA7ACCDu) == 0u ? 1u : 0u;
    ctx.gpr[2] = ((tag + 0x8BA7ACCDu) == 0u ? 1u : 0u) | ctx.gpr[5];
}

// func_08B20A54(a, b, c) -> the word func_08B209C4(a, b, &word) leaves in a local initialised to c.
void func_08B20A54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    rt.memory().store32(frame.at(0u), ctx.gpr[6]);
    (void)call_guest(rt, ctx, 0x08B209C4u, {ctx.gpr[4], ctx.gpr[5], frame.at(0u)});
    ctx.gpr[2] = word(rt, frame.at(0u));
}

// func_08B20920(this, x): +0x0 = x, +0x4 = func_08B208C8(x).
void func_08B20920(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t x = ctx.gpr[5];
    rt.memory().store32(self, x);
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t value = call_guest(rt, ctx, 0x08B208C8u, {x});
    rt.memory().store32(self + 0x4u, value);
    ctx.gpr[2] = value;
}

// func_08AE0968(key) -> the word at +0xC of the entry func_08ADDCD4(key, 0) finds, 0 when none.
void func_08AE0968(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t entry = call_guest(rt, ctx, 0x08ADDCD4u, {ctx.gpr[4], 0u});
    ctx.gpr[2] = entry != 0u ? word(rt, entry + 0xCu) : 0u;
}

// func_08AD5624(this) -> bit 1 of func_08AE0968(the handle at +0x10), 0 for a negative handle;
// func_08AC9470(-, obj) -> that for the object at obj's +0x4, as a byte.
void func_08AD5624(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t handle = word(rt, ctx.gpr[4] + 0x10u);
    std::uint32_t result = 0u;
    if (static_cast<std::int32_t>(handle) >= 0) {
        GuestFrame frame(ctx, 0x10u);
        result = (call_guest(rt, ctx, 0x08AE0968u, {handle}) >> 1) & 1u;
    }
    ctx.gpr[2] = result;
}
void func_08AC9470(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[5] + 0x4u);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, 0x08AD5624u, {object}) & 0xFFu;
}

// func_08B24AB8(ring, n) -> the address of the item n past the read position (wrapping): items of +0x14
// bytes at +0x10, read position +0x8, capacity +0x18.
void func_08B24AB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t ring = ctx.gpr[4];
    const std::uint32_t capacity = word(rt, ring + 0x18u);
    std::uint32_t position = word(rt, ring + 0x8u) + ctx.gpr[5];
    if (position >= capacity) position -= capacity;
    ctx.gpr[2] = word(rt, ring + 0x10u) + position * word(rt, ring + 0x14u);
}

// Forwarders: func_088DA458(this) -> func_088D60B4(this + 4); func_089CBFDC(this) -> func_08B855E0(this
// + 0x10); func_08A02218(this): func_0896B0E4(this), then func_08A02110(this, 1).
void func_088DA458(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x088D60B4u, {ctx.gpr[4] + 0x4u}); }
void func_089CBFDC(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x08B855E0u, {ctx.gpr[4] + 0x10u}); }
void func_08A02218(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x0896B0E4u, {self});
    }
    tail_call(ctx, 0x08A02110u, {self, 1u});
}

// func_08A683D0() -> a new 8-byte object constructed by func_08A683AC.
void func_08A683D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t object = call_guest(rt, ctx, kAllocate, {8u});
    (void)call_guest(rt, ctx, 0x08A683ACu, {object});
    ctx.gpr[2] = object;
}

// func_08A7C868(this) -> func_08A5B9EC(&local), after func_08A74A4C(&local) and func_08A7C5C8(this,
// &local): a temporary built, filled and destroyed.
void func_08A7C868(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    (void)call_guest(rt, ctx, 0x08A74A4Cu, {frame.at(0u)});
    (void)call_guest(rt, ctx, 0x08A7C5C8u, {self, frame.at(0u)});
    ctx.gpr[2] = call_guest(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// func_08A6BF88(this, other) -> this, after func_08A69654(this, other + 0x14);
// func_089ECF44(this, -, x) -> this, after func_08B88F44(this, x).
void func_08A6BF88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08A69654u, {self, ctx.gpr[5] + 0x14u});
    }
    ctx.gpr[2] = self;
}
void func_089ECF44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08B88F44u, {self, ctx.gpr[6]});
    }
    ctx.gpr[2] = self;
}

// func_08AE29B0(this, x) -> func_08AE85E8(the handle at +0x98, &{6, 0, x}).
void func_08AE29B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t handle = word(rt, ctx.gpr[4] + 0x98u);
    const std::uint32_t x = ctx.gpr[5];
    GuestFrame frame(ctx, 0x50u);
    rt.memory().store32(frame.at(0x0u), 6u);
    rt.memory().store32(frame.at(0x8u), x);
    rt.memory().store32(frame.at(0x4u), 0u);
    ctx.gpr[2] = call_guest(rt, ctx, 0x08AE85E8u, {handle, frame.at(0u)});
}

// func_08B2A934(bitset, i, bool on) -> bitset, setting or clearing bit i (most significant first) of the
// word array at +0x4.
void func_08B2A934(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t i = ctx.gpr[5];
    const std::uint32_t address = word(rt, ctx.gpr[4] + 0x4u) + ((i >> 5) << 2);
    const std::uint32_t bits = word(rt, address);
    const std::uint32_t bit = 1u << (~i & 31u);
    rt.memory().store32(address, (ctx.gpr[6] & 0xFFu) != 0u ? (bits | bit) : (bits & ~bit));
    ctx.gpr[2] = ctx.gpr[4];
}

// func_08AFA5E4(this, -, x) -> func_08B52578(the virtual +0x40 (this, x)).
void func_08AFA5E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t result;
    {
        GuestFrame frame(ctx, 0x10u);
        result = call_guest(rt, ctx, word(rt, word(rt, self) + 0x40u), {self, ctx.gpr[6]});
    }
    tail_call(ctx, 0x08B52578u, {result});
}

// func_08AFD45C(grid, column, row) -> &cells[column + row × width]: cells of 28 bytes at +0x0, the u16
// width at +0x4.
void func_08AFD45C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t grid = ctx.gpr[4];
    const std::uint32_t index = ctx.gpr[5] + rt.memory().load16(grid + 0x4u) * ctx.gpr[6];
    ctx.gpr[2] = word(rt, grid) + index * 28u;
}

// func_089E0288(this): func_0896B0E4(this), the virtual +0x230 (this, 1), then func_089E00E4(this).
void func_089E0288(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x0896B0E4u, {self});
        (void)call_guest(rt, ctx, word(rt, word(rt, self) + 0x230u), {self, 1u});
    }
    tail_call(ctx, 0x089E00E4u, {self});
}

} // namespace gt::game

GT_DECOMP(0x089BA68Cu, gt::game::func_089BA68C, gt::ReturnKind::Word);
GT_DECOMP(0x08A17BD0u, gt::game::func_08A17BD0, gt::ReturnKind::Word);
GT_DECOMP(0x08A17B68u, gt::game::func_08A17B68, gt::ReturnKind::Word);
GT_DECOMP(0x08A7ADD0u, gt::game::func_08A7ADD0, gt::ReturnKind::Word);
// Not registered: it reaches a kernel import the isolated diff run cannot replay ("not comparable" in
// all 16 cases), so the gate can't check it.
// GT_DECOMP(0x08ABBEF8u, gt::game::func_08ABBEF8, gt::ReturnKind::Word);
GT_DECOMP(0x089A1628u, gt::game::func_089A1628, gt::ReturnKind::Word);
GT_DECOMP(0x08A9A414u, gt::game::func_08A9A414, gt::ReturnKind::Word);
GT_DECOMP(0x08AE2B40u, gt::game::func_08AE2B40, gt::ReturnKind::Word);
GT_DECOMP(0x08AE28A4u, gt::game::func_08AE28A4, gt::ReturnKind::Word);
GT_DECOMP(0x08B23778u, gt::game::func_08B23778, gt::ReturnKind::Word);
GT_DECOMP(0x08B237A0u, gt::game::func_08B237A0, gt::ReturnKind::Word);
GT_DECOMP(0x08AFAC7Cu, gt::game::func_08AFAC7C, gt::ReturnKind::Word);
GT_DECOMP(0x08A0DD40u, gt::game::func_08A0DD40, gt::ReturnKind::Word);
GT_DECOMP(0x08A5B860u, gt::game::func_08A5B860, gt::ReturnKind::Word);
GT_DECOMP(0x089D76A8u, gt::game::func_089D76A8, gt::ReturnKind::Word);
GT_DECOMP(0x08A64AECu, gt::game::func_08A64AEC, gt::ReturnKind::Word);
GT_DECOMP(0x08AA65A8u, gt::game::func_08AA65A8, gt::ReturnKind::Word);
GT_DECOMP(0x088FB560u, gt::game::func_088FB560, gt::ReturnKind::Word);
GT_DECOMP(0x08AF6CE8u, gt::game::func_08AF6CE8, gt::ReturnKind::Word);
GT_DECOMP(0x08B20A54u, gt::game::func_08B20A54, gt::ReturnKind::Word);
GT_DECOMP(0x08B20920u, gt::game::func_08B20920, gt::ReturnKind::Word);
// Not registered: the harness never sees a call to it on the three pad routes ("calls 0"), so the gate can't check it.
// GT_DECOMP(0x08AE0968u, gt::game::func_08AE0968, gt::ReturnKind::Word);
// Not registered: the harness never sees a call to it on the three pad routes ("calls 0"), so the gate can't check it.
// GT_DECOMP(0x08AD5624u, gt::game::func_08AD5624, gt::ReturnKind::Word);
// Not registered: the harness never sees a call to it on the three pad routes ("calls 0"), so the gate can't check it.
// GT_DECOMP(0x08AC9470u, gt::game::func_08AC9470, gt::ReturnKind::Word);
GT_DECOMP(0x08B24AB8u, gt::game::func_08B24AB8, gt::ReturnKind::Word);
GT_DECOMP(0x088DA458u, gt::game::func_088DA458, gt::ReturnKind::Word);
GT_DECOMP(0x089CBFDCu, gt::game::func_089CBFDC, gt::ReturnKind::Word);
GT_DECOMP(0x08A02218u, gt::game::func_08A02218, gt::ReturnKind::Word);
GT_DECOMP(0x08A683D0u, gt::game::func_08A683D0, gt::ReturnKind::Word);
GT_DECOMP(0x08A7C868u, gt::game::func_08A7C868, gt::ReturnKind::Word);
GT_DECOMP(0x08A6BF88u, gt::game::func_08A6BF88, gt::ReturnKind::Word);
GT_DECOMP(0x089ECF44u, gt::game::func_089ECF44, gt::ReturnKind::Word);
GT_DECOMP(0x08AE29B0u, gt::game::func_08AE29B0, gt::ReturnKind::Word);
GT_DECOMP(0x08B2A934u, gt::game::func_08B2A934, gt::ReturnKind::Word);
GT_DECOMP(0x08AFA5E4u, gt::game::func_08AFA5E4, gt::ReturnKind::Word);
GT_DECOMP(0x08AFD45Cu, gt::game::func_08AFD45C, gt::ReturnKind::Word);
GT_DECOMP(0x089E0288u, gt::game::func_089E0288, gt::ReturnKind::Word);

// --- batch 27 ---

namespace gt::game {
namespace {

constexpr std::uint32_t kPool2B80 = 0x08D62B80u;      // the object func_08B17AA8 / func_08B17A98 pass first
constexpr std::uint32_t kVtable8738 = 0x08CC8738u;    // vtables the constructors below install
constexpr std::uint32_t kVtable84E0 = 0x08CC84E0u;
constexpr std::uint32_t kVtable2CF0 = 0x08CD2CF0u;
constexpr std::uint32_t kVtable03E0 = 0x08CD03E0u;
constexpr std::uint32_t kContext04CC = 0x08D404CCu;   // the context func_08AD3CB0 / func_08AD3C38 use

// func_089D8EB8(this) unless the record `query(this)` returns has its word at +0x14 set (as func_089A1628).
void record_gate(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t query) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t record;
    {
        GuestFrame frame(ctx, 0x10u);
        record = call_guest(rt, ctx, query, {self});
    }
    ctx.gpr[2] = record;
    ctx.gpr[4] = self;
    if (word(rt, record + 0x14u) == 0u) tail_call(ctx, 0x089D8EB8u, {self});
}

// A temporary built by `build(&local)`, handed to `use(this, &local)`, then destroyed; returns `result`.
void with_temporary(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t build, std::uint32_t use,
                    bool return_self) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    (void)call_guest(rt, ctx, build, {frame.at(0u)});
    (void)call_guest(rt, ctx, use, {self, frame.at(0u)});
    const std::uint32_t destroyed = call_guest(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    ctx.gpr[2] = return_self ? self : destroyed;
}

// The constructor shape of func_08A64AEC: vtable, func_08A64554(this), then func_08A5BB84(this, 12).
void construct_64554(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t vtable) {
    const std::uint32_t self = ctx.gpr[4];
    rt.memory().store32(self, vtable);
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08A64554u, {self});
    }
    tail_call(ctx, 0x08A5BB84u, {self, 12u});
}

// `target(the word at `field`)`, returned.
void call_on_field(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t field, std::uint32_t target) {
    const std::uint32_t object = word(rt, ctx.gpr[4] + field);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, target, {object});
}

} // namespace

void func_08975270(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { record_gate(rt, ctx, 0x08BECE24u); }
void func_089F8A88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { record_gate(rt, ctx, 0x08C0C820u); }
void func_08A7C82C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { with_temporary(rt, ctx, 0x08A74F00u, 0x08A7C5C8u, false); }
void func_08AA2C90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { with_temporary(rt, ctx, 0x08A87EECu, 0x08A76ABCu, true); }
void func_08A64594(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { construct_64554(rt, ctx, kVtable2CF0); }
void func_08B166C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_on_field(rt, ctx, 0xA4u, 0x08B23FA0u); }
void func_08B11E84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_on_field(rt, ctx, 0x1ACu, 0x08B23FA0u); }

// Initialisers: func_08A17B10(this, x) (x, then +0x10, the byte at +0x4, +0x8, +0xC cleared),
// func_08B1FD24(this, x) (x, +0x8, +0x4 cleared), func_08926384(this, v) (+0xC cleared, v at +0x10,
// +0x4, +0x8), func_08B24D84(this, a, n) (+0xC = a + n, +0x8 = a), func_08AD5EBC(this) (two cleared words).
void func_08A17B10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    rt.memory().store32(self, ctx.gpr[5]);
    rt.memory().store32(self + 0x10u, 0u);
    rt.memory().store8(self + 0x4u, 0u);
    rt.memory().store32(self + 0x8u, 0u);
    rt.memory().store32(self + 0xCu, 0u);
}
void func_08B1FD24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store32(ctx.gpr[4], ctx.gpr[5]);
    rt.memory().store32(ctx.gpr[4] + 0x8u, 0u);
    rt.memory().store32(ctx.gpr[4] + 0x4u, 0u);
}
void func_08926384(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t value = std::bit_cast<std::uint32_t>(ctx.fpr[12]);
    rt.memory().store32(ctx.gpr[4] + 0xCu, 0u);
    for (const std::uint32_t offset : {0x10u, 0x4u, 0x8u}) rt.memory().store32(ctx.gpr[4] + offset, value);
}
void func_08B24D84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[6] += ctx.gpr[5];
    rt.memory().store32(ctx.gpr[4] + 0xCu, ctx.gpr[6]);
    rt.memory().store32(ctx.gpr[4] + 0x8u, ctx.gpr[5]);
}
void func_08AD5EBC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store32(ctx.gpr[4] + 0x4u, 0u);
    rt.memory().store32(ctx.gpr[4], 0u);
}

// A singly linked free list (head at +0x10, free count +0x18, used count +0x14): func_08B2B7D8(list) ->
// the popped head; func_08B2B800(list, node) pushes node back.
void func_08B2B7D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t list = ctx.gpr[4];
    const std::uint32_t head = word(rt, list + 0x10u);
    const std::uint32_t free_count = word(rt, list + 0x18u) - 1u;
    const std::uint32_t used = word(rt, list + 0x14u) + 1u;
    const std::uint32_t next = word(rt, head);
    rt.memory().store32(list + 0x14u, used);
    rt.memory().store32(list + 0x10u, next);
    rt.memory().store32(list + 0x18u, free_count);
    ctx.gpr[2] = head;
    ctx.gpr[3] = used;
    ctx.gpr[5] = free_count;
    ctx.gpr[6] = next;
}
void func_08B2B800(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t list = ctx.gpr[4];
    const std::uint32_t node = ctx.gpr[5];
    rt.memory().store32(node, word(rt, list + 0x10u));
    const std::uint32_t free_count = word(rt, list + 0x18u) + 1u;
    const std::uint32_t used = word(rt, list + 0x14u) - 1u;
    rt.memory().store32(list + 0x10u, node);
    rt.memory().store32(list + 0x18u, free_count);
    rt.memory().store32(list + 0x14u, used);
    ctx.gpr[2] = free_count;
    ctx.gpr[3] = used;
}

// func_08B1FBEC(list, node): appends node to a doubly linked list (head +0x4, tail +0x8; node prev +0x4,
// next +0x8).
void func_08B1FBEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t list = ctx.gpr[4];
    const std::uint32_t node = ctx.gpr[5];
    rt.memory().store32(node + 0x8u, 0u);
    rt.memory().store32(node + 0x4u, word(rt, list + 0x8u));
    const std::uint32_t tail = word(rt, list + 0x8u);
    if (tail == 0u) rt.memory().store32(list + 0x4u, node);
    else rt.memory().store32(tail + 0x8u, node);
    rt.memory().store32(list + 0x8u, node);
    ctx.gpr[2] = tail;
}

// Forwarders: func_08B17AA8 / func_08B17A98(x) -> func_08B2C248 / func_08B2C1C0(the pool at 0x08D62B80, x);
// func_089BAC14(key) -> func_08A7C0D8(key, the table at 0x08CF6ED8); func_08A16E88(-, x) ->
// func_08966CF0(x); func_089C4558 -> func_08B599E0; func_089F0B04(this) -> func_08B0B36C(this + 0xC).
void func_08B17AA8(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x08B2C248u, {kPool2B80, ctx.gpr[4]}); }
void func_08B17A98(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x08B2C1C0u, {kPool2B80, ctx.gpr[4]}); }
void func_089BAC14(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x08A7C0D8u, {ctx.gpr[4], kLookupTable}); }
void func_08A16E88(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x08966CF0u, {ctx.gpr[5]}); }
void func_089C4558(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x08B599E0u, {}); }
void func_089F0B04(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x08B0B36Cu, {ctx.gpr[4] + 0xCu}); }

// func_08A6C158(this) -> func_08B71EC8(the 64-bit func_08B77BF4(the word at +0x14)).
void func_08A6C158(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4] + 0x14u);
    GuestFrame frame(ctx, 0x10u);
    const GuestResult wide = call_guest(rt, ctx, 0x08B77BF4u, {object}, {});
    const GuestResult result = call_guest(rt, ctx, 0x08B71EC8u, {wide.v0, wide.v1}, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
}

// func_08A76724(this): clears +0x8, +0x0, +0x4, then func_08A76330(this).
void func_08A76724(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    for (const std::uint32_t offset : {0x8u, 0x0u, 0x4u}) rt.memory().store32(ctx.gpr[4] + offset, 0u);
    tail_call(ctx, 0x08A76330u, {ctx.gpr[4]});
}

// func_089FE990(this): vtables at +0x38 and +0x0, then the base constructor func_08A0DD40(this).
void func_089FE990(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store32(ctx.gpr[4] + 0x38u, kVtable8738);
    rt.memory().store32(ctx.gpr[4], kVtable84E0);
    ctx.gpr[3] = kVtable8738;
    ctx.gpr[5] = kVtable84E0;
    tail_call(ctx, 0x08A0DD40u, {ctx.gpr[4]});
}

// func_08AF9744(this, x): func_08C4183C(x, this + 0x14), then func_08C4176C(x, this + 0x18).
void func_08AF9744(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t x = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08C4183Cu, {x, self + 0x14u});
    }
    tail_call(ctx, 0x08C4176Cu, {x, self + 0x18u});
}

// func_08A6974C(this, other) -> this, after copying other's counted pointer at +0x14 (func_08A6972C).
void func_08A6974C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08A6972Cu, {self, ctx.gpr[5] + 0x14u});
    }
    ctx.gpr[2] = self;
}

// func_08993B9C(this): func_08A009BC(this), then func_089939D4(this, 0).
void func_08993B9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08A009BCu, {self});
    }
    tail_call(ctx, 0x089939D4u, {self, 0u});
}

// func_08AD3CB0(a, b) -> func_08AD3A8C(context, a, b, context), 0 without the context at 0x08D404CC;
// func_08AD3C38(a) -> func_08AD3A14(context, a), -1 without it.
void func_08AD3CB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t context = word(rt, kContext04CC);
    const std::uint32_t a = ctx.gpr[4];
    const std::uint32_t b = ctx.gpr[5];
    ctx.gpr[3] = a;
    ctx.gpr[2] = kContext04CC & 0xFFFF0000u;
    if (context == 0u) {
        ctx.gpr[4] = 0u;
        ctx.gpr[5] = a;
        ctx.gpr[6] = b;
        ctx.gpr[7] = 0u;
        ctx.gpr[2] = 0u;
        return;
    }
    tail_call(ctx, 0x08AD3A8Cu, {context, a, b, context});
}
void func_08AD3C38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t context = word(rt, kContext04CC);
    const std::uint32_t a = ctx.gpr[4];
    ctx.gpr[3] = context;
    ctx.gpr[2] = kContext04CC & 0xFFFF0000u;
    if (context == 0u) {
        ctx.gpr[4] = 0u;
        ctx.gpr[5] = a;
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }
    tail_call(ctx, 0x08AD3A14u, {context, a});
}

// func_08B2BA24(this) -> the word at +0x44 minus the word at +0x3C.
void func_08B2BA24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[3] = word(rt, ctx.gpr[4] + 0x3Cu);
    ctx.gpr[2] = word(rt, ctx.gpr[4] + 0x44u) - ctx.gpr[3];
}

// func_08A12534(this): the base constructor func_089E4AEC(this), then the vtable and two cleared fields.
void func_08A12534(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x089E4AECu, {self});
    }
    rt.memory().store32(self, kVtable03E0);
    rt.memory().store8(self + 0x24u, 0u);
    rt.memory().store32(self + 0x20u, 0u);
    ctx.gpr[2] = kVtable03E0;
}

// func_089A0710(this) -> func_08A105B8(this), after which the word at +0xB4 is cleared.
void func_089A0710(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t result;
    {
        GuestFrame frame(ctx, 0x10u);
        result = call_guest(rt, ctx, 0x08A105B8u, {self});
    }
    rt.memory().store32(self + 0xB4u, 0u);
    ctx.gpr[2] = result;
}

// func_08AB3D1C(this, a, x) -> x, after func_08BEED1C(the word at +0x20, a, x).
void func_08AB3D1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t x = ctx.gpr[6];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08BEED1Cu, {word(rt, ctx.gpr[4] + 0x20u), ctx.gpr[5], x});
    }
    ctx.gpr[2] = x;
}

// func_08B1D1BC(this, i) -> the 64-bit value at +0x8 of entry i (16 bytes, table at +0x2C), 0 when i is
// not below the count at +0x34.
void func_08B1D1BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t i = ctx.gpr[5];
    std::uint32_t low = 0u;
    std::uint32_t high = 0u;
    if (i < word(rt, self + 0x34u)) {
        const std::uint32_t entry = word(rt, self + 0x2Cu) + (i << 4);
        low = word(rt, entry + 0x8u);
        high = word(rt, entry + 0xCu);
    }
    ctx.gpr[2] = low;
    ctx.gpr[3] = high;
}

// func_08AD1D38(this, a, b) -> func_08AD6600(the word at +0x29C of the object at +0x78, this + 0x44, a, b),
// 0 without the object.
void func_08AD1D38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t object = word(rt, self + 0x78u);
    std::uint32_t result = 0u;
    if (object != 0u) {
        GuestFrame frame(ctx, 0x10u);
        result = call_guest(rt, ctx, 0x08AD6600u, {word(rt, object + 0x29Cu), self + 0x44u, ctx.gpr[5], ctx.gpr[6]});
    }
    ctx.gpr[2] = result;
}

// func_089FE3CC(this): func_08A17A28(this), then func_08A5B8B8(this), which leaves the first call's v0.
void func_089FE3CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08A17A28u, {self});
    }
    tail_call(ctx, 0x08A5B8B8u, {self});
}

// func_08AF75A0(this): func_08AF7504(this) when func_08AF6D14(this) holds.
void func_08AF75A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t holds;
    {
        GuestFrame frame(ctx, 0x10u);
        holds = call_guest(rt, ctx, 0x08AF6D14u, {self});
    }
    ctx.gpr[2] = holds;
    ctx.gpr[4] = self;
    if (holds != 0u) tail_call(ctx, 0x08AF7504u, {self});
}

// func_089685BC(this, x): stores x at +0xB8 and sets bit 20 of +0xE0.
void func_089685BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_and_flag(rt, ctx, 0xB8u); }

} // namespace gt::game

GT_DECOMP(0x08975270u, gt::game::func_08975270, gt::ReturnKind::Word);
GT_DECOMP(0x089F8A88u, gt::game::func_089F8A88, gt::ReturnKind::Word);
GT_DECOMP(0x08A7C82Cu, gt::game::func_08A7C82C, gt::ReturnKind::Word);
GT_DECOMP(0x08AA2C90u, gt::game::func_08AA2C90, gt::ReturnKind::Word);
GT_DECOMP(0x08A64594u, gt::game::func_08A64594, gt::ReturnKind::Word);
GT_DECOMP(0x08B166C0u, gt::game::func_08B166C0, gt::ReturnKind::Word);
GT_DECOMP(0x08B11E84u, gt::game::func_08B11E84, gt::ReturnKind::Word);
GT_DECOMP(0x08A17B10u, gt::game::func_08A17B10, gt::ReturnKind::Void);
GT_DECOMP(0x08B1FD24u, gt::game::func_08B1FD24, gt::ReturnKind::Void);
GT_DECOMP(0x08926384u, gt::game::func_08926384, gt::ReturnKind::Void);
GT_DECOMP(0x08B24D84u, gt::game::func_08B24D84, gt::ReturnKind::Void);
GT_DECOMP(0x08AD5EBCu, gt::game::func_08AD5EBC, gt::ReturnKind::Void);
GT_DECOMP(0x08B2B7D8u, gt::game::func_08B2B7D8, gt::ReturnKind::Word);
GT_DECOMP(0x08B2B800u, gt::game::func_08B2B800, gt::ReturnKind::Word);
GT_DECOMP(0x08B1FBECu, gt::game::func_08B1FBEC, gt::ReturnKind::Word);
GT_DECOMP(0x08B17AA8u, gt::game::func_08B17AA8, gt::ReturnKind::Word);
GT_DECOMP(0x08B17A98u, gt::game::func_08B17A98, gt::ReturnKind::Word);
GT_DECOMP(0x089BAC14u, gt::game::func_089BAC14, gt::ReturnKind::Word);
GT_DECOMP(0x08A16E88u, gt::game::func_08A16E88, gt::ReturnKind::Word);
// Not registered: it reaches a kernel import the isolated diff run cannot replay ("not comparable" in
// all 30 cases), so the gate can't check it.
// GT_DECOMP(0x089C4558u, gt::game::func_089C4558, gt::ReturnKind::Word);
GT_DECOMP(0x089F0B04u, gt::game::func_089F0B04, gt::ReturnKind::Word);
GT_DECOMP(0x08A6C158u, gt::game::func_08A6C158, gt::ReturnKind::DoubleWord);
GT_DECOMP(0x08A76724u, gt::game::func_08A76724, gt::ReturnKind::Word);
GT_DECOMP(0x089FE990u, gt::game::func_089FE990, gt::ReturnKind::Word);
GT_DECOMP(0x08AF9744u, gt::game::func_08AF9744, gt::ReturnKind::Word);
GT_DECOMP(0x08A6974Cu, gt::game::func_08A6974C, gt::ReturnKind::Word);
GT_DECOMP(0x08993B9Cu, gt::game::func_08993B9C, gt::ReturnKind::Word);
GT_DECOMP(0x08AD3CB0u, gt::game::func_08AD3CB0, gt::ReturnKind::Word);
GT_DECOMP(0x08AD3C38u, gt::game::func_08AD3C38, gt::ReturnKind::Word);
GT_DECOMP(0x08B2BA24u, gt::game::func_08B2BA24, gt::ReturnKind::Word);
GT_DECOMP(0x08A12534u, gt::game::func_08A12534, gt::ReturnKind::Word);
GT_DECOMP(0x089A0710u, gt::game::func_089A0710, gt::ReturnKind::Word);
GT_DECOMP(0x08AB3D1Cu, gt::game::func_08AB3D1C, gt::ReturnKind::Word);
GT_DECOMP(0x08B1D1BCu, gt::game::func_08B1D1BC, gt::ReturnKind::DoubleWord);
GT_DECOMP(0x08AD1D38u, gt::game::func_08AD1D38, gt::ReturnKind::Word);
GT_DECOMP(0x089FE3CCu, gt::game::func_089FE3CC, gt::ReturnKind::Word);
GT_DECOMP(0x08AF75A0u, gt::game::func_08AF75A0, gt::ReturnKind::Word);
GT_DECOMP(0x089685BCu, gt::game::func_089685BC, gt::ReturnKind::Word);

// --- batch 28 ---

namespace gt::game {
namespace {

constexpr std::uint32_t kCallback7AC8 = 0x08B17AC8u;  // the function func_08B17AE4 installs at +0x48
constexpr std::uint32_t kVtable8D28 = 0x08CB8D28u;    // vtables the constructors below install
constexpr std::uint32_t kVtableC990 = 0x08CBC990u;
constexpr std::uint32_t kVtableAD00 = 0x08CBAD00u;
constexpr std::uint32_t kVtableAAA8 = 0x08CBAAA8u;
constexpr std::uint32_t kVtable4918 = 0x08CD4918u;
constexpr std::uint32_t kVtable7B80 = 0x08CD7B80u;
constexpr std::uint32_t kCrcTableMsb = 0x08CD9E88u;      // 256 words, most significant byte first
constexpr std::uint32_t kSingleton112C = 0x08D6112Cu; // the object func_08B1753C hands out

// r = first(this); then the tail call `next(r, this, extra...)`.
void pass_result(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t first, std::uint32_t next,
                 std::initializer_list<std::uint32_t> extra) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t result;
    {
        GuestFrame frame(ctx, 0x10u);
        result = call_guest(rt, ctx, first, {self});
    }
    ctx.gpr[4] = result;
    ctx.gpr[5] = self;
    unsigned reg = 6u;
    for (const std::uint32_t value : extra) ctx.gpr[reg++] = value;
    tail_call(ctx, next, {});
}

void store_vtable(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t vtable) {
    rt.memory().store32(ctx.gpr[4], vtable);
    ctx.gpr[2] = vtable;
}

} // namespace

// func_08B17AE4(this, x): +0x50 = x, +0x48 = func_08B17AC8, +0x4C = 0.
void func_08B17AE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store32(ctx.gpr[4] + 0x50u, ctx.gpr[5]);
    rt.memory().store32(ctx.gpr[4] + 0x48u, kCallback7AC8);
    rt.memory().store32(ctx.gpr[4] + 0x4Cu, 0u);
    ctx.gpr[2] = kCallback7AC8;
}

// func_08B17A88(x) -> func_08B2BFDC(the pool at 0x08D62B80, x).
void func_08B17A88(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x08B2BFDCu, {kPool2B80, ctx.gpr[4]}); }

// func_08AD5ED8(-, obj, x) -> the virtual +0x30 (obj, x), 0 for a null obj.
void func_08AD5ED8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[5];
    const std::uint32_t x = ctx.gpr[6];
    std::uint32_t result = 0u;
    if (object != 0u) {
        GuestFrame frame(ctx, 0x10u);
        result = call_guest(rt, ctx, word(rt, word(rt, object) + 0x30u), {object, x});
    }
    ctx.gpr[2] = result;
}

// func_08B17A20(this) -> the virtual +0x18.
void func_08B17A20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = word(rt, ctx.gpr[4]);
    tail_call(ctx, word(rt, ctx.gpr[2] + 0x18u), {ctx.gpr[4]});
}

// func_08AD55EC(this): defaults (-1 at +0x10 and +0x4, 1.0 at +0x18, +0x1C and +0x20, zero elsewhere).
void func_08AD55EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t one = std::bit_cast<std::uint32_t>(1.0f);
    rt.memory().store32(self + 0x10u, 0xFFFFFFFFu);
    rt.memory().store32(self + 0x20u, one);
    rt.memory().store32(self + 0x24u, 0u);
    rt.memory().store32(self + 0x0u, 0u);
    rt.memory().store32(self + 0x4u, 0xFFFFFFFFu);
    for (const std::uint32_t offset : {0x8u, 0xCu, 0x14u}) rt.memory().store32(self + offset, 0u);
    rt.memory().store32(self + 0x18u, one);
    rt.memory().store32(self + 0x1Cu, one);
    ctx.gpr[2] = 0xFFFFFFFFu;
    ctx.fpr[0] = 1.0f;
}

// func_08AD37F8(this, i) -> the word table[i] (table at +0x24).
void func_08AD37F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = word(rt, (ctx.gpr[5] << 2) + word(rt, ctx.gpr[4] + 0x24u));
}

// func_08ABBE00(holder, x) -> the virtual +0x3C of *holder (object, holder, x), 0 when either is null.
void func_08ABBE00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t holder = ctx.gpr[4];
    const std::uint32_t x = ctx.gpr[5];
    ctx.gpr[6] = x;
    const std::uint32_t object = holder != 0u ? word(rt, holder) : 0u;
    if (object == 0u) {
        if (holder != 0u) {
            ctx.gpr[4] = 0u;
            ctx.gpr[5] = holder;
        }
        ctx.gpr[2] = 0u;
        return;
    }
    ctx.gpr[2] = word(rt, object);
    tail_call(ctx, word(rt, word(rt, object) + 0x3Cu), {object, holder, x});
}

// func_08ABB678(holder): the virtual +0x44 of *holder (object, holder) when both are set.
void func_08ABB678(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t holder = ctx.gpr[4];
    ctx.gpr[5] = holder;
    if (holder == 0u) return;  // v0 untouched, as in the original
    const std::uint32_t object = word(rt, holder);
    ctx.gpr[2] = object;
    ctx.gpr[4] = object;
    if (object == 0u) return;
    ctx.gpr[2] = word(rt, object);
    tail_call(ctx, word(rt, word(rt, object) + 0x44u), {object, holder});
}

// func_089FD978(this): the base constructor func_08A76ACC(this), then the vtable and two cleared words;
// func_089B9EA4(this, x): func_089FD978(this), x at +0x1C and the derived vtable.
void func_089FD978(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08A76ACCu, {self});
    }
    rt.memory().store32(self, kVtable8D28);
    rt.memory().store32(self + 0x18u, 0u);
    rt.memory().store32(self + 0x14u, 0u);
    ctx.gpr[2] = kVtable8D28;
}
void func_089B9EA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t x = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x089FD978u, {self});
    }
    rt.memory().store32(self + 0x1Cu, x);
    rt.memory().store32(self, kVtableC990);
    ctx.gpr[2] = kVtableC990;
}

// func_088BB57C(this): the C library rand() step on the u16 seed at +0x68:
// seed = ((seed * 1103515245 + 12345) >> 16) & 0x7FFF.
void func_088BB57C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t seed = rt.memory().load16(ctx.gpr[4] + 0x68u);
    const std::uint32_t next = ((seed * 1103515245u + 12345u) >> 16) & 0x7FFFu;
    rt.memory().store16(ctx.gpr[4] + 0x68u, static_cast<std::uint16_t>(next));
    ctx.gpr[2] = next;
    ctx.gpr[3] = 1103515245u;
}

// func_089FF54C(this): func_089BD168(this), then func_089B8BA0(this + 0x78) (as func_089675A0).
void func_089FF54C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x089BD168u, {self});
    }
    tail_call(ctx, 0x089B8BA0u, {self + 0x78u});
}

// func_08B29E00(crc, const u8 *p, n) -> crc updated over n bytes, most significant byte first
// (crc = table[(byte ^ (crc >> 24))] ^ (crc << 8)), table at 0x08CD9E88.
void func_08B29E00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t crc = ctx.gpr[4];
    std::uint32_t p = ctx.gpr[5];
    const std::uint32_t end = p + ctx.gpr[6];
    ctx.gpr[6] = end;
    while (p != end) {
        crc = word(rt, kCrcTableMsb + ((rt.memory().load8(p) ^ (crc >> 24)) << 2)) ^ (crc << 8);
        ++p;
    }
    ctx.gpr[2] = crc;
}

// Forwarders: func_08AD569C(a) -> func_08AD5658(a, 1); func_089CBFD4(this) -> func_08B855E0(this + 0xC);
// func_08969160(key) -> func_089BAC44(key, the table at 0x08CF6ED8).
void func_08AD569C(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x08AD5658u, {ctx.gpr[4], 1u}); }
void func_089CBFD4(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x08B855E0u, {ctx.gpr[4] + 0xCu}); }
void func_08969160(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x089BAC44u, {ctx.gpr[4], kLookupTable}); }

// func_08A92114(this, a, -, b, c, d): stores a, b, c, d and a cleared word, then func_08A920E0 with the
// same arguments.
void func_08A92114(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    rt.memory().store32(self + 0x0u, ctx.gpr[5]);
    rt.memory().store32(self + 0x4u, ctx.gpr[7]);
    rt.memory().store32(self + 0x8u, ctx.gpr[8]);
    rt.memory().store32(self + 0xCu, ctx.gpr[9]);
    rt.memory().store32(self + 0x10u, 0u);
    tail_call(ctx, 0x08A920E0u, {});
}

// Initialisers: func_08B24D6C(this) (the halfword at +0x20 = 0xFFF0, three cleared words),
// func_089582F8(this) (zeroed, then the byte at +0x14 = 1).
void func_08B24D6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store16(ctx.gpr[4] + 0x20u, 0xFFF0u);
    for (const std::uint32_t offset : {0xCu, 0x4u, 0x8u}) rt.memory().store32(ctx.gpr[4] + offset, 0u);
    ctx.gpr[2] = 0xFFFFFFF0u;
}
void func_089582F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    rt.memory().store32(self + 0x14u, 0u);
    rt.memory().store32(self + 0x0u, 0u);
    rt.memory().store8(self + 0x14u, 1u);
    for (const std::uint32_t offset : {0x4u, 0x8u, 0xCu, 0x10u, 0x18u, 0x1Cu}) rt.memory().store32(self + offset, 0u);
    ctx.gpr[2] = 1u;
}

// func_08B1753C() -> the object at 0x08D6112C when func_08B1815C accepts it, else 0.
void func_08B1753C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, 0x08B1815Cu, {kSingleton112C}) != 0u ? kSingleton112C : 0u;
}

// Member addresses: func_08A06FB8 / func_08A06FB0 / func_08A06FA8(this) -> this + 0x100 / 0xF4 / 0xE8;
// func_0881ACB0(this, i) -> &this->words[i] at +0x1C4.
void func_08A06FB8(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = ctx.gpr[4] + 0x100u; }
void func_08A06FB0(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = ctx.gpr[4] + 0xF4u; }
void func_08A06FA8(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = ctx.gpr[4] + 0xE8u; }
void func_0881ACB0(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = (ctx.gpr[5] << 2) + ctx.gpr[4] + 0x1C4u; }

// func_0896601C(this, bool on): bit 1 of the flags at +0xE0 = on, keeping bits 0, 2 and 3.
void func_0896601C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t flags = word(rt, ctx.gpr[4] + 0xE0u);
    const std::uint32_t on = ctx.gpr[5] & 0xFFu;
    const std::uint32_t low = (on != 0u ? 2u : 0u) | (flags & 13u);
    const std::uint32_t updated = (flags & ~0xFu) | low;
    rt.memory().store32(ctx.gpr[4] + 0xE0u, updated);
    ctx.gpr[2] = low;
    ctx.gpr[3] = flags & 13u;
    ctx.gpr[5] = on;
    ctx.gpr[6] = updated;
}

// func_089A2528(this): the base constructor func_08A0D658(this), then the vtables at +0x38 and +0x0;
// func_089742E8(this) -> func_08A105B8(this), clearing +0xB4 after (as func_089A0710).
void func_089A2528(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08A0D658u, {self});
    }
    rt.memory().store32(self + 0x38u, kVtableAD00);
    rt.memory().store32(self, kVtableAAA8);
    ctx.gpr[2] = kVtableAD00;
    ctx.gpr[3] = kVtableAAA8;
}
void func_089742E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t result;
    {
        GuestFrame frame(ctx, 0x10u);
        result = call_guest(rt, ctx, 0x08A105B8u, {self});
    }
    rt.memory().store32(self + 0xB4u, 0u);
    ctx.gpr[2] = result;
}

// func_089324EC(-, x, low, high) -> x clamped to [low, high] (signed).
void func_089324EC(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    const auto x = static_cast<std::int32_t>(ctx.gpr[5]);
    const auto low = static_cast<std::int32_t>(ctx.gpr[6]);
    const auto high = static_cast<std::int32_t>(ctx.gpr[7]);
    ctx.gpr[2] = static_cast<std::uint32_t>(std::min(std::max(x, low), high));
}

// func_08B1FC10(list, before, node): inserts node before `before` in the doubly linked list of
// func_08B1FBEC, or appends it when `before` is null.
void func_08B1FC10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t list = ctx.gpr[4];
    const std::uint32_t before = ctx.gpr[5];
    const std::uint32_t node = ctx.gpr[6];
    ctx.gpr[3] = before;
    ctx.gpr[5] = node;
    if (before == 0u) {
        tail_call(ctx, 0x08B1FBECu, {});
        return;
    }
    const std::uint32_t prev = word(rt, before + 0x4u);
    rt.memory().store32(node + 0x8u, before);
    rt.memory().store32(node + 0x4u, prev);
    if (prev != 0u) rt.memory().store32(prev + 0x8u, node);
    else rt.memory().store32(list + 0x4u, node);
    rt.memory().store32(before + 0x4u, node);
    ctx.gpr[2] = prev;
}

// func_08968ED8(this, const float v[2]): +0xA4 = v[0], +0xA8 = v[1], setting bit 20 of +0xE0.
void func_08968ED8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    rt.memory().store32(self + 0xA4u, word(rt, ctx.gpr[5]));
    const std::uint32_t flags = word(rt, self + 0xE0u) | 0x100000u;
    const std::uint32_t second = word(rt, ctx.gpr[5] + 0x4u);
    rt.memory().store32(self + 0xE0u, flags);
    rt.memory().store32(self + 0xA8u, second);
    ctx.gpr[2] = 0x100000u;
    ctx.gpr[3] = flags;
    ctx.fpr[0] = std::bit_cast<float>(second);
}

// func_08B0DD20(this): func_08B0DF38(this), func_08B0C304(this + 0xF8), then func_08B0DBCC(this).
void func_08B0DD20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08B0DF38u, {self});
        (void)call_guest(rt, ctx, 0x08B0C304u, {self + 0xF8u});
    }
    tail_call(ctx, 0x08B0DBCCu, {self});
}

// func_089BA6CC(this, x): +0x48 = x, +0x4C = x's word at +0x4 (0 for a null x).
void func_089BA6CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t x = ctx.gpr[5];
    rt.memory().store32(ctx.gpr[4] + 0x48u, x);
    if (x == 0u) {
        rt.memory().store32(ctx.gpr[4] + 0x4Cu, 0u);
        return;
    }
    ctx.gpr[2] = word(rt, x + 0x4u);
    rt.memory().store32(ctx.gpr[4] + 0x4Cu, ctx.gpr[2]);
}

// func_08931014(this) -> func_089364AC(func_089363FC(this), this, 34); func_08A5CC14(this) ->
// func_08AA3F44(func_08A5CB28(this), this).
void func_08931014(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pass_result(rt, ctx, 0x089363FCu, 0x089364ACu, {34u}); }
void func_08A5CC14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pass_result(rt, ctx, 0x08A5CB28u, 0x08AA3F44u, {}); }

// func_08ADB314(p) -> p when it starts with the tag "SGXD" (0x44584753), else 0.
void func_08ADB314(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t p = ctx.gpr[4];
    ctx.gpr[3] = 0x44584753u;
    ctx.gpr[2] = (p != 0u && word(rt, p) == 0x44584753u) ? p : 0u;
    if (p != 0u && ctx.gpr[2] == 0u) ctx.gpr[4] = 0u;
}

// func_08A9E848 / func_08A9E820(this): the vtable (and, for the second, a cleared word at +0x1C).
void func_08A9E848(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable(rt, ctx, kVtable4918); }
void func_08A9E820(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    store_vtable(rt, ctx, kVtable4918);
    rt.memory().store32(ctx.gpr[4] + 0x1Cu, 0u);
}

// func_08AB86C0(this, const word *p) -> this, after func_08AD2D48(this, *p).
void func_08AB86C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08AD2D48u, {self, word(rt, ctx.gpr[5])});
    }
    ctx.gpr[2] = self;
}

// func_08B0CC04(this): func_08B0CBCC(this), the vtable, then func_08B0E0CC(this + 0x20);
// func_08B0C808(this): the vtable, func_08B0C778(this), func_08B0DDF0(this + 0x20), then func_08B0C1D8(this).
void func_08B0CC04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08B0CBCCu, {self});
    }
    rt.memory().store32(self, kVtable7B80);
    ctx.gpr[2] = kVtable7B80;
    tail_call(ctx, 0x08B0E0CCu, {self + 0x20u});
}
void func_08B0C808(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    rt.memory().store32(self, kVtable7B80);
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08B0C778u, {self});
        (void)call_guest(rt, ctx, 0x08B0DDF0u, {self + 0x20u});
    }
    tail_call(ctx, 0x08B0C1D8u, {self});
}

} // namespace gt::game

GT_DECOMP(0x08B17AE4u, gt::game::func_08B17AE4, gt::ReturnKind::Word);
GT_DECOMP(0x08B17A88u, gt::game::func_08B17A88, gt::ReturnKind::Word);
GT_DECOMP(0x08AD5ED8u, gt::game::func_08AD5ED8, gt::ReturnKind::Word);
GT_DECOMP(0x08B17A20u, gt::game::func_08B17A20, gt::ReturnKind::Word);
GT_DECOMP(0x08AD55ECu, gt::game::func_08AD55EC, gt::ReturnKind::Word);
GT_DECOMP(0x08AD37F8u, gt::game::func_08AD37F8, gt::ReturnKind::Word);
GT_DECOMP(0x08ABBE00u, gt::game::func_08ABBE00, gt::ReturnKind::Word);
// Not registered: it reaches a kernel import the isolated diff run cannot replay ("not comparable" in
// all 11 cases), so the gate can't check it.
// GT_DECOMP(0x08ABB678u, gt::game::func_08ABB678, gt::ReturnKind::Word);
GT_DECOMP(0x089FD978u, gt::game::func_089FD978, gt::ReturnKind::Word);
GT_DECOMP(0x089B9EA4u, gt::game::func_089B9EA4, gt::ReturnKind::Word);
GT_DECOMP(0x088BB57Cu, gt::game::func_088BB57C, gt::ReturnKind::Word);
GT_DECOMP(0x089FF54Cu, gt::game::func_089FF54C, gt::ReturnKind::Word);
GT_DECOMP(0x08B29E00u, gt::game::func_08B29E00, gt::ReturnKind::Word);
GT_DECOMP(0x08AD569Cu, gt::game::func_08AD569C, gt::ReturnKind::Word);
GT_DECOMP(0x089CBFD4u, gt::game::func_089CBFD4, gt::ReturnKind::Word);
GT_DECOMP(0x08969160u, gt::game::func_08969160, gt::ReturnKind::Word);
GT_DECOMP(0x08A92114u, gt::game::func_08A92114, gt::ReturnKind::Word);
GT_DECOMP(0x08B24D6Cu, gt::game::func_08B24D6C, gt::ReturnKind::Word);
GT_DECOMP(0x089582F8u, gt::game::func_089582F8, gt::ReturnKind::Word);
GT_DECOMP(0x08B1753Cu, gt::game::func_08B1753C, gt::ReturnKind::Word);
GT_DECOMP(0x08A06FB8u, gt::game::func_08A06FB8, gt::ReturnKind::Word);
GT_DECOMP(0x08A06FB0u, gt::game::func_08A06FB0, gt::ReturnKind::Word);
GT_DECOMP(0x08A06FA8u, gt::game::func_08A06FA8, gt::ReturnKind::Word);
GT_DECOMP(0x0881ACB0u, gt::game::func_0881ACB0, gt::ReturnKind::Word);
GT_DECOMP(0x0896601Cu, gt::game::func_0896601C, gt::ReturnKind::Word);
GT_DECOMP(0x089A2528u, gt::game::func_089A2528, gt::ReturnKind::Word);
GT_DECOMP(0x089742E8u, gt::game::func_089742E8, gt::ReturnKind::Word);
GT_DECOMP(0x089324ECu, gt::game::func_089324EC, gt::ReturnKind::Word);
GT_DECOMP(0x08B1FC10u, gt::game::func_08B1FC10, gt::ReturnKind::Word);
GT_DECOMP(0x08968ED8u, gt::game::func_08968ED8, gt::ReturnKind::Word);
GT_DECOMP(0x08B0DD20u, gt::game::func_08B0DD20, gt::ReturnKind::Word);
GT_DECOMP(0x089BA6CCu, gt::game::func_089BA6CC, gt::ReturnKind::Void);
GT_DECOMP(0x08931014u, gt::game::func_08931014, gt::ReturnKind::Word);
GT_DECOMP(0x08A5CC14u, gt::game::func_08A5CC14, gt::ReturnKind::Word);
GT_DECOMP(0x08ADB314u, gt::game::func_08ADB314, gt::ReturnKind::Word);
GT_DECOMP(0x08A9E848u, gt::game::func_08A9E848, gt::ReturnKind::Word);
GT_DECOMP(0x08A9E820u, gt::game::func_08A9E820, gt::ReturnKind::Word);
GT_DECOMP(0x08AB86C0u, gt::game::func_08AB86C0, gt::ReturnKind::Word);
GT_DECOMP(0x08B0CC04u, gt::game::func_08B0CC04, gt::ReturnKind::Word);
GT_DECOMP(0x08B0C808u, gt::game::func_08B0C808, gt::ReturnKind::Word);

// --- batch 29 ---

namespace gt::game {
namespace {

constexpr std::uint32_t kMemalign5840 = 0x08B35840u;  // (alignment, size): the other allocator of func_08B15490
constexpr std::uint32_t kContext8A70 = 0x08D68A70u;   // the object func_08B1C47C passes first
constexpr std::uint32_t kVtable4FDC = 0x08D74FDCu;    // vtables the constructors below install
constexpr std::uint32_t kVtable7C08 = 0x08CD7C08u;
constexpr std::uint32_t kVtableD618 = 0x08CCD618u;

// The virtual at `slot` of *holder (object, holder, a, b), 0 when either is null.
void holder_call4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t slot) {
    const std::uint32_t holder = ctx.gpr[4];
    const std::uint32_t a = ctx.gpr[5];
    const std::uint32_t b = ctx.gpr[6];
    ctx.gpr[3] = a;
    ctx.gpr[7] = b;
    const std::uint32_t object = holder != 0u ? word(rt, holder) : 0u;
    if (object == 0u) {
        if (holder != 0u) {
            ctx.gpr[4] = 0u;
            ctx.gpr[5] = holder;
            ctx.gpr[6] = a;
        }
        ctx.gpr[2] = 0u;
        return;
    }
    ctx.gpr[2] = word(rt, object);
    tail_call(ctx, word(rt, word(rt, object) + slot), {object, holder, a, b});
}

// The word at `offset` of the record func_08ABA410(x, y) finds, 0 when none.
void record_field(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t offset) {
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t record = call_guest(rt, ctx, 0x08ABA410u, {ctx.gpr[6], ctx.gpr[7]});
    ctx.gpr[2] = record != 0u ? word(rt, record + offset) : 0u;
}

// Stores the bytes at `offset` and `offset + 1` into *a and *b.
void byte_pair(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t offset) {
    ctx.gpr[2] = rt.memory().load8(ctx.gpr[4] + offset);
    rt.memory().store32(ctx.gpr[5], ctx.gpr[2]);
    ctx.gpr[3] = rt.memory().load8(ctx.gpr[4] + offset + 1u);
    rt.memory().store32(ctx.gpr[6], ctx.gpr[3]);
}

} // namespace

// func_08B0C99C(this, a, b, c) -> func_08B0C8D8(this, a, 0, b, c).
void func_08B0C99C(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[8] = ctx.gpr[7];
    ctx.gpr[7] = ctx.gpr[6];
    ctx.gpr[6] = 0u;
    tail_call(ctx, 0x08B0C8D8u, {});
}

// func_08991B98(this): the virtual +0x1B8 of the object at +0x9C when set, then func_08A10C54(this).
void func_08991B98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t object = word(rt, self + 0x9Cu);
    ctx.gpr[2] = object;
    if (object != 0u) {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, word(rt, word(rt, object) + 0x1B8u), {object});
    }
    tail_call(ctx, 0x08A10C54u, {self});
}

// func_089DFAAC / func_089DFAC0(this, int *a, int *b): the byte pairs at +0x208 / +0x20A.
void func_089DFAAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { byte_pair(rt, ctx, 0x208u); }
void func_089DFAC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { byte_pair(rt, ctx, 0x20Au); }

// func_089A29E4(this): the vtables of func_089A2528 at +0x38 and +0x0, then the base constructor
// func_08A0DD40(this).
void func_089A29E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store32(ctx.gpr[4] + 0x38u, kVtableAD00);
    rt.memory().store32(ctx.gpr[4], kVtableAAA8);
    ctx.gpr[3] = kVtableAD00;
    ctx.gpr[5] = kVtableAAA8;
    tail_call(ctx, 0x08A0DD40u, {ctx.gpr[4]});
}

// func_0895DCAC(this, value): the byte at +0x5F8 of the object at +0x8 (func_0895534C reads it).
void func_0895DCAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4] + 0x8u);
    ctx.gpr[2] = object;
    rt.memory().store8(object + 0x5F8u, static_cast<std::uint8_t>(ctx.gpr[5]));
}

// func_08B15490(size, bool aligned) -> a 64-byte aligned block of size rounded up to 64, from
// func_08B2B4D8 or (aligned == false) func_08B35840.
void func_08B15490(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t size = (ctx.gpr[4] + 63u) & ~63u;
    const std::uint32_t allocator = (ctx.gpr[5] & 0xFFu) != 0u ? kAllocateAligned : kMemalign5840;
    ctx.gpr[2] = allocator;
    ctx.gpr[25] = allocator;
    tail_call(ctx, allocator, {64u, size});
}

// func_088BB63C(this, a, b) -> a + (the u16 at +0x6A / 32768) × (b - a) (as func_088BB5C4 with +0x68).
void func_088BB63C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t raw = rt.memory().load16(ctx.gpr[4] + 0x6Au);
    const float a = ctx.fpr[12];
    const float span = ctx.fpr[13] - a;
    ctx.gpr[4] = raw;
    ctx.fpr[13] = span;
    ctx.fpr[1] = std::bit_cast<float>(0x38000000u);
    ctx.fpr[0] = a + psp_mul(psp_mul(static_cast<float>(static_cast<std::int32_t>(raw)), std::bit_cast<float>(0x38000000u)), span);
}

// func_088A5928(holder) -> whether the signed byte at +0x2C of *holder is non-zero.
void func_088A5928(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[3] = word(rt, ctx.gpr[4]);
    ctx.gpr[2] = rt.memory().load8(ctx.gpr[3] + 0x2Cu) != 0u ? 1u : 0u;
}

// func_08932558(pair) -> the 64-bit value at pair; func_088A5EE0(holder) -> that of row (signed byte
// at +0x8 of *holder) in the rows of 384 bytes at +0x38.
void func_08932558(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = word(rt, ctx.gpr[4]);
    ctx.gpr[3] = word(rt, ctx.gpr[4] + 0x4u);
}
void func_088A5EE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4]);
    const auto row = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(rt.memory().load8(object + 0x8u))));
    ctx.gpr[2] = object;
    ctx.gpr[3] = row << 7;
    tail_call(ctx, 0x08932558u, {row * 384u + object + 0x38u});
}

// Forwarders: func_08B1C47C(-, x) -> func_08B1C590(the object at 0x08D68A70, x, -5, -1);
// func_08992A4C(this): func_08A009BC(this), then func_089928A0(this, 0); func_08936578(-, -, a, b) ->
// func_089304C8(a, b); func_089E4BB8(key) -> func_089FE2D8(key, the table at 0x08CF6ED8).
void func_08B1C47C(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    tail_call(ctx, 0x08B1C590u, {kContext8A70, ctx.gpr[5], 0xFFFFFFFBu, 0xFFFFFFFFu});
}
void func_08992A4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08A009BCu, {self});
    }
    tail_call(ctx, 0x089928A0u, {self, 0u});
}
void func_08936578(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x089304C8u, {ctx.gpr[6], ctx.gpr[7]}); }
void func_089E4BB8(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x089FE2D8u, {ctx.gpr[4], kLookupTable}); }

// func_08AE9F78(const word *a, const word *b) -> func_08B35FD8(*a, *b).
void func_08AE9F78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = word(rt, ctx.gpr[4]);
    const std::uint32_t b = word(rt, ctx.gpr[5]);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, 0x08B35FD8u, {a, b});
}

// func_089660E8(this) -> bit 3 of the flags at +0xE0.
void func_089660E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = (word(rt, ctx.gpr[4] + 0xE0u) >> 3) & 1u; }

// Holder virtuals with two arguments: func_08ABCB7C / func_08ABCB44 / func_08ABCB0C / func_08ABCAD4 /
// func_08ABC8F4 (+0x5C / +0x58 / +0x54 / +0x50 / +0x60).
void func_08ABCB7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_call4(rt, ctx, 0x5Cu); }
void func_08ABCB44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_call4(rt, ctx, 0x58u); }
void func_08ABCB0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_call4(rt, ctx, 0x54u); }
void func_08ABCAD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_call4(rt, ctx, 0x50u); }
void func_08ABC8F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_call4(rt, ctx, 0x60u); }

// Record fields: func_08ABA818 / func_08ABA7E8 / func_08ABA7B8 / func_08ABA788 / func_08ABA674(-, -, x, y)
// -> the word at +0x1C / +0x8 / +0x0 / +0x18 / +0x14 of the record func_08ABA410(x, y) finds.
void func_08ABA818(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { record_field(rt, ctx, 0x1Cu); }
void func_08ABA7E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { record_field(rt, ctx, 0x8u); }
void func_08ABA7B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { record_field(rt, ctx, 0x0u); }
void func_08ABA788(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { record_field(rt, ctx, 0x18u); }
void func_08ABA674(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { record_field(rt, ctx, 0x14u); }

// func_08926440(this, a, b): +0x4 = a, +0x10 = +0x8 = b, +0xC = 0.
void func_08926440(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store32(ctx.gpr[4] + 0x4u, std::bit_cast<std::uint32_t>(ctx.fpr[12]));
    rt.memory().store32(ctx.gpr[4] + 0x10u, std::bit_cast<std::uint32_t>(ctx.fpr[13]));
    rt.memory().store32(ctx.gpr[4] + 0xCu, 0u);
    rt.memory().store32(ctx.gpr[4] + 0x8u, std::bit_cast<std::uint32_t>(ctx.fpr[13]));
}

// func_08A6C078(this) -> func_08A9FCD8(the word at +0x14, &local) with the local cleared first.
void func_08A6C078(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4] + 0x14u);
    GuestFrame frame(ctx, 0x20u);
    rt.memory().store32(frame.at(0u), 0u);
    ctx.gpr[2] = call_guest(rt, ctx, 0x08A9FCD8u, {object, frame.at(0u)});
}

// Constructors: func_08AAB5DC (the vtable 0x08D74FDC), func_08B0DC34 (func_08B0E008, the vtable,
// func_08B0CD6C(this + 0xF8), then func_08B0DBCC), func_08A719F0 (func_08A76ACC, the vtable, 2 at +0x14,
// 0 at +0x18).
void func_08AAB5DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable(rt, ctx, kVtable4FDC); }
void func_08B0DC34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08B0E008u, {self});
        rt.memory().store32(self, kVtable7C08);
        (void)call_guest(rt, ctx, 0x08B0CD6Cu, {self + 0xF8u});
    }
    tail_call(ctx, 0x08B0DBCCu, {self});
}
void func_08A719F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08A76ACCu, {self});
    }
    rt.memory().store32(self, kVtableD618);
    rt.memory().store32(self + 0x14u, 2u);
    rt.memory().store32(self + 0x18u, 0u);
    ctx.gpr[2] = kVtableD618;
    ctx.gpr[3] = 2u;
}

// func_089624EC(x) -> x.
void func_089624EC(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = ctx.gpr[4]; }

// func_0885D4CC(this) -> func_088A5928(this + 0x50) when the byte at +0x16 is set and the one at +0x17
// is clear, else 0.
void func_0885D4CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t result = 0u;
    if (rt.memory().load8(self + 0x16u) != 0u) {
        ctx.gpr[4] = self + 0x50u;
        if (rt.memory().load8(self + 0x17u) == 0u) {
            GuestFrame frame(ctx, 0x10u);
            result = call_guest(rt, ctx, 0x088A5928u, {self + 0x50u});
        }
    }
    ctx.gpr[2] = result;
}

// func_08B18DC4(list, node): appends node to the list at +0xC (head +0xC, tail +0x10, count +0x14;
// node next at +0x4), clearing node's next.
void func_08B18DC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t list = ctx.gpr[4];
    const std::uint32_t node = ctx.gpr[5];
    const std::uint32_t tail = word(rt, list + 0x10u);
    rt.memory().store32(list + 0x10u, node);
    if (tail != 0u) rt.memory().store32(tail + 0x4u, node);
    else rt.memory().store32(list + 0xCu, node);
    const std::uint32_t next_field = node != 0u ? node + 0x4u : 0u;
    rt.memory().store32(next_field, 0u);
    const std::uint32_t count = word(rt, list + 0x14u) + 1u;
    rt.memory().store32(list + 0x14u, count);
    ctx.gpr[2] = next_field;
    ctx.gpr[3] = count;
    ctx.gpr[6] = list + 0xCu;
}

// func_08AD1D70(this, a, b, c) -> func_08AD650C(the word at +0x29C of the object at +0x78, this + 0x44,
// a, b, c), 0 without the object (as func_08AD1D38).
void func_08AD1D70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t object = word(rt, self + 0x78u);
    std::uint32_t result = 0u;
    if (object != 0u) {
        GuestFrame frame(ctx, 0x10u);
        result = call_guest(rt, ctx, 0x08AD650Cu, {word(rt, object + 0x29Cu), self + 0x44u, ctx.gpr[5], ctx.gpr[6], ctx.gpr[7]});
    }
    ctx.gpr[2] = result;
}

// func_08A5CC40(x) -> func_08A5CB28(x) + 4.
void func_08A5CC40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, 0x08A5CB28u, {ctx.gpr[4]}) + 4u;
}

// func_08AE2C68(this, x) -> func_08AE3BE8(the handle at +0x98, &{3, x}).
void func_08AE2C68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t handle = word(rt, ctx.gpr[4] + 0x98u);
    const std::uint32_t x = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    rt.memory().store32(frame.at(0x0u), 3u);
    rt.memory().store32(frame.at(0x4u), x);
    ctx.gpr[2] = call_guest(rt, ctx, 0x08AE3BE8u, {handle, frame.at(0u)});
}

// func_08977390(this, i): clears entry i's bytes at +0x14A and +0x14B and its halfword at +0x156
// (entries of 2 bytes).
void func_08977390(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t base = (ctx.gpr[5] << 1) + ctx.gpr[4];
    rt.memory().store8(base + 0x14Bu, 0u);
    rt.memory().store16(base + 0x156u, 0u);
    rt.memory().store8(base + 0x14Au, 0u);
}

} // namespace gt::game

// Not registered: it reaches a kernel import the isolated diff run cannot replay ("not comparable" in
// all 25 cases), so the gate can't check it.
// GT_DECOMP(0x08B0C99Cu, gt::game::func_08B0C99C, gt::ReturnKind::Word);
GT_DECOMP(0x08991B98u, gt::game::func_08991B98, gt::ReturnKind::Word);
GT_DECOMP(0x089DFAACu, gt::game::func_089DFAAC, gt::ReturnKind::Word);
GT_DECOMP(0x089DFAC0u, gt::game::func_089DFAC0, gt::ReturnKind::Word);
GT_DECOMP(0x089A29E4u, gt::game::func_089A29E4, gt::ReturnKind::Word);
GT_DECOMP(0x0895DCACu, gt::game::func_0895DCAC, gt::ReturnKind::Word);
GT_DECOMP(0x08B15490u, gt::game::func_08B15490, gt::ReturnKind::Word);
GT_DECOMP(0x088BB63Cu, gt::game::func_088BB63C, gt::ReturnKind::Float);
GT_DECOMP(0x088A5928u, gt::game::func_088A5928, gt::ReturnKind::Word);
GT_DECOMP(0x08932558u, gt::game::func_08932558, gt::ReturnKind::DoubleWord);
GT_DECOMP(0x088A5EE0u, gt::game::func_088A5EE0, gt::ReturnKind::DoubleWord);
// Not registered: it reaches a kernel import the isolated diff run cannot replay ("not comparable" in
// all 23 cases), so the gate can't check it.
// GT_DECOMP(0x08B1C47Cu, gt::game::func_08B1C47C, gt::ReturnKind::Word);
GT_DECOMP(0x08992A4Cu, gt::game::func_08992A4C, gt::ReturnKind::Word);
GT_DECOMP(0x08936578u, gt::game::func_08936578, gt::ReturnKind::Word);
GT_DECOMP(0x089E4BB8u, gt::game::func_089E4BB8, gt::ReturnKind::Word);
GT_DECOMP(0x08AE9F78u, gt::game::func_08AE9F78, gt::ReturnKind::Word);
GT_DECOMP(0x089660E8u, gt::game::func_089660E8, gt::ReturnKind::Word);
GT_DECOMP(0x08ABCB7Cu, gt::game::func_08ABCB7C, gt::ReturnKind::Word);
GT_DECOMP(0x08ABCB44u, gt::game::func_08ABCB44, gt::ReturnKind::Word);
GT_DECOMP(0x08ABCB0Cu, gt::game::func_08ABCB0C, gt::ReturnKind::Word);
GT_DECOMP(0x08ABCAD4u, gt::game::func_08ABCAD4, gt::ReturnKind::Word);
GT_DECOMP(0x08ABC8F4u, gt::game::func_08ABC8F4, gt::ReturnKind::Word);
GT_DECOMP(0x08ABA818u, gt::game::func_08ABA818, gt::ReturnKind::Word);
GT_DECOMP(0x08ABA7E8u, gt::game::func_08ABA7E8, gt::ReturnKind::Word);
GT_DECOMP(0x08ABA7B8u, gt::game::func_08ABA7B8, gt::ReturnKind::Word);
GT_DECOMP(0x08ABA788u, gt::game::func_08ABA788, gt::ReturnKind::Word);
GT_DECOMP(0x08ABA674u, gt::game::func_08ABA674, gt::ReturnKind::Word);
GT_DECOMP(0x08926440u, gt::game::func_08926440, gt::ReturnKind::Void);
GT_DECOMP(0x08A6C078u, gt::game::func_08A6C078, gt::ReturnKind::Word);
GT_DECOMP(0x08AAB5DCu, gt::game::func_08AAB5DC, gt::ReturnKind::Word);
GT_DECOMP(0x08B0DC34u, gt::game::func_08B0DC34, gt::ReturnKind::Word);
GT_DECOMP(0x08A719F0u, gt::game::func_08A719F0, gt::ReturnKind::Word);
GT_DECOMP(0x089624ECu, gt::game::func_089624EC, gt::ReturnKind::Word);
GT_DECOMP(0x0885D4CCu, gt::game::func_0885D4CC, gt::ReturnKind::Word);
GT_DECOMP(0x08B18DC4u, gt::game::func_08B18DC4, gt::ReturnKind::Word);
GT_DECOMP(0x08AD1D70u, gt::game::func_08AD1D70, gt::ReturnKind::Word);
GT_DECOMP(0x08A5CC40u, gt::game::func_08A5CC40, gt::ReturnKind::Word);
GT_DECOMP(0x08AE2C68u, gt::game::func_08AE2C68, gt::ReturnKind::Word);
GT_DECOMP(0x08977390u, gt::game::func_08977390, gt::ReturnKind::Void);

// --- batch 30 ---

namespace gt::game {
namespace {

constexpr std::uint32_t kDefaultA46C = 0x08C8A46Cu;   // what func_088FAF9C returns with no object
constexpr std::uint32_t kVtable3000 = 0x08CC3000u;    // vtables the constructors below install
constexpr std::uint32_t kVtable6110 = 0x08CC6110u;
constexpr std::uint32_t kVtable9D40 = 0x08CC9D40u;
constexpr std::uint32_t kVtable9AE8 = 0x08CC9AE8u;
constexpr std::uint32_t kListHead8ADC = 0x08D68ADCu;  // the head of the list func_08B1DDEC pushes onto
constexpr std::uint32_t kLockF450 = 0x08CFF450u;      // the lock func_08A7BE84 holds
constexpr std::uint32_t kObject705C = 0x08CF705Cu;    // ... around func_08AAD7A0 on this object

// The virtual at `slot` of *holder (object, holder, x), 0 when either is null (as func_08ABBE00).
void holder_call_or0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t slot) {
    const std::uint32_t holder = ctx.gpr[4];
    const std::uint32_t x = ctx.gpr[5];
    ctx.gpr[6] = x;
    const std::uint32_t object = holder != 0u ? word(rt, holder) : 0u;
    if (object == 0u) {
        if (holder != 0u) {
            ctx.gpr[4] = 0u;
            ctx.gpr[5] = holder;
        }
        ctx.gpr[2] = 0u;
        return;
    }
    ctx.gpr[2] = word(rt, object);
    tail_call(ctx, word(rt, word(rt, object) + slot), {object, holder, x});
}

} // namespace

// func_08AF7420(this): clears bit 7 of the first byte, calling func_08B5281C(the word at +0x8) first
// when it is set.
void func_08AF7420(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t value = rt.memory().load8(self);
    if ((value & 0x80u) != 0u) {
        {
            GuestFrame frame(ctx, 0x10u);
            (void)call_guest(rt, ctx, 0x08B5281Cu, {word(rt, self + 0x8u)});
        }
        value = rt.memory().load8(self);
    }
    rt.memory().store8(self, static_cast<std::uint8_t>(value & 0x7Fu));
    ctx.gpr[2] = value & 0x7Fu;
    ctx.gpr[3] = value;
}

// func_08A68070() -> a new 8-byte object constructed by func_08A6804C.
void func_08A68070(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t object = call_guest(rt, ctx, kAllocate, {8u});
    (void)call_guest(rt, ctx, 0x08A6804Cu, {object});
    ctx.gpr[2] = object;
}

// Forwarders: func_089D8A54(key) -> func_08A0FD20(key, the table at 0x08CF6ED8); func_0894E9B4(this, a)
// -> func_0894E87C(this, a, 0, 0.0); func_08B0EA48(a, b) -> func_08B112FC(b, a).
void func_089D8A54(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x08A0FD20u, {ctx.gpr[4], kLookupTable}); }
void func_0894E9B4(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    ctx.fpr[13] = 0.0f;
    ctx.gpr[6] = 0u;
    tail_call(ctx, 0x0894E87Cu, {});
}
void func_08B0EA48(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = ctx.gpr[4];
    tail_call(ctx, 0x08B112FCu, {ctx.gpr[5], ctx.gpr[4]});
}

// func_089448AC(this, i, value) -> 1 after setting byte i of the table at +0x53F of the object at +0x8,
// for 0 < i < 7; 0 otherwise.
void func_089448AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const auto i = static_cast<std::int32_t>(ctx.gpr[5]);
    const std::uint32_t object = word(rt, ctx.gpr[4] + 0x8u);
    ctx.gpr[3] = object;
    ctx.gpr[7] = i < 7 ? 1u : 0u;
    ctx.gpr[2] = 0u;
    if (i > 0 && i < 7) {
        rt.memory().store8(ctx.gpr[5] + object + 0x53Fu, static_cast<std::uint8_t>(ctx.gpr[6]));
        ctx.gpr[2] = 1u;
    }
}

// func_088FAF74(this) -> the word at +0xAC of the second object (+0x4), else the halfword at +0x52 of
// the first (+0x0), else 0.
void func_088FAF74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t second = word(rt, ctx.gpr[4] + 0x4u);
    ctx.gpr[3] = second;
    if (second != 0u) {
        ctx.gpr[2] = word(rt, second + 0xACu);
        return;
    }
    const std::uint32_t first = word(rt, ctx.gpr[4]);
    ctx.gpr[4] = first;
    ctx.gpr[2] = first != 0u ? rt.memory().load16(first + 0x52u) : 0u;
}

// func_088FAF9C(this, i) -> &entry i (16 bytes) of the second object's table at +0xB0, else of the
// first object's table at +0x90, else the default at 0x08C8A46C.
void func_088FAF9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t second = word(rt, ctx.gpr[4] + 0x4u);
    const std::uint32_t offset = ctx.gpr[5] << 4;
    ctx.gpr[3] = offset;
    if (second != 0u) {
        ctx.gpr[2] = word(rt, second + 0xB0u) + offset;
        return;
    }
    const std::uint32_t first = word(rt, ctx.gpr[4]);
    ctx.gpr[4] = first;
    ctx.gpr[5] = offset;
    ctx.gpr[2] = first != 0u ? word(rt, first + 0x90u) + offset : kDefaultA46C;
}

// func_08AD3C14(a) -> func_08AD39E4(context, a), 0 without the context at 0x08D404CC.
void func_08AD3C14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t context = word(rt, kContext04CC);
    const std::uint32_t a = ctx.gpr[4];
    ctx.gpr[3] = context;
    ctx.gpr[2] = kContext04CC & 0xFFFF0000u;
    if (context == 0u) {
        ctx.gpr[4] = 0u;
        ctx.gpr[5] = a;
        ctx.gpr[2] = 0u;
        return;
    }
    tail_call(ctx, 0x08AD39E4u, {context, a});
}

// Holder virtuals: func_08ABCAA4 / func_08ABCA74 (+0x4C / +0x48, 0 for null), func_08ABB61C (+0x3C),
// func_08ABB550 (+0x34 with the object alone, 0 for null).
void func_08ABCAA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_call_or0(rt, ctx, 0x4Cu); }
void func_08ABCA74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_call_or0(rt, ctx, 0x48u); }
void func_08ABB61C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_call(rt, ctx, 0x3Cu); }
void func_08ABB550(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t holder = ctx.gpr[4];
    const std::uint32_t object = holder != 0u ? word(rt, holder) : 0u;
    if (object == 0u) {
        if (holder != 0u) ctx.gpr[4] = 0u;
        ctx.gpr[2] = 0u;
        return;
    }
    ctx.gpr[2] = word(rt, object);
    tail_call(ctx, word(rt, word(rt, object) + 0x34u), {object});
}

// func_08AA20F4(this) -> this, with a temporary built by func_08A85B1C (as func_08AA2C90).
void func_08AA20F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { with_temporary(rt, ctx, 0x08A85B1Cu, 0x08A76ABCu, true); }

// Word tables: func_089366EC(this, i) (+0x8C), func_089775B8 / func_089775C8(this, i[, value]) (read /
// write +0x15C).
void func_089366EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = word(rt, (ctx.gpr[5] << 2) + ctx.gpr[4] + 0x8Cu); }
void func_089775B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = word(rt, (ctx.gpr[5] << 2) + ctx.gpr[4] + 0x15Cu); }
void func_089775C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store32((ctx.gpr[5] << 2) + ctx.gpr[4] + 0x15Cu, ctx.gpr[6]);
}

// func_088B2E4C(this) -> func_088B2DB0(the object at +0x4).
void func_088B2E4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_on_field(rt, ctx, 0x4u, 0x088B2DB0u); }

// func_0897D534(this, a, b, c): stores a, b, c and a cleared word, then func_0897D4AC with the same
// arguments.
void func_0897D534(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    rt.memory().store32(self + 0x0u, ctx.gpr[5]);
    rt.memory().store32(self + 0x4u, ctx.gpr[6]);
    rt.memory().store32(self + 0x8u, ctx.gpr[7]);
    rt.memory().store32(self + 0xCu, 0u);
    tail_call(ctx, 0x0897D4ACu, {});
}

// func_08927E44(this) -> this + 0x2058; func_08917848(this, i) -> the word at +0x70 of
// func_08927E44(the object i at +0x8).
void func_08927E44(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = ctx.gpr[4] + 0x2058u; }
void func_08917848(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, (ctx.gpr[5] << 2) + ctx.gpr[4] + 0x8u);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = word(rt, call_guest(rt, ctx, 0x08927E44u, {object}) + 0x70u);
}

// func_0890B030(holder, bit, bool on): sets or clears bit `bit` of the word *holder points to.
void func_0890B030(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t mask = 1u << (ctx.gpr[5] & 31u);
    const std::uint32_t target = word(rt, ctx.gpr[4]);
    const std::uint32_t value = (ctx.gpr[6] & 0xFFu) != 0u ? (word(rt, target) | mask) : (word(rt, target) & ~mask);
    rt.memory().store32(target, value);
    ctx.gpr[2] = value;
    ctx.gpr[3] = mask;
    ctx.gpr[4] = target;
    ctx.gpr[5] = ~mask;
    ctx.gpr[6] &= 0xFFu;
}

// func_08B2E1F8(int *p): *p = -1.
void func_08B2E1F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = 0xFFFFFFFFu;
    rt.memory().store32(ctx.gpr[4], 0xFFFFFFFFu);
}

// Constructors: func_08AC0640 (vtable 0x08CC3000, cleared fields), func_08925FE4(this, x) (x at +0x4,
// vtable 0x08CC6110), func_08995714 (vtables at +0x38 and +0x0, then func_08994D78).
void func_08AC0640(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    rt.memory().store32(self, kVtable3000);
    for (const std::uint32_t offset : {0x1Cu, 0x4u, 0x8u}) rt.memory().store32(self + offset, 0u);
    for (const std::uint32_t offset : {0xCu, 0xDu, 0xEu}) rt.memory().store8(self + offset, 0u);
    for (const std::uint32_t offset : {0x10u, 0x14u, 0x18u}) rt.memory().store32(self + offset, 0u);
    ctx.gpr[2] = kVtable3000;
}
void func_08925FE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store32(ctx.gpr[4] + 0x4u, ctx.gpr[5]);
    store_vtable(rt, ctx, kVtable6110);
}
void func_08995714(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store32(ctx.gpr[4] + 0x38u, kVtable9D40);
    rt.memory().store32(ctx.gpr[4], kVtable9AE8);
    ctx.gpr[3] = kVtable9D40;
    ctx.gpr[5] = kVtable9AE8;
    tail_call(ctx, 0x08994D78u, {ctx.gpr[4]});
}

// func_08936544(this, a, i, x) -> func_08938CB4(the object i at +0x13C, a, x), 0 without it.
void func_08936544(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, (ctx.gpr[6] << 2) + ctx.gpr[4] + 0x13Cu);
    ctx.gpr[6] = ctx.gpr[7];
    ctx.gpr[4] = object;
    if (object == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    ctx.gpr[2] = object;
    tail_call(ctx, 0x08938CB4u, {object, ctx.gpr[5], ctx.gpr[6]});
}

// func_08B207AC(this, a, b): +0x4 = a, +0x8 = max(a, b) (signed), then func_08B20760(this, a, b).
void func_08B207AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const bool below = static_cast<std::int32_t>(ctx.gpr[6]) < static_cast<std::int32_t>(ctx.gpr[5]);
    rt.memory().store32(ctx.gpr[4] + 0x8u, below ? ctx.gpr[5] : ctx.gpr[6]);
    rt.memory().store32(ctx.gpr[4] + 0x4u, ctx.gpr[5]);
    ctx.gpr[2] = below ? 1u : 0u;
    tail_call(ctx, 0x08B20760u, {});
}

// func_08B1DDEC(node, a, b, c): fills node (c, b, a, 0, 0) and pushes it onto the global list at
// 0x08D68ADC (next at +0x14).
void func_08B1DDEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t node = ctx.gpr[4];
    rt.memory().store32(node + 0x0u, ctx.gpr[7]);
    rt.memory().store32(node + 0x4u, ctx.gpr[6]);
    rt.memory().store32(node + 0x8u, ctx.gpr[5]);
    rt.memory().store32(node + 0xCu, 0u);
    rt.memory().store32(node + 0x10u, 0u);
    const std::uint32_t head = word(rt, kListHead8ADC);
    rt.memory().store32(node + 0x14u, head);
    rt.memory().store32(kListHead8ADC, node);
    ctx.gpr[2] = head;
    ctx.gpr[3] = 0x08D70000u;
}

// func_08A008BC() -> the length of the chain func_08A0085C() starts, following func_08968E70.
void func_08A008BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    std::uint32_t count = 0u;
    for (std::uint32_t node = call_guest(rt, ctx, 0x08A0085Cu, {}); node != 0u; ++count)
        node = call_guest(rt, ctx, 0x08968E70u, {node});
    ctx.gpr[2] = count;
}

// func_089D9E30(this): func_089D9B88(this, the float at +0x9C), then +0xA0 = the word at +0x80.
void func_089D9E30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x089D9B88u, {self}, {float_at(rt, self + 0x9Cu)});
    }
    ctx.gpr[2] = word(rt, self + 0x80u);
    rt.memory().store32(self + 0xA0u, ctx.gpr[2]);
}

// func_089324DC(this) -> 1, clearing the bytes at +0x154 and +0x153; func_08B1D5B0(this, x): +0x8 = x,
// +0x0 = 6.
void func_089324DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store8(ctx.gpr[4] + 0x154u, 0u);
    rt.memory().store8(ctx.gpr[4] + 0x153u, 0u);
    ctx.gpr[2] = 1u;
}
void func_08B1D5B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store32(ctx.gpr[4] + 0x8u, ctx.gpr[5]);
    rt.memory().store32(ctx.gpr[4], 6u);
    ctx.gpr[2] = 6u;
}

// func_088FB17C(this) -> func_089010EC(second) or func_089035B4(first) (a float; 0.0 with neither).
void func_088FB17C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    if (word(rt, ctx.gpr[4] + 0x4u) == 0u) ctx.fpr[0] = 0.0f;
    pair_dispatch(rt, ctx, 0x089010ECu, 0x089035B4u);
}

// func_08A69798(node **slot, node *n): stores n and takes a reference to it.
void func_08A69798(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t node = ctx.gpr[5];
    rt.memory().store32(ctx.gpr[4], node);
    ctx.gpr[4] = node;
    if (node != 0u) tail_call(ctx, kRetainNode, {node});
}

// func_088A6C18(this, const word *p) -> this, after func_088A5450(*this, *p).
void func_088A6C18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x088A5450u, {word(rt, self), word(rt, ctx.gpr[5])});
    }
    ctx.gpr[2] = self;
}

// func_08B28BE8(this): func_08B34C1C(this) when the byte at +0x28 is set, else sets the byte at +0x29.
void func_08B28BE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = 1u;
    if (rt.memory().load8(ctx.gpr[4] + 0x28u) != 0u) {
        tail_call(ctx, 0x08B34C1Cu, {ctx.gpr[4]});
        return;
    }
    rt.memory().store8(ctx.gpr[4] + 0x29u, 1u);
}

// func_08B0DE84(this, const word src[3]): copies src to +0x80 and points +0x8C at it; +0x8C = 0 for a
// null src.
void func_08B0DE84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t source = ctx.gpr[5];
    ctx.gpr[6] = self;
    std::uint32_t pointer = 0u;
    if (source != 0u) {
        const std::uint32_t c = word(rt, source + 0x8u);
        const std::uint32_t a = word(rt, source);
        const std::uint32_t b = word(rt, source + 0x4u);
        rt.memory().store32(self + 0x88u, c);
        rt.memory().store32(self + 0x80u, a);
        rt.memory().store32(self + 0x84u, b);
        pointer = self + 0x80u;
        ctx.gpr[2] = c;
        ctx.gpr[3] = a;
        ctx.gpr[4] = b;
    }
    ctx.gpr[5] = pointer;
    rt.memory().store32(self + 0x8Cu, pointer);
}

// func_08A7BE84(): func_08AAD7A0(the object at 0x08CF705C) under the lock at 0x08CFF450.
void func_08A7BE84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, kHeapLock, {kLockF450});
        (void)call_guest(rt, ctx, 0x08AAD7A0u, {kObject705C});
    }
    tail_call(ctx, kHeapUnlock, {kLockF450});
}

} // namespace gt::game

GT_DECOMP(0x08AF7420u, gt::game::func_08AF7420, gt::ReturnKind::Word);
GT_DECOMP(0x08A68070u, gt::game::func_08A68070, gt::ReturnKind::Word);
GT_DECOMP(0x089D8A54u, gt::game::func_089D8A54, gt::ReturnKind::Word);
GT_DECOMP(0x0894E9B4u, gt::game::func_0894E9B4, gt::ReturnKind::Word);
GT_DECOMP(0x08B0EA48u, gt::game::func_08B0EA48, gt::ReturnKind::Word);
GT_DECOMP(0x089448ACu, gt::game::func_089448AC, gt::ReturnKind::Word);
GT_DECOMP(0x088FAF74u, gt::game::func_088FAF74, gt::ReturnKind::Word);
GT_DECOMP(0x088FAF9Cu, gt::game::func_088FAF9C, gt::ReturnKind::Word);
GT_DECOMP(0x08AD3C14u, gt::game::func_08AD3C14, gt::ReturnKind::Word);
GT_DECOMP(0x08ABCAA4u, gt::game::func_08ABCAA4, gt::ReturnKind::Word);
GT_DECOMP(0x08ABCA74u, gt::game::func_08ABCA74, gt::ReturnKind::Word);
GT_DECOMP(0x08ABB61Cu, gt::game::func_08ABB61C, gt::ReturnKind::Word);
GT_DECOMP(0x08ABB550u, gt::game::func_08ABB550, gt::ReturnKind::Word);
GT_DECOMP(0x08AA20F4u, gt::game::func_08AA20F4, gt::ReturnKind::Word);
GT_DECOMP(0x089366ECu, gt::game::func_089366EC, gt::ReturnKind::Word);
GT_DECOMP(0x089775B8u, gt::game::func_089775B8, gt::ReturnKind::Word);
GT_DECOMP(0x089775C8u, gt::game::func_089775C8, gt::ReturnKind::Void);
GT_DECOMP(0x088B2E4Cu, gt::game::func_088B2E4C, gt::ReturnKind::Word);
GT_DECOMP(0x0897D534u, gt::game::func_0897D534, gt::ReturnKind::Word);
GT_DECOMP(0x08927E44u, gt::game::func_08927E44, gt::ReturnKind::Word);
GT_DECOMP(0x08917848u, gt::game::func_08917848, gt::ReturnKind::Word);
GT_DECOMP(0x0890B030u, gt::game::func_0890B030, gt::ReturnKind::Word);
GT_DECOMP(0x08B2E1F8u, gt::game::func_08B2E1F8, gt::ReturnKind::Word);
GT_DECOMP(0x08AC0640u, gt::game::func_08AC0640, gt::ReturnKind::Word);
// Not registered: the harness never sees a call to it on the three pad routes ("calls 0"), so it is
// untested.
// GT_DECOMP(0x08925FE4u, gt::game::func_08925FE4, gt::ReturnKind::Word);
GT_DECOMP(0x08995714u, gt::game::func_08995714, gt::ReturnKind::Word);
GT_DECOMP(0x08936544u, gt::game::func_08936544, gt::ReturnKind::Word);
GT_DECOMP(0x08B207ACu, gt::game::func_08B207AC, gt::ReturnKind::Word);
GT_DECOMP(0x08B1DDECu, gt::game::func_08B1DDEC, gt::ReturnKind::Word);
GT_DECOMP(0x08A008BCu, gt::game::func_08A008BC, gt::ReturnKind::Word);
GT_DECOMP(0x089D9E30u, gt::game::func_089D9E30, gt::ReturnKind::Word);
GT_DECOMP(0x089324DCu, gt::game::func_089324DC, gt::ReturnKind::Word);
GT_DECOMP(0x08B1D5B0u, gt::game::func_08B1D5B0, gt::ReturnKind::Word);
GT_DECOMP(0x088FB17Cu, gt::game::func_088FB17C, gt::ReturnKind::Float);
GT_DECOMP(0x08A69798u, gt::game::func_08A69798, gt::ReturnKind::Void);
GT_DECOMP(0x088A6C18u, gt::game::func_088A6C18, gt::ReturnKind::Word);
GT_DECOMP(0x08B28BE8u, gt::game::func_08B28BE8, gt::ReturnKind::Word);
GT_DECOMP(0x08B0DE84u, gt::game::func_08B0DE84, gt::ReturnKind::Void);
GT_DECOMP(0x08A7BE84u, gt::game::func_08A7BE84, gt::ReturnKind::Word);

// --- batch 31 ---

namespace gt::game {
namespace {

constexpr std::uint32_t kVtable9498 = 0x08CD9498u;    // vtables the constructors below install
constexpr std::uint32_t kVtable9360 = 0x08CD9360u;
constexpr std::uint32_t kContext88A0 = 0x08D688A0u;   // the object func_08B18A28 passes first
constexpr std::uint32_t kListHead73E4 = 0x08D773E4u;  // the head of the list func_08B1E364 pushes onto
constexpr std::uint32_t kFallback290C = 0x08CF290Cu;  // what func_0892A07C returns for null

// r = func_0892FFE0(a, b, &buffer[128], 128); then func_089300CC(kind, buffer), or -1 when r is 0.
void buffer_query(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t kind) {
    GuestFrame frame(ctx, 0x90u);
    const std::uint32_t found = call_guest(rt, ctx, 0x0892FFE0u, {ctx.gpr[4], ctx.gpr[5], frame.at(0u), 128u});
    ctx.gpr[2] = found != 0u ? call_guest(rt, ctx, 0x089300CCu, {kind, frame.at(0u)}) : 0xFFFFFFFFu;
}

// Stores the byte value at `offset` unless the byte at +0xA is set.
void store_unless_locked(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t offset) {
    ctx.gpr[5] &= 0xFFu;
    ctx.gpr[2] = rt.memory().load8(ctx.gpr[4] + 0xAu);
    if (ctx.gpr[2] == 0u) rt.memory().store8(ctx.gpr[4] + offset, static_cast<std::uint8_t>(ctx.gpr[5]));
}

} // namespace

// Sequences: func_089DB55C(this): func_0896B0E4(this), then func_089DB454(this, 1); func_08999F68(this):
// func_08A009BC(this), then func_08999CB4(this, 0); func_08954870(this): func_089547A0(this), then
// func_0895469C(this).
void func_089DB55C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x0896B0E4u, {self});
    }
    tail_call(ctx, 0x089DB454u, {self, 1u});
}
void func_08999F68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08A009BCu, {self});
    }
    tail_call(ctx, 0x08999CB4u, {self, 0u});
}
void func_08954870(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x089547A0u, {self});
    }
    tail_call(ctx, 0x0895469Cu, {self});
}

// Constructors and initialisers: func_08B1FD44 (vtable, two cleared words), func_088D2F9C, func_08B0B06C
// and func_08B0CE58 (cleared fields; the last then func_08B17E84(this + 0x24, 0)), func_08973CC4 (the
// byte at +0x14 = 1).
void func_08B1FD44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    store_vtable(rt, ctx, kVtable9498);
    rt.memory().store32(ctx.gpr[4] + 0x8u, 0u);
    rt.memory().store32(ctx.gpr[4] + 0x4u, 0u);
}
void func_088D2F9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    for (const std::uint32_t offset : {0x0u, 0x4u, 0x8u, 0xCu, 0x14u, 0x10u}) rt.memory().store32(self + offset, 0u);
    rt.memory().store8(self + 0x20u, 0u);
    rt.memory().store32(self + 0x18u, 0u);
    rt.memory().store32(self + 0x1Cu, 0u);
    ctx.gpr[2] = self + 0xCu;
}
void func_08B0B06C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    for (const std::uint32_t offset : {0x18u, 0x0u, 0x4u}) rt.memory().store32(self + offset, 0u);
    rt.memory().store8(self + 0x8u, 0u);
    for (const std::uint32_t offset : {0xCu, 0x10u, 0x14u}) rt.memory().store32(self + offset, 0u);
}
void func_08B0CE58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    for (const std::uint32_t offset : {0x0u, 0x10u, 0x18u, 0x14u, 0x1Cu, 0x4u, 0x8u, 0x20u}) rt.memory().store32(self + offset, 0u);
    ctx.gpr[2] = self;
    tail_call(ctx, 0x08B17E84u, {self + 0x24u, 0u});
}
void func_08973CC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store8(ctx.gpr[4] + 0x14u, 1u);
    ctx.gpr[2] = 1u;
}

// func_08ABC990(holder, a, b, u8 c) -> the virtual +0x18 of *holder (object, holder, a, b, c), 0 when
// either is null.
void func_08ABC990(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t holder = ctx.gpr[4];
    const std::uint32_t a = ctx.gpr[5];
    const std::uint32_t b = ctx.gpr[6];
    const std::uint32_t c = ctx.gpr[7] & 0xFFu;
    ctx.gpr[3] = a;
    ctx.gpr[9] = b;
    ctx.gpr[8] = c;
    const std::uint32_t object = holder != 0u ? word(rt, holder) : 0u;
    if (object == 0u) {
        if (holder != 0u) {
            ctx.gpr[4] = 0u;
            ctx.gpr[5] = holder;
            ctx.gpr[6] = a;
            ctx.gpr[7] = b;
        }
        ctx.gpr[2] = 0u;
        return;
    }
    ctx.gpr[2] = word(rt, object);
    tail_call(ctx, word(rt, word(rt, object) + 0x18u), {object, holder, a, b, c});
}

// Field forwarders: func_08A4D2D4(this) -> func_08A50758(the object at +0x78); func_08A4D2F8(this) ->
// func_08A503C8(the same object); func_089F0AF4(this) -> func_08B0B8CC(this + 0xC);
// func_08B18A28(x) -> func_08B20968(the object at 0x08D688A0, x); func_0893273C(pair) ->
// func_08930290(pair[0], pair[1]).
void func_08A4D2D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_on_field(rt, ctx, 0x78u, 0x08A50758u); }
void func_08A4D2F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x08A503C8u, {word(rt, ctx.gpr[4] + 0x78u)}); }
void func_089F0AF4(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x08B0B8CCu, {ctx.gpr[4] + 0xCu}); }
void func_08B18A28(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x08B20968u, {kContext88A0, ctx.gpr[4]}); }
void func_0893273C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    tail_call(ctx, 0x08930290u, {word(rt, ctx.gpr[4]), word(rt, ctx.gpr[4] + 0x4u)});
}

// func_08930204 / func_089301C4(a, b) -> func_089300CC(34 / 0, buffer) for the 128-byte buffer
// func_0892FFE0(a, b, buffer, 128) fills, or -1 when it fails.
void func_08930204(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { buffer_query(rt, ctx, 34u); }
void func_089301C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { buffer_query(rt, ctx, 0u); }

void func_08A05C98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { record_gate(rt, ctx, 0x08C10E60u); }

// func_089B3560(this, x) -> this, after func_08AA4748(this, x + 0x118).
void func_089B3560(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08AA4748u, {self, ctx.gpr[5] + 0x118u});
    }
    ctx.gpr[2] = self;
}

// func_0893A814(this, bool reset): the bytes at +0x1..+0x3 = 4, +0x8/+0x10/+0x18 = 0 when reset, and the
// byte at +0x21 capped at 50.
void func_0893A814(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    ctx.gpr[5] &= 0xFFu;
    rt.memory().store8(self + 0x3u, 4u);
    rt.memory().store8(self + 0x1u, 4u);
    rt.memory().store8(self + 0x2u, 4u);
    if (ctx.gpr[5] != 0u)
        for (const std::uint32_t offset : {0x8u, 0x10u, 0x18u}) rt.memory().store8(self + offset, 0u);
    if (rt.memory().load8(self + 0x21u) >= 51u) rt.memory().store8(self + 0x21u, 50u);
    ctx.gpr[2] = 50u;
}

// func_0890337C(this, i, j) -> the float i of row j (rows of 8 bytes at +0x58, the float array at
// +0x4 of each), 0.0 when j is not below the u16 count at +0x38.
void func_0890337C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t j = ctx.gpr[6];
    ctx.gpr[3] = j << 3;
    ctx.gpr[5] <<= 2;
    ctx.gpr[2] = static_cast<std::int32_t>(j) < static_cast<std::int32_t>(rt.memory().load16(self + 0x38u)) ? 1u : 0u;
    ctx.fpr[0] = 0.0f;
    if (ctx.gpr[2] == 0u) return;
    ctx.gpr[2] = (j << 3) + word(rt, self + 0x58u);
    ctx.gpr[3] = ctx.gpr[5] + word(rt, ctx.gpr[2] + 0x4u);
    ctx.fpr[0] = float_at(rt, ctx.gpr[3]);
}

// func_088FAE64(this) -> func_08900E14(second) or func_0890337C(first) (a float; 0.0 with neither).
void func_088FAE64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    if (word(rt, ctx.gpr[4] + 0x4u) == 0u) ctx.fpr[0] = 0.0f;
    pair_dispatch(rt, ctx, 0x08900E14u, 0x0890337Cu);
}

// func_088F88E4 / func_088F88FC(this, u8 value): the byte at +0x9 / +0x8 unless the one at +0xA is set.
void func_088F88E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_unless_locked(rt, ctx, 0x9u); }
void func_088F88FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_unless_locked(rt, ctx, 0x8u); }

// func_088EF524: a four-step countdown (a short delay), then func_088EF448.
void func_088EF524(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = 0xFFFFFFFFu;
    tail_call(ctx, 0x088EF448u, {});
}

// func_088A6164(holder) -> func_08932790(&row) for the row (signed byte at +0x8 of *holder, rows of
// 384 bytes at +0x38).
void func_088A6164(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4]);
    const auto row = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(rt.memory().load8(object + 0x8u))));
    ctx.gpr[2] = object;
    ctx.gpr[3] = row << 7;
    tail_call(ctx, 0x08932790u, {row * 384u + object + 0x38u});
}

// func_088FAED4(this) -> the word at +0x4C of the second object, else the halfword at +0x3A of the
// first, else 0 (as func_088FAF74).
void func_088FAED4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t second = word(rt, ctx.gpr[4] + 0x4u);
    ctx.gpr[3] = second;
    if (second != 0u) {
        ctx.gpr[2] = word(rt, second + 0x4Cu);
        return;
    }
    const std::uint32_t first = word(rt, ctx.gpr[4]);
    ctx.gpr[4] = first;
    ctx.gpr[2] = first != 0u ? rt.memory().load16(first + 0x3Au) : 0u;
}

// func_08B1E364(this, x): the vtable, x at +0xC, and the node at +0x4 pushed onto the global list at
// 0x08D773E4 (next +0x4, owner +0x8).
void func_08B1E364(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t node = self + 0x4u;
    rt.memory().store32(self, kVtable9360);
    const std::uint32_t head = word(rt, kListHead73E4);
    rt.memory().store32(node + 0x4u, self);
    rt.memory().store32(self + 0x4u, head);
    rt.memory().store32(kListHead73E4, node);
    rt.memory().store32(self + 0xCu, ctx.gpr[5]);
    ctx.gpr[2] = head;
    ctx.gpr[3] = node;
    ctx.gpr[6] = kListHead73E4 & 0xFFFF0000u;
}

// func_08991FE0(this) -> func_08991E44(this), clearing +0xA0 after.
void func_08991FE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t result;
    {
        GuestFrame frame(ctx, 0x10u);
        result = call_guest(rt, ctx, 0x08991E44u, {self});
    }
    rt.memory().store32(self + 0xA0u, 0u);
    ctx.gpr[2] = result;
}

// func_08937D90(this, i) -> entry i of the table at +0x4: table + 0x10 + count × 4 + offsets[i] + 2
// (count at +0x4, offsets at +0x10), 0 when i is out of range.
void func_08937D90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t table = word(rt, ctx.gpr[4] + 0x4u);
    const std::uint32_t i = ctx.gpr[5];
    const std::uint32_t count = word(rt, table + 0x4u);
    const std::uint32_t strings = table + 0x10u + (count << 2);
    ctx.gpr[7] = (i << 2) + table;
    ctx.gpr[6] = strings;
    std::uint32_t result = 0u;
    if (i < count) result = strings + word(rt, (i << 2) + table + 0x10u) + 2u;
    ctx.gpr[3] = i < count ? 1u : 0u;
    ctx.gpr[4] = result;
    ctx.gpr[2] = result;
}

// func_0881A8C0(a, b, c, d, i) -> the low nibble of byte i + 2 of the record func_0881A840(a, b, c, d)
// returns.
void func_0881A8C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t i = ctx.gpr[8];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t record = call_guest(rt, ctx, 0x0881A840u, {});
    ctx.gpr[2] = rt.memory().load8(record + i + 0x2u) & 0xFu;
}

// func_0892A07C(...) -> func_08929FD8(...), or the default at 0x08CF290C when it returns null.
void func_0892A07C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t found = call_guest(rt, ctx, 0x08929FD8u, {});
    ctx.gpr[2] = found != 0u ? found : kFallback290C;
}

// Floats of the object at +0xC: func_08955284 (+0x148), func_0895526C (+0x140), func_08955260 (+0x68).
void func_08955284(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { object12_float(rt, ctx, 0x148u); }
void func_0895526C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { object12_float(rt, ctx, 0x140u); }
void func_08955260(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { object12_float(rt, ctx, 0x68u); }

// func_08B1FC48(list, node): unlinks node from the doubly linked list of func_08B1FBEC.
void func_08B1FC48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t list = ctx.gpr[4];
    const std::uint32_t node = ctx.gpr[5];
    const std::uint32_t prev = word(rt, node + 0x4u);
    const std::uint32_t next = word(rt, node + 0x8u);
    if (prev != 0u) rt.memory().store32(prev + 0x8u, next);
    else rt.memory().store32(list + 0x4u, next);
    if (next == 0u) rt.memory().store32(list + 0x8u, prev);
    else rt.memory().store32(next + 0x4u, prev);
    rt.memory().store32(node + 0x8u, 0u);
    rt.memory().store32(node + 0x4u, 0u);
    ctx.gpr[2] = next;
    ctx.gpr[3] = prev;
}

// func_08AC05F4(this): clears the four words at +0x6C..+0x78 of the object at +0x8 when it is set.
void func_08AC05F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4] + 0x8u);
    ctx.gpr[2] = object;
    if (object == 0u) return;
    for (const std::uint32_t offset : {0x6Cu, 0x78u, 0x74u, 0x70u}) rt.memory().store32(object + offset, 0u);
}

// func_08AA2C10(this) -> this, with a temporary built by func_08A5FA8C (as func_08AA2C90).
void func_08AA2C10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { with_temporary(rt, ctx, 0x08A5FA8Cu, 0x08A76ABCu, true); }

} // namespace gt::game

GT_DECOMP(0x089DB55Cu, gt::game::func_089DB55C, gt::ReturnKind::Word);
GT_DECOMP(0x08999F68u, gt::game::func_08999F68, gt::ReturnKind::Word);
GT_DECOMP(0x08954870u, gt::game::func_08954870, gt::ReturnKind::Word);
GT_DECOMP(0x08B1FD44u, gt::game::func_08B1FD44, gt::ReturnKind::Word);
GT_DECOMP(0x088D2F9Cu, gt::game::func_088D2F9C, gt::ReturnKind::Word);
GT_DECOMP(0x08B0B06Cu, gt::game::func_08B0B06C, gt::ReturnKind::Void);
GT_DECOMP(0x08B0CE58u, gt::game::func_08B0CE58, gt::ReturnKind::Word);
GT_DECOMP(0x08973CC4u, gt::game::func_08973CC4, gt::ReturnKind::Word);
GT_DECOMP(0x08ABC990u, gt::game::func_08ABC990, gt::ReturnKind::Word);
// Not registered: it reaches a kernel import the isolated diff run cannot replay ("not comparable" in
// all 18 cases), so the gate can't check it.
// GT_DECOMP(0x08A4D2D4u, gt::game::func_08A4D2D4, gt::ReturnKind::Word);
// Not registered: it reaches a kernel import the isolated diff run cannot replay ("not comparable" in
// all 17 cases), so the gate can't check it.
// GT_DECOMP(0x08A4D2F8u, gt::game::func_08A4D2F8, gt::ReturnKind::Word);
// Not registered: it reaches a kernel import the isolated diff run cannot replay ("not comparable" in
// all 17 cases), so the gate can't check it.
// GT_DECOMP(0x089F0AF4u, gt::game::func_089F0AF4, gt::ReturnKind::Word);
GT_DECOMP(0x08B18A28u, gt::game::func_08B18A28, gt::ReturnKind::Word);
GT_DECOMP(0x0893273Cu, gt::game::func_0893273C, gt::ReturnKind::Word);
GT_DECOMP(0x08930204u, gt::game::func_08930204, gt::ReturnKind::Word);
GT_DECOMP(0x089301C4u, gt::game::func_089301C4, gt::ReturnKind::Word);
GT_DECOMP(0x08A05C98u, gt::game::func_08A05C98, gt::ReturnKind::Word);
GT_DECOMP(0x089B3560u, gt::game::func_089B3560, gt::ReturnKind::Word);
GT_DECOMP(0x0893A814u, gt::game::func_0893A814, gt::ReturnKind::Word);
GT_DECOMP(0x0890337Cu, gt::game::func_0890337C, gt::ReturnKind::Float);
GT_DECOMP(0x088FAE64u, gt::game::func_088FAE64, gt::ReturnKind::Float);
GT_DECOMP(0x088F88E4u, gt::game::func_088F88E4, gt::ReturnKind::Word);
GT_DECOMP(0x088F88FCu, gt::game::func_088F88FC, gt::ReturnKind::Word);
GT_DECOMP(0x088EF524u, gt::game::func_088EF524, gt::ReturnKind::Word);
GT_DECOMP(0x088A6164u, gt::game::func_088A6164, gt::ReturnKind::Word);
GT_DECOMP(0x088FAED4u, gt::game::func_088FAED4, gt::ReturnKind::Word);
GT_DECOMP(0x08B1E364u, gt::game::func_08B1E364, gt::ReturnKind::Word);
GT_DECOMP(0x08991FE0u, gt::game::func_08991FE0, gt::ReturnKind::Word);
GT_DECOMP(0x08937D90u, gt::game::func_08937D90, gt::ReturnKind::Word);
GT_DECOMP(0x0881A8C0u, gt::game::func_0881A8C0, gt::ReturnKind::Word);
GT_DECOMP(0x0892A07Cu, gt::game::func_0892A07C, gt::ReturnKind::Word);
GT_DECOMP(0x08955284u, gt::game::func_08955284, gt::ReturnKind::Float);
GT_DECOMP(0x0895526Cu, gt::game::func_0895526C, gt::ReturnKind::Float);
GT_DECOMP(0x08955260u, gt::game::func_08955260, gt::ReturnKind::Float);
GT_DECOMP(0x08B1FC48u, gt::game::func_08B1FC48, gt::ReturnKind::Word);
GT_DECOMP(0x08AC05F4u, gt::game::func_08AC05F4, gt::ReturnKind::Word);
GT_DECOMP(0x08AA2C10u, gt::game::func_08AA2C10, gt::ReturnKind::Word);

// --- batch 32 ---

namespace gt::game {
namespace {

constexpr std::uint32_t kFlag8AE0 = 0x08D68AE0u;      // the byte func_08B1E1C0 checks
constexpr std::uint32_t kList88EC = 0x08D688ECu;      // the list func_08B18E40 appends to (head, tail, count)
constexpr std::uint32_t kVtable7C38 = 0x08CD7C38u;    // vtables the constructors below install
constexpr std::uint32_t kVtable60B8 = 0x08CC60B8u;
constexpr std::uint32_t kVtable9620 = 0x08CD9620u;
constexpr std::uint32_t kHookFB38 = 0x08CDFB38u;      // the function pointer func_08AD85C8 calls
constexpr std::uint32_t kFarValue = 0x481907FFu;      // the float func_088D07C0 fills with (156703.98)

// Appends node to a singly linked list (head at list+0, tail at list+4, count at list+8, the node's
// next at node+`next`).
void append_node(psprecomp::Runtime &rt, std::uint32_t list, std::uint32_t node, std::uint32_t next) {
    const std::uint32_t tail = word(rt, list + 0x4u);
    rt.memory().store32(list + 0x4u, node);
    if (tail != 0u) rt.memory().store32(tail + next, node);
    else rt.memory().store32(list, node);
    rt.memory().store32(node == 0u ? 0u : node + next, 0u);
    rt.memory().store32(list + 0x8u, word(rt, list + 0x8u) + 1u);
}

} // namespace

// Forwarders: func_08932730(pair) -> func_08930244(pair[0], pair[1]); func_089F0AFC(this) ->
// func_08B0B630(this + 0xC); func_08A94CE8(key) -> func_08A76AB4(key, the table at 0x08CF6ED8).
void func_08932730(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    tail_call(ctx, 0x08930244u, {word(rt, ctx.gpr[4]), word(rt, ctx.gpr[4] + 0x4u)});
}
void func_089F0AFC(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x08B0B630u, {ctx.gpr[4] + 0xCu}); }
void func_08A94CE8(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x08A76AB4u, {ctx.gpr[4], kLookupTable}); }

// func_088DA7F0(this, i) -> &entry i (0x2F8 bytes, table at +0xC0) + 0x200.
void func_088DA7F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[3] = ctx.gpr[5] * 0x2F8u;
    ctx.gpr[2] = word(rt, ctx.gpr[4] + 0xC0u) + ctx.gpr[3] + 0x200u;
}

// func_088D09A8(this, i) -> the difference of the words i and i-1 of the table at +0xBC (the first
// word alone for i = 0).
void func_088D09A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t i = ctx.gpr[5];
    if (i == 0u) {
        ctx.gpr[2] = word(rt, self + 0xC0u);
        return;
    }
    const std::uint32_t base = (i << 2) + self;
    ctx.gpr[3] = word(rt, base + 0xBCu);
    ctx.gpr[2] = word(rt, base + 0xC0u) - ctx.gpr[3];
}

// func_088D0580(this, i, x): the float table at +0x0, entry i.
void func_088D0580(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store32((ctx.gpr[5] << 2) + ctx.gpr[4], std::bit_cast<std::uint32_t>(ctx.fpr[12]));
}

// func_08B1E1C0(): func_08B1E14C() when the byte at 0x08D68AE0 is set.
void func_08B1E1C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = 0x08D70000u;
    ctx.gpr[3] = rt.memory().load8(kFlag8AE0);
    if (ctx.gpr[3] != 0u) tail_call(ctx, 0x08B1E14Cu, {});
}

// func_08977658(this, x): the callback at +0x1AC (x) when it is set.
void func_08977658(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t callback = word(rt, ctx.gpr[4] + 0x1ACu);
    ctx.gpr[25] = callback;
    ctx.gpr[4] = ctx.gpr[5];
    if (callback != 0u) tail_call(ctx, callback, {});
}

// Small readers: func_08945628 (1), func_088F8A14(this, i) (the signed byte i at +0x24), func_08B2080C
// (+0x8 minus +0x4), func_089499F8 (whether the byte at +0x158 is 9).
void func_08945628(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = 1u; }
void func_088F8A14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[4] += ctx.gpr[5];
    ctx.gpr[2] = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(rt.memory().load8(ctx.gpr[4] + 0x24u))));
}
void func_08B2080C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[3] = word(rt, ctx.gpr[4] + 0x4u);
    ctx.gpr[2] = word(rt, ctx.gpr[4] + 0x8u) - ctx.gpr[3];
}
void func_089499F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = rt.memory().load8(ctx.gpr[4] + 0x158u) == 9u ? 1u : 0u;
}

// Initialisers: func_0892020C (the byte at +0x10 and the word at +0xC cleared), func_08B2EE10(this, a,
// b) (a, flag, 0, b, -1), func_08B24D20(this, a, b, c) (a, b, c at +0x10, three cleared words),
// func_08B055EC (1.0 at +0x4, cleared elsewhere).
void func_0892020C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store8(ctx.gpr[4] + 0x10u, 0u);
    rt.memory().store32(ctx.gpr[4] + 0xCu, 0u);
}
void func_08B2EE10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    rt.memory().store32(self + 0x10u, 0xFFFFFFFFu);
    rt.memory().store32(self + 0x0u, ctx.gpr[5]);
    rt.memory().store32(self + 0xCu, ctx.gpr[6]);
    rt.memory().store8(self + 0x4u, 0u);
    rt.memory().store32(self + 0x8u, 0u);
    ctx.gpr[2] = 0xFFFFFFFFu;
}
void func_08B24D20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    rt.memory().store32(self + 0x10u, ctx.gpr[5]);
    rt.memory().store32(self + 0x14u, ctx.gpr[6]);
    rt.memory().store32(self + 0x18u, ctx.gpr[7]);
    for (const std::uint32_t offset : {0xCu, 0x4u, 0x8u}) rt.memory().store32(self + offset, 0u);
}
void func_08B055EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    rt.memory().store32(self + 0x4u, std::bit_cast<std::uint32_t>(1.0f));
    for (const std::uint32_t offset : {0x1Cu, 0xCu, 0x8u, 0x0u, 0x10u, 0x14u, 0x18u}) rt.memory().store32(self + offset, 0u);
    ctx.fpr[0] = 1.0f;
}

// Results of passed-through calls: func_089BD668 (the word at +0x14C of func_089BD4DC's result, 0 for
// null), func_0894CF58 (func_0894CF40's result + 12).
void func_089BD668(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t found = call_guest(rt, ctx, 0x089BD4DCu, {});
    ctx.gpr[2] = found != 0u ? word(rt, found + 0x14Cu) : 0u;
}
void func_0894CF58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, 0x0894CF40u, {}) + 12u;
}

// func_0893EB08(this): the byte at +0x12 = 1, 2 or 3 for the u16 at +0x36 below 4500, below 5000, or more.
void func_0893EB08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t value = rt.memory().load16(ctx.gpr[4] + 0x36u);
    ctx.gpr[3] = value < 4500u ? 1u : 0u;
    const std::uint32_t band = value < 4500u ? 1u : (value < 5000u ? 2u : 3u);
    rt.memory().store8(ctx.gpr[4] + 0x12u, static_cast<std::uint8_t>(band));
    ctx.gpr[2] = band;
}

// func_088D07C0(this): fills the word at +0x7C and the two 16-word tables at +0x80 and +0x144 with
// 0x481907FF.
void func_088D07C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t p = ctx.gpr[4];
    rt.memory().store32(p + 0x7Cu, kFarValue);
    for (std::uint32_t i = 0; i < 16u; ++i, p += 4u) {
        rt.memory().store32(p + 0x80u, kFarValue);
        rt.memory().store32(p + 0x144u, kFarValue);
    }
    ctx.gpr[4] = p;
    ctx.gpr[2] = kFarValue;
    ctx.gpr[3] = 16u;
    ctx.gpr[5] = 16u;
}

// func_08B18E40(node): appends node to the global list at 0x08D688EC (next at +0x0).
void func_08B18E40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t node = ctx.gpr[4];
    ctx.gpr[5] = word(rt, kList88EC + 0x4u);
    append_node(rt, kList88EC, node, 0x0u);
    ctx.gpr[3] = kList88EC;
    ctx.gpr[2] = word(rt, kList88EC + 0x8u);
}

// func_08889D4C(holder, node): appends node to the list *holder points to (next at +0x4).
void func_08889D4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t list = word(rt, ctx.gpr[4]);
    const std::uint32_t node = ctx.gpr[5];
    append_node(rt, list, node, 0x4u);
    ctx.gpr[4] = list;
    ctx.gpr[2] = node == 0u ? 0u : node + 0x4u;
    ctx.gpr[3] = word(rt, list + 0x8u);
}

// Constructors: func_08B0DDDC (vtable, then func_08B28BE0(this + 4)), func_08926D60 (vtable, a short
// delay, then func_08926C2C), func_08B24A0C (vtable).
void func_08B0DDDC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    store_vtable(rt, ctx, kVtable7C38);
    tail_call(ctx, 0x08B28BE0u, {ctx.gpr[4] + 0x4u});
}
void func_08926D60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store32(ctx.gpr[4], kVtable60B8);
    ctx.gpr[2] = 0xFFFFFFFFu;
    tail_call(ctx, 0x08926C2Cu, {});
}
void func_08B24A0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable(rt, ctx, kVtable9620); }

// func_0893069C(a, b) -> the byte at +0x14 of the record func_089305CC(a, b, &record) fills.
void func_0893069C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    (void)call_guest(rt, ctx, 0x089305CCu, {ctx.gpr[4], ctx.gpr[5], frame.at(0u)});
    ctx.gpr[2] = rt.memory().load8(frame.at(0x14u));
}

// func_08897574(holder, value): the big-endian word at +0x9C of *holder.
void func_08897574(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4]);
    ctx.gpr[5] = bswap32(ctx.gpr[5]);
    ctx.gpr[2] = object;
    rt.memory().store32(object + 0x9Cu, ctx.gpr[5]);
}

// func_08AC0618(this): func_08AC05F4(this), then clears +0x8.
void func_08AC0618(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08AC05F4u, {self});
    }
    rt.memory().store32(self + 0x8u, 0u);
}

// func_08993BC8(this): func_08A02218(this), then func_089939D4(this, 0).
void func_08993BC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08A02218u, {self});
    }
    tail_call(ctx, 0x089939D4u, {self, 0u});
}

// func_0895EDA0(this, float out[2]): the two floats at +0x13D0 divided by 19.6.
void func_0895EDA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const float divisor = std::bit_cast<float>(0x419CCCCDu);
    for (std::uint32_t i = 0; i < 2u; ++i) {
        ctx.fpr[0] = float_at(rt, ctx.gpr[4] + 0x13D0u) / divisor;
        rt.memory().store32(ctx.gpr[5], std::bit_cast<std::uint32_t>(ctx.fpr[0]));
        ctx.gpr[4] += 4u;
        ctx.gpr[5] += 4u;
    }
    ctx.fpr[1] = divisor;
    ctx.gpr[2] = 2u;
    ctx.gpr[3] = 2u;
}

// func_088FAF3C(this, i) -> &entry i (16 bytes) of the second object's table at +0x50, else of the
// first's at +0x5C, else 0.
void func_088FAF3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t second = word(rt, ctx.gpr[4] + 0x4u);
    const std::uint32_t offset = ctx.gpr[5] << 4;
    ctx.gpr[3] = offset;
    if (second != 0u) {
        ctx.gpr[2] = word(rt, second + 0x50u) + offset;
        return;
    }
    const std::uint32_t first = word(rt, ctx.gpr[4]);
    ctx.gpr[4] = first;
    ctx.gpr[5] = offset;
    ctx.gpr[2] = first != 0u ? word(rt, first + 0x5Cu) + offset : 0u;
}

// func_08B18C3C(holder, a) -> 1 after the virtual +0x10 of *holder (object, a, holder[1]), 0 for null.
void func_08B18C3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t holder = ctx.gpr[4];
    const std::uint32_t object = word(rt, holder);
    std::uint32_t result = 0u;
    if (object != 0u) {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, word(rt, word(rt, object) + 0x10u), {object, ctx.gpr[5], word(rt, holder + 0x4u)});
        result = 1u;
    }
    ctx.gpr[2] = result;
}

// func_089F9C6C(i, ...) -> the first word of entry i (12 bytes) of the table func_089F9BFC(...) returns.
void func_089F9C6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t i = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t table = call_guest(rt, ctx, 0x089F9BFCu, {});
    ctx.gpr[2] = word(rt, word(rt, table) + i * 12u);
}

// func_089B3704(this): the virtual +0x130 of the object at +0x11C when set, then func_089BCB40(this).
void func_089B3704(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t object = word(rt, self + 0x11Cu);
    ctx.gpr[2] = object;
    if (object != 0u) {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, word(rt, word(rt, object) + 0x130u), {object});
    }
    tail_call(ctx, 0x089BCB40u, {self});
}

// func_08AF94CC(this, key) -> the word at +0x4 of the entry func_08C40C90(the object at +0x34, the u16
// at +0x12, key) finds, -1 when none.
void func_08AF94CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t entry = call_guest(rt, ctx, 0x08C40C90u, {word(rt, self + 0x34u), rt.memory().load16(self + 0x12u), ctx.gpr[5]});
    ctx.gpr[2] = entry != 0u ? word(rt, entry + 0x4u) : 0xFFFFFFFFu;
}

// func_08AD85C8(p, ...) -> the hook at 0x08CDFB38 (p, ...) for a non-null p.
void func_08AD85C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    if (ctx.gpr[4] == 0u) return;
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, word(rt, kHookFB38), {});
}

} // namespace gt::game

GT_DECOMP(0x08932730u, gt::game::func_08932730, gt::ReturnKind::Word);
GT_DECOMP(0x089F0AFCu, gt::game::func_089F0AFC, gt::ReturnKind::Word);
GT_DECOMP(0x08A94CE8u, gt::game::func_08A94CE8, gt::ReturnKind::Word);
GT_DECOMP(0x088DA7F0u, gt::game::func_088DA7F0, gt::ReturnKind::Word);
GT_DECOMP(0x088D09A8u, gt::game::func_088D09A8, gt::ReturnKind::Word);
GT_DECOMP(0x088D0580u, gt::game::func_088D0580, gt::ReturnKind::Void);
GT_DECOMP(0x08B1E1C0u, gt::game::func_08B1E1C0, gt::ReturnKind::Word);
GT_DECOMP(0x08977658u, gt::game::func_08977658, gt::ReturnKind::Word);
GT_DECOMP(0x08945628u, gt::game::func_08945628, gt::ReturnKind::Word);
GT_DECOMP(0x088F8A14u, gt::game::func_088F8A14, gt::ReturnKind::Word);
GT_DECOMP(0x08B2080Cu, gt::game::func_08B2080C, gt::ReturnKind::Word);
GT_DECOMP(0x089499F8u, gt::game::func_089499F8, gt::ReturnKind::Word);
GT_DECOMP(0x0892020Cu, gt::game::func_0892020C, gt::ReturnKind::Void);
GT_DECOMP(0x08B2EE10u, gt::game::func_08B2EE10, gt::ReturnKind::Word);
GT_DECOMP(0x08B24D20u, gt::game::func_08B24D20, gt::ReturnKind::Void);
GT_DECOMP(0x08B055ECu, gt::game::func_08B055EC, gt::ReturnKind::Void);
GT_DECOMP(0x089BD668u, gt::game::func_089BD668, gt::ReturnKind::Word);
GT_DECOMP(0x0894CF58u, gt::game::func_0894CF58, gt::ReturnKind::Word);
GT_DECOMP(0x0893EB08u, gt::game::func_0893EB08, gt::ReturnKind::Word);
GT_DECOMP(0x088D07C0u, gt::game::func_088D07C0, gt::ReturnKind::Word);
GT_DECOMP(0x08B18E40u, gt::game::func_08B18E40, gt::ReturnKind::Word);
// Not registered: the harness never sees a call to it on the three pad routes ("calls 0"), so it is
// untested.
// GT_DECOMP(0x08889D4Cu, gt::game::func_08889D4C, gt::ReturnKind::Word);
GT_DECOMP(0x08B0DDDCu, gt::game::func_08B0DDDC, gt::ReturnKind::Word);
// Not registered: the harness never sees a call to it on the three pad routes ("calls 0"), so it is
// untested.
// GT_DECOMP(0x08926D60u, gt::game::func_08926D60, gt::ReturnKind::Word);
GT_DECOMP(0x08B24A0Cu, gt::game::func_08B24A0C, gt::ReturnKind::Word);
GT_DECOMP(0x0893069Cu, gt::game::func_0893069C, gt::ReturnKind::Word);
GT_DECOMP(0x08897574u, gt::game::func_08897574, gt::ReturnKind::Word);
GT_DECOMP(0x08AC0618u, gt::game::func_08AC0618, gt::ReturnKind::Word);
GT_DECOMP(0x08993BC8u, gt::game::func_08993BC8, gt::ReturnKind::Word);
GT_DECOMP(0x0895EDA0u, gt::game::func_0895EDA0, gt::ReturnKind::Word);
GT_DECOMP(0x088FAF3Cu, gt::game::func_088FAF3C, gt::ReturnKind::Word);
GT_DECOMP(0x08B18C3Cu, gt::game::func_08B18C3C, gt::ReturnKind::Word);
GT_DECOMP(0x089F9C6Cu, gt::game::func_089F9C6C, gt::ReturnKind::Word);
GT_DECOMP(0x089B3704u, gt::game::func_089B3704, gt::ReturnKind::Word);
GT_DECOMP(0x08AF94CCu, gt::game::func_08AF94CC, gt::ReturnKind::Word);
GT_DECOMP(0x08AD85C8u, gt::game::func_08AD85C8, gt::ReturnKind::Word);

// --- batch 33 ---

namespace gt::game {
namespace {

constexpr std::uint32_t kVtable6D38 = 0x08CD6D38u;  // vtables the constructors below install
constexpr std::uint32_t kVtable5F38 = 0x08CD5F38u;
constexpr std::uint32_t kVtable7C18 = 0x08CC7C18u;
constexpr std::uint32_t kVtable79C0 = 0x08CC79C0u;

// The signed byte at `offset` of *this (v1 keeps *this).
void deref_signed_byte(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t offset) {
    ctx.gpr[3] = word(rt, ctx.gpr[4]);
    ctx.gpr[2] = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(rt.memory().load8(ctx.gpr[3] + offset))));
}

// The byte at `offset` of the object at +0x4 (v1 keeps it).
void object4_byte(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t offset) {
    ctx.gpr[3] = word(rt, ctx.gpr[4] + 0x4u);
    ctx.gpr[2] = rt.memory().load8(ctx.gpr[3] + offset);
}

// `query(...)` (arguments passed through) plus `add`.
void offset_result(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t query, std::uint32_t add) {
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, query, {}) + add;
}

// `target(&entry i, flag)` for entry i (0x2F8 bytes) of the table at +0xC0.
void entry_call(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t target, std::uint32_t flag) {
    const std::uint32_t i = ctx.gpr[5];
    ctx.gpr[3] = i << 5;
    ctx.gpr[2] = i * 0x2F8u;
    tail_call(ctx, target, {word(rt, ctx.gpr[4] + 0xC0u) + i * 0x2F8u, flag});
}

} // namespace

// Constructors: func_08AD408C (vtable), func_08AB94C4 (vtable, cleared fields), func_089FB918 (the base
// constructor func_08A0D658, then vtables at +0x38 and +0x0).
void func_08AD408C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable(rt, ctx, kVtable6D38); }
void func_08AB94C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    store_vtable(rt, ctx, kVtable5F38);
    rt.memory().store8(self + 0xDu, 0u);
    rt.memory().store32(self + 0x4u, 0u);
    rt.memory().store32(self + 0x8u, 0u);
    rt.memory().store8(self + 0xCu, 0u);
}
void func_089FB918(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08A0D658u, {self});
    }
    rt.memory().store32(self + 0x38u, kVtable7C18);
    rt.memory().store32(self, kVtable79C0);
    ctx.gpr[2] = kVtable7C18;
    ctx.gpr[3] = kVtable79C0;
}

// func_08A7CBA8(this) -> func_08A5B9EC(&local), with the local built by func_08A7B544 and filled by
// func_08A7C5C8(this, &local) (as func_08A7C868).
void func_08A7CBA8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { with_temporary(rt, ctx, 0x08A7B544u, 0x08A7C5C8u, false); }

// func_08A663C0() -> a new 8-byte object constructed by func_08A6639C.
void func_08A663C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t object = call_guest(rt, ctx, kAllocate, {8u});
    (void)call_guest(rt, ctx, 0x08A6639Cu, {object});
    ctx.gpr[2] = object;
}

// func_08A17414(this, a, b, c, d): stores the four floats, then func_08A17104 with the same arguments.
void func_08A17414(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    rt.memory().store32(self + 0xCu, std::bit_cast<std::uint32_t>(ctx.fpr[15]));
    rt.memory().store32(self + 0x0u, std::bit_cast<std::uint32_t>(ctx.fpr[12]));
    rt.memory().store32(self + 0x4u, std::bit_cast<std::uint32_t>(ctx.fpr[13]));
    rt.memory().store32(self + 0x8u, std::bit_cast<std::uint32_t>(ctx.fpr[14]));
    tail_call(ctx, 0x08A17104u, {});
}

// Forwarders: func_089B47EC(this) -> func_08B855E0(this + 0x118); func_089B45EC(this):
// func_089BA59C(this), then func_089B44E4(this); func_089B39DC(this, x) -> this, after
// func_08B88F44(this, x + 0x118); func_088C4D20(this) -> func_088C4BA4(this, 0).
void func_089B47EC(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x08B855E0u, {ctx.gpr[4] + 0x118u}); }
void func_089B45EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x089BA59Cu, {self});
    }
    tail_call(ctx, 0x089B44E4u, {self});
}
void func_089B39DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08B88F44u, {self, ctx.gpr[5] + 0x118u});
    }
    ctx.gpr[2] = self;
}
void func_088C4D20(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x088C4BA4u, {ctx.gpr[4], 0u}); }

// func_0895B58C(this) -> whether the byte at +0x1 is 7.
void func_0895B58C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = rt.memory().load8(ctx.gpr[4] + 0x1u) == 7u ? 1u : 0u;
}

// func_089588BC(this): when the byte at +0x52 of the object at +0xC is 6, sets the pair at +0x778 of the
// object at +0x8 to (1.0, 0).
void func_089588BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t kind = rt.memory().load8(word(rt, self + 0xCu) + 0x52u);
    ctx.gpr[2] = word(rt, self + 0xCu);
    ctx.gpr[3] = 6u;
    ctx.gpr[4] = kind;
    ctx.gpr[5] = self;
    if (kind != 6u) return;
    const std::uint32_t object = word(rt, self + 0x8u);
    ctx.gpr[2] = object;
    ctx.fpr[0] = 1.0f;
    rt.memory().store32(object + 0x778u, std::bit_cast<std::uint32_t>(1.0f));
    rt.memory().store32(object + 0x77Cu, 0u);
}

// Setters of the object at +0x8: func_08957B18 (clears +0x60E..+0x617), func_08957148 (clears the byte
// at +0x737), func_089559B8(this, x) (+0x4FC = x), func_089525D4(this, value) (the byte at +0x5),
// func_08945EE8 (clears +0x724 and +0x728).
void func_08957B18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4] + 0x8u);
    ctx.gpr[2] = object;
    rt.memory().store8(object + 0x60Fu, 0u);
    rt.memory().store32(object + 0x610u, 0u);
    rt.memory().store32(object + 0x614u, 0u);
    rt.memory().store8(object + 0x60Eu, 0u);
}
void func_08957148(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = word(rt, ctx.gpr[4] + 0x8u);
    rt.memory().store8(ctx.gpr[2] + 0x737u, 0u);
}
void func_089559B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = word(rt, ctx.gpr[4] + 0x8u);
    rt.memory().store32(ctx.gpr[2] + 0x4FCu, std::bit_cast<std::uint32_t>(ctx.fpr[12]));
}
void func_089525D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = word(rt, ctx.gpr[4] + 0x8u);
    rt.memory().store8(ctx.gpr[2] + 0x5u, static_cast<std::uint8_t>(ctx.gpr[5]));
}
void func_08945EE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = word(rt, ctx.gpr[4] + 0x8u);
    rt.memory().store32(ctx.gpr[2] + 0x728u, 0u);
    rt.memory().store32(ctx.gpr[2] + 0x724u, 0u);
}

// func_08955290(this, float out[3]): (0, -y, -x) from the floats x at +0x70 and y at +0x74 of the object
// at +0xC.
void func_08955290(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4] + 0xCu);
    const std::uint32_t out = ctx.gpr[5];
    ctx.gpr[2] = object;
    rt.memory().store32(out, 0u);
    ctx.fpr[0] = std::bit_cast<float>(word(rt, object + 0x74u) ^ 0x80000000u);
    rt.memory().store32(out + 0x4u, std::bit_cast<std::uint32_t>(ctx.fpr[0]));
    ctx.fpr[1] = std::bit_cast<float>(word(rt, object + 0x70u) ^ 0x80000000u);
    rt.memory().store32(out + 0x8u, std::bit_cast<std::uint32_t>(ctx.fpr[1]));
}

// Floats of the object at +0xC: func_08955254 (+0x8), func_08955248 (+0x4), func_0895523C (+0x0).
void func_08955254(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { object12_float(rt, ctx, 0x8u); }
void func_08955248(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { object12_float(rt, ctx, 0x4u); }
void func_0895523C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { object12_float(rt, ctx, 0x0u); }

// func_0894CFD8 / func_0894CF98(...) -> func_0894CF40(...) + 0x1F50 / + 0x14E0.
void func_0894CFD8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { offset_result(rt, ctx, 0x0894CF40u, 0x1F50u); }
void func_0894CF98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { offset_result(rt, ctx, 0x0894CF40u, 0x14E0u); }

// func_08949DC4(this): for the object at +0x8, +0x594 = (float) the signed halfword at +0x58E, +0x598 = 0.
void func_08949DC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4] + 0x8u);
    const auto value = static_cast<std::int32_t>(static_cast<std::int16_t>(rt.memory().load16(object + 0x58Eu)));
    rt.memory().store32(object + 0x598u, 0u);
    ctx.fpr[0] = static_cast<float>(value);
    rt.memory().store32(object + 0x594u, std::bit_cast<std::uint32_t>(ctx.fpr[0]));
    ctx.gpr[2] = object;
    ctx.gpr[3] = static_cast<std::uint32_t>(value);
}

// func_08949AC4(this): clears the byte at +0x1 and then the low byte of the halfword at +0x0, the
// bytes at +0xE and +0xF and the halfword at +0xC.
void func_08949AC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    rt.memory().store8(self + 0x1u, 0u);
    const std::uint32_t kept = rt.memory().load16(self) & 0xFF00u;
    rt.memory().store8(self + 0xFu, 0u);
    rt.memory().store16(self, static_cast<std::uint16_t>(kept));
    rt.memory().store16(self + 0xCu, 0u);
    rt.memory().store8(self + 0xEu, 0u);
    ctx.gpr[2] = kept;
}

// func_08945654(this): +0x570 = the float at +0x534, +0x574 = 0.
void func_08945654(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.fpr[0] = float_at(rt, ctx.gpr[4] + 0x534u);
    rt.memory().store32(ctx.gpr[4] + 0x574u, 0u);
    rt.memory().store32(ctx.gpr[4] + 0x570u, std::bit_cast<std::uint32_t>(ctx.fpr[0]));
}

// func_08936184(this): func_08936154(this, i, i) for the signed byte i at +0x10, nothing for -1.
void func_08936184(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const auto i = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(rt.memory().load8(ctx.gpr[4] + 0x10u))));
    ctx.gpr[6] = i;
    ctx.gpr[5] = i;
    ctx.gpr[3] = 0xFFFFFFFFu;
    ctx.gpr[2] = 0u;
    if (i != 0xFFFFFFFFu) tail_call(ctx, 0x08936154u, {});
}

// func_088FAFD8(this) -> the float at +0x18 of the second object, else of the first, else 0.0.
void func_088FAFD8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t second = word(rt, ctx.gpr[4] + 0x4u);
    ctx.gpr[2] = second;
    if (second != 0u) {
        ctx.fpr[0] = float_at(rt, second + 0x18u);
        return;
    }
    const std::uint32_t first = word(rt, ctx.gpr[4]);
    ctx.gpr[4] = first;
    ctx.fpr[0] = first != 0u ? float_at(rt, first + 0x18u) : 0.0f;
}

// func_088FAEFC(this, i) -> the float at +0xC of entry i (16 bytes) of the second object's table at
// +0x50, else of the first's at +0x5C, else 0.0.
void func_088FAEFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t second = word(rt, ctx.gpr[4] + 0x4u);
    const std::uint32_t offset = ctx.gpr[5] << 4;
    ctx.gpr[3] = second;
    ctx.gpr[6] = offset;
    if (second != 0u) {
        ctx.gpr[2] = offset + word(rt, second + 0x50u);
        ctx.fpr[0] = float_at(rt, ctx.gpr[2] + 0xCu);
        return;
    }
    const std::uint32_t first = word(rt, ctx.gpr[4]);
    ctx.gpr[4] = first;
    ctx.gpr[5] = offset;
    ctx.fpr[0] = 0.0f;
    if (first == 0u) return;
    ctx.gpr[2] = offset + word(rt, first + 0x5Cu);
    ctx.fpr[0] = float_at(rt, ctx.gpr[2] + 0xCu);
}

// func_088F8A84(this, i, value): the byte table at +0x24, entry i (func_088F8A14 reads it).
void func_088F8A84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[4] += ctx.gpr[5];
    rt.memory().store8(ctx.gpr[4] + 0x24u, static_cast<std::uint8_t>(ctx.gpr[6]));
}

// func_088E8C88(this): sets the bytes at +0x2B7 and +0x12, the byte at +0x3 of the object at +0x2E4 when
// +0x1C is clear and +0x4 is 1, then func_088E7F1C(this).
void func_088E8C88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    ctx.gpr[3] = word(rt, self + 0x1Cu);
    ctx.gpr[5] = self;
    ctx.gpr[2] = 1u;
    rt.memory().store8(self + 0x2B7u, 1u);
    rt.memory().store8(self + 0x12u, 1u);
    if (ctx.gpr[3] == 0u) {
        ctx.gpr[6] = word(rt, self + 0x2E4u);
        if (ctx.gpr[6] != 0u) {
            ctx.gpr[3] = word(rt, self + 0x4u);
            if (ctx.gpr[3] == 1u) rt.memory().store8(ctx.gpr[6] + 0x3u, 1u);
        }
    }
    tail_call(ctx, 0x088E7F1Cu, {});
}

// func_088E8A44(this, k): +0x8 = k, also the word at +0x38 of the object at +0x2E4 when +0x1C is clear
// and +0x4 is 1; the byte at +0x2A2 = 1 when k is 4 or 5.
void func_088E8A44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t k = ctx.gpr[5];
    ctx.gpr[3] = word(rt, self + 0x1Cu);
    ctx.gpr[2] = k - 4u;
    ctx.gpr[6] = (k - 4u) < 2u ? 1u : 0u;
    if (ctx.gpr[3] == 0u) {
        ctx.gpr[7] = word(rt, self + 0x2E4u);
        ctx.gpr[2] = 1u;
        if (ctx.gpr[7] != 0u) {
            ctx.gpr[3] = word(rt, self + 0x4u);
            if (ctx.gpr[3] == 1u) rt.memory().store32(ctx.gpr[7] + 0x38u, k);
        }
    }
    rt.memory().store32(self + 0x8u, k);
    if (ctx.gpr[6] != 0u) {
        ctx.gpr[2] = 1u;
        rt.memory().store8(self + 0x2A2u, 1u);
    }
}

// func_088DA74C / func_088DA704(this, i) -> func_088E80C0 (entry, 0) / func_088E8078 (entry, 1) for entry
// i of the table at +0xC0.
void func_088DA74C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { entry_call(rt, ctx, 0x088E80C0u, 0u); }
void func_088DA704(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { entry_call(rt, ctx, 0x088E8078u, 1u); }

// Constants and readers: func_088D698C (-1), func_088B4390 (549), func_088D4FA8 / func_088D4CB8 (the
// signed byte at +0x71 / +0x48 of *this), func_088B1A18 / func_088B19F4 (the byte at +0x32A / +0x328 of
// the object at +0x4).
void func_088D698C(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = 0xFFFFFFFFu; }
void func_088B4390(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = 549u; }
void func_088D4FA8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_signed_byte(rt, ctx, 0x71u); }
void func_088D4CB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { deref_signed_byte(rt, ctx, 0x48u); }
void func_088B1A18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { object4_byte(rt, ctx, 0x32Au); }
void func_088B19F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { object4_byte(rt, ctx, 0x328u); }

} // namespace gt::game

GT_DECOMP(0x08AD408Cu, gt::game::func_08AD408C, gt::ReturnKind::Word);
GT_DECOMP(0x08AB94C4u, gt::game::func_08AB94C4, gt::ReturnKind::Word);
// func_089FB918 is reached only from inside its generated unit (the harness saw no call), so the gate
// can't check it.
// GT_DECOMP(0x089FB918u, gt::game::func_089FB918, gt::ReturnKind::Word);
GT_DECOMP(0x08A7CBA8u, gt::game::func_08A7CBA8, gt::ReturnKind::Word);
GT_DECOMP(0x08A663C0u, gt::game::func_08A663C0, gt::ReturnKind::Word);
GT_DECOMP(0x08A17414u, gt::game::func_08A17414, gt::ReturnKind::Word);
GT_DECOMP(0x089B47ECu, gt::game::func_089B47EC, gt::ReturnKind::Word);
// func_089B45EC reaches kernel imports the isolated run can't replay (not comparable in all 12 cases), so
// the gate can't check it.
// GT_DECOMP(0x089B45ECu, gt::game::func_089B45EC, gt::ReturnKind::Word);
GT_DECOMP(0x089B39DCu, gt::game::func_089B39DC, gt::ReturnKind::Word);
GT_DECOMP(0x088C4D20u, gt::game::func_088C4D20, gt::ReturnKind::Word);
GT_DECOMP(0x0895B58Cu, gt::game::func_0895B58C, gt::ReturnKind::Word);
GT_DECOMP(0x089588BCu, gt::game::func_089588BC, gt::ReturnKind::Void);
GT_DECOMP(0x08957B18u, gt::game::func_08957B18, gt::ReturnKind::Word);
GT_DECOMP(0x08957148u, gt::game::func_08957148, gt::ReturnKind::Word);
GT_DECOMP(0x089559B8u, gt::game::func_089559B8, gt::ReturnKind::Word);
GT_DECOMP(0x089525D4u, gt::game::func_089525D4, gt::ReturnKind::Word);
GT_DECOMP(0x08945EE8u, gt::game::func_08945EE8, gt::ReturnKind::Word);
GT_DECOMP(0x08955290u, gt::game::func_08955290, gt::ReturnKind::Word);
GT_DECOMP(0x08955254u, gt::game::func_08955254, gt::ReturnKind::Float);
GT_DECOMP(0x08955248u, gt::game::func_08955248, gt::ReturnKind::Float);
GT_DECOMP(0x0895523Cu, gt::game::func_0895523C, gt::ReturnKind::Float);
GT_DECOMP(0x0894CFD8u, gt::game::func_0894CFD8, gt::ReturnKind::Word);
GT_DECOMP(0x0894CF98u, gt::game::func_0894CF98, gt::ReturnKind::Word);
GT_DECOMP(0x08949DC4u, gt::game::func_08949DC4, gt::ReturnKind::Word);
GT_DECOMP(0x08949AC4u, gt::game::func_08949AC4, gt::ReturnKind::Word);
GT_DECOMP(0x08945654u, gt::game::func_08945654, gt::ReturnKind::Void);
GT_DECOMP(0x08936184u, gt::game::func_08936184, gt::ReturnKind::Word);
GT_DECOMP(0x088FAFD8u, gt::game::func_088FAFD8, gt::ReturnKind::Float);
GT_DECOMP(0x088FAEFCu, gt::game::func_088FAEFC, gt::ReturnKind::Float);
GT_DECOMP(0x088F8A84u, gt::game::func_088F8A84, gt::ReturnKind::Void);
GT_DECOMP(0x088E8C88u, gt::game::func_088E8C88, gt::ReturnKind::Word);
GT_DECOMP(0x088E8A44u, gt::game::func_088E8A44, gt::ReturnKind::Word);
GT_DECOMP(0x088DA74Cu, gt::game::func_088DA74C, gt::ReturnKind::Word);
GT_DECOMP(0x088DA704u, gt::game::func_088DA704, gt::ReturnKind::Word);
GT_DECOMP(0x088D698Cu, gt::game::func_088D698C, gt::ReturnKind::Word);
GT_DECOMP(0x088B4390u, gt::game::func_088B4390, gt::ReturnKind::Word);
GT_DECOMP(0x088D4FA8u, gt::game::func_088D4FA8, gt::ReturnKind::Word);
GT_DECOMP(0x088D4CB8u, gt::game::func_088D4CB8, gt::ReturnKind::Word);
GT_DECOMP(0x088B1A18u, gt::game::func_088B1A18, gt::ReturnKind::Word);
GT_DECOMP(0x088B19F4u, gt::game::func_088B19F4, gt::ReturnKind::Word);

// --- batch 34 ---

namespace gt::game {
namespace {

constexpr std::uint32_t kVtable9488 = 0x08CD9488u;  // vtables the constructors below install
constexpr std::uint32_t kVtable9370 = 0x08CD9370u;
constexpr std::uint32_t kVtableA6F0 = 0x08CDA6F0u;
constexpr std::uint32_t kVtable77F0 = 0x08CD77F0u;
constexpr std::uint32_t kVtable9390 = 0x08CC9390u;
constexpr std::uint32_t kVtable9138 = 0x08CC9138u;
constexpr std::uint32_t kVtable6798 = 0x08CC6798u;
constexpr std::uint32_t kVtable3E60 = 0x08CC3E60u;
constexpr std::uint32_t kRegistry73E8 = 0x08D773E8u;  // the head of the list func_08B1E334 / func_08B1E2D4 push onto
constexpr std::uint32_t kGlobalF428 = 0x08CFF428u;    // the word func_08A7AD8C returns

// The address of entry i (1088 bytes) of the table at +0x10 (v0 = i << 10, as the original leaves it).
std::uint32_t entry_1088(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t i = ctx.gpr[5];
    ctx.gpr[5] = i * 1088u;
    return word(rt, ctx.gpr[4] + 0x10u) + i * 1088u;
}

// The virtual at `slot` of *holder (object, holder, a, b, c); nothing (or 0, `zero_on_null`) when
// either is null. `c_mask` is applied to c first (0xFF for a bool argument).
void holder_call5(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t slot, std::uint32_t c_mask,
                  bool zero_on_null) {
    const std::uint32_t holder = ctx.gpr[4];
    const std::uint32_t a = ctx.gpr[5];
    const std::uint32_t b = ctx.gpr[6];
    const std::uint32_t c = ctx.gpr[7] & c_mask;
    ctx.gpr[3] = a;
    ctx.gpr[9] = b;
    ctx.gpr[8] = c;
    const std::uint32_t object = holder != 0u ? word(rt, holder) : 0u;
    if (object == 0u) {
        if (holder != 0u) {
            ctx.gpr[4] = 0u;
            ctx.gpr[5] = holder;
            ctx.gpr[6] = a;
            ctx.gpr[7] = b;
            ctx.gpr[2] = 0u;
        }
        if (zero_on_null) ctx.gpr[2] = 0u;
        return;
    }
    ctx.gpr[2] = word(rt, object);
    tail_call(ctx, word(rt, word(rt, object) + slot), {object, holder, a, b, c});
}

// `base(this)`, the vtable, then the tail call `next(this + offset)`.
void base_vtable_tail(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t base, std::uint32_t vtable,
                      std::uint32_t offset, std::uint32_t next) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, base, {self});
    }
    rt.memory().store32(self, vtable);
    ctx.gpr[2] = vtable;
    tail_call(ctx, next, {self + offset});
}

// Registers the object on the list at kRegistry73E8 through its link at +0x4 (next, then owner at
// +0x8), with the key at +0xC and the flag at +0x10, then func_08B1E1C0(this).
void register_object(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint8_t flag) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t link = self + 0x4u;
    rt.memory().store32(self, kVtable9370);
    const std::uint32_t head = word(rt, kRegistry73E8);
    rt.memory().store32(link + 0x4u, self);
    rt.memory().store32(link, head);
    rt.memory().store32(kRegistry73E8, link);
    rt.memory().store8(self + 0x10u, flag);
    rt.memory().store32(self + 0xCu, ctx.gpr[5]);
    ctx.gpr[2] = head;
    tail_call(ctx, 0x08B1E1C0u, {});
}

// `target(&field)` twice in a row (start, then the matching step) on the member at +0x4C; returns
// this + `result`.
void member_76_pair(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t result) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08B344E0u, {self + 0x4Cu});
        (void)call_guest(rt, ctx, 0x08B3457Cu, {self + 0x4Cu});
    }
    ctx.gpr[2] = self + result;
}

// r = first(a0, a1); then the tail call `next(r, a2, extra...)`.
void result_then(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t first, std::uint32_t next,
                 std::initializer_list<std::uint32_t> extra) {
    const std::uint32_t kept = ctx.gpr[6];
    std::uint32_t result;
    {
        GuestFrame frame(ctx, 0x10u);
        result = call_guest(rt, ctx, first, {ctx.gpr[4], ctx.gpr[5]});
    }
    ctx.gpr[2] = result;
    ctx.gpr[4] = result;
    ctx.gpr[5] = kept;
    unsigned reg = 6u;
    for (const std::uint32_t value : extra) ctx.gpr[reg++] = value;
    tail_call(ctx, next, {});
}

// Mirrors a value into the object at +0x2E4 when +0x1C is clear (as func_088E8A44).
std::uint32_t mirror_target(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t mirror = word(rt, ctx.gpr[4] + 0x2E4u);
    ctx.gpr[3] = mirror;
    if (mirror == 0u) return 0u;
    ctx.gpr[2] = word(rt, ctx.gpr[4] + 0x1Cu);
    return ctx.gpr[2] == 0u ? mirror : 0u;
}

// A constructor that clears its five words after the vtable (+0x14 = `first`).
void construct_cleared6(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t first) {
    const std::uint32_t self = ctx.gpr[4];
    rt.memory().store32(self, kVtable9620);
    rt.memory().store32(self + 0x14u, first);
    rt.memory().store32(self + 0x18u, 0u);
    rt.memory().store32(self + 0x4u, 0u);
    rt.memory().store32(self + 0x8u, 0u);
    rt.memory().store32(self + 0xCu, 0u);
    rt.memory().store32(self + 0x10u, 0u);
    ctx.gpr[2] = kVtable9620;
}

} // namespace

// Entries of the 1088-byte table at +0x10: func_088AF28C(this, i) -> &entry i; func_088AF2E0 /
// func_088AF2C8(this, i) -> func_088AD76C / func_088AD9B8(&entry i, the byte offset).
void func_088AF28C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[3] = ctx.gpr[5] << 10;
    ctx.gpr[2] = entry_1088(rt, ctx);
}
void func_088AF2E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t low = ctx.gpr[5] << 10;
    const std::uint32_t entry = entry_1088(rt, ctx);
    ctx.gpr[2] = low;
    tail_call(ctx, 0x088AD76Cu, {entry});
}
void func_088AF2C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t low = ctx.gpr[5] << 10;
    const std::uint32_t entry = entry_1088(rt, ctx);
    ctx.gpr[2] = low;
    tail_call(ctx, 0x088AD9B8u, {entry});
}

// Pointer arithmetic: func_088B19E8 (the object at +0x4, + 0x10), func_088970D4 (*this + 0x8C),
// func_089624F4 (this + 8).
void func_088B19E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = word(rt, ctx.gpr[4] + 0x4u) + 0x10u; }
void func_088970D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = word(rt, ctx.gpr[4]) + 0x8Cu; }
void func_089624F4(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = ctx.gpr[4] + 0x8u; }

// func_088AA758(this) -> the word at +0x8 of the object at +0x54 of *this, 0 without one.
void func_088AA758(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, word(rt, ctx.gpr[4]) + 0x54u);
    ctx.gpr[3] = object != 0u ? word(rt, object + 0x8u) : 0u;
    ctx.gpr[2] = ctx.gpr[3];
}

// func_088AA744(this) -> *this + 0x5C when its byte at +0x2DC is set, else 0.
void func_088AA744(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4]);
    ctx.gpr[3] = rt.memory().load8(object + 0x2DCu);
    ctx.gpr[2] = ctx.gpr[3] != 0u ? object + 0x5Cu : 0u;
}

// Pair setters: func_088A7C6C (+0x2C, +0x30), func_088A7C14 (+0x8, +0x10), func_088E7E94 (+0x24, +0x28).
void func_088A7C6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store32(ctx.gpr[4] + 0x30u, ctx.gpr[6]);
    rt.memory().store32(ctx.gpr[4] + 0x2Cu, ctx.gpr[5]);
}
void func_088A7C14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store32(ctx.gpr[4] + 0x10u, ctx.gpr[6]);
    rt.memory().store32(ctx.gpr[4] + 0x8u, ctx.gpr[5]);
}
void func_088E7E94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store32(ctx.gpr[4] + 0x28u, ctx.gpr[6]);
    rt.memory().store32(ctx.gpr[4] + 0x24u, ctx.gpr[5]);
}

// func_088A3718(this): the virtual at +0x30 with (this, 0), then sets the byte at +0x59.
void func_088A3718(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t result;
    {
        GuestFrame frame(ctx, 0x10u);
        result = call_guest(rt, ctx, word(rt, word(rt, self) + 0x30u), {self, 0u});
    }
    rt.memory().store8(self + 0x59u, 1u);
    ctx.gpr[2] = result;
    ctx.gpr[3] = 1u;
}

// Big-endian words of *this: func_088977D4 (+0x9C), func_0889717C (whether +0xA4 is nonzero).
void func_088977D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[3] = word(rt, ctx.gpr[4]);
    ctx.gpr[2] = bswap32(word(rt, ctx.gpr[3] + 0x9Cu));
}
void func_0889717C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[3] = word(rt, ctx.gpr[4]);
    ctx.gpr[2] = bswap32(word(rt, ctx.gpr[3] + 0xA4u)) != 0u ? 1u : 0u;
}

// Calls on a field of *this: func_0885BE14 -> func_088D4C9C(the object at +0x7C); func_0885EC1C ->
// func_088D9F80(the object at +0x5AC) as a tail call; func_0885E8B8 -> whether func_088D7FB4(the
// object at +0x5AC) is nonzero.
void func_0885BE14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, word(rt, ctx.gpr[4]) + 0x7Cu);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, 0x088D4C9Cu, {object});
}
void func_0885EC1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = word(rt, ctx.gpr[4]);
    tail_call(ctx, 0x088D9F80u, {word(rt, ctx.gpr[2] + 0x5ACu)});
}
void func_0885E8B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, word(rt, ctx.gpr[4]) + 0x5ACu);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, 0x088D7FB4u, {object}) != 0u ? 1u : 0u;
}

// Calls on *this: func_08AC3554 -> func_08AC3490, func_08AC3360 -> func_08AC3270, func_088936A4 ->
// func_0889349C.
void func_08AC3554(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_on_field(rt, ctx, 0x0u, 0x08AC3490u); }
void func_08AC3360(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_on_field(rt, ctx, 0x0u, 0x08AC3270u); }
void func_088936A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_on_field(rt, ctx, 0x0u, 0x0889349Cu); }

// Fixed-argument tail calls: func_08B2E924 -> func_08C7A9B0(0, 0x4000); func_08AD354C ->
// func_08C1A0C4(the object at 0x08D404AC); func_08AF98D0(a, b) -> func_08C41AFC(b, a);
// func_08AF76E8(a, b) -> func_08C4063C(b, a + 4); func_08ACF184(this, flag) -> func_08AF5164(this + 0xFC,
// flag); func_089099F4(this): clears *this, then func_089099B8(this).
void func_08B2E924(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x08C7A9B0u, {0u, 0x4000u}); }
void func_08AD354C(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x08C1A0C4u, {0x08D404ACu}); }
void func_08AF98D0(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = ctx.gpr[4];
    tail_call(ctx, 0x08C41AFCu, {ctx.gpr[5], ctx.gpr[2]});
}
void func_08AF76E8(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = ctx.gpr[4] + 0x4u;
    tail_call(ctx, 0x08C4063Cu, {ctx.gpr[5], ctx.gpr[2]});
}
void func_08ACF184(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    tail_call(ctx, 0x08AF5164u, {ctx.gpr[4] + 0xFCu, ctx.gpr[5] & 0xFFu});
}
void func_089099F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store32(ctx.gpr[4], 0u);
    tail_call(ctx, 0x089099B8u, {});
}

// Constructors: func_08B24954(this, a, b, c) and func_08B248FC (vtable 0x08CD9620, six fields),
// func_08B2B654 (vtable, six cleared words), func_0892F820 (vtable, two -1 words, two cleared),
// func_088F8884 (vtable, flags), func_08B2EDD4(this, x) (no vtable).
void func_08B24954(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    rt.memory().store32(self + 0x18u, ctx.gpr[7]);
    rt.memory().store32(self, kVtable9620);
    rt.memory().store32(self + 0x10u, ctx.gpr[5]);
    rt.memory().store32(self + 0x14u, ctx.gpr[6]);
    rt.memory().store32(self + 0x4u, 0u);
    rt.memory().store32(self + 0x8u, 0u);
    rt.memory().store32(self + 0xCu, 0u);
    ctx.gpr[2] = kVtable9620;
}
void func_08B248FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    construct_cleared6(rt, ctx, 1u);
    ctx.gpr[3] = 1u;
}
void func_08B2B654(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    rt.memory().store32(self, kVtableA6F0);
    for (std::uint32_t offset = 0x4u; offset <= 0x18u; offset += 0x4u) rt.memory().store32(self + offset, 0u);
    ctx.gpr[2] = kVtableA6F0;
}
void func_0892F820(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    rt.memory().store32(self, kVtable6798);
    rt.memory().store32(self + 0x8u, 0xFFFFFFFFu);
    rt.memory().store32(self + 0xCu, 0xFFFFFFFFu);
    rt.memory().store32(self + 0x14u, 0u);
    rt.memory().store32(self + 0x10u, 0u);
    ctx.gpr[2] = kVtable6798;
    ctx.gpr[6] = 0xFFFFFFFFu;
    ctx.gpr[7] = 0xFFFFFFFFu;
}
void func_088F8884(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    rt.memory().store32(self, kVtable3E60);
    rt.memory().store8(self + 0xCu, 1u);
    rt.memory().store32(self + 0x10u, 0u);
    rt.memory().store32(self + 0x4u, 0u);
    for (std::uint32_t offset = 0x8u; offset <= 0xBu; ++offset) rt.memory().store8(self + offset, 0u);
    ctx.gpr[2] = kVtable3E60;
    ctx.gpr[3] = 1u;
}
void func_08B2EDD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    rt.memory().store32(self + 0x10u, 0xFFFFFFFFu);
    rt.memory().store32(self, 1u);
    rt.memory().store32(self + 0x8u, ctx.gpr[5]);
    rt.memory().store8(self + 0x4u, 0u);
    rt.memory().store32(self + 0xCu, 0xFFFFFFFFu);
    ctx.gpr[2] = 1u;
    ctx.gpr[3] = 0xFFFFFFFFu;
}

// Constructors over a base: func_08B1F810 (func_08B1FD44, vtable, then func_08B28DCC(this + 0xC)),
// func_08AFB4B4 (func_08AFB360, vtable, then func_08B52B48(this + 0x2C)).
void func_08B1F810(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    base_vtable_tail(rt, ctx, 0x08B1FD44u, kVtable9488, 0xCu, 0x08B28DCCu);
}
void func_08AFB4B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    base_vtable_tail(rt, ctx, 0x08AFB360u, kVtable77F0, 0x2Cu, 0x08B52B48u);
}

// Registering constructors: func_08B1E334(this, key, flag) and func_08B1E2D4(this, key) (flag 0).
void func_08B1E334(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const auto flag = static_cast<std::uint8_t>(ctx.gpr[6]);
    ctx.gpr[7] = ctx.gpr[4] + 0x4u;
    ctx.gpr[8] = 0x08D70000u;
    register_object(rt, ctx, flag);
}
void func_08B1E2D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[6] = ctx.gpr[4] + 0x4u;
    ctx.gpr[7] = 0x08D70000u;
    register_object(rt, ctx, 0u);
}

// Holder virtuals: func_08ABC8B4 (slot +0x40, five arguments), func_08ABC9D0 (slot +0x30, the last a
// bool, 0 without an object), func_08ABBE30 (slot +0x20).
void func_08ABC8B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_call5(rt, ctx, 0x40u, 0xFFFFFFFFu, false); }
void func_08ABC9D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_call5(rt, ctx, 0x30u, 0xFFu, true); }
void func_08ABBE30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_call(rt, ctx, 0x20u); }

// func_089FB954: as func_089FB918 (base constructor func_08A0D658, then the vtables at +0x38 and +0x0).
void func_089FB954(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08A0D658u, {self});
    }
    rt.memory().store32(self + 0x38u, kVtable7C18);
    rt.memory().store32(self, kVtable79C0);
    ctx.gpr[2] = kVtable7C18;
    ctx.gpr[3] = kVtable79C0;
}

// Constants and plain readers: func_0894D6C8 (2448), func_088F8A9C (8), func_088FE2A4 (the byte at
// +0x8), func_08A7AD8C (the word at kGlobalF428), func_088D4D14(this, i) (the signed byte at +0x58 + i
// of *this), func_08ACCC60 (whether the int at +0x32C is positive).
void func_0894D6C8(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = 2448u; }
void func_088F8A9C(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = 8u; }
void func_088FE2A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = rt.memory().load8(ctx.gpr[4] + 0x8u); }
void func_08A7AD8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[3] = 0x08D00000u;
    ctx.gpr[2] = word(rt, kGlobalF428);
}
void func_088D4D14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[5] += word(rt, ctx.gpr[4]);
    ctx.gpr[2] = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(rt.memory().load8(ctx.gpr[5] + 0x58u))));
}
void func_08ACCC60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = static_cast<std::int32_t>(word(rt, ctx.gpr[4] + 0x32Cu)) > 0 ? 1u : 0u;
}

// func_088B5D6C(a, b) -> whether the halfwords *a and *b are equal.
void func_088B5D6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[3] = rt.memory().load16(ctx.gpr[5]);
    ctx.gpr[2] = rt.memory().load16(ctx.gpr[4]) == ctx.gpr[3] ? 1u : 0u;
}

// func_08B1ECC8() -> func_08B1ED60(the object at 0x08D68B2C) + func_08C7A9F8().
void func_08B1ECC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t first = call_guest(rt, ctx, 0x08B1ED60u, {0x08D68B2Cu});
    ctx.gpr[2] = first + call_guest(rt, ctx, 0x08C7A9F8u, {0x08D68B2Cu});
}

// func_08AD3B5C(x) -> func_08AD393C(the context at kContext04CC, x), 0 without a context.
void func_08AD3B5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t context = word(rt, kContext04CC);
    ctx.gpr[3] = context;
    ctx.gpr[5] = ctx.gpr[4];
    ctx.gpr[4] = context;
    if (context == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    ctx.gpr[2] = 0x08D40000u;
    tail_call(ctx, 0x08AD393Cu, {});
}

// Byte setters of *this: func_08AB8720 (sets +0x249), func_08AB8858(this, value) (+0x278).
void func_08AB8720(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[3] = word(rt, ctx.gpr[4]);
    ctx.gpr[2] = 1u;
    rt.memory().store8(ctx.gpr[3] + 0x249u, 1u);
}
void func_08AB8858(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { object0_store_byte(rt, ctx, 0x278u); }

// func_08AA62D4(this, out) -> this, after storing the 64-bit func_08AA623C(this, out) into out.
void func_08AA62D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t out = ctx.gpr[5];
    GuestResult result;
    {
        GuestFrame frame(ctx, 0x10u);
        result = call_guest(rt, ctx, 0x08AA623Cu, {self, out}, {});
    }
    rt.memory().store32(out, result.v0);
    rt.memory().store32(out + 0x4u, result.v1);
    ctx.gpr[2] = self;
    ctx.gpr[3] = result.v1;
}

// func_08AA2D50(this) -> this, with a temporary built by func_08A6D2D4 and filled by
// func_08A76ABC(this, &local).
void func_08AA2D50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { with_temporary(rt, ctx, 0x08A6D2D4u, 0x08A76ABCu, true); }

// func_0899AFCC(this): vtables at +0x38 and +0x0, then func_08994D78(this, the second vtable).
void func_0899AFCC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store32(ctx.gpr[4] + 0x38u, kVtable9390);
    rt.memory().store32(ctx.gpr[4], kVtable9138);
    ctx.gpr[3] = kVtable9390;
    tail_call(ctx, 0x08994D78u, {ctx.gpr[4], kVtable9138});
}

// func_088C4D28(this) -> func_088C4BA4(this, 50) when the signed byte at +0x3 of the object at +0x8 is
// 2 and the byte at +0x96 of the object at +0x18 is set, else func_088C4BA4(this, 0).
void func_088C4D28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const auto kind = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(rt.memory().load8(word(rt, self + 0x8u) + 0x3u))));
    ctx.gpr[6] = self;
    ctx.gpr[2] = 2u;
    ctx.gpr[3] = kind;
    std::uint32_t amount = 0u;
    if (kind == 2u) {
        ctx.gpr[2] = word(rt, self + 0x18u);
        ctx.gpr[3] = rt.memory().load8(ctx.gpr[2] + 0x96u);
        if (ctx.gpr[3] != 0u) amount = 50u;
    }
    tail_call(ctx, 0x088C4BA4u, {self, amount});
}

// func_08B1B96C(a, b, flag) -> func_08B1B92C(a, &local pair (0, 0), b, flag).
void func_08B1B96C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = ctx.gpr[4];
    const std::uint32_t b = ctx.gpr[5];
    const std::uint32_t flag = ctx.gpr[6] & 0xFFu;
    GuestFrame frame(ctx, 0x20u);
    rt.memory().store32(frame.at(0x0u), 0u);
    rt.memory().store32(frame.at(0x4u), 0u);
    ctx.gpr[2] = call_guest(rt, ctx, 0x08B1B92Cu, {a, frame.at(0x0u), b, flag});
}

// func_0892FBE0(this): +0x8 / +0xC = -1, func_08937B04(the object at +0x10), then clears +0x10 / +0x14.
void func_0892FBE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t object = word(rt, self + 0x10u);
    rt.memory().store32(self + 0x8u, 0xFFFFFFFFu);
    rt.memory().store32(self + 0xCu, 0xFFFFFFFFu);
    ctx.gpr[2] = 0xFFFFFFFFu;  // what the original returns when func_08937B04 leaves v0 alone
    ctx.gpr[3] = 0xFFFFFFFFu;
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08937B04u, {object});
    }
    rt.memory().store32(self + 0x14u, 0u);
    rt.memory().store32(self + 0x10u, 0u);
}

// func_089211A0(this): clears the two 16-byte blocks at +0xC and +0x1C and sets the byte at +0x2C.
void func_089211A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    for (std::uint32_t offset = 0xCu; offset < 0x2Cu; offset += 0x4u) rt.memory().store32(self + offset, 0u);
    rt.memory().store8(self + 0x2Cu, 1u);
    ctx.gpr[2] = 1u;
    ctx.gpr[3] = self + 0x1Cu;
}

// func_0890B014(this, bit) -> whether bit `bit` of **this is set.
void func_0890B014(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[6] = word(rt, ctx.gpr[4]);
    ctx.gpr[3] = 1u << (ctx.gpr[5] & 31u);
    ctx.gpr[2] = (word(rt, ctx.gpr[6]) & ctx.gpr[3]) != 0u ? 1u : 0u;
}

// func_088D4DB8(this) -> whether the byte at +0x54 of *this is nonzero.
void func_088D4DB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[3] = word(rt, ctx.gpr[4]);
    ctx.gpr[2] = rt.memory().load8(ctx.gpr[3] + 0x54u) != 0u ? 1u : 0u;
}

// func_088A6068(this) -> func_0892E51C(entry i (384 bytes) at +0x38 of *this), i the signed byte at +0x8.
void func_088A6068(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4]);
    const std::uint32_t i = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(rt.memory().load8(object + 0x8u))));
    ctx.gpr[2] = object;
    ctx.gpr[3] = i << 7;
    tail_call(ctx, 0x0892E51Cu, {object + i * 384u + 0x38u});
}

// func_08B28C04(this): clears the pending byte at +0x29 if set; otherwise func_08B34B2C(this) with the
// busy byte at +0x28 held.
void func_08B28C04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t pending = rt.memory().load8(self + 0x29u);
    ctx.gpr[2] = pending;
    if (pending != 0u) {
        rt.memory().store8(self + 0x29u, 0u);
        return;
    }
    rt.memory().store8(self + 0x28u, 1u);
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08B34B2Cu, {self});
    }
    rt.memory().store8(self + 0x28u, 0u);
}

// func_08B1CD8C(this, x): func_08B7C830(data + read, x, end - read) over the buffer (data +0x0, end
// +0x4, read +0x8), then func_08B1CB80(this).
void func_08B1CD8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t read = word(rt, self + 0x8u);
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08B7C830u, {word(rt, self) + read, ctx.gpr[5], word(rt, self + 0x4u) - read});
    }
    tail_call(ctx, 0x08B1CB80u, {self});
}

// func_08ADC988(a, b) -> func_08ADC8DC(a, b, 14, the table at 0x08C90F1C).
void func_08ADC988(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, 0x08ADC8DCu, {ctx.gpr[4], ctx.gpr[5], 14u, 0x08C90F1Cu});
}

// The member at +0x4C started and stepped: func_08A9BD30 -> this + 0x40, func_08A9BC30 -> this + 0x34.
void func_08A9BD30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { member_76_pair(rt, ctx, 0x40u); }
void func_08A9BC30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { member_76_pair(rt, ctx, 0x34u); }

// func_08A6C134(this) -> func_08A9F9CC(the object at +0x14, &local), with the local word cleared.
void func_08A6C134(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4] + 0x14u);
    GuestFrame frame(ctx, 0x20u);
    rt.memory().store32(frame.at(0x0u), 0u);
    ctx.gpr[2] = call_guest(rt, ctx, 0x08A9F9CCu, {object, frame.at(0x0u)});
}

// func_08A40BF8(this) -> the number of words between +0x14 and +0x18 (a vector's size).
void func_08A40BF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[3] = word(rt, ctx.gpr[4] + 0x14u);
    ctx.gpr[2] = static_cast<std::uint32_t>(static_cast<std::int32_t>(word(rt, ctx.gpr[4] + 0x18u) - ctx.gpr[3]) >> 2);
}

// func_089788A8(this) -> the free room: +0x80 + (+0x7C - +0x74) * 8 - +0x78.
void func_089788A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    ctx.gpr[3] = (word(rt, self + 0x7Cu) - word(rt, self + 0x74u)) << 3;
    ctx.gpr[2] = word(rt, self + 0x80u) + ctx.gpr[3] - word(rt, self + 0x78u);
}

// func_08961058(this, first, slots, count): a table of `count` slots pointing at 48-byte records.
void func_08961058(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t slots = ctx.gpr[6];
    const auto count = static_cast<std::int32_t>(ctx.gpr[7]);
    rt.memory().store32(self + 0xCu, 0u);
    rt.memory().store32(self, slots);
    rt.memory().store32(self + 0x4u, ctx.gpr[7]);
    rt.memory().store32(self + 0x8u, 0u);
    if (count <= 0) return;
    for (std::int32_t i = 0; i < count; ++i) {
        rt.memory().store32(ctx.gpr[6], ctx.gpr[5]);
        ctx.gpr[6] += 4u;
        ctx.gpr[5] += 48u;
    }
    ctx.gpr[2] = static_cast<std::uint32_t>(count);
}

// func_08959EE4(this, i): for a valid index, sets +0xA0 of record func_08959180(this, i).
void func_08959EE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const auto i = static_cast<std::int32_t>(ctx.gpr[5]);
    if (i < 0) return;
    ctx.gpr[2] = i < static_cast<std::int32_t>(word(rt, ctx.gpr[4] + 0x8u)) ? 1u : 0u;
    if (ctx.gpr[2] == 0u) return;
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08959180u, {ctx.gpr[4], ctx.gpr[5]});
    }
    rt.memory().store32(ctx.gpr[2] + 0xA0u, 1u);
    ctx.gpr[3] = 1u;
}

// Results passed on: func_0894D9A0(a, b, c) -> func_0894F1C8(func_0894CF78(a, b), c, 1);
// func_088DF1F4(a, b, c) -> func_088D176C(func_088DF13C(a, b), c).
void func_0894D9A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { result_then(rt, ctx, 0x0894CF78u, 0x0894F1C8u, {1u}); }
void func_088DF1F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { result_then(rt, ctx, 0x088DF13Cu, 0x088D176Cu, {}); }

// func_08935C2C(this) -> the first word func_08936584(func_089363FC(this), ., +0x30, +0x34, out) writes.
void func_08935C2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x90u);
    const std::uint32_t first = call_guest(rt, ctx, 0x089363FCu, {self});
    (void)call_guest(rt, ctx, 0x08936584u, {first, ctx.gpr[5], word(rt, self + 0x30u), word(rt, self + 0x34u), frame.at(0x0u)});
    ctx.gpr[2] = word(rt, frame.at(0x0u));
}

// func_0892BF64(this): clears the byte at +0x0 and the words at +0x4..+0x10.
void func_0892BF64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    rt.memory().store32(self + 0x10u, 0u);
    rt.memory().store8(self, 0u);
    rt.memory().store32(self + 0x4u, 0u);
    rt.memory().store32(self + 0x8u, 0u);
    rt.memory().store32(self + 0xCu, 0u);
}

// func_08926514(this, x, flag): func_08926488(this, x, flag), then func_089264E8(this + 0x2C, the
// table at 0x08CDDD50).
void func_08926514(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08926488u, {self, ctx.gpr[5], ctx.gpr[6] & 0xFFu});
    }
    tail_call(ctx, 0x089264E8u, {self + 0x2Cu, 0x08CDDD50u});
}

// func_08918B84(this): sets the byte at +0x3FC, func_089262EC(the object at +0x204), clears +0x208.
void func_08918B84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t object = word(rt, self + 0x204u);
    rt.memory().store8(self + 0x3FCu, 1u);
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x089262ECu, {object});
    }
    rt.memory().store32(self + 0x208u, 0u);
}

// Mirrored setters (as func_088E8A44): func_088E8A24(this, x) (+0x2E0, mirrored at +0x30) and
// func_088E7EBC(this, x) (+0x2B8, mirrored as a byte at +0x3C).
void func_088E8A24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store32(ctx.gpr[4] + 0x2E0u, ctx.gpr[5]);
    if (const std::uint32_t mirror = mirror_target(rt, ctx); mirror != 0u) rt.memory().store32(mirror + 0x30u, ctx.gpr[5]);
}
void func_088E7EBC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store32(ctx.gpr[4] + 0x2B8u, ctx.gpr[5]);
    if (const std::uint32_t mirror = mirror_target(rt, ctx); mirror != 0u)
        rt.memory().store8(mirror + 0x3Cu, static_cast<std::uint8_t>(ctx.gpr[5]));
}

// func_088E7F6C(this) -> 0 when both +0x4 and +0x2C4 are 3, else 1.
void func_088E7F6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    ctx.gpr[3] = word(rt, self + 0x4u);
    ctx.gpr[2] = 3u;
    std::uint32_t result = 1u;
    if (ctx.gpr[3] == 3u) {
        ctx.gpr[2] = word(rt, self + 0x2C4u);
        if (ctx.gpr[2] == 3u) result = 0u;
    }
    ctx.gpr[5] = result;
    ctx.gpr[2] = result;
}

} // namespace gt::game

GT_DECOMP(0x088B19E8u, gt::game::func_088B19E8, gt::ReturnKind::Word);
GT_DECOMP(0x088AF2E0u, gt::game::func_088AF2E0, gt::ReturnKind::Word);
GT_DECOMP(0x088AF2C8u, gt::game::func_088AF2C8, gt::ReturnKind::Word);
GT_DECOMP(0x088AF28Cu, gt::game::func_088AF28C, gt::ReturnKind::Word);
GT_DECOMP(0x088AA758u, gt::game::func_088AA758, gt::ReturnKind::Word);
GT_DECOMP(0x088AA744u, gt::game::func_088AA744, gt::ReturnKind::Word);
GT_DECOMP(0x088A7C6Cu, gt::game::func_088A7C6C, gt::ReturnKind::Void);
GT_DECOMP(0x088A7C14u, gt::game::func_088A7C14, gt::ReturnKind::Void);
GT_DECOMP(0x088A3718u, gt::game::func_088A3718, gt::ReturnKind::Word);
GT_DECOMP(0x088977D4u, gt::game::func_088977D4, gt::ReturnKind::Word);
GT_DECOMP(0x088970D4u, gt::game::func_088970D4, gt::ReturnKind::Word);
GT_DECOMP(0x0885BE14u, gt::game::func_0885BE14, gt::ReturnKind::Word);
// func_08B2E924 reaches kernel imports the isolated run can't replay (not comparable in all 12 cases), so the gate can't check it.
// GT_DECOMP(0x08B2E924u, gt::game::func_08B2E924, gt::ReturnKind::Word);
GT_DECOMP(0x08B24954u, gt::game::func_08B24954, gt::ReturnKind::Word);
GT_DECOMP(0x08B1F810u, gt::game::func_08B1F810, gt::ReturnKind::Word);
GT_DECOMP(0x08B1E334u, gt::game::func_08B1E334, gt::ReturnKind::Word);
GT_DECOMP(0x08B1E2D4u, gt::game::func_08B1E2D4, gt::ReturnKind::Word);
GT_DECOMP(0x08AD354Cu, gt::game::func_08AD354C, gt::ReturnKind::Word);
GT_DECOMP(0x08AC3554u, gt::game::func_08AC3554, gt::ReturnKind::Word);
GT_DECOMP(0x08AC3360u, gt::game::func_08AC3360, gt::ReturnKind::Word);
GT_DECOMP(0x08ABC8B4u, gt::game::func_08ABC8B4, gt::ReturnKind::Word);
GT_DECOMP(0x08ABBE30u, gt::game::func_08ABBE30, gt::ReturnKind::Word);
GT_DECOMP(0x089FB954u, gt::game::func_089FB954, gt::ReturnKind::Word);
GT_DECOMP(0x089624F4u, gt::game::func_089624F4, gt::ReturnKind::Word);
GT_DECOMP(0x0894D6C8u, gt::game::func_0894D6C8, gt::ReturnKind::Word);
GT_DECOMP(0x088FE2A4u, gt::game::func_088FE2A4, gt::ReturnKind::Word);
GT_DECOMP(0x088B5D6Cu, gt::game::func_088B5D6C, gt::ReturnKind::Word);
GT_DECOMP(0x08B2B654u, gt::game::func_08B2B654, gt::ReturnKind::Word);
GT_DECOMP(0x08B1ECC8u, gt::game::func_08B1ECC8, gt::ReturnKind::Word);
GT_DECOMP(0x08AFB4B4u, gt::game::func_08AFB4B4, gt::ReturnKind::Word);
GT_DECOMP(0x08AD3B5Cu, gt::game::func_08AD3B5C, gt::ReturnKind::Word);
// func_08ABC9D0 reaches kernel imports through its virtual (not comparable in all 10 cases), so the gate can't check it.
// GT_DECOMP(0x08ABC9D0u, gt::game::func_08ABC9D0, gt::ReturnKind::Word);
GT_DECOMP(0x08AB8720u, gt::game::func_08AB8720, gt::ReturnKind::Word);
GT_DECOMP(0x08AA62D4u, gt::game::func_08AA62D4, gt::ReturnKind::Word);
GT_DECOMP(0x08AA2D50u, gt::game::func_08AA2D50, gt::ReturnKind::Word);
GT_DECOMP(0x0899AFCCu, gt::game::func_0899AFCC, gt::ReturnKind::Word);
GT_DECOMP(0x088C4D28u, gt::game::func_088C4D28, gt::ReturnKind::Word);
GT_DECOMP(0x08B248FCu, gt::game::func_08B248FC, gt::ReturnKind::Word);
// func_08B1B96C reaches kernel imports the isolated run can't replay (not comparable in all 12 cases), so the gate can't check it.
// GT_DECOMP(0x08B1B96Cu, gt::game::func_08B1B96C, gt::ReturnKind::Word);
GT_DECOMP(0x08AB8858u, gt::game::func_08AB8858, gt::ReturnKind::Word);
GT_DECOMP(0x0892FBE0u, gt::game::func_0892FBE0, gt::ReturnKind::Word);
GT_DECOMP(0x0892F820u, gt::game::func_0892F820, gt::ReturnKind::Word);
GT_DECOMP(0x089211A0u, gt::game::func_089211A0, gt::ReturnKind::Word);
GT_DECOMP(0x0890B014u, gt::game::func_0890B014, gt::ReturnKind::Word);
GT_DECOMP(0x088D4DB8u, gt::game::func_088D4DB8, gt::ReturnKind::Word);
GT_DECOMP(0x088D4D14u, gt::game::func_088D4D14, gt::ReturnKind::Word);
GT_DECOMP(0x088A6068u, gt::game::func_088A6068, gt::ReturnKind::Word);
GT_DECOMP(0x0889717Cu, gt::game::func_0889717C, gt::ReturnKind::Word);
GT_DECOMP(0x088936A4u, gt::game::func_088936A4, gt::ReturnKind::Word);
GT_DECOMP(0x0885EC1Cu, gt::game::func_0885EC1C, gt::ReturnKind::Word);
GT_DECOMP(0x0885E8B8u, gt::game::func_0885E8B8, gt::ReturnKind::Word);
GT_DECOMP(0x08B2EDD4u, gt::game::func_08B2EDD4, gt::ReturnKind::Word);
GT_DECOMP(0x08B28C04u, gt::game::func_08B28C04, gt::ReturnKind::Word);
GT_DECOMP(0x08B1CD8Cu, gt::game::func_08B1CD8C, gt::ReturnKind::Word);
GT_DECOMP(0x08AF98D0u, gt::game::func_08AF98D0, gt::ReturnKind::Word);
GT_DECOMP(0x08AF76E8u, gt::game::func_08AF76E8, gt::ReturnKind::Word);
GT_DECOMP(0x08ADC988u, gt::game::func_08ADC988, gt::ReturnKind::Word);
GT_DECOMP(0x08ACF184u, gt::game::func_08ACF184, gt::ReturnKind::Word);
GT_DECOMP(0x08ACCC60u, gt::game::func_08ACCC60, gt::ReturnKind::Word);
GT_DECOMP(0x08A9BD30u, gt::game::func_08A9BD30, gt::ReturnKind::Word);
GT_DECOMP(0x08A9BC30u, gt::game::func_08A9BC30, gt::ReturnKind::Word);
GT_DECOMP(0x08A7AD8Cu, gt::game::func_08A7AD8C, gt::ReturnKind::Word);
GT_DECOMP(0x08A6C134u, gt::game::func_08A6C134, gt::ReturnKind::Word);
GT_DECOMP(0x08A40BF8u, gt::game::func_08A40BF8, gt::ReturnKind::Word);
GT_DECOMP(0x089788A8u, gt::game::func_089788A8, gt::ReturnKind::Word);
GT_DECOMP(0x08961058u, gt::game::func_08961058, gt::ReturnKind::Void);
GT_DECOMP(0x08959EE4u, gt::game::func_08959EE4, gt::ReturnKind::Void);
GT_DECOMP(0x0894D9A0u, gt::game::func_0894D9A0, gt::ReturnKind::Word);
GT_DECOMP(0x08935C2Cu, gt::game::func_08935C2C, gt::ReturnKind::Word);
GT_DECOMP(0x0892BF64u, gt::game::func_0892BF64, gt::ReturnKind::Void);
GT_DECOMP(0x08926514u, gt::game::func_08926514, gt::ReturnKind::Word);
GT_DECOMP(0x08918B84u, gt::game::func_08918B84, gt::ReturnKind::Word);
GT_DECOMP(0x089099F4u, gt::game::func_089099F4, gt::ReturnKind::Word);
GT_DECOMP(0x088F8A9Cu, gt::game::func_088F8A9C, gt::ReturnKind::Word);
GT_DECOMP(0x088F8884u, gt::game::func_088F8884, gt::ReturnKind::Word);
GT_DECOMP(0x088E8A24u, gt::game::func_088E8A24, gt::ReturnKind::Void);
GT_DECOMP(0x088E7F6Cu, gt::game::func_088E7F6C, gt::ReturnKind::Word);
GT_DECOMP(0x088E7EBCu, gt::game::func_088E7EBC, gt::ReturnKind::Void);
GT_DECOMP(0x088E7E94u, gt::game::func_088E7E94, gt::ReturnKind::Void);
GT_DECOMP(0x088DF1F4u, gt::game::func_088DF1F4, gt::ReturnKind::Word);

// --- batch 35 ---

namespace gt::game {
namespace {

constexpr std::uint32_t kScriptValueBase = 0x08CD3638u;  // the base vtable script values fall back to when destroyed
constexpr std::uint32_t kReleaseLocal = 0x08A5B9ECu;     // destroys a temporary (as with_temporary)
constexpr std::uint32_t kAssignValue = 0x08A76ABCu;      // (this, &temporary): assigns a built value
constexpr std::uint32_t kFree = 0x08A5BB84u;             // (object, size)
constexpr std::uint32_t kFreeAligned = 0x08A5BC54u;      // (object, size, alignment)

// `new` of a script value: memory for `size` bytes, then `construct(object, x)` (x = a0 & mask).
void make_object(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t size, std::uint32_t construct,
                 std::uint32_t mask = 0xFFFFFFFFu) {
    const std::uint32_t x = ctx.gpr[4] & mask;
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t object = call_guest(rt, ctx, kAllocate, {size});
    (void)call_guest(rt, ctx, construct, {object, x});
    ctx.gpr[2] = object;
}

// The deleting destructor of a script value: its vtable, `member_dtor(this + 8)`, the base vtable, then
// the memory freed (`size` bytes).
void delete_script_value(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t vtable,
                         std::uint32_t member_dtor, std::uint32_t size) {
    const std::uint32_t self = ctx.gpr[4];
    rt.memory().store32(self, vtable);
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, member_dtor, {self + 0x8u});
    }
    rt.memory().store32(self, kScriptValueBase);
    ctx.gpr[2] = kScriptValueBase;
    tail_call(ctx, kFree, {self, size});
}

// A deleting destructor: its vtable, the member at `member` released, `base_dtor(this)`, then the
// memory freed — (this, 24, 4) aligned, or (this, 16) for `aligned` false.
void delete_object(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t vtable, std::uint32_t member,
                   std::uint32_t base_dtor, bool aligned) {
    const std::uint32_t self = ctx.gpr[4];
    rt.memory().store32(self, vtable);
    ctx.gpr[2] = vtable;
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, kReleaseLocal, {self + member});
        ctx.gpr[2] = call_guest(rt, ctx, base_dtor, {self});
    }
    if (aligned) tail_call(ctx, kFreeAligned, {self, 24u, 4u});
    else tail_call(ctx, kFree, {self, 16u});
}

// `this = value built by build(&local, args...)`: built, assigned (kAssignValue), released; returns this.
void assign_built(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t build,
                  std::initializer_list<std::uint32_t> args, std::initializer_list<float> floats = {}) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    psprecomp::AllegrexContext call = ctx;
    unsigned reg = 5u;
    for (const std::uint32_t value : args) call.gpr[reg++] = value;
    (void)call_guest(rt, call, build, {frame.at(0u)}, floats);
    (void)call_guest(rt, ctx, kAssignValue, {self, frame.at(0u)});
    (void)call_guest(rt, ctx, kReleaseLocal, {frame.at(0u)});
    ctx.gpr[2] = self;
}

// Four optional outputs (signed halfwords from `offset` on); null pointers are skipped.
void halfword_outputs(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t offset) {
    for (unsigned i = 0; i < 4u; ++i) {
        const std::uint32_t out = ctx.gpr[5u + i];
        if (out == 0u) continue;
        ctx.gpr[2] = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(rt.memory().load16(ctx.gpr[4] + offset + 2u * i))));
        rt.memory().store32(out, ctx.gpr[2]);
    }
}

// Three virtuals in a row on this: +0x1B8, +0x19C, then the tail call through `last`.
void three_virtuals(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t last) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, word(rt, word(rt, self) + 0x1B8u), {self});
        ctx.gpr[2] = call_guest(rt, ctx, word(rt, word(rt, self) + 0x19Cu), {self});
    }
    ctx.gpr[3] = word(rt, self);
    tail_call(ctx, word(rt, ctx.gpr[3] + last), {self});
}

// The object behind a smart pointer (func_08A5BB7C(the message at 0x08CB2B50) when it is null).
std::uint32_t checked_target(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4]);
    if (object == 0u) {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08A5BB7Cu, {0x08CB2B50u});
    }
    return object;
}

} // namespace

// `new` script values (12 bytes unless noted): func_08A652B0 (func_08A65288), func_08A6598C,
// func_08A66E04 (24), func_08A66548, func_08A64E8C (20), func_08A65154, func_08A68310, func_08A67EBC,
// func_08A65BE4, func_08A65A54, func_08A64B7C, func_08A66AFC, func_08A65F0C (20), func_08A64624,
// func_08A65B1C, func_08A66998, func_08A689EC, func_08A66750, func_08A67270, func_08A64FF0,
// func_08A67C80, func_08A68B7C (a bool); each constructor sits right before its factory.
void func_08A652B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_object(rt, ctx, 12u, 0x08A65288u); }
void func_08A6598C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_object(rt, ctx, 12u, 0x08A65964u); }
void func_08A66E04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_object(rt, ctx, 24u, 0x08A66DB8u); }
void func_08A66548(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_object(rt, ctx, 12u, 0x08A6651Cu); }
void func_08A64E8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_object(rt, ctx, 20u, 0x08A64E60u); }
void func_08A65154(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_object(rt, ctx, 12u, 0x08A65128u); }
void func_08A68310(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_object(rt, ctx, 12u, 0x08A682E8u); }
void func_08A67EBC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_object(rt, ctx, 12u, 0x08A67E94u); }
void func_08A65BE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_object(rt, ctx, 12u, 0x08A65BBCu); }
void func_08A65A54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_object(rt, ctx, 12u, 0x08A65A2Cu); }
void func_08A64B7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_object(rt, ctx, 12u, 0x08A64B44u); }
void func_08A66AFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_object(rt, ctx, 12u, 0x08A66AD0u); }
void func_08A65F0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_object(rt, ctx, 20u, 0x08A65EE0u); }
void func_08A64624(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_object(rt, ctx, 12u, 0x08A645ECu); }
void func_08A65B1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_object(rt, ctx, 12u, 0x08A65AF4u); }
void func_08A66998(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_object(rt, ctx, 12u, 0x08A6696Cu); }
void func_08A689EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_object(rt, ctx, 12u, 0x08A689C4u); }
void func_08A66750(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_object(rt, ctx, 12u, 0x08A66728u); }
void func_08A67270(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_object(rt, ctx, 12u, 0x08A67244u); }
void func_08A64FF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_object(rt, ctx, 12u, 0x08A64FC4u); }
void func_08A67C80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_object(rt, ctx, 12u, 0x08A67C54u); }
void func_08A68B7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_object(rt, ctx, 12u, 0x08A68B54u, 0xFFu); }

// func_08A65680(x) -> a new 12-byte script value func_08A65658(object, x) builds from the float x.
void func_08A65680(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const float x = ctx.fpr[12];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t object = call_guest(rt, ctx, kAllocate, {12u});
    (void)call_guest(rt, ctx, 0x08A65658u, {object}, {x});
    ctx.gpr[2] = object;
}

// Deleting destructors of script values: func_08A671D0 / func_08A66EC8 (24 bytes), func_08A6702C /
// func_08A64F50 / func_08A65FD0 (20), over func_08A75A88; func_08A65218 / func_08A66BC0 / func_08A66A5C
// (12) over func_08A69D68.
void func_08A671D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { delete_script_value(rt, ctx, 0x08CD2E80u, 0x08A75A88u, 24u); }
void func_08A66EC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { delete_script_value(rt, ctx, 0x08CD2E30u, 0x08A75A88u, 24u); }
void func_08A6702C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { delete_script_value(rt, ctx, 0x08CD2E58u, 0x08A75A88u, 20u); }
void func_08A64F50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { delete_script_value(rt, ctx, 0x08CD2908u, 0x08A75A88u, 20u); }
void func_08A65FD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { delete_script_value(rt, ctx, 0x08CD2BD8u, 0x08A75A88u, 20u); }
void func_08A65218(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { delete_script_value(rt, ctx, 0x08CD2958u, 0x08A69D68u, 12u); }
void func_08A66BC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { delete_script_value(rt, ctx, 0x08CD2DE0u, 0x08A69D68u, 12u); }
void func_08A66A5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { delete_script_value(rt, ctx, 0x08CD2DB8u, 0x08A69D68u, 12u); }

// Deleting destructors that release the member at +0x14 then run a base destructor, freeing 24 bytes
// aligned to 4: func_08A87510 (func_08A780B0), func_08A19134 (func_08A5D638), func_08A74E24
// (func_08A8036C), func_08A752D4 (func_08A86D24), func_08A74660 (func_08A72654); and with the member at
// +0xC over func_08A64554, freeing 16: func_08A646A0, func_08A6494C.
void func_08A87510(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { delete_object(rt, ctx, 0x08CBEF70u, 0x14u, 0x08A780B0u, true); }
void func_08A19134(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { delete_object(rt, ctx, 0x08CD0618u, 0x14u, 0x08A5D638u, true); }
void func_08A74E24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { delete_object(rt, ctx, 0x08CD3A58u, 0x14u, 0x08A8036Cu, true); }
void func_08A752D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { delete_object(rt, ctx, 0x08CD3AA0u, 0x14u, 0x08A86D24u, true); }
void func_08A74660(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { delete_object(rt, ctx, 0x08CD3A18u, 0x14u, 0x08A72654u, true); }
void func_08A646A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { delete_object(rt, ctx, 0x08CD2BB0u, 0xCu, 0x08A64554u, false); }
void func_08A6494C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { delete_object(rt, ctx, 0x08CD2A20u, 0xCu, 0x08A64554u, false); }

// func_08A1289C(this): deleting destructor (vtable, func_08A1256C, func_089E4C98, 40 bytes freed).
void func_08A1289C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    rt.memory().store32(self, kVtable03E0);
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08A1256Cu, {self});
        ctx.gpr[2] = call_guest(rt, ctx, 0x089E4C98u, {self});
    }
    tail_call(ctx, kFreeAligned, {self, 40u, 4u});
}

// func_08A72B94(this): func_08A72AF4(this), then frees the 32 + 4 * count bytes (count at +0x4).
void func_08A72B94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t count = word(rt, self + 0x4u);
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08A72AF4u, {self});
    }
    tail_call(ctx, kFreeAligned, {self, count * 4u + 32u, 4u});
}

// Assignments through a built temporary (returning this): func_08A5DFD0(this, src) builds with
// func_08A5DEE8(&t, src, +0x18, +0x1C); func_08A85BF0 with func_08A85B1C(&t, +0x14); func_08A6D334 with
// func_08A6D2D4(&t, src + 0x14); func_08A87FC0 with func_08A87EEC(&t, the float at +0x14);
// func_08A97B40 with func_08A97A6C(&t, whether the byte at +0x14 is set).
void func_08A5DFD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t src = ctx.gpr[5];
    assign_built(rt, ctx, 0x08A5DEE8u, {src, word(rt, src + 0x18u), word(rt, src + 0x1Cu)});
}
void func_08A85BF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    assign_built(rt, ctx, 0x08A85B1Cu, {word(rt, ctx.gpr[5] + 0x14u)});
}
void func_08A6D334(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    assign_built(rt, ctx, 0x08A6D2D4u, {ctx.gpr[5] + 0x14u});
}
void func_08A87FC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    assign_built(rt, ctx, 0x08A87EECu, {ctx.gpr[5]}, {float_at(rt, ctx.gpr[5] + 0x14u)});
}
void func_08A97B40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    assign_built(rt, ctx, 0x08A97A6Cu, {rt.memory().load8(ctx.gpr[5] + 0x14u) != 0u ? 1u : 0u});
}

// func_08A97D80(this, out): builds a value with func_08A97A6C(&t, whether the byte at +0x14 is clear)
// and hands it to func_08A5BAD8(out, t).
void func_08A97D80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[5];
    const std::uint32_t clear = rt.memory().load8(ctx.gpr[4] + 0x14u) == 0u ? 1u : 0u;
    GuestFrame frame(ctx, 0x20u);
    (void)call_guest(rt, ctx, 0x08A97A6Cu, {frame.at(0u), clear});
    if (out != frame.at(0u)) (void)call_guest(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(0u))});
    ctx.gpr[2] = call_guest(rt, ctx, kReleaseLocal, {frame.at(0u)});
}

// func_088FB100(this, flag) -> func_089010A0(second, flag) / func_08903568(first, flag), 0.0 without.
void func_088FB100(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t flag = ctx.gpr[5] & 0xFFu;
    ctx.gpr[3] = flag;
    ctx.gpr[6] = self;
    ctx.gpr[5] = flag;
    if (const std::uint32_t second = word(rt, self + 0x4u); second != 0u) {
        ctx.gpr[2] = second;
        tail_call(ctx, 0x089010A0u, {second});
        return;
    }
    const std::uint32_t first = word(rt, self);
    ctx.gpr[2] = first;
    ctx.gpr[4] = first;
    ctx.fpr[0] = 0.0f;
    if (first != 0u) tail_call(ctx, 0x08903568u, {});
}

// func_08A69654(this): this->value = func_08AACAF8(the table at 0x08CFF060); func_08A69638(value) when set.
void func_08A69654(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t value;
    {
        GuestFrame frame(ctx, 0x10u);
        value = call_guest(rt, ctx, 0x08AACAF8u, {0x08CFF060u});
    }
    rt.memory().store32(self, value);
    ctx.gpr[2] = value;
    ctx.gpr[4] = value;
    if (value != 0u) tail_call(ctx, 0x08A69638u, {});
}

// func_088C4E68(this, a, float *out) -> y of func_088FAC80(&pair, the object at +0x10, a); x into *out.
void func_088C4E68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[6];
    const std::uint32_t object = word(rt, ctx.gpr[4] + 0x10u);
    GuestFrame frame(ctx, 0x20u);
    (void)call_guest(rt, ctx, 0x088FAC80u, {frame.at(0u), object, ctx.gpr[5]});
    if (out != 0u) rt.memory().store32(out, word(rt, frame.at(0u)));
    ctx.fpr[0] = float_at(rt, frame.at(4u));
}

// func_08B07F6C(a, b, c): func_08B07ACC(a, b, c), then func_08B059D0(a, b, c).
void func_08B07F6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = ctx.gpr[4], b = ctx.gpr[5], c = ctx.gpr[6];
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08B07ACCu, {a, b, c});
    }
    tail_call(ctx, 0x08B059D0u, {a, b, c});
}

// func_08AD1528(this): func_08B4E648(this + 0x40), then func_08B4E734(this, entry i (80 bytes) at
// +0x80), i the word at +0x1C0.
void func_08AD1528(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08B4E648u, {self + 0x40u});
    }
    const std::uint32_t i = word(rt, self + 0x1C0u);
    ctx.gpr[2] = i << 6;
    tail_call(ctx, 0x08B4E734u, {self, self + i * 80u + 0x80u});
}

// func_089260FC(..., y, z) -> func_08B662C4(...) * (y * 0.5) * z (PSP float products).
void func_089260FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const float y = ctx.fpr[13];
    const float z = ctx.fpr[14];
    GuestFrame frame(ctx, 0x20u);
    const float r = call_guest(rt, ctx, 0x08B662C4u, {}, {}).f0;
    ctx.fpr[0] = psp_mul(psp_mul(r, psp_mul(y, 0.5f)), z);
}

// func_08A5CFB8(this, x): func_08A76ACC(this, x), the vtable, then func_08A6972C(this + 0x14, x).
void func_08A5CFB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t x = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08A76ACCu, {self, x});
    }
    rt.memory().store32(self, 0x08CCB870u);
    ctx.gpr[2] = 0x08CCB870u;
    tail_call(ctx, 0x08A6972Cu, {self + 0x14u, x});
}

// Assignment operators: func_089FB570(this, other) -> this (func_08C0E654, then the word at +0xC);
// func_089DB5CC(this, other) (func_089641C4 on the member at +0x150, then its word at +0x10).
void func_089FB570(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t other = ctx.gpr[5];
    if (self != other) {
        {
            GuestFrame frame(ctx, 0x10u);
            (void)call_guest(rt, ctx, 0x08C0E654u, {self, other});
        }
        ctx.gpr[3] = word(rt, other + 0xCu);
        rt.memory().store32(self + 0xCu, ctx.gpr[3]);
    }
    ctx.gpr[2] = self;
}
void func_089DB5CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t member = ctx.gpr[4] + 0x150u;
    const std::uint32_t other = ctx.gpr[5];
    ctx.gpr[4] = member;
    if (member == other) return;
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x089641C4u, {member, other});
    }
    ctx.gpr[3] = word(rt, other + 0x10u);
    rt.memory().store32(member + 0x10u, ctx.gpr[3]);
}

// func_08A0F114 / func_08A0F0CC(this): the virtuals +0x1B8 and +0x19C, then +0x1B4 / +0x1B0.
void func_08A0F114(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { three_virtuals(rt, ctx, 0x1B4u); }
void func_08A0F0CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { three_virtuals(rt, ctx, 0x1B0u); }

// func_0892085C(_, object): func_08B4E648(object), then func_08B4E9D4(object, object + 0xC0 / + 0x100 /
// + 0x140).
void func_0892085C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08B4E648u, {object});
        (void)call_guest(rt, ctx, 0x08B4E9D4u, {object, object + 0xC0u});
        ctx.gpr[2] = call_guest(rt, ctx, 0x08B4E9D4u, {object, object + 0x100u});
    }
    tail_call(ctx, 0x08B4E9D4u, {object, object + 0x140u});
}

// func_0890B3E0(this, value, flag, x): appends (value, x, flag) to the 12-byte entries at +0x40 while
// fewer than 2 (count at +0x5C).
void func_0890B3E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t count = word(rt, self + 0x5Cu);
    const std::uint32_t entry = self + count * 12u + 0x40u;
    ctx.gpr[7] = self;
    ctx.gpr[6] &= 0xFFu;
    ctx.gpr[3] = entry;
    ctx.gpr[4] = count + 1u;
    ctx.gpr[8] = entry + 0x4u;
    ctx.gpr[2] = static_cast<std::int32_t>(count) < 2 ? 1u : 0u;
    if (ctx.gpr[2] == 0u) return;
    rt.memory().store32(self + 0x5Cu, count + 1u);
    rt.memory().store32(entry + 0x4u, ctx.gpr[5]);
    rt.memory().store8(entry + 0xCu, static_cast<std::uint8_t>(ctx.gpr[6]));
    rt.memory().store32(entry + 0x8u, std::bit_cast<std::uint32_t>(ctx.fpr[12]));
}

// func_088F29A8(a, b): func_088F2CEC(a's +0x8, b's +0x1C), then func_08AF1DE0(b's +0x4, a's +0x8, 0x10000).
void func_088F29A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = ctx.gpr[4];
    const std::uint32_t b = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x088F2CECu, {word(rt, a + 0x8u), word(rt, b + 0x1Cu)});
    }
    tail_call(ctx, 0x08AF1DE0u, {word(rt, b + 0x4u), word(rt, a + 0x8u), 0x10000u});
}

// func_088BD678(_, x) -> func_088BD574(x) when func_088BBC40(x) holds, else 0.0.
void func_088BD678(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t x = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x088BBC40u, {x});
    }
    ctx.gpr[4] = x;
    if (ctx.gpr[2] != 0u) {
        tail_call(ctx, 0x088BD574u, {});
        return;
    }
    ctx.fpr[0] = 0.0f;
}

// func_089F9B74(this): func_08BE5888(+0x0, +0x4, &local), then func_08B35254(+0x0) when it is set.
void func_089F9B74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    (void)call_guest(rt, ctx, 0x08BE5888u, {word(rt, self), word(rt, self + 0x4u), frame.at(0u)});
    const std::uint32_t object = word(rt, self);
    ctx.gpr[2] = object;
    if (object != 0u) ctx.gpr[2] = call_guest(rt, ctx, 0x08B35254u, {object});
}

// Optional halfword outputs: func_08A0D048 (from +0x118), func_0899A9E4 (from +0x120).
void func_08A0D048(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { halfword_outputs(rt, ctx, 0x118u); }
void func_0899A9E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { halfword_outputs(rt, ctx, 0x120u); }

// func_08ADF7F8(a, b) -> func_08ADF670(found, b, 0.0) for found = func_08ADDD88(a, the table at
// 0x08C91128), 0 when none.
void func_08ADF7F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t b = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08ADDD88u, {ctx.gpr[4], 0x08C91128u});
    }
    ctx.gpr[4] = ctx.gpr[2];
    ctx.gpr[5] = b;
    ctx.fpr[12] = 0.0f;
    if (ctx.gpr[2] != 0u) tail_call(ctx, 0x08ADF670u, {});
}

// func_08AD5658(this, flag): func_08ADF7F8(the handle at +0x10, !flag) and forgets the handle (-1).
void func_08AD5658(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t inverted = (ctx.gpr[5] & 0xFFu) ^ 1u;
    const std::uint32_t handle = word(rt, self + 0x10u);
    ctx.gpr[2] = handle;
    if (static_cast<std::int32_t>(handle) < 0) return;
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08ADF7F8u, {handle, inverted});
    }
    ctx.gpr[2] = 0xFFFFFFFFu;
    rt.memory().store32(self + 0x10u, 0xFFFFFFFFu);
}

// func_08ABC138(holder) -> the virtual at +0x6C of *holder (object, holder), 0 when either is null.
void func_08ABC138(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t holder = ctx.gpr[4];
    const std::uint32_t object = holder != 0u ? word(rt, holder) : 0u;
    ctx.gpr[2] = 0u;
    if (object == 0u) return;
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, word(rt, word(rt, object) + 0x6Cu), {object, holder});
}

// Smart-pointer virtuals: func_08AA2AFC(p) -> the virtual at +0x30 of *p; func_08AA2898(p) -> +0x54
// (as a tail call).
void func_08AA2AFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = checked_target(rt, ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, word(rt, word(rt, object) + 0x30u), {object});
}
void func_08AA2898(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = checked_target(rt, ctx);
    ctx.gpr[3] = word(rt, object);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 0x54u);
    tail_call(ctx, ctx.gpr[2], {object});
}

// func_08968C84(this) -> func_08968C38(this) and bit 17 of the flags at +0xE0.
void func_08968C84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t ok;
    {
        GuestFrame frame(ctx, 0x10u);
        ok = call_guest(rt, ctx, 0x08968C38u, {self});
    }
    ctx.gpr[2] = ok != 0u && (word(rt, self + 0xE0u) & 0x20000u) != 0u ? 1u : 0u;
}

// func_08930170(this) -> func_08930140(this) (64-bit), or func_08930110(this) when that is -1.
void func_08930170(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestResult result;
    {
        GuestFrame frame(ctx, 0x10u);
        result = call_guest(rt, ctx, 0x08930140u, {self}, {});
    }
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.gpr[5] = 0xFFFFFFFFu;
    if (result.v0 == 0xFFFFFFFFu && result.v1 == 0xFFFFFFFFu) tail_call(ctx, 0x08930110u, {self});
}

// func_08ACCB50(this, i, out_a, out_b): the pair at +0x3FC of entry i (236 bytes) for 0 <= i <= the
// count at +0x328, else (0, 0).
void func_08ACCB50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const auto i = static_cast<std::int32_t>(ctx.gpr[5]);
    ctx.gpr[2] = ctx.gpr[5] << 2;
    if (i >= 0) {
        const std::uint32_t entry = ctx.gpr[4] + ctx.gpr[5] * 236u;
        ctx.gpr[3] = entry;
        ctx.gpr[2] = static_cast<std::int32_t>(word(rt, ctx.gpr[4] + 0x328u)) < i ? 1u : 0u;
        if (ctx.gpr[2] == 0u) {
            ctx.gpr[2] = word(rt, entry + 0x3FCu);
            rt.memory().store32(ctx.gpr[6], ctx.gpr[2]);
            ctx.gpr[3] = word(rt, entry + 0x400u);
            rt.memory().store32(ctx.gpr[7], ctx.gpr[3]);
            return;
        }
    }
    rt.memory().store32(ctx.gpr[6], 0u);
    rt.memory().store32(ctx.gpr[7], 0u);
}

// func_089E4AEC(this): base constructor func_089FE2F8, the vtable, then 1.0 at +0x18 / +0x1C and the
// halfwords at +0x12 / +0x14 cleared.
void func_089E4AEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x089FE2F8u, {self});
    }
    const std::uint32_t one = std::bit_cast<std::uint32_t>(1.0f);
    rt.memory().store32(self + 0x1Cu, one);
    rt.memory().store32(self, 0x08CCF8B0u);
    rt.memory().store16(self + 0x12u, 0u);
    rt.memory().store16(self + 0x14u, 0u);
    rt.memory().store32(self + 0x18u, one);
    ctx.fpr[0] = 1.0f;
    ctx.gpr[2] = 0x08CCF8B0u;
}

// func_08B2BA34(this, at): removes the 8-byte entry `at` from the array whose start is at +0x44
// (func_08B79BAC moves the earlier entries up), one less at +0x48.
void func_08B2BA34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t start = word(rt, self + 0x44u);
    rt.memory().store32(self + 0x44u, start + 8u);
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08B79BACu, {start + 8u, start, (ctx.gpr[5] - start) & ~7u});
    }
    ctx.gpr[3] = word(rt, self + 0x48u) - 1u;
    rt.memory().store32(self + 0x48u, ctx.gpr[3]);
}

// func_08B17308(this): when the flag at +0x19DD is set: clears +0x19EC / +0x19F0, func_08B28C74(this +
// 0x1984), func_08B28C44(this + 0x1900), and clears the flag.
void func_08B17308(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    ctx.gpr[2] = rt.memory().load8(self + 0x19DDu);
    ctx.gpr[4] = self + 0x1984u;
    if (ctx.gpr[2] == 0u) return;
    rt.memory().store32(self + 0x19F0u, 0u);
    rt.memory().store32(self + 0x19ECu, 0u);
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08B28C74u, {self + 0x1984u});
        ctx.gpr[2] = call_guest(rt, ctx, 0x08B28C44u, {self + 0x1900u});
    }
    rt.memory().store8(self + 0x19DDu, 0u);
}

// func_08AD5F18(this): releases the object at +0x28 with func_08ABBF28 inside func_08AB9CB0 /
// func_08AB9CFC, and clears it.
void func_08AD5F18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    ctx.gpr[2] = word(rt, self + 0x28u);
    if (ctx.gpr[2] == 0u) return;
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08AB9CB0u, {});
        (void)call_guest(rt, ctx, 0x08ABBF28u, {word(rt, self + 0x28u)});
        ctx.gpr[2] = call_guest(rt, ctx, 0x08AB9CFCu, {word(rt, self + 0x28u)});
    }
    rt.memory().store32(self + 0x28u, 0u);
}

// func_08A52824(this) -> func_08A527E8(this) + the virtual at +0x120.
void func_08A52824(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t first = call_guest(rt, ctx, 0x08A527E8u, {self});
    ctx.gpr[2] = first + call_guest(rt, ctx, word(rt, word(rt, self) + 0x120u), {self});
}

// func_08965A3C(this): destructor — vtable, func_08A5B930(the object at +0x20) when set, then
// func_089BA380(this).
void func_08965A3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t object = word(rt, self + 0x20u);
    rt.memory().store32(self, 0x08CC6970u);
    ctx.gpr[2] = 0x08CC6970u;
    if (object != 0u) {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08A5B930u, {object});
    }
    tail_call(ctx, 0x089BA380u, {self});
}

// func_08932450(dst, src) -> dst, after copying 384 bytes.
void func_08932450(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t dst = ctx.gpr[4];
    const std::uint32_t src = ctx.gpr[5];
    for (std::uint32_t offset = 0u; offset < 384u; offset += 4u) rt.memory().store32(dst + offset, word(rt, src + offset));
    ctx.gpr[2] = dst;
}

// func_089FFD5C(this): destructor — vtables at +0x0 / +0x38, func_089B8884(this + 0x78), then
// func_08A101F8(this).
void func_089FFD5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    rt.memory().store32(self, 0x08CC73A8u);
    rt.memory().store32(self + 0x38u, 0x08CC7588u);
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x089B8884u, {self + 0x78u});
    }
    tail_call(ctx, 0x08A101F8u, {self});
}

} // namespace gt::game

GT_DECOMP(0x088FB100u, gt::game::func_088FB100, gt::ReturnKind::Float);
GT_DECOMP(0x08A69654u, gt::game::func_08A69654, gt::ReturnKind::Word);
GT_DECOMP(0x088C4E68u, gt::game::func_088C4E68, gt::ReturnKind::Float);
GT_DECOMP(0x08B07F6Cu, gt::game::func_08B07F6C, gt::ReturnKind::Word);
GT_DECOMP(0x08AD1528u, gt::game::func_08AD1528, gt::ReturnKind::Word);
GT_DECOMP(0x08A671D0u, gt::game::func_08A671D0, gt::ReturnKind::Word);
GT_DECOMP(0x089260FCu, gt::game::func_089260FC, gt::ReturnKind::Float);
GT_DECOMP(0x08A87510u, gt::game::func_08A87510, gt::ReturnKind::Word);
GT_DECOMP(0x08A5CFB8u, gt::game::func_08A5CFB8, gt::ReturnKind::Word);
GT_DECOMP(0x08A19134u, gt::game::func_08A19134, gt::ReturnKind::Word);
GT_DECOMP(0x089FB570u, gt::game::func_089FB570, gt::ReturnKind::Word);
GT_DECOMP(0x08A0F114u, gt::game::func_08A0F114, gt::ReturnKind::Word);
GT_DECOMP(0x0892085Cu, gt::game::func_0892085C, gt::ReturnKind::Word);
GT_DECOMP(0x089DB5CCu, gt::game::func_089DB5CC, gt::ReturnKind::Void);
GT_DECOMP(0x0890B3E0u, gt::game::func_0890B3E0, gt::ReturnKind::Void);
GT_DECOMP(0x088F29A8u, gt::game::func_088F29A8, gt::ReturnKind::Word);
GT_DECOMP(0x088BD678u, gt::game::func_088BD678, gt::ReturnKind::Float);
GT_DECOMP(0x089F9B74u, gt::game::func_089F9B74, gt::ReturnKind::Word);
GT_DECOMP(0x08A652B0u, gt::game::func_08A652B0, gt::ReturnKind::Word);
GT_DECOMP(0x08A0D048u, gt::game::func_08A0D048, gt::ReturnKind::Void);
GT_DECOMP(0x08A6702Cu, gt::game::func_08A6702C, gt::ReturnKind::Word);
GT_DECOMP(0x08A6598Cu, gt::game::func_08A6598C, gt::ReturnKind::Word);
GT_DECOMP(0x08A66E04u, gt::game::func_08A66E04, gt::ReturnKind::Word);
GT_DECOMP(0x08A66548u, gt::game::func_08A66548, gt::ReturnKind::Word);
GT_DECOMP(0x08A64E8Cu, gt::game::func_08A64E8C, gt::ReturnKind::Word);
GT_DECOMP(0x08A66EC8u, gt::game::func_08A66EC8, gt::ReturnKind::Word);
GT_DECOMP(0x08A65680u, gt::game::func_08A65680, gt::ReturnKind::Word);
GT_DECOMP(0x08A72B94u, gt::game::func_08A72B94, gt::ReturnKind::Word);
GT_DECOMP(0x08A65154u, gt::game::func_08A65154, gt::ReturnKind::Word);
GT_DECOMP(0x08A64F50u, gt::game::func_08A64F50, gt::ReturnKind::Word);
// func_08ADF7F8 reaches kernel imports the isolated run can't replay (not comparable in all 15 cases),
// so the gate can't check it.
// GT_DECOMP(0x08ADF7F8u, gt::game::func_08ADF7F8, gt::ReturnKind::Word);
GT_DECOMP(0x08A68310u, gt::game::func_08A68310, gt::ReturnKind::Word);
GT_DECOMP(0x08A85BF0u, gt::game::func_08A85BF0, gt::ReturnKind::Word);
GT_DECOMP(0x08A67EBCu, gt::game::func_08A67EBC, gt::ReturnKind::Word);
GT_DECOMP(0x08A65BE4u, gt::game::func_08A65BE4, gt::ReturnKind::Word);
GT_DECOMP(0x08AD5658u, gt::game::func_08AD5658, gt::ReturnKind::Void);
GT_DECOMP(0x08A65218u, gt::game::func_08A65218, gt::ReturnKind::Word);
GT_DECOMP(0x08A68B7Cu, gt::game::func_08A68B7C, gt::ReturnKind::Word);
GT_DECOMP(0x0899A9E4u, gt::game::func_0899A9E4, gt::ReturnKind::Void);
GT_DECOMP(0x08ABC138u, gt::game::func_08ABC138, gt::ReturnKind::Word);
GT_DECOMP(0x08A65A54u, gt::game::func_08A65A54, gt::ReturnKind::Word);
GT_DECOMP(0x08A646A0u, gt::game::func_08A646A0, gt::ReturnKind::Word);
GT_DECOMP(0x08A74E24u, gt::game::func_08A74E24, gt::ReturnKind::Word);
GT_DECOMP(0x08AA2AFCu, gt::game::func_08AA2AFC, gt::ReturnKind::Word);
GT_DECOMP(0x08968C84u, gt::game::func_08968C84, gt::ReturnKind::Word);
GT_DECOMP(0x08A64B7Cu, gt::game::func_08A64B7C, gt::ReturnKind::Word);
GT_DECOMP(0x08A6494Cu, gt::game::func_08A6494C, gt::ReturnKind::Word);
GT_DECOMP(0x08A66AFCu, gt::game::func_08A66AFC, gt::ReturnKind::Word);
GT_DECOMP(0x08930170u, gt::game::func_08930170, gt::ReturnKind::DoubleWord);
GT_DECOMP(0x08A65F0Cu, gt::game::func_08A65F0C, gt::ReturnKind::Word);
GT_DECOMP(0x08A65FD0u, gt::game::func_08A65FD0, gt::ReturnKind::Word);
GT_DECOMP(0x08ACCB50u, gt::game::func_08ACCB50, gt::ReturnKind::Void);
GT_DECOMP(0x08A66BC0u, gt::game::func_08A66BC0, gt::ReturnKind::Word);
GT_DECOMP(0x08A64624u, gt::game::func_08A64624, gt::ReturnKind::Word);
GT_DECOMP(0x08A752D4u, gt::game::func_08A752D4, gt::ReturnKind::Word);
GT_DECOMP(0x089E4AECu, gt::game::func_089E4AEC, gt::ReturnKind::Word);
GT_DECOMP(0x08A5DFD0u, gt::game::func_08A5DFD0, gt::ReturnKind::Word);
GT_DECOMP(0x08B2BA34u, gt::game::func_08B2BA34, gt::ReturnKind::Word);
GT_DECOMP(0x08A6D334u, gt::game::func_08A6D334, gt::ReturnKind::Word);
GT_DECOMP(0x08A65B1Cu, gt::game::func_08A65B1C, gt::ReturnKind::Word);
GT_DECOMP(0x08A66998u, gt::game::func_08A66998, gt::ReturnKind::Word);
GT_DECOMP(0x08A97D80u, gt::game::func_08A97D80, gt::ReturnKind::Void);
GT_DECOMP(0x08AA2898u, gt::game::func_08AA2898, gt::ReturnKind::Word);
GT_DECOMP(0x08A1289Cu, gt::game::func_08A1289C, gt::ReturnKind::Word);
GT_DECOMP(0x08A87FC0u, gt::game::func_08A87FC0, gt::ReturnKind::Word);
GT_DECOMP(0x08A74660u, gt::game::func_08A74660, gt::ReturnKind::Word);
GT_DECOMP(0x08A689ECu, gt::game::func_08A689EC, gt::ReturnKind::Word);
GT_DECOMP(0x08A66750u, gt::game::func_08A66750, gt::ReturnKind::Word);
GT_DECOMP(0x08A66A5Cu, gt::game::func_08A66A5C, gt::ReturnKind::Word);
GT_DECOMP(0x089FFD5Cu, gt::game::func_089FFD5C, gt::ReturnKind::Word);
GT_DECOMP(0x08A97B40u, gt::game::func_08A97B40, gt::ReturnKind::Word);
// func_08B17308 reaches kernel imports the isolated run can't replay (not comparable in all 28 cases),
// so the gate can't check it.
// GT_DECOMP(0x08B17308u, gt::game::func_08B17308, gt::ReturnKind::Void);
GT_DECOMP(0x08A0F0CCu, gt::game::func_08A0F0CC, gt::ReturnKind::Word);
GT_DECOMP(0x08AD5F18u, gt::game::func_08AD5F18, gt::ReturnKind::Void);
GT_DECOMP(0x08A52824u, gt::game::func_08A52824, gt::ReturnKind::Word);
GT_DECOMP(0x08A67270u, gt::game::func_08A67270, gt::ReturnKind::Word);
GT_DECOMP(0x08A64FF0u, gt::game::func_08A64FF0, gt::ReturnKind::Word);
GT_DECOMP(0x08A67C80u, gt::game::func_08A67C80, gt::ReturnKind::Word);
GT_DECOMP(0x08965A3Cu, gt::game::func_08965A3C, gt::ReturnKind::Word);
GT_DECOMP(0x08932450u, gt::game::func_08932450, gt::ReturnKind::Word);

// --- batch 36 ---

namespace gt::game {
namespace {

constexpr std::uint32_t kLock = 0x08B344E0u;        // (mutex): lock (as member_76_pair)
constexpr std::uint32_t kUnlock = 0x08B3457Cu;      // (mutex): unlock
constexpr std::uint32_t kListeners = 0x08D7717Cu;   // the listener list func_08AB9CB0 / func_08AB9CFC walk
constexpr std::uint32_t kGlobalLock4470 = 0x08CF4470u;  // the mutex around the reads of func_089948A8 / func_08994810
constexpr std::uint32_t kRegistry0598 = 0x08D40598u;    // what func_08AD5DFC / func_08AD5940 register with
constexpr std::uint32_t kAssignSmart = 0x08A5BAD8u;     // (dst, value): smart-pointer assignment

// `virtual at slot` of each listener (node +0x4) on the list at kListeners (next at +0x0).
void notify_listeners(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t slot) {
    ctx.gpr[2] = 0x08D70000u;
    GuestFrame frame(ctx, 0x10u);
    for (std::uint32_t node = word(rt, kListeners); node != 0u; node = word(rt, node)) {
        const std::uint32_t listener = word(rt, node + 0x4u);
        ctx.gpr[2] = call_guest(rt, ctx, word(rt, word(rt, listener) + slot), {listener});
    }
}

// `virtual at slot` of each object in the sibling chain from the word at `first` (next at +0x54).
void for_each_sibling(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t first, std::uint32_t slot) {
    GuestFrame frame(ctx, 0x10u);
    for (std::uint32_t child = word(rt, ctx.gpr[4] + first); child != 0u; child = word(rt, child + 0x54u))
        ctx.gpr[2] = call_guest(rt, ctx, word(rt, word(rt, child) + slot), {child});
}

// Resets the member at `offset` to a fresh value: `base(this)`, then build(&t, the table at
// kLookupTable), assigned with kAssignSmart and released.
void reset_member(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t base, std::uint32_t build,
                  std::uint32_t offset) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    (void)call_guest(rt, ctx, base, {self});
    (void)call_guest(rt, ctx, build, {frame.at(0u), kLookupTable});
    if (self + offset != frame.at(0u)) (void)call_guest(rt, ctx, kAssignSmart, {self + offset, word(rt, frame.at(0u))});
    ctx.gpr[2] = call_guest(rt, ctx, kReleaseLocal, {frame.at(0u)});
}

// `out = value built by build(&t, x)` (x computed by the caller), skipped when out is &t itself.
void store_built(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t build, std::uint32_t x) {
    const std::uint32_t out = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    (void)call_guest(rt, ctx, build, {frame.at(0u), x});
    if (out != frame.at(0u)) (void)call_guest(rt, ctx, kAssignSmart, {out, word(rt, frame.at(0u))});
    ctx.gpr[2] = call_guest(rt, ctx, kReleaseLocal, {frame.at(0u)});
}

// `base(this)`, then the object at +0x14 (func_08A5BB7C(message) when null) handed to
// func_08A7BE18(object, x).
void forward_member20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t message) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t x = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, 0x08A5D4A0u, {self, x});
    const std::uint32_t object = word(rt, self + 0x14u);
    if (object == 0u) ctx.gpr[2] = call_guest(rt, ctx, 0x08A5BB7Cu, {message});
    tail_call(ctx, 0x08A7BE18u, {object, x});
}

// A deleting destructor that releases two members (+0x1C and +0x18), runs func_08A5D354, frees 32
// bytes aligned to 4.
void delete_two_members(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t vtable) {
    const std::uint32_t self = ctx.gpr[4];
    rt.memory().store32(self, vtable);
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, kReleaseLocal, {self + 0x1Cu});
        (void)call_guest(rt, ctx, kReleaseLocal, {self + 0x18u});
        ctx.gpr[2] = call_guest(rt, ctx, 0x08A5D354u, {self});
    }
    tail_call(ctx, kFreeAligned, {self, 32u, 4u});
}

// `call(this, x)` between kLock / kUnlock of the mutex at this + `mutex` (`target` gets this + `arg`).
void locked_call(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t mutex, std::uint32_t call,
                 std::uint32_t arg) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, kLock, {self + mutex});
        ctx.gpr[2] = call_guest(rt, ctx, call, {self + arg, ctx.gpr[5]});
    }
    tail_call(ctx, kUnlock, {self + mutex});
}

// The word at `offset`, read under the mutex at kGlobalLock4470.
void locked_read(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t offset) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    (void)call_guest(rt, ctx, kLock, {kGlobalLock4470});
    const std::uint32_t value = word(rt, self + offset);
    (void)call_guest(rt, ctx, kUnlock, {kGlobalLock4470});
    ctx.gpr[2] = value;
}

// The byte at `offset` returned and cleared, inside func_08B34888 / func_08B34B24 on this + 0x10.
void take_flag(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t offset) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    (void)call_guest(rt, ctx, 0x08B34888u, {self + 0x10u});
    const std::uint32_t flag = rt.memory().load8(self + offset);
    rt.memory().store8(self + offset, 0u);
    (void)call_guest(rt, ctx, 0x08B34B24u, {self + 0x10u});
    ctx.gpr[2] = flag;
}

// func_0898A188 / func_0898A1D4: `first(this)`, then func_0898A0CC(this, the table at 0x08C8C0C4) when
// the state byte at +0x1E8 is `state`.
void when_state(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t first, std::uint32_t state) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, first, {self});
    }
    ctx.gpr[3] = rt.memory().load8(self + 0x1E8u);
    ctx.gpr[2] = state;
    ctx.gpr[4] = self;
    ctx.gpr[5] = 0x08C8C0C4u;
    if (ctx.gpr[3] == state) tail_call(ctx, 0x0898A0CCu, {});
}

// Bit 19 of the flags at +0x60 set to `on`; unless bit 18 was set, sets it too and calls
// func_08A10820(this, the object at +0x74 if its +0x4 is set, else 0).
void update_flags(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, bool on) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t flags = on ? word(rt, self + 0x60u) | 0x80000u : word(rt, self + 0x60u) & ~0x80000u;
    ctx.gpr[6] = self;
    rt.memory().store32(self + 0x60u, flags);
    ctx.gpr[2] = on ? flags & 0x40000u : flags;
    ctx.gpr[3] = on ? flags | 0x40000u : flags & 0x40000u;
    if ((flags & 0x40000u) != 0u) return;
    if (!on) ctx.gpr[7] = flags | 0x40000u;
    rt.memory().store32(self + 0x60u, flags | 0x40000u);
    std::uint32_t object = word(rt, self + 0x74u);
    if (object != 0u) {
        ctx.gpr[2] = word(rt, object + 0x4u);
        if (ctx.gpr[2] == 0u) object = 0u;
    }
    tail_call(ctx, 0x08A10820u, {self, object});
}

// Stores x into the float at `offset` of the object at +0x80 when func_08966174(this) allows it, and
// marks the flags at +0xE4 (bit 2).
void store_if_allowed(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t offset) {
    const std::uint32_t self = ctx.gpr[4];
    const float x = ctx.fpr[12];
    {
        GuestFrame frame(ctx, 0x20u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08966174u, {self});
    }
    if (ctx.gpr[2] == 0u) return;
    ctx.gpr[3] = word(rt, self + 0x80u);
    rt.memory().store32(ctx.gpr[3] + offset, std::bit_cast<std::uint32_t>(x));
    ctx.gpr[2] = word(rt, self + 0xE4u) | 4u;
    rt.memory().store32(self + 0xE4u, ctx.gpr[2]);
}

} // namespace

// func_088A9524(this, float out[6]): func_0893AD80(the object at +0xC, &out[1..5]) into out[0].
void func_088A9524(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[5];
    const std::uint32_t object = word(rt, ctx.gpr[4] + 0xCu);
    ctx.gpr[2] = object;
    if (object == 0u) return;
    GuestResult result;
    {
        GuestFrame frame(ctx, 0x10u);
        result = call_guest(rt, ctx, 0x0893AD80u, {object, out + 4u, out + 8u, out + 12u, out + 16u, out + 20u}, {});
    }
    rt.memory().store32(out, std::bit_cast<std::uint32_t>(result.f0));
    ctx.gpr[2] = result.v0;
}

// func_08A5D5A0(this, x): base func_08A5B828(this, x), the vtable, func_08A6972C(this + 0xC, x), clears +0x10.
void func_08A5D5A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t x = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08A5B828u, {self, x});
        rt.memory().store32(self, 0x08CD2578u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08A6972Cu, {self + 0xCu, x});
    }
    rt.memory().store32(self + 0x10u, 0u);
}

// Listener broadcasts: func_08AB9CFC (virtual +0x1C), func_08AB9CB0 (+0x18).
void func_08AB9CFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { notify_listeners(rt, ctx, 0x1Cu); }
void func_08AB9CB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { notify_listeners(rt, ctx, 0x18u); }

// func_08B1B198(this) -> the 64-bit value at +0x30, read under the mutex at this.
void func_08B1B198(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    (void)call_guest(rt, ctx, kLock, {self});
    const std::uint32_t low = word(rt, self + 0x30u);
    const std::uint32_t high = word(rt, self + 0x34u);
    (void)call_guest(rt, ctx, kUnlock, {self});
    ctx.gpr[2] = low;
    ctx.gpr[3] = high;
}

// func_08977790(this, x): unless the byte at +0x1B4 is set, func_089AA708(target, x) for target =
// func_08977260(this) when its byte at +0x6E is set.
void func_08977790(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const float x = ctx.fpr[12];
    ctx.gpr[2] = rt.memory().load8(ctx.gpr[4] + 0x1B4u);
    if (ctx.gpr[2] != 0u) return;
    std::uint32_t target;
    {
        GuestFrame frame(ctx, 0x20u);
        target = call_guest(rt, ctx, 0x08977260u, {ctx.gpr[4]});
    }
    ctx.gpr[2] = target;
    ctx.gpr[3] = rt.memory().load8(target + 0x6Eu);
    ctx.fpr[12] = x;
    ctx.gpr[4] = target;
    if (ctx.gpr[3] != 0u) tail_call(ctx, 0x089AA708u, {});
}

// func_0888A24C(this) -> func_08BAA56C(&{the first word, 0}, &{0, the second word}, &{28, 1}, *this) for
// the object *this.
void func_0888A24C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4]);
    GuestFrame frame(ctx, 0x40u);
    rt.memory().store32(frame.at(0x14u), 0u);
    rt.memory().store32(frame.at(0x10u), word(rt, object));
    rt.memory().store32(frame.at(0x4u), 1u);
    rt.memory().store32(frame.at(0xCu), word(rt, object + 0x4u));
    rt.memory().store32(frame.at(0x0u), 28u);
    rt.memory().store32(frame.at(0x8u), 0u);
    ctx.gpr[2] = call_guest(rt, ctx, 0x08BAA56Cu, {frame.at(0x10u), frame.at(0x8u), frame.at(0x0u), object});
}

// func_08AFBF3C(): func_08B54FB8 for 120, 100, (func_08B58A74(0)), 106, 112, 111, 0x8002 and 108.
void func_08AFBF3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08B54FB8u, {120u});
        (void)call_guest(rt, ctx, 0x08B54FB8u, {100u});
        (void)call_guest(rt, ctx, 0x08B58A74u, {0u});
        for (const std::uint32_t value : {106u, 112u, 111u, 0x8002u}) ctx.gpr[2] = call_guest(rt, ctx, 0x08B54FB8u, {value});
    }
    tail_call(ctx, 0x08B54FB8u, {108u});
}

// func_0897756C(this, i): advances counter i (halfwords at +0x156) unless it is 0, wrapping to the
// start at +0x174 past start + span (+0x176); returns whether it was at the start.
void func_0897756C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t slot = (ctx.gpr[5] << 1) + self;
    ctx.gpr[5] = slot;
    const std::uint32_t count = rt.memory().load16(slot + 0x156u);
    const std::uint32_t next = (count + 1u) & 0xFFFFu;
    ctx.gpr[3] = count;
    ctx.gpr[7] = next;
    ctx.gpr[2] = count + 1u;
    ctx.gpr[6] = 0u;
    if (count != 0u) {
        const auto span = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(rt.memory().load16(self + 0x176u))));
        const auto start = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(rt.memory().load16(self + 0x174u))));
        rt.memory().store16(slot + 0x156u, static_cast<std::uint16_t>(next));
        ctx.gpr[4] = start;
        ctx.gpr[3] = count == start ? 1u : 0u;
        ctx.gpr[6] = ctx.gpr[3];
        ctx.gpr[2] = static_cast<std::int32_t>(next) < static_cast<std::int32_t>(start + span) ? 1u : 0u;
        if (ctx.gpr[2] == 0u) rt.memory().store16(slot + 0x156u, static_cast<std::uint16_t>(start));
    }
    ctx.gpr[2] = ctx.gpr[6];
}

// Members forwarded through func_08A7BE18 (after func_08A5D4A0): func_08A18E48, func_08A7B44C,
// func_08A91934 (each with its own null-object message).
void func_08A18E48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_member20(rt, ctx, 0x08CA9C88u); }
void func_08A7B44C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_member20(rt, ctx, 0x08CB12B8u); }
void func_08A91934(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_member20(rt, ctx, 0x08CB1EA8u); }

// Members reset to a fresh value built from the table at kLookupTable: func_08A18FD0 (func_089BAC3C),
// func_08A74974 / func_08A74E6C (func_08A90470), func_08A74418 (func_08A76AAC), func_08A7B5B4
// (func_08A7C0E0), func_08A91984 (func_08A924F0) — all at +0x14 after func_08A5D6D0 — and
// func_08969B48 (func_0897D9C0 at +0xA0, after func_089BA630).
void func_08A18FD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_member(rt, ctx, 0x08A5D6D0u, 0x089BAC3Cu, 0x14u); }
void func_08A74974(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_member(rt, ctx, 0x08A5D6D0u, 0x08A90470u, 0x14u); }
void func_08A74E6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_member(rt, ctx, 0x08A5D6D0u, 0x08A90470u, 0x14u); }
void func_08A74418(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_member(rt, ctx, 0x08A5D6D0u, 0x08A76AACu, 0x14u); }
void func_08A7B5B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_member(rt, ctx, 0x08A5D6D0u, 0x08A7C0E0u, 0x14u); }
void func_08A91984(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_member(rt, ctx, 0x08A5D6D0u, 0x08A924F0u, 0x14u); }
void func_08969B48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_member(rt, ctx, 0x089BA630u, 0x0897D9C0u, 0xA0u); }

// Values assigned to *out built by func_08A85B1C: func_08A86420(this, out) (whether +0x14 is 0),
// func_08A86468 (its negation).
void func_08A86420(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_built(rt, ctx, 0x08A85B1Cu, word(rt, ctx.gpr[4] + 0x14u) == 0u ? 1u : 0u); }
void func_08A86468(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_built(rt, ctx, 0x08A85B1Cu, 0u - word(rt, ctx.gpr[4] + 0x14u)); }

// func_08A00574(this, x): func_08968A40(r) for r = the virtual +0x16C of x, then bit 3 of +0xE4.
void func_08A00574(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t x = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        const std::uint32_t r = call_guest(rt, ctx, word(rt, word(rt, x) + 0x16Cu), {x, x});
        if (r != 0u) (void)call_guest(rt, ctx, 0x08968A40u, {r});
    }
    ctx.gpr[2] = word(rt, self + 0xE4u) | 8u;
    rt.memory().store32(self + 0xE4u, ctx.gpr[2]);
}

// func_0892632C(this, x): recursively on the child at +0x4 first, then the virtual +0x10 (this, x).
void func_0892632C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t x = ctx.gpr[5];
    ctx.gpr[2] = word(rt, self + 0x4u);
    if (ctx.gpr[2] != 0u) {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x0892632Cu, {ctx.gpr[2], x});
    }
    ctx.gpr[2] = word(rt, self);
    tail_call(ctx, word(rt, ctx.gpr[2] + 0x10u), {self, x});
}

// func_088F5B6C(this, x): +0x18 = x, +0x1C = func_08AF6EA8(x, the table at 0x08CA0C3C) (-1 without x).
void func_088F5B6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t x = ctx.gpr[5];
    ctx.gpr[2] = x;
    ctx.gpr[3] = 0xFFFFFFFFu;
    rt.memory().store32(self + 0x1Cu, 0xFFFFFFFFu);
    rt.memory().store32(self + 0x18u, x);
    if (x == 0u) return;
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08AF6EA8u, {x, 0x08CA0C3Cu});
    }
    rt.memory().store32(self + 0x1Cu, ctx.gpr[2]);
}

// Deleting destructors: func_08A7B308 / func_08A84AD8 / func_08A8A6D0 (two members, func_08A5D354, 32
// bytes); func_08A7B728 / func_08A91F40 (as func_08A19134); func_08A650B4 / func_08A67D44 (script
// values over func_08A69D68, 12 bytes).
void func_08A7B308(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { delete_two_members(rt, ctx, 0x08CD2758u); }
void func_08A84AD8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { delete_two_members(rt, ctx, 0x08CD2648u); }
void func_08A8A6D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { delete_two_members(rt, ctx, 0x08CD3438u); }
void func_08A7B728(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { delete_object(rt, ctx, 0x08CD40C8u, 0x14u, 0x08A5D638u, true); }
void func_08A91F40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { delete_object(rt, ctx, 0x08CD43D8u, 0x14u, 0x08A5D638u, true); }
void func_08A650B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { delete_script_value(rt, ctx, 0x08CD2930u, 0x08A69D68u, 12u); }
void func_08A67D44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { delete_script_value(rt, ctx, 0x08CD3010u, 0x08A69D68u, 12u); }

// func_08A80D38(this): deleting destructor (members +0x18 and +0x14, func_08A780B0, 28 bytes).
void func_08A80D38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    rt.memory().store32(self, 0x08CCFA70u);
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, kReleaseLocal, {self + 0x18u});
        (void)call_guest(rt, ctx, kReleaseLocal, {self + 0x14u});
        ctx.gpr[2] = call_guest(rt, ctx, 0x08A780B0u, {self});
    }
    tail_call(ctx, kFreeAligned, {self, 28u, 4u});
}

// func_08A75564(this): deleting destructor (member +0x18 released, +0x14 via func_08A69D68,
// func_08A982D4, 28 bytes).
void func_08A75564(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    rt.memory().store32(self, 0x08CD3AE8u);
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, kReleaseLocal, {self + 0x18u});
        (void)call_guest(rt, ctx, 0x08A69D68u, {self + 0x14u});
        ctx.gpr[2] = call_guest(rt, ctx, 0x08A982D4u, {self});
    }
    tail_call(ctx, kFreeAligned, {self, 28u, 4u});
}

// func_08A1A878(this): deleting destructor (member +0xC, func_08A5BB5C, 36 bytes).
void func_08A1A878(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    rt.memory().store32(self, 0x08CD0678u);
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, kReleaseLocal, {self + 0xCu});
        ctx.gpr[2] = call_guest(rt, ctx, 0x08A5BB5Cu, {self});
    }
    tail_call(ctx, kFreeAligned, {self, 36u, 4u});
}

// func_08ADC230(x): one less at +0x198 of record func_08ADB278(x, 0), inside func_08AEB808 / func_08AEB82C.
void func_08ADC230(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t x = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    (void)call_guest(rt, ctx, 0x08AEB808u, {x});
    const std::uint32_t record = call_guest(rt, ctx, 0x08ADB278u, {x, 0u});
    if (record != 0u) rt.memory().store32(record + 0x198u, word(rt, record + 0x198u) - 1u);
    ctx.gpr[2] = call_guest(rt, ctx, 0x08AEB82Cu, {x});
}

// func_08A99A64(this): after func_08A77FF8(this), once (flag at +0x20): func_08B85798(this + 0x14, the
// pair at +0x14).
void func_08A99A64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08A77FF8u, {self});
    }
    ctx.gpr[2] = rt.memory().load8(self + 0x20u);
    if (ctx.gpr[2] != 0u) return;
    rt.memory().store8(self + 0x20u, 1u);
    ctx.gpr[2] = 1u;
    tail_call(ctx, 0x08B85798u, {self + 0x14u, word(rt, self + 0x14u), word(rt, self + 0x18u)});
}

// Sibling walks: func_08A00A64 (after func_0896A540, virtual +0x12C from +0xF4), func_089B522C
// (virtual +0x19C from +0x84, then clears +0x9C), func_089B518C (after func_08A10A3C, virtual +0x1B0
// from +0x84).
void func_08A00A64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x0896A540u, {ctx.gpr[4]});
    }
    for_each_sibling(rt, ctx, 0xF4u, 0x12Cu);
}
void func_089B522C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    for_each_sibling(rt, ctx, 0x84u, 0x19Cu);
    rt.memory().store32(ctx.gpr[4] + 0x9Cu, 0u);
}
void func_089B518C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08A10A3Cu, {ctx.gpr[4]});
    }
    for_each_sibling(rt, ctx, 0x84u, 0x1B0u);
}

// Words read under the mutex at kGlobalLock4470: func_089948A8 (+0x11C), func_08994810 (+0x118).
void func_089948A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { locked_read(rt, ctx, 0x11Cu); }
void func_08994810(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { locked_read(rt, ctx, 0x118u); }

// func_08AD039C(this) -> the virtual +0x50 (this, &{the table at 0x08CD6BE8, func_08AFAA04(),
// func_08AFAA1C()}).
void func_08AD039C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    rt.memory().store32(frame.at(0u), 0x08CD6BE8u);
    rt.memory().store32(frame.at(4u), call_guest(rt, ctx, 0x08AFAA04u, {self}));
    rt.memory().store32(frame.at(8u), call_guest(rt, ctx, 0x08AFAA1Cu, {self}));
    ctx.gpr[2] = call_guest(rt, ctx, word(rt, word(rt, self) + 0x50u), {self, frame.at(0u)});
}

// func_08A86FA8(this, x): base func_08A76ACC(this, x), the vtable, then func_08A86D00(this + 0x14, &x).
void func_08A86FA8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t x = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    (void)call_guest(rt, ctx, 0x08A76ACCu, {self, x});
    rt.memory().store32(self, 0x08CBEF70u);
    rt.memory().store32(frame.at(0u), x);
    ctx.gpr[2] = call_guest(rt, ctx, 0x08A86D00u, {self + 0x14u, frame.at(0u)});
}

// func_08AF7460(this, rect): func_08C3FEB8(rect, this + 0x94), then func_08B522BC(this, rect[0], rect[2],
// rect[1], rect[3]).
void func_08AF7460(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t rect = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08C3FEB8u, {rect, self + 0x94u});
    }
    tail_call(ctx, 0x08B522BCu, {self, word(rt, rect), word(rt, rect + 0x8u), word(rt, rect + 0x4u), word(rt, rect + 0xCu)});
}

// func_08B1FCD4(a, b) -> whether func_08B1FBAC(a, b) held (then func_08B1FC48(a, b)).
void func_08B1FCD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = ctx.gpr[4];
    const std::uint32_t b = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t found = call_guest(rt, ctx, 0x08B1FBACu, {a, b});
    if (found != 0u) (void)call_guest(rt, ctx, 0x08B1FC48u, {a, b});
    ctx.gpr[2] = found != 0u ? 1u : 0u;
}

// State-gated: func_0898A188 (after func_089BA5D4, state 1), func_0898A1D4 (after func_08969B48, state 2).
void func_0898A188(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { when_state(rt, ctx, 0x089BA5D4u, 1u); }
void func_0898A1D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { when_state(rt, ctx, 0x08969B48u, 2u); }

// func_089FE458(this, x): base func_08A0D658(this, x), vtables at +0x0 / +0x38, func_08A17B10(this + 0x120, x).
void func_089FE458(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t x = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08A0D658u, {self, x});
    }
    rt.memory().store32(self, 0x08CC84E0u);
    rt.memory().store32(self + 0x38u, 0x08CC8738u);
    ctx.gpr[2] = 0x08CC8738u;
    tail_call(ctx, 0x08A17B10u, {self + 0x120u, x});
}

// Flags taken (returned and cleared): func_08A13568 (+0x781), func_08A13518 (+0x780).
void func_08A13568(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { take_flag(rt, ctx, 0x781u); }
void func_08A13518(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { take_flag(rt, ctx, 0x780u); }

// func_089FE2F8(this, x): base func_08A5B828(this, x), the vtable, +0xC = x, bytes +0x10 = 0 / +0x11 = 1.
void func_089FE2F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t x = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08A5B828u, {self, x});
    }
    rt.memory().store8(self + 0x11u, 1u);
    rt.memory().store32(self, 0x08CCFD98u);
    rt.memory().store32(self + 0xCu, x);
    rt.memory().store8(self + 0x10u, 0u);
    ctx.gpr[2] = 0x08CCFD98u;
    ctx.gpr[3] = 1u;
}

// func_08B18A98(this): when active (+0x8), the virtual +0x1C of *this with (+0x0's object, +0x4);
// then clears all three.
void func_08B18A98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    ctx.gpr[2] = rt.memory().load8(self + 0x8u);
    if (ctx.gpr[2] != 0u) {
        const std::uint32_t object = word(rt, self);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, word(rt, word(rt, object) + 0x1Cu), {object, word(rt, self + 0x4u)});
    }
    rt.memory().store8(self + 0x8u, 0u);
    rt.memory().store32(self + 0x4u, 0u);
    rt.memory().store32(self, 0u);
}

// Locked calls: func_08B1F9B4 / func_08B1FA64(this, x) -> func_08B1FBEC / func_08B1FC48(this, x) under
// the mutex at +0xC; func_0897BB18(this, x) -> func_08BEE778(this + 0x18, x) under the one at +0x20.
void func_08B1F9B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { locked_call(rt, ctx, 0xCu, 0x08B1FBECu, 0x0u); }
void func_08B1FA64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { locked_call(rt, ctx, 0xCu, 0x08B1FC48u, 0x0u); }
void func_0897BB18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { locked_call(rt, ctx, 0x20u, 0x08BEE778u, 0x18u); }

// func_08AD5DFC(this): once (+0x10C), func_08AD828C(this), then func_08B1F9B4(kRegistry0598, this + 0x48);
// func_08AD5940(this): the reverse — func_08B1FA64(kRegistry0598, this + 0x48), then func_08AD8294(this).
void func_08AD5DFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    ctx.gpr[2] = rt.memory().load8(self + 0x10Cu);
    if (ctx.gpr[2] != 0u) return;
    rt.memory().store8(self + 0x10Cu, 1u);
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08AD828Cu, {self});
    }
    tail_call(ctx, 0x08B1F9B4u, {kRegistry0598, self + 0x48u});
}
void func_08AD5940(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    ctx.gpr[2] = rt.memory().load8(self + 0x10Cu);
    ctx.gpr[4] = kRegistry0598;
    ctx.gpr[5] = self + 0x48u;
    if (ctx.gpr[2] == 0u) return;
    rt.memory().store8(self + 0x10Cu, 0u);
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08B1FA64u, {kRegistry0598, self + 0x48u});
    }
    tail_call(ctx, 0x08AD8294u, {self});
}

// func_088DE9CC(this) -> func_088D4C9C(this + 4) and the byte at +0x5BC and the word at +0x640.
void func_088DE9CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t found;
    {
        GuestFrame frame(ctx, 0x10u);
        found = call_guest(rt, ctx, 0x088D4C9Cu, {self + 0x4u});
    }
    ctx.gpr[2] = found;
    std::uint32_t result = 0u;
    if (found != 0u) {
        ctx.gpr[2] = rt.memory().load8(self + 0x5BCu);
        if (ctx.gpr[2] != 0u) {
            ctx.gpr[2] = word(rt, self + 0x640u);
            if (ctx.gpr[2] != 0u) result = 1u;
        }
    }
    ctx.gpr[3] = result;
    ctx.gpr[2] = result;
}

// func_08B0C778(this) -> func_08B0C71C(this) (-1 when +0x8 is clear), then func_08B0C358(this).
void func_08B0C778(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    if (word(rt, self + 0x8u) == 0u) {
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t result = call_guest(rt, ctx, 0x08B0C71Cu, {self});
    (void)call_guest(rt, ctx, 0x08B0C358u, {self});
    ctx.gpr[2] = result;
}

// func_08B1065C(_, x): clears x's +0xC; when bit 0 of +0xB0 of x's +0x8 object and x's byte +0x14 are
// set, x's +0xC = func_08B1753C() and its byte +0x15 = 0.
void func_08B1065C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t x = ctx.gpr[5];
    ctx.gpr[3] = word(rt, x + 0x8u);
    rt.memory().store32(x + 0xCu, 0u);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 0xB0u) & 1u;
    if (ctx.gpr[2] == 0u) return;
    ctx.gpr[2] = rt.memory().load8(x + 0x14u);
    if (ctx.gpr[2] == 0u) return;
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08B1753Cu, {});
    }
    rt.memory().store32(x + 0xCu, ctx.gpr[2]);
    rt.memory().store8(x + 0x15u, 0u);
}

// Flag updates: func_08A10A3C (clears bit 19), func_08A109EC (sets it).
void func_08A10A3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { update_flags(rt, ctx, false); }
void func_08A109EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { update_flags(rt, ctx, true); }

// Guarded float setters on the object at +0x80: func_08966204 (+0x20), func_089661B8 (+0x24).
void func_08966204(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_if_allowed(rt, ctx, 0x20u); }
void func_089661B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_if_allowed(rt, ctx, 0x24u); }

// `new` script values: func_08A68AB4 (func_08A68A8C), func_08A67480 (func_08A67454).
void func_08A68AB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_object(rt, ctx, 12u, 0x08A68A8Cu); }
void func_08A67480(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_object(rt, ctx, 12u, 0x08A67454u); }

// func_08AA2A30(p) -> the virtual +0x3C of *p (as func_08AA2898).
void func_08AA2A30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = checked_target(rt, ctx);
    ctx.gpr[3] = word(rt, object);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 0x3Cu);
    tail_call(ctx, ctx.gpr[2], {object});
}

// func_08ADDB18(this) -> this + the offset at +0x8 of record func_08ADB314(this) when its +0xC is
// negative, else 0.
void func_08ADDB18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t record;
    {
        GuestFrame frame(ctx, 0x10u);
        record = call_guest(rt, ctx, 0x08ADB314u, {self});
    }
    ctx.gpr[3] = record;
    std::uint32_t result = 0u;
    if (record != 0u && static_cast<std::int32_t>(word(rt, record + 0xCu)) < 0) result = self + word(rt, record + 0x8u);
    ctx.gpr[2] = result;
}

// func_08AA92D0(this, x) -> func_08C3133C(the object at +0x4, x) (func_08A5BB7C(message) when null).
void func_08AA92D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4] + 0x4u);
    const std::uint32_t x = ctx.gpr[5];
    if (object == 0u) {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08A5BB7Cu, {0x08CB30B0u});
    }
    tail_call(ctx, 0x08C3133Cu, {object, x});
}

// func_08AA1B3C(this, value) -> the virtual +0x10C (this, value as 4 big-endian bytes, 4).
void func_08AA1B3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t value = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    for (std::uint32_t i = 0; i < 4u; ++i)
        rt.memory().store8(frame.at(i), static_cast<std::uint8_t>(static_cast<std::int32_t>(value) >> (24u - 8u * i)));
    ctx.gpr[2] = call_guest(rt, ctx, word(rt, word(rt, self) + 0x10Cu), {self, frame.at(0u), 4u});
}

// func_08AAB8A0(out, x) -> out, set by func_08BEED1C(out, text, its length) to x formatted with the
// format at 0x08CB3190 (func_08B37640 / func_08B35F60).
void func_08AAB8A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t x = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    (void)call_guest(rt, ctx, 0x08B37640u, {frame.at(0u), 0x08CB3190u, x});
    const std::uint32_t length = call_guest(rt, ctx, 0x08B35F60u, {frame.at(0u)});
    (void)call_guest(rt, ctx, 0x08BEED1Cu, {out, frame.at(0u), length});
    ctx.gpr[2] = out;
}

// func_08AD6294(this): once (+0x1D), func_08ABB678(the object at +0x24) between func_08AB9CB0 /
// func_08AB9CFC.
void func_08AD6294(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    ctx.gpr[2] = rt.memory().load8(self + 0x1Du);
    if (ctx.gpr[2] == 0u) return;
    rt.memory().store8(self + 0x1Du, 0u);
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08AB9CB0u, {self});
        ctx.gpr[2] = call_guest(rt, ctx, 0x08ABB678u, {word(rt, self + 0x24u)});
    }
    tail_call(ctx, 0x08AB9CFCu, {word(rt, self + 0x24u)});
}

// func_089A5450(this): func_08A02218, func_089A5314, func_089A4DB8 unless bits 0/2 of +0x138, then
// clears bit 9 there.
void func_089A5450(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08A02218u, {self});
        (void)call_guest(rt, ctx, 0x089A5314u, {self});
        if ((word(rt, self + 0x138u) & 5u) == 0u) (void)call_guest(rt, ctx, 0x089A4DB8u, {self});
    }
    ctx.gpr[3] = word(rt, self + 0x138u) & ~0x200u;
    rt.memory().store32(self + 0x138u, ctx.gpr[3]);
}

// Destructors with two vtables: func_08993ECC (func_08A5B930(the object at +0x144) when set, then
// func_08A0DD40(this)); func_08994FD4 (after func_089946BC, clears +0x120..+0x128).
void func_08993ECC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t object = word(rt, self + 0x144u);
    rt.memory().store32(self, 0x08CC89D0u);
    rt.memory().store32(self + 0x38u, 0x08CC8C28u);
    ctx.gpr[2] = object;
    if (object != 0u) {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08A5B930u, {object});
    }
    tail_call(ctx, 0x08A0DD40u, {self});
}
void func_08994FD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x089946BCu, {self});
    }
    rt.memory().store32(self, 0x08CC9AE8u);
    rt.memory().store32(self + 0x38u, 0x08CC9D40u);
    rt.memory().store32(self + 0x128u, 0u);
    rt.memory().store32(self + 0x120u, 0u);
    rt.memory().store32(self + 0x124u, 0u);
    ctx.gpr[2] = 0x08CC9AE8u;
    ctx.gpr[3] = 0x08CC9D40u;
}

// func_088978B4(this): the virtual +0x0 of the object at +0x4 with (object, +0x0), then clears both.
void func_088978B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t object = word(rt, self + 0x4u);
    if (object == 0u) return;
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, word(rt, word(rt, object)), {object, word(rt, self)});
    }
    rt.memory().store32(self, 0u);
    rt.memory().store32(self + 0x4u, 0u);
}

// func_08A84D50(this): after func_08A77FF8, the virtual +0x4 (a deleting destructor) of the object at
// +0x14, then clears it.
void func_08A84D50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08A77FF8u, {self});
        const std::uint32_t object = word(rt, self + 0x14u);
        ctx.gpr[2] = object;
        if (object != 0u) (void)call_guest(rt, ctx, word(rt, word(rt, object) + 0x4u), {object});
    }
    rt.memory().store32(self + 0x14u, 0u);
}

} // namespace gt::game

GT_DECOMP(0x088A9524u, gt::game::func_088A9524, gt::ReturnKind::Word);
GT_DECOMP(0x08A5D5A0u, gt::game::func_08A5D5A0, gt::ReturnKind::Word);
GT_DECOMP(0x08AB9CFCu, gt::game::func_08AB9CFC, gt::ReturnKind::Word);
GT_DECOMP(0x08AB9CB0u, gt::game::func_08AB9CB0, gt::ReturnKind::Word);
GT_DECOMP(0x08B1B198u, gt::game::func_08B1B198, gt::ReturnKind::DoubleWord);
GT_DECOMP(0x08977790u, gt::game::func_08977790, gt::ReturnKind::Void);
GT_DECOMP(0x0888A24Cu, gt::game::func_0888A24C, gt::ReturnKind::Word);
GT_DECOMP(0x08AFBF3Cu, gt::game::func_08AFBF3C, gt::ReturnKind::Word);
GT_DECOMP(0x0897756Cu, gt::game::func_0897756C, gt::ReturnKind::Word);
GT_DECOMP(0x08A18E48u, gt::game::func_08A18E48, gt::ReturnKind::Word);
GT_DECOMP(0x08A18FD0u, gt::game::func_08A18FD0, gt::ReturnKind::Void);
GT_DECOMP(0x08A00574u, gt::game::func_08A00574, gt::ReturnKind::Word);
GT_DECOMP(0x0892632Cu, gt::game::func_0892632C, gt::ReturnKind::Word);
GT_DECOMP(0x088F5B6Cu, gt::game::func_088F5B6C, gt::ReturnKind::Word);
GT_DECOMP(0x08A7B308u, gt::game::func_08A7B308, gt::ReturnKind::Word);
GT_DECOMP(0x08A84AD8u, gt::game::func_08A84AD8, gt::ReturnKind::Word);
GT_DECOMP(0x08ADC230u, gt::game::func_08ADC230, gt::ReturnKind::Word);
GT_DECOMP(0x08A80D38u, gt::game::func_08A80D38, gt::ReturnKind::Word);
GT_DECOMP(0x08A99A64u, gt::game::func_08A99A64, gt::ReturnKind::Void);
GT_DECOMP(0x08969B48u, gt::game::func_08969B48, gt::ReturnKind::Void);
GT_DECOMP(0x08A00A64u, gt::game::func_08A00A64, gt::ReturnKind::Void);
GT_DECOMP(0x089948A8u, gt::game::func_089948A8, gt::ReturnKind::Word);
GT_DECOMP(0x08994810u, gt::game::func_08994810, gt::ReturnKind::Word);
GT_DECOMP(0x08AD039Cu, gt::game::func_08AD039C, gt::ReturnKind::Word);
GT_DECOMP(0x08A74974u, gt::game::func_08A74974, gt::ReturnKind::Void);
GT_DECOMP(0x08A86FA8u, gt::game::func_08A86FA8, gt::ReturnKind::Word);
GT_DECOMP(0x08AF7460u, gt::game::func_08AF7460, gt::ReturnKind::Word);
GT_DECOMP(0x08B1FCD4u, gt::game::func_08B1FCD4, gt::ReturnKind::Word);
GT_DECOMP(0x0898A188u, gt::game::func_0898A188, gt::ReturnKind::Word);
GT_DECOMP(0x089FE458u, gt::game::func_089FE458, gt::ReturnKind::Word);
GT_DECOMP(0x08A13568u, gt::game::func_08A13568, gt::ReturnKind::Word);
GT_DECOMP(0x08A13518u, gt::game::func_08A13518, gt::ReturnKind::Word);
GT_DECOMP(0x08A75564u, gt::game::func_08A75564, gt::ReturnKind::Word);
GT_DECOMP(0x08A7B44Cu, gt::game::func_08A7B44C, gt::ReturnKind::Word);
GT_DECOMP(0x08A74E6Cu, gt::game::func_08A74E6C, gt::ReturnKind::Void);
GT_DECOMP(0x089FE2F8u, gt::game::func_089FE2F8, gt::ReturnKind::Word);
GT_DECOMP(0x08B18A98u, gt::game::func_08B18A98, gt::ReturnKind::Void);
GT_DECOMP(0x08A8A6D0u, gt::game::func_08A8A6D0, gt::ReturnKind::Word);
GT_DECOMP(0x0898A1D4u, gt::game::func_0898A1D4, gt::ReturnKind::Word);
GT_DECOMP(0x08A91934u, gt::game::func_08A91934, gt::ReturnKind::Word);
GT_DECOMP(0x08A74418u, gt::game::func_08A74418, gt::ReturnKind::Void);
GT_DECOMP(0x08B1F9B4u, gt::game::func_08B1F9B4, gt::ReturnKind::Word);
GT_DECOMP(0x08AD5DFCu, gt::game::func_08AD5DFC, gt::ReturnKind::Void);
GT_DECOMP(0x08B1FA64u, gt::game::func_08B1FA64, gt::ReturnKind::Word);
GT_DECOMP(0x088DE9CCu, gt::game::func_088DE9CC, gt::ReturnKind::Word);
// func_08AD5940 reaches kernel imports the isolated run can't replay (not comparable in all 9 cases),
// so the gate can't check it.
// GT_DECOMP(0x08AD5940u, gt::game::func_08AD5940, gt::ReturnKind::Void);
GT_DECOMP(0x08B0C778u, gt::game::func_08B0C778, gt::ReturnKind::Word);
GT_DECOMP(0x08B1065Cu, gt::game::func_08B1065C, gt::ReturnKind::Void);
GT_DECOMP(0x08A10A3Cu, gt::game::func_08A10A3C, gt::ReturnKind::Void);
GT_DECOMP(0x08966204u, gt::game::func_08966204, gt::ReturnKind::Void);
GT_DECOMP(0x089661B8u, gt::game::func_089661B8, gt::ReturnKind::Void);
GT_DECOMP(0x08A68AB4u, gt::game::func_08A68AB4, gt::ReturnKind::Word);
GT_DECOMP(0x08AA2A30u, gt::game::func_08AA2A30, gt::ReturnKind::Word);
GT_DECOMP(0x08A86420u, gt::game::func_08A86420, gt::ReturnKind::Void);
GT_DECOMP(0x08A650B4u, gt::game::func_08A650B4, gt::ReturnKind::Word);
GT_DECOMP(0x08ADDB18u, gt::game::func_08ADDB18, gt::ReturnKind::Word);
GT_DECOMP(0x08AA92D0u, gt::game::func_08AA92D0, gt::ReturnKind::Word);
GT_DECOMP(0x08AA1B3Cu, gt::game::func_08AA1B3C, gt::ReturnKind::Word);
GT_DECOMP(0x08A86468u, gt::game::func_08A86468, gt::ReturnKind::Void);
GT_DECOMP(0x08A7B728u, gt::game::func_08A7B728, gt::ReturnKind::Word);
GT_DECOMP(0x08A7B5B4u, gt::game::func_08A7B5B4, gt::ReturnKind::Void);
GT_DECOMP(0x08A1A878u, gt::game::func_08A1A878, gt::ReturnKind::Word);
GT_DECOMP(0x08A91F40u, gt::game::func_08A91F40, gt::ReturnKind::Word);
GT_DECOMP(0x08A91984u, gt::game::func_08A91984, gt::ReturnKind::Void);
GT_DECOMP(0x08A67D44u, gt::game::func_08A67D44, gt::ReturnKind::Word);
GT_DECOMP(0x08A109ECu, gt::game::func_08A109EC, gt::ReturnKind::Void);
GT_DECOMP(0x08AAB8A0u, gt::game::func_08AAB8A0, gt::ReturnKind::Word);
GT_DECOMP(0x0897BB18u, gt::game::func_0897BB18, gt::ReturnKind::Word);
GT_DECOMP(0x089B522Cu, gt::game::func_089B522C, gt::ReturnKind::Void);
GT_DECOMP(0x08AD6294u, gt::game::func_08AD6294, gt::ReturnKind::Void);
GT_DECOMP(0x089A5450u, gt::game::func_089A5450, gt::ReturnKind::Void);
GT_DECOMP(0x08993ECCu, gt::game::func_08993ECC, gt::ReturnKind::Word);
GT_DECOMP(0x08A67480u, gt::game::func_08A67480, gt::ReturnKind::Word);
GT_DECOMP(0x088978B4u, gt::game::func_088978B4, gt::ReturnKind::Void);
GT_DECOMP(0x089B518Cu, gt::game::func_089B518C, gt::ReturnKind::Void);
GT_DECOMP(0x08994FD4u, gt::game::func_08994FD4, gt::ReturnKind::Word);
GT_DECOMP(0x08A84D50u, gt::game::func_08A84D50, gt::ReturnKind::Void);

// --- batch 37 ---

namespace gt::game {
namespace {

constexpr std::uint32_t kLock4ACC = 0x08CF4ACCu;  // the mutex func_089AB068 / func_089AA5FC hold
constexpr std::uint32_t kLock49DC = 0x08CF49DCu;  // the mutex func_089A9570 holds

// The object at `offset` when it is live (its +0x4 set), else 0, read under `lock`.
void locked_live(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t lock, std::uint32_t offset) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    (void)call_guest(rt, ctx, kLock, {lock});
    std::uint32_t object = word(rt, self + offset);
    if (object != 0u && word(rt, object + 0x4u) == 0u) object = 0u;
    (void)call_guest(rt, ctx, kUnlock, {lock});
    ctx.gpr[2] = object;
}

// `new` of 16 bytes constructed by `construct(object, a, b)`.
void make_object2(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t construct) {
    const std::uint32_t a = ctx.gpr[4];
    const std::uint32_t b = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t object = call_guest(rt, ctx, kAllocate, {16u});
    (void)call_guest(rt, ctx, construct, {object, a, b});
    ctx.gpr[2] = object;
}

// When func_08966174(this) allows it: sets bit `bit` of the flags at +0x30 of the object at +0x80 to
// the bool argument, and marks +0xE4 (bit 2).
void allowed_bit(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, unsigned bit) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t on = ctx.gpr[5] & 0xFFu;
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08966174u, {self});
    }
    if (ctx.gpr[2] == 0u) return;
    ctx.gpr[2] = word(rt, self + 0x80u);
    ctx.gpr[3] = (word(rt, ctx.gpr[2] + 0x30u) & ~(1u << bit)) | ((on & 1u) << bit);
    rt.memory().store32(ctx.gpr[2] + 0x30u, ctx.gpr[3]);
    ctx.gpr[4] = word(rt, self + 0xE4u) | 4u;
    rt.memory().store32(self + 0xE4u, ctx.gpr[4]);
}

// When func_08966174(this) allows it and it changes: the signed halfword at `offset` of the object at
// +0x80 = value, and marks +0xE4 (bit 2).
void allowed_half(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t offset) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t value = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08966174u, {self});
    }
    if (ctx.gpr[2] == 0u) return;
    ctx.gpr[3] = word(rt, self + 0x80u);
    ctx.gpr[2] = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(rt.memory().load16(ctx.gpr[3] + offset))));
    if (ctx.gpr[2] == value) return;
    rt.memory().store16(ctx.gpr[3] + offset, static_cast<std::uint16_t>(value));
    ctx.gpr[2] = word(rt, self + 0xE4u) | 4u;
    rt.memory().store32(self + 0xE4u, ctx.gpr[2]);
}

// Two optional outputs filled by `first(this)` / `second(this)` (bytes or float words).
void two_outputs(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t first, std::uint32_t second, bool floats) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t out1 = ctx.gpr[5];
    const std::uint32_t out2 = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    for (const auto [out, getter] : {std::pair{out1, first}, std::pair{out2, second}}) {
        if (out == 0u) continue;
        const GuestResult result = call_guest(rt, ctx, getter, {self}, {});
        ctx.gpr[2] = result.v0;
        if (floats) rt.memory().store32(out, std::bit_cast<std::uint32_t>(result.f0));
        else rt.memory().store8(out, static_cast<std::uint8_t>(result.v0));
    }
}

// `push_back(x)` on the word vector at `vector` (begin, end, capacity); `grow(vector, end, &x)` when full.
void push_word(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t vector, std::uint32_t grow) {
    const std::uint32_t vec = ctx.gpr[4] + vector;
    const std::uint32_t x = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    rt.memory().store32(frame.at(0u), x);
    std::uint32_t end = word(rt, vec + 0x4u);
    ctx.gpr[2] = word(rt, vec + 0x8u);
    if (end == ctx.gpr[2]) {
        ctx.gpr[2] = call_guest(rt, ctx, grow, {vec, end, frame.at(0u)});
        return;
    }
    if (end != 0u) {
        rt.memory().store32(end, x);
        end = word(rt, vec + 0x4u);
    }
    rt.memory().store32(vec + 0x4u, end + 4u);
}

// The member at +0xA8 assigned from other's (`assign(this's, other's)` after func_089D8974(this));
// returns other + 0xA8.
void assign_member168(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t assign) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t other = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x089D8974u, {self, other});
    }
    ctx.gpr[2] = other + 0xA8u;
    if (other == 0u || self + 0xA8u == other + 0xA8u) return;
    tail_call(ctx, assign, {self + 0xA8u, other + 0xA8u});
}

} // namespace

// func_08AD8E24(dst, src): merges the halfword fields src marks (bit i of src[1] for field i, 2..12),
// then ORs the masks src[1] and src[0] into dst's.
void func_08AD8E24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t dst = ctx.gpr[4];
    const std::uint32_t src = ctx.gpr[5];
    const auto mask = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(rt.memory().load16(src + 2u))));
    if (mask != 0u) {
        for (std::uint32_t i = 2u; i < 13u; ++i)
            if (((static_cast<std::int32_t>(mask) >> i) & 1) != 0)
                rt.memory().store16(dst + 2u * i, rt.memory().load16(src + 2u * i));
        rt.memory().store16(dst + 2u, static_cast<std::uint16_t>(rt.memory().load16(dst + 2u) | mask));
    }
    const auto low = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(rt.memory().load16(src))));
    rt.memory().store16(dst, static_cast<std::uint16_t>(rt.memory().load16(dst) | low));
}

// func_08A780B0(this): destructor — the vtable, the 576-byte block at +0x10 released (func_08C25BFC,
// then freed), then func_08A5BB5C(this); func_08A7805C(this): the same release, clearing +0x10.
void func_08A780B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t block = word(rt, self + 0x10u);
    rt.memory().store32(self, 0x08CD3C58u);
    ctx.gpr[2] = 0x08CD3C58u;
    if (block != 0u) {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08C25BFCu, {block, word(rt, block + 0x8u)});
        ctx.gpr[2] = call_guest(rt, ctx, kFreeAligned, {block, 576u, 4u});
    }
    tail_call(ctx, 0x08A5BB5Cu, {self});
}
void func_08A7805C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t block = word(rt, self + 0x10u);
    ctx.gpr[2] = self;
    if (block == 0u) return;
    rt.memory().store32(self + 0x10u, 0u);
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08C25BFCu, {block, word(rt, block + 0x8u)});
    }
    tail_call(ctx, kFreeAligned, {block, 576u, 4u});
}

// func_088A97F8(this, float out[2]) -> func_0893A6BC / func_0893A764(the object at +0xC of +0x8, +0x4).
void func_088A97F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t out = ctx.gpr[5];
    ctx.gpr[2] = word(rt, self + 0x8u);
    if (ctx.gpr[2] == 0u) return;
    const std::uint32_t object = word(rt, ctx.gpr[2] + 0xCu);
    GuestFrame frame(ctx, 0x10u);
    GuestResult r = call_guest(rt, ctx, 0x0893A6BCu, {object, word(rt, self + 0x4u)}, {});
    rt.memory().store32(out, std::bit_cast<std::uint32_t>(r.f0));
    r = call_guest(rt, ctx, 0x0893A764u, {object, word(rt, self + 0x4u)}, {});
    rt.memory().store32(out + 0x4u, std::bit_cast<std::uint32_t>(r.f0));
    ctx.gpr[2] = r.v0;
}

// func_08A75A88(vector): destroys the elements (func_08C24F04) and frees the storage.
void func_08A75A88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t vec = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    (void)call_guest(rt, ctx, 0x08C24F04u, {word(rt, vec), word(rt, vec + 0x4u), frame.at(0u)});
    const std::uint32_t data = word(rt, vec);
    ctx.gpr[2] = data;
    if (data == 0u) return;
    const auto bytes = static_cast<std::uint32_t>((static_cast<std::int32_t>(word(rt, vec + 0x8u) - data) >> 2) << 2);
    ctx.gpr[2] = call_guest(rt, ctx, kFreeAligned, {data, bytes, 4u});
}

// func_088DA170(this, i) -> the halfword i of the table at +0x138 when 0 <= i < func_088D9F80(this), else -1.
void func_088DA170(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const auto i = static_cast<std::int32_t>(ctx.gpr[5]);
    ctx.gpr[2] = 0xFFFFFFFFu;
    if (i < 0) return;
    std::uint32_t count;
    {
        GuestFrame frame(ctx, 0x10u);
        count = call_guest(rt, ctx, 0x088D9F80u, {self, ctx.gpr[5]});
    }
    ctx.gpr[3] = ctx.gpr[5] << 1;
    if (i < static_cast<std::int32_t>(count)) ctx.gpr[2] = rt.memory().load16(word(rt, self + 0x138u) + ctx.gpr[3]);
}

// func_08A728A0(this, value*): appends *value at slot +0x1C of the array at +0x0 (keeping the high
// mark at +0x18).
void func_08A728A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t value = ctx.gpr[5];
    std::uint32_t count = word(rt, self + 0x1Cu);
    const std::uint32_t slot = word(rt, self) + (count << 2);
    if (slot != value) {
        {
            GuestFrame frame(ctx, 0x10u);
            (void)call_guest(rt, ctx, kAssignSmart, {slot, word(rt, value)});
        }
        count = word(rt, self + 0x1Cu);
    }
    ctx.gpr[2] = static_cast<std::int32_t>(word(rt, self + 0x18u)) < static_cast<std::int32_t>(count) ? 1u : 0u;
    if (ctx.gpr[2] != 0u) rt.memory().store32(self + 0x18u, count);
    ctx.gpr[3] = count + 1u;
    rt.memory().store32(self + 0x1Cu, count + 1u);
}

// func_08A10C54(this): sets bit 19 of the flags at +0x60; when bit 18 was set too, clears it and calls
// func_08A10A88(this, the object at +0x74 if its +0x4 is set, else 0).
void func_08A10C54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t flags = word(rt, self + 0x60u) | 0x80000u;
    ctx.gpr[6] = self;
    ctx.gpr[2] = flags;
    ctx.gpr[7] = flags & ~0x40000u;
    ctx.gpr[3] = flags & 0x40000u;
    rt.memory().store32(self + 0x60u, flags);
    if (ctx.gpr[3] == 0u) return;
    rt.memory().store32(self + 0x60u, ctx.gpr[7]);
    std::uint32_t object = word(rt, self + 0x74u);
    if (object != 0u) {
        ctx.gpr[2] = word(rt, object + 0x4u);
        if (ctx.gpr[2] == 0u) object = 0u;
    }
    tail_call(ctx, 0x08A10A88u, {self, object});
}

// func_088FC1B4(this) -> record + 0x6C of the virtual +0x30's record when its owner's +0x2C is clear and
// func_08906398(record + 0x34) holds, else 0.
void func_088FC1B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t record = call_guest(rt, ctx, word(rt, word(rt, self) + 0x30u), {self});
    std::uint32_t result = 0u;
    if (record != 0u && word(rt, word(rt, record + 0x4u) + 0x2Cu) == 0u &&
        call_guest(rt, ctx, 0x08906398u, {record + 0x34u}) != 0u)
        result = record + 0x6Cu;
    ctx.gpr[2] = result;
}

// func_088E88C4(this, _, low, high) -> whether the 64-bit stamp (+0x2D4 high, +0x2D0 low) is older than
// (high, low), for an unmarked (+0x2B4), started (+0xC), unpaused (+0x11) object.
void func_088E88C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t mark = rt.memory().load8(self + 0x2B4u);
    ctx.gpr[5] = self;
    std::uint32_t result = mark ^ 1u;
    if (mark == 0u) {
        result = 1u;
        if (word(rt, self + 0xCu) != 0u) {
            if (rt.memory().load8(self + 0x11u) != 0u) result = 0u;
            else {
                const std::uint32_t high = word(rt, self + 0x2D4u);
                ctx.gpr[4] = high;
                if (static_cast<std::int32_t>(high) >= static_cast<std::int32_t>(ctx.gpr[7])) {
                    if (high != ctx.gpr[7]) result = 0u;
                    else if (word(rt, self + 0x2D0u) >= ctx.gpr[6]) result = 0u;
                }
            }
        }
    }
    ctx.gpr[3] = result;
    ctx.gpr[2] = result;
}

// func_089503E0(this): blends the pair at +0x750 of the object at +0x8 towards +0x534 / +0x538 by the
// factors at 0x08CDE09C / 0x08CDE098 (PSP float products).
void func_089503E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4] + 0x8u);
    const float k1 = float_at(rt, 0x08CDE09Cu);
    const float k2 = float_at(rt, 0x08CDE098u);
    const float x = psp_mul(float_at(rt, object + 0x534u), k1) + psp_mul(float_at(rt, object + 0x750u), 1.0f - k1);
    rt.memory().store32(object + 0x750u, std::bit_cast<std::uint32_t>(x));
    const float y = psp_mul(float_at(rt, object + 0x538u), k2) + psp_mul(float_at(rt, object + 0x754u), 1.0f - k2);
    rt.memory().store32(object + 0x754u, std::bit_cast<std::uint32_t>(y));
    ctx.gpr[3] = object;
    ctx.gpr[2] = 0x08CE0000u;
}

// func_08AD1660(this): entry i (80 bytes) at +0x80: func_08B4E648, clears its +0x40 triple, then
// func_08B4E734(this, this + 0x40).
void func_08AD1660(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t entry = self + word(rt, self + 0x1C0u) * 80u + 0x80u;
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08B4E648u, {entry});
    }
    rt.memory().store32(entry + 0x40u, 0u);
    rt.memory().store32(entry + 0x48u, 0u);
    rt.memory().store32(entry + 0x44u, 0u);
    ctx.gpr[2] = entry + 0x40u;
    tail_call(ctx, 0x08B4E734u, {self, self + 0x40u});
}

// func_089AB068(this) -> func_089A3130(func_089AAC8C(this)) (0 without one), under kLock4ACC.
void func_089AB068(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    (void)call_guest(rt, ctx, kLock, {kLock4ACC});
    const std::uint32_t found = call_guest(rt, ctx, 0x089AAC8Cu, {self});
    const std::uint32_t result = found != 0u ? call_guest(rt, ctx, 0x089A3130u, {found}) : 0u;
    (void)call_guest(rt, ctx, kUnlock, {kLock4ACC});
    ctx.gpr[2] = result;
}

// Live objects read under a mutex: func_089AA5FC (+0x48, kLock4ACC), func_08A7C230 (+0x1C, kLockF450).
void func_089AA5FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { locked_live(rt, ctx, kLock4ACC, 0x48u); }
void func_08A7C230(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { locked_live(rt, ctx, kLockF450, 0x1Cu); }

// func_088D9C70(this): func_088E43A8(the object at +0x640, the byte at +0x5B5, 3, 0), or without it
// func_08B702B4(the 64-bit +0x98 * 50, 3, 0).
void func_088D9C70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    ctx.gpr[3] = self;
    const std::uint32_t object = word(rt, self + 0x640u);
    ctx.gpr[2] = object;
    if (object != 0u) {
        tail_call(ctx, 0x088E43A8u, {object, rt.memory().load8(self + 0x5B5u), 3u, 0u});
        return;
    }
    const std::uint64_t product = static_cast<std::uint64_t>(word(rt, self + 0x98u)) * 50u;
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, 0x08B702B4u, {static_cast<std::uint32_t>(product), static_cast<std::uint32_t>(product >> 32), 3u, 0u});
}

// func_088A8E30(this) -> the float at +0x30 (halved when func_08956530(the object at +0xC) holds), 0.0
// when inactive (+0x18) or without the object.
void func_088A8E30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    ctx.fpr[0] = 0.0f;
    if (rt.memory().load8(self + 0x18u) == 0u) return;
    const std::uint32_t object = word(rt, self + 0xCu);
    ctx.gpr[2] = object;
    if (object == 0u) return;
    float value = float_at(rt, self + 0x30u);
    {
        GuestFrame frame(ctx, 0x20u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08956530u, {object});
    }
    if (ctx.gpr[2] != 0u) value = psp_mul(value, 0.5f);
    ctx.fpr[0] = value;
}

// func_089545BC(this): resets the record (handles -1, scale 1.0, everything else cleared).
void func_089545BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    rt.memory().store32(self + 0x30u, 0xFFFFFFFFu);
    rt.memory().store32(self + 0x34u, 0xFFFFFFFFu);
    rt.memory().store32(self + 0x20u, std::bit_cast<std::uint32_t>(1.0f));
    rt.memory().store8(self + 0x2Fu, 0u);
    for (std::uint32_t offset = 0u; offset < 0x20u; offset += 4u) rt.memory().store32(self + offset, 0u);
    rt.memory().store32(self + 0x24u, 0u);
    rt.memory().store16(self + 0x28u, 0u);
    rt.memory().store16(self + 0x2Au, 0u);
    for (std::uint32_t offset = 0x2Cu; offset < 0x2Fu; ++offset) rt.memory().store8(self + offset, 0u);
    ctx.gpr[2] = 0xFFFFFFFFu;
    ctx.gpr[3] = 0xFFFFFFFFu;
    ctx.fpr[0] = 1.0f;
}

// func_088C4F3C(this, x) -> func_088AEFD4(&local, x) on a local (func_088AFAF0 / func_088AFB14) holding
// the object at +0x10.
void func_088C4F3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t x = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    (void)call_guest(rt, ctx, 0x088AFAF0u, {frame.at(0u), x});
    rt.memory().store32(frame.at(4u), word(rt, self + 0x10u));
    const std::uint32_t result = call_guest(rt, ctx, 0x088AEFD4u, {frame.at(0u), x});
    (void)call_guest(rt, ctx, 0x088AFB14u, {frame.at(0u), x});
    ctx.gpr[2] = result;
}

// func_08977E1C(this): +0x1B5 = whether the list at +0x18 is non-empty (under the mutex at +0x20),
// then func_08B28C74(this + 0x48).
void func_08977E1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, kLock, {self + 0x20u});
        rt.memory().store8(self + 0x1B5u, word(rt, self + 0x18u) != self + 0x18u ? 1u : 0u);
        ctx.gpr[2] = call_guest(rt, ctx, kUnlock, {self + 0x20u});
    }
    tail_call(ctx, 0x08B28C74u, {self + 0x48u});
}

// func_089777E0(this, x): as func_08977790, but through the target's virtual +0x168.
void func_089777E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const float x = ctx.fpr[12];
    ctx.gpr[2] = rt.memory().load8(ctx.gpr[4] + 0x1B4u);
    if (ctx.gpr[2] != 0u) return;
    std::uint32_t target;
    {
        GuestFrame frame(ctx, 0x20u);
        target = call_guest(rt, ctx, 0x08977260u, {ctx.gpr[4]});
    }
    ctx.gpr[3] = target;
    ctx.gpr[2] = rt.memory().load8(target + 0x6Eu);
    ctx.fpr[12] = x;
    ctx.gpr[4] = target;
    if (ctx.gpr[2] == 0u) return;
    ctx.gpr[2] = word(rt, target);
    tail_call(ctx, word(rt, ctx.gpr[2] + 0x168u), {});
}

// func_08866DA4(this, x) -> func_08866CD0(this, x), with x = 0.0 when the owner's byte +0x5B6 is set or
// func_08866960(this) holds.
void func_08866DA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    float x = ctx.fpr[12];
    ctx.gpr[3] = word(rt, self + 0x5ACu);
    ctx.gpr[2] = rt.memory().load8(ctx.gpr[3] + 0x5B6u);
    if (ctx.gpr[2] != 0u) x = 0.0f;
    else {
        GuestFrame frame(ctx, 0x20u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08866960u, {self});
        if (ctx.gpr[2] != 0u) x = 0.0f;
    }
    ctx.fpr[12] = x;
    tail_call(ctx, 0x08866CD0u, {self});
}

// func_089A9570(this): empties the list at +0x178 (func_08BF8B64) under kLock49DC.
void func_089A9570(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t head = ctx.gpr[4] + 0x178u;
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, kLock, {kLock49DC});
        ctx.gpr[2] = word(rt, head);
        if (ctx.gpr[2] != head) {
            ctx.gpr[2] = call_guest(rt, ctx, 0x08BF8B64u, {head});
            rt.memory().store32(head, head);
            rt.memory().store32(head + 0x4u, head);
        }
    }
    tail_call(ctx, kUnlock, {kLock49DC});
}

// func_08A7D4E4(this): destructor — vtable, func_08C00860(this + 0x20, +0x28), func_08A5B930(+0x1C),
// func_08A69D68(this + 0x18), then func_08A780B0(this).
void func_08A7D4E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    rt.memory().store32(self, 0x08CB6DA8u);
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08C00860u, {self + 0x20u, word(rt, self + 0x28u)});
        if (const std::uint32_t object = word(rt, self + 0x1Cu); object != 0u) (void)call_guest(rt, ctx, 0x08A5B930u, {object});
        ctx.gpr[2] = call_guest(rt, ctx, 0x08A69D68u, {self + 0x18u});
    }
    tail_call(ctx, 0x08A780B0u, {self});
}

// func_088E7284(this) -> func_088DF324(owner, +0x4, func_08917888(owner's +0x5C8)) for owner +0x8; 0 without.
void func_088E7284(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t owner = word(rt, self + 0x8u);
    ctx.gpr[2] = 0u;
    if (owner == 0u) return;
    const std::uint32_t second = word(rt, self + 0x4u);
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08917888u, {word(rt, owner + 0x5C8u)});
    }
    tail_call(ctx, 0x088DF324u, {owner, second, ctx.gpr[2]});
}

// func_088DA880(this) -> 1 + func_088FAE2C(func_088FE518(+0x14), 0) when func_088FE518(+0x14) holds, else 0.
void func_088DA880(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t object = word(rt, self + 0x14u);
    ctx.gpr[2] = 0u;
    if (object == 0u) return;
    GuestFrame frame(ctx, 0x10u);
    if (call_guest(rt, ctx, 0x088FE518u, {object}) == 0u) return;
    const std::uint32_t found = call_guest(rt, ctx, 0x088FE518u, {word(rt, self + 0x14u)});
    ctx.gpr[2] = call_guest(rt, ctx, 0x088FAE2Cu, {found, 0u}) + 1u;
}

// func_08926488(this, float src[6], notify): copies the six floats (the fourth scaled by the factor at
// 0x08CDDD68), then the virtual +0x10 when notify.
void func_08926488(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t src = ctx.gpr[5];
    for (std::uint32_t i = 0; i < 3u; ++i) rt.memory().store32(self + 0x14u + 4u * i, word(rt, src + 4u * i));
    ctx.gpr[6] &= 0xFFu;
    ctx.fpr[12] = 0.0f;
    rt.memory().store32(self + 0x24u, word(rt, src + 0x10u));
    const float scaled = psp_mul(float_at(rt, src + 0xCu), float_at(rt, 0x08CDDD68u));
    rt.memory().store32(self + 0x20u, std::bit_cast<std::uint32_t>(scaled));
    rt.memory().store32(self + 0x28u, word(rt, src + 0x14u));
    ctx.gpr[2] = 0x08CE0000u;
    if (ctx.gpr[6] == 0u) return;
    ctx.gpr[2] = word(rt, self);
    tail_call(ctx, word(rt, ctx.gpr[2] + 0x10u), {});
}

// func_08B0151C(this): the virtual +0xC (object, this) of every object in the array +0xA8..+0xAC.
void func_08B0151C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t end = word(rt, self + 0xACu);
    GuestFrame frame(ctx, 0x10u);
    for (std::uint32_t at = word(rt, self + 0xA8u); at != end; at += 4u) {
        const std::uint32_t object = word(rt, at);
        ctx.gpr[2] = call_guest(rt, ctx, word(rt, word(rt, object) + 0xCu), {object, self});
    }
}

// func_0892BB1C(this, x): the virtual +0x10 of the object behind +0x260 (object, the halfword at +0x2 of
// +0x60, x, &its +0x5C), then +0x6C = that +0x5C.
void func_0892BB1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t holder = word(rt, self + 0x260u);
    const std::uint32_t object = word(rt, holder);
    ctx.gpr[2] = object;
    if (object == 0u) return;
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, word(rt, word(rt, object) + 0x10u),
                         {object, rt.memory().load16(word(rt, self + 0x60u) + 0x2u), ctx.gpr[5], holder + 0x5Cu});
    }
    ctx.gpr[2] = word(rt, self + 0x260u) + 0x5Cu;
    rt.memory().store32(self + 0x6Cu, ctx.gpr[2]);
}

// func_088AF9F4(this): func_088AF3C8(this, i) for i = 0, 1, ... while it holds, when +0x8, +0xC and
// +0x14 are all set.
void func_088AF9F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    for (const std::uint32_t offset : {0x8u, 0xCu, 0x14u}) {
        ctx.gpr[2] = word(rt, self + offset);
        if (ctx.gpr[2] == 0u) return;
    }
    GuestFrame frame(ctx, 0x10u);
    for (std::uint32_t i = 0u;; ++i) {
        ctx.gpr[2] = call_guest(rt, ctx, 0x088AF3C8u, {self, i});
        if (ctx.gpr[2] == 0u) break;
    }
}

// Word vectors: func_08AF2654(this, x) (at +0x4, grown by func_08C3F648), func_089AD6FC (at +0x12C,
// func_08BFAA94).
void func_08AF2654(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { push_word(rt, ctx, 0x4u, 0x08C3F648u); }
void func_089AD6FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { push_word(rt, ctx, 0x12Cu, 0x08BFAA94u); }

// func_08809BB0(this, x): func_08969B98(this, the record +0x118's fields +0x8..+0x14) when its +0x0 is
// set, then func_089AD6FC(x, this).
void func_08809BB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t x = ctx.gpr[5];
    const std::uint32_t record = word(rt, self + 0x118u);
    if (word(rt, record) != 0u) {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08969B98u, {self, record + 0x8u, record + 0xCu, record + 0x10u, record + 0x14u});
    }
    tail_call(ctx, 0x089AD6FCu, {x, self});
}

// func_088DB56C(this, a, b) -> func_088FAB84(found, a, b) for found = func_088FE518(+0x14), 0 without.
void func_088DB56C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = ctx.gpr[5];
    const std::uint32_t b = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t found = call_guest(rt, ctx, 0x088FE518u, {word(rt, ctx.gpr[4] + 0x14u)});
    ctx.gpr[2] = found != 0u ? call_guest(rt, ctx, 0x088FAB84u, {found, a, b}) : 0u;
}

// func_08A0D658(this): base func_08A02B98, vtables at +0x0 / +0x38, bit 0 of +0x110, the halfwords
// +0x118..+0x11E cleared, then func_0896A824(this, 1).
void func_08A0D658(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08A02B98u, {self});
    }
    const std::uint32_t flags = word(rt, self + 0x110u) | 1u;
    rt.memory().store32(self, 0x08CBA830u);
    rt.memory().store32(self + 0x38u, 0x08CBAA88u);
    rt.memory().store32(self + 0x110u, flags);
    for (std::uint32_t offset = 0x118u; offset < 0x120u; offset += 2u) rt.memory().store16(self + offset, 0u);
    ctx.gpr[2] = 0x08CBAA88u;
    ctx.gpr[3] = flags;
    tail_call(ctx, 0x0896A824u, {self, 1u});
}

// func_08A0D0CC(this, other): func_08A013D8(this, other), then other's halfwords +0x118..+0x11E.
void func_08A0D0CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t other = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08A013D8u, {self, other});
    }
    if (other == 0u) return;
    for (std::uint32_t offset = 0x118u; offset < 0x120u; offset += 2u) rt.memory().store16(self + offset, rt.memory().load16(other + offset));
}

// func_08AB983C() -> the first non-zero virtual +0x28 among the listeners at kListeners, else 0.
void func_08AB983C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = 0u;
    GuestFrame frame(ctx, 0x10u);
    for (std::uint32_t node = word(rt, kListeners); node != 0u; node = word(rt, node)) {
        const std::uint32_t listener = word(rt, node + 0x4u);
        ctx.gpr[2] = call_guest(rt, ctx, word(rt, word(rt, listener) + 0x28u), {listener});
        if (ctx.gpr[2] != 0u) return;
    }
}

// func_08A90BF4(this, x): the smart pointer at +0x14 = x (func_08A5BA08 retains, func_08A5B930 releases).
void func_08A90BF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t x = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        if (x != 0u) (void)call_guest(rt, ctx, 0x08A5BA08u, {x});
        ctx.gpr[2] = word(rt, self + 0x14u);
        if (ctx.gpr[2] != 0u) (void)call_guest(rt, ctx, 0x08A5B930u, {ctx.gpr[2]});
    }
    rt.memory().store32(self + 0x14u, x);
}

// `new` 16-byte objects (a, b): func_08A67DE4 (func_08A67DB8), func_08A647A0 (func_08A64754),
// func_08A64A4C (func_08A64A00).
void func_08A67DE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_object2(rt, ctx, 0x08A67DB8u); }
void func_08A647A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_object2(rt, ctx, 0x08A64754u); }
void func_08A64A4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_object2(rt, ctx, 0x08A64A00u); }

// Guarded setters on the object at +0x80: bits 6 / 7 / 10 of +0x30 (func_089665BC / func_08966480 /
// func_089662A4), the halfwords +0x2A / +0x28 (func_08966A70 / func_08966AC8).
void func_089665BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { allowed_bit(rt, ctx, 6u); }
void func_08966480(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { allowed_bit(rt, ctx, 7u); }
void func_089662A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { allowed_bit(rt, ctx, 10u); }
void func_08966A70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { allowed_half(rt, ctx, 0x2Au); }
void func_08966AC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { allowed_half(rt, ctx, 0x28u); }

// Two optional outputs: func_0896726C (bytes from func_089670DC / func_08967124), func_08967380 (floats
// from func_089671FC / func_08967234), func_089672C8 (bytes from func_08966434 / func_08966370).
void func_0896726C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { two_outputs(rt, ctx, 0x089670DCu, 0x08967124u, false); }
void func_08967380(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { two_outputs(rt, ctx, 0x089671FCu, 0x08967234u, true); }
void func_089672C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { two_outputs(rt, ctx, 0x08966434u, 0x08966370u, false); }

// func_089FF16C(this): base func_08A0FDD8, vtables at +0x0 / +0x38, func_089B883C(this + 0x78, this),
// +0x94 = 1, the byte at +0x98 cleared.
void func_089FF16C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08A0FDD8u, {self});
        rt.memory().store32(self, 0x08CC73A8u);
        rt.memory().store32(self + 0x38u, 0x08CC7588u);
        (void)call_guest(rt, ctx, 0x089B883Cu, {self + 0x78u, self});
    }
    rt.memory().store32(self + 0x94u, 1u);
    rt.memory().store8(self + 0x98u, 0u);
    ctx.gpr[2] = 1u;
}

// func_08A88D50(this): when bit 1 of +0x18 is set: func_08B0C508(this + 0x20, &{+0x138, +0x140}) when
// +0x140 is set, then clears +0x13C / +0x140.
void func_08A88D50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    ctx.gpr[2] = word(rt, self + 0x18u) & 2u;
    if (ctx.gpr[2] == 0u) return;
    if (const std::uint32_t handle = word(rt, self + 0x140u); handle != 0u) {
        GuestFrame frame(ctx, 0x20u);
        rt.memory().store32(frame.at(4u), handle);
        rt.memory().store32(frame.at(0u), word(rt, self + 0x138u));
        ctx.gpr[2] = call_guest(rt, ctx, 0x08B0C508u, {self + 0x20u, frame.at(0u)});
    }
    rt.memory().store32(self + 0x13Cu, 0u);
    rt.memory().store32(self + 0x140u, 0u);
}

// Members assigned from another object's (+0xA8): func_0897557C (func_08BECF40), func_089A1934
// (func_08BF4CF0), func_089F951C (func_08C0CECC).
void func_0897557C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { assign_member168(rt, ctx, 0x08BECF40u); }
void func_089A1934(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { assign_member168(rt, ctx, 0x08BF4CF0u); }
void func_089F951C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { assign_member168(rt, ctx, 0x08C0CECCu); }

// func_08993B3C(this, x): func_08A00574(this, x), then func_089939D4(this, r) for r = x's virtual +0x16C.
void func_08993B3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t x = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08A00574u, {self, x});
        ctx.gpr[2] = call_guest(rt, ctx, word(rt, word(rt, x) + 0x16Cu), {x});
    }
    if (ctx.gpr[2] != 0u) tail_call(ctx, 0x089939D4u, {self, ctx.gpr[2]});
}

// func_08A8A4E0(this, x) -> the virtual +0x74 of the object at +0x18 (object, this + 0x1C, x).
void func_08A8A4E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t x = ctx.gpr[5];
    const std::uint32_t object = word(rt, self + 0x18u);
    if (object == 0u) {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08A5BB7Cu, {0x08CB1B8Cu});
    }
    ctx.gpr[2] = word(rt, object);
    tail_call(ctx, word(rt, ctx.gpr[2] + 0x74u), {object, self + 0x1Cu, x});
}

// func_08B19590(a, b, c, d) -> a, after func_08B19538(a, b, func_08B1935C(b, c), d).
void func_08B19590(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = ctx.gpr[4], b = ctx.gpr[5], c = ctx.gpr[6], d = ctx.gpr[7];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t found = call_guest(rt, ctx, 0x08B1935Cu, {b, c});
    (void)call_guest(rt, ctx, 0x08B19538u, {a, b, found, d});
    ctx.gpr[2] = a;
}

// func_08AD6600(_, out, _, key) -> whether func_08ABC430(key, &low, &high) found it; out = {low,
// high * 2 + 1}.
void func_08AD6600(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[5];
    rt.memory().store32(out, 0u);
    GuestFrame frame(ctx, 0x20u);
    std::uint32_t result = 0u;
    if (call_guest(rt, ctx, 0x08ABC430u, {ctx.gpr[7], frame.at(0u), frame.at(4u)}) != 0u) {
        rt.memory().store32(out + 0x4u, (word(rt, frame.at(4u)) << 1) | 1u);
        rt.memory().store32(out, word(rt, frame.at(0u)));
        result = 1u;
    }
    ctx.gpr[2] = result;
}

// func_089BB3E8(this, x) -> this, after func_089BB3A8(this, &a value func_08A77444 built) and
// func_089BA9E4(*this, x).
void func_089BB3E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t x = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    (void)call_guest(rt, ctx, 0x08A77444u, {frame.at(0u), x});
    (void)call_guest(rt, ctx, 0x089BB3A8u, {self, frame.at(0u)});
    (void)call_guest(rt, ctx, kReleaseLocal, {frame.at(0u)});
    (void)call_guest(rt, ctx, 0x089BA9E4u, {word(rt, self), x});
    ctx.gpr[2] = self;
}

// func_08A865A0(this, out): out = a value func_08A85B1C builds from the counter at +0x14, post-incremented.
void func_08A865A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t count = word(rt, ctx.gpr[4] + 0x14u);
    rt.memory().store32(ctx.gpr[4] + 0x14u, count + 1u);
    store_built(rt, ctx, 0x08A85B1Cu, count);
}

// func_08A79D80(x, out): out = a value func_08A76AAC builds from &x; func_08A531C0(this, _, x) -> this
// = a value func_08A577FC builds from &x.
void func_08A79D80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t x = ctx.gpr[4];
    const std::uint32_t out = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    rt.memory().store32(frame.at(0u), x);
    (void)call_guest(rt, ctx, 0x08A76AACu, {frame.at(4u), frame.at(0u)});
    if (out != frame.at(4u)) (void)call_guest(rt, ctx, kAssignSmart, {out, word(rt, frame.at(4u))});
    ctx.gpr[2] = call_guest(rt, ctx, kReleaseLocal, {frame.at(4u)});
}
void func_08A531C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    rt.memory().store32(frame.at(0u), ctx.gpr[6]);
    (void)call_guest(rt, ctx, 0x08A577FCu, {frame.at(4u), frame.at(0u)});
    (void)call_guest(rt, ctx, kAssignValue, {self, frame.at(4u)});
    (void)call_guest(rt, ctx, kReleaseLocal, {frame.at(4u)});
    ctx.gpr[2] = self;
}

// func_08B2B9BC(this, key) -> the index of key in the sorted 8-byte entries (+0x44, count +0x48), or
// ~(insertion point).
void func_08B2B9BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t key = ctx.gpr[5];
    std::uint32_t high = word(rt, ctx.gpr[4] + 0x48u);
    std::uint32_t v0 = 0u;
    std::uint32_t mid = 0u;
    if (high != 0u) {
        const std::uint32_t entries = word(rt, ctx.gpr[4] + 0x44u);
        std::uint32_t low = 0u;
        for (;;) {
            mid = static_cast<std::uint32_t>(static_cast<std::int32_t>(high + low) >> 1);
            const std::uint32_t at = word(rt, entries + (mid << 3));
            if (at == key) {
                ctx.gpr[2] = mid;
                ctx.gpr[3] = mid;
                return;
            }
            v0 = at < key ? 1u : 0u;
            if (mid == low) break;
            if (v0 != 0u) {
                low = mid;
                if (mid == high) break;
            } else {
                high = mid;
            }
        }
        v0 += mid;
    }
    ctx.gpr[3] = ~v0;
    ctx.gpr[2] = ~v0;
}

// func_08A75410(this, b, p) -> func_08A79DD4(*p, b, this + 0x14, this + 0x18) (func_08A5BB7C(message)
// when *p is null).
void func_08A75410(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t b = ctx.gpr[5];
    const std::uint32_t object = word(rt, ctx.gpr[6]);
    if (object == 0u) {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08A5BB7Cu, {0x08CB0D54u});
    }
    tail_call(ctx, 0x08A79DD4u, {object, b, self + 0x14u, self + 0x18u});
}

// func_0896B9CC(this, x): +0x1BC = x * the first value func_0896A35C(this, 0, 0, &out, 0) writes, and
// bit 20 of +0xE0.
void func_0896B9CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const float x = ctx.fpr[12];
    GuestFrame frame(ctx, 0x30u);
    (void)call_guest(rt, ctx, 0x0896A35Cu, {self, 0u, 0u, frame.at(0u), 0u});
    ctx.fpr[0] = psp_mul(float_at(rt, frame.at(0u)), x);
    ctx.gpr[3] = word(rt, self + 0xE0u) | 0x100000u;
    rt.memory().store32(self + 0xE0u, ctx.gpr[3]);
    rt.memory().store32(self + 0xBCu, std::bit_cast<std::uint32_t>(ctx.fpr[0]));
    ctx.gpr[2] = 0x100000u;
}

// func_08A811E0(this): deleting destructor (members +0x30 / +0x2C released, func_08C28648(this + 0x14,
// +0x1C), func_08A780B0, 52 bytes).
void func_08A811E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    rt.memory().store32(self, 0x08CB8798u);
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, kReleaseLocal, {self + 0x30u});
        (void)call_guest(rt, ctx, kReleaseLocal, {self + 0x2Cu});
        (void)call_guest(rt, ctx, 0x08C28648u, {self + 0x14u, word(rt, self + 0x1Cu)});
        ctx.gpr[2] = call_guest(rt, ctx, 0x08A780B0u, {self});
    }
    tail_call(ctx, kFreeAligned, {self, 52u, 4u});
}

} // namespace gt::game

GT_DECOMP(0x08AD8E24u, gt::game::func_08AD8E24, gt::ReturnKind::Void);
GT_DECOMP(0x08A780B0u, gt::game::func_08A780B0, gt::ReturnKind::Word);
GT_DECOMP(0x08A7805Cu, gt::game::func_08A7805C, gt::ReturnKind::Void);
GT_DECOMP(0x088A97F8u, gt::game::func_088A97F8, gt::ReturnKind::Void);
GT_DECOMP(0x08A75A88u, gt::game::func_08A75A88, gt::ReturnKind::Void);
GT_DECOMP(0x088DA170u, gt::game::func_088DA170, gt::ReturnKind::Word);
GT_DECOMP(0x08A728A0u, gt::game::func_08A728A0, gt::ReturnKind::Void);
GT_DECOMP(0x08A10C54u, gt::game::func_08A10C54, gt::ReturnKind::Void);
GT_DECOMP(0x088FC1B4u, gt::game::func_088FC1B4, gt::ReturnKind::Word);
GT_DECOMP(0x088E88C4u, gt::game::func_088E88C4, gt::ReturnKind::Word);
GT_DECOMP(0x089503E0u, gt::game::func_089503E0, gt::ReturnKind::Void);
GT_DECOMP(0x08AD1660u, gt::game::func_08AD1660, gt::ReturnKind::Word);
GT_DECOMP(0x089AB068u, gt::game::func_089AB068, gt::ReturnKind::Word);
GT_DECOMP(0x089AA5FCu, gt::game::func_089AA5FC, gt::ReturnKind::Word);
GT_DECOMP(0x08A7C230u, gt::game::func_08A7C230, gt::ReturnKind::Word);
GT_DECOMP(0x088D9C70u, gt::game::func_088D9C70, gt::ReturnKind::Word);
GT_DECOMP(0x088A8E30u, gt::game::func_088A8E30, gt::ReturnKind::Float);
GT_DECOMP(0x089545BCu, gt::game::func_089545BC, gt::ReturnKind::Word);
GT_DECOMP(0x088C4F3Cu, gt::game::func_088C4F3C, gt::ReturnKind::Word);
GT_DECOMP(0x08977E1Cu, gt::game::func_08977E1C, gt::ReturnKind::Word);
GT_DECOMP(0x089777E0u, gt::game::func_089777E0, gt::ReturnKind::Void);
GT_DECOMP(0x08866DA4u, gt::game::func_08866DA4, gt::ReturnKind::Word);
GT_DECOMP(0x089A9570u, gt::game::func_089A9570, gt::ReturnKind::Word);
GT_DECOMP(0x08A7D4E4u, gt::game::func_08A7D4E4, gt::ReturnKind::Word);
GT_DECOMP(0x088E7284u, gt::game::func_088E7284, gt::ReturnKind::Word);
GT_DECOMP(0x088DA880u, gt::game::func_088DA880, gt::ReturnKind::Word);
GT_DECOMP(0x08926488u, gt::game::func_08926488, gt::ReturnKind::Void);
GT_DECOMP(0x08B0151Cu, gt::game::func_08B0151C, gt::ReturnKind::Void);
GT_DECOMP(0x0892BB1Cu, gt::game::func_0892BB1C, gt::ReturnKind::Void);
GT_DECOMP(0x088AF9F4u, gt::game::func_088AF9F4, gt::ReturnKind::Word);
GT_DECOMP(0x08AF2654u, gt::game::func_08AF2654, gt::ReturnKind::Void);
GT_DECOMP(0x089AD6FCu, gt::game::func_089AD6FC, gt::ReturnKind::Void);
GT_DECOMP(0x08809BB0u, gt::game::func_08809BB0, gt::ReturnKind::Word);
GT_DECOMP(0x088DB56Cu, gt::game::func_088DB56C, gt::ReturnKind::Word);
GT_DECOMP(0x08A0D658u, gt::game::func_08A0D658, gt::ReturnKind::Word);
GT_DECOMP(0x08A0D0CCu, gt::game::func_08A0D0CC, gt::ReturnKind::Void);
GT_DECOMP(0x08AB983Cu, gt::game::func_08AB983C, gt::ReturnKind::Word);
GT_DECOMP(0x08A90BF4u, gt::game::func_08A90BF4, gt::ReturnKind::Void);
GT_DECOMP(0x08A67DE4u, gt::game::func_08A67DE4, gt::ReturnKind::Word);
GT_DECOMP(0x08A647A0u, gt::game::func_08A647A0, gt::ReturnKind::Word);
GT_DECOMP(0x08A64A4Cu, gt::game::func_08A64A4C, gt::ReturnKind::Word);
GT_DECOMP(0x089665BCu, gt::game::func_089665BC, gt::ReturnKind::Void);
GT_DECOMP(0x08966480u, gt::game::func_08966480, gt::ReturnKind::Void);
GT_DECOMP(0x089662A4u, gt::game::func_089662A4, gt::ReturnKind::Void);
GT_DECOMP(0x08966A70u, gt::game::func_08966A70, gt::ReturnKind::Void);
GT_DECOMP(0x08966AC8u, gt::game::func_08966AC8, gt::ReturnKind::Void);
GT_DECOMP(0x0896726Cu, gt::game::func_0896726C, gt::ReturnKind::Void);
GT_DECOMP(0x08967380u, gt::game::func_08967380, gt::ReturnKind::Void);
GT_DECOMP(0x089672C8u, gt::game::func_089672C8, gt::ReturnKind::Void);
GT_DECOMP(0x089FF16Cu, gt::game::func_089FF16C, gt::ReturnKind::Word);
GT_DECOMP(0x08A88D50u, gt::game::func_08A88D50, gt::ReturnKind::Void);
GT_DECOMP(0x0897557Cu, gt::game::func_0897557C, gt::ReturnKind::Word);
GT_DECOMP(0x089A1934u, gt::game::func_089A1934, gt::ReturnKind::Word);
GT_DECOMP(0x089F951Cu, gt::game::func_089F951C, gt::ReturnKind::Word);
GT_DECOMP(0x08993B3Cu, gt::game::func_08993B3C, gt::ReturnKind::Word);
GT_DECOMP(0x08A8A4E0u, gt::game::func_08A8A4E0, gt::ReturnKind::Word);
GT_DECOMP(0x08B19590u, gt::game::func_08B19590, gt::ReturnKind::Word);
GT_DECOMP(0x08AD6600u, gt::game::func_08AD6600, gt::ReturnKind::Word);
GT_DECOMP(0x089BB3E8u, gt::game::func_089BB3E8, gt::ReturnKind::Word);
GT_DECOMP(0x08A865A0u, gt::game::func_08A865A0, gt::ReturnKind::Void);
GT_DECOMP(0x08A79D80u, gt::game::func_08A79D80, gt::ReturnKind::Void);
GT_DECOMP(0x08A531C0u, gt::game::func_08A531C0, gt::ReturnKind::Word);
GT_DECOMP(0x08B2B9BCu, gt::game::func_08B2B9BC, gt::ReturnKind::Word);
GT_DECOMP(0x08A75410u, gt::game::func_08A75410, gt::ReturnKind::Word);
GT_DECOMP(0x0896B9CCu, gt::game::func_0896B9CC, gt::ReturnKind::Void);
GT_DECOMP(0x08A811E0u, gt::game::func_08A811E0, gt::ReturnKind::Word);

// --- batch 38 ---

namespace gt::game {
namespace {

constexpr std::uint32_t kLock4AA4 = 0x08CF4AA4u;      // the mutex func_089AA6B0 holds
constexpr std::uint32_t kLock61B0 = 0x08CF61B0u;      // the mutex func_08A10718 holds
constexpr std::uint32_t kListeners7188 = 0x08D77188u; // the second listener list (func_08ABB924)
constexpr std::uint32_t kAllocateAligned4 = 0x08A5BC20u;  // (size, alignment) -> memory

// The 576-byte block at +0x10 released (func_08C25BFC, then freed) and forgotten, as func_08A7805C.
void release_block576(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t self) {
    const std::uint32_t block = word(rt, self + 0x10u);
    if (block == 0u) return;
    rt.memory().store32(self + 0x10u, 0u);
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08C25BFCu, {block, word(rt, block + 0x8u)});
    }
    tail_call(ctx, kFreeAligned, {block, 576u, 4u});
}

// `wrap(this, &object)` for a new `size`-byte object (aligned to 4) built by `construct(object, x)`.
void wrap_new(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t size, std::uint32_t construct,
              std::uint32_t wrap, bool signed_byte = false) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t x = signed_byte ? static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(ctx.gpr[5]))) : ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t object = call_guest(rt, ctx, kAllocateAligned4, {size, 4u});
    (void)call_guest(rt, ctx, construct, {object, x});
    rt.memory().store32(frame.at(0u), object);
    ctx.gpr[2] = call_guest(rt, ctx, wrap, {self, frame.at(0u)});
}

// `new` of `size` bytes built by `construct(object, args...)`.
void make_object_n(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t size, std::uint32_t construct,
                   std::initializer_list<std::uint32_t> args) {
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t object = call_guest(rt, ctx, kAllocate, {size});
    std::uint32_t regs[4]{object, 0u, 0u, 0u};
    unsigned n = 1u;
    for (const std::uint32_t value : args) regs[n++] = value;
    (void)call_guest(rt, ctx, construct, {regs[0], regs[1], regs[2], regs[3]});
    ctx.gpr[2] = object;
}

// When func_08966174(this) allows it: a field (`mask` at `shift`) of the flags at +0x30 of the object at
// +0x80 set to `value`, marking +0xE4 (bit 2) — only when it changes, for `if_changed`.
void allowed_field(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, unsigned shift, std::uint32_t mask, bool if_changed) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t value = ctx.gpr[5] & (if_changed && mask == 3u ? 0xFFFFFFFFu : 0xFFu);
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08966174u, {self});
    }
    if (ctx.gpr[2] == 0u) return;
    const std::uint32_t object = word(rt, self + 0x80u);
    const std::uint32_t flags = word(rt, object + 0x30u);
    ctx.gpr[4] = object;
    ctx.gpr[2] = (flags >> shift) & mask;
    ctx.gpr[3] = (flags & ~(mask << shift)) | ((value & mask) << shift);
    if (if_changed && ctx.gpr[2] == value) return;
    rt.memory().store32(object + 0x30u, ctx.gpr[3]);
    ctx.gpr[2] = word(rt, self + 0xE4u) | 4u;
    rt.memory().store32(self + 0xE4u, ctx.gpr[2]);
}

// A deleting destructor of a 28-byte script value: vtable, members released (+0x18 with kReleaseLocal
// / func_08A69D68, then the rest), the base vtable, freed.
void delete_value28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t vtable,
                    std::initializer_list<std::pair<std::uint32_t, std::uint32_t>> members) {
    const std::uint32_t self = ctx.gpr[4];
    rt.memory().store32(self, vtable);
    {
        GuestFrame frame(ctx, 0x10u);
        for (const auto &[offset, release] : members) ctx.gpr[2] = call_guest(rt, ctx, release, {self + offset});
    }
    rt.memory().store32(self, kScriptValueBase);
    ctx.gpr[2] = kScriptValueBase;
    tail_call(ctx, kFree, {self, 28u});
}

// The virtual at `slot` of every child in the chain from +0x84 (next at +0x54), with (child, the byte
// at +0x98).
void for_each_child_flag(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t slot) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    for (std::uint32_t child = word(rt, self + 0x84u); child != 0u; child = word(rt, child + 0x54u))
        ctx.gpr[2] = call_guest(rt, ctx, word(rt, word(rt, child) + slot), {child, rt.memory().load8(self + 0x98u)});
}

} // namespace

// func_08A77FF8(this): func_08A5B8B8(this), then the block at +0x10 released (as func_08A7805C).
void func_08A77FF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08A5B8B8u, {self});
    }
    release_block576(rt, ctx, self);
}

// Smart-pointer forwarders: func_08AA61D8(p, a, b) -> the virtual +0x110 of *p; func_08AA9240(this, x)
// -> func_08C312E4(the object at +0x4, x); func_08AA9D34(this) -> func_08A71B00(the object at +0x4,
// func_08AA9BEC(this)). Null objects go through func_08A5BB7C(message) first.
void func_08AA61D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = ctx.gpr[5], b = ctx.gpr[6];
    const std::uint32_t object = word(rt, ctx.gpr[4]);
    if (object == 0u) {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08A5BB7Cu, {0x08CB2EB8u});
    }
    ctx.gpr[3] = word(rt, object);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 0x110u);
    tail_call(ctx, ctx.gpr[2], {object, a, b});
}
void func_08AA9240(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4] + 0x4u);
    const std::uint32_t x = ctx.gpr[5];
    if (object == 0u) {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08A5BB7Cu, {0x08CB30B0u});
    }
    tail_call(ctx, 0x08C312E4u, {object, x});
}
void func_08AA9D34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t value = call_guest(rt, ctx, 0x08AA9BECu, {self});
    ctx.gpr[2] = value;
    const std::uint32_t object = word(rt, self + 0x4u);
    if (object == 0u) ctx.gpr[2] = call_guest(rt, ctx, 0x08A5BB7Cu, {0x08CB30B0u});
    tail_call(ctx, 0x08A71B00u, {object, value});
}

// func_08929774(this, float out[4]): func_08927A0C(+0x8, &a), func_08B4F5E0(&b, &a), out = b.
void func_08929774(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[5];
    const std::uint32_t object = word(rt, ctx.gpr[4] + 0x8u);
    GuestFrame frame(ctx, 0x60u);
    (void)call_guest(rt, ctx, 0x08927A0Cu, {object, frame.at(0x10u)});
    ctx.gpr[2] = call_guest(rt, ctx, 0x08B4F5E0u, {frame.at(0u), frame.at(0x10u)});
    for (std::uint32_t i = 0; i < 4u; ++i) rt.memory().store32(out + 4u * i, word(rt, frame.at(4u * i)));
}

// func_08B22630(this): the virtual +0xC of every entry's object in the list at +0x30 (next at +0x8),
// under the mutex at +0x4.
void func_08B22630(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, kLock, {self + 0x4u});
        for (std::uint32_t node = word(rt, self + 0x30u); node != 0u; node = word(rt, node + 0x8u)) {
            const std::uint32_t object = word(rt, node);
            ctx.gpr[2] = call_guest(rt, ctx, word(rt, word(rt, object) + 0xCu), {object});
        }
    }
    tail_call(ctx, kUnlock, {self + 0x4u});
}

// func_08B214D8(this, i) -> the halfwords func_08B21484(this, 2i) | func_08B21484(this, 2i + 1) << 16.
void func_08B214D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t i = ctx.gpr[5] << 1;
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t low = call_guest(rt, ctx, 0x08B21484u, {self, i});
    const std::uint32_t high = call_guest(rt, ctx, 0x08B21484u, {self, i + 1u});
    ctx.gpr[2] = low | (high << 16);
}

// func_08AF213C(a, b): the virtual +0x10 of a fills a local, func_08AF2758(b, &local) gives x, then
// func_08AF2978(b, a, x).
void func_08AF213C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = ctx.gpr[4];
    const std::uint32_t b = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    (void)call_guest(rt, ctx, word(rt, word(rt, a) + 0x10u), {a, frame.at(0u)});
    const float x = call_guest(rt, ctx, 0x08AF2758u, {b, frame.at(0u)}, {}).f0;
    ctx.gpr[2] = call_guest(rt, ctx, 0x08AF2978u, {b, a}, {x}).v0;
}

// func_0892B6B0(dst, src): copies the 12-byte record (bit 0 of the first byte only) and unpacks bits
// 1..5 of its first halfword into byte 12.
void func_0892B6B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t dst = ctx.gpr[4];
    const std::uint32_t src = ctx.gpr[5];
    rt.memory().store8(dst, rt.memory().load8(src) & 1u);
    rt.memory().store8(dst + 1u, rt.memory().load8(src + 1u));
    rt.memory().store16(dst + 2u, rt.memory().load16(src + 2u));
    rt.memory().store16(dst + 4u, rt.memory().load16(src + 4u));
    for (std::uint32_t i = 6u; i < 12u; ++i) rt.memory().store8(dst + i, rt.memory().load8(src + i));
    ctx.gpr[3] = rt.memory().load8(src + 11u);
    ctx.gpr[2] = (rt.memory().load16(src) >> 1) & 0x1Fu;
    rt.memory().store8(dst + 12u, static_cast<std::uint8_t>(ctx.gpr[2]));
}

// func_088A3A8C(this) -> 3.6 * the length of the vector func_08955358(the object at +0x18) gives, 0.0
// without the object (a speed in km/h).
void func_088A3A8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4] + 0x18u);
    ctx.fpr[0] = 0.0f;
    ctx.gpr[2] = object;
    if (object == 0u) return;
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[2] = call_guest(rt, ctx, 0x08955358u, {object, frame.at(0u)});
    const float x = float_at(rt, frame.at(0u)), y = float_at(rt, frame.at(4u)), z = float_at(rt, frame.at(8u));
    const float sum = psp_mul(x, x) + psp_mul(y, y) + psp_mul(z, z);
    ctx.fpr[0] = psp_mul(std::sqrt(sum), std::bit_cast<float>(0x40666666u));
}

// func_0892BB9C(this): unless the record +0x260's kind (+0x98) is negative: clears its halfwords
// +0x5E / +0x60 when the kind's flag (table at 0x08CF2948) is clear, func_08B21A4C(this, record), then
// +0x6C = record + 0x40.
void func_0892BB9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t record = word(rt, self + 0x260u);
    const std::uint32_t kind = word(rt, record + 0x98u);
    ctx.gpr[5] = record;
    ctx.gpr[3] = kind;
    ctx.gpr[2] = kind + 0x08CF2948u;
    if (static_cast<std::int32_t>(kind) < 0) return;
    if (rt.memory().load8(ctx.gpr[2]) == 0u) {
        rt.memory().store16(record + 0x5Eu, 0u);
        rt.memory().store16(word(rt, self + 0x260u) + 0x60u, 0u);
    }
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08B21A4Cu, {self, record});
    }
    ctx.gpr[2] = word(rt, self + 0x260u) + 0x40u;
    rt.memory().store32(self + 0x6Cu, ctx.gpr[2]);
}

// func_088CD6D8(_, object, x): func_088CD62C(func_088CB360(i), x, the float at +0x20 of object) for i 0..3.
void func_088CD6D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[5];
    const std::uint32_t x = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    for (std::uint32_t i = 0u; i < 4u; ++i) {
        const std::uint32_t entry = call_guest(rt, ctx, 0x088CB360u, {i});
        ctx.gpr[2] = call_guest(rt, ctx, 0x088CD62Cu, {entry, x}, {float_at(rt, object + 0x20u)}).v0;
    }
}

// New objects handed to a wrapper: func_08A18F14 (24 bytes, func_08A18EB8, func_08A5D40C), func_08A6D2D4
// (24, func_08A6D290, func_08A76AB4), func_08A55E14 (20, func_08A55D70, func_08A7263C), func_08A960C0
// (24, func_08A95AAC with a signed byte, func_08A76AB4), func_089E4C2C (40, func_08A12534,
// func_089FE2D8), func_08A98C1C (36, func_08A98B54, func_08A76AB4), func_08A894C8 (328, func_08A89448,
// func_08A71E10), func_08A851CC (24, func_08A84D10, func_08A76AB4).
void func_08A18F14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { wrap_new(rt, ctx, 24u, 0x08A18EB8u, 0x08A5D40Cu); }
void func_08A6D2D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { wrap_new(rt, ctx, 24u, 0x08A6D290u, 0x08A76AB4u); }
void func_08A55E14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { wrap_new(rt, ctx, 20u, 0x08A55D70u, 0x08A7263Cu); }
void func_08A960C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { wrap_new(rt, ctx, 24u, 0x08A95AACu, 0x08A76AB4u, true); }
void func_089E4C2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { wrap_new(rt, ctx, 40u, 0x08A12534u, 0x089FE2D8u); }
void func_08A98C1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { wrap_new(rt, ctx, 36u, 0x08A98B54u, 0x08A76AB4u); }
void func_08A894C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { wrap_new(rt, ctx, 328u, 0x08A89448u, 0x08A71E10u); }
void func_08A851CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { wrap_new(rt, ctx, 24u, 0x08A84D10u, 0x08A76AB4u); }

// func_088F95A8(a, b, c): r = c's virtual +0x28; when set, a's +0x8 object's +0xC = r and
// func_08AF1DC8(b's +0x4, that object, 144).
void func_088F95A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t target = word(rt, ctx.gpr[4] + 0x8u);
    const std::uint32_t first = word(rt, ctx.gpr[5] + 0x4u);
    const std::uint32_t c = ctx.gpr[6];
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, word(rt, word(rt, c) + 0x28u), {c});
    }
    ctx.gpr[4] = first;
    ctx.gpr[5] = target;
    ctx.gpr[6] = 144u;
    if (ctx.gpr[2] == 0u) return;
    rt.memory().store32(target + 0xCu, ctx.gpr[2]);
    tail_call(ctx, 0x08AF1DC8u, {});
}

// func_08ADC1CC(x) -> whether the record func_08ADB278(x, 0) was idle (signed byte +0x10 clear); then one
// more at its +0x198, inside func_08AEB808 / func_08AEB82C.
void func_08ADC1CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t x = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    (void)call_guest(rt, ctx, 0x08AEB808u, {x});
    const std::uint32_t record = call_guest(rt, ctx, 0x08ADB278u, {x, 0u});
    std::uint32_t result = 0u;
    if (record != 0u && rt.memory().load8(record + 0x10u) == 0u) {
        rt.memory().store32(record + 0x198u, word(rt, record + 0x198u) + 1u);
        result = 1u;
    }
    (void)call_guest(rt, ctx, 0x08AEB82Cu, {x});
    ctx.gpr[2] = result;
}

// func_08AF7684(this, x): func_08C4045C(x, this); marks the byte at +0xB when x's range (+0x0 / +0x4,
// or +0xC set) is not empty.
void func_08AF7684(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t x = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08C4045Cu, {x, self});
    }
    ctx.gpr[2] = word(rt, x + 0xCu);
    ctx.gpr[3] = word(rt, x + 0x4u);
    if (ctx.gpr[2] == 0u && ctx.gpr[3] == 0u) return;
    const std::uint32_t first = word(rt, x);
    ctx.gpr[2] = 1u;
    if (first != ctx.gpr[3]) rt.memory().store8(self + 0xBu, 1u);
}

// func_08B177E4(this, keep): func_08B28C44(this + 0x1958); unless keep or +0x19F8 is non-negative:
// func_08B28C74(this + 0x19B0), the byte +0x15 of +0x19E0 set, func_08B7B34C(this + 0x1878, 2).
void func_08B177E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t keep = ctx.gpr[5] & 0xFFu;
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, 0x08B28C44u, {self + 0x1958u});
    if (keep != 0u) return;
    ctx.gpr[2] = word(rt, self + 0x19F8u);
    if (static_cast<std::int32_t>(ctx.gpr[2]) >= 0) return;
    (void)call_guest(rt, ctx, 0x08B28C74u, {self + 0x19B0u});
    rt.memory().store8(word(rt, self + 0x19E0u) + 0x15u, 1u);
    ctx.gpr[2] = call_guest(rt, ctx, 0x08B7B34Cu, {self + 0x1878u, 2u});
}

// func_08B0C6B8(a, b) -> the virtual +0x80 (r, a, b) of r = func_08B0C43C(a, b), -1 without one.
void func_08B0C6B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = ctx.gpr[4], b = ctx.gpr[5];
    std::uint32_t r;
    {
        GuestFrame frame(ctx, 0x10u);
        r = call_guest(rt, ctx, 0x08B0C43Cu, {a, b});
    }
    if (r == 0u) {
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }
    ctx.gpr[3] = word(rt, r);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 0x80u);
    tail_call(ctx, ctx.gpr[2], {r, a, b});
}

// func_08A13B34(this, a, b): func_08ACCB50(this + 0x240, 0, a, b) between func_08B34888 / func_08B34B24
// (this + 0x10).
void func_08A13B34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t a = ctx.gpr[5], b = ctx.gpr[6];
    {
        GuestFrame frame(ctx, 0x20u);
        (void)call_guest(rt, ctx, 0x08B34888u, {self + 0x10u});
        ctx.gpr[2] = call_guest(rt, ctx, 0x08ACCB50u, {self + 0x240u, 0u, a, b});
    }
    tail_call(ctx, 0x08B34B24u, {self + 0x10u});
}

// func_089B3688(this, p, q) -> func_08B488BC(this + 0x120, *p, *q).
void func_089B3688(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    tail_call(ctx, 0x08B488BCu, {ctx.gpr[4] + 0x120u, word(rt, ctx.gpr[5]), word(rt, ctx.gpr[6])});
}

// func_08911F48(a, b, c): func_08911D88(a, b, c), then the virtual +0x8 (*b, 0, c) when *b is set.
void func_08911F48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t b = ctx.gpr[5], c = ctx.gpr[6];
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08911D88u, {ctx.gpr[4], b, c});
    }
    const std::uint32_t object = word(rt, b);
    ctx.gpr[3] = object;
    ctx.gpr[4] = object;
    ctx.gpr[5] = 0u;
    ctx.gpr[6] = c;
    if (object == 0u) return;
    ctx.gpr[3] = word(rt, object);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 0x8u);
    tail_call(ctx, ctx.gpr[2], {});
}

// func_08AD6398(this, x): x's virtuals +0x1C, +0x2C and +0x0, then func_08AD6334(this + 0x8, x).
void func_08AD6398(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t x = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        for (const std::uint32_t slot : {0x1Cu, 0x2Cu, 0x0u}) ctx.gpr[2] = call_guest(rt, ctx, word(rt, word(rt, x) + slot), {x});
    }
    tail_call(ctx, 0x08AD6334u, {self + 0x8u, x});
}

// func_0894BEFC(this): the default 16-byte settings record.
void func_0894BEFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    static constexpr std::uint8_t kDefaults[16]{127, 0, 1, 0, 1, 2, 0, 0, 0, 0, 0xFF, 1, 0, 0, 0, 0};
    for (std::uint32_t i = 0; i < 16u; ++i) rt.memory().store8(self + i, kDefaults[i]);
    ctx.gpr[2] = 0xFFFFFFFFu;
    ctx.gpr[3] = 2u;
    ctx.gpr[5] = 1u;
}

// func_08965984(this, _, x): base func_089B9EA4, the vtable, the smart pointer +0x20 = x (retained).
void func_08965984(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t x = ctx.gpr[6];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x089B9EA4u, {self, ctx.gpr[5], x});
    }
    rt.memory().store32(self, 0x08CC6970u);
    rt.memory().store32(self + 0x20u, x);
    ctx.gpr[2] = 0x08CC6970u;
    ctx.gpr[4] = x;
    if (x != 0u) tail_call(ctx, 0x08A5BA08u, {});
}

// Live objects read under a mutex (as func_08A7C230): func_08A7BDBC (+0x1C, kLockF450), func_089AA544
// (+0x54, kLock4ACC).
void func_08A7BDBC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { locked_live(rt, ctx, kLockF450, 0x1Cu); }
void func_089AA544(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { locked_live(rt, ctx, kLock4ACC, 0x54u); }

// func_089654E8(this) -> r = func_0896545C(this, +0x1C), after the virtual +0x120 when r is 2.
void func_089654E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t r = call_guest(rt, ctx, 0x0896545Cu, {self, word(rt, self + 0x1Cu)});
    if (r == 2u) (void)call_guest(rt, ctx, word(rt, word(rt, self) + 0x120u), {self});
    ctx.gpr[2] = r;
}

// func_089E008C(this, p): bit 9 of +0x1B0 = func_08B20A54(&key, *p, 0) for key = func_08B20920(&key,
// the name at 0x08C8D41C).
void func_089E008C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t p = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    (void)call_guest(rt, ctx, 0x08B20920u, {frame.at(0u), 0x08C8D41Cu});
    const std::uint32_t found = call_guest(rt, ctx, 0x08B20A54u, {frame.at(0u), word(rt, p), 0u});
    ctx.gpr[2] = found;
    ctx.gpr[3] = (word(rt, self + 0x1B0u) & ~0x200u) | ((found & 1u) << 9);
    rt.memory().store32(self + 0x1B0u, ctx.gpr[3]);
}

// More guarded setters on the object at +0x80: halfwords +0x2E / +0x2C (func_089669C0 / func_08966A18),
// the 2-bit field at bit 4 (func_089666F8) and bits 3 / 2 (func_08966758 / func_089667B8) when they
// change, bit 11 (func_08966250).
void func_089669C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { allowed_half(rt, ctx, 0x2Eu); }
void func_08966A18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { allowed_half(rt, ctx, 0x2Cu); }
void func_089666F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { allowed_field(rt, ctx, 4u, 3u, true); }
void func_08966758(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { allowed_field(rt, ctx, 3u, 1u, true); }
void func_089667B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { allowed_field(rt, ctx, 2u, 1u, true); }
void func_08966250(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { allowed_bit(rt, ctx, 11u); }

// func_08AD3C5C(x) -> func_08AD39E4(context, func_08AD3A14(context, x)) for the context at kContext04CC,
// 0 without it.
void func_08AD3C5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t x = ctx.gpr[4];
    const std::uint32_t context = word(rt, kContext04CC);
    ctx.gpr[5] = x;
    ctx.gpr[4] = context;
    if (context == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08AD3A14u, {context, x});
    }
    tail_call(ctx, 0x08AD39E4u, {context, ctx.gpr[2]});
}

// func_08A5A7D4(this, _, x) -> this = a value func_08A577FC builds from &x (as func_08A531C0);
// func_08AA20B0(this, flag) -> this = func_08A97A6C(flag).
void func_08A5A7D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    rt.memory().store32(frame.at(0u), ctx.gpr[6]);
    (void)call_guest(rt, ctx, 0x08A577FCu, {frame.at(4u), frame.at(0u)});
    (void)call_guest(rt, ctx, kAssignValue, {self, frame.at(4u)});
    (void)call_guest(rt, ctx, kReleaseLocal, {frame.at(4u)});
    ctx.gpr[2] = self;
}
void func_08AA20B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    assign_built(rt, ctx, 0x08A97A6Cu, {ctx.gpr[5] & 0xFFu});
}

// func_089FF3E0(this, flag): the byte at +0x98 = flag, handed to every child's virtual +0x1A8.
void func_089FF3E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store8(ctx.gpr[4] + 0x98u, static_cast<std::uint8_t>(ctx.gpr[5]));
    for_each_child_flag(rt, ctx, 0x1A8u);
}

// Deleting destructors: func_0899C4F4 (members +0x28 / +0x24, +0x20 released, func_089BA380, 44 bytes),
// func_08A852C4 (the +0x14 object deleted, func_08A780B0, 24 bytes), func_089B71CC (two vtables,
// members +0x134 / +0x130 / +0x12C, func_089A29E4, 384 bytes); script values func_08A654E4 /
// func_08A658E4 (28 bytes).
void func_0899C4F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    rt.memory().store32(self, 0x08CBCAB0u);
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, kReleaseLocal, {self + 0x28u});
        (void)call_guest(rt, ctx, kReleaseLocal, {self + 0x24u});
        if (const std::uint32_t object = word(rt, self + 0x20u); object != 0u) (void)call_guest(rt, ctx, 0x08A5B930u, {object});
        ctx.gpr[2] = call_guest(rt, ctx, 0x089BA380u, {self});
    }
    tail_call(ctx, kFreeAligned, {self, 44u, 4u});
}
void func_08A852C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t object = word(rt, self + 0x14u);
    rt.memory().store32(self, 0x08CBEA58u);
    {
        GuestFrame frame(ctx, 0x10u);
        if (object != 0u) (void)call_guest(rt, ctx, word(rt, word(rt, object) + 0x4u), {object});
        ctx.gpr[2] = call_guest(rt, ctx, 0x08A780B0u, {self});
    }
    tail_call(ctx, kFreeAligned, {self, 24u, 4u});
}
void func_089B71CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    rt.memory().store32(self, 0x08CCAC10u);
    rt.memory().store32(self + 0x38u, 0x08CCAE6Cu);
    {
        GuestFrame frame(ctx, 0x10u);
        for (const std::uint32_t member : {0x134u, 0x130u, 0x12Cu}) (void)call_guest(rt, ctx, kReleaseLocal, {self + member});
        ctx.gpr[2] = call_guest(rt, ctx, 0x089A29E4u, {self});
    }
    tail_call(ctx, kFreeAligned, {self, 384u, 4u});
}
void func_08A654E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    delete_value28(rt, ctx, 0x08CD29A8u, {{0x18u, kReleaseLocal}, {0xCu, 0x08A75A88u}, {0x8u, 0x08A69D68u}});
}
void func_08A658E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    delete_value28(rt, ctx, 0x08CD2A48u, {{0x18u, 0x08A69D68u}, {0x14u, 0x08A69D68u}, {0x8u, 0x08A75A88u}});
}

// `new` objects with arguments: func_08A653F0(a, b) (28 bytes, func_08A6538C), func_08A679E4(a, flag)
// (16, func_08A679B8), func_08A657E0(a, b, c) (28, func_08A6576C).
void func_08A653F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = ctx.gpr[4], b = ctx.gpr[5];
    make_object_n(rt, ctx, 28u, 0x08A6538Cu, {a, b});
}
void func_08A679E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = ctx.gpr[4], flag = ctx.gpr[5] & 0xFFu;
    make_object_n(rt, ctx, 16u, 0x08A679B8u, {a, flag});
}
void func_08A657E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = ctx.gpr[4], b = ctx.gpr[5], c = ctx.gpr[6];
    make_object_n(rt, ctx, 28u, 0x08A6576Cu, {a, b, c});
}

// func_089AA6B0(this) -> the first element of the list +0x30..+0x34 (0 when empty), under kLock4AA4.
void func_089AA6B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    (void)call_guest(rt, ctx, kLock, {kLock4AA4});
    const std::uint32_t first = word(rt, self + 0x30u);
    const std::uint32_t result = first != word(rt, self + 0x34u) ? word(rt, first) : 0u;
    (void)call_guest(rt, ctx, kUnlock, {kLock4AA4});
    ctx.gpr[2] = result;
}

// func_089929EC(this, x): as func_08993B3C, ending in func_089928A0(this, r).
void func_089929EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t x = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08A00574u, {self, x});
        ctx.gpr[2] = call_guest(rt, ctx, word(rt, word(rt, x) + 0x16Cu), {x});
    }
    if (ctx.gpr[2] != 0u) tail_call(ctx, 0x089928A0u, {self, ctx.gpr[2]});
}

// func_08A61838(this, x): func_08AA6404(x, this + 0x8); from x's level 12 (+0x28) on, the byte +0xC =
// whether func_08AA64DC(x) is 1.
void func_08A61838(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t x = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    (void)call_guest(rt, ctx, 0x08AA6404u, {x, self + 0x8u});
    ctx.gpr[2] = static_cast<std::int32_t>(word(rt, x + 0x28u)) < 12 ? 1u : 0u;
    if (ctx.gpr[2] != 0u) return;
    ctx.gpr[2] = call_guest(rt, ctx, 0x08AA64DCu, {x}) == 1u ? 1u : 0u;
    rt.memory().store8(self + 0xCu, static_cast<std::uint8_t>(ctx.gpr[2]));
}

// func_089B8BB4(a, b, c): c retained (func_08A5BA28) for func_089B8BAC(a, c) and func_089BA4D8(c, b)
// (when b), then released (func_08A5B970).
void func_089B8BB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = ctx.gpr[4], b = ctx.gpr[5], c = ctx.gpr[6];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08A5BA28u, {c});
        ctx.gpr[2] = call_guest(rt, ctx, 0x089B8BACu, {a, c});
        if (b != 0u) ctx.gpr[2] = call_guest(rt, ctx, 0x089BA4D8u, {c, b});
    }
    tail_call(ctx, 0x08A5B970u, {c});
}

// func_08AF7320(this): unless bit 5 of the flags byte at +0x8 is set, clears bit 7 (after
// func_08B5281C(+0x1C) when it was set).
void func_08AF7320(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t flags = rt.memory().load8(self + 0x8u);
    ctx.gpr[3] = flags;
    ctx.gpr[2] = flags & 32u;
    if (ctx.gpr[2] != 0u) return;
    if ((flags & 0x80u) != 0u) {
        {
            GuestFrame frame(ctx, 0x10u);
            (void)call_guest(rt, ctx, 0x08B5281Cu, {word(rt, self + 0x1Cu)});
        }
        flags = rt.memory().load8(self + 0x8u);
        ctx.gpr[3] = flags;
    }
    ctx.gpr[2] = flags & 127u;
    rt.memory().store8(self + 0x8u, static_cast<std::uint8_t>(ctx.gpr[2]));
}

// func_08AD650C(_, out, key, busy, x) -> whether out = {func_08ABB9E8(key), x} was found (not when busy).
void func_08AD650C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[5];
    const std::uint32_t x = ctx.gpr[8];
    rt.memory().store32(out, 0u);
    std::uint32_t result = 0u;
    if (ctx.gpr[7] == 0u && x != 0u) {
        std::uint32_t found;
        {
            GuestFrame frame(ctx, 0x10u);
            found = call_guest(rt, ctx, 0x08ABB9E8u, {ctx.gpr[6]});
        }
        rt.memory().store32(out, found);
        if (found != 0u) {
            rt.memory().store32(out + 0x4u, x);
            result = 1u;
        }
    }
    ctx.gpr[3] = result;
    ctx.gpr[2] = result;
}

// func_08ABB924(x) -> the first non-zero virtual +0x20 (listener, x) on the list at kListeners7188.
void func_08ABB924(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t x = ctx.gpr[4];
    ctx.gpr[2] = 0u;
    GuestFrame frame(ctx, 0x10u);
    for (std::uint32_t node = word(rt, kListeners7188); node != 0u; node = word(rt, node)) {
        const std::uint32_t listener = word(rt, node + 0x4u);
        ctx.gpr[2] = call_guest(rt, ctx, word(rt, word(rt, listener) + 0x20u), {listener, x});
        if (ctx.gpr[2] != 0u) return;
    }
    ctx.gpr[2] = 0u;
}

// func_08A10718(this, value*): the smart pointer at +0x68 = *value, under kLock61B0.
void func_08A10718(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t member = ctx.gpr[4] + 0x68u;
    const std::uint32_t value = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, kLock, {kLock61B0});
        if (member != value) ctx.gpr[2] = call_guest(rt, ctx, kAssignSmart, {member, word(rt, value)});
    }
    tail_call(ctx, kUnlock, {kLock61B0});
}

// func_089A53FC(this): func_08A02244, func_089A30D4 unless bits 0/2 of +0x138, func_089A5314,
// func_089A301C(this, 0), then clears +0x14C.
void func_089A53FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08A02244u, {self});
        if ((word(rt, self + 0x138u) & 5u) == 0u) (void)call_guest(rt, ctx, 0x089A30D4u, {self});
        (void)call_guest(rt, ctx, 0x089A5314u, {self});
        ctx.gpr[2] = call_guest(rt, ctx, 0x089A301Cu, {self, 0u});
    }
    rt.memory().store32(self + 0x14Cu, 0u);
}

// func_089D5BD0(this) -> func_089A3130(o) or else o, for the live object o at +0x24; 0 without one.
void func_089D5BD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4] + 0x24u);
    ctx.gpr[4] += 0x24u;
    std::uint32_t result = 0u;
    if (object != 0u && word(rt, object + 0x4u) != 0u) {
        GuestFrame frame(ctx, 0x10u);
        const std::uint32_t found = call_guest(rt, ctx, 0x089A3130u, {object});
        result = found != 0u ? found : object;
    }
    ctx.gpr[3] = result;
    ctx.gpr[2] = result;
}

// func_089A318C(x) -> the last object in the chain from func_089A3130(x) (next: func_08965CE0) for which
// func_08968C84 holds, else 0.
void func_089A318C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    std::uint32_t result = 0u;
    for (std::uint32_t object = call_guest(rt, ctx, 0x089A3130u, {ctx.gpr[4]}); object != 0u;
         object = call_guest(rt, ctx, 0x08965CE0u, {object}))
        if (call_guest(rt, ctx, 0x08968C84u, {object}) != 0u) result = object;
    ctx.gpr[2] = result;
}

// func_08918438(this, x, i) -> whether the virtual +0x24 (this, 0, i) is x, for i <= 0 and the flag at
// +0x2180 of the +0x8 object set; else 0.
void func_08918438(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t x = ctx.gpr[5];
    const auto i = static_cast<std::int32_t>(ctx.gpr[6]);
    ctx.gpr[7] = self;
    std::uint32_t result = 0u;
    if (rt.memory().load8(word(rt, self + 0x8u) + 0x2180u) != 0u) {
        ctx.gpr[5] = 0u;
        if (i <= 0) {
            GuestFrame frame(ctx, 0x10u);
            result = call_guest(rt, ctx, word(rt, word(rt, self) + 0x24u), {self, 0u, ctx.gpr[6]}) == x ? 1u : 0u;
        }
    }
    ctx.gpr[3] = result;
    ctx.gpr[2] = result;
}

// func_08995138(this, other): func_08A013D8(this, other), then other's +0x120 / +0x124 / +0x128.
void func_08995138(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t other = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08A013D8u, {self, other});
    }
    if (other == 0u) return;
    for (const std::uint32_t offset : {0x128u, 0x120u, 0x124u}) rt.memory().store32(self + offset, word(rt, other + offset));
}

// func_08926398(this): the vtable, func_08926384(this, 0.0), then the scales +0x14 / +0x1C = 1.0 and the
// rest cleared.
void func_08926398(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    rt.memory().store32(self, 0x08CC6130u);
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08926384u, {self}, {0.0f}).v0;
    }
    const std::uint32_t one = std::bit_cast<std::uint32_t>(1.0f);
    rt.memory().store32(self + 0x1Cu, one);
    rt.memory().store32(self + 0x20u, 0u);
    rt.memory().store32(self + 0x14u, one);
    rt.memory().store32(self + 0x18u, 0u);
    rt.memory().store32(self + 0x24u, 0u);
    rt.memory().store32(self + 0x28u, 0u);
    ctx.fpr[0] = 1.0f;
}

// func_08B1BA0C(this, flag): base func_08B2E1F8, then the byte +0xC = flag and the rest cleared.
void func_08B1BA0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t flag = ctx.gpr[5] & 0xFFu;
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08B2E1F8u, {self});
    }
    rt.memory().store8(self + 0xCu, static_cast<std::uint8_t>(flag));
    rt.memory().store32(self + 0x14u, 0u);
    rt.memory().store32(self + 0x4u, 0u);
    rt.memory().store32(self + 0x8u, 0u);
    rt.memory().store8(self + 0xDu, 0u);
    rt.memory().store32(self + 0x10u, 0u);
}

// func_088A7548(this): when the owner's byte +0x4CC is set, clears it and calls func_088A5A0C(this);
// then the virtual +0x0 of +0x4 (object, +0x0) and both cleared (as func_088978B4).
void func_088A7548(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t owner = word(rt, self);
    GuestFrame frame(ctx, 0x10u);
    if (rt.memory().load8(owner + 0x4CCu) != 0u) {
        rt.memory().store8(owner + 0x4CCu, 0u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x088A5A0Cu, {self});
    }
    const std::uint32_t object = word(rt, self + 0x4u);
    if (object == 0u) return;
    ctx.gpr[2] = call_guest(rt, ctx, word(rt, word(rt, object)), {object, word(rt, self)});
    rt.memory().store32(self, 0u);
    rt.memory().store32(self + 0x4u, 0u);
}

// func_08AA1BE4(this, value) -> the virtual +0x10C (this, value as 4 big-endian bytes, 4) — the
// unsigned twin of func_08AA1B3C.
void func_08AA1BE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t value = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    for (std::uint32_t i = 0; i < 4u; ++i) rt.memory().store8(frame.at(i), static_cast<std::uint8_t>(value >> (24u - 8u * i)));
    ctx.gpr[2] = call_guest(rt, ctx, word(rt, word(rt, self) + 0x10Cu), {self, frame.at(0u), 4u});
}

// func_0897C0E4(this, x) -> func_08BEE82C(this + 0x18, x) under the mutex at +0x20.
void func_0897C0E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { locked_call(rt, ctx, 0x20u, 0x08BEE82Cu, 0x18u); }

} // namespace gt::game

GT_DECOMP(0x08A77FF8u, gt::game::func_08A77FF8, gt::ReturnKind::Word);
GT_DECOMP(0x08AA61D8u, gt::game::func_08AA61D8, gt::ReturnKind::Word);
GT_DECOMP(0x08AA9240u, gt::game::func_08AA9240, gt::ReturnKind::Word);
GT_DECOMP(0x08AA9D34u, gt::game::func_08AA9D34, gt::ReturnKind::Word);
GT_DECOMP(0x08929774u, gt::game::func_08929774, gt::ReturnKind::Void);
GT_DECOMP(0x08B22630u, gt::game::func_08B22630, gt::ReturnKind::Word);
GT_DECOMP(0x08B214D8u, gt::game::func_08B214D8, gt::ReturnKind::Word);
GT_DECOMP(0x08AF213Cu, gt::game::func_08AF213C, gt::ReturnKind::Word);
GT_DECOMP(0x0892B6B0u, gt::game::func_0892B6B0, gt::ReturnKind::Word);
GT_DECOMP(0x088A3A8Cu, gt::game::func_088A3A8C, gt::ReturnKind::Float);
GT_DECOMP(0x0892BB9Cu, gt::game::func_0892BB9C, gt::ReturnKind::Void);
GT_DECOMP(0x088CD6D8u, gt::game::func_088CD6D8, gt::ReturnKind::Void);
GT_DECOMP(0x08A18F14u, gt::game::func_08A18F14, gt::ReturnKind::Word);
GT_DECOMP(0x08A6D2D4u, gt::game::func_08A6D2D4, gt::ReturnKind::Word);
GT_DECOMP(0x08A55E14u, gt::game::func_08A55E14, gt::ReturnKind::Word);
GT_DECOMP(0x08A960C0u, gt::game::func_08A960C0, gt::ReturnKind::Word);
GT_DECOMP(0x089E4C2Cu, gt::game::func_089E4C2C, gt::ReturnKind::Word);
GT_DECOMP(0x08A98C1Cu, gt::game::func_08A98C1C, gt::ReturnKind::Word);
GT_DECOMP(0x08A894C8u, gt::game::func_08A894C8, gt::ReturnKind::Word);
GT_DECOMP(0x08A851CCu, gt::game::func_08A851CC, gt::ReturnKind::Word);
GT_DECOMP(0x088F95A8u, gt::game::func_088F95A8, gt::ReturnKind::Word);
GT_DECOMP(0x08ADC1CCu, gt::game::func_08ADC1CC, gt::ReturnKind::Word);
GT_DECOMP(0x08AF7684u, gt::game::func_08AF7684, gt::ReturnKind::Void);
GT_DECOMP(0x08B177E4u, gt::game::func_08B177E4, gt::ReturnKind::Void);
// func_08B0C6B8 reaches kernel imports the isolated run can't replay (not comparable in all 36 cases),
// so the gate can't check it.
// GT_DECOMP(0x08B0C6B8u, gt::game::func_08B0C6B8, gt::ReturnKind::Word);
GT_DECOMP(0x08A13B34u, gt::game::func_08A13B34, gt::ReturnKind::Word);
GT_DECOMP(0x089B3688u, gt::game::func_089B3688, gt::ReturnKind::Word);
GT_DECOMP(0x08911F48u, gt::game::func_08911F48, gt::ReturnKind::Word);
GT_DECOMP(0x08AD6398u, gt::game::func_08AD6398, gt::ReturnKind::Word);
GT_DECOMP(0x0894BEFCu, gt::game::func_0894BEFC, gt::ReturnKind::Word);
GT_DECOMP(0x08965984u, gt::game::func_08965984, gt::ReturnKind::Word);
GT_DECOMP(0x08A7BDBCu, gt::game::func_08A7BDBC, gt::ReturnKind::Word);
GT_DECOMP(0x089AA544u, gt::game::func_089AA544, gt::ReturnKind::Word);
GT_DECOMP(0x089654E8u, gt::game::func_089654E8, gt::ReturnKind::Word);
GT_DECOMP(0x089E008Cu, gt::game::func_089E008C, gt::ReturnKind::Void);
GT_DECOMP(0x089669C0u, gt::game::func_089669C0, gt::ReturnKind::Void);
GT_DECOMP(0x08966A18u, gt::game::func_08966A18, gt::ReturnKind::Void);
GT_DECOMP(0x089666F8u, gt::game::func_089666F8, gt::ReturnKind::Void);
GT_DECOMP(0x08966758u, gt::game::func_08966758, gt::ReturnKind::Void);
GT_DECOMP(0x089667B8u, gt::game::func_089667B8, gt::ReturnKind::Void);
GT_DECOMP(0x08966250u, gt::game::func_08966250, gt::ReturnKind::Void);
GT_DECOMP(0x08AD3C5Cu, gt::game::func_08AD3C5C, gt::ReturnKind::Word);
GT_DECOMP(0x08A5A7D4u, gt::game::func_08A5A7D4, gt::ReturnKind::Word);
GT_DECOMP(0x08AA20B0u, gt::game::func_08AA20B0, gt::ReturnKind::Word);
GT_DECOMP(0x089FF3E0u, gt::game::func_089FF3E0, gt::ReturnKind::Void);
GT_DECOMP(0x0899C4F4u, gt::game::func_0899C4F4, gt::ReturnKind::Word);
GT_DECOMP(0x08A852C4u, gt::game::func_08A852C4, gt::ReturnKind::Word);
GT_DECOMP(0x089B71CCu, gt::game::func_089B71CC, gt::ReturnKind::Word);
GT_DECOMP(0x08A654E4u, gt::game::func_08A654E4, gt::ReturnKind::Word);
GT_DECOMP(0x08A658E4u, gt::game::func_08A658E4, gt::ReturnKind::Word);
GT_DECOMP(0x08A653F0u, gt::game::func_08A653F0, gt::ReturnKind::Word);
GT_DECOMP(0x08A679E4u, gt::game::func_08A679E4, gt::ReturnKind::Word);
GT_DECOMP(0x08A657E0u, gt::game::func_08A657E0, gt::ReturnKind::Word);
GT_DECOMP(0x089AA6B0u, gt::game::func_089AA6B0, gt::ReturnKind::Word);
GT_DECOMP(0x089929ECu, gt::game::func_089929EC, gt::ReturnKind::Word);
GT_DECOMP(0x08A61838u, gt::game::func_08A61838, gt::ReturnKind::Void);
GT_DECOMP(0x089B8BB4u, gt::game::func_089B8BB4, gt::ReturnKind::Word);
GT_DECOMP(0x08AF7320u, gt::game::func_08AF7320, gt::ReturnKind::Void);
GT_DECOMP(0x08AD650Cu, gt::game::func_08AD650C, gt::ReturnKind::Word);
GT_DECOMP(0x08ABB924u, gt::game::func_08ABB924, gt::ReturnKind::Word);
GT_DECOMP(0x08A10718u, gt::game::func_08A10718, gt::ReturnKind::Word);
GT_DECOMP(0x089A53FCu, gt::game::func_089A53FC, gt::ReturnKind::Void);
GT_DECOMP(0x089D5BD0u, gt::game::func_089D5BD0, gt::ReturnKind::Word);
GT_DECOMP(0x089A318Cu, gt::game::func_089A318C, gt::ReturnKind::Word);
GT_DECOMP(0x08918438u, gt::game::func_08918438, gt::ReturnKind::Word);
GT_DECOMP(0x08995138u, gt::game::func_08995138, gt::ReturnKind::Void);
GT_DECOMP(0x08926398u, gt::game::func_08926398, gt::ReturnKind::Void);
GT_DECOMP(0x08B1BA0Cu, gt::game::func_08B1BA0C, gt::ReturnKind::Void);
GT_DECOMP(0x088A7548u, gt::game::func_088A7548, gt::ReturnKind::Void);
GT_DECOMP(0x08AA1BE4u, gt::game::func_08AA1BE4, gt::ReturnKind::Word);
GT_DECOMP(0x0897C0E4u, gt::game::func_0897C0E4, gt::ReturnKind::Word);

// --- batch 39 ---

namespace gt::game {
namespace {

constexpr std::uint32_t kGuardAcquire = 0x08B6441Cu;  // (guard) -> whether this caller must construct (C++ static locals)
constexpr std::uint32_t kGuardRelease = 0x08B64494u;  // (guard)
constexpr std::uint32_t kAtExit = 0x08B77B1Cu;        // (function): registers a destructor for exit

// The function-local static at `object`: constructed once by `construct(object)` under `guard`, its
// destructor `dtor` registered at exit; returns &object.
void static_instance(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t guard, std::uint32_t object,
                     std::uint32_t construct, std::uint32_t dtor) {
    ctx.gpr[3] = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(rt.memory().load8(guard))));
    ctx.gpr[4] = guard;
    if (ctx.gpr[3] == 0u) {
        GuestFrame frame(ctx, 0x10u);
        if (call_guest(rt, ctx, kGuardAcquire, {guard}) != 0u) {
            (void)call_guest(rt, ctx, construct, {object});
            (void)call_guest(rt, ctx, kGuardRelease, {guard});
            (void)call_guest(rt, ctx, kAtExit, {dtor});
        }
    }
    ctx.gpr[2] = object;
}

// `wrap(this, &object)` for a new `size`-byte object (aligned to 4) built by `construct(object, x, y)`.
void wrap_new2(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t size, std::uint32_t construct,
               std::uint32_t wrap) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t x = ctx.gpr[5];
    const std::uint32_t y = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t object = call_guest(rt, ctx, kAllocateAligned4, {size, 4u});
    (void)call_guest(rt, ctx, construct, {object, x, y});
    rt.memory().store32(frame.at(0u), object);
    ctx.gpr[2] = call_guest(rt, ctx, wrap, {self, frame.at(0u)});
}

// t = func_089559C4(this) + x (x in f12); returns t * the float at `far` of record func_089551B0(this) +
// (1 - t) * `near_value` (PSP float products).
float blend_by_progress(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t far, std::uint32_t near,
                        bool near_in_record) {
    const std::uint32_t self = ctx.gpr[4];
    const float x = ctx.fpr[12];
    GuestFrame frame(ctx, 0x20u);
    const float t = call_guest(rt, ctx, 0x089559C4u, {self}, {}).f0 + x;
    const std::uint32_t record = call_guest(rt, ctx, 0x089551B0u, {self});
    const float far_value = float_at(rt, record + far);
    float near_value;
    if (near_in_record) near_value = float_at(rt, call_guest(rt, ctx, 0x089551B0u, {self}) + near);
    else near_value = float_at(rt, word(rt, self + 0x8u) + near);
    return psp_mul(t, far_value) + psp_mul(near_value, 1.0f - t);
}

// The virtual +0x1C of the object at +0x40 of the record at +0xC (with a1 passed on), when both exist.
std::uint32_t object64_query(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, word(rt, ctx.gpr[4] + 0xCu) + 0x40u);
    if (object == 0u) return 0u;
    return call_guest(rt, ctx, word(rt, word(rt, object) + 0x1Cu), {object, ctx.gpr[5]});
}

// The x range check of func_08AF7684: whether x (+0x0..+0x4, or +0xC set) is a non-empty range.
bool range_nonempty(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t x) {
    ctx.gpr[2] = word(rt, x + 0xCu);
    ctx.gpr[3] = word(rt, x + 0x4u);
    if (ctx.gpr[2] == 0u && ctx.gpr[3] == 0u) return false;
    ctx.gpr[2] = word(rt, x);
    return ctx.gpr[2] != ctx.gpr[3];
}

} // namespace

// Function-local statics (guard, object, constructor, exit destructor): func_08A68480, func_08A67F7C,
// func_08A68214, func_08A688F0, func_08A5BC88 (built by func_08B28DCC), func_08A68120.
void func_08A68480(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { static_instance(rt, ctx, 0x08CFF228u, 0x08CFF2D0u, 0x08A6845Cu, 0x08A6850Cu); }
void func_08A67F7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { static_instance(rt, ctx, 0x08CFF240u, 0x08CFF2B8u, 0x08A67F58u, 0x08A68008u); }
void func_08A68214(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { static_instance(rt, ctx, 0x08CFF230u, 0x08CFF2C8u, 0x08A681F0u, 0x08A682A0u); }
void func_08A688F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { static_instance(rt, ctx, 0x08CFF218u, 0x08CFF2E0u, 0x08A688CCu, 0x08A6897Cu); }
void func_08A5BC88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { static_instance(rt, ctx, 0x08CFF138u, 0x08CFF140u, 0x08B28DCCu, 0x08A5BC7Cu); }
void func_08A68120(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { static_instance(rt, ctx, 0x08CFF238u, 0x08CFF2C0u, 0x08A680FCu, 0x08A681ACu); }

// New objects (x, y) handed to a wrapper: func_08AAF890 (28 bytes, func_08AAF838, func_08A76AB4),
// func_08A74A4C (24, func_08A74A08, func_08A802B4), func_08A744F0 (24, func_08A744AC, func_08A7263C),
// func_08A74F00 (24, func_08A74EBC, func_08A86CF8), func_08A8A2DC (32, func_08A8A278, func_08A5D190),
// func_08A7B544 (24, func_08A7B500, func_08A5D40C).
void func_08AAF890(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { wrap_new2(rt, ctx, 28u, 0x08AAF838u, 0x08A76AB4u); }
void func_08A74A4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { wrap_new2(rt, ctx, 24u, 0x08A74A08u, 0x08A802B4u); }
void func_08A744F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { wrap_new2(rt, ctx, 24u, 0x08A744ACu, 0x08A7263Cu); }
void func_08A74F00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { wrap_new2(rt, ctx, 24u, 0x08A74EBCu, 0x08A86CF8u); }
void func_08A8A2DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { wrap_new2(rt, ctx, 32u, 0x08A8A278u, 0x08A5D190u); }
void func_08A7B544(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { wrap_new2(rt, ctx, 24u, 0x08A7B500u, 0x08A5D40Cu); }

// func_08A00FA4(this, x): func_089660A0, bit 7 of +0xE4 = whether +0x108 is set, func_089B8980(this +
// 0xE8), the virtual +0x244 (this, x), then func_089B87B8(this + 0xE8).
void func_08A00FA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t x = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x089660A0u, {self, x});
        const std::uint32_t set = word(rt, self + 0x108u) != 0u ? 1u : 0u;
        rt.memory().store32(self + 0xE4u, (word(rt, self + 0xE4u) & ~0x80u) | (set << 7));
        (void)call_guest(rt, ctx, 0x089B8980u, {self + 0xE8u});
        ctx.gpr[2] = call_guest(rt, ctx, word(rt, word(rt, self) + 0x244u), {self, x});
    }
    tail_call(ctx, 0x089B87B8u, {self + 0xE8u});
}

// func_08AD8DB4(dst, src): the halfword fields both of src's masks select (bit i of src[0] and src[1],
// 2..12) are copied, then the masks are ORed into dst's; nothing without src[0].
void func_08AD8DB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t dst = ctx.gpr[4];
    const std::uint32_t src = ctx.gpr[5];
    const auto first = static_cast<std::int32_t>(static_cast<std::int16_t>(rt.memory().load16(src)));
    if (first == 0) return;
    const auto second = static_cast<std::int32_t>(static_cast<std::int16_t>(rt.memory().load16(src + 2u)));
    for (std::uint32_t i = 2u; i < 13u; ++i)
        if (((first >> i) & 1) != 0 && ((second >> i) & 1) != 0) rt.memory().store16(dst + 2u * i, rt.memory().load16(src + 2u * i));
    const auto low = static_cast<std::int32_t>(static_cast<std::int16_t>(rt.memory().load16(dst))) | first;
    const auto high = static_cast<std::int32_t>(static_cast<std::int16_t>(rt.memory().load16(dst + 2u))) | second;
    rt.memory().store16(dst + 2u, static_cast<std::uint16_t>(high));
    rt.memory().store16(dst, static_cast<std::uint16_t>(low));
}

// func_08AEF874(i): for entry i (56 bytes) of the table at 0x08D41B6C, func_08AEF484(entry) in state 1
// or func_08AEF714(entry) in state 2 (+0x18).
void func_08AEF874(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t entry = word(rt, 0x08D41B6Cu) + ctx.gpr[4] * 56u;
    const std::uint32_t state = word(rt, entry + 0x18u);
    ctx.gpr[4] = entry;
    ctx.gpr[8] = entry;
    ctx.gpr[3] = state;
    ctx.gpr[9] = static_cast<std::int32_t>(state) < 2 ? 1u : 0u;
    ctx.gpr[2] = 1u;
    if (state != 1u && state != 2u) {
        if (ctx.gpr[9] == 0u) ctx.gpr[2] = 2u;
        return;
    }
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, state == 1u ? 0x08AEF484u : 0x08AEF714u, {entry});
}

// func_089EC958(this, c): appends c to the vector at +0x14 (func_08C0A8A0 grows it); a newline (10)
// first takes one off +0xC.
void func_089EC958(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    if (ctx.gpr[5] == 10u) rt.memory().store32(ctx.gpr[4] + 0xCu, word(rt, ctx.gpr[4] + 0xCu) - 1u);
    push_word(rt, ctx, 0x14u, 0x08C0A8A0u);
}

// func_08AF4088(this, source) -> this: the virtual +0xC (this, object) of source's +0x4 or +0x8 object,
// or func_08B4E648 / func_08B4E734 with a fresh local when it has neither.
void func_08AF4088(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t source = ctx.gpr[5];
    GuestFrame frame(ctx, 0x50u);
    std::uint32_t object = word(rt, source + 0x4u);
    if (object == 0u) object = word(rt, source + 0x8u);
    if (object != 0u) (void)call_guest(rt, ctx, word(rt, word(rt, object) + 0xCu), {self, object});
    else {
        (void)call_guest(rt, ctx, 0x08B4E648u, {frame.at(0u)});
        (void)call_guest(rt, ctx, 0x08B4E734u, {self, frame.at(0u)});
    }
    ctx.gpr[2] = self;
}

// func_08A0F8C8(this) -> the virtual +0x16C of the first object in the chain from +0x4C (next +0x4C)
// whose virtual +0x178 is 0; 0 when none.
void func_08A0F8C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t object = word(rt, ctx.gpr[4] + 0x4Cu);
    {
        GuestFrame frame(ctx, 0x10u);
        for (; object != 0u; object = word(rt, object + 0x4Cu))
            if (call_guest(rt, ctx, word(rt, word(rt, object) + 0x178u), {object}) == 0u) break;
    }
    if (object == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    ctx.gpr[3] = word(rt, object);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 0x16Cu);
    tail_call(ctx, ctx.gpr[2], {object});
}

// func_088A9730(this): func_08946C58(owner, +0x4, x) with x the float +0x54 of func_08955300(object,
// +0x4), for the object at +0xC of +0x8 and its owner at +0xC; 0.0 without +0x8.
void func_088A9730(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t holder = word(rt, ctx.gpr[4] + 0x8u);
    ctx.fpr[0] = 0.0f;
    ctx.gpr[2] = holder;
    if (holder == 0u) return;
    const std::uint32_t second = word(rt, ctx.gpr[4] + 0x4u);
    const std::uint32_t object = word(rt, holder + 0xCu);
    const std::uint32_t owner = word(rt, object + 0xCu);
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08955300u, {object, second});
    }
    ctx.fpr[12] = float_at(rt, ctx.gpr[2] + 0x54u);
    tail_call(ctx, 0x08946C58u, {owner, second});
}

// func_088FAC80(out, pair, x) -> out: func_08902F00(out, second, x) or func_08905300(out, first, x), or
// (0, 0) when the pair is empty.
void func_088FAC80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t pair = ctx.gpr[5];
    if (const std::uint32_t second = word(rt, pair + 0x4u); second != 0u) {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08902F00u, {out, second, ctx.gpr[6]});
    } else if (const std::uint32_t first = word(rt, pair); first != 0u) {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08905300u, {out, first, ctx.gpr[6]});
    } else {
        rt.memory().store32(out, 0u);
        rt.memory().store32(out + 0x4u, 0u);
    }
    ctx.gpr[2] = out;
}

// Entry lookups by index (0 <= i < func_088D9F80(this)): func_088DB67C -> func_088E8474(entry i, 760
// bytes, of +0xC0); func_088DA588 -> func_088F8D58(the object at +0x14C, i). 0 otherwise.
void func_088DB67C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const auto i = static_cast<std::int32_t>(ctx.gpr[5]);
    ctx.gpr[2] = 0u;
    if (i < 0) return;
    GuestFrame frame(ctx, 0x10u);
    const auto count = static_cast<std::int32_t>(call_guest(rt, ctx, 0x088D9F80u, {self, ctx.gpr[5]}));
    if (i >= count) return;
    ctx.gpr[2] = call_guest(rt, ctx, 0x088E8474u, {word(rt, self + 0xC0u) + ctx.gpr[5] * 760u});
}
void func_088DA588(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const auto i = static_cast<std::int32_t>(ctx.gpr[5]);
    ctx.gpr[2] = 0u;
    if (i < 0) return;
    std::int32_t count;
    {
        GuestFrame frame(ctx, 0x10u);
        count = static_cast<std::int32_t>(call_guest(rt, ctx, 0x088D9F80u, {self, ctx.gpr[5]}));
    }
    ctx.gpr[2] = 0u;
    if (i >= count) return;
    const std::uint32_t object = word(rt, self + 0x14Cu);
    if (object == 0u) return;
    tail_call(ctx, 0x088F8D58u, {object, ctx.gpr[5]});
}

// func_089002E8(this, x): func_089000BC(this + 0xC, x); when func_08AF293C(x) has bits 17/18, func_0890E3A4
// (the byte +0x4B4, the floats +0x4A8..+0x4B0).
void func_089002E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t x = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x089000BCu, {self + 0xCu, x});
        ctx.gpr[2] = call_guest(rt, ctx, 0x08AF293Cu, {x}) & 0x60000u;
    }
    ctx.gpr[3] = 0x60000u;
    if (ctx.gpr[2] == 0u) return;
    ctx.fpr[12] = float_at(rt, self + 0x4A8u);
    ctx.fpr[13] = float_at(rt, self + 0x4ACu);
    ctx.fpr[14] = float_at(rt, self + 0x4B0u);
    tail_call(ctx, 0x0890E3A4u, {rt.memory().load8(self + 0x4B4u)});
}

// Progress blends (see blend_by_progress): func_08955FB0 (+0x28 / record +0xC), func_08955F44 (+0x2C /
// +0x10), func_08955ED8 (+0x30 / +0x14), func_089559D0 (+0x108 / +0x18), and against the owner's
// +0x704 / +0x70C / +0x708: func_08956408 (+0x3C), func_089563A0 (+0x44), func_08956338 (+0x40).
void func_08955FB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[0] = blend_by_progress(rt, ctx, 0x28u, 0xCu, true); }
void func_08955F44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[0] = blend_by_progress(rt, ctx, 0x2Cu, 0x10u, true); }
void func_08955ED8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[0] = blend_by_progress(rt, ctx, 0x30u, 0x14u, true); }
void func_089559D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[0] = blend_by_progress(rt, ctx, 0x108u, 0x18u, true); }
void func_08956408(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[0] = blend_by_progress(rt, ctx, 0x3Cu, 0x704u, false); }
void func_089563A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[0] = blend_by_progress(rt, ctx, 0x44u, 0x70Cu, false); }
void func_08956338(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[0] = blend_by_progress(rt, ctx, 0x40u, 0x708u, false); }

// func_08956104(this, x) -> max(0, func_08956098(t) - func_08955F44(t)) for t = func_089559C4(this) + x.
void func_08956104(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const float x = ctx.fpr[12];
    GuestFrame frame(ctx, 0x20u);
    const float t = call_guest(rt, ctx, 0x089559C4u, {self}, {}).f0 + x;
    const float a = call_guest(rt, ctx, 0x08956098u, {self}, {t}).f0;
    const float b = call_guest(rt, ctx, 0x08955F44u, {self}, {t}).f0;
    const float d = a - b;
    ctx.fpr[0] = d < 0.0f ? 0.0f : d;
}

// func_088A4684(float out[4], source) -> out: the three floats func_089564D8 / func_089564F0 /
// func_089564E4 give for source's +0x18 object (all 0 without it).
void func_088A4684(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t source = ctx.gpr[5];
    for (std::uint32_t i = 0; i < 4u; ++i) rt.memory().store32(out + 4u * i, 0u);
    if (word(rt, source + 0x18u) != 0u) {
        GuestFrame frame(ctx, 0x10u);
        std::uint32_t at = out;
        for (const std::uint32_t getter : {0x089564D8u, 0x089564F0u, 0x089564E4u}) {
            const float value = call_guest(rt, ctx, getter, {word(rt, source + 0x18u)}, {}).f0;
            rt.memory().store32(at, std::bit_cast<std::uint32_t>(value));
            at += 4u;
        }
    }
    ctx.gpr[2] = out;
}

// func_08A7BEBC(a, b, c): func_08AAD93C(kObject705C, a, b, c) under kLockF450.
void func_08A7BEBC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = ctx.gpr[4], b = ctx.gpr[5], c = ctx.gpr[6];
    {
        GuestFrame frame(ctx, 0x20u);
        (void)call_guest(rt, ctx, kLock, {kLockF450});
        ctx.gpr[2] = call_guest(rt, ctx, 0x08AAD93Cu, {kObject705C, a, b, c});
    }
    tail_call(ctx, kUnlock, {kLockF450});
}

// func_08AB8BDC(this, dt): moves the value at +0x0 towards the target at +0x4 by rate +0x8 (up) or +0xC
// (down) times dt, without overshooting.
void func_08AB8BDC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const float dt = ctx.fpr[12];
    const float value = float_at(rt, self);
    const float target = float_at(rt, self + 0x4u);
    float next;
    if (0.0f <= target - value) {
        next = value + psp_mul(dt, float_at(rt, self + 0x8u));
        if (!(next < target)) next = target;
    } else {
        next = value - psp_mul(float_at(rt, self + 0xCu), dt);
        if (next < target) next = target;
    }
    rt.memory().store32(self, std::bit_cast<std::uint32_t>(next));
    ctx.fpr[0] = next;
}

// func_0889DB3C(this, a, b): when the virtual +0x20 holds, func_0889AB24(this + 0x28, b, a's byte +0x25,
// a's float +0x1C).
void func_0889DB3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t a = ctx.gpr[5], b = ctx.gpr[6];
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, word(rt, word(rt, self) + 0x20u), {self});
    }
    ctx.gpr[4] = self + 0x28u;
    ctx.gpr[5] = b;
    if (ctx.gpr[2] == 0u) return;
    ctx.fpr[12] = float_at(rt, a + 0x1Cu);
    tail_call(ctx, 0x0889AB24u, {self + 0x28u, b, rt.memory().load8(a + 0x25u)});
}

// func_08B03C78(this): for r = func_08B03BB4(this): when bit 14 of its flags (+0xE2) is set, clears bit
// 14 of its 6-byte entry (+0xEA) in the owner's table (+0x7C); always clears bit 2 of the flags.
void func_08B03C78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t r;
    {
        GuestFrame frame(ctx, 0x10u);
        r = call_guest(rt, ctx, 0x08B03BB4u, {self});
    }
    ctx.gpr[6] = r;
    if ((rt.memory().load16(r + 0xE2u) & 0x4000u) != 0u) {
        const auto index = static_cast<std::int32_t>(static_cast<std::int16_t>(rt.memory().load16(r + 0xEAu)));
        const std::uint32_t entry = word(rt, word(rt, self) + 0x7Cu) + static_cast<std::uint32_t>(index) * 6u;
        rt.memory().store16(entry + 2u, static_cast<std::uint16_t>(rt.memory().load16(entry + 2u) & ~0x4000u));
    }
    ctx.gpr[2] = rt.memory().load16(r + 0xE2u) & ~4u;
    rt.memory().store16(r + 0xE2u, static_cast<std::uint16_t>(ctx.gpr[2]));
}

// func_088D636C(this, i) -> the big-endian signed halfword i of func_088D4D88(this, i)'s table, or i
// itself when there is none or func_088D6358(this) is 2.
void func_088D636C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t i = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t table = call_guest(rt, ctx, 0x088D4D88u, {self, i});
    const std::uint32_t mode = call_guest(rt, ctx, 0x088D6358u, {self});
    std::uint32_t result = i;
    if (mode != 2u && table != 0u)
        result = static_cast<std::uint32_t>(static_cast<std::int32_t>(bswap32(rt.memory().load16(table + (i << 1)))) >> 16);
    ctx.gpr[2] = result;
}

// func_088B4640(this): adds 50 to the counter at +0x8 of +0x28 (capped at 180000), the virtual +0x14 of
// the object at +0x38, then clears the byte +0x97 of +0x28.
void func_088B4640(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t counter = word(rt, self + 0x28u);
    const std::uint32_t n = word(rt, counter + 0x8u);
    rt.memory().store32(counter + 0x8u, static_cast<std::int32_t>(n) > 0x2BF1F ? 0x2BF20u : n + 50u);
    const std::uint32_t object = word(rt, self + 0x38u);
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, word(rt, word(rt, object) + 0x14u), {object, self + 0x4u, self});
    }
    rt.memory().store8(counter + 0x97u, 0u);
}

// Through the object at +0x40 of the record at +0xC (its virtual +0x1C gives r): func_088995CC(this, _,
// x) -> r's virtual +0xC (r, x) when x; func_08899564 -> r's virtual +0x8 (r, x), else 0;
// func_08899410 -> r's virtual +0x10, else 0.0.
void func_088995CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t x = ctx.gpr[6];
    const std::uint32_t object = word(rt, word(rt, ctx.gpr[4] + 0xCu) + 0x40u);
    if (object == 0u || x == 0u) return;
    std::uint32_t r;
    {
        GuestFrame frame(ctx, 0x10u);
        r = call_guest(rt, ctx, word(rt, word(rt, object) + 0x1Cu), {object, ctx.gpr[5]});
    }
    ctx.gpr[2] = r;
    if (r == 0u) return;
    ctx.gpr[2] = word(rt, r);
    tail_call(ctx, word(rt, ctx.gpr[2] + 0xCu), {r, x});
}
void func_08899564(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t x = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t r = object64_query(rt, ctx);
    ctx.gpr[2] = r != 0u ? call_guest(rt, ctx, word(rt, word(rt, r) + 0x8u), {r, x}) : 0u;
}
void func_08899410(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t x = ctx.gpr[6];
    std::uint32_t r;
    {
        GuestFrame frame(ctx, 0x10u);
        r = object64_query(rt, ctx);
    }
    ctx.fpr[0] = 0.0f;
    if (r == 0u) return;
    ctx.gpr[2] = word(rt, r);
    tail_call(ctx, word(rt, ctx.gpr[2] + 0x10u), {r, x});
}

// func_088994F8(this, i, src): a copy of the record src (func_0892B6B0) handed to the virtual +0x68 of
// func_088F8D58(+0xC, i).
void func_088994F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t i = ctx.gpr[5];
    if (word(rt, self + 0xCu) == 0u) return;
    GuestFrame frame(ctx, 0x20u);
    (void)call_guest(rt, ctx, 0x0892B6B0u, {frame.at(0u), ctx.gpr[6]});
    const std::uint32_t target = call_guest(rt, ctx, 0x088F8D58u, {word(rt, self + 0xCu), i});
    if (target != 0u) ctx.gpr[2] = call_guest(rt, ctx, word(rt, word(rt, target) + 0x68u), {target, frame.at(0u)});
}

// func_08AFD748(this, object): when bit 0 of +0xE2 is set, +0xD0 = object's virtual +0x8 (object, this +
// 0xB0, this + 0x20, &{0, -1.0, 0}, +0xD0).
void func_08AFD748(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t object = ctx.gpr[5];
    ctx.gpr[2] = rt.memory().load16(self + 0xE2u) & 1u;
    if (ctx.gpr[2] == 0u) return;
    GuestFrame frame(ctx, 0x20u);
    rt.memory().store32(frame.at(4u), std::bit_cast<std::uint32_t>(-1.0f));
    rt.memory().store32(frame.at(0u), 0u);
    rt.memory().store32(frame.at(8u), 0u);
    ctx.gpr[2] = call_guest(rt, ctx, word(rt, word(rt, object) + 0x8u),
                            {object, self + 0xB0u, self + 0x20u, frame.at(0u), word(rt, self + 0xD0u)});
    rt.memory().store32(self + 0xD0u, ctx.gpr[2]);
}

// func_08AF1490(this): clears the three words of every channel (count +0x298, 20-byte entries from
// +0x10, each through its +0x8 pointer) of the object at +0x4, then resets its cursor (+0xC/+0x10 =
// +0x4/+0x8, +0x14 = 0).
void func_08AF1490(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t holder = ctx.gpr[4] + 0x4u;
    std::uint32_t object = word(rt, holder);
    for (std::int32_t i = 0; i < static_cast<std::int32_t>(word(rt, object + 0x298u)); ++i) {
        const std::uint32_t channel = word(rt, object + 0x10u + 20u * static_cast<std::uint32_t>(i) + 0x8u);
        rt.memory().store32(channel + 0x4u, 0u);
        rt.memory().store32(channel + 0xCu, 0u);
        rt.memory().store32(channel + 0x8u, 0u);
        object = word(rt, holder);
    }
    object = word(rt, holder);
    ctx.gpr[2] = word(rt, object + 0x4u);
    ctx.gpr[3] = word(rt, object + 0x8u);
    rt.memory().store32(object + 0x14u, 0u);
    rt.memory().store32(object + 0xCu, ctx.gpr[2]);
    rt.memory().store32(object + 0x10u, ctx.gpr[3]);
}

// func_08968850(this, flag): func_08966050(this, flag), then down the chain func_08965CE0 gives while
// bit 12 of +0xE0 is set.
void func_08968850(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t flag = ctx.gpr[5] & 0xFFu;
    std::uint32_t next = 0u;
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08966050u, {self, flag});
        if ((word(rt, self + 0xE0u) & 0x1000u) == 0u) return;
        next = call_guest(rt, ctx, 0x08965CE0u, {self});
    }
    ctx.gpr[2] = next;
    if (next != 0u) tail_call(ctx, 0x08968850u, {next, flag});
}

// func_088FBDC0(this, a, b) -> func_0890640C(r + 0x34, a, b) for r = the virtual +0x34, unless absent or
// its owner's +0x2C is set; func_088FC0D4(this) -> func_08906498(r + 0x34) when func_08906398 holds.
void func_088FBDC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t a = ctx.gpr[5], b = ctx.gpr[6];
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, word(rt, word(rt, self) + 0x34u), {self, a, b});
    }
    const std::uint32_t r = ctx.gpr[2];
    ctx.gpr[4] = r + 0x34u;
    if (r == 0u) return;
    ctx.gpr[2] = word(rt, word(rt, r + 0x4u) + 0x2Cu);
    if (ctx.gpr[2] != 0u) return;
    tail_call(ctx, 0x0890640Cu, {r + 0x34u, a, b});
}
void func_088FC0D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t r;
    {
        GuestFrame frame(ctx, 0x10u);
        r = call_guest(rt, ctx, word(rt, word(rt, self) + 0x34u), {self});
        ctx.gpr[2] = r;
        if (r == 0u) return;
        ctx.gpr[2] = word(rt, word(rt, r + 0x4u) + 0x2Cu);
        if (ctx.gpr[2] != 0u) return;
        ctx.gpr[2] = call_guest(rt, ctx, 0x08906398u, {r + 0x34u});
    }
    if (ctx.gpr[2] != 0u) tail_call(ctx, 0x08906498u, {r + 0x34u});
}

// func_08A77444(out, x) -> out = func_08A923F4(out, x's virtual +0x24), then *out's virtual +0x1C (x).
void func_08A77444(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t x = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t value = call_guest(rt, ctx, word(rt, word(rt, x) + 0x24u), {x});
    (void)call_guest(rt, ctx, 0x08A923F4u, {out, value});
    if (const std::uint32_t object = word(rt, out); object != 0u) (void)call_guest(rt, ctx, word(rt, word(rt, object) + 0x1Cu), {object, x});
    ctx.gpr[2] = out;
}

// func_088DF324(this, a, flag) -> the word at +0x230 of +0x618 when func_088D62FC(this + 4) is 1 and flag
// isn't 1, else the word at +0x7C of func_088DF13C(this, a).
void func_088DF324(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t a = ctx.gpr[5];
    const std::uint32_t flag = ctx.gpr[6] & 0xFFu;
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t state = call_guest(rt, ctx, 0x088D62FCu, {self + 0x4u});
    if (state == 1u && flag != 1u) ctx.gpr[2] = word(rt, word(rt, self + 0x618u) + 0x230u);
    else ctx.gpr[2] = word(rt, call_guest(rt, ctx, 0x088DF13Cu, {self, a}) + 0x7Cu);
}

// func_08898804(this, _, x): func_088F9190(this, ...), then the virtual +0x24 (object, x) of +0x40.
void func_08898804(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t x = ctx.gpr[6];
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x088F9190u, {self, ctx.gpr[5], x});
    }
    const std::uint32_t object = word(rt, self + 0x40u);
    ctx.gpr[2] = object;
    ctx.gpr[4] = object;
    ctx.gpr[5] = x;
    if (object == 0u) return;
    ctx.gpr[2] = word(rt, object);
    tail_call(ctx, word(rt, ctx.gpr[2] + 0x24u), {});
}

// func_088F5EA0(this, _, flag): func_088F6FC8(this + 0x20), then func_088F7440(c, this + 0x20, flag) for
// each child c from +0x4 (next +0x108).
void func_088F5EA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t flag = ctx.gpr[6] & 0xFFu;
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, 0x088F6FC8u, {self + 0x20u, ctx.gpr[5], ctx.gpr[6]});
    for (std::uint32_t child = word(rt, self + 0x4u); child != 0u; child = word(rt, child + 0x108u))
        ctx.gpr[2] = call_guest(rt, ctx, 0x088F7440u, {child, self + 0x20u, flag});
}

// func_088F1D30(this, a, _, d): func_088F1B60(+0x8, d's virtual +0x24), func_088F5B6C(+0x8's +0x844,
// a's +0x1C), then func_08AF1DE0(a's +0x4, +0x8, 0x10000).
void func_088F1D30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t a = ctx.gpr[5];
    const std::uint32_t d = ctx.gpr[7];
    {
        GuestFrame frame(ctx, 0x10u);
        const std::uint32_t target = word(rt, self + 0x8u);
        const std::uint32_t value = call_guest(rt, ctx, word(rt, word(rt, d) + 0x24u), {d});
        (void)call_guest(rt, ctx, 0x088F1B60u, {target, value});
        ctx.gpr[2] = call_guest(rt, ctx, 0x088F5B6Cu, {word(rt, self + 0x8u) + 0x844u, word(rt, a + 0x1Cu)});
    }
    tail_call(ctx, 0x08AF1DE0u, {word(rt, a + 0x4u), word(rt, self + 0x8u), 0x10000u});
}

// func_089C87A8(this, owner, key) -> this: the vtable, then func_08B855E0(this, entry + 0x14) for the entry
// func_08C0080C(owner's +0x2C + 0x18, &key) finds (unless it is the end, +0x2C + 0x1C).
void func_089C87A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t owner = ctx.gpr[5];
    rt.memory().store32(self, 0x08D74FDCu);
    GuestFrame frame(ctx, 0x20u);
    rt.memory().store32(frame.at(0u), ctx.gpr[6]);
    const std::uint32_t entry = call_guest(rt, ctx, 0x08C0080Cu, {word(rt, owner + 0x2Cu) + 0x18u, frame.at(0u)});
    if (entry != word(rt, owner + 0x2Cu) + 0x1Cu) (void)call_guest(rt, ctx, 0x08B855E0u, {self, entry + 0x14u});
    ctx.gpr[2] = self;
}

// func_088B5E68(float out[2], m) -> out = the 2D point (+0x190, +0x194) transformed by m (rows at +0x0,
// +0x10, +0x20 and translation +0x30) (PSP float products; the zero y column still multiplies).
void func_088B5E68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t m = ctx.gpr[5];
    const float a = float_at(rt, m + 0x190u), b = float_at(rt, m + 0x194u);
    const float x = psp_mul(a, float_at(rt, m)) + psp_mul(float_at(rt, m + 0x10u), 0.0f) + psp_mul(b, float_at(rt, m + 0x20u)) + float_at(rt, m + 0x30u);
    const float y = psp_mul(a, float_at(rt, m + 0x8u)) + psp_mul(float_at(rt, m + 0x18u), 0.0f) + psp_mul(b, float_at(rt, m + 0x28u)) + float_at(rt, m + 0x38u);
    rt.memory().store32(out, std::bit_cast<std::uint32_t>(x));
    rt.memory().store32(out + 0x4u, std::bit_cast<std::uint32_t>(y));
    ctx.gpr[2] = out;
}

// func_08A61DD4(this, i): +0x38 = &slot i of the table at +0x1C of the object behind +0x34's +0x30 (0
// without +0x34).
void func_08A61DD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t i = ctx.gpr[5];
    std::uint32_t slot = 0u;
    if (const std::uint32_t holder = word(rt, self + 0x34u); holder != 0u) {
        const std::uint32_t object = word(rt, holder + 0x30u);
        if (object == 0u) {
            GuestFrame frame(ctx, 0x10u);
            (void)call_guest(rt, ctx, 0x08A5BB7Cu, {0x08CB02A4u});
        }
        slot = word(rt, object + 0x1Cu) + (i << 2);
    }
    ctx.gpr[2] = slot;
    rt.memory().store32(self + 0x38u, slot);
}

// func_08AD4F1C(i): selects i (byte +0x85 of the state at 0x08CFFE40, func_08AD44B4), then
// func_08AB9894(handle) when func_08AB983C() isn't already entry i's handle (68-byte entries at 0x08D404E4).
void func_08AD4F1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t i = ctx.gpr[4];
    rt.memory().store8(0x08CFFE40u + 0x85u, static_cast<std::uint8_t>(i));
    std::uint32_t current;
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08AD44B4u, {0x08CFFE40u});
        current = call_guest(rt, ctx, 0x08AB983Cu, {});
    }
    const std::uint32_t handle = word(rt, 0x08D404E4u + i * 68u);
    ctx.gpr[2] = current;
    if (current != handle) tail_call(ctx, 0x08AB9894u, {handle, handle});
}

// func_08A873C0(this, a, b, c) -> the virtual +0x34 (object, a, b, c) of the object at +0x14.
void func_08A873C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = ctx.gpr[5], b = ctx.gpr[6], c = ctx.gpr[7];
    const std::uint32_t object = word(rt, ctx.gpr[4] + 0x14u);
    if (object == 0u) {
        GuestFrame frame(ctx, 0x20u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08A5BB7Cu, {0x08CB190Cu});
    }
    ctx.gpr[2] = word(rt, object);
    tail_call(ctx, word(rt, ctx.gpr[2] + 0x34u), {object, a, b, c});
}

// func_08B0C650(a, b) -> the virtual +0x84 (r, a, b, 1) of r = func_08B0C43C(a, b), -1 without one.
void func_08B0C650(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = ctx.gpr[4], b = ctx.gpr[5];
    std::uint32_t r;
    {
        GuestFrame frame(ctx, 0x10u);
        r = call_guest(rt, ctx, 0x08B0C43Cu, {a, b});
    }
    if (r == 0u) {
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }
    ctx.gpr[3] = word(rt, r);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 0x84u);
    tail_call(ctx, ctx.gpr[2], {r, a, b, 1u});
}

// func_08A101F8(this): destructor — two vtables, +0x74 released (func_08A5B930), the members
// +0x70..+0x64 released, then func_089BBAC8(this).
void func_08A101F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t object = word(rt, self + 0x74u);
    rt.memory().store32(self, 0x08CB9110u);
    rt.memory().store32(self + 0x38u, 0x08CB92DCu);
    {
        GuestFrame frame(ctx, 0x10u);
        if (object != 0u) (void)call_guest(rt, ctx, 0x08A5B930u, {object});
        for (const std::uint32_t member : {0x70u, 0x6Cu, 0x68u, 0x64u}) ctx.gpr[2] = call_guest(rt, ctx, kReleaseLocal, {self + member});
    }
    tail_call(ctx, 0x089BBAC8u, {self});
}

// func_08AF7618(this, x): func_08C4045C(x, this + 4); clears bit 0 of the byte +0x0 when x's range is not
// empty (as func_08AF7684).
void func_08AF7618(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t x = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08C4045Cu, {x, self + 0x4u});
    }
    if (!range_nonempty(rt, ctx, x)) return;
    ctx.gpr[2] = rt.memory().load8(self) & ~1u;
    rt.memory().store8(self, static_cast<std::uint8_t>(ctx.gpr[2]));
}

// func_08AFA60C(this, a, b, c, d): func_08AFA3F0(this, a, b, c, d), then func_08B52CE4(this + 0x2C, a, b, c, d).
void func_08AFA60C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], a = ctx.gpr[5], b = ctx.gpr[6], c = ctx.gpr[7], d = ctx.gpr[8];
    {
        GuestFrame frame(ctx, 0x20u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08AFA3F0u, {self, a, b, c, d});
    }
    tail_call(ctx, 0x08B52CE4u, {self + 0x2Cu, a, b, c, d});
}

// func_08B17E84(this): clears the 24-byte record.
void func_08B17E84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    for (std::uint32_t i = 0; i < 24u; ++i) rt.memory().store8(self + i, 0u);
    ctx.gpr[2] = self + 0x4u;
    ctx.gpr[3] = self + 0xCu;
    ctx.gpr[5] = self + 0x14u;
    ctx.gpr[6] = self + 0x8u;
    ctx.gpr[7] = self + 0x10u;
}

// func_08A41258(out, source, i) -> out: func_08A40C24(out, &element i of source's vector at +0x14) for a
// valid i, else func_08A40C14(out, kLookupTable).
void func_08A41258(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t source = ctx.gpr[5];
    const auto i = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t begin = word(rt, source + 0x14u);
    GuestFrame frame(ctx, 0x10u);
    if (i >= 0 && i < (static_cast<std::int32_t>(word(rt, source + 0x18u) - begin) >> 2))
        (void)call_guest(rt, ctx, 0x08A40C24u, {out, begin + (ctx.gpr[6] << 2), ctx.gpr[6], ctx.gpr[6] << 2});
    else
        (void)call_guest(rt, ctx, 0x08A40C14u, {out, kLookupTable});
    ctx.gpr[2] = out;
}

// func_089364AC(this, _, i) -> the 64-bit (func_08938188(slot i at +0x8C), i), or (-1, -1) when the slot
// is empty or that gives -1.
void func_089364AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t i = ctx.gpr[6];
    const std::uint32_t slot = word(rt, ctx.gpr[4] + 0x8Cu + (i << 2));
    std::uint32_t low = 0xFFFFFFFFu, high = 0xFFFFFFFFu;
    if (slot != 0u) {
        GuestFrame frame(ctx, 0x10u);
        const std::uint32_t r = call_guest(rt, ctx, 0x08938188u, {slot});
        if (r != 0xFFFFFFFFu) {
            low = r;
            high = i;
        }
    }
    ctx.gpr[2] = low;
    ctx.gpr[3] = high;
}

// func_08A78168(this): deleting destructor of func_08A780B0's object (20 bytes).
void func_08A78168(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t block = word(rt, self + 0x10u);
    rt.memory().store32(self, 0x08CD3C58u);
    {
        GuestFrame frame(ctx, 0x10u);
        if (block != 0u) {
            (void)call_guest(rt, ctx, 0x08C25BFCu, {block, word(rt, block + 0x8u)});
            (void)call_guest(rt, ctx, kFreeAligned, {block, 576u, 4u});
        }
        ctx.gpr[2] = call_guest(rt, ctx, 0x08A5BB5Cu, {self});
    }
    tail_call(ctx, kFreeAligned, {self, 20u, 4u});
}

// func_08B180F4(this): the virtual +0x1C (this, 0), func_08B24D84(this, the pair at +0x1860 + 8 * side),
// flips the side (+0x1874), then the virtual +0x18.
void func_08B180F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, word(rt, word(rt, self) + 0x1Cu), {self, 0u});
        const std::uint32_t pair = self + 0x1860u + (word(rt, self + 0x1874u) << 3);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08B24D84u, {self, word(rt, pair + 0x4u), word(rt, pair + 0x8u)});
    }
    rt.memory().store32(self + 0x1874u, word(rt, self + 0x1874u) ^ 1u);
    ctx.gpr[3] = word(rt, self);
    tail_call(ctx, word(rt, ctx.gpr[3] + 0x18u), {self});
}

// func_08ACD284(this, i) -> the word at +0x500 of slot i (240 bytes), read under the slot's mutex (+0x4A0).
void func_08ACD284(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t slot = ctx.gpr[4] + ctx.gpr[5] * 240u;
    GuestFrame frame(ctx, 0x20u);
    (void)call_guest(rt, ctx, kLock, {slot + 0x4A0u});
    const std::uint32_t value = word(rt, slot + 0x500u);
    (void)call_guest(rt, ctx, kUnlock, {slot + 0x4A0u});
    ctx.gpr[2] = value;
}

// func_08B121BC(out, object, source) -> out: func_08B11A98(out, object, source), kind 9, then object's
// virtual +0xA4 (object, out, func_08B17A98(source's +0x50)) inside func_08B17AA8.
void func_08B121BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t object = ctx.gpr[5];
    const std::uint32_t inner = word(rt, ctx.gpr[6] + 0x50u);
    GuestFrame frame(ctx, 0x10u);
    (void)call_guest(rt, ctx, 0x08B11A98u, {out, object, ctx.gpr[6]});
    rt.memory().store32(out, 9u);
    const std::uint32_t value = call_guest(rt, ctx, 0x08B17A98u, {inner});
    (void)call_guest(rt, ctx, word(rt, word(rt, object) + 0xA4u), {object, out, value});
    (void)call_guest(rt, ctx, 0x08B17AA8u, {inner});
    ctx.gpr[2] = out;
}

// func_089660F4(this) -> 1 when the owner func_089BD5D4 gives has bit 6 of +0x138, else whether
// func_089660E8(this) is 0.
void func_089660F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t owner = call_guest(rt, ctx, 0x089BD5D4u, {self});
    std::uint32_t result;
    if (owner != 0u && (word(rt, owner + 0x138u) & 64u) != 0u) result = 1u;
    else if (owner != 0u) result = call_guest(rt, ctx, 0x089660E8u, {self}) != 0u ? 0u : 1u;
    else result = (call_guest(rt, ctx, 0x089660E8u, {self}) ^ 1u) & 0xFFu;
    ctx.gpr[2] = result;
}

// func_08A9212C() -> the value func_08A9C0A8(&t, a local func_08A9DD14 built, 1) produces.
void func_08A9212C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    (void)call_guest(rt, ctx, 0x08A9DD14u, {frame.at(0u)});
    const std::uint32_t source = word(rt, frame.at(0u));
    if (source == 0u) (void)call_guest(rt, ctx, 0x08A5BB7Cu, {0x08CB1F14u});
    (void)call_guest(rt, ctx, 0x08A9C0A8u, {frame.at(4u), source, 1u});
    (void)call_guest(rt, ctx, kReleaseLocal, {frame.at(0u)});
    const std::uint32_t result = word(rt, frame.at(4u));
    (void)call_guest(rt, ctx, kReleaseLocal, {frame.at(4u)});
    ctx.gpr[2] = result;
}

} // namespace gt::game

GT_DECOMP(0x08A68480u, gt::game::func_08A68480, gt::ReturnKind::Word);
GT_DECOMP(0x08A67F7Cu, gt::game::func_08A67F7C, gt::ReturnKind::Word);
GT_DECOMP(0x08A68214u, gt::game::func_08A68214, gt::ReturnKind::Word);
GT_DECOMP(0x08A688F0u, gt::game::func_08A688F0, gt::ReturnKind::Word);
GT_DECOMP(0x08A5BC88u, gt::game::func_08A5BC88, gt::ReturnKind::Word);
GT_DECOMP(0x08A68120u, gt::game::func_08A68120, gt::ReturnKind::Word);
GT_DECOMP(0x08AAF890u, gt::game::func_08AAF890, gt::ReturnKind::Word);
GT_DECOMP(0x08A74A4Cu, gt::game::func_08A74A4C, gt::ReturnKind::Word);
GT_DECOMP(0x08A744F0u, gt::game::func_08A744F0, gt::ReturnKind::Word);
GT_DECOMP(0x08A74F00u, gt::game::func_08A74F00, gt::ReturnKind::Word);
GT_DECOMP(0x08A8A2DCu, gt::game::func_08A8A2DC, gt::ReturnKind::Word);
GT_DECOMP(0x08A7B544u, gt::game::func_08A7B544, gt::ReturnKind::Word);
GT_DECOMP(0x08A00FA4u, gt::game::func_08A00FA4, gt::ReturnKind::Word);
GT_DECOMP(0x08AD8DB4u, gt::game::func_08AD8DB4, gt::ReturnKind::Void);
GT_DECOMP(0x08AEF874u, gt::game::func_08AEF874, gt::ReturnKind::Void);
GT_DECOMP(0x089EC958u, gt::game::func_089EC958, gt::ReturnKind::Void);
GT_DECOMP(0x08AF4088u, gt::game::func_08AF4088, gt::ReturnKind::Word);
GT_DECOMP(0x08A0F8C8u, gt::game::func_08A0F8C8, gt::ReturnKind::Word);
GT_DECOMP(0x088A9730u, gt::game::func_088A9730, gt::ReturnKind::Float);
GT_DECOMP(0x088FAC80u, gt::game::func_088FAC80, gt::ReturnKind::Word);
GT_DECOMP(0x088DB67Cu, gt::game::func_088DB67C, gt::ReturnKind::Word);
GT_DECOMP(0x088DA588u, gt::game::func_088DA588, gt::ReturnKind::Word);
GT_DECOMP(0x089002E8u, gt::game::func_089002E8, gt::ReturnKind::Word);
GT_DECOMP(0x08955FB0u, gt::game::func_08955FB0, gt::ReturnKind::Float);
GT_DECOMP(0x08955F44u, gt::game::func_08955F44, gt::ReturnKind::Float);
GT_DECOMP(0x08955ED8u, gt::game::func_08955ED8, gt::ReturnKind::Float);
GT_DECOMP(0x089559D0u, gt::game::func_089559D0, gt::ReturnKind::Float);
GT_DECOMP(0x08956408u, gt::game::func_08956408, gt::ReturnKind::Float);
GT_DECOMP(0x089563A0u, gt::game::func_089563A0, gt::ReturnKind::Float);
GT_DECOMP(0x08956338u, gt::game::func_08956338, gt::ReturnKind::Float);
GT_DECOMP(0x08956104u, gt::game::func_08956104, gt::ReturnKind::Float);
GT_DECOMP(0x088A4684u, gt::game::func_088A4684, gt::ReturnKind::Word);
GT_DECOMP(0x08A7BEBCu, gt::game::func_08A7BEBC, gt::ReturnKind::Word);
GT_DECOMP(0x08AB8BDCu, gt::game::func_08AB8BDC, gt::ReturnKind::Float);
GT_DECOMP(0x0889DB3Cu, gt::game::func_0889DB3C, gt::ReturnKind::Word);
GT_DECOMP(0x08B03C78u, gt::game::func_08B03C78, gt::ReturnKind::Void);
GT_DECOMP(0x088D636Cu, gt::game::func_088D636C, gt::ReturnKind::Word);
GT_DECOMP(0x088B4640u, gt::game::func_088B4640, gt::ReturnKind::Void);
GT_DECOMP(0x088995CCu, gt::game::func_088995CC, gt::ReturnKind::Void);
GT_DECOMP(0x08899564u, gt::game::func_08899564, gt::ReturnKind::Word);
GT_DECOMP(0x08899410u, gt::game::func_08899410, gt::ReturnKind::Float);
GT_DECOMP(0x088994F8u, gt::game::func_088994F8, gt::ReturnKind::Void);
GT_DECOMP(0x08AFD748u, gt::game::func_08AFD748, gt::ReturnKind::Void);
GT_DECOMP(0x08AF1490u, gt::game::func_08AF1490, gt::ReturnKind::Void);
GT_DECOMP(0x08968850u, gt::game::func_08968850, gt::ReturnKind::Void);
GT_DECOMP(0x088FBDC0u, gt::game::func_088FBDC0, gt::ReturnKind::Word);
GT_DECOMP(0x088FC0D4u, gt::game::func_088FC0D4, gt::ReturnKind::Word);
GT_DECOMP(0x08A77444u, gt::game::func_08A77444, gt::ReturnKind::Word);
GT_DECOMP(0x088DF324u, gt::game::func_088DF324, gt::ReturnKind::Word);
GT_DECOMP(0x08898804u, gt::game::func_08898804, gt::ReturnKind::Void);
GT_DECOMP(0x088F5EA0u, gt::game::func_088F5EA0, gt::ReturnKind::Void);
GT_DECOMP(0x088F1D30u, gt::game::func_088F1D30, gt::ReturnKind::Word);
GT_DECOMP(0x089C87A8u, gt::game::func_089C87A8, gt::ReturnKind::Word);
GT_DECOMP(0x088B5E68u, gt::game::func_088B5E68, gt::ReturnKind::Word);
GT_DECOMP(0x08A61DD4u, gt::game::func_08A61DD4, gt::ReturnKind::Void);
GT_DECOMP(0x08AD4F1Cu, gt::game::func_08AD4F1C, gt::ReturnKind::Void);
GT_DECOMP(0x08A873C0u, gt::game::func_08A873C0, gt::ReturnKind::Word);
// func_08B0C650 reaches kernel imports the isolated run can't replay (not comparable in all 42 cases),
// so the gate can't check it.
// GT_DECOMP(0x08B0C650u, gt::game::func_08B0C650, gt::ReturnKind::Word);
GT_DECOMP(0x08A101F8u, gt::game::func_08A101F8, gt::ReturnKind::Word);
GT_DECOMP(0x08AF7618u, gt::game::func_08AF7618, gt::ReturnKind::Void);
GT_DECOMP(0x08AFA60Cu, gt::game::func_08AFA60C, gt::ReturnKind::Word);
GT_DECOMP(0x08B17E84u, gt::game::func_08B17E84, gt::ReturnKind::Void);
GT_DECOMP(0x08A41258u, gt::game::func_08A41258, gt::ReturnKind::Word);
GT_DECOMP(0x089364ACu, gt::game::func_089364AC, gt::ReturnKind::DoubleWord);
GT_DECOMP(0x08A78168u, gt::game::func_08A78168, gt::ReturnKind::Word);
// func_08B180F4 reaches kernel imports the isolated run can't replay (not comparable in all 33 cases),
// so the gate can't check it.
// GT_DECOMP(0x08B180F4u, gt::game::func_08B180F4, gt::ReturnKind::Word);
GT_DECOMP(0x08ACD284u, gt::game::func_08ACD284, gt::ReturnKind::Word);
GT_DECOMP(0x08B121BCu, gt::game::func_08B121BC, gt::ReturnKind::Word);
GT_DECOMP(0x089660F4u, gt::game::func_089660F4, gt::ReturnKind::Word);
GT_DECOMP(0x08A9212Cu, gt::game::func_08A9212C, gt::ReturnKind::Word);

// --- batch 40 ---

namespace gt::game {
namespace {

constexpr std::uint32_t kEmptyString = 0x08D74FDCu;  // the shared empty representation strings start from

// A function-local static whose constructor is inline: the vtable at +0x0 and the word (or byte, for
// `byte_flag`) at +0x4 = `field`; its destructor `dtor` registered at exit; returns &object.
void static_value(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t guard, std::uint32_t object,
                  std::uint32_t vtable, std::uint32_t field, bool byte_flag, std::uint32_t dtor) {
    ctx.gpr[3] = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(rt.memory().load8(guard))));
    ctx.gpr[4] = guard;
    if (ctx.gpr[3] == 0u) {
        GuestFrame frame(ctx, 0x10u);
        if (call_guest(rt, ctx, kGuardAcquire, {guard}) != 0u) {
            if (byte_flag) rt.memory().store8(object + 0x4u, static_cast<std::uint8_t>(field));
            else rt.memory().store32(object + 0x4u, field);
            rt.memory().store32(object, vtable);
            (void)call_guest(rt, ctx, kGuardRelease, {guard});
            (void)call_guest(rt, ctx, kAtExit, {dtor});
        }
    }
    ctx.gpr[2] = object;
}

// `wrap(this, &object)` for a new `size`-byte object (aligned to 4) built by `construct(object, x, y, z)`.
void wrap_new3(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t size, std::uint32_t construct,
               std::uint32_t wrap) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t x = ctx.gpr[5], y = ctx.gpr[6], z = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t object = call_guest(rt, ctx, kAllocateAligned4, {size, 4u});
    (void)call_guest(rt, ctx, construct, {object, x, y, z});
    rt.memory().store32(frame.at(0u), object);
    ctx.gpr[2] = call_guest(rt, ctx, wrap, {self, frame.at(0u)});
}

// out = a value `wrap(&t, &object)` builds around a new `size`-byte object `construct(object, arg)` made.
void store_new(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t out, std::uint32_t size,
               std::uint32_t construct, std::uint32_t arg, std::uint32_t wrap) {
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t object = call_guest(rt, ctx, kAllocateAligned4, {size, 4u});
    (void)call_guest(rt, ctx, construct, {object, arg});
    rt.memory().store32(frame.at(0u), object);
    (void)call_guest(rt, ctx, wrap, {frame.at(4u), frame.at(0u)});
    if (out != frame.at(4u)) (void)call_guest(rt, ctx, kAssignSmart, {out, word(rt, frame.at(4u))});
    ctx.gpr[2] = call_guest(rt, ctx, kReleaseLocal, {frame.at(4u)});
}

// func_08B55AAC(5, the signed byte +0x150 is 0) and (6, +0x151 is 0), unless both are 0.
void apply_two_flags(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object) {
    const auto first = static_cast<std::int32_t>(static_cast<std::int8_t>(rt.memory().load8(object + 0x150u)));
    const auto second = static_cast<std::int32_t>(static_cast<std::int8_t>(rt.memory().load8(object + 0x151u)));
    if ((first | second) == 0) return;
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08B55AACu, {5u, first == 0 ? 1u : 0u});
    }
    tail_call(ctx, 0x08B55AACu, {6u, second == 0 ? 1u : 0u});
}

} // namespace

// Function-local statics with inline constructors: func_08916474, func_088CA548, func_088C7F64 (vtable,
// +0x4 = 0) and func_08B200F0, func_08B1F028 (vtable, the byte +0x4 = 1).
void func_08916474(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { static_value(rt, ctx, 0x08CF2108u, 0x08CF2110u, 0x08CC5840u, 0u, false, 0x08916A10u); }
void func_088CA548(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { static_value(rt, ctx, 0x08CE6DF0u, 0x08CE6DF8u, 0x08CC3488u, 0u, false, 0x088CA654u); }
void func_088C7F64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { static_value(rt, ctx, 0x08CE3BB0u, 0x08CE3BB8u, 0x08CC3438u, 0u, false, 0x088C8940u); }
void func_08B200F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { static_value(rt, ctx, 0x08D68B40u, 0x08D68B48u, 0x08CD94A8u, 1u, true, 0x08B20678u); }
void func_08B1F028(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { static_value(rt, ctx, 0x08D68B30u, 0x08D68B38u, 0x08CD9438u, 1u, true, 0x08B1F118u); }

// func_08B2A868(bits, start) -> the first set bit at or after start (bits are MSB-first in each word;
// count at +0x0, words at +0x4), or -1.
void func_08B2A868(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const auto count = static_cast<std::int32_t>(word(rt, ctx.gpr[4]));
    const std::uint32_t start = ctx.gpr[5];
    std::uint32_t at = word(rt, ctx.gpr[4] + 0x4u) + static_cast<std::uint32_t>((static_cast<std::int32_t>(start) >> 5) << 2);
    std::uint32_t mask = (0xFFFFFFFFu << (start & 31u)) >> (start & 31u);
    auto base = static_cast<std::int32_t>(start & ~31u);
    ctx.gpr[2] = 0xFFFFFFFFu;
    while (base < count) {
        const auto zeros = static_cast<std::uint32_t>(std::countl_zero(word(rt, at) & mask));
        at += 4u;
        if (zeros < 32u) {
            const auto index = base + static_cast<std::int32_t>(zeros);
            ctx.gpr[2] = index < count ? static_cast<std::uint32_t>(index) : 0xFFFFFFFFu;
            return;
        }
        base += 32;
        mask = 0xFFFFFFFFu;
    }
}

// func_08A125D0(this, x): when the virtual +0x20 gives r: func_08B52578(r's +0x18), then x's two flags
// (as func_089C4B24).
void func_08A125D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t x = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, word(rt, word(rt, self) + 0x20u), {self, x});
        if (ctx.gpr[2] == 0u) return;
        (void)call_guest(rt, ctx, 0x08B52578u, {word(rt, ctx.gpr[2] + 0x18u)});
    }
    apply_two_flags(rt, ctx, x);
}

// func_089C4B24(this): func_08AFA5E4(this + 0x188, 0, 0), then this's two flags.
void func_089C4B24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08AFA5E4u, {self + 0x188u, 0u, 0u});
    }
    apply_two_flags(rt, ctx, self);
}

// func_089D9D2C(this) -> func_089D9B90(this) (0 when bit 19 of +0x60 is set); 1 unless bit 18 is set, in
// which case the virtual +0x1C4 (this, 1) runs first.
void func_089D9D2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    std::uint32_t result = 0u;
    if ((word(rt, self + 0x60u) & 0x80000u) == 0u) result = call_guest(rt, ctx, 0x089D9B90u, {self});
    if ((word(rt, self + 0x60u) & 0x40000u) == 0u) {
        ctx.gpr[2] = 1u;
        return;
    }
    (void)call_guest(rt, ctx, word(rt, word(rt, self) + 0x1C4u), {self, 1u});
    ctx.gpr[2] = result;
}

// func_08911044(this): a three-step smoother: the value at +0x0 moves +0x4 -> +0x8 (blended by +0x10 once
// warmed up), +0xC keeps the previous +0x8, and the step count at +0x14 saturates at 2.
void func_08911044(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t steps = rt.memory().load8(self + 0x14u);
    const float value = float_at(rt, self);
    if (steps < 2u) {
        rt.memory().store32(self + 0x8u, std::bit_cast<std::uint32_t>(value));
        rt.memory().store32(self + 0xCu, std::bit_cast<std::uint32_t>(value));
    } else {
        const float previous = float_at(rt, self + 0x4u);
        rt.memory().store32(self + 0xCu, word(rt, self + 0x8u));
        const float blended = previous + psp_mul(value - previous, float_at(rt, self + 0x10u));
        rt.memory().store32(self + 0x8u, std::bit_cast<std::uint32_t>(blended));
    }
    rt.memory().store32(self + 0x4u, std::bit_cast<std::uint32_t>(value));
    const std::uint32_t next = steps + 1u;
    ctx.gpr[3] = next < 3u ? next : 2u;
    rt.memory().store8(self + 0x14u, static_cast<std::uint8_t>(ctx.gpr[3]));
    ctx.gpr[4] = next;
    ctx.gpr[5] = self;
}

// func_08AF83AC(): the 2D render states — func_08B54FB8(0x8002 / 111 / 2), func_08B58874(112 / 106),
// func_08B584D0(1024, 774, 775), func_08B5877C(6, 0.251), func_08B587A8(5), func_08B58A74(1),
// func_08B58AD0(1, 1, 1, 1). 0x3E008081 is 32/255.
void func_08AF83AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    {
        GuestFrame frame(ctx, 0x10u);
        for (const std::uint32_t state : {0x8002u, 111u, 2u}) (void)call_guest(rt, ctx, 0x08B54FB8u, {state});
        for (const std::uint32_t state : {112u, 106u}) (void)call_guest(rt, ctx, 0x08B58874u, {state});
        (void)call_guest(rt, ctx, 0x08B584D0u, {1024u, 774u, 775u});
        (void)call_guest(rt, ctx, 0x08B5877Cu, {6u}, {std::bit_cast<float>(0x3E008081u)});
        (void)call_guest(rt, ctx, 0x08B587A8u, {5u});
        ctx.gpr[2] = call_guest(rt, ctx, 0x08B58A74u, {1u});
    }
    tail_call(ctx, 0x08B58AD0u, {1u, 1u, 1u, 1u});
}

// func_08AF6EA8(this, name) -> the index of the entry (152 bytes, name at +0x94, count at +0x14) whose name
// func_08B35FD8 matches, or -1.
void func_08AF6EA8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t name = ctx.gpr[5];
    const auto count = static_cast<std::int32_t>(rt.memory().load16(self + 0x14u));
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[2] = 0xFFFFFFFFu;
    const std::uint32_t entries = word(rt, self + 0x18u);
    for (std::int32_t i = 0; i < count; ++i) {
        if (call_guest(rt, ctx, 0x08B35FD8u, {name, word(rt, entries + static_cast<std::uint32_t>(i) * 152u + 0x94u)}) == 0u) {
            ctx.gpr[2] = static_cast<std::uint32_t>(i);
            return;
        }
    }
}

// func_08988F44(this, x): x's virtual +0x110 (x, +0x1E4), func_08983F74(this + 0xF0, x),
// func_089A9478(x, this + 0x1F4), func_08983440(this + 0xF0), then x's virtual +0x114.
void func_08988F44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t x = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, word(rt, word(rt, x) + 0x110u), {x, word(rt, self + 0x1E4u)});
        (void)call_guest(rt, ctx, 0x08983F74u, {self + 0xF0u, x});
        (void)call_guest(rt, ctx, 0x089A9478u, {x, self + 0x1F4u});
        ctx.gpr[2] = call_guest(rt, ctx, 0x08983440u, {self + 0xF0u});
    }
    ctx.gpr[2] = word(rt, x);
    tail_call(ctx, word(rt, ctx.gpr[2] + 0x114u), {x});
}

// func_08ABFF20(this, fn): fn(entry's +0x0) for each node of the list at +0x4 (next +0x8), with the mutex
// at +0xC released around each call.
void func_08ABFF20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t fn = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x20u);
        (void)call_guest(rt, ctx, kLock, {self + 0xCu});
        for (std::uint32_t node = word(rt, self + 0x4u); node != 0u;) {
            const std::uint32_t value = word(rt, node);
            const std::uint32_t next = word(rt, node + 0x8u);
            (void)call_guest(rt, ctx, kUnlock, {self + 0xCu});
            (void)call_guest(rt, ctx, fn, {value});
            (void)call_guest(rt, ctx, kLock, {self + 0xCu});
            node = next;
        }
    }
    tail_call(ctx, kUnlock, {self + 0xCu});
}

// func_08AF7214(this): func_08AF7148(entry, &material entry's index (12 bytes, +0x2C)) for each 28-byte
// entry at +0x38 (count +0x34).
void func_08AF7214(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    for (std::int32_t i = 0; i < static_cast<std::int32_t>(rt.memory().load16(self + 0x34u)); ++i) {
        const std::uint32_t entry = word(rt, self + 0x38u) + static_cast<std::uint32_t>(i) * 28u;
        const std::uint32_t index = rt.memory().load16(entry + 0x6u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08AF7148u, {entry, word(rt, self + 0x2Cu) + index * 12u});
    }
}

// func_08AD1A3C(this, x): func_08B4B114(x, &local, 0, 0, 0), func_08B4E9D4(current entry, &local), then
// func_08B4E994(this, this + 0x40, current entry) (80-byte entries at +0x80, index +0x1C0); that last
// callee leaves v0 alone, so the original returns index << 6.
void func_08AD1A3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x50u);
    (void)call_guest(rt, ctx, 0x08B4B114u, {ctx.gpr[5], frame.at(0u)}, {0.0f, 0.0f, 0.0f});
    (void)call_guest(rt, ctx, 0x08B4E9D4u, {self + word(rt, self + 0x1C0u) * 80u + 0x80u, frame.at(0u)});
    ctx.gpr[2] = word(rt, self + 0x1C0u) << 6;  // the index scaling the original leaves in v0
    ctx.gpr[2] = call_guest(rt, ctx, 0x08B4E994u, {self, self + 0x40u, self + word(rt, self + 0x1C0u) * 80u + 0x80u});
}

// func_088E1B90(this, i) -> whether entry i (760 bytes, +0xC0) is ready: with +0x1FC below -1, both bytes
// +0x14 / +0x15 set; otherwise func_088E1ADC(this, i) failing.
void func_088E1B90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t i = ctx.gpr[5];
    const std::uint32_t entry = word(rt, self + 0xC0u) + i * 760u;
    std::uint32_t result;
    if (static_cast<std::int32_t>(word(rt, entry + 0x1FCu)) < -1) {
        result = rt.memory().load8(entry + 0x14u) != 0u && rt.memory().load8(entry + 0x15u) != 0u ? 1u : 0u;
    } else {
        GuestFrame frame(ctx, 0x10u);
        result = call_guest(rt, ctx, 0x088E1ADCu, {self, i, i << 5, entry}) == 0u ? 1u : 0u;
    }
    ctx.gpr[2] = result;
}

// func_088BB704(this, x) -> the curve value at x: +0x24 at or below 0, +0x28 above 125 or where x * +0x20
// reaches 1, else lerp(+0x24, +0x28, x * +0x20).
void func_088BB704(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const float x = ctx.fpr[12];
    if (x <= 0.0f) {
        ctx.fpr[0] = float_at(rt, self + 0x24u);
        return;
    }
    if (!(x <= std::bit_cast<float>(0x42FA0001u))) {
        ctx.fpr[0] = float_at(rt, self + 0x28u);
        return;
    }
    const float t = psp_mul(x, float_at(rt, self + 0x20u));
    ctx.fpr[12] = t;
    if (1.0f <= t) {
        ctx.fpr[0] = float_at(rt, self + 0x28u);
        return;
    }
    const float low = float_at(rt, self + 0x24u);
    ctx.fpr[1] = low;
    ctx.fpr[0] = low + psp_mul(t, float_at(rt, self + 0x28u) - low);
}

// func_089DAEA0(this, i, x, y, dx, dy): writes the point pair (x, y), (x + dx, y + dy) at i of the current
// buffer (bit 7 of +0x140 picks +0xEC or +0xF8).
void func_089DAEA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t i = ctx.gpr[5];
    const auto buffer = [&] { return word(rt, self + 0xECu + ((word(rt, self + 0x140u) >> 7) & 1u) * 12u); };
    const float x0 = ctx.fpr[12] + 0.0f, y0 = ctx.fpr[13] + 0.0f;
    std::uint32_t at = buffer() + (i << 3);
    rt.memory().store32(at + 0x4u, std::bit_cast<std::uint32_t>(y0));
    rt.memory().store32(at, std::bit_cast<std::uint32_t>(x0));
    const float x1 = ctx.fpr[12] + ctx.fpr[14], y1 = ctx.fpr[13] + ctx.fpr[15];
    at = buffer() + ((i + 1u) << 3);
    rt.memory().store32(at + 0x4u, std::bit_cast<std::uint32_t>(y1));
    rt.memory().store32(at, std::bit_cast<std::uint32_t>(x1));
    ctx.fpr[0] = x0;
    ctx.fpr[1] = y0;
    ctx.fpr[12] = x1;
    ctx.fpr[13] = y1;
}

// func_08AFD06C(this): the object at +0x0 gets +0x4 / +0x8 and its virtual +0x34 twice — (0, 0, +0x4,
// +0x8) then (+0xC, +0x10, +0x14, +0x18).
void func_08AFD06C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t object = word(rt, self);
    if (object == 0u) return;
    rt.memory().store32(object + 0x4u, word(rt, self + 0x4u));
    rt.memory().store32(object + 0x8u, word(rt, self + 0x8u));
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, word(rt, word(rt, object) + 0x34u), {object, 0u, 0u, word(rt, self + 0x4u), word(rt, self + 0x8u)});
    }
    const std::uint32_t again = word(rt, self);
    ctx.gpr[2] = word(rt, again);
    tail_call(ctx, word(rt, ctx.gpr[2] + 0x34u),
              {again, word(rt, self + 0xCu), word(rt, self + 0x10u), word(rt, self + 0x14u), word(rt, self + 0x18u)});
}

// func_08AFA0F0(this, a, b): func_08AF8904(this, a, b), func_08AF9E88(this, i, a, b) for each of the +0x10
// items, then func_08AF8434(this).
void func_08AFA0F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t a = ctx.gpr[5], b = ctx.gpr[6];
    {
        GuestFrame frame(ctx, 0x20u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08AF8904u, {self, a, b});
        for (std::uint32_t i = 0; i < rt.memory().load16(self + 0x10u); ++i) ctx.gpr[2] = call_guest(rt, ctx, 0x08AF9E88u, {self, i, a, b});
    }
    tail_call(ctx, 0x08AF8434u, {self});
}

// func_089687D8(this, flag): func_08965DDC(this, flag) (twice when bit 12 of +0xE0 is set), then down the
// chain func_08965CE0 gives (as func_08968850).
void func_089687D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t flag = ctx.gpr[5] & 0xFFu;
    std::uint32_t next;
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08965DDCu, {self, flag});
        if ((word(rt, self + 0xE0u) & 0x1000u) == 0u) return;
        (void)call_guest(rt, ctx, 0x08965DDCu, {self, flag});
        next = call_guest(rt, ctx, 0x08965CE0u, {self});
    }
    ctx.gpr[2] = next;
    if (next != 0u) tail_call(ctx, 0x089687D8u, {next, flag});
}

// func_08ABEEE0(this): func_08AD4050 on every entry of the set at +0x144 (func_08ABF868 first,
// func_08ABF6F0 next) without bit 15 of its +0x4, then func_08ABED84(this).
void func_08ABEEE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        for (std::uint32_t entry = call_guest(rt, ctx, 0x08ABF868u, {self + 0x144u}); entry != 0u;
             entry = call_guest(rt, ctx, 0x08ABF6F0u, {self + 0x144u, entry}))
            if (((word(rt, entry + 0x4u) >> 15) & 1u) == 0u) (void)call_guest(rt, ctx, 0x08AD4050u, {entry});
    }
    tail_call(ctx, 0x08ABED84u, {self});
}

// func_088D0F98(a, b, wrap) -> (whole steps, remainder) of b - a for (int, float) pairs, borrowing one
// `wrap` when the fraction has the other sign.
void func_088D0F98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = ctx.gpr[4], b = ctx.gpr[5];
    auto steps = static_cast<std::int32_t>(word(rt, b) - word(rt, a));
    float fraction = float_at(rt, b + 0x4u) - float_at(rt, a + 0x4u);
    if (steps > 0) {
        if (!(0.0f <= fraction)) {
            fraction = fraction + ctx.fpr[12];
            --steps;
        }
    } else if (steps < 0) {
        if (0.0f < fraction) {
            fraction = fraction - ctx.fpr[12];
            ++steps;
        }
    }
    ctx.gpr[2] = static_cast<std::uint32_t>(steps);
    ctx.gpr[3] = std::bit_cast<std::uint32_t>(fraction);
}

// func_088FC13C(this, x): as func_088FC0D4, ending in func_0890638C(r + 0x34, x).
void func_088FC13C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const float x = ctx.fpr[12];
    std::uint32_t r;
    {
        GuestFrame frame(ctx, 0x20u);
        r = call_guest(rt, ctx, word(rt, word(rt, self) + 0x34u), {self});
        ctx.gpr[2] = r;
        if (r == 0u) return;
        ctx.gpr[2] = word(rt, word(rt, r + 0x4u) + 0x2Cu);
        if (ctx.gpr[2] != 0u) return;
        ctx.gpr[2] = call_guest(rt, ctx, 0x08906398u, {r + 0x34u});
    }
    ctx.fpr[12] = x;
    if (ctx.gpr[2] != 0u) tail_call(ctx, 0x0890638Cu, {r + 0x34u});
}

// func_08AD10F4(this, x): func_08B4BA68(x, this + 0x4C, &{0, 0, -1}) and (x, this + 0x58, &{0, 1, 0}).
void func_08AD10F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t x = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    rt.memory().store32(frame.at(0x8u), std::bit_cast<std::uint32_t>(-1.0f));
    rt.memory().store32(frame.at(0x10u), std::bit_cast<std::uint32_t>(1.0f));
    for (const std::uint32_t offset : {0x0u, 0x4u, 0xCu, 0x14u}) rt.memory().store32(frame.at(offset), 0u);
    (void)call_guest(rt, ctx, 0x08B4BA68u, {x, self + 0x4Cu, frame.at(0u)});
    ctx.gpr[2] = call_guest(rt, ctx, 0x08B4BA68u, {x, self + 0x58u, frame.at(0xCu)});
}

// func_08A7C5C8(this, p) -> this's virtual +0x110 (this, func_08A5D444(*p), p).
void func_08A7C5C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t p = ctx.gpr[5];
    const std::uint32_t method = word(rt, word(rt, self) + 0x110u);
    const std::uint32_t object = word(rt, p);
    std::uint32_t value;
    {
        GuestFrame frame(ctx, 0x20u);
        if (object == 0u) (void)call_guest(rt, ctx, 0x08A5BB7Cu, {0x08CB1358u});
        value = call_guest(rt, ctx, 0x08A5D444u, {object});
    }
    ctx.gpr[2] = value;
    tail_call(ctx, method, {self, value, p});
}

// Values stored through a new object: func_08A86C7C(x, out) (24 bytes, func_08A86FA8(object, x),
// func_08A87328), func_0898D52C(out) (512, func_0898D484, func_08969190), func_08A78964(out) (20,
// func_08A76B04, func_08A76AAC).
void func_08A86C7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_new(rt, ctx, ctx.gpr[5], 24u, 0x08A86FA8u, ctx.gpr[4], 0x08A87328u); }
void func_0898D52C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_new(rt, ctx, ctx.gpr[4], 512u, 0x0898D484u, 4u, 0x08969190u); }
void func_08A78964(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_new(rt, ctx, ctx.gpr[4], 20u, 0x08A76B04u, 4u, 0x08A76AACu); }

// Base-class constructor / destructor pairs of the display tree: func_089BB804 (constructor),
// func_089BBAC8 (destructor), func_089BD168 (members released).
void func_089BB804(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    (void)call_guest(rt, ctx, 0x08A7E88Cu, {self, 0x08CF6FDCu});
    rt.memory().store32(self, 0x08CB8F58u);
    rt.memory().store32(self + 0x38u, 0x08CB90F4u);
    for (std::uint32_t offset = 0x3Cu; offset <= 0x54u; offset += 4u) rt.memory().store32(self + offset, 0u);
    (void)call_guest(rt, ctx, 0x08A696E4u, {self + 0x58u});
    rt.memory().store32(self + 0x5Cu, 0u);
    ctx.gpr[2] = word(rt, self + 0x60u) & ~0x3FFu;
    rt.memory().store32(self + 0x60u, ctx.gpr[2]);
}
void func_089BBAC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    rt.memory().store32(self, 0x08CB8F58u);
    rt.memory().store32(self + 0x38u, 0x08CB90F4u);
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08A69D68u, {self + 0x58u});
        for (const std::uint32_t member : {0x40u, 0x3Cu})
            if (const std::uint32_t object = word(rt, self + member); object != 0u) (void)call_guest(rt, ctx, 0x08A5B930u, {object});
        ctx.gpr[2] = call_guest(rt, ctx, 0x0897D548u, {self + 0x38u});
    }
    tail_call(ctx, 0x08A7D4E4u, {self});
}
void func_089BD168(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t block;
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08A7D398u, {self});
        if (const std::uint32_t object = word(rt, self + 0x40u); object != 0u) ctx.gpr[2] = call_guest(rt, ctx, 0x08A5B930u, {object});
        block = word(rt, self + 0x5Cu);
        rt.memory().store32(self + 0x40u, 0u);
        if (block == 0u) return;
        (void)call_guest(rt, ctx, 0x08BFEDA4u, {block, word(rt, block + 0x8u)});
    }
    tail_call(ctx, kFreeAligned, {block, 576u, 4u});
}

// func_08B030A8(this, compare): sorts the halfword array +0x2C..+0x30 (func_08C496A4 introsort to depth
// 2 * log2(n), then func_08C49140 insertion sort) with the comparator {compare, this}.
void func_08B030A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t begin = word(rt, ctx.gpr[4] + 0x2Cu);
    const std::uint32_t end = word(rt, ctx.gpr[4] + 0x30u);
    GuestFrame frame(ctx, 0x20u);
    rt.memory().store32(frame.at(4u), ctx.gpr[4]);
    rt.memory().store32(frame.at(0u), ctx.gpr[5]);
    if (begin == end) return;
    auto n = static_cast<std::int32_t>(end - begin) >> 1;
    std::uint32_t depth = 0u;
    if (n != 1) {
        do {
            n >>= 1;
            ++depth;
        } while (n != 1);
        depth <<= 1;
    }
    (void)call_guest(rt, ctx, 0x08C496A4u, {begin, end, depth, frame.at(0u)});
    ctx.gpr[2] = call_guest(rt, ctx, 0x08C49140u, {begin, end, frame.at(0u)});
}

// func_08955540(this) -> the signed state byte +0x732 of +0x8, or -1 when it is 1 but the record at +0x270
// of +0xC is busy (its byte +0x10 is 1, or func_0895BBA4 / func_0893EB40 say so).
void func_08955540(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const auto state = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(rt.memory().load8(word(rt, self + 0x8u) + 0x732u))));
    ctx.gpr[2] = state;
    if (state != 1u) return;
    const std::uint32_t record = word(rt, self + 0xCu) + 0x270u;
    GuestFrame frame(ctx, 0x10u);
    if (rt.memory().load8(record + 0x10u) != state && call_guest(rt, ctx, 0x0895BBA4u, {record}) == 0u &&
        call_guest(rt, ctx, 0x0893EB40u, {record}) == 0u) {
        ctx.gpr[2] = state;
        return;
    }
    ctx.gpr[2] = 0xFFFFFFFFu;
}

// func_089554C8(this) -> 82 ('R') without the byte +0x5F8 of +0x8, 68 ('D') when the record at +0x270 of
// +0xC is busy, else '0' + that byte.
void func_089554C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t gear = rt.memory().load8(word(rt, self + 0x8u) + 0x5F8u);
    ctx.gpr[2] = 82u;
    if (gear == 0u) return;
    const std::uint32_t record = word(rt, self + 0xCu) + 0x270u;
    GuestFrame frame(ctx, 0x10u);
    if (rt.memory().load8(record + 0x10u) != 1u && call_guest(rt, ctx, 0x0895BBA4u, {record}) == 0u &&
        call_guest(rt, ctx, 0x0893EB40u, {record}) == 0u) {
        ctx.gpr[2] = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(gear + 48u)));
        return;
    }
    ctx.gpr[2] = 68u;
}

// func_08917060(this, a, _, d): func_0890AEF0(this + 4, a's +0x1C), then func_0890B00C(this + 4, a's +0x4,
// a's +0x8, &{0x08CC58A8, d}, a's byte +0x28); the local is destroyed with func_08AF484C.
void func_08917060(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t a = ctx.gpr[5];
    const std::uint32_t d = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    (void)call_guest(rt, ctx, 0x0890AEF0u, {self + 0x4u, word(rt, a + 0x1Cu)});
    rt.memory().store32(frame.at(0u), 0x08CC58A8u);
    rt.memory().store32(frame.at(4u), d);
    (void)call_guest(rt, ctx, 0x0890B00Cu, {self + 0x4u, word(rt, a + 0x4u), word(rt, a + 0x8u), frame.at(0u), rt.memory().load8(a + 0x28u)});
    rt.memory().store32(frame.at(0u), 0x08CD7310u);
    ctx.gpr[2] = call_guest(rt, ctx, 0x08AF484Cu, {frame.at(0u)});
}

// func_088DAAF4(this) -> the 64-bit func_088D9C70(this) minus the start at +0x88 (0 when func_088DAA84
// fails or there is no start).
void func_088DAAF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    std::uint32_t low = 0u, high = 0u;
    if (call_guest(rt, ctx, 0x088DAA84u, {self}) != 0u && (word(rt, self + 0x88u) | word(rt, self + 0x8Cu)) != 0u) {
        const GuestResult now = call_guest(rt, ctx, 0x088D9C70u, {self}, {});
        const std::uint64_t start = (static_cast<std::uint64_t>(word(rt, self + 0x8Cu)) << 32) | word(rt, self + 0x88u);
        const std::uint64_t value = ((static_cast<std::uint64_t>(now.v1) << 32) | now.v0) - start;
        low = static_cast<std::uint32_t>(value);
        high = static_cast<std::uint32_t>(value >> 32);
    }
    ctx.gpr[2] = low;
    ctx.gpr[3] = high;
}

// func_088CC7B0(this, x): when func_088CB430(this, 0) holds, func_088CC114(func_088CB360(i), x, +0x10,
// +0x14) for i 0..3.
void func_088CC7B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t x = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    if (call_guest(rt, ctx, 0x088CB430u, {self, 0u}) == 0u) return;
    for (std::uint32_t i = 0; i < 4u; ++i) {
        const std::uint32_t entry = call_guest(rt, ctx, 0x088CB360u, {i});
        ctx.gpr[2] = call_guest(rt, ctx, 0x088CC114u, {entry, x, word(rt, self + 0x10u), word(rt, self + 0x14u)});
    }
}

// func_08A806D0(this, a, b, c) -> the virtual +0x38 (object, a, this + 0x14, b, c) of the object at +0x18;
// func_08A74ABC(this, p, b, c, d) -> func_08A62DC8(*p, this + 0x14, b, c, d).
void func_08A806D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t a = ctx.gpr[5], b = ctx.gpr[6], c = ctx.gpr[7];
    const std::uint32_t object = word(rt, self + 0x18u);
    if (object == 0u) {
        GuestFrame frame(ctx, 0x20u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08A5BB7Cu, {0x08CB1524u});
    }
    ctx.gpr[2] = word(rt, object);
    tail_call(ctx, word(rt, ctx.gpr[2] + 0x38u), {object, a, self + 0x14u, b, c});
}
void func_08A74ABC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t b = ctx.gpr[6], c = ctx.gpr[7], d = ctx.gpr[8];
    const std::uint32_t object = word(rt, ctx.gpr[5]);
    if (object == 0u) {
        GuestFrame frame(ctx, 0x20u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08A5BB7Cu, {0x08CB0C8Cu});
    }
    tail_call(ctx, 0x08A62DC8u, {object, self + 0x14u, b, c, d});
}

// func_08B2C248(this, key): under the mutex at this, drops one reference (bit 0 counted in steps of 2) of
// key's entry (func_08B2B9BC) and one from +0x4C.
void func_08B2C248(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, kLock, {self});
        const std::uint32_t index = call_guest(rt, ctx, 0x08B2B9BCu, {self, ctx.gpr[5]});
        if (static_cast<std::int32_t>(index) >= 0) {
            const std::uint32_t entry = word(rt, word(rt, self + 0x44u) + (index << 3) + 0x4u);
            const std::uint32_t refs = word(rt, entry);
            if ((refs & 1u) != 0u) {
                rt.memory().store32(entry, refs - 1u);
                rt.memory().store32(self + 0x4Cu, word(rt, self + 0x4Cu) - 1u);
            }
        }
    }
    tail_call(ctx, kUnlock, {self});
}

// func_08A81B74(out, source) -> out: assigned from the entry func_08C29370(source + 0x14, &key) finds for
// the key func_08A818A0(&key, source) gives, or cleared (func_08A76AA0) without one.
void func_08A81B74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t source = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    (void)call_guest(rt, ctx, 0x08A818A0u, {frame.at(0u), source});
    bool found = false;
    if (word(rt, frame.at(0u)) != 0u) {
        const std::uint32_t entry = call_guest(rt, ctx, 0x08C29370u, {source + 0x14u, frame.at(0u)});
        if (entry != source + 0x18u) {
            (void)call_guest(rt, ctx, kAssignValue, {out, entry + 0x14u});
            found = true;
        }
    }
    if (!found) (void)call_guest(rt, ctx, 0x08A76AA0u, {out});
    (void)call_guest(rt, ctx, kReleaseLocal, {frame.at(0u)});
    ctx.gpr[2] = out;
}

// func_08A8631C(this, out, _, p): out = func_08A85B1C(whether +0x14 is below *p's virtual +0x3C).
void func_08A8631C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t object = word(rt, ctx.gpr[7]);
    const auto value = static_cast<std::int32_t>(word(rt, self + 0x14u));
    std::uint32_t limit;
    {
        GuestFrame frame(ctx, 0x20u);
        if (object == 0u) (void)call_guest(rt, ctx, 0x08A5BB7Cu, {0x08CB181Cu});
        limit = call_guest(rt, ctx, word(rt, word(rt, object) + 0x3Cu), {object});
    }
    store_built(rt, ctx, 0x08A85B1Cu, value < static_cast<std::int32_t>(limit) ? 1u : 0u);
}

// func_08A0FDD8(this): base func_089BB804, two vtables, flags at +0x60 (bit 16 set, bits 17..23 and
// 24..27 cleared), the four values +0x64..+0x70 emptied (func_08A76AA0), +0x74 cleared.
void func_08A0FDD8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    (void)call_guest(rt, ctx, 0x089BB804u, {self});
    const std::uint32_t flags = ((word(rt, self + 0x60u) | 0x10000u) & ~0x00FE0000u) & ~0x0F000000u;
    rt.memory().store32(self, 0x08CB9110u);
    rt.memory().store32(self + 0x38u, 0x08CB92DCu);
    rt.memory().store32(self + 0x60u, flags);
    for (const std::uint32_t member : {0x64u, 0x68u, 0x6Cu, 0x70u}) ctx.gpr[2] = call_guest(rt, ctx, 0x08A76AA0u, {self + member});
    rt.memory().store32(self + 0x74u, 0u);
}

// func_08A0F560(this, other): func_089BD1E0(this, other), then other's flag bits 16, 17, 20..23 and 24..27
// (from +0x62, +0x60 and +0x63).
void func_08A0F560(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t other = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x089BD1E0u, {self, other});
    }
    if (other == 0u) return;
    std::uint32_t flags = (word(rt, self + 0x60u) & ~0x10000u) | ((rt.memory().load16(other + 0x62u) & 1u) << 16);
    rt.memory().store32(self + 0x60u, flags);
    flags = (flags & ~0x20000u) | (((word(rt, other + 0x60u) >> 17) & 1u) << 17);
    rt.memory().store32(self + 0x60u, flags);
    flags = (flags & ~0x00F00000u) | (((word(rt, other + 0x60u) >> 20) & 15u) << 20);
    rt.memory().store32(self + 0x60u, flags);
    ctx.gpr[3] = rt.memory().load8(other + 0x63u);
    flags = (flags & ~0x0F000000u) | ((ctx.gpr[3] & 15u) << 24);
    rt.memory().store32(self + 0x60u, flags);
    ctx.gpr[2] = flags;
}

// func_089D8E2C(this): base func_08A0FDD8, two vtables, an empty string at +0x78, the rest cleared;
// func_089D8974(this, other): func_08A0F560, then other's string (func_08B855E0) and fields +0x7C..+0x8D.
void func_089D8E2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08A0FDD8u, {self});
    }
    rt.memory().store32(self, 0x08CBCDE0u);
    rt.memory().store32(self + 0x38u, 0x08CBCFCCu);
    rt.memory().store32(self + 0x78u, kEmptyString);
    rt.memory().store32(self + 0xA4u, 0u);
    for (std::uint32_t offset = 0x7Cu; offset < 0x8Cu; offset += 4u) rt.memory().store32(self + offset, 0u);
    rt.memory().store8(self + 0x8Cu, 0u);
    rt.memory().store8(self + 0x8Du, 0u);
    for (std::uint32_t offset = 0x90u; offset < 0xA4u; offset += 4u) rt.memory().store32(self + offset, 0u);
    ctx.gpr[2] = kEmptyString;
    ctx.gpr[3] = 0x08CBCFCCu;
}
void func_089D8974(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t other = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08A0F560u, {self, other});
        if (other == 0u) return;
        (void)call_guest(rt, ctx, 0x08B855E0u, {self + 0x78u, other + 0x78u});
    }
    rt.memory().store8(self + 0x8Du, rt.memory().load8(other + 0x8Du));
    for (std::uint32_t offset = 0x7Cu; offset < 0x8Cu; offset += 4u) rt.memory().store32(self + offset, word(rt, other + offset));
    rt.memory().store8(self + 0x8Cu, rt.memory().load8(other + 0x8Cu));
}

// func_08AF72A4(this): refreshes the cached handle +0x1C (func_08B527B4 / func_08B52814(0x88EB, +0x4,
// +0x0)) unless already valid (bit 7 of +0x8); 0 without +0x0.
void func_08AF72A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t source = word(rt, self);
    if (source == 0u) {
        rt.memory().store32(self + 0x1Cu, 0u);
        return;
    }
    ctx.gpr[2] = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(rt.memory().load8(self + 0x8u))));
    if (static_cast<std::int32_t>(ctx.gpr[2]) < 0) return;
    GuestFrame frame(ctx, 0x10u);
    if (rt.memory().load8(self + 0xBu) == 0u) {
        ctx.gpr[2] = call_guest(rt, ctx, 0x08B527B4u, {0x88EBu, word(rt, self + 0x4u), source});
        rt.memory().store32(self + 0x1Cu, ctx.gpr[2]);
        rt.memory().store8(self + 0x8u, static_cast<std::uint8_t>(rt.memory().load8(self + 0x8u) | 0x80u));
    } else {
        ctx.gpr[2] = call_guest(rt, ctx, 0x08B52814u, {0x88EBu, word(rt, self + 0x4u), source});
        rt.memory().store32(self + 0x1Cu, ctx.gpr[2]);
    }
}

// New objects (x, y, z) handed to a wrapper: func_08A7B0E8 (32 bytes, func_08A7B08C, func_08A5D190),
// func_08A84910 (32, func_08A845C8, func_08A5D190), func_08A758BC (28, func_08A75738, func_08A982BC).
void func_08A7B0E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { wrap_new3(rt, ctx, 32u, 0x08A7B08Cu, 0x08A5D190u); }
void func_08A84910(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { wrap_new3(rt, ctx, 32u, 0x08A845C8u, 0x08A5D190u); }
void func_08A758BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { wrap_new3(rt, ctx, 28u, 0x08A75738u, 0x08A982BCu); }

// func_08AB50FC(this, x): base func_08A5B828(this, x), the vtable, two empty strings (+0x28 / +0x2C), the
// flag +0x38 = 1 and everything else cleared.
void func_08AB50FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08A5B828u, {self, ctx.gpr[5]});
    }
    rt.memory().store32(self, 0x08CD5798u);
    for (std::uint32_t offset = 0xCu; offset <= 0x24u; offset += 4u) rt.memory().store32(self + offset, 0u);
    rt.memory().store32(self + 0x2Cu, kEmptyString);
    rt.memory().store8(self + 0x38u, 1u);
    rt.memory().store8(self + 0x39u, 0u);
    rt.memory().store32(self + 0x28u, kEmptyString);
    rt.memory().store32(self + 0x30u, 0u);
    rt.memory().store32(self + 0x34u, 0u);
    ctx.gpr[2] = 1u;
    ctx.gpr[3] = kEmptyString;
}

// func_0896B0E4(this, x): func_089BA654(this, x); with the owner func_089BD4DC gives: its +0x14C = x,
// func_0896AD58(this) and func_08965B24(+0xD0, x, this) when set; then marks +0xE0 (bit 20) / +0xE4 (bit 2).
void func_0896B0E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t x = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x089BA654u, {self, x});
        const std::uint32_t owner = call_guest(rt, ctx, 0x089BD4DCu, {self});
        if (owner != 0u) {
            rt.memory().store32(owner + 0x14Cu, x);
            (void)call_guest(rt, ctx, 0x0896AD58u, {self});
            if (const std::uint32_t object = word(rt, self + 0xD0u); object != 0u) (void)call_guest(rt, ctx, 0x08965B24u, {object, x, self});
        }
    }
    ctx.gpr[2] = word(rt, self + 0xE4u) | 4u;
    ctx.gpr[3] = word(rt, self + 0xE0u) | 0x100000u;
    rt.memory().store32(self + 0xE0u, ctx.gpr[3]);
    rt.memory().store32(self + 0xE4u, ctx.gpr[2]);
}

// func_08966DA4(this, float *a, float *b) -> the floats +0x8 / +0xC of +0x80 when func_08966174 allows,
// else 0.
void func_08966DA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t a = ctx.gpr[5], b = ctx.gpr[6];
    std::uint32_t allowed;
    {
        GuestFrame frame(ctx, 0x10u);
        allowed = call_guest(rt, ctx, 0x08966174u, {self});
    }
    ctx.gpr[2] = allowed;
    if (allowed != 0u) {
        if (a != 0u) rt.memory().store32(a, word(rt, word(rt, self + 0x80u) + 0x8u));
        if (b != 0u) rt.memory().store32(b, word(rt, word(rt, self + 0x80u) + 0xCu));
    } else {
        if (a != 0u) rt.memory().store32(a, 0u);
        if (b != 0u) rt.memory().store32(b, 0u);
    }
}

} // namespace gt::game

GT_DECOMP(0x08916474u, gt::game::func_08916474, gt::ReturnKind::Word);
GT_DECOMP(0x088CA548u, gt::game::func_088CA548, gt::ReturnKind::Word);
GT_DECOMP(0x088C7F64u, gt::game::func_088C7F64, gt::ReturnKind::Word);
GT_DECOMP(0x08B200F0u, gt::game::func_08B200F0, gt::ReturnKind::Word);
GT_DECOMP(0x08B1F028u, gt::game::func_08B1F028, gt::ReturnKind::Word);
GT_DECOMP(0x08B2A868u, gt::game::func_08B2A868, gt::ReturnKind::Word);
GT_DECOMP(0x08A125D0u, gt::game::func_08A125D0, gt::ReturnKind::Void);
GT_DECOMP(0x089C4B24u, gt::game::func_089C4B24, gt::ReturnKind::Void);
GT_DECOMP(0x089D9D2Cu, gt::game::func_089D9D2C, gt::ReturnKind::Word);
GT_DECOMP(0x08911044u, gt::game::func_08911044, gt::ReturnKind::Void);
GT_DECOMP(0x08AF83ACu, gt::game::func_08AF83AC, gt::ReturnKind::Word);
GT_DECOMP(0x08AF6EA8u, gt::game::func_08AF6EA8, gt::ReturnKind::Word);
GT_DECOMP(0x08988F44u, gt::game::func_08988F44, gt::ReturnKind::Word);
GT_DECOMP(0x08ABFF20u, gt::game::func_08ABFF20, gt::ReturnKind::Word);
GT_DECOMP(0x08AF7214u, gt::game::func_08AF7214, gt::ReturnKind::Void);
GT_DECOMP(0x08AD1A3Cu, gt::game::func_08AD1A3C, gt::ReturnKind::Word);
GT_DECOMP(0x088E1B90u, gt::game::func_088E1B90, gt::ReturnKind::Word);
GT_DECOMP(0x088BB704u, gt::game::func_088BB704, gt::ReturnKind::Float);
GT_DECOMP(0x089DAEA0u, gt::game::func_089DAEA0, gt::ReturnKind::Void);
GT_DECOMP(0x08AFD06Cu, gt::game::func_08AFD06C, gt::ReturnKind::Void);
GT_DECOMP(0x08AFA0F0u, gt::game::func_08AFA0F0, gt::ReturnKind::Word);
GT_DECOMP(0x089687D8u, gt::game::func_089687D8, gt::ReturnKind::Void);
GT_DECOMP(0x08ABEEE0u, gt::game::func_08ABEEE0, gt::ReturnKind::Word);
GT_DECOMP(0x088D0F98u, gt::game::func_088D0F98, gt::ReturnKind::DoubleWord);
GT_DECOMP(0x088FC13Cu, gt::game::func_088FC13C, gt::ReturnKind::Word);
GT_DECOMP(0x08AD10F4u, gt::game::func_08AD10F4, gt::ReturnKind::Word);
GT_DECOMP(0x08A7C5C8u, gt::game::func_08A7C5C8, gt::ReturnKind::Word);
GT_DECOMP(0x08A86C7Cu, gt::game::func_08A86C7C, gt::ReturnKind::Void);
GT_DECOMP(0x0898D52Cu, gt::game::func_0898D52C, gt::ReturnKind::Void);
GT_DECOMP(0x08A78964u, gt::game::func_08A78964, gt::ReturnKind::Void);
GT_DECOMP(0x089BB804u, gt::game::func_089BB804, gt::ReturnKind::Word);
GT_DECOMP(0x089BBAC8u, gt::game::func_089BBAC8, gt::ReturnKind::Word);
GT_DECOMP(0x089BD168u, gt::game::func_089BD168, gt::ReturnKind::Word);
GT_DECOMP(0x08B030A8u, gt::game::func_08B030A8, gt::ReturnKind::Void);
GT_DECOMP(0x08955540u, gt::game::func_08955540, gt::ReturnKind::Word);
GT_DECOMP(0x089554C8u, gt::game::func_089554C8, gt::ReturnKind::Word);
GT_DECOMP(0x08917060u, gt::game::func_08917060, gt::ReturnKind::Word);
GT_DECOMP(0x088DAAF4u, gt::game::func_088DAAF4, gt::ReturnKind::DoubleWord);
GT_DECOMP(0x088CC7B0u, gt::game::func_088CC7B0, gt::ReturnKind::Void);
GT_DECOMP(0x08A806D0u, gt::game::func_08A806D0, gt::ReturnKind::Word);
GT_DECOMP(0x08A74ABCu, gt::game::func_08A74ABC, gt::ReturnKind::Word);
GT_DECOMP(0x08B2C248u, gt::game::func_08B2C248, gt::ReturnKind::Word);
GT_DECOMP(0x08A81B74u, gt::game::func_08A81B74, gt::ReturnKind::Word);
GT_DECOMP(0x08A8631Cu, gt::game::func_08A8631C, gt::ReturnKind::Void);
GT_DECOMP(0x08A0FDD8u, gt::game::func_08A0FDD8, gt::ReturnKind::Void);
GT_DECOMP(0x08A0F560u, gt::game::func_08A0F560, gt::ReturnKind::Void);
GT_DECOMP(0x089D8E2Cu, gt::game::func_089D8E2C, gt::ReturnKind::Word);
GT_DECOMP(0x089D8974u, gt::game::func_089D8974, gt::ReturnKind::Void);
GT_DECOMP(0x08AF72A4u, gt::game::func_08AF72A4, gt::ReturnKind::Void);
GT_DECOMP(0x08A7B0E8u, gt::game::func_08A7B0E8, gt::ReturnKind::Word);
GT_DECOMP(0x08A84910u, gt::game::func_08A84910, gt::ReturnKind::Word);
GT_DECOMP(0x08A758BCu, gt::game::func_08A758BC, gt::ReturnKind::Word);
GT_DECOMP(0x08AB50FCu, gt::game::func_08AB50FC, gt::ReturnKind::Word);
GT_DECOMP(0x0896B0E4u, gt::game::func_0896B0E4, gt::ReturnKind::Void);
GT_DECOMP(0x08966DA4u, gt::game::func_08966DA4, gt::ReturnKind::Void);

// --- batch 41 ---

namespace gt::game {
namespace {

constexpr std::uint32_t kEmptyStringRep = 0x08D74FD0u;  // the shared empty string representation (data at +0xC)

// Releases the string whose data pointer is `data` (reference count at -4, representation at -12):
// func_08B60804(&count, -1) and, on the last reference, func_08B8542C(rep, &scratch).
void release_string(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t data) {
    const std::uint32_t rep = data - 12u;
    if (rep == kEmptyStringRep) return;
    GuestFrame frame(ctx, 0x20u);
    if (static_cast<std::int32_t>(call_guest(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu})) <= 0)
        (void)call_guest(rt, ctx, 0x08B8542Cu, {rep, frame.at(0u)});
}

// As blend_by_progress, with the far value indexed (+ 4 * i) and the near value from `near_fn(this, i)`.
float blend_indexed(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t far, std::uint32_t near_fn,
                    std::uint32_t near) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t i = ctx.gpr[5];
    const float x = ctx.fpr[12];
    GuestFrame frame(ctx, 0x20u);
    const float t = call_guest(rt, ctx, 0x089559C4u, {self, i}, {}).f0 + x;
    const std::uint32_t record = call_guest(rt, ctx, 0x089551B0u, {self, i});
    const float far_value = float_at(rt, record + (i << 2) + far);
    const float near_value = float_at(rt, call_guest(rt, ctx, near_fn, {self, i}) + near);
    return psp_mul(t, far_value) + psp_mul(near_value, 1.0f - t);
}

// func_08AF7148-style walkers: `visit(entry)` for each 28-byte entry at +0x38 (count +0x34) whose kind
// byte is `kind` (all of them for a negative kind).
void for_each_entry_kind(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t visit) {
    const std::uint32_t self = ctx.gpr[4];
    const auto kind = static_cast<std::int32_t>(ctx.gpr[5]);
    GuestFrame frame(ctx, 0x20u);
    for (std::int32_t i = 0; i < static_cast<std::int32_t>(rt.memory().load16(self + 0x34u)); ++i) {
        const std::uint32_t entry = word(rt, self + 0x38u) + static_cast<std::uint32_t>(i) * 28u;
        if (kind < 0 || rt.memory().load8(entry) == static_cast<std::uint32_t>(kind)) ctx.gpr[2] = call_guest(rt, ctx, visit, {entry});
    }
}

// The handle refresh of func_08AF72A4: func_08B527B4 (marking bit 7 of `flags`) or func_08B52814, with
// (0x88EB, size, data); the result goes to `out`.
void refresh_handle(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, bool first, std::uint32_t size,
                    std::uint32_t data, std::uint32_t out, std::uint32_t flags) {
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, first ? 0x08B527B4u : 0x08B52814u, {0x88EBu, size, data});
    rt.memory().store32(out, ctx.gpr[2]);
    if (first) rt.memory().store8(flags, static_cast<std::uint8_t>(rt.memory().load8(flags) | 0x80u));
}

} // namespace

// func_08AD2174(this, x): this+0x20 = the point func_08AD1518(owner +0x78, &p, x) gives; this+0x2C = the
// current entry's +0x40 point of the owner (80-byte entries at +0x80, index +0x1C0).
void func_08AD2174(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    (void)call_guest(rt, ctx, 0x08AD1518u, {word(rt, self + 0x78u), frame.at(0u), ctx.gpr[5]});
    rt.memory().store32(self + 0x20u, word(rt, frame.at(0u)));
    rt.memory().store32(self + 0x28u, word(rt, frame.at(8u)));
    rt.memory().store32(self + 0x24u, word(rt, frame.at(4u)));
    const std::uint32_t owner = word(rt, self + 0x78u);
    const std::uint32_t point = owner + word(rt, owner + 0x1C0u) * 80u + 0x80u + 0x40u;
    rt.memory().store32(frame.at(0u), word(rt, point));
    rt.memory().store32(frame.at(4u), word(rt, point + 0x4u));
    rt.memory().store32(self + 0x2Cu, word(rt, point));
    rt.memory().store32(self + 0x34u, word(rt, point + 0x8u));
    rt.memory().store32(self + 0x30u, word(rt, point + 0x4u));
}

// Indexed progress blends: func_08955E54 (+0x78 / func_08955300 +0xC), func_08955DD0 (+0x58 / +0x10),
// func_08955D4C (+0x68 / +0x1C), func_08955CC8 (+0x88 / +0x20), func_08955C44 (+0xA8 / func_0893C780
// +0x10), func_08955BC0 (+0xB8 / +0x14).
void func_08955E54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[0] = blend_indexed(rt, ctx, 0x78u, 0x08955300u, 0xCu); }
void func_08955DD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[0] = blend_indexed(rt, ctx, 0x58u, 0x08955300u, 0x10u); }
void func_08955D4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[0] = blend_indexed(rt, ctx, 0x68u, 0x08955300u, 0x1Cu); }
void func_08955CC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[0] = blend_indexed(rt, ctx, 0x88u, 0x08955300u, 0x20u); }
void func_08955C44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[0] = blend_indexed(rt, ctx, 0xA8u, 0x0893C780u, 0x10u); }
void func_08955BC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[0] = blend_indexed(rt, ctx, 0xB8u, 0x0893C780u, 0x14u); }

// func_08AE2B7C(this): refreshes the status bits at +0xC from func_08AE3C58(the handle at +0x98), inside
// func_08AEB808 / func_08AEB82C; nothing for a negative handle.
void func_08AE2B7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t handle = word(rt, self + 0x98u);
    if (static_cast<std::int32_t>(handle) < 0) return;
    GuestFrame frame(ctx, 0x10u);
    (void)call_guest(rt, ctx, 0x08AEB808u, {self});
    std::uint32_t status = word(rt, self + 0xCu);
    const std::uint32_t bits = call_guest(rt, ctx, 0x08AE3C58u, {handle});
    status = (status & ~0x142u) | (bits & 0x142u);
    status |= 32u;
    if ((bits & 4u) != 0u) status &= ~0x42u;
    if ((bits & 2u) == 0u) status = (status & ~2u) | 16u;
    rt.memory().store32(self + 0xCu, status);
    ctx.gpr[2] = call_guest(rt, ctx, 0x08AEB82Cu, {handle});
}

// Vector copy constructors: func_08A75D04(dst, src) (words, func_08C250A0), func_089FA280(dst, src)
// (32-byte elements via func_08B35840(16, n) and func_08C0D7F8, plus +0xC).
void func_08A75D04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t dst = ctx.gpr[4];
    const std::uint32_t src = ctx.gpr[5];
    rt.memory().store32(dst + 0x4u, 0u);
    rt.memory().store32(dst, 0u);
    rt.memory().store32(dst + 0x8u, 0u);
    const std::uint32_t bytes = (word(rt, src + 0x4u) - word(rt, src)) & ~3u;
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t data = call_guest(rt, ctx, kAllocateAligned4, {bytes, 4u});
    rt.memory().store32(dst, data);
    rt.memory().store32(dst + 0x4u, data);
    rt.memory().store32(dst + 0x8u, data + bytes);
    ctx.gpr[2] = call_guest(rt, ctx, 0x08C250A0u, {word(rt, src), word(rt, src + 0x4u), data, frame.at(0u)});
    rt.memory().store32(dst + 0x4u, ctx.gpr[2]);
}
void func_089FA280(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t dst = ctx.gpr[4];
    const std::uint32_t src = ctx.gpr[5];
    rt.memory().store32(dst + 0x4u, 0u);
    const std::uint32_t bytes = (word(rt, src + 0x4u) - word(rt, src)) & ~31u;
    rt.memory().store32(dst, 0u);
    rt.memory().store32(dst + 0x8u, 0u);
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t data = call_guest(rt, ctx, 0x08B35840u, {16u, bytes});
    rt.memory().store32(dst, data);
    rt.memory().store32(dst + 0x4u, data);
    rt.memory().store32(dst + 0x8u, data + bytes);
    const std::uint32_t end = call_guest(rt, ctx, 0x08C0D7F8u, {word(rt, src), word(rt, src + 0x4u), data, frame.at(0u)});
    rt.memory().store32(dst + 0x4u, end);
    ctx.gpr[2] = word(rt, src + 0xCu);
    rt.memory().store32(dst + 0xCu, ctx.gpr[2]);
}

// func_0892B4B0(dst, src) -> dst, after copying 32 bytes (words when both are aligned, halfwords else).
void func_0892B4B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t dst = ctx.gpr[4];
    const std::uint32_t src = ctx.gpr[5];
    if (((src | dst) & 3u) == 0u) {
        for (std::uint32_t i = 0; i < 32u; i += 4u) rt.memory().store32(dst + i, word(rt, src + i));
    } else {
        for (std::uint32_t i = 0; i < 32u; i += 2u) rt.memory().store16(dst + i, rt.memory().load16(src + i));
    }
    ctx.gpr[2] = dst;
}

// func_0889DBAC(this, _, x): when the virtual +0x20 holds and x's virtual +0x30 doesn't, x's virtual +0x10
// fills +0x38 and +0x44 = func_08911290(this + 0x30); otherwise +0x38..+0x44 are cleared.
void func_0889DBAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t x = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    bool fill = call_guest(rt, ctx, word(rt, word(rt, self) + 0x20u), {self}) != 0u;
    if (fill) fill = call_guest(rt, ctx, word(rt, word(rt, x) + 0x30u), {x}) == 0u;
    if (fill) {
        (void)call_guest(rt, ctx, word(rt, word(rt, x) + 0x10u), {x, self + 0x38u});
        const float value = call_guest(rt, ctx, 0x08911290u, {self + 0x30u}, {}).f0;
        rt.memory().store32(self + 0x44u, std::bit_cast<std::uint32_t>(value));
    } else {
        for (const std::uint32_t offset : {0x38u, 0x40u, 0x3Cu, 0x44u}) rt.memory().store32(self + offset, 0u);
    }
}

// func_089033D8(this, x) -> the first i < func_08903350(this, 0) with x < func_0890337C(this, i, 0), else
// the count.
void func_089033D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const float x = ctx.fpr[12];
    GuestFrame frame(ctx, 0x20u);
    const auto count = static_cast<std::int32_t>(call_guest(rt, ctx, 0x08903350u, {self, 0u}));
    std::int32_t i = 0;
    for (; i < count; ++i)
        if (x < call_guest(rt, ctx, 0x0890337Cu, {self, static_cast<std::uint32_t>(i), 0u}, {}).f0) break;
    ctx.gpr[2] = static_cast<std::uint32_t>(i);
}

// func_08AD1740(this, x, y, z): transforms (x, y, z) by the current entry (func_08B4A774), then
// func_08B4AA34(its +0x40, its +0x40, &result).
void func_08AD1740(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t entry = self + word(rt, self + 0x1C0u) * 80u + 0x80u;
    GuestFrame frame(ctx, 0x30u);
    rt.memory().store32(frame.at(12u), std::bit_cast<std::uint32_t>(ctx.fpr[12]));
    rt.memory().store32(frame.at(16u), std::bit_cast<std::uint32_t>(ctx.fpr[13]));
    rt.memory().store32(frame.at(20u), std::bit_cast<std::uint32_t>(ctx.fpr[14]));
    rt.memory().store32(frame.at(24u), 0u);
    ctx.gpr[2] = entry - self;  // what the original has in v0; both callees can leave it alone
    ctx.gpr[2] = call_guest(rt, ctx, 0x08B4A774u, {frame.at(12u), entry, frame.at(12u)});
    for (std::uint32_t i = 0; i < 3u; ++i) rt.memory().store32(frame.at(4u * i), word(rt, frame.at(12u + 4u * i)));
    ctx.gpr[2] = call_guest(rt, ctx, 0x08B4AA34u, {entry + 0x40u, entry + 0x40u, frame.at(0u)});
}

// func_088E8920(this, force) -> whether the object may act: marked (+0x2B4), state 1 (+0x4), then either
// the mirror's byte +0x3 clear (+0x1C is 1) or the byte +0x10 set; finally force or the byte +0x2B7 clear.
void func_088E8920(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t force = ctx.gpr[5] & 0xFFu;
    ctx.gpr[5] = force;
    std::uint32_t result = 0u;
    bool ready = rt.memory().load8(self + 0x2B4u) != 0u && word(rt, self + 0x4u) == 1u;
    if (ready) {
        if (word(rt, self + 0x1Cu) != 1u) {
            ready = rt.memory().load8(self + 0x10u) != 0u;
        } else {
            const std::uint32_t mirror = word(rt, self + 0x2E4u);
            ready = mirror == 0u || rt.memory().load8(mirror + 0x3u) != 0u;
        }
    }
    if (ready) result = force != 0u || rt.memory().load8(self + 0x2B7u) == 0u ? 1u : 0u;
    ctx.gpr[3] = result;
    ctx.gpr[2] = result;
}

// func_088E75A0(this) -> whether entry +0x4 of the owner's (+0x8) list at +0x14C answers its virtual
// +0x58, unless func_088DC858(owner, +0x4) holds.
void func_088E75A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    ctx.gpr[2] = 0u;
    const std::uint32_t owner = word(rt, self + 0x8u);
    if (owner == 0u) return;
    const std::uint32_t list = word(rt, owner + 0x14Cu);
    if (list == 0u) return;
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t entry = call_guest(rt, ctx, 0x088F8D0Cu, {list, word(rt, self + 0x4u), 0u});
    std::uint32_t result = 0u;
    if (entry != 0u && call_guest(rt, ctx, 0x088DC858u, {word(rt, self + 0x8u), word(rt, self + 0x4u)}) == 0u &&
        call_guest(rt, ctx, word(rt, word(rt, entry) + 0x58u), {entry}) != 0u)
        result = 1u;
    ctx.gpr[2] = result;
}

// func_08AFA170(this, i, a, b): func_08AFA0F0(this, a, b) for a negative i, else func_08AF8904,
// func_08AF9E88 for item i and func_08AF8434.
void func_08AFA170(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t i = ctx.gpr[5], a = ctx.gpr[6], b = ctx.gpr[7];
    if (static_cast<std::int32_t>(i) < 0) {
        tail_call(ctx, 0x08AFA0F0u, {self, a, b});
        return;
    }
    {
        GuestFrame frame(ctx, 0x20u);
        (void)call_guest(rt, ctx, 0x08AF8904u, {self, a, b});
        ctx.gpr[2] = call_guest(rt, ctx, 0x08AF9E88u, {self, i, a, b});
    }
    tail_call(ctx, 0x08AF8434u, {self});
}

// func_0897D5D0(this, a, key) -> whether key is in the map the virtual +0xC returns (func_08BEE8B8); when
// it is, the virtual +0x10 (this, its value, a).
void func_0897D5D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t a = ctx.gpr[5];
    const std::uint32_t key = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t map = call_guest(rt, ctx, word(rt, word(rt, self) + 0xCu), {self, a, key});
    const std::uint32_t found = call_guest(rt, ctx, 0x08BEE8B8u, {map, key});
    std::uint32_t result = 0u;
    if (found != map + 4u) {
        (void)call_guest(rt, ctx, word(rt, word(rt, self) + 0x10u), {self, word(rt, found + 0x14u), a});
        result = 1u;
    }
    ctx.gpr[2] = result;
}

// func_08B1B1E4(this): under the mutex at this, +0x30 = now - the previous stamp (+0x28, when positive),
// then the stamp = now (func_08B2E05C, 64-bit).
void func_08B1B1E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        const GuestResult now = call_guest(rt, ctx, 0x08B2E05Cu, {self}, {});
        (void)call_guest(rt, ctx, kLock, {self});
        const std::uint32_t low = word(rt, self + 0x28u), high = word(rt, self + 0x2Cu);
        const std::uint64_t delta = ((static_cast<std::uint64_t>(now.v1) << 32) | now.v0) - ((static_cast<std::uint64_t>(high) << 32) | low);
        if (static_cast<std::int32_t>(high) > 0 || (high == 0u && low != 0u)) {
            rt.memory().store32(self + 0x30u, static_cast<std::uint32_t>(delta));
            rt.memory().store32(self + 0x34u, static_cast<std::uint32_t>(delta >> 32));
        }
        rt.memory().store32(self + 0x28u, now.v0);
        rt.memory().store32(self + 0x2Cu, now.v1);
    }
    tail_call(ctx, kUnlock, {self});
}

// func_088F4BE0(this): resets the two command lists of *this (counts +0x404 / +0x80C) and, when the flag
// +0x824 is set, appends one 0x20001 command to each (up to 256) and clears the flag.
void func_088F4BE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    rt.memory().store32(word(rt, self) + 0x404u, 0u);
    rt.memory().store32(word(rt, self) + 0x80Cu, 0u);
    std::uint32_t object = word(rt, self);
    if (rt.memory().load8(object + 0x824u) == 0u) return;
    for (const std::uint32_t list_offset : {0x0u, 0x408u}) {
        const std::uint32_t list = word(rt, self) + list_offset;
        const std::uint32_t count = word(rt, list + 0x404u);
        if (count + 1u < 257u) {
            rt.memory().store32(list + (count << 2) + 0x4u, 0x20001u);
            rt.memory().store32(list + 0x404u, count + 1u);
        }
    }
    object = word(rt, self);
    rt.memory().store8(object + 0x824u, 0u);
}

// func_08AF2864(this): the source object's virtual +0x18 (or func_08B55098(0) without one, when +0x4 is
// set), then the virtual +0x1C of its +0x4 object.
void func_08AF2864(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t pair = word(rt, self);
    {
        GuestFrame frame(ctx, 0x10u);
        if (const std::uint32_t first = word(rt, pair); first != 0u) (void)call_guest(rt, ctx, word(rt, word(rt, first) + 0x18u), {first});
        else if (word(rt, pair + 0x4u) != 0u) (void)call_guest(rt, ctx, 0x08B55098u, {0u});
        else return;
    }
    const std::uint32_t second = word(rt, word(rt, self) + 0x4u);
    ctx.gpr[2] = second;
    if (second == 0u) return;
    tail_call(ctx, word(rt, word(rt, second) + 0x1Cu), {second});
}

// func_08A7E88C(this, x): base func_08A76ACC(this, x), the vtable, a fresh id (+0x14) from the counter at
// 0x08CFF05C, func_08A6972C(this + 0x18, x), then an empty tree at +0x20.
void func_08A7E88C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t x = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08A76ACCu, {self, x});
        rt.memory().store32(self, 0x08CB6DA8u);
        const std::uint32_t id = word(rt, 0x08CFF05Cu);
        rt.memory().store32(0x08CFF05Cu, id + 1u);
        rt.memory().store32(self + 0x14u, id);
        (void)call_guest(rt, ctx, 0x08A6972Cu, {self + 0x18u, x});
    }
    const std::uint32_t header = self + 0x24u;
    rt.memory().store32(self + 0x1Cu, 0u);
    rt.memory().store32(self + 0x30u, header);
    rt.memory().store32(self + 0x34u, 0u);
    rt.memory().store32(self + 0x24u, 0u);
    rt.memory().store32(self + 0x28u, 0u);
    rt.memory().store32(self + 0x2Cu, header);
    ctx.gpr[2] = self + 0x20u;
    ctx.gpr[3] = header;
}

// func_088A9F10(this) -> max(0, 1 - max(0, func_08956104(+0xC, 0.0))) when active (+0x18), else 0.0.
void func_088A9F10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t object = word(rt, self + 0xCu);
    ctx.fpr[0] = 0.0f;
    if (object == 0u || rt.memory().load8(self + 0x18u) == 0u) return;
    GuestFrame frame(ctx, 0x20u);
    const float r = call_guest(rt, ctx, 0x08956104u, {object}, {0.0f}).f0;
    const float clamped = 0.0f < r ? r : 0.0f;
    const float rest = 1.0f - clamped;
    ctx.fpr[0] = 0.0f < rest ? rest : 0.0f;
}

// func_0890640C(this, x): when func_08906398(this): func_08906038(engine, +0x154 of *this) if positive,
// then func_089061F4(engine, x's +0x1C) — engine = +0x48 of *this's +0x14; else 0.0.
void func_0890640C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t x = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x20u);
        if (call_guest(rt, ctx, 0x08906398u, {self}) == 0u) {
            ctx.fpr[0] = 0.0f;
            return;
        }
        const float first = float_at(rt, word(rt, self) + 0x154u);
        if (0.0f < first) (void)call_guest(rt, ctx, 0x08906038u, {word(rt, word(rt, word(rt, self) + 0x14u) + 0x48u)}, {first});
    }
    ctx.fpr[12] = float_at(rt, x + 0x1Cu);
    tail_call(ctx, 0x089061F4u, {word(rt, word(rt, word(rt, self) + 0x14u) + 0x48u)});
}

// func_089207D4(this, x): smooths the value at +0xC towards the virtual +0x14 (this, x) by 12 * the factor
// at 0x08CDDD68, then func_08B4F3B0(x, it) — unless the owner's flag +0x24C is set.
void func_089207D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t x = ctx.gpr[5];
    float target;
    {
        GuestFrame frame(ctx, 0x10u);
        target = call_guest(rt, ctx, word(rt, word(rt, self) + 0x14u), {self, x}, {}).f0;
    }
    ctx.fpr[0] = target;
    if (rt.memory().load8(word(rt, self + 0x8u) + 0x24Cu) != 0u) return;
    const float current = float_at(rt, self + 0xCu);
    const float next = current + psp_mul(target - current, psp_mul(float_at(rt, 0x08CDDD68u), 12.0f));
    rt.memory().store32(self + 0xCu, std::bit_cast<std::uint32_t>(next));
    ctx.fpr[12] = next;
    tail_call(ctx, 0x08B4F3B0u, {x});
}

// func_0890E3A4(flag, x, y, z) -> func_08B57D90(p, (y - p * k) / (1 - k)) for k = min(z, 0.99) and p =
// flag ? -1000000 : x.
void func_0890E3A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const float x = ctx.fpr[12], y = ctx.fpr[13], z = ctx.fpr[14];
    const float limit = std::bit_cast<float>(0x3F7D70A4u);
    const float k = z < limit ? z : limit;
    const float p = (ctx.gpr[4] & 0xFFu) != 0u ? -1000000.0f : x;
    const float q = (y - psp_mul(p, k)) / (1.0f - k);
    GuestFrame frame(ctx, 0x30u);
    ctx.gpr[2] = call_guest(rt, ctx, 0x08B57D90u, {ctx.gpr[4] & 0xFFu}, {p, q}).v0;
}

// func_089A9DDC(this, x): clears every slot equal to x in the vector at +0x12C; when it held something and
// is now all empty, func_08BF9054(vector, begin, end) empties it.
void func_089A9DDC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t vec = ctx.gpr[4] + 0x12Cu;
    const std::uint32_t x = ctx.gpr[5];
    std::uint32_t begin = word(rt, vec), end = word(rt, vec + 0x4u);
    bool all_empty = begin != end;
    std::uint32_t i = 0;
    for (; i < static_cast<std::uint32_t>(static_cast<std::int32_t>(end - begin) >> 2); ++i) {
        if (word(rt, begin + (i << 2)) == x) {
            rt.memory().store32(begin + (i << 2), 0u);
            end = word(rt, vec + 0x4u);
            begin = word(rt, vec);
        }
        if (word(rt, begin + (i << 2)) != 0u) all_empty = false;
    }
    // func_08BF9054 is erase(vector, first, last): the original leaves begin / end in a1 / a2.
    if (all_empty) tail_call(ctx, 0x08BF9054u, {vec, begin, end, i << 2});
}

// func_08809C08(this, x): func_0885E314(the +0x118 record's object, its rectangle scaled by the factors at
// 0x08CDE764 / 0x08CDE760, its +0x18) when set, then func_089A9DDC(x, this).
void func_08809C08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t x = ctx.gpr[5];
    const std::uint32_t record = word(rt, self + 0x118u);
    if (const std::uint32_t object = word(rt, record); object != 0u) {
        const float kx = float_at(rt, 0x08CDE764u), ky = float_at(rt, 0x08CDE760u);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x0885E314u, {object, ctx.gpr[5], record},
                                {float_at(rt, record + 0x8u) / kx, float_at(rt, record + 0xCu) / ky, float_at(rt, record + 0x10u) / kx,
                                 float_at(rt, record + 0x14u) / ky, float_at(rt, record + 0x18u)})
                         .v0;
    }
    tail_call(ctx, 0x089A9DDCu, {x, self});
}

// Destructors releasing a string: func_08A6D508 (+0x14, then func_08A780B0, 24 bytes), func_08A6667C (+0x8,
// a script value, 12 bytes), func_0898A428 (sets the state +0x1E8 = 2 when func_0898944C's name is empty).
void func_08A6D508(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    rt.memory().store32(self, 0x08CB7328u);
    release_string(rt, ctx, word(rt, self + 0x14u));
    GuestFrame frame(ctx, 0x20u);
    (void)call_guest(rt, ctx, 0x08A780B0u, {self});
    ctx.gpr[2] = call_guest(rt, ctx, kFreeAligned, {self, 24u, 4u});
}
void func_08A6667C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    rt.memory().store32(self, 0x08CD2D18u);
    release_string(rt, ctx, word(rt, self + 0x8u));
    rt.memory().store32(self, kScriptValueBase);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[2] = call_guest(rt, ctx, kFree, {self, 12u});
}
void func_0898A428(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t name;
    {
        GuestFrame frame(ctx, 0x20u);
        (void)call_guest(rt, ctx, 0x089BA5F8u, {self});
        (void)call_guest(rt, ctx, 0x0898944Cu, {frame.at(4u), self});
        name = word(rt, frame.at(4u));
    }
    if (word(rt, name - 12u) == 0u) rt.memory().store8(self + 0x1E8u, 2u);
    release_string(rt, ctx, name);
}

// func_08A849CC(this): empties the values at +0x1C and +0x18 (func_08A76AA0 locals assigned), then
// func_08A77FF8(this).
void func_08A849CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    // v0 is chained through the calls: func_08A77FF8 can return without writing it.
    for (const auto [offset, local] : {std::pair{0x1Cu, 4u}, std::pair{0x18u, 0u}}) {
        ctx.gpr[2] = call_guest(rt, ctx, 0x08A76AA0u, {frame.at(local)});
        if (self + offset != frame.at(local)) ctx.gpr[2] = call_guest(rt, ctx, kAssignSmart, {self + offset, word(rt, frame.at(local))});
        ctx.gpr[2] = call_guest(rt, ctx, kReleaseLocal, {frame.at(local)});
    }
    ctx.gpr[2] = call_guest(rt, ctx, 0x08A77FF8u, {self});
}

// func_089A8DFC(this) -> the unsigned 64-bit count at +0x108 converted to float (func_08B6EE4C /
// func_08B6EF68), divided by 60.
void func_089A8DFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t low = word(rt, ctx.gpr[4] + 0x108u), high = word(rt, ctx.gpr[4] + 0x10Cu);
    GuestFrame frame(ctx, 0x10u);
    float value;
    if (static_cast<std::int32_t>(call_guest(rt, ctx, 0x08B6EE4Cu, {low, high, 0u, 0u})) > 0) {
        value = call_guest(rt, ctx, 0x08B6EF68u, {low, high}, {}).f0;
    } else {
        const std::uint32_t half_low = (low & 1u) | (high << 31) | (low >> 1);
        value = call_guest(rt, ctx, 0x08B6EF68u, {half_low, high >> 1}, {}).f0;
        value = value + value;
    }
    ctx.fpr[0] = value / 60.0f;
}

// func_08A72818(a, b, c, d) -> a new (a + b + c + d) * 4 + 32-byte object func_08A726B0 builds, or 0.
void func_08A72818(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = ctx.gpr[4], b = ctx.gpr[5], c = ctx.gpr[6], d = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t object = call_guest(rt, ctx, kAllocateAligned4, {((a + b + c + d) << 2) + 32u, 4u});
    if (object != 0u) (void)call_guest(rt, ctx, 0x08A726B0u, {object, a, b, c, d});
    ctx.gpr[2] = object;
}

// func_08A988B4(this): deleting destructor (the vector at +0x14 destroyed and freed, func_08A780B0, 36 bytes).
void func_08A988B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    rt.memory().store32(self, 0x08CB7108u);
    GuestFrame frame(ctx, 0x20u);
    (void)call_guest(rt, ctx, 0x08B856B0u, {word(rt, self + 0x14u), word(rt, self + 0x18u), frame.at(0u)});
    if (const std::uint32_t data = word(rt, self + 0x14u); data != 0u) {
        const auto bytes = static_cast<std::uint32_t>((static_cast<std::int32_t>(word(rt, self + 0x1Cu) - data) >> 2) << 2);
        (void)call_guest(rt, ctx, kFreeAligned, {data, bytes, 4u});
    }
    (void)call_guest(rt, ctx, 0x08A780B0u, {self});
    ctx.gpr[2] = call_guest(rt, ctx, kFreeAligned, {self, 36u, 4u});
}

// Entry walkers by kind: func_08AF70C4 (func_08AF70B4), func_08AF7030 (func_08AF6FE0).
void func_08AF70C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { for_each_entry_kind(rt, ctx, 0x08AF70B4u); }
void func_08AF7030(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { for_each_entry_kind(rt, ctx, 0x08AF6FE0u); }

// func_0896D6A4(this, x): the member +0xA0 reset (func_0897D9C0 from kLookupTable), func_0896D65C(+0xD0,
// x, this) when set, then func_089BA68C(this, x).
void func_0896D6A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t x = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    (void)call_guest(rt, ctx, 0x0897D9C0u, {frame.at(0u), kLookupTable});
    if (self + 0xA0u != frame.at(0u)) (void)call_guest(rt, ctx, kAssignSmart, {self + 0xA0u, word(rt, frame.at(0u))});
    (void)call_guest(rt, ctx, kReleaseLocal, {frame.at(0u)});
    if (const std::uint32_t object = word(rt, self + 0xD0u); object != 0u) (void)call_guest(rt, ctx, 0x0896D65Cu, {object, x, self});
    ctx.gpr[2] = call_guest(rt, ctx, 0x089BA68Cu, {self, x});
}

// func_08AD986C(angle) -> a fixed-point sine-like value: the angle clamped to +-6144 split into a fine
// (128 steps, table 0x08CDFF9C) and a coarse part (table 0x08CDFE18), multiplied and scaled by 1/4096.
void func_08AD986C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    auto angle = static_cast<std::int32_t>(ctx.gpr[4]);
    angle = angle < -6144 ? -6144 : (angle < 6144 ? angle : 6144);
    const std::int32_t magnitude = angle > -angle ? angle : -angle;
    const std::int32_t sign = angle < 0 ? -1 : 1;
    const std::int32_t fine = (magnitude & 127) * sign;
    const std::int32_t coarse = (magnitude >> 7) * sign;
    const auto f = static_cast<std::int32_t>(static_cast<std::int16_t>(rt.memory().load16(0x08CDFF9Cu + static_cast<std::uint32_t>(fine << 1) + 256u)));
    const auto c = static_cast<std::int32_t>(word(rt, 0x08CDFE18u + static_cast<std::uint32_t>(coarse << 2) + 192u));
    ctx.gpr[2] = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::uint32_t>(f * c)) >> 12);
}

// func_08B2C1C0(this, key) -> &key's entry value (+0x4) after taking a reference (bit 0, and one more at
// +0x4C) under the mutex at this; 0 when absent.
void func_08B2C1C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    (void)call_guest(rt, ctx, kLock, {self});
    const std::uint32_t index = call_guest(rt, ctx, 0x08B2B9BCu, {self, ctx.gpr[5]});
    std::uint32_t result = 0u;
    if (static_cast<std::int32_t>(index) >= 0) {
        const std::uint32_t entry = word(rt, word(rt, self + 0x44u) + (index << 3) + 0x4u);
        const std::uint32_t refs = word(rt, entry);
        result = entry + 0x4u;
        if ((refs & 1u) == 0u) {
            rt.memory().store32(entry, refs | 1u);
            rt.memory().store32(self + 0x4Cu, word(rt, self + 0x4Cu) + 1u);
        }
    }
    (void)call_guest(rt, ctx, kUnlock, {self});
    ctx.gpr[2] = result;
}

// func_08AF7398(this): refreshes the cached handle +0x8 (as func_08AF72A4) for the buffer +0x4 of half(+0x2)
// elements of 4 or 2 bytes (byte +0x1 is 3 for 4).
void func_08AF7398(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t flags = rt.memory().load8(self);
    if (static_cast<std::int8_t>(flags) < 0) return;
    const std::uint32_t element = rt.memory().load8(self + 0x1u) != 3u ? 2u : 4u;
    const std::uint32_t size = element * rt.memory().load16(self + 0x2u);
    refresh_handle(rt, ctx, (flags & 1u) != 0u, size, word(rt, self + 0x4u), self + 0x8u, self);
}

// func_08AD5AC4(this, i): for slot i (16 bytes from +0x4), +0x4 of its record = func_08ADF258(handle, 0, 1)
// when the handle is valid, then the virtual +0x20 (this, i).
void func_08AD5AC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t i = ctx.gpr[5];
    const std::uint32_t record = word(rt, self + (i << 4) + 0x4u);
    const std::uint32_t handle = word(rt, record);
    if (static_cast<std::int32_t>(handle) < 0) return;
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08ADF258u, {handle, 0u, 1u});
    }
    rt.memory().store32(record + 0x4u, ctx.gpr[2]);
    ctx.gpr[2] = word(rt, self);
    tail_call(ctx, word(rt, ctx.gpr[2] + 0x20u), {self, i});
}

// func_08A568EC(this, a, b): func_08A76E3C(this, a, b); while *a is still empty, the virtual +0x10C (a, b)
// of the object at +0x14.
void func_08A568EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t a = ctx.gpr[5], b = ctx.gpr[6];
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08A76E3Cu, {self, a, b});
        if (word(rt, a) != 0u) return;
        if (word(rt, self + 0x14u) == 0u) (void)call_guest(rt, ctx, 0x08A5BB7Cu, {0x08CAF798u});
    }
    const std::uint32_t object = word(rt, self + 0x14u);
    ctx.gpr[2] = word(rt, object);
    tail_call(ctx, word(rt, ctx.gpr[2] + 0x10Cu), {object, a, b});
}

// func_08A7C4CC(out, a, p) -> out: emptied, then func_08A7BFD0(a, out, &v) with v = *p's virtual +0x5C.
void func_08A7C4CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t a = ctx.gpr[5];
    const std::uint32_t p = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    (void)call_guest(rt, ctx, 0x08A76AA0u, {out});
    const std::uint32_t object = word(rt, p);
    if (object == 0u) (void)call_guest(rt, ctx, 0x08A5BB7Cu, {0x08CB1350u});
    (void)call_guest(rt, ctx, word(rt, word(rt, object) + 0x5Cu), {frame.at(0u), object});
    (void)call_guest(rt, ctx, 0x08A7BFD0u, {a, out, frame.at(0u)});
    (void)call_guest(rt, ctx, 0x08A69D68u, {frame.at(0u)});
    ctx.gpr[2] = out;
}

// func_089662F8(this, on): when allowed, bits 7 and 9 of the +0x80 object's flags (+0x30) set, or bit 9
// cleared; marks +0xE4.
void func_089662F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t on = ctx.gpr[5] & 0xFFu;
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08966174u, {self});
    }
    if (ctx.gpr[2] == 0u) return;
    const std::uint32_t object = word(rt, self + 0x80u);
    if (on != 0u) {
        rt.memory().store32(object + 0x30u, word(rt, object + 0x30u) | 128u);
        const std::uint32_t again = word(rt, self + 0x80u);
        rt.memory().store32(again + 0x30u, word(rt, again + 0x30u) | 512u);
    } else {
        rt.memory().store32(object + 0x30u, word(rt, object + 0x30u) & ~0x200u);
    }
    ctx.gpr[2] = word(rt, self + 0xE4u) | 4u;
    rt.memory().store32(self + 0xE4u, ctx.gpr[2]);
}

// func_08A86110(this, out, _, p): out = func_08A85B1C(whether +0x14 equals *p's virtual +0x3C).
void func_08A86110(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t object = word(rt, ctx.gpr[7]);
    const std::uint32_t value = word(rt, self + 0x14u);
    std::uint32_t other;
    {
        GuestFrame frame(ctx, 0x20u);
        if (object == 0u) (void)call_guest(rt, ctx, 0x08A5BB7Cu, {0x08CB181Cu});
        other = call_guest(rt, ctx, word(rt, word(rt, object) + 0x3Cu), {object});
    }
    store_built(rt, ctx, 0x08A85B1Cu, value == other ? 1u : 0u);
}

} // namespace gt::game

GT_DECOMP(0x08AD2174u, gt::game::func_08AD2174, gt::ReturnKind::Void);
GT_DECOMP(0x08955E54u, gt::game::func_08955E54, gt::ReturnKind::Float);
GT_DECOMP(0x08955DD0u, gt::game::func_08955DD0, gt::ReturnKind::Float);
GT_DECOMP(0x08955D4Cu, gt::game::func_08955D4C, gt::ReturnKind::Float);
GT_DECOMP(0x08955CC8u, gt::game::func_08955CC8, gt::ReturnKind::Float);
GT_DECOMP(0x08955C44u, gt::game::func_08955C44, gt::ReturnKind::Float);
GT_DECOMP(0x08955BC0u, gt::game::func_08955BC0, gt::ReturnKind::Float);
GT_DECOMP(0x08AE2B7Cu, gt::game::func_08AE2B7C, gt::ReturnKind::Void);
GT_DECOMP(0x08A75D04u, gt::game::func_08A75D04, gt::ReturnKind::Word);
GT_DECOMP(0x089FA280u, gt::game::func_089FA280, gt::ReturnKind::Word);
GT_DECOMP(0x0892B4B0u, gt::game::func_0892B4B0, gt::ReturnKind::Word);
GT_DECOMP(0x0889DBACu, gt::game::func_0889DBAC, gt::ReturnKind::Void);
GT_DECOMP(0x089033D8u, gt::game::func_089033D8, gt::ReturnKind::Word);
GT_DECOMP(0x08AD1740u, gt::game::func_08AD1740, gt::ReturnKind::Word);
GT_DECOMP(0x088E8920u, gt::game::func_088E8920, gt::ReturnKind::Word);
GT_DECOMP(0x088E75A0u, gt::game::func_088E75A0, gt::ReturnKind::Word);
GT_DECOMP(0x08AFA170u, gt::game::func_08AFA170, gt::ReturnKind::Word);
GT_DECOMP(0x0897D5D0u, gt::game::func_0897D5D0, gt::ReturnKind::Word);
GT_DECOMP(0x08B1B1E4u, gt::game::func_08B1B1E4, gt::ReturnKind::Word);
GT_DECOMP(0x088F4BE0u, gt::game::func_088F4BE0, gt::ReturnKind::Void);
GT_DECOMP(0x08AF2864u, gt::game::func_08AF2864, gt::ReturnKind::Void);
GT_DECOMP(0x08A7E88Cu, gt::game::func_08A7E88C, gt::ReturnKind::Word);
GT_DECOMP(0x088A9F10u, gt::game::func_088A9F10, gt::ReturnKind::Float);
GT_DECOMP(0x0890640Cu, gt::game::func_0890640C, gt::ReturnKind::Float);
GT_DECOMP(0x089207D4u, gt::game::func_089207D4, gt::ReturnKind::Float);
GT_DECOMP(0x0890E3A4u, gt::game::func_0890E3A4, gt::ReturnKind::Word);
GT_DECOMP(0x089A9DDCu, gt::game::func_089A9DDC, gt::ReturnKind::Void);
GT_DECOMP(0x08809C08u, gt::game::func_08809C08, gt::ReturnKind::Void);
GT_DECOMP(0x08A6D508u, gt::game::func_08A6D508, gt::ReturnKind::Word);
GT_DECOMP(0x08A6667Cu, gt::game::func_08A6667C, gt::ReturnKind::Word);
GT_DECOMP(0x0898A428u, gt::game::func_0898A428, gt::ReturnKind::Void);
GT_DECOMP(0x08A849CCu, gt::game::func_08A849CC, gt::ReturnKind::Word);
GT_DECOMP(0x089A8DFCu, gt::game::func_089A8DFC, gt::ReturnKind::Float);
GT_DECOMP(0x08A72818u, gt::game::func_08A72818, gt::ReturnKind::Word);
GT_DECOMP(0x08A988B4u, gt::game::func_08A988B4, gt::ReturnKind::Word);
GT_DECOMP(0x08AF70C4u, gt::game::func_08AF70C4, gt::ReturnKind::Void);
GT_DECOMP(0x08AF7030u, gt::game::func_08AF7030, gt::ReturnKind::Void);
GT_DECOMP(0x0896D6A4u, gt::game::func_0896D6A4, gt::ReturnKind::Word);
GT_DECOMP(0x08AD986Cu, gt::game::func_08AD986C, gt::ReturnKind::Word);
GT_DECOMP(0x08B2C1C0u, gt::game::func_08B2C1C0, gt::ReturnKind::Word);
GT_DECOMP(0x08AF7398u, gt::game::func_08AF7398, gt::ReturnKind::Void);
GT_DECOMP(0x08AD5AC4u, gt::game::func_08AD5AC4, gt::ReturnKind::Word);
GT_DECOMP(0x08A568ECu, gt::game::func_08A568EC, gt::ReturnKind::Word);
GT_DECOMP(0x08A7C4CCu, gt::game::func_08A7C4CC, gt::ReturnKind::Word);
GT_DECOMP(0x089662F8u, gt::game::func_089662F8, gt::ReturnKind::Void);
GT_DECOMP(0x08A86110u, gt::game::func_08A86110, gt::ReturnKind::Void);
