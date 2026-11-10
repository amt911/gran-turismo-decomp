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
