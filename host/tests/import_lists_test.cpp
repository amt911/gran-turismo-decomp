// Invariants of the harness's import lists (host/import_lists.hpp): a replay-safe import is never also recorded, the
// imports that block or always switch threads are on neither list, and the ones batch 103 added are where their
// evidence put them. Plain executable, run by CTest.
#include "import_lists.hpp"

#include <array>
#include <cstdio>

namespace {

int failures = 0;

void check(bool condition, const char *what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

}  // namespace

int main() {
    const auto &safe = gt::replay_safe_nids();
    const auto &recordable = gt::recordable_nids();

    for (const auto nid : safe) check(!recordable.contains(nid), "an import is replay-safe or recordable, not both");
    for (const auto &[nid, mask] : recordable) {
        (void)nid;
        check(mask <= 0x7Fu, "argument masks name at most a0-a3 and t0-t2 here");
    }

    // A recordable import's mask must name every argument its HLE reads: an argument left out is not compared, and an
    // effect on host-side state (the Sas voice table) is invisible to the RAM diff. __sceSasSetVoice reads core, voice,
    // the VAG address, its size and the loop flag (lcs_sas.cpp), a0-a3 and t0 (review of #251).
    if (const auto it = recordable.find(0x99944089u); it != recordable.end())
        check(it->second == 0x1Fu, "__sceSasSetVoice compares all five arguments (mask 0x1F)");

    // Batch 104: the display call is compared on every argument its HLE reads (a missing bit would leave an
    // argument unchecked while the effect lives in host state). sceDisplaySetFrameBuf(address, stride, format, sync) reads a0-a3
    // (gt_kernel.cpp).
    check(recordable.contains(0x289D82FEu) && recordable.at(0x289D82FEu) == 0x0Fu, "sceDisplaySetFrameBuf compares a0-a3 (mask 0x0F)");
    for (const auto nid : {0x7E69EDA4u, 0x02D7F94Bu}) check(safe.contains(nid), "a constant answer that reads nothing is replay-safe");
    // The waits stay out: the display waits and the GE sync block on the host.
    check(!safe.contains(0x289D82FEu), "SetFrameBuf changes host state: recorded, never run twice");

    // Imports that block, wait on a device or switch threads on every call: never allowed inside a case.
    const std::array<std::uint32_t, 13> never{
        0xCEADEB47u,  // sceKernelDelayThread
        0x68DA9E36u,  // sceKernelDelayThreadCB
        0x278C0DF5u,  // sceKernelWaitThreadEnd
        0x36CDFADEu, 0x984C27E7u, 0x46F186C3u,  // sceDisplayWaitVblankStart*
        0xB287BD61u,  // sceGeDrawSync
        0xEBD177D6u,  // scePowerSetClockFrequency: delays the caller 150 ms
        0x2A2B3DE0u,  // sceUtilityLoadModule: delays the caller
        0x6A638D83u,  // sceIoRead: writes the buffer with a system call and defers the thread
        0x2D53F36Eu,  // sceAudioOutput2OutputBlocking
        0x28B6489Cu,  // sceKernelDeleteSema: wakes the waiters and may preempt
        0xEF9E4C70u,  // sceKernelDeleteEventFlag: same
    };
    for (const auto nid : never) {
        check(!safe.contains(nid), "a blocking import is not replay-safe");
        check(!recordable.contains(nid), "a blocking import is not recordable");
    }

    // The semaphore pair is recordable only because the kernel refuses a blocking wait / a waking signal in a case.
    check(recordable.contains(0x4E3A1105u) && recordable.contains(0x6D212BACu) && recordable.contains(0x3F53E640u),
          "sceKernelWaitSema(CB) and sceKernelSignalSema are recordable");
    check(!safe.contains(0x4E3A1105u) && !safe.contains(0x3F53E640u), "semaphores are host state: recorded, never run twice");
    check(recordable.contains(0x402FCF22u) && recordable.contains(0x1FB15A32u) && !safe.contains(0x402FCF22u),
          "event flag wait / set are recordable (refused in a case when they would block or wake)");
    check(safe.contains(0x13A5ABEFu), "sceKernelPrintf is replay-safe");

    if (failures != 0) return 1;
    std::puts("import_lists: ok");
    return 0;
}
