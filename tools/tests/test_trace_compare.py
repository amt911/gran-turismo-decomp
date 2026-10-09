from pathlib import Path

from tools.trace_compare import (Divergence, compare, first_divergence, parse_gt_trace, parse_ppsspp_imports,
                                 parse_ppsspp_log)

PPSSPP_LOG = """\
D Loadable Segment Copied to 08804000, size 004d8488
I Module PDIAPP: 08ce9390 08c7b244 08c7b25c
D Importing sceKernelUnloadModule : 08c7a8c8
D Importing sceKernelCreateThread : 08c7a960
D Importing sceIoOpen : 08c7a8f0
I Function (scesupPreAcc,049d3ece) unresolved in 'PDIAPP', storing for later resolving
I 276=sceKernelCreateThread(user_main, 08804220, 00000040, 65536, 80004000, 00000000)
D Context switch: root -> user_main (275->276, pc: 08000030->08804220, returned from module) +159us
D 00000112=sceKernelGetModuleIdByAddress(0880410c): 00000112
D sceIoOpen(disc0:/PSP_GAME/USRDIR/GT.VOL, 00000001, 00000000): 0000000a
"""


def test_parse_ppsspp_log_keeps_only_hle_calls_in_order(tmp_path: Path):
    log = tmp_path / "ppsspp.log"
    log.write_text(PPSSPP_LOG)
    assert parse_ppsspp_log(log) == ["sceKernelCreateThread", "sceKernelGetModuleIdByAddress", "sceIoOpen"]


def test_parse_ppsspp_imports_maps_stub_address_to_name(tmp_path: Path):
    log = tmp_path / "ppsspp.log"
    log.write_text(PPSSPP_LOG)
    assert parse_ppsspp_imports(log) == {0x08C7A8C8: "sceKernelUnloadModule",
                                         0x08C7A960: "sceKernelCreateThread",
                                         0x08C7A8F0: "sceIoOpen"}


def test_parse_gt_trace_names_stubs_and_keeps_unknown_ones(tmp_path: Path):
    trace = tmp_path / "gt.trace"
    trace.write_text("0 0x08c7a960\n1 0x08c7b000\n\n2 0x08C7A8F0\n")
    names = {0x08C7A960: "sceKernelCreateThread", 0x08C7A8F0: "sceIoOpen"}
    assert parse_gt_trace(trace, names) == ["sceKernelCreateThread", "0x08c7b000", "sceIoOpen"]


def test_first_divergence_none_when_one_is_prefix_of_other():
    assert first_divergence(["a", "b"], ["a", "b", "c"]) is None


def test_first_divergence_reports_index_calls_and_three_previous():
    a = ["w", "x", "y", "z", "same", "left"]
    b = ["w", "x", "y", "z", "same", "right"]
    assert first_divergence(a, b) == Divergence(index=5, a_call="left", b_call="right",
                                                context=["y", "z", "same"])


def test_compare_drops_calls_ppsspp_never_logs():
    ppsspp = ["sceKernelCreateThread", "sceIoOpen"]
    ours = ["sceKernelGetSystemTimeLow", "sceKernelCreateThread", "sceKernelGetSystemTimeLow", "sceIoOpen"]
    result = compare(ppsspp, ours)
    assert result.divergence is None and result.matched == 2


def test_compare_reports_divergence_after_filtering():
    ppsspp = ["sceKernelCreateThread", "sceIoOpen", "sceIoRead"]
    ours = ["sceKernelCreateThread", "sceIoRead", "sceIoOpen"]
    result = compare(ppsspp, ours)
    # sceIoOpen is found after skipping sceIoRead; sceIoRead is then needed but was skipped: reordering.
    assert result.matched == 2
    assert result.divergence == Divergence(index=2, a_call="sceIoRead", b_call="<end>",
                                           context=["sceKernelCreateThread", "sceIoOpen"])


def test_parse_ppsspp_log_accepts_symbolic_return_values(tmp_path: Path):
    log = tmp_path / "ppsspp.log"
    log.write_text("D 00000112=sceKernelGetModuleId()\n"
                   "E ILLEGAL_PRIORITY=sceKernelChangeThreadPriority(0, 1): bogus priority\n"
                   "I scePowerSetClockFrequency(333,333,166)\n")
    assert parse_ppsspp_log(log) == ["sceKernelGetModuleId", "sceKernelChangeThreadPriority",
                                     "scePowerSetClockFrequency"]


def test_import_names_with_numeric_suffix_map_to_the_logged_call_name(tmp_path: Path):
    log = tmp_path / "ppsspp.log"
    log.write_text("D Importing scePowerSetClockFrequency350 : 08c7af70\n"
                   "I scePowerSetClockFrequency(333,333,166)\n")
    trace = tmp_path / "gt.trace"
    trace.write_text("0 0x08c7af70\n")
    ours = parse_gt_trace(trace, parse_ppsspp_imports(log))
    assert compare(parse_ppsspp_log(log), ours).matched == 1


def test_compare_tolerates_extra_calls_ppsspp_does_not_log_every_time():
    ppsspp = ["CreateCallback", "RegisterExitCallback", "SetSamplingMode"]
    ours = ["CreateCallback", "CheckCallback", "RegisterExitCallback", "SetSamplingMode"]
    ppsspp_logged = ppsspp + ["CheckCallback"]  # PPSSPP logs CheckCallback elsewhere, not here
    result = compare(ppsspp_logged[:3], ours, extra_names={"CheckCallback"})
    assert result.divergence is None and result.matched == 3


def test_compare_still_fails_on_reordering():
    result = compare(["a", "b", "c"], ["a", "c", "b"])
    assert result.matched == 2 and result.divergence is not None and result.divergence.a_call == "c"


def test_compare_gives_up_after_too_many_extra_calls():
    ppsspp = ["a", "b"]
    ours = ["a"] + ["x"] * 60 + ["b"]
    result = compare(ppsspp, ours, extra_names={"x"}, max_gap=50)
    assert result.matched == 1 and result.divergence is not None


def test_game_calls_keeps_only_imported_functions():
    from tools.trace_compare import game_calls
    imports = {0x08C7A8C0: "sceKernelRegisterExitCallback", 0x08C7AF70: "scePowerSetClockFrequency350"}
    calls = ["sceKernelRegisterExitCallback", "__KernelReturnFromMipsCall", "scePowerSetClockFrequency"]
    assert game_calls(calls, imports) == ["sceKernelRegisterExitCallback", "scePowerSetClockFrequency"]
