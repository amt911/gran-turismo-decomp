"""tools.batch_emit: the register-level emitter's statements, delay slots, frames, refusals and the pool bookkeeping."""

from __future__ import annotations

import pytest

from tools import batch_emit
from tools.batch_emit import Refused, draft, done_addresses, generate, parse_disasm


def disasm(*rows: str, address: int = 0x08800000) -> str:
    lines = [f"== func_{address:08X} {len(rows)} insns"]
    for index, row in enumerate(rows):
        word, text = row.split("|", 1)
        lines.append(f"  {address + 4 * index:08X}: {word}  {text}")
    return "\n".join(lines)


def emit(monkeypatch: pytest.MonkeyPatch, *rows: str) -> str:
    monkeypatch.setattr(batch_emit, "disassemble", lambda address: disasm(*rows))
    return draft(0x08800000)


def test_leaf_with_branch_and_delay_slot(monkeypatch):
    out = emit(monkeypatch,
               "10800003|beqz        $a0, -> 08800010",   # beq a0, zero, +2
               "24020001|addiu       $v0, $zero, 0x1",   # delay slot
               "03E00008|jr          $ra",
               "00000000|nop",
               "03E00008|jr          $ra",
               "00000000|nop")
    assert "{ const bool tk = G[4] == 0u; G[2] = 0x1u; if (tk) goto L_08800010; }" in out
    assert out.count("goto exit_;") == 3 and "GuestFrame" not in out


def test_frame_stores_and_call(monkeypatch):
    out = emit(monkeypatch,
               "27BDFFE0|addiu       $sp, $sp, -0x20",
               "AFBF001C|sw          $ra, 0x1C($sp)",
               "0C000000|jal         func_0 -> func_08800100",
               "24050004|addiu       $a1, $zero, 0x4",
               "8FBF001C|lw          $ra, 0x1C($sp)",
               "03E00008|jr          $ra",
               "27BD0020|addiu       $sp, $sp, 0x20")
    assert "GuestFrame frame(ctx, 0x20u);" in out
    assert "put(rt, frame.at(0x1Cu), G[31]);" in out
    assert "G[5] = 0x4u; call_here(rt, ctx, 0x08800100u);" in out
    assert "G[31] = word(rt, frame.at(0x1Cu));" in out


def test_tail_jump_outside(monkeypatch):
    out = emit(monkeypatch, "08000000|j           func_0 -> func_08900000", "24020001|addiu       $v0, $zero, 0x1")
    assert "G[2] = 0x1u; tail = 0x08900000u; goto exit_;" in out
    assert "if (tail != 0u) jump(ctx, tail);" in out


@pytest.mark.parametrize("rows, reason", [
    (["46000000|add.s       $f0, $f0, $f0", "03E00008|jr          $ra", "00000000|nop"], "opcode"),
    (["03200008|jr          $t9", "00000000|nop"], "jr to a register"),
    (["10000001|b           -> 08800008", "10000001|b           -> 08800008", "00000000|nop"], "delay slot"),
    (["8FA20004|lw          $v0, 0x4($sp)", "03E00008|jr          $ra", "00000000|nop"], "without a frame"),
    (["3C020009|lui         $v0, 0x9  [HI]", "03E00008|jr          $ra", "00000000|nop"], "high half"),
])
def test_refusals(monkeypatch, rows, reason):
    with pytest.raises(Refused, match=reason):
        emit(monkeypatch, *rows)


def test_relocated_pair_is_one_constant(monkeypatch):
    out = emit(monkeypatch,
               "3C020009|lui         $v0, 0x9  [HI]",
               "8C43006C|lw          $v1, 0x6C($v0)  [LO] = 0x08D6CE00",
               "03E00008|jr          $ra",
               "00000000|nop")
    assert "G[2] = high_half(0x08D6CE00u);" in out
    assert "G[3] = word(rt, 0x08D6CE00u);" in out


def test_low_half_of_a_changed_register_keeps_the_register(monkeypatch):
    out = emit(monkeypatch,
               "3C020009|lui         $v0, 0x9  [HI]",
               "00441021|addu        $v0, $v0, $a0",
               "8C43006C|lw          $v1, 0x6C($v0)  [LO] = 0x08D6CE00",
               "03E00008|jr          $ra",
               "00000000|nop")
    assert "G[3] = word(rt, G[2] + sext(0x08D6CE00u, 16u));" in out


def test_alu_family(monkeypatch):
    out = emit(monkeypatch,
               "00851021|addu        $v0, $a0, $a1",
               "7C021C20|seb         $v1, $v0",
               "7CA31E00|ext         $v1, $a1, 24, 4",
               "00850018|mult        $a0, $a1",
               "00001012|mflo        $v0",
               "03E00008|jr          $ra",
               "00000000|nop")
    assert "G[2] = G[4] + G[5];" in out and "ctx.lo = " in out and "G[2] = ctx.lo;" in out


def test_generate_and_helpers():
    source = generate(7, ["void func_08800000(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = sext(G[4], 8u); }"],
                      "test,")
    assert "namespace hot_batch_7" in source and "HOT7(0x08800000u, func_08800000, Word);" in source
    assert "std::uint32_t sext(" in source and "void divide(" not in source


def test_parse_and_done_addresses(tmp_path):
    address, insns, jumps, calls = parse_disasm(disasm("08000000|j           func_0 -> func_08800008", "00000000|nop"))
    assert address == 0x08800000 and len(insns) == 2 and jumps == {0x08800000: 0x08800008} and calls == {}
    (tmp_path / "a.cpp").write_text("void func_08800000(int) {}\nHOT1(0x08800010u, f, Word);\n// HOT1(0x08800020u, g, Word);\n")
    assert done_addresses(tmp_path) == {0x08800000, 0x08800010, 0x08800020}


@pytest.mark.parametrize("word, text, expected", [
    ("00851023", "subu        $v0, $a0, $a1", "G[2] = G[4] - G[5];"),
    ("00851024", "and         $v0, $a0, $a1", "G[2] = G[4] & G[5];"),
    ("00851025", "or          $v0, $a0, $a1", "G[2] = G[4] | G[5];"),
    ("00851026", "xor         $v0, $a0, $a1", "G[2] = G[4] ^ G[5];"),
    ("00851027", "nor         $v0, $a0, $a1", "G[2] = ~(G[4] | G[5]);"),
    ("0085102A", "slt         $v0, $a0, $a1", "G[2] = static_cast<std::int32_t>(G[4]) < static_cast<std::int32_t>(G[5]) ? 1u : 0u;"),
    ("0085102B", "sltu        $v0, $a0, $a1", "G[2] = G[4] < G[5] ? 1u : 0u;"),
    ("00041100", "sll         $v0, $a0, 4", "G[2] = G[4] << 4u;"),
    ("00041102", "srl         $v0, $a0, 4", "G[2] = G[4] >> 4u;"),
    ("00041103", "sra         $v0, $a0, 4", "G[2] = sra(G[4], 4u);"),
    ("00A41004", "sllv        $v0, $a0, $a1", "G[2] = G[4] << (G[5] & 31u);"),
    ("00A41006", "srlv        $v0, $a0, $a1", "G[2] = G[4] >> (G[5] & 31u);"),
    ("00A41007", "srav        $v0, $a0, $a1", "G[2] = sra(G[4], G[5] & 31u);"),
    ("0085100A", "movz        $v0, $a0, $a1", "if (G[5] == 0u) G[2] = G[4];"),
    ("0085100B", "movn        $v0, $a0, $a1", "if (G[5] != 0u) G[2] = G[4];"),
    ("00851019", "multu       $a0, $a1", "ctx.lo = "),
    ("0085001A", "div         $a0, $a1", "divide(ctx, G[4], G[5]);"),
    ("0085001B", "divu        $a0, $a1", "ctx.lo = G[4] / G[5];"),
    ("00001010", "mfhi        $v0", "G[2] = ctx.hi;"),
    ("00800011", "mthi        $a0", "ctx.hi = G[4];"),
    ("00800013", "mtlo        $a0", "ctx.lo = G[4];"),
    ("0085102C", "max         $v0, $a0, $a1", "std::max("),
    ("0085102D", "min         $v0, $a0, $a1", "std::min("),
    ("2882FFFF", "slti        $v0, $a0, -0x1", "G[2] = static_cast<std::int32_t>(G[4]) < static_cast<std::int32_t>(0xFFFFFFFFu) ? 1u : 0u;"),
    ("28820005", "slti        $v0, $a0, 0x5", "G[2] = static_cast<std::int32_t>(G[4]) < 0x5 ? 1u : 0u;"),
    ("2C820005", "sltiu       $v0, $a0, 0x5", "G[2] = G[4] < 0x5u ? 1u : 0u;"),
    ("30820005", "andi        $v0, $a0, 0x5", "G[2] = G[4] & 0x5u;"),
    ("34820005", "ori         $v0, $a0, 0x5", "G[2] = G[4] | 0x5u;"),
    ("34020005", "ori         $v0, $zero, 0x5", "G[2] = 0x5u;"),
    ("38820005", "xori        $v0, $a0, 0x5", "G[2] = G[4] ^ 0x5u;"),
    ("3C020005", "lui         $v0, 0x5", "G[2] = 0x50000u;"),
    ("2482FFFC", "addiu       $v0, $a0, -0x4", "G[2] = G[4] - 0x4u;"),
    ("24820004", "addiu       $v0, $a0, 0x4", "G[2] = G[4] + 0x4u;"),
    ("80820004", "lb          $v0, 0x4($a0)", "G[2] = sext(load8(G[4] + 0x4u), 8u);"),
    ("90820004", "lbu         $v0, 0x4($a0)", "G[2] = load8(G[4] + 0x4u);"),
    ("84820004", "lh          $v0, 0x4($a0)", "G[2] = sext(load16(G[4] + 0x4u), 16u);"),
    ("94820004", "lhu         $v0, 0x4($a0)", "G[2] = load16(G[4] + 0x4u);"),
    ("A0820004", "sb          $v0, 0x4($a0)", "store8(G[4] + 0x4u, G[2]);"),
    ("A4820004", "sh          $v0, 0x4($a0)", "store16(G[4] + 0x4u, G[2]);"),
    ("7CA41004", "ins         $a0, $a1, 0, 3", "G[4] = ins(G[4], G[5], 0u, 3u);"),
    ("7C041C20", "seb         $v1, $a0", "G[3] = sext(G[4], 8u);"),
    ("00000000", "nop", ""),
    ("2400FFFF", "addiu       $zero, $zero, -0x1", ""),
    ("8C000004", "lw          $zero, 0x4($zero)", ""),
])
def test_single_instructions(monkeypatch, word, text, expected):
    out = emit(monkeypatch, f"{word}|{text}", "03E00008|jr          $ra", "00000000|nop")
    assert expected in out


@pytest.mark.parametrize("word, text, condition", [
    ("04800003", "bltz        $a0, -> 08800010", "static_cast<std::int32_t>(G[4]) < 0"),
    ("04810003", "bgez        $a0, -> 08800010", "static_cast<std::int32_t>(G[4]) >= 0"),
    ("18800003", "blez        $a0, -> 08800010", "static_cast<std::int32_t>(G[4]) <= 0"),
    ("1C800003", "bgtz        $a0, -> 08800010", "static_cast<std::int32_t>(G[4]) > 0"),
    ("14850003", "bne         $a0, $a1, -> 08800010", "G[4] != G[5]"),
    ("10850003", "beq         $a0, $a1, -> 08800010", "G[4] == G[5]"),
    ("10000003", "b           -> 08800010", "true"),
])
def test_branch_conditions(monkeypatch, word, text, condition):
    out = emit(monkeypatch, f"{word}|{text}", "00000000|nop", "03E00008|jr          $ra", "00000000|nop", "03E00008|jr          $ra",
               "00000000|nop")
    assert f"if ({condition}) goto L_08800010;" in out


def test_likely_branch_runs_its_delay_slot_only_when_taken(monkeypatch):
    out = emit(monkeypatch, "50850003|beql        $a0, $a1, -> 08800010", "24020001|addiu       $v0, $zero, 0x1",
               "03E00008|jr          $ra", "00000000|nop", "03E00008|jr          $ra", "00000000|nop")
    assert "if (G[4] == G[5]) { G[2] = 0x1u; goto L_08800010; }" in out


def test_jalr_reads_its_register_before_the_delay_slot(monkeypatch):
    out = emit(monkeypatch, "0040F809|jalr        $v0", "00402021|addu        $a0, $v0, $zero",
               "03E00008|jr          $ra", "00000000|nop")
    assert "{ const std::uint32_t callee = G[2]; G[4] = G[2]; call_here(rt, ctx, callee); }" in out


@pytest.mark.parametrize("rows, reason", [
    (["0040F809|jalr        $v0", "00000000|nop", "0040F809|jalr        $v0", "00000000|nop"], None),
    (["00401009|jalr        $v0, $v0", "00000000|nop"], "link register"),
    (["04900003|bltzal      $a0, -> 08800010", "00000000|nop", "03E00008|jr          $ra", "00000000|nop", "03E00008|jr          $ra", "00000000|nop""00000000|nop"], "linking"),
    (["10800009|beqz        $a0, -> 08800028", "00000000|nop", "03E00008|jr          $ra", "00000000|nop"], "outside"),
    (["27BDFFE0|addiu       $sp, $sp, -0x20", "27BD0020|addiu       $sp, $sp, 0x20", "8FA20004|lw          $v0, 0x4($sp)",
      "03E00008|jr          $ra", "00000000|nop"], "released"),
    (["8FA20004|lw          $v0, 0x4($sp)", "27BDFFE0|addiu       $sp, $sp, -0x20", "03E00008|jr          $ra", "00000000|nop"],
     "before the frame"),
    (["27BDFFE0|addiu       $sp, $sp, -0x20", "8FA2FFFC|lw          $v0, -0x4($sp)", "03E00008|jr          $ra",
      "00000000|nop"], "negative"),
    (["00800008|jr          $a0", "00000000|nop"], "jr to a register"),
    (["7C8410A0|wsbh        $v0, $a0", "03E00008|jr          $ra", "00000000|nop"], "special3"),
    (["00001020|add         $v0, $zero, $zero", "03E00008|jr          $ra", "00000000|nop"], "special"),
    (["10000001|b           -> 08800004", "00000000|nop"], "outside|delay"),
])
def test_more_refusals(monkeypatch, rows, reason):
    if reason is None:
        assert "call_here" in emit(monkeypatch, *rows)
        return
    with pytest.raises(Refused, match=reason):
        emit(monkeypatch, *rows)


def test_command_line(monkeypatch, tmp_path, capsys):
    monkeypatch.setattr(batch_emit, "disassemble", lambda address: disasm("24020001|addiu       $v0, $zero, 0x1",
                                                                         "03E00008|jr          $ra", "00000000|nop",
                                                                         address=address))
    (tmp_path / "src").mkdir()
    (tmp_path / "src" / "x.cpp").write_text("void func_08800010(int) {}\n")
    (tmp_path / "functions.tsv").write_text("0\t12\n" + f"{0x8800010 - batch_emit.LOAD_BASE & 0xFFFFFFFF:x}\t12\n"
                                            f"{0x8800100 - batch_emit.LOAD_BASE & 0xFFFFFFFF:x}\t12\n")
    assert batch_emit.main(["emit", "08800000"]) == 0
    assert "void func_08800000(" in capsys.readouterr().out
    out = tmp_path / "hot.cpp"
    assert batch_emit.main(["gen", "9", "08800000", "--out", str(out)]) == 0
    assert "HOT9(0x08800000u" in out.read_text()
    assert batch_emit.main(["pool", "--min", "1", "--max", "9", "--src", str(tmp_path / "src"),
                            "--functions-tsv", str(tmp_path / "functions.tsv")]) == 0
    assert "08800100 3" in capsys.readouterr().out


def test_command_line_reports_refusals(monkeypatch, capsys, tmp_path):
    monkeypatch.setattr(batch_emit, "disassemble", lambda address: disasm("46000000|add.s $f0, $f0, $f0"))
    assert batch_emit.main(["emit", "08800000"]) == 1
    assert "REFUSED" in capsys.readouterr().err
    assert batch_emit.main(["gen", "9", "08800000", "--out", str(tmp_path / "o.cpp")]) == 0


def test_madd_family_is_modelled_not_dropped(monkeypatch):
    out = emit(monkeypatch, "00D3001C|madd        $a2, $s3", "03E00008|jr          $ra", "00000000|nop")
    assert "accumulator" in out and "G[6]" in out and "G[19]" in out


def test_sync_is_refused_not_dropped(monkeypatch):
    with pytest.raises(Refused, match="special 0xf"):
        emit(monkeypatch, "0000000F|sync", "03E00008|jr          $ra", "00000000|nop")
