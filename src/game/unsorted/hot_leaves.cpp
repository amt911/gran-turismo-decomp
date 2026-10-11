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
