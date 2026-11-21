// New batch 6: constructor- and destructor-shaped functions the extra seeds made emittable, written from `tools.disasm` as short sequences of
// named steps on one object (`Scope`): store a vtable word, store a constant, call a function with `this` or `this + offset`, then return or
// finish with a tail call. Every step is one or two instructions of the original, in the original's order (a delay slot runs before its
// `jal`). `Scope` keeps what the asm keeps: s0 holds `this` for the calls and the caller's s-registers come back, the frame of 16 bytes is
// below sp while the calls run, and `v0` is the last vtable word loaded or the last call's result.
// Verified by synthetic cases (some through corpus recipes, config/synth_corpus/<ADDRESS>.txt). No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_6 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;

class Scope {
public:
    Scope(Runtime &rt, AllegrexContext &ctx) : rt_(rt), ctx_(ctx), this_(ctx.gpr[4]), frame_(ctx, 0x10u) {
        for (unsigned i = 0u; i < 8u; ++i) saved_[i] = ctx.gpr[16u + i];
        saved_[8] = ctx.gpr[30];
        ctx.gpr[16] = this_;  // addu s0, a0, zero
    }
    ~Scope() {
        for (unsigned i = 0u; i < 8u; ++i) ctx_.gpr[16u + i] = saved_[i];
        ctx_.gpr[30] = saved_[8];
    }
    Scope(const Scope &) = delete;
    Scope &operator=(const Scope &) = delete;

    // `lui/addiu v0, VTABLE; sw v0, offset(this)`: the word is stored and stays in v0.
    void store_vtable(u32 offset, u32 value) {
        put(rt_, this_ + offset, value);
        ctx_.gpr[2] = value;
    }
    void store(u32 offset, u32 value) { put(rt_, this_ + offset, value); }
    void set_v0(u32 value) { ctx_.gpr[2] = value; }
    // `jal target` with a0 still `this`.
    void call_base(u32 target) { call(target, {}); }
    // `jal target` with `addiu a0, this, offset` in its delay slot.
    void call_member(u32 target, u32 offset) { call(target, {this_ + offset}); }
    // `jal target` with a constant in a0.
    void call_value(u32 target, u32 value) { call(target, {value}); }
    // The frame is popped and the function ends with `j target`; a0.. are what the original sets before it (default: this).
    void tail(u32 target, std::initializer_list<u32> arguments = {}) { tail_call(ctx_, target, arguments); }
    u32 self() const { return this_; }

private:
    void call(u32 target, std::initializer_list<u32> arguments) { ctx_.gpr[2] = call_guest(rt_, ctx_, target, arguments); }

    Runtime &rt_;
    AllegrexContext &ctx_;
    u32 this_;
    GuestFrame frame_;
    u32 saved_[9];
};

void func_08A1A968(Runtime &rt, AllegrexContext &ctx) {
    Scope scope(rt, ctx);
    scope.store_vtable(0x0u, 0x08CD0698u);
    scope.call_member(0x08B28DC4u, 0x68u);
    scope.tail(0x08ACFFC4u);
}

void func_08A380B8(Runtime &rt, AllegrexContext &ctx) {
    Scope scope(rt, ctx);
    scope.store_vtable(0x0u, 0x08CD16E0u);
    scope.call_value(0x08A37DD0u, 0x00000001u);
    scope.tail(0x08A780B0u);
}

void func_089B9908(Runtime &rt, AllegrexContext &ctx) {
    Scope scope(rt, ctx);
    scope.store(0x0u, 0x08CCA7C8u);
    scope.store_vtable(0x38u, 0x08CCA978u);
    scope.call_member(0x089B8884u, 0x64u);
    scope.tail(0x089BBAC8u);
}

void func_08B17C78(Runtime &rt, AllegrexContext &ctx) {
    Scope scope(rt, ctx);
    scope.store(0x0u, 0x08CD91C8u);
    scope.store_vtable(0xA0u, 0x08CD7C98u);
    scope.call_member(0x08B1FD74u, 0xA0u);
    scope.tail(0x08B11918u);
}

void func_08A3AE60(Runtime &rt, AllegrexContext &ctx) {
    Scope scope(rt, ctx);
    scope.store_vtable(0x0u, 0x08CBB9A0u);
    scope.call_base(0x08A3A6FCu);
    scope.call_member(0x08A5B9ECu, 0x34u);
    scope.call_member(0x08A5B9ECu, 0x30u);
    scope.tail(0x08A780B0u);
}

void func_08A9425C(Runtime &rt, AllegrexContext &ctx) {
    Scope scope(rt, ctx);
    scope.store_vtable(0x0u, 0x08CD3B28u);
    scope.call_base(0x08A93704u);
    scope.call_member(0x08A5B9ECu, 0x20u);
    scope.call_member(0x08A5B9ECu, 0x1Cu);
    scope.tail(0x08A72308u);
}

void func_08B447D4(Runtime &rt, AllegrexContext &ctx) {
    Scope scope(rt, ctx);
    scope.store_vtable(0x0u, 0x08CDA9A0u);
    scope.call_base(0x08B404C8u);
    scope.call_member(0x08B28DC4u, 0x12Cu);
    scope.call_member(0x08B2C3BCu, 0xCCu);
    scope.tail(0x08B08420u);
}

void func_08B1CEF0(Runtime &rt, AllegrexContext &ctx) {
    Scope scope(rt, ctx);
    scope.store_vtable(0x0u, 0x08CD9350u);
    scope.call_member(0x08B28DCCu, 0x4u);
    scope.store(0x34u, 0x00000000u);
    scope.store(0x2Cu, 0x00000000u);
    scope.store(0x30u, 0x00000000u);
}

void func_08B1CF30(Runtime &rt, AllegrexContext &ctx) {
    Scope scope(rt, ctx);
    scope.store_vtable(0x0u, 0x08CD9350u);
    scope.call_member(0x08B28DCCu, 0x4u);
    scope.store(0x34u, 0x00000000u);
    scope.store(0x2Cu, 0x00000000u);
    scope.store(0x30u, 0x00000000u);
}

void func_08A8A650(Runtime &rt, AllegrexContext &ctx) {
    Scope scope(rt, ctx);
    scope.store_vtable(0x0u, 0x08CD3438u);
    scope.call_member(0x08A5B9ECu, 0x1Cu);
    scope.call_member(0x08A5B9ECu, 0x18u);
    scope.tail(0x08A5D354u);
}

void func_08A65444(Runtime &rt, AllegrexContext &ctx) {
    Scope scope(rt, ctx);
    scope.store_vtable(0x0u, 0x08CD29A8u);
    scope.call_member(0x08A5B9ECu, 0x18u);
    scope.call_member(0x08A75A88u, 0xCu);
    scope.call_member(0x08A69D68u, 0x8u);
    scope.store_vtable(0x0u, 0x08CD3638u);
}

void func_08A65844(Runtime &rt, AllegrexContext &ctx) {
    Scope scope(rt, ctx);
    scope.store_vtable(0x0u, 0x08CD2A48u);
    scope.call_member(0x08A69D68u, 0x18u);
    scope.call_member(0x08A69D68u, 0x14u);
    scope.call_member(0x08A75A88u, 0x8u);
    scope.store_vtable(0x0u, 0x08CD3638u);
}

void func_08B11D20(Runtime &rt, AllegrexContext &ctx) {
    Scope scope(rt, ctx);
    scope.store(0x0u, 0x08CD7D60u);
    scope.store_vtable(0xA0u, 0x08CD7E40u);
    scope.call_member(0x08B2B910u, 0x280u);
    scope.call_member(0x08B28DC4u, 0x1E0u);
    scope.call_member(0x08B28DC4u, 0x1B8u);
    scope.call_member(0x08B0CEB4u, 0xA0u);
    scope.tail(0x08B11918u);
}

void func_0893A09C(Runtime &rt, AllegrexContext &ctx) {
    Scope scope(rt, ctx);
    scope.store_vtable(0x0u, 0x08CC6808u);
    scope.call_base(0x08939FECu);
    scope.tail(0x08B28DC4u, {scope.self() + 0x14u});
}

void func_08939008(Runtime &rt, AllegrexContext &ctx) {
    Scope scope(rt, ctx);
    scope.store_vtable(0x0u, 0x08CC67F8u);
    scope.call_base(0x08938F18u);
    scope.call_base(0x08938F50u);
    scope.tail(0x0893A068u, {scope.self() + 0xA0u});
}

void func_08B0C844(Runtime &rt, AllegrexContext &ctx) {
    Scope scope(rt, ctx);
    scope.store_vtable(0x0u, 0x08CD7B80u);
    scope.call_base(0x08B0C778u);
    scope.call_member(0x08B0DDF0u, 0x20u);
    scope.tail(0x08B0C1D8u);
}

void func_08B083A8(Runtime &rt, AllegrexContext &ctx) {
    Scope scope(rt, ctx);
    scope.store_vtable(0x0u, 0x08CD79E0u);
    scope.call_member(0x08B28DCCu, 0x4u);
    scope.tail(0x08C4C3E0u, {scope.self() + 0x2Cu, scope.self()});
}

void func_08AC64BC(Runtime &rt, AllegrexContext &ctx) {
    Scope scope(rt, ctx);
    scope.store_vtable(0x0u, 0x08CD6690u);
    scope.call_member(0x08B0C808u, 0x8u);
    scope.store_vtable(0x0u, 0x08CD6718u);
}

void func_08B22AAC(Runtime &rt, AllegrexContext &ctx) {
    Scope scope(rt, ctx);
    scope.store(0x0u, 0x08CD9588u);
    scope.store_vtable(0x2Cu, 0x08CD95A0u);
    scope.call_member(0x08B1FD74u, 0x2Cu);
    scope.tail(0x08B28DC4u, {scope.self() + 0x4u});
}

void func_08B24158(Runtime &rt, AllegrexContext &ctx) {
    Scope scope(rt, ctx);
    scope.store_vtable(0x0u, 0x08CD9600u);
    scope.call_member(0x08B28BE0u, 0x70u);
    scope.call_member(0x08B28BE0u, 0x44u);
    scope.call_member(0x08B28DC4u, 0x1Cu);
    scope.tail(0x08B24A0Cu);
}

void func_089384F8(Runtime &rt, AllegrexContext &ctx) {
    Scope scope(rt, ctx);
    scope.store_vtable(0x0u, 0x08CC67E8u);
    scope.call_member(0x08939FA4u, 0x28u);
    scope.store(0x8u, 0xFFFFFFFFu);
    scope.store(0x24u, 0x00000000u);
    scope.store(0x4u, 0x00000000u);
    scope.store(0xCu, 0x00000000u);
    scope.store(0x10u, 0x00000000u);
    scope.store(0x14u, 0x00000000u);
    scope.store(0x18u, 0x00000000u);
    scope.store(0x1Cu, 0x00000000u);
    scope.store(0x20u, 0x00000000u);
    scope.set_v0(0xFFFFFFFFu);
}

} // namespace
} // namespace new_batch_6
} // namespace gt::game

#define NEW6(address, name, kind) GT_DECOMP(address, gt::game::new_batch_6::name, gt::ReturnKind::kind)
NEW6(0x08A1A968u, func_08A1A968, Word);
NEW6(0x08A380B8u, func_08A380B8, Word);
NEW6(0x089B9908u, func_089B9908, Word);
NEW6(0x08B17C78u, func_08B17C78, Word);
NEW6(0x08A3AE60u, func_08A3AE60, Word);
NEW6(0x08A9425Cu, func_08A9425C, Word);
NEW6(0x08B447D4u, func_08B447D4, Word);
NEW6(0x08B1CEF0u, func_08B1CEF0, Word);
NEW6(0x08B1CF30u, func_08B1CF30, Word);
NEW6(0x08A8A650u, func_08A8A650, Word);
NEW6(0x08A65444u, func_08A65444, Word);
NEW6(0x08A65844u, func_08A65844, Word);
NEW6(0x08B11D20u, func_08B11D20, Word);
NEW6(0x0893A09Cu, func_0893A09C, Word);
NEW6(0x08939008u, func_08939008, Word);
NEW6(0x08B0C844u, func_08B0C844, Word);
NEW6(0x08B083A8u, func_08B083A8, Word);
NEW6(0x08AC64BCu, func_08AC64BC, Word);
NEW6(0x08B22AACu, func_08B22AAC, Word);
NEW6(0x08B24158u, func_08B24158, Word);
NEW6(0x089384F8u, func_089384F8, Word);
