// Call chains the compiler emits many times over, one body per shape and a table with each
// instance's targets:
//   call_return_self(this, ...)  first(this, ...); return this;
//   call_then(args...)           return next(first(args...));
// Listed: the ones the race path calls (the harness compared them equal); the others stay
// recompiled until a run reaches them.

#include "decomp_registry.hpp"
#include "guest_call.hpp"

#include <unordered_map>

namespace gt::core {
namespace {

struct Chain {
    std::uint32_t first;
    std::uint32_t next;
};

std::unordered_map<std::uint32_t, Chain> &chain_table() {
    static std::unordered_map<std::uint32_t, Chain> table;
    return table;
}

} // namespace

struct ChainRegistrar {
    ChainRegistrar(std::uint32_t entry, std::uint32_t first, std::uint32_t next) { chain_table()[entry] = {first, next}; }
};

// call_return_self(this, ...) -> this, after first(this, ...).
void call_return_self(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t first = chain_table().at(ctx.pc).first;
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    call_guest(rt, ctx, first, {});
    ctx.gpr[2] = self;
}

// call_then(args...): continue in `next` with first(args...) as its argument.
void call_then(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const Chain &c = chain_table().at(ctx.pc);
    std::uint32_t result;
    {
        GuestFrame frame(ctx, 0x10u);
        result = call_guest(rt, ctx, c.first, {});
    }
    tail_call(ctx, c.next, {result});
}

} // namespace gt::core

#define GT_DECOMP_CHAIN_(fn, entry, first, next)                                                         \
    static const ::gt::core::ChainRegistrar GT_DECOMP_CAT(gt_chain_, __LINE__){(entry), (first), (next)}; \
    GT_DECOMP(entry, gt::core::fn, gt::ReturnKind::Word)
#define GT_DECOMP_CALL_RETURN_SELF(entry, first) GT_DECOMP_CHAIN_(call_return_self, entry, first, 0u)
#define GT_DECOMP_CALL_THEN(entry, first, next) GT_DECOMP_CHAIN_(call_then, entry, first, next)

GT_DECOMP_CALL_RETURN_SELF(0x089641F4u, 0x089641DCu);
GT_DECOMP_CALL_RETURN_SELF(0x089CC040u, 0x089CBFE4u);
GT_DECOMP_CALL_RETURN_SELF(0x08A567FCu, 0x08A56784u);
GT_DECOMP_CALL_RETURN_SELF(0x08B8C5ACu, 0x08AA2C90u);
GT_DECOMP_CALL_RETURN_SELF(0x08BE9040u, 0x08AA20F4u);
GT_DECOMP_CALL_RETURN_SELF(0x08BE9098u, 0x089761DCu);
GT_DECOMP_CALL_RETURN_SELF(0x08BF3BD0u, 0x08AA2C10u);
GT_DECOMP_CALL_RETURN_SELF(0x08BF7BE0u, 0x08A76ABCu);
GT_DECOMP_CALL_RETURN_SELF(0x08C34248u, 0x08AAACF4u);

GT_DECOMP_CALL_THEN(0x08B556A0u, 0x08B55630u, 0x08B55678u);
