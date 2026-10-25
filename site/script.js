(() => {
  'use strict';
  const scenes = {
    aurora: ['01', 'Aurora', 'Ribbons of light. A quieter kind of energy.'],
    orbit: ['02', 'Orbit', 'Take the long way around.'],
    smoke: ['03', 'Smoke', 'Colour in motion. Never quite the same.'],
    bloom: ['04', 'Bloom', 'Soft folds. Slowly unfolding.'],
    silk: ['05', 'Silk', 'Translucent ribbons. Colour flowing through colour.'],
    eclipse: ['06', 'Eclipse', 'A quiet sun. A slowly shifting corona.'],
    lattice: ['07', 'Lattice', 'An open sculpture. A new angle in every turn.'],
    drift: ['08', 'Drift', 'Small lights. Gentle currents.'],
    contour: ['09', 'Contour', 'Imaginary landscapes. Lines that keep moving.'],
    mercury: ['10', 'Mercury', 'Liquid forms. Reflections in motion.']
  };
  const video = document.querySelector('#world-video');
  const toggle = document.querySelector('#play-toggle');
  const cinema = document.querySelector('.cinema');
  const reducedMotion = matchMedia('(prefers-reduced-motion: reduce)');
  let wantsMotion = !reducedMotion.matches;
  let selected = 'aurora';
  const syncButton = () => {
    const label = video.paused ? 'Play animation' : 'Pause animation';
    toggle.textContent = label + (video.paused ? ' ▷' : ' Ⅱ');
    toggle.setAttribute('aria-label', label);
  };
  const play = () => { if (wantsMotion && !document.hidden) video.play().catch(syncButton); };
  video.addEventListener('play', syncButton);
  video.addEventListener('pause', syncButton);
  video.addEventListener('error', () => {
    toggle.textContent = 'Retry animation ▷';
    toggle.setAttribute('aria-label', 'Retry animation');
  });
  toggle.addEventListener('click', () => {
    wantsMotion = video.paused;
    if (wantsMotion) { if (video.error) video.load(); play(); } else video.pause();
  });
  document.querySelectorAll('[data-world]').forEach(button => {
    button.addEventListener('click', () => {
      const name = button.dataset.world;
      if (name === selected) return;
      selected = name;
      const [number, title, description] = scenes[name];
      document.querySelectorAll('[data-world]').forEach(item => {
        const active = item === button;
        item.classList.toggle('active', active);
        item.setAttribute('aria-pressed', String(active));
      });
      cinema.dataset.scene = name;
      document.querySelector('#scene-number').textContent = `WORLD ${number} / 10`;
      document.querySelector('#scene-title').textContent = title;
      document.querySelector('#scene-description').textContent = description;
      video.pause();
      video.poster = `assets/${name}.jpg`;
      video.setAttribute('aria-label', `${title}: native animated wallpaper`);
      video.querySelector('source').src = `assets/${name}.mp4`;
      video.load();
      play();
    });
  });
  const expand = document.querySelector('#fullscreen');
  if (!cinema.requestFullscreen || !document.fullscreenEnabled) expand.hidden = true;
  expand.addEventListener('click', async () => {
    try { if (document.fullscreenElement) await document.exitFullscreen(); else await cinema.requestFullscreen(); } catch { /* The browser may deny fullscreen. */ }
  });
  document.addEventListener('fullscreenchange', () => {
    expand.textContent = document.fullscreenElement ? 'Close ↙' : 'Expand ↗';
    expand.setAttribute('aria-label', document.fullscreenElement ? 'Exit fullscreen' : 'View animation fullscreen');
  });
  document.addEventListener('visibilitychange', () => { if (document.hidden) video.pause(); else play(); });
  reducedMotion.addEventListener('change', () => { wantsMotion = !reducedMotion.matches; if (wantsMotion) play(); else video.pause(); });
  new IntersectionObserver(entries => {
    if (entries[0].isIntersecting) play(); else video.pause();
  }, { threshold: 0.1 }).observe(cinema);
})();
