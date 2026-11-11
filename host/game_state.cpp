#include "game_state.hpp"

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
            if (m.load8(entry + kEntryReplay) != 0u || m.load32(entry + kEntryInput) == 0u) continue;  // a replay, or not reading a pad
            role = CarRole::Human;
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

} // namespace gt::game_state
