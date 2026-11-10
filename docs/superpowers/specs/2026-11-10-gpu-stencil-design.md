# GPU stencil (NativeGE) — design

Status: draft for the user's approval (2026-11-10). Follows PR #518 (software stencil).

## Why

GT's Time Trial ghost is screen-door translucency built on the GE stencil. #518 implements it in the software renderer.
`NativeGE=true` (the Vulkan backend: high resolution, the path meant for cheap phones and the Raspberry Pi) still ignores
0x24/0xDC/0xDD, so with the GPU the ghost stays opaque, and so does any other stencil effect (races use an ALWAYS/REPLACE
0x80 pass and an EQUAL 0x80 pass every frame: FINDINGS "The GE stencil is the framebuffer alpha").

## The difficulty

On the PSP the stencil **is** the framebuffer's alpha channel:
- a draw with the stencil test **on** writes the op's result (keep/zero/replace/invert/incr/decr) into the alpha;
- a draw with the stencil test **off** writes its fragment alpha into the alpha (through the alpha write mask 0xE9);
- a clear with the alpha bit (0xD3 bit 9) sets it to the clear colour's alpha.

In Vulkan the stencil is a separate depth/stencil aspect. The colour alpha and the stencil have to be kept equal wherever
the game can observe either one: stencil tests, blends with destination-alpha factors, and framebuffer read-backs to guest
memory.

## Goals

- The Time Trial ghost with `NativeGE=true` looks like the software renderer's: the same pixels covered.
- The races' every-frame stencil passes stay correct; there is no regression in `tools/gpu_gate.sh`.
- It runs on GPUs without optional extensions (Raspberry Pi / phones). An extension may make it exact where available, but
  must not be required.

## Non-goals

- Pixel-exact stencil for effects GT does not use. Measured use only: the ghost pattern plus the two race passes.
- The DX12 backend (a later port of whatever this settles).

## Design (my calls, marked; open to change)

0. **Measure first** (software renderer, exact since #518). On the TT ghost frame and on a race frame, dump the framebuffer
   alpha at three points: before the pattern sprite, after it, and after the ghost. This shows which alpha values the
   stencil-off draws leave there, and so which approximation is enough. A Time Trial route that finishes one lap (the ghost
   appears) is needed for this and for the gate. Today no route does, so this step adds `tt_ghost.pad`, from the user's
   save, built on `perf_tt_newyork`.
1. **Attachment:** the depth image becomes depth+stencil. Use `D32_SFLOAT_S8_UINT` if supported, else `D24_UNORM_S8_UINT`
   (my call: query both, as PPSSPP does).
2. **Stencil-on draws** map 1:1 to Vulkan stencil state:
   - the compare is `(ref & mask) OP (stencil & mask)` (same order in both);
   - the ops map one to one;
   - `writeMask = ~0xE9 & 0xFF`;
   - stencil state is part of the pipeline key, with ref, compare mask and write mask as dynamic state.
   The colour alpha written by these draws is the same op's result. The fragment shader cannot know the result, so the
   colour alpha write is masked off for these draws. The alpha is rebuilt from the stencil when it is read (step 5).
3. **Clears** with the alpha bit also clear the stencil to the clear alpha.
4. **Stencil-off draws** that write alpha (0xE9 not 0xFF):
   - **exact path:** `VK_EXT_shader_stencil_export` writes the fragment alpha as the stencil (`gl_FragStencilRefARB`), when
     the device has it (AMD, Intel Mesa; not NVIDIA, not the Pi);
   - **fallback:** chosen from the step-0 measurement. Either leave the stencil unchanged (enough if, for every stencil-on
     draw GT makes, the stencil-off draws before it leave the alpha bit 0x80 as the last clear set it), or replace the
     stencil with the draw's constant alpha when that alpha does not vary per fragment (no vertex colour, no texture alpha).
     Step 0 decides between them; the spec is updated before any code.
5. **Read-backs** (framebuffer copied to guest memory, `readback` path): the alpha byte comes from the stencil, in a copy
   pass, so the guest sees what the PSP would.

## Testing

- **Unit:** the descriptor→Vulkan stencil state mapping as pure functions, with tests (ops, compare, masks, the key).
- **GPU gate:** the new TT route plus the existing routes, with GPU frames compared to software frames
  (`tools/gpu_gate.sh`). On the ghost frames, the ghost's pixel coverage must match the software renderer. Sabotage: drop
  the stencil test and the gate must fail on the ghost frames.
- **User:** the Time Trial ghost with `NativeGE=true`, on the desktop and the laptop.

## Open, for the user

- Is the fallback in 4 acceptable as "decided after measuring", or should the GPU path require the extension and fall back
  to the software renderer for stencil frames without it? My call: decide after measuring.
- **Effort:** step 0 (route + measurement) is a day. Steps 1–5 plus tests are a few days.
