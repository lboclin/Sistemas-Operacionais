'use strict';
const test=require('node:test'),assert=require('node:assert/strict');
const {normalize,parseH24}=require('../../apps/visual/static/reports.js');
function fixture(){return {schema:'os-llm-run',finished:true,config:{policy:'FCFS'},
  input:[{id:1,arrival_tick:0,total_tokens:4,priority:1}],
  metrics:{average_wait_ticks:1,average_response_ticks:1,average_turnaround_ticks:5,
  throughput_per_tick:.2,page_faults:0}}}
test('normalizes metrics without inventing numbers',()=>{
 const r=normalize(fixture());assert.equal(r.metrics.average_wait_ticks,1);assert.equal(r.name,'FCFS');
});
test('fingerprints same workload across policies',()=>{
 const a=fixture(),b=fixture();b.config.policy='SJF-EMA';
 assert.equal(normalize(a).fingerprint,normalize(b).fingerprint);
 b.input[0].total_tokens=999;assert.notEqual(normalize(a).fingerprint,normalize(b).fingerprint);
});
test('invalid raw reports fail closed',()=>{
 const r=fixture();r.metrics.average_wait_ticks=Infinity;
 assert.throws(()=>normalize(r),/inválida/);
 r.metrics.average_wait_ticks=1;r.input.push({...r.input[0]});
 assert.throws(()=>normalize(r),/Input inválido/);
});
test('H24 report contract preserves provenance and measured numbers',()=>{
 const data={schema:'os-llm-report/v1',provenance:{source_claim:'synthetic_fixture'},
  scheduling:[{name:'fcfs',average_wait_ticks:4,throughput_per_tick:.2}],memory:[]};
 const parsed=parseH24(data);assert.equal(parsed.rows[0].wait,4);assert.equal(parsed.source,'synthetic_fixture');
 data.scheduling[0].throughput_per_tick=NaN;
 assert.throws(()=>parseH24(data),/inválidas/);
});
test('no fake data when H24 has no runs',()=>{
 const out=parseH24({schema:'os-llm-report/v1',scheduling:[],memory:[]});assert.deepEqual(out.rows,[]);
});
