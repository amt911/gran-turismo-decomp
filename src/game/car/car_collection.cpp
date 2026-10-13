// CarCollection: the engine side of MCarCollection, the player's garage — how many cars and car types they
// own, whether they own a given car, the favourites (layout and evidence:
// include/gt/game/car_collection.hpp; names from GT's registration, config/symbol_addrs.txt).
//
// Engine functions take the CarCollectionData in a0 (the script object's engine object); the natives are
// `MCarCollection_<method>`. Engine functions with no script name keep func_XXXXXXXX. Registered
// functions replace the recompiled originals; the ones commented out at the end are never compared on
// their own by the pad routes (the harness replays nested calls), so the gate can't verify them.
//
// Modding: the garage limits and the ownership queries come out of here (getOneCarMax is the constant 10,
// the favourites are 30 slots); a mod that wants a bigger garage changes them. Keep the original
// behaviour for the differential gate and make such changes opt-in.

#include "decomp_registry.hpp"
#include "gt/game/car_collection.hpp"
#include "guest_call.hpp"
#include "../adhoc/natives.hpp"

#include <cstdint>

namespace gt::game::car_collection {
namespace {

using guest::View;
using namespace gt::adhoc;

constexpr std::uint32_t kSelfAsCarCollection = 0x0881BD98u;  // (holder *out) [a1 = self]
constexpr std::uint32_t kCarCollectionCore = 0x0881B068u;    // (collection *) -> its engine object
constexpr std::uint32_t kCarCollectionFind = 0x0881BF5Cu;    // (core, argc, argv, u64 *code, int *index) -> kind
constexpr std::uint32_t kCarCollectionHasKind1 = 0x0881A8F0u;  // (core, -, u64 code) -> count
constexpr std::uint32_t kCarCollectionHasKind2 = 0x0881A8C0u;  // (core, -, u64 code, index) -> count
constexpr std::uint32_t kCarCollectionHasFavorite = 0x0881AEA4u;      // (core, -, u64 code, index) -> bool
constexpr std::uint32_t kRecordIndex = 0x089301C4u;     // (code low, code high) -> index of the car's record
constexpr std::uint32_t kColorCount = 0x08930458u;      // (code low, code high) -> how many colours the car has
constexpr std::uint32_t kCarParameterCarCode = 0x088A5EE0u;  // CarParameter::car_code (handle) -> 64-bit code
constexpr std::uint32_t kFavoriteCount = 30u;           // MCarCollection::getFavoriteSize

} // namespace

// --- Engine -------------------------------------------------------------------------------------------

// The record of the car with the code (a2:a3) in the collection a0 (34 bytes each, from CarCollectionData::unk_30).
void func_0881A840(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const View<CarCollectionData> collection(rt.memory(), ctx.gpr[4]);
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t index = call_guest(rt, ctx, kRecordIndex, {ctx.gpr[6], ctx.gpr[7]});
    ctx.gpr[2] = collection.unk_30() + index * 34u;
}

// Sums, over the car's colours, the low nibble of the record's byte (colour + 2): how many of the car the
// player owns (hasCar and getCarCount, for a car given by its code alone).
void func_0881A8F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t low = ctx.gpr[6], high = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t record = call_guest(rt, ctx, 0x0881A840u, {ctx.gpr[4], ctx.gpr[5], low, high});
    const auto colors = static_cast<std::int32_t>(call_guest(rt, ctx, kColorCount, {low, high}));
    std::uint32_t total = 0u;
    for (std::int32_t i = 0; i < colors; ++i) total += rt.memory().load8(record + static_cast<std::uint32_t>(i) + 2u) & 0xFu;
    ctx.gpr[2] = total;
}

// getFlg: bit t1 of the high nibble of the record's byte (t0 + 2).
void get_flg(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t record = call_guest(rt, ctx, 0x0881A840u, {ctx.gpr[4], ctx.gpr[5], ctx.gpr[6], ctx.gpr[7]});
    const std::uint32_t flags = rt.memory().load8(record + ctx.gpr[8] + 2u) >> 4;
    ctx.gpr[2] = (flags >> (ctx.gpr[9] & 31u)) & 1u;
}

// getFavoriteSize: the number of favourite slots.
void get_favorite_size(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = kFavoriteCount; }

// getCarCountFavorite: the index of the first empty favourite slot (its car code is all ones), or 30.
void get_car_count_favorite(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const View<CarCollectionData> collection(rt.memory(), ctx.gpr[4]);
    GuestFrame frame(ctx, 0x20u);
    for (std::uint32_t i = 0; i != kFavoriteCount; ++i) {
        const GuestResult code = call_guest(rt, ctx, kCarParameterCarCode, {collection.favorites(i).address()}, {});
        if (code.v0 == 0xFFFFFFFFu && code.v1 == 0xFFFFFFFFu) {
            ctx.gpr[2] = i;
            return;
        }
    }
    ctx.gpr[2] = kFavoriteCount;
}

// func_0881A8C0(a, b, c, d, i) -> the low nibble of byte i + 2 of the record func_0881A840(a, b, c, d)
// returns.
void func_0881A8C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t i = ctx.gpr[8];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t record = call_guest(rt, ctx, 0x0881A840u, {});
    ctx.gpr[2] = rt.memory().load8(record + i + 0x2u) & 0xFu;
}

// hasFavorite:  -> 1 when func_0881AD9C(...) isn't negative, else 0.
void has_favorite(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = ~call_guest(rt, ctx, 0x0881AD9Cu, {ctx.gpr[4], ctx.gpr[5], ctx.gpr[6], ctx.gpr[7]}) >> 31;
}

// func_0881AC68(this) -> func_0881AA94(this, this + 0x34).
void func_0881AC68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, 0x0881AA94u, {self, self + 52u, ctx.gpr[6], ctx.gpr[7]});
}

// func_0881AC84(this) -> func_0881AA94(this, this + 0x1C4).
void func_0881AC84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, 0x0881AA94u, {self, self + 452u, ctx.gpr[6], ctx.gpr[7]});
}

void func_0881ACB0(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = (ctx.gpr[5] << 2) + ctx.gpr[4] + 0x1C4u; }

// func_0881AA44(this) -> 34 * the word +0x0 + 48 + func_0881AA18(this).
void func_0881AA44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t base = View<CarCollectionData>(rt.memory(), self).unk_00() * 34u + 48u;
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = base + call_guest(rt, ctx, 0x0881AA18u, {self, ctx.gpr[5], ctx.gpr[6], ctx.gpr[7]});
}

// --- Natives --------------------------------------------------------------------------------------------

// MCarCollection.hasCar(car...) -> boolean: the arguments name a car (its code, plus an index for
// kind 2); no car (code -1) gives false, otherwise whether the collection holds at least one.
void MCarCollection_hasCar(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    if (argc <= 0) return;
    auto &memory = rt.memory();
    std::uint32_t core = 0u;
    with_self(rt, ctx, kSelfAsCarCollection, [&](std::uint32_t collection) { core = call_guest(rt, ctx, kCarCollectionCore, {collection}); });
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t index = frame.at(0x0u);
    const std::uint32_t code = frame.at(0x8u);  // u64, 8-aligned
    const std::uint32_t kind =
        call_guest(rt, ctx, kCarCollectionFind, {core, static_cast<std::uint32_t>(argc), argv, code, index});
    const std::uint32_t low = memory.load32(code);
    const std::uint32_t high = memory.load32(code + 4u);
    std::uint32_t has = 0u;
    if (low != 0xFFFFFFFFu || high != 0xFFFFFFFFu) {
        std::int32_t count = 0;
        if (kind == 1u) count = static_cast<std::int32_t>(call_guest(rt, ctx, kCarCollectionHasKind1, {core, 0u, low, high}));
        else if (kind == 2u)
            count = static_cast<std::int32_t>(
                call_guest(rt, ctx, kCarCollectionHasKind2, {core, 0u, low, high, memory.load32(index)}));
        has = count > 0 ? 1u : 0u;
    }
    GuestFrame value(ctx, 0x20u);
    return_bool(rt, ctx, result, value.at(0u), has);
}

// MCarCollection.hasFavorite(car...) -> boolean (false for no car).
void MCarCollection_hasFavorite(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    if (argc <= 0) return;
    auto &memory = rt.memory();
    const std::uint32_t core = self_core(rt, ctx, kSelfAsCarCollection, kCarCollectionCore);
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t index = frame.at(0x0u);
    const std::uint32_t code = frame.at(0x8u);
    const std::uint32_t kind = call_guest(rt, ctx, kCarCollectionFind, {core, static_cast<std::uint32_t>(argc), argv, code, index});
    const std::uint32_t low = memory.load32(code);
    const std::uint32_t high = memory.load32(code + 4u);
    std::uint32_t favorite = 0u;
    if (low != 0xFFFFFFFFu || high != 0xFFFFFFFFu)
        favorite = call_guest(rt, ctx, kCarCollectionHasFavorite, {core, kind, low, high, memory.load32(index)});
    return_boolean(rt, ctx, result, favorite);
}

// getOneCarMax() -> 10.
void MCarCollection_getOneCarMax(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { return_int(rt, ctx, ctx.gpr[4], 10u); }

// getMyAllCarCount() / getMyCarTypeCount() -> how many cars / car types the player owns.
void MCarCollection_getMyAllCarCount(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const View<CarCollectionData> collection(rt.memory(), self_core(rt, ctx, kSelfAsCarCollection, kCarCollectionCore));
    return_int(rt, ctx, result, static_cast<std::uint32_t>(collection.my_all_car_count().value()));
}
void MCarCollection_getMyCarTypeCount(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const View<CarCollectionData> collection(rt.memory(), self_core(rt, ctx, kSelfAsCarCollection, kCarCollectionCore));
    return_int(rt, ctx, result, static_cast<std::uint32_t>(collection.my_car_type_count().value()));
}

// getFavoriteSize() / getCarCountFavorite(): the engine function of the collection, the integer back.
void MCarCollection_getFavoriteSize(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t core = self_core(rt, ctx, kSelfAsCarCollection, kCarCollectionCore);
    return_int(rt, ctx, result, call_guest(rt, ctx, 0x0881AF28u, {core}));
}
void MCarCollection_getCarCountFavorite(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t core = self_core(rt, ctx, kSelfAsCarCollection, kCarCollectionCore);
    return_int(rt, ctx, result, call_guest(rt, ctx, 0x0881AF30u, {core}));
}

// getCarCount(car...) -> how many of the car the player owns: the arguments name a car (its code, plus an
// index for kind 2); no car gives 0.
void MCarCollection_getCarCount(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    if (argc <= 0) return;
    auto &memory = rt.memory();
    const std::uint32_t core = self_core(rt, ctx, kSelfAsCarCollection, kCarCollectionCore);
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t index = frame.at(0x0u);
    const std::uint32_t code = frame.at(0x8u);
    const std::uint32_t kind = call_guest(rt, ctx, kCarCollectionFind, {core, static_cast<std::uint32_t>(argc), argv, code, index});
    const std::uint32_t low = memory.load32(code);
    const std::uint32_t high = memory.load32(code + 4u);
    std::uint32_t count = 0u;
    if (low != 0xFFFFFFFFu || high != 0xFFFFFFFFu) {
        if (kind == 1u) count = call_guest(rt, ctx, kCarCollectionHasKind1, {core, 0u, low, high});
        else if (kind == 2u) count = call_guest(rt, ctx, kCarCollectionHasKind2, {core, 0u, low, high, memory.load32(index)});
    }
    return_int(rt, ctx, result, count);
}

} // namespace gt::game::car_collection

namespace car_collection_registrations {
using gt::ReturnKind;
#define CAR_COLLECTION(address, name, kind) GT_DECOMP_NESTED(address, gt::game::car_collection::name, ReturnKind::kind)

CAR_COLLECTION(0x0881D538u, MCarCollection_hasCar, Void);
CAR_COLLECTION(0x0881E6C0u, MCarCollection_hasFavorite, Void);
CAR_COLLECTION(0x0881A8C0u, func_0881A8C0, Word);
CAR_COLLECTION(0x0881AC68u, func_0881AC68, Word);
CAR_COLLECTION(0x0881AC84u, func_0881AC84, Word);
CAR_COLLECTION(0x0881ACB0u, func_0881ACB0, Word);
CAR_COLLECTION(0x0881AA44u, func_0881AA44, Word);
CAR_COLLECTION(0x0881D7F0u, MCarCollection_getMyAllCarCount, Void);
CAR_COLLECTION(0x0881D674u, MCarCollection_getCarCount, Void);
CAR_COLLECTION(0x0881AEA4u, has_favorite, Word);
// Verified through nested verification (2026-10-13): compared directly (9 and 3 cases) once the whole
// subsystem was installed, 0 mismatches, sabotage pinned on each.
CAR_COLLECTION(0x0881A840u, func_0881A840, Word);
CAR_COLLECTION(0x0881A8F0u, func_0881A8F0, Word);
// Not registered: compared 0 in every fast, dense and full run (the pad routes never call them on their own,
// directly or through a decompiled caller), so nothing could verify them; they run as the recompiled originals.
// CAR_COLLECTION(0x0881A9CCu, get_flg, Word);
// CAR_COLLECTION(0x0881AF28u, get_favorite_size, Word);
// CAR_COLLECTION(0x0881AF30u, get_car_count_favorite, Word);
// CAR_COLLECTION(0x0881D7ACu, MCarCollection_getOneCarMax, Void);
// CAR_COLLECTION(0x0881D87Cu, MCarCollection_getMyCarTypeCount, Void);
// CAR_COLLECTION(0x0881ECECu, MCarCollection_getFavoriteSize, Void);
// CAR_COLLECTION(0x0881ED80u, MCarCollection_getCarCountFavorite, Void);
#undef CAR_COLLECTION
} // namespace car_collection_registrations
