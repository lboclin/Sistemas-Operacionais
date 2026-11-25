'use strict';
/* Form validation and CSV import of app.js, run in a VM with a minimal DOM stub. */
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const ROOT = path.resolve(__dirname, '..', '..');
const SOURCE = fs.readFileSync(path.join(ROOT, 'apps/visual/static/app.js'), 'utf8');

function load() {
  const element = () => ({value: '', hidden: false, textContent: '', className: '', disabled: false,
    classList: {toggle() {}}, addEventListener() {}, setAttribute() {}, removeAttribute() {},
    append() {}, replaceChildren() {}, click() {}});
  const elements = new Map();
  const byId = (id) => {
    if (!elements.has(id)) elements.set(id, element());
    return elements.get(id);
  };
  byId('policy').value = 'fcfs';
  byId('workers').value = '1';
  byId('quantum').value = '2';
  const context = vm.createContext({
    document: {getElementById: byId, createElement: element, querySelectorAll: () => []},
    window: {location: {hash: ''}, addEventListener() {}}
  });
  vm.runInContext(SOURCE, context);
  return context;
}

test('each limit matches exactly one CSV field', () => {
  const match = /const limits = (\[\[.*\]\]);/.exec(SOURCE);
  assert.ok(match, 'limits table not found');
  assert.equal(vm.runInNewContext(match[1]).length, 4);
});

test('bundled seed42-v2.csv (priority 0) is imported', () => {
  const app = load();
  const text = fs.readFileSync(path.join(ROOT, 'scenarios/seed42-v2.csv'), 'utf8');
  app.importCSV(text);
  const payload = app.validateForm();
  assert.equal(payload.requests.length, 4);
  assert.equal(payload.requests[0].priority, '0');
});

test('signed 32-bit priority bounds are inclusive', () => {
  for (const priority of ['-2147483648', '-1', '0', '1', '2147483647']) {
    const app = load();
    app.importCSV(`1,0,5,${priority}\n`);
    assert.equal(app.validateForm().requests[0].priority, priority);
  }
});

test('priority outside int32 and malformed values are rejected', () => {
  for (const priority of ['-2147483649', '2147483648', '4294967295', '1.5', '-0', '']) {
    const app = load();
    assert.throws(() => app.importCSV(`1,0,5,${priority}\n`), undefined, priority);
  }
});

test('remaining fields keep their own bounds', () => {
  for (const row of ['0,0,5,1', '-1,0,5,1', '4294967296,0,5,1', '1,-1,5,1',
                     '1,18446744073709551615,5,1', '1,0,0,1', '1,0,4294967296,1', '1,0,-5,1']) {
    const app = load();
    assert.throws(() => app.importCSV(row + '\n'), undefined, row);
  }
});
