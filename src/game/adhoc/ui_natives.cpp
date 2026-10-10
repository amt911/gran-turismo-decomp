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
constexpr std::uint32_t kSelfAsOptionMenu = 0x089B6204u;       // (holder *out, [self])
constexpr std::uint32_t kOptionMenuCount = 0x089B5828u;        // (option menu *) -> item count
constexpr std::uint32_t kOptionMenuCurrentItem = 0x089B5E18u;  // (holder *out, option menu *)
constexpr std::uint32_t kOptionMenuItem = 0x089B5DB4u;         // (holder *out, option menu *, index)
constexpr std::uint32_t kOptionMenuSetIndex = 0x089B7C80u;     // (option menu *, render context *, index)
constexpr std::uint32_t kSelfAsIconBox = 0x089E5E50u;          // (holder *out) [a1 = self]
constexpr std::uint32_t kIconBoxItemCount = 0x089E6B00u;       // (icon box *) -> count
constexpr std::uint32_t kIconBoxItemVisible = 0x089E7700u;     // (icon box *, index) -> bool
constexpr std::uint32_t kSelfAsListBox = 0x089CD62Cu;          // (holder *out) [a1 = self]
constexpr std::uint32_t kListBoxSetItemCount = 0x089D027Cu;    // (list box *, count)
constexpr std::uint32_t kCompositeChild = 0x08A00BF4u;         // (composite *, index) -> widget
constexpr std::uint32_t kRemoveChildSlot = 141u;               // composite->remove(context, child)
constexpr std::uint32_t kClearWindowSlot = 147u;               // composite->clear(context)
constexpr std::uint32_t kUpdateContextSync = 0x089C4558u;      // (frames): jumped to
constexpr std::uint32_t kListBoxItemWidget = 0x089CF584u;      // (holder *out, list box *, index, template)
constexpr std::uint32_t kIconBoxItemWidget = 0x089E7AF4u;      // (holder *out, icon box *, index, template)
constexpr std::uint32_t kRenderContextClosePage = 0x089AFC78u; // (render context *, root window *)

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

// self as `self_as` into a frame holder, `use(object)`, then drop the holder. The *As* helpers get
// self in a1, which call_guest leaves as the native's own.
template <typename Use>
void self_call(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t self_as, Use use) {
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t holder = frame.at(0u);
    (void)call_guest(rt, ctx, self_as, {holder});
    use(rt.memory().load32(holder));  // NULL is not checked (the originals only report it)
    destroy(rt, ctx, holder);
}

// Argument `index` as an integer (its toInt()).
std::int32_t int_argument(psprecomp::Runtime &rt, const psprecomp::AllegrexContext &ctx, std::uint32_t argv,
                          std::uint32_t index) {
    return to_int(rt, ctx, rt.memory().load32(argv + 4u * index));
}

// *result = the holder `make(out)` fills, dropping `out` afterwards (the getters returning objects).
template <typename Make>
void return_holder(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t result, Make make) {
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t out = frame.at(0u);
    make(out);
    if (result != out) (void)call_guest(rt, ctx, kHolderReset, {result, rt.memory().load32(out)});
    destroy(rt, ctx, out);
}

// MOptionMenu.Count() -> integer.
void MOptionMenu_Count(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    self_call(rt, ctx, kSelfAsOptionMenu, [&](std::uint32_t menu) {
        GuestFrame frame(ctx, 0x20u);
        return_integer(rt, ctx, result, frame.at(0u), call_guest(rt, ctx, kOptionMenuCount, {menu}));
    });
}

// MOptionMenu.getItem() -> the current item; getItem(index) -> that item.
void MOptionMenu_getItem(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    self_call(rt, ctx, kSelfAsOptionMenu, [&](std::uint32_t menu) {
        if (argc < 1) {
            return_holder(rt, ctx, result, [&](std::uint32_t out) { (void)call_guest(rt, ctx, kOptionMenuCurrentItem, {out, menu}); });
            return;
        }
        const std::int32_t index = int_argument(rt, ctx, argv, 0u);
        return_holder(rt, ctx, result, [&](std::uint32_t out) {
            (void)call_guest(rt, ctx, kOptionMenuItem, {out, menu, static_cast<std::uint32_t>(index)});
        });
    });
}

// MOptionMenu.setIndex(context, index): with at least two arguments; a nil context is passed as NULL.
void MOptionMenu_setIndex(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[5];
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    if (argc < 2) return;
    auto &memory = rt.memory();
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t menu = frame.at(0x0u);
    const std::uint32_t holder = frame.at(0x4u);
    std::uint32_t context = 0u;
    if (memory.load32(argv) != 0u) {
        (void)call_guest(rt, ctx, kSelfAsRenderContext, {holder, argv});
        context = memory.load32(holder);  // read before the holder drops its reference, as the original
        destroy(rt, ctx, holder);
    }
    (void)call_guest(rt, ctx, kSelfAsOptionMenu, {menu, self});
    const std::int32_t index = int_argument(rt, ctx, argv, 1u);
    (void)call_guest(rt, ctx, kOptionMenuSetIndex, {memory.load32(menu), context, static_cast<std::uint32_t>(index)});
    destroy(rt, ctx, menu);
}

// MIconBox.getItemCount() -> integer, with no arguments.
void MIconBox_getItemCount(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    if (ctx.gpr[6] != 0u) return;
    self_call(rt, ctx, kSelfAsIconBox, [&](std::uint32_t box) {
        GuestFrame frame(ctx, 0x20u);
        return_integer(rt, ctx, result, frame.at(0u), call_guest(rt, ctx, kIconBoxItemCount, {box}));
    });
}

// MIconBox.getItemVisible(index) -> boolean, with exactly one argument.
void MIconBox_getItemVisible(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t argv = ctx.gpr[7];
    if (ctx.gpr[6] != 1u) return;
    self_call(rt, ctx, kSelfAsIconBox, [&](std::uint32_t box) {
        const std::int32_t index = int_argument(rt, ctx, argv, 0u);
        const std::uint32_t visible = call_guest(rt, ctx, kIconBoxItemVisible, {box, static_cast<std::uint32_t>(index)});
        GuestFrame frame(ctx, 0x20u);
        return_bool(rt, ctx, result, frame.at(0u), visible);
    });
}

// MListBox.setItemCount(count), with exactly one argument.
void MListBox_setItemCount(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t argv = ctx.gpr[7];
    if (ctx.gpr[6] != 1u) return;
    self_call(rt, ctx, kSelfAsListBox, [&](std::uint32_t box) {
        const std::int32_t count = int_argument(rt, ctx, argv, 0u);
        (void)call_guest(rt, ctx, kListBoxSetItemCount, {box, static_cast<std::uint32_t>(count)});
    });
}

// MComposite.getChild(index) -> the child widget, with at least one argument.
void MComposite_getChild(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    if (argc <= 0) return;
    self_call(rt, ctx, kSelfAsComposite, [&](std::uint32_t composite) {
        const std::int32_t index = int_argument(rt, ctx, argv, 0u);
        GuestFrame frame(ctx, 0x20u);
        const std::uint32_t raw = frame.at(0x0u);
        rt.memory().store32(raw, call_guest(rt, ctx, kCompositeChild, {composite, static_cast<std::uint32_t>(index)}));
        return_holder(rt, ctx, result, [&](std::uint32_t out) { (void)call_guest(rt, ctx, kHolderCopy, {out, raw}); });
    });
}

// MComposite.removeChild(child) / removeChild(context, child).
void MComposite_removeChild(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t argc = ctx.gpr[6];
    const std::uint32_t argv = ctx.gpr[7];
    auto &memory = rt.memory();
    self_call(rt, ctx, kSelfAsComposite, [&](std::uint32_t composite) {
        GuestFrame frame(ctx, 0x20u);
        const std::uint32_t context = frame.at(0x0u);
        const std::uint32_t child = frame.at(0x4u);
        if (argc == 2u) {
            (void)call_guest(rt, ctx, kSelfAsRenderContext, {context, argv});
            (void)call_guest(rt, ctx, kArgumentAsWidget, {child, argv + 4u});
            (void)call_guest(rt, ctx, virtual_function(rt, composite, kRemoveChildSlot),
                             {composite, memory.load32(context), memory.load32(child)});
            destroy(rt, ctx, child);
            destroy(rt, ctx, context);
        } else if (argc == 1u) {
            (void)call_guest(rt, ctx, kArgumentAsWidget, {child, argv});
            (void)call_guest(rt, ctx, virtual_function(rt, composite, kRemoveChildSlot), {composite, 0u, memory.load32(child)});
            destroy(rt, ctx, child);
        }
    });
}

// MComposite.clearWindow(context), with at least one argument.
void MComposite_clearWindow(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    if (argc <= 0) return;
    self_call(rt, ctx, kSelfAsComposite, [&](std::uint32_t composite) {
        GuestFrame frame(ctx, 0x20u);
        const std::uint32_t context = frame.at(0x0u);
        (void)call_guest(rt, ctx, kSelfAsRenderContext, {context, argv});
        (void)call_guest(rt, ctx, virtual_function(rt, composite, kClearWindowSlot), {composite, rt.memory().load32(context)});
        destroy(rt, ctx, context);
    });
}

// Allegrex trunc.w.s: toward zero; NaN and out-of-range values give 0x7FFFFFFF (portable: no C++ UB).
std::int32_t trunc_w_s(float value) {
    if (!(value > -2147483648.0f && value < 2147483648.0f)) return 0x7FFFFFFF;
    return static_cast<std::int32_t>(value);
}

// MUpdateContext.Sync(seconds = 1/60): waits that many frames (seconds * 60). Module function
// (result, argc, argv); the original jumps to the wait.
void MUpdateContext_Sync(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const auto argc = static_cast<std::int32_t>(ctx.gpr[5]);
    const std::uint32_t argv = ctx.gpr[6];
    std::int32_t frames = 1;
    if (argc > 0) frames = trunc_w_s(to_float(rt, ctx, rt.memory().load32(argv)) * 60.0f);
    tail_call(ctx, kUpdateContextSync, {static_cast<std::uint32_t>(frames)});
}

// MNode.doInitialize(context), with at least one argument: the node's virtual initialize(context).
void MNode_doInitialize(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t kInitializeSlot = 88u;
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    if (argc <= 0) return;
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t node = frame.at(0x0u);
    const std::uint32_t context = frame.at(0x4u);
    (void)call_guest(rt, ctx, kSelfAsNode, {node});
    (void)call_guest(rt, ctx, kSelfAsRenderContext, {context, argv});
    const std::uint32_t object = rt.memory().load32(node);
    (void)call_guest(rt, ctx, virtual_function(rt, object, kInitializeSlot), {object, rt.memory().load32(context)});
    destroy(rt, ctx, context);
    destroy(rt, ctx, node);
}

// MListBox/MIconBox.getItemWidget(index[, template]) -> the item's widget (made from the template,
// if one is given).
void get_item_widget(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t self_as, std::uint32_t item_widget) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t argc = ctx.gpr[6];
    const std::uint32_t argv = ctx.gpr[7];
    if (argc != 1u && argc != 2u) return;
    self_call(rt, ctx, self_as, [&](std::uint32_t box) {
        GuestFrame frame(ctx, 0x10u);
        const std::uint32_t raw = frame.at(0x0u);
        const std::uint32_t item_template = frame.at(0x4u);
        std::uint32_t template_object = 0u;
        if (argc == 2u) {
            rt.memory().store32(raw, rt.memory().load32(argv + 4u));
            (void)call_guest(rt, ctx, kHolderCopyRenderContext, {item_template, raw});
            template_object = rt.memory().load32(item_template);
        }
        const std::int32_t index = int_argument(rt, ctx, argv, 0u);
        return_holder(rt, ctx, result, [&](std::uint32_t out) {
            (void)call_guest(rt, ctx, item_widget, {out, box, static_cast<std::uint32_t>(index), template_object});
        });
        if (argc == 2u) destroy(rt, ctx, item_template);
    });
}
void MListBox_getItemWidget(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { get_item_widget(rt, ctx, kSelfAsListBox, kListBoxItemWidget); }
void MIconBox_getItemWidget(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { get_item_widget(rt, ctx, kSelfAsIconBox, kIconBoxItemWidget); }

// MRenderContext.closePage(page), with exactly one argument.
void MRenderContext_closePage(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[5];
    const std::uint32_t argv = ctx.gpr[7];
    if (ctx.gpr[6] != 1u) return;
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t page = frame.at(0x0u);
    const std::uint32_t context = frame.at(0x4u);
    (void)call_guest(rt, ctx, kSelfAsRootWindow, {page, argv});
    (void)call_guest(rt, ctx, kSelfAsRenderContext, {context, self});
    (void)call_guest(rt, ctx, kRenderContextClosePage, {rt.memory().load32(context), rt.memory().load32(page)});
    destroy(rt, ctx, context);
    destroy(rt, ctx, page);
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
GT_DECOMP(0x089B6E00u, gt::adhoc::ui::MOptionMenu_Count, gt::ReturnKind::Void);
GT_DECOMP(0x089B6CE8u, gt::adhoc::ui::MOptionMenu_getItem, gt::ReturnKind::Void);
GT_DECOMP(0x089B7FD8u, gt::adhoc::ui::MOptionMenu_setIndex, gt::ReturnKind::Void);
GT_DECOMP(0x089E98B4u, gt::adhoc::ui::MIconBox_getItemCount, gt::ReturnKind::Void);
GT_DECOMP(0x089E9AC8u, gt::adhoc::ui::MIconBox_getItemVisible, gt::ReturnKind::Void);
// Not comparable yet (calls a kernel import that is not replay-safe), so not swapped in: GT_DECOMP(0x089D02B8u, gt::adhoc::ui::MListBox_setItemCount, gt::ReturnKind::Void);
GT_DECOMP(0x08A0235Cu, gt::adhoc::ui::MComposite_getChild, gt::ReturnKind::Void);
GT_DECOMP(0x08A01558u, gt::adhoc::ui::MComposite_removeChild, gt::ReturnKind::Void);
GT_DECOMP(0x08A019A0u, gt::adhoc::ui::MComposite_clearWindow, gt::ReturnKind::Void);
// Not comparable yet (calls a kernel import that is not replay-safe), so not swapped in: GT_DECOMP(0x08978080u, gt::adhoc::ui::MUpdateContext_Sync, gt::ReturnKind::Void);
// Not comparable yet (calls a kernel import that is not replay-safe), so not swapped in: GT_DECOMP(0x089AB5D8u, gt::adhoc::ui::MRenderContext_finish, gt::ReturnKind::Void);
GT_DECOMP(0x089BB440u, gt::adhoc::ui::MNode_doInitialize, gt::ReturnKind::Void);
GT_DECOMP(0x089D0F94u, gt::adhoc::ui::MListBox_getItemWidget, gt::ReturnKind::Void);
GT_DECOMP(0x089E9944u, gt::adhoc::ui::MIconBox_getItemWidget, gt::ReturnKind::Void);
// Not comparable yet (sleeps the thread: sceKernelSleepThread), so not swapped in: GT_DECOMP(0x089AFE00u, gt::adhoc::ui::MRenderContext_closePage, gt::ReturnKind::Void);
