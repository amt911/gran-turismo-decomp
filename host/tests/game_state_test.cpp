// host/game_state.hpp: the named accessors of the mod SDK against a guest memory image built here.
#include "game_state.hpp"

#include <cmath>
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

    // ---- v7 (2026-11-18) ----
    {
        // The Euler convention (0x0893C3E4): yaw 1.571 is the race start's heading (-1, 0, 0); pitch up lifts the forward row; the basis is left-handed.
        float rows[9];
        const float start[3] = {0.0f, 0.0f, 1.5708f};
        euler_to_rows(start, rows);
        check(std::fabs(rows[0] + 1.0f) < 1e-3f && std::fabs(rows[1]) < 1e-3f && std::fabs(rows[8] - 1.0f) < 1e-6f, "euler: yaw 1.571 faces -x, up stays up");
        const float nose[3] = {0.3f, 0.0f, 0.0f};
        euler_to_rows(nose, rows);
        check(rows[2] > 0.29f && rows[8] > 0.95f, "euler: positive pitch raises the forward row");
        const float rolled[3] = {0.0f, 0.5f, 0.0f};
        euler_to_rows(rolled, rows);
        check(rows[5] < -0.47f && rows[8] > 0.87f && rows[6] > 0.47f, "euler: positive roll tips the up row toward the right row (x here)");
        // left-handed: forward x right = -up, at many angles; and the round trip rows -> Euler -> rows, near the poles included
        int bad_hand = 0, bad_trip = 0, n = 0;
        for (float p = -1.5707f; p <= 1.5708f; p += 0.0611f)
            for (float r = -3.14f; r <= 3.15f; r += 0.1571f)
                for (float y = -3.14f; y <= 3.15f; y += 0.4f) {
                    const float e[3] = {p, r, y};
                    float a[9], b[9], e2[3];
                    euler_to_rows(e, a);
                    const float cx = a[1] * a[5] - a[2] * a[4], cy = a[2] * a[3] - a[0] * a[5], cz = a[0] * a[4] - a[1] * a[3];
                    if (std::fabs(cx + a[6]) > 1e-4f || std::fabs(cy + a[7]) > 1e-4f || std::fabs(cz + a[8]) > 1e-4f) ++bad_hand;
                    rows_to_euler(a, e2);
                    euler_to_rows(e2, b);
                    float worst = 0;
                    for (int k = 0; k < 9; ++k) worst = std::fmax(worst, std::fabs(a[k] - b[k]));
                    if (worst > 2e-3f) ++bad_trip;
                    ++n;
                }
        check(bad_hand == 0, "euler: the basis is left-handed (f x r = -u) at every angle");
        std::fprintf(stderr, "euler round trip: %d angle triples, %d off\n", n, bad_trip);
        check(bad_trip == 0, "euler: rows -> Euler -> rows round trip, upside down and at +-90 degrees of pitch");
        // the pole itself
        const float pole[3] = {1.5707963f, 0.0f, 0.7f};
        float pr[9], pe[3], pr2[9];
        euler_to_rows(pole, pr);
        rows_to_euler(pr, pe);
        euler_to_rows(pe, pr2);
        float worst = 0;
        for (int k = 0; k < 9; ++k) worst = std::fmax(worst, std::fabs(pr[k] - pr2[k]));
        check(worst < 5e-3f && std::fabs(pe[0]) <= 1.5708f, "euler: at the pole the rebuilt rows match");
        // a smooth attitude through the band 0.9999 < |forward.z| < 1: yaw 0.5, roll 0.3 + 2 x (angle below the pole), pitch going to the pole: the written angles must not wobble
        for (int sign = -1; sign <= 1; sign += 2) {
            float prev[3] = {0.0f, 0.3f, 0.5f}, prev_rows[9];
            bool first = true;
            float worst_jump = 0.0f, worst_rows = 0.0f;
            for (double below = 2e-2; below > 1e-6; below *= 0.93) {
                const float eu[3] = {float(sign * (1.5707963267948966 - below)), float(0.3 + 5.0 * below), 0.5f};
                float rr[9], got[3], back[9];
                euler_to_rows(eu, rr);
                rows_to_euler(rr, got, first ? nullptr : prev);
                euler_to_rows(got, back);
                for (int k = 0; k < 9; ++k) worst_rows = std::fmax(worst_rows, std::fabs(rr[k] - back[k]));
                if (!first) worst_jump = std::fmax(worst_jump, std::fmax(std::fabs(got[1] - prev[1]), std::fabs(got[2] - prev[2])));
                std::memcpy(prev, got, sizeof prev);
                std::memcpy(prev_rows, rr, sizeof prev_rows);
                first = false;
            }
            std::fprintf(stderr, "pole band sign %d: worst step in roll/yaw %.5f, rows error %.6f\n", sign, worst_jump, worst_rows);
            check(worst_jump < 0.02f, "euler: the angles do not wobble through the pole band (continuity)");
            check(worst_rows < 1e-3f, "euler: the rows rebuilt from the angles of the pole band match");
        }
    }
    {
        // car_set_pose / car_state round trip, dimensions and wheel records on the fake world of the cars test (slot 0)
        const std::uint32_t car0 = 0x09100000u + 0x2010u, body0 = 0x09300000u;
        const float pos[3] = {10.0f, -20.0f, 5.0f}, vel[3] = {3.0f, 4.0f, 0.0f}, w[3] = {0.1f, 0.2f, 0.3f};
        const float e[3] = {0.2f, 2.9f, -1.0f};  // upside down
        float rows[9];
        euler_to_rows(e, rows);
        CarState before{};
        car_state(m, body0, before);
        check(car_set_pose(m, body0, pos, rows, vel, w), "pose: written");
        CarState s{};
        check(car_state(m, body0, s), "pose: read back");
        check(s.position[0] == 10.0f && s.position[2] == 5.0f && std::fabs(s.speed - 5.0f) < 1e-6f && s.velocity[1] == 4.0f, "pose: position, velocity and speed |v|");
        bool rates0 = true;
        for (float r : s.rates) rates0 = rates0 && r == 0.0f;
        check(rates0, "pose: the five rate fields are zero");
        check(std::fabs(s.euler[0] - e[0]) < 1e-4f && std::fabs(s.euler[1] - e[1]) < 1e-4f && std::fabs(s.euler[2] - e[2]) < 1e-4f, "pose: the Euler angles are derived from the rows");
        bool rows_ok = true;
        for (int k = 0; k < 9; ++k) rows_ok = rows_ok && s.basis[k] == rows[k];
        check(rows_ok && s.throttle == before.throttle && s.brake == before.brake, "pose: rows written as given, pedals untouched");
        float bad[9];
        for (int k = 0; k < 9; ++k) bad[k] = rows[k];
        bad[0] *= 2.0f;
        check(!car_set_pose(m, body0, pos, bad, vel, w), "pose: rows that are not a rotation are refused");
        float righthanded[9];
        for (int k = 0; k < 9; ++k) righthanded[k] = rows[k];
        for (int k = 0; k < 3; ++k) righthanded[3 + k] = -rows[3 + k];  // mirror the right row: a right-handed, still orthonormal set
        check(!car_set_pose(m, body0, pos, righthanded, vel, w), "pose: a right-handed basis is refused");
        const float nan[3] = {std::nanf(""), 0.0f, 0.0f};
        check(!car_set_pose(m, body0, nan, rows, vel, w) && !car_set_pose(m, body0, pos, rows, vel, nan) && !car_set_pose(m, 0u, pos, rows, vel, w), "pose: non-finite values and a null handle are refused");
        check(s.position[0] == 10.0f, "pose: a refused call writes nothing");
        // dimensions: the car record at word(car + 0xC)
        const std::uint32_t rec = 0x09500000u;
        const auto put = [&](std::uint32_t at, float v) { std::uint32_t b; std::memcpy(&b, &v, 4); m.store32(at, b); };
        m.store32(car0 + 0xCu, rec);
        put(rec + 0x08u, 1.783f); put(rec + 0x18u, 2.442f); put(rec + 0x1Cu, 1.434f); put(rec + 0x40u, 0.879f); put(rec + 0x48u, -1.563f);
        CarDimensions d{};
        check(car_dimensions(m, W, body0, d) && d.wheelbase == 2.442f && d.track == 1.434f && d.front_axle == 0.879f && d.rear_axle == -1.563f && d.radius == 1.783f, "dimensions: the car record");
        check(!car_dimensions(m, W, 0x09999999u, d) && !car_dimensions(m, 0u, body0, d), "dimensions: a body that is no slot of the world is refused");
        // wheel records: word(car + 0x10) + 0x38 i: x, -y (Y-up), ground height; byte +0x2C = 0 found; +0x2D surface; load at K + 0x70 + 0x118 i + 0x54
        const std::uint32_t wrec = 0x09510000u;
        m.store32(car0 + 0x10u, wrec);
        for (std::uint32_t i = 0; i < 4; ++i) {
            put(wrec + 0x38u * i, 1.0f + i); put(wrec + 0x38u * i + 8u, -(10.0f + i)); put(wrec + 0x38u * i + 0xCu, 3.0f + 0.1f * i);
            m.store32(wrec + 0x38u * i + 0x2Cu, i == 2 ? 0x0100u + 1u : i == 3 ? 0x0300u : 0x0200u);  // byte +0x2C = 1 (no road) on wheel 2; +0x2D = 2 elsewhere
            put(body0 + 0x70u + 0x118u * i + 0x54u, 100.0f * i);
        }
        WheelContact wc[4]{};
        check(car_wheel_contacts(m, W, body0, wc), "wheels: read");
        check(wc[1].x == 2.0f && wc[1].y == 11.0f && std::fabs(wc[1].ground_height - 3.1f) < 1e-6f && wc[1].valid == 1 && wc[1].surface == 2, "wheels: x, y (= -z of the Y-up record), height, valid, surface");
        check(wc[2].valid == 0 && wc[3].load == 300.0f && wc[0].load == 0.0f, "wheels: byte +0x2C != 0 is not valid; the load comes from the wheel struct");
        check(wc[3].valid == 1 && wc[3].surface == 3, "wheels: the off-track word 0x0300 reads valid with surface 3");
        check(!car_wheel_contacts(m, W, 0x09999999u, wc), "wheels: a body that is no slot is refused");
    }

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
    // ---- v5: the car -> view -> model link and the model's blobs ----
    {
        constexpr std::uint32_t a0 = 0x09600000u, P = 0x09600100u, entry = 0x09600200u, car = 0x09600300u, body = 0x09600400u, o20 = 0x09600500u,
                                model = 0x09600600u, parts = 0x09600800u, blob0 = 0x09601000u, blob1 = 0x09602000u;
        m.store32(a0 + 4u, P);
        m.store32(P + 0xB04u, entry);
        m.store32(entry + 0xCu, car);
        m.store32(car + 8u, body);
        m.store32(P + 0x20u, o20);
        m.store32(o20 + 8u, model);
        m.store32(model, 0x4D444C33u);
        m.store32(model + 0x14u, 3u);          // three part entries, the middle one not a mesh
        m.store32(model + 0xA8u, parts);
        const auto cmd = [](std::uint32_t op, std::uint32_t param) { return (op << 24) | (param & 0xFFFFFFu); };
        for (const std::uint32_t at : {blob0, blob1}) {
            m.store32(at, cmd(0x10, ((at + 20u) >> 8) & 0xFF0000u));
            m.store32(at + 4u, cmd(0x01, (at + 20u) & 0xFFFFFFu));
            m.store32(at + 8u, cmd(0x12, 0x112));
            m.store32(at + 12u, cmd(0x04, 0x040003));
            m.store32(at + 16u, cmd(0x0B, 0));
        }
        m.store32(parts, blob0);
        m.store32(parts + 32u, 0x09609999u);   // not a blob
        m.store32(parts + 64u, blob1);
        CarDrawLink link;
        check(car_draw_link(m, a0, link) && link.car == body && link.model == model, "link: the car's body and model from the draw's a0");
        std::uint32_t blobs[8];
        check(model_blobs(m, model, blobs, 8) == 2u && blobs[0] == blob0 && blobs[1] == blob1, "link: the model's parts that are meshes");
        check(model_blobs(m, model, blobs, 1) == 1u, "link: capacity limits the list");
        m.store32(model, 0u);
        check(!car_draw_link(m, a0, link) && model_blobs(m, model, blobs, 8) == 0u, "link: a model without the 3LDM magic is refused");
        m.store32(model, 0x4D444C33u);
        m.store32(entry + 0xCu, 0u);
        check(!car_draw_link(m, a0, link), "link: a null link is refused");
    }
    return failures == 0 ? 0 : 1;
}
