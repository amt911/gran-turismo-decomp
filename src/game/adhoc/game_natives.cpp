// Adhoc natives of GT's game-side classes (names from GT's registrations, config/symbol_addrs.txt).
// Same shape as the UI natives: fetch self as the engine class (helpers take the native's self in
// a1, left untouched here unless an argument is converted instead), call the engine, drop holders.

#include "decomp_registry.hpp"
#include "value.hpp"

namespace gt::adhoc::game {
namespace {

constexpr std::uint32_t kHolderReset = 0x08A5BAD8u;
constexpr std::uint32_t kHolderDestroy = 0x08A5B9ECu;

constexpr std::uint32_t kSelfAsWatcher = 0x08973EF8u;        // (holder *out) [a1 = self]
constexpr std::uint32_t kWatcherAppend = 0x08973DD0u;        // (watcher *)
constexpr std::uint32_t kWatcherRemoveSlot = 66u;            // watcher->remove()
constexpr std::uint32_t kAsOrganizer = 0x088297A8u;          // (holder *out, self or argument)
constexpr std::uint32_t kOrganizerCore = 0x088294D0u;        // (organizer *) -> its engine object
constexpr std::uint32_t kOrganizerStartLoop = 0x08860058u;   // (engine object)
constexpr std::uint32_t kArgumentAsRaceSound = 0x08835BD8u;  // (holder *out, argv)
constexpr std::uint32_t kOrganizerSetRaceSound = 0x08829538u;   // (organizer *, holder *)
constexpr std::uint32_t kArgumentAsSoundContext = 0x08A22050u;  // (holder *out, argv)
constexpr std::uint32_t kOrganizerSetSoundContext = 0x088294D8u; // (organizer *, holder *)
constexpr std::uint32_t kSelfAsRaceOperator = 0x088248E4u;   // (holder *out) [a1 = self]
constexpr std::uint32_t kRaceOperatorSetOrganizer = 0x0885BB9Cu; // (operator engine object, organizer object)
constexpr std::uint32_t kCarLabelCount = 0x08931090u;        // () -> int
constexpr std::uint32_t kCarCode = 0x0883B848u;              // (argv) -> 64-bit car code
constexpr std::uint32_t kNewLongObject = 0x08A5DEE8u;        // (holder *out, -, 64-bit value in a2:a3)
constexpr std::uint32_t kGameRecordStructure = 0x08CE28E4u;  // global: the game-record structure object
constexpr std::uint32_t kHolderCopyRecord = 0x08A53224u;     // holder copy (an instance used here)
constexpr std::uint32_t kArgumentAsRecord = 0x08A57854u;     // (holder *out, argv): argument as STRecord
constexpr std::uint32_t kInitializeGameRecord = 0x08805D9Cu; // (record data)
constexpr std::uint32_t kEngineObject = 0x14u;               // MRaceOperator: its engine object

void destroy(psprecomp::Runtime &rt, const psprecomp::AllegrexContext &ctx, std::uint32_t holder) {
    (void)call_guest(rt, ctx, kHolderDestroy, {holder});
}

// self as `cls` into a frame holder, `use(object)`, then drop the holder.
template <typename Use>
void with_self(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t self_as, Use use) {
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t holder = frame.at(0u);
    (void)call_guest(rt, ctx, self_as, {holder});
    use(rt.memory().load32(holder));  // NULL is not checked (the originals only report it)
    destroy(rt, ctx, holder);
}

} // namespace

void MWatcher_append(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    with_self(rt, ctx, kSelfAsWatcher, [&](std::uint32_t watcher) { (void)call_guest(rt, ctx, kWatcherAppend, {watcher}); });
}

void MWatcher_remove(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    with_self(rt, ctx, kSelfAsWatcher, [&](std::uint32_t watcher) {
        (void)call_guest(rt, ctx, virtual_function(rt, watcher, kWatcherRemoveSlot), {watcher});
    });
}

// MOrganizer.startGameObjectLoop(): the holder is dropped before the engine call. (initialize(),
// 0x0882A638, has the same shape but creates threads, which a harness case can't replay: it stays
// recompiled until it can be compared.)
void organizer_call(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t engine_call) {
    std::uint32_t core = 0u;
    with_self(rt, ctx, kAsOrganizer, [&](std::uint32_t organizer) { core = call_guest(rt, ctx, kOrganizerCore, {organizer}); });
    (void)call_guest(rt, ctx, engine_call, {core});
}
void MOrganizer_startGameObjectLoop(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { organizer_call(rt, ctx, kOrganizerStartLoop); }

// MOrganizer.setRaceSound(sound) / setSoundContext(context): with at least one argument.
void organizer_set(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t convert, std::uint32_t set) {
    const std::uint32_t self = ctx.gpr[5];
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    if (argc <= 0) return;
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t organizer = frame.at(0x0u);
    const std::uint32_t value = frame.at(0x4u);
    (void)call_guest(rt, ctx, convert, {value, argv});
    (void)call_guest(rt, ctx, kAsOrganizer, {organizer, self});
    (void)call_guest(rt, ctx, set, {rt.memory().load32(organizer), value});
    destroy(rt, ctx, organizer);
    destroy(rt, ctx, value);
}
void MOrganizer_setRaceSound(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    organizer_set(rt, ctx, kArgumentAsRaceSound, kOrganizerSetRaceSound);
}
void MOrganizer_setSoundContext(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    organizer_set(rt, ctx, kArgumentAsSoundContext, kOrganizerSetSoundContext);
}

// MRaceOperator.setOrganizer(organizer): with at least one argument.
void MRaceOperator_setOrganizer(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    if (argc <= 0) return;
    std::uint32_t engine = 0u;
    with_self(rt, ctx, kSelfAsRaceOperator, [&](std::uint32_t op) { engine = rt.memory().load32(op + kEngineObject); });
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t organizer = frame.at(0x0u);
    (void)call_guest(rt, ctx, kAsOrganizer, {organizer, argv});
    const std::uint32_t core = call_guest(rt, ctx, kOrganizerCore, {rt.memory().load32(organizer)});
    (void)call_guest(rt, ctx, kRaceOperatorSetOrganizer, {engine, core});
    destroy(rt, ctx, organizer);
}

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
    (void)call_guest(rt, ctx, kNewLongObject, {value, ctx.gpr[5], code.v0, code.v1});
    if (result != value) (void)call_guest(rt, ctx, kHolderReset, {result, rt.memory().load32(value)});
    destroy(rt, ctx, value);
}

// GameRecord.CreateGameRecordStructure() -> the global game-record structure.
void CreateGameRecordStructure(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t raw = frame.at(0x0u);
    const std::uint32_t value = frame.at(0x4u);
    rt.memory().store32(raw, rt.memory().load32(kGameRecordStructure));
    (void)call_guest(rt, ctx, kHolderCopyRecord, {value, raw});
    if (result != value) (void)call_guest(rt, ctx, kHolderReset, {result, rt.memory().load32(value)});
    destroy(rt, ctx, value);
}

// GameRecord.InitializeGameRecord(record): initializes the record's data (its own, or its STData's).
void InitializeGameRecord(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const auto argc = static_cast<std::int32_t>(ctx.gpr[5]);
    const std::uint32_t argv = ctx.gpr[6];
    if (argc <= 0) return;
    auto &memory = rt.memory();
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t holder = frame.at(0u);
    (void)call_guest(rt, ctx, kArgumentAsRecord, {holder, argv});
    const std::uint32_t record = memory.load32(holder);
    std::uint32_t data = memory.load32(record + 0x1Cu);
    if (data == 0u) data = memory.load32(memory.load32(record + 0x18u) + 0xCu);  // via its STData
    destroy(rt, ctx, holder);
    (void)call_guest(rt, ctx, kInitializeGameRecord, {data});
}

} // namespace gt::adhoc::game

GT_DECOMP(0x08974058u, gt::adhoc::game::MWatcher_append, gt::ReturnKind::Void);
GT_DECOMP(0x08974004u, gt::adhoc::game::MWatcher_remove, gt::ReturnKind::Void);
GT_DECOMP(0x0882A530u, gt::adhoc::game::MOrganizer_startGameObjectLoop, gt::ReturnKind::Void);
GT_DECOMP(0x088297E8u, gt::adhoc::game::MOrganizer_setRaceSound, gt::ReturnKind::Void);
GT_DECOMP(0x08829868u, gt::adhoc::game::MOrganizer_setSoundContext, gt::ReturnKind::Void);
GT_DECOMP(0x08824E88u, gt::adhoc::game::MRaceOperator_setOrganizer, gt::ReturnKind::Void);
GT_DECOMP(0x0883BD04u, gt::adhoc::game::MSpecDB_getCarLabelCount, gt::ReturnKind::Void);
GT_DECOMP(0x0883BF90u, gt::adhoc::game::MSpecDB_getCarCode, gt::ReturnKind::Void);
GT_DECOMP(0x08805FD0u, gt::adhoc::game::CreateGameRecordStructure, gt::ReturnKind::Void);
GT_DECOMP(0x08806384u, gt::adhoc::game::InitializeGameRecord, gt::ReturnKind::Void);
