// Container code the compiler emits once per element type, one body per shape and a table with
// each instance's constants:
//   free_list_push(node)  if (node) { node->next = head; head = node; } return head;
//                         (a pool's free list in one global; `next` at a per-type offset)
//   tree_clear()          Tree *t = get(); erase(t, t->root); reset t's header to empty;
//                         return &t->header;   (the shape of clear() on a global std::map/set)
// Listed: the ones the race path calls (the harness compared them equal); the others stay
// recompiled until a run reaches them.

#include "decomp_registry.hpp"
#include "guest_call.hpp"

#include <unordered_map>

namespace gt::core {
namespace {

struct Container {
    std::uint32_t first;   // free list: the head's global; tree: the getter
    std::uint32_t second;  // free list: offset of `next`; tree: the subtree eraser
};

std::unordered_map<std::uint32_t, Container> &container_table() {
    static std::unordered_map<std::uint32_t, Container> table;
    return table;
}

} // namespace

struct ContainerRegistrar {
    ContainerRegistrar(std::uint32_t entry, std::uint32_t first, std::uint32_t second) {
        container_table()[entry] = {first, second};
    }
};

// free_list_push(node) -> the list's head after pushing `node` (when not null).
void free_list_push(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const Container &c = container_table().at(ctx.pc);
    const std::uint32_t head = c.first;
    const std::uint32_t node = ctx.gpr[4];
    if (node != 0u) {
        rt.memory().store32(node + c.second, rt.memory().load32(head));
        rt.memory().store32(head, node);
    }
    ctx.gpr[2] = rt.memory().load32(head);
}

// tree_clear() -> &tree->header, with every node erased and the header pointing at itself.
// Layout as the code uses it (it matches libstdc++ _Rb_tree): +4 header {color, root, leftmost,
// rightmost}, +0x14 node count.
void tree_clear(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const Container &c = container_table().at(ctx.pc);
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t tree = call_guest(rt, ctx, c.first, {});
    call_guest(rt, ctx, c.second, {tree, rt.memory().load32(tree + 0x8u)});
    const std::uint32_t header = tree + 0x4u;
    rt.memory().store32(tree + 0x10u, header);  // rightmost
    rt.memory().store32(tree + 0x14u, 0u);      // count
    rt.memory().store32(tree + 0xCu, header);   // leftmost
    rt.memory().store32(tree + 0x8u, 0u);       // root
    ctx.gpr[2] = header;
}

} // namespace gt::core

#define GT_DECOMP_CONTAINER_(fn, entry, first, second)                                                   \
    static const ::gt::core::ContainerRegistrar GT_DECOMP_CAT(gt_container_, __LINE__){(entry), (first), (second)}; \
    GT_DECOMP(entry, gt::core::fn, gt::ReturnKind::Word)
#define GT_DECOMP_FREE_LIST_PUSH(entry, head, next_offset) GT_DECOMP_CONTAINER_(free_list_push, entry, head, next_offset)
#define GT_DECOMP_TREE_CLEAR(entry, get, erase) GT_DECOMP_CONTAINER_(tree_clear, entry, get, erase)

GT_DECOMP_FREE_LIST_PUSH(0x08A920E0u, 0x08CFF610u, 0x10u);
GT_DECOMP_FREE_LIST_PUSH(0x08BE8DE8u, 0x08D76A30u, 0xCu);
GT_DECOMP_FREE_LIST_PUSH(0x08BEC8A8u, 0x08D76A60u, 0xCu);
GT_DECOMP_FREE_LIST_PUSH(0x08BEC8E8u, 0x08D76A64u, 0xCu);
GT_DECOMP_FREE_LIST_PUSH(0x08BEEA3Cu, 0x08D76AC0u, 0xCu);
GT_DECOMP_FREE_LIST_PUSH(0x08BEF030u, 0x08D76AE8u, 0xCu);
GT_DECOMP_FREE_LIST_PUSH(0x08BEFCDCu, 0x08D76B10u, 0xCu);
GT_DECOMP_FREE_LIST_PUSH(0x08BF0960u, 0x08D76B60u, 0xCu);
GT_DECOMP_FREE_LIST_PUSH(0x08BF0A48u, 0x08D76B88u, 0xCu);
GT_DECOMP_FREE_LIST_PUSH(0x08BF0BB0u, 0x08D76BB0u, 0xCu);
GT_DECOMP_FREE_LIST_PUSH(0x08BF3334u, 0x08D76BD8u, 0xCu);
GT_DECOMP_FREE_LIST_PUSH(0x08BF346Cu, 0x08D76C00u, 0xCu);
GT_DECOMP_FREE_LIST_PUSH(0x08BF409Cu, 0x08D76C28u, 0xCu);
GT_DECOMP_FREE_LIST_PUSH(0x08BF4688u, 0x08D76C58u, 0xCu);
GT_DECOMP_FREE_LIST_PUSH(0x08BF46C8u, 0x08D76C5Cu, 0xCu);
GT_DECOMP_FREE_LIST_PUSH(0x08BF5A9Cu, 0x08D76C98u, 0xCu);
GT_DECOMP_FREE_LIST_PUSH(0x08BFD1E4u, 0x08D76CC0u, 0xCu);
GT_DECOMP_FREE_LIST_PUSH(0x08BFDEDCu, 0x08D76CE8u, 0xCu);
GT_DECOMP_FREE_LIST_PUSH(0x08BFE5A4u, 0x08D76D10u, 0xCu);
GT_DECOMP_FREE_LIST_PUSH(0x08BFE6BCu, 0x08D76D38u, 0xCu);
GT_DECOMP_FREE_LIST_PUSH(0x08BFFDC0u, 0x08D76D88u, 0xCu);
GT_DECOMP_FREE_LIST_PUSH(0x08C01D9Cu, 0x08D76DB0u, 0xCu);
GT_DECOMP_FREE_LIST_PUSH(0x08C02400u, 0x08D76DD8u, 0xCu);
GT_DECOMP_FREE_LIST_PUSH(0x08C04484u, 0x08D76E00u, 0xCu);
GT_DECOMP_FREE_LIST_PUSH(0x08C0457Cu, 0x08D76E28u, 0xCu);
GT_DECOMP_FREE_LIST_PUSH(0x08C06438u, 0x08D76E50u, 0xCu);
GT_DECOMP_FREE_LIST_PUSH(0x08C07FE8u, 0x08D76E78u, 0xCu);
GT_DECOMP_FREE_LIST_PUSH(0x08C09670u, 0x08D76EA0u, 0xCu);
GT_DECOMP_FREE_LIST_PUSH(0x08C0AAA0u, 0x08D76EC8u, 0xCu);
GT_DECOMP_FREE_LIST_PUSH(0x08C0B428u, 0x08D76EF0u, 0xCu);
GT_DECOMP_FREE_LIST_PUSH(0x08C0B560u, 0x08D76F18u, 0xCu);
GT_DECOMP_FREE_LIST_PUSH(0x08C0C30Cu, 0x08D76F48u, 0xCu);
GT_DECOMP_FREE_LIST_PUSH(0x08C0C34Cu, 0x08D76F4Cu, 0xCu);
GT_DECOMP_FREE_LIST_PUSH(0x08C0ECACu, 0x08D76F88u, 0xCu);
GT_DECOMP_FREE_LIST_PUSH(0x08C0EE58u, 0x08D76FB0u, 0xCu);
GT_DECOMP_FREE_LIST_PUSH(0x08C0EF40u, 0x08D76FD8u, 0xCu);
GT_DECOMP_FREE_LIST_PUSH(0x08C0F5BCu, 0x08D77000u, 0xCu);
GT_DECOMP_FREE_LIST_PUSH(0x08C10974u, 0x08D77030u, 0xCu);
GT_DECOMP_FREE_LIST_PUSH(0x08C109B4u, 0x08D77034u, 0xCu);
GT_DECOMP_FREE_LIST_PUSH(0x08C12020u, 0x08D77070u, 0xCu);
GT_DECOMP_FREE_LIST_PUSH(0x08C14714u, 0x08D77098u, 0xCu);
GT_DECOMP_FREE_LIST_PUSH(0x08C1482Cu, 0x08D770C0u, 0xCu);
GT_DECOMP_FREE_LIST_PUSH(0x08C18E94u, 0x08D770E8u, 0xCu);

