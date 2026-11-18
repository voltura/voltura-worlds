# Voltura Worlds 0.6.12

- Stream artwork mip levels through a reusable pixel buffer and load graphics pipelines when their output mode or overlay is first used.
- Free intermediate rendering buffers after five seconds of paused playback. Windows keeps the last frame visible while the World holds its place.
- Streamline the shared glow pass and skip video redraws when the decoded frame and framing are unchanged.
- Reduce repeated shading work in Earth, Ripples, Forest Water and Swedish Christmas. Smoke runs the same twelve pressure iterations in a separate shader without transitioning unrelated textures.

These changes preserve the existing resolution, detail and frame-limit controls.

The Intel Arc profile covered all 27 built-in Worlds. The median World used 3.2% less GPU time per frame, and pausing a 4K World released about 71 MiB of rendering buffers. Results and visual comparisons are in the [performance notes](https://github.com/voltura/voltura-worlds/blob/v0.6.12/docs/performance-0.6.12.md).
