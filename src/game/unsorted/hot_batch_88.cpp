// Hot batch 88: reached functions that no earlier batch has written, picked from the call probe of batch 78 (every function of up to 70 instructions not yet decompiled, run on the five routes with --decomp off), the same selection as batches 85-87
// (main includes batch 87, 6,059 functions). Before reading any function its address was searched for in src/ (main and the open PR branches: none). Candidates without VFPU, without imports that the harness cannot replay (calls into the import
// stubs from 0x08C70000 on were left out), without endless loops, jump tables and multi-entry blobs (func_08B207A4 shares its bytes with three other entry points, func_08AC7EDC is a worker-thread loop); each of the 71 written was read by hand word by
// word with `tools/disasm.py` (branch targets and relocations resolved by the tool, every delay slot part of its branch's path).
//
//  - bit, range and table helpers (func_08B1CA60 a bit writer, func_08B2068C a wrapping or clamped position, func_08BB1A00 the sift-down of a heap, func_08BFA640 the recursion of a stable sort, func_08C4FF60 a stream cipher, func_08B71DC8 double to int with
//    saturation, func_08B8A8A8, func_08AD5CE8, func_08AE9F98, func_088E96F4, func_088F8BAC, func_088F9008, func_0887CCA8, func_089FB3A4, func_08BCF61C, func_0885CBCC, func_088D5028) and the vector reserve family (`vector_reserve<Shift>`: func_08B893E8,
//    func_08B894E4; func_08B904D8 with the other allocator; the range copy func_08B8AF08);
//  - constructors with guarded statics (func_088710DC, func_0887F4FC, func_0886F5FC, func_08BA3718), a type-key lookup (`type_key_cast`: func_08BB9C24, func_08BB9E4C) and a destructor (func_08B958E8);
//  - script natives of the Adhoc classes: getters, setters and registration tables over holders and string temporaries (func_0880CE2C, func_0880F470, func_0881EAD0, func_0881FDEC, func_088213BC, func_08A25FAC, func_08A213C0, func_088388BC, func_0883C4B4,
//    func_08855C68, func_08829CC4, func_0882B744, func_08835E1C, func_088381F8, func_08994994, func_0899EF08, func_089D30D4, func_089F01F4, func_08A9FA9C, func_08A3BD88 and the property wrappers `property_accessor`, `property_pair_b`, `bool_property_a`, `bool_property_b`).
//
// Written but not registered (the commented HOT88 lines and each function's comment say why): four whose every case stops on a held LwMutex (func_0897AEE4, func_08A4AE9C, func_08BCF9E4, func_08A8712C), two that reach an import the harness does not replay
// (func_08AB9B04 sceUtility, func_08A37510 sceIoRead), nine without a compared case that changed memory (func_088285AC, func_088286B8, func_08828C8C, func_088341EC, func_088941A0, func_088D7158, func_0883365C, func_08845BA8, func_0898BAB4: the getters
// were compared, the setters and the copy were not) and func_088A1A3C (no route called it in the fast gates).
// func_088710DC runs only inside the cases of its decompiled parent func_0887F4FC (the parent's single call per route swallows its compared calls: FINDINGS, batch 86), so it is registered NESTED (return contract audited: `tools.contract_audit` 0);
// the gate counts it as `nested in 6 equal / 0 mismatched cases`.
//
// Compared on their main path but with a side path read from the asm only (each comment lists what the cases did not take): the second round of func_08B1CA60, the wrap-around and clamp paths of func_08B2068C, the walk and recursion of func_08969F64 (flag 0 only),
// the main path of func_088A1BC8 (the virtual slot 0x7C always answered 0), the deeper walk of func_08BB1A00, the recursive halves of func_08BFA640 (func_08BFA07C only), the saturating cases of func_08B71DC8, the getter half of func_0880F470 / func_08835E1C /
// func_08828ABC (setter only) and of func_088342F4 / func_0884A5CC / func_0884A6DC (the other half), and func_08994994 / func_0899EF08 / func_089D30D4 (one mode each).
// Measured on a quiet host (load average 1-5): the usual route times, uncapped; func_08BFA640 with GT_DECOMP_DENSE (58 calls, 3 cases that changed memory).
//
// Every call_guest / tail_call passes the argument registers the original sets before the jal (a0-a3, t0-t3, f12-f14): the ones it leaves alone keep the caller's value, which call_guest copies from ctx.
// Every call goes through `call_keep` (the callee's v0, v1 and f0 stay in the registers like the original's), and the callee-saved registers (s0-s7, fp) hold the original's values around the calls (`SavedRegs`
// restores the entry values on the way out, like the original's epilogue): a callee may store the caller's s0 (FINDINGS, batch 84). The delay slot of a `jal` runs BEFORE the call: a move from v0 there captures the
// PREVIOUS call's result (func_08C4FF60, func_0883365C and func_08BCF61C read it that way). v0 (and v1 / f0 where a caller could read them) is set as the original leaves it on every path, including the early returns; the other caller-saved
// registers (a0-a3, t0-t9) are set to the original's value where it is cheap and a callee could read them, not otherwise (the gate compares v0, s0-s7, sp, ra and RAM).
//
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>

namespace gt::game {
namespace hot_batch_88 {
namespace {

using namespace gt::game::hot;

inline void take(psprecomp::AllegrexContext &ctx, const GuestResult &result) {
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}

// A guest call whose results stay in the registers like the original's: v0 / v1 / f0 of the callee are what the next instruction sees, and a callee that sets
// no v0 hands the previous one on to the code after it (the caller's own v0 is what call_guest starts the callee with).
inline GuestResult call_keep(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments,
                             std::initializer_list<float> float_arguments) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, float_arguments);
    take(ctx, result);
    return result;
}
inline std::uint32_t call_keep(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments) {
    return call_keep(rt, ctx, address, arguments, {}).v0;
}

// The callee-saved registers s0-s7 and fp as they were at entry: the original saves them in its frame and restores them before it returns or tail-calls.
class SavedRegs {
public:
    explicit SavedRegs(psprecomp::AllegrexContext &ctx) : ctx_(ctx) {
        for (unsigned i = 0; i < 8u; ++i) saved_[i] = ctx.gpr[16u + i];
        saved_fp_ = ctx.gpr[30];
    }
    ~SavedRegs() {
        for (unsigned i = 0; i < 8u; ++i) ctx_.gpr[16u + i] = saved_[i];
        ctx_.gpr[30] = saved_fp_;
    }
    SavedRegs(const SavedRegs &) = delete;
    SavedRegs &operator=(const SavedRegs &) = delete;

private:
    psprecomp::AllegrexContext &ctx_;
    std::uint32_t saved_[8];
    std::uint32_t saved_fp_;
};

inline std::int32_t signed_of(std::uint32_t value) { return static_cast<std::int32_t>(value); }
inline std::uint32_t byte_u(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load8(address); }
inline std::int8_t byte_at(psprecomp::Runtime &rt, std::uint32_t address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }
inline std::int16_t half_at(psprecomp::Runtime &rt, std::uint32_t address) { return static_cast<std::int16_t>(rt.memory().load16(address)); }

// The object o at `holder_slot` of a destructor, when set, has its vtable slot 0 called with (o, word at `value_address`) (the word the original reads in the delay slot of the call).
inline void drop_holder(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t holder_slot, std::uint32_t value_address) {
    const std::uint32_t object = word(rt, holder_slot);
    ctx.gpr[3] = object;
    ctx.gpr[4] = object;
    if (object == 0u) return;
    const std::uint32_t vtable = word(rt, object);
    const std::uint32_t function = word(rt, vtable);
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = function;
    const std::uint32_t argument = word(rt, value_address);
    ctx.gpr[5] = argument;
    (void)call_keep(rt, ctx, function, {object, argument});
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

inline std::uint32_t half_u(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load16(address); }

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// The release of a reference-counted body whose data pointer is `data` (body = data - 0xC, `shared` the shared empty body): unless it is the shared one, func_08B60804(data - 4, -1) takes a reference away and an answer <= 0 frees it with
// `free_function`(body, free_argument).
inline std::uint32_t release_body(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t data, std::uint32_t shared, std::uint32_t free_function, std::uint32_t free_argument,
                                  unsigned body_register) {
    const std::uint32_t body = data - 0xCu;
    ctx.gpr[body_register] = body;
    ctx.gpr[5] = 0xFFFFFFFFu;
    ctx.gpr[4] = data - 4u;
    if (body != shared) {
        const std::uint32_t references = call_keep(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu});
        ctx.gpr[4] = body;
        ctx.gpr[5] = free_argument;
        if (signed_of(references) <= 0) (void)call_keep(rt, ctx, free_function, {body, free_argument});
    }
    return body;
}

// The array destruction of an `operator delete[]`-style teardown: `array` is the first element, the element count sits at array[-1] (4 bytes before), each element of `stride` bytes has its vtable slot 0 called with (element) from the last to the first
// (the loop ends when the word at `slot` equals the element just destroyed, which is the first one), then func_08B61FC0(first - 4) frees the block. s0 = the element, s1 = the cursor, as in the original.
inline void destroy_elements(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t slot, std::uint32_t array, std::uint32_t stride) {
    const std::uint32_t count = word(rt, array - 4u);
    ctx.gpr[2] = count;
    const std::uint32_t end = array + count * stride;
    std::uint32_t cursor = end - stride;
    ctx.gpr[16] = end;
    ctx.gpr[3] = cursor;
    ctx.gpr[17] = cursor;
    std::uint32_t element = end;
    if (array != end) {
        for (;;) {
            element = cursor;
            const std::uint32_t vtable = word(rt, element);
            const std::uint32_t function = word(rt, vtable);
            ctx.gpr[16] = element;
            ctx.gpr[4] = element;
            ctx.gpr[2] = vtable;
            ctx.gpr[3] = function;
            cursor -= stride;
            ctx.gpr[17] = cursor;
            (void)call_keep(rt, ctx, function, {element});
            ctx.gpr[2] = word(rt, slot);
            ctx.gpr[3] = element - stride;
            if (word(rt, slot) == element) break;
        }
    }
    ctx.gpr[4] = element - 4u;
    (void)call_keep(rt, ctx, 0x08B61FC0u, {element - 4u});
}

// The release of a reference-counted string body that the temporary at `holder` points to (its data pointer p; body = p - 0xC; D_08D74FD0 is the shared empty body): unless it is the empty one,
// func_08B60804(p - 4, -1) takes a reference away and when that answers <= 0 func_08B8542C(body, sp) frees it. s0 holds the body and v0 the shared body for the compare (the function leaves it there when the body is shared).
inline void release_string(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t holder, std::uint32_t frame_sp) {
    const std::uint32_t data = word(rt, holder);
    const std::uint32_t body = data - 0xCu;
    ctx.gpr[3] = data;
    ctx.gpr[16] = body;
    ctx.gpr[2] = 0x08D74FD0u;
    ctx.gpr[5] = 0xFFFFFFFFu;
    ctx.gpr[4] = data - 4u;
    if (body != 0x08D74FD0u) {
        const std::uint32_t references = call_keep(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu});
        ctx.gpr[4] = body;
        ctx.gpr[5] = frame_sp;
        if (signed_of(references) <= 0) (void)call_keep(rt, ctx, 0x08B8542Cu, {body, frame_sp});
    }
}

// func_08B1CA60(this, value (a2:a3), count (t0)): the bit writer of a stream object. this[4] = the output pointer, this[8] = the byte being filled, this[0xC] = the bits already in it (0-7). The `count` top bits of the 64-bit value (the value is first
// shifted left by 64 - count) are appended, most significant first, a byte at a time: each round takes min(8 - bits, count left) bits; a full byte is stored at the pointer and the pointer moves on. After the last round a partly filled byte is stored at the
// pointer without moving it, and the three fields are written back. v0 = the byte being filled (low 8 bits).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a count that outgrows the byte in progress (`room < left`: the byte is completed and stored, the pointer advances, 0x08B1CB14-0x08B1CB20, and a second round runs) never occurred: every compared call added its bits to the byte in one round and only the partly filled byte was stored.
void func_08B1CA60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::int32_t count = signed_of(ctx.gpr[8]);
    std::uint32_t pending = byte_u(rt, self + 8u);
    std::uint32_t cursor = word(rt, self + 4u);
    std::uint32_t bits = word(rt, self + 0xCu);
    std::uint64_t value = (static_cast<std::uint64_t>(ctx.gpr[7]) << 32u | ctx.gpr[6]) << ((0x40u - ctx.gpr[8]) & 63u);
    std::int32_t left = count;
    if (left != 0) {
        do {
            const std::int32_t room = 8 - signed_of(bits);
            std::uint32_t chunk;
            if (room < left) {
                chunk = static_cast<std::uint32_t>(room);
                left -= room;
            } else {
                chunk = static_cast<std::uint32_t>(left);
                left = 0;
            }
            pending |= static_cast<std::uint32_t>(value >> ((bits + 0x38u) & 63u));
            bits += chunk;
            if (bits == 8u) {
                rt.memory().store8(cursor, static_cast<std::uint8_t>(pending));
                bits = 0u;
                ++cursor;
                pending = 0u;
            }
            value <<= (chunk & 63u);
        } while (left != 0);
    }
    ctx.gpr[2] = pending & 0xFFu;
    if (bits != 0u) rt.memory().store8(cursor, static_cast<std::uint8_t>(pending));
    put(rt, self + 0xCu, bits);
    put(rt, self + 4u, cursor);
    rt.memory().store8(self + 8u, static_cast<std::uint8_t>(ctx.gpr[2]));
}

// func_08B2068C(this, delta): a position this[0] moved by `delta` inside the range [this[4], this[8]) (empty range: nothing happens, v0 = 0). With this[0xC] set the position wraps around (this[0xD] = 1 when the new position was outside the range) by
// adding or subtracting the range length until it is inside; with this[0xC] clear it is clamped to this[4] .. this[8] - 1. v0 = 1 when the position changed, else 0.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the empty range (0x08B20694), the wrap-around mode (this[0xC] set: the outside flag at this[0xD], the add loop 0x08B206E8-0x08B206F8 and the subtract loop 0x08B2070C-0x08B20720) and both clamps (position below the low end 0x08B2072C, at or above the high end 0x08B20740) never ran, and the answer was always 0 (the position unchanged): the clamp mode was only compared with the position already inside the range -> the wrap and clamp paths are read from the asm only.
void func_08B2068C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::int32_t low = signed_of(word(rt, self + 4u)), high = signed_of(word(rt, self + 8u));
    ctx.gpr[7] = static_cast<std::uint32_t>(low);
    ctx.gpr[8] = static_cast<std::uint32_t>(high);
    if (low == high) {
        ctx.gpr[2] = 0u;
        return;
    }
    const std::int32_t old = signed_of(word(rt, self));
    std::int32_t position = static_cast<std::int32_t>(static_cast<std::uint32_t>(old) + ctx.gpr[5]);
    put(rt, self, static_cast<std::uint32_t>(position));
    if (byte_u(rt, self + 0xCu) != 0u) {
        rt.memory().store8(self + 0xDu, (position < low || position >= high) ? 1u : 0u);
        const std::uint32_t length = static_cast<std::uint32_t>(high) - static_cast<std::uint32_t>(low);
        if (position < low) {
            do {
                position = static_cast<std::int32_t>(static_cast<std::uint32_t>(position) + length);
                put(rt, self, static_cast<std::uint32_t>(position));
            } while (position < low);
        }
        if (position >= high) {
            position = static_cast<std::int32_t>(static_cast<std::uint32_t>(position) - length);
            while (position >= high) position = static_cast<std::int32_t>(static_cast<std::uint32_t>(position) - length);
            put(rt, self, static_cast<std::uint32_t>(position));
        }
    } else {
        if (position < low) {
            position = low;
            put(rt, self, static_cast<std::uint32_t>(low));
        }
        if (position >= high) {
            position = high - 1;
            put(rt, self, static_cast<std::uint32_t>(position));
        }
    }
    ctx.gpr[6] = static_cast<std::uint32_t>(position);
    ctx.gpr[2] = old != position ? 1u : 0u;
}

// func_088941A0(src_pointer, dest): copies the 0x24 bytes at *src_pointer to dest (words, four at a time, then one), then v0 = dest + func_08894020() (the size 48 of the object).
void func_088941A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t destination = ctx.gpr[5];
    SavedRegs saved(ctx);
    ctx.gpr[16] = destination;
    const std::uint32_t source = word(rt, ctx.gpr[4]);
    for (std::uint32_t at = 0u; at < 0x20u; at += 0x10u) {
        const std::uint32_t w0 = word(rt, source + at), w1 = word(rt, source + at + 4u), w2 = word(rt, source + at + 8u), w3 = word(rt, source + at + 0xCu);
        put(rt, destination + at, w0);
        put(rt, destination + at + 4u, w1);
        put(rt, destination + at + 8u, w2);
        put(rt, destination + at + 0xCu, w3);
        ctx.gpr[2] = w0;
        ctx.gpr[3] = w1;
        ctx.gpr[4] = w2;
        ctx.gpr[5] = w3;
    }
    const std::uint32_t last = word(rt, source + 0x20u);
    put(rt, destination + 0x20u, last);
    ctx.gpr[2] = last;
    ctx.gpr[6] = destination + 0x20u;
    ctx.gpr[7] = source + 0x20u;
    ctx.gpr[8] = source + 0x20u;
    const std::uint32_t size = call_keep(rt, ctx, 0x08894020u, {});
    ctx.gpr[2] = destination + size;
}

// func_088D5028(this, dest): size = func_088D4808(this); when size > 0x60, memset(dest, 0, size); then the 0x60 bytes at *this are copied to dest (words, four at a time); v0 = dest + size.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a size above 0x60 (the memset of the whole destination, 0x088D5054-0x088D505C) never occurred: the size was always 0x60 or less.
void func_088D5028(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], destination = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    SavedRegs saved(ctx);
    ctx.gpr[17] = self;
    ctx.gpr[16] = destination;
    const std::uint32_t size = call_keep(rt, ctx, 0x088D4808u, {self});
    ctx.gpr[18] = size;
    ctx.gpr[2] = size - 0x60u;
    if (signed_of(size - 0x60u) > 0) {
        ctx.gpr[4] = destination;
        ctx.gpr[5] = 0u;
        (void)call_keep(rt, ctx, 0x08B36018u, {destination, 0u, size});
    }
    const std::uint32_t source = word(rt, self);
    for (std::uint32_t at = 0u; at < 0x60u; at += 0x10u) {
        const std::uint32_t w0 = word(rt, source + at), w1 = word(rt, source + at + 4u), w2 = word(rt, source + at + 8u), w3 = word(rt, source + at + 0xCu);
        put(rt, destination + at, w0);
        put(rt, destination + at + 4u, w1);
        put(rt, destination + at + 8u, w2);
        put(rt, destination + at + 0xCu, w3);
    }
    ctx.gpr[2] = destination + size;
}

// func_088A1A3C(this, index, arg, flag): element = this[8] + 1072 * max(this[0x10] as halfwords [index], 0), 8 bytes in; with flag (low byte) set func_088A5814(element, arg) else func_088A6C48(element, arg); then func_088A6068(element, this[0x4C] + 1680 * k)
// (k the clamped index again); v0 = the element.
void func_088A1A3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], index = ctx.gpr[5], arg = ctx.gpr[6], flag = ctx.gpr[7] & 0xFFu;
    GuestFrame frame(ctx, 0x10u);
    SavedRegs saved(ctx);
    ctx.gpr[7] = flag;
    ctx.gpr[18] = self;
    const std::int32_t entry = half_at(rt, word(rt, self + 0x10u) + index * 2u);
    const std::uint32_t clamped = static_cast<std::uint32_t>(std::max(entry, 0));
    ctx.gpr[16] = clamped;
    const std::uint32_t element = word(rt, self + 8u) + clamped * 1072u + 8u;
    ctx.gpr[17] = element;
    ctx.gpr[4] = element;
    ctx.gpr[5] = arg;
    ctx.gpr[3] = 0u;
    ctx.gpr[8] = static_cast<std::uint32_t>(entry);
    if (flag != 0u) (void)call_keep(rt, ctx, 0x088A5814u, {element, arg});
    else (void)call_keep(rt, ctx, 0x088A6C48u, {element, arg});
    const std::uint32_t table = word(rt, self + 0x4Cu) + clamped * 1680u;
    ctx.gpr[3] = clamped << 4u;
    ctx.gpr[4] = word(rt, self + 0x4Cu);
    ctx.gpr[5] = table;
    (void)call_keep(rt, ctx, 0x088A6068u, {element, table});
    ctx.gpr[2] = element;
}

// func_088D7158(this, flag): v0 = 0 unless func_088D9E58(this) and func_088DA0BC(this) both answer non-zero; then, unless this[0x640] is set and flag (low byte) clear, func_088E4120(this[0x640]) runs (only when this[0x640] is set and the flag is clear), the byte this[0x130][0x13] = flag ^ 1,
// and when func_088DAA84(this) answers non-zero func_088D6FD4(this, 0, 0, -1) runs and this[0x1F] = 1, else func_088DDB6C(this, 0, 0, -1); v0 = 1.
void func_088D7158(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    SavedRegs saved(ctx);
    const std::uint32_t flag = ctx.gpr[5] & 0xFFu;
    ctx.gpr[17] = flag;
    ctx.gpr[16] = self;
    if (call_keep(rt, ctx, 0x088D9E58u, {self}) == 0u) {
        ctx.gpr[3] = 0u;
        ctx.gpr[2] = 0u;
        return;
    }
    ctx.gpr[4] = self;
    if (call_keep(rt, ctx, 0x088DA0BCu, {self}) == 0u) {
        ctx.gpr[3] = 0u;
        ctx.gpr[2] = 0u;
        return;
    }
    const std::uint32_t holder = word(rt, self + 0x640u);
    ctx.gpr[4] = holder;
    if (holder != 0u && flag == 0u) (void)call_keep(rt, ctx, 0x088E4120u, {holder});
    const std::uint32_t slot = word(rt, self + 0x130u);
    ctx.gpr[3] = slot;
    ctx.gpr[2] = flag ^ 1u;
    rt.memory().store8(slot + 0x13u, static_cast<std::uint8_t>(flag ^ 1u));
    ctx.gpr[4] = self;
    const std::uint32_t found = call_keep(rt, ctx, 0x088DAA84u, {self});
    ctx.gpr[4] = self;
    ctx.gpr[5] = 0u;
    ctx.gpr[6] = 0u;
    ctx.gpr[7] = 0xFFFFFFFFu;
    if (found != 0u) {
        (void)call_keep(rt, ctx, 0x088D6FD4u, {self, 0u, 0u, 0xFFFFFFFFu});
        rt.memory().store8(self + 0x1Fu, 1u);
    } else {
        (void)call_keep(rt, ctx, 0x088DDB6Cu, {self, 0u, 0u, 0xFFFFFFFFu});
    }
    ctx.gpr[3] = 1u;
    ctx.gpr[2] = 1u;
}

// func_088E96F4(this): for i in 0 .. func_088D9F80(this) (asked again every round): the halfwords this[0x138][i] and this[0x140][i] = i; then this[0x144] = 0, func_088E9614(this); then for i in 0 .. func_088D9F80(this):
// func_088D0924(func_088DA788(this, this[0x138][i]), i). v0 = 0 (the last compare).
void func_088E96F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    SavedRegs saved(ctx);
    ctx.gpr[18] = 0u;
    ctx.gpr[17] = 0u;
    ctx.gpr[16] = self;
    std::uint32_t index = 0u, offset = 0u;
    for (;;) {
        ctx.gpr[4] = self;
        const std::uint32_t count = call_keep(rt, ctx, 0x088D9F80u, {self});
        const bool more = signed_of(index) < signed_of(count);
        ctx.gpr[2] = more ? 1u : 0u;
        ctx.gpr[4] = index & 0xFFFFu;
        ++index;
        ctx.gpr[17] = index;
        if (!more) break;
        rt.memory().store16(word(rt, self + 0x138u) + offset, static_cast<std::uint16_t>(ctx.gpr[4]));
        rt.memory().store16(word(rt, self + 0x140u) + offset, static_cast<std::uint16_t>(ctx.gpr[4]));
        offset += 2u;
        ctx.gpr[18] = offset;
    }
    put(rt, self + 0x144u, 0u);
    ctx.gpr[4] = self;
    (void)call_keep(rt, ctx, 0x088E9614u, {self});
    index = 0u;
    ctx.gpr[17] = 0u;
    for (;;) {
        ctx.gpr[4] = self;
        const std::uint32_t count = call_keep(rt, ctx, 0x088D9F80u, {self});
        const bool more = signed_of(index) < signed_of(count);
        ctx.gpr[2] = more ? 1u : 0u;
        ctx.gpr[3] = index << 1u;
        ctx.gpr[4] = self;
        if (!more) break;
        const std::uint32_t entry = half_u(rt, word(rt, self + 0x138u) + (index << 1u));
        ctx.gpr[5] = entry;
        const std::uint32_t element = call_keep(rt, ctx, 0x088DA788u, {self, entry});
        ctx.gpr[4] = element;
        ctx.gpr[5] = index;
        (void)call_keep(rt, ctx, 0x088D0924u, {element, index});
        ++index;
        ctx.gpr[17] = index;
    }
}

// func_088F8BAC(this, out, max): walks the four 8-byte slots of this (slot i = this + 8 * i, skipping those whose bit i is set in the bit set at this + 0x2C); for each of the two object pointers at slot + 4 and slot + 8 that is set, the virtual slot 0x24 is called with
// (object); when it answers non-zero the counter n grows (the object is stored at out[n] when `out` is set and n < max, out advances either way). v0 = n.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a slot whose bit is set in the bit set at this + 0x2C (skipped, 0x088F8C14) never occurred, and the store into `out` was always taken (`out` set and n < max).
void func_088F8BAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], out = ctx.gpr[5], limit = ctx.gpr[6];
    GuestFrame frame(ctx, 0x40u);
    SavedRegs saved(ctx);
    ctx.gpr[20] = self;
    ctx.gpr[30] = out;
    ctx.gpr[22] = 0u;
    ctx.gpr[19] = 0u;
    std::uint32_t count = 0u, slot = self, index = 0u;
    std::uint32_t out_cursor = 0u;
    do {
        const std::uint32_t bits = word(rt, self + 0x2Cu + ((index >> 5u) << 2u));
        ctx.gpr[3] = (bits >> (index & 31u)) & 1u;
        if (ctx.gpr[3] == 0u) {
            out_cursor = out + count * 4u;
            ctx.gpr[21] = out_cursor;
            ctx.gpr[18] = 0u;
            ctx.gpr[23] = 2u;
            std::uint32_t cursor = slot + 4u;
            std::uint32_t object = word(rt, cursor);
            for (std::uint32_t round = 1u; round <= 2u; ++round) {
                ctx.gpr[17] = cursor + 4u;
                ctx.gpr[18] = round;
                ctx.gpr[16] = object;
                cursor += 4u;
                if (object != 0u) {
                    const std::uint32_t vtable = word(rt, object);
                    const std::uint32_t function = word(rt, vtable + 0x24u);
                    ctx.gpr[4] = object;
                    ctx.gpr[3] = vtable;
                    ctx.gpr[2] = function;
                    if (call_keep(rt, ctx, function, {object}) != 0u) {
                        const bool below = signed_of(count) < signed_of(limit);
                        ctx.gpr[3] = limit;
                        ctx.gpr[2] = below ? 1u : 0u;
                        ++count;
                        ctx.gpr[22] = count;
                        if (out != 0u && below) put(rt, out_cursor, object);
                        out_cursor += 4u;
                        ctx.gpr[21] = out_cursor;
                    }
                }
                if (round != 2u) object = word(rt, cursor);
            }
        }
        ++index;
        ctx.gpr[19] = index;
        slot += 8u;
        ctx.gpr[20] = slot;
    } while (index != 4u);
    ctx.gpr[2] = count;
}

// func_088F9008(this): the same walk over the four slots as func_088F8BAC (bit set at this + 0x2C skips a slot): for each of the two object pointers at slot + 4 and slot + 8 that is set, the virtual slot 0x3C is called with (object). v0 is the last
// load or call answer (a skipped slot leaves the address of its bit-set word).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a slot whose bit is set in the bit set at this + 0x2C (skipped, 0x088F9058) never occurred.
void func_088F9008(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[22] = 4u;
    ctx.gpr[21] = self + 0x2Cu;
    ctx.gpr[19] = self;
    ctx.gpr[18] = 0u;
    std::uint32_t index = 0u, slot = self;
    do {
        const std::uint32_t address = self + 0x2Cu + ((index >> 5u) << 2u);
        ctx.gpr[2] = address;
        ctx.gpr[3] = (word(rt, address) >> (index & 31u)) & 1u;
        if (ctx.gpr[3] == 0u) {
            std::uint32_t cursor = slot + 4u;
            ctx.gpr[16] = cursor;
            ctx.gpr[17] = 0u;
            ctx.gpr[20] = 2u;
            std::uint32_t object = word(rt, cursor);
            ctx.gpr[2] = object;
            for (std::uint32_t round = 1u; round <= 2u; ++round) {
                ctx.gpr[17] = round;
                cursor += 4u;
                ctx.gpr[16] = cursor;
                ctx.gpr[4] = object;
                if (object != 0u) {
                    const std::uint32_t vtable = word(rt, object);
                    const std::uint32_t function = word(rt, vtable + 0x3Cu);
                    ctx.gpr[2] = vtable;
                    ctx.gpr[3] = function;
                    (void)call_keep(rt, ctx, function, {object});
                }
                if (round != 2u) {
                    object = word(rt, cursor);
                    ctx.gpr[2] = object;
                }
            }
        }
        ++index;
        ctx.gpr[18] = index;
        slot += 8u;
        ctx.gpr[19] = slot;
    } while (index != 4u);
}

// func_08969F64(this, flag): kind = func_08965BD4(this); unless it is 3: func_08969B98(this, sp, sp+4, sp+8, sp+0xC) fills four floats, func_08966040(this, 3), and the four floats are copied (swapped in pairs: [sp+0] -> [sp+0x1C], [sp+4] -> [sp+0x18], [sp+8] -> [sp+0x14], [sp+0xC] -> [sp+0x10])
// before func_08969D2C(this, sp+0x1C, sp+0x18, sp+0x14, sp+0x10). With the flag (low byte) set and the virtual slot 0x19C of this answering non-zero, the chain this[0xF4], func_08968A38(node), ... is walked and func_08969F64(node, 1) runs on every node. v0 is the last call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the flag (second argument) was always 0: the virtual slot 0x19C, the walk over this[0xF4] / func_08968A38 and the recursive func_08969F64(node, 1) calls (0x08969FE4-0x0896A02C) never ran; a kind of 3 (the floats are skipped, 0x08969F84) never occurred.
void func_08969F64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    const std::uint32_t flag = ctx.gpr[5] & 0xFFu;
    ctx.gpr[17] = flag;
    ctx.gpr[16] = self;
    const std::uint32_t kind = call_keep(rt, ctx, 0x08965BD4u, {self});
    ctx.gpr[3] = 3u;
    if (kind != 3u) {
        ctx.gpr[4] = self;
        ctx.gpr[5] = frame.at(0u);
        ctx.gpr[6] = frame.at(4u);
        ctx.gpr[7] = frame.at(8u);
        ctx.gpr[8] = frame.at(0xCu);
        (void)call_keep(rt, ctx, 0x08969B98u, {self, frame.at(0u), frame.at(4u), frame.at(8u), frame.at(0xCu)});
        ctx.gpr[4] = self;
        ctx.gpr[5] = 3u;
        (void)call_keep(rt, ctx, 0x08966040u, {self, 3u});
        put(rt, frame.at(0x1Cu), word(rt, frame.at(0u)));
        put(rt, frame.at(0x18u), word(rt, frame.at(4u)));
        put(rt, frame.at(0x14u), word(rt, frame.at(8u)));
        put(rt, frame.at(0x10u), word(rt, frame.at(0xCu)));
        ctx.fpr[0] = float_at(rt, frame.at(8u));
        ctx.fpr[1] = float_at(rt, frame.at(0xCu));
        ctx.gpr[4] = self;
        ctx.gpr[5] = frame.at(0x1Cu);
        ctx.gpr[6] = frame.at(0x18u);
        ctx.gpr[7] = frame.at(0x14u);
        ctx.gpr[8] = frame.at(0x10u);
        (void)call_keep(rt, ctx, 0x08969D2Cu, {self, frame.at(0x1Cu), frame.at(0x18u), frame.at(0x14u), frame.at(0x10u)});
    }
    if (flag == 0u) return;
    const std::uint32_t vtable = word(rt, self);
    const std::uint32_t function = word(rt, vtable + 0x19Cu);
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = function;
    ctx.gpr[4] = self;
    if (call_keep(rt, ctx, function, {self}) == 0u) return;
    std::uint32_t node = word(rt, self + 0xF4u);
    ctx.gpr[16] = node;
    if (node == 0u) return;
    do {
        ctx.gpr[4] = node;
        ctx.gpr[5] = 1u;
        (void)call_keep(rt, ctx, 0x08969F64u, {node, 1u});
        ctx.gpr[4] = node;
        node = call_keep(rt, ctx, 0x08968A38u, {node});
        ctx.gpr[16] = node;
    } while (node != 0u);
}

// func_08972AB0(this, ref): a string temporary at sp+4 (vtable D_08CCD9C0, data D_08D74FD0 + 0xC (the shared empty body), a -1 and the address sp+1) is filled by the virtual slot 8 of *ref called with (ref, sp+4); the temporary's vtable becomes D_08CCD990 and its body is released
// (unless it is the shared empty one: func_08B60804(data - 4, -1) and, when that answers <= 0, func_08B8542C(body, sp)); then func_08965DDC(this, byte at sp+1). v0 = that call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the shared empty body (0x08972B20) and a remaining reference (func_08B60804 answering > 0, which skips the free, 0x08972B34) never occurred: the body was always released and freed.
void func_08972AB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], ref = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    ctx.gpr[16] = 0x08D74FD0u;
    ctx.gpr[18] = self;
    put(rt, frame.at(8u), 0x08D74FD0u + 0xCu);
    put(rt, frame.at(0xCu), 0xFFFFFFFFu);
    put(rt, frame.at(0x10u), frame.at(1u));
    put(rt, frame.at(4u), 0x08CCD9C0u);
    const std::uint32_t vtable = word(rt, ref);
    const std::uint32_t function = word(rt, vtable + 8u);
    ctx.gpr[4] = ref;
    ctx.gpr[5] = frame.at(4u);
    ctx.gpr[6] = vtable;
    ctx.gpr[3] = frame.at(1u);
    ctx.gpr[2] = function;
    (void)call_keep(rt, ctx, function, {ref, frame.at(4u)});
    const std::uint32_t data = word(rt, frame.at(8u));
    const std::uint32_t body = data - 0xCu;
    ctx.gpr[3] = data;
    ctx.gpr[17] = body;
    ctx.gpr[5] = 0xFFFFFFFFu;
    put(rt, frame.at(4u), 0x08CCD990u);
    ctx.gpr[2] = 0x08CCD990u;
    ctx.gpr[4] = data - 4u;
    if (body != 0x08D74FD0u) {
        const std::uint32_t references = call_keep(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu});
        ctx.gpr[4] = body;
        ctx.gpr[5] = frame.at(0u);
        if (signed_of(references) <= 0) (void)call_keep(rt, ctx, 0x08B8542Cu, {body, frame.at(0u)});
    }
    const std::uint32_t byte = byte_u(rt, frame.at(1u));
    ctx.gpr[5] = byte;
    ctx.gpr[4] = self;
    (void)call_keep(rt, ctx, 0x08965DDCu, {self, byte});
}

// func_088A1BC8(this, index): when this[0x50] is set its virtual slot 0x14 runs on it; k = halfword this[0x10][index] (< 0: return, v0 = the address of that halfword); the byte this[0xC][k] must be set (else v0 = 0); o = this[8] + 1072 * k must answer non-zero to its virtual slot
// 0x7C (else v0 = 0); then func_0894D5F0(this[0x54], index), o's virtual slot 0xE0 (o, 0), o[0x1E0] = o[0x1E4] = 0 and a tail call to func_0894D63C(this[0x54], index).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the virtual slot 0x7C of the object always answered 0, so everything after it (func_0894D5F0, the virtual slot 0xE0, the clears of o[0x1E0] / o[0x1E4] and the tail call to func_0894D63C, 0x088A1C5C-0x088A1C9C) never ran; a null this[0x50] holder, a negative slot (0x088A1C10) and a clear flag byte (0x088A1C34) never occurred either -> read from the asm only.
void func_088A1BC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], index = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        SavedRegs saved(ctx);
        ctx.gpr[18] = index;
        ctx.gpr[17] = self;
        const std::uint32_t holder = word(rt, self + 0x50u);
        ctx.gpr[2] = holder;
        if (holder != 0u) {
            const std::uint32_t vtable = word(rt, holder);
            const std::uint32_t function = word(rt, vtable + 0x14u);
            ctx.gpr[4] = holder;
            ctx.gpr[3] = function;
            ctx.gpr[2] = vtable;
            (void)call_keep(rt, ctx, function, {holder});
        }
        const std::uint32_t halves = word(rt, self + 0x10u);
        ctx.gpr[3] = halves;
        ctx.gpr[2] = halves + index * 2u;
        const std::int32_t slot = half_at(rt, halves + index * 2u);
        ctx.gpr[5] = static_cast<std::uint32_t>(slot);
        ctx.gpr[4] = static_cast<std::uint32_t>(slot) << 6u;
        if (slot < 0) return;
        const std::uint32_t flags = word(rt, self + 0xCu) + static_cast<std::uint32_t>(slot);
        ctx.gpr[3] = flags;
        const std::uint32_t offset = static_cast<std::uint32_t>(slot) * 1072u;
        ctx.gpr[4] = offset;
        ctx.gpr[2] = byte_u(rt, flags);
        if (ctx.gpr[2] == 0u) return;
        const std::uint32_t object = word(rt, self + 8u) + offset;
        ctx.gpr[16] = object;
        ctx.gpr[2] = word(rt, self + 8u);
        const std::uint32_t vtable = word(rt, object);
        const std::uint32_t function = word(rt, vtable + 0x7Cu);
        ctx.gpr[3] = vtable;
        ctx.gpr[2] = function;
        ctx.gpr[4] = object;
        if (call_keep(rt, ctx, function, {object}) == 0u) return;
        ctx.gpr[5] = index;
        ctx.gpr[4] = word(rt, self + 0x54u);
        (void)call_keep(rt, ctx, 0x0894D5F0u, {word(rt, self + 0x54u), index});
        const std::uint32_t object_vtable = word(rt, object);
        const std::uint32_t reset = word(rt, object_vtable + 0xE0u);
        ctx.gpr[3] = object_vtable;
        ctx.gpr[4] = object;
        ctx.gpr[2] = reset;
        ctx.gpr[5] = 0u;
        (void)call_keep(rt, ctx, reset, {object, 0u});
        put(rt, object + 0x1E0u, 0u);
        ctx.gpr[5] = index;
        put(rt, object + 0x1E4u, 0u);
        ctx.gpr[4] = word(rt, self + 0x54u);
    }
    tail_call(ctx, 0x0894D63Cu, {ctx.gpr[4], index});
}

// func_08AB9B04(): sorts the nodes of the list at D_08D7717C (each node: next at +0, an object at +4) into the list at D_08CFFA10 (its head at +4, nodes of 12 bytes: object, ?, next at +8): for every source node the position is the first list node whose object's virtual slot 8 answer
// is not below the source object's (unsigned); func_08B1FC10(D_08CFFA10, that node (or 0), source object + 0xC) inserts it. Then the virtual slot 0x10 runs on the object of every list node, and a tail call goes to func_08ABA264.
void func_08AB9B04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    {
        GuestFrame frame(ctx, 0x20u);
        SavedRegs saved(ctx);
        const std::uint32_t list = 0x08CFFA10u;
        std::uint32_t source = word(rt, 0x08D7717Cu);
        ctx.gpr[19] = source;
        ctx.gpr[20] = 0x08D00000u;
        if (source != 0u) {
            ctx.gpr[21] = list;
            std::uint32_t object = word(rt, source + 4u);
            for (;;) {
                ctx.gpr[18] = object;
                std::uint32_t vtable = word(rt, object);
                std::uint32_t function = word(rt, vtable + 8u);
                ctx.gpr[3] = vtable;
                ctx.gpr[2] = function;
                ctx.gpr[4] = object;
                const std::uint32_t key = call_keep(rt, ctx, function, {object});
                ctx.gpr[17] = key;
                std::uint32_t node = word(rt, list + 4u);
                ctx.gpr[16] = node;
                std::uint32_t before = node;
                if (node != 0u) {
                    for (;;) {
                        const std::uint32_t member = word(rt, node);
                        ctx.gpr[4] = member;
                        vtable = word(rt, member);
                        function = word(rt, vtable + 8u);
                        ctx.gpr[3] = vtable;
                        ctx.gpr[2] = function;
                        const std::uint32_t other = call_keep(rt, ctx, function, {member});
                        ctx.gpr[2] = key < other ? 1u : 0u;
                        before = node;
                        ctx.gpr[5] = before;
                        if (key < other) break;
                        node = word(rt, node + 8u);
                        ctx.gpr[16] = node;
                        before = node;
                        if (node == 0u) {
                            ctx.gpr[5] = 0u;
                            break;
                        }
                        ctx.gpr[4] = word(rt, node);
                    }
                } else {
                    ctx.gpr[5] = 0u;
                }
                ctx.gpr[6] = object + 0xCu;
                ctx.gpr[4] = list;
                (void)call_keep(rt, ctx, 0x08B1FC10u, {list, before, object + 0xCu});
                source = word(rt, source);
                ctx.gpr[19] = source;
                if (source == 0u) break;
                object = word(rt, source + 4u);
            }
        }
        ctx.gpr[2] = list;
        std::uint32_t node = word(rt, list + 4u);
        ctx.gpr[16] = node;
        if (node != 0u) {
            for (;;) {
                const std::uint32_t member = word(rt, node);
                ctx.gpr[4] = member;
                const std::uint32_t vtable = word(rt, member);
                const std::uint32_t function = word(rt, vtable + 0x10u);
                ctx.gpr[3] = vtable;
                ctx.gpr[2] = function;
                (void)call_keep(rt, ctx, function, {member});
                const std::uint32_t next = word(rt, node + 8u);
                ctx.gpr[3] = next;
                node = next;
                ctx.gpr[16] = node;
                if (next == 0u) break;
            }
        }
    }
    tail_call(ctx, 0x08ABA264u, {});
}

// func_08BB1A00(base, hole, length, value, compare): the sift-down half of a binary heap of 4-byte elements (the second half of std::__adjust_heap): child = 2 * hole + 2; while child < length: the larger child (compare(base[child], base[child - 1]) true
// picks child - 1) moves up into the hole; when length is even the last element has only a left child, which moves up as well; the walk ends with a tail call to func_08BB191C(base, hole, top, value, compare) (the sift-up half).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the walk always ended after its first level (the loop back `j 0x08BB1A50` at 0x08BB1A98 never ran, so the larger-child choice `child--` ran once at most) and the one-child case (child == length, 0x08BB1AB0-0x08BB1AC8) never occurred; a length at or below the first child (0x08BB1A44) never occurred either.
void func_08BB1A00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t base = ctx.gpr[4], top = ctx.gpr[5], length = ctx.gpr[6], value = ctx.gpr[7], compare = ctx.gpr[8];
    std::uint32_t hole = top;
    {
        GuestFrame frame(ctx, 0x30u);
        SavedRegs saved(ctx);
        ctx.gpr[18] = base;
        ctx.gpr[21] = top;
        ctx.gpr[20] = length;
        ctx.gpr[23] = value;
        ctx.gpr[22] = compare;
        std::uint32_t child = top * 2u + 2u;
        ctx.gpr[17] = child;
        ctx.gpr[3] = static_cast<std::uint32_t>(signed_of(child) < signed_of(length));
        if (signed_of(child) < signed_of(length)) {
            ctx.gpr[19] = top;
            std::uint32_t current = top;
            for (;;) {
                std::uint32_t cursor = base + child * 4u;
                ctx.gpr[16] = cursor;
                ctx.gpr[5] = word(rt, cursor - 4u);
                ctx.gpr[4] = word(rt, cursor);
                const std::uint32_t smaller = call_keep(rt, ctx, compare, {word(rt, cursor), word(rt, cursor - 4u)});
                ctx.gpr[3] = current << 2u;
                const std::uint32_t destination = (current << 2u) + base;
                ctx.gpr[5] = destination;
                if (smaller != 0u) {
                    --child;
                    ctx.gpr[17] = child;
                    cursor = base + child * 4u;
                    ctx.gpr[16] = cursor;
                }
                const std::uint32_t moved = word(rt, cursor);
                ctx.gpr[2] = moved;
                const std::uint32_t next = child * 2u + 2u;
                ctx.gpr[3] = next;
                ctx.gpr[4] = static_cast<std::uint32_t>(signed_of(next) < signed_of(length));
                current = child;
                ctx.gpr[19] = current;
                put(rt, destination, moved);
                if (!(signed_of(next) < signed_of(length))) {
                    ctx.gpr[5] = child;
                    hole = child;
                    child = next;
                    ctx.gpr[17] = child;
                    break;
                }
                child = next;
                ctx.gpr[17] = child;
            }
        }
        if (child == length) {
            const std::uint32_t address = child * 4u + base;
            ctx.gpr[2] = address;
            const std::uint32_t moved = word(rt, address - 4u);
            ctx.gpr[4] = moved;
            const std::uint32_t destination = (hole << 2u) + base;
            ctx.gpr[3] = destination;
            put(rt, destination, moved);
            hole = child - 1u;
            ctx.gpr[5] = hole;
        }
        ctx.gpr[4] = base;
    }
    tail_call(ctx, 0x08BB191Cu, {base, hole, top, value, compare});
}

// func_08BB9C24 / func_08BB9E4C (a family: only the second guarded static differs): the pointer a0 (null: v0 = 0) holds an object o; its virtual slot 8 answers a type key (null o: the key is the holder D_08D75878, a guarded static zeroed on first use
// through func_08B6441C / func_08B64494 with the guard D_08D75290); a second guarded static `second_object` (guard `second_guard`) is zeroed the same way; v0 = *a0 + 4 when the key equals that second static, else 0.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a null a0 (v0 = 0), a null first object (the guarded static holder D_08D75878, its acquire and zeroing, 0x08BB9C68-0x08BB9CA8), the first-use initialisation of the second guarded static (0x08BB9CC8-0x08BB9CE8) and a key different from the second static (v0 = 0, 0x08BB9CF0) never ran: the key was always the virtual answer and equal to the second static.
inline void type_key_cast(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t second_guard, std::uint32_t second_object) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[16] = self;
    if (self == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const std::uint32_t object = word(rt, self);
    ctx.gpr[4] = object;
    std::uint32_t key;
    if (object != 0u) {
        const std::uint32_t vtable = word(rt, object);
        const std::uint32_t function = word(rt, vtable + 8u);
        ctx.gpr[3] = vtable;
        ctx.gpr[2] = function;
        key = call_keep(rt, ctx, function, {object});
        ctx.gpr[17] = key;
    } else {
        const std::uint32_t guard = 0x08D75290u, holder = 0x08D75878u;
        ctx.gpr[3] = static_cast<std::uint32_t>(static_cast<std::int32_t>(byte_at(rt, guard)));
        ctx.gpr[17] = guard;
        key = holder;
        if (byte_at(rt, guard) == 0) {
            ctx.gpr[4] = guard;
            if (call_keep(rt, ctx, 0x08B6441Cu, {guard}) != 0u) {
                ctx.gpr[3] = 0x08D70000u;
                put(rt, holder + 4u, 0u);
                put(rt, holder + 8u, 0u);
                put(rt, holder, 0u);
                ctx.gpr[17] = holder;
                ctx.gpr[4] = guard;
                (void)call_keep(rt, ctx, 0x08B64494u, {guard});
            }
        }
        ctx.gpr[17] = holder;
    }
    ctx.gpr[2] = 0x08D70000u;
    ctx.gpr[3] = static_cast<std::uint32_t>(static_cast<std::int32_t>(byte_at(rt, second_guard)));
    ctx.gpr[18] = second_guard;
    ctx.gpr[19] = 0x08D70000u;
    if (byte_at(rt, second_guard) == 0) {
        ctx.gpr[4] = second_guard;
        if (call_keep(rt, ctx, 0x08B6441Cu, {second_guard}) != 0u) {
            put(rt, second_object + 8u, 0u);
            put(rt, second_object + 4u, 0u);
            put(rt, second_object, 0u);
            ctx.gpr[4] = second_guard;
            (void)call_keep(rt, ctx, 0x08B64494u, {second_guard});
        }
    }
    ctx.gpr[2] = second_object;
    if (ctx.gpr[17] != second_object) {
        ctx.gpr[2] = 0u;
        return;
    }
    ctx.gpr[2] = word(rt, self) + 4u;
}
void func_08BB9C24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { type_key_cast(rt, ctx, 0x08D76050u, 0x08D76084u); }
void func_08BB9E4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { type_key_cast(rt, ctx, 0x08D76048u, 0x08D76078u); }

// func_08BCF9E4(a0, a1, a2, a3, t0, pair): the shared pointer `pair` (pointer, counted body) is copied to sp (a reference of the body taken, ll / sc), func_08961928(a1, a2, D_088FEFC0, a0, a3, t0, sp) runs, and the copy is released: the strong counter at
// body + 4 is decremented and, when it was 1, the virtual slot 8 runs on the body, then the weak counter at body + 8 is decremented and, when it was 1, the slot 0xC. v0 is the last call's, or 1 after a counter that did not reach zero.
void func_08BCF9E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a0 = ctx.gpr[4], a1 = ctx.gpr[5], a2 = ctx.gpr[6], a3 = ctx.gpr[7], t0 = ctx.gpr[8], pair = ctx.gpr[9];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[11] = a0;
    ctx.gpr[10] = a3;
    const std::uint32_t body = word(rt, pair + 4u);
    const std::uint32_t pointer = word(rt, pair);
    ctx.gpr[3] = body;
    ctx.gpr[2] = pointer;
    ctx.gpr[9] = t0;
    put(rt, frame.at(4u), body);
    put(rt, frame.at(0u), pointer);
    if (body != 0u) {
        ctx.gpr[3] = body + 4u;
        put(rt, body + 4u, word(rt, body + 4u) + 1u);
        ctx.gpr[2] = 1u;
    }
    ctx.gpr[4] = a1;
    ctx.gpr[5] = a2;
    ctx.gpr[6] = 0x088FEFC0u;
    ctx.gpr[8] = a3;
    ctx.gpr[7] = a0;
    ctx.gpr[10] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08961928u, {a1, a2, 0x088FEFC0u, a0, a3, t0, frame.at(0u)});
    const std::uint32_t shared = word(rt, frame.at(4u));
    ctx.gpr[16] = shared;
    if (shared == 0u) return;
    ctx.gpr[3] = shared + 4u;
    const std::uint32_t strong = word(rt, shared + 4u);
    put(rt, shared + 4u, strong - 1u);
    ctx.gpr[4] = strong;
    ctx.gpr[2] = 1u;
    if (strong != 1u) return;
    const std::uint32_t vtable = word(rt, shared);
    const std::uint32_t dispose = word(rt, vtable + 8u);
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = dispose;
    ctx.gpr[4] = shared;
    (void)call_keep(rt, ctx, dispose, {shared});
    ctx.gpr[3] = shared + 8u;
    const std::uint32_t weak = word(rt, shared + 8u);
    put(rt, shared + 8u, weak - 1u);
    ctx.gpr[4] = weak;
    ctx.gpr[2] = 1u;
    if (weak != 1u) return;
    const std::uint32_t final_vtable = word(rt, shared);
    const std::uint32_t destroy = word(rt, final_vtable + 0xCu);
    ctx.gpr[3] = final_vtable;
    ctx.gpr[2] = destroy;
    ctx.gpr[4] = shared;
    (void)call_keep(rt, ctx, destroy, {shared});
}

// func_08B958E8(a0, object): a null object does nothing. Otherwise object[0x78], when set, has its virtual slot 0x2C called and is cleared; the string bodies behind object[0x134] (func_08B60804(data - 4, -1), freed with func_08B93554(body, sp + 1) when that
// answers <= 0) and object[0x130] (freed with func_08B93554(body, sp)) are released unless they are the shared empty body D_08D75298; then object[0x90] = D_08CD71E8, func_08AF484C(object + 0x90), func_0886FCA4(object + 0x64), func_088733F4(object) and
// func_08B64654(object) (the deleting tail). v0 is the last call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a null object, an object[0x78] that is set (its virtual slot 0x2C and the clear), and non-shared string bodies (the func_08B60804 releases and the func_08B93554 frees, 0x08B95934-0x08B95958 and 0x08B95964-0x08B9598C) never occurred: both strings were the shared empty body.
void func_08B958E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[16] = object;
    if (object == 0u) return;
    const std::uint32_t holder = word(rt, object + 0x78u);
    ctx.gpr[4] = holder;
    if (holder != 0u) {
        const std::uint32_t vtable = word(rt, holder);
        const std::uint32_t function = word(rt, vtable + 0x2Cu);
        ctx.gpr[3] = vtable;
        ctx.gpr[2] = function;
        (void)call_keep(rt, ctx, function, {holder});
        put(rt, object + 0x78u, 0u);
    }
    std::uint32_t data = word(rt, object + 0x134u);
    ctx.gpr[18] = 0x08D75298u;
    std::uint32_t body = data - 0xCu;
    ctx.gpr[17] = body;
    ctx.gpr[4] = data;
    if (body != 0x08D75298u) {
        ctx.gpr[4] = data - 4u;
        ctx.gpr[5] = 0xFFFFFFFFu;
        const std::uint32_t references = call_keep(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu});
        if (signed_of(references) <= 0) {
            ctx.gpr[4] = body;
            ctx.gpr[5] = frame.at(1u);
            (void)call_keep(rt, ctx, 0x08B93554u, {body, frame.at(1u)});
        }
    }
    data = word(rt, object + 0x130u);
    ctx.gpr[4] = data;
    body = data - 0xCu;
    ctx.gpr[17] = body;
    ctx.gpr[2] = 0x08CD0000u;
    if (body != 0x08D75298u) {
        ctx.gpr[4] = data - 4u;
        ctx.gpr[5] = 0xFFFFFFFFu;
        const std::uint32_t references = call_keep(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu});
        ctx.gpr[2] = 0x08CD0000u;
        if (signed_of(references) <= 0) {
            ctx.gpr[4] = body;
            ctx.gpr[5] = frame.at(0u);
            (void)call_keep(rt, ctx, 0x08B93554u, {body, frame.at(0u)});
            ctx.gpr[2] = 0x08CD0000u;
        }
    }
    ctx.gpr[2] = 0x08CD71E8u;
    put(rt, object + 0x90u, 0x08CD71E8u);
    ctx.gpr[4] = object + 0x90u;
    (void)call_keep(rt, ctx, 0x08AF484Cu, {object + 0x90u});
    ctx.gpr[4] = object + 0x64u;
    (void)call_keep(rt, ctx, 0x0886FCA4u, {object + 0x64u});
    ctx.gpr[4] = object;
    (void)call_keep(rt, ctx, 0x088733F4u, {object});
    ctx.gpr[4] = object;
    (void)call_keep(rt, ctx, 0x08B64654u, {object});
}

// func_08BFA640(first, last, a2, limit): the recursive half of a stable sort of 8-byte elements: middle = first + 8 * ((n + 1) / 2) (n the element count of [first, last)). When limit < (n + 1) / 2 both halves recurse (first..middle, middle..last); otherwise
// func_08BFA07C sorts each. A tail call to func_08BFA3E0(first, middle, last, (middle - first) / 8, (last - middle) / 8, a2, limit, middle) merges them.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the recursive halves (limit < half: the two self calls at 0x08BFA698 and 0x08BFA6B0) never ran with the gate's sample; the dense run (GT_DECOMP_DENSE) compared all 58 calls, only the func_08BFA07C path.
void func_08BFA640(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t first = ctx.gpr[4], last = ctx.gpr[5], a2 = ctx.gpr[6], limit = ctx.gpr[7];
    std::uint32_t middle, left_bytes;
    {
        GuestFrame frame(ctx, 0x20u);
        SavedRegs saved(ctx);
        const std::int32_t count = (signed_of(last - first) >> 3) + 1;
        const std::int32_t half = (count + static_cast<std::int32_t>(static_cast<std::uint32_t>(count) >> 31u)) >> 1;
        ctx.gpr[2] = static_cast<std::uint32_t>(signed_of(limit) < half);
        ctx.gpr[3] = static_cast<std::uint32_t>(half) << 3u;
        middle = first + (static_cast<std::uint32_t>(half) << 3u);
        ctx.gpr[16] = middle;
        ctx.gpr[20] = limit;
        ctx.gpr[19] = a2;
        ctx.gpr[18] = last;
        ctx.gpr[17] = first;
        ctx.gpr[5] = middle;
        if (signed_of(limit) < half) {
            (void)call_keep(rt, ctx, 0x08BFA640u, {first, middle, a2, limit});
            ctx.gpr[4] = middle;
            ctx.gpr[5] = last;
            ctx.gpr[6] = a2;
            ctx.gpr[7] = limit;
            ctx.gpr[8] = middle;
            (void)call_keep(rt, ctx, 0x08BFA640u, {middle, last, a2, limit, middle});
        } else {
            (void)call_keep(rt, ctx, 0x08BFA07Cu, {first, middle});
            ctx.gpr[4] = middle;
            ctx.gpr[5] = last;
            ctx.gpr[6] = a2;
            ctx.gpr[7] = middle;
            (void)call_keep(rt, ctx, 0x08BFA07Cu, {middle, last, a2, middle});
        }
        left_bytes = middle - first;
        ctx.gpr[8] = last - middle;
        ctx.gpr[4] = first;
        ctx.gpr[5] = middle;
        ctx.gpr[6] = last;
        ctx.gpr[9] = a2;
        ctx.gpr[10] = limit;
        ctx.gpr[11] = middle;
    }
    tail_call(ctx, 0x08BFA3E0u, {first, middle, last, static_cast<std::uint32_t>(signed_of(left_bytes) >> 3), static_cast<std::uint32_t>(signed_of(last - middle) >> 3), a2, limit, middle});
}

// func_08C4FF60(data, length, key_a, key_b): a stream cipher over `data`: for each whole word (while length >= 4): word' = bswap(bswap(word) ^ func_08B1EEF0(key_a)) - ...: concretely s0 = func_08B1EEF0(key_a); s1 = bswap(*data); s0 = (s0 ^ s1) - func_08B3A37C(key_b); *data = bswap(s0);
// the remaining 1-3 bytes use the same two calls per byte: byte' = (byte ^ func_08B1EEF0(key_a)) - func_08B3A37C(key_b). v0 = 1 after the word loop (the last length compare), or the last func_08B3A37C answer after the byte loop.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a length below 4 (the word loop skipped, 0x08C4FF90) and a length that is a multiple of 4 (no byte tail, 0x08C4FFD4) never occurred.
void func_08C4FF60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t key_a = ctx.gpr[6], key_b = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    std::uint32_t cursor = ctx.gpr[4], length = ctx.gpr[5];
    ctx.gpr[21] = key_a;
    ctx.gpr[20] = key_b;
    ctx.gpr[19] = length;
    ctx.gpr[18] = cursor;
    ctx.gpr[2] = length < 4u ? 1u : 0u;
    if (length >= 4u) {
        for (;;) {
            ctx.gpr[4] = key_a;
            const std::uint32_t mask = call_keep(rt, ctx, 0x08B1EEF0u, {key_a});
            ctx.gpr[17] = word(rt, cursor);
            ctx.gpr[16] = mask;
            ctx.gpr[4] = key_b;
            const std::uint32_t swapped = __builtin_bswap32(word(rt, cursor));
            ctx.gpr[17] = swapped;
            const std::uint32_t step = call_keep(rt, ctx, 0x08B3A37Cu, {key_b});
            ctx.gpr[16] = __builtin_bswap32((mask ^ swapped) - step);
            length -= 4u;
            ctx.gpr[19] = length;
            ctx.gpr[2] = length < 4u ? 1u : 0u;
            put(rt, cursor, ctx.gpr[16]);
            cursor += 4u;
            ctx.gpr[18] = cursor;
            if (length < 4u) break;
        }
    }
    if (length == 0u) return;
    const std::uint32_t end = cursor + length;
    ctx.gpr[19] = end;
    std::uint32_t current = byte_u(rt, cursor);
    for (;;) {
        ctx.gpr[16] = current;
        ctx.gpr[4] = key_a;
        const std::uint32_t mask = call_keep(rt, ctx, 0x08B1EEF0u, {key_a});
        ctx.gpr[17] = mask;
        ctx.gpr[4] = key_b;
        const std::uint32_t step = call_keep(rt, ctx, 0x08B3A37Cu, {key_b});
        ctx.gpr[16] = (current ^ mask) - step;
        rt.memory().store8(cursor, static_cast<std::uint8_t>(ctx.gpr[16]));
        ++cursor;
        ctx.gpr[18] = cursor;
        if (end == cursor) break;
        current = byte_u(rt, cursor);
    }
}

// func_08BCF61C(this): when this[0x538] is set, func_08900148(this + 0x78) runs and the store in its jal delay slot puts the previous v0 (the this[0x538] flag) at sp, not its result, func_08BCF5D8(sp, this + 0x78) and func_08900350(this + 0x78) run; then func_0890633C(this + 0x34, this + 0x6C) and func_08906378(this + 0x34, this + 0x6C); then the ring
// list headed at this + 0x540 is walked: for every node whose byte node[0xC] is clear, node[0xC] = func_08BBD828(this, (virtual slot 8 of node[8])(node[8]), node[8]). v0 is the last flag byte or call answer.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): this[0x538] clear (the preamble skipped, 0x08BCF638), an empty node list (0x08BCF68C), a node whose flag byte is already set (the skip at 0x08BCF69C) and a second node never occurred: the list always held one unflagged node.
void func_08BCF61C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    const std::uint32_t flag = word(rt, self + 0x538u);
    ctx.gpr[2] = flag;
    ctx.gpr[18] = self;
    if (flag != 0u) {
        const std::uint32_t part = self + 0x78u;
        ctx.gpr[16] = part;
        ctx.gpr[4] = part;
        put(rt, frame.at(0u), flag);
        const std::uint32_t state = call_keep(rt, ctx, 0x08900148u, {part});
        (void)state;
        ctx.gpr[4] = frame.at(0u);
        ctx.gpr[5] = part;
        (void)call_keep(rt, ctx, 0x08BCF5D8u, {frame.at(0u), part});
        ctx.gpr[4] = part;
        (void)call_keep(rt, ctx, 0x08900350u, {part});
    }
    const std::uint32_t first = self + 0x34u, second = self + 0x6Cu;
    ctx.gpr[16] = first;
    ctx.gpr[17] = second;
    ctx.gpr[4] = first;
    (void)call_keep(rt, ctx, 0x0890633Cu, {first, second});
    ctx.gpr[4] = first;
    ctx.gpr[5] = second;
    (void)call_keep(rt, ctx, 0x08906378u, {first, second});
    std::uint32_t node = word(rt, self + 0x540u);
    const std::uint32_t head = self + 0x540u;
    ctx.gpr[3] = node;
    ctx.gpr[19] = head;
    if (node == head) return;
    ctx.gpr[16] = node;
    for (;;) {
        ctx.gpr[17] = node + 8u;
        const std::uint32_t done = byte_u(rt, node + 0xCu);
        ctx.gpr[2] = done;
        if (done != 0u) {
            ctx.gpr[16] = word(rt, node);
        } else {
            const std::uint32_t object = word(rt, node + 8u);
            const std::uint32_t vtable = word(rt, object);
            const std::uint32_t function = word(rt, vtable + 8u);
            ctx.gpr[4] = object;
            ctx.gpr[3] = vtable;
            ctx.gpr[2] = function;
            const std::uint32_t answer = call_keep(rt, ctx, function, {object});
            const std::uint32_t argument = word(rt, ctx.gpr[16] + 8u);
            ctx.gpr[6] = argument;
            ctx.gpr[4] = self;
            ctx.gpr[5] = answer;
            const std::uint32_t result = call_keep(rt, ctx, 0x08BBD828u, {self, answer, argument});
            rt.memory().store8(node + 0xCu, static_cast<std::uint8_t>(result));
            ctx.gpr[16] = word(rt, ctx.gpr[16]);
        }
        node = ctx.gpr[16];
        ctx.gpr[3] = node;
        if (node == head) break;
    }
}

// func_08B71DC8(low, high): a double (the two words in a0, a1) to a 32-bit integer, saturating: func_08B72314(sp, sp + 8) splits it into class [sp+8], sign [sp+0xC], exponent [sp+0x10] and a 64-bit mantissa [sp+0x18..0x20]. Classes 0, 1 and 2 give 0; class 4 and
// exponents of 31 or more saturate (0x80000000 for a negative value, 0x7FFFFFFF otherwise); a negative exponent gives 0; otherwise v0 = (mantissa >> (60 - exponent)) low word, negated for a negative sign.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): classes 0-2 (v0 = 0, 0x08B71DEC / 0x08B71DF4), class 4 and exponents of 31 or more (saturation, 0x08B71E0C-0x08B71E10 / 0x08B71E88) and a negative sign (the negation, 0x08B71E7C) never occurred.
void func_08B71DC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    put(rt, frame.at(0u), ctx.gpr[4]);
    ctx.gpr[4] = frame.at(0u);
    put(rt, frame.at(4u), ctx.gpr[5]);
    ctx.gpr[5] = frame.at(8u);
    (void)call_keep(rt, ctx, 0x08B72314u, {frame.at(0u), frame.at(8u)});
    const std::uint32_t kind = word(rt, frame.at(8u));
    ctx.gpr[3] = kind;
    ctx.gpr[2] = 2u;
    if (kind == 2u || kind < 2u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const std::uint32_t sign = word(rt, frame.at(0xCu));
    const std::int32_t exponent = signed_of(word(rt, frame.at(0x10u)));
    ctx.gpr[4] = static_cast<std::uint32_t>(exponent);
    bool saturate = kind == 4u;
    if (!saturate) {
        if (exponent < 0) {
            ctx.gpr[2] = 0u;
            return;
        }
        saturate = exponent >= 0x1F;
    }
    if (saturate) {
        ctx.gpr[2] = sign != 0u ? 0x80000000u : 0x7FFFFFFFu;
        return;
    }
    const std::uint32_t shift = 0x3Cu - static_cast<std::uint32_t>(exponent);
    const std::uint64_t mantissa = static_cast<std::uint64_t>(word(rt, frame.at(0x1Cu))) << 32u | word(rt, frame.at(0x18u));
    const std::uint32_t value = static_cast<std::uint32_t>(mantissa >> (shift & 63u));
    ctx.gpr[2] = sign == 0u ? value : 0u - value;
}

// func_08B8A8A8(this, list, forward): list is a vector of 24-byte elements (count = bytes / 24); for i in 0 ..< count the element (i when `forward` is non-zero, else count - 1 - i) is turned into the holder at sp by func_08A5DEE8(sp, list[0], element[0], element[1]),
// copied into this[0]'s array slot i (this[0][0x14] + 4 * i) by func_08A5BAD8(slot, [sp]) unless that slot is sp itself, and func_08A5B9EC(sp) destroys the holder. v0 = the last call's (0xAAAAAAAB for an empty list: the constant of the division by 3).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): an empty list (v0 = 0xAAAAAAAB), the reversed order (flag 0: `movz` picks the last element first, 0x08B8A91C), the target slot being the holder at sp (the copy skipped, 0x08B8A950) and lists of more than one element never occurred.
void func_08B8A8A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], list = ctx.gpr[5], forward = ctx.gpr[6] & 0xFFu;
    GuestFrame frame(ctx, 0x40u);
    SavedRegs saved(ctx);
    ctx.gpr[22] = self;
    ctx.gpr[20] = list;
    ctx.gpr[2] = 0xAAAAAAABu;
    const std::uint32_t begin = word(rt, list);
    const std::uint32_t count = static_cast<std::uint32_t>(signed_of(word(rt, list + 4u) - begin) >> 3) * 0xAAAAAAABu;
    ctx.gpr[19] = count;
    ctx.gpr[3] = static_cast<std::uint32_t>(signed_of(word(rt, list + 4u) - begin) >> 3);
    ctx.gpr[5] = begin;
    ctx.gpr[21] = forward;
    if (count == 0u) return;
    ctx.gpr[23] = frame.at(0u);
    std::uint32_t reverse = count - 1u, index = 0u;
    ctx.gpr[17] = reverse;
    ctx.gpr[18] = 0u;
    for (;;) {
        const std::uint32_t vector_begin = word(rt, list);
        const std::uint32_t chosen = forward == 0u ? reverse : index;
        const std::uint32_t object = word(rt, self);
        const std::uint32_t element = chosen * 24u + vector_begin;
        const std::uint32_t slot = word(rt, object + 0x14u) + index * 4u;
        ctx.gpr[16] = slot;
        ctx.gpr[5] = vector_begin;
        ctx.gpr[6] = word(rt, element);
        ctx.gpr[7] = word(rt, element + 4u);
        ctx.gpr[4] = frame.at(0u);
        ctx.gpr[3] = chosen * 24u + 0u;
        (void)call_keep(rt, ctx, 0x08A5DEE8u, {frame.at(0u), vector_begin, word(rt, element), word(rt, element + 4u)});
        ++index;
        ctx.gpr[18] = index;
        ctx.gpr[4] = slot;
        if (slot != frame.at(0u)) (void)call_keep(rt, ctx, 0x08A5BAD8u, {slot, word(rt, frame.at(0u))});
        ctx.gpr[4] = frame.at(0u);
        (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
        const bool more = index < count;
        ctx.gpr[2] = more ? 1u : 0u;
        --reverse;
        ctx.gpr[17] = reverse;
        if (!more) break;
    }
}

// func_08B893E8 / func_08B894E4 (a family: the element size 1 << Shift, the largest element count, the range copy and the range destroy differ): std::vector<T>::reserve(this, n) with an allocator (func_08A5BC20(bytes, 4) / func_08A5BC54(block, bytes, 4)).
// n above the largest count calls func_08B5ADDC(D_08C986E4) (the length_error throw: it does not return); when the capacity (this[2] - this[0]) >> Shift is below n: a new block of n elements is allocated, [this[0], this[1]) is copied into it by copy(begin, end, block, sp) and
// the old elements destroyed by destroy(this[0], this[1], sp + 1), the old block is freed when set, and this[0..2] = (block, block + size, block + n elements). v0 = the new end, or the capacity compare (0) when nothing had to grow.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the length_error throw (n above the largest count, 0x08B89414), a request that fits the capacity (0x08B8943C) and a vector that already owned a block (the func_08A5BC54 free, 0x08B89494) never occurred: every call grew an empty vector.
template <unsigned Shift>
inline void vector_reserve(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t maximum, std::uint32_t copy, std::uint32_t destroy) {
    const std::uint32_t self = ctx.gpr[4], count = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    ctx.gpr[17] = self;
    ctx.gpr[2] = maximum < count ? 1u : 0u;
    if (maximum < count) {
        ctx.gpr[4] = 0x08C986E4u;
        (void)call_keep(rt, ctx, 0x08B5ADDCu, {0x08C986E4u});
        return;
    }
    const std::uint32_t begin = word(rt, self);
    ctx.gpr[18] = begin;
    const std::uint32_t capacity = static_cast<std::uint32_t>(signed_of(word(rt, self + 8u) - begin) >> Shift);
    ctx.gpr[2] = capacity < count ? 1u : 0u;
    if (!(capacity < count)) return;
    const std::uint32_t end = word(rt, self + 4u);
    ctx.gpr[16] = end;
    const std::uint32_t bytes = count << Shift;
    ctx.gpr[20] = bytes;
    ctx.gpr[2] = end - begin;
    ctx.gpr[19] = 0u;
    ctx.gpr[21] = static_cast<std::uint32_t>(signed_of(end - begin) >> Shift);
    ctx.gpr[4] = bytes;
    ctx.gpr[5] = 4u;
    const std::uint32_t block = call_keep(rt, ctx, 0x08A5BC20u, {bytes, 4u});
    ctx.gpr[6] = block;
    ctx.gpr[4] = begin;
    ctx.gpr[5] = end;
    ctx.gpr[7] = frame.at(0u);
    ctx.gpr[19] = block;
    (void)call_keep(rt, ctx, copy, {begin, end, block, frame.at(0u)});
    const std::uint32_t old_begin = word(rt, self), old_end = word(rt, self + 4u);
    ctx.gpr[4] = old_begin;
    ctx.gpr[5] = old_end;
    ctx.gpr[6] = frame.at(1u);
    (void)call_keep(rt, ctx, destroy, {old_begin, old_end, frame.at(1u)});
    const std::uint32_t old = word(rt, self);
    const std::uint32_t old_bytes = static_cast<std::uint32_t>(signed_of(word(rt, self + 8u) - old) >> Shift) << Shift;
    ctx.gpr[4] = old;
    ctx.gpr[2] = static_cast<std::uint32_t>(signed_of(word(rt, self + 8u) - old) >> Shift);
    if (old != 0u) {
        ctx.gpr[5] = old_bytes;
        ctx.gpr[6] = 4u;
        (void)call_keep(rt, ctx, 0x08A5BC54u, {old, old_bytes, 4u});
    }
    const std::uint32_t used = ctx.gpr[21] << Shift;
    ctx.gpr[2] = block + used;
    ctx.gpr[3] = block + bytes;
    put(rt, self + 8u, block + bytes);
    put(rt, self + 4u, block + used);
    put(rt, self, block);
}
void func_08B893E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { vector_reserve<5u>(rt, ctx, 0x07FFFFFFu, 0x08B89238u, 0x08B891ACu); }
void func_08B894E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { vector_reserve<4u>(rt, ctx, 0x0FFFFFFFu, 0x08B891ECu, 0x08B891BCu); }

// func_08B904D8(this, n): the same reserve for 16-byte elements with the other allocator (func_08B35DA0(bytes) allocates, func_08B35254(block) frees), the range copy func_08B9045C(begin, end, block, sp) and the destroy func_08B8FE44(this[0], this[1], sp + 1); n above 0x0FFFFFFF
// calls func_08B5ADDC(D_08C9F59C) (the length_error throw). A request of 0 elements allocates nothing (the block stays 0). v0 = the new end, or the capacity compare (0) when nothing had to grow.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the length_error throw (0x08B90504), a request that fits the capacity (0x08B9052C), an empty request (no allocation, 0x08B90540) and a vector that already owned a block (the func_08B35254 free, 0x08B90580) never occurred: every call grew an empty vector.
void func_08B904D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], count = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    ctx.gpr[16] = self;
    ctx.gpr[2] = 0x0FFFFFFFu < count ? 1u : 0u;
    if (0x0FFFFFFFu < count) {
        ctx.gpr[4] = 0x08C9F59Cu;
        (void)call_keep(rt, ctx, 0x08B5ADDCu, {0x08C9F59Cu});
        return;
    }
    const std::uint32_t begin = word(rt, self);
    ctx.gpr[17] = begin;
    const std::uint32_t capacity = static_cast<std::uint32_t>(signed_of(word(rt, self + 8u) - begin) >> 4u);
    ctx.gpr[2] = capacity < count ? 1u : 0u;
    ctx.gpr[19] = 0u;
    if (!(capacity < count)) return;
    const std::uint32_t end = word(rt, self + 4u);
    ctx.gpr[18] = end;
    ctx.gpr[20] = 0u;
    ctx.gpr[2] = end - begin;
    const std::uint32_t used = static_cast<std::uint32_t>(signed_of(end - begin) >> 4u);
    ctx.gpr[21] = used;
    std::uint32_t block = 0u, bytes = 0u;
    if (count != 0u) {
        bytes = count << 4u;
        ctx.gpr[20] = bytes;
        ctx.gpr[4] = bytes;
        block = call_keep(rt, ctx, 0x08B35DA0u, {bytes});
        ctx.gpr[19] = block;
    }
    ctx.gpr[4] = begin;
    ctx.gpr[5] = end;
    ctx.gpr[6] = block;
    ctx.gpr[7] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08B9045Cu, {begin, end, block, frame.at(0u)});
    const std::uint32_t old_begin = word(rt, self), old_end = word(rt, self + 4u);
    ctx.gpr[4] = old_begin;
    ctx.gpr[5] = old_end;
    ctx.gpr[6] = frame.at(1u);
    (void)call_keep(rt, ctx, 0x08B8FE44u, {old_begin, old_end, frame.at(1u)});
    const std::uint32_t old = word(rt, self);
    ctx.gpr[4] = old;
    if (old != 0u) {
        ctx.gpr[2] = used << 4u;
        (void)call_keep(rt, ctx, 0x08B35254u, {old});
    }
    ctx.gpr[2] = used << 4u;
    ctx.gpr[3] = block + bytes;
    put(rt, self + 8u, block + bytes);
    put(rt, self + 4u, block + (used << 4u));
    put(rt, self, block);
    ctx.gpr[2] = block + (used << 4u);
}

// func_08B8AF08(first, last, dest): copy-constructs the 24-byte elements [first, last) at dest (a null destination cursor skips the element but still advances): the 8 bytes at +0 are copied; the vector at +8 (begin, end, capacity) is deep-copied: a block of
// ((end - begin) with bit 0 cleared) bytes comes from func_08A5BC20(size, 4), the three fields are set up around it (+8 = block, +0xC = the end func_08B8A654(begin, end, block, sp) answers, +0x10 = block + size). v0 = the destination cursor after the last element.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): an empty range (0x08B8AF34), a null destination cursor (the element skipped, 0x08B8AF4C) and a range of more than one element never occurred.
void func_08B8AF08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t first = ctx.gpr[4], last = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    std::uint32_t destination = ctx.gpr[6];
    ctx.gpr[22] = last;
    ctx.gpr[20] = first;
    ctx.gpr[17] = destination;
    if (last == first) {
        ctx.gpr[2] = destination;
        return;
    }
    std::uint32_t source = first;
    for (;;) {
        ctx.gpr[19] = source;
        ctx.gpr[21] = source + 8u;
        ctx.gpr[18] = destination + 8u;
        ctx.gpr[5] = 4u;
        ctx.gpr[20] += 0x18u;
        const std::uint32_t next = ctx.gpr[20];
        if (destination != 0u) {
            put(rt, destination + 4u, word(rt, source + 4u));
            put(rt, destination, word(rt, source));
            const std::uint32_t inner_begin = word(rt, source + 8u);
            ctx.gpr[4] = inner_begin;
            std::uint32_t size = word(rt, source + 0xCu) - inner_begin;
            put(rt, destination + 8u, 0u);
            size &= ~1u;
            ctx.gpr[16] = size;
            ctx.gpr[4] = size;
            put(rt, destination + 0xCu, 0u);
            put(rt, destination + 0x10u, 0u);
            const std::uint32_t block = call_keep(rt, ctx, 0x08A5BC20u, {size, 4u});
            ctx.gpr[16] = block + size;
            put(rt, destination + 8u, block);
            ctx.gpr[6] = block;
            ctx.gpr[7] = frame.at(0u);
            put(rt, destination + 0xCu, block);
            const std::uint32_t old_begin = word(rt, source + 8u), old_end = word(rt, source + 0xCu);
            ctx.gpr[4] = old_begin;
            ctx.gpr[5] = old_end;
            put(rt, destination + 0x10u, block + size);
            const std::uint32_t copied = call_keep(rt, ctx, 0x08B8A654u, {old_begin, old_end, block, frame.at(0u)});
            put(rt, destination + 0xCu, copied);
        }
        source = next;
        ctx.gpr[19] = source;
        destination += 0x18u;
        ctx.gpr[17] = destination;
        if (next == last) break;
    }
    ctx.gpr[2] = destination;
}

// func_08BA3718(this): eight 0x74-byte records at `this` are initialised in place, each by the eleven constructors func_08BA3678 (+0), func_08BA35D8 (+0x10), func_08BA34D4 (+0x20), func_08BA33D0 (+0x30), func_08BA32CC (+0x40), func_08BA31C8 (+0x50), func_08BA30D8 (+0x60),
// func_08BA2FE8 (+0x64), func_08BA2EF8 (+0x68), func_08BA2E08 (+0x6C), func_08BA2D18 (+0x70); then the guarded static D_08D75DC0 (guard D_08D75640, func_08B6441C / func_08B64494): func_08BA1A10(D_08D75DC0, D_08CA02E4, 0x3C), D_08D75DC0[0] = D_08CC0B80, [0x10] = 8,
// func_08B9E328(), and a tail call to the guard release. v0 = hi(guard) when the guard byte is already set, 0 when another caller is building it, else the release's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the acquire answering 0 (another caller building the static, 0x08BA37B4) never occurred.
void func_08BA3718(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    static constexpr std::uint32_t kInit[11] = {0x08BA3678u, 0x08BA35D8u, 0x08BA34D4u, 0x08BA33D0u, 0x08BA32CCu, 0x08BA31C8u, 0x08BA30D8u, 0x08BA2FE8u, 0x08BA2EF8u, 0x08BA2E08u, 0x08BA2D18u};
    static constexpr std::uint32_t kOffset[11] = {0x00u, 0x10u, 0x20u, 0x30u, 0x40u, 0x50u, 0x60u, 0x64u, 0x68u, 0x6Cu, 0x70u};
    const std::uint32_t guard = 0x08D75640u, object = 0x08D75DC0u;
    {
        GuestFrame frame(ctx, 0x10u);
        SavedRegs saved(ctx);
        ctx.gpr[18] = 0xFFFFFFFFu;
        ctx.gpr[17] = 7u;
        std::uint32_t record = ctx.gpr[4];
        ctx.gpr[16] = record;
        do {
            for (unsigned i = 0u; i < 11u; ++i) {
                if (i == 10u) ctx.gpr[17] = ctx.gpr[17] - 1u;
                ctx.gpr[4] = record + kOffset[i];
                (void)call_keep(rt, ctx, kInit[i], {record + kOffset[i]});
            }
            record += 0x74u;
            ctx.gpr[16] = record;
        } while (ctx.gpr[17] != ctx.gpr[18]);
        ctx.gpr[2] = 0x08D70000u;
        ctx.gpr[3] = static_cast<std::uint32_t>(static_cast<std::int32_t>(byte_at(rt, guard)));
        ctx.gpr[17] = guard;
        if (byte_at(rt, guard) != 0) return;
        ctx.gpr[4] = guard;
        if (call_keep(rt, ctx, 0x08B6441Cu, {guard}) == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
        ctx.gpr[18] = 0x08D70000u;
        ctx.gpr[16] = object;
        ctx.gpr[5] = 0x08CA02E4u;
        ctx.gpr[4] = object;
        ctx.gpr[6] = 0x3Cu;
        (void)call_keep(rt, ctx, 0x08BA1A10u, {object, 0x08CA02E4u, 0x3Cu});
        ctx.gpr[2] = 0x08CC0B80u;
        ctx.gpr[3] = 8u;
        put(rt, object + 0x10u, 8u);
        put(rt, object, 0x08CC0B80u);
        (void)call_keep(rt, ctx, 0x08B9E328u, {});
        ctx.gpr[4] = guard;
    }
    tail_call(ctx, 0x08B64494u, {guard});
}

// func_08AD5CE8(this, a1, a2, a3): func_08AD5834(this), func_08AD8334(this, a1, a2, a3); then for each of the this[0x28] records (16 bytes each, at this[0x24], key at +0xC): the byte this[0x64 + i] = the index of the first earlier record with the same key, or, when
// there is none (always for i = 0), a new number s (counting up from 0) with a fresh object func_08ADB508(key, 0) stored in the next word of the array at this + 0x84. v0 = 0 after the loop (the last compare), or func_08AD8334's answer when there are no records.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a record count of 0 (0x08AD5D30) and a record whose key equals an earlier one (the index byte of the earlier record, 0x08AD5D7C-0x08AD5DA4) never occurred: every record was new.
void func_08AD5CE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], a1 = ctx.gpr[5], a2 = ctx.gpr[6], a3 = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[19] = self;
    ctx.gpr[18] = a3;
    ctx.gpr[17] = a2;
    ctx.gpr[16] = a1;
    (void)call_keep(rt, ctx, 0x08AD5834u, {self});
    ctx.gpr[7] = a3;
    ctx.gpr[4] = self;
    ctx.gpr[5] = a1;
    ctx.gpr[6] = a2;
    (void)call_keep(rt, ctx, 0x08AD8334u, {self, a1, a2, a3});
    std::int32_t total = signed_of(word(rt, self + 0x28u));
    ctx.gpr[7] = static_cast<std::uint32_t>(total);
    if (total <= 0) return;
    ctx.gpr[17] = self;
    ctx.gpr[20] = 0u;
    ctx.gpr[16] = 0u;
    std::uint32_t output = self + 0x84u;
    ctx.gpr[18] = output;
    std::uint32_t created = 0u;
    for (std::int32_t i = 0; i < total; ++i) {
        const std::uint32_t records = word(rt, self + 0x24u);
        ctx.gpr[3] = records;
        const std::uint32_t key = word(rt, records + static_cast<std::uint32_t>(i) * 16u + 0xCu);
        ctx.gpr[4] = key;
        std::int32_t found = -1;
        for (std::int32_t j = 0; j < i; ++j) {
            if (word(rt, records + static_cast<std::uint32_t>(j) * 16u + 0xCu) == key) {
                found = j;
                break;
            }
        }
        std::uint32_t index_byte;
        if (found >= 0) {
            index_byte = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(found)));
        } else {
            ctx.gpr[5] = 0u;
            const std::uint32_t made = call_keep(rt, ctx, 0x08ADB508u, {key, 0u});
            total = signed_of(word(rt, self + 0x28u));
            ctx.gpr[7] = static_cast<std::uint32_t>(total);
            put(rt, output, made);
            index_byte = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(created)));
            output += 4u;
            ctx.gpr[18] = output;
            ++created;
            ctx.gpr[20] = created;
        }
        ctx.gpr[6] = index_byte;
        rt.memory().store8(self + 0x64u + static_cast<std::uint32_t>(i), static_cast<std::uint8_t>(index_byte));
        ctx.gpr[17] = self + static_cast<std::uint32_t>(i) + 1u;
        ctx.gpr[16] = static_cast<std::uint32_t>(i) + 1u;
        ctx.gpr[2] = (i + 1 < total) ? 1u : 0u;
    }
}

// func_08AE9F98(): builds a sorted copy of the table of (key, value) pairs at D_08CE08AC (terminated by a zero key): D_08D41AC4 = {1, block, n}; the block comes from func_08AD84B4(8 * n, 1) (0: func_08AEA0A4() and v0 = 0), the n pairs are copied into it and
// func_08B7A3BC(block, n, 8, D_08AE9F78) (a qsort with its compare function) sorts them. v0 = 1.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): an empty table (0x08AE9FC8) and the allocation failing (func_08AEA0A4(), v0 = 0, 0x08AEA094) never occurred.
void func_08AE9F98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t head = 0x08D41AC4u, table = 0x08CE08ACu;
    GuestFrame frame(ctx, 0x10u);
    SavedRegs saved(ctx);
    ctx.gpr[16] = 0x08D40000u;
    put(rt, head, 1u);
    ctx.gpr[2] = 1u;
    ctx.gpr[4] = head;
    put(rt, head + 4u, 0u);
    put(rt, head + 8u, 0u);
    std::uint32_t count = 0u;
    if (word(rt, table) != 0u) {
        std::uint32_t entry = table;
        std::uint32_t key;
        do {
            entry += 8u;
            key = word(rt, entry);
            put(rt, head + 8u, word(rt, head + 8u) + 1u);
            ++count;
        } while (key != 0u);
    }
    const std::uint32_t bytes = count << 3u;
    ctx.gpr[7] = bytes;
    ctx.gpr[4] = bytes;
    ctx.gpr[5] = 1u;
    const std::uint32_t block = call_keep(rt, ctx, 0x08AD84B4u, {bytes, 1u});
    ctx.gpr[4] = head;
    put(rt, head + 4u, block);
    if (block == 0u) {
        (void)call_keep(rt, ctx, 0x08AEA0A4u, {});
        ctx.gpr[2] = 0u;
        return;
    }
    if (word(rt, table) != 0u) {
        std::uint32_t offset = 0u, copied = 0u;
        std::uint32_t key;
        do {
            const std::uint32_t destination = offset + word(rt, head + 4u);
            put(rt, destination, word(rt, table + offset));
            ++copied;
            put(rt, destination + 4u, word(rt, table + offset + 4u));
            offset = copied << 3u;
            key = word(rt, table + offset);
        } while (key != 0u);
    }
    ctx.gpr[5] = word(rt, head + 8u);
    ctx.gpr[4] = word(rt, head + 4u);
    ctx.gpr[7] = 0x08AE9F78u;
    ctx.gpr[6] = 8u;
    (void)call_keep(rt, ctx, 0x08B7A3BCu, {word(rt, head + 4u), word(rt, head + 8u), 8u, 0x08AE9F78u});
    ctx.gpr[2] = 1u;
}

// func_0880CE2C(out, a1, argc, args): a script native with one argument: with argc > 0 the object o filled in by func_0880B8F8(sp+0xC) (null: the assertion call func_08A5BB7C(D_08C9848C) first) and the argument object *args (null: func_08A5BB7C(D_08C98418)) give a string
// by its virtual slot 0xC (called with (text at sp+8, argument)); the answer of func_0880B750(o[0xE8], text) decides: flag = 1 when non-zero, else 0. The text is released (release_string), the holder at sp+0xC destroyed, and func_08A85B1C(sp+4, flag) builds
// the result holder, copied to `out` by func_08A5BAD8 unless it is `out` itself, then func_08A5B9EC(sp+4). With argc <= 0 the flag is 0. v0 is the last call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): argc <= 0 (flag 0, 0x0880CE4C), the null-object assertions (0x0880CE64, 0x0880CE7C), a zero answer of func_0880B750 (flag 0, 0x0880CEF0) and `out` being the result holder (0x0880CF08) never occurred.
void func_0880CE2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], argc = ctx.gpr[6], args = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    ctx.gpr[19] = out;
    ctx.gpr[17] = args;
    std::uint32_t flag = 0u;
    if (signed_of(argc) > 0) {
        const std::uint32_t holder = frame.at(0xCu);
        ctx.gpr[20] = holder;
        ctx.gpr[4] = holder;
        (void)call_keep(rt, ctx, 0x0880B8F8u, {holder});
        const std::uint32_t object = word(rt, holder);
        ctx.gpr[16] = object;
        if (object == 0u) {
            ctx.gpr[4] = 0x08C9848Cu;
            (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08C9848Cu});
        }
        const std::uint32_t argument = word(rt, args);
        ctx.gpr[17] = argument;
        const std::uint32_t owner = word(rt, object + 0xE8u);
        ctx.gpr[18] = owner;
        if (argument == 0u) {
            ctx.gpr[4] = 0x08C98418u;
            (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08C98418u});
        }
        const std::uint32_t vtable = word(rt, argument);
        const std::uint32_t function = word(rt, vtable + 0xCu);
        const std::uint32_t text = frame.at(8u);
        ctx.gpr[16] = text;
        ctx.gpr[5] = argument;
        ctx.gpr[4] = text;
        ctx.gpr[3] = vtable;
        ctx.gpr[2] = function;
        (void)call_keep(rt, ctx, function, {text, argument});
        ctx.gpr[4] = owner;
        ctx.gpr[5] = text;
        const std::uint32_t answer = call_keep(rt, ctx, 0x0880B750u, {owner, text});
        ctx.gpr[17] = answer;
        release_string(rt, ctx, text, frame.at(0u));
        ctx.gpr[4] = holder;
        (void)call_keep(rt, ctx, 0x08A5B9ECu, {holder});
        flag = answer != 0u ? 1u : 0u;
    }
    ctx.gpr[5] = flag;
    const std::uint32_t result = frame.at(4u);
    ctx.gpr[16] = result;
    ctx.gpr[4] = result;
    (void)call_keep(rt, ctx, 0x08A85B1Cu, {result, flag});
    ctx.gpr[5] = word(rt, result);
    ctx.gpr[4] = out;
    if (out != result) (void)call_keep(rt, ctx, 0x08A5BAD8u, {out, word(rt, result)});
    ctx.gpr[4] = result;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {result});
}

// func_0880F470(out, a1, argc, args): a float property of the object o that func_0880F0C0 fills in (null: the assertion call func_08A5BB7C(D_08C986F4) first). With argc > 0 it is a setter: the argument object *args (null: func_08A5BB7C(D_08C986DC)) answers
// its virtual slot 0x54 with a float, stored in o[0xE8][0x2D4]; the holder at sp+8 is destroyed (v0 = that call's). Otherwise a getter: func_08A87EEC(sp+4, o[0xE8][0x2D4]) builds the result holder, copied to `out` unless it is `out` itself (func_08A5BAD8), and
// both holders are destroyed. v0 is the last call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the getter half (argc <= 0, 0x0880F4F0-0x0880F544) never ran, nor the null-object / null-argument assertions (0x0880F4A0, 0x0880F4B8).
void func_0880F470(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], argc = ctx.gpr[6], args = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[17] = out;
    ctx.gpr[16] = args;
    if (signed_of(argc) > 0) {
        const std::uint32_t holder = frame.at(8u);
        ctx.gpr[18] = holder;
        ctx.gpr[4] = holder;
        (void)call_keep(rt, ctx, 0x0880F0C0u, {holder});
        const std::uint32_t object = word(rt, holder);
        ctx.gpr[17] = object;
        if (object == 0u) {
            ctx.gpr[4] = 0x08C986F4u;
            (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08C986F4u});
        }
        const std::uint32_t argument = word(rt, args);
        ctx.gpr[16] = argument;
        const std::uint32_t owner = word(rt, object + 0xE8u);
        ctx.gpr[17] = owner;
        if (argument == 0u) {
            ctx.gpr[4] = 0x08C986DCu;
            (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08C986DCu});
        }
        const std::uint32_t vtable = word(rt, argument);
        const std::uint32_t function = word(rt, vtable + 0x54u);
        ctx.gpr[3] = vtable;
        ctx.gpr[2] = function;
        ctx.gpr[4] = argument;
        const GuestResult answer = call_keep(rt, ctx, function, {argument}, {});
        put(rt, owner + 0x2D4u, std::bit_cast<std::uint32_t>(answer.f0));
        ctx.gpr[4] = holder;
        (void)call_keep(rt, ctx, 0x08A5B9ECu, {holder});
        return;
    }
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x0880F0C0u, {frame.at(0u)});
    const std::uint32_t object = word(rt, frame.at(0u));
    ctx.gpr[16] = object;
    if (object == 0u) {
        ctx.gpr[4] = 0x08C986F4u;
        (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08C986F4u});
    }
    const std::uint32_t owner = word(rt, object + 0xE8u);
    ctx.gpr[2] = owner;
    const std::uint32_t result = frame.at(4u);
    ctx.gpr[16] = result;
    ctx.gpr[4] = result;
    (void)call_keep(rt, ctx, 0x08A87EECu, {result}, {float_at(rt, owner + 0x2D4u)});
    ctx.gpr[5] = word(rt, result);
    ctx.gpr[4] = out;
    if (out != result) (void)call_keep(rt, ctx, 0x08A5BAD8u, {out, word(rt, result)});
    ctx.gpr[4] = result;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {result});
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// func_0881EAD0(out, a1, argc, args): with argc > 0: core = func_0881B068(o) for the object o that func_0881BD98(sp+8) fills in (null: func_08A5BB7C(D_08C98F48) first), the holder at sp+8 is destroyed, func_0881BF5C(core, argc, args, sp+0x10, sp+0xC) looks the argument
// up (a 64-bit code at sp+0x10 and an index at sp+0xC), and the result holder is made by func_08A97A6C(sp+4, 0) when the code is -1:-1, else func_08A97A6C(sp, func_0881BAC0(core, argc, code, index)); it is copied to `out` by func_08A5BAD8 unless it is `out`, and destroyed.
// With argc <= 0 nothing happens (v0 untouched). v0 is the last call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): argc <= 0 (0x0881EAF4), the null-object assertion (0x0881EB0C), the code -1:-1 (the holder made by func_08A97A6C(sp+4, 0), 0x0881EB5C-0x0881EB8C) and `out` being the holder at sp (0x0881EBA8) never occurred: only the lookup by name ran.
void func_0881EAD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], argc = ctx.gpr[6], args = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    SavedRegs saved(ctx);
    ctx.gpr[19] = args;
    ctx.gpr[18] = out;
    ctx.gpr[17] = argc;
    if (signed_of(argc) <= 0) return;
    const std::uint32_t holder = frame.at(8u);
    ctx.gpr[20] = holder;
    ctx.gpr[4] = holder;
    (void)call_keep(rt, ctx, 0x0881BD98u, {holder});
    std::uint32_t object = word(rt, holder);
    ctx.gpr[16] = object;
    if (object == 0u) {
        ctx.gpr[4] = 0x08C98F48u;
        (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08C98F48u});
    }
    ctx.gpr[4] = object;
    const std::uint32_t core = call_keep(rt, ctx, 0x0881B068u, {object});
    ctx.gpr[16] = core;
    ctx.gpr[4] = holder;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {holder});
    ctx.gpr[6] = args;
    ctx.gpr[7] = frame.at(0x10u);
    ctx.gpr[4] = core;
    ctx.gpr[5] = argc;
    ctx.gpr[8] = frame.at(0xCu);
    (void)call_keep(rt, ctx, 0x0881BF5Cu, {core, argc, args, frame.at(0x10u), frame.at(0xCu)});
    const std::uint32_t code_low = word(rt, frame.at(0x10u)), code_high = word(rt, frame.at(0x14u)), index = word(rt, frame.at(0xCu));
    ctx.gpr[6] = code_low;
    ctx.gpr[2] = 0xFFFFFFFFu;
    ctx.gpr[8] = index;
    std::uint32_t result;
    if (code_low == 0xFFFFFFFFu && code_high == 0xFFFFFFFFu) {
        result = frame.at(4u);
        ctx.gpr[7] = code_high;
        ctx.gpr[16] = result;
        ctx.gpr[4] = result;
        ctx.gpr[5] = 0u;
        (void)call_keep(rt, ctx, 0x08A97A6Cu, {result, 0u});
        ctx.gpr[5] = word(rt, result);
        ctx.gpr[4] = out;
        if (out != result) (void)call_keep(rt, ctx, 0x08A5BAD8u, {out, word(rt, result)});
        ctx.gpr[4] = result;
        (void)call_keep(rt, ctx, 0x08A5B9ECu, {result});
        return;
    }
    ctx.gpr[7] = code_high;
    ctx.gpr[4] = core;
    const std::uint32_t name = call_keep(rt, ctx, 0x0881BAC0u, {core, argc, code_low, code_high, index});
    ctx.gpr[5] = name;
    result = frame.at(0u);
    ctx.gpr[4] = result;
    (void)call_keep(rt, ctx, 0x08A97A6Cu, {result, name});
    ctx.gpr[5] = word(rt, result);
    ctx.gpr[4] = out;
    if (out != result) (void)call_keep(rt, ctx, 0x08A5BAD8u, {out, word(rt, result)});
    ctx.gpr[4] = result;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {result});
}

// func_08A25FAC(out): the text for the current language: i = D_08D69514; the string pointer D_08CDEBC8[i] is turned into a temporary string at sp+4 by func_08B85EB8(sp+4, text, sp+1); func_08A6D2D4(sp+8, sp+4) makes the holder, copied to `out` by func_08A5BAD8 unless it is
// `out`, destroyed by func_08A5B9EC; then the temporary string is released (func_08B60804 and, when that answers <= 0, func_08B8542C(body, sp), unless it is the shared empty body D_08D74FD0, which stays in v0).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): `out` being the holder (0x08A26000), the shared empty body (v0 = D_08D74FD0, 0x08A2602C) and the last reference (the func_08B8542C free, 0x08A26040) never occurred.
void func_08A25FAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    const std::uint32_t temporary = frame.at(4u), holder = frame.at(8u);
    ctx.gpr[18] = holder;
    ctx.gpr[17] = out;
    ctx.gpr[16] = temporary;
    ctx.gpr[4] = temporary;
    ctx.gpr[6] = frame.at(1u);
    const std::uint32_t index = word(rt, 0x08D69514u);
    const std::uint32_t text = word(rt, 0x08CDEBC8u + index * 4u);
    ctx.gpr[5] = text;
    ctx.gpr[3] = 0x08CDEBC8u + index * 4u;
    ctx.gpr[2] = 0x08CDEBC8u;
    (void)call_keep(rt, ctx, 0x08B85EB8u, {temporary, text, frame.at(1u)});
    ctx.gpr[4] = holder;
    ctx.gpr[5] = temporary;
    (void)call_keep(rt, ctx, 0x08A6D2D4u, {holder, temporary});
    ctx.gpr[4] = out;
    ctx.gpr[5] = word(rt, holder);
    if (out != holder) (void)call_keep(rt, ctx, 0x08A5BAD8u, {out, word(rt, holder)});
    ctx.gpr[4] = holder;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {holder});
    const std::uint32_t data = word(rt, temporary);
    ctx.gpr[3] = data;
    ctx.gpr[2] = 0x08D74FD0u;
    const std::uint32_t body = data - 0xCu;
    ctx.gpr[16] = body;
    ctx.gpr[5] = 0xFFFFFFFFu;
    ctx.gpr[4] = data - 4u;
    if (body != 0x08D74FD0u) {
        const std::uint32_t references = call_keep(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu});
        ctx.gpr[4] = body;
        ctx.gpr[5] = frame.at(0u);
        if (signed_of(references) <= 0) (void)call_keep(rt, ctx, 0x08B8542Cu, {body, frame.at(0u)});
    }
}

// func_0897AEE4(out, a1, argc, args): with argc > 0: the argument object *args (null: func_08A5BB7C(D_08CA2F84)) gives a string by its virtual slot 0xC (called with (sp+0x10, argument)); the lookup object o that func_089C9C34(sp+8) fills in (null:
// func_08A5BB7C(D_08CA3268)) answers p = func_089C4C20(o, string data, sp+0xC); when p is non-zero func_08A44D9C(sp+4, p, [sp+0xC], 1) makes the result holder, copied to `out` by func_08A5BAD8 unless it is `out`, and destroyed; the string is released
// (release_string). With argc <= 0 nothing happens (v0 untouched). v0 is the last call's, or the shared empty body D_08D74FD0.
void func_0897AEE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], argc = ctx.gpr[6], args = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    ctx.gpr[18] = out;
    if (signed_of(argc) <= 0) return;
    std::uint32_t argument = word(rt, args);
    ctx.gpr[16] = argument;
    put(rt, frame.at(0xCu), 0u);
    if (argument == 0u) {
        ctx.gpr[4] = 0x08CA2F84u;
        (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CA2F84u});
    }
    const std::uint32_t vtable = word(rt, argument);
    const std::uint32_t function = word(rt, vtable + 0xCu);
    ctx.gpr[3] = vtable;
    ctx.gpr[5] = argument;
    ctx.gpr[4] = frame.at(0x10u);
    ctx.gpr[2] = function;
    ctx.gpr[17] = frame.at(8u);
    (void)call_keep(rt, ctx, function, {frame.at(0x10u), argument});
    ctx.gpr[4] = frame.at(8u);
    (void)call_keep(rt, ctx, 0x089C9C34u, {frame.at(8u)});
    const std::uint32_t lookup = word(rt, frame.at(8u));
    ctx.gpr[16] = lookup;
    ctx.gpr[5] = word(rt, frame.at(0x10u));
    if (lookup == 0u) {
        ctx.gpr[4] = 0x08CA3268u;
        (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CA3268u});
        ctx.gpr[5] = word(rt, frame.at(0x10u));
    }
    ctx.gpr[4] = lookup;
    ctx.gpr[6] = frame.at(0xCu);
    const std::uint32_t found = call_keep(rt, ctx, 0x089C4C20u, {lookup, word(rt, frame.at(0x10u)), frame.at(0xCu)});
    ctx.gpr[16] = found;
    ctx.gpr[4] = frame.at(8u);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(8u)});
    ctx.gpr[6] = word(rt, frame.at(0xCu));
    if (found != 0u) {
        const std::uint32_t result = frame.at(4u);
        ctx.gpr[17] = result;
        ctx.gpr[5] = found;
        ctx.gpr[4] = result;
        ctx.gpr[7] = 1u;
        (void)call_keep(rt, ctx, 0x08A44D9Cu, {result, found, word(rt, frame.at(0xCu)), 1u});
        ctx.gpr[5] = word(rt, result);
        ctx.gpr[4] = out;
        if (out != result) (void)call_keep(rt, ctx, 0x08A5BAD8u, {out, word(rt, result)});
        ctx.gpr[4] = result;
        (void)call_keep(rt, ctx, 0x08A5B9ECu, {result});
    }
    release_string(rt, ctx, frame.at(0x10u), frame.at(0u));
}

// func_08A4AE9C(out, argc, args): with argc > 0: the argument object *args (null: func_08A5BB7C(D_08CAED38)) gives a string by its virtual slot 0xC (called with (sp+8, argument)); func_08B0EC60(sp+0x50, string data) builds a 0x3C-byte record that is copied to sp+0x10;
// func_08A85B1C(sp+4, record[0] == 0) makes the result holder, copied to `out` by func_08A5BAD8 unless it is `out`, and destroyed; the string is released. With argc <= 0 nothing happens (v0 untouched). v0 is the last call's, or the shared empty body.
void func_08A4AE9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], argc = ctx.gpr[5], args = ctx.gpr[6];
    GuestFrame frame(ctx, 0xA0u);
    SavedRegs saved(ctx);
    ctx.gpr[17] = out;
    if (signed_of(argc) <= 0) return;
    const std::uint32_t argument = word(rt, args);
    ctx.gpr[16] = argument;
    if (argument == 0u) {
        ctx.gpr[4] = 0x08CAED38u;
        (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CAED38u});
    }
    const std::uint32_t vtable = word(rt, argument);
    const std::uint32_t function = word(rt, vtable + 0xCu);
    ctx.gpr[3] = function;
    ctx.gpr[5] = argument;
    ctx.gpr[4] = frame.at(8u);
    (void)call_keep(rt, ctx, function, {frame.at(8u), argument});
    ctx.gpr[16] = frame.at(0x50u);
    ctx.gpr[5] = word(rt, frame.at(8u));
    ctx.gpr[4] = frame.at(0x50u);
    (void)call_keep(rt, ctx, 0x08B0EC60u, {frame.at(0x50u), word(rt, frame.at(8u))});
    for (std::uint32_t at = 0u; at < 0x3Cu; at += 4u) put(rt, frame.at(0x10u) + at, word(rt, frame.at(0x50u) + at));
    ctx.gpr[16] = frame.at(0x80u);
    const std::uint32_t result = frame.at(4u);
    ctx.gpr[16] = result;
    ctx.gpr[4] = result;
    const std::uint32_t empty = word(rt, frame.at(0x10u)) < 1u ? 1u : 0u;
    ctx.gpr[5] = empty;
    (void)call_keep(rt, ctx, 0x08A85B1Cu, {result, empty});
    ctx.gpr[5] = word(rt, result);
    ctx.gpr[4] = out;
    if (out != result) (void)call_keep(rt, ctx, 0x08A5BAD8u, {out, word(rt, result)});
    ctx.gpr[4] = result;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {result});
    release_string(rt, ctx, frame.at(8u), frame.at(0u));
}

// func_089F01F4(this, source): two holders built by func_089E4C14(holder, D_08CF6ED8) are stored into this + 0x240 and this + 0x244 (each copied by func_08A5BAD8 unless it is the slot itself, then destroyed); func_089E1F4C(this, source); with a non-null
// source its floats and strings are copied: this[0x22C], [0x230], [0x234] from the source, func_08B855E0(this + 0x238, source + 0x238), func_08B855E0(this + 0x23C, source + 0x23C), the four floats at +0x248 .. +0x254, then func_089EDA30(this, source + 0x264). v0 is the last call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the slots being the holders themselves (0x089F0238, 0x089F025C) and a null source (0x089F0284) never occurred.
void func_089F01F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], source = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    ctx.gpr[21] = 0x08CF0000u;
    const std::uint32_t first = frame.at(4u);
    ctx.gpr[19] = first;
    ctx.gpr[18] = self;
    ctx.gpr[17] = source;
    ctx.gpr[16] = self + 0x240u;
    ctx.gpr[20] = self + 0x244u;
    ctx.gpr[4] = first;
    ctx.gpr[5] = 0x08CF6ED8u;
    (void)call_keep(rt, ctx, 0x089E4C14u, {first, 0x08CF6ED8u});
    ctx.gpr[4] = self + 0x240u;
    if (self + 0x240u != first) (void)call_keep(rt, ctx, 0x08A5BAD8u, {self + 0x240u, word(rt, first)});
    ctx.gpr[4] = first;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {first});
    const std::uint32_t second = frame.at(0u);
    ctx.gpr[4] = second;
    ctx.gpr[5] = 0x08CF6ED8u;
    (void)call_keep(rt, ctx, 0x089E4C14u, {second, 0x08CF6ED8u});
    ctx.gpr[4] = self + 0x244u;
    if (self + 0x244u != second) (void)call_keep(rt, ctx, 0x08A5BAD8u, {self + 0x244u, word(rt, second)});
    ctx.gpr[4] = second;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {second});
    ctx.gpr[4] = self;
    ctx.gpr[5] = source;
    (void)call_keep(rt, ctx, 0x089E1F4Cu, {self, source});
    ctx.gpr[4] = self + 0x238u;
    ctx.gpr[5] = source + 0x238u;
    if (source == 0u) return;
    ctx.fpr[0] = float_at(rt, source + 0x234u);
    ctx.gpr[3] = word(rt, source + 0x230u);
    ctx.gpr[2] = word(rt, source + 0x22Cu);
    put(rt, self + 0x234u, word(rt, source + 0x234u));
    put(rt, self + 0x230u, ctx.gpr[3]);
    put(rt, self + 0x22Cu, ctx.gpr[2]);
    (void)call_keep(rt, ctx, 0x08B855E0u, {self + 0x238u, source + 0x238u});
    ctx.gpr[4] = self + 0x23Cu;
    ctx.gpr[5] = source + 0x23Cu;
    (void)call_keep(rt, ctx, 0x08B855E0u, {self + 0x23Cu, source + 0x23Cu});
    ctx.fpr[0] = float_at(rt, source + 0x248u);
    ctx.fpr[1] = float_at(rt, source + 0x24Cu);
    ctx.fpr[2] = float_at(rt, source + 0x250u);
    ctx.fpr[3] = float_at(rt, source + 0x254u);
    put(rt, self + 0x248u, word(rt, source + 0x248u));
    ctx.gpr[4] = self;
    ctx.gpr[5] = source + 0x264u;
    put(rt, self + 0x24Cu, word(rt, source + 0x24Cu));
    put(rt, self + 0x250u, word(rt, source + 0x250u));
    put(rt, self + 0x254u, word(rt, source + 0x254u));
    (void)call_keep(rt, ctx, 0x089EDA30u, {self, source + 0x264u});
}

// func_08A9FA9C(out, in): with a null `in` func_08A76AAC(out, D_08CF6ED8) and v0 = out. Otherwise the holders at sp+4 (func_08A9DD14) and sp+8 (func_08A76AA0) are made; the object o = *func_08A9BD30(*(sp+4)) (null checks: func_08A5BB7C(D_08CB277C) /
// func_08A5BB7C(D_08CB2784)) takes func_08A69B3C(sp, in), func_08A7BFD0(o, sp+8, sp), func_08A69D68(sp); func_08A76ABC(out, sp+8) copies the answer out and both holders are destroyed. v0 = out.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a null `in` (func_08A76AAC, 0x08A9FACC) and the null-object assertions (0x08A9FAFC, 0x08A9FB1C) never occurred.
void func_08A9FA9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], input = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    ctx.gpr[17] = input;
    ctx.gpr[5] = 0x08CF6ED8u;
    ctx.gpr[20] = frame.at(4u);
    ctx.gpr[19] = out;
    ctx.gpr[18] = frame.at(8u);
    if (input == 0u) {
        (void)call_keep(rt, ctx, 0x08A76AACu, {out, 0x08CF6ED8u});
        ctx.gpr[2] = out;
        return;
    }
    ctx.gpr[4] = frame.at(4u);
    (void)call_keep(rt, ctx, 0x08A9DD14u, {frame.at(4u)});
    ctx.gpr[4] = frame.at(8u);
    (void)call_keep(rt, ctx, 0x08A76AA0u, {frame.at(8u)});
    std::uint32_t object = word(rt, frame.at(4u));
    ctx.gpr[16] = object;
    ctx.gpr[4] = 0x08CB277Cu;
    if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CB277Cu});
    ctx.gpr[4] = object;
    const std::uint32_t table = call_keep(rt, ctx, 0x08A9BD30u, {object});
    object = word(rt, table);
    ctx.gpr[16] = object;
    ctx.gpr[4] = 0x08CB2784u;
    if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CB2784u});
    ctx.gpr[5] = input;
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08A69B3Cu, {frame.at(0u), input});
    ctx.gpr[6] = frame.at(0u);
    ctx.gpr[5] = frame.at(8u);
    ctx.gpr[4] = object;
    (void)call_keep(rt, ctx, 0x08A7BFD0u, {object, frame.at(8u), frame.at(0u)});
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08A69D68u, {frame.at(0u)});
    ctx.gpr[4] = out;
    ctx.gpr[5] = frame.at(8u);
    (void)call_keep(rt, ctx, 0x08A76ABCu, {out, frame.at(8u)});
    ctx.gpr[4] = frame.at(8u);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(8u)});
    ctx.gpr[4] = frame.at(4u);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
    ctx.gpr[2] = out;
}

// func_088388BC(out, a1): n = func_088374A0(a1, 0); func_08A813F4(sp+4); func_08838380(sp+8, n, 0); the object o = [sp+4] (null: func_08A5BB7C(D_08C9BB48) first) answers its virtual slot 0xA0 with (o, sp, sp+8) after func_08A85B1C(sp, 0) has
// made the holder at sp; the holders at sp and sp+8 are destroyed, func_08A76ABC(out, sp+4) copies the holder at sp+4 into `out` and it is destroyed too. v0 = out.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the null-object assertion (0x08838914) never occurred.
void func_088388BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], input = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    ctx.gpr[20] = out;
    ctx.gpr[19] = frame.at(4u);
    ctx.gpr[18] = frame.at(8u);
    ctx.gpr[4] = input;
    ctx.gpr[5] = 0u;
    const std::uint32_t count = call_keep(rt, ctx, 0x088374A0u, {input, 0u});
    ctx.gpr[16] = count;
    ctx.gpr[4] = frame.at(4u);
    (void)call_keep(rt, ctx, 0x08A813F4u, {frame.at(4u)});
    ctx.gpr[4] = frame.at(8u);
    ctx.gpr[5] = count;
    ctx.gpr[6] = 0u;
    (void)call_keep(rt, ctx, 0x08838380u, {frame.at(8u), count, 0u});
    const std::uint32_t object = word(rt, frame.at(4u));
    ctx.gpr[17] = object;
    ctx.gpr[4] = 0x08C9BB48u;
    if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08C9BB48u});
    const std::uint32_t vtable = word(rt, object);
    ctx.gpr[2] = vtable;
    ctx.gpr[4] = frame.at(0u);
    ctx.gpr[5] = 0u;
    const std::uint32_t function = word(rt, vtable + 0xA0u);
    ctx.gpr[16] = function;
    (void)call_keep(rt, ctx, 0x08A85B1Cu, {frame.at(0u), 0u});
    ctx.gpr[6] = frame.at(8u);
    ctx.gpr[5] = frame.at(0u);
    ctx.gpr[4] = object;
    (void)call_keep(rt, ctx, function, {object, frame.at(0u), frame.at(8u)});
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    ctx.gpr[4] = frame.at(8u);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(8u)});
    ctx.gpr[4] = out;
    ctx.gpr[5] = frame.at(4u);
    (void)call_keep(rt, ctx, 0x08A76ABCu, {out, frame.at(4u)});
    ctx.gpr[4] = frame.at(4u);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
    ctx.gpr[2] = out;
}

// func_0883C4B4(out, mode, source): nothing for mode < 2 (v0 untouched). Otherwise a 0x1A0-byte state at sp+0x30: func_0893600C(state), func_08935B10(state, 0, func_0883B848(source) as a pair), the object o = source[1] (null: func_08A5BB7C(D_08C9BD48) first)
// gives its virtual slot 0x3C answer to func_08932748(state, answer); the colour func_08935F64(state) | 0xFF000000 and a record of four 1.0f (at sp+0x10 .. +0x1C) and -1 (sp+0x20) go to func_089641DC(sp+0x10, colour); func_08A042F4(sp, sp+0x10) makes the
// result holder, copied to `out` by func_08A5BAD8 unless it is `out`, destroyed, and func_08932448(state) destroys the state. v0 is the last call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): mode < 2 (nothing done, 0x0883C4E0), the null-object assertion (0x0883C510) and `out` being the holder (0x0883C580) never occurred.
void func_0883C4B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], mode = ctx.gpr[5], source = ctx.gpr[6];
    GuestFrame frame(ctx, 0x1D0u);
    SavedRegs saved(ctx);
    const std::uint32_t state = frame.at(0x30u), record = frame.at(0x10u);
    ctx.gpr[17] = state;
    ctx.gpr[19] = record;
    ctx.gpr[18] = out;
    ctx.gpr[16] = source;
    ctx.gpr[4] = state;
    ctx.gpr[5] = static_cast<std::uint32_t>(signed_of(mode) < 2);
    if (signed_of(mode) < 2) return;
    (void)call_keep(rt, ctx, 0x0893600Cu, {state});
    ctx.gpr[4] = source;
    const GuestResult size = call_keep(rt, ctx, 0x0883B848u, {source}, {});
    ctx.gpr[6] = size.v0;
    ctx.gpr[4] = state;
    ctx.gpr[7] = size.v1;
    ctx.gpr[5] = 0u;
    (void)call_keep(rt, ctx, 0x08935B10u, {state, 0u, size.v0, size.v1});
    const std::uint32_t object = word(rt, source + 4u);
    ctx.gpr[16] = object;
    ctx.gpr[4] = 0x08C9BD48u;
    if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08C9BD48u});
    const std::uint32_t vtable = word(rt, object);
    const std::uint32_t function = word(rt, vtable + 0x3Cu);
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = function;
    ctx.gpr[4] = object;
    const std::uint32_t answer = call_keep(rt, ctx, function, {object});
    ctx.gpr[5] = answer;
    ctx.gpr[4] = state;
    (void)call_keep(rt, ctx, 0x08932748u, {state, answer});
    ctx.gpr[4] = state;
    const std::uint32_t base = call_keep(rt, ctx, 0x08935F64u, {state});
    const std::uint32_t colour = base | 0xFF000000u;
    ctx.gpr[5] = colour;
    ctx.gpr[4] = record;
    ctx.gpr[2] = 0xFFFFFFFFu;
    ctx.fpr[0] = std::bit_cast<float>(0x3F800000u);
    put(rt, frame.at(0x10u), 0x3F800000u);
    put(rt, frame.at(0x20u), 0xFFFFFFFFu);
    put(rt, frame.at(0x1Cu), 0x3F800000u);
    put(rt, frame.at(0x18u), 0x3F800000u);
    put(rt, frame.at(0x14u), 0x3F800000u);
    (void)call_keep(rt, ctx, 0x089641DCu, {record, colour});
    ctx.gpr[4] = frame.at(0u);
    ctx.gpr[5] = record;
    (void)call_keep(rt, ctx, 0x08A042F4u, {frame.at(0u), record});
    ctx.gpr[4] = out;
    ctx.gpr[5] = word(rt, frame.at(0u));
    if (out != frame.at(0u)) (void)call_keep(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(0u))});
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    ctx.gpr[4] = state;
    (void)call_keep(rt, ctx, 0x08932448u, {state});
}

// func_08A213C0(out, a1, argc, args): with argc > 0: the argument object *args (null: func_08A5BB7C(D_08CAA7EC)) gives a string by its virtual slot 0xC (called with (sp+0xC, argument)); func_08A20CC8(sp+4, a1) fills the lookup holder (null object:
// func_08A5BB7C(D_08CAA7F4) first), func_08A97A6C(sp+8, func_08A20AA4(o, string data)) makes the result holder, copied to `out` by func_08A5BAD8 unless it is `out`; both holders are destroyed and the string is released. With argc <= 0 nothing happens (v0 untouched).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): argc <= 0 (0x08A213DC), the null assertions (0x08A213E8, 0x08A21424) and `out` being the holder (0x08A21454) never occurred.
void func_08A213C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], key = ctx.gpr[5], argc = ctx.gpr[6], args = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    ctx.gpr[18] = out;
    ctx.gpr[17] = key;
    if (signed_of(argc) <= 0) return;
    const std::uint32_t argument = word(rt, args);
    ctx.gpr[16] = argument;
    if (argument == 0u) {
        ctx.gpr[4] = 0x08CAA7ECu;
        (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CAA7ECu});
    }
    const std::uint32_t vtable = word(rt, argument);
    const std::uint32_t function = word(rt, vtable + 0xCu);
    ctx.gpr[3] = vtable;
    ctx.gpr[5] = argument;
    ctx.gpr[4] = frame.at(0xCu);
    ctx.gpr[2] = function;
    ctx.gpr[19] = frame.at(4u);
    (void)call_keep(rt, ctx, function, {frame.at(0xCu), argument});
    ctx.gpr[5] = key;
    ctx.gpr[4] = frame.at(4u);
    (void)call_keep(rt, ctx, 0x08A20CC8u, {frame.at(4u), key});
    const std::uint32_t lookup = word(rt, frame.at(4u));
    ctx.gpr[16] = lookup;
    ctx.gpr[5] = word(rt, frame.at(0xCu));
    if (lookup == 0u) {
        ctx.gpr[4] = 0x08CAA7F4u;
        (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CAA7F4u});
        ctx.gpr[5] = word(rt, frame.at(0xCu));
    }
    ctx.gpr[4] = lookup;
    ctx.gpr[16] = frame.at(8u);
    const std::uint32_t answer = call_keep(rt, ctx, 0x08A20AA4u, {lookup, word(rt, frame.at(0xCu))});
    ctx.gpr[5] = answer;
    ctx.gpr[4] = frame.at(8u);
    (void)call_keep(rt, ctx, 0x08A97A6Cu, {frame.at(8u), answer});
    ctx.gpr[5] = word(rt, frame.at(8u));
    ctx.gpr[4] = out;
    if (out != frame.at(8u)) (void)call_keep(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(8u))});
    ctx.gpr[4] = frame.at(8u);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(8u)});
    ctx.gpr[4] = frame.at(4u);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
    release_string(rt, ctx, frame.at(0xCu), frame.at(0u));
}

// func_0886F5FC(this): this[0] = D_08CBF7D8; the 0x1D0-byte member object m = operator new (func_08B62344) is built by func_0887347C(m) and func_0886FC04(m + 0x64), its fields set (m[0x84] = 1.0f, m[0x90] = D_08CD71E8, the two string slots m[0x130] / m[0x134] = the shared empty body
// D_08D75298 + 0xC, the rest zeroed) and stored at this[0x70]; the guarded static D_08D752F8 (guard D_08D752F0, func_08B6441C / func_08B64494) gets D_08CBF810 once; this[0x74] = D_08D752F8 and a tail call to func_0886AAA4(this, m).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the acquire answering 0 (another caller building the static, 0x0886F6C0) never occurred.
void func_0886F5FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x20u);
        SavedRegs saved(ctx);
        const std::uint32_t guard = 0x08D752F0u;
        std::uint32_t holder = 0x08D752F8u;
        ctx.gpr[17] = self;
        ctx.gpr[18] = self + 0x70u;
        (void)call_keep(rt, ctx, 0x0886AD04u, {self});
        ctx.gpr[2] = 0x08CBF7D8u;
        ctx.gpr[4] = 0x1D0u;
        put(rt, self, 0x08CBF7D8u);
        const std::uint32_t member = call_keep(rt, ctx, 0x08B62344u, {0x1D0u});
        ctx.gpr[16] = member;
        ctx.gpr[4] = member;
        (void)call_keep(rt, ctx, 0x0887347Cu, {member});
        ctx.gpr[4] = member + 0x64u;
        (void)call_keep(rt, ctx, 0x0886FC04u, {member + 0x64u});
        const std::uint32_t empty = 0x08D75298u + 0xCu;
        ctx.gpr[5] = empty;
        ctx.gpr[2] = 0x08CD71E8u;
        ctx.fpr[0] = std::bit_cast<float>(0x3F800000u);
        put(rt, member + 0x84u, 0x3F800000u);
        ctx.gpr[6] = 0x08D70000u;
        ctx.gpr[3] = 0x08D70000u;
        put(rt, member + 0x90u, 0x08CD71E8u);
        ctx.gpr[19] = guard;
        ctx.gpr[20] = holder;
        put(rt, member + 0x134u, empty);
        put(rt, member + 0x130u, empty);
        put(rt, member + 0x78u, 0u);
        rt.memory().store8(member + 0x7Cu, 0u);
        put(rt, member + 0x80u, 0u);
        rt.memory().store8(member + 0x138u, 0u);
        put(rt, member + 0x13Cu, 0u);
        rt.memory().store8(member + 0x140u, 0u);
        put(rt, self + 0x70u, member);
        ctx.gpr[2] = static_cast<std::uint32_t>(static_cast<std::int32_t>(byte_at(rt, guard)));
        if (byte_at(rt, guard) == 0) {
            ctx.gpr[4] = guard;
            const std::uint32_t acquired = call_keep(rt, ctx, 0x08B6441Cu, {guard});
            ctx.gpr[5] = 0x08D70000u;
            ctx.gpr[20] = holder;
            if (acquired != 0u) {
                ctx.gpr[20] = holder;
                ctx.gpr[4] = guard;
                ctx.gpr[2] = 0x08CB0000u;
                put(rt, holder, 0x08CBF810u);
                (void)call_keep(rt, ctx, 0x08B64494u, {guard});
            }
        }
        const std::uint32_t first = word(rt, self + 0x70u);
        ctx.gpr[5] = first;
        put(rt, self + 0x74u, holder);
        ctx.gpr[4] = self;
    }
    tail_call(ctx, 0x0886AAA4u, {self, word(rt, self + 0x70u)});
}

// func_088710DC(this): func_0887E1E0(this), this[0] = D_08CBF948; a 0xC0-byte member m = operator new (func_08B62344) is set up (m[0x80] = m[0x84] = 0, m[0x88] = the shared empty string data D_08D75298 + 0xC, byte m[0x8C] = 0, m[0x90] = m[0x94] = 0, the string slots
// m[0xA0], m[0xAC], m[0xB8] = that data, the bytes m[0x98], m[0xA4], m[0xB0] = 0), func_08B4E648(m) and func_08B4E648(m + 0x40); this[0xC] = m; the guarded static D_08D75368 (guard D_08D75360, func_08B6441C / func_08B64494) gets D_08CBF970 once; this[0x10] = D_08D75368.
// v0 = hi(guard) when the guard byte was set or acquired by another caller, else the release's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the acquire answering 0 (0x0887119C: the holder is set without the construction) never occurred.
void func_088710DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    SavedRegs saved(ctx);
    const std::uint32_t guard = 0x08D75360u, holder = 0x08D75368u, empty = 0x08D75298u + 0xCu;
    ctx.gpr[16] = self;
    (void)call_keep(rt, ctx, 0x0887E1E0u, {self});
    ctx.gpr[2] = 0x08CBF948u;
    put(rt, self, 0x08CBF948u);
    ctx.gpr[4] = 0xC0u;
    const std::uint32_t member = call_keep(rt, ctx, 0x08B62344u, {0xC0u});
    ctx.gpr[17] = member;
    ctx.gpr[2] = empty;
    ctx.gpr[16] = self + 0xCu;
    put(rt, member + 0x80u, 0u);
    put(rt, member + 0x84u, 0u);
    put(rt, member + 0x88u, empty);
    rt.memory().store8(member + 0x8Cu, 0u);
    put(rt, member + 0x90u, 0u);
    put(rt, member + 0x94u, 0u);
    put(rt, member + 0xA0u, empty);
    put(rt, member + 0xACu, empty);
    put(rt, member + 0xB8u, empty);
    ctx.gpr[3] = member + 0xC4u;
    ctx.gpr[4] = 0xFFFFFFFFu;
    ctx.gpr[5] = 0xFFFFFFFFu;
    ctx.gpr[4] = member;
    (void)call_keep(rt, ctx, 0x08B4E648u, {member});
    ctx.gpr[4] = member + 0x40u;
    (void)call_keep(rt, ctx, 0x08B4E648u, {member + 0x40u});
    for (std::uint32_t i = 0u; i < 3u; ++i) rt.memory().store8(member + 0x98u + i * 0xCu, 0u);
    ctx.gpr[2] = member + 0x24u;
    ctx.gpr[3] = 3u;
    ctx.gpr[4] = 3u;
    put(rt, self + 0xCu, member);
    ctx.gpr[3] = 0x08D70000u;
    ctx.gpr[2] = static_cast<std::uint32_t>(static_cast<std::int32_t>(byte_at(rt, guard)));
    ctx.gpr[17] = guard;
    ctx.gpr[18] = holder;
    if (byte_at(rt, guard) != 0) {
        ctx.gpr[2] = 0x08D70000u;
    } else {
        ctx.gpr[4] = guard;
        if (call_keep(rt, ctx, 0x08B6441Cu, {guard}) != 0u) {
            ctx.gpr[3] = 0x08D70000u;
            put(rt, holder, 0x08CBF970u);
            ctx.gpr[4] = guard;
            (void)call_keep(rt, ctx, 0x08B64494u, {guard});
        } else {
            ctx.gpr[2] = 0x08D70000u;
        }
    }
    put(rt, self + 0x10u, holder);
}

// func_0887CCA8(this, count, values, table): v0 = 1 when, for each of the `count` floats in `values`, a lookup succeeds: func_08B9CC58(sp+0xC, this[4]); then for i in 0 ..< count: func_08B18A58(sp, sp+0xC), func_08B196B8(sp+0x18, sp, table, i), func_08B18A98(sp);
// when the holder at sp+0x18 is set, func_08B19E4C(sp+0x18, values[i]) must answer non-zero (else the holder is destroyed and v0 = 0), and func_08B18A98(sp+0x18) destroys it. A count <= 0 gives 1. func_08B18A98(sp+0xC) closes.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): count <= 0 (v0 = 1, 0x0887CCEC), an empty lookup holder at sp+0x18 (0x0887CD30) and a value func_08B19E4C rejects (0x0887CD44, v0 = 0) never occurred: every call accepted all its values.
void func_0887CCA8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], count = ctx.gpr[5], values = ctx.gpr[6], table = ctx.gpr[7];
    GuestFrame frame(ctx, 0x50u);
    SavedRegs saved(ctx);
    const std::uint32_t scratch = frame.at(0u), outer = frame.at(0xCu), found = frame.at(0x18u);
    ctx.gpr[21] = table;
    ctx.gpr[20] = count;
    ctx.gpr[19] = outer;
    ctx.gpr[16] = values;
    const std::uint32_t source = word(rt, self + 4u);
    ctx.gpr[4] = outer;
    ctx.gpr[5] = source;
    (void)call_keep(rt, ctx, 0x08B9CC58u, {outer, source});
    std::uint32_t result = 1u;
    if (signed_of(count) > 0) {
        ctx.gpr[22] = scratch;
        ctx.gpr[18] = found;
        std::uint32_t index = 0u, cursor = values;
        ctx.gpr[17] = 0u;
        for (;;) {
            ctx.gpr[4] = scratch;
            ctx.gpr[5] = outer;
            (void)call_keep(rt, ctx, 0x08B18A58u, {scratch, outer});
            ctx.gpr[7] = index;
            ctx.gpr[4] = found;
            ctx.gpr[5] = scratch;
            ctx.gpr[6] = table;
            (void)call_keep(rt, ctx, 0x08B196B8u, {found, scratch, table, index});
            ctx.gpr[4] = scratch;
            (void)call_keep(rt, ctx, 0x08B18A98u, {scratch});
            const std::uint32_t holder = word(rt, found);
            ctx.gpr[2] = holder;
            ctx.gpr[4] = found;
            ++index;
            ctx.gpr[17] = index;
            bool accepted = false;
            if (holder != 0u) {
                const float value = float_at(rt, cursor);
                cursor += 4u;
                ctx.gpr[16] = cursor;
                accepted = call_keep(rt, ctx, 0x08B19E4Cu, {found}, {value}).v0 != 0u;
            }
            ctx.gpr[4] = found;
            (void)call_keep(rt, ctx, 0x08B18A98u, {found});
            if (!accepted) {
                result = 0u;
                ctx.gpr[16] = 0u;
                break;
            }
            ctx.gpr[4] = scratch;
            if (count == index) {
                ctx.gpr[16] = 1u;
                break;
            }
        }
    } else {
        ctx.gpr[16] = 1u;
    }
    ctx.gpr[4] = outer;
    (void)call_keep(rt, ctx, 0x08B18A98u, {outer});
    ctx.gpr[2] = result;
}

// func_0887F4FC(this): func_0886AD04(this), this[0] = D_08CC1130, this[0x70] = D_08CC116C; the 0x54-byte member m = operator new (func_08B62344) is built by func_08884420(m), func_0886FC04(m + 0x14), func_088710DC(m + 0x28), func_08884AC8(m + 0x3C) and stored at this[0x74];
// the guarded static D_08D75E00 (guard D_08D75DF8, func_08B6441C / func_08B64494) gets D_08CC1198 once; this[0x78] = D_08D75E00; func_0886AAA4(this, m + 0x3C), func_0886AAA4(this, m) and a tail call to func_0886AAA4(this, m + 0x28).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the acquire answering 0 (0x0887F594) never occurred.
void func_0887F4FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x20u);
        SavedRegs saved(ctx);
        const std::uint32_t guard = 0x08D75DF8u, holder = 0x08D75E00u;
        ctx.gpr[17] = self;
        ctx.gpr[18] = self + 0x74u;
        (void)call_keep(rt, ctx, 0x0886AD04u, {self});
        ctx.gpr[2] = 0x08CC1130u;
        put(rt, self, 0x08CC1130u);
        ctx.gpr[2] = 0x08CC116Cu;
        ctx.gpr[4] = 0x54u;
        put(rt, self + 0x70u, 0x08CC116Cu);
        const std::uint32_t member = call_keep(rt, ctx, 0x08B62344u, {0x54u});
        ctx.gpr[16] = member;
        ctx.gpr[4] = member;
        (void)call_keep(rt, ctx, 0x08884420u, {member});
        ctx.gpr[4] = member + 0x14u;
        (void)call_keep(rt, ctx, 0x0886FC04u, {member + 0x14u});
        ctx.gpr[4] = member + 0x28u;
        (void)call_keep(rt, ctx, 0x088710DCu, {member + 0x28u});
        ctx.gpr[4] = member + 0x3Cu;
        (void)call_keep(rt, ctx, 0x08884AC8u, {member + 0x3Cu});
        put(rt, self + 0x74u, member);
        ctx.gpr[3] = 0x08D70000u;
        ctx.gpr[2] = 0x08D70000u;
        ctx.gpr[5] = static_cast<std::uint32_t>(static_cast<std::int32_t>(byte_at(rt, guard)));
        ctx.gpr[16] = guard;
        ctx.gpr[19] = holder;
        ctx.gpr[4] = guard;
        if (byte_at(rt, guard) == 0) {
            ctx.gpr[19] = holder;
            if (call_keep(rt, ctx, 0x08B6441Cu, {guard}) != 0u) {
                ctx.gpr[5] = 0x08D70000u;
                ctx.gpr[4] = guard;
                ctx.gpr[2] = 0x08CC0000u;
                put(rt, holder, 0x08CC1198u);
                (void)call_keep(rt, ctx, 0x08B64494u, {guard});
            } else {
                ctx.gpr[5] = 0x08D70000u;
                ctx.gpr[4] = guard;
            }
        }
        const std::uint32_t object = word(rt, self + 0x74u);
        put(rt, self + 0x78u, holder);
        ctx.gpr[4] = self;
        ctx.gpr[5] = object + 0x3Cu;
        (void)call_keep(rt, ctx, 0x0886AAA4u, {self, object + 0x3Cu});
        const std::uint32_t second = word(rt, self + 0x74u);
        ctx.gpr[5] = second;
        ctx.gpr[4] = self;
        (void)call_keep(rt, ctx, 0x0886AAA4u, {self, second});
        ctx.gpr[5] = word(rt, self + 0x74u) + 0x28u;
        ctx.gpr[4] = self;
    }
    tail_call(ctx, 0x0886AAA4u, {self, word(rt, self + 0x74u) + 0x28u});
}

// func_0885CBCC(this, a1, a2, a3, t0, t1): under the lock func_08B344E0(this + 8) (released by func_08B3457C): n = this[0][1] (the slot count): n >= 4 gives -1; otherwise slot = this[0][0] + 392 * n is set up by func_0885D480(slot), func_0885DAB0(slot, a2),
// func_0885DBF4(slot, a3 & 0xFF, t0, t1); the id func_0885BBD8(this) (v0:v1) is stored at slot[0x38] / slot[0x3C] (the high word or-ed with n << 24) and this[0][1] = n + 1. v0 = n (or -1).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a full table (n >= 4: v0 = -1, 0x0885CC38) never occurred.
void func_0885CBCC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], a1 = ctx.gpr[5], a2 = ctx.gpr[6], a3 = ctx.gpr[7] & 0xFFu, t0 = ctx.gpr[8], t1 = ctx.gpr[9];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    ctx.gpr[21] = self + 8u;
    ctx.gpr[18] = self;
    ctx.gpr[16] = a1;
    ctx.gpr[23] = t1;
    ctx.gpr[22] = t0;
    ctx.gpr[20] = a3;
    ctx.gpr[19] = a2;
    ctx.gpr[4] = self + 8u;
    (void)call_keep(rt, ctx, 0x08B344E0u, {self + 8u});
    ctx.gpr[5] = a1;
    std::uint32_t table = word(rt, self);
    ctx.gpr[16] = table;
    std::uint32_t used = word(rt, table + 4u);
    ctx.gpr[17] = used;
    ctx.gpr[4] = used << 3u;
    ctx.gpr[2] = (used << 6u) - (used << 3u);
    ctx.gpr[3] = ((used << 6u) - (used << 3u)) * 7u;
    if (signed_of(used) >= 4) {
        used = 0xFFFFFFFFu;
        ctx.gpr[17] = used;
    } else {
        const std::uint32_t slot = word(rt, table) + used * 392u;
        ctx.gpr[16] = slot;
        ctx.gpr[4] = slot;
        (void)call_keep(rt, ctx, 0x0885D480u, {slot});
        ctx.gpr[4] = slot;
        ctx.gpr[5] = a2;
        (void)call_keep(rt, ctx, 0x0885DAB0u, {slot, a2});
        ctx.gpr[5] = a3;
        ctx.gpr[4] = slot;
        ctx.gpr[6] = t0;
        ctx.gpr[7] = t1;
        (void)call_keep(rt, ctx, 0x0885DBF4u, {slot, a3, t0, t1});
        ctx.gpr[4] = self;
        const GuestResult id = call_keep(rt, ctx, 0x0885BBD8u, {self}, {});
        ctx.gpr[5] = id.v1 | (used << 24u);
        ctx.gpr[4] = id.v0;
        put(rt, slot + 0x38u, id.v0);
        put(rt, slot + 0x3Cu, id.v1 | (used << 24u));
        put(rt, word(rt, self) + 4u, used + 1u);
        ctx.gpr[3] = used + 1u;
    }
    ctx.gpr[4] = self + 8u;
    (void)call_keep(rt, ctx, 0x08B3457Cu, {self + 8u});
    ctx.gpr[2] = used;
}

// func_0881FDEC(this): a script class registers its natives: the holder at sp is made by func_08A69B3C(sp, D_08C99304); the 0x38-byte object o = func_08A5BC20(0x38, 4) is built by func_08A7E914(o, sp) and stored at sp+4; func_08A76AB4(sp+8, sp+4) copies it into the holder at sp+8
// and func_08A69D68(sp) destroys the first; then for each (name, function) pair of the table D_08C87E34 (its end is the next pair starting with 0 words: the original reads the following entry's first word) func_08B86FB0(*(sp+8), name, function) registers the native (the
// holder null: func_08A5BB7C(D_08C99314) first); finally func_08A7CE50(*this, sp+8) (null *this: the same assertion call) and func_08A5B9EC(sp+8). v0 is the last call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): an empty native table (0x0881FE60) and the null-object assertions (0x0881FE74, 0x0881FEA8) never occurred.
void func_0881FDEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    const std::uint32_t temporary = frame.at(0u), holder = frame.at(8u);
    ctx.gpr[21] = self;
    ctx.gpr[4] = temporary;
    ctx.gpr[5] = 0x08C99304u;
    ctx.gpr[20] = holder;
    (void)call_keep(rt, ctx, 0x08A69B3Cu, {temporary, 0x08C99304u});
    ctx.gpr[4] = 0x38u;
    ctx.gpr[5] = 4u;
    const std::uint32_t object = call_keep(rt, ctx, 0x08A5BC20u, {0x38u, 4u});
    ctx.gpr[16] = object;
    ctx.gpr[4] = object;
    ctx.gpr[5] = temporary;
    (void)call_keep(rt, ctx, 0x08A7E914u, {object, temporary});
    ctx.gpr[5] = frame.at(4u);
    ctx.gpr[4] = holder;
    put(rt, frame.at(4u), object);
    (void)call_keep(rt, ctx, 0x08A76AB4u, {holder, frame.at(4u)});
    ctx.gpr[4] = temporary;
    (void)call_keep(rt, ctx, 0x08A69D68u, {temporary});
    const std::uint32_t table = 0x08C87E34u;
    ctx.gpr[5] = 0x08C80000u;
    ctx.gpr[2] = word(rt, table);
    ctx.gpr[17] = table;
    if (word(rt, table) != 0u) {
        ctx.gpr[19] = 0x08C90000u;
        std::uint32_t entry = table;
        for (;;) {
            const std::uint32_t target = word(rt, holder);
            ctx.gpr[16] = target;
            ctx.gpr[18] = entry;
            ctx.gpr[4] = 0x08C99314u;
            if (target == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08C99314u});
            ctx.gpr[5] = word(rt, entry);
            ctx.gpr[6] = word(rt, entry + 4u);
            ctx.gpr[4] = target;
            entry += 8u;
            ctx.gpr[17] = entry;
            (void)call_keep(rt, ctx, 0x08B86FB0u, {target, word(rt, entry - 8u), word(rt, entry - 4u)});
            ctx.gpr[2] = word(rt, ctx.gpr[18] + 8u);
            ctx.gpr[16] = word(rt, holder);
            if (ctx.gpr[2] == 0u) break;
        }
    }
    const std::uint32_t first = word(rt, self);
    ctx.gpr[16] = first;
    ctx.gpr[4] = first;
    if (first == 0u) {
        ctx.gpr[4] = 0x08C99314u;
        (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08C99314u});
        ctx.gpr[4] = first;
    }
    ctx.gpr[5] = holder;
    (void)call_keep(rt, ctx, 0x08A7CE50u, {first, holder});
    ctx.gpr[4] = holder;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {holder});
}

// func_088213BC(this): registers five natives of a script class, each as func_08B86FB0(this[0], name, function) after the null check of this[0] (func_08A5BB7C(D_08C995F4) when it is null; this[0] is read again before each): D_08C995FC -> D_088214CC, D_08C9960C -> D_08821514,
// D_08C99620 -> D_088216C0, D_08C99638 -> D_08821C4C; the fifth, D_08C99650 -> D_0882160C, is a tail call.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the null-object assertions (0x088213D0, 0x08821400, 0x08821430, 0x08821460, 0x08821490) never occurred.
void func_088213BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    static constexpr std::uint32_t kName[5] = {0x08C995FCu, 0x08C9960Cu, 0x08C99620u, 0x08C99638u, 0x08C99650u};
    static constexpr std::uint32_t kFunction[5] = {0x088214CCu, 0x08821514u, 0x088216C0u, 0x08821C4Cu, 0x0882160Cu};
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t last_object;
    {
        GuestFrame frame(ctx, 0x10u);
        SavedRegs saved(ctx);
        ctx.gpr[17] = self;
        std::uint32_t object = word(rt, self);
        ctx.gpr[16] = object;
        for (unsigned i = 0u; i < 5u; ++i) {
            if (object == 0u) {
                ctx.gpr[4] = 0x08C995F4u;
                (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08C995F4u});
            }
            ctx.gpr[5] = kName[i];
            ctx.gpr[6] = kFunction[i];
            ctx.gpr[4] = object;
            if (i == 4u) break;
            (void)call_keep(rt, ctx, 0x08B86FB0u, {object, kName[i], kFunction[i]});
            object = word(rt, self);
            ctx.gpr[16] = object;
        }
        last_object = object;
    }
    tail_call(ctx, 0x08B86FB0u, {last_object, kName[4], kFunction[4]});
}

// func_08828ABC / func_08828C8C (a family: the accessor pair and the holder maker differ; the second normalises the written value to 0 / 1): a script property of the object o that func_088248E4(sp+4) fills in (null: func_08A5BB7C(D_08C99B4C) first), whose member o[0x14] is the
// property store p. With argc <= 0 it is a getter: make(sp, get(p)), copied to `out` by func_08A5BAD8 unless it is `out`, destroyed (v0 is that call's); otherwise a setter: the argument object *args (null: func_08A5BB7C(D_08C99B24) first) answers its virtual slot 0x3C and set(p, answer)
// stores it (v0 is the setter's).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the null-object assertion (0x08828AF8) and the whole setter half (argc > 0, 0x08828B14-0x08828B54) never ran; `out` was never the result holder (0x08828B6C).
template <bool Normalise>
inline void property_accessor(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t set, std::uint32_t get, std::uint32_t make) {
    const std::uint32_t out = ctx.gpr[4], argc = ctx.gpr[6], args = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    const std::uint32_t holder = frame.at(4u);
    ctx.gpr[19] = holder;
    ctx.gpr[20] = out;
    ctx.gpr[4] = holder;
    ctx.gpr[18] = args;
    ctx.gpr[17] = argc;
    (void)call_keep(rt, ctx, 0x088248E4u, {holder});
    const std::uint32_t object = word(rt, holder);
    ctx.gpr[16] = object;
    ctx.gpr[4] = 0x08C99B4Cu;
    if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08C99B4Cu});
    const std::uint32_t store = word(rt, object + 0x14u);
    ctx.gpr[19] = store;
    ctx.gpr[4] = holder;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {holder});
    if (signed_of(argc) > 0) {
        const std::uint32_t argument = word(rt, args);
        ctx.gpr[16] = argument;
        ctx.gpr[4] = 0x08C99B24u;
        if (argument == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08C99B24u});
        const std::uint32_t vtable = word(rt, argument);
        const std::uint32_t function = word(rt, vtable + 0x3Cu);
        ctx.gpr[3] = vtable;
        ctx.gpr[2] = function;
        ctx.gpr[4] = argument;
        const std::uint32_t answer = call_keep(rt, ctx, function, {argument});
        const std::uint32_t value = Normalise ? (answer != 0u ? 1u : 0u) : answer;
        ctx.gpr[5] = value;
        ctx.gpr[4] = store;
        (void)call_keep(rt, ctx, set, {store, value});
        return;
    }
    ctx.gpr[4] = store;
    const std::uint32_t current = call_keep(rt, ctx, get, {store});
    const std::uint32_t result = frame.at(0u);
    ctx.gpr[5] = current;
    ctx.gpr[4] = result;
    (void)call_keep(rt, ctx, make, {result, current});
    ctx.gpr[5] = word(rt, result);
    ctx.gpr[4] = out;
    if (out != result) (void)call_keep(rt, ctx, 0x08A5BAD8u, {out, word(rt, result)});
    ctx.gpr[4] = result;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {result});
}
void func_08828ABC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { property_accessor<false>(rt, ctx, 0x0885BBB0u, 0x0885BBBCu, 0x08A85B1Cu); }
void func_08828C8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { property_accessor<true>(rt, ctx, 0x0885BC90u, 0x0885BC9Cu, 0x08A97A6Cu); }

// func_088285AC / func_088286B8 (a family: the accessors, the holder maker, the argument converter and the second assertion string differ): a script property of the object o that func_088248E4 fills in (null: func_08A5BB7C(D_08C99B4C) first), whose member p = o[0x14].
// With argc (a2) zero it is a getter: make(sp+8, get(p)), copied to `out` by func_08A5BAD8 unless it is `out`, destroyed (v0 is that call's). Otherwise a setter: build(sp, args) fills a holder whose object q (null: func_08A5BB7C(message) first) is converted by
// convert(q) and set(p, converted) stores it; the holder at sp is destroyed. v0 is the last call's.
inline void property_pair_b(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t get, std::uint32_t make, std::uint32_t build, std::uint32_t message, std::uint32_t convert,
                            std::uint32_t set) {
    const std::uint32_t out = ctx.gpr[4], argc = ctx.gpr[6], args = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[18] = out;
    ctx.gpr[17] = args;
    if (argc == 0u) {
        const std::uint32_t holder = frame.at(0xCu);
        ctx.gpr[17] = holder;
        ctx.gpr[4] = holder;
        (void)call_keep(rt, ctx, 0x088248E4u, {holder});
        const std::uint32_t object = word(rt, holder + 0u);
        ctx.gpr[16] = object;
        ctx.gpr[4] = 0x08C99B4Cu;
        if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08C99B4Cu});
        const std::uint32_t store = word(rt, object + 0x14u);
        ctx.gpr[16] = store;
        ctx.gpr[4] = holder;
        (void)call_keep(rt, ctx, 0x08A5B9ECu, {holder});
        ctx.gpr[4] = store;
        const std::uint32_t current = call_keep(rt, ctx, get, {store});
        const std::uint32_t result = frame.at(8u);
        ctx.gpr[17] = result;
        ctx.gpr[5] = current;
        ctx.gpr[4] = result;
        (void)call_keep(rt, ctx, make, {result, current});
        ctx.gpr[5] = word(rt, result);
        ctx.gpr[4] = out;
        if (out != result) (void)call_keep(rt, ctx, 0x08A5BAD8u, {out, word(rt, result)});
        ctx.gpr[4] = result;
        (void)call_keep(rt, ctx, 0x08A5B9ECu, {result});
        return;
    }
    const std::uint32_t holder = frame.at(4u);
    ctx.gpr[18] = holder;
    ctx.gpr[4] = holder;
    (void)call_keep(rt, ctx, 0x088248E4u, {holder});
    const std::uint32_t object = word(rt, holder);
    ctx.gpr[16] = object;
    ctx.gpr[4] = 0x08C99B4Cu;
    if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08C99B4Cu});
    ctx.gpr[4] = holder;
    const std::uint32_t store = word(rt, object + 0x14u);
    ctx.gpr[18] = store;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {holder});
    ctx.gpr[4] = frame.at(0u);
    ctx.gpr[5] = args;
    (void)call_keep(rt, ctx, build, {frame.at(0u), args});
    const std::uint32_t converted_object = word(rt, frame.at(0u));
    ctx.gpr[16] = converted_object;
    ctx.gpr[4] = message;
    if (converted_object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {message});
    ctx.gpr[4] = converted_object;
    const std::uint32_t converted = call_keep(rt, ctx, convert, {converted_object});
    ctx.gpr[5] = converted;
    ctx.gpr[4] = store;
    (void)call_keep(rt, ctx, set, {store, converted});
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}
void func_088285AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    property_pair_b(rt, ctx, 0x0885BEA4u, 0x0884642Cu, 0x088458D0u, 0x08C9A1C4u, 0x08845798u, 0x0885BE98u);
}
void func_088286B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    property_pair_b(rt, ctx, 0x0885BEC8u, 0x08845694u, 0x088447E8u, 0x08C9A1D4u, 0x08844634u, 0x0885BEBCu);
}

// func_088341EC / func_088342F4 (a family: the three accessors differ): a boolean script property of the object o that func_088297A8(sp+4) fills in (null: func_08A5BB7C(D_08C9A2F8) first), p = func_088294D0(o). With argc <= 0 a getter: func_08A85B1C(sp, get(p)), copied to
// `out` by func_08A5BAD8 unless it is `out`, destroyed. Otherwise the argument object *args (null: func_08A5BB7C(D_08C9A2B4) first) answers its virtual slot 0x3C and set_true(p) / set_false(p) runs for a non-zero / zero answer. v0 is the last call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the getter half (argc <= 0, 0x08834354), the null-object assertion and the null-argument assertion never ran: only the setter (a zero or non-zero answer) was compared.
inline void bool_property_a(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t set_true, std::uint32_t set_false, std::uint32_t get) {
    const std::uint32_t out = ctx.gpr[4], argc = ctx.gpr[6], args = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    const std::uint32_t holder = frame.at(4u);
    ctx.gpr[20] = holder;
    ctx.gpr[21] = out;
    ctx.gpr[4] = holder;
    ctx.gpr[19] = args;
    ctx.gpr[18] = argc;
    (void)call_keep(rt, ctx, 0x088297A8u, {holder});
    const std::uint32_t object = word(rt, holder);
    ctx.gpr[16] = object;
    ctx.gpr[4] = 0x08C9A2F8u;
    if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08C9A2F8u});
    ctx.gpr[4] = object;
    const std::uint32_t store = call_keep(rt, ctx, 0x088294D0u, {object});
    ctx.gpr[4] = holder;
    ctx.gpr[17] = store;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {holder});
    if (signed_of(argc) > 0) {
        const std::uint32_t argument = word(rt, args);
        ctx.gpr[16] = argument;
        ctx.gpr[4] = 0x08C9A2B4u;
        if (argument == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08C9A2B4u});
        const std::uint32_t vtable = word(rt, argument);
        const std::uint32_t function = word(rt, vtable + 0x3Cu);
        ctx.gpr[3] = vtable;
        ctx.gpr[2] = function;
        ctx.gpr[4] = argument;
        const std::uint32_t answer = call_keep(rt, ctx, function, {argument});
        ctx.gpr[4] = store;
        (void)call_keep(rt, ctx, answer != 0u ? set_true : set_false, {store});
        return;
    }
    ctx.gpr[4] = store;
    const std::uint32_t current = call_keep(rt, ctx, get, {store});
    ctx.gpr[5] = current;
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08A85B1Cu, {frame.at(0u), current});
    ctx.gpr[5] = word(rt, frame.at(0u));
    ctx.gpr[4] = out;
    if (out != frame.at(0u)) (void)call_keep(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(0u))});
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}
void func_088341EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bool_property_a(rt, ctx, 0x0885EF18u, 0x0885EF0Cu, 0x0885EEECu); }
void func_088342F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bool_property_a(rt, ctx, 0x0885EF50u, 0x0885EF44u, 0x0885EF24u); }

// func_0884A5CC / func_0884A6DC (a family: the two accessors differ): a boolean script property of o = func_08846E50(the object that func_08847008 fills in; null: func_08A5BB7C(D_08C9CBBC) first). With argc > 0 a setter: the argument object *args (null: func_08A5BB7C(D_08C9CB80)
// first) answers its virtual slot 0x3C and set(o, answer != 0) stores it; otherwise a getter: func_08A85B1C(sp, get(o)), copied to `out` by func_08A5BAD8 unless it is `out`, destroyed. v0 is the last call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the setter half (argc > 0, 0x0884A5E8 / 0x0884A6F8), the null-object assertion of the getter and `out` being the holder never ran: only the getter was compared.
inline void bool_property_b(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t set, std::uint32_t get) {
    const std::uint32_t out = ctx.gpr[4], argc = ctx.gpr[6], args = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    ctx.gpr[17] = out;
    ctx.gpr[18] = args;
    if (signed_of(argc) > 0) {
        const std::uint32_t holder = frame.at(8u);
        ctx.gpr[19] = holder;
        ctx.gpr[4] = holder;
        (void)call_keep(rt, ctx, 0x08847008u, {holder});
        std::uint32_t object = word(rt, holder);
        ctx.gpr[16] = object;
        ctx.gpr[4] = 0x08C9CBBCu;
        if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08C9CBBCu});
        ctx.gpr[4] = object;
        const std::uint32_t target = call_keep(rt, ctx, 0x08846E50u, {object});
        ctx.gpr[4] = holder;
        ctx.gpr[17] = target;
        (void)call_keep(rt, ctx, 0x08A5B9ECu, {holder});
        const std::uint32_t argument = word(rt, args);
        ctx.gpr[16] = argument;
        ctx.gpr[4] = 0x08C9CB80u;
        if (argument == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08C9CB80u});
        const std::uint32_t vtable = word(rt, argument);
        const std::uint32_t function = word(rt, vtable + 0x3Cu);
        ctx.gpr[3] = vtable;
        ctx.gpr[2] = function;
        ctx.gpr[4] = argument;
        const std::uint32_t answer = call_keep(rt, ctx, function, {argument});
        ctx.gpr[5] = answer != 0u ? 1u : 0u;
        ctx.gpr[4] = target;
        (void)call_keep(rt, ctx, set, {target, answer != 0u ? 1u : 0u});
        return;
    }
    const std::uint32_t holder = frame.at(4u);
    ctx.gpr[18] = holder;
    ctx.gpr[4] = holder;
    (void)call_keep(rt, ctx, 0x08847008u, {holder});
    const std::uint32_t object = word(rt, holder);
    ctx.gpr[16] = object;
    ctx.gpr[4] = 0x08C9CBBCu;
    if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08C9CBBCu});
    ctx.gpr[4] = object;
    const std::uint32_t target = call_keep(rt, ctx, 0x08846E50u, {object});
    ctx.gpr[16] = target;
    ctx.gpr[4] = holder;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {holder});
    ctx.gpr[4] = target;
    const std::uint32_t current = call_keep(rt, ctx, get, {target});
    ctx.gpr[5] = current;
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08A85B1Cu, {frame.at(0u), current});
    ctx.gpr[5] = word(rt, frame.at(0u));
    ctx.gpr[4] = out;
    if (out != frame.at(0u)) (void)call_keep(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(0u))});
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}
void func_0884A5CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bool_property_b(rt, ctx, 0x088D4C60u, 0x088D4C50u); }
void func_0884A6DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bool_property_b(rt, ctx, 0x088D4DACu, 0x088D4D9Cu); }

// func_08829CC4(a0, context, argc, args): nothing for argc < 2. Otherwise func_089AB370(sp+4, args) and func_08829598(*(sp+4)), func_08969244(sp+8, args + 4) and func_08829604(*(sp+8)); the object o that func_088297A8(sp, context) fills in (null: func_08A5BB7C(D_08C9A2F8) first) gives
// p = func_088294D0(o), then func_0885E758(p, D_08831648, 0) and the three holders are destroyed. v0 is the last call's (untouched for argc < 2).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): argc < 2 (0x08829CF4) and the null-object assertion (0x08829D34) never occurred.
void func_08829CC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t context = ctx.gpr[5], argc = ctx.gpr[6], args = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    const std::uint32_t first = frame.at(4u), second = frame.at(8u);
    ctx.gpr[19] = first;
    ctx.gpr[18] = second;
    ctx.gpr[17] = context;
    ctx.gpr[16] = args;
    ctx.gpr[6] = static_cast<std::uint32_t>(signed_of(argc) < 2);
    ctx.gpr[5] = args;
    ctx.gpr[4] = first;
    if (signed_of(argc) < 2) return;
    (void)call_keep(rt, ctx, 0x089AB370u, {first, args});
    ctx.gpr[4] = word(rt, first);
    (void)call_keep(rt, ctx, 0x08829598u, {word(rt, first)});
    ctx.gpr[5] = args + 4u;
    ctx.gpr[4] = second;
    (void)call_keep(rt, ctx, 0x08969244u, {second, args + 4u});
    ctx.gpr[4] = word(rt, second);
    (void)call_keep(rt, ctx, 0x08829604u, {word(rt, second)});
    ctx.gpr[4] = frame.at(0u);
    ctx.gpr[5] = context;
    (void)call_keep(rt, ctx, 0x088297A8u, {frame.at(0u), context});
    std::uint32_t object = word(rt, frame.at(0u));
    ctx.gpr[16] = object;
    ctx.gpr[4] = 0x08C9A2F8u;
    if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08C9A2F8u});
    ctx.gpr[4] = object;
    const std::uint32_t target = call_keep(rt, ctx, 0x088294D0u, {object});
    ctx.gpr[16] = target;
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    ctx.gpr[5] = 0x08831648u;
    ctx.gpr[4] = target;
    ctx.gpr[6] = 0u;
    (void)call_keep(rt, ctx, 0x0885E758u, {target, 0x08831648u, 0u});
    ctx.gpr[4] = second;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {second});
    ctx.gpr[4] = first;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {first});
}

// func_0882B744(a0, context, argc, args): nothing for argc < 2. Otherwise o = func_088297A8(sp+4, context) (null: func_08A5BB7C(D_08C9A2F8) first), p = func_088294D0(o); the argument object *args (null: func_08A5BB7C(D_08C9A2B4) first) answers its virtual slot 0x3C
// (a); func_088547C4(sp, args + 4) fills a holder whose object q (null: func_08A5BB7C(D_08C9A340) first) gives b = func_08854678(q); func_0885EC80(p, a, b) runs and the holders are destroyed. v0 is the last call's (untouched for argc < 2).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): argc < 2 (0x0882B768) and the null assertions (0x0882B780, 0x0882B7AC, 0x0882B7E4) never occurred.
void func_0882B744(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t context = ctx.gpr[5], argc = ctx.gpr[6], args = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    const std::uint32_t holder = frame.at(4u);
    ctx.gpr[18] = holder;
    ctx.gpr[4] = holder;
    ctx.gpr[17] = args;
    ctx.gpr[6] = static_cast<std::uint32_t>(signed_of(argc) < 2);
    if (signed_of(argc) < 2) return;
    (void)call_keep(rt, ctx, 0x088297A8u, {holder, context});
    std::uint32_t object = word(rt, holder);
    ctx.gpr[16] = object;
    ctx.gpr[4] = 0x08C9A2F8u;
    if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08C9A2F8u});
    ctx.gpr[4] = object;
    const std::uint32_t target = call_keep(rt, ctx, 0x088294D0u, {object});
    ctx.gpr[19] = target;
    ctx.gpr[4] = holder;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {holder});
    const std::uint32_t argument = word(rt, args);
    ctx.gpr[16] = argument;
    ctx.gpr[4] = 0x08C9A2B4u;
    if (argument == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08C9A2B4u});
    const std::uint32_t vtable = word(rt, argument);
    const std::uint32_t function = word(rt, vtable + 0x3Cu);
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = function;
    ctx.gpr[4] = argument;
    const std::uint32_t first = call_keep(rt, ctx, function, {argument});
    ctx.gpr[18] = first;
    ctx.gpr[4] = frame.at(0u);
    ctx.gpr[5] = args + 4u;
    (void)call_keep(rt, ctx, 0x088547C4u, {frame.at(0u), args + 4u});
    object = word(rt, frame.at(0u));
    ctx.gpr[16] = object;
    ctx.gpr[4] = 0x08C9A340u;
    if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08C9A340u});
    ctx.gpr[4] = object;
    const std::uint32_t second = call_keep(rt, ctx, 0x08854678u, {object});
    ctx.gpr[16] = second;
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    ctx.gpr[4] = target;
    ctx.gpr[5] = first;
    ctx.gpr[6] = second;
    (void)call_keep(rt, ctx, 0x0885EC80u, {target, first, second});
}

// func_0883365C(out): o = func_088297A8(sp+4, context) (null: func_08A5BB7C(D_08C9A2F8) first), p = func_0885EE70(func_088294D0(o)); with q = func_08891608(p) non-zero: func_088915F0(p), func_08891600(p), a = func_08892138(p), b = func_08B2B5E0(a), c = func_088915E0(p),
// func_08B79A88(b, c, a); func_08A44D9C(sp, b, a, 1) makes the result holder, copied to `out` by func_08A5BAD8 unless it is `out`, and destroyed. The holder at sp+4 is destroyed first. v0 is the last call's.
void func_0883365C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    const std::uint32_t holder = frame.at(4u);
    ctx.gpr[17] = holder;
    ctx.gpr[19] = out;
    ctx.gpr[4] = holder;
    (void)call_keep(rt, ctx, 0x088297A8u, {holder});
    std::uint32_t object = word(rt, holder);
    ctx.gpr[16] = object;
    ctx.gpr[4] = 0x08C9A2F8u;
    if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08C9A2F8u});
    ctx.gpr[4] = object;
    const std::uint32_t target = call_keep(rt, ctx, 0x088294D0u, {object});
    ctx.gpr[16] = target;
    ctx.gpr[4] = holder;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {holder});
    ctx.gpr[4] = target;
    const std::uint32_t pointer = call_keep(rt, ctx, 0x0885EE70u, {target});
    ctx.gpr[4] = pointer;
    ctx.gpr[18] = pointer;
    const std::uint32_t check = call_keep(rt, ctx, 0x08891608u, {pointer});
    ctx.gpr[4] = pointer;
    if (check == 0u) return;
    (void)call_keep(rt, ctx, 0x088915F0u, {pointer});
    ctx.gpr[4] = pointer;
    (void)call_keep(rt, ctx, 0x08891600u, {pointer});
    ctx.gpr[4] = pointer;
    const std::uint32_t first = call_keep(rt, ctx, 0x08892138u, {pointer});
    ctx.gpr[4] = first;
    ctx.gpr[16] = first;
    const std::uint32_t second = call_keep(rt, ctx, 0x08B2B5E0u, {first});
    ctx.gpr[17] = second;
    ctx.gpr[4] = pointer;
    const std::uint32_t third = call_keep(rt, ctx, 0x088915E0u, {pointer});
    ctx.gpr[5] = third;
    ctx.gpr[4] = second;
    ctx.gpr[6] = first;
    (void)call_keep(rt, ctx, 0x08B79A88u, {second, third, first});
    ctx.gpr[4] = frame.at(0u);
    ctx.gpr[5] = second;
    ctx.gpr[6] = first;
    ctx.gpr[7] = 1u;
    (void)call_keep(rt, ctx, 0x08A44D9Cu, {frame.at(0u), second, first, 1u});
    ctx.gpr[4] = out;
    if (out != frame.at(0u)) (void)call_keep(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(0u))});
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// func_08835E1C(out, a1, argc, args): a float property o[0x1C] of the object o that func_08835BD8 fills in (null: func_08A5BB7C(D_08C9B7B8) first). With argc > 0 a setter: the argument object *args (null: func_08A5BB7C(D_08C9B798) first) answers its virtual slot 0x54 with a float,
// stored in o[0x1C]; the holder at sp+8 is destroyed. Otherwise a getter: func_08A87EEC(sp+4, o[0x1C]) makes the result holder, copied to `out` by func_08A5BAD8 unless it is `out`, both holders destroyed. v0 is the last call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the getter half (argc <= 0, 0x08835E9C-0x08835EE8) never ran, nor the null-object / null-argument assertions (0x08835E4C, 0x08835E64).
void func_08835E1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], argc = ctx.gpr[6], args = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[17] = out;
    ctx.gpr[16] = args;
    if (signed_of(argc) > 0) {
        const std::uint32_t holder = frame.at(8u);
        ctx.gpr[18] = holder;
        ctx.gpr[4] = holder;
        (void)call_keep(rt, ctx, 0x08835BD8u, {holder});
        const std::uint32_t object = word(rt, holder);
        ctx.gpr[17] = object;
        if (object == 0u) {
            ctx.gpr[4] = 0x08C9B7B8u;
            (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08C9B7B8u});
        }
        const std::uint32_t argument = word(rt, args);
        ctx.gpr[16] = argument;
        ctx.gpr[17] = object + 0x18u;
        if (argument == 0u) {
            ctx.gpr[4] = 0x08C9B798u;
            (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08C9B798u});
        }
        const std::uint32_t vtable = word(rt, argument);
        const std::uint32_t function = word(rt, vtable + 0x54u);
        ctx.gpr[3] = vtable;
        ctx.gpr[2] = function;
        ctx.gpr[4] = argument;
        const GuestResult answer = call_keep(rt, ctx, function, {argument}, {});
        put(rt, object + 0x1Cu, std::bit_cast<std::uint32_t>(answer.f0));
        ctx.gpr[4] = holder;
        (void)call_keep(rt, ctx, 0x08A5B9ECu, {holder});
        return;
    }
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08835BD8u, {frame.at(0u)});
    const std::uint32_t object = word(rt, frame.at(0u));
    ctx.gpr[16] = object;
    if (object == 0u) {
        ctx.gpr[4] = 0x08C9B7B8u;
        (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08C9B7B8u});
    }
    const std::uint32_t result = frame.at(4u);
    ctx.gpr[16] = result;
    ctx.gpr[4] = result;
    (void)call_keep(rt, ctx, 0x08A87EECu, {result}, {float_at(rt, object + 0x1Cu)});
    ctx.gpr[5] = word(rt, result);
    ctx.gpr[4] = out;
    if (out != result) (void)call_keep(rt, ctx, 0x08A5BAD8u, {out, word(rt, result)});
    ctx.gpr[4] = result;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {result});
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// func_088381F8(a0, context, argc, args): nothing for argc <= 0 (v0 = 1: the compare argc < 2). Otherwise flag = 0, or with argc >= 2 the answer of the virtual slot 0x3C of args[1] (null: func_08A5BB7C(D_08C9BA2C) first); o = func_08837498(the object that func_088378F0(sp, context)
// fills in (null: func_08A5BB7C(D_08C9BAB4) first), 0); func_08A810F4(sp+4, args) and func_08837DB0(o, sp+4, flag) run; the holders are destroyed. v0 is the last call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): argc <= 0 (0x08838220), argc = 1 (the flag stays 0, 0x08838228) and the null assertions (0x08838238, 0x08838270) never occurred: the second argument was always present.
void func_088381F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t context = ctx.gpr[5], argc = ctx.gpr[6], args = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    const std::uint32_t second = frame.at(4u);
    ctx.gpr[2] = static_cast<std::uint32_t>(signed_of(argc) < 2);
    ctx.gpr[19] = args;
    ctx.gpr[18] = second;
    ctx.gpr[17] = context;
    if (signed_of(argc) <= 0) return;
    std::uint32_t flag = 0u;
    ctx.gpr[20] = 0u;
    if (!(signed_of(argc) < 2)) {
        const std::uint32_t argument = word(rt, args + 4u);
        ctx.gpr[16] = argument;
        ctx.gpr[4] = 0x08C9BA2Cu;
        if (argument == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08C9BA2Cu});
        const std::uint32_t vtable = word(rt, argument);
        const std::uint32_t function = word(rt, vtable + 0x3Cu);
        ctx.gpr[3] = vtable;
        ctx.gpr[2] = function;
        ctx.gpr[4] = argument;
        flag = call_keep(rt, ctx, function, {argument});
        ctx.gpr[20] = flag;
    }
    ctx.gpr[4] = frame.at(0u);
    ctx.gpr[5] = context;
    (void)call_keep(rt, ctx, 0x088378F0u, {frame.at(0u), context});
    const std::uint32_t object = word(rt, frame.at(0u));
    ctx.gpr[16] = object;
    ctx.gpr[4] = 0x08C9BAB4u;
    if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08C9BAB4u});
    ctx.gpr[4] = object;
    ctx.gpr[5] = 0u;
    const std::uint32_t target = call_keep(rt, ctx, 0x08837498u, {object, 0u});
    ctx.gpr[16] = target;
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    ctx.gpr[5] = args;
    ctx.gpr[4] = second;
    (void)call_keep(rt, ctx, 0x08A810F4u, {second, args});
    ctx.gpr[4] = target;
    ctx.gpr[5] = second;
    ctx.gpr[6] = flag;
    (void)call_keep(rt, ctx, 0x08837DB0u, {target, second, flag});
    ctx.gpr[4] = second;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {second});
}

// func_08845BA8(out, context, argc, args): a property of the object p = func_08845798(o) for the object o that func_088458D0 fills in (null: func_08A5BB7C(D_08C9CA90) first). With argc (a2) zero a getter: func_08A85B1C(sp+4, *func_088940E4(p)), copied to `out` by
// func_08A5BAD8 unless it is `out`, destroyed. Otherwise a setter: the argument object *args (null: func_08A5BB7C(D_08C9CA54) first) answers its virtual slot 0x3C and func_088940F4(p, answer) stores it. v0 is the last call's.
void func_08845BA8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], argc = ctx.gpr[6], args = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[18] = out;
    ctx.gpr[17] = args;
    if (argc == 0u) {
        const std::uint32_t holder = frame.at(8u);
        ctx.gpr[17] = holder;
        ctx.gpr[4] = holder;
        (void)call_keep(rt, ctx, 0x088458D0u, {holder});
        const std::uint32_t object = word(rt, holder);
        ctx.gpr[16] = object;
        ctx.gpr[4] = 0x08C9CA90u;
        if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08C9CA90u});
        ctx.gpr[4] = object;
        const std::uint32_t target = call_keep(rt, ctx, 0x08845798u, {object});
        ctx.gpr[16] = target;
        ctx.gpr[4] = holder;
        (void)call_keep(rt, ctx, 0x08A5B9ECu, {holder});
        ctx.gpr[4] = target;
        const std::uint32_t cell = call_keep(rt, ctx, 0x088940E4u, {target});
        const std::uint32_t value = word(rt, cell);
        ctx.gpr[5] = value;
        const std::uint32_t result = frame.at(4u);
        ctx.gpr[16] = result;
        ctx.gpr[4] = result;
        (void)call_keep(rt, ctx, 0x08A85B1Cu, {result, value});
        ctx.gpr[5] = word(rt, result);
        ctx.gpr[4] = out;
        if (out != result) (void)call_keep(rt, ctx, 0x08A5BAD8u, {out, word(rt, result)});
        ctx.gpr[4] = result;
        (void)call_keep(rt, ctx, 0x08A5B9ECu, {result});
        return;
    }
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x088458D0u, {frame.at(0u)});
    std::uint32_t object = word(rt, frame.at(0u));
    ctx.gpr[16] = object;
    ctx.gpr[4] = 0x08C9CA90u;
    if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08C9CA90u});
    ctx.gpr[4] = object;
    const std::uint32_t target = call_keep(rt, ctx, 0x08845798u, {object});
    ctx.gpr[4] = frame.at(0u);
    ctx.gpr[18] = target;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    const std::uint32_t argument = word(rt, args);
    ctx.gpr[16] = argument;
    ctx.gpr[4] = 0x08C9CA54u;
    if (argument == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08C9CA54u});
    const std::uint32_t vtable = word(rt, argument);
    const std::uint32_t function = word(rt, vtable + 0x3Cu);
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = function;
    ctx.gpr[4] = argument;
    const std::uint32_t answer = call_keep(rt, ctx, function, {argument});
    ctx.gpr[5] = answer;
    ctx.gpr[4] = target;
    (void)call_keep(rt, ctx, 0x088940F4u, {target, answer});
}

// func_08855C68(out): o = func_08854678(the object that func_088547C4(sp+8) fills in (null: func_08A5BB7C(D_08C9E188) first)); func_088A6414(o, sp+0xC, sp+0x10) yields two values; func_08A98C1C(sp+0x14, 2) makes a two-element table holder; its first slot
// (table[0x14] -> slot 0) is set with func_08A85B1C(sp+4, [sp+0xC]) and its second (slot 1, at +4) with func_08A85B1C(sp, [sp+0x10]), each copied by func_08A5BAD8 unless it is the holder itself; then the table holder is copied to `out` (unless it is `out`) and destroyed. v0 is the last call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the null-object assertion (0x08855CA4) and the result slots being the holders themselves / `out` being the table holder (0x08855CF8, 0x08855D28, 0x08855D40) never occurred.
void func_08855C68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    GuestFrame frame(ctx, 0x40u);
    SavedRegs saved(ctx);
    const std::uint32_t temp_a = frame.at(4u), holder = frame.at(8u), table = frame.at(0x14u);
    ctx.gpr[18] = holder;
    ctx.gpr[20] = out;
    ctx.gpr[4] = holder;
    ctx.gpr[19] = table;
    ctx.gpr[17] = temp_a;
    (void)call_keep(rt, ctx, 0x088547C4u, {holder});
    std::uint32_t object = word(rt, holder);
    ctx.gpr[16] = object;
    ctx.gpr[4] = 0x08C9E188u;
    if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08C9E188u});
    ctx.gpr[4] = object;
    const std::uint32_t target = call_keep(rt, ctx, 0x08854678u, {object});
    ctx.gpr[16] = target;
    ctx.gpr[4] = holder;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {holder});
    ctx.gpr[4] = target;
    ctx.gpr[5] = frame.at(0xCu);
    ctx.gpr[6] = frame.at(0x10u);
    (void)call_keep(rt, ctx, 0x088A6414u, {target, frame.at(0xCu), frame.at(0x10u)});
    ctx.gpr[4] = table;
    ctx.gpr[5] = 2u;
    (void)call_keep(rt, ctx, 0x08A98C1Cu, {table, 2u});
    std::uint32_t slots = word(rt, table);
    ctx.gpr[2] = slots;
    ctx.gpr[5] = word(rt, frame.at(0xCu));
    ctx.gpr[4] = temp_a;
    std::uint32_t slot = word(rt, slots + 0x14u);
    ctx.gpr[16] = slot;
    (void)call_keep(rt, ctx, 0x08A85B1Cu, {temp_a, word(rt, frame.at(0xCu))});
    ctx.gpr[4] = slot;
    ctx.gpr[5] = word(rt, temp_a);
    if (slot != temp_a) (void)call_keep(rt, ctx, 0x08A5BAD8u, {slot, word(rt, temp_a)});
    ctx.gpr[4] = temp_a;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {temp_a});
    slots = word(rt, table);
    ctx.gpr[2] = slots;
    ctx.gpr[5] = word(rt, frame.at(0x10u));
    ctx.gpr[4] = frame.at(0u);
    slot = word(rt, slots + 0x14u);
    (void)call_keep(rt, ctx, 0x08A85B1Cu, {frame.at(0u), word(rt, frame.at(0x10u))});
    slot += 4u;
    ctx.gpr[16] = slot;
    ctx.gpr[4] = slot;
    ctx.gpr[5] = word(rt, frame.at(0u));
    if (slot != frame.at(0u)) (void)call_keep(rt, ctx, 0x08A5BAD8u, {slot, word(rt, frame.at(0u))});
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    ctx.gpr[4] = out;
    ctx.gpr[5] = word(rt, table);
    if (out != table) (void)call_keep(rt, ctx, 0x08A5BAD8u, {out, word(rt, table)});
    ctx.gpr[4] = table;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {table});
}

// func_0898BAB4(out, a1, mode, args): a boolean property of the object o that func_089896CC fills in (null: func_08A5BB7C(D_08CA39F0) first). Mode 0 (getter): func_08A85B1C(sp+8, o[0x127] != 0), copied to `out` by func_08A5BAD8 unless it is `out`, both holders
// destroyed (v0 is the last call's). Mode 1 (setter): the argument object *args (null: func_08A5BB7C(D_08CA39E8) first) answers its virtual slot 0x3C: o[0x1B4] = 1 and o[0x127] = answer != 0, the holder destroyed. Any other mode: v0 = 1, nothing else.
void func_0898BAB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], mode = ctx.gpr[6], args = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[18] = out;
    ctx.gpr[17] = args;
    ctx.gpr[16] = mode;
    if (mode == 0u) {
        const std::uint32_t holder = frame.at(4u);
        ctx.gpr[17] = holder;
        ctx.gpr[4] = holder;
        (void)call_keep(rt, ctx, 0x089896CCu, {holder});
        const std::uint32_t object = word(rt, holder);
        ctx.gpr[16] = object;
        if (object == 0u) {
            ctx.gpr[4] = 0x08CA39F0u;
            (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CA39F0u});
        }
        const std::uint32_t flag = byte_u(rt, object + 0x127u) != 0u ? 1u : 0u;
        ctx.gpr[5] = flag;
        const std::uint32_t result = frame.at(8u);
        ctx.gpr[16] = result;
        ctx.gpr[4] = result;
        (void)call_keep(rt, ctx, 0x08A85B1Cu, {result, flag});
        ctx.gpr[5] = word(rt, result);
        ctx.gpr[4] = out;
        if (out != result) (void)call_keep(rt, ctx, 0x08A5BAD8u, {out, word(rt, result)});
        ctx.gpr[4] = result;
        (void)call_keep(rt, ctx, 0x08A5B9ECu, {result});
        ctx.gpr[4] = holder;
        (void)call_keep(rt, ctx, 0x08A5B9ECu, {holder});
        return;
    }
    ctx.gpr[2] = 1u;
    if (mode != 1u) return;
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x089896CCu, {frame.at(0u)});
    const std::uint32_t object = word(rt, frame.at(0u));
    ctx.gpr[18] = object;
    if (object == 0u) {
        ctx.gpr[4] = 0x08CA39F0u;
        (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CA39F0u});
    }
    const std::uint32_t argument = word(rt, args);
    ctx.gpr[17] = argument;
    if (argument == 0u) {
        ctx.gpr[4] = 0x08CA39E8u;
        (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CA39E8u});
    }
    const std::uint32_t vtable = word(rt, argument);
    const std::uint32_t function = word(rt, vtable + 0x3Cu);
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = function;
    ctx.gpr[4] = argument;
    const std::uint32_t answer = call_keep(rt, ctx, function, {argument});
    ctx.gpr[2] = answer != 0u ? 1u : 0u;
    ctx.gpr[3] = object + 0xF0u;
    rt.memory().store8(object + 0xF0u + 0xC4u, static_cast<std::uint8_t>(mode));
    ctx.gpr[4] = frame.at(0u);
    rt.memory().store8(object + 0xF0u + 0x37u, static_cast<std::uint8_t>(answer != 0u ? 1u : 0u));
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// func_08994994(out, a1, mode, args): mode 0 (getter): o = the object that func_08994954(sp+8) fills in (null: func_08A5BB7C(D_08CA41DC) first), func_08A76AAC(sp+0x10, &v) with v = func_089948A8(o) stored at sp+0xC, copied to `out` by func_08A5BAD8 unless it is `out`, both holders destroyed.
// Mode 1 (setter): func_08A0EBB4(sp, args) makes a holder and func_08994574(o, [sp]) stores it, both holders destroyed. Any other mode: v0 = 1, nothing else.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the getter half (mode 0, 0x08994994-0x08994A18), the null-object assertion of the setter (0x08994A38) and the other modes (v0 = 1, 0x08994A20) never ran: only the setter was compared.
void func_08994994(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], mode = ctx.gpr[6], args = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    ctx.gpr[17] = out;
    ctx.gpr[16] = args;
    if (mode == 0u) {
        const std::uint32_t holder = frame.at(8u);
        ctx.gpr[18] = holder;
        ctx.gpr[4] = holder;
        (void)call_keep(rt, ctx, 0x08994954u, {holder});
        const std::uint32_t object = word(rt, holder);
        ctx.gpr[16] = object;
        ctx.gpr[4] = object;
        if (object == 0u) {
            ctx.gpr[4] = 0x08CA41DCu;
            (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CA41DCu});
            ctx.gpr[4] = object;
        }
        const std::uint32_t value = call_keep(rt, ctx, 0x089948A8u, {object});
        const std::uint32_t result = frame.at(0x10u);
        ctx.gpr[16] = result;
        ctx.gpr[4] = result;
        ctx.gpr[5] = frame.at(0xCu);
        put(rt, frame.at(0xCu), value);
        (void)call_keep(rt, ctx, 0x08A76AACu, {result, frame.at(0xCu)});
        ctx.gpr[5] = word(rt, result);
        ctx.gpr[4] = out;
        if (out != result) (void)call_keep(rt, ctx, 0x08A5BAD8u, {out, word(rt, result)});
        ctx.gpr[4] = result;
        (void)call_keep(rt, ctx, 0x08A5B9ECu, {result});
        ctx.gpr[4] = holder;
        (void)call_keep(rt, ctx, 0x08A5B9ECu, {holder});
        return;
    }
    ctx.gpr[2] = 1u;
    if (mode != 1u) return;
    const std::uint32_t holder = frame.at(4u);
    ctx.gpr[18] = holder;
    ctx.gpr[4] = holder;
    (void)call_keep(rt, ctx, 0x08994954u, {holder});
    const std::uint32_t object = word(rt, holder);
    ctx.gpr[17] = object;
    ctx.gpr[5] = args;
    if (object == 0u) {
        ctx.gpr[4] = 0x08CA41DCu;
        (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CA41DCu});
        ctx.gpr[5] = args;
    }
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08A0EBB4u, {frame.at(0u), args});
    ctx.gpr[5] = word(rt, frame.at(0u));
    ctx.gpr[4] = object;
    (void)call_keep(rt, ctx, 0x08994574u, {object, word(rt, frame.at(0u))});
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    ctx.gpr[4] = holder;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {holder});
}

// func_0899EF08(out, a1, mode, args): a float property o[0x130] of the object o that func_0899DD60 fills in (null: func_08A5BB7C(D_08CA496C) first). Mode 0 (getter): func_08A87EEC(sp+8, o[0x130]), copied to `out` by func_08A5BAD8 unless it is `out`, both holders destroyed. Mode 1 (setter): the
// argument object *args (null: func_08A5BB7C(D_08CA485C) first) answers its virtual slot 0x54 with a float, stored in o[0x130]; the holder is destroyed. Any other mode: v0 = 1, nothing else.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the setter half (mode 1, 0x0899EF84-0x0899EFE4), the other modes (v0 = 1, 0x0899EF88), the null-object assertion (0x0899EF38) and `out` being the holder (0x0899EF5C) never occurred: only the getter was compared.
void func_0899EF08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], mode = ctx.gpr[6], args = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[17] = out;
    ctx.gpr[16] = args;
    if (mode == 0u) {
        const std::uint32_t holder = frame.at(4u);
        ctx.gpr[18] = holder;
        ctx.gpr[4] = holder;
        (void)call_keep(rt, ctx, 0x0899DD60u, {holder});
        const std::uint32_t object = word(rt, holder);
        ctx.gpr[16] = object;
        if (object == 0u) {
            ctx.gpr[4] = 0x08CA496Cu;
            (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CA496Cu});
        }
        const float value = float_at(rt, object + 0x130u);
        ctx.fpr[12] = value;
        const std::uint32_t result = frame.at(8u);
        ctx.gpr[16] = result;
        ctx.gpr[4] = result;
        (void)call_keep(rt, ctx, 0x08A87EECu, {result}, {value});
        ctx.gpr[5] = word(rt, result);
        ctx.gpr[4] = out;
        if (out != result) (void)call_keep(rt, ctx, 0x08A5BAD8u, {out, word(rt, result)});
        ctx.gpr[4] = result;
        (void)call_keep(rt, ctx, 0x08A5B9ECu, {result});
        ctx.gpr[4] = holder;
        (void)call_keep(rt, ctx, 0x08A5B9ECu, {holder});
        return;
    }
    ctx.gpr[2] = 1u;
    if (mode != 1u) return;
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x0899DD60u, {frame.at(0u)});
    const std::uint32_t object = word(rt, frame.at(0u));
    ctx.gpr[17] = object;
    if (object == 0u) {
        ctx.gpr[4] = 0x08CA496Cu;
        (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CA496Cu});
    }
    const std::uint32_t argument = word(rt, args);
    ctx.gpr[16] = argument;
    if (argument == 0u) {
        ctx.gpr[4] = 0x08CA485Cu;
        (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CA485Cu});
    }
    const std::uint32_t vtable = word(rt, argument);
    const std::uint32_t function = word(rt, vtable + 0x54u);
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = function;
    ctx.gpr[4] = argument;
    const GuestResult answer = call_keep(rt, ctx, function, {argument}, {});
    put(rt, object + 0x130u, std::bit_cast<std::uint32_t>(answer.f0));
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// func_089D30D4(out, a1, mode, args): a holder property of the object o that func_089CD62C(sp+4) fills in (null: func_08A5BB7C(D_08CA69B8) first). Mode 0 (getter): func_089CC934(sp, o), copied to `out` by func_08A5BAD8 unless it is `out`, both holders destroyed. Mode 1 (setter):
// func_08A76ABC(sp+8, args) makes a holder and func_089D30B8(o, sp+8) stores it, both holders destroyed. Any other mode: v0 = 1, nothing else.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the getter half (mode 0, 0x089D30EC-0x089D3144), the other modes (v0 = 1, 0x089D3150) and the null-object assertion of the setter (0x089D3178) never ran: only the setter was compared.
void func_089D30D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], mode = ctx.gpr[6], args = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[17] = out;
    ctx.gpr[16] = args;
    const std::uint32_t holder = frame.at(4u);
    if (mode == 0u) {
        ctx.gpr[18] = holder;
        ctx.gpr[4] = holder;
        (void)call_keep(rt, ctx, 0x089CD62Cu, {holder});
        const std::uint32_t object = word(rt, holder);
        ctx.gpr[16] = object;
        ctx.gpr[5] = object;
        if (object == 0u) {
            ctx.gpr[4] = 0x08CA69B8u;
            (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CA69B8u});
            ctx.gpr[5] = object;
        }
        ctx.gpr[4] = frame.at(0u);
        (void)call_keep(rt, ctx, 0x089CC934u, {frame.at(0u), object});
        ctx.gpr[5] = word(rt, frame.at(0u));
        ctx.gpr[4] = out;
        if (out != frame.at(0u)) (void)call_keep(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(0u))});
        ctx.gpr[4] = frame.at(0u);
        (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
        ctx.gpr[4] = holder;
        (void)call_keep(rt, ctx, 0x08A5B9ECu, {holder});
        return;
    }
    ctx.gpr[2] = 1u;
    if (mode != 1u) return;
    ctx.gpr[18] = holder;
    ctx.gpr[4] = holder;
    ctx.gpr[17] = frame.at(8u);
    (void)call_keep(rt, ctx, 0x089CD62Cu, {holder});
    ctx.gpr[5] = args;
    ctx.gpr[4] = frame.at(8u);
    (void)call_keep(rt, ctx, 0x08A76ABCu, {frame.at(8u), args});
    const std::uint32_t object = word(rt, holder);
    ctx.gpr[16] = object;
    ctx.gpr[4] = object;
    if (object == 0u) {
        ctx.gpr[4] = 0x08CA69B8u;
        (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CA69B8u});
        ctx.gpr[4] = object;
    }
    ctx.gpr[5] = frame.at(8u);
    (void)call_keep(rt, ctx, 0x089D30B8u, {object, frame.at(8u)});
    ctx.gpr[4] = frame.at(8u);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(8u)});
    ctx.gpr[4] = holder;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {holder});
}

// func_089FB3A4(n): c = func_089F9BFC(n) (a vector of 12-byte elements: begin c[0], end c[1]); the range after the first n elements is erased by func_08C0EBC0(c, begin + 12 * n, end, sp+4) when n < size, else func_08C0E874(c, end, n - size, sp+4) grows it; the elements
// func_08BE5888([sp+4], [sp+8], sp) are destroyed and the block [sp+4] freed by func_08B35254 when set; then o = func_089F9E54(): func_08C0DD50(o, o[8]), o[0x10] = o[0xC] = o + 4, o[0x14] = o[8] = 0. v0 = o + 4.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): n below the current size (the erase path func_08C0EBC0, 0x089FB404-0x089FB410) and a non-null erased block (the func_08B35254 free, 0x089FB440) never occurred.
void func_089FB3A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t n = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[16] = n;
    const std::uint32_t container = call_keep(rt, ctx, 0x089F9BFCu, {n});
    ctx.gpr[10] = container;
    put(rt, frame.at(4u), 0u);
    put(rt, frame.at(8u), 0u);
    put(rt, frame.at(0xCu), 0u);
    ctx.gpr[2] = 0xAAAAAAABu;
    const std::uint32_t end = word(rt, container + 4u), begin = word(rt, container);
    ctx.gpr[9] = end;
    ctx.gpr[8] = begin;
    ctx.gpr[7] = frame.at(4u);
    ctx.gpr[6] = end;
    const std::uint32_t size = static_cast<std::uint32_t>(signed_of(end - begin) >> 2) * 0xAAAAAAABu;
    ctx.gpr[3] = size;
    ctx.gpr[5] = begin + n * 12u;
    ctx.gpr[4] = container;
    if (n < size) {
        (void)call_keep(rt, ctx, 0x08C0EBC0u, {container, begin + n * 12u, end, frame.at(4u)});
    } else {
        ctx.gpr[6] = n - size;
        ctx.gpr[5] = end;
        (void)call_keep(rt, ctx, 0x08C0E874u, {container, end, n - size, frame.at(4u)});
    }
    const std::uint32_t first = word(rt, frame.at(4u));
    ctx.gpr[4] = first;
    ctx.gpr[5] = word(rt, frame.at(8u));
    ctx.gpr[6] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08BE5888u, {first, word(rt, frame.at(8u)), frame.at(0u)});
    const std::uint32_t block = word(rt, frame.at(4u));
    ctx.gpr[2] = block;
    if (block != 0u) {
        ctx.gpr[4] = block;
        (void)call_keep(rt, ctx, 0x08B35254u, {block});
    }
    const std::uint32_t object = call_keep(rt, ctx, 0x089F9E54u, {});
    ctx.gpr[5] = word(rt, object + 8u);
    ctx.gpr[16] = object;
    ctx.gpr[4] = object;
    (void)call_keep(rt, ctx, 0x08C0DD50u, {object, word(rt, object + 8u)});
    ctx.gpr[2] = object + 4u;
    put(rt, object + 0x10u, object + 4u);
    put(rt, object + 0x14u, 0u);
    put(rt, object + 0xCu, object + 4u);
    put(rt, object + 8u, 0u);
}

// func_08A37510(a0, context, argc, args): nothing for argc <= 0 (v0 untouched). Otherwise the object o that func_08A36FD0(sp+8, context) fills in (null: func_08A5BB7C(D_08CAD0CC) first) and the argument object *args (null: func_08A5BB7C(D_08CAD0C4) first), whose virtual slot 0xC
// gives a string at sp+4: func_08A36E44(o, sp+4); the string is released (release_string) and the holder at sp+8 destroyed. v0 is the last call's.
void func_08A37510(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t context = ctx.gpr[5], argc = ctx.gpr[6], args = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    ctx.gpr[19] = frame.at(8u);
    ctx.gpr[18] = frame.at(4u);
    ctx.gpr[16] = args;
    ctx.gpr[4] = frame.at(8u);
    if (signed_of(argc) <= 0) return;
    (void)call_keep(rt, ctx, 0x08A36FD0u, {frame.at(8u), context});
    const std::uint32_t object = word(rt, frame.at(8u));
    ctx.gpr[17] = object;
    ctx.gpr[4] = 0x08CAD0CCu;
    if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CAD0CCu});
    const std::uint32_t argument = word(rt, args);
    ctx.gpr[16] = argument;
    ctx.gpr[4] = 0x08CAD0C4u;
    if (argument == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CAD0C4u});
    const std::uint32_t vtable = word(rt, argument);
    const std::uint32_t function = word(rt, vtable + 0xCu);
    ctx.gpr[3] = vtable;
    ctx.gpr[5] = argument;
    ctx.gpr[4] = frame.at(4u);
    ctx.gpr[2] = function;
    (void)call_keep(rt, ctx, function, {frame.at(4u), argument});
    ctx.gpr[4] = object;
    ctx.gpr[5] = frame.at(4u);
    (void)call_keep(rt, ctx, 0x08A36E44u, {object, frame.at(4u)});
    release_string(rt, ctx, frame.at(4u), frame.at(0u));
    ctx.gpr[4] = frame.at(8u);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(8u)});
}

// func_08A3BD88(a0, context, argc, args): the object o that func_08A3A9D0(sp+4, context) fills in; with argc > 0 the argument object *args (null: func_08A5BB7C(D_08CAD518) first) gives a string by its virtual slot 0xC (called with (sp+8, argument)) and func_08A3BD88's
// worker func_08A3A73C(o, string data, 0) runs (o null: func_08A5BB7C(D_08CAD544) first), then the string is released (release_string); with argc <= 0 func_08A3A73C(o, D_08CAD66C, 0) runs instead. The holder at sp+4 is destroyed. v0 is the last call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): argc <= 0 (the default-string path, 0x08A3BDB0-0x08A3BE44) and the null assertions (0x08A3BDBC, 0x08A3BDE8, 0x08A3BE44) never occurred.
void func_08A3BD88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t context = ctx.gpr[5], argc = ctx.gpr[6], args = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    const std::uint32_t holder = frame.at(4u);
    ctx.gpr[18] = holder;
    ctx.gpr[4] = holder;
    ctx.gpr[16] = argc;
    ctx.gpr[17] = args;
    (void)call_keep(rt, ctx, 0x08A3A9D0u, {holder, context});
    std::uint32_t object = word(rt, holder);
    ctx.gpr[16] = object;
    if (signed_of(argc) <= 0) {
        ctx.gpr[5] = 0x08CA0000u;
        if (object == 0u) {
            ctx.gpr[4] = 0x08CAD544u;
            (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CAD544u});
            ctx.gpr[5] = 0x08CB0000u;
        }
        ctx.gpr[4] = object;
        ctx.gpr[5] = 0x08CAD66Cu;
        ctx.gpr[6] = 0u;
        (void)call_keep(rt, ctx, 0x08A3A73Cu, {object, 0x08CAD66Cu, 0u});
    } else {
        const std::uint32_t argument = word(rt, args);
        ctx.gpr[16] = argument;
        if (argument == 0u) {
            ctx.gpr[4] = 0x08CAD518u;
            (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CAD518u});
        }
        const std::uint32_t vtable = word(rt, argument);
        const std::uint32_t function = word(rt, vtable + 0xCu);
        ctx.gpr[3] = vtable;
        ctx.gpr[5] = argument;
        ctx.gpr[4] = frame.at(8u);
        ctx.gpr[2] = function;
        (void)call_keep(rt, ctx, function, {frame.at(8u), argument});
        object = word(rt, holder);
        ctx.gpr[16] = object;
        ctx.gpr[5] = word(rt, frame.at(8u));
        if (object == 0u) {
            ctx.gpr[4] = 0x08CAD544u;
            (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CAD544u});
            ctx.gpr[5] = word(rt, frame.at(8u));
        }
        ctx.gpr[4] = object;
        ctx.gpr[6] = 0u;
        (void)call_keep(rt, ctx, 0x08A3A73Cu, {object, word(rt, frame.at(8u)), 0u});
        release_string(rt, ctx, frame.at(8u), frame.at(0u));
    }
    ctx.gpr[4] = holder;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {holder});
}

// func_08A8712C(out, value, a2, a3): the holder at sp+4 is made by func_08A76AAC(sp+4, &value) (value kept at sp); the 12-byte object o = operator new (func_08B62344) gets vtable D_08CD1F30, func_08A76AA0(o + 4), vtable D_08CD4168 and func_08A62AB8(o + 8);
// the slot s = o[8] (null: func_08A5BB7C(D_08CB18C4) first) is set by func_08A641F4(s, sp+4, a2, a3); func_08A851CC(out, o) fills `out` and the holder is destroyed. v0 = out.
void func_08A8712C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], value = ctx.gpr[5], a2 = ctx.gpr[6], a3 = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    const std::uint32_t holder = frame.at(4u);
    ctx.gpr[18] = holder;
    ctx.gpr[21] = out;
    ctx.gpr[4] = holder;
    put(rt, frame.at(0u), value);
    ctx.gpr[5] = frame.at(0u);
    ctx.gpr[20] = a3;
    ctx.gpr[19] = a2;
    (void)call_keep(rt, ctx, 0x08A76AACu, {holder, frame.at(0u)});
    ctx.gpr[4] = 0xCu;
    const std::uint32_t object = call_keep(rt, ctx, 0x08B62344u, {0xCu});
    ctx.gpr[16] = object;
    ctx.gpr[2] = 0x08CD1F30u;
    put(rt, object, 0x08CD1F30u);
    ctx.gpr[4] = object + 4u;
    (void)call_keep(rt, ctx, 0x08A76AA0u, {object + 4u});
    ctx.gpr[2] = 0x08CD4168u;
    ctx.gpr[4] = object + 8u;
    put(rt, object, 0x08CD4168u);
    (void)call_keep(rt, ctx, 0x08A62AB8u, {object + 8u});
    const std::uint32_t slot = word(rt, object + 8u);
    ctx.gpr[17] = slot;
    ctx.gpr[4] = 0x08CB18C4u;
    if (slot == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CB18C4u});
    ctx.gpr[6] = a2;
    ctx.gpr[7] = a3;
    ctx.gpr[4] = slot;
    ctx.gpr[5] = holder;
    (void)call_keep(rt, ctx, 0x08A641F4u, {slot, holder, a2, a3});
    ctx.gpr[4] = out;
    ctx.gpr[5] = object;
    (void)call_keep(rt, ctx, 0x08A851CCu, {out, object});
    ctx.gpr[4] = holder;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {holder});
    ctx.gpr[2] = out;
}


}  // namespace
}  // namespace hot_batch_88
}  // namespace gt::game

#define HOT88(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_88::name, gt::ReturnKind::kind)
#define HOT88N(address, name, kind) GT_DECOMP_NESTED(address, gt::game::hot_batch_88::name, gt::ReturnKind::kind)
HOT88(0x08B1CA60u, func_08B1CA60, Word);
HOT88(0x08B2068Cu, func_08B2068C, Word);
// Not registered (no compared case changed memory): HOT88(0x088941A0u, func_088941A0, Word);
HOT88(0x088D5028u, func_088D5028, Word);
// Not registered (never reached by the routes (calls 0) in the fast gates): HOT88(0x088A1A3Cu, func_088A1A3C, Word);
// Not registered (no compared case changed memory): HOT88(0x088D7158u, func_088D7158, Word);
HOT88(0x088E96F4u, func_088E96F4, Word);
HOT88(0x088F8BACu, func_088F8BAC, Word);
HOT88(0x088F9008u, func_088F9008, Word);
HOT88(0x08969F64u, func_08969F64, Word);
HOT88(0x08972AB0u, func_08972AB0, Word);
HOT88(0x088A1BC8u, func_088A1BC8, Word);
// Not registered (every case reaches an import stub (not comparable: sceUtility)): HOT88(0x08AB9B04u, func_08AB9B04, Word);
HOT88(0x08BB1A00u, func_08BB1A00, Word);
HOT88(0x08BB9C24u, func_08BB9C24, Word);
HOT88(0x08BB9E4Cu, func_08BB9E4C, Word);
// Not registered (every case stops on a held LwMutex (not comparable)): HOT88(0x08BCF9E4u, func_08BCF9E4, Word);
HOT88(0x08B958E8u, func_08B958E8, Word);
HOT88(0x08BFA640u, func_08BFA640, Word);
HOT88(0x08C4FF60u, func_08C4FF60, Word);
HOT88(0x08BCF61Cu, func_08BCF61C, Word);
HOT88(0x08B71DC8u, func_08B71DC8, Word);
HOT88(0x08B8A8A8u, func_08B8A8A8, Word);
HOT88(0x08B893E8u, func_08B893E8, Word);
HOT88(0x08B894E4u, func_08B894E4, Word);
HOT88(0x08B904D8u, func_08B904D8, Word);
HOT88(0x08B8AF08u, func_08B8AF08, Word);
HOT88(0x08BA3718u, func_08BA3718, Word);
HOT88(0x08AD5CE8u, func_08AD5CE8, Word);
HOT88(0x08AE9F98u, func_08AE9F98, Word);
HOT88(0x0880CE2Cu, func_0880CE2C, Word);
HOT88(0x0880F470u, func_0880F470, Word);
HOT88(0x0881EAD0u, func_0881EAD0, Word);
HOT88(0x08A25FACu, func_08A25FAC, Word);
// Not registered (every case stops on a held LwMutex (not comparable)): HOT88(0x0897AEE4u, func_0897AEE4, Word);
// Not registered (every case stops on a held LwMutex (not comparable)): HOT88(0x08A4AE9Cu, func_08A4AE9C, Word);
HOT88(0x089F01F4u, func_089F01F4, Word);
HOT88(0x08A9FA9Cu, func_08A9FA9C, Word);
HOT88(0x088388BCu, func_088388BC, Word);
HOT88(0x0883C4B4u, func_0883C4B4, Word);
HOT88(0x08A213C0u, func_08A213C0, Word);
HOT88(0x0886F5FCu, func_0886F5FC, Word);
HOT88N(0x088710DCu, func_088710DC, Word);
HOT88(0x0887CCA8u, func_0887CCA8, Word);
HOT88(0x0887F4FCu, func_0887F4FC, Word);
HOT88(0x0885CBCCu, func_0885CBCC, Word);
HOT88(0x0881FDECu, func_0881FDEC, Word);
HOT88(0x088213BCu, func_088213BC, Word);
HOT88(0x08828ABCu, func_08828ABC, Word);
// Not registered (no compared case changed memory): HOT88(0x08828C8Cu, func_08828C8C, Word);
// Not registered (no compared case changed memory): HOT88(0x088285ACu, func_088285AC, Word);
// Not registered (no compared case changed memory): HOT88(0x088286B8u, func_088286B8, Word);
// Not registered (no compared case changed memory): HOT88(0x088341ECu, func_088341EC, Word);
HOT88(0x088342F4u, func_088342F4, Word);
HOT88(0x0884A5CCu, func_0884A5CC, Word);
HOT88(0x0884A6DCu, func_0884A6DC, Word);
HOT88(0x08829CC4u, func_08829CC4, Word);
HOT88(0x0882B744u, func_0882B744, Word);
// Not registered (no compared case changed memory): HOT88(0x0883365Cu, func_0883365C, Word);
HOT88(0x08835E1Cu, func_08835E1C, Word);
HOT88(0x088381F8u, func_088381F8, Word);
// Not registered (no compared case changed memory): HOT88(0x08845BA8u, func_08845BA8, Word);
HOT88(0x08855C68u, func_08855C68, Word);
// Not registered (no compared case changed memory): HOT88(0x0898BAB4u, func_0898BAB4, Word);
HOT88(0x08994994u, func_08994994, Word);
HOT88(0x0899EF08u, func_0899EF08, Word);
HOT88(0x089D30D4u, func_089D30D4, Word);
HOT88(0x089FB3A4u, func_089FB3A4, Word);
// Not registered (every case reaches an import stub (not comparable: sceIoRead)): HOT88(0x08A37510u, func_08A37510, Word);
HOT88(0x08A3BD88u, func_08A3BD88, Word);
// Not registered (every case stops on a held LwMutex (not comparable)): HOT88(0x08A8712Cu, func_08A8712C, Word);
