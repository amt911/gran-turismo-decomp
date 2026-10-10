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
