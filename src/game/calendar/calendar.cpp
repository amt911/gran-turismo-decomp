// Calendar: the natives of MCalendar, the game's day counter (layout and evidence:
// include/gt/game/calendar.hpp; names from GT's registration, config/symbol_addrs.txt).
//
// Registered functions replace the recompiled originals in the game. The ones MCalendar has that are
// not written here (dayToDate, getEvent, putRunCourseEvent, putRunLicenseEvent, putGarageEvent) still run
// as the originals.
//
// Modding: the day progression is MCalendar::incDate (one day on the counter) and the date <-> day
// conversions below; a mod that wants longer or shorter seasons changes them here. Keep the original
// behaviour for the differential gate and make such changes opt-in.

#include "decomp_registry.hpp"
#include "gt/game/calendar.hpp"
#include "guest_call.hpp"
#include "../adhoc/value.hpp"

#include <cstdint>

namespace gt::game::calendar {
namespace {

using guest::View;

constexpr std::uint32_t kHolderReset = 0x08A5BAD8u;
constexpr std::uint32_t kHolderDestroy = 0x08A5B9ECu;
constexpr std::uint32_t kSelfAsCalendar = 0x08817FCCu;  // (holder *out) [a1 = self]
constexpr std::uint32_t kNewDayObject = 0x08A5FA8Cu;    // (holder *out, day): the Adhoc value dateToDay returns
constexpr std::uint32_t kHolderCopy = 0x08A76AACu;      // (holder *out, const holder *source)
constexpr std::uint32_t kNil = 0x08CF6ED8u;             // the global nil holder
constexpr std::uint32_t kDateToDays = 0x08B1B2F4u;      // (year, month, day) -> day number

void destroy(psprecomp::Runtime &rt, const psprecomp::AllegrexContext &ctx, std::uint32_t holder) {
    (void)call_guest(rt, ctx, kHolderDestroy, {holder});
}

// The engine object of self (a1): self as a Calendar, then func_08817A54 (the pointer at +0x14 of the script
// object; already written as gt::core::field_getter), with the holder dropped.
View<CalendarData> engine_of_self(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t holder = frame.at(0u);
    (void)call_guest(rt, ctx, kSelfAsCalendar, {holder});
    const std::uint32_t core = call_guest(rt, ctx, 0x08817A54u, {rt.memory().load32(holder)});
    destroy(rt, ctx, holder);
    return View<CalendarData>(rt.memory(), core);
}

// *result = a copy of nil: what a native with too few arguments answers.
void return_nil(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t result, std::uint32_t scratch) {
    (void)call_guest(rt, ctx, kHolderCopy, {scratch, kNil});
    if (result != scratch) (void)call_guest(rt, ctx, kHolderReset, {result, rt.memory().load32(scratch)});
    destroy(rt, ctx, scratch);
}

// The static natives are called (result, argc, argv).
std::int32_t argument_int(psprecomp::Runtime &rt, const psprecomp::AllegrexContext &ctx, std::uint32_t argv, std::uint32_t i) {
    return gt::adhoc::to_int(rt, ctx, rt.memory().load32(argv + 4u * i));
}

} // namespace

// The first day of the calendar: 2009-10-01 as a day number (every day <-> game day conversion uses it).
void func_08817924(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, kDateToDays, {2009u, 10u, 1u}); }

// incDate's engine function: one day later.
void inc_date(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const View<CalendarData> calendar(rt.memory(), ctx.gpr[4]);
    calendar.today() = calendar.today() + 1u;
}

// --- Natives ------------------------------------------------------------------------------------------

// MCalendar.incDate(): one day on the counter.
void MCalendar_incDate(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const View<CalendarData> calendar = engine_of_self(rt, ctx);
    (void)call_guest(rt, ctx, 0x08817A0Cu, {calendar.address()});
}

// MCalendar.getGameDay() -> integer: days since the calendar's first day.
void MCalendar_getGameDay(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const View<CalendarData> calendar = engine_of_self(rt, ctx);
    const std::uint32_t day = calendar.today();
    const std::uint32_t first = call_guest(rt, ctx, 0x08817924u, {});
    GuestFrame frame(ctx, 0x10u);
    gt::adhoc::return_integer(rt, ctx, result, frame.at(0u), day - first);
}

// MCalendar.getEventSize() -> integer: how many events are scheduled.
void MCalendar_getEventSize(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const View<CalendarData> calendar = engine_of_self(rt, ctx);
    const std::int32_t count = calendar.event_size();
    GuestFrame frame(ctx, 0x10u);
    gt::adhoc::return_integer(rt, ctx, result, frame.at(0u), static_cast<std::uint32_t>(count));
}

// MCalendar.dayToWeekDay(day) -> integer: (day - 5) modulo 7 (a signed remainder), nil without an argument.
void MCalendar_dayToWeekDay(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4], argv = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    if (static_cast<std::int32_t>(ctx.gpr[5]) <= 0) return return_nil(rt, ctx, result, frame.at(0u));
    const std::int32_t day = argument_int(rt, ctx, argv, 0u);
    gt::adhoc::return_integer(rt, ctx, result, frame.at(4u), static_cast<std::uint32_t>((day - 5) % 7));
}

// MCalendar.dayToGameDay(day) -> integer: day minus the first day. gameDayToDay is the inverse.
void MCalendar_dayToGameDay(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4], argv = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    if (static_cast<std::int32_t>(ctx.gpr[5]) <= 0) return return_nil(rt, ctx, result, frame.at(0u));
    const std::uint32_t day = static_cast<std::uint32_t>(argument_int(rt, ctx, argv, 0u));
    const std::uint32_t first = call_guest(rt, ctx, 0x08817924u, {});
    gt::adhoc::return_integer(rt, ctx, result, frame.at(4u), day - first);
}
void MCalendar_gameDayToDay(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4], argv = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    if (static_cast<std::int32_t>(ctx.gpr[5]) <= 0) return return_nil(rt, ctx, result, frame.at(0u));
    const std::uint32_t day = static_cast<std::uint32_t>(argument_int(rt, ctx, argv, 0u));
    const std::uint32_t first = call_guest(rt, ctx, 0x08817924u, {});
    gt::adhoc::return_integer(rt, ctx, result, frame.at(4u), day + first);
}

// MCalendar.dateToDay(year, month, day) -> the day number (func_08B1B2F4), nil with fewer than 3 arguments.
void MCalendar_dateToDay(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4], argv = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    if (static_cast<std::int32_t>(ctx.gpr[5]) < 3) return return_nil(rt, ctx, result, frame.at(0u));
    const std::uint32_t year = static_cast<std::uint32_t>(argument_int(rt, ctx, argv, 0u));
    const std::uint32_t month = static_cast<std::uint32_t>(argument_int(rt, ctx, argv, 1u));
    const std::uint32_t day = static_cast<std::uint32_t>(argument_int(rt, ctx, argv, 2u));
    const std::uint32_t number = call_guest(rt, ctx, kDateToDays, {year, month, day});
    const std::uint32_t holder = frame.at(4u);
    (void)call_guest(rt, ctx, kNewDayObject, {holder, number});
    if (result != holder) (void)call_guest(rt, ctx, kHolderReset, {result, rt.memory().load32(holder)});
    destroy(rt, ctx, holder);
}

} // namespace gt::game::calendar

namespace calendar_registrations {
using gt::ReturnKind;

GT_DECOMP(0x08817924u, gt::game::calendar::func_08817924, ReturnKind::Word);
GT_DECOMP(0x08818560u, gt::game::calendar::MCalendar_getGameDay, ReturnKind::Void);
// Not registered yet: the three pad routes never call these (compared 0, calls 0), so the gate can't
// check them; they run as the recompiled originals until a route reaches them.
// GT_DECOMP(0x08817A0Cu, gt::game::calendar::inc_date, ReturnKind::Void);
// GT_DECOMP(0x0881800Cu, gt::game::calendar::MCalendar_incDate, ReturnKind::Void);
// GT_DECOMP(0x088185F4u, gt::game::calendar::MCalendar_getEventSize, ReturnKind::Void);
// GT_DECOMP(0x088187ACu, gt::game::calendar::MCalendar_dayToWeekDay, ReturnKind::Void);
// GT_DECOMP(0x0881888Cu, gt::game::calendar::MCalendar_dayToGameDay, ReturnKind::Void);
// GT_DECOMP(0x08818948u, gt::game::calendar::MCalendar_gameDayToDay, ReturnKind::Void);
// GT_DECOMP(0x08818680u, gt::game::calendar::MCalendar_dateToDay, ReturnKind::Void);
} // namespace calendar_registrations
