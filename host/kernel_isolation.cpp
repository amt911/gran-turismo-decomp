#include "kernel_isolation.hpp"

namespace gt {
namespace {
bool g_kernel_isolated = false;
bool g_kernel_isolation_broken = false;
const char *g_kernel_isolation_reason = "";
} // namespace

void set_kernel_isolated(bool isolated) noexcept {
    g_kernel_isolated = isolated;
    g_kernel_isolation_broken = false;
}
bool kernel_isolated() noexcept { return g_kernel_isolated; }
void break_kernel_isolation(const char *reason) noexcept {
    g_kernel_isolation_broken = true;
    g_kernel_isolation_reason = reason;
}
const char *kernel_isolation_reason() noexcept { return g_kernel_isolation_reason; }
bool kernel_isolation_broken() noexcept { return g_kernel_isolation_broken; }

} // namespace gt
