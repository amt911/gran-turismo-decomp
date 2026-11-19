#include "game_state.hpp"

#include <vector>

#include "mesh_blob.hpp"

#include <cmath>
#include <cstring>

namespace gt::game_state {

namespace {
constexpr std::uint32_t kMoneyCell = 0x08F6AC00u;  // docs/FACTS.md "Money and cars" (probe runs 2026-11-08..10)

bool money_cell_ok(const psprecomp::GuestMemory &memory) {
    if (!memory.contains(kMoneyCell, 12u)) return false;
    const std::uint32_t high = memory.load32(kMoneyCell + 4u), kind = memory.load32(kMoneyCell + 8u);
    return high == 0u && (kind == 0x00020100u || kind == 0x01030000u) && memory.load32(kMoneyCell) <= kMoneyCap;
}
} // namespace

bool money_get(const psprecomp::GuestMemory &memory, std::uint32_t &value) {
    if (!money_cell_ok(memory)) return false;
    value = memory.load32(kMoneyCell);
    return true;
}

bool money_set(psprecomp::GuestMemory &memory, std::uint32_t value) {
    if (value > kMoneyCap || !money_cell_ok(memory)) return false;
    memory.store32(kMoneyCell, value);
    return true;
}

namespace {
// docs/FACTS.md "Car body state" / gt-mods cheats (measured): the world and driver layout.
constexpr std::uint32_t kSlotSize = 0x2100u, kCarInSlot = 0x2010u, kBody = 8u;
constexpr std::uint32_t kHumanVtable = 0x08CC1F60u;  // the human driver entry's vtable (FACTS "Analog pedals")
constexpr std::uint32_t kEntryReplay = 0xA8u, kEntryInput = 0x6Cu;
constexpr std::uint32_t kPosition = 0x10u, kBasis = 0x1Cu, kSpeed = 0x6C4u, kVelocity = 0x6CCu, kEuler = 0x6FCu;
constexpr std::uint32_t kRates[5] = {0x6E4u, 0x6E8u, 0x6F0u, 0x6F4u, 0x6F8u};
constexpr std::uint32_t kThrottle = 0x530u, kBrake = 0x538u;
constexpr std::uint32_t kStateEnd = 0x708u;

std::uint32_t word_or_0(const psprecomp::GuestMemory &m, std::uint32_t address) {
    return address != 0u && m.contains(address, 4u) ? m.load32(address) : 0u;
}
float load_f(const psprecomp::GuestMemory &m, std::uint32_t address) {
    const std::uint32_t bits = m.load32(address);
    float v;
    std::memcpy(&v, &bits, sizeof v);
    return v;
}
void store_f(psprecomp::GuestMemory &m, std::uint32_t address, float v) {
    std::uint32_t bits;
    std::memcpy(&bits, &v, sizeof bits);
    m.store32(address, bits);
}
bool body_ok(const psprecomp::GuestMemory &m, std::uint32_t handle) { return handle != 0u && m.contains(handle, kStateEnd); }
} // namespace

std::uint32_t race_cars(const psprecomp::GuestMemory &m, std::uint32_t world, Car *out) {
    if (world == 0u || !m.contains(world, 0x14u)) return 0u;
    const std::uint32_t slots = m.load32(world + 4u), controller = m.load32(world + 0x10u);
    const std::uint32_t list = word_or_0(m, controller == 0u ? 0u : controller + 0xCu);
    if (slots == 0u || list == 0u) return 0u;
    const std::uint32_t count = m.load32(world + 8u);
    std::uint32_t n = 0u;
    for (std::uint32_t i = 0; i < count && i < kMaxCars; ++i) {
        if (!m.contains(list + 0x24u + i, 1u)) break;
        const int ix = static_cast<std::int8_t>(m.load8(list + 0x24u + i));
        if (ix < 0) continue;
        const std::uint32_t entry = word_or_0(m, list + 4u + (2u * i + static_cast<std::uint32_t>(ix)) * 4u);
        if (entry == 0u || !m.contains(entry, kEntryReplay + 1u)) continue;
        CarRole role = CarRole::Ai;
        if (m.load32(entry) == kHumanVtable) {
            // a replay re-simulates the race from the recorded pad stream (FACTS "A replay is a re-simulation"): its human car is listed so
            // that a mod can act there as it did live; a live human entry that reads no pad (the Time Trial ghost/recorder) is left out
            if (m.load8(entry + kEntryReplay) != 0u) role = CarRole::ReplayHuman;
            else if (m.load32(entry + kEntryInput) == 0u) continue;
            else role = CarRole::Human;
        }
        const std::uint32_t body = word_or_0(m, slots + i * kSlotSize + kCarInSlot + kBody);
        if (!body_ok(m, body)) continue;
        out[n++] = Car{body, i, role};
    }
    return n;
}

std::uint32_t player_car(const psprecomp::GuestMemory &m, std::uint32_t world) {
    Car cars[kMaxCars];
    const std::uint32_t n = race_cars(m, world, cars);
    std::uint32_t found = 0u, humans = 0u;
    for (std::uint32_t i = 0; i < n; ++i)
        if (cars[i].role == CarRole::Human) {
            ++humans;
            found = cars[i].handle;
        }
    return humans == 1u ? found : 0u;
}

bool car_state(const psprecomp::GuestMemory &m, std::uint32_t h, CarState &s) {
    if (!body_ok(m, h)) return false;
    for (int i = 0; i < 3; ++i) s.position[i] = load_f(m, h + kPosition + 4u * i);
    for (int i = 0; i < 9; ++i) s.basis[i] = load_f(m, h + kBasis + 4u * i);
    s.speed = load_f(m, h + kSpeed);
    for (int i = 0; i < 3; ++i) s.velocity[i] = load_f(m, h + kVelocity + 4u * i);
    for (int i = 0; i < 5; ++i) s.rates[i] = load_f(m, h + kRates[i]);
    for (int i = 0; i < 3; ++i) s.euler[i] = load_f(m, h + kEuler + 4u * i);
    s.throttle = load_f(m, h + kThrottle);
    s.brake = load_f(m, h + kBrake);
    return true;
}

bool car_set_state(psprecomp::GuestMemory &m, std::uint32_t h, const CarState &s) {
    if (!body_ok(m, h)) return false;
    for (int i = 0; i < 3; ++i) store_f(m, h + kPosition + 4u * i, s.position[i]);
    for (int i = 0; i < 9; ++i) store_f(m, h + kBasis + 4u * i, s.basis[i]);
    store_f(m, h + kSpeed, s.speed);
    for (int i = 0; i < 3; ++i) store_f(m, h + kVelocity + 4u * i, s.velocity[i]);
    for (int i = 0; i < 5; ++i) store_f(m, h + kRates[i], s.rates[i]);
    for (int i = 0; i < 3; ++i) store_f(m, h + kEuler + 4u * i, s.euler[i]);
    store_f(m, h + kThrottle, s.throttle);
    store_f(m, h + kBrake, s.brake);
    return true;
}

void euler_to_rows(const float e[3], float rows[9]) {
    const float s1 = std::sin(e[0]), c1 = std::cos(e[0]), s2 = std::sin(e[1]), c2 = std::cos(e[1]), s3 = std::sin(e[2]), c3 = std::cos(e[2]);
    rows[0] = -s3 * c1; rows[1] = c3 * c1; rows[2] = s1;
    rows[3] = c3 * c2 - s1 * s2 * s3; rows[4] = s3 * c2 + s1 * s2 * c3; rows[5] = -c1 * s2;
    rows[6] = c3 * s2 + s1 * c2 * s3; rows[7] = s3 * s2 - s1 * c2 * c3; rows[8] = c1 * c2;
}

namespace {
float wrap_pi(float a) {
    const float pi = 3.14159265358979f;
    while (a > pi) a -= 2.0f * pi;
    while (a < -pi) a += 2.0f * pi;
    return a;
}
}  // namespace

void rows_to_euler(const float r[9], float e[3], const float *previous) {
    const float fz = r[2] > 1.0f ? 1.0f : r[2] < -1.0f ? -1.0f : r[2];
    e[0] = std::asin(fz);
    const float c1 = std::sqrt(std::fmax(0.0f, 1.0f - fz * fz));  // cos(pitch)
    // Near the pole yaw and roll turn about almost the same axis and the standard formulas divide by cos(pitch): their float noise grows as it shrinks (a wobble of the
    // written angles). The right row has unit length in that plane, so there the roll comes from the invariant it carries: yaw + roll at the +pole, yaw - roll at the -pole.
    const float sign = fz > 0 ? 1.0f : -1.0f;
    const float invariant = std::atan2(r[4], r[3]);
    if (c1 > 0.0014f) {
        e[2] = std::atan2(-r[0], r[1]);
        e[1] = c1 < 0.014f ? wrap_pi(sign * (invariant - e[2])) : std::atan2(-r[5], r[8]);
    } else {  // within 0.08 degrees of the pole: the forward row's horizontal part is noise; keep the previous roll (continuity) and put the rest in yaw
        e[1] = previous != nullptr && std::isfinite(previous[1]) ? previous[1] : 0.0f;
        e[2] = wrap_pi(invariant - sign * e[1]);
    }
}

namespace {
constexpr std::uint32_t kWheelRecords = 0x10u, kWheelStride = 0x38u, kSurface = 0x2Cu, kOffTrack = 0x300u, kCarRecord = 0xCu;
constexpr std::uint32_t kWheelBase = 0x18u, kTrack = 0x1Cu;
float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
} // namespace

bool car_impact(const psprecomp::GuestMemory &m, std::uint32_t world, std::uint32_t handle, ImpactTracker &tracker, Impact &out) {
    if (!body_ok(m, handle)) return false;
    const std::uint32_t slots = word_or_0(m, world + 4u), count = word_or_0(m, world + 8u);
    std::uint32_t car = 0u;
    for (std::uint32_t i = 0; i < count && i < kMaxCars && slots != 0u; ++i) {
        const std::uint32_t candidate = slots + i * kSlotSize + kCarInSlot;
        if (m.contains(candidate, 0x14u) && m.load32(candidate + kBody) == handle) {
            car = candidate;
            break;
        }
    }
    if (car == 0u) return false;
    float v[3], rows[9];
    for (int i = 0; i < 3; ++i) v[i] = load_f(m, handle + kVelocity + 4u * i);
    for (int i = 0; i < 9; ++i) rows[i] = load_f(m, handle + kBasis + 4u * i);
    const float speed = load_f(m, handle + kSpeed), yaw_rate = load_f(m, handle + kRates[4]);
    ImpactTracker::Sample *sample = nullptr;
    for (auto &s : tracker.samples)
        if (s.valid && s.handle == handle) sample = &s;
    if (sample == nullptr)
        for (auto &s : tracker.samples)
            if (!s.valid) {
                sample = &s;
                break;
            }
    out = Impact{};
    if (sample != nullptr && sample->valid) {
        const float dv[3] = {v[0] - sample->velocity[0], v[1] - sample->velocity[1], v[2] - sample->velocity[2]};
        for (int r = 0; r < 3; ++r) out.dv_body[r] = rows[3 * r] * dv[0] + rows[3 * r + 1] * dv[1] + rows[3 * r + 2] * dv[2];
        out.dv_magnitude = std::sqrt(dv[0] * dv[0] + dv[1] * dv[1] + dv[2] * dv[2]);
        out.speed_before = sample->speed;
        out.yaw_rate_jump = std::fabs(yaw_rate - sample->yaw_rate);
        if (out.dv_magnitude > 0.f)
            for (int i = 0; i < 3; ++i) out.contact_dir[i] = -out.dv_body[i] / out.dv_magnitude;
    } else {
        out.speed_before = speed;
    }
    const std::uint32_t wheels = word_or_0(m, car + kWheelRecords);
    for (std::uint32_t i = 0; i < 4u && wheels != 0u; ++i)
        if (m.contains(wheels + i * kWheelStride, kSurface + 4u) && m.load32(wheels + i * kWheelStride + kSurface) == kOffTrack) out.off_track = true;
    const std::uint32_t record = word_or_0(m, car + kCarRecord);
    const float wheel_base = record != 0u && m.contains(record, kTrack + 4u) ? load_f(m, record + kWheelBase) : 0.f;
    const float track = record != 0u && m.contains(record, kTrack + 4u) ? load_f(m, record + kTrack) : 0.f;
    out.box_half_length = clampf(0.8f * wheel_base, 1.7f, 2.6f);
    out.box_half_width = clampf(track / 2.f + 0.12f, 0.75f, 1.1f);
    out.box_floor = -0.03f;
    out.box_roof = 0.8f;
    if (sample != nullptr) {
        sample->handle = handle;
        sample->valid = true;
        for (int i = 0; i < 3; ++i) sample->velocity[i] = v[i];
        sample->speed = speed;
        sample->yaw_rate = yaw_rate;
    }
    return true;
}

bool car_draw_link(const psprecomp::GuestMemory &m, std::uint32_t a0, CarDrawLink &out) {
    const std::uint32_t view = word_or_0(m, a0 + 4u), entry = word_or_0(m, view + 0xB04u), car = word_or_0(m, entry + 0xCu);
    const std::uint32_t body = word_or_0(m, car + kBody), model = word_or_0(m, word_or_0(m, view + 0x20u) + 8u);
    if (view == 0u || entry == 0u || car == 0u || body == 0u || model == 0u || word_or_0(m, model) != kModelMagic) return false;
    out.car = body;
    out.model = model;
    return true;
}

std::uint32_t model_blobs(const psprecomp::GuestMemory &m, std::uint32_t model, std::uint32_t *out, std::uint32_t capacity) {
    if (word_or_0(m, model) != kModelMagic) return 0u;
    const std::uint32_t base = word_or_0(m, model + 0xA8u), count = word_or_0(m, model + 0x14u);
    std::uint32_t n = 0u;
    for (std::uint32_t i = 0; i < count && i < 256u && base != 0u && n < capacity; ++i) {
        const std::uint32_t blob = word_or_0(m, base + 32u * i);
        gt::mesh::BlobInfo info;
        if (gt::mesh::parse_blob(m, blob, info)) out[n++] = blob;
    }
    return n;
}

std::uint32_t model_runs(const psprecomp::GuestMemory &m, std::uint32_t model, RunRef *out, std::uint32_t capacity) {
    if (word_or_0(m, model) != kModelMagic) return 0u;
    const std::uint32_t base = word_or_0(m, model + 0xA8u), count = word_or_0(m, model + 0x14u);
    std::uint32_t n = 0u;
    std::vector<gt::mesh::Run> runs;
    for (std::uint32_t i = 0; i < count && i < 256u && base != 0u; ++i) {
        const std::uint32_t blob = word_or_0(m, base + 32u * i);
        if (!gt::mesh::parse_runs(m, blob, runs)) continue;
        for (std::uint32_t r = 0; r < runs.size() && n < capacity; ++r) out[n++] = RunRef{blob, r, runs[r].count};
    }
    return n;
}

bool list_world_matrix(const psprecomp::GuestMemory &m, float out[12], std::uint32_t max_bytes) {
    const std::uint32_t cursor = word_or_0(m, kListState + 0x5Cu);
    if (cursor < 8u + max_bytes || !m.contains(cursor - 4u, 4u)) return false;
    for (std::uint32_t at = cursor - 4u; at + max_bytes > cursor && at >= 4u; at -= 4u) {
        if ((word_or_0(m, at) >> 24) != 0x3Au) continue;  // WORLDN
        bool block = true;
        for (std::uint32_t i = 1; i <= 12u && block; ++i) block = at + 4u * i < cursor && (word_or_0(m, at + 4u * i) >> 24) == 0x3Bu;
        if (!block) continue;
        for (std::uint32_t i = 0; i < 12u; ++i) {
            const std::uint32_t bits = (word_or_0(m, at + 4u * (i + 1u)) & 0xFFFFFFu) << 8;
            std::memcpy(&out[i], &bits, 4);
        }
        return true;
    }
    return false;
}

} // namespace gt::game_state

namespace gt::game_state {

std::uint32_t car_object(const psprecomp::GuestMemory &m, std::uint32_t world, std::uint32_t handle) {
    if (handle == 0u || world == 0u || !m.contains(world, 0xCu)) return 0u;
    const std::uint32_t slots = m.load32(world + 4u), count = m.load32(world + 8u);
    if (slots == 0u) return 0u;
    for (std::uint32_t i = 0; i < count && i < kMaxCars; ++i) {
        const std::uint32_t car = slots + i * kSlotSize + kCarInSlot;
        if (m.contains(car, 0x14u) && m.load32(car + kBody) == handle) return car;
    }
    return 0u;
}

bool car_dimensions(const psprecomp::GuestMemory &m, std::uint32_t world, std::uint32_t handle, CarDimensions &out) {
    const std::uint32_t car = car_object(m, world, handle);
    const std::uint32_t rec = car == 0u ? 0u : word_or_0(m, car + 0xCu);
    if (rec == 0u || !m.contains(rec, 0x50u)) return false;
    out.radius = load_f(m, rec + 0x08u);
    out.wheelbase = load_f(m, rec + 0x18u);
    out.track = load_f(m, rec + 0x1Cu);
    out.front_axle = load_f(m, rec + 0x40u);
    out.rear_axle = load_f(m, rec + 0x48u);
    return std::isfinite(out.radius + out.wheelbase + out.track + out.front_axle + out.rear_axle);
}

bool car_wheel_contacts(const psprecomp::GuestMemory &m, std::uint32_t world, std::uint32_t handle, WheelContact out[4]) {
    const std::uint32_t car = car_object(m, world, handle);
    const std::uint32_t recs = car == 0u ? 0u : word_or_0(m, car + 0x10u);
    if (recs == 0u || !m.contains(recs, 4u * 0x38u) || !body_ok(m, handle)) return false;
    for (std::uint32_t i = 0; i < 4u; ++i) {
        const std::uint32_t r = recs + 0x38u * i;
        out[i].x = load_f(m, r);
        out[i].y = -load_f(m, r + 8u);  // the records are Y-up: the physics frame's y is -z
        out[i].ground_height = load_f(m, r + 0xCu);
        // The surface word at +0x2C is 0 on asphalt and 0x0300 off the track (FACTS "Car body state"), little-endian: byte +0x2C = 0 and byte +0x2D = the type 0 / 3 (2 also seen).
        // FACTS "Road query and the wheel contact records" (measured against the game's own results): the LOW byte +0x2C = 1 means no road found (the height is then a fallback),
        // 0 means ground, off-track terrain included (it has a real height). So `valid` is that byte, `surface` the next one.
        out[i].surface = m.load8(r + 0x2Du);
        out[i].valid = m.load8(r + 0x2Cu) == 0u && std::isfinite(out[i].x + out[i].y + out[i].ground_height) ? 1 : 0;
        out[i].load = load_f(m, handle + 0x70u + 0x118u * i + 0x54u);
    }
    return true;
}

bool car_set_pose(psprecomp::GuestMemory &m, std::uint32_t handle, const float position[3], const float basis[9], const float velocity[3],
                  const float angular_velocity[3]) {
    if (!body_ok(m, handle)) return false;
    for (int i = 0; i < 3; ++i)
        if (!std::isfinite(position[i]) || !std::isfinite(velocity[i]) || !std::isfinite(angular_velocity[i])) return false;
    for (int i = 0; i < 9; ++i)
        if (!std::isfinite(basis[i])) return false;
    for (int a = 0; a < 3; ++a) {
        const float *ra = basis + 3 * a;
        if (std::fabs(ra[0] * ra[0] + ra[1] * ra[1] + ra[2] * ra[2] - 1.0f) > 1e-2f) return false;
        for (int b = a + 1; b < 3; ++b) {
            const float *rb = basis + 3 * b;
            if (std::fabs(ra[0] * rb[0] + ra[1] * rb[1] + ra[2] * rb[2]) > 1e-2f) return false;
        }
    }
    float euler[3], previous[3];
    for (int i = 0; i < 3; ++i) previous[i] = load_f(m, handle + kEuler + 4u * i);  // continuity near the pole
    rows_to_euler(basis, euler, previous);
    // the game's basis is left-handed (forward x right = -up, FACTS "Rollovers"): a right-handed set would be mirrored by the Euler rebuild
    if (basis[0] * (basis[4] * basis[8] - basis[5] * basis[7]) + basis[1] * (basis[5] * basis[6] - basis[3] * basis[8]) + basis[2] * (basis[3] * basis[7] - basis[4] * basis[6]) > -0.5f) return false;
    for (int i = 0; i < 3; ++i) store_f(m, handle + kPosition + 4u * i, position[i]);
    for (int i = 0; i < 9; ++i) store_f(m, handle + kBasis + 4u * i, basis[i]);
    for (int i = 0; i < 3; ++i) store_f(m, handle + kEuler + 4u * i, euler[i]);
    for (int i = 0; i < 3; ++i) store_f(m, handle + kVelocity + 4u * i, velocity[i]);
    store_f(m, handle + kSpeed, std::sqrt(velocity[0] * velocity[0] + velocity[1] * velocity[1] + velocity[2] * velocity[2]));
    for (int i = 0; i < 5; ++i) store_f(m, handle + kRates[i], 0.0f);
    return true;
}

} // namespace gt::game_state
