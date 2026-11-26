// New batch main-1046 (draft, unverified): one function of the two assigned, 0x08C48488. 0x08B6D6B4 was skipped as
// not certain enough (the note is next to the function below). Written as structured C from tools.disasm.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1046 {
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

// 0x08C48488 (a0 = vector header {begin, end, capacity}, a1 = position, a2 = element): inserts the 0xF0-byte element at
// `position`. The element at the old end is duplicated into the new slot (end += 0xF0), the staged copy of the source is
// written at the position and the elements between the position and the old last element are shifted one slot forward
// (copy_backward, count = (last - position) / 0xF0 via the signed-division trick (n >> 4) * 0xEEEEEEEF mod 2^32).
// With end == capacity the buffer grows first: count = (end - begin) / 0xF0 likewise; the new size is 2 * count elements
// (0xF0 bytes when count is 0), func_08C481C8 copies [begin, position) into it and answers the slot for the new element,
// func_08C481C8 copies [position, end) after that element, func_08C47014 runs over the old range, the old buffer is freed
// and the header takes the new buffer, the answered end and the new capacity end. v0: the word at source + 0xE0 on the
// no-growth path (the last word of the staged copy the original leaves in v0), the new capacity end after a growth.
//
// 0x08B6D6B4 was skipped: its 529 instructions are a float wheel-packing routine (a 3-float output, float constants read
// from 0x08CB64E8..0x08CB6500, two int/float staging arrays and a base-256 carry chain); a hand transcription was not
// certain enough to register.

constexpr u32 kElementBytes = 0xF0u;

void copy_element(Runtime &rt, u32 to, u32 from) {
    for (u32 i = 0u; i < kElementBytes; i += 4u) put(rt, to + i, word(rt, from + i));
}

// The low 32 bits of the product, as `mult` + `mflo` leave them.
u32 mul_lo(u32 a, u32 b) { return static_cast<u32>(static_cast<std::uint64_t>(a) * static_cast<std::uint64_t>(b)); }

void func_08C48488(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x140u);
    const u32 container = ctx.gpr[4];
    const u32 position = ctx.gpr[5];
    const u32 source = ctx.gpr[6];
    put(rt, frame.at(0x100u), position);
    const u32 end = word(rt, container + 4u);

    if (end != word(rt, container + 8u)) {  // room left: in-place insert
        if (end != 0u) copy_element(rt, end, end - kElementBytes);
        put(rt, container + 4u, word(rt, container + 4u) + kElementBytes);
        copy_element(rt, frame.at(4u), source);
        const u32 last = end - kElementBytes;
        const u32 moved = mul_lo(static_cast<u32>(static_cast<s32>(last - position) >> 4), 0xEEEEEEEFu);
        if (static_cast<s32>(moved) > 0) {
            u32 from = last - kElementBytes;
            u32 to = end - kElementBytes;
            for (u32 i = 0u; i != moved; ++i) {
                copy_element(rt, to, from);
                from -= kElementBytes;
                to -= kElementBytes;
            }
        }
        copy_element(rt, position, frame.at(4u));
        ctx.gpr[2] = word(rt, source + 0xE0u);  // the last word of the copy loop the original leaves in v0
        return;
    }

    // end == capacity: grow to twice the element count, copy both halves around the new element.
    const u32 begin = word(rt, container);
    const u32 count = mul_lo(static_cast<u32>(static_cast<s32>(end - begin) >> 4), 0xEEEEEEEFu);
    u32 scaled = count;
    if (count == 0x11111111u) scaled = call(rt, ctx, 0x08B5ADDCu, {0x08CB4AD4u});

    u32 buffer;
    u32 extent_256;  // frame + 0x10C
    u32 extent_16;   // frame + 0x108
    if (scaled == 0u) {
        extent_256 = 0x100u;
        extent_16 = 0x10u;
        put(rt, frame.at(0x10Cu), extent_256);
        put(rt, frame.at(0x108u), extent_16);
        buffer = call(rt, ctx, 0x08B35DA0u, {kElementBytes});
        put(rt, frame.at(0x104u), buffer);
    } else {
        const u32 twice = scaled << 1;
        if (twice < scaled) {  // the original's overflow branch
            extent_256 = 0x11111100u;
            extent_16 = 0x11111110u;
            put(rt, frame.at(0x10Cu), extent_256);
            put(rt, frame.at(0x108u), extent_16);
            buffer = call(rt, ctx, 0x08B35DA0u, {0xFFFFFFF0u});
            put(rt, frame.at(0x104u), buffer);
        } else if (twice == 0u) {
            buffer = 0u;
            extent_256 = 0u;
            extent_16 = 0u;
            put(rt, frame.at(0x104u), 0u);
            put(rt, frame.at(0x108u), extent_16);
            put(rt, frame.at(0x10Cu), extent_256);
        } else {
            extent_256 = twice << 8;
            extent_16 = twice << 4;
            put(rt, frame.at(0x10Cu), extent_256);
            put(rt, frame.at(0x108u), extent_16);
            buffer = call(rt, ctx, 0x08B35DA0u, {extent_256 - extent_16});
            put(rt, frame.at(0x104u), buffer);
        }
    }

    const u32 inserted = call(rt, ctx, 0x08C481C8u, {word(rt, container), position, buffer, frame.at(2u)});
    if (inserted != 0u) copy_element(rt, inserted, source);
    const u32 new_end =
        call(rt, ctx, 0x08C481C8u, {position, word(rt, container + 4u), inserted + kElementBytes, frame.at(1u)});
    (void)call(rt, ctx, 0x08C47014u, {word(rt, container), word(rt, container + 4u), frame.at(0u)});
    if (word(rt, container) != 0u) (void)call(rt, ctx, 0x08B35254u, {word(rt, container)});

    const u32 capacity_end = buffer + (extent_256 - extent_16);
    put(rt, container + 4u, new_end);
    put(rt, container + 8u, capacity_end);
    put(rt, container, buffer);
    ctx.gpr[2] = capacity_end;
}

} // namespace
} // namespace new_batch_main_1046
} // namespace gt::game

#define NEWMAIN1046(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1046::name, gt::ReturnKind::kind)
NEWMAIN1046(0x08C48488u, func_08C48488, Word);
