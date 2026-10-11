// Small leaf functions the race path calls hundreds of thousands of times, picked with a call probe
// (GT_PROBE_SYMBOLS over every undone function). Their classes and subsystems aren't known yet, so
// they keep their addresses as names; each comment says what the code does. Written from the asm.

#include "decomp_registry.hpp"
#include "guest_call.hpp"

#include <bit>
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
