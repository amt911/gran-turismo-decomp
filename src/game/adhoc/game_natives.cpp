// Adhoc natives of GT's game-side classes (names from GT's registrations, config/symbol_addrs.txt).
// Same shape as the UI natives: fetch self as the engine class (helpers take the native's self in
// a1, left untouched here unless an argument is converted instead), call the engine, drop holders.

#include "decomp_registry.hpp"
#include "natives.hpp"

namespace gt::adhoc::game {
namespace {

constexpr std::uint32_t kHolderReset = 0x08A5BAD8u;
constexpr std::uint32_t kHolderDestroy = 0x08A5B9ECu;

constexpr std::uint32_t kSelfAsWatcher = 0x08973EF8u;        // (holder *out) [a1 = self]
constexpr std::uint32_t kWatcherAppend = 0x08973DD0u;        // (watcher *)
constexpr std::uint32_t kWatcherRemoveSlot = 66u;            // watcher->remove()
constexpr std::uint32_t kSelfAsCarParameter = 0x088547C4u;   // (holder *out) [a1 = self]
constexpr std::uint32_t kCarParameterCore = 0x08854678u;     // (car parameter *) -> engine object
constexpr std::uint32_t kCarParameterBeginPartsChange = 0x088A5E54u;
constexpr std::uint32_t kCarParameterEndPartsChange = 0x088A5CD0u;
constexpr std::uint32_t kCarParameterBeginInfo = 0x088A61A4u;
constexpr std::uint32_t kCarParameterEndInfo = 0x088A5A0Cu;
constexpr std::uint32_t kCarParameterUpdateScratch = 0x088A6FB4u;
constexpr std::uint32_t kCarSettingsBegin = 0x088A657Cu;
constexpr std::uint32_t kCarSettingsEnd = 0x088A5CD8u;
constexpr std::uint32_t kSelfAsCarDriverParameter = 0x0884D660u;  // (holder *out) [a1 = self]
constexpr std::uint32_t kCarDriverParameterCore = 0x0884D3D8u;
constexpr std::uint32_t kCarDriverParameterSetDefault = 0x0889793Cu;
constexpr std::uint32_t kLoadContainer = 0x08808F2Cu;        // (holder *out, argc, argv, is_array, element factory)
constexpr std::uint32_t kCarArrayElement = 0x08808640u;      // element factory of loadCarArray
constexpr std::uint32_t kCourseMapElement = 0x088085B8u;     // element factory of loadCourseMap
constexpr std::uint32_t kSelfAsSceneFace = 0x0880B8F8u;      // (holder *out) [a1 = self]
constexpr std::uint32_t kSceneSetClearColor = 0x0880BBF4u;   // (scene *, bool)
constexpr std::uint32_t kParentalLevel = 0x08ABDA84u;        // () -> int
constexpr std::uint32_t kSelfAsSound = 0x08A354ECu;          // (holder *out) [a1 = self]
constexpr std::uint32_t kSoundStopMusic = 0x08A35088u;       // (sound *)
constexpr std::uint32_t kGameRecordStructure = 0x08CE28E4u;  // global: the game-record structure object
constexpr std::uint32_t kHolderCopyRecord = 0x08A53224u;     // holder copy (an instance used here)
constexpr std::uint32_t kArgumentAsRecord = 0x08A57854u;     // (holder *out, argv): argument as STRecord
constexpr std::uint32_t kInitializeGameRecord = 0x08805D9Cu; // (record data)
constexpr std::uint32_t kCarParameterCarCode = 0x088A5EE0u;  // (engine object) -> 64-bit car code
constexpr std::uint32_t kSelfAsMusic = 0x08A3A9D0u;          // (holder *out) [a1 = self]
constexpr std::uint32_t kMusicIsPlaying = 0x08A3A788u;       // (music *) -> int
constexpr std::uint32_t kCarParameterHasTarget = 0x088A5928u;  // (core) -> int
constexpr std::uint32_t kCarParameterIsRemote = 0x088A5984u;   // (core) -> int
constexpr std::uint32_t kDriverParameterIsHuman = 0x08897788u; // (core) -> bool
constexpr std::uint32_t kDriverParameterSetAILevel = 0x08897290u;  // (core, level)
constexpr std::uint32_t kSelfAsRaceParameter = 0x08847008u;  // (holder *out) [a1 = self]
constexpr std::uint32_t kRaceParameterCore = 0x08846E50u;    // (race parameter *) -> engine object
constexpr std::uint32_t kRaceParameterSetLaunchPosition = 0x088D59A0u;  // (core, a, b)
constexpr std::uint32_t kRaceParameterSetLaunchSpeed = 0x088D4E3Cu;     // (core, a, (u8)b)
constexpr std::uint32_t kCarParameterPP = 0x088A5AE4u;                // (core) -> float
constexpr std::uint32_t kCarParameterTransmission = 0x088A6608u;      // (core) -> transmission info
constexpr std::uint32_t kTransmissionIsATOnly = 0x089325E0u;          // (info) -> bool
constexpr std::uint32_t kCarParameterPowerScratch = 0x088A7608u;
constexpr std::uint32_t kCarParameterSetRearTire = 0x088A6508u;       // (core, category) -> int
constexpr std::uint32_t kCarParameterSetFrontTire = 0x088A6494u;
constexpr std::uint32_t kDriverParameterSetPlayer = 0x08897554u;      // (core, player)
constexpr std::uint32_t kRaceParameterDelayStart = 0x088D4D14u;       // (core, index) -> int
constexpr std::uint32_t kRaceParameterUseLaunchData = 0x088D4E18u;    // (core, bool)
constexpr std::uint32_t kRaceParameterSetEventV = 0x088D5988u;        // (core, a, b)
constexpr std::uint32_t kRaceParameterSetBoostA = 0x088D4E68u;        // (core, side, value)
constexpr std::uint32_t kRaceParameterSetBoostB = 0x088D4EE4u;
constexpr std::uint32_t kRaceParameterSetBoostC = 0x088D4F48u;
constexpr std::uint32_t kSystemIsExitGame = 0x08B1A844u;              // () -> int
constexpr std::uint32_t kTemporalAntialias = 0x08CF6568u;             // u8 flag
constexpr std::uint32_t kEnterButtonQuery = 0x08C19BECu;              // (system parameter *) -> nonzero: circle
constexpr std::uint32_t kEnterButtonParameter = 0x08CF6540u;
constexpr std::uint32_t kTextCross = 0x08CABB64u;                     // "cross"
constexpr std::uint32_t kTextCircle = 0x08CABB5Cu;                    // "circle"
constexpr std::uint32_t kGameRegionCode = 0x08ABDC28u;                // () -> const char *
constexpr std::uint32_t kSelfAsController = 0x088378F0u;              // (holder *out, self holder *)
constexpr std::uint32_t kControllerCore = 0x08837498u;                // (controller *, 0) -> config owner
constexpr std::uint32_t kWrapControllerConfig = 0x08838380u;          // (holder *out, owner, index)
constexpr std::uint32_t kSoundFadeOutMusic = 0x08A35040u;   // (seconds in $f12, sound *)
constexpr std::uint32_t kMusicFadeIn = 0x08A3A3E4u;         // (seconds in $f12, music *, std::string name)
constexpr std::uint32_t kSoundPlay = 0x08A34DA8u;            // (sound *, std::string name)
constexpr std::uint32_t kSoundPlayWith = 0x08A34D98u;        // (sound *, std::string name, float in $f12)
constexpr std::uint32_t kSelfAsMenuObjectManager = 0x08A14610u;  // (holder *out) [a1 = self]
constexpr std::uint32_t kMenuObjectManagerStart = 0x08A142C8u;   // (manager *, const std::string *name)


// self as `cls` into a frame holder, `use(object)`, then drop the holder.

} // namespace

void MWatcher_append(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    with_self(rt, ctx, kSelfAsWatcher, [&](std::uint32_t watcher) { (void)call_guest(rt, ctx, kWatcherAppend, {watcher}); });
}

void MWatcher_remove(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    with_self(rt, ctx, kSelfAsWatcher, [&](std::uint32_t watcher) {
        (void)call_guest(rt, ctx, virtual_function(rt, watcher, kWatcherRemoveSlot), {watcher});
    });
}

// The common shape of argument-less natives: self as the engine class, its engine object
// (`core_of`), then `engine(core)` — called after the holder is dropped (`DropFirst::yes`) or before
// (`DropFirst::no`), as each original does.


// MCarParameter: a car's parameter set (parts, info). begin*/end* bracket changes to it.
void car_parameter_call(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t engine, DropFirst drop_first) {
    core_call(rt, ctx, kSelfAsCarParameter, kCarParameterCore, engine, drop_first);
}
void MCarParameter_beginPartsChange(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { car_parameter_call(rt, ctx, kCarParameterBeginPartsChange, DropFirst::no); }
void MCarParameter_endPartsChange(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { car_parameter_call(rt, ctx, kCarParameterEndPartsChange, DropFirst::no); }
void MCarParameter_beginInfo(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { car_parameter_call(rt, ctx, kCarParameterBeginInfo, DropFirst::no); }
void MCarParameter_endInfo(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { car_parameter_call(rt, ctx, kCarParameterEndInfo, DropFirst::no); }
void MCarParameter_updateScratch(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { car_parameter_call(rt, ctx, kCarParameterUpdateScratch, DropFirst::yes); }

// The car-settings class registered by initializer 0x04B808 (suspension, gears, wing, ... — its
// script name isn't known yet). It takes self as MCarParameter, like the class above.
void CarSettings_beginSetting(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { car_parameter_call(rt, ctx, kCarSettingsBegin, DropFirst::no); }
void CarSettings_endSetting(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { car_parameter_call(rt, ctx, kCarSettingsEnd, DropFirst::no); }

// MCarDriverParameter.setDefault().
void MCarDriverParameter_setDefault(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    core_call(rt, ctx, kSelfAsCarDriverParameter, kCarDriverParameterCore, kCarDriverParameterSetDefault, DropFirst::yes);
}


// loadCarArray(...) / loadCourseMap(...): the shared loader, building an array of cars or a map of
// courses from the arguments; its result goes to *result. Module functions: (result, argc, argv).
void load_container(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t is_array,
                    std::uint32_t element) {
    const std::uint32_t result = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t loaded = frame.at(0u);
    (void)call_guest(rt, ctx, kLoadContainer, {loaded, ctx.gpr[5], ctx.gpr[6], is_array, element});
    if (result != loaded) (void)call_guest(rt, ctx, kHolderReset, {result, rt.memory().load32(loaded)});
    destroy(rt, ctx, loaded);
}
void loadCarArray(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { load_container(rt, ctx, 1u, kCarArrayElement); }
void loadCourseMap(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { load_container(rt, ctx, 0u, kCourseMapElement); }

// MSceneFace.setClearColorBuffer(flag) -> 1 after setting the scene's clear-colour flag to
// toInt(argv[0]) != 0; 0 (and nothing set) without an argument. Method: (result, self, argc, argv).
void MSceneFace_setClearColorBuffer(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    std::uint32_t done = 0u;
    if (argc > 0) {
        const std::uint32_t self = frame.at(4u);
        (void)call_guest(rt, ctx, kSelfAsSceneFace, {self});
        const std::uint32_t flag = rt.memory().load32(argv);
        const std::uint32_t truth = call_guest(rt, ctx, virtual_function(rt, flag, kToIntSlot), {flag});
        const std::uint32_t scene = rt.memory().load32(rt.memory().load32(self) + 0xE8u);
        (void)call_guest(rt, ctx, kSceneSetClearColor, {scene, truth != 0u ? 1u : 0u});
        destroy(rt, ctx, self);
        done = 1u;
    }
    return_integer(rt, ctx, result, frame.at(0u), done);
}


// pdistd.IsDebugVersion() -> 0: this is the release build.
void pdistd_IsDebugVersion(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    return_integer(rt, ctx, result, frame.at(0u), 0u);
}

// MCarParameter.getCarCode() -> the 64-bit code of the parameter's car.
void MCarParameter_getCarCode(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    std::uint32_t core = 0u;
    with_self(rt, ctx, kSelfAsCarParameter, [&](std::uint32_t object) { core = call_guest(rt, ctx, kCarParameterCore, {object}); });
    const GuestResult code = call_guest(rt, ctx, kCarParameterCarCode, {core}, {});
    GuestFrame frame(ctx, 0x20u);
    return_long(rt, ctx, result, frame.at(0u), code.v0, code.v1);
}


// MMusic.isPlaying() -> integer (the engine's state, not a boolean).
void MMusic_isPlaying(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    with_self(rt, ctx, kSelfAsMusic, [&](std::uint32_t music) {
        GuestFrame frame(ctx, 0x20u);
        return_integer(rt, ctx, result, frame.at(0u), call_guest(rt, ctx, kMusicIsPlaying, {music}));
    });
}


// MCarParameter.isTargetLocal() -> integer: it has a target and the target isn't remote.
void MCarParameter_isTargetLocal(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t core = self_core(rt, ctx, kSelfAsCarParameter, kCarParameterCore);
    std::uint32_t local = 0u;
    if (call_guest(rt, ctx, kCarParameterHasTarget, {core}) != 0u && call_guest(rt, ctx, kCarParameterIsRemote, {core}) == 0u) {
        local = 1u;
    }
    GuestFrame frame(ctx, 0x10u);
    return_integer(rt, ctx, result, frame.at(0u), local);
}

// MCarDriverParameter.isHuman() -> boolean; setAILevel(level), with at least one argument.
void MCarDriverParameter_isHuman(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t core = self_core(rt, ctx, kSelfAsCarDriverParameter, kCarDriverParameterCore);
    const std::uint32_t human = call_guest(rt, ctx, kDriverParameterIsHuman, {core});
    GuestFrame frame(ctx, 0x10u);
    return_bool(rt, ctx, result, frame.at(0u), human);
}
void MCarDriverParameter_setAILevel(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    if (argc <= 0) return;
    const std::uint32_t core = self_core(rt, ctx, kSelfAsCarDriverParameter, kCarDriverParameterCore);
    (void)call_guest(rt, ctx, kDriverParameterSetAILevel, {core, int_argument(rt, ctx, argv, 0u)});
}

// MRaceParameter.setLaunchPosition(a, b) / setLaunchSpeed(a, b): with at least two arguments
// (setLaunchSpeed passes b as a byte).
void race_parameter_set(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t set, std::uint32_t mask) {
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    if (argc <= 1) return;
    const std::uint32_t core = self_core(rt, ctx, kSelfAsRaceParameter, kRaceParameterCore);
    const std::uint32_t a = int_argument(rt, ctx, argv, 0u);
    const std::uint32_t b = int_argument(rt, ctx, argv, 1u) & mask;
    (void)call_guest(rt, ctx, set, {core, a, b});
}
void MRaceParameter_setLaunchPosition(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    race_parameter_set(rt, ctx, kRaceParameterSetLaunchPosition, 0xFFFFFFFFu);
}
void MRaceParameter_setLaunchSpeed(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    race_parameter_set(rt, ctx, kRaceParameterSetLaunchSpeed, 0xFFu);
}

// MSound.fadeOutMusic([seconds = 1]).
void MSound_fadeOutMusic(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[5];
    const std::uint32_t argv = ctx.gpr[7];
    float seconds = 1.0f;
    if (ctx.gpr[6] == 1u) seconds = to_float(rt, ctx, rt.memory().load32(argv));
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t sound = frame.at(0u);
    (void)call_guest(rt, ctx, kSelfAsSound, {sound, self});
    (void)call_guest(rt, ctx, kSoundFadeOutMusic, {rt.memory().load32(sound)}, {seconds});
    destroy(rt, ctx, sound);
}

// MMusic.fadein(name[, seconds = 3]).
void MMusic_fadein(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[5];
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    if (argc <= 0) return;
    auto &memory = rt.memory();
    float seconds = 3.0f;
    if (argc >= 2) seconds = to_float(rt, ctx, memory.load32(argv + 4u));
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t name = frame.at(0x0u);
    const std::uint32_t music = frame.at(0x4u);
    to_string(rt, ctx, memory.load32(argv), name);
    (void)call_guest(rt, ctx, kSelfAsMusic, {music, self});
    (void)call_guest(rt, ctx, kMusicFadeIn, {memory.load32(music), memory.load32(name)}, {seconds});
    destroy(rt, ctx, music);
    string_release(rt, ctx, memory.load32(name));
}

// MSound.play(name[, value]): plays the named sound (with a float parameter when given).
void MSound_play(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[5];
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    if (argc <= 0) return;
    auto &memory = rt.memory();
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t name = frame.at(0x0u);
    const std::uint32_t sound = frame.at(0x4u);
    to_string(rt, ctx, memory.load32(argv), name);
    (void)call_guest(rt, ctx, kSelfAsSound, {sound, self});
    if (argc < 2) {
        (void)call_guest(rt, ctx, kSoundPlay, {memory.load32(sound), memory.load32(name)});
    } else {
        const float value = to_float(rt, ctx, memory.load32(argv + 4u));
        (void)call_guest(rt, ctx, kSoundPlayWith, {memory.load32(sound), memory.load32(name)}, {value});
    }
    destroy(rt, ctx, sound);
    string_release(rt, ctx, memory.load32(name));
}

// MMenuGameObjectManager.start(name), with at least one argument.
void MMenuGameObjectManager_start(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    if (argc <= 0) return;
    auto &memory = rt.memory();
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t manager = frame.at(0x0u);
    const std::uint32_t name = frame.at(0x4u);
    (void)call_guest(rt, ctx, kSelfAsMenuObjectManager, {manager});
    to_string(rt, ctx, memory.load32(argv), name);
    (void)call_guest(rt, ctx, kMenuObjectManagerStart, {memory.load32(manager), name});
    string_release(rt, ctx, memory.load32(name));
    destroy(rt, ctx, manager);
}


// --- MCarParameter / MCarDriverParameter / MRaceParameter ---
std::int32_t trunc_w_s(float value) {  // Allegrex trunc.w.s: toward zero, NaN/out of range -> 0x7FFFFFFF
    if (!(value > -2147483648.0f && value < 2147483648.0f)) return 0x7FFFFFFF;
    return static_cast<std::int32_t>(value);
}
void MCarParameter_getPP1000(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t core = self_core(rt, ctx, kSelfAsCarParameter, kCarParameterCore);
    const float pp = call_guest(rt, ctx, kCarParameterPP, {core}, {}).f0;
    return_int(rt, ctx, result, static_cast<std::uint32_t>(trunc_w_s(pp * 1000.0f)));
}
void MCarParameter_isATOnly(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t info = call_guest(rt, ctx, kCarParameterTransmission, {self_core(rt, ctx, kSelfAsCarParameter, kCarParameterCore)});
    return_boolean(rt, ctx, result, call_guest(rt, ctx, kTransmissionIsATOnly, {info}));
}
void MCarParameter_getPowerScratch(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t core = self_core(rt, ctx, kSelfAsCarParameter, kCarParameterCore);
    return_int(rt, ctx, result, call_guest(rt, ctx, kCarParameterPowerScratch, {core}));
}
void car_parameter_set_tire(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t set) {
    const std::uint32_t result = ctx.gpr[4];
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    if (argc <= 0) return;
    const std::uint32_t core = self_core(rt, ctx, kSelfAsCarParameter, kCarParameterCore);
    return_int(rt, ctx, result, call_guest(rt, ctx, set, {core, int_argument(rt, ctx, argv, 0u)}));
}
void MCarParameter_setRearTireCategory(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { car_parameter_set_tire(rt, ctx, kCarParameterSetRearTire); }
void MCarParameter_setFrontTireCategory(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { car_parameter_set_tire(rt, ctx, kCarParameterSetFrontTire); }
void MCarDriverParameter_setPlayer(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    if (argc <= 0) return;
    const std::uint32_t core = self_core(rt, ctx, kSelfAsCarDriverParameter, kCarDriverParameterCore);
    (void)call_guest(rt, ctx, kDriverParameterSetPlayer, {core, int_argument(rt, ctx, argv, 0u)});
}
std::uint32_t race_parameter_core(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    return self_core(rt, ctx, kSelfAsRaceParameter, kRaceParameterCore);
}
void MRaceParameter_getDelayStartSecond(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    if (argc <= 0) return;
    const std::uint32_t core = race_parameter_core(rt, ctx);
    return_int(rt, ctx, result, call_guest(rt, ctx, kRaceParameterDelayStart, {core, int_argument(rt, ctx, argv, 0u)}));
}
void MRaceParameter_useLaunchData(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    if (argc <= 0) return;
    const std::uint32_t core = race_parameter_core(rt, ctx);
    const auto use = static_cast<std::int32_t>(int_argument(rt, ctx, argv, 0u));
    (void)call_guest(rt, ctx, kRaceParameterUseLaunchData, {core, use > 0 ? 1u : 0u});
}
void MRaceParameter_setEventV(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    race_parameter_set(rt, ctx, kRaceParameterSetEventV, 0xFFFFFFFFu);
}
// MRaceParameter.setBoostParams(a0, a1, b0, b1, c0, c1): three boost settings for both sides.
void MRaceParameter_setBoostParams(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    if (argc <= 5) return;
    const std::uint32_t core = race_parameter_core(rt, ctx);
    std::uint32_t value[6];
    for (std::uint32_t i = 0u; i < 6u; ++i) value[i] = int_argument(rt, ctx, argv, i);
    const std::uint32_t setters[3] = {kRaceParameterSetBoostA, kRaceParameterSetBoostB, kRaceParameterSetBoostC};
    for (std::uint32_t i = 0u; i < 3u; ++i) {
        (void)call_guest(rt, ctx, setters[i], {core, 0u, value[i * 2u]});
        (void)call_guest(rt, ctx, setters[i], {core, 1u, value[i * 2u + 1u]});
    }
}


// --- system natives (module functions: result, argc, argv) ---
void MSystemCondition_IsExitGame(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    return_int(rt, ctx, result, call_guest(rt, ctx, kSystemIsExitGame, {}));
}
void MSystemConfiguration_SetTemporalAntialias(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const auto argc = static_cast<std::int32_t>(ctx.gpr[5]);
    const std::uint32_t argv = ctx.gpr[6];
    if (argc <= 0) return;
    rt.memory().store8(kTemporalAntialias, static_cast<std::uint8_t>(int_argument(rt, ctx, argv, 0u)));
}
// MSystemConfiguration.GetEnterButtonAssign() -> "cross" or "circle" (the system's enter button).
void MSystemConfiguration_GetEnterButtonAssign(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const bool circle = call_guest(rt, ctx, kEnterButtonQuery, {kEnterButtonParameter}) != 0u;
    return_c_string(rt, ctx, result, circle ? kTextCircle : kTextCross);
}
void MProductInformation_GetGameRegionCode(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    return_c_string(rt, ctx, result, call_guest(rt, ctx, kGameRegionCode, {}));
}

// MController.getConfig([index]) -> the controller configuration.
void MController_getConfig(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t self = ctx.gpr[5];
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    const std::uint32_t index = argc > 0 ? int_argument(rt, ctx, argv, 0u) : 0u;
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t holder = frame.at(0x0u);
    const std::uint32_t out = frame.at(0x4u);
    (void)call_guest(rt, ctx, kSelfAsController, {holder, self});
    const std::uint32_t owner = call_guest(rt, ctx, kControllerCore, {rt.memory().load32(holder), 0u});
    destroy(rt, ctx, holder);
    (void)call_guest(rt, ctx, kWrapControllerConfig, {out, owner, index});
    if (result != out) (void)call_guest(rt, ctx, kHolderReset, {result, rt.memory().load32(out)});
    destroy(rt, ctx, out);
}

// MProductInformation.GetParentalLevel() -> integer.
void MProductInformation_GetParentalLevel(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    return_integer(rt, ctx, result, frame.at(0u), call_guest(rt, ctx, kParentalLevel, {}));
}

// MSound.stopMusic().
void MSound_stopMusic(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    with_self(rt, ctx, kSelfAsSound, [&](std::uint32_t sound) { (void)call_guest(rt, ctx, kSoundStopMusic, {sound}); });
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
// Not comparable yet (reads files inside a case), so not swapped in: GT_DECOMP(0x08854A78u, gt::adhoc::game::MCarParameter_beginPartsChange, gt::ReturnKind::Void);
GT_DECOMP(0x08854ACCu, gt::adhoc::game::MCarParameter_endPartsChange, gt::ReturnKind::Void);
GT_DECOMP(0x088558F8u, gt::adhoc::game::MCarParameter_getCarCode, gt::ReturnKind::Void);
GT_DECOMP(0x08A3B220u, gt::adhoc::game::MMusic_isPlaying, gt::ReturnKind::Void);
GT_DECOMP(0x0885600Cu, gt::adhoc::game::MCarParameter_isTargetLocal, gt::ReturnKind::Void);
GT_DECOMP(0x0884D81Cu, gt::adhoc::game::MCarDriverParameter_isHuman, gt::ReturnKind::Void);
GT_DECOMP(0x0884D77Cu, gt::adhoc::game::MCarDriverParameter_setAILevel, gt::ReturnKind::Void);
GT_DECOMP(0x088472C0u, gt::adhoc::game::MRaceParameter_setLaunchPosition, gt::ReturnKind::Void);
GT_DECOMP(0x088471E4u, gt::adhoc::game::MRaceParameter_setLaunchSpeed, gt::ReturnKind::Void);
GT_DECOMP(0x08A36608u, gt::adhoc::game::MSound_play, gt::ReturnKind::Void);
// Not comparable yet (sleeps the thread: sceKernelSleepThread), so not swapped in: GT_DECOMP(0x08A1511Cu, gt::adhoc::game::MMenuGameObjectManager_start, gt::ReturnKind::Void);
GT_DECOMP(0x08854B74u, gt::adhoc::game::MCarParameter_beginInfo, gt::ReturnKind::Void);
GT_DECOMP(0x08854BC8u, gt::adhoc::game::MCarParameter_endInfo, gt::ReturnKind::Void);
GT_DECOMP(0x08854C1Cu, gt::adhoc::game::MCarParameter_updateScratch, gt::ReturnKind::Void);
GT_DECOMP(0x0884F760u, gt::adhoc::game::CarSettings_beginSetting, gt::ReturnKind::Void);
GT_DECOMP(0x0884F7B4u, gt::adhoc::game::CarSettings_endSetting, gt::ReturnKind::Void);
GT_DECOMP(0x0884D9F0u, gt::adhoc::game::MCarDriverParameter_setDefault, gt::ReturnKind::Void);
GT_DECOMP(0x08805FD0u, gt::adhoc::game::CreateGameRecordStructure, gt::ReturnKind::Void);
GT_DECOMP(0x08A4C848u, gt::adhoc::game::pdistd_IsDebugVersion, gt::ReturnKind::Void);
// Not comparable: the loader reads files (sceIo), which a harness case can't replay.
// GT_DECOMP(0x088094DCu, gt::adhoc::game::loadCarArray, gt::ReturnKind::Void);
// GT_DECOMP(0x08809528u, gt::adhoc::game::loadCourseMap, gt::ReturnKind::Void);
GT_DECOMP(0x0880C2D4u, gt::adhoc::game::MSceneFace_setClearColorBuffer, gt::ReturnKind::Void);
GT_DECOMP(0x08A27F20u, gt::adhoc::game::MProductInformation_GetParentalLevel, gt::ReturnKind::Void);
GT_DECOMP(0x08A35648u, gt::adhoc::game::MSound_stopMusic, gt::ReturnKind::Void);
GT_DECOMP(0x08806384u, gt::adhoc::game::InitializeGameRecord, gt::ReturnKind::Void);
GT_DECOMP(0x08857A78u, gt::adhoc::game::MCarParameter_getPP1000, gt::ReturnKind::Void);
GT_DECOMP(0x0885794Cu, gt::adhoc::game::MCarParameter_isATOnly, gt::ReturnKind::Void);
GT_DECOMP(0x08857584u, gt::adhoc::game::MCarParameter_getPowerScratch, gt::ReturnKind::Void);
GT_DECOMP(0x08855E54u, gt::adhoc::game::MCarParameter_setRearTireCategory, gt::ReturnKind::Void);
GT_DECOMP(0x08855D78u, gt::adhoc::game::MCarParameter_setFrontTireCategory, gt::ReturnKind::Void);
GT_DECOMP(0x0884D950u, gt::adhoc::game::MCarDriverParameter_setPlayer, gt::ReturnKind::Void);
GT_DECOMP(0x0884BDA8u, gt::adhoc::game::MRaceParameter_getDelayStartSecond, gt::ReturnKind::Void);
GT_DECOMP(0x08847144u, gt::adhoc::game::MRaceParameter_useLaunchData, gt::ReturnKind::Void);
GT_DECOMP(0x088477A0u, gt::adhoc::game::MRaceParameter_setEventV, gt::ReturnKind::Void);
GT_DECOMP(0x0884739Cu, gt::adhoc::game::MRaceParameter_setBoostParams, gt::ReturnKind::Void);
GT_DECOMP(0x08A29540u, gt::adhoc::game::MSystemCondition_IsExitGame, gt::ReturnKind::Void);
GT_DECOMP(0x08A29730u, gt::adhoc::game::MSystemConfiguration_SetTemporalAntialias, gt::ReturnKind::Void);
GT_DECOMP(0x08A29FD8u, gt::adhoc::game::MSystemConfiguration_GetEnterButtonAssign, gt::ReturnKind::Void);
GT_DECOMP(0x08A28CE4u, gt::adhoc::game::MProductInformation_GetGameRegionCode, gt::ReturnKind::Void);
GT_DECOMP(0x088387DCu, gt::adhoc::game::MController_getConfig, gt::ReturnKind::Void);
GT_DECOMP(0x08A35B8Cu, gt::adhoc::game::MSound_fadeOutMusic, gt::ReturnKind::Void);
// Not comparable yet (wakes a thread: sceKernelWakeupThread), so not swapped in: GT_DECOMP(0x08A3B9E8u, gt::adhoc::game::MMusic_fadein, gt::ReturnKind::Void);
