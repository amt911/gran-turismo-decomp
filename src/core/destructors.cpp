// Destructor-shaped code the compiler emits once per class, one body per shape and a table with
// each instance's constants:
//   destroy_then(this)         *this = table; body(this); next(this);
//                              (a deleting destructor: own vtable, the destructor body, then the
//                              deallocation — or the base destructor — as a tail call)
//   destroy_member_then(this)  *this = table; member_dtor(this + offset); base(this);
//                              (the destructor of a class whose only own work is one member)
// Listed: the ones the race path calls (the harness compared them equal); the others stay
// recompiled until a run reaches them.

#include "decomp_registry.hpp"
#include "guest_call.hpp"

#include <unordered_map>

namespace gt::core {
namespace {

struct Destructor {
    std::uint32_t table;
    std::uint32_t offset;
    std::uint32_t first;
    std::uint32_t next;
};

std::unordered_map<std::uint32_t, Destructor> &destructor_table() {
    static std::unordered_map<std::uint32_t, Destructor> table;
    return table;
}

} // namespace

struct DestructorRegistrar {
    DestructorRegistrar(std::uint32_t entry, Destructor destructor) { destructor_table()[entry] = destructor; }
};

// destroy_then(this): reset the vtable, run `first` on this, continue in `next` with this.
// destroy_member_then(this): the same with `first` run on this + offset.
void destroy_then(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const Destructor &d = destructor_table().at(ctx.pc);
    const std::uint32_t self = ctx.gpr[4];
    rt.memory().store32(self, d.table);
    {
        GuestFrame frame(ctx, 0x10u);
        call_guest(rt, ctx, d.first, {self + d.offset});
    }
    tail_call(ctx, d.next, {self});
}

} // namespace gt::core

#define GT_DECOMP_DESTROY_(entry, table, offset, first, next)                                            \
    static const ::gt::core::DestructorRegistrar GT_DECOMP_CAT(gt_destructor_, __LINE__){                \
        (entry), {(table), (offset), (first), (next)}};                                                  \
    GT_DECOMP(entry, gt::core::destroy_then, gt::ReturnKind::Word)
#define GT_DECOMP_DESTROY(entry, table, first, next) GT_DECOMP_DESTROY_(entry, table, 0u, first, next)
#define GT_DECOMP_DESTROY_MEMBER(entry, table, offset, first, next) \
    GT_DECOMP_DESTROY_(entry, table, offset, first, next)

GT_DECOMP_DESTROY(0x0892FC8Cu, 0x08CC6798u, 0x0892FBE0u, 0x08B64654u);
GT_DECOMP_DESTROY(0x08BE41B0u, 0x08CC6928u, 0x08BAEF88u, 0x08B64654u);
GT_DECOMP_DESTROY(0x08C18CF4u, 0x08CD05C0u, 0x08B0DC70u, 0x08B64654u);

GT_DECOMP_DESTROY_MEMBER(0x08A5D354u, 0x08CCB870u, 0x0014u, 0x08A69D68u, 0x08A780B0u);
GT_DECOMP_DESTROY_MEMBER(0x08B0DC70u, 0x08CD7C08u, 0x00F8u, 0x08B0C24Cu, 0x08B0DDDCu);
GT_DECOMP_DESTROY_MEMBER(0x08B0DCA8u, 0x08CD7C08u, 0x00F8u, 0x08B0C24Cu, 0x08B0DDDCu);
