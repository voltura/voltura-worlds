# Earth World — sources and rendering

Earth is available in the library and the tray’s Solar System submenu, and participates in Random and Random Solar System. Its stable settings/package identity is `earth`.

## Sources and reproduction

See [Earth attribution and rendering model](../assets/earth/ATTRIBUTION.md) for
NASA source records, Natural Earth water-mask data, physical references,
conversion and approximation limits. Exact source and runtime hashes are
recorded beside that document. Public-domain Natural Earth ocean/lake geometry supplies the water mask; NASA imagery supplies the surface, clouds, night lights and shared sky.

Run `powershell -File scripts/build-earth-textures.ps1` to regenerate the two
8K runtime DDS files. The 8K BC4 cloud map and star map are shared with Orbit.
The source download/geometry/PNG cache is development-only and is not shipped.
The DDS reader accepts complete-mip BC5 two-channel textures.
