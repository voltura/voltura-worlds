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
    plastic_metal: ['10', 'Plastic metal', 'Liquid forms. Reflections in motion.'],
    ripples: ['11', 'Ripples', 'Drops meet. Clouds drift. A quieter moment by the water.'],
    mercury: ['12', 'Mercury', 'Sunlit craters. A world of extremes.'],
    venus: ['13', 'Venus', 'Golden clouds. A hidden world.'],
    earth: ['14', 'Earth', 'Blue oceans. Clouds in motion. Home.'],
    mars: ['15', 'Mars', 'Red deserts. A different horizon.'],
    jupiter: ['16', 'Jupiter', 'Cloud belts. A storm that keeps turning.'],
    saturn: ['17', 'Saturn', 'Golden light. Rings full of detail.'],
    uranus: ['18', 'Uranus', 'Pale cyan. A world on its side.'],
    neptune: ['19', 'Neptune', 'Blue winds. White clouds. A distant storm.'],
    moon: ['20', 'Moon', 'Cratered light. A tour of the near and far sides.'],
    pluto: ['21', 'Pluto', 'A bright heart. At the edge of the familiar.'],
    sun: ['22', 'Sun', 'Ultraviolet light. Plasma in motion.']
  };
  const video = document.querySelector('#world-video');
  const toggle = document.querySelector('#play-toggle');
  const cinema = document.querySelector('.cinema');
  const reducedMotion = matchMedia('(prefers-reduced-motion: reduce)');
  let wantsMotion = !reducedMotion.matches;
  let inView = false;
  let selected = 'earth';
  const syncButton = () => {
    const label = video.paused ? 'Play animation' : 'Pause animation';
    toggle.textContent = label + (video.paused ? ' ▷' : ' Ⅱ');
    toggle.setAttribute('aria-label', label);
  };
  const play = () => { if (wantsMotion && inView && !document.hidden) video.play().catch(syncButton); };
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
      document.querySelector('#scene-number').textContent = `WORLD ${number} / ${Object.keys(scenes).length}`;
      document.querySelector('#scene-title').textContent = title;
      document.querySelector('#scene-description').textContent = description;
      video.pause();
      video.poster = `assets/${name}.jpg?v=0.1.10`;
      video.setAttribute('aria-label', `${title}: native animated wallpaper`);
      video.querySelector('source').src = `assets/${name}.mp4?v=0.1.10`;
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
    inView = entries[0].isIntersecting;
    if (inView) play(); else video.pause();
  }, { threshold: 0.1 }).observe(cinema);
})();
