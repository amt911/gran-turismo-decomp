from tools.verdict_scope import needs_synthetic

RENDERER_PATCH = "--- a/lcs/host/ge_renderer.cpp\n+++ b/lcs/host/ge_renderer.cpp\n@@ -1 +1 @@\n"
RUNTIME_PATCH = "--- a/include/psprecomp/guest_memory.hpp\n+++ b/include/psprecomp/guest_memory.hpp\n"


def read(path):
    return {"tools/patches/lcs-recomp/0063-ge-stencil.patch": RENDERER_PATCH,
            "tools/patches/lcs-recomp/0060-mem.patch": RUNTIME_PATCH}.get(path)


def test_decomp_source_needs_synthetic():
    assert needs_synthetic(["src/game/unsorted/hot_batch_400.cpp", "README.md"], read)


def test_headers_harness_and_build_need_synthetic():
    for path in ["include/gt/guest.hpp", "host/guest_call.cpp", "tools/decomp_gate.py", "CMakeLists.txt",
                 "config/symbol_addrs.txt", "tools/patches/lcs-recomp/0060-mem.patch"]:
        assert needs_synthetic([path], read), path


def test_unreadable_patch_needs_synthetic():
    assert needs_synthetic(["tools/patches/lcs-recomp/0099-unknown.patch"], read)


def test_docs_mods_routes_and_renderer_skip_it():
    assert not needs_synthetic(["docs/FACTS.md", "README.md", "AGENTS.md", "config/pad_routes/x.pad",
                                "tools/mod_pedals_check.py", "tools/tests/test_mod_pedals_check.py",
                                "host/mod_store.cpp", "host/tests/mod_store_test.cpp", "host/ge_trace.cpp",
                                "tools/patches/lcs-recomp/0063-ge-stencil.patch"], read)


def test_empty_diff_skips():
    assert not needs_synthetic([], read)


# --- only_functions: the synthetic gate limited to the PR's own functions (user 2026-11-24: "y no se pueden acelerar mas
# las verificaciones"; each scene spent 650-950 s comparing all ~17.5k registered functions, verdict 842).
from tools.verdict_scope import only_functions

BATCH = "NEW7(0x0880A000u, func_0880A000, Word);\nNEW7(0x0880A0A0u, func_0880A0A0, Word);\n"
REWRITE = "HOT(0x08C0FFEEu, func_08C0FFEE);\n// mentions 0x08800000 in a comment\n"


def heads(path):
    return {"src/game/unsorted/new_batch_main_7.cpp": BATCH, "src/game/unsorted/hot_batch_3.cpp": REWRITE,
            "config/synth_shapes.txt": "0x0880A000 reg:a0=ptr\n0x08900000 reg:a1=ptr\n0x08C0FFEE reg:a2=ptr\n",
            "src/game/unsorted/x.hpp": "// a header the batch files include\n"}.get(path)


def bases(path):
    return {"config/synth_shapes.txt": "0x08900000 reg:a1=ptr\n"}.get(path)


def test_decomp_files_give_their_registrations():
    got = only_functions(["src/game/unsorted/new_batch_main_7.cpp", "src/game/unsorted/hot_batch_3.cpp", "README.md"],
                         heads, bases)
    assert got == [0x0880A000, 0x0880A0A0, 0x08C0FFEE]


def test_new_shape_lines_add_their_address():
    assert only_functions(["config/synth_shapes.txt"], heads, bases) == [0x0880A000, 0x08C0FFEE]


def test_anything_else_runs_every_function():
    for path in ["include/gt/game/hot_common.hpp", "host/guest_call.cpp", "tools/decomp_gate.py", "CMakeLists.txt",
                 "src/game/unsorted/x.hpp", "config/extra_seeds.txt", "tools/patches/lcs-recomp/0060-mem.patch"]:
        assert only_functions(["src/game/unsorted/new_batch_main_7.cpp", path], heads, bases) is None, path


def test_deleted_file_or_nothing_registered_runs_every_function():
    assert only_functions(["src/game/unsorted/gone.cpp"], heads, bases) is None
    assert only_functions(["README.md"], heads, bases) is None


def test_cli_writes_the_list(tmp_path, monkeypatch):
    import tools.verdict_scope as vs
    monkeypatch.setattr(vs, "_changed", lambda base, head: ["src/game/unsorted/new_batch_main_7.cpp"])
    monkeypatch.setattr(vs, "_reader", lambda ref: heads if ref == "HEAD" else bases)
    out = tmp_path / "only.txt"
    printed = []
    monkeypatch.setattr("builtins.print", lambda *a: printed.append(" ".join(map(str, a))))
    assert vs.main(["x", "BASE", "HEAD", "--only-file", str(out)]) == 0
    assert printed == ["only 2"] and out.read_text() == "0x0880A000\n0x0880A0A0\n"


def test_cli_without_only_file_keeps_the_old_answer(monkeypatch):
    import tools.verdict_scope as vs
    monkeypatch.setattr(vs, "_changed", lambda base, head: ["src/game/unsorted/new_batch_main_7.cpp"])
    monkeypatch.setattr(vs, "_reader", lambda ref: heads if ref == "HEAD" else bases)
    printed = []
    monkeypatch.setattr("builtins.print", lambda *a: printed.append(" ".join(map(str, a))))
    vs.main(["x", "BASE", "HEAD"])
    assert printed == ["synthetic"]
