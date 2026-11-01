/* Memory measurements: display only what the engine exported. */
(function(root){
'use strict';
const $=id=>document.getElementById(id);
function config(){return $('mem-on').checked?{frames:$('mem-frames').value,page_tokens:$('mem-tokens').value,policy:$('mem-policy').value,fault_latency:$('mem-latency').value}:null;}
function reset(){ $('mem-result').hidden=true; $('mem-empty').hidden=false; }
function show(data){
  const m=data.metrics||{},list=$('mem-log');list.replaceChildren();
  $('mem-empty').hidden=true;$('mem-result').hidden=false;
  for(const [id,key] of [['mem-hits','page_hits'],['mem-faults','page_faults'],['mem-evicts','page_evictions'],['mem-peak','peak_resident_frames']])$(id).textContent=m[key]??'—';
  const events=(Array.isArray(data.events)?data.events:[]).filter(e=>['page_hit','page_fault','page_evicted','blocked','woken'].includes(e.type));
  for(const ev of events.slice(-120)){const li=document.createElement('li');li.textContent='#'+ev.sequence+' tick '+ev.tick+' '+ev.type+' PCB '+ev.request_id+(ev.page_id===null?'':' pág. '+ev.page_id);list.append(li);}
  if(!events.length){const li=document.createElement('li');li.textContent='Sem eventos de paginação/bloqueio.';list.append(li);}
  $('mem-info').textContent=data.config?.memory_enabled?'Métricas e eventos reais; o export não permite determinar cada frame residente em cada tick.':'Memória não ativada nessa execução.';
}
async function runLab(lab,mode){
  const risky=mode==='unsafe'||mode==='deadlock';
  if(risky&&!root.confirm('Demonstração isolada de '+mode+' com watchdog. Confirmar?'))return;
  const s=$('lab-status');s.textContent='Executando laboratório…';
  document.querySelectorAll('[data-lab]').forEach(b=>b.disabled=true);
  try{
    const res=await fetch('/api/lab',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({lab,mode,acknowledge:risky})});
    const result=await res.json();if(!res.ok)throw Error(result.error||'Falha no laboratório');
    s.textContent=result.output+' (código '+result.exit_code+')';
  }catch(e){s.textContent='Erro: '+e.message;}
  finally{document.querySelectorAll('[data-lab]').forEach(b=>b.disabled=false);}
}
root.OSLLMMemory={config,reset,show};
document.addEventListener('DOMContentLoaded',()=>{
  $('mem-on').addEventListener('change',()=>{for(const id of ['mem-frames','mem-tokens','mem-policy','mem-latency'])$(id).disabled=!$('mem-on').checked;if(typeof invalidate==='function')invalidate();});
  for(const id of ['mem-frames','mem-tokens','mem-policy','mem-latency'])$(id).addEventListener('change',()=>{if(typeof invalidate==='function')invalidate();});
  for(const b of document.querySelectorAll('[data-lab]'))b.addEventListener('click',()=>runLab(b.dataset.lab,b.dataset.mode));
});
})(window);
