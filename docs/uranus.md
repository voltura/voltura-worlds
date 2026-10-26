# Uranus sources and rendering

Uranus appears pale blue-green in visible light because methane absorbs red
light. Its rotation axis is tilted 97.77 degrees relative to its orbital normal,
and its actual rotation period is approximately 17 hours.

- [NASA Uranus facts](https://science.nasa.gov/uranus/facts/)
- [ESA Uranus physical parameters](https://sci.esa.int/web/solar-system/-/uranus)

This World is a procedural artistic reconstruction, not a spacecraft texture or
an atmospheric simulation. A slightly oblate opaque cloud deck uses equatorial
and polar radii of 25,559 and 24,973 km, subdued cyan latitude bands, faint cloud
features, polar variation, and a narrow scattering limb. Rings and moons are
omitted. The fixed view is near equinox, with the 97.8-degree axis in the image
plane; it is not an ephemeris for the current date. No axis precession or orbital
camera movement is applied. Sunlight remains fixed in world space.

Rotation is accelerated to one turn per 15 minutes at default Movement. Movement
scales both body rotation and cloud drift; zero freezes animation and audio
response. Cloud detail controls fine structure, Band contrast adjusts latitude
bands, Cloud drift controls differential atmospheric advection, and Atmosphere
adjusts limb haze without exposing a solid surface. Brightness sets exposure.
Natural Uranus is the default palette; Pale aqua, Cool blue, and Neutral survey
are subtle artistic grades. Audio Off, Gentle, and Expressive use the shared
audio source and strength setting to gently brighten clouds and haze, without
changing geometry, the fixed camera, lighting direction, or star positions.

The static background reuses the existing shared NASA Milky Way and catalog-star
texture. No additional Uranus texture allocation is required. See
[shared sky attribution](../assets/space/ATTRIBUTION.md).

Native diagnostic captures support `--scene uranus` and the existing parameter,
audio, and start-time flags. `--uranus-probe` selects ordinary rendering (0),
unlit cloud albedo (1), silhouette (2), or fixed illumination (3). These probes
are capture diagnostics and do not add library settings.
