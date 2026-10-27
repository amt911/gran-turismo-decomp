// Virtual calls through a holder, one body for every slot:
//
//     void call(Holder *holder) { if (holder && holder->object) holder->object->slot(holder->object, holder); }
//
// (`beqz $a0; lw $v0, 0($a0); beqz $v0; lw $v1, 0($v0); lw $t9, slot($v1); jr $t9` with the holder in
// $a1). The table gives each instance's vtable offset. Listed: the ones the routes call (the harness
// compared them equal); the others stay recompiled until a run reaches them.

#include "decomp_registry.hpp"

#include <unordered_map>

namespace gt::core {
namespace {

std::unordered_map<std::uint32_t, std::uint32_t> &holder_virtual_table() {
    static std::unordered_map<std::uint32_t, std::uint32_t> table;
    return table;
}

} // namespace

struct HolderVirtualRegistrar {
    HolderVirtualRegistrar(std::uint32_t entry, std::uint32_t slot_offset) { holder_virtual_table()[entry] = slot_offset; }
};

// holder_virtual(holder): continues in the object's virtual (object, holder); nothing for an empty holder.
void holder_virtual(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t holder = ctx.gpr[4];
    const std::uint32_t slot_offset = holder_virtual_table().at(ctx.pc);
    ctx.gpr[5] = holder;
    if (holder == 0u) return;
    const std::uint32_t object = rt.memory().load32(holder);
    ctx.gpr[2] = object;
    if (object == 0u) return;
    const std::uint32_t target = rt.memory().load32(rt.memory().load32(object) + slot_offset);
    tail_call(ctx, target, {object});
}

} // namespace gt::core

#define GT_DECOMP_HOLDER_VIRTUAL(entry, slot_offset)                                                     \
    static const ::gt::core::HolderVirtualRegistrar GT_DECOMP_CAT(gt_holder_virtual_, __LINE__){(entry), (slot_offset)}; \
    GT_DECOMP(entry, gt::core::holder_virtual, gt::ReturnKind::Word)

GT_DECOMP_HOLDER_VIRTUAL(0x08ABB64Cu, 0x40u);
GT_DECOMP_HOLDER_VIRTUAL(0x08ABB6D4u, 0x4Cu);
GT_DECOMP_HOLDER_VIRTUAL(0x08ABB700u, 0x50u);
GT_DECOMP_HOLDER_VIRTUAL(0x08ABB72Cu, 0x54u);
GT_DECOMP_HOLDER_VIRTUAL(0x08ABBF28u, 0x40u);
GT_DECOMP_HOLDER_VIRTUAL(0x08ABBF54u, 0x44u);
GT_DECOMP_HOLDER_VIRTUAL(0x08ABBF80u, 0x48u);
GT_DECOMP_HOLDER_VIRTUAL(0x08ABBFACu, 0x4Cu);
GT_DECOMP_HOLDER_VIRTUAL(0x08ABC008u, 0x54u);
GT_DECOMP_HOLDER_VIRTUAL(0x08ABC17Cu, 0x70u);
GT_DECOMP_HOLDER_VIRTUAL(0x08ABC1A8u, 0x74u);
GT_DECOMP_HOLDER_VIRTUAL(0x08ABCA48u, 0x44u);
GT_DECOMP_HOLDER_VIRTUAL(0x08ABCC48u, 0x38u);

// Batch 164: more members of the 0x08ABB64C family (tools.family_rows), verified by synthetic cases.
GT_DECOMP_HOLDER_VIRTUAL(0x08ABC584u, 0x2Cu);
GT_DECOMP_HOLDER_VIRTUAL(0x08ABC92Cu, 0x14u);
GT_DECOMP_HOLDER_VIRTUAL(0x08ABCCA0u, 0x3Cu);
