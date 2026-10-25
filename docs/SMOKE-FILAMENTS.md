# Smoke gold tracer capture prototype

Use `--smoke-filaments` with an ordinary live Smoke capture. This compatibility
option now draws persistent gold tracers, replacing the earlier instantaneous
streamline experiment. It remains off by default and is not in library/presets.

4096 particles hold current position/age and previous position/lifetime in a
192KiB GPU buffer. A compute shader integrates the staggered fluid velocity
using midpoint stepping. Particles spawn near the gold source, have bounded
lifetimes, and respawn outside the domain. Spawn and lifetime are deterministic.
A subset of128particles also stores32history knots at15Hz (about two seconds).
History resets on respawn and uses bounded cubic interpolation for curved trails.
Short point streaks remain for all4096particles. Thin trail cores use pixel
footprint coverage inside wider geometry to avoid subpixel raster holes.
Density along the camera ray attenuates each streak; local gold concentration
controls visibility. Fade-in/out avoids visible respawn pops.

The fixed buffer is allocated only when this option is used and included in
GPU allocation accounting. It is reused across scene switches; particle state
resets on return to Smoke. No particle dispatch occurs when dt is zero after
initialization. Rendering pause at the application level still stops rendering.
The added layer does not light the surrounding smoke or replace its simulation.

Reference volumes, transport diagnostics, coordinate experiments, live flow
maps and fine atlases remain incompatible with this option. Their representation
would require matching tracing and occlusion.

This is experimental artwork, not an accepted visual checkpoint. Gold flecks
are a small part of the reference; rolling plumes and fine volume wisps remain
unresolved. Native GPU validation is not physical-desktop proof.
