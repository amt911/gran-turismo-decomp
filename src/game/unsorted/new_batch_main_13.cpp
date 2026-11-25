// New batch main 13: 80 functions the recompiler emitted and nothing had decompiled, written by hand from
// `tools.disasm` as structured C. DRAFT: none of them has run the differential gate yet.
//
// Shapes several addresses share are one helper with the per-function constants in the wrapper: 14 map inserts,
// 5 multimap inserts, 5 member-pointer dispatches, 4 global-list resets, 4 constructor tests, 3 virtual erases,
// 2 string assigns, 2 pair applies, 2 text compares and 2 poll loops. A helper's literals are outside what
// tools.literal_check reads (it only walks `void func_X(` bodies), so each one was read back against every
// function that uses it.
//
// Every call passes the argument registers the original sets before the `jal` (call_set writes them into ctx, so
// a register the original leaves holding an earlier call's result keeps the caller's value here); the delay slot
// of a `jal` runs BEFORE the call, and the delay slot of a `beql`/`bnel`/`bltzl`/`bgtzl` only when the branch is
// taken. v0 is set as the original leaves it on every exit path: a path ending in a call keeps that call's v0, a
// path that never writes v0 keeps the caller's. The dead stack is not reproduced; the frames callees read or write
// are (GuestFrame, offsets as in the original).
//
// No names: none of these has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_13 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using std::int16_t;
using std::int8_t;
using std::uint8_t;
using u32 = std::uint32_t;
using s32 = std::int32_t;

inline s32 signed_of(u32 value) { return static_cast<s32>(value); }
inline u32 byte_at(Runtime &rt, u32 address) { return rt.memory().load8(address); }
inline u32 half_at(Runtime &rt, u32 address) { return rt.memory().load16(address); }
inline void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<uint8_t>(value)); }
inline u32 sign_byte(u32 value) { return static_cast<u32>(static_cast<s32>(static_cast<int8_t>(value))); }
inline s32 sign_half(u32 value) { return static_cast<s32>(static_cast<int16_t>(value)); }

inline void take(AllegrexContext &ctx, const GuestResult &result) {
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}
inline GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                             std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, floats);
    take(ctx, result);
    return result;
}
inline u32 call_keep(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    return call_keep(rt, ctx, address, arguments, {}).v0;
}
// The same when the caller needs v1 / f0 as well.
inline GuestResult call_full(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    return call_keep(rt, ctx, address, arguments, {});
}
// A call with the argument registers the original sets before the `jal` (a0-a3, t0-t3 in order).
inline u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments);
}
// The same for a callee that also takes floats in $f12-$f15.
inline GuestResult call_setf(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                             std::initializer_list<float> floats) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    unsigned freg = 12u;
    for (const float argument : floats) {
        if (freg <= 15u) ctx.fpr[freg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments, floats);
}
// `lw v0, slot(vtable); jalr v0`: the callee starts with v0 = its own address, which one that never writes v0
// returns — the caller then sees the target, as the original's caller does.
inline u32 call_virtual(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    ctx.gpr[2] = target;
    return call_keep(rt, ctx, target, arguments);
}
// An indirect call whose target the compiler kept out of v0 (`lw $sN, slot(vtable); jalr $sN`, `jr $t9`): v0 is
// left as the previous code set it, so a callee that never writes v0 hands that value on.
inline GuestResult call_indirect_keep(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    return call_keep(rt, ctx, target, arguments, {});
}
// MIPS `ins rd, rs, pos, size`.
inline u32 insert_bits(u32 target, u32 value, unsigned pos, unsigned size) {
    const u32 mask = (size >= 32u ? 0xFFFFFFFFu : ((1u << size) - 1u)) << pos;
    return (target & ~mask) | ((value << pos) & mask);
}

// --- the map insert (14 addresses) ---------------------------------------------------------------------------
// a0 = the map, a1 = a holder whose first word is the tree. The key is `key_function(map, 0)` (one of the global
// getters the wrappers name, so it reads no argument). With no tree, the holder's zero is released and the answer
// is 1. With a tree, the key is looked up (func_08A76F20): found -> the tree is released (func_08A5BAD8) and the
// answer is 1; not found -> func_08A76AA0 puts the new node in the frame's first word, which is released when it
// came back non-zero, func_08A5B9EC destroys the temporary, and the answer is 1 when a node came back, else 0.
u32 map_insert(Runtime &rt, AllegrexContext &ctx, u32 key_function) {
    const u32 map = ctx.gpr[4];
    const u32 holder = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    u32 inserted = 1u;
    const u32 tree = word(rt, holder);
    if (tree == 0u) {
        call_set(rt, ctx, 0x08A5BAD8u, {map, 0u});
        ctx.gpr[2] = inserted;
        return inserted;
    }
    const u32 key = call_set(rt, ctx, key_function, {map, 0u});
    if (call_set(rt, ctx, 0x08A76F20u, {tree, key}) != 0u) {
        call_set(rt, ctx, 0x08A5BAD8u, {map, tree});
        ctx.gpr[2] = inserted;
        return inserted;
    }
    inserted = 0u;  // the call's delay slot
    call_set(rt, ctx, 0x08A76AA0u, {frame.at(0u)});
    const u32 node = word(rt, frame.at(0u));
    if (node != 0u) {
        inserted = 1u;  // that call's delay slot
        call_set(rt, ctx, 0x08A5BAD8u, {map, node});
    }
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    ctx.gpr[2] = inserted;
    return inserted;
}

// --- a map erase through the holder's node (3 addresses) -----------------------------------------------------
// a1 = the argument `prepare` takes in a0 (with a1 = the pair at sp+8 it fills), a2 = the holder. With no node,
// `erase` runs on the pair at sp+4 with the two words `prepare` returned, the holder is released when it is not
// that pair, and the temporary is destroyed. With a node, `erase` runs on the frame and the node's virtual at
// vtable + 0xF4 (a0 = node, a1 = the frame) does the work before the same destruction. v0 = func_08A5B9EC's.
void map_erase(Runtime &rt, AllegrexContext &ctx, u32 prepare, u32 erase) {
    const u32 argument = ctx.gpr[5];
    const u32 holder = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    const u32 pair = frame.at(8u);
    const u32 temporary = frame.at(4u);
    call_set(rt, ctx, prepare, {argument, pair});
    const u32 node = word(rt, holder);
    if (node == 0u) {
        call_set(rt, ctx, erase, {temporary, ctx.gpr[5], word(rt, pair), word(rt, pair + 4u)});
        if (holder != temporary) call_set(rt, ctx, 0x08A5BAD8u, {holder, word(rt, temporary)});
        ctx.gpr[2] = call_set(rt, ctx, 0x08A5B9ECu, {temporary});
        return;
    }
    const u32 target = word(rt, word(rt, node) + 0xF4u);
    call_set(rt, ctx, erase, {frame.at(0u), ctx.gpr[5], word(rt, pair), word(rt, pair + 4u)});
    call_indirect_keep(rt, ctx, target, {node, temporary});
    ctx.gpr[2] = call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// --- a map insert keyed by a value read from the frame (5 addresses) -----------------------------------------
enum class KeyLoad { UnsignedHalf, UnsignedByte, SignedHalf, SignedByte };

u32 load_key(Runtime &rt, u32 address, KeyLoad kind) {
    switch (kind) {
    case KeyLoad::UnsignedHalf: return half_at(rt, address);
    case KeyLoad::UnsignedByte: return byte_at(rt, address);
    case KeyLoad::SignedHalf: return static_cast<u32>(sign_half(half_at(rt, address)));
    case KeyLoad::SignedByte: return sign_byte(byte_at(rt, address));
    }
    return 0u;
}

// a1 = the map (moved to a0), a2 = the holder; `prepare` fills the frame for the map, and the key is the first
// halfword/byte of it. With no node, `insert` adds the key with the temporary at sp+8 as its a0, the holder is
// released when it is not that temporary, and the temporary is destroyed. With a node, `insert` works on sp+4, the
// node's virtual at vtable + 0xF4 (a0 = node, a1 = sp+4) runs, and the temporary at sp+4 is destroyed.
void multimap_insert(Runtime &rt, AllegrexContext &ctx, u32 prepare, u32 insert, KeyLoad kind) {
    const u32 map = ctx.gpr[5];
    const u32 holder = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    const u32 pair = frame.at(0u);
    const u32 node_slot = frame.at(4u);
    const u32 key_slot = frame.at(8u);
    call_set(rt, ctx, prepare, {map, pair});
    const u32 node = word(rt, holder);
    const u32 key = load_key(rt, pair, kind);
    if (node == 0u) {
        call_set(rt, ctx, insert, {key_slot, key});
        if (holder != key_slot) call_set(rt, ctx, 0x08A5BAD8u, {holder, word(rt, key_slot)});
        ctx.gpr[2] = call_set(rt, ctx, 0x08A5B9ECu, {key_slot});
        return;
    }
    const u32 target = word(rt, word(rt, node) + 0xF4u);
    call_set(rt, ctx, insert, {node_slot, key});
    call_indirect_keep(rt, ctx, target, {node, node_slot});
    ctx.gpr[2] = call_set(rt, ctx, 0x08A5B9ECu, {node_slot});
}

// --- the member-pointer dispatch (5 addresses) ---------------------------------------------------------------
// a1 holds two (offset, function) pairs: the packed word at +0x18 resp. +0x20 has bit 0 set when the call is
// indirect, and its upper bits are an offset added to a2 (first) / to the first result (second). The target is the
// word at +0x14 resp. +0x1C, or the virtual at that word of the adjusted object. The last call takes a0 = a0 and
// a1 = the second result; v0 = a0 (the original's s1) however the calls went.
void dispatch_pair(Runtime &rt, AllegrexContext &ctx, u32 last_call) {
    const u32 result_holder = ctx.gpr[4];
    const u32 pairs = ctx.gpr[5];
    const u32 base = ctx.gpr[6];
    const u32 first_offset = word(rt, pairs + 0x18u);
    const u32 first_object = base + static_cast<u32>(signed_of(first_offset) >> 1);
    const u32 first_function = word(rt, pairs + 0x14u);
    const u32 first_target = (first_offset & 1u) != 0u ? word(rt, word(rt, first_object) + first_function) : first_function;
    ctx.gpr[2] = first_target;  // both paths leave the target in v0 before the jalr
    const u32 first_result = call_indirect_keep(rt, ctx, first_target, {first_object}).v0;
    const u32 second_offset = word(rt, pairs + 0x20u);
    const u32 second_object = first_result + static_cast<u32>(signed_of(second_offset) >> 1);
    const u32 second_function = word(rt, pairs + 0x1Cu);
    const u32 second_target = (second_offset & 1u) != 0u ? word(rt, word(rt, second_object) + second_function) : second_function;
    ctx.gpr[2] = second_target;
    const u32 second_result = call_indirect_keep(rt, ctx, second_target, {second_object}).v0;
    call_set(rt, ctx, last_call, {result_holder, second_result});
    ctx.gpr[2] = result_holder;
}

// --- reset the global list at 0x08D75F5C (4 addresses) -------------------------------------------------------
// Destroy every node from the list tail back to the head (the destructor 0x08BB1534 takes the word before each
// node), then rebuild the list around the table 0x08D75F68 with zeroed counters (0x08BB1324 / 0x08BB1304) and free
// the head (0x08B35254) when it is not null. v0 = the last call's answer.
void reset_global_list(Runtime &rt, AllegrexContext &ctx) {
    constexpr u32 kList = 0x08D75F5Cu;
    GuestFrame frame(ctx, 0x20u);
    u32 node = word(rt, kList + 4u);
    const u32 head = word(rt, kList);
    while (node != head) {
        call_set(rt, ctx, 0x08BB1534u, {word(rt, node - 4u)});
        node -= 4u;
    }
    const u32 body = kList;
    call_set(rt, ctx, 0x08BB1324u, {body, ctx.gpr[5], word(rt, body + 4u)});
    put(rt, body + 0xCu, 0x08D75F68u);
    put(rt, body + 0x10u, 0u);
    put(rt, body + 0x1Cu, 0u);
    put(rt, body + 0x20u, 0u);
    put(rt, body + 0x24u, 0u);
    put(rt, body + 0x14u, 0u);
    call_set(rt, ctx, 0x08BB1304u, {word(rt, kList), word(rt, body + 4u), frame.at(0u)});
    const u32 first = word(rt, kList);
    if (first != 0u) call_set(rt, ctx, 0x08B35254u, {first});
}

// --- two stack-built pieces handed to a setter (2 addresses) -------------------------------------------------
// a0 = the output holder. `first(a0 = sp)` and func_08A3F848(a0 = sp+4) build two handles (each checked against an
// "empty" string), then func_08B2171C copies what the second holds (+0x14) from the first (+0x1C), the output is
// released when it is not sp+4, and both temporaries are destroyed. v0 = the last destruction's answer.
void pair_apply(Runtime &rt, AllegrexContext &ctx, u32 first, u32 first_assert) {
    const u32 out = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    const u32 source = frame.at(0u);
    const u32 target = frame.at(4u);
    call_set(rt, ctx, first, {source, ctx.gpr[5]});
    call_set(rt, ctx, 0x08A3F848u, {target, ctx.gpr[5]});
    const u32 first_value = word(rt, source);
    if (first_value == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {first_assert});
    const u32 second_value = word(rt, target);
    if (second_value == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CADA5Cu});
    call_set(rt, ctx, 0x08B2171Cu, {second_value + 0x14u, first_value + 0x1Cu});
    if (out != target) call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, target)});
    call_set(rt, ctx, 0x08A5B9ECu, {target});
    ctx.gpr[2] = call_set(rt, ctx, 0x08A5B9ECu, {source});
}

// --- assign into a string object (2 addresses) ---------------------------------------------------------------
// a0 = the string object, a1 = a value passed through, a2 = an offset added to the object's data pointer. The
// first call (a0 = data + offset, a1 = the object's length, a2 = a1) returns the destination block; its byte
// content (0xC) is copied when the length is 1 (func_08B79A88 otherwise), the length is stored at +0, a marker byte
// (the const char at `marker`) follows the content, +8 is zeroed, and v0 = the block + 0xC.
void string_assign(Runtime &rt, AllegrexContext &ctx, u32 first_call, u32 marker) {
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    const u32 offset = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    const u32 block = call_set(rt, ctx, first_call, {word(rt, self) + offset, word(rt, self + 4u), value});
    const u32 length = word(rt, self);
    const u32 content = block + 0xCu;
    if (length != 0u) {
        if (length == 1u) put8(rt, content, byte_at(rt, self + 0xCu));
        else call_set(rt, ctx, 0x08B79A88u, {content, self + 0xCu, length});
    }
    const u32 terminator = byte_at(rt, marker);
    put(rt, block, length);
    put(rt, block + 8u, 0u);
    put8(rt, content + length, terminator);
    ctx.gpr[2] = content;
}

// --- a constructor plus two name tests (4 addresses) ---------------------------------------------------------
// a0 = this, a1 = the boolean the base constructor stores, a2 = a name pointer the base keeps. The vtable is
// stored at *this, then func_08B35FD8(a1, name) tells the type: the first name miss returns its own answer, the
// second leaves 0, a match constructs the sub-object at this + 8 (func_08B61BC8 then func_08B61B90). v0 = the
// last call's answer on every path.
void construct_testing(Runtime &rt, AllegrexContext &ctx, u32 base_constructor, u32 vtable) {
    const u32 self = ctx.gpr[4];
    const u32 flag = ctx.gpr[5];
    const u32 name = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    call_set(rt, ctx, base_constructor, {self, name});
    put(rt, self, vtable);
    if (call_set(rt, ctx, 0x08B35FD8u, {flag, 0x08CB57E4u}) != 0u) {
        if (call_set(rt, ctx, 0x08B35FD8u, {flag, 0x08CB57E8u}) != 0u) {
            call_set(rt, ctx, 0x08B61BC8u, {self + 8u});
            ctx.gpr[2] = call_set(rt, ctx, 0x08B61B90u, {self + 8u, flag, 0u});
        }
    }
}

// --- a handle stored into an object (2 addresses) -----------------------------------------------------------
// a0 = the object, a1 = the value stored by the first call, a2 = whether the flag goes to func_08876188 as 1.
// func_08AB85F0 builds a handle at sp+4, func_08AB856C reads it twice (the second answer is the argument of
// func_088757EC), func_08876188 stores with the flag, and the handle's word at sp+4 (an object at -0xC) is
// released when it is not the table 0x08D75298: func_08B60804 decides (> 0 returns it, else func_08B93554 runs).
void handle_assign(Runtime &rt, AllegrexContext &ctx, u32 flag_value) {
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    const u32 handle = frame.at(4u);
    call_set(rt, ctx, 0x08AB85F0u, {handle, value});
    call_set(rt, ctx, 0x08AB856Cu, {handle});
    const u32 second = call_set(rt, ctx, 0x08AB856Cu, {handle});
    call_set(rt, ctx, 0x088757ECu, {self, second});
    call_set(rt, ctx, 0x08876188u, {self, value, flag_value, 0u});
    const u32 holder = word(rt, handle);
    const u32 tail = holder - 0xCu;
    if (tail == 0x08D75298u) {
        ctx.gpr[2] = 0x08D75298u;
        return;
    }
    const u32 code = call_set(rt, ctx, 0x08B60804u, {holder - 4u, 0xFFFFFFFFu});
    if (signed_of(code) > 0) {
        ctx.gpr[2] = code;
        return;
    }
    ctx.gpr[2] = call_set(rt, ctx, 0x08B93554u, {tail, frame.at(0u)});
}

void func_08835B2C(Runtime &rt, AllegrexContext &ctx) { map_insert(rt, ctx, 0x08835940u); }
void func_088434FC(Runtime &rt, AllegrexContext &ctx) { map_insert(rt, ctx, 0x088433FCu); }
void func_08A324EC(Runtime &rt, AllegrexContext &ctx) { map_insert(rt, ctx, 0x08A32270u); }
void func_08A93B7C(Runtime &rt, AllegrexContext &ctx) { map_insert(rt, ctx, 0x08A93690u); }

void func_08C3253C(Runtime &rt, AllegrexContext &ctx) { map_erase(rt, ctx, 0x08C31954u, 0x08C3150Cu); }





void func_08B854DC(Runtime &rt, AllegrexContext &ctx) { string_assign(rt, ctx, 0x08B8543Cu, 0x08CB7430u); }
void func_08B94288(Runtime &rt, AllegrexContext &ctx) { string_assign(rt, ctx, 0x08B93B20u, 0x08CBF760u); }



// 0x08C0E054 (a0 = the first record, a1 = the end, a2 = the output): for every 0xC-byte record from a0 to a1
// (exclusive), func_08B35840(0x10, (its word at +4 minus its word at +0) rounded down to 32) allocates the block the
// output's words at +0 / +4 point into (+8 is its end), and func_08C0D7F8(the record's +0 / +4, the block, sp) fills
// it, leaving its answer at +4. v0 = the output record the loop stopped at (a2 + 0xC per record).

// 0x08B28524 (a0 = object): with a positive count at +4, func_08B28470(a0) non-zero answers 1; otherwise, with the
// count still positive and 0xFFFF below the word at +0x64, that word becomes 0xFFFF, the byte at +0x58 is cleared,
// and the 0x8000 bytes from where +0x5C points each go through func_08B272D4. Every other path answers
// func_08B279BC(a0), which also runs after the sweep.

// 0x08B2D4E0: bring three pieces up in order, recording the stage in the global word 0x08D68CFC (0, 1, 2, 3). A
// negative answer ends the sequence with the cleanup func_08B2D45C (a0 = 0x1CB, the stage it reads) and returns that
// answer; v0 = the third answer once the stage reaches 3.

// 0x08B59A24 (a0 = a count, a1 = a base, a2 = a flag byte): with a positive count, wait until the graphics state's
// counter at 0x08D6CE20 has moved a0 (minus one when the flag is zero) past a1, polling func_08C7AC18; a zero flag
// polls once more. v0 = the counter.
void func_08B59A24(Runtime &rt, AllegrexContext &ctx) {
    const u32 count = ctx.gpr[4];
    const u32 base = ctx.gpr[5];
    const u32 flag = ctx.gpr[6] & 0xFFu;
    GuestFrame frame(ctx, 0x20u);
    if (signed_of(count) > 0) {
        const u32 wanted = count - (flag < 1u ? 1u : 0u);
        if ((word(rt, kGuState + 0x20u) - base) < wanted) {
            do {
                call_set(rt, ctx, 0x08C7AC18u, {kGuState});
            } while ((word(rt, kGuState + 0x20u) - base) < wanted);
        }
    }
    if (flag == 0u) call_set(rt, ctx, 0x08C7AC18u, {kGuState});
    ctx.gpr[2] = word(rt, kGuState + 0x20u);
}

// 0x0885765C (a0 = the output object): func_088547C4 builds a handle at sp+4 (checked against the table
// 0x08C9E188); func_08854678 resolves it, func_088A7618 reads an int, and func_08A87EEC stores that int times
// 0x38D1B717 (0.0001f) in a stack object at sp. The output is released when it is not sp, and the stack object is
// destroyed: v0 = the destruction's answer.

// 0x08896BE0 (a0 = object): the destructor. Store the vtable 0x08CC1F60, run func_08B1C950 on +0xDC, release the
// member at +0xD0 through the virtual at its vtable +0 (a1 = the word at +0xD0) when +0xD4 holds one, run
// func_08B1C950 on +0x98 and release the member at +0x8C the same way, then func_08898F78(a0) and continue in the
// tail call func_08B64654(a0).
void func_08896BE0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    put(rt, self, 0x08CC1F60u);
    call_set(rt, ctx, 0x08B1C950u, {self + 0xDCu});
    const u32 first = word(rt, self + 0xD0u + 4u);
    if (first != 0u) {
        call_virtual(rt, ctx, word(rt, word(rt, first)), {first, word(rt, self + 0xD0u)});
        put(rt, self + 0xD0u + 4u, 0u);
        put(rt, self + 0xD0u, 0u);
    }
    call_set(rt, ctx, 0x08B1C950u, {self + 0x98u});
    const u32 second = word(rt, self + 0x8Cu + 4u);
    if (second != 0u) {
        call_virtual(rt, ctx, word(rt, word(rt, second)), {second, word(rt, self + 0x8Cu)});
        put(rt, self + 0x8Cu + 4u, 0u);
        put(rt, self + 0x8Cu, 0u);
    }
    call_set(rt, ctx, 0x08898F78u, {self});
    tail_call(ctx, 0x08B64654u, {self});
}

// 0x088DE348 (a0 = object): func_088E4AA8 on the word at +0x640, and func_088E59A0 on it when that answered
// non-zero; then func_088DA168(a0): a positive answer is the return. Otherwise the object at +0x148 answers its
// virtual at vtable +0x70, func_088E49D0 puts that into the word at +0x640, func_088DE24C(a0, 1, 0) and
// func_088E57CC run, and the work continues in the tail call func_088D9BBC(a0, 1, 0).

// 0x08958D50 (a0 = object, a1 = a value passed through): func_0894CFF8(a0) names a record whose byte at +3 is held
// against 4; unless it is 4, when the object at +0x1C exists it is called through the virtual at its vtable +8 with
// a1 = the value and a2 = a 0x34-byte stack block (-1 then zeros). Then func_089593DC(a0, a1) answers.
void func_08958D50(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    GuestFrame frame(ctx, 0x50u);
    const u32 record = call_set(rt, ctx, 0x0894CFF8u, {self});
    const u32 kind = sign_byte(byte_at(rt, word(rt, record + 8u) + 3u));
    const u32 object = word(rt, self + 0x1Cu);
    if (kind != 4u && object != 0u) {
        put(rt, frame.at(0u), 0xFFFFFFFFu);
        for (u32 offset = 4u; offset <= 0x30u; offset += 4u) put(rt, frame.at(offset), 0u);
        const u32 vtable = word(rt, object);
        ctx.gpr[2] = vtable;
        call_keep(rt, ctx, word(rt, vtable + 8u), {object, value, frame.at(0u)});
    }
    ctx.gpr[2] = call_set(rt, ctx, 0x089593DCu, {self, value});
}

// 0x08A99728 (a0 = holder, a1 = a record whose word at +0x14 is an array, a2 = first index, a3 = last): func_08A98C1C
// runs first; when a2 < a3 the elements [a2, a3) are moved one by one from the array the holder's word at +0x14
// points at into the record's array, each destination released through func_08A5BAD8 first (a0 = the source element,
// a1 = the destination's word). v0 = a0.
void func_08A99728(Runtime &rt, AllegrexContext &ctx) {
    const u32 holder = ctx.gpr[4];
    const u32 record = ctx.gpr[5];
    const u32 first = ctx.gpr[6];
    const u32 last = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    const u32 count = last - first;
    const bool any = signed_of(first) < signed_of(last);
    call_set(rt, ctx, 0x08A98C1Cu, {holder, count});
    ctx.gpr[2] = holder;
    if (!any) return;
    u32 index = 0u;
    u32 offset = first << 2;
    do {
        const u32 table = word(rt, holder);
        const u32 destination = word(rt, record + 0x14u);
        const u32 source = word(rt, table + 0x14u) + (index << 2);
        const u32 target = offset + destination;
        ++index;
        offset += 4u;
        if (source != target) call_set(rt, ctx, 0x08A5BAD8u, {source, word(rt, target)});
        if (count == index) break;
    } while (true);
    ctx.gpr[2] = holder;
}

// 0x08ABC388 (a0, a1): walk the global list at 0x08D7718C; for each node's object (word at +4) call its virtual at
// vtable +8, then func_08B35FD8(that answer, a0). On a zero answer the object's virtual at vtable +0x28 is called,
// and a non-zero result there ends the walk with it; v0 = that result, else 0.

// 0x08ACBC78 (a0 = holder, a1 = a value): lock func_08B34888(holder's word + 0xA8) and run func_08ACBC10 on the
// holder's word; when neither of its bytes at +0xA5 / +0xA4 is set, func_08ACB788 runs, the virtual at its vtable
// +0x1C is called and +0xA5 becomes 1 (answer 1; else 0). Unlock func_08B34B24 and v0 = the answer.

// 0x08B0E4B8 (a0 = object): lock func_08B34888(object + 4); the total starts at the int at +0x108 converted to
// float, and each node of the list at +0xFC (next at +8) adds the f0 of its object's virtual at vtable +0x14. When
// the count at +0x104 is non-zero the total is divided by its float. Unlock and f0 = the answer (1.0f when both the
// list and the count are empty).

// 0x08BBB790 (a0 = first, a1 = last, a2 = a holder): for every 4-byte slot from a0 to a1 (exclusive) call the word
// at the slot plus the holder's offset — through the virtual at that word when the holder's second word is odd —
// with a0 = the slot plus half the second word. v0 = the holder's offset, v1 = its second word.
void func_08BBB790(Runtime &rt, AllegrexContext &ctx) {
    const u32 begin = ctx.gpr[4];
    const u32 end = ctx.gpr[5];
    const u32 holder = ctx.gpr[6];
    GuestFrame frame(ctx, 0x40u);
    const u32 offset = word(rt, holder);
    const u32 span = word(rt, holder + 4u);
    if (begin != end) {
        const u32 indirect = span & 1u;
        const u32 half = static_cast<u32>(signed_of(span) >> 1);
        u32 current = begin;
        do {
            const u32 slot = current + half;
            const u32 target = indirect != 0u ? word(rt, word(rt, slot) + offset) : offset;
            ctx.gpr[2] = target;
            call_keep(rt, ctx, target, {slot});
            current += 4u;
        } while (current != end);
    }
    ctx.gpr[2] = offset;
    ctx.gpr[3] = span;
}

// 0x08C2B458 (a0 = a list head): walk the list from the word at +0 until it points back at the head. For each node
// func_08C2AA40(a0 = its word at +0xC, a1 = its word at +0x10, a2 = sp) trims the array, the array at +0xC is
// released (count from +0x14, element 4) when it exists, func_08A69D68 releases +8, and the node's 0x18 bytes are
// released (element 4). A list already pointing at its head leaves v0 alone.

// 0x08C328B4 (a0 = holder, a1 = object, a2 = the output holder): func_08A950D0 builds a 0x10-byte record at sp from
// the holder's word at +8; the object at a1 + 4 (func_08A5BB7C against 0x08CB30B0 when null) takes its virtual at
// vtable +0x110 with a0 = the object, a1 = the record's word at +0x14 and a2 = the holder's word at +8. The output
// is released when it is not sp, then the record: v0 = that destruction's answer.

// 0x08C74068 (a0 = a holder, a1 = an offset, a2 = a limit, a3 = another holder, t0 = an offset, t1 = a limit): a
// bounded lexicographic compare of the two strings those holders point at (length at -0xC). A length below its
// offset calls func_08B5AEDC(0x08CB5830) — the original does not come back from there. v0 = func_08B36170's answer,
// or the difference of the two remaining lengths when that answer is 0.
void func_08C74068(Runtime &rt, AllegrexContext &ctx) {
    const u32 lhs_offset = ctx.gpr[5];
    const u32 lhs_limit = ctx.gpr[6];
    const u32 rhs_holder = ctx.gpr[7];
    const u32 rhs_offset = ctx.gpr[8];
    const u32 rhs_limit = ctx.gpr[9];
    const u32 lhs_holder = word(rt, ctx.gpr[4]);
    GuestFrame frame(ctx, 0x20u);
    const u32 lhs_length = word(rt, lhs_holder - 0xCu);
    if (lhs_length < lhs_offset) {
        ctx.gpr[2] = call_set(rt, ctx, 0x08B5AEDCu, {0x08CB5830u});
        return;
    }
    const u32 rhs = word(rt, rhs_holder);
    const u32 rhs_length = word(rt, rhs - 0xCu);
    if (rhs_length < rhs_offset) {
        ctx.gpr[2] = call_set(rt, ctx, 0x08B5AEDCu, {0x08CB5830u});
        return;
    }
    const u32 lhs_room = lhs_length - lhs_offset;
    const u32 rhs_room = rhs_length - rhs_offset;
    const u32 lhs_compared = lhs_limit < lhs_room ? lhs_limit : lhs_room;
    const u32 rhs_compared = rhs_limit < rhs_room ? rhs_limit : rhs_room;
    const u32 shortest = rhs_compared < lhs_compared ? rhs_compared : lhs_compared;
    const u32 compared = call_set(rt, ctx, 0x08B36170u,
                                {lhs_holder + lhs_offset, rhs + rhs_offset, shortest, rhs, ctx.gpr[8], ctx.gpr[9], ctx.gpr[5], lhs_holder});
    ctx.gpr[2] = compared != 0u ? compared : (lhs_compared - rhs_compared);
}

// 0x0881EF0C (a0 = the output holder): func_0881BD98 builds a handle at sp (checked against 0x08C98F48),
// func_0881B068 resolves it, func_0881AA7C reads a value, func_08A950D0 builds a 0x10-byte record at sp+4 from that
// value, and func_0881B324 takes the resolved object with the record's word at +0x14. The output is released when it
// is not sp+4, then the record: v0 = that destruction's answer.

// 0x0888879C (a0 = a holder): release the object the holder's first word points at (its virtual at vtable +8), then
// the object at the holder's second word: its counter at +4 is decremented, and when it was 1 the object's virtual at
// vtable +8 runs, its counter at +8 is decremented, and when that was 1 the work continues in the tail call at
// vtable +0xC (v0 = the vtable word, as the original's `jr $t9` leaves it).
void func_0888879C(Runtime &rt, AllegrexContext &ctx) {
    const u32 holder = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const u32 first = word(rt, holder);
    const u32 second = word(rt, holder + 4u);  // the branch's delay slot
    if (first != 0u) {
        ctx.gpr[2] = word(rt, word(rt, first) + 8u);
        call_keep(rt, ctx, ctx.gpr[2], {first});
    }
    if (second == 0u) return;
    const u32 old = word(rt, second + 4u);
    put(rt, second + 4u, old - 1u);
    ctx.gpr[2] = 1u;
    if (old != 1u) return;
    ctx.gpr[2] = word(rt, word(rt, second) + 8u);
    call_keep(rt, ctx, ctx.gpr[2], {second});
    const u32 old_second = word(rt, second + 8u);
    put(rt, second + 8u, old_second - 1u);
    ctx.gpr[2] = 1u;
    if (old_second != 1u) return;
    const u32 vtable = word(rt, second);
    ctx.gpr[2] = vtable;
    tail_call(ctx, word(rt, vtable + 0xCu), {second});
}

// 0x088D0CCC (a0 = the output object, a1, a2 = a value): func_088DCD28(a2) names an object; its virtual at vtable
// +0x8C gives a count, and for every index below it the virtual at vtable +0x90 runs: a non-zero answer increments
// the word at +0x5C of the output, a zero answer lowers the answer to 0. The byte at +0x5B of the output becomes
// that answer (1 when every call answered non-zero, and 0 when the count was not positive).

// 0x088E649C (a0 = a holder, a1 = a byte): with the lock func_08B344E0 taken on the object's block at +0xEC, the
// virtual at its vtable +0x50 (a1 = -1) answers an object whose byte at +0xA goes to the value; unlock
// func_08B3457C. When the second object at +4 exists the same runs on its block, and the work continues in the tail
// call func_08B3457C on that block.

// 0x089C9528 (a0 = object): the destructor. Store the vtable 0x08CBB888, trim the array at +0x14 through
// func_08C012FC, point its words at +0x10 / +0xC at +0x18 and clear +8 / +0x14, run func_089C946C, release the
// arrays at +0x48 (func_08C01C34), +0x40, +0x3C and +0x38, release +0x30 (func_08C00980), trim +0x14 again,
// func_08A780B0, and continue in the tail call func_08A5BC54(a0, 0x80, 4).

// 0x08A13F1C (a0 = object): the constructor. Store the vtable 0x08CD0418, func_08ACF184(global 0x08CFFAAC, 0), the
// byte at +0x41 becomes 1, then func_08A1371C, func_08B28C74(+0x10), func_08B1B99C(+0x3C), func_08A12AE0,
// func_08ACEF04(+0x240), func_08B28BE0(+0x200), func_08A19DD4(+0x178), func_08A19DD4(+0x140), func_08B241A0(+0xA0),
// func_08C15D30(+0x5C), func_08B28BE0(+0x10), and continue in the tail call func_089C16C8(a0).

// 0x08A142C8 (a0 = object, a1 = a string cell): lock func_08B34888(object + 0xAC) and wait out the word at +0x108
// (func_08B34B2C) before it is compared-and-set to 1; func_08B7C830 copies 0x1F bytes of the cell into +0x18D;
// unlock (func_08B34B24), func_08B28C74 on +0xD8, and the work continues in the tail call func_08B28C44(object + 0xAC).

// 0x08A2D434 (a0 = object, a1 = a value): func_08B0979C(a0), func_08A2D338(sp, a1) builds a handle at sp (checked
// against 0x08CAC2A8), func_08A2D1B8 reads its object, func_08B0A034 combines it with the first answer,
// func_08A97A6C stores the result in a record at sp+4; the output is released when it is not sp+4, then the record and
// the handle are destroyed. v0 = the last destruction's answer.

// 0x08A9AF28 (a0 = a vector at +0x14, a1 = a wanted count): round the capacity up from 1 by doubling until it covers
// a1 + 1, then grow the vector: func_08B85798 moves the elements (capacity >= count) or func_08B85F30 grows it
// (capacity < count). The temporary from func_08A76AA0 is destroyed; v0 = that destruction's answer.

// 0x08AD83B8 (a0 = the buffer, a1 = a text, a2, a3, t0, t1 = the arguments after the format): the text's first '/'
// is skipped (func_08B7C9E4 with 0x2F) and func_08B37640 formats into 0x400 bytes at sp with the table 0x08C9084C and
// the va-list words at sp+0x820; func_08B373EC then func_08AD8464 finish it. v0 = the last answer.

// 0x08B59F6C (a0 = a table, a1 = the wanted word): a hash probe. The index at +0x202 (0xFF ends it) picks a
// 0x10-byte slot; its word at +8 is compared with the wanted one and the next index is the slot's byte at +3. A match
// runs func_08B59BC0(a0, +0x202, index) and func_08B59D6C(a0, index), and answers 1; no match (or a null wanted
// word) answers 0.

// 0x08B62198 (a0 = a block pointer): func_08B62100(a0) answers the list, and the block is a0 - 0x30. Its magic word
// at +0x30 is 0x432B2B00 with 0x474E5543 at +0x34 for a counted block: its count at +0x14 steps away from zero
// (count + 1, or 1 - count when negative), the list's word at +4 drops by one, and unless the block is already the
// list's head it is linked in (+0x10 = the old head, the head becomes the block). v0 = the block's word at +0x28.
// A block that is not the head and whose list is null links itself in and answers 0; anything else calls
// func_08B6459C.
void func_08B62198(Runtime &rt, AllegrexContext &ctx) {
    const u32 argument = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const u32 list = call_set(rt, ctx, 0x08B62100u, {argument});
    const u32 block = argument - 0x30u;
    const u32 head = word(rt, list);
    if (word(rt, block + 0x30u) == 0x432B2B00u && word(rt, block + 0x34u) == 0x474E5543u) {
        const s32 count = signed_of(word(rt, block + 0x14u));
        put(rt, block + 0x14u, count < 0 ? 1u - static_cast<u32>(count) : static_cast<u32>(count) + 1u);
        put(rt, list + 4u, word(rt, list + 4u) - 1u);
        if (head != block) {
            put(rt, block + 0x10u, head);
            put(rt, list, block);
        }
        ctx.gpr[2] = word(rt, block + 0x28u);
        return;
    }
    if (head == 0u) {
        put(rt, list, block);
        ctx.gpr[2] = 0u;
        return;
    }
    ctx.gpr[2] = call_set(rt, ctx, 0x08B6459Cu, {});
}

// 0x08BBB988 (a0 = a table holder, a1 = an index, a2 = the output record): the entry is a1*4 into the table's word
// at +4; func_08B03BB4 looks the entry's halfword at +0 up in func_08B03BB4's owner (a0 = the holder's word at
// +0x1C). The output gets: the entry's +2 sign-extended to 15 bits at +0, zero at +2, the object at +4, the object's
// +0x20 / +0x38 / +0x50 at +8 / +0x10 / +0x14 and — unless the object's word at +0xE2 has bit 0 — the object's
// +0xB0 at +0xC. v0 = 1 when the object's bit 2 of +0xE2 is clear or the entry's +2 has bit 15 set, else 0.

// 0x08BD5D54 (a0 = the map's holder, a1 = a key holder): walk the list at a0 + 0x98 while its nodes' strings (word at
// +8, length at -0xC) compare below the key's string (func_08B36170 over the shorter length); a node whose length
// equals the key's ends the walk, and a non-zero compare moves to the node's next word. v0 = the node reached (the
// sentinel a0 + 0x98 when the walk ends).

// 0x08BDA6D8 (a0 = a range, a1 = the range's end, a2 = a key holder, a3 = a byte): a partition step. The left cursor
// advances while func_08B35FD8(range's word, key) < 0, the right one steps back while func_08B35FD8(key, its word) < 0;
// while the left is below the right the two 8-byte elements are swapped and the walk goes on. v0 = the left cursor
// when it reaches the right one.
void func_08BDA6D8(Runtime &rt, AllegrexContext &ctx) {
    const u32 holder = ctx.gpr[4];
    const u32 end = ctx.gpr[5];
    const u32 key_holder = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    put8(rt, frame.at(0u), ctx.gpr[7]);
    const u32 key = word(rt, key_holder);
    u32 left = holder;
    u32 right = end;
    for (;;) {
        u32 pivot;
        for (;;) {
            pivot = word(rt, left);
            if (signed_of(call_set(rt, ctx, 0x08B35FD8u, {pivot, key})) >= 0) break;
            left += 8u;
        }
        u32 right_value;
        for (;;) {
            right -= 8u;
            right_value = word(rt, right);
            if (signed_of(call_set(rt, ctx, 0x08B35FD8u, {key, right_value})) >= 0) break;
        }
        if (left >= right) {
            ctx.gpr[2] = left;
            return;
        }
        const u32 right_word = word(rt, right + 4u);
        const u32 left_word = word(rt, left + 4u);
        put(rt, left, right_value);
        put(rt, left + 4u, right_word);
        left += 8u;
        put(rt, right + 4u, left_word);
        put(rt, right, pivot);
    }
}

// 0x08C246E4 (a0 = a string object, a1 = a haystack, a2 = a start index, a3 = a needle): walk back from
// min(a2, length - 1) at most that many steps plus one, calling func_08B79998(haystack, the string's signed byte,
// needle); the index of the first zero answer is the return, else -1. An empty string answers -1 at once.
void func_08C246E4(Runtime &rt, AllegrexContext &ctx) {
    const u32 holder = ctx.gpr[4];
    const u32 haystack = ctx.gpr[5];
    const u32 start = ctx.gpr[6];
    const u32 needle = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    const u32 length = word(rt, word(rt, holder) - 0xCu);
    if (length == 0u) {
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }
    const u32 highest = length - 1u;
    const u32 steps = start < highest ? start : highest;
    u32 index = steps;
    for (u32 step = 0u;; ++step) {
        const u32 text = word(rt, holder);
        const u32 found = call_set(rt, ctx, 0x08B79998u, {haystack, sign_byte(byte_at(rt, text + index)), needle});
        if (found == 0u) {
            ctx.gpr[2] = index;
            return;
        }
        if (step == steps) {
            ctx.gpr[2] = 0xFFFFFFFFu;
            return;
        }
        index -= 1u;
    }
}

// 0x0885FCA4 (a0 = a holder): the object at the holder's word + 0x5AC is created by func_088D6AD0; a loop of at most
// 0x1C2 rounds waits for it: func_088D6B38 answers 1 (return 1), func_08B1A844 answers non-zero (return 0), the poll
// func_088D4C9C on its block at +4 non-zero with its byte at +0x5BC non-zero returns 0, and func_08B2E2B8(0x186A0)
// ends each round. A loop that runs out answers 0.

// 0x0886E648 (a0 = object): the destructor. Store the vtable 0x08CBF770, run func_0886A8B8(+0, index 0..2, 0),
// func_0886AA0C for 2, 5 and 6, then release the member at +0x70 through the virtual at its vtable +0 (a1 = the word
// at +0x70) when +0x74 holds one; continue in the tail call func_0886A858(a0).

// 0x0888CAFC (a0 = object, a1 = unused, a2 = a selector, a3 = a direction): func_088DA788(object's word at +4)
// answers a record whose word at +0x618 is the table. Selector 1 reads the pair at +0x210 / +0x214 and the object's
// word at +0x70; any other selector's index at +0x30 picks a 0xC-byte entry (index*16 - index*4) into that table
// (words at +0x10 / +0x14) and the object's word at index*4 + 0x100. v0 = the direction ? that word - the entry's
// : the entry's - that word; v1 (and a3) = a1's word at +0x14 minus the pair's second word (the other order when
// the direction is set).

// 0x088B3D38 (a0 = a source, a1 = a destination sink): func_08BB15E4(sink, sp) with the source's count at +4 stored
// at sp first; every one of that many entries (word at +0 plus the index) goes through func_08BB1610(sink, entry);
// then func_088BA424(sink, word at +8, (word at +0xC + 0x1F) >> 5 << 2), func_08BB1610(sink, +0x12) and
// func_08BB163C(that answer, +0x13). v0 = the last answer.

// 0x088EC56C (a0, a1, a2 = the object): func_088EDD5C gets a 0x1C-byte stack descriptor built from the object's
// words at +0xC0 and the first answer's at +0x10 when the object's word at +0xB4 exists; the object's own virtual at
// vtable +0xA0 answers, and a non-zero answer runs func_088EE154(a0 = object, a1 = the caller's a0). v0 = 0 on the
// paths that do not call.

// 0x0893A4AC (a0 = object, a1, a2, a3): func_08939F4C(a0), func_0893A210(a0) gives a handle, func_0893A0D0(a0, it)
// stores a1 in it, func_0893A3A4(a0, it) stores a2, func_0893A104(a0, it) answers the slot where 1 is stored through
// func_0893A3DC(a0, it), and func_08B79A88 copies a3 into that slot's object with a2 = the object's word at +4; the
// work continues in the tail call func_08939F54(a0).

// 0x0896BABC (a0 = object): the destructor. Store the vtable 0x08CB92F8 and 0x08CB9530 at +0x38, func_08965B78 on
// the word at +0x80, func_08968F98 on the word at +0xD0, then the object at +0xCC - 0xC is released when it is not the
// table 0x08D74FD0 (func_08B60804 answers > 0, else func_08B8542C); release +0xA0, func_089B8884(+0x84),
// func_089BBAC8(a0), and func_08A5BC54(a0, 0xF0, 4). v0 = the last answer.

} // namespace
} // namespace new_batch_main_13
} // namespace gt::game

#define NEW13(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_13::name, gt::ReturnKind::kind)
NEW13(0x08B59A24u, func_08B59A24, Word);
NEW13(0x08B854DCu, func_08B854DC, Word);
NEW13(0x08B94288u, func_08B94288, Word);
NEW13(0x08C3253Cu, func_08C3253C, Word);
NEW13(0x08896BE0u, func_08896BE0, Word);
NEW13(0x08958D50u, func_08958D50, Word);
NEW13(0x08A99728u, func_08A99728, Word);
NEW13(0x08BBB790u, func_08BBB790, DoubleWord);
NEW13(0x08C74068u, func_08C74068, Word);
NEW13(0x08835B2Cu, func_08835B2C, Word);
NEW13(0x088434FCu, func_088434FC, Word);
NEW13(0x0888879Cu, func_0888879C, Word);
NEW13(0x08A324ECu, func_08A324EC, Word);
NEW13(0x08A93B7Cu, func_08A93B7C, Word);
NEW13(0x08B62198u, func_08B62198, Word);
NEW13(0x08BDA6D8u, func_08BDA6D8, Word);
NEW13(0x08C246E4u, func_08C246E4, Word);
