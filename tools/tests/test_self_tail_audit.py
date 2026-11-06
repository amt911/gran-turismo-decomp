"""Tests for tools/self_tail_audit.py: decompiled functions that tail-call their own entry."""

from __future__ import annotations

from pathlib import Path

from tools import self_tail_audit as audit

REPO = Path(__file__).resolve().parents[2]


def test_a_tail_call_to_the_own_entry_is_found():
    text = (
        "void func_08968850(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {\n"
        "    if (next != 0u) tail_call(ctx, 0x08968850u, {next, flag});\n"
        "}\n"
    )
    assert audit.self_tail_calls(text) == ["08968850"]


def test_jump_and_tail_with_to_the_own_entry_are_found():
    text = (
        "void func_08B1928C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { if (tail) jump(ctx, 0x08B1928Cu); }\n"
        "void func_08B19354(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_with(ctx, 0x08B19354u, 6u, 1u); }\n"
    )
    assert audit.self_tail_calls(text) == ["08B1928C", "08B19354"]


def test_a_tail_call_to_another_function_and_a_call_to_itself_are_fine():
    text = (
        "void func_08B19354(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_with(ctx, 0x08B1928Cu, 6u, 1u); }\n"
        "void func_0890E41C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {\n"
        "    (void)call_guest(rt, ctx, 0x0890E41Cu, {self});\n"
        "}\n"
    )
    assert audit.self_tail_calls(text) == []


def test_the_tree_has_no_self_tail_calls():
    # A tail call to the function's own entry leaves ctx.pc == entry, which the trampoline, the fibers and call_here
    # read as "returned": the recursion is silently skipped (the black cars of --decomp on, docs/FINDINGS.md).
    found = audit.audit(sorted((REPO / "src").rglob("*.cpp")))
    assert found == [], f"self tail calls (write them as a loop): {found}"
