// host/game_state.hpp: the named accessors of the mod SDK against a guest memory image built here.
#include "game_state.hpp"

#include <cstdio>
#include <cstring>
#include <memory>

namespace {
int failures = 0;
void check(bool condition, const char *what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}
constexpr std::uint32_t kCell = 0x08F6AC00u;
} // namespace

int main() {
    using namespace gt::game_state;
    auto owned = std::make_unique<psprecomp::GuestMemory>();
    psprecomp::GuestMemory &m = *owned;
    std::uint32_t value = 7u;
    check(!money_get(m, value) && value == 7u, "money: an empty cell is not the balance");
    m.store32(kCell, 100000u);
    m.store32(kCell + 4u, 0u);
    m.store32(kCell + 8u, 0x00020100u);
    check(money_get(m, value) && value == 100000u, "money: new-game cell read");
    m.store32(kCell + 8u, 0x01030000u);
    check(money_get(m, value) && value == 100000u, "money: saved/loaded cell read");
    check(money_set(m, 2100000u) && m.load32(kCell) == 2100000u, "money: set writes the cell");
    check(!money_set(m, kMoneyCap + 1u) && m.load32(kCell) == 2100000u, "money: above the cap refused, nothing written");
    m.store32(kCell + 4u, 1u);
    check(!money_get(m, value) && !money_set(m, 5u) && m.load32(kCell) == 2100000u, "money: a high word set is not the balance");
    m.store32(kCell + 4u, 0u);
    m.store32(kCell + 8u, 0x00020101u);
    check(!money_set(m, 5u), "money: another +8 word is not the balance");
    // A race world: three slots — 0 the live human, 1 an AI car, 2 a replayed human (not live), 3 no driver.
    constexpr std::uint32_t W = 0x09000000u, slots = 0x09100000u, controller = 0x09010000u, list = 0x09011000u;
    m.store32(W + 4u, slots);
    m.store32(W + 8u, 4u);
    m.store32(W + 0x10u, controller);
    m.store32(controller + 0xCu, list);
    const auto entry_at = [&](std::uint32_t i, std::uint32_t entry) {
        m.store8(list + 0x24u + i, 0u);
        m.store32(list + 4u + (2u * i) * 4u, entry);
    };
    const auto body_of = [&](std::uint32_t i, std::uint32_t body) { m.store32(slots + i * 0x2100u + 0x2010u + 8u, body); };
    entry_at(0, 0x09020000u); m.store32(0x09020000u, 0x08CC1F60u); m.store8(0x09020000u + 0xA8u, 0u); m.store32(0x09020000u + 0x6Cu, 0x094168D8u);
    entry_at(1, 0x09021000u); m.store32(0x09021000u, 0x08CC0000u);  // another vtable: an AI driver
    entry_at(2, 0x09022000u); m.store32(0x09022000u, 0x08CC1F60u); m.store8(0x09022000u + 0xA8u, 1u); m.store32(0x09022000u + 0x6Cu, 0x094168D8u);
    m.store8(list + 0x24u + 3u, 0xFFu);                               // no driver
    body_of(0, 0x09300000u); body_of(1, 0x09310000u); body_of(2, 0x09320000u); body_of(3, 0x09330000u);
    Car cars[kMaxCars];
    const std::uint32_t n = race_cars(m, W, cars);
    check(n == 3 && cars[0].role == CarRole::Human && cars[0].handle == 0x09300000u && cars[1].role == CarRole::Ai &&
              cars[1].index == 1u && cars[2].role == CarRole::ReplayHuman && cars[2].handle == 0x09320000u,
          "cars: the live human, the AI car and the replayed human; the empty slot left out");
    m.store32(0x09020000u + 0x6Cu, 0u);  // a live human entry that reads no pad (the Time Trial ghost/recorder): left out
    check(race_cars(m, W, cars) == 2u && cars[0].role == CarRole::Ai, "cars: a live human without a pad is left out");
    m.store32(0x09020000u + 0x6Cu, 0x094168D8u);
    check(player_car(m, W) == 0x09300000u, "player car: the one live human");
    m.store8(0x09022000u + 0xA8u, 0u);  // the replayed human goes live: two humans
    check(player_car(m, W) == 0u, "player car: two live humans -> none");
    m.store8(0x09022000u + 0xA8u, 1u);
    check(player_car(m, 0u) == 0u && race_cars(m, 0u, cars) == 0u, "no world, no cars");

    CarState s{};
    check(car_state(m, 0x09300000u, s), "car state read");
    s.position[0] = 1.5f; s.basis[8] = 1.0f; s.speed = 33.0f; s.velocity[2] = -4.0f; s.rates[3] = 0.25f; s.euler[1] = 0.5f;
    s.throttle = 0.6f; s.brake = 0.1f;
    check(car_set_state(m, 0x09300000u, s), "car state write");
    const auto f = [&](std::uint32_t off) { float v; const std::uint32_t b = m.load32(0x09300000u + off); std::memcpy(&v, &b, 4); return v; };
    check(f(0x10u) == 1.5f && f(0x3Cu) == 1.0f && f(0x6C4u) == 33.0f && f(0x6D4u) == -4.0f && f(0x6F4u) == 0.25f &&
              f(0x700u) == 0.5f && f(0x530u) == 0.6f && f(0x538u) == 0.1f, "car state lands on the FACTS offsets");
    CarState back{};
    check(car_state(m, 0x09300000u, back) && back.speed == 33.0f && back.throttle == 0.6f, "car state round trip");
    check(!car_state(m, 0u, back) && !car_set_state(m, 0u, s), "a null handle is refused");

    if (failures == 0) std::puts("game_state: all checks passed");
    // ---- v5: impact from the velocity change (docs/FACTS.md "Collision code ... NOT FOUND": the delta-v fallback). ----
    {
        // A world with one slot: the car at slot + 0x2010, its body K at word(car + 8), its wheel records at word(car + 0x10).
        constexpr std::uint32_t IW = 0x09500000u, islots = 0x09510000u, K = 0x09400000u, wheels = 0x09410000u;
        m.store32(IW + 4u, islots);
        m.store32(IW + 8u, 1u);
        const std::uint32_t icar = islots + 0x2010u;
        m.store32(icar + 8u, K);
        m.store32(icar + 0x10u, wheels);
        const auto setf = [&](std::uint32_t off, float v) { std::uint32_t b; std::memcpy(&b, &v, 4); m.store32(K + off, b); };
        const float identity[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
        for (int i = 0; i < 9; ++i) setf(0x1Cu + 4u * i, identity[i]);
        setf(0x6CCu, 10.f); setf(0x6D0u, 0.f); setf(0x6D4u, 0.f); setf(0x6C4u, 10.f); setf(0x6F8u, 0.f);
        ImpactTracker tracker;
        Impact impact{};
        check(!car_impact(m, IW, 0u, tracker, impact), "impact: a null body is refused");
        check(!car_impact(m, IW, 0x09999000u, tracker, impact), "impact: a body that is no slot of the world is refused");
        check(car_impact(m, IW, K, tracker, impact) && impact.dv_magnitude == 0.f, "impact: the first sample has no previous velocity: no impact");
        check(car_impact(m, IW, K, tracker, impact) && impact.dv_magnitude == 0.f, "impact: a steady step has dv 0");
        setf(0x6D0u, 5.f);
        check(car_impact(m, IW, K, tracker, impact) && impact.dv_magnitude > 4.99f && impact.dv_magnitude < 5.01f, "impact: |dv| of a 5 m/s sideways change");
        check(impact.dv_body[1] > 4.99f && impact.dv_body[0] == 0.f, "impact: identity basis keeps world axes (right = +y)");
        check(impact.contact_dir[1] < -0.99f, "impact: the contact direction is -dv (the hit came from the -y side)");
        const float turned[9] = {0, 1, 0, -1, 0, 0, 0, 0, 1};  // forward = world +y
        for (int i = 0; i < 9; ++i) setf(0x1Cu + 4u * i, turned[i]);
        check(car_impact(m, IW, K, tracker, impact) && impact.dv_magnitude == 0.f, "impact: no change since the last sample");
        setf(0x6D0u, 10.f);
        check(car_impact(m, IW, K, tracker, impact) && impact.dv_body[0] > 4.99f && impact.dv_body[0] < 5.01f,
              "impact: the basis rows rotate dv into the body frame (forward = row 0)");
        check(impact.speed_before > 9.9f, "impact: speed_before is the previous speed");
        check(!impact.off_track, "impact: wheels on asphalt are not off track");
        m.store32(wheels + 0x38u * 2u + 0x2Cu, 0x300u);
        setf(0x6F8u, 3.f);
        check(car_impact(m, IW, K, tracker, impact) && impact.off_track && impact.yaw_rate_jump > 2.99f,
              "impact: the 0x300 surface word of a wheel means off track; the yaw-rate jump is reported");
        check(impact.box_roof > 0.f && impact.box_floor < 0.f && impact.box_half_length > 0.f && impact.box_half_width > 0.f,
              "impact: the body box is reported");
    }
    return failures == 0 ? 0 : 1;
}
