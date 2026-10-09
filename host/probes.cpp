// GT_PROBE=0x08AD90AC,0x08ADB3E8 — debugging aid (docs/agents/debugging.md: instrument before you
// ablate). Each listed guest function gets a pass-through wrapper that counts entries and prints the
// first one (return address, order of first hits); a summary is printed at exit. Behaviour is
// unchanged: the wrapper always calls the original.

#include "probes.hpp"

#include "psprecomp/common.hpp"

#include <cstdlib>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

namespace gt {
namespace {

struct Probe {
    psprecomp::Runtime::RecompiledFunction original{};
    std::uint64_t hits{};
    unsigned first_order{};
};

std::map<std::uint32_t, Probe> g_probes;
unsigned g_next_order = 1u;

void probe_trampoline(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const auto it = g_probes.find(ctx.pc);
    if (it == g_probes.end()) {
        rt.unsupported(ctx.pc, 0u, "probe trampoline reached for an unregistered address");
        return;
    }
    Probe &probe = it->second;
    if (probe.hits++ == 0u) {
        probe.first_order = g_next_order++;
        std::cerr << "[probe] first entry #" << probe.first_order << " " << psprecomp::hex32(ctx.pc)
                  << " ra=" << psprecomp::hex32(ctx.gpr[31]) << " a0=" << psprecomp::hex32(ctx.gpr[4]) << "\n";
    }
    probe.original(rt, ctx);
}

} // namespace

void install_probes(psprecomp::Runtime &runtime) {
    const char *list = std::getenv("GT_PROBE");
    if (list == nullptr) return;
    std::stringstream stream(list);
    std::string item;
    while (std::getline(stream, item, ',')) {
        const auto address = static_cast<std::uint32_t>(std::strtoul(item.c_str(), nullptr, 0));
        const auto original = runtime.function_at(address);
        if (original == nullptr) {
            std::cerr << "[probe] no registered function at " << item << "\n";
            continue;
        }
        g_probes[address] = Probe{original};
        runtime.register_function(address, &probe_trampoline, "probe:" + item);
    }
}

void probe_report() {
    for (const auto &[address, probe] : g_probes) {
        std::cerr << "[probe] " << psprecomp::hex32(address) << " hits=" << probe.hits;
        if (probe.hits != 0u) std::cerr << " first=#" << probe.first_order;
        std::cerr << "\n";
    }
}

} // namespace gt
