# Product website

`site/` is the standalone public product site, separate from the native app and developer documentation. It is plain HTML, CSS and JavaScript with local imagery and H.264 clips. No build or package installation is needed.

## Preview

```powershell
python -m http.server 8080 --bind 127.0.0.1 --directory site
```

Visit `http://127.0.0.1:8080`. Stop the server with Ctrl+C when finished. Verify the four scene choices, play/pause, fullscreen, narrow layouts, keyboard focus and reduced-motion behavior. Visitors who prefer reduced motion see a still until they choose Play.

## Native media

The four 1600 × 900, eight-second, silent clips are captured directly by the app's native renderer at 30 frames per second. They use ordinary scene settings, without capture-only visual experiments, interpolation or retiming. They show actual native graphics, not the approved concept mockups or proof of physical desktop attachment. Only the selected clip plays; it pauses when hidden or out of view. Posters remain available when video playback is blocked.

To regenerate after building the app, install the development-only Pillow and imageio-ffmpeg packages (the script also supports `.deps/python`) and run:

```powershell
python scripts/capture-site-media.py
```

Intermediate frames and metrics stay in ignored `artifacts/site-media/`. Encoded MP4s and JPEG posters live in `site/assets/`. Artwork credits follow [THIRD-PARTY-NOTICES.md](../THIRD-PARTY-NOTICES.md) and the linked asset notices.

## Publish

Source lives on `main`. The `gh-pages` branch contains only the contents of `site/`. After reviewing and committing the intended site changes:

```powershell
git push origin main
git subtree push --prefix site origin gh-pages
```

GitHub Pages builds from `gh-pages:/`. `site/.nojekyll` keeps the static files unchanged. There is no application binary in the website branch.

## Domain

- Public URL: `https://worlds.voltura.se/`
- `site/CNAME`: `worlds.voltura.se`
- one.com DNS: CNAME host `worlds`, target `voltura.github.io`, default TTL.
- GitHub Pages custom domain: `worlds.voltura.se`; enforce HTTPS after certificate provisioning.

Preserve unrelated DNS records. Following publication, verify the canonical domain, valid HTTPS, page content, CSS, posters and clips. A successful Pages build alone does not prove DNS propagation or certificate readiness.

## Downloads

The two Download for Windows buttons point to the matching installer under `voltura/voltura-worlds/releases/download/v<version>/`. Publish the complete release and its checksum before pushing site links. Update both buttons, the preview version label, README and user guide together. Verify a fresh download against the published checksum after Pages finishes.
