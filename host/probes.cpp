// GT_PROBE=0x08AD90AC,0x08ADB3E8 — debugging aid (docs/agents/debugging.md: instrument before you
// ablate). Each listed guest function gets a pass-through wrapper that counts entries and prints the
// first one (return address, order of first hits); a summary is printed at exit. Behaviour is
// unchanged: the wrapper always calls the original.

#include "probes.hpp"

#include "psprecomp/common.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>

namespace gt {
namespace {

struct Probe {
    psprecomp::Runtime::RecompiledFunction original{};
    std::string name;
    std::uint64_t hits{};
    unsigned first_order{};
};

std::map<std::uint32_t, Probe> g_probes;
bool g_quiet = false;  // GT_PROBE_SYMBOLS probes hundreds of functions: only the summary prints
unsigned g_next_order = 1u;

// ` "text"` when `address` holds a short printable C string (paths, names), else nothing.
std::string guest_text(psprecomp::Runtime &rt, std::uint32_t address) {
    std::string text;
    for (std::uint32_t i = 0; i < 96u; ++i) {
        if (!rt.memory().contains(address + i, 1u)) return {};
        const auto c = static_cast<char>(rt.memory().load8(address + i));
        if (c == '\0') break;
        if (c < 0x20 || c > 0x7E) return {};
        text.push_back(c);
    }
    return text.size() >= 2u ? " \"" + text + "\"" : std::string{};
}

void probe_trampoline(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const auto it = g_probes.find(ctx.pc);
    if (it == g_probes.end()) {
        rt.unsupported(ctx.pc, 0u, "probe trampoline reached for an unregistered address");
        return;
    }
    Probe &probe = it->second;
    if (probe.hits++ == 0u && !g_quiet) {
        probe.first_order = g_next_order++;
        std::cerr << "[probe] first entry #" << probe.first_order << " " << psprecomp::hex32(ctx.pc)
                  << " ra=" << psprecomp::hex32(ctx.gpr[31]) << " a0=" << psprecomp::hex32(ctx.gpr[4])
                  << " a1=" << psprecomp::hex32(ctx.gpr[5]) << guest_text(rt, ctx.gpr[5]) << "\n";
    }
    probe.original(rt, ctx);
}

void add_probe(psprecomp::Runtime &runtime, std::uint32_t address, std::string name) {
    const auto original = runtime.function_at(address);
    if (original == nullptr) {
        if (!g_quiet) std::cerr << "[probe] no registered function at " << psprecomp::hex32(address) << "\n";
        return;
    }
    g_probes[address] = Probe{original, std::move(name)};
    runtime.register_function(address, &probe_trampoline, "probe:" + g_probes[address].name);
}

} // namespace

void install_probes(psprecomp::Runtime &runtime) {
    if (const char *list = std::getenv("GT_PROBE")) {
        std::stringstream stream(list);
        std::string item;
        while (std::getline(stream, item, ','))
            add_probe(runtime, static_cast<std::uint32_t>(std::strtoul(item.c_str(), nullptr, 0)), item);
    }
    // GT_PROBE_SYMBOLS=config/symbol_addrs.txt: probe every named function ("name = 0xADDR; ...").
    if (const char *path = std::getenv("GT_PROBE_SYMBOLS")) {
        g_quiet = true;
        std::ifstream symbols(path);
        std::string line;
        while (std::getline(symbols, line)) {
            const auto equals = line.find(" = 0x");
            if (line.rfind("//", 0) == 0u || equals == std::string::npos) continue;
            add_probe(runtime, static_cast<std::uint32_t>(std::strtoul(line.c_str() + equals + 3, nullptr, 16)),
                      line.substr(0, equals));
        }
    }
}

void probe_report() {
    for (const auto &[address, probe] : g_probes) {
        if (g_quiet && probe.hits == 0u) continue;
        std::cerr << "[probe] " << psprecomp::hex32(address) << " hits=" << probe.hits;
        if (probe.hits != 0u) std::cerr << " first=#" << probe.first_order;
        if (g_quiet) std::cerr << " " << probe.name;
        std::cerr << "\n";
    }
}

} // namespace gt

namespace gt {

// SPIKE (docs/superpowers/specs/2026-11-10-recorded-arguments-design.md, measurement only): GT_LIVE_SCAN=<file> appends,
// each call, "S <vblank>" then "O <object> <vptr>" for every heap word that points at a RAM word holding a vtable address,
// and once "V <first> <count>" for every run of >= 2 consecutive recompiled-function addresses in .rodata/.data.
void live_scan(psprecomp::Runtime &rt, std::uint64_t vblank) {
    const char *path = std::getenv("GT_LIVE_SCAN");
    if (path == nullptr) return;
    constexpr std::uint32_t kText0 = 0x08804028u, kText1 = 0x08C7A7DCu, kHeap0 = 0x08D80000u, kHeap1 = 0x09F00000u,
                            kData1 = 0x08DA0000u;
    static bool vtables_done = false;
    std::ofstream out(path, std::ios::app);
    if (!vtables_done) {
        vtables_done = true;
        std::uint32_t run = 0u, count = 0u;
        for (std::uint32_t a = kText1; a < kData1 + 4u; a += 4u) {
            const std::uint32_t w = a < kData1 ? rt.memory().load32(a) : 0u;
            const bool fn = (a < kData1) && w >= kText0 && w < kText1 && rt.function_at(w) != nullptr;
            if (fn) { if (count++ == 0u) run = a; }
            else { if (count >= 2u) out << "V " << std::hex << run << " " << std::dec << count << "\n"; count = 0u; }
        }
    }
    out << "S " << vblank << "\n";
    static std::set<std::uint64_t> printed;  // (object, vptr) pairs already written: only new ones go out
    std::set<std::uint32_t> seen;
    for (std::uint32_t a = kHeap0; a < kHeap1; a += 4u) {
        const std::uint32_t p = rt.memory().load32(a);
        if (p < kHeap0 || p >= kHeap1 || (p & 3u) != 0u || !seen.insert(p).second) continue;
        const std::uint32_t t = rt.memory().load32(p);
        if (t < kText1 || t >= kData1 || (t & 3u) != 0u) continue;
        const std::uint32_t f = rt.memory().load32(t);
        if (f >= kText0 && f < kText1 && rt.function_at(f) != nullptr) if (printed.insert((std::uint64_t{p} << 32) | t).second) out << "O " << std::hex << p << " " << t << "\n";
    }
}

} // namespace gt
