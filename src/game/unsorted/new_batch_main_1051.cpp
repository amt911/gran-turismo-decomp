// Main-session batch 1051: two 596-instruction bodies that earlier drafts skipped. Hand-written structured C from
// `tools.disasm`; no names (none has evidence), no byte-for-byte claim. Unverified drafts: the gate decides.
// Conventions: a guest call leaves v0 = its result (`call`), v1/f0 too (`call_g`). The compiler's counted base (use
// count at +4, weak count at +8, dispose = vtable +8, destroy = vtable +0xC) is `release`, which leaves v0 as the
// original does (1 after an atomic update that did not reach zero, else the last virtual call's answer). Stack
// temporaries whose address is passed to a callee live in the guest frame at the asm's offsets.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1051 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using GuestResult = gt::GuestResult;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call: the listed arguments go to a0-a3/t0-t3 (what is not listed keeps the register's current value),
// and afterwards v0/v1/f0 are the callee's, as after the `jal`.
GuestResult call_g(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    return call_g(rt, ctx, target, arguments).v0;
}
// The virtual call: v0 = the vtable word, callee = vtable[offset].
u32 vcall(Runtime &rt, AllegrexContext &ctx, u32 object, u32 offset, std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    const u32 function = word(rt, table + offset);
    ctx.gpr[2] = table;
    return call(rt, ctx, function, arguments);
}
// ll/sc loop as one operation: increment the word at `address`; v0 = 1 (the sc answer), v1 = address.
void atomic_inc(Runtime &rt, AllegrexContext &ctx, u32 address) {
    put(rt, address, word(rt, address) + 1u);
    ctx.gpr[2] = 1u;
    ctx.gpr[3] = address;
}
// ll/sc loop as one operation: decrement; the old value comes back (it is what the asm compares with 1), v0 = 1.
u32 atomic_dec(Runtime &rt, AllegrexContext &ctx, u32 address) {
    const u32 old = word(rt, address);
    put(rt, address, old - 1u);
    ctx.gpr[2] = 1u;
    ctx.gpr[3] = address;
    ctx.gpr[4] = old;
    return old;
}
// Drop one reference of a counted block (a null one is skipped and leaves v0 alone).
void release(Runtime &rt, AllegrexContext &ctx, u32 counted) {
    if (counted == 0u) return;
    if (atomic_dec(rt, ctx, counted + 4u) != 1u) return;
    vcall(rt, ctx, counted, 8u, {counted});
    if (atomic_dec(rt, ctx, counted + 8u) != 1u) return;
    vcall(rt, ctx, counted, 0xCu, {counted});
}
// The counted pair at `slot` (data) / `slot + 4` (count): the data word is stored, then the count with the asm's
// sequence -- the new count is incremented, the one it replaces released, and the count stored.
void assign_counted(Runtime &rt, AllegrexContext &ctx, u32 slot, u32 data, u32 counted) {
    put(rt, slot, data);
    if (counted == word(rt, slot + 4u)) return;
    if (counted != 0u) atomic_inc(rt, ctx, counted + 4u);
    release(rt, ctx, word(rt, slot + 4u));
    put(rt, slot + 4u, counted);
}
// Signed `div` (quotient in LO). The original traps on a zero divisor; the draft keeps the C defined there.
s32 quotient(u32 numerator, u32 denominator) {
    if (denominator == 0u) return 0;
    return static_cast<s32>(static_cast<std::int64_t>(static_cast<s32>(numerator)) /
                            static_cast<std::int64_t>(static_cast<s32>(denominator)));
}
// libstdc++'s copy-on-write string whose text starts at `text`: its representation is the 0xC bytes before it, with
// the use count at text - 4. The shared empty representation 0x08D75298 is left alone; when the count does not stay
// positive (0x08B60804 answers the old one) 0x08B93554 frees the representation. `tag` is the stack byte passed as a1.
void release_text(Runtime &rt, AllegrexContext &ctx, u32 text, u32 tag) {
    const u32 representation = text - 0xCu;
    if (representation == 0x08D75298u) return;
    if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {text - 4u, 0xFFFFFFFFu})) > 0) return;
    (void)call(rt, ctx, 0x08B93554u, {representation, tag});
}
// One element of the container (four words: two shared pointers, data then count): the words are written and each
// count taken (the asm's two ll/sc increments).
void push_element(Runtime &rt, AllegrexContext &ctx, u32 destination, u32 source) {
    const u32 count1 = word(rt, source + 4u);
    const u32 count2 = word(rt, source + 0xCu);
    put(rt, destination, word(rt, source));
    put(rt, destination + 4u, count1);
    if (count1 != 0u) atomic_inc(rt, ctx, count1 + 4u);
    put(rt, destination + 8u, word(rt, source + 8u));
    put(rt, destination + 0xCu, count2);
    if (count2 != 0u) atomic_inc(rt, ctx, count2 + 4u);
}

// The callee-saved integer registers a body keeps values in (the caller's values have to come back).
class SavedRegs {
public:
    SavedRegs(AllegrexContext &ctx, std::initializer_list<unsigned> regs) : ctx_(ctx), count_(regs.size()) {
        unsigned i = 0u;
        for (const unsigned reg : regs) {
            regs_[i] = reg;
            values_[i] = ctx.gpr[reg];
            ++i;
        }
    }
    ~SavedRegs() {
        for (unsigned i = count_; i-- > 0u;) ctx_.gpr[regs_[i]] = values_[i];
    }
    SavedRegs(const SavedRegs &) = delete;
    SavedRegs &operator=(const SavedRegs &) = delete;

private:
    AllegrexContext &ctx_;
    unsigned regs_[16]{};
    u32 values_[16]{};
    unsigned count_;
};

// ---------------------------------------------------------------------------------------------------------------------
// 0x08BE2F7C (a0 = object): the sibling of 0x08BE27C4 (new_batch_main_131) -- a shared copy of itself at sp+0x30 is
// made by 0x08BE1528, the virtual at +0xC runs with a zero, and the state word at +4 picks the body:
//   0    the update start: the string at +0x54 is measured (0x08B93CD8) and 0x08BC17C8 fills the pair at sp+0x44
//        (byte -> +0x6C, word -> +0x70, counted -> +0x74); when the byte is set the found object's data goes to
//        +0x80 and the state becomes 0xA1, otherwise a clear +0x70 registers this function again (0x08BE191C) and
//        the state becomes 0xCD; a set +0x70 instead refreshes the pair at +0x7C from +0x74.
//   0xA1 owner (+0x8) not flagged at +0xA8: owner's +0xB8 goes to +0x68, the pair at +0x78 is rebuilt from a new
//        string (0x08BC728C with +0x68 / +0x64) and the state becomes 0xB3.
//   0xB3 owner not flagged: the function pointer at +0x60 (over +0x78) runs, then the pair at +0x80 is cleared and
//        its counted half released. Both 0xA1 and 0xB3 end in the function pointer at +0x58 over (+0x78, +0x5C).
//   0xCD the same update as 0, then (a clear byte and a set +0x70) the pair at +0x7C is refreshed and +0x58 runs.
// Every state but the defaults pair the epilogue with one more release of the copy's count.
void func_08BE2F7C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegs saved(ctx, {16u, 17u, 18u, 19u, 20u, 21u, 22u});
    GuestFrame frame(ctx, 0x70u);
    const u32 copy = frame.at(0x30u);
    (void)call(rt, ctx, 0x08BE1528u, {copy, self});
    vcall(rt, ctx, self, 0xCu, {self, 0u});
    const u32 state = word(rt, self + 4u);
    ctx.gpr[3] = state;
    if (state == 0u) {
        const u32 made = call(rt, ctx, 0x089624ECu, {self + 0x40u});
        (void)call(rt, ctx, 0x08B93CD8u, {frame.at(0x14u), word(rt, self + 0x54u), frame.at(7u)});
        (void)call(rt, ctx, 0x08BC17C8u, {frame.at(0x44u), made, frame.at(0x14u)});
        rt.memory().store8(self + 0x6Cu, rt.memory().load8(frame.at(0x44u)));
        assign_counted(rt, ctx, self + 0x70u, word(rt, frame.at(0x48u)), word(rt, frame.at(0x4Cu)));
        release(rt, ctx, word(rt, frame.at(0x4Cu)));
        release_text(rt, ctx, word(rt, frame.at(0x14u)), frame.at(3u));
        const u32 mode = rt.memory().load8(self + 0x6Cu);
        ctx.gpr[2] = mode;
        if (mode != 0u) {
            const u32 found = call(rt, ctx, 0x089624ECu, {self + 0x40u});
            (void)call(rt, ctx, 0x08BC7744u, {frame.at(0x28u), found, self + 0x54u});
            assign_counted(rt, ctx, self + 0x80u, word(rt, frame.at(0x28u)), word(rt, frame.at(0x2Cu)));
            release(rt, ctx, word(rt, frame.at(0x2Cu)));
            (void)call(rt, ctx, 0x08AC04B4u, {self, 0u, word(rt, self + 0x50u), 1u, 0u});
            put(rt, self + 4u, 0xA1u);
            vcall(rt, ctx, self, 0xCu, {self, copy});
            const u32 owner = word(rt, self + 8u);
            vcall(rt, ctx, owner, 8u, {owner});
        } else if (word(rt, self + 0x70u) == 0u) {
            vcall(rt, ctx, self, 0xCu, {self, copy});
            put(rt, self + 4u, 0xCDu);
            const u32 made_again = call(rt, ctx, 0x089624ECu, {self + 0x40u});
            (void)call(rt, ctx, 0x08B93CD8u, {frame.at(0xCu), word(rt, self + 0x54u), frame.at(5u)});
            put(rt, frame.at(0x18u), 0x08BE2F7Cu);
            put(rt, frame.at(0x1Cu), 0u);
            (void)call(rt, ctx, 0x08BE191Cu, {made_again, frame.at(0xCu), frame.at(0x18u), self});
            release_text(rt, ctx, word(rt, frame.at(0xCu)), frame.at(1u));
        } else {
            // 0x08BE3794: +0x78 takes the value at +0x70, the counted pair at +0x7C is refreshed from +0x74.
            const u32 counted = word(rt, self + 0x74u);
            const u32 value = word(rt, self + 0x70u);
            assign_counted(rt, ctx, self + 0x78u, value, counted);
            (void)call(rt, ctx, word(rt, self + 0x58u), {self + 0x78u, word(rt, self + 0x5Cu)});
        }
    } else if (state == 0xA1u) {
        const u32 owner = word(rt, self + 8u);
        ctx.gpr[2] = owner;
        const u32 flag = word(rt, owner + 0xA8u);
        ctx.gpr[3] = flag;
        if (flag == 0u) {
            const u32 fresh = word(rt, owner + 0xA8u + 0x10u);
            ctx.gpr[2] = fresh;
            put(rt, self + 0x68u, fresh);
            const u32 found = call(rt, ctx, 0x089624ECu, {self + 0x40u});
            (void)call(rt, ctx, 0x08B93CD8u, {frame.at(0x10u), word(rt, self + 0x54u), frame.at(6u)});
            (void)call(rt, ctx, 0x08BC728Cu,
                       {frame.at(0x20u), found, frame.at(0x10u), word(rt, self + 0x68u), word(rt, self + 0x64u)});
            assign_counted(rt, ctx, self + 0x78u, word(rt, frame.at(0x20u)), word(rt, frame.at(0x24u)));
            release(rt, ctx, word(rt, frame.at(0x24u)));
            release_text(rt, ctx, word(rt, frame.at(0x10u)), frame.at(2u));
            put(rt, self + 4u, 0xB3u);
            (void)call(rt, ctx, 0x08AC0448u, {self, word(rt, self + 0x78u), word(rt, self + 0x68u), copy});
        }
    } else if (state == 0xB3u) {
        const u32 owner = word(rt, self + 8u);
        ctx.gpr[2] = owner;
        const u32 flag = word(rt, owner + 0xA8u);
        ctx.gpr[3] = flag;
        if (flag == 0u) {
            const u32 notify = word(rt, self + 0x60u);
            if (notify != 0u) (void)call(rt, ctx, notify, {word(rt, self + 0x78u)});
            const u32 counted = word(rt, self + 0x84u);
            put(rt, self + 0x80u, 0u);
            put(rt, self + 0x84u, 0u);
            release(rt, ctx, counted);
            (void)call(rt, ctx, word(rt, self + 0x58u), {self + 0x78u, word(rt, self + 0x5Cu)});
        }
    } else if (state == 0xCDu) {
        const u32 made = call(rt, ctx, 0x089624ECu, {self + 0x40u});
        (void)call(rt, ctx, 0x08B93CD8u, {frame.at(8u), word(rt, self + 0x54u), frame.at(4u)});
        (void)call(rt, ctx, 0x08BC17C8u, {frame.at(0x38u), made, frame.at(8u)});
        rt.memory().store8(self + 0x6Cu, rt.memory().load8(frame.at(0x38u)));
        assign_counted(rt, ctx, self + 0x70u, word(rt, frame.at(0x3Cu)), word(rt, frame.at(0x40u)));
        release(rt, ctx, word(rt, frame.at(0x40u)));
        release_text(rt, ctx, word(rt, frame.at(8u)), frame.at(0u));
        const u32 mode = rt.memory().load8(self + 0x6Cu);
        ctx.gpr[2] = mode;
        if (mode == 0u && word(rt, self + 0x70u) != 0u) {
            const u32 counted = word(rt, self + 0x74u);
            const u32 value = word(rt, self + 0x70u);
            assign_counted(rt, ctx, self + 0x78u, value, counted);
            (void)call(rt, ctx, word(rt, self + 0x58u), {self + 0x78u, word(rt, self + 0x5Cu)});
        }
    }
    release(rt, ctx, word(rt, copy + 4u));
}

// ---------------------------------------------------------------------------------------------------------------------
// 0x08863D8C (a0 = object, a1 = count, a2/a3/t0/t1 = the four words of a block): the counted pair at +0x4490 is
// replaced by the one 0x08963418 builds from (a2+a3, t0+t1) (and both temporaries released); a container at +0x44A0 is
// refilled: 0x08B904D8 sets it up for `count` elements and `count` times 0x08962AC0 builds an element out of the pair
// at +0x4490 and the quotients (a2/count, t0/count), which is pushed (0x08B8F81C grows the buffer); after the loop one
// more element is made from the pair 0x089626C8 answers, assigned to the two counted pairs at +0x44AC / +0x44B4, and
// the local container is moved into +0x44A0 (old range destroyed by 0x08B8FE44, old buffer freed by 0x08B35254,
// 0x08B90024 takes the three words); the local's own range and buffer are freed the same way last.
void func_08863D8C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 count = ctx.gpr[5];
    const u32 block_x = ctx.gpr[6];
    const u32 block_y = ctx.gpr[7];
    const u32 block_z = ctx.gpr[8];
    const u32 block_w = ctx.gpr[9];
    SavedRegs saved(ctx, {16u, 17u, 18u, 19u, 20u, 21u, 22u, 23u, 30u});
    GuestFrame frame(ctx, 0xC0u);
    if (call(rt, ctx, 0x089624FCu, {self + 0x4490u}) != 0u) return;

    put(rt, frame.at(0x14u), block_x + block_y);
    put(rt, frame.at(0x18u), block_z + block_w);
    (void)call(rt, ctx, 0x08963418u, {frame.at(0x48u), frame.at(0x14u)});
    assign_counted(rt, ctx, self + 0x4490u, word(rt, frame.at(0x48u)), word(rt, frame.at(0x4Cu)));
    assign_counted(rt, ctx, self + 0x4490u + 8u, word(rt, frame.at(0x50u)), word(rt, frame.at(0x54u)));
    release(rt, ctx, word(rt, frame.at(0x54u)));
    release(rt, ctx, word(rt, frame.at(0x4Cu)));

    for (u32 offset = 0x58u; offset <= 0x70u; offset += 4u) put(rt, frame.at(offset), 0u);
    put(rt, frame.at(0x10u), static_cast<u32>(quotient(block_z, count)));
    put(rt, frame.at(0xCu), static_cast<u32>(quotient(block_x, count)));
    (void)call(rt, ctx, 0x08B904D8u, {frame.at(0x58u), count});

    if (static_cast<s32>(count) > 0) {
        for (u32 i = 0u; i != count; ++i) {
            (void)call(rt, ctx, 0x08962AC0u, {frame.at(0x38u), self + 0x4490u, frame.at(0xCu)});
            const u32 end = word(rt, frame.at(0x5Cu));
            if (end == word(rt, frame.at(0x60u))) {
                (void)call(rt, ctx, 0x08B8F81Cu, {frame.at(0x58u), end, frame.at(0x38u)});
            } else {
                push_element(rt, ctx, end, frame.at(0x38u));
                put(rt, frame.at(0x5Cu), end + 0x10u);
            }
            release(rt, ctx, word(rt, frame.at(0x44u)));
            release(rt, ctx, word(rt, frame.at(0x3Cu)));
        }
    }

    call_g(rt, ctx, 0x089626C8u, {self + 0x4490u});
    put(rt, frame.at(4u), ctx.gpr[2]);
    put(rt, frame.at(8u), ctx.gpr[3]);
    (void)call(rt, ctx, 0x08962AC0u, {frame.at(0x28u), self + 0x4490u, frame.at(4u)});
    assign_counted(rt, ctx, frame.at(0x64u), word(rt, frame.at(0x28u)), word(rt, frame.at(0x2Cu)));
    assign_counted(rt, ctx, frame.at(0x6Cu), word(rt, frame.at(0x30u)), word(rt, frame.at(0x34u)));
    release(rt, ctx, word(rt, frame.at(0x34u)));
    release(rt, ctx, word(rt, frame.at(0x2Cu)));

    if (self + 0x44A0u != frame.at(0x58u)) {
        const u32 old_begin = word(rt, self + 0x44A0u);
        const u32 old_end = word(rt, self + 0x44A0u + 4u);
        put(rt, self + 0x44A0u, 0u);
        put(rt, self + 0x44A0u + 4u, 0u);
        put(rt, self + 0x44A0u + 8u, 0u);
        (void)call(rt, ctx, 0x08B8FE44u, {old_begin, old_end, frame.at(1u)});
        if (old_begin != 0u) (void)call(rt, ctx, 0x08B35254u, {old_begin, frame.at(0x58u)});
        (void)call(rt, ctx, 0x08B90024u, {self + 0x44A0u, frame.at(0x58u)});
        assign_counted(rt, ctx, self + 0x44A0u + 0xCu, word(rt, frame.at(0x64u)), word(rt, frame.at(0x68u)));
        assign_counted(rt, ctx, self + 0x44A0u + 0x14u, word(rt, frame.at(0x6Cu)), word(rt, frame.at(0x70u)));
    }
    release(rt, ctx, word(rt, frame.at(0x70u)));
    release(rt, ctx, word(rt, frame.at(0x68u)));
    (void)call(rt, ctx, 0x08B8FE44u, {word(rt, frame.at(0x58u)), word(rt, frame.at(0x5Cu)), frame.at(0u)});
    if (word(rt, frame.at(0x58u)) != 0u) (void)call(rt, ctx, 0x08B35254u, {word(rt, frame.at(0x58u))});
}

} // namespace
} // namespace new_batch_main_1051
} // namespace gt::game

#define NEW1051(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1051::name, gt::ReturnKind::kind)
NEW1051(0x08BE2F7Cu, func_08BE2F7C, Word);
NEW1051(0x08863D8Cu, func_08863D8C, Word);
