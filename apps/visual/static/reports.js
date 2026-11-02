/* I28: authentic measurements from the engine or imported H24 reports. */
(function(root){
'use strict';
const $=id=>document.getElementById(id);
const METRIC=['average_wait_ticks','average_response_ticks','average_turnaround_ticks','throughput_per_tick','page_faults'];
const LABEL={average_wait_ticks:'Espera média (ticks)',average_response_ticks:'Resposta média (ticks)',average_turnaround_ticks:'Retorno médio (ticks)',throughput_per_tick:'Throughput (requisições/tick)',page_faults:'Page faults'};
function finite(value){return typeof value==='number'&&Number.isFinite(value)&&value>=0;}
function normalize(run){
 if(!run||run.schema!=='os-llm-run'||run.finished!==true||!Array.isArray(run.input)||!run.input.length||
    !run.config||!run.metrics||typeof run.config.policy!=='string')throw Error('Execução inválida');
 for(const key of METRIC)if(!finite(run.metrics[key]))throw Error('Métrica ausente ou inválida: '+key);
 const ids=new Set();
 const input=run.input.map(r=>{
  if(!r||!Number.isSafeInteger(r.id)||r.id<=0||ids.has(r.id)||!Number.isSafeInteger(r.arrival_tick)||
     r.arrival_tick<0||!Number.isSafeInteger(r.total_tokens)||r.total_tokens<=0||
     !Number.isSafeInteger(r.predicted_tokens)||r.predicted_tokens<=0||
     !Number.isSafeInteger(r.priority))throw Error('Input inválido ou inseguro para comparação');
  ids.add(r.id);
  return [r.id,r.arrival_tick,r.total_tokens,r.predicted_tokens,r.priority];
 }).sort((a,b)=>a[0]-b[0]);
 const fields=['worker_count','memory_enabled','frame_capacity','tokens_per_page',
               'page_policy','page_fault_latency_ticks','starvation_threshold_ticks'];
 const comparableConfig=Object.fromEntries(fields.map(k=>[k,run.config[k]??null]));
 // Do not include scheduling policy, RR quantum, or EMA parameters: those are the
 // treatments being compared, not characteristics of the common workload.
 return {name:run.config.policy,metrics:Object.fromEntries(METRIC.map(k=>[k,run.metrics[k]])),
         fingerprint:JSON.stringify({input,comparableConfig})};
}
function parseH24(report){
 if(!report||report.schema!=='os-llm-report/v1'||!Array.isArray(report.scheduling)||!Array.isArray(report.memory))throw Error('Arquivo não é relatório H24');
 const rows=[];
 for(const row of report.scheduling){
   if(!row||typeof row.name!=='string'||!/^[a-z_0-9-]+$/.test(row.name)||!finite(row.average_wait_ticks)||
      !finite(row.throughput_per_tick))throw Error('Métricas de relatório H24 inválidas');
   rows.push({name:row.name,wait:row.average_wait_ticks,throughput:row.throughput_per_tick});
 }
 return {source:report.provenance?.source_claim||'não identificada',rows};
}
const API={normalize,parseH24};
if(typeof module!=='undefined'&&module.exports)module.exports=API;
root.OSLLMReports=API;
if(typeof document==='undefined')return;
const history=[];
let latest=null;
function element(tag,cls,value){const el=document.createElement(tag);if(cls)el.className=cls;if(value!==undefined)el.textContent=String(value);return el;}
function drawBars(target,rows,key){
 target.replaceChildren();
 if(!rows.length){target.append(element('p','caption','Nenhum resultado comparável disponível.'));return;}
 const max=Math.max(1,...rows.map(r=>r[key]));
 for(const row of rows){
   const entry=element('div','report-bar-row');
   entry.append(element('span','report-label',row.name));
   const track=element('div','report-track');
   const bar=element('div','report-fill');
   bar.style.width=Math.max(.5,100*row[key]/max)+'%';
   bar.setAttribute('aria-label',row.name+': '+row[key]);
   track.append(bar);entry.append(track);entry.append(element('strong','',Number(row[key]).toLocaleString('pt-BR',{maximumFractionDigits:4})));
   target.append(entry);
 }
}
function redraw(){
 const message=$('report-status');const table=$('report-runs');table.replaceChildren();
 const compatible=latest?history.filter(r=>r.fingerprint===latest.fingerprint):[];
 for(const row of compatible){
  const tr=element('tr');
  for(const value of [row.name,row.metrics.average_wait_ticks,row.metrics.average_response_ticks,row.metrics.throughput_per_tick,row.metrics.page_faults]){
    tr.append(element('td','',value));
  }
  table.append(tr);
 }
 message.textContent=compatible.length>1?'Comparação de '+compatible.length+' runs com o mesmo conjunto de requisições. Os ticks são virtuais.':
                      'Execute o mesmo cenário com outra política para comparar medições pareadas.';
 const metric=$('report-metric').value;
 drawBars($('report-chart'),compatible.map(r=>({name:r.name,value:r.metrics[metric]})),'value');
}
function onRun(run){
 try{
   const x=normalize(run);latest=x;
   history.push({...x,name:x.name+' #'+history.length});
   redraw();$('report-export').disabled=false;
   API.lastRaw=run; // unmodified JSON object, not a fabricated reconstruction
 }catch(e){$('report-status').textContent='Comparação indisponível: '+e.message;}
}
function exportRun(){
 if(!API.lastRaw)return;
 const blob=new Blob([JSON.stringify(API.lastRaw,null,2)],{type:'application/json'});
 const url=URL.createObjectURL(blob);const a=element('a');a.href=url;a.download='osllm-run.json';a.click();
 setTimeout(()=>URL.revokeObjectURL(url),1000);
}
API.onRun=onRun;
document.addEventListener('DOMContentLoaded',()=>{
 $('report-metric').addEventListener('change',redraw);
 $('report-export').addEventListener('click',exportRun);
 $('report-import-btn').addEventListener('click',()=>$('report-file').click());
 $('report-file').addEventListener('change',async e=>{
   const file=e.target.files?.[0];if(!file)return;
   try{
     if(file.size>2*1024*1024)throw Error('Relatório maior que 2 MiB');
     const parsed=parseH24(JSON.parse(await file.text()));
     $('report-h24').hidden=false;
     $('report-h24-source').textContent='Fonte declarada: '+parsed.source+' (não verificada pelo navegador)';
     drawBars($('report-h24-bars'),parsed.rows.map(r=>({name:r.name,value:r.wait})),'value');
   }catch(err){$('report-h24').hidden=true;$('report-status').textContent='Importação recusada: '+err.message;}
   e.target.value='';
 });
});
})(typeof window==='undefined'?globalThis:window);
