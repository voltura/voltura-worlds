# Voltura Worlds 0.6.2

Shared rendering and presentation improvements across the complete visualization library, including personal video wallpapers:

- Desktop output prefers a native Direct3D 12 composition swap chain, eliminating the D3D11 translation device and the full-frame presentation copy.
- Drivers that cannot create the native path fall back to direct rendering into a composition surface. That path removes the two intermediate desktop frame textures. The established copy path remains available when direct surface access is unsupported.
- SDR/HDR and Orbit/non-Orbit post-processing shaders are specialized at compilation, retaining the existing color and exposure math. Aligned bloom downsampling loads exact source texels without unnecessary sampler interpolation.
- GPU timing readback stays mapped and is read only after the corresponding frame fence completes, removing per-frame map/unmap calls.
- Video skips a redundant clear once its opaque full-screen draw is available.
- Journey retains visible bodies and preloads the next arrival instead of keeping every body's textures resident for the full loop. Assets retain their existing resolution and mip levels.

See [full-set measurements](performance-0.6.2.md) for all 26 built-in Worlds and video. Results distinguish native rendering, composition presentation, GPU allocations and process memory. The measured private-memory reduction was 28–58% across all 27 workloads. The 20% rendering-throughput target was reached in 5/27 workloads and was not achieved across the entire set. Short-profile timing variation and the limits of hidden-window measurements are documented.

Resolution, frame caps and detail settings were not reduced to obtain these savings. Settings, presets, imported videos, signature checks and updater asset-digest validation remain in place. Visual assessment and physical HDR behavior remain for the user.

The app, shell components and installer are signed and timestamped with Voltura AB's existing self-signed certificate.
