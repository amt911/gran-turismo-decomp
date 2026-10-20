// Which Sony imports the diff harness may run inside a case, and how (gt::ImportStub, host/diff_harness.hpp,
// docs/superpowers/specs/2026-10-13-import-record-replay-design.md). Kept apart from main.cpp so a unit test
// (host/tests/import_lists_test.cpp) can check the invariants of the lists without booting the game.
#pragma once

#include <cstdint>
#include <unordered_map>
#include <unordered_set>

namespace gt {

// NIDs whose HLE only touches guest RAM and the snapshotted kernel state (host/kernel_replay_state.hpp): they
// run in both runs of a harness case. Their HLE must write guest RAM through GuestMemory, never with a system
// call into it — a case write-protects RAM to see which pages change (host/dirty_pages.hpp), and a system call
// would fail on a protected page instead of faulting.
inline const std::unordered_set<std::uint32_t> &replay_safe_nids() {
    static const std::unordered_set<std::uint32_t> nids{
        0xBEA46419u, 0x1FC64E09u, 0x15B6446Bu, 0xDC692EE3u,  // Lock/LockCB/Unlock/TryLock LwMutex
        0x092968F4u, 0x5F10D406u,                            // CpuSuspendIntr / CpuResumeIntr
        0x79D1C3FAu, 0xB435DEC5u, 0x3EE30821u, 0x34B9FA9Eu, 0xBFA98062u,  // Dcache maintenance
        0x369ED59Du, 0x82BC5777u,  // GetSystemTimeLow / Wide: read the virtual clock only
        0x94AA61EEu,               // GetThreadCurrentPriority: read only
        0x71BC9871u,               // ChangeThreadPriority: priorities snapshotted, no switch in a case
        0x19CFF145u, 0x60107536u,  // Create / Delete LwMutex: work area in RAM, uid counter snapshotted
        0xB011B11Fu, 0x6B30100Fu,  // Lock / Unlock Mutex: owner and count snapshotted
        0xFE707FDFu, 0x50F61D8Au, 0xDB83A952u,  // Alloc/Free/GetAddr MemoryBlock: allocator snapshotted
        0xA5DA2406u, 0x34B78343u,  // sceUtilityGetSystemParamInt / String: constants written to guest RAM
        0x13A5ABEFu,               // sceKernelPrintf: the GT HLE returns 0 and prints nothing (gt_kernel.cpp)
        0x0C622081u,               // sceWlanGetEtherAddr(mac): a fixed MAC written to guest RAM through GuestMemory
        // Batch 104: HLEs that answer a constant and read no argument (gt_kernel.cpp / font_hle.cpp), so nothing but v0 changes.
        0x7E69EDA4u,               // sceHprmIsHeadphoneExist: set_success (0)
        0x02D7F94Bu,               // sceFont call that answers 0 (font_hle.cpp)
    };
    return nids;
}

// NIDs the harness records in the original's run and replays in the decompiled run: their HLE never switches PSP
// threads (or refuses to inside a case: break_kernel_isolation). Value: the argument registers the import reads
// (bit i = a0-a3, t0-t3; EABI puts a 64-bit argument in an even pair, leaving a padding register unread).
inline const std::unordered_map<std::uint32_t, std::uint8_t> &recordable_nids() {
    static const std::unordered_map<std::uint32_t, std::uint8_t> nids{
        {0x8EF08FCEu, 0x01u},  // sceUmdWaitDriveStat(stat): returns at once
        {0x446D8DE6u, 0x3Fu},  // sceKernelCreateThread(name, entry, prio, stack, attr, opt): no switch until Start
        {0x293B45B8u, 0x00u},  // sceKernelGetThreadId
        {0xEA748E31u, 0x03u},  // sceKernelChangeCurrentThreadAttr(clear, set)
        {0xFDB5BFE9u, 0x00u},  // scePowerGetCpuClockFrequencyInt: 333
        {0x090CCB3Fu, 0x01u},  // sceKernelPowerTick(type)
        {0xE175EF66u, 0x00u},  // sceSasGetOutputmode
        {0x099EF33Cu, 0x07u},  // sceFontFindOptimumFont(lib, style, error)
        {0xA1336091u, 0x01u},  // sceNpDrmSetLicenseeKey(key)
        {0x17C1684Eu, 0x03u},  // sceKernelReferThreadStatus(uid, info): writes the info struct
        {0xD59EAD2Fu, 0x01u},  // sceKernelWakeupThread(uid): refused in a case when it would preempt
        {0x9ACE131Eu, 0x00u},  // sceKernelSleepThread: only when a pending wakeup returns it at once,
        {0x82826F70u, 0x00u},  // sceKernelSleepThreadCB  otherwise refused in a case (gt_kernel.cpp)
        {0x109F50BCu, 0x07u},  // sceIoOpen(path, flags, mode): synchronous host open
        {0x27EB27B8u, 0x1Du},  // sceIoLseek(fd, -, offset lo, offset hi, whence): a1 is padding
        {0xF475845Du, 0x07u},  // sceKernelStartThread(uid, size, args): refused in a case when it would preempt
        {0xE7C27D1Bu, 0x01u},  // sceRtcGetCurrentClockLocalTime(time): a fixed start date plus the virtual time, written as a 16-byte date to guest RAM
        // Batch 103 (evidence: FINDINGS "Batch 103 unlocked ..."; every HLE below was read in host/gt_kernel.cpp or
        // lcs_sas.cpp: none switches threads, none blocks; the host-side tables they change are not in the
        // replay state, which is why they are recorded once instead of run twice).
        {0xA291F107u, 0x00u},  // sceKernelMaxFreeMemSize(): reads the partition and thread-stack cursors
        {0x237DBD4Fu, 0x0Au},  // sceKernelAllocPartitionMemory(-, name, -, size, -): bump allocation, zeroes the block
        {0x9D9A5BA1u, 0x01u},  // sceKernelGetBlockHeadAddr(uid): table lookup
        {0xB6D61D02u, 0x01u},  // sceKernelFreePartitionMemory(uid): table erase
        {0xD6DA4BA1u, 0x0Du},  // sceKernelCreateSema(name, -, initial, max): new table entry, no switch
        {0x55C20A00u, 0x07u},  // sceKernelCreateEventFlag(name, attr, pattern): new table entry, no switch
        // NOT sceKernelCreateMutex (0xB7D098C6): its table entry is part of the replay state (owner, count), so recording it made func_08B6441C mismatch with "kernel state the import changes differs" (full gate f1).
        {0xCA04A2B9u, 0x0Fu},  // sceKernelRegisterSubIntrHandler(intr, sub, handler, arg): table insert, the handler is not called
        {0xFB8E22ECu, 0x03u},  // sceKernelEnableSubIntr(intr, sub): flag in the table
        {0xD61E6961u, 0x03u},  // sceKernelReleaseSubIntrHandler(intr, sub): table erase
        {0x9FA03CD3u, 0x01u},  // sceKernelDeleteThread(uid): only a Created / Completed thread, never switches
        {0x810C4BC3u, 0x01u},  // sceIoClose(fd): file table erase, synchronous
        {0x3F53E640u, 0x03u},  // sceKernelSignalSema(uid, n): refused in a case when it would wake a waiter or preempt
        {0x4E3A1105u, 0x03u},  // sceKernelWaitSema(uid, n): refused in a case unless the count satisfies it at once
        {0x6D212BACu, 0x03u},  // sceKernelWaitSemaCB(uid, n): same handler
        {0x1FB15A32u, 0x03u},  // sceKernelSetEventFlag(uid, bits): refused in a case when it would wake a waiter or preempt
        {0x812346E4u, 0x03u},  // sceKernelClearEventFlag(uid, bits): pattern update only
        {0x402FCF22u, 0x0Fu},  // sceKernelWaitEventFlag(uid, bits, mode, out): refused in a case unless the pattern satisfies it
        {0x328C546Au, 0x0Fu},  // sceKernelWaitEventFlagCB: same handler
        {0x30FD48F0u, 0x0Fu},  // sceKernelPollEventFlag(uid, bits, mode, out): never waits
        // sceSasCore (tools/src/lcs-recomp/lcs/host/lcs_sas.cpp): the voice table and the mixer live on the host; the
        // mix functions write the output buffer through GuestMemory. Nothing waits.
        {0x42778A9Fu, 0x1Fu},  // __sceSasInit(core, grain, voices, mode, rate)
        {0x99944089u, 0x1Fu},  // __sceSasSetVoice(core, voice, vag address, size, loop): all read by the HLE
        {0x019B25EBu, 0x7Fu},  // __sceSasSetADSR(core, voice, flags, a, d, s, r)
        {0x9EC3676Au, 0x7Fu},  // __sceSasSetADSRmode(core, voice, flags, a, d, s, r)
        {0x5F9529F6u, 0x07u},  // __sceSasSetSL(core, voice, level)
        {0xB7660A23u, 0x07u},  // __sceSasSetNoise(core, voice, frequency)
        {0xAD84D37Fu, 0x07u},  // __sceSasSetPitch(core, voice, pitch)
        {0x440CA7D8u, 0x3Fu},  // __sceSasSetVolume(core, voice, l, r, el, er)
        {0xCBCD4F79u, 0x0Fu},  // __sceSasSetSimpleADSR(core, voice, adsr1, adsr2)
        {0x76F01ACAu, 0x03u},  // __sceSasSetKeyOn(core, voice)
        {0xA0CF2FA4u, 0x03u},  // __sceSasSetKeyOff(core, voice)
        {0x787D04D5u, 0x07u},  // __sceSasSetPause(core, mask, pause)
        {0x2C8E6AB3u, 0x01u},  // __sceSasGetPauseFlag(core)
        {0x68A46B95u, 0x01u},  // __sceSasGetEndFlag(core)
        {0x74AE582Au, 0x03u},  // __sceSasGetEnvelopeHeight(core, voice)
        {0x33D4AB37u, 0x03u},  // __sceSasSetEffectType(core, type)
        {0x267A6DD2u, 0x07u},  // __sceSasSetEffectParam(core, delay, feedback)
        {0xD5A229C9u, 0x07u},  // __sceSasSetEffectVolume(core, l, r)
        {0xF983B186u, 0x07u},  // __sceSasSetEffect(core, dry, wet)
        {0xA3589D81u, 0x03u},  // __sceSasCoreWithMix(core, out): renders one grain into guest RAM
        {0x50A14DFCu, 0x0Fu},  // __sceSasCore(core, inout, l, r)
        // Batch 104 (host/gt_kernel.cpp: none waits or switches threads; the state they change is in host tables the replay
        // state does not hold, so they run once in the original's run and every argument their HLE reads is compared).
        {0x289D82FEu, 0x0Fu},  // sceDisplaySetFrameBuf(address, stride, format, sync): reads a0-a3, sets display_state, counts a flip; no wait
    };
    return nids;
}

}  // namespace gt
