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
// Disposing an object can block in the kernel (seen under --decomp on); the strong release and the
// holder reset call dispose and then carry on, which works because decompiled code runs on fibers
// (host/fiber.hpp). Where the original ends in a tail call (weak release -> delete, holder destroy ->
// strong release), the decompiled code does the same with gt::tail_call.

#include "decomp_registry.hpp"
#include "guest_call.hpp"

namespace gt::core {
namespace {

constexpr std::uint32_t kReleaseWeak = 0x08A5B930u;
constexpr std::uint32_t kRelease = 0x08A5B970u;  // strong release (object_release below)
constexpr std::uint32_t kStrong = 4u;
constexpr std::uint32_t kWeak = 8u;
constexpr std::uint32_t kDeleteSlot = 1u;
constexpr std::uint32_t kDisposeSlot = 2u;

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

// Strong release; the last one disposes of the contents, then drops the object's weak reference
// (the weak release at 0x08A5B930, called through the guest so its delete tail call works).
void release(psprecomp::Runtime &rt, const psprecomp::AllegrexContext &ctx, std::uint32_t object) {
    if (bump(rt, object, kStrong, ~0u) != 1u) return;
    (void)call_guest(rt, ctx, virtual_function(rt, object, kDisposeSlot), {object});
    (void)call_guest(rt, ctx, kReleaseWeak, {object});
}

} // namespace

// Object entry points: a0 = object.
void object_retain_weak(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { retain_weak(rt, ctx.gpr[4]); }
void object_retain(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { retain(rt, ctx.gpr[4]); }

void object_release(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4];
    if (bump(rt, object, kStrong, ~0u) != 1u) return;
    (void)call_guest(rt, ctx, virtual_function(rt, object, kDisposeSlot), {object});
    tail_call(ctx, kReleaseWeak, {object});  // the original's `j` into the weak release
}

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

// holder_reset(holder *h, object *p): retains p, releases the previous object, stores p.
void holder_reset(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t holder = ctx.gpr[4];
    const std::uint32_t object = ctx.gpr[5];
    if (object != 0u) retain(rt, object);
    const std::uint32_t previous = rt.memory().load32(holder);
    if (previous != 0u) release(rt, ctx, previous);
    rt.memory().store32(holder, object);
}

// holder_destroy(holder *h): releases the held object, if any (the word itself is left as is).
void holder_destroy(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = rt.memory().load32(ctx.gpr[4]);
    if (object != 0u) tail_call(ctx, kRelease, {object});
}

} // namespace gt::core

GT_DECOMP(0x08A5B930u, gt::core::object_release_weak, gt::ReturnKind::Void);
GT_DECOMP(0x08A5B970u, gt::core::object_release, gt::ReturnKind::Void);
GT_DECOMP(0x08A5B9ECu, gt::core::holder_destroy, gt::ReturnKind::Void);
GT_DECOMP(0x08A5BA08u, gt::core::object_retain_weak, gt::ReturnKind::Void);
GT_DECOMP(0x08A5BA28u, gt::core::object_retain, gt::ReturnKind::Void);
GT_DECOMP(0x08A5BA78u, gt::core::holder_copy, gt::ReturnKind::Void);
GT_DECOMP(0x08A5BAB8u, gt::core::holder_copy, gt::ReturnKind::Void);
GT_DECOMP(0x08A5BAD8u, gt::core::holder_reset, gt::ReturnKind::Void);
