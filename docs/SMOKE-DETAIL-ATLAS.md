# Selective fine-density atlas — experimental foundation

`src/smoke_detail_cache.h` supplies CPU residency bookkeeping, exercised by
`smoke_detail_cache_contracts`. The renderer now uses it for a fixed capture-only
refinement region. No accepted visual or GPU performance improvement is claimed.

The proposed atlas keeps coarse simulation as the complete fallback and assigns
fine detail to selected regions. An 8-cell interior plus one border voxel on
each side gives 10-cubed tiles. Three RGBA16F fields (two density buffers and
lighting) fit 4096 tiles in 160-cubed textures: 98,304,000 bytes, below a 96 MiB
field budget. This excludes resource alignment, metadata, coarse fields, render
targets, upload/readback and all other application allocations. It is not proof
of meeting the aggregate 256 MiB target.

Residency contract:

- Single owner on the render thread; no implicit synchronization.
- A resident virtual page keeps its slot until explicitly released.
- Exhaustion returns no allocation. The renderer must use coarse density.
- Removal must be published before release; no new GPU uses may be submitted.
- Released storage cannot be recycled before the supplied last-use fence.
- A new generation invalidates old handles, even if the slot is reused.
- Callers must fade detail before eviction and initialize newly allocated tiles
  before publishing them. The cache itself does neither and does not prevent
  visible popping by itself.

Acquisition uses a reserved free list and does not allocate per request.
Collection scans the bounded slot array. The test covers full capacity, repeat
collection, pinned regions, stale handles and 10,000 retirement/reuse cycles.

Remaining integration: GPU fields and page table, border sampling against coarse
and fine neighbors, detail initialization and transport, priority selection,
fade transitions, generation-aware publication, native visual capture, measured
memory/timing and motion validation. None of these may be inferred from the CPU
test passing. The atlas is intended to retain real fine density; filling it with
upsampled coarse density alone cannot recover the missing artwork.

## Sampling implementation

`shaders/smoke_atlas.hlsli` now implements a manual trilinear residual sampler.
Each of eight fine-grid corners resolves its own virtual page and blends the
fine-minus-coarse correction. The exact coarse sample is added once at the
requested position, so an absent atlas leaves the coarse field unchanged.
Independent corner lookup avoids interpolation into unrelated physical slots;
per-page blend weights support gradual activation and retirement. Borders are
reserved for a future optimized path and currently unused by this sampler.

`smoke_atlas_probe.cso` compiles for Shader Model 6.0 and has now executed in
`smoke_atlas_gpu_contracts`. The test uploads an analytic coarse field, a known
fine correction, reversed physical slot assignments and sentinel-filled
borders. It samples 4,922 positions in four residency/blend states, including
both sides of the internal tile boundaries. All samples match an independent
analytic oracle within 0.0005 absolute channel error. Empty and fully faded
atlases produce byte-identical values to a direct GPU coarse sample. D3D12 debug
validation passes. Residency and GPU tests both passed in the Release build.

The initial test uses RGBA32F textures to isolate addressing/interpolation. Actual
GPU atlas allocation/retirement, native artwork,
performance and integration with the scene shader remain unverified. The probe
does not establish natural motion or visual fidelity.
Density positivity and pigment consistency must also be enforced after residual
reconstruction before this can be used by the fluid renderer.

## Fine-density transport

`smoke_atlas_advect.hlsl` now initializes or advances destination tile interiors
using a midpoint backtrace through a supplied velocity texture. The previous
atlas and its page table are read-only; a separate destination atlas is written
using an inverse slot-to-virtual-page table. Unused slots are skipped. Outside
the bounded world, transported density is zero. Pigments are clipped to
nonnegative values and total density is reconstructed from their sum.

The native GPU test now also verifies 12,288 cells across one-cell translation,
coarse initialization, and zero-timestep transport. Translation crosses virtual
tile boundaries while physical slots are reversed. Each output is compared with
the independently defined source field; pigment consistency is checked. All
checks and D3D12 debug validation pass. An initial test harness descriptor-copy
error was corrected by creating views directly rather than reading descriptors
from a shader-visible heap.

This is a first-order density transport baseline with midpoint trajectory
integration. Repeated resampling may still diffuse detail. It is not a claim of
long-term detail retention, fluid quality, or application integration. Tests use
a constant velocity fixture; actual Smoke velocity, birth/death fades and
release-build timing remain to be exercised.

## RGBA16F validation

`smoke_atlas_half_contracts` repeats the native sampling and transport checks with
RGBA16F coarse, fine, velocity and destination textures. The first run exposed
a transport error just over 0.001 in the density channel. Explicit nearest-even
rounding before typed UAV storage fixed that case without increasing the test
tolerance. The existing fluid `storeHalf` helper is now shared through
`smoke_half.hlsli`; its arithmetic is unchanged. The atlas transport constant
`halfStorage` enables it for RGBA16F destinations only.

Both precision variants now pass. The FP16 sampler's largest measured absolute
channel error against the unquantized fixture is 0.000438213; the FP32 error is
0.000000476837. Empty/faded fine storage remains bit-identical to direct GPU
coarse lookup in both formats. Transport checks allow 0.001 absolute error for
the bounded FP16 fixture, including independently rounded pigment/total fields.
This is not a general error bound for arbitrary values or long-running flow.
The existing native pigment regression also passes after extracting the shared
rounding helper. These checks still do not establish visual fidelity.

## Actual Smoke velocity input

The GPU contract executable accepts `--half --velocity <RGBA16F-file>` after
its two shader paths. It validates a finite cubic field, uploads it, and checks
one 1/60-second transport step against CPU midpoint backtracing and the known
fine-density fixture. The uploaded field from
`artifacts/revision-119/control/velocity-rgba16f.bin` passed, along with the
existing constant-velocity FP32/FP16 contracts.

Integration inspection found that Smoke's velocity channels are stored on
different negative cell faces. `staggeredVelocityGrid` now selects matching
per-component reconstruction in the transport shader; zero retains collocated
sampling for the constant fixture. Sampling all three captured channels at one
coordinate would offset the physical flow and is not used for captured Smoke.

This is a frozen native velocity snapshot driving a test density field. It is
not yet per-frame application integration, live fine-fluid evidence, or an
artwork capture. The next renderer work must allocate the atlas, bind current
coarse velocity/density safely, and expose the reconstructed fine field to the
ray marcher and lighting without changing normal wallpaper mode prematurely.

## Live capture integration (revision126)

`--fine-atlas` enables per-frame fine-density transport and ray-march sampling
for ordinary live cubic Smoke. It rejects reference volumes, analytic transport,
wide density, flow-map experiments and detail-coordinate experiments. Normal
wallpaper mode remains unchanged. This switch is a development diagnostic.

The prototype has 512 fixed resident tiles in the central [0.375,0.625] cubed
region, at 256-cubed virtual resolution. Two 80-cubed RGBA16F atlases alternate;
a coarse-history texture supplies the previous frame outside resident tiles.
The existing coarse solver provides the current staggered velocity. Resource
allocation sizes are included in the renderer's allocation estimate. Tables
remain immutable; release on grid/scene changes follows the renderer's idle wait.

The ray marcher reads refined density in this region and coarse density
elsewhere. Lighting remains coarse. Empty-space skipping is disabled around
the refinement region with a full coarse-brick margin to prevent jumps across
fine boundaries. Graphics descriptors 376..379 hold atlas/page pairs, 380..381
hold null fallback views; compute tables occupy 352..375. Heap size is 384.

First 720-step native 1080p capture on Arc completed with debug validation:
`artifacts/revision-126/refined720/ink.png`. GPU p95 was 14.404ms, private bytes
271917056, estimated GPU bytes 128873472. This precedes the expanded skip margin
and is not a performance acceptance claim. The image visibly shows the refined
box boundary and remains soft. No visual gate passed. The integration is kept
only as an experimental path for further work, not enabled for users.

Known remaining issues include matching coarse decay/source evolution, gradual
boundary blending, fine lighting, higher-order transport/detail retention,
adaptive region selection and sustained motion/resource validation. There is no
new fine-detail source yet: this starts with coarse density and transports it at
finer resolution. It does not by itself generate the reference's intricate curls.

The final expanded-margin build also completed 720 native steps with debug
validation (`artifacts/revision-126/final-margin`): GPU p95 14.825ms, private
bytes 268783616, estimated GPU bytes 128873472. FP32/FP16 atlas contracts and
the existing empty-space regression all passed (12.50 seconds combined).

## Boundary and decay correction (revision127)

Fine-density transport now applies the main solver's exponential decay rate
of 0.22 per second after advection. Initialization does not decay the already
current coarse input. The transport constant block is now 12 DWORDs, including
an explicit decay rate. Both FP32 and FP16 native contracts include an additional
decayed transport case: 16,384 checked cells per variant. Both pass.

The ray marcher blends the fine-minus-coarse correction through a smooth spatial
window, fading over the outer 0.0625 world units on each side of the fixed region.
This makes the correction vanish smoothly at the region boundary. It is a
static-region boundary treatment, not an adaptive residency fade policy.

The 720-step 1080p debug capture in `artifacts/revision-127/blended` passes and
visually reduces the hard rectangular transition seen in revision126. GPU p95
14.978ms, private bytes 278319104, estimated GPU bytes 128873472. These remain
offscreen measurements and miss the 8ms target. The image is still soft and
lacks the approved fine folds; no visual gate accepted. Fine lighting and
fine-scale structure generation are still absent.

## Corrected transport prototype (revision129)

The capture path enables a bounded forward/reverse density correction. To avoid
another atlas allocation in this first implementation, the reverse estimate
recomputes eight forward predictions in the shader. Corrections outside the
departure neighborhood's component-wise extrema fall back to the forward
result. Correction is also skipped when reverse samples leave the domain or
resident, fully blended pages. Decay occurs after correction.

Both native precision contracts pass with integer-cell corrected transport and
a half-cell sinusoidal translation comparison. In the interior fixture region,
squared RGB error decreases from 0.000443436 to 0.0000487897 in FP32, and from
0.000478022 to 0.0000874351 in FP16. The regression requires at least a 20%
decrease, so simply disabling correction cannot pass. This is a one-step
analytic fixture, not proof of long-running retention or smoke fidelity.

The 720-step native capture at `artifacts/revision-129/corrected` completes with
D3D12 debug validation. Central detail changes modestly, but the artwork still
does not approach the reference. GPU p95 18.297ms, private bytes 286601216,
estimated GPU bytes 128873472. Recomputing predictions is costly; a stored
forward pass may be more efficient, but that is not measured. Normal wallpaper
still does not enable the atlas. The shader flag is `correctedTransport` in
DWORD 9 of the 12-DWORD transport constants; native tests leave it off except
for corrected cases. No visual gate accepted.

## Expanded capture region and sources (revision135)

`--fine-atlas --fine-atlas-front` selects an experimental fixed region spanning
x [0.25,0.75], y [0.125,0.875], z [0.25,0.5625]. It uses 3840 resident tiles
in a capacity of 4096, with two 160-cubed RGBA16F textures. Detail blends to
coarse density across one tile at the boundary. This is not adaptive selection
or a normal wallpaper quality option. The original central region remains the
default capture experiment.

Expanding the region over the emitters exposed missing fine-field source
injection. Fine transport now uses the same `smokeEmitter` helper as the coarse
solver, after decay. Initialization samples the already initialized coarse
density and does not inject twice. Pigment sums and the density cap are retained.
The transport root constants now occupy 16 DWORDs: grid, interior, atlas axis,
slot capacity, dt, initialize, half storage, velocity grid, decay, correction,
time, flow, audio, inject sources, and two reserved values. The correction flag
remains DWORD 9. FP32 and FP16 native contracts check all pigments receive new
mass, far cells remain unchanged, zero flow disables emission, and pigment
consistency is preserved alongside the existing transport checks.

The enlarged region has substantially higher GPU cost. It remains a research
capture mode, not accepted art or evidence of meeting performance targets.

## Cached prediction pass (revision136)

The native capture path now uses `smoke_atlas_cached.hlsl`, which shares the
transport implementation with the original single-pass shader. A first pass
writes unmodified forward predictions into an FP32 atlas. A transition from UAV
to shader-resource state precedes the correction pass, which reads those values
instead of tracing and reconstructing every reverse-neighbor prediction again.
Decay and emitter injection occur only in the correction pass. Initialization
bypasses the prediction pass. DWORD 14 selects forward versus correction mode.

FP32 storage avoids introducing another half-precision rounding step. The
native 120-step and 720-step comparison PNGs are byte-identical to revision135.
Both original and cached transport variants pass FP32/FP16 analytic GPU tests,
including correction, decay, sources and pigment consistency. These are specific
equivalence checks, not proof of all possible scenes or long-run stability.

For the expanded 1080p capture, observed GPU p95 changed from 57.655 to 44.412 ms.
Estimated GPU allocation increased from 184,579,072 to 250,180,608 bytes; private
memory was 398,151,680 bytes. This trades storage for repeated computation and
still misses the product timing/private-memory targets. It does not change the
artwork or make the experimental atlas a production-ready feature.

## Fine curl experiment (revision137)

`--fine-curl 0.18` optionally adds spatially windowed curl motion to fine-density
transport. It requires `--fine-atlas`, accepts 0 through 0.3, and defaults to zero.
The analytic field combines twelve directional modes at 18 and 36 cycles per
world unit. It includes both the windowed curl and the cross product of the
window gradient with the vector potential. Strength follows the supplied flow
and turbulence controls; the phase evolves slowly with simulation time.

This is an artistic small-scale velocity experiment, not a measured turbulence
energy model. It does not change coarse pressure/velocity or their lighting.
Stronger motion breaks up some broad sheets, but lacks matching fine shadows
and does not meet the visual reference. It is not enabled in normal wallpaper.
DWORD 15 carries strength in the experimental shader. A separate compiled
variant keeps the zero-strength path identical to the previous renderer.

## Fine-field lighting (revision138)

`--fine-atlas-lighting` optionally computes gradients and shadow integration
from the current blended fine density. It requires `--fine-atlas` and defaults
off. After transport correction completes, the current coarse field is copied
to the history/fallback texture. The completed fine field and fallback then
feed `smoke_atlas_light.hlsl`. Its constant layout matches the transport root;
DWORD 8 carries density scale instead of decay for this separate pipeline.

The now-unused FP32 prediction atlas stores RGB lighting. It transitions from
compute read to UAV write, then to pixel-shader read for drawing, and back to
compute read after drawing. The next frame overwrites it with predictions.
No extra volume allocation is introduced. Eight-corner page-aware sampling
and the existing boundary fade blend lighting into the coarse fallback.

Outside the refinement region, lighting still uses coarse-density shadows;
this prototype does not propagate every fine occluder's influence throughout
the coarse volume. At 1080p/720 steps with front region and curl 0.18, it reveals
more fine texture but does not match the reference, and GPU p95 is 84.372ms.
It is not accepted visual quality or production performance. Native captures
pass debug validation. Disabling the option preserves the previous baseline
PNG; enabled empty-space skipping matches the no-skip capture byte for byte.
