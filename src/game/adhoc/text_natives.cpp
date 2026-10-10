// Adhoc natives that return text: spec-database labels, the system's nickname and language, unit
// keywords, money strings, Object.toString. Each builds a std::string (from a C string or a buffer the
// engine fills on the guest stack) and returns it as an Adhoc string (src/game/adhoc/value.cpp).

#include "decomp_registry.hpp"
#include "value.hpp"

namespace gt::adhoc::text {
namespace {

constexpr std::uint32_t kMemset = 0x08B36018u;
constexpr std::uint32_t kSnprintf = 0x08B3767Cu;
constexpr std::uint32_t kHolderReset = 0x08A5BAD8u;
constexpr std::uint32_t kHolderDestroy = 0x08A5B9ECu;

constexpr std::uint32_t kLabelBytes = 0x100u;
constexpr std::uint32_t kNoSuchCar = 0x08C8821Cu;        // "no such car" (MSpecDB)
constexpr std::uint32_t kNoSuchCarParameter = 0x08C88F94u; // "no such car" (MCarParameter's copy)
constexpr std::uint32_t kNoSuchCourse = 0x08C88440u;     // "no such course"
constexpr std::uint32_t kLabelOfCode = 0x0892FFE0u;      // (u64 code, char *buffer, size): car/course label
constexpr std::uint32_t kShortNameOfCode = 0x08930A9Cu;  // (u64 code, char *buffer, size): car short name
constexpr std::uint32_t kCarCode = 0x0883B848u;          // (argv) -> 64-bit car code
constexpr std::uint32_t kStringClass = 0x08A6BF34u;      // () -> Adhoc's string class
constexpr std::uint32_t kIsInstanceOf = 0x08A76F20u;     // (object *, class *) -> bool
constexpr std::uint32_t kSelfAsCarParameter = 0x088547C4u;
constexpr std::uint32_t kCarParameterCore = 0x08854678u;
constexpr std::uint32_t kCarParameterCarCode = 0x088A5EE0u;  // (engine object) -> 64-bit car code
constexpr std::uint32_t kTunerName = 0x0883B7F8u;        // (argc, argv) -> const char *
constexpr std::uint32_t kTunerFormat = 0x08C9C49Cu;      // "tuner_%s"
constexpr std::uint32_t kLanguageName = 0x08B49634u;     // (language) -> const char *
constexpr std::uint32_t kCurrentLanguage = 0x08D69524u;
constexpr std::uint32_t kSystemParamString = 0x08B2ED6Cu;  // thunk to sceUtilityGetSystemParamString(id, buf, size)
constexpr std::uint32_t kNicknameParam = 1u;
constexpr std::uint32_t kDefaultNickname = 0x08C8E644u;
constexpr std::uint32_t kVersionBranch = 0x08ABD9B8u;    // () -> const char *
constexpr std::uint32_t kProductNumberShort = 0x08ABDC6Cu;  // () -> const char *
constexpr std::uint32_t kLengthUnit = 0x08D6950Cu;       // the chosen length unit (index)
constexpr std::uint32_t kPowerUnit = 0x08D69510u;        // the chosen power unit (index)
constexpr std::uint32_t kCourseLengthKeywords = 0x08CDEBBCu;  // const char *[unit]
constexpr std::uint32_t kCourseHeightKeywords = 0x08CDEBB0u;
constexpr std::uint32_t kPowerKeywords = 0x08CDEBD8u;
constexpr std::uint32_t kMoneyString = 0x08ABE53Cu;      // (u64 amount, char *buffer, size)
constexpr std::uint32_t kToLongSlot = 16u;               // u64 toLong(this)

// The label buffer: the default text, then zeros to kLabelBytes (as the originals fill it).
void fill_label(psprecomp::Runtime &rt, std::uint32_t buffer, std::uint32_t text, std::uint32_t text_bytes) {
    for (std::uint32_t i = 0u; i < kLabelBytes; ++i)
        rt.memory().store8(buffer + i, i < text_bytes ? rt.memory().load8(text + i) : 0u);
}

GuestResult to_ulong(psprecomp::Runtime &rt, const psprecomp::AllegrexContext &ctx, std::uint32_t object) {
    return call_guest(rt, ctx, virtual_function(rt, object, kToULongSlot), {object}, {});
}

void return_label(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t result, std::uint32_t lookup,
                  GuestResult code, std::uint32_t buffer) {
    (void)call_guest(rt, ctx, lookup, {code.v0, code.v1, buffer, kLabelBytes});
    return_c_string(rt, ctx, result, buffer);
}

void return_keyword(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t table, std::uint32_t unit) {
    const std::uint32_t result = ctx.gpr[4];
    return_c_string(rt, ctx, result, rt.memory().load32(table + rt.memory().load32(unit) * 4u));
}

} // namespace

// MSpecDB.getCarLabel(code) -> label ("no such car" when unknown). Module function: (result, argc, argv).
void MSpecDB_getCarLabel(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const auto argc = static_cast<std::int32_t>(ctx.gpr[5]);
    const std::uint32_t argv = ctx.gpr[6];
    if (argc <= 0) return;
    GuestFrame frame(ctx, kLabelBytes);
    fill_label(rt, frame.at(0u), kNoSuchCar, 12u);
    return_label(rt, ctx, result, kLabelOfCode, to_ulong(rt, ctx, rt.memory().load32(argv)), frame.at(0u));
}

// MSpecDB.getCourseLabel(course) -> label; a string argument is returned as it is.
void MSpecDB_getCourseLabel(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const auto argc = static_cast<std::int32_t>(ctx.gpr[5]);
    const std::uint32_t argv = ctx.gpr[6];
    if (argc <= 0) return;
    const std::uint32_t course = rt.memory().load32(argv);
    if (call_guest(rt, ctx, kIsInstanceOf, {course, call_guest(rt, ctx, kStringClass, {})}) != 0u) {
        if (argv != result) (void)call_guest(rt, ctx, kHolderReset, {result, course});
        return;
    }
    GuestFrame frame(ctx, kLabelBytes);
    fill_label(rt, frame.at(0u), kNoSuchCourse, 15u);
    return_label(rt, ctx, result, kLabelOfCode, to_ulong(rt, ctx, course), frame.at(0u));
}

// MSpecDB.getCarShortName(car) -> short name.
void MSpecDB_getCarShortName(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const auto argc = static_cast<std::int32_t>(ctx.gpr[5]);
    const std::uint32_t argv = ctx.gpr[6];
    if (argc <= 0) return;
    GuestFrame frame(ctx, kLabelBytes);
    fill_label(rt, frame.at(0u), kNoSuchCar, 12u);
    (void)call_guest(rt, ctx, kCarCode, {argv}, {});  // its result is not used
    return_label(rt, ctx, result, kShortNameOfCode, to_ulong(rt, ctx, rt.memory().load32(argv)), frame.at(0u));
}

// MCarParameter.getCarLabel() -> the label of the parameter's car.
void MCarParameter_getCarLabel(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    std::uint32_t core = 0u;
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, kSelfAsCarParameter, {frame.at(0u)});
        core = call_guest(rt, ctx, kCarParameterCore, {rt.memory().load32(frame.at(0u))});
        (void)call_guest(rt, ctx, kHolderDestroy, {frame.at(0u)});
    }
    GuestFrame frame(ctx, kLabelBytes);
    fill_label(rt, frame.at(0u), kNoSuchCarParameter, 12u);
    return_label(rt, ctx, result, kLabelOfCode, call_guest(rt, ctx, kCarParameterCarCode, {core}, {}), frame.at(0u));
}

// MSpecDB.getTunerCode(tuner...) -> "tuner_<name>".
void MSpecDB_getTunerCode(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const auto argc = static_cast<std::int32_t>(ctx.gpr[5]);
    const std::uint32_t argv = ctx.gpr[6];
    if (argc <= 0) return;
    const std::uint32_t name = call_guest(rt, ctx, kTunerName, {static_cast<std::uint32_t>(argc), argv});
    GuestFrame frame(ctx, kLabelBytes);
    (void)call_guest(rt, ctx, kSnprintf, {frame.at(0u), kLabelBytes, kTunerFormat, name});
    return_c_string(rt, ctx, result, frame.at(0u));
}

// MLocale.getLanguage() -> the current language's name.
void MLocale_getLanguage(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    return_c_string(rt, ctx, result, call_guest(rt, ctx, kLanguageName, {rt.memory().load32(kCurrentLanguage)}));
}

// MSystemUtility.getNickname() -> the system nickname, or a default when it can't be read.
void MSystemUtility_getNickname(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    GuestFrame frame(ctx, 0x90u);
    const std::uint32_t buffer = frame.at(0u);
    (void)call_guest(rt, ctx, kMemset, {buffer, 0u, 0x80u});
    const bool read = call_guest(rt, ctx, kSystemParamString, {kNicknameParam, buffer, 0x80u}) == 0u;
    return_c_string(rt, ctx, result, read ? buffer : kDefaultNickname);
}

// MProductInformation.GetVersionBranch() / GetGameProductNumberShort().
void MProductInformation_GetVersionBranch(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    return_c_string(rt, ctx, result, call_guest(rt, ctx, kVersionBranch, {}));
}
void MProductInformation_GetGameProductNumberShort(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    return_c_string(rt, ctx, result, call_guest(rt, ctx, kProductNumberShort, {}));
}

// MUnit.GetUnitKeywordOfCourseLength/CourseHeight/Power(): the chosen unit's keyword.
void MUnit_GetUnitKeywordOfCourseLength(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    return_keyword(rt, ctx, kCourseLengthKeywords, kLengthUnit);
}
void MUnit_GetUnitKeywordOfCourseHeight(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    return_keyword(rt, ctx, kCourseHeightKeywords, kLengthUnit);
}
void MUnit_GetUnitKeywordOfPower(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    return_keyword(rt, ctx, kPowerKeywords, kPowerUnit);
}

// MMisc.GetMoneyString(amount) -> the amount formatted as money.
void MMisc_GetMoneyString(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const auto argc = static_cast<std::int32_t>(ctx.gpr[5]);
    const std::uint32_t argv = ctx.gpr[6];
    if (argc <= 0) return;
    const std::uint32_t amount = rt.memory().load32(argv);
    const GuestResult value = call_guest(rt, ctx, virtual_function(rt, amount, kToLongSlot), {amount}, {});
    GuestFrame frame(ctx, 0x50u);
    (void)call_guest(rt, ctx, kMoneyString, {value.v0, value.v1, frame.at(0u), 0x40u});
    return_c_string(rt, ctx, result, frame.at(0u));
}

// Object.toString() -> self as a string.
void Object_toString(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t self = rt.memory().load32(ctx.gpr[5]);
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t string = frame.at(0u);
    to_string(rt, ctx, self, string);
    return_string(rt, ctx, result, string);
    string_release(rt, ctx, rt.memory().load32(string));
}

} // namespace gt::adhoc::text

GT_DECOMP(0x0883BBD8u, gt::adhoc::text::MSpecDB_getCarLabel, gt::ReturnKind::Void);
GT_DECOMP(0x0883E534u, gt::adhoc::text::MSpecDB_getCourseLabel, gt::ReturnKind::Void);
GT_DECOMP(0x0883C1C8u, gt::adhoc::text::MSpecDB_getCarShortName, gt::ReturnKind::Void);
GT_DECOMP(0x088557B4u, gt::adhoc::text::MCarParameter_getCarLabel, gt::ReturnKind::Void);
GT_DECOMP(0x0883DE98u, gt::adhoc::text::MSpecDB_getTunerCode, gt::ReturnKind::Void);
GT_DECOMP(0x08A3D61Cu, gt::adhoc::text::MLocale_getLanguage, gt::ReturnKind::Void);
GT_DECOMP(0x08A2B928u, gt::adhoc::text::MSystemUtility_getNickname, gt::ReturnKind::Void);
GT_DECOMP(0x08A2868Cu, gt::adhoc::text::MProductInformation_GetVersionBranch, gt::ReturnKind::Void);
GT_DECOMP(0x08A28E18u, gt::adhoc::text::MProductInformation_GetGameProductNumberShort, gt::ReturnKind::Void);
GT_DECOMP(0x08A25EF0u, gt::adhoc::text::MUnit_GetUnitKeywordOfCourseLength, gt::ReturnKind::Void);
GT_DECOMP(0x08A25E34u, gt::adhoc::text::MUnit_GetUnitKeywordOfCourseHeight, gt::ReturnKind::Void);
GT_DECOMP(0x08A26068u, gt::adhoc::text::MUnit_GetUnitKeywordOfPower, gt::ReturnKind::Void);
GT_DECOMP(0x08A26C38u, gt::adhoc::text::MMisc_GetMoneyString, gt::ReturnKind::Void);
GT_DECOMP(0x08A7928Cu, gt::adhoc::text::Object_toString, gt::ReturnKind::Void);
