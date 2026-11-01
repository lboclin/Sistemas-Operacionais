'use strict';
const assert = require('node:assert/strict');
const test = require('node:test');
const {decode} = require('../../apps/visual/static/timeline.js');
const ok = [
  {sequence:0,tick:0,type:'arrived',request_id:7,worker_index:null},
  {sequence:1,tick:0,type:'dispatched',request_id:7,worker_index:0},
  {sequence:2,tick:2,type:'preempted',request_id:7,worker_index:0},
  {sequence:3,tick:2,type:'dispatched',request_id:9,worker_index:1},
  {sequence:4,tick:4,type:'completed',request_id:9,worker_index:1}
];
test('intervals come exclusively from matching dispatch/end pairs',() => {
  const {events,intervals,open}=decode(ok);
  assert.equal(events.length,5); assert.equal(intervals.length,2);
  assert.deepEqual(intervals.map(x=>[x.worker,x.from,x.until,x.request]),[[0,0,2,7],[1,2,4,9]]);
  assert.deepEqual(open,[]);
});
test('partial traces have no invented end',() => {
  const x=decode(ok.slice(0,4));assert.equal(x.intervals.length,1);assert.equal(x.open.length,1);
});
test('empty event stream is safe',() => assert.deepEqual(decode([]),{events:[],intervals:[],open:[]}));
test('rejects duplicate or backwards sequence',() => {
  const bad=ok.map(x=>({...x}));bad[2].sequence=1;assert.throws(()=>decode(bad),/inválido/);
});
test('rejects unmatched completion and duplicate dispatch',() => {
  assert.throws(()=>decode([{sequence:0,tick:1,type:'completed',request_id:1,worker_index:0}]),/despacho/);
  assert.throws(()=>decode([ok[1],{sequence:2,tick:1,type:'dispatched',request_id:9,worker_index:0}]),/worker/);
});
test('rejects unsafe tick precision and event vocabulary',() => {
  assert.throws(()=>decode([{sequence:0,tick:Number.MAX_SAFE_INTEGER+1,type:'arrived',request_id:1}]),/inválido/);
  assert.throws(()=>decode([{sequence:0,tick:0,type:'evil',request_id:1}]),/inválido/);
});
