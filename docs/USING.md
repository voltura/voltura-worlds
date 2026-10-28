# Using Voltura Worlds

The per-user Windows 11 x64 app includes **25 built-in Worlds**: Aurora, Orbit, Smoke, Bloom, Silk, Eclipse, Lattice, Drift, Contour, Plastic metal, Ripples, Mercury, Venus, Earth, Mars, Jupiter, Saturn, Uranus, Neptune, Moon, Pluto, Sun, Forest Water, Tropical Paradise and Tropical Island. C++20, Direct3D 12, HLSL. MIT licensed.

Version **0.5.2** includes Tropical Island, Tropical Paradise, Forest Water and optional desktop right-click menus, alongside HDR output, per-display adaptive performance, video controls and saved desktop presets.

The window title and tray icon tooltip show the running version. When upgrading from 0.1.10 or earlier, run the installer once; those versions cannot automatically upgrade to the current release.

[Download the published installer](https://github.com/voltura/voltura-worlds/releases/download/v0.5.2/VolturaWorlds-Setup-0.5.2-win-x64.exe), or follow the build instructions below. Settings, imports and startup preferences are retained when upgrading.

## Install and try it

Run the downloaded installer, or `dist/VolturaWorlds-Setup-0.5.2-win-x64.exe` after packaging a local build. The main installer runs for your account; the optional compact-menu certificate-trust step can require administrator approval. **Start with Windows** is enabled by default for new installations. Toggle it from the tray icon's right-click menu; setup and updates preserve your choice. The installer opens the library when you choose **Start Voltura Worlds** on the final page.

Choose a display, then a World. In the tray menu, the eleven bodies are grouped under **Solar System**. Changes apply immediately behind the desktop icons. Choose **Off** in the visualization dropdown or tray menu to reveal your normal Windows desktop while Worlds stays running. The choice is saved per display. In the desktop right-click menu, **Off** is under **Voltura Worlds** controls. **Apply to all displays** copies the current selection and settings. If the desktop connection or a renderer fails, Worlds retries automatically and shows **Retry now** for an immediate attempt. Preview mode shows **Use on desktop** instead; the button is hidden during healthy desktop use. A failed video or renderer is retried per display; switching to another World recovers immediately while other displays keep animating.

Choose **Random** to shuffle through all built-in Worlds, showing each once before reshuffling. The interval beside the visualization selector defaults to **Every 5 minutes**, with choices from 30 seconds to 30 minutes. Each display saves its own Random mode and interval. The countdown pauses with playback. Selecting a specific World, Off, or an imported video stops the shuffle.

Choose **Random Solar System** to cycle through the eight planets, Moon, Pluto and Sun. Enable **Show body statistics** for palette-matched facts in the lower-right corner, saved per display. See [statistics and sources](body-statistics.md).

Closing the library leaves the background running. Reopen it through the notification-area icon or the Start-menu shortcut. Use **Pause**, **Resume**, or **Exit Voltura Worlds** to control playback. Exit reveals your ordinary Windows wallpaper; Worlds does not overwrite it.

The library follows your Windows light/dark theme. Opening **Adjustments** minimizes other app windows to reveal the desktop, keeps the Worlds interface in front, and reveals detailed artwork and playback controls. The library becomes translucent so you can see the World while adjusting it. Collapsing the settings leaves other apps minimized; you can restore them from the taskbar. **Restore defaults**, inside Adjustments, restores the selected World’s adjustments and global playback preferences without deleting imports or changing assigned scenes. It uses built-in default values; it does not load or change a saved preset.

Audio reaction defaults to Off. Choose Gentle or Expressive and adjust Audio strength to use local playback-device analysis. No audio is recorded or uploaded. Rendering pauses on battery by default; clear **Pause on battery** to animate while unplugged. Full display coverage, lock, sleep, suspend, and remote sessions also pause rendering.

Personal SDR videos can be imported, copied to the managed library, and looped silently. MP4/M4V, MOV, MKV, WebM, WMV and AVI are offered; actual support depends on the native Windows container/decoder path. H.264 MP4 is the baseline. Every import must produce a transferable GPU frame before confirmation is enabled. HEVC and AV1 require compatible installed decoders; Worlds does not install codecs or transcode files. Data-only `.world` packages open an import preview before applying.

User settings and imported assets live in `%LOCALAPPDATA%\Voltura\Worlds`. Uninstall from Windows Settings; exit Worlds first. Uninstall asks whether to keep your library and preferences.

Enable **Automatic updates** in the notification-area menu for background update downloads. Choose **Restart to update** when available, or **Exit Voltura Worlds** to install a verified download on exit. Updates are off by default. See [UPDATES.md](UPDATES.md).

## Desktop right-click menus

Setup offers **Add Voltura Worlds to desktop right-click menus**, initially unchecked. Both desktop menus offer a short list of visualization shortcuts and **Select more...** to open the full library for that display. Solar System choices are listed directly, using names such as **Sun**, **Mercury** and **Earth**, with no �Solar System:� prefix. The tray menu retains its **Visualizations > Solar System** hierarchy. Visualization choices and checkmarks belong to the display where you opened the desktop menu; direct scene shortcuts do not change the library's display selection, while **Select more...** selects that display in the library. Pause and updates remain global. If Worlds is closed or not responding, the submenu contains only **Open Voltura Worlds**; opening the menu does not start the app.

**Also enable the Windows 11 compact menu** is a separate, optional setup choice. It registers the same three independent menu groups. Windows limits `IExplorerCommand` submenu depth. Neither integration appears on files or ordinary Explorer folder backgrounds.

The app, installer, classic menu and package registration run for your account. With the self-signed build, enabling the compact menu may show a one-time Windows administrator prompt to trust Voltura's public signing certificate in **Local Computer > Trusted People**. The dedicated signed helper trusts only its own signing certificate; it never installs a private key. Declining leaves the classic menu usable. A computer policy can still block package registration.

Setup and automatic updates preserve your choices. Silent updates never prompt for elevation or trust a replacement certificate. Rerun interactive setup to enable the optional trust step after a certificate change. Unsigned local installer builds offer the classic menu only. Disabling integration in setup, or uninstalling, unregisters the desktop menus. Windows may retain loaded shell code until you sign out; setup never forcibly restarts Explorer.

Registration details and partial failures are recorded in `%LOCALAPPDATA%\Voltura\Worlds\desktop-menu.log`. Removing the app leaves certificate trust intact because other Voltura apps can share the signer. To revoke that trust separately, an administrator can open **Manage computer certificates > Trusted People > Certificates**, check the certificate's thumbprint against `shell\menu.json` before uninstalling, and remove that specific Voltura certificate. Do not remove other publishers' certificates.

## Display performance

Adjustments saves **Quality** and **Frame limit** independently for each display. Quality offers Eco, Auto and High. Frame limit offers Auto, 30 FPS and 60 FPS. Auto uses the current Windows display refresh rate, up to 60 FPS; fractional rates such as 59.94 Hz are retained. Explicit limits also respect the display refresh rate.

Auto favors native-resolution detail. Under sustained GPU load it lowers cadence first, down to about 24 FPS (or the display rate if lower), then reduces detail only where the World supports it and Quality is Auto. After sustained headroom returns, it restores detail before cadence. High and Eco keep their chosen detail. With both settings explicit, no automatic performance adjustment is made beyond respecting the refresh-rate ceiling. The status in Adjustments shows the effective cadence target and detail tier; it is not a measured presentation FPS counter.

**Apply to all displays** copies performance settings along with the World. An explicit frame limit is copied only to displays whose current Windows refresh setting supports it; other displays retain their previous frame-limit choice. Auto can be copied everywhere. **Restore defaults**, inside Adjustments, restores Auto/Auto on the selected display. Importing a version-1 single World keeps the destination display's performance choices. Version-2 desktop presets restore their saved performance choices to mapped displays.

New displays default to Auto/Auto. Runtime adjustments are temporary and do not overwrite these preferences. Displays using the same GPU share built-in textures and rendering pipelines while retaining independent motion, simulations and video playback. Display changes re-detect refresh rates and GPU assignments.

## Display output

Adjustments has an **Output** choice for the selected display. **Auto** (the default) uses HDR for built-in Worlds when Windows HDR is already enabled on that display and HDR presentation succeeds. Otherwise it uses SDR. **SDR** always keeps the existing SDR appearance. Worlds never changes Windows HDR or monitor settings.

HDR follows Windows' SDR content brightness setting for ordinary whites and body-statistics cards, with brighter scene highlights limited to the display's reported peak. The performance line shows the effective HDR/SDR mode and any fallback reason. If luminance information is unavailable, conservative brightness estimates are used.

Imported videos continue to play in SDR, including on an HDR desktop. Selecting a built-in World returns to Auto's eligible HDR output. Apply to all copies the preference and resolves each display independently; Restore defaults restores Auto on the selected display. Version-1 single-World imports preserve the destination's Output preference. Version-2 desktop presets save and restore the preference; actual HDR availability is resolved separately for each mapped display.

HDR implementation and diagnostic verification limits are described in [HDR output](HDR.md).

## Video framing and speed

In Adjustments, **Video position** offers Fill or Fit. Fill's **Horizontal position** and **Vertical position** sliders choose which part of a cropped video stays visible: 0% is left/top, 50% is centered, and 100% is right/bottom. An uncropped axis does not move. **Center** resets positioning without changing speed. Fit centers the whole image with black bars and retains the saved crop positions for a later return to Fill.

**Video speed** offers 0.25x, 0.5x, 0.75x, 1x, 1.25x, 1.5x and 2x. Position and accepted speed are saved per display. Changes apply live without reopening the video. Speed changes media time; Frame limit still controls presentation cadence. Pause and looping retain the speed. If Windows rejects a rate, the previous accepted rate is retained and the status explains it. A rate request made while playback is paused takes effect on resume and is saved after acceptance.

Apply to all copies video framing and speed with the selected World, subject to the existing display frame-limit rules. Restore defaults restores Fill, centered positions and 1x. Rotated and nonsquare-pixel videos use their display geometry rather than being cropped as raw encoded frames.

Known PQ/HLG HDR and protected videos are rejected. Videos always use SDR output, preserving the display's Output preference; selecting a built-in World returns to eligible HDR. A copied file is validated again and remains usable after the original is moved. Container names and file extensions alone do not guarantee decoder support. If a file fails, an SDR H.264 MP4 is the most compatible alternative.

Version-1 `.world` packages import a single World and support H.264 MP4 with centered positioning and 1x playback. Version-2 desktop packages preserve framing, speed and every saved display setting.

## Saved desktop presets and export

Open **Presets…** in the library. Enter a name and optional description/attribution, then choose **Save current…** to capture every connected display, including Off, Random mode/pool/interval, artwork adjustments, audio reaction, body statistics, video framing/speed, quality, frame limit and Output preference. The preset also includes the global audio-output device and Pause on battery. Disconnected remembered displays, pause state, animation time and Random countdown progress are excluded. Saving does not change playback.

Select a saved preset to **Apply…**, **Update selected**, **Rename / details**, **Delete**, or **Export selected…**. Update explicitly replaces its configurations with the current desktop; later desktop adjustments never silently edit a preset. Names must be nonblank, unique without regard to case, and at most 120 UTF-8 bytes. Rename / details changes metadata while retaining the saved configurations. Deletion asks for confirmation and leaves the active desktop and installed media intact.

Apply opens a display-mapping review. Recognized monitors are selected automatically; choose a destination or **Skip this display** for other entries. Each destination may be used once. Skipped and unassigned displays retain their current configuration. Audio and battery preferences are restored together with mapped assignments; an unavailable saved audio device falls back to system output. Random countdowns restart. Videos must decode and transfer a GPU frame, and accept the saved speed, on each mapped adapter before anything is applied. A failed validation or cancellation leaves the current desktop unchanged.

**Export current…** exports the live desktop without saving a named preset first. The name/description/attribution fields are used for either export action, followed by a Windows Save As dialog. Version-2 `.world` files contain the configurations, a labelled PNG summary and required video files, with identical media bundled once. Videos are copied without transcoding, contain no original filesystem paths, and still require compatible Windows decoders on the receiving computer. Packages and saved preset archives are limited to 100 MiB and 32 displays. Progress and **Cancel operation** are available during file operations; existing destinations are replaced only after writing succeeds.

Import a version-2 `.world` through **Presets… → Import preset…** or its file association. Review its metadata, name, display mapping and global preferences, then choose **Apply preset** to restore the file without creating or changing a saved preset. **Save and apply…** also saves the imported configuration; if that name already exists, a themed confirmation offers to replace it. Cancel preserves the existing preset, or edit the name to save a separate copy. Cancel/Back does not install or apply it. Presets live as self-contained archives under `%LOCALAPPDATA%\Voltura\Worlds\presets`; moving the original video or exported package does not break a saved preset. See [package and persistence details](PRESETS.md).

## World adjustments

**Aurora** animates luminous artwork with a separate stationary sky. **Orbit** shows a ringed planet with clouds, moons and atmospheric lighting. **Smoke** renders evolving colored fluid. Use palettes, brightness, movement and the artwork controls in Adjustments to tune them.

Bloom is an original animated folded-sheet World inspired by the blue Windows 11 wallpaper. It uses native 3D geometry with slow breathing, curling, and rocking movement, four palettes, and optional audio reaction.

**Mars** shows a complete globe using NASA/USGS Viking imagery and MOLA terrain data. Brightness controls exposure; Movement controls its 15-minute rotation and slow drift; Terrain relief adjusts surface normals; Camera drift adjusts the framing motion; Dust variation changes suspended-dust contrast; Atmosphere adjusts the thin haze. The four Mars palettes preserve natural surface colors. Gentle and Expressive audio modes modulate atmospheric scattering, scaled by Audio strength. Movement at zero freezes the scene and audio response. Sunlight uses a fixed mean distance of 1.524 AU and a fixed orbital composition. See [source data and reproduction](../assets/mars/ATTRIBUTION.md).

**Ripples** shows calm reflective water from above, with scattered drops producing expanding concentric waves that overlap and interfere. Overlaps share one displaced surface: crests reinforce, opposite phases cancel, and reflections and refraction follow the combined shape. Gentle wind produces drifting patches of small, curved ripples between the drops, while clouds move across the reflections and softly shade the water. A sandy, pebbled bottom slopes from clear foreground shallows into deeper tinted water; surface motion refracts its fixed features, and depth gradually reduces their visibility. Density adjusts drop frequency, Flow controls wave and wind speed, Detail controls drop-wave fineness, and Turbulence adjusts the wind disturbance. Movement at zero freezes the water and clouds together. It supports the shared palettes, quality settings, and optional audio reaction. Higher quality refines the surface intersection as well as the small water texture.

**Forest Water** looks down onto a dark woodland pond. Tall trunks, branching trees and irregular green canopy reflect through a gently rippling water surface, with blue sky between the crowns and scattered drifting autumn leaves. A generated photographic woodland environment supplies the tree and foliage reflections; water distortion and floating leaves are rendered in real time, inspired by the video reference rather than playing that clip. Localized breeze patches produce curved ripples with quieter water between them. Floating leaves drift with the slow current and sample the same wave field for bobbing, sideways motion, tilt and rotation; their lighting follows that tilt. Natural, Summer, Autumn and Overcast palettes change the forest colors. Flow adjusts drift speed, Turbulence adjusts water disturbance, Detail adjusts ripple fineness, and Floating leaves adjusts leaf density. Movement at zero freezes all animation; optional audio reaction gently strengthens water disturbance. Quality adjusts small wave detail. The fixed camera fills landscape, ultrawide and portrait displays without stretching the scene.

**Tropical Paradise** frames a sunlit jungle waterfall and clear turquoise pool with photorealistic generated artwork. Native GPU effects animate falling water, ripples spreading from the impact, drifting mist and a gentle breeze through selected foliage. Natural, Warm sunlight, Cool lagoon and Soft overcast palettes are available. Waterfall flow changes the falling-water pace and spray, Water ripples changes surface disturbance, Water detail adjusts fine variation, and Foliage breeze controls leaf movement. Movement at zero freezes animation; optional audio reaction strengthens pool disturbance. The scene fills each display without stretching.

**Tropical Island** looks from a sunlit beach toward a small island. The ocean and sloping sandy beach are rendered as a three-dimensional surface with depth-dependent waves, refraction, reflections, generated shoreline foam and wet sand. The distant scenery and palms use generated artwork; no photographic water or breakers are used in the ocean surface. Wave retreat varies between arrivals and along the beach; foam disperses in patches while damp sand dries gradually across successive waves. The shoreline is an analytic approximation of gentle spilling surf, not a full fluid simulation or an overturning breaker. Natural, Warm sunlight, Cool lagoon and Soft overcast palettes are available. Wave speed controls water pace (zero freezes the water independently), Swell strength controls water displacement and foam, Water detail controls small surface ripples, and Palm breeze controls fronds and cast shadows together. Brightness, palettes and adjustments are saved per display and in desktop presets. Movement at zero freezes all animation; optional audio reaction subtly strengthens the water. The view fills the display without stretching.

**Silk** shows translucent flowing ribbons; **Eclipse**, a glowing corona; **Lattice**, a porous rotating sculpture; **Drift**, bioluminescent currents; **Contour**, living topography; and **Plastic metal**, merging metallic forms. Each supports the existing palettes, brightness, movement, detail, flow, turbulence, density, and optional audio reaction with Audio strength. Quality adjusts rendering detail; Frame limit controls each display’s cadence. Movement at zero freezes procedural motion. Lattice and Plastic metal use bounded mathematical surfaces rather than physical material simulation.

**Moon** uses NASA LROC 2025 color and LOLA terrain for an 8K lunar globe. Its camera circles the tidally locked Moon in 15 minutes, revealing the far side and changing phases under a fixed Sun. Earth appears briefly at its natural scale and distance. Advanced controls adjust Terrain relief, Camera drift, Sun angle and Earth brightness. Audio gently affects exposure; Movement zero freezes the scene and audio. See [Moon sources and rendering limits](../assets/moon/ATTRIBUTION.md).

**Jupiter** shows the observed December 2025 Hubble OPAL cloud deck on its measured oblate shape. The Great Red Spot starts facing the viewer; a complete turn takes 15 minutes at Movement 1. Cloud detail, Camera drift, Cloud drift and Atmosphere adjust image contrast, framing, wind-derived cloud flow with a gently swirling Red Spot, and neutral haze. Methane-band observations guide a high-cloud proxy, rather than fabricated terrain. Movement zero freezes motion/audio. See [Jupiter sources and limitations](../assets/jupiter/ATTRIBUTION.md).

**Saturn** shows a muted gold cloud globe with translucent rings, fine radial bands, the Cassini Division and shadows in both directions. The complete tilted ring system remains inside the frame on portrait and landscape displays. A full rotation takes 15 minutes at Movement 1. Cloud detail, Camera drift, Cloud drift and Atmosphere adjust cloud contrast, bounded framing drift, subtle cloud motion and haze. Its four palettes start with Natural Saturn. Audio gently changes planet/ring illumination while stars remain stationary; Movement zero freezes motion and audio. See [Saturn imagery, ring model and attribution](../assets/saturn/ATTRIBUTION.md).

Earth, Mars, Moon, Jupiter, Saturn, Mercury, Venus, Uranus, Neptune and Pluto use the shared NASA catalog-star background. Stars remain stationary during globe rotation; the Moon’s view follows its orbiting camera. See [shared sky sources](../assets/space/ATTRIBUTION.md).

### Pluto

Pluto starts on its heart region and rotates over 15 minutes. Brightness, Movement, Surface detail, Camera drift, Sun angle, Atmosphere and Audio strength tune the view. Surface detail changes photographic contrast, without simulated terrain relief. Four palettes start with Natural Pluto. Movement zero freezes motion and audio. Select Pluto in the library or tray; per-display selection, Apply to all, Random and World import/export work as for the other planets. See [Pluto sources and coverage limits](../assets/pluto/ATTRIBUTION.md).

### Mercury, Venus, Uranus, Neptune and Sun

- **Mercury:** MESSENGER surface imagery, measured relief and no atmospheric halo. Terrain relief, Camera drift, Sun angle and Surface contrast tune the view. See [Mercury sources](../assets/mercury/ATTRIBUTION.md).
- **Venus:** opaque cream-and-gold clouds, retrograde cloud motion and a warm limb. See [Venus rendering](venus.md).
- **Uranus:** a pale cyan globe with a fixed camera and tilted rotation axis. Cloud detail, Band contrast, Cloud drift and Atmosphere tune its appearance. See [Uranus rendering](uranus.md).
- **Neptune:** cloud bands, white streaks and an artistic dark storm, with four palettes including Natural blue-green. See [Neptune rendering](neptune.md).
- **Sun:** an offline NASA ultraviolet observation with localized plasma motion and false-color palettes. Surface detail, Camera drift, Solar activity and Corona tune the image. See [Sun controls and sources](sun.md).

### Earth

Earth combines NASA geography, independently moving clouds and shadows, ocean glints, night lights and blue atmospheric scattering. A full rotation takes 15 minutes at default Movement. See [Earth sources and rendering](../assets/earth/ATTRIBUTION.md).

## Build

Windows 11 x64, Visual Studio 2026 C++ desktop tools, Windows SDK and PowerShell are required to build. NSIS is needed for packaging. Python and `requirements-dev.txt` are optional for texture processing, diagnostics and media capture. Application dependencies are packaged locally; development tools are not needed by users.

```powershell
./scripts/build.ps1
./scripts/package.ps1 -SkipBuild
```

`package.ps1` optionally accepts `-SigningConfig` pointing to a local JSON configuration with `certificateThumbprint`, `timestampUrl`, and `allowSelfSigned`. It builds the installer; it never executes it. `dist/release-manifest.json` records the final version, size and signing status.

Development uses code review and focused visual verification in the running app, with user feedback guiding changes. Automated tests are not part of this repository. See the [release checklist](RELEASING.md).

Normal launch attaches to the desktop and opens the library. `--wallpaper` starts quietly; `--preview` opens a separate development preview surface.

See the [documentation index](README.md) for rendering, package and maintenance references. Capture-only diagnostic flags are not product quality settings.
