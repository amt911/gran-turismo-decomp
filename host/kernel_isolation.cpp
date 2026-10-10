#include "kernel_isolation.hpp"

namespace gt {
namespace {
bool g_kernel_isolated = false;
bool g_kernel_isolation_broken = false;
} // namespace

void set_kernel_isolated(bool isolated) noexcept {
    g_kernel_isolated = isolated;
    g_kernel_isolation_broken = false;
}
bool kernel_isolated() noexcept { return g_kernel_isolated; }
void break_kernel_isolation() noexcept { g_kernel_isolation_broken = true; }
bool kernel_isolation_broken() noexcept { return g_kernel_isolation_broken; }

} // namespace gt
