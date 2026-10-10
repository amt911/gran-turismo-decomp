// The printf family of the C library statically linked into GT. It is not newlib's: formatting goes
// through C++ "sink" objects (vtable at +0; the formatter at 0x08B36358 calls the sink's write for
// every run of output). Two sinks matter here:
//   string sink   +0x20 write pointer                    (sprintf: unbounded)
//   bounded sink  +0x20 write pointer, +0x24 room left   (snprintf: -1 when the output didn't fit)
// The variadic front ends spill their register arguments to the stack and pass that as the va_list.
//
// Name evidence: snprintf from its call sites ("%s/%s/rtext.rt2" into a 0x80 buffer, the catalogue
// texts "%d kg"); the others are its siblings by shape (the unbounded twin, their v- cores, the
// sinks those build on the stack).

#include "decomp_registry.hpp"
#include "guest_call.hpp"

namespace gt::libc {
namespace {

constexpr std::uint32_t kFormat = 0x08B36358u;            // (sink *, fmt, va_list) -> count
constexpr std::uint32_t kMemcpy = 0x08B79A88u;            // src/libc/memcpy.cpp
constexpr std::uint32_t kSinkInit = 0x08B36290u;          // sink base constructor (below)
constexpr std::uint32_t kStringSinkInit = 0x08B37314u;    // (sink *, char *buffer)
constexpr std::uint32_t kStringSinkDestroy = 0x08B37364u; // vtable reset + base destructor
constexpr std::uint32_t kBoundedSinkInit = 0x08B3749Cu;   // (sink *, char *buffer, size)
constexpr std::uint32_t kBoundedSinkDestroy = 0x08B374FCu;
constexpr std::uint32_t kVsprintf = 0x08B373ECu;
constexpr std::uint32_t kVsnprintf = 0x08B375C8u;
constexpr std::uint32_t kVprint = 0x08B3A710u;            // (fmt, va_list): the unbuffered front end's core

constexpr std::uint32_t kSinkVtable = 0x08CDA878u;
constexpr std::uint32_t kStringSinkVtable = 0x08CDA898u;
constexpr std::uint32_t kBoundedSinkVtable = 0x08CDA8B8u;
constexpr std::uint32_t kOtherSinkVtable = 0x08CDA8D8u;   // a third sink, zeroed +4..+0xC

constexpr std::uint32_t kWritePointer = 0x20u;
constexpr std::uint32_t kRoomLeft = 0x24u;

// Spills the register arguments from a{first} on (up to t3) where the original puts them and
// returns the va_list pointing at the first one.
std::uint32_t spill_varargs(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, const GuestFrame &frame,
                            unsigned first) {
    const std::uint32_t va = frame.at(0x40u - (12u - first) * 4u);
    for (unsigned reg = first; reg <= 11u; ++reg) rt.memory().store32(va + (reg - first) * 4u, ctx.gpr[reg]);
    rt.memory().store32(frame.at(0u), va);
    return va;
}

} // namespace

// sink_init(sink): the base sink — its vtable, +0x1C cleared.
void sink_init(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store32(ctx.gpr[4], kSinkVtable);
    rt.memory().store32(ctx.gpr[4] + 0x1Cu, 0u);
}

// sink_destroy(sink): the base destructor only resets the vtable.
void sink_destroy(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store32(ctx.gpr[4], kSinkVtable);
}

// string_sink_init(sink, buffer) (two copies: the complete and the base constructor).
void string_sink_init(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t sink = ctx.gpr[4];
    const std::uint32_t buffer = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, kSinkInit, {sink});
    }
    rt.memory().store32(sink + kWritePointer, buffer);
    rt.memory().store32(sink, kStringSinkVtable);
}

// bounded_sink_init(sink, buffer, size) (two copies, as above).
void bounded_sink_init(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t sink = ctx.gpr[4];
    const std::uint32_t buffer = ctx.gpr[5];
    const std::uint32_t size = ctx.gpr[6];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, kSinkInit, {sink});
    }
    rt.memory().store32(sink + kRoomLeft, size);
    rt.memory().store32(sink, kBoundedSinkVtable);
    rt.memory().store32(sink + kWritePointer, buffer);
}

// other_sink_init(sink): vtable and three cleared words.
void other_sink_init(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t sink = ctx.gpr[4];
    rt.memory().store32(sink, kOtherSinkVtable);
    rt.memory().store32(sink + 0xCu, 0u);
    rt.memory().store32(sink + 0x4u, 0u);
    rt.memory().store32(sink + 0x8u, 0u);
}

// string_sink_write(sink, data, length) -> length: appends without a bound.
void string_sink_write(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t sink = ctx.gpr[4];
    const std::uint32_t length = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    (void)call_guest(rt, ctx, kMemcpy, {rt.memory().load32(sink + kWritePointer), ctx.gpr[5], length});
    rt.memory().store32(sink + kWritePointer, rt.memory().load32(sink + kWritePointer) + length);
    ctx.gpr[2] = length;
}

// bounded_sink_write(sink, data, length) -> length when it fits; otherwise copies what fits and
// returns -1 (also with no room at all).
void bounded_sink_write(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t sink = ctx.gpr[4];
    const std::uint32_t data = ctx.gpr[5];
    const auto length = static_cast<std::int32_t>(ctx.gpr[6]);
    const auto room = static_cast<std::int32_t>(rt.memory().load32(sink + kRoomLeft));
    GuestFrame frame(ctx, 0x10u);
    if (length < room) {
        (void)call_guest(rt, ctx, kMemcpy, {rt.memory().load32(sink + kWritePointer), data, ctx.gpr[6]});
        rt.memory().store32(sink + kWritePointer, rt.memory().load32(sink + kWritePointer) + ctx.gpr[6]);
        rt.memory().store32(sink + kRoomLeft, rt.memory().load32(sink + kRoomLeft) - ctx.gpr[6]);
        ctx.gpr[2] = ctx.gpr[6];
        return;
    }
    if (room != 0) {
        (void)call_guest(rt, ctx, kMemcpy, {rt.memory().load32(sink + kWritePointer), data, static_cast<std::uint32_t>(room)});
        const std::uint32_t left = rt.memory().load32(sink + kRoomLeft);
        rt.memory().store32(sink + kRoomLeft, 0u);
        rt.memory().store32(sink + kWritePointer, rt.memory().load32(sink + kWritePointer) + left);
    }
    ctx.gpr[2] = 0xFFFFFFFFu;
}

// vsprintf(buffer, fmt, va) -> characters written; the output is NUL-terminated.
void vsprintf(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t buffer = ctx.gpr[4];
    const std::uint32_t fmt = ctx.gpr[5];
    const std::uint32_t va = ctx.gpr[6];
    GuestFrame frame(ctx, 0x40u);
    const std::uint32_t sink = frame.at(0u);
    (void)call_guest(rt, ctx, kStringSinkInit, {sink, buffer});
    const std::uint32_t count = call_guest(rt, ctx, kFormat, {sink, fmt, va});
    rt.memory().store8(rt.memory().load32(sink + kWritePointer), 0u);
    (void)call_guest(rt, ctx, kStringSinkDestroy, {sink});
    ctx.gpr[2] = count;
}

// vsnprintf(buffer, size, fmt, va) -> characters written, or -1 when the output (with its NUL)
// didn't fit — then the buffer is left unterminated.
void vsnprintf(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t buffer = ctx.gpr[4];
    const std::uint32_t size = ctx.gpr[5];
    const std::uint32_t fmt = ctx.gpr[6];
    const std::uint32_t va = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    const std::uint32_t sink = frame.at(0u);
    (void)call_guest(rt, ctx, kBoundedSinkInit, {sink, buffer, size});
    std::uint32_t count = call_guest(rt, ctx, kFormat, {sink, fmt, va});
    if (rt.memory().load32(sink + kRoomLeft) == 0u) {
        count = 0xFFFFFFFFu;
    } else {
        rt.memory().store8(rt.memory().load32(sink + kWritePointer), 0u);
    }
    (void)call_guest(rt, ctx, kBoundedSinkDestroy, {sink});
    ctx.gpr[2] = count;
}

// sprintf(buffer, fmt, ...).
void sprintf(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x40u);
    const std::uint32_t va = spill_varargs(rt, ctx, frame, 6u);
    ctx.gpr[2] = call_guest(rt, ctx, kVsprintf, {ctx.gpr[4], ctx.gpr[5], va});
}

// snprintf(buffer, size, fmt, ...).
void snprintf(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x40u);
    const std::uint32_t va = spill_varargs(rt, ctx, frame, 7u);
    ctx.gpr[2] = call_guest(rt, ctx, kVsnprintf, {ctx.gpr[4], ctx.gpr[5], ctx.gpr[6], va});
}

// func_08B376B4(fmt, ...): the variadic front end of 0x08B3A710 (fmt, va_list).
void func_08B376B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x40u);
    const std::uint32_t va = spill_varargs(rt, ctx, frame, 5u);
    ctx.gpr[2] = call_guest(rt, ctx, kVprint, {ctx.gpr[4], va});
}

// parse_decimal(const char **cursor) -> the decimal number at *cursor (0 if none); *cursor is left
// after its last digit. The formatter's width/precision reader.
void parse_decimal(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t cursor = ctx.gpr[4];
    std::uint32_t p = rt.memory().load32(cursor);
    std::uint32_t value = 0u;
    std::uint32_t digit = static_cast<std::uint8_t>(static_cast<std::int8_t>(rt.memory().load8(p)) - '0');
    while (digit < 10u) {
        ++p;
        value = digit + value * 10u;
        digit = static_cast<std::uint8_t>(static_cast<std::int8_t>(rt.memory().load8(p)) - '0');
    }
    rt.memory().store32(cursor, p);
    ctx.gpr[2] = value;
}

// func_08B36264(a, b): stores its two arguments in two globals.
void func_08B36264(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rt.memory().store32(0x08D694CCu, ctx.gpr[4]);
    rt.memory().store32(0x08D694C8u, ctx.gpr[5]);
}

} // namespace gt::libc

GT_DECOMP(0x08B36290u, gt::libc::sink_init, gt::ReturnKind::Void);
GT_DECOMP(0x08B362B8u, gt::libc::sink_destroy, gt::ReturnKind::Void);
GT_DECOMP(0x08B37314u, gt::libc::string_sink_init, gt::ReturnKind::Void);
GT_DECOMP(0x08B3749Cu, gt::libc::bounded_sink_init, gt::ReturnKind::Void);
GT_DECOMP(0x08B373A8u, gt::libc::string_sink_write, gt::ReturnKind::Word);
GT_DECOMP(0x08B37540u, gt::libc::bounded_sink_write, gt::ReturnKind::Word);
GT_DECOMP(0x08B373ECu, gt::libc::vsprintf, gt::ReturnKind::Word);
GT_DECOMP(0x08B375C8u, gt::libc::vsnprintf, gt::ReturnKind::Word);
GT_DECOMP(0x08B37640u, gt::libc::sprintf, gt::ReturnKind::Word);
GT_DECOMP(0x08B3767Cu, gt::libc::snprintf, gt::ReturnKind::Word);
GT_DECOMP(0x08B362F0u, gt::libc::parse_decimal, gt::ReturnKind::Word);
GT_DECOMP(0x08B36264u, gt::libc::func_08B36264, gt::ReturnKind::Void);
// Written but not registered: 0x08B372D4 / 0x08B3744C (the other copy of each constructor) have no
// recompiled entry; other_sink_init (0x08B37710) and func_08B376B4 are never called on the routes.
