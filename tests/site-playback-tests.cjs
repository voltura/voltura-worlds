// Exercise page visibility and viewport transitions without a browser dependency.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');
const listeners = () => ({
  handlers: {},
  addEventListener(name, handler) { this.handlers[name] = handler; },
  emit(name) { this.handlers[name]?.(); },
  setAttribute() {},
});
const video = { ...listeners(), paused: true, plays: 0,
  play() { this.paused = false; this.plays++; this.emit('play'); return Promise.resolve(); },
  pause() { this.paused = true; this.emit('pause'); },
  source: { src: 'assets/earth.mp4?v=0.1.10' },
  querySelector() { return this.source; },
  load() {},
};
const html = fs.readFileSync(path.join(__dirname, '../site/index.html'), 'utf8');
const worlds = [...html.matchAll(/data-world="([a-z_]+)"/g)].map(match => ({
  ...listeners(), dataset: { world: match[1] }, classList: { toggle() {} },
}));
const toggle = listeners(), cinema = { dataset: { scene: 'earth' } }, expand = listeners();
const motion = { ...listeners(), matches: false };
const elements = { '#world-video': video, '#play-toggle': toggle, '.cinema': cinema, '#fullscreen': expand,
  '#scene-number': {}, '#scene-title': {}, '#scene-description': {} };
const document = { ...listeners(), hidden: false,
  querySelector: selector => elements[selector], querySelectorAll: () => worlds,
};
let intersection;
vm.runInNewContext(fs.readFileSync(path.join(__dirname, '../site/script.js'), 'utf8'), {
  document, matchMedia: () => motion,
  IntersectionObserver: class { constructor(handler) { intersection = handler; } observe() {} },
});
const visible = value => intersection([{ isIntersecting: value }]);
visible(true);
assert.equal(video.plays, 1);
visible(false);
assert.ok(video.paused);
document.hidden = true; document.emit('visibilitychange');
document.hidden = false; document.emit('visibilitychange');
assert.equal(video.plays, 1, 'Returning to a tab must not play an offscreen video');
motion.matches = true; motion.emit('change');
motion.matches = false; motion.emit('change');
assert.equal(video.plays, 1, 'Changing motion preference must not play an offscreen video');
visible(true);
assert.equal(video.plays, 2);
toggle.emit('click');
visible(false); visible(true);
assert.equal(video.plays, 2, 'Explicit pause must survive leaving and returning to the viewport');
assert.equal(worlds.length, 22, 'All 22 native Worlds must be selectable');
for (const button of worlds) {
  button.emit('click');
  assert.equal(video.source.src, `assets/${button.dataset.world}.mp4?v=0.1.10`);
  assert.equal(video.poster, `assets/${button.dataset.world}.jpg?v=0.1.10`);
  assert.match(elements['#scene-number'].textContent, / \/ 22$/);
  assert.equal(video.plays, 2, 'Choosing a scene must preserve explicit pause');
}
assert.equal(elements['#scene-title'].textContent, 'Sun');
assert.match(elements['#scene-description'].textContent, /Plasma/);
console.log('PASS: offscreen, tab visibility, motion preference and explicit pause transitions');
console.log('PASS: 22 scene selections, matching posters/clips, counters and Sun description');
