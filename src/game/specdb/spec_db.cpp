// SpecDB: the engine and natives of MSpecDB's simple reads — a car's price, year and category, a course's
// condition and entry limit, table sizes, and the 64-bit code helpers (layout and evidence:
// include/gt/game/spec_db.hpp; names from GT's registration, config/symbol_addrs.txt).
//
// Engine functions are named after the MSpecDB native that calls them (`get_car_price` for the engine
// function of MSpecDB::getCarPrice); the natives themselves are `MSpecDB_<method>`. The functions with
// no script name keep func_XXXXXXXX. Registered functions replace the recompiled originals; the ones
// commented out at the end are never called by the pad routes, so the gate can't verify them.
//
// Modding: prices, years and course conditions come out of these readers; a mod that wants, say, every
// car cheaper or every course dry changes the reader (get_car_price, get_course_condition). Keep the
// original behaviour for the differential gate and make such changes opt-in.

#include "decomp_registry.hpp"
#include "gt/game/spec_db.hpp"
#include "guest_call.hpp"
#include "../adhoc/value.hpp"

#include <cstdint>

namespace gt::game::spec_db {
namespace {

using guest::View;

constexpr std::uint32_t kHolderReset = 0x08A5BAD8u;
constexpr std::uint32_t kHolderDestroy = 0x08A5B9ECu;
constexpr std::uint32_t kCarLabelCount = 0x08931090u;        // () -> int
constexpr std::uint32_t kCarCode = 0x0883B848u;              // (argv) -> 64-bit car code
constexpr std::uint32_t kCourseCode = 0x0883B950u;           // (argv) -> 64-bit course code
constexpr std::uint32_t kCarCategory = 0x08930D98u;          // (u64 car code) -> category
constexpr std::uint32_t kCourseCondition = 0x0893069Cu;      // (u64 course code) -> condition
constexpr std::uint32_t kCarPrice = 0x08930E08u;             // (u64 car code) -> price in credits
constexpr std::uint32_t kCarSpecConstruct = 0x0893600Cu;     // (spec *) — a 0x188-byte stack object
constexpr std::uint32_t kCarSpecLoad = 0x08935B10u;          // (spec *, -, u64 car code)
constexpr std::uint32_t kCarSpecVariation = 0x0893273Cu;     // (spec *) -> variation (colour count)
constexpr std::uint32_t kCarSpecDestroy = 0x08932448u;       // (spec *)
constexpr std::uint32_t kSnprintf = 0x08B3767Cu;             // src/libc (snprintf)
constexpr std::uint32_t kMemset = 0x08B36018u;               // src/libc (memset)
constexpr std::uint32_t kNoSuchCar = 0x08C8821Cu;            // "no such car" (12 bytes with its NUL)
constexpr std::uint32_t kNoSuchCarColor = 0x08C9C434u;       // "no such car ..." (colour-name fallback)
constexpr std::uint32_t kCarName = 0x089311ECu;              // (u64 car code, char *buffer, size)
constexpr std::uint32_t kCarSpecSetColor = 0x08932748u;      // (spec *, colour index)
constexpr std::uint32_t kCarSpecColorName = 0x08936268u;     // (spec *) -> const char * or NULL
constexpr std::uint32_t kCatalogNone = 0x08C9C45Cu;          // "---"
constexpr std::uint32_t kCatalogPlain = 0x08C9C46Cu;         // "%d"
constexpr std::uint32_t kCatalogKg = 0x08C9C47Cu;            // "%d kg"
constexpr std::uint32_t kCatalogMm = 0x08C9C484u;            // "%d mm"
constexpr std::uint32_t kCarSpecMass = 0x08932AECu;          // (spec *) -> kg
constexpr std::uint32_t kCarSpecLength = 0x08932A38u;        // (spec *) -> mm
constexpr std::uint32_t kCarSpecHeight = 0x08932A74u;        // (spec *) -> mm
constexpr std::uint32_t kCarSpecWidth = 0x08932AB0u;         // (spec *) -> mm
constexpr std::uint32_t kRegistry = 0x089363FCu;               // () -> the database registry
constexpr std::uint32_t kLookupId = 0x08936578u;               // (registry, _, code low, code high) -> id
constexpr std::uint32_t kLookupTable = 0x0893656Cu;            // (registry, _, code low, code high) -> table number
constexpr std::uint32_t kLookup = 0x08936584u;                 // (registry, _, low, high, record *) -> found
constexpr std::uint32_t kLookupInTable = 0x08936544u;          // (registry, id, table, record *) -> found
constexpr std::uint32_t kNewIdObject = 0x08A5FA8Cu;            // (holder *out, value): the Adhoc value of an id
constexpr std::uint32_t kCarTable = 0u;                        // CarCode32ToCarCode64 appends it
constexpr std::uint32_t kCourseTable = 34u;                    // CourseCode32ToCourseCode64 appends it
constexpr std::uint32_t kGenericCarTable = 38u;                // car codes of this table resolve through another code

void destroy(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t holder) {
    (void)call_guest(rt, ctx, kHolderDestroy, {holder});
}

// *result = the holder `scratch` holds (the holder assignment skips self-assignment), then scratch dies.
void hand_back(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t result, std::uint32_t scratch) {
    if (result != scratch) (void)call_guest(rt, ctx, kHolderReset, {result, rt.memory().load32(scratch)});
    destroy(rt, ctx, scratch);
}

// Copies `words` words from `from` to `to` (the original loads them all, then stores them).
void copy_words(psprecomp::Runtime &rt, std::uint32_t from, std::uint32_t to, std::uint32_t words) {
    for (std::uint32_t i = 0; i != words; ++i) rt.memory().store32(to + 4u * i, rt.memory().load32(from + 4u * i));
}

// The static natives are called (result, argc, argv).
std::uint32_t argv_of(psprecomp::AllegrexContext &ctx) { return ctx.gpr[6]; }

} // namespace

// --- Engine: the codes and the records ----------------------------------------------------------------

// extractTableNumber / extractID (0x089304C0 / 0x089304C8): a code is (id, table number) as the (low, high)
// words; both are written as return-argument leaves in src/core/leaves.cpp.
// CarCode32ToCarCode64 (a car code is the id in table 0) and CourseCode32ToCourseCode64 (table 34).
void car_code_32_to_64(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = ctx.gpr[4];
    ctx.gpr[3] = kCarTable;
}
void course_code_32_to_64(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = ctx.gpr[4];
    ctx.gpr[3] = 0u | kCourseTable;
}

// The record of the car code (low, high) in a2: 1 when found (a code of the generic table resolves through
// another code first), 0 otherwise. The caller's record (a2) is filled by the table's database object.
void func_08930B60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t low = ctx.gpr[4], high = ctx.gpr[5];
    const std::uint32_t record = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    std::uint32_t registry = call_guest(rt, ctx, kRegistry, {});
    if (call_guest(rt, ctx, kLookupTable, {registry, 0u, low, high}) == kGenericCarTable) {
        registry = call_guest(rt, ctx, kRegistry, {});
        if (call_guest(rt, ctx, kLookup, {registry, 0u, low, high, frame.at(0u)}) == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
        low = rt.memory().load32(frame.at(0u));
        high = rt.memory().load32(frame.at(4u));
    }
    registry = call_guest(rt, ctx, kRegistry, {});
    ctx.gpr[2] = call_guest(rt, ctx, kLookup, {registry, 0u, low, high, record}) != 0u ? 1u : 0u;
}

// The record of the car code (a0:a1) copied to a2 (32 bytes); 1 when found.
void func_08930D20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    const bool found = call_guest(rt, ctx, 0x08930B60u, {ctx.gpr[4], ctx.gpr[5], frame.at(0u)}) != 0u;
    if (found) copy_words(rt, frame.at(0u), out, 8u);
    ctx.gpr[2] = found ? 1u : 0u;
}

// The record of the course code (a0:a1) copied to a2 (28 bytes); 1 when found.
void func_089305CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t low = ctx.gpr[4], high = ctx.gpr[5], out = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t id = call_guest(rt, ctx, kLookupId, {call_guest(rt, ctx, kRegistry, {}), 0u, low, high});
    const bool found = call_guest(rt, ctx, kLookupInTable, {call_guest(rt, ctx, kRegistry, {}), id, kCourseTable, frame.at(0u)}) != 0u;
    if (found) copy_words(rt, frame.at(0u), out, 7u);
    ctx.gpr[2] = found ? 1u : 0u;
}

// --- Engine: one field of a record each ----------------------------------------------------------------

namespace {
View<CarSpecRecord> car_record(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, const GuestFrame &frame) {
    (void)call_guest(rt, ctx, 0x08930D20u, {ctx.gpr[4], ctx.gpr[5], frame.at(0u), ctx.gpr[7]});
    return View<CarSpecRecord>(rt.memory(), frame.at(0u));
}
View<CourseRecord> course_record(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, const GuestFrame &frame) {
    (void)call_guest(rt, ctx, 0x089305CCu, {ctx.gpr[4], ctx.gpr[5], frame.at(0u), ctx.gpr[7]});
    return View<CourseRecord>(rt.memory(), frame.at(0u));
}
} // namespace

void get_car_price(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    ctx.gpr[2] = car_record(rt, ctx, frame).price();
}
// getCarSellPrice: the price divided by 4 (rounded toward zero).
void get_car_sell_price(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const auto price = static_cast<std::int32_t>(car_record(rt, ctx, frame).price().value());
    ctx.gpr[2] = static_cast<std::uint32_t>(price / 4);
}
void get_car_year(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    ctx.gpr[2] = static_cast<std::uint32_t>(static_cast<std::int32_t>(car_record(rt, ctx, frame).year().value()));
}
void get_car_country(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    ctx.gpr[2] = car_record(rt, ctx, frame).country();
}
void get_car_category(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    ctx.gpr[2] = car_record(rt, ctx, frame).category();
}
void get_course_max_entry(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    ctx.gpr[2] = course_record(rt, ctx, frame).max_entry();
}
void get_course_condition(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    ctx.gpr[2] = course_record(rt, ctx, frame).condition();
}
// getCarLabelCount: the entry count of table 0's database object, or -1 without it.
void get_car_label_count(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x08931044u, {0u}); }

// --- Natives --------------------------------------------------------------------------------------------

namespace {

// A native taking a car code: car code of the argument, `engine(code)`, the integer back.
void car_int_native(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t engine) {
    const std::uint32_t result = ctx.gpr[4];
    if (static_cast<std::int32_t>(ctx.gpr[5]) <= 0) return;
    GuestFrame frame(ctx, 0x20u);
    const GuestResult code = call_guest(rt, ctx, kCarCode, {argv_of(ctx)}, {});
    const std::uint32_t value = call_guest(rt, ctx, engine, {code.v0, code.v1});
    gt::adhoc::return_integer(rt, ctx, result, frame.at(0u), value);
}
// The same for a course code.
void course_int_native(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t engine) {
    const std::uint32_t result = ctx.gpr[4];
    if (static_cast<std::int32_t>(ctx.gpr[5]) <= 0) return;
    GuestFrame frame(ctx, 0x20u);
    const GuestResult code = call_guest(rt, ctx, kCourseCode, {argv_of(ctx)}, {});
    const std::uint32_t value = call_guest(rt, ctx, engine, {code.v0, code.v1});
    gt::adhoc::return_integer(rt, ctx, result, frame.at(0u), value);
}
// A table size: the engine function fills it through a pointer.
void size_native(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t engine) {
    const std::uint32_t result = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    rt.memory().store32(frame.at(4u), 0u);
    (void)call_guest(rt, ctx, engine, {frame.at(4u)});
    gt::adhoc::return_integer(rt, ctx, result, frame.at(0u), rt.memory().load32(frame.at(4u)));
}
// The 64-bit value of the argument object (its toULong()).
GuestResult unsigned_long_of(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object) {
    return call_guest(rt, ctx, gt::adhoc::virtual_function(rt, object, gt::adhoc::kToULongSlot), {object}, {});
}

} // namespace

void MSpecDB_getCarSellPrice(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { car_int_native(rt, ctx, 0x08930DD8u); }
void MSpecDB_getCarYear(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { car_int_native(rt, ctx, 0x08930E28u); }
void MSpecDB_getCourseMaxEntry(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { course_int_native(rt, ctx, 0x0893067Cu); }
void MSpecDB_getMakerSize(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { size_native(rt, ctx, 0x0892C890u); }
void MSpecDB_getTunerSize(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { size_native(rt, ctx, 0x0892C8C8u); }



// canRun(car code, course code) -> true, unless the car is flagged (its unk_16 test) and the course is dirt.
void MSpecDB_canRun(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4], argv = argv_of(ctx);
    if (static_cast<std::int32_t>(ctx.gpr[5]) < 2) return;
    GuestFrame frame(ctx, 0x20u);
    const GuestResult car = call_guest(rt, ctx, kCarCode, {argv}, {});
    std::uint32_t allowed = 1u;
    if (call_guest(rt, ctx, 0x0892C810u, {0x08C9C4F8u, 0u, car.v0, car.v1}) != 0u) {
        const GuestResult course = call_guest(rt, ctx, kCourseCode, {argv + 4u}, {});
        const std::uint32_t condition = call_guest(rt, ctx, 0x0893069Cu, {course.v0, course.v1});
        allowed = condition - 2u < 2u ? 0u : 1u;
    }
    gt::adhoc::return_bool(rt, ctx, result, frame.at(0u), allowed);
}

// extractTableNumber(code) / extractID(code) -> the high / low word of the code.
void MSpecDB_extractTableNumber(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    if (static_cast<std::int32_t>(ctx.gpr[5]) <= 0) return;
    GuestFrame frame(ctx, 0x20u);
    const GuestResult code = unsigned_long_of(rt, ctx, rt.memory().load32(argv_of(ctx)));
    (void)call_guest(rt, ctx, kNewIdObject, {frame.at(0u), call_guest(rt, ctx, 0x089304C0u, {code.v0, code.v1})});
    hand_back(rt, ctx, result, frame.at(0u));
}
void MSpecDB_extractID(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    if (static_cast<std::int32_t>(ctx.gpr[5]) <= 0) return;
    GuestFrame frame(ctx, 0x20u);
    const GuestResult code = unsigned_long_of(rt, ctx, rt.memory().load32(argv_of(ctx)));
    (void)call_guest(rt, ctx, kNewIdObject, {frame.at(0u), call_guest(rt, ctx, 0x089304C8u, {code.v0, code.v1})});
    hand_back(rt, ctx, result, frame.at(0u));
}

// CarCode32ToCarCode64(id) / CourseCode32ToCourseCode64(id) -> the 64-bit code (id, table).
void MSpecDB_CarCode32ToCarCode64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    if (static_cast<std::int32_t>(ctx.gpr[5]) <= 0) return;
    GuestFrame frame(ctx, 0x20u);
    const auto id = static_cast<std::uint32_t>(gt::adhoc::to_int(rt, ctx, rt.memory().load32(argv_of(ctx))));
    const GuestResult code = call_guest(rt, ctx, 0x089300C0u, {id}, {});
    gt::adhoc::return_long(rt, ctx, result, frame.at(0u), code.v0, code.v1);
}
void MSpecDB_CourseCode32ToCourseCode64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    if (static_cast<std::int32_t>(ctx.gpr[5]) <= 0) return;
    GuestFrame frame(ctx, 0x20u);
    const auto id = static_cast<std::uint32_t>(gt::adhoc::to_int(rt, ctx, rt.memory().load32(argv_of(ctx))));
    const GuestResult code = call_guest(rt, ctx, 0x089300B0u, {id}, {});
    gt::adhoc::return_long(rt, ctx, result, frame.at(0u), code.v0, code.v1);
}


// --- Natives that look up a car or a course ----------------------------------------------------------

using namespace gt::adhoc;

// MSpecDB.getCarLabelCount() -> integer.
void MSpecDB_getCarLabelCount(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    return_integer(rt, ctx, result, frame.at(0u), call_guest(rt, ctx, kCarLabelCount, {}));
}

// MSpecDB.getCarCode(label) -> 64-bit car code (module function: result, argc, argv).
void MSpecDB_getCarCode(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const auto argc = static_cast<std::int32_t>(ctx.gpr[5]);
    const std::uint32_t argv = ctx.gpr[6];
    if (argc <= 0) return;
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t value = frame.at(0u);
    const GuestResult code = call_guest(rt, ctx, kCarCode, {argv}, {});
    return_long(rt, ctx, result, value, code.v0, code.v1);
}

// MSpecDB.getCarCategory(car) / getCourseCondition(course) / isDirt(course): look the code up and
// return the integer (isDirt: whether the course condition is 2 or 3). Module functions:
// (result, argc, argv); nothing happens without an argument.
void spec_lookup(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t code_of, std::uint32_t lookup,
                 bool dirt) {
    const std::uint32_t result = ctx.gpr[4];
    const auto argc = static_cast<std::int32_t>(ctx.gpr[5]);
    const std::uint32_t argv = ctx.gpr[6];
    if (argc <= 0) return;
    GuestFrame frame(ctx, 0x20u);
    const GuestResult code = call_guest(rt, ctx, code_of, {argv}, {});
    const std::uint32_t value = call_guest(rt, ctx, lookup, {code.v0, code.v1});
    if (dirt) return_bool(rt, ctx, result, frame.at(0u), value - 2u < 2u ? 1u : 0u);
    else return_integer(rt, ctx, result, frame.at(0u), value);
}

void MSpecDB_getCarCategory(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { spec_lookup(rt, ctx, kCarCode, kCarCategory, false); }

void MSpecDB_getCourseCondition(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { spec_lookup(rt, ctx, kCourseCode, kCourseCondition, false); }

void MSpecDB_isDirt(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { spec_lookup(rt, ctx, kCourseCode, kCourseCondition, true); }

void MSpecDB_getCarPrice(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { spec_lookup(rt, ctx, kCarCode, kCarPrice, false); }

// MSpecDB.getCarVariation(car) -> the car's variation count, read from a spec record built on the
// stack (constructed, loaded for the car code, read, destroyed). Module function: (result, argc, argv).
void MSpecDB_getCarVariation(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const auto argc = static_cast<std::int32_t>(ctx.gpr[5]);
    const std::uint32_t argv = ctx.gpr[6];
    if (argc <= 0) return;
    GuestFrame frame(ctx, 0x1A0u);
    const std::uint32_t spec = frame.at(8u);
    (void)call_guest(rt, ctx, kCarSpecConstruct, {spec});
    const GuestResult code = call_guest(rt, ctx, kCarCode, {argv}, {});
    (void)call_guest(rt, ctx, kCarSpecLoad, {spec, ctx.gpr[5], code.v0, code.v1});
    const std::uint32_t variation = call_guest(rt, ctx, kCarSpecVariation, {spec});
    return_integer(rt, ctx, result, frame.at(0u), variation);
    (void)call_guest(rt, ctx, kCarSpecDestroy, {spec});
}

// MSpecDB.getCarCatalogMs/Length/Height/Width(car[, with_unit]) -> the catalogue sheet's text for one
// value of the car: "---" when it is 0, otherwise "%d" plus the unit unless toInt(argv[1]) is 0. Module
// functions: (result, argc, argv); nothing happens without an argument.
void car_catalog_text(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t read,
                      std::uint32_t with_unit) {
    const std::uint32_t result = ctx.gpr[4];
    const auto argc = static_cast<std::int32_t>(ctx.gpr[5]);
    const std::uint32_t argv = ctx.gpr[6];
    if (argc <= 0) return;
    GuestFrame frame(ctx, 0x230u);
    const std::uint32_t text = frame.at(0xCu);
    const std::uint32_t spec = frame.at(0x90u);
    (void)call_guest(rt, ctx, kCarSpecConstruct, {spec});
    const GuestResult code = call_guest(rt, ctx, kCarCode, {argv}, {});
    (void)call_guest(rt, ctx, kCarSpecLoad, {spec, ctx.gpr[5], code.v0, code.v1});
    const std::uint32_t value = call_guest(rt, ctx, read, {spec});
    if (value == 0u) {
        for (std::uint32_t i = 0u; i < 4u; ++i) rt.memory().store8(text + i, rt.memory().load8(kCatalogNone + i));
    } else {
        bool unit = true;
        if (argc != 1) {
            const std::uint32_t flag = rt.memory().load32(argv + 4u);
            unit = call_guest(rt, ctx, virtual_function(rt, flag, kToIntSlot), {flag}) != 0u;
        }
        (void)call_guest(rt, ctx, kSnprintf, {text, 0x80u, unit ? with_unit : kCatalogPlain, value});
    }
    return_c_string(rt, ctx, result, text);
    (void)call_guest(rt, ctx, kCarSpecDestroy, {spec});
}

void MSpecDB_getCarCatalogMs(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { car_catalog_text(rt, ctx, kCarSpecMass, kCatalogKg); }

void MSpecDB_getCarCatalogLength(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { car_catalog_text(rt, ctx, kCarSpecLength, kCatalogMm); }

void MSpecDB_getCarCatalogHeight(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { car_catalog_text(rt, ctx, kCarSpecHeight, kCatalogMm); }

void MSpecDB_getCarCatalogWidth(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { car_catalog_text(rt, ctx, kCarSpecWidth, kCatalogMm); }

// MSpecDB.getCarName(car) -> the car's name; "no such car" when the lookup leaves the buffer alone.
// Module function: (result, argc, argv); nothing happens without an argument.
void MSpecDB_getCarName(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const auto argc = static_cast<std::int32_t>(ctx.gpr[5]);
    const std::uint32_t argv = ctx.gpr[6];
    if (argc <= 0) return;
    GuestFrame frame(ctx, 0x130u);
    const std::uint32_t name = frame.at(0xCu);  // char[0x100]
    for (std::uint32_t i = 0u; i < 12u; i += 4u) rt.memory().store32(name + i, rt.memory().load32(kNoSuchCar + i));
    (void)call_guest(rt, ctx, kMemset, {name + 12u, 0u, 0x100u - 12u});
    const GuestResult code = call_guest(rt, ctx, kCarCode, {argv}, {});
    (void)call_guest(rt, ctx, kCarName, {code.v0, code.v1, name, 0x100u});
    return_c_string(rt, ctx, result, name);
}

// MSpecDB.getCarColorName(car, colour) -> the name of the car's colour `toInt(argv[1])`, or a
// "no such car" text when there is none. Module function: (result, argc, argv); needs 2 arguments.
void MSpecDB_getCarColorName(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const auto argc = static_cast<std::int32_t>(ctx.gpr[5]);
    const std::uint32_t argv = ctx.gpr[6];
    if (argc < 2) return;
    GuestFrame frame(ctx, 0x1B0u);
    const std::uint32_t spec = frame.at(0x10u);
    (void)call_guest(rt, ctx, kCarSpecConstruct, {spec});
    const GuestResult code = call_guest(rt, ctx, kCarCode, {argv}, {});
    (void)call_guest(rt, ctx, kCarSpecLoad, {spec, ctx.gpr[5], code.v0, code.v1});
    const std::uint32_t colour_object = rt.memory().load32(argv + 4u);
    const std::uint32_t colour = call_guest(rt, ctx, virtual_function(rt, colour_object, kToIntSlot), {colour_object});
    (void)call_guest(rt, ctx, kCarSpecSetColor, {spec, colour});
    const std::uint32_t colour_name = call_guest(rt, ctx, kCarSpecColorName, {spec});
    return_c_string(rt, ctx, result, colour_name != 0u ? colour_name : kNoSuchCarColor);
    (void)call_guest(rt, ctx, kCarSpecDestroy, {spec});
}

} // namespace gt::game::spec_db

namespace spec_db_registrations {
using gt::ReturnKind;
#define SPEC_DB(address, name, kind) GT_DECOMP_NESTED(address, gt::game::spec_db::name, ReturnKind::kind)

SPEC_DB(0x089300C0u, car_code_32_to_64, DoubleWord);
SPEC_DB(0x0893067Cu, get_course_max_entry, Word);
SPEC_DB(0x0893069Cu, get_course_condition, Word);
SPEC_DB(0x08840F04u, MSpecDB_canRun, Void);
SPEC_DB(0x0883BD04u, MSpecDB_getCarLabelCount, Void);
SPEC_DB(0x0883BF90u, MSpecDB_getCarCode, Void);
SPEC_DB(0x0883D9E8u, MSpecDB_getCarCategory, Void);
SPEC_DB(0x0883F948u, MSpecDB_getCourseCondition, Void);
SPEC_DB(0x08840FA8u, MSpecDB_isDirt, Void);
SPEC_DB(0x0883D928u, MSpecDB_getCarPrice, Void);
SPEC_DB(0x0883C2FCu, MSpecDB_getCarVariation, Void);
SPEC_DB(0x0883CE74u, MSpecDB_getCarCatalogMs, Void);
SPEC_DB(0x0883C0BCu, MSpecDB_getCarName, Void);
SPEC_DB(0x0883C390u, MSpecDB_getCarColorName, Void);
SPEC_DB(0x0883CFFCu, MSpecDB_getCarCatalogLength, Void);
SPEC_DB(0x0883D184u, MSpecDB_getCarCatalogHeight, Void);
SPEC_DB(0x0883D30Cu, MSpecDB_getCarCatalogWidth, Void);
// Not registered yet: the three pad routes never call these (compared 0, calls 0), so the gate can't
// check them; they run as the recompiled originals until a route reaches them.
// SPEC_DB(0x089300B0u, course_code_32_to_64, Word);
// SPEC_DB(0x08930DD8u, get_car_sell_price, Word);
// SPEC_DB(0x08930E28u, get_car_year, Word);
// SPEC_DB(0x08930E48u, get_car_country, Word);
// SPEC_DB(0x0883F8E8u, MSpecDB_getCourseMaxEntry, Void);
// SPEC_DB(0x0883D988u, MSpecDB_getCarSellPrice, Void);
// SPEC_DB(0x0883D8C8u, MSpecDB_getCarYear, Void);
// SPEC_DB(0x08840278u, MSpecDB_getMakerSize, Void);
// SPEC_DB(0x08840780u, MSpecDB_getTunerSize, Void);
// SPEC_DB(0x08840C7Cu, MSpecDB_extractTableNumber, Void);
// SPEC_DB(0x08840D00u, MSpecDB_extractID, Void);
// SPEC_DB(0x0883DA48u, MSpecDB_CarCode32ToCarCode64, Void);
// SPEC_DB(0x0883E924u, MSpecDB_CourseCode32ToCourseCode64, Void);
// Reached inside the natives' own cases (some also directly): verified since nested callees run decompiled in
// a case's decompiled run (2026-10-13; 0 mismatches, sabotage pinned on each).
SPEC_DB(0x08930B60u, func_08930B60, Word);
SPEC_DB(0x08930D20u, func_08930D20, Word);
SPEC_DB(0x089305CCu, func_089305CC, Word);
SPEC_DB(0x08930E08u, get_car_price, Word);
// Unregistered after review (PR #129) for calls 0; registered again 2026-10-13, verified nested in the natives'
// cases (get_car_category was rewritten when it moved here from hot_leaves.cpp).
SPEC_DB(0x08930D98u, get_car_category, Word);
SPEC_DB(0x08931090u, get_car_label_count, Word);
#undef SPEC_DB
} // namespace spec_db_registrations
