# Body statistics

**Show body statistics** is an opt-in preference for each display. It appears
below the visualization selector, is copied by Apply to all displays, and is
turned off by Restore defaults. Switching scenes retains the preference; only
the eight planets, Moon, Pluto and Sun render a card. Version-2 desktop presets preserve this preference; version-1 single-World packages do not include it.

**Random Solar System** shuffles those same 11 bodies. General Random retains
its existing pool. Both modes use the existing active-playback interval and
avoid consecutive repeats, including between shuffles.

## Fact conventions and sources

Facts were checked against NASA on October 26, 2026. The offline catalog is in
`src/body_catalog.h`; it never requests location, network access or ephemerides.

Diameters are rounded volumetric mean diameters (twice the volumetric mean
radius), rather than equatorial diameters. The Sun's diameter is deliberately
rounded to 1.4 million km. Giant-planet sizes describe the conventional pressure
level, not a solid surface.

Earth distances are **approximate orbital envelopes**, not current distances
or predictions of achievable close approaches. Using NASA's perihelion and
aphelion radii, the lower bound is Earth's perihelion minus the inner planet's
aphelion, or the outer body's perihelion minus Earth's aphelion. The upper bound
is the sum of both aphelion radii. Values are rounded for a compact display.
Inclinations, apsidal alignment and orbital timing are omitted. Moon uses its
mean geocentric distance, 384,400 km; Sun uses the mean Earth–Sun distance,
149.6 million km. Earth's card says “Your home planet.”

- [NASA metric planetary fact sheet: orbital radii and distances](https://nssdc.gsfc.nasa.gov/planetary/factsheet/)
- [Mean radii and atmosphere: Mercury](https://nssdc.gsfc.nasa.gov/planetary/factsheet/mercuryfact.html), [Venus](https://nssdc.gsfc.nasa.gov/planetary/factsheet/venusfact.html), [Earth](https://nssdc.gsfc.nasa.gov/planetary/factsheet/earthfact.html), [Moon](https://nssdc.gsfc.nasa.gov/planetary/factsheet/moonfact.html), [Mars](https://nssdc.gsfc.nasa.gov/planetary/factsheet/marsfact.html)
- [Mean radii: Jupiter](https://nssdc.gsfc.nasa.gov/planetary/factsheet/jupiterfact.html), [Saturn](https://nssdc.gsfc.nasa.gov/planetary/factsheet/saturnfact.html), [Uranus](https://nssdc.gsfc.nasa.gov/planetary/factsheet/uranusfact.html), [Neptune](https://nssdc.gsfc.nasa.gov/planetary/factsheet/neptunefact.html), [Pluto](https://nssdc.gsfc.nasa.gov/planetary/factsheet/plutofact.html)
- [Mercury: rocky terrain, size and exosphere](https://science.nasa.gov/mercury/facts/)
- [Venus: heat, rotation and atmosphere](https://science.nasa.gov/venus/venus-facts/)
- [Earth: oceans, life and atmosphere](https://science.nasa.gov/earth/facts/)
- [Moon: synchronous rotation, exosphere and ancient lava plains](https://science.nasa.gov/moon/facts/)
- [Mars: iron oxides, Olympus Mons and atmosphere](https://science.nasa.gov/mars/facts/)
- [Jupiter: size, Great Red Spot and composition](https://science.nasa.gov/jupiter/jupiter-facts/)
- [Saturn: icy rings, density and composition](https://science.nasa.gov/saturn/facts/)
- [Uranus: axial tilt, methane color and atmosphere](https://science.nasa.gov/uranus/facts/)
- [Neptune: position, winds and atmosphere](https://science.nasa.gov/neptune/neptune-facts/)
- [Pluto: classification, Kuiper Belt, heart and atmosphere](https://science.nasa.gov/dwarf-planets/pluto/facts/)
- [Sun: size, fusion, gravity and corona](https://science.nasa.gov/sun/facts/)

## Rendering and diagnostics

The card sits in the lower-right corner, inset from the monitor work area to
clear the taskbar. Auto-hidden taskbars also reserve space; preview windows and
native captures use their client area. Taskbar placement is checked once per
second so moving it or changing its size updates the card.

The card uses a cached GDI+ texture composited after tone mapping in the
wallpaper's D3D12 render target. It receives no mouse input and is included in
native screenshots. Body, palette, output size and work-area insets invalidate the cache. Hiding
the card releases its texture after waiting for outstanding GPU work.

Native capture accepts `--show-body-statistics` for deliberate visual review. Verify card placement and appearance in the running app with the intended display and taskbar layout.
