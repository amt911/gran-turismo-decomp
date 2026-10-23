// Hot batch 132: functions of 500+ instructions the 11-route gate reaches, written by hand from `tools.disasm`;
// role words describe access shapes, none is a game name.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>
#include <optional>

namespace gt::game {
namespace hot_batch_132 {
namespace {

using namespace gt::game::hot;

inline void take(psprecomp::AllegrexContext &ctx, const GuestResult &result) {
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}
// A guest call whose results stay in the registers like the original's: v0 / v1 / f0 of the callee are what the next instruction sees.
inline GuestResult call_keep(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments,
                             std::initializer_list<float> float_arguments) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, float_arguments);
    take(ctx, result);
    return result;
}
// A guest call with the argument registers set like the original sets them before the jal (a0-a3, t0-t3 in order): what the callee does not take stays as the caller had it.
inline std::uint32_t call_set(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments) {
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments, {}).v0;
}
// The same with float arguments in f12-f15 as well.
inline GuestResult call_setf(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments,
                             std::initializer_list<float> float_arguments) {
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    unsigned freg = 12u;
    for (const float argument : float_arguments) {
        if (freg <= 15u) ctx.fpr[freg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments, float_arguments);
}
// A virtual call `object->slot(...)`: `lw v1, 0(object); lw v0, slot(v1); jalr v0`, both registers set like the original sets them.
inline std::uint32_t call_virtual_at(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object, std::uint32_t slot,
                                     std::initializer_list<std::uint32_t> arguments) {
    ctx.gpr[3] = word(rt, object);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + slot);
    return call_set(rt, ctx, ctx.gpr[2], arguments);
}
inline std::uint32_t byte_u(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load8(address); }
inline std::uint32_t half_u(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load16(address); }
inline void put8(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
inline void put16(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
inline std::uint32_t sext8(std::uint32_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(value))); }
inline std::uint32_t sext16(std::uint32_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(value))); }
inline std::int32_t signed_of(std::uint32_t value) { return static_cast<std::int32_t>(value); }
// The Allegrex div's quotient: -1 (numerator >= 0) or 1 when dividing by zero, 0x80000000 for 0x80000000 / -1.
inline std::int32_t allegrex_div(std::int32_t numerator, std::int32_t denominator) {
    if (denominator == 0) return numerator >= 0 ? -1 : 1;
    if (numerator == INT32_MIN && denominator == -1) return INT32_MIN;
    return numerator / denominator;
}

// The lock pair several of these wrap their body in: func_08B344E0(lock) before, a tail jump to func_08B3457C(lock) after.
constexpr std::uint32_t kLock = 0x08B344E0u;
constexpr std::uint32_t kUnlock = 0x08B3457Cu;
// The value-object pair of the 0x08A5xxxx library: func_08A5BAD8(dst, src->+0) assigns, func_08A5B9EC(object) destroys.
constexpr std::uint32_t kAssign = 0x08A5BAD8u;
constexpr std::uint32_t kDestroy = 0x08A5B9ECu;

// The end of a function that built a value object in its frame at `temporary`: assigned to `target` (skipped when the
// target is that temporary itself), then the temporary is destroyed.
inline void keep_temporary(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t target, std::uint32_t temporary) {
    if (target != temporary) call_set(rt, ctx, kAssign, {target, word(rt, temporary)});
    call_set(rt, ctx, kDestroy, {temporary});
}

// Releases the shared string whose data pointer sits at `slot` (the libstdc++ copy-on-write layout: the rep 0xC bytes
// before the data, its count at -4): unless it is the empty rep `empty`, func_08B60804(&count, -1) steps the count and
// `release`(rep, frame) frees it when the old count was not positive.
inline void release_string(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t slot, std::uint32_t empty,
                           std::uint32_t release, std::uint32_t frame_base) {
    const std::uint32_t text = word(rt, slot);
    const std::uint32_t rep = text - 0xCu;
    ctx.gpr[4] = text - 4u;
    ctx.gpr[5] = 0xFFFFFFFFu;
    if (rep == empty) return;
    if (signed_of(call_set(rt, ctx, 0x08B60804u, {text - 4u, 0xFFFFFFFFu})) > 0) return;
    call_set(rt, ctx, release, {rep, frame_base});
}

// A script class's method table: func_08B86FB0(*holder, name, method) for each pair, *holder read again each time (an
// empty one first reported through func_08A5BB7C(report)); with `tail` the last registration is the function's tail jump.
inline void register_methods(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t holder,
                             std::uint32_t report, const std::uint32_t (*methods)[2], std::size_t count, bool tail) {
    for (std::size_t index = 0u; index < count; ++index) {
        const std::uint32_t object = word(rt, holder);
        if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {report});
        if (tail && index + 1u == count) return tail_call(ctx, 0x08B86FB0u, {object, methods[index][0], methods[index][1]});
        (void)call_set(rt, ctx, 0x08B86FB0u, {object, methods[index][0], methods[index][1]});
    }
}

// A script class definition (func_08822920's shape): the class object is made at begin (symbol, func_08A5BC20(0x5C, 4),
// func_08A54F3C(object, symbol, size), held at `handle` through func_08A52AB0), each attribute's value is built as a
// chain (a typed object or a plain value, then up to two 0x44-byte boxes), added by func_08A535A4(class, &name, &value,
// a3, flags) and its holders released last to first; finish hands the class to func_08A7CE50(owner, &class), keeps it
// in a global and releases the handle. An empty class / owner is reported through func_08A5BB7C first, as the original.
class ClassBuilder {
public:
    struct Field {
        std::uint32_t offset;
        std::uint32_t value;
    };
    ClassBuilder(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, const GuestFrame &frame, std::uint32_t handle,
                 std::uint32_t report)
        : rt_(rt), ctx_(ctx), frame_(frame), handle_(handle), report_(report) {}
    void begin(std::uint32_t symbol_slot, std::uint32_t symbol, std::uint32_t size, std::uint32_t object_slot) {
        (void)call_set(rt_, ctx_, 0x08A69B3Cu, {sp(symbol_slot), symbol});
        const std::uint32_t object = call_set(rt_, ctx_, 0x08A5BC20u, {0x5Cu, 4u});
        (void)call_set(rt_, ctx_, 0x08A54F3Cu, {object, sp(symbol_slot), size});
        put(rt_, sp(object_slot), object);
        (void)call_set(rt_, ctx_, 0x08A52AB0u, {handle_, sp(object_slot)});
        (void)call_set(rt_, ctx_, 0x08A69D68u, {sp(symbol_slot)});
    }
    void typed(std::uint32_t size, std::uint32_t type_slot, std::uint32_t type, std::initializer_list<Field> fields,
               std::uint32_t object_slot, std::uint32_t holder_slot) {
        const std::uint32_t object = call_set(rt_, ctx_, 0x08A5BC20u, {size, 4u});
        (void)call_set(rt_, ctx_, 0x08A69B3Cu, {sp(type_slot), type});
        (void)call_set(rt_, ctx_, 0x08A52660u, {object, sp(type_slot)});
        (void)call_set(rt_, ctx_, 0x08A69D68u, {sp(type_slot)});
        for (const Field &field : fields) put(rt_, object + field.offset, field.value);
        put(rt_, sp(object_slot), object);
        hold(holder_slot, object_slot);
    }
    void plain(std::uint32_t maker, std::uint32_t slot) {
        (void)call_set(rt_, ctx_, maker, {sp(slot)});
        held_.push_back(slot);
    }
    void box(std::uint32_t inner_slot, std::uint32_t n, std::uint32_t object_slot, std::uint32_t holder_slot) {
        const std::uint32_t object = call_set(rt_, ctx_, 0x08A5BC20u, {0x44u, 4u});
        (void)call_set(rt_, ctx_, 0x08A5B70Cu, {object, sp(inner_slot), n});
        put(rt_, sp(object_slot), object);
        hold(holder_slot, object_slot);
    }
    template <typename Build>
    void define(std::uint32_t name_slot, std::uint32_t name, std::uint32_t value_slot, std::uint32_t a3, std::uint32_t flags,
                const Build &build) {
        const std::uint32_t target = word(rt_, handle_);
        if (target == 0u) (void)call_set(rt_, ctx_, 0x08A5BB7Cu, {report_});
        (void)call_set(rt_, ctx_, 0x08A69B3Cu, {sp(name_slot), name});
        held_.clear();
        build();
        (void)call_set(rt_, ctx_, 0x08A535A4u, {target, sp(name_slot), sp(value_slot), a3, flags});
        for (auto slot = held_.rbegin(); slot != held_.rend(); ++slot) (void)call_set(rt_, ctx_, 0x08A5B9ECu, {sp(*slot)});
        (void)call_set(rt_, ctx_, 0x08A69D68u, {sp(name_slot)});
    }
    void finish(std::uint32_t owner, std::uint32_t report, std::uint32_t global) {
        if (owner == 0u) (void)call_set(rt_, ctx_, 0x08A5BB7Cu, {report});
        (void)call_set(rt_, ctx_, 0x08A7CE50u, {owner, handle_});
        put(rt_, global, word(rt_, handle_));
        (void)call_set(rt_, ctx_, 0x08A5B9ECu, {handle_});
    }

private:
    std::uint32_t sp(std::uint32_t offset) const { return frame_.at(offset); }
    void hold(std::uint32_t holder_slot, std::uint32_t object_slot) {
        (void)call_set(rt_, ctx_, 0x08A52AB0u, {sp(holder_slot), sp(object_slot)});
        held_.push_back(holder_slot);
    }
    psprecomp::Runtime &rt_;
    psprecomp::AllegrexContext &ctx_;
    const GuestFrame &frame_;
    std::uint32_t handle_;
    std::uint32_t report_;
    std::vector<std::uint32_t> held_;
};

} // namespace

// func_0883A4D8(holder) -> holder: makes a script class (name 0x08C9BD74; a 0x38-byte object from func_08A5BC20(0x38,
// 4) built by func_08A7E914 and handed to func_08A76AB4(holder, &object)), adds two integer constants through
// func_08B8D810(class, name, &value) with the values made by func_08A5DEE8(value, -, low, high) (0x08C9BD84 = -1 as 64
// bits, 0x08C9BD90 = 0xFFFFFFFF) and destroyed by func_08A5B9EC, then 86 methods (report 0x08C9BD7C for an empty holder).
void func_0883A4D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    static constexpr std::uint32_t kMethods[][2] = {
        {0x08C9BD9Cu, 0x0883BA58u}, {0x08C9BDACu, 0x0883BB48u}, {0x08C9BDB8u, 0x0883BAA0u},
        {0x08C9BDC8u, 0x0883B600u}, {0x08C9BDD4u, 0x0883A3D4u}, {0x08C9BDE0u, 0x0883BB54u},
        {0x08C9BDECu, 0x0883F9A8u}, {0x08C9BDFCu, 0x0883FB1Cu}, {0x08C9BE18u, 0x0883BBD8u},
        {0x08C9BE24u, 0x0883BD04u}, {0x08C9BE38u, 0x0883BD4Cu}, {0x08C9BE4Cu, 0x0883BE74u},
        {0x08C9BE5Cu, 0x0883BF90u}, {0x08C9BE68u, 0x0883BFE8u}, {0x08C9BE7Cu, 0x0883C0BCu},
        {0x08C9BE88u, 0x0883C1C8u}, {0x08C9BE98u, 0x0883C2FCu}, {0x08C9BEA8u, 0x0883C390u},
        {0x08C9BEB8u, 0x0883C4B4u}, {0x08C9BEC8u, 0x0883C5BCu}, {0x08C9BEDCu, 0x0883C758u},
        {0x08C9BEF0u, 0x0883C7ECu}, {0x08C9BF00u, 0x0883C988u}, {0x08C9BF14u, 0x0883CB34u},
        {0x08C9BF24u, 0x0883CCC8u}, {0x08C9BF38u, 0x0883CE74u}, {0x08C9BF48u, 0x0883CFFCu},
        {0x08C9BF5Cu, 0x0883D184u}, {0x08C9BF70u, 0x0883D30Cu}, {0x08C9BF84u, 0x0883D494u},
        {0x08C9BFA0u, 0x0883D648u}, {0x08C9BFB0u, 0x0883D73Cu}, {0x08C9BFC4u, 0x0883D800u},
        {0x08C9BFD4u, 0x0883D8C8u}, {0x08C9BFE0u, 0x0883D928u}, {0x08C9BFECu, 0x0883D988u},
        {0x08C9BFFCu, 0x0883D9E8u}, {0x08C9C00Cu, 0x0883DA48u}, {0x08C9C024u, 0x0883DACCu},
        {0x08C9C038u, 0x0883DB94u}, {0x08C9C04Cu, 0x0883DC54u}, {0x08C9C064u, 0x0883DCD8u},
        {0x08C9C074u, 0x0883DDB8u}, {0x08C9C084u, 0x0883DE98u}, {0x08C9C094u, 0x0883DF78u},
        {0x08C9C0A4u, 0x0883E058u}, {0x08C9C0B8u, 0x0883E228u}, {0x08C9C0CCu, 0x0883E3E0u},
        {0x08C9C0E0u, 0x0883E534u}, {0x08C9C0F0u, 0x0883E6B0u}, {0x08C9C104u, 0x0883E7CCu},
        {0x08C9C114u, 0x0883E924u}, {0x08C9C130u, 0x0883E9A8u}, {0x08C9C13Cu, 0x0883EB00u},
        {0x08C9C14Cu, 0x0883EC3Cu}, {0x08C9C158u, 0x0883F654u}, {0x08C9C16Cu, 0x0883ED54u},
        {0x08C9C180u, 0x0883F10Cu}, {0x08C9C194u, 0x0883F1FCu}, {0x08C9C1B0u, 0x0883F2E8u},
        {0x08C9C1C4u, 0x0883F40Cu}, {0x08C9C1D8u, 0x0883F73Cu}, {0x08C9C1E8u, 0x0883F8E8u},
        {0x08C9C1FCu, 0x0883F948u}, {0x08C9C210u, 0x0883FD7Cu}, {0x08C9C22Cu, 0x0883A45Cu},
        {0x08C9C23Cu, 0x0883A49Cu}, {0x08C9C24Cu, 0x0883FF50u}, {0x08C9C260u, 0x088400B4u},
        {0x08C9C278u, 0x08840278u}, {0x08C9C288u, 0x088402C8u}, {0x08C9C294u, 0x08840390u},
        {0x08C9C2A8u, 0x088404F4u}, {0x08C9C2C0u, 0x088406B8u}, {0x08C9C2D0u, 0x08840780u},
        {0x08C9C2E0u, 0x088407D0u}, {0x08C9C2ECu, 0x0884085Cu}, {0x08C9C300u, 0x08840A0Cu},
        {0x08C9C318u, 0x08840C7Cu}, {0x08C9C32Cu, 0x08840D00u}, {0x08C9C338u, 0x0884100Cu},
        {0x08C9C34Cu, 0x0884125Cu}, {0x08C9C360u, 0x088414ACu}, {0x08C9C378u, 0x08840D84u},
        {0x08C9C390u, 0x08840F04u}, {0x08C9C398u, 0x08840FA8u},
    };
    const std::uint32_t holder = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    (void)call_set(rt, ctx, 0x08A69B3Cu, {frame.at(0u), 0x08C9BD74u});
    const std::uint32_t object = call_set(rt, ctx, 0x08A5BC20u, {0x38u, 4u});
    (void)call_set(rt, ctx, 0x08A7E914u, {object, frame.at(0u)});
    put(rt, frame.at(4u), object);
    (void)call_set(rt, ctx, 0x08A76AB4u, {holder, frame.at(4u)});
    (void)call_set(rt, ctx, 0x08A69D68u, {frame.at(0u)});
    const auto constant = [&](std::uint32_t value, std::uint32_t name, std::uint32_t high) {
        const std::uint32_t target = word(rt, holder);
        if (target == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9BD7Cu});
        (void)call_set(rt, ctx, 0x08A5DEE8u, {value, ctx.gpr[5], 0xFFFFFFFFu, high});
        (void)call_set(rt, ctx, 0x08B8D810u, {target, name, value});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {value});
    };
    constant(frame.at(0xCu), 0x08C9BD84u, 0xFFFFFFFFu);
    constant(frame.at(8u), 0x08C9BD90u, 0u);
    register_methods(rt, ctx, holder, 0x08C9BD7Cu, kMethods, std::size(kMethods), false);
    ctx.gpr[2] = holder;
}

// func_08823BBC(holder): defines a script class: the class symbol 0x08C9997C, a 0x5C-byte class object from
// func_08A5BC20(0x5C, 4) built by func_08A54F3C(object, symbol, 0x14C) and held at frame +0xE8 (func_08A52AB0). Then
// 19 attributes through func_08A535A4(class, &name, &value, -1, flags) (an empty class reported through
// func_08A5BB7C(0x08C9971C) first): typed ones hold a new 0x38-byte object (func_08A52660(object, type symbol), its
// vtable stored), plain ones a value made by func_08B8C1E4, boxed ones a 0x44-byte func_08A5B70C(object, value, 10);
// each value and name is released after (func_08A5B9EC, func_08A69D68). Last func_08A7CE50(*holder, &class) (an empty
// holder reported through func_08A5BB7C(0x08C99738)), the class kept at 0x08CE2B10 and its handle released.
void func_08823BBC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    enum class Kind { kTyped, kValue, kBoxed };
    struct Attribute {
        Kind kind;
        std::uint32_t name_slot;
        std::uint32_t name;
        std::uint32_t value_slot;   // what func_08A535A4 receives
        std::uint32_t inner_slot;   // typed: the type symbol; boxed: the plain value inside
        std::uint32_t object_slot;  // the new object's pointer, handed to func_08A52AB0
        std::uint32_t type;
        std::uint32_t vtable;
        std::uint32_t flags;
    };
    static constexpr Attribute kAttributes[] = {
        {Kind::kTyped, 0xE4u, 0x08C99994u, 0xE0u, 0x38u, 0x3Cu, 0x08C99748u, 0x08CB7F98u, 0x0u},
        {Kind::kTyped, 0xDCu, 0x08C999A0u, 0xD8u, 0x30u, 0x34u, 0x08C9977Cu, 0x08CB7840u, 0x2u},
        {Kind::kTyped, 0xD4u, 0x08C999ACu, 0xD0u, 0x28u, 0x2Cu, 0x08C9977Cu, 0x08CB7840u, 0x3u},
        {Kind::kTyped, 0xCCu, 0x08C999C0u, 0xC8u, 0x20u, 0x24u, 0x08C9977Cu, 0x08CB7840u, 0x140u},
        {Kind::kTyped, 0xC4u, 0x08C999C8u, 0xC0u, 0x18u, 0x1Cu, 0x08C9977Cu, 0x08CB7840u, 0x145u},
        {Kind::kValue, 0xBCu, 0x08C999DCu, 0xB8u, 0x0u, 0x0u, 0x0u, 0x0u, 0x8Cu},
        {Kind::kValue, 0xB4u, 0x08C999F0u, 0xB0u, 0x0u, 0x0u, 0x0u, 0x0u, 0x90u},
        {Kind::kValue, 0xACu, 0x08C99A08u, 0xA8u, 0x0u, 0x0u, 0x0u, 0x0u, 0x94u},
        {Kind::kValue, 0xA4u, 0x08C99A1Cu, 0xA0u, 0x0u, 0x0u, 0x0u, 0x0u, 0x98u},
        {Kind::kValue, 0x9Cu, 0x08C99A34u, 0x98u, 0x0u, 0x0u, 0x0u, 0x0u, 0x9Cu},
        {Kind::kValue, 0x94u, 0x08C99A48u, 0x90u, 0x0u, 0x0u, 0x0u, 0x0u, 0xA0u},
        {Kind::kValue, 0x8Cu, 0x08C99A5Cu, 0x88u, 0x0u, 0x0u, 0x0u, 0x0u, 0xA4u},
        {Kind::kValue, 0x84u, 0x08C99A70u, 0x80u, 0x0u, 0x0u, 0x0u, 0x0u, 0xA8u},
        {Kind::kValue, 0x7Cu, 0x08C99A84u, 0x78u, 0x0u, 0x0u, 0x0u, 0x0u, 0xACu},
        {Kind::kValue, 0x74u, 0x08C99A98u, 0x70u, 0x0u, 0x0u, 0x0u, 0x0u, 0xB4u},
        {Kind::kTyped, 0x6Cu, 0x08C99AACu, 0x68u, 0x10u, 0x14u, 0x08C99730u, 0x08CB8110u, 0xB0u},
        {Kind::kTyped, 0x64u, 0x08C99AC8u, 0x60u, 0x8u, 0xCu, 0x08C99748u, 0x08CB7F98u, 0x108u},
        {Kind::kBoxed, 0x5Cu, 0x08C99AD8u, 0x58u, 0x54u, 0x4u, 0x0u, 0x0u, 0xB8u},
        {Kind::kBoxed, 0x50u, 0x08C99AECu, 0x4Cu, 0x48u, 0x0u, 0x0u, 0x0u, 0xE0u},
    };
    const std::uint32_t holder = ctx.gpr[4];
    GuestFrame frame(ctx, 0x120u);
    const auto sp = [&](std::uint32_t offset) { return frame.at(offset); };
    const std::uint32_t handle = sp(0xE8u);
    (void)call_set(rt, ctx, 0x08A69B3Cu, {sp(0x40u), 0x08C9997Cu});
    const std::uint32_t made = call_set(rt, ctx, 0x08A5BC20u, {0x5Cu, 4u});
    (void)call_set(rt, ctx, 0x08A54F3Cu, {made, sp(0x40u), 0x14Cu});
    put(rt, sp(0x44u), made);
    (void)call_set(rt, ctx, 0x08A52AB0u, {handle, sp(0x44u)});
    (void)call_set(rt, ctx, 0x08A69D68u, {sp(0x40u)});
    for (const Attribute &attribute : kAttributes) {
        const std::uint32_t target = word(rt, handle);
        if (target == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9971Cu});
        (void)call_set(rt, ctx, 0x08A69B3Cu, {sp(attribute.name_slot), attribute.name});
        switch (attribute.kind) {
        case Kind::kTyped: {
            const std::uint32_t object = call_set(rt, ctx, 0x08A5BC20u, {0x38u, 4u});
            (void)call_set(rt, ctx, 0x08A69B3Cu, {sp(attribute.inner_slot), attribute.type});
            (void)call_set(rt, ctx, 0x08A52660u, {object, sp(attribute.inner_slot)});
            (void)call_set(rt, ctx, 0x08A69D68u, {sp(attribute.inner_slot)});
            put(rt, object, attribute.vtable);
            put(rt, sp(attribute.object_slot), object);
            (void)call_set(rt, ctx, 0x08A52AB0u, {sp(attribute.value_slot), sp(attribute.object_slot)});
            break;
        }
        case Kind::kValue:
            (void)call_set(rt, ctx, 0x08B8C1E4u, {sp(attribute.value_slot)});
            break;
        case Kind::kBoxed: {
            (void)call_set(rt, ctx, 0x08B8C1E4u, {sp(attribute.inner_slot)});
            const std::uint32_t object = call_set(rt, ctx, 0x08A5BC20u, {0x44u, 4u});
            (void)call_set(rt, ctx, 0x08A5B70Cu, {object, sp(attribute.inner_slot), 0xAu});
            put(rt, sp(attribute.object_slot), object);
            (void)call_set(rt, ctx, 0x08A52AB0u, {sp(attribute.value_slot), sp(attribute.object_slot)});
            break;
        }
        }
        (void)call_set(rt, ctx, 0x08A535A4u, {target, sp(attribute.name_slot), sp(attribute.value_slot), 0xFFFFFFFFu, attribute.flags});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {sp(attribute.value_slot)});
        if (attribute.kind == Kind::kBoxed) (void)call_set(rt, ctx, 0x08A5B9ECu, {sp(attribute.inner_slot)});
        (void)call_set(rt, ctx, 0x08A69D68u, {sp(attribute.name_slot)});
    }
    const std::uint32_t owner = word(rt, holder);
    if (owner == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08C99738u});
    (void)call_set(rt, ctx, 0x08A7CE50u, {owner, handle});
    put(rt, 0x08CE2B10u, word(rt, handle));
    (void)call_set(rt, ctx, 0x08A5B9ECu, {handle});
}

// func_08822920(holder): defines a script class (symbol 0x08C997C0; a 0x5C-byte object built by func_08A54F3C(object, symbol, 0x26D0),
// held at frame +0x1D0) with 25 attributes through func_08A535A4 (report 0x08C9971C for an empty class), as func_08822920 does;
// then func_08A7CE50(*holder, &class) (report 0x08C99738), the class kept at 0x08CE2B14 and its handle released.
void func_08822920(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t holder = ctx.gpr[4];
    GuestFrame frame(ctx, 0x220u);
    const auto sp = [&](std::uint32_t offset) { return frame.at(offset); };
    put(rt, sp(0x1E0u), holder);
    const std::uint32_t handle = sp(0x1D0u);
    ClassBuilder builder(rt, ctx, frame, handle, 0x08C9971Cu);
    builder.begin(0xC0u, 0x08C997C0u, 0x26D0u, 0xC4u);
    const auto plain = [&](std::uint32_t maker, std::uint32_t slot) { builder.plain(maker, slot); };
    const auto typed = [&](std::uint32_t size, std::uint32_t type_slot, std::uint32_t type, std::initializer_list<ClassBuilder::Field> fields,
                           std::uint32_t object_slot, std::uint32_t holder_slot) {
        builder.typed(size, type_slot, type, fields, object_slot, holder_slot);
    };
    const auto box = [&](std::uint32_t inner, std::uint32_t n, std::uint32_t object_slot, std::uint32_t holder_slot) {
        builder.box(inner, n, object_slot, holder_slot);
    };
    const auto define = [&](std::uint32_t name_slot, std::uint32_t name, std::uint32_t value_slot, std::uint32_t a3, std::uint32_t flags,
                            const auto &build) { builder.define(name_slot, name, value_slot, a3, flags, build); };
    (void)plain; (void)typed; (void)box;
    define(0x1CC, 0x08C997D8u, 0x1C8, 0xFFFFFFFFu, 0x0u, [&] { typed(0x38, 0xB8, 0x08C9977Cu, {{0x0, 0x08CB7840u}}, 0xBC, 0x1C4); box(0x1C4, 0x4, 0xB4, 0x1C8); });
    define(0x1C0, 0x08C99798u, 0x1BC, 0x4u, 0x4F8u, [&] { plain(0x08B8C25C, 0x1BC); });
    define(0x1B8, 0x08C997E4u, 0x1B4, 0x4u, 0x4F0u, [&] { plain(0x08B8C25C, 0x1B4); });
    define(0x1B0, 0x08C997F0u, 0x1AC, 0x4u, 0x4E8u, [&] { plain(0x08B8C25C, 0x1AC); });
    define(0x1A8, 0x08C997FCu, 0x1A4, 0x4u, 0x500u, [&] { plain(0x08B8C25C, 0x1A4); });
    define(0x1A0, 0x08C9980Cu, 0x19C, 0x4u, 0x508u, [&] { plain(0x08B8C25C, 0x19C); });
    define(0x198, 0x08C9981Cu, 0x194, 0x4u, 0x510u, [&] { plain(0x08B8C25C, 0x194); });
    define(0x190, 0x08C9982Cu, 0x18C, 0x4u, 0x568u, [&] { typed(0x38, 0xAC, 0x08C9983Cu, {{0x0, 0x08CB7B30u}}, 0xB0, 0x18C); });
    define(0x188, 0x08C99844u, 0x184, 0x4u, 0x518u, [&] { typed(0x38, 0xA4, 0x08C997A8u, {{0x0, 0x08CB7E20u}}, 0xA8, 0x184); });
    define(0x180, 0x08C99858u, 0x17C, 0x4u, 0x538u, [&] { typed(0x38, 0x9C, 0x08C997A8u, {{0x0, 0x08CB7E20u}}, 0xA0, 0x17C); });
    define(0x178, 0x08C99868u, 0x174, 0x4u, 0x368u, [&] { plain(0x08B8C25C, 0x170); box(0x170, 0x10, 0x98, 0x174); });
    define(0x16C, 0x08C9987Cu, 0x168, 0x4u, 0x3E8u, [&] { plain(0x08B8C25C, 0x164); box(0x164, 0x10, 0x94, 0x168); });
    define(0x160, 0x08C99894u, 0x15C, 0x4u, 0x468u, [&] { plain(0x08B8C25C, 0x158); box(0x158, 0x10, 0x90, 0x15C); });
    define(0x154, 0x08C998A8u, 0x150, 0x4u, 0x578u, [&] { typed(0x3C, 0x88, 0x08C99768u, {{0x38, 0x4u}, {0x0, 0x08CB8288u}}, 0x8C, 0x150); });
    define(0x14C, 0x08C998B8u, 0x148, 0x4u, 0x588u, [&] { typed(0x3C, 0x80, 0x08C99768u, {{0x38, 0x80u}, {0x0, 0x08CB8288u}}, 0x84, 0x148); });
    define(0x144, 0x08C998C8u, 0x140, 0xFFFFFFFFu, 0xA08u, [&] { typed(0x3C, 0x78, 0x08C99768u, {{0x38, 0x20u}, {0x0, 0x08CB8288u}}, 0x7C, 0x13C); box(0x13C, 0x4, 0x74, 0x140); });
    define(0x138, 0x08C998DCu, 0x134, 0xFFFFFFFFu, 0x988u, [&] { typed(0x3C, 0x6C, 0x08C99768u, {{0x38, 0x20u}, {0x0, 0x08CB8288u}}, 0x70, 0x130); box(0x130, 0x4, 0x68, 0x134); });
    define(0x12C, 0x08C998ECu, 0x128, 0x4u, 0x18u, [&] { typed(0x38, 0x60, 0x08C9983Cu, {{0x0, 0x08CB7B30u}}, 0x64, 0x128); });
    define(0x124, 0x08C99900u, 0x120, 0xFFFFFFFFu, 0x788u, [&] { typed(0x3C, 0x58, 0x08C99768u, {{0x38, 0x20u}, {0x0, 0x08CB8288u}}, 0x5C, 0x11C); box(0x11C, 0x4, 0x54, 0x120); });
    define(0x118, 0x08C9990Cu, 0x114, 0xFFFFFFFFu, 0x808u, [&] { typed(0x3C, 0x4C, 0x08C99768u, {{0x38, 0x20u}, {0x0, 0x08CB8288u}}, 0x50, 0x110); box(0x110, 0x4, 0x48, 0x114); });
    define(0x10C, 0x08C99920u, 0x108, 0xFFFFFFFFu, 0x908u, [&] { typed(0x3C, 0x40, 0x08C99768u, {{0x38, 0x20u}, {0x0, 0x08CB8288u}}, 0x44, 0x104); box(0x104, 0x4, 0x3C, 0x108); });
    define(0x100, 0x08C99930u, 0xFC, 0xFFFFFFFFu, 0x888u, [&] { typed(0x3C, 0x34, 0x08C99768u, {{0x38, 0x20u}, {0x0, 0x08CB8288u}}, 0x38, 0xF8); box(0xF8, 0x4, 0x30, 0xFC); });
    define(0xF4, 0x08C99940u, 0xF0, 0xFFFFFFFFu, 0x1288u, [&] { typed(0x3C, 0x28, 0x08C99768u, {{0x38, 0x20u}, {0x0, 0x08CB8288u}}, 0x2C, 0xE8); box(0xE8, 0x10, 0x24, 0xEC); box(0xEC, 0x4, 0x20, 0xF0); });
    define(0xE4, 0x08C99954u, 0xE0, 0xFFFFFFFFu, 0x1A88u, [&] { typed(0x3C, 0x18, 0x08C99768u, {{0x38, 0x20u}, {0x0, 0x08CB8288u}}, 0x1C, 0xD8); box(0xD8, 0x10, 0x14, 0xDC); box(0xDC, 0x4, 0x10, 0xE0); });
    define(0xD4, 0x08C99968u, 0xD0, 0xFFFFFFFFu, 0xA88u, [&] { typed(0x3C, 0x8, 0x08C99768u, {{0x38, 0x20u}, {0x0, 0x08CB8288u}}, 0xC, 0xC8); box(0xC8, 0x10, 0x4, 0xCC); box(0xCC, 0x4, 0x0, 0xD0); });
    builder.finish(word(rt, word(rt, sp(0x1E0u))), 0x08C99738u, 0x08CE2B14u);
}

// func_08A270AC(holder) -> holder: makes a script class (name symbol 0x08CAB3D8; a 0x38-byte object from func_08A5BC20(0x38, 4)
// built by func_08A7E914 and handed to func_08A76AB4(holder, &object)) and registers its 16 methods through
// func_08B86FB0 (report 0x08CAB3E8 for an empty holder).
void func_08A270AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    static constexpr std::uint32_t kMethods[][2] = {
        {0x08CAB3F0u, 0x08A26E98u}, {0x08CAB3F8u, 0x08A26EA0u}, {0x08CAB3FCu, 0x08A26EA8u},
        {0x08CAB404u, 0x08A2741Cu}, {0x08CAB410u, 0x08A27460u}, {0x08CAB420u, 0x08A27808u},
        {0x08CAB42Cu, 0x08A274A8u}, {0x08CAB438u, 0x08A274F0u}, {0x08CAB444u, 0x08A26EB0u},
        {0x08CAB454u, 0x08A26F30u}, {0x08CAB460u, 0x08A27538u}, {0x08CAB470u, 0x08A26EC4u},
        {0x08CAB480u, 0x08A27610u}, {0x08CAB490u, 0x08A26E28u}, {0x08CAB4A0u, 0x08A276E8u},
        {0x08CAB4B0u, 0x08A277C0u},
    };
    const std::uint32_t holder = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    (void)call_set(rt, ctx, 0x08A69B3Cu, {frame.at(0u), 0x08CAB3D8u});
    const std::uint32_t object = call_set(rt, ctx, 0x08A5BC20u, {0x38u, 4u});
    (void)call_set(rt, ctx, 0x08A7E914u, {object, frame.at(0u)});
    put(rt, frame.at(4u), object);
    (void)call_set(rt, ctx, 0x08A76AB4u, {holder, frame.at(4u)});
    (void)call_set(rt, ctx, 0x08A69D68u, {frame.at(0u)});
    register_methods(rt, ctx, holder, 0x08CAB3E8u, kMethods, std::size(kMethods), false);
    ctx.gpr[2] = holder;
}

// func_08A20010(holder) -> holder: makes a script class (name symbol 0x08CAA6BC; a 0x38-byte object from func_08A5BC20(0x38, 4)
// built by func_08A7E914 and handed to func_08A76AB4(holder, &object)) and registers its 13 methods through
// func_08B86FB0 (report 0x08CAA6C8 for an empty holder).
void func_08A20010(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    static constexpr std::uint32_t kMethods[][2] = {
        {0x08CAA6D0u, 0x08A2041Cu}, {0x08CAA6D8u, 0x08A20414u}, {0x08CAA6E0u, 0x08A2040Cu},
        {0x08CAA6E8u, 0x08A20404u}, {0x08CAA6F0u, 0x08A203FCu}, {0x08CAA6F8u, 0x08A20360u},
        {0x08CAA700u, 0x08A1FF94u}, {0x08CAA70Cu, 0x08A206E8u}, {0x08CAA718u, 0x08A20424u},
        {0x08CAA724u, 0x08A20514u}, {0x08CAA734u, 0x08A2055Cu}, {0x08CAA740u, 0x08A205ACu},
        {0x08CAA74Cu, 0x08A205FCu},
    };
    const std::uint32_t holder = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    (void)call_set(rt, ctx, 0x08A69B3Cu, {frame.at(0u), 0x08CAA6BCu});
    const std::uint32_t object = call_set(rt, ctx, 0x08A5BC20u, {0x38u, 4u});
    (void)call_set(rt, ctx, 0x08A7E914u, {object, frame.at(0u)});
    put(rt, frame.at(4u), object);
    (void)call_set(rt, ctx, 0x08A76AB4u, {holder, frame.at(4u)});
    (void)call_set(rt, ctx, 0x08A69D68u, {frame.at(0u)});
    register_methods(rt, ctx, holder, 0x08CAA6C8u, kMethods, std::size(kMethods), false);
    ctx.gpr[2] = holder;
}

// func_08834670(holder): defines a script class (symbol 0x08C9B490; a 0x5C-byte object built by func_08A54F3C(object, symbol, 0x8),
// held at frame +0x58) with 5 attributes through func_08A535A4 (report 0x08C9B1D4 for an empty class), as func_08822920 does;
// then func_08A7CE50(*holder, &class) (report 0x08C9B4F8), the class kept at 0x08CE2B90 and its handle released.
void func_08834670(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t holder = ctx.gpr[4];
    GuestFrame frame(ctx, 0x90u);
    const auto sp = [&](std::uint32_t offset) { return frame.at(offset); };
    const std::uint32_t handle = sp(0x58u);
    ClassBuilder builder(rt, ctx, frame, handle, 0x08C9B1D4u);
    builder.begin(0x28u, 0x08C9B490u, 0x8u, 0x2Cu);
    const auto plain = [&](std::uint32_t maker, std::uint32_t slot) { builder.plain(maker, slot); };
    const auto typed = [&](std::uint32_t size, std::uint32_t type_slot, std::uint32_t type, std::initializer_list<ClassBuilder::Field> fields,
                           std::uint32_t object_slot, std::uint32_t holder_slot) {
        builder.typed(size, type_slot, type, fields, object_slot, holder_slot);
    };
    const auto box = [&](std::uint32_t inner, std::uint32_t n, std::uint32_t object_slot, std::uint32_t holder_slot) {
        builder.box(inner, n, object_slot, holder_slot);
    };
    const auto define = [&](std::uint32_t name_slot, std::uint32_t name, std::uint32_t value_slot, std::uint32_t a3, std::uint32_t flags,
                            const auto &build) { builder.define(name_slot, name, value_slot, a3, flags, build); };
    (void)plain; (void)typed; (void)box;
    define(0x54, 0x08C9B49Cu, 0x50, 0xFFFFFFFFu, 0x1u, [&] { typed(0x38, 0x20, 0x08C9B4A8u, {{0x0, 0x08CB7840u}}, 0x24, 0x50); });
    define(0x4C, 0x08C9B4B0u, 0x48, 0xFFFFFFFFu, 0x2u, [&] { typed(0x38, 0x18, 0x08C9B4A8u, {{0x0, 0x08CB7840u}}, 0x1C, 0x48); });
    define(0x44, 0x08C9B4BCu, 0x40, 0xFFFFFFFFu, 0x3u, [&] { typed(0x38, 0x10, 0x08C9B4A8u, {{0x0, 0x08CB7840u}}, 0x14, 0x40); });
    define(0x3C, 0x08C9B4CCu, 0x38, 0xFFFFFFFFu, 0x4u, [&] { typed(0x38, 0x8, 0x08C9B4E0u, {{0x0, 0x08CB7F98u}}, 0xC, 0x38); });
    define(0x34, 0x08C9B4E8u, 0x30, 0xFFFFFFFFu, 0x6u, [&] { typed(0x38, 0x0, 0x08C9B4E0u, {{0x0, 0x08CB7F98u}}, 0x4, 0x30); });
    builder.finish(word(rt, holder), 0x08C9B4F8u, 0x08CE2B90u);
}

// func_08834A8C(holder): defines a script class (symbol 0x08C9B500; a 0x5C-byte object built by func_08A54F3C(object, symbol, 0x18),
// held at frame +0x88) with 8 attributes through func_08A535A4 (report 0x08C9B1D4 for an empty class), as func_08822920 does;
// then func_08A7CE50(*holder, &class) (report 0x08C9B4F8), the class kept at 0x08CE2B8C and its handle released.
void func_08834A8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t holder = ctx.gpr[4];
    GuestFrame frame(ctx, 0xD0u);
    const auto sp = [&](std::uint32_t offset) { return frame.at(offset); };
    put(rt, sp(0x90u), holder);
    const std::uint32_t handle = sp(0x88u);
    ClassBuilder builder(rt, ctx, frame, handle, 0x08C9B1D4u);
    builder.begin(0x40u, 0x08C9B500u, 0x18u, 0x44u);
    const auto plain = [&](std::uint32_t maker, std::uint32_t slot) { builder.plain(maker, slot); };
    const auto typed = [&](std::uint32_t size, std::uint32_t type_slot, std::uint32_t type, std::initializer_list<ClassBuilder::Field> fields,
                           std::uint32_t object_slot, std::uint32_t holder_slot) {
        builder.typed(size, type_slot, type, fields, object_slot, holder_slot);
    };
    const auto box = [&](std::uint32_t inner, std::uint32_t n, std::uint32_t object_slot, std::uint32_t holder_slot) {
        builder.box(inner, n, object_slot, holder_slot);
    };
    const auto define = [&](std::uint32_t name_slot, std::uint32_t name, std::uint32_t value_slot, std::uint32_t a3, std::uint32_t flags,
                            const auto &build) { builder.define(name_slot, name, value_slot, a3, flags, build); };
    (void)plain; (void)typed; (void)box;
    define(0x84, 0x08C9B510u, 0x80, 0xFFFFFFFFu, 0x9u, [&] { typed(0x38, 0x38, 0x08C9B4A8u, {{0x0, 0x08CB7840u}}, 0x3C, 0x80); });
    define(0x7C, 0x08C9B51Cu, 0x78, 0xFFFFFFFFu, 0x0u, [&] { typed(0x38, 0x30, 0x08C9B520u, {{0x0, 0x08CB7E20u}}, 0x34, 0x78); });
    define(0x74, 0x08C9B528u, 0x70, 0xFFFFFFFFu, 0xCu, [&] { typed(0x38, 0x28, 0x08C9B530u, {{0x0, 0x08CB79B8u}}, 0x2C, 0x70); });
    define(0x6C, 0x08C9B538u, 0x68, 0xFFFFFFFFu, 0x10u, [&] { typed(0x38, 0x20, 0x08C9B4E0u, {{0x0, 0x08CB7F98u}}, 0x24, 0x68); });
    define(0x64, 0x08C9B53Cu, 0x60, 0xFFFFFFFFu, 0x12u, [&] { typed(0x38, 0x18, 0x08C9B4E0u, {{0x0, 0x08CB7F98u}}, 0x1C, 0x60); });
    define(0x5C, 0x08C9B540u, 0x58, 0xFFFFFFFFu, 0x14u, [&] { typed(0x38, 0x10, 0x08C9B4E0u, {{0x0, 0x08CB7F98u}}, 0x14, 0x58); });
    define(0x54, 0x08C9B550u, 0x50, 0xFFFFFFFFu, 0x16u, [&] { typed(0x38, 0x8, 0x08C9B4A8u, {{0x0, 0x08CB7840u}}, 0xC, 0x50); });
    define(0x4C, 0x08C9B560u, 0x48, 0xFFFFFFFFu, 0x17u, [&] { typed(0x38, 0x0, 0x08C9B4A8u, {{0x0, 0x08CB7840u}}, 0x4, 0x48); });
    builder.finish(word(rt, word(rt, sp(0x90u))), 0x08C9B4F8u, 0x08CE2B8Cu);
}

// func_08828D74(holder): defines a script class (symbol 0x08C9A214; a 0x5C-byte object built by func_08A54F3C(object, symbol, 0x1C),
// held at frame +0x78) with 7 attributes through func_08A535A4 (report 0x08C9A1A8 for an empty class), as func_08822920 does;
// then func_08A7CE50(*holder, &class) (report 0x08C9A28C), the class kept at 0x08CE2B3C and its handle released.
void func_08828D74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t holder = ctx.gpr[4];
    GuestFrame frame(ctx, 0xB0u);
    const auto sp = [&](std::uint32_t offset) { return frame.at(offset); };
    const std::uint32_t handle = sp(0x78u);
    ClassBuilder builder(rt, ctx, frame, handle, 0x08C9A1A8u);
    builder.begin(0x38u, 0x08C9A214u, 0x1Cu, 0x3Cu);
    const auto plain = [&](std::uint32_t maker, std::uint32_t slot) { builder.plain(maker, slot); };
    const auto typed = [&](std::uint32_t size, std::uint32_t type_slot, std::uint32_t type, std::initializer_list<ClassBuilder::Field> fields,
                           std::uint32_t object_slot, std::uint32_t holder_slot) {
        builder.typed(size, type_slot, type, fields, object_slot, holder_slot);
    };
    const auto box = [&](std::uint32_t inner, std::uint32_t n, std::uint32_t object_slot, std::uint32_t holder_slot) {
        builder.box(inner, n, object_slot, holder_slot);
    };
    const auto define = [&](std::uint32_t name_slot, std::uint32_t name, std::uint32_t value_slot, std::uint32_t a3, std::uint32_t flags,
                            const auto &build) { builder.define(name_slot, name, value_slot, a3, flags, build); };
    (void)plain; (void)typed; (void)box;
    define(0x74, 0x08C9A224u, 0x70, 0xFFFFFFFFu, 0x0u, [&] { typed(0x38, 0x30, 0x08C9A230u, {{0x0, 0x08CB79B8u}}, 0x34, 0x70); });
    define(0x6C, 0x08C9A238u, 0x68, 0xFFFFFFFFu, 0x4u, [&] { typed(0x38, 0x28, 0x08C9A244u, {{0x0, 0x08CB7B30u}}, 0x2C, 0x68); });
    define(0x64, 0x08C9A24Cu, 0x60, 0xFFFFFFFFu, 0x8u, [&] { typed(0x38, 0x20, 0x08C9A244u, {{0x0, 0x08CB7B30u}}, 0x24, 0x60); });
    define(0x5C, 0x08C9A258u, 0x58, 0xFFFFFFFFu, 0xCu, [&] { typed(0x38, 0x18, 0x08C9A244u, {{0x0, 0x08CB7B30u}}, 0x1C, 0x58); });
    define(0x54, 0x08C9A268u, 0x50, 0xFFFFFFFFu, 0x10u, [&] { typed(0x38, 0x10, 0x08C9A244u, {{0x0, 0x08CB7B30u}}, 0x14, 0x50); });
    define(0x4C, 0x08C9A274u, 0x48, 0xFFFFFFFFu, 0x14u, [&] { typed(0x38, 0x8, 0x08C9A244u, {{0x0, 0x08CB7B30u}}, 0xC, 0x48); });
    define(0x44, 0x08C9A280u, 0x40, 0xFFFFFFFFu, 0x18u, [&] { typed(0x38, 0x0, 0x08C9A244u, {{0x0, 0x08CB7B30u}}, 0x4, 0x40); });
    builder.finish(word(rt, holder), 0x08C9A28Cu, 0x08CE2B3Cu);
}

// func_08A1ED68(holder): registers the script methods of the class *holder through func_08B86FB0 (report 0x08CAA488 for
// an empty holder): 11 methods, the integer constant 0x08CAA56C = 0x481907FF (made by func_08A85B1C in the frame, added
// by func_08B86E98(class, name, &value) and destroyed by func_08A5B9EC), then 7 more.
void func_08A1ED68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    static constexpr std::uint32_t kFirst[][2] = {
        {0x08CAA490u, 0x08A1EBE4u}, {0x08CAA4A4u, 0x08A1EBECu}, {0x08CAA4B8u, 0x08A1EBF4u},
        {0x08CAA4C8u, 0x08A1EBFCu}, {0x08CAA4E0u, 0x08A1EC04u}, {0x08CAA4F8u, 0x08A1FE40u},
        {0x08CAA50Cu, 0x08A1FCECu}, {0x08CAA520u, 0x08A1FBA0u}, {0x08CAA534u, 0x08A1FA54u},
        {0x08CAA54Cu, 0x08A1F8FCu}, {0x08CAA55Cu, 0x08A1F7C0u},
    };
    static constexpr std::uint32_t kSecond[][2] = {
        {0x08CAA57Cu, 0x08A1ED00u}, {0x08CAA588u, 0x08A1F130u}, {0x08CAA598u, 0x08A1EC0Cu},
        {0x08CAA5A8u, 0x08A1F5D4u}, {0x08CAA5B8u, 0x08A1F520u}, {0x08CAA5C8u, 0x08A1F174u},
        {0x08CAA5DCu, 0x08A1F328u},
    };
    const std::uint32_t holder = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    register_methods(rt, ctx, holder, 0x08CAA488u, kFirst, std::size(kFirst), false);
    const std::uint32_t object = word(rt, holder);
    if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAA488u});
    (void)call_set(rt, ctx, 0x08A85B1Cu, {frame.at(0u), 0x481907FFu});
    (void)call_set(rt, ctx, 0x08B86E98u, {object, 0x08CAA56Cu, frame.at(0u)});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    register_methods(rt, ctx, holder, 0x08CAA488u, kSecond, std::size(kSecond), false);
}

// func_088541F0(holder): defines 7 script classes in a row, each made like func_08822920's (symbol, a 0x5C-byte object
// from func_08A54F3C(object, symbol, size) held at frame +0x38), given its method table by func_08854000(&class, table),
// handed to func_08A7CE50(*holder, &class) (report 0x08C9E0C8 for an empty holder) and kept in its global.
void func_088541F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    struct Class {
        std::uint32_t symbol_slot;
        std::uint32_t symbol;
        std::uint32_t size;
        std::uint32_t object_slot;
        std::uint32_t methods;
        std::uint32_t global;
    };
    static constexpr Class kClasses[] = {
        {0x30u, 0x08C9E0B0u, 0x38u, 0x34u, 0x08C88ED4u, 0x08CE2D08u},
        {0x28u, 0x08C9E0D0u, 0x1B0u, 0x2Cu, 0x08C889B8u, 0x08CE2D04u},
        {0x20u, 0x08C9E0E8u, 0x60u, 0x24u, 0x08C8888Cu, 0x08CE2D00u},
        {0x18u, 0x08C9E100u, 0x10u, 0x1Cu, 0x08C88850u, 0x08CE2CFCu},
        {0x10u, 0x08C9E114u, 0x38u, 0x14u, 0x08C8879Cu, 0x08CE2CF8u},
        {0x8u, 0x08C9E12Cu, 0x70u, 0xCu, 0x08C88640u, 0x08CE2CF4u},
        {0x0u, 0x08C9E13Cu, 0x18u, 0x4u, 0x08C885F8u, 0x08CE2CF0u},
    };
    const std::uint32_t holder = ctx.gpr[4];
    GuestFrame frame(ctx, 0x60u);
    const std::uint32_t handle = frame.at(0x38u);
    for (const Class &entry : kClasses) {
        ClassBuilder builder(rt, ctx, frame, handle, 0x08C9E0C8u);
        builder.begin(entry.symbol_slot, entry.symbol, entry.size, entry.object_slot);
        (void)call_set(rt, ctx, 0x08854000u, {handle, entry.methods});
        builder.finish(word(rt, holder), 0x08C9E0C8u, entry.global);
    }
}

} // namespace hot_batch_132
} // namespace gt::game

#define HOT132(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_132::name, gt::ReturnKind::kind)
HOT132(0x0883A4D8u, func_0883A4D8, Word);  // boot-time registration: compared in the fast gate, 0 in the full gate
HOT132(0x08823BBCu, func_08823BBC, Word);  // boot-time registration: compared in the fast gate, 0 in the full gate
HOT132(0x08822920u, func_08822920, Word);  // boot-time registration: compared in the fast gate, 0 in the full gate
HOT132(0x08A270ACu, func_08A270AC, Word);  // boot-time registration: compared in the fast gate, 0 in the full gate
HOT132(0x08A20010u, func_08A20010, Word);  // boot-time registration: compared in the fast gate, 0 in the full gate
HOT132(0x08834670u, func_08834670, Word);  // boot-time registration: compared in the fast gate, 0 in the full gate
HOT132(0x08834A8Cu, func_08834A8C, Word);  // boot-time registration: compared in the fast gate, 0 in the full gate
HOT132(0x08828D74u, func_08828D74, Word);  // boot-time registration: compared in the fast gate, 0 in the full gate
HOT132(0x08A1ED68u, func_08A1ED68, Word);  // boot-time registration: compared in the fast gate, 0 in the full gate
HOT132(0x088541F0u, func_088541F0, Word);  // boot-time registration: compared in the fast gate, 0 in the full gate
