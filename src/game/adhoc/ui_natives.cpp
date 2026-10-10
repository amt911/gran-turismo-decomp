// Adhoc natives of GT's UI classes (names from GT's registrations, config/symbol_addrs.txt). Each one
// fetches `self` as its engine class (the *As* helpers take the native's self in a1, which these
// calls leave untouched), converts the arguments, calls the engine and drops its holders, in the
// original's order.

#include "decomp_registry.hpp"
#include "value.hpp"

namespace gt::adhoc::ui {
namespace {

constexpr std::uint32_t kHolderCopy = 0x08969188u;      // thunk to holder_copy(holder *to, const holder *from)
constexpr std::uint32_t kHolderReset = 0x08A5BAD8u;
constexpr std::uint32_t kHolderDestroy = 0x08A5B9ECu;

constexpr std::uint32_t kSelfAsRootWindow = 0x089A354Cu;    // (holder *out) [a1 = self]
constexpr std::uint32_t kRootWindowSetFocus = 0x089A301Cu;  // (root window *, widget *)
constexpr std::uint32_t kSelfAsNode = 0x089BB3A8u;          // (holder *out) [a1 = self]
constexpr std::uint32_t kNodeCopy = 0x089BB6C0u;            // (holder *out, node *, a2, a3)
constexpr std::uint32_t kSelfAsRenderContext = 0x089AB370u; // (holder *out, [self or argument])
constexpr std::uint32_t kArgumentAsEvent = 0x089FDB7Cu;     // (holder *out, argument)
constexpr std::uint32_t kPushEvent = 0x0897783Cu;           // (event queue *, event holder *)
constexpr std::uint32_t kSelfAsComposite = 0x08A01338u;     // (holder *out) [a1 = self]
constexpr std::uint32_t kArgumentAsWidget = 0x08969244u;    // (holder *out, argument)
constexpr std::uint32_t kAppendChildSlot = 142u;            // composite->append(context, child)
constexpr std::uint32_t kRenderContextEvents = 0x14u;       // render context +0x14: its update context (event queue)
constexpr std::uint32_t kSelfAsUpdateContext = 0x08978000u; // (holder *out) [a1 = self]
constexpr std::uint32_t kSelfAsTextFace = 0x089896CCu;      // (holder *out) [a1 = self]
constexpr std::uint32_t kTextFaceAdjustScale = 0x0898930Cu;
constexpr std::uint32_t kSelfAsToolTipFace = 0x0897E844u;
constexpr std::uint32_t kToolTipFaceRemove = 0x0897E3D0u;
constexpr std::uint32_t kSelfAsMovieFace = 0x089BEE4Cu;
constexpr std::uint32_t kMovieFaceUnload = 0x089C10C8u;
constexpr std::uint32_t kRootWindowFocusedWidget = 0x089A3130u;  // (root window *) -> widget
constexpr std::uint32_t kRenderContextFinish = 0x0897BB68u;      // (update context *)
constexpr std::uint32_t kCompositeChildCount = 0xF0u;
constexpr std::uint32_t kUpdateContextRenderContext = 0x08977260u;  // (update context *) -> render context
constexpr std::uint32_t kHolderCopyRenderContext = 0x089AB244u;     // holder copy instances
constexpr std::uint32_t kHolderCopyUpdateContext = 0x08977AE4u;

void destroy(psprecomp::Runtime &rt, const psprecomp::AllegrexContext &ctx, std::uint32_t holder) {
    (void)call_guest(rt, ctx, kHolderDestroy, {holder});
}

} // namespace

// MRootWindow.setFocus(widget): with at least one argument, focuses argv[0].
void MRootWindow_setFocus(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    if (argc <= 0) return;
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t root = frame.at(0x4u);
    const std::uint32_t widget = frame.at(0x8u);
    (void)call_guest(rt, ctx, kSelfAsRootWindow, {root});
    rt.memory().store32(frame.at(0x0u), rt.memory().load32(argv));
    (void)call_guest(rt, ctx, kHolderCopy, {widget, frame.at(0x0u)});
    (void)call_guest(rt, ctx, kRootWindowSetFocus, {rt.memory().load32(root), rt.memory().load32(widget)});
    destroy(rt, ctx, widget);
    destroy(rt, ctx, root);
}

// MNode.doCopy(a2, a3) -> the copy.
void MNode_doCopy(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t a2 = ctx.gpr[6];
    const std::uint32_t a3 = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t node = frame.at(0x0u);
    const std::uint32_t copy = frame.at(0x4u);
    (void)call_guest(rt, ctx, kSelfAsNode, {node});
    (void)call_guest(rt, ctx, kNodeCopy, {copy, rt.memory().load32(node), a2, a3});
    if (result != copy) (void)call_guest(rt, ctx, kHolderReset, {result, rt.memory().load32(copy)});
    destroy(rt, ctx, copy);
    destroy(rt, ctx, node);
}

// MRenderContext.pushEvent(event): with exactly one argument, queues it on the context.
void MRenderContext_pushEvent(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t argc = ctx.gpr[6];
    const std::uint32_t argument = ctx.gpr[7];
    if (argc != 1u) return;
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t context = frame.at(0x4u);
    const std::uint32_t event = frame.at(0x0u);
    (void)call_guest(rt, ctx, kSelfAsRenderContext, {context});
    const std::uint32_t events = rt.memory().load32(rt.memory().load32(context) + kRenderContextEvents);
    (void)call_guest(rt, ctx, kArgumentAsEvent, {event, argument});
    (void)call_guest(rt, ctx, kPushEvent, {events, event});
    destroy(rt, ctx, event);
    destroy(rt, ctx, context);
}

// MComposite.appendChild(child) / appendChild(context, child).
void MComposite_appendChild(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t argc = ctx.gpr[6];
    const std::uint32_t argv = ctx.gpr[7];
    auto &memory = rt.memory();
    if (argc == 1u) {
        GuestFrame frame(ctx, 0x20u);
        const std::uint32_t composite = frame.at(0x4u);
        const std::uint32_t child = frame.at(0x0u);
        (void)call_guest(rt, ctx, kSelfAsComposite, {composite});
        (void)call_guest(rt, ctx, kArgumentAsWidget, {child, argv});
        const std::uint32_t object = memory.load32(composite);
        (void)call_guest(rt, ctx, virtual_function(rt, object, kAppendChildSlot), {object, 0u, memory.load32(child)});
        destroy(rt, ctx, child);
        destroy(rt, ctx, composite);
    } else if (argc == 2u) {
        GuestFrame frame(ctx, 0x20u);
        const std::uint32_t composite = frame.at(0x0u);
        const std::uint32_t context = frame.at(0x4u);
        const std::uint32_t child = frame.at(0x8u);
        (void)call_guest(rt, ctx, kSelfAsComposite, {composite});
        (void)call_guest(rt, ctx, kSelfAsRenderContext, {context, argv});
        (void)call_guest(rt, ctx, kArgumentAsWidget, {child, argv + 4u});
        const std::uint32_t object = memory.load32(composite);
        (void)call_guest(rt, ctx, virtual_function(rt, object, kAppendChildSlot),
                         {object, memory.load32(context), memory.load32(child)});
        destroy(rt, ctx, child);
        destroy(rt, ctx, context);
        destroy(rt, ctx, composite);
    }
}

// MUpdateContext.getRenderContext() -> the update context's render context.
void MUpdateContext_getRenderContext(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t raw = frame.at(0x0u);
    const std::uint32_t value = frame.at(0x4u);
    const std::uint32_t update = frame.at(0x8u);
    (void)call_guest(rt, ctx, kSelfAsUpdateContext, {update});
    rt.memory().store32(raw, call_guest(rt, ctx, kUpdateContextRenderContext, {rt.memory().load32(update)}));
    (void)call_guest(rt, ctx, kHolderCopyRenderContext, {value, raw});
    if (result != value) (void)call_guest(rt, ctx, kHolderReset, {result, rt.memory().load32(value)});
    destroy(rt, ctx, value);
    destroy(rt, ctx, update);
}

// MRenderContext.getUpdateContext() -> the render context's update context (+0x14).
void MRenderContext_getUpdateContext(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t context = frame.at(0x0u);
    const std::uint32_t raw = frame.at(0x4u);
    const std::uint32_t value = frame.at(0x8u);
    (void)call_guest(rt, ctx, kSelfAsRenderContext, {context});
    rt.memory().store32(raw, rt.memory().load32(rt.memory().load32(context) + kRenderContextEvents));
    (void)call_guest(rt, ctx, kHolderCopyUpdateContext, {value, raw});
    if (result != value) (void)call_guest(rt, ctx, kHolderReset, {result, rt.memory().load32(value)});
    destroy(rt, ctx, value);
    destroy(rt, ctx, context);
}

// self as `self_as`, then engine(object), then the holder is dropped.
void object_call(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t self_as, std::uint32_t engine) {
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t holder = frame.at(0u);
    (void)call_guest(rt, ctx, self_as, {holder});
    (void)call_guest(rt, ctx, engine, {rt.memory().load32(holder)});  // NULL is not checked
    destroy(rt, ctx, holder);
}
void MTextFace_adjustScale(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { object_call(rt, ctx, kSelfAsTextFace, kTextFaceAdjustScale); }
void MToolTipFace_remove(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { object_call(rt, ctx, kSelfAsToolTipFace, kToolTipFaceRemove); }
void MMovieFace_unload(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { object_call(rt, ctx, kSelfAsMovieFace, kMovieFaceUnload); }

// MRootWindow.getFocusedWidget() -> the focused widget.
void MRootWindow_getFocusedWidget(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t raw = frame.at(0x0u);
    const std::uint32_t value = frame.at(0x4u);
    const std::uint32_t root = frame.at(0x8u);
    (void)call_guest(rt, ctx, kSelfAsRootWindow, {root});
    rt.memory().store32(raw, call_guest(rt, ctx, kRootWindowFocusedWidget, {rt.memory().load32(root)}));
    (void)call_guest(rt, ctx, kHolderCopy, {value, raw});
    if (result != value) (void)call_guest(rt, ctx, kHolderReset, {result, rt.memory().load32(value)});
    destroy(rt, ctx, value);
    destroy(rt, ctx, root);
}

// MComposite.countChild() -> integer (the child count at +0xF0).
void MComposite_countChild(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t composite = frame.at(0x4u);
    (void)call_guest(rt, ctx, kSelfAsComposite, {composite});
    const std::uint32_t count = rt.memory().load32(rt.memory().load32(composite) + kCompositeChildCount);
    return_integer(rt, ctx, result, frame.at(0x0u), count);
    destroy(rt, ctx, composite);
}

// MRenderContext.finish(): with no arguments, finishes the context's update context.
void MRenderContext_finish(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    if (ctx.gpr[6] != 0u) return;
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t context = frame.at(0u);
    (void)call_guest(rt, ctx, kSelfAsRenderContext, {context});
    (void)call_guest(rt, ctx, kRenderContextFinish, {rt.memory().load32(rt.memory().load32(context) + kRenderContextEvents)});
    destroy(rt, ctx, context);
}

} // namespace gt::adhoc::ui

GT_DECOMP(0x089A358Cu, gt::adhoc::ui::MRootWindow_setFocus, gt::ReturnKind::Void);
GT_DECOMP(0x089BBE38u, gt::adhoc::ui::MNode_doCopy, gt::ReturnKind::Void);
GT_DECOMP(0x089AB54Cu, gt::adhoc::ui::MRenderContext_pushEvent, gt::ReturnKind::Void);
GT_DECOMP(0x08A01668u, gt::adhoc::ui::MComposite_appendChild, gt::ReturnKind::Void);
GT_DECOMP(0x08979514u, gt::adhoc::ui::MUpdateContext_getRenderContext, gt::ReturnKind::Void);
GT_DECOMP(0x089AD804u, gt::adhoc::ui::MRenderContext_getUpdateContext, gt::ReturnKind::Void);
GT_DECOMP(0x08989890u, gt::adhoc::ui::MTextFace_adjustScale, gt::ReturnKind::Void);
GT_DECOMP(0x0897E994u, gt::adhoc::ui::MToolTipFace_remove, gt::ReturnKind::Void);
// Not comparable yet (calls a kernel import that is not replay-safe), so not swapped in: GT_DECOMP(0x089C11D0u, gt::adhoc::ui::MMovieFace_unload, gt::ReturnKind::Void);
GT_DECOMP(0x089A3F54u, gt::adhoc::ui::MRootWindow_getFocusedWidget, gt::ReturnKind::Void);
GT_DECOMP(0x08A022DCu, gt::adhoc::ui::MComposite_countChild, gt::ReturnKind::Void);
// Not comparable yet (calls a kernel import that is not replay-safe), so not swapped in: GT_DECOMP(0x089AB5D8u, gt::adhoc::ui::MRenderContext_finish, gt::ReturnKind::Void);
