"""tools.sp_loads: a load of a callee-saved register from the stack that is no restore of a save is found."""

from tools import sp_loads

LINES = [
    "  08A1F328: 27BDFFB0  addiu       $sp, $sp, -0x50",
    "  08A1F32C: AFB50044  sw          $s5, 0x44($sp)",
    "  08A1F330: AFBF0048  sw          $ra, 0x48($sp)",
    "  08A1F334: AFB00030  sw          $s0, 0x30($sp)",
    "  08A1F3E0: 8FB00004  lw          $s0, 0x4($sp)",
    "  08A1F4E0: 8FB50044  lw          $s5, 0x44($sp)",
    "  08A1F4E4: 8FBF0048  lw          $ra, 0x48($sp)",
    "  08A1F4E8: 8FB00030  lw          $s0, 0x30($sp)",
    "  08A1F4EC: 8FA20010  lw          $v0, 0x10($sp)",
]


def test_only_the_load_without_a_matching_save_is_reported():
    found = sp_loads.real_loads(LINES)
    assert [line.split()[0] for line in found] == ["08A1F3E0:"]


def test_a_load_of_the_same_register_from_another_offset_than_its_save_is_reported():
    lines = ["  08000000: AFB00010  sw          $s0, 0x10($sp)", "  08000004: 8FB00014  lw          $s0, 0x14($sp)"]
    assert len(sp_loads.real_loads(lines)) == 1


def test_functions_of_a_source_are_the_literal_checks_functions():
    source = "void func_08000010(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {\n    call(0x08C49F24u);\n}\n"
    assert sp_loads.functions(source) == [0x08000010]
