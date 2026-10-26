// Hot batch 103: the import unlock batch (see FINDINGS "Batch 103"). Functions of the reached, unregistered pool (probe `b103p`, five routes) written from `tools.disasm` one at a time; role words describe access shapes, none is a game name.
//
// Written 33, registered 31 (all compared in the five-route fast gate and the real-time full gate, 0 mismatches, sabotage 31 of 31 pinned in round one). Not registered, each with a commented HOT103 line: func_08B34B2C (101 of 101 cases sleep: not comparable) and func_088B0520 (writes memory, but a dense dealer run of 3,000 cases never found the list non-empty). The families: the four sceSasCore effect setters, the six name-registering constructors, the tree insert-with-hint (three key types), the heap allocate / free, the MAC address, the sub-interrupt hook, destructors with shared-object releases and the big-endian record initialiser func_088D5AFC.
//
// The batch also re-registers 13 functions parked in earlier batches as "not comparable" (func_08B35418, func_08AEB850, func_08B7771C in hot_batch_102.cpp; func_08AEB908, func_08AEB930, func_08B84270, func_08B83C84, func_08AD96A0, func_08B33A24 in hot_batch_76.cpp; func_08B59A24 in hot_batch_82.cpp; func_08A235F8 in hot_batch_91.cpp; func_08AEB4CC, func_08AF0B78 in hot_batch_97.cpp): the imports that blocked them became recordable (host/import_lists.hpp, FINDINGS \"Batch 103\"). Their code is unchanged.
//
// The pool this time: tools.pool over a fresh probe (b103p); the static import walk marked 140 of 277 candidates for sceKernelDelayThread alone, so the blockers were measured dynamically (one fast gate over 74 parked registrations) instead.
//
// Every call_guest / tail_call passes the argument registers the original sets before the jal (a0-a3, t0-t3, f12-f15): `call_set` writes them to ctx like the original and the ones it leaves alone keep the caller's value. The callee-saved registers are never written here (the original restores them before it returns or tail-calls). The delay slot of a `jal` / `jalr` runs BEFORE the call, a `bnel` / `beql` / `bgtzl` / `blezl` / `bc1fl` / `bc1tl` delay slot only when the branch is taken, and the delay slot of a non-likely branch on both paths. v0 (and v1 / f0 where a caller could read them) is set as the original leaves it on every path; the dead stack is not reproduced, the frames that callees read or write are (`GuestFrame`, offsets as in the original).
// Branches the compared cases do not take are listed in each function's comment; they were read against the asm by hand.
//
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>

namespace gt::game {
namespace hot_batch_103 {
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
inline std::uint32_t half_u(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load16(address); }
inline void put16(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
inline std::uint32_t sext16(std::uint32_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(value))); }
// The value `lui` leaves for an address built by a relocated lui/addiu (or lui/load/store) pair: the high half, rounded up when the low half is negative.
inline constexpr std::uint32_t lui_of(std::uint32_t address) { return (address + 0x8000u) & 0xFFFF0000u; }
inline std::int32_t signed_of(std::uint32_t value) { return static_cast<std::int32_t>(value); }
inline std::uint32_t byte_u(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load8(address); }
inline void put8(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
inline std::uint32_t sext8(std::uint32_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(value))); }
// A guest call with the argument registers set like the original sets them before the jal (a0-a3, t0-t3 in order): what the callee does not take stays as the caller had it.
inline std::uint32_t call_set(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments) {
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments);
}
// A guest call with integer and float argument registers set like the original sets them before the jal (a0-a3, t0-t3 and f12-f15): v0 / v1 / f0 of the callee stay in the context.
inline GuestResult call_setf(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments,
                             std::initializer_list<float> float_arguments) {
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    unsigned freg = 12u;
    for (const float argument : float_arguments) {
        if (freg <= 15u) ctx.fpr[freg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments, float_arguments);
}
// A virtual call the way the compiler issues it: v1 = the vtable (object[0]), v0 = the slot's function, then jalr v0. A callee that returns without setting v0 hands the function's address on to the caller's code, so both registers are set like the original sets them.
inline GuestResult call_virtual_full(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object, std::uint32_t offset,
                                     std::initializer_list<std::uint32_t> arguments) {
    const std::uint32_t table = word(rt, object);
    const std::uint32_t function = word(rt, table + offset);
    ctx.gpr[3] = table;
    ctx.gpr[2] = function;
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, function, arguments, {});
}
inline std::uint32_t call_virtual(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object, std::uint32_t offset,
                                  std::initializer_list<std::uint32_t> arguments) {
    return call_virtual_full(rt, ctx, object, offset, arguments).v0;
}


// The release of a string cell whose data pointer is `data` (a COW string of the standard library): nothing happens when data - 0xC is the shared empty cell `empty_cell`, otherwise func_08B60804(data - 4, -1) and, when its result is not positive, `dispose`(data - 0xC, flag) (func_08B8542C for the narrow strings, func_08B93554 for the wide ones with the empty cell D_08D75298). Leaves a0 / a1 like the original.
inline void release_cell(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t data, std::uint32_t flag, std::uint32_t empty_cell = 0x08D74FD0u, std::uint32_t dispose = 0x08B8542Cu) {
    ctx.gpr[5] = 0xFFFFFFFFu;
    ctx.gpr[4] = data - 4u;  // the delay slot of the beq
    if (data - 0xCu == empty_cell) return;
    const std::uint32_t references = call_set(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu});
    ctx.gpr[4] = data - 0xCu;
    ctx.gpr[5] = flag;  // the delay slot of the bgtz
    if (signed_of(references) <= 0) (void)call_set(rt, ctx, dispose, {data - 0xCu, flag});
}

// The four setters of the sound library's effect unit (reverb type, parameters, volume, switches) share one guard around their sceSasCore import. The init word D_08D740F8 is 0 before the unit exists (answer 0x80420100); when it is 1 sceSasGetOutputmode() is asked first and a raw output (1) answers 0x80420024; otherwise the import runs with the core address 0x08D74100 and the arguments: `arguments` is 2 for the ones that pass (a0, a1) on, 1 for the type setter. v0 = v1 = the answer on every path (the original ends with `addu v0, v1, zero`).
// Branches the routes do not take (checked against the asm by hand, branch targets and delay slots read): the init word D_08D740F8 equal to 0 (the answer 0x80420100 at once) or above 1 (no sceSasGetOutputmode call), and a raw output mode (the answer 0x80420024); the compared cases all have the word at 1 and a stereo output, so the import runs.
inline void sas_effect_setter_n(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t import_stub, unsigned arguments) {
    const std::uint32_t first = ctx.gpr[4], second = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t state = word(rt, 0x08D740F8u);
    std::uint32_t answer = 0x80420100u;
    if (state != 0u) {
        std::uint32_t mode = 0x80420100u;
        if (state == 1u) mode = call_set(rt, ctx, 0x08C7B0D8u, {0x08D74100u, 1u});
        if (mode == 1u) {
            answer = 0x80420024u;
        } else if (arguments == 2u) {
            answer = call_set(rt, ctx, import_stub, {0x08D74100u, first, second});
        } else {
            answer = call_set(rt, ctx, import_stub, {0x08D74100u, first});
        }
    }
    ctx.gpr[2] = answer;
    ctx.gpr[3] = answer;
}

inline void sas_effect_setter(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t import_stub) { sas_effect_setter_n(rt, ctx, import_stub, 2u); }
inline void sas_effect_setter_one(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t import_stub) { sas_effect_setter_n(rt, ctx, import_stub, 1u); }

// func_08B844D0(a, b): sceSasSetEffectVolume(core, a, b) behind the guard (sas_effect_setter).
void func_08B844D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { sas_effect_setter(rt, ctx, 0x08C7B0D0u); }
// func_08B8443C(a, b): sceSasSetEffect(core, a, b) behind the guard.
void func_08B8443C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { sas_effect_setter(rt, ctx, 0x08C7B0F0u); }
// func_08B845E8(a, b): sceSasSetEffectParam(core, a, b) behind the guard.
void func_08B845E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { sas_effect_setter(rt, ctx, 0x08C7B030u); }
// func_08B84564(a): sceSasSetEffectType(core, a) behind the guard (the one-argument member: its state word sits in a1 and the import gets two arguments).
void func_08B84564(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { sas_effect_setter_one(rt, ctx, 0x08C7B040u); }

// func_08B2E28C(slot = a0): func_08B2E1AC(slot, 0), then the tail jump to the import stub 0x08C7AAA0 (sceKernelDeleteThread) with a0 = *slot (the thread id; the load sits in the delay slot of the jal). v0 / v1 are the import's.
void func_08B2E28C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t thread;
    {
        GuestFrame frame(ctx, 0x10u);
        thread = word(rt, ctx.gpr[4]);
        (void)call_set(rt, ctx, 0x08B2E1ACu, {ctx.gpr[4], 0u});
    }
    tail_call(ctx, 0x08C7AAA0u, {thread});
}

// func_08869220(object = a0, b = a1): the tail jump func_0886A464(object + 0x1B0, b, 1, 0).
void func_08869220(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    tail_call(ctx, 0x0886A464u, {ctx.gpr[4] + 0x1B0u, ctx.gpr[5], 1u, 0u});
}

// func_0894CE2C(this = a0): this[2] = 0; with an array at this[1] (the element count at array[-1], elements of 0x2100 bytes whose first word is a vtable) the virtual destructor (slot 0) of each element runs from the last one to the first, until the array pointer this[1] equals the element just destroyed (it is re-read after every call); then func_08B61FC0(the last element address - 8) frees the block and this[1] = 0. v0 = the free call's. An empty array (this[1] == 0) only stores this[2] = 0 and returns with v0 untouched.
// Branches the routes do not take (checked against the asm by hand): an empty array (this[1] == 0), count == 0 (the loop skipped, the free call gets array - 8) and arrays of more than one element (the destructor loop ran once in every compared case: the array pointer equals the element just destroyed at once).
void func_0894CE2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t array = word(rt, self + 4u);
    ctx.gpr[4] = array;
    put(rt, self + 8u, 0u);
    if (array == 0u) return;
    const std::uint32_t count = word(rt, array - 4u);
    std::uint32_t last = array + (count << 13) + (count << 8);
    if (last != array) {
        std::uint32_t element = last - 0x2100u;
        for (;;) {
            const std::uint32_t vtable = word(rt, element);
            const std::uint32_t function = word(rt, vtable);
            ctx.gpr[2] = vtable;
            ctx.gpr[3] = function;
            last = element;
            (void)call_set(rt, ctx, function, {element});
            ctx.gpr[2] = word(rt, self + 4u);
            if (ctx.gpr[2] == last) break;
            element = last - 0x2100u;
        }
    }
    (void)call_set(rt, ctx, 0x08B61FC0u, {last - 8u});
    put(rt, self + 4u, 0u);
}

// func_08A17A28(key = a0): under the lock D_08CF6250 (func_08B344E0 / the tail jump func_08B3457C with a0 = the lock): when the registered key D_08CF6248 equals `key`, the object D_08CF6240 (when not null) gets its virtual slot 0xC with (object, 1); otherwise the list func_08A17698() answers is walked (an empty list, node[0] == the list itself, ends it) for the first node whose word +0xC equals `key`, and func_08C18DDC(func_08A17698(), node) is called for it.
// Branches the routes do not take (checked against the asm by hand): the key-equal arm (D_08CF6248 == key: the virtual slot 0xC call and the null-object skip) and the whole list walk (the list is empty in every compared case, func_08A17698 answers a list whose first node is itself), so the node loop, its end-of-list exit and the found arm (func_08C18DDC) never ran.
void func_08A17A28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t key = ctx.gpr[4];
    constexpr std::uint32_t lock = 0x08CF6250u;
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_set(rt, ctx, 0x08B344E0u, {lock});
        if (word(rt, 0x08CF6248u) == key) {
            const std::uint32_t object = word(rt, 0x08CF6240u);
            ctx.gpr[4] = lock;  // the delay slot of the beql
            if (object != 0u) (void)call_virtual_full(rt, ctx, object, 0xCu, {object, 1u});
        } else {
            std::uint32_t list = call_set(rt, ctx, 0x08A17698u, {});
            ctx.gpr[3] = word(rt, list);
            if (ctx.gpr[3] != list) {
                (void)call_set(rt, ctx, 0x08A17698u, {});
                const std::uint32_t second = ctx.gpr[2];
                std::uint32_t node = word(rt, second);
                const std::uint32_t head = call_set(rt, ctx, 0x08A17698u, {});
                ctx.gpr[3] = head;
                if (node != head) {
                    for (;;) {
                        if (word(rt, node + 0xCu) == key) {
                            const std::uint32_t holder = call_set(rt, ctx, 0x08A17698u, {});
                            (void)call_set(rt, ctx, 0x08C18DDCu, {holder, node});
                            break;
                        }
                        node = word(rt, node);
                        if (node == head) break;
                    }
                }
            }
        }
        ctx.gpr[4] = lock;
    }
    tail_call(ctx, 0x08B3457Cu, {lock});
}

// func_08BF738C(tree = a0, node = a1): frees a binary tree post-order, right subtree first (func_08BF738C(tree, node[3])), then for each node on the left spine: the holder node[5] (when not null) gets func_08A5B930, the string node[4] is released (release_cell), and the node block (0x18 bytes, align 4) is freed with func_08A5BC54; the loop continues with node[2]. v0 = the last free's answer; a null node returns with v0 untouched.
// Branches the routes do not take (checked against the asm by hand): a null holder at node[5] (the func_08A5B930 call skipped) and the string cell arms release_cell lists (the shared empty cell, and a reference count that drops to zero: func_08B8542C never ran).
void func_08BF738C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t tree = ctx.gpr[4];
    std::uint32_t node = ctx.gpr[5];
    if (node == 0u) return;
    for (;;) {
        (void)call_set(rt, ctx, 0x08BF738Cu, {tree, word(rt, node + 0xCu)});
        const std::uint32_t holder = word(rt, node + 0x14u);
        const std::uint32_t next = word(rt, node + 8u);
        ctx.gpr[4] = holder;
        if (holder != 0u) (void)call_set(rt, ctx, 0x08A5B930u, {holder});
        release_cell(rt, ctx, word(rt, node + 0x10u), frame.at(0u));
        (void)call_set(rt, ctx, 0x08A5BC54u, {node, 0x18u, 4u});
        node = next;
        if (next == 0u) break;
    }
}

// func_088B0520(this = a0): under the lock this + 0x20 (func_08B34888 / func_08B34B24): while func_088AFB3C(this) answers a node other than the end marker this + 0xC: the entry func_08B5F444(node, marker); its shared object entry[6] (when not null) loses one reference (the `ll/sc` decrement of the word +8, single-threaded here) and, when that was the last, gets the virtual slot 0xC; the string entry[4] is released (the empty cell is D_08D75298 and the free func_08B93554 here); func_08B35254(entry) frees the entry and the count this[7] goes down by one. v0 = the unlock's.
// Branches the routes do not take (checked against the asm by hand): the whole body of the loop (func_088AFB3C answers the end marker at once in every compared case: only the lock and unlock calls ran) -- which is why it is not registered.
void func_088B0520(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x40u);
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t lock = self + 0x20u, marker = self + 0xCu;
    (void)call_set(rt, ctx, 0x08B34888u, {lock});
    for (;;) {
        const std::uint32_t node = call_set(rt, ctx, 0x088AFB3Cu, {self});
        ctx.gpr[4] = node;
        if (node == marker) break;
        const std::uint32_t entry = call_set(rt, ctx, 0x08B5F444u, {node, marker});
        const std::uint32_t shared = word(rt, entry + 0x18u);
        ctx.gpr[4] = shared;
        if (shared != 0u) {
            const std::uint32_t counter = shared + 8u;
            const std::uint32_t old = word(rt, counter);
            ctx.gpr[3] = counter;
            ctx.gpr[5] = old;
            ctx.gpr[2] = 1u;
            put(rt, counter, old - 1u);
            if (old == 1u) (void)call_virtual_full(rt, ctx, shared, 0xCu, {shared});
        }
        release_cell(rt, ctx, word(rt, entry + 0x10u), frame.at(0u), 0x08D75298u, 0x08B93554u);
        (void)call_set(rt, ctx, 0x08B35254u, {entry});
        ctx.gpr[2] = word(rt, self + 0x1Cu) - 1u;
        put(rt, self + 0x1Cu, ctx.gpr[2]);
    }
    (void)call_set(rt, ctx, 0x08B34B24u, {lock});
}


// The reference-count steps of the shared-object holder: `ll/sc` on a word of guest memory (single-threaded here, the host never preempts inside one). The decrement leaves a0 = the old value, v1 = the word's address and v0 = 1 (the successful `sc`); true when the old value was 1 (the count reached zero).
inline bool count_down(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address) {
    const std::uint32_t old = word(rt, address);
    ctx.gpr[3] = address;
    ctx.gpr[4] = old;
    ctx.gpr[2] = 1u;
    put(rt, address, old - 1u);
    return old == 1u;
}

// One shared object of func_08B8FE44's elements: its use count (+4) goes down; when that was the last use the virtual slot 8 runs, then the weak count (+8) goes down and when that was the last one slot 0xC runs (a0 = the object for both).
inline void release_shared(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object) {
    if (count_down(rt, ctx, object + 4u)) {
        (void)call_virtual_full(rt, ctx, object, 8u, {object});
        if (count_down(rt, ctx, object + 8u)) (void)call_virtual_full(rt, ctx, object, 0xCu, {object});
    }
}

// func_08B8FE44(first = a0, last = a1): the range of 16-byte elements [first, last) is destroyed in order: each element holds two shared-object pointers, at +0xC and then at +4, released one after the other (release_shared; a null pointer is skipped). Returns with v0 as the last operation left it; an empty range returns at once.
// Branches the routes do not take (checked against the asm by hand): a null second member (+0xC), and every release_shared whose use count reaches zero (the virtual slots 8 and 0xC never ran: the counts are above 1 in every compared case); the compared cases only have elements with a non-null first member.
void func_08B8FE44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    std::uint32_t cursor = ctx.gpr[4];
    const std::uint32_t last = ctx.gpr[5];
    if (cursor == last) return;
    do {
        const std::uint32_t second = word(rt, cursor + 0xCu);
        if (second != 0u) release_shared(rt, ctx, second);
        const std::uint32_t first = word(rt, cursor + 4u);
        if (first != 0u) release_shared(rt, ctx, first);
        cursor += 0x10u;
    } while (cursor != last);
}

// func_08B34B2C(lock = a0): the recursive-lock entry: thread = func_08B2E6DC(lock), then with the interrupts masked (sceKernelCpuSuspendIntr, 0x08C7A888, and the resume 0x08C7A8A8 with the saved mask) the queue head lock[8] is looked at: with lock[0x20] == 0 or an owner lock[6] other than the thread, the answer is "not ours" (s1 = 1) and nothing changes; when the caller owns it, a wait node on the stack {0, thread, lock[7], lock[0x20], lock[9]} is pushed (lock[0xC] or lock[8] points at it, lock[0x20] = 1, lock[0xC] = sp, lock[0x24] = lock[7] = -1) and, after the resume, func_08B349C4(lock, 0) (a non-zero answer puts the thread to sleep, 0x08C7AA98), func_08B34748(lock, 0), and lock[0x20], lock[7], lock[9] get their saved values back.
// Branches the compared cases do not take (checked against the asm by hand): the whole owner arm (the wait node, the sleep import and the restore of the three words) when no case has the caller as owner of a held lock.
void func_08B34B2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t lock = ctx.gpr[4];
    const std::uint32_t thread = call_set(rt, ctx, 0x08B2E6DCu, {lock});
    const std::uint32_t mask = call_set(rt, ctx, 0x08C7A888u, {});
    ctx.gpr[4] = mask;
    const std::uint32_t pending = word(rt, lock + 0x20u);
    ctx.gpr[5] = pending;
    std::uint32_t not_ours = 1u;
    if (pending != 0u && thread == word(rt, lock + 0x18u)) {
        const std::uint32_t saved_a = word(rt, lock + 0x1Cu);
        const std::uint32_t saved_b = word(rt, lock + 0x24u);
        const std::uint32_t tail = word(rt, lock + 0xCu);
        put(rt, frame.at(4u), thread);
        put(rt, frame.at(8u), saved_a);
        put(rt, frame.at(0xCu), pending);
        put(rt, frame.at(0x10u), saved_b);
        put(rt, frame.at(0u), 0u);
        if (tail != 0u) {
            put(rt, tail, frame.at(0u));
        } else {
            put(rt, lock + 8u, frame.at(0u));
        }
        ctx.gpr[3] = 0xFFFFFFFFu;
        ctx.gpr[2] = 1u;
        not_ours = 0u;
        put(rt, lock + 0x20u, 1u);
        put(rt, lock + 0xCu, frame.at(0u));
        put(rt, lock + 0x24u, 0xFFFFFFFFu);
        put(rt, lock + 0x1Cu, 0xFFFFFFFFu);
    }
    (void)call_set(rt, ctx, 0x08C7A8A8u, {mask});
    if (not_ours != 0u) return;
    if (call_set(rt, ctx, 0x08B349C4u, {lock, 0u}) != 0u) (void)call_set(rt, ctx, 0x08C7AA98u, {lock});
    (void)call_set(rt, ctx, 0x08B34748u, {lock, 0u});
    ctx.gpr[4] = word(rt, frame.at(0xCu));
    ctx.gpr[3] = word(rt, frame.at(0x10u));
    ctx.gpr[2] = word(rt, frame.at(8u));
    put(rt, lock + 0x20u, ctx.gpr[4]);
    put(rt, lock + 0x1Cu, ctx.gpr[2]);
    put(rt, lock + 0x24u, ctx.gpr[3]);
}

// func_08B33B30(fd = a0, -, offset low = a2, offset high = a3, whence = t0): a seek that retries when it was interrupted. Each round: func_08B33AF4(), func_08B33ACC(fd), the import sceIoLseek (0x08C7A880, a 64-bit answer in v0:v1 = s0:s1), func_08B33A7C(fd) (non-zero = retry later: s3); with no retry pending and func_08B2DE00() non-zero the answer becomes 0x80020321:0. A negative high word with a retry pending starts another round; otherwise the answer is returned (v0:v1 = s0:s1).
// Branches the routes do not take (checked against the asm by hand): a retry pending with func_08B33A7C == 0 and func_08B2DE00() non-zero (the answer 0x80020321:0), and a negative high word with a retry pending (another round): every compared case answers at once with a retry flag set and a non-negative high word.
void func_08B33B30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t fd = ctx.gpr[4], offset_low = ctx.gpr[6], offset_high = ctx.gpr[7], whence = ctx.gpr[8];
    std::uint32_t low = 0u, high = 0u;
    for (;;) {
        (void)call_set(rt, ctx, 0x08B33AF4u, {});
        (void)call_set(rt, ctx, 0x08B33ACCu, {fd});
        const GuestResult seek = call_setf(rt, ctx, 0x08C7A880u, {fd, ctx.gpr[5], offset_low, offset_high, whence}, {});
        low = seek.v0;
        high = seek.v1;
        const bool retry = call_set(rt, ctx, 0x08B33A7Cu, {fd}) != 0u;
        if (!retry && call_set(rt, ctx, 0x08B2DE00u, {}) != 0u) {
            low = 0x80020321u;
            high = 0u;
            break;
        }
        if (signed_of(high) < 0 && retry) {
            ctx.gpr[2] = low;
            continue;
        }
        break;
    }
    ctx.gpr[2] = low;
    ctx.gpr[3] = high;
}

// func_08B31C14(-, out = a1, length = a2): the console's MAC address: with length >= 6 sceWlanGetEtherAddr (0x08C7B1F0) fills a 6-byte buffer on the stack and, when it answers 0, the six bytes are copied to out; v0 = 1 then, else 0.
// Branches the routes do not take (checked against the asm by hand): length < 6 (v0 = 0 with no import call) and sceWlanGetEtherAddr answering non-zero (v0 = 0, nothing copied).
void func_08B31C14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t out = ctx.gpr[5];
    std::uint32_t answer = 0u;
    ctx.gpr[4] = frame.at(0u);
    if (ctx.gpr[6] >= 6u) {
        const std::uint32_t status = call_set(rt, ctx, 0x08C7B1F0u, {frame.at(0u)});
        ctx.gpr[5] = 1u;
        if (status == 0u) {
            for (std::uint32_t i = 0u; i < 6u; ++i) put8(rt, out + i, byte_u(rt, frame.at(i)));
            ctx.gpr[3] = byte_u(rt, frame.at(5u));
            answer = 1u;
        }
    }
    ctx.gpr[2] = answer;
}

// func_08B84B5C(sub = a0, handler = a1, argument = a2): the sub-interrupt 0x1E hook: with a handler, sceKernelRegisterSubIntrHandler(0x1E, sub, handler, argument) (0x08C7A7F8) and, when that did not fail (v0 >= 0), sceKernelEnableSubIntr(0x1E, sub) (0x08C7A808); without one sceKernelReleaseSubIntrHandler(0x1E, sub) (0x08C7A800). v0 is the last import's.
// Branches the routes do not take (checked against the asm by hand): the release arm (no handler: sceKernelReleaseSubIntrHandler) and a failing register call (v0 < 0: no sceKernelEnableSubIntr).
void func_08B84B5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t sub = ctx.gpr[4], handler = ctx.gpr[5], argument = ctx.gpr[6];
    if (handler == 0u) {
        (void)call_set(rt, ctx, 0x08C7A800u, {0x1Eu, sub});
        return;
    }
    const std::uint32_t result = call_set(rt, ctx, 0x08C7A7F8u, {0x1Eu, sub, handler, argument});
    if (signed_of(result) >= 0) (void)call_set(rt, ctx, 0x08C7A808u, {0x1Eu, sub});
}


// The tail of the "register every name of a list" constructors (a base constructor, then each node of a singly linked list that `first_function(0)` starts, next pointer at +0xC): the node's name word is copied into a string (func_08B85EB8), copied again into the first member of a (name, node) pair at sp + 0x10 (func_08B88F44) whose second member is the node, the pair is inserted into this object's map (func_08BE9D90), and both strings are released (the first with the flag sp + 1, the second with sp). `base` runs first with this. v0 is whatever the last operation left (the answer of the first function, 0, when the list is empty).
// Branches the routes do not take (checked against the asm by hand): an empty list (the answer of the first function is 0: only the base constructor ran).
inline void register_names(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t base, std::uint32_t first_function) {
    GuestFrame frame(ctx, 0x50u);
    const std::uint32_t self = ctx.gpr[4];
    (void)call_set(rt, ctx, base, {self});
    std::uint32_t node = call_set(rt, ctx, first_function, {0u});
    if (node == 0u) return;
    for (;;) {
        (void)call_set(rt, ctx, 0x08B85EB8u, {frame.at(4u), word(rt, node), frame.at(2u)});
        (void)call_set(rt, ctx, 0x08B88F44u, {frame.at(0x10u), frame.at(4u)});
        put(rt, frame.at(0x14u), node);
        (void)call_set(rt, ctx, 0x08BE9D90u, {self, frame.at(0x10u)});
        release_cell(rt, ctx, word(rt, frame.at(0x10u)), frame.at(1u));
        ctx.gpr[2] = word(rt, frame.at(4u));
        release_cell(rt, ctx, word(rt, frame.at(4u)), frame.at(0u));
        node = word(rt, node + 0xCu);
        if (node == 0u) break;
    }
}
void func_0896BF44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_names(rt, ctx, 0x089BCCDCu, 0x08BE8DE8u); }
void func_089958A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_names(rt, ctx, 0x08A02C2Cu, 0x08BF0A48u); }
void func_0899B04C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_names(rt, ctx, 0x08A02C2Cu, 0x08BF346Cu); }
void func_089D8F3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_names(rt, ctx, 0x08A10020u, 0x08C0457Cu); }
void func_089DBFC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_names(rt, ctx, 0x0896BF44u, 0x08C06438u); }
void func_089E2DB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_names(rt, ctx, 0x08A093ECu, 0x08C07FE8u); }
// Batch 140 (2026-10-25): the rest of this family, which no route reaches — verified by synthetic cases.
void func_08974F64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_names(rt, ctx, 0x089D8F3Cu, 0x08BEC8A8u); }
void func_08975088(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_names(rt, ctx, 0x089D7BE0u, 0x08BEC8E8u); }
void func_0897FB14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_names(rt, ctx, 0x0896BF44u, 0x08BEEA3Cu); }
void func_08984358(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_names(rt, ctx, 0x0897D578u, 0x08BEF030u); }
void func_0898A958(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_names(rt, ctx, 0x0896BF44u, 0x08BEFCDCu); }
void func_089908D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_names(rt, ctx, 0x089958A0u, 0x08BF087Cu); }
void func_089940BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_names(rt, ctx, 0x08A0DF1Cu, 0x08BF0960u); }
void func_08997794(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_names(rt, ctx, 0x08A02C2Cu, 0x08BF0BB0u); }
void func_0899A660(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_names(rt, ctx, 0x08A0DF1Cu, 0x08BF3334u); }
void func_0899E074(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_names(rt, ctx, 0x08A0DF1Cu, 0x08BF409Cu); }
void func_089A131C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_names(rt, ctx, 0x089D8F3Cu, 0x08BF4688u); }
void func_089A1440(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_names(rt, ctx, 0x089D7BE0u, 0x08BF46C8u); }
void func_089A4AE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_names(rt, ctx, 0x08A0DF1Cu, 0x08BF5A9Cu); }
void func_089B4258(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_names(rt, ctx, 0x08A02C2Cu, 0x08BFD1E4u); }
void func_089B8580(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_names(rt, ctx, 0x08A0DF1Cu, 0x08BFDEDCu); }
void func_089B9B48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_names(rt, ctx, 0x089BCCDCu, 0x08BFE5A4u); }
void func_089C0F84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_names(rt, ctx, 0x089E2DB0u, 0x08BFFAD0u); }
void func_089C2FAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_names(rt, ctx, 0x08A10020u, 0x08BFFDC0u); }
void func_089CAF8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_names(rt, ctx, 0x08A0DF1Cu, 0x08C01D9Cu); }
void func_089D0A5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_names(rt, ctx, 0x089958A0u, 0x08C02400u); }
void func_089E6624(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_names(rt, ctx, 0x0899B04Cu, 0x08C09670u); }
void func_089EFD70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_names(rt, ctx, 0x089E2DB0u, 0x08C0AAA0u); }
void func_089F450C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_names(rt, ctx, 0x089E2DB0u, 0x08C0B428u); }
void func_089F6F74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_names(rt, ctx, 0x08A10020u, 0x08C0B560u); }
void func_089F877C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_names(rt, ctx, 0x089D8F3Cu, 0x08C0C30Cu); }
void func_089F88A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_names(rt, ctx, 0x089D7BE0u, 0x08C0C34Cu); }
void func_089FD2C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_names(rt, ctx, 0x089BCCDCu, 0x08C0ECACu); }
void func_089FEAE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_names(rt, ctx, 0x08A0DF1Cu, 0x08C0EE58u); }
void func_089FFE38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_names(rt, ctx, 0x08A10020u, 0x08C0EF40u); }
void func_08A0598C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_names(rt, ctx, 0x089D8F3Cu, 0x08C10974u); }
void func_08A05AB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_names(rt, ctx, 0x089D7BE0u, 0x08C109B4u); }
void func_08A19F68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_names(rt, ctx, 0x0897D578u, 0x08C18E94u); }

// The "insert with a hint" of a sorted unique-key tree (the standard library's _Rb_tree::insert_unique(hint, value)): tree = a0 (header at +4, leftmost +0xC, rightmost +0x10, size +0x14), hint = a1 (a node, key at +0x10), value = a2 (key = its first word). The three members differ in the key comparison (`signed_keys`) and in the two callees `insert_unique` (value) -> {iterator in v0} and `insert_at` (x, y, value) -> iterator:
//  - hint == the header: an empty tree, or a key that is not above the rightmost one, goes through insert_unique; otherwise insert_at(0, rightmost);
//  - key < hint key: hint == leftmost -> insert_at(hint, hint); else with the predecessor p (func_08B5F174): p key < key -> (p's right child (+0xC) set ? insert_at(hint, hint) : insert_at(0, p)), otherwise insert_unique;
//  - hint key < key: hint == rightmost -> insert_at(0, hint); else with the successor s (func_08B5F100): key < s key -> (hint's right child set ? insert_at(s, s) : insert_at(0, hint)), otherwise insert_unique;
//  - equal keys: v0 = hint.
// v0 is the result on every path; v1 = the same iterator after insert_unique (the original copies the answer pair), the hint key on the equal-key return.
// Branches the routes do not take (checked against the asm by hand): every compared case has the hint at the header (the first arm) or a key below the hint's with a predecessor whose key is below the new key; the arms where the hint key is below the key (rightmost / successor), the equal-key return and a predecessor key not below the key (insert_unique after the predecessor) never ran.
inline void tree_insert_hint_n(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t insert_unique, std::uint32_t insert_at, bool signed_keys) {
    GuestFrame frame(ctx, 0x40u);
    const std::uint32_t tree = ctx.gpr[4], hint = ctx.gpr[5], value = ctx.gpr[6];
    const auto less = [signed_keys](std::uint32_t a, std::uint32_t b) { return signed_keys ? signed_of(a) < signed_of(b) : a < b; };
    const auto unique = [&]() {
        const GuestResult answer = call_setf(rt, ctx, insert_unique, {tree, value}, {});
        ctx.gpr[3] = answer.v0;
        return answer.v0;
    };
    const auto at = [&](std::uint32_t x, std::uint32_t y) { return call_set(rt, ctx, insert_at, {tree, x, y, value}); };
    std::uint32_t result;
    if (tree + 4u == hint) {
        result = 0u;
        bool above_rightmost = false;
        std::uint32_t rightmost = 0u;
        if (word(rt, tree + 0x14u) != 0u) {
            rightmost = word(rt, tree + 0x10u);
            above_rightmost = less(word(rt, rightmost + 0x10u), word(rt, value));
        }
        result = above_rightmost ? at(0u, rightmost) : unique();
    } else {
        const std::uint32_t key = word(rt, value);
        const std::uint32_t hint_key = word(rt, hint + 0x10u);
        if (less(key, hint_key)) {
            if (hint == word(rt, tree + 0xCu)) {
                result = at(hint, hint);
            } else {
                const std::uint32_t predecessor = call_set(rt, ctx, 0x08B5F174u, {hint});
                if (less(word(rt, predecessor + 0x10u), word(rt, value))) {
                    result = word(rt, predecessor + 0xCu) != 0u ? at(hint, hint) : at(0u, predecessor);
                } else {
                    result = unique();
                }
            }
        } else if (less(hint_key, key)) {
            if (word(rt, tree + 0x10u) == hint) {
                result = at(0u, hint);
            } else {
                const std::uint32_t successor = call_set(rt, ctx, 0x08B5F100u, {hint});
                if (less(word(rt, value), word(rt, successor + 0x10u))) {
                    result = word(rt, hint + 0xCu) != 0u ? at(successor, successor) : at(0u, hint);
                } else {
                    result = unique();
                }
            }
        } else {
            result = hint;
            ctx.gpr[3] = hint_key;
        }
    }
    ctx.gpr[2] = result;
}
inline void tree_insert_hint_unsigned(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t insert_unique, std::uint32_t insert_at) { tree_insert_hint_n(rt, ctx, insert_unique, insert_at, false); }
inline void tree_insert_hint_signed(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t insert_unique, std::uint32_t insert_at) { tree_insert_hint_n(rt, ctx, insert_unique, insert_at, true); }
// func_08BFEBAC: unsigned keys. func_08C1EEEC / func_08B940D8: signed keys.
void func_08BFEBAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tree_insert_hint_unsigned(rt, ctx, 0x08BFEAD4u, 0x08BFE808u); }
void func_08C1EEEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tree_insert_hint_signed(rt, ctx, 0x08C1EE14u, 0x08C1ED4Cu); }
void func_08B940D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tree_insert_hint_signed(rt, ctx, 0x08B94000u, 0x08B93F38u); }


// func_08B2BFDC(heap = a0, size = a1): the heap's allocate under its lock (func_08B344E0 / func_08B3457C with a0 = heap). The request is rounded up to a multiple of 4 and 0xC bytes of header are added (need); when the free-byte count heap[0xD] is below need nothing happens (v0 = 0). Otherwise the count drops by need and func_08B2BA24(heap) is asked: below 8 it makes room (func_08B2BC5C) and asks again, still below 8 gives the bytes back (the count is raised by need again, v0 = 0). With 8 or more (or after the retry) func_08B2BA7C(heap) names a block whose first word is the answer (s4) and func_08B2BE68(heap, size) tries to place the request: a non-zero answer is stored at block[1]; zero makes func_08B2BC5C run, takes the table entry heap[0x11] + 8 * func_08B2B9BC(heap, s4) as the block and tries func_08B2BE68 again: a non-zero answer is stored at block[1], zero hands the entry back (func_08B2BA34(heap, entry), the count raised by need, v0 = 0). v0 = s4 when a block was placed.
// Branches the routes do not take (checked against the asm by hand): the not-enough-free-bytes return, the "level below 8" arm (func_08B2BC5C and the second check, the bytes given back) and the fallback when func_08B2BE68 places nothing (the table-entry arm, func_08B2BA34 and the bytes given back): every compared case places the request at once.
void func_08B2BFDC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t heap = ctx.gpr[4], request = ctx.gpr[5];
    std::uint32_t result = 0u;  // s4
    const std::uint32_t rounded = (request + 3u) & ~3u;
    (void)call_set(rt, ctx, 0x08B344E0u, {heap});
    const std::uint32_t need = rounded + 0xCu;
    const std::uint32_t free_bytes = word(rt, heap + 0x34u);
    ctx.gpr[3] = free_bytes - need;
    const auto unlock = [&]() { (void)call_set(rt, ctx, 0x08B3457Cu, {heap}); };
    if (free_bytes < need) {
        unlock();
        ctx.gpr[2] = result;
        return;
    }
    put(rt, heap + 0x34u, free_bytes - need);
    std::uint32_t level = call_set(rt, ctx, 0x08B2BA24u, {heap});
    if (signed_of(level) < 8) {
        (void)call_set(rt, ctx, 0x08B2BC5Cu, {heap});
        level = call_set(rt, ctx, 0x08B2BA24u, {heap});
        if (signed_of(level) < 8) {
            put(rt, heap + 0x34u, word(rt, heap + 0x34u) + need);
            unlock();
            ctx.gpr[2] = result;
            return;
        }
    }
    const std::uint32_t block = call_set(rt, ctx, 0x08B2BA7Cu, {heap});
    std::uint32_t entry = block;  // s0
    result = word(rt, block);
    std::uint32_t placed = call_set(rt, ctx, 0x08B2BE68u, {heap, rounded});
    if (placed == 0u) {
        (void)call_set(rt, ctx, 0x08B2BC5Cu, {heap});
        entry = word(rt, heap + 0x44u);
        const std::uint32_t index = call_set(rt, ctx, 0x08B2B9BCu, {heap, result});
        entry += index << 3;
        placed = call_set(rt, ctx, 0x08B2BE68u, {heap, rounded});
        ctx.gpr[5] = entry;
        if (placed == 0u) {
            (void)call_set(rt, ctx, 0x08B2BA34u, {heap, entry});
            result = 0u;
            put(rt, heap + 0x34u, word(rt, heap + 0x34u) + need);
            unlock();
            ctx.gpr[2] = result;
            return;
        }
    }
    put(rt, entry + 4u, placed);
    unlock();
    ctx.gpr[2] = result;
}

// func_08B33C0C(path = a0, flags = a1, mode = a2): sceIoOpen (0x08C7A870) with bookkeeping. `tracked` = func_08B7C6BC(path, D_08CB54E8, 6) == 0 (the path starts with that 6-byte prefix). Each round: func_08B33AF4(), func_08B33948() when tracked, the open. Untracked: a non-zero func_08B2DE00() turns the answer into 0x80020321 and ends; otherwise the answer is returned as it is. Tracked: a negative answer other than 0x8002012F starts another round, 0x8002012F ends; a non-negative one is appended, under the lock D_08D69214 (func_08B34D40 / func_08B34D80), to the table D_08D6921C (count D_08D6925C) and returned.
// Branches the routes do not take (checked against the asm by hand): an untracked path (func_08B2DE00 and the answer 0x80020321), a negative handle (the retry round, and the end on 0x8002012F): every compared case is a tracked open that succeeds and is appended to the table.
void func_08B33C0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t path = ctx.gpr[4], flags = ctx.gpr[5], mode = ctx.gpr[6];
    const bool tracked = call_set(rt, ctx, 0x08B7C6BCu, {path, 0x08CB54E8u, 6u}) == 0u;
    std::uint32_t handle;
    for (;;) {
        (void)call_set(rt, ctx, 0x08B33AF4u, {});
        if (tracked) (void)call_set(rt, ctx, 0x08B33948u, {});
        handle = call_set(rt, ctx, 0x08C7A870u, {path, flags, mode});
        if (!tracked) {
            if (call_set(rt, ctx, 0x08B2DE00u, {}) != 0u) {
                handle = 0x80020321u;
                break;
            }
        }
        if (signed_of(handle) >= 0) {
            if (tracked) {
                (void)call_set(rt, ctx, 0x08B34D40u, {0x08D69214u});
                const std::uint32_t count = word(rt, 0x08D6925Cu);
                put(rt, 0x08D6921Cu + (count << 2), handle);
                put(rt, 0x08D6925Cu, count + 1u);
                (void)call_set(rt, ctx, 0x08B34D80u, {0x08D69214u});
            }
            break;
        }
        if (!tracked || handle == 0x8002012Fu) break;
    }
    ctx.gpr[2] = handle;
}

// func_08B2CE7C(heap = a0, pointer = a1): the heap's free under its lock (this + 0x58, func_08B34D40 / the tail jump func_08B34D80). A null pointer returns at once; func_08B2C780(heap, block) with block = pointer - 0x10 answering 0 returns v0 = 0. Otherwise the block's previous neighbour p = block[0]: without one func_08B2C458(heap, block); with one whose word +8 is not -1, func_08B2C720(heap, p) and func_08B2C484(heap, p, block) merge them (block = p). Then the next neighbour n = block[1]: when n < heap[2] and n's word +8 is not -1, func_08B2C720(heap, n) and func_08B2C484(heap, block, n) merge it; finally func_08B2C504(heap, block).
// Branches the routes do not take (checked against the asm by hand): a null pointer, func_08B2C780 answering 0, and a block without a previous neighbour (func_08B2C458).
void func_08B2CE7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t heap = ctx.gpr[4], pointer = ctx.gpr[5];
    if (pointer == 0u) return;
    const std::uint32_t lock = heap + 0x58u;
    {
        GuestFrame frame(ctx, 0x20u);
        std::uint32_t block = pointer - 0x10u;
        if (call_set(rt, ctx, 0x08B2C780u, {heap, block}) == 0u) return;
        (void)call_set(rt, ctx, 0x08B34D40u, {lock});
        const std::uint32_t previous = word(rt, pointer - 0x10u);
        if (previous != 0u) {
            if (word(rt, previous + 8u) != 0xFFFFFFFFu) {
                (void)call_set(rt, ctx, 0x08B2C720u, {heap, previous});
                (void)call_set(rt, ctx, 0x08B2C484u, {heap, previous, block});
                block = previous;
            }
        } else {
            (void)call_set(rt, ctx, 0x08B2C458u, {heap, block});
        }
        const std::uint32_t next = word(rt, block + 4u);
        if (next < word(rt, heap + 8u) && word(rt, next + 8u) != 0xFFFFFFFFu) {
            (void)call_set(rt, ctx, 0x08B2C720u, {heap, next});
            (void)call_set(rt, ctx, 0x08B2C484u, {heap, block, next});
        }
        (void)call_set(rt, ctx, 0x08B2C504u, {heap, block});
    }
    tail_call(ctx, 0x08B34D80u, {lock});
}


// A counted-pointer slot assigned: when `replacement` differs from `previous` (the value read before), its count at +4 goes up (v1 = the address, v0 = 1), the block that was in the slot (when not null) is counted down at +4 (not 1: done) and, if that reached zero, destroyed through virtual slot 8 and counted down at +8 (reaching zero: virtual slot 0xC), then the slot gets the replacement.
inline void assign_counted(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t slot, std::uint32_t replacement, std::uint32_t previous) {
    if (replacement == previous) return;
    if (replacement != 0u) {
        const std::uint32_t counter = word(rt, replacement + 4u);
        ctx.gpr[3] = replacement + 4u;
        ctx.gpr[2] = 1u;
        put(rt, replacement + 4u, counter + 1u);
    }
    const std::uint32_t block = word(rt, slot);
    if (block != 0u && count_down(rt, ctx, block + 4u)) {
        (void)call_virtual_full(rt, ctx, block, 8u, {block});
        if (count_down(rt, ctx, block + 8u)) (void)call_virtual_full(rt, ctx, block, 0xCu, {block});
    }
    put(rt, slot, replacement);
}

// The end of a destructor that releases its last shared object with a tail jump: `jr t9` to the object's virtual slot 0xC with a0 = the object (v0 = its vtable, t9 = the slot's function), after the frame is gone.
inline void tail_virtual_slot_c(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object) {
    const std::uint32_t vtable = word(rt, object);
    const std::uint32_t function = word(rt, vtable + 0xCu);
    ctx.gpr[2] = vtable;
    ctx.gpr[25] = function;
    tail_call(ctx, function, {object});
}

// func_08962A20(this = a0): func_08B28BE0(this + 0x40), func_08BE4084(this + 8), then the shared object this[1] (when not null) is released: use count (+4) down, at zero its virtual slot 8, weak count (+8) down, at zero the tail jump to its slot 0xC. v0 = 1 and v1 = the count's address on the early returns (the `ll/sc` leaves them), the second call's answer when this[1] is null.
// Branches the routes do not take (checked against the asm by hand): a null this[1], a use count other than 1 and a weak count other than 1 (every compared case destroys the object and ends in the tail jump).
void func_08962A20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t object;
    {
        GuestFrame frame(ctx, 0x10u);
        const std::uint32_t self = ctx.gpr[4];
        (void)call_set(rt, ctx, 0x08B28BE0u, {self + 0x40u});
        (void)call_set(rt, ctx, 0x08BE4084u, {self + 8u});
        object = word(rt, self + 4u);
        if (object == 0u) return;
        if (!count_down(rt, ctx, object + 4u)) return;
        (void)call_virtual_full(rt, ctx, object, 8u, {object});
        if (!count_down(rt, ctx, object + 8u)) return;
    }
    tail_virtual_slot_c(rt, ctx, object);
}

// Branches the routes do not take (checked against the asm by hand): a non-null this[1] (the use-count, slot 8 and tail-jump part): every compared case has it null, so only the this[2] call and the this[3] release can run.
// func_08909D98(this = a0): this[2] (when not null) gets func_08AF8AB0(it, 0); the shared object this[3] (when not null) is released (use count down; at zero slot 8, weak count down, at zero slot 0xC as a call); then the shared object this[1] the same way, ending with the tail jump to slot 0xC.
void func_08909D98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t last;
    {
        GuestFrame frame(ctx, 0x10u);
        const std::uint32_t self = ctx.gpr[4];
        const std::uint32_t first = word(rt, self + 8u);
        ctx.gpr[4] = first;
        if (first != 0u) (void)call_set(rt, ctx, 0x08AF8AB0u, {first, 0u});
        const std::uint32_t weak = word(rt, self + 0xCu);
        if (weak != 0u) release_shared(rt, ctx, weak);
        last = word(rt, self + 4u);
        if (last == 0u) return;
        if (!count_down(rt, ctx, last + 4u)) return;
        (void)call_virtual_full(rt, ctx, last, 8u, {last});
        if (!count_down(rt, ctx, last + 8u)) return;
    }
    tail_virtual_slot_c(rt, ctx, last);
}

// func_08AE2160(): func_08AEB808(), then a list of D_08D4069C words on the stack (the frame grows by (4 * n + 15) & ~15 bytes) is filled by func_08ADE268(list), which answers the count c of entries (kept in s0 by the delay slot of the next jal); func_08AEB82C() is called, and each of the first c entries gets func_08AD8D54(entry + 0x3C, 4). v0 is the last call's answer.
// Branches the routes do not take (checked against the asm by hand): the loop (func_08ADE268 answers a count of 0 or less in every compared case, so no entry gets func_08AD8D54).
void func_08AE2160(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    (void)call_set(rt, ctx, 0x08AEB808u, {});
    const std::uint32_t capacity = word(rt, 0x08D4069Cu);
    GuestFrame list(ctx, ((capacity << 2) + 0xFu) & ~0xFu);
    const std::uint32_t count = call_set(rt, ctx, 0x08ADE268u, {list.at(0u)});  // s0 = v0 sits in the delay slot of the next jal
    (void)call_set(rt, ctx, 0x08AEB82Cu, {});
    if (signed_of(count) <= 0) return;
    std::uint32_t cursor = list.at(0u);
    std::uint32_t remaining = count;
    do {
        const std::uint32_t entry = word(rt, cursor);
        --remaining;
        (void)call_set(rt, ctx, 0x08AD8D54u, {entry + 0x3Cu, 4u});
        cursor += 4u;
    } while (remaining != 0u);
}

// func_08AEFB40(-, selector = a1): the sound effect presets (the unit of func_08B8443C and its siblings). A selector of 10 or more returns (v0 = 0); 0 switches the unit off (func_08B8443C(1, 0), func_08B844D0(0, 0)); 1..9 set the effect type selector - 1 (func_08B84564), switch it on (func_08B8443C(1, 1)) and pass the preset words D_08D41AD0 + 0x4A0 / 0x4A4 to func_08B844D0 and + 0x4A8 / 0x4AC to func_08B845E8. The selector jumps through the table at 0x08C92590 (ten entries).
// Branches the routes do not take (checked against the asm by hand): selectors of 10 or more and selector 0 (the switch-off arm); every compared case is a preset 1..9.
void func_08AEFB40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t selector = ctx.gpr[5];
    ctx.gpr[2] = selector < 10u ? 1u : 0u;
    if (selector >= 10u) return;
    if (selector == 0u) {
        (void)call_set(rt, ctx, 0x08B8443Cu, {1u, 0u});
        (void)call_set(rt, ctx, 0x08B844D0u, {0u, 0u});
        return;
    }
    constexpr std::uint32_t presets = 0x08D41AD0u;
    (void)call_set(rt, ctx, 0x08B84564u, {selector - 1u});
    (void)call_set(rt, ctx, 0x08B8443Cu, {1u, 1u});
    (void)call_set(rt, ctx, 0x08B844D0u, {word(rt, presets + 0x4A0u), word(rt, presets + 0x4A4u)});
    (void)call_set(rt, ctx, 0x08B845E8u, {word(rt, presets + 0x4A8u), word(rt, presets + 0x4ACu)});
}

// Branches the routes do not take (checked against the asm by hand): a replacement equal to the old pointer, a null replacement, and an old block whose use count reaches zero (slots 8 and 0xC): the single compared case replaces a live pointer with another whose old block stays referenced.
// func_088D8B20(this = a0, source = a1): this[0x1A9] = source[0], the counted pointer this[0x1AA] takes source[1], this[0x1AB] = source[2], the counted pointer this[0x1AC] takes source[3] (assign_counted; v0 = source[2] and v1 = the old pointer when the second assignment changes nothing).
void func_088D8B20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t self = ctx.gpr[4], source = ctx.gpr[5];
    const std::uint32_t block = self + 0x6A4u;
    put(rt, block, word(rt, source));
    const std::uint32_t previous = word(rt, block + 4u);
    assign_counted(rt, ctx, block + 4u, word(rt, source + 4u), previous);
    const std::uint32_t middle = word(rt, source + 8u);
    const std::uint32_t previous_last = word(rt, block + 0xCu);
    ctx.gpr[2] = middle;
    ctx.gpr[3] = previous_last;
    put(rt, block + 8u, middle);
    assign_counted(rt, ctx, block + 0xCu, word(rt, source + 0xCu), previous_last);
}


// `wsbh`: the bytes of each halfword swapped; `wsbw`: the bytes of the word reversed (the PSP is little-endian, these records are big-endian).
inline std::uint32_t wsbh(std::uint32_t value) { return ((value & 0x00FF00FFu) << 8) | ((value >> 8) & 0x00FF00FFu); }
inline std::uint32_t wsbw(std::uint32_t value) { return __builtin_bswap32(value); }

// func_088D5AFC(holder = a0): fills the record b = holder[0] with big-endian constants (word 0 = 0x66, halfwords 1 at +0xC and 4 at +0x18 / +0x1A / +0x36, words 0x1770 at +0x10 and 0xBB8 at +0x14, -1 at +0x20 / +0x28 / +0x2C, halfword 0x3E8 at +0x30 / +0x32 / +0x34, zero words at +0x1C / +0x38 / +0x3C), four -1 halfwords at +0x40 with four zero bytes at +0x58, the bytes +0x25 = 0xFF, +0x4B = +0x49 = 1 and a run of zero bytes (+0x24, +0x26, +0x27, +0x48, +0x4A, +0x4C .. +0x57, +0x60 .. +0x64, +0x6E .. +0x70, +0x72 .. +0x79), four -1 halfwords at +0x66 and four at +0x7A, +0x71 = 0xFF; then the tail jump func_088EF51C(b + 0x82, 4, -1, 4, ...). v0 = b + 0x82 and v1 = 4 at the jump.
// Straight-line code and four counted loops: no branch is left for the routes to skip.
void func_088D5AFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t b = word(rt, ctx.gpr[4]);
    const std::uint32_t a3 = wsbh(4u), t0 = wsbw(0xFFFFFFFFu), t1 = wsbh(0x3E8u), v1 = wsbw(0x66u), a0 = wsbh(1u), a2 = wsbw(0x1770u), a1 = wsbw(0xBB8u);
    const std::uint32_t v0 = wsbh(0u), t2 = wsbw(0u);
    put(rt, b + 0x00u, v1);
    put16(rt, b + 0x0Cu, a0);
    const std::uint32_t minus = wsbh(0xFFFFFFFFu);
    put16(rt, b + 0x0Eu, v0);
    put(rt, b + 0x10u, a2);
    put(rt, b + 0x14u, a1);
    put16(rt, b + 0x1Au, a3);
    put(rt, b + 0x1Cu, t2);
    put(rt, b + 0x2Cu, t0);
    put16(rt, b + 0x34u, t1);
    put16(rt, b + 0x04u, v0);
    put16(rt, b + 0x06u, v0);
    put16(rt, b + 0x08u, v0);
    put16(rt, b + 0x0Au, v0);
    put16(rt, b + 0x36u, a3);
    put16(rt, b + 0x18u, a3);
    put(rt, b + 0x20u, t0);
    put(rt, b + 0x28u, t0);
    put16(rt, b + 0x30u, t1);
    put16(rt, b + 0x32u, t1);
    put(rt, b + 0x38u, 0u);
    put(rt, b + 0x3Cu, 0u);
    for (std::uint32_t i = 0u; i < 4u; ++i) {
        put16(rt, b + 0x40u + 2u * i, minus);
        put8(rt, b + 0x58u + i, 0u);
    }
    put8(rt, b + 0x4Bu, 1u);
    put8(rt, b + 0x25u, 0xFFFFFFFFu);
    const std::uint32_t minus2 = wsbh(0xFFFFFFFFu);
    put8(rt, b + 0x48u, 0u);
    put8(rt, b + 0x49u, 1u);
    for (const std::uint32_t offset : {0x4Au, 0x4Cu, 0x4Du, 0x24u, 0x26u, 0x27u, 0x4Eu, 0x4Fu, 0x50u, 0x51u, 0x52u, 0x53u, 0x54u, 0x55u, 0x56u, 0x57u, 0x60u}) put8(rt, b + offset, 0u);
    for (std::uint32_t i = 0u; i < 4u; ++i) {
        put8(rt, b + 0x61u + i, 0u);
        put16(rt, b + 0x66u + 2u * i, minus2);
    }
    put8(rt, b + 0x71u, 0xFFFFFFFFu);
    for (const std::uint32_t offset : {0x6Eu, 0x6Fu, 0x70u, 0x72u, 0x73u, 0x74u, 0x75u, 0x76u, 0x77u, 0x78u, 0x79u}) put8(rt, b + offset, 0u);
    for (std::uint32_t i = 0u; i < 4u; ++i) put16(rt, b + 0x7Au + 2u * i, wsbh(0xFFFFFFFFu));
    ctx.gpr[2] = b + 0x82u;
    ctx.gpr[3] = 4u;
    tail_call(ctx, 0x088EF51Cu, {b + 0x82u, 4u, minus2, 4u, 0xFFFFFFFFu, t1, t2, b});
}

// ---- end of functions ----
}  // namespace
}  // namespace hot_batch_103
}  // namespace gt::game

#define HOT103(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_103::name, gt::ReturnKind::kind)
#define HOT103N(address, name, kind) GT_DECOMP_NESTED(address, gt::game::hot_batch_103::name, gt::ReturnKind::kind)

// HOT103REGS
HOT103(0x08B844D0u, func_08B844D0, Word);
HOT103(0x08B8443Cu, func_08B8443C, Word);
HOT103(0x08B845E8u, func_08B845E8, Word);
HOT103(0x08B84564u, func_08B84564, Word);
HOT103(0x08B2E28Cu, func_08B2E28C, Word);
HOT103(0x08869220u, func_08869220, Word);
HOT103(0x0894CE2Cu, func_0894CE2C, Word);
HOT103(0x08A17A28u, func_08A17A28, Word);
HOT103(0x08BF738Cu, func_08BF738C, Word);
// HOT103(0x088B0520u, func_088B0520, Word);  // HOT103: writes memory but no compared case changed it (77 sampled cases and a dense dealer run of 3,000 cases, all equal: the list func_088AFB3C walks is empty at every call, only the lock is taken and released)
HOT103(0x08B8FE44u, func_08B8FE44, Word);
// HOT103(0x08B34B2Cu, func_08B34B2C, Word);  // HOT103: compared 0 in every run: all 101 sampled cases are not comparable (the caller owns the lock in every case and the thread sleeps: kernel isolation, sleep would block; 15,000-55,000 calls per route)
HOT103(0x08B33B30u, func_08B33B30, Word);
HOT103(0x08B31C14u, func_08B31C14, Word);
HOT103(0x08B84B5Cu, func_08B84B5C, Word);
HOT103(0x0896BF44u, func_0896BF44, Word);
HOT103(0x089958A0u, func_089958A0, Word);
HOT103(0x0899B04Cu, func_0899B04C, Word);
HOT103(0x089D8F3Cu, func_089D8F3C, Word);
HOT103(0x089DBFC8u, func_089DBFC8, Word);
HOT103(0x089E2DB0u, func_089E2DB0, Word);
HOT103(0x08BFEBACu, func_08BFEBAC, Word);
HOT103(0x08C1EEECu, func_08C1EEEC, Word);
HOT103(0x08B940D8u, func_08B940D8, Word);
HOT103(0x08B2BFDCu, func_08B2BFDC, Word);
HOT103(0x08B33C0Cu, func_08B33C0C, Word);
HOT103(0x08B2CE7Cu, func_08B2CE7C, Word);
HOT103(0x08962A20u, func_08962A20, Word);
HOT103(0x08909D98u, func_08909D98, Word);
HOT103(0x08AE2160u, func_08AE2160, Word);
HOT103(0x08AEFB40u, func_08AEFB40, Word);
HOT103(0x088D8B20u, func_088D8B20, Word);
HOT103(0x088D5AFCu, func_088D5AFC, Word);

// Batch 140 (2026-10-25): the rows above, verified by synthetic cases; a commented registration had no comparable
// synthetic case (every one faulted in the original), so its row runs as the recompiled original.
// HOT103(0x08974F64u, func_08974F64, Word);  // no comparable synthetic case yet
// HOT103(0x08975088u, func_08975088, Word);  // no comparable synthetic case yet
// HOT103(0x0897FB14u, func_0897FB14, Word);  // no comparable synthetic case yet
// HOT103(0x08984358u, func_08984358, Word);  // no comparable synthetic case yet
HOT103(0x0898A958u, func_0898A958, Word);
HOT103(0x089908D8u, func_089908D8, Word);
// HOT103(0x089940BCu, func_089940BC, Word);  // no comparable synthetic case yet
// HOT103(0x08997794u, func_08997794, Word);  // no comparable synthetic case yet
// HOT103(0x0899A660u, func_0899A660, Word);  // no comparable synthetic case yet
HOT103(0x0899E074u, func_0899E074, Word);
// HOT103(0x089A131Cu, func_089A131C, Word);  // no comparable synthetic case yet
HOT103(0x089A1440u, func_089A1440, Word);
// HOT103(0x089A4AE4u, func_089A4AE4, Word);  // no comparable synthetic case yet
HOT103(0x089B4258u, func_089B4258, Word);
// HOT103(0x089B8580u, func_089B8580, Word);  // no comparable synthetic case yet
// HOT103(0x089B9B48u, func_089B9B48, Word);  // no comparable synthetic case yet
// HOT103(0x089C0F84u, func_089C0F84, Word);  // no comparable synthetic case yet
// HOT103(0x089C2FACu, func_089C2FAC, Word);  // no comparable synthetic case yet
// HOT103(0x089CAF8Cu, func_089CAF8C, Word);  // no comparable synthetic case yet
// HOT103(0x089D0A5Cu, func_089D0A5C, Word);  // no comparable synthetic case yet
// HOT103(0x089E6624u, func_089E6624, Word);  // no comparable synthetic case yet
// HOT103(0x089EFD70u, func_089EFD70, Word);  // no comparable synthetic case yet
HOT103(0x089F450Cu, func_089F450C, Word);
HOT103(0x089F6F74u, func_089F6F74, Word);
HOT103(0x089F877Cu, func_089F877C, Word);
// HOT103(0x089F88A0u, func_089F88A0, Word);  // no comparable synthetic case yet
// HOT103(0x089FD2C0u, func_089FD2C0, Word);  // no comparable synthetic case yet
// HOT103(0x089FEAE4u, func_089FEAE4, Word);  // no comparable synthetic case yet
// HOT103(0x089FFE38u, func_089FFE38, Word);  // no comparable synthetic case yet
// HOT103(0x08A0598Cu, func_08A0598C, Word);  // no comparable synthetic case yet
HOT103(0x08A05AB0u, func_08A05AB0, Word);
HOT103(0x08A19F68u, func_08A19F68, Word);
