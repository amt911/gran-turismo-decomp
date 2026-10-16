#include "fiber.hpp"

#include "guest_call.hpp"

#include <ucontext.h>

#include <memory>
#include <vector>

namespace gt::fiber {
namespace {

constexpr std::size_t kStackBytes = 2u * 1024u * 1024u;  // recompiled code runs nested on it too
// Each fiber's return address. Below guest RAM, so the runtime keeps it out of its direct tables
// (like its own 0x0 / 0x4 thread and callback returns).
constexpr std::uint32_t kReturnBase = 0x00000100u;

struct Fiber {
    ucontext_t context{};
    ucontext_t *parent{};                 // where to switch when done or suspended
    std::unique_ptr<char[]> stack;
    std::uint32_t id{};
    psprecomp::Runtime *rt{};
    psprecomp::AllegrexContext *live{};   // the runtime's context
    psprecomp::AllegrexContext top{};     // the decompiled function's own context
    psprecomp::AllegrexContext *local{};  // call_guest's context while suspended
    DecompiledFn fn{};
    std::uint32_t entry{};
    bool done{};
    bool suspended{};
};

std::vector<std::unique_ptr<Fiber>> g_fibers;  // index = id
std::vector<Fiber *> g_free;
Fiber *g_current = nullptr;
std::uint64_t g_suspensions = 0u;

std::uint32_t return_of(const Fiber &f) { return kReturnBase + f.id * 8u; }

void fiber_main() {
    Fiber &f = *g_current;
    f.top = *f.live;
    f.top.pc = f.entry;
    try {
        f.fn(*f.rt, f.top);
    } catch (const GuestStopped &) {
        // The runtime stopped under the decompiled code (gt::call_guest): end the fiber; the dispatcher sees the stop.
    }
    f.top.gpr[0] = 0u;
    if (f.top.pc == f.entry) f.top.pc = f.top.gpr[31];
    *f.live = f.top;
    f.done = true;
    swapcontext(&f.context, f.parent);
}

void switch_into(Fiber &f) {
    ucontext_t here;
    f.parent = &here;
    Fiber *previous = g_current;
    g_current = &f;
    swapcontext(&here, &f.context);
    g_current = previous;
    if (f.done) g_free.push_back(&f);
}

// Registered at every fiber's return address: the fiber's PSP thread came back from the guest call
// it was blocked in.
void resume_at_return(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t id = (ctx.pc - kReturnBase) / 8u;
    if (id >= g_fibers.size() || !g_fibers[id]->suspended) {
        rt.stop("fiber return address reached with no suspended fiber");
        return;
    }
    Fiber &f = *g_fibers[id];
    *f.local = ctx;
    f.suspended = false;
    switch_into(f);
}

Fiber &acquire(psprecomp::Runtime &rt) {
    if (!g_free.empty()) {
        Fiber *f = g_free.back();
        g_free.pop_back();
        return *f;
    }
    auto fiber = std::make_unique<Fiber>();
    fiber->id = static_cast<std::uint32_t>(g_fibers.size());
    fiber->stack.reset(new char[kStackBytes]);  // uninitialised: pages are committed on use
    rt.register_function(return_of(*fiber), &resume_at_return, "fiber_return");
    g_fibers.push_back(std::move(fiber));
    return *g_fibers.back();
}

} // namespace

void run(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, DecompiledFn fn, std::uint32_t entry) {
    if (g_current != nullptr) {  // already on a fiber: its stack can hold this call too
        ctx.pc = entry;
        fn(rt, ctx);
        ctx.gpr[0] = 0u;
        if (ctx.pc == entry) ctx.pc = ctx.gpr[31];
        return;
    }
    Fiber &f = acquire(rt);
    f.rt = &rt;
    f.live = &ctx;
    f.fn = fn;
    f.entry = entry;
    f.done = false;
    f.suspended = false;
    getcontext(&f.context);
    f.context.uc_stack.ss_sp = f.stack.get();
    f.context.uc_stack.ss_size = kStackBytes;
    f.context.uc_link = nullptr;
    makecontext(&f.context, &fiber_main, 0);
    switch_into(f);
}

bool on_fiber() noexcept { return g_current != nullptr; }
std::size_t created() noexcept { return g_fibers.size(); }
std::uint64_t suspensions() noexcept { return g_suspensions; }

std::uint32_t return_address() noexcept { return g_current != nullptr ? return_of(*g_current) : 0u; }

void suspend(psprecomp::AllegrexContext &local) {
    Fiber &f = *g_current;
    *f.live = local;  // the dispatcher continues with the thread that runs next
    f.local = &local;
    f.suspended = true;
    ++g_suspensions;
    swapcontext(&f.context, f.parent);
    // Resumed by resume_at_return: `local` is the fiber's thread, back at return_address().
}

} // namespace gt::fiber
