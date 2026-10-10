// Reference counting of the engine's shared objects and of the smart pointer that holds them.
//
// Every counted object starts with
//     +0 vtable, +4 strong count, +8 weak count
// (the counts are updated with ll/sc loops on the PSP; one core makes them plain read-modify-write).
// The first strong reference also takes a weak one, and the last strong release first disposes of
// the object's contents (vtable slot 2) and then drops that weak reference; the last weak release
// deletes the object (vtable slot 1). Evidence: the code below, and every Adhoc native going
// through it (docs/FACTS.md, Adhoc bridge).
//
// The smart pointer ("holder") is one word, the object pointer. Its copy and destructor exist as
// several identical template instantiations; each address the recompiler knows is registered.
//
// The strong release (0x08A5B970) and holder reset (0x08A5BAD8) stay recompiled: they call the
// object's dispose function and then carry on, and disposing can block in the kernel (seen with
// --decomp on), which a native frame can't survive. Where the original ends in a tail call (weak
// release -> delete, holder destroy -> strong release), the decompiled code does the same with
// gt::tail_call, so nothing native is left on the stack.

#include "decomp_registry.hpp"

namespace gt::core {
namespace {

constexpr std::uint32_t kRelease = 0x08A5B970u;  // strong release, recompiled (see above)
constexpr std::uint32_t kStrong = 4u;
constexpr std::uint32_t kWeak = 8u;
constexpr std::uint32_t kDeleteSlot = 1u;

std::uint32_t virtual_function(psprecomp::Runtime &rt, std::uint32_t object, std::uint32_t slot) {
    return rt.memory().load32(rt.memory().load32(object) + slot * 4u);
}

// Adds `delta` to the count at object + offset and returns the count before.
std::uint32_t bump(psprecomp::Runtime &rt, std::uint32_t object, std::uint32_t offset, std::uint32_t delta) {
    const std::uint32_t before = rt.memory().load32(object + offset);
    rt.memory().store32(object + offset, before + delta);
    return before;
}

void retain_weak(psprecomp::Runtime &rt, std::uint32_t object) { (void)bump(rt, object, kWeak, 1u); }

void retain(psprecomp::Runtime &rt, std::uint32_t object) {
    if (bump(rt, object, kStrong, 1u) == 0u) retain_weak(rt, object);
}

} // namespace

// Object entry points: a0 = object.
void object_retain_weak(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { retain_weak(rt, ctx.gpr[4]); }
void object_retain(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { retain(rt, ctx.gpr[4]); }

// The last weak release deletes the object: a tail call to its virtual delete.
void object_release_weak(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4];
    if (bump(rt, object, kWeak, ~0u) == 1u && object != 0u)
        tail_call(ctx, virtual_function(rt, object, kDeleteSlot), {object});
}

// holder_copy(holder *to, const holder *from): *to = *from, retaining the object.
void holder_copy(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = rt.memory().load32(ctx.gpr[5]);
    rt.memory().store32(ctx.gpr[4], object);
    if (object != 0u) retain(rt, object);
}

// holder_destroy(holder *h): releases the held object, if any (the word itself is left as is).
void holder_destroy(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = rt.memory().load32(ctx.gpr[4]);
    if (object != 0u) tail_call(ctx, kRelease, {object});
}

} // namespace gt::core

GT_DECOMP(0x08A5B930u, gt::core::object_release_weak, gt::ReturnKind::Void);
GT_DECOMP(0x08A5B9ECu, gt::core::holder_destroy, gt::ReturnKind::Void);
GT_DECOMP(0x08A5BA08u, gt::core::object_retain_weak, gt::ReturnKind::Void);
GT_DECOMP(0x08A5BA28u, gt::core::object_retain, gt::ReturnKind::Void);
GT_DECOMP(0x08A5BA78u, gt::core::holder_copy, gt::ReturnKind::Void);
GT_DECOMP(0x08A5BAB8u, gt::core::holder_copy, gt::ReturnKind::Void);
