'use strict';
const test=require('node:test'),assert=require('node:assert/strict');
const {normalize,parseH24}=require('../../apps/visual/static/reports.js');
function fixture(){return {schema:'os-llm-run',finished:true,config:{policy:'FCFS'},
  input:[{id:1,arrival_tick:0,total_tokens:4,predicted_tokens:3,priority:1}],
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

test('predicted tokens are part of the workload fingerprint',()=>{
 const a=fixture(),b=fixture();b.input[0].predicted_tokens=99;
 assert.notEqual(normalize(a).fingerprint,normalize(b).fingerprint);
});
test('memory and worker config changes invalidate comparability',()=>{
 const a=fixture(),b=fixture();
 a.config.worker_count=1;b.config.worker_count=2;
 assert.notEqual(normalize(a).fingerprint,normalize(b).fingerprint);
 b.config.worker_count=1;
 a.config.memory_enabled=false;b.config.memory_enabled=true;
 assert.notEqual(normalize(a).fingerprint,normalize(b).fingerprint);
 b.config.memory_enabled=false;
 a.config.frame_capacity=2;b.config.frame_capacity=4;
 assert.notEqual(normalize(a).fingerprint,normalize(b).fingerprint);
});
test('algorithm and round-robin quantum are comparison dimensions',()=>{
 const a=fixture(),b=fixture();
 a.config.worker_count=1;b.config.worker_count=1;
 b.config.policy='Round-Robin';b.config.quantum_tokens=3;
 assert.equal(normalize(a).fingerprint,normalize(b).fingerprint);
});
test('invalid and missing prediction values are rejected',()=>{
 const x=fixture();delete x.input[0].predicted_tokens;
 assert.throws(()=>normalize(x),/Input inválido/);
 x.input[0].predicted_tokens=0;
 assert.throws(()=>normalize(x),/Input inválido/);
});
