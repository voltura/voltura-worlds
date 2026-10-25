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
};
const toggle = listeners(), cinema = {}, expand = listeners();
const motion = { ...listeners(), matches: false };
const elements = { '#world-video': video, '#play-toggle': toggle, '.cinema': cinema, '#fullscreen': expand };
const document = { ...listeners(), hidden: false,
  querySelector: selector => elements[selector], querySelectorAll: () => [],
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
console.log('PASS: offscreen, tab visibility, motion preference and explicit pause transitions');
