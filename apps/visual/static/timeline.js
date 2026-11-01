/* I26: deterministic visualization of the C engine trace; never mutates simulation data. */
(function (root) {
  'use strict';
  const ENDS = new Set(['preempted', 'blocked', 'completed']);
  const ALLOWED = new Set(['arrived','dispatched','preempted','completed','page_hit','page_fault','page_evicted','blocked','woken']);
  function decode(events) {
    if (!Array.isArray(events)) throw Error('Eventos ausentes ou inválidos');
    const running = new Map(), intervals = [], normalized = [];
    let lastSequence = -1, lastTick = -1;
    for (const ev of events) {
      if (!ev || !Number.isSafeInteger(ev.sequence) || ev.sequence <= lastSequence ||
          !Number.isSafeInteger(ev.tick) || ev.tick < lastTick ||
          !Number.isSafeInteger(ev.request_id) || ev.request_id < 0 ||
          !ALLOWED.has(ev.type)) throw Error('Trace inválido ou fora de ordem');
      const worker = ev.worker_index;
      if (worker !== null && worker !== undefined && (!Number.isSafeInteger(worker) || worker < 0 || worker > 4095))
        throw Error('Índice de worker inválido');
      if (ev.type === 'dispatched') {
        if (worker === null || worker === undefined || running.has(worker))
          throw Error('Dispatch sem worker livre');
        running.set(worker, {worker: worker, request: ev.request_id, from: ev.tick, startSequence: ev.sequence});
      } else if (ENDS.has(ev.type)) {
        const active = running.get(worker);
        if (!active || active.request !== ev.request_id || ev.tick < active.from)
          throw Error('Fim de execução sem despacho correspondente');
        intervals.push({...active, until: ev.tick, endSequence: ev.sequence, cause: ev.type});
        running.delete(worker);
      }
      lastSequence = ev.sequence;
      lastTick = ev.tick;
      normalized.push({sequence: ev.sequence, tick: ev.tick, type: ev.type, request: ev.request_id,
                       worker: worker === undefined ? null : worker});
    }
    return {events: normalized, intervals, open: Array.from(running.values())};
  }
  const api = {decode};
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  root.OSLLMTimeline = api;
  if (typeof document === 'undefined') return;
  let state = null, timer = null;
  const byId = id => document.getElementById(id);
  function clearTimer() { if (timer !== null) { clearInterval(timer); timer = null; } }
  function setStatus(text) { byId('timeline-status').textContent = text; }
  function reset() {
    clearTimer(); state = null;
    const section = byId('timeline-panel');
    if (section) section.hidden = true;
  }
  function node(tag, cls, text) {
    const el = document.createElement(tag);
    if (cls) el.className = cls;
    if (text !== undefined) el.textContent = String(text);
    return el;
  }
  function paint() {
    if (!state) return;
    const {trace, maxTick} = state, until = state.position;
    const shown = trace.events.slice(0, until);
    const visibleBars = trace.intervals.filter(i => shown.length && i.endSequence <= shown[shown.length-1].sequence);
    const chart = byId('timeline-chart'), log = byId('timeline-log');
    chart.replaceChildren(); log.replaceChildren();
    const lanes = [...new Set(visibleBars.map(i => i.worker))].sort((a,b) => a-b);
    if (!lanes.length) chart.append(node('p', 'caption', 'Nenhum intervalo completo neste ponto do replay.'));
    for (const worker of lanes) {
      const lane = node('div', 'timeline-lane');
      lane.append(node('strong','timeline-worker','W' + worker));
      const track = node('div','timeline-track');
      for (const item of visibleBars.filter(i => i.worker === worker)) {
        const bar = node('div','timeline-bar','P' + item.request);
        bar.style.left = (100 * item.from / maxTick) + '%';
        bar.style.width = Math.max(.4, 100 * (item.until - item.from) / maxTick) + '%';
        bar.title = 'Worker ' + worker + ': PCB ' + item.request + ' [' + item.from + ', ' + item.until + ') · ' + item.cause;
        bar.setAttribute('aria-label', bar.title);
        track.append(bar);
      }
      lane.append(track); chart.append(lane);
    }
    for (const ev of shown.slice(-150)) {
      log.append(node('li','timeline-event', '#' + ev.sequence + ' · tick ' + ev.tick + ' · ' + ev.type +
               ' · PCB ' + ev.request + (ev.worker === null ? '' : ' · W' + ev.worker)));
    }
    byId('timeline-position').textContent = until + ' / ' + trace.events.length + ' eventos';
    byId('timeline-scrub').value = String(until);
    setStatus(trace.open.length ? 'Trace parcial: ' + trace.open.length + ' despacho(s) sem encerramento.' : 'Replay visual. Não pausa o motor C.');
  }
  function show(result) {
    reset();
    byId('timeline-panel').hidden = false;
    try {
      const trace = decode(result.events);
      const maxTick = Math.max(1, Number(result.metrics && result.metrics.makespan_ticks) || 0,
                               ...trace.events.map(e => e.tick));
      state = {trace, position: trace.events.length, maxTick};
      byId('timeline-scrub').max = String(trace.events.length);
      paint();
    } catch (error) {
      byId('timeline-chart').replaceChildren();
      byId('timeline-log').replaceChildren();
      setStatus('Gantt indisponível: ' + error.message);
    }
  }
  function seek(n) { if (!state) return; state.position = Math.max(0,Math.min(state.trace.events.length,n)); paint(); }
  document.addEventListener('DOMContentLoaded', () => {
    byId('timeline-start').addEventListener('click', () => {clearTimer();seek(0);});
    byId('timeline-step').addEventListener('click', () => {clearTimer();if (state) seek(state.position+1);});
    byId('timeline-pause').addEventListener('click', clearTimer);
    byId('timeline-play').addEventListener('click', () => {
      if (!state || timer !== null) return;
      if (state.position >= state.trace.events.length) seek(0);
      timer = setInterval(() => {seek(state.position+1);if (state.position >= state.trace.events.length) clearTimer();}, 250);
    });
    byId('timeline-scrub').addEventListener('input', e => {clearTimer();seek(Number(e.target.value));});
  });
  api.show = show; api.reset = reset;
})(typeof window === 'undefined' ? globalThis : window);
