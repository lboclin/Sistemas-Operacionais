'use strict';
const $ = (id) => document.getElementById(id);
const fields = ['id', 'arrival_tick', 'total_tokens', 'priority'];
const labels = ['ID', 'Chegada', 'Tokens de entrada', 'Prioridade'];
const demo = [
  {id:'1',arrival_tick:'0',total_tokens:'6',priority:'1'},
  {id:'2',arrival_tick:'0',total_tokens:'2',priority:'3'},
  {id:'3',arrival_tick:'2',total_tokens:'3',priority:'2'}
];
let requests = demo.map((r) => ({...r}));
let busy = false;
let scenarioRevision = 0;
function node(tag, className, value) {
  const result = document.createElement(tag);
  if (className) result.className = className;
  if (value !== undefined) result.textContent = String(value);
  return result;
}
function notice(text, kind = '') {
  $('notice').className = kind ? 'notice ' + kind : 'notice';
  $('notice').textContent = text;
}
function invalidate() {
  scenarioRevision += 1;
  for (const key of ['m-completed','m-wait','m-throughput','m-duration','m-utilization','m-count']) $(key).textContent = '—';
  $('results').hidden = true;
  $('empty-state').hidden = false;
  $('run-state').textContent = 'AGUARDANDO';
  $('run-state').className = 'pill';
  if (window.OSLLMTimeline && window.OSLLMTimeline.reset) window.OSLLMTimeline.reset();
  if (window.OSLLMMemory) window.OSLLMMemory.reset();
}
function renderRows() {
  const body = $('requests');
  body.replaceChildren();
  requests.forEach((request, index) => {
    const tr = document.createElement('tr');
    fields.forEach((key, i) => {
      const td = document.createElement('td');
      const input = document.createElement('input');
      input.type = 'text';
      input.inputMode = 'numeric';
      input.autocomplete = 'off';
      input.value = request[key];
      input.setAttribute('aria-label', `${labels[i]} da requisição ${index + 1}`);
      input.addEventListener('input', () => {
        request[key] = input.value.trim();
        invalidate();
      });
      td.append(input);
      tr.append(td);
    });
    const td = document.createElement('td');
    const remove = node('button','remove','×');
    remove.type = 'button';
    remove.setAttribute('aria-label', `Remover requisição ${index + 1}`);
    remove.addEventListener('click', () => {requests.splice(index, 1); renderRows(); invalidate();});
    td.append(remove);
    tr.append(td);
    body.append(tr);
  });
  $('entry-count').textContent = `${requests.length} / 256 requisições`;
  $('add-btn').disabled = requests.length >= 256 || busy;
}
function validateForm() {
  if (requests.length < 1 || requests.length > 256) throw Error('Adicione de 1 a 256 requisições.');
  const seen = new Set();
  const limits = [[1n,4294967295n],[0n,18446744073709551614n],[1n,4294967295n],[1n,4294967295n],[-2147483648n,2147483647n]];
  for (let index = 0; index < requests.length; index++) {
    for (let pos = 0; pos < fields.length; pos++) {
      const key = fields[pos], value = requests[index][key];
      if (!/^(0|[1-9][0-9]*)$/.test(value) && !(key === 'priority' && /^(0|-[1-9][0-9]*|[1-9][0-9]*)$/.test(value))) throw Error(`Linha ${index+1}: ${labels[pos]} inválido.`);
      const number = BigInt(value);
      if (number < limits[pos][0] || number > limits[pos][1]) throw Error(`Linha ${index+1}: ${labels[pos]} fora do intervalo.`);
    }
    if (seen.has(requests[index].id)) throw Error(`ID duplicado: ${requests[index].id}`);
    seen.add(requests[index].id);
  }
  const workers = $('workers').value, quantum = $('quantum').value;
  for (const [name, str, min, max] of [['Workers',workers,1,16],['Quantum',quantum,1,4294967295]]) {
    if (!/^[1-9][0-9]*$/.test(str) || !Number.isSafeInteger(Number(str)) || Number(str)<min || Number(str)>max) throw Error(`${name}: valor inválido.`);
  }
  const memory = window.OSLLMMemory ? window.OSLLMMemory.config() : null;
  let latency = 0n;
  if (memory) {
    for (const [name, value, min, max] of [
      ['Frames', memory.frames, 1n, 4096n],
      ['Tokens por página', memory.page_tokens, 1n, 4294967295n],
      ['Latência de falta', memory.fault_latency, 0n, 18446744073709551614n]]) {
      if (!/^(0|[1-9][0-9]*)$/.test(value) || BigInt(value) < min || BigInt(value) > max)
        throw Error(name + ': valor inválido.');
    }
    latency = BigInt(memory.fault_latency);
  }
  // JSON.parse in browsers converts integers into IEEE-754 doubles. The C CLI
  // handles full uint64_t, but the UI must not show rounded ticks as exact.
  const maxArrival = requests.reduce((max, r) => {
    const tick = BigInt(r.arrival_tick);
    return tick > max ? tick : max;
  }, 0n);
  const tokens = requests.reduce((sum, r) => sum + BigInt(r.total_tokens), 0n);
  const conservativeEnd = maxArrival + tokens * (1n + latency);
  if (conservativeEnd > BigInt(Number.MAX_SAFE_INTEGER))
    throw Error('O cenário ultrapassa a precisão de ticks do navegador. Use a CLI C para tempos uint64 completos.');
  return {requests:requests.map((r) => ({...r})),policy:$('policy').value,workers,quantum,memory};
}
function display(result) {
  const m = result.metrics;
  const number = (key, digits = 2) => typeof m[key] === 'number' && Number.isFinite(m[key]) ? m[key].toLocaleString('pt-BR',{maximumFractionDigits:digits}) : '—';
  $('m-completed').textContent = number('completed',0);
  $('m-count').textContent = result.input.length;
  $('m-wait').textContent = number('average_wait_ticks');
  $('m-throughput').textContent = number('throughput_per_tick',4);
  $('m-duration').textContent = number('makespan_ticks',0);
  const utilization = m.worker_utilization;
  $('m-utilization').textContent = typeof utilization === 'number' && Number.isFinite(utilization) && utilization >= 0 && utilization <= 1
    ? (utilization * 100).toLocaleString('pt-BR',{maximumFractionDigits:1}) + '%' : '—';
  $('r-policy').textContent = result.config.policy;
  $('r-events').textContent = Array.isArray(result.events) ? result.events.length : '—';
  const body = $('result-rows');
  body.replaceChildren();
  result.requests.forEach((request) => {
    const tr = document.createElement('tr');
    const data = [request.id,request.state,request.first_run_tick ?? '—',request.finished_tick ?? '—',request.waiting_ticks];
    data.forEach((value,index) => {
      const td = document.createElement('td');
      if (index === 1) td.append(node('span','state-pill',value)); else td.textContent = String(value);
      tr.append(td);
    });
    body.append(tr);
  });
  $('empty-state').hidden = true;
  $('results').hidden = false;
  $('run-state').textContent = 'CONCLUÍDO';
  $('run-state').className = 'pill done';
  if (window.OSLLMTimeline && window.OSLLMTimeline.show) window.OSLLMTimeline.show(result);
  if (window.OSLLMMemory) window.OSLLMMemory.show(result);
  if (window.OSLLMReports && window.OSLLMReports.onRun) window.OSLLMReports.onRun(result);
}
async function run() {
  if (busy) return;
  let data;
  try { data = validateForm(); } catch (error) {notice(error.message,'error'); return;}
  busy = true;
  const thisRevision = scenarioRevision;
  $('run-btn').disabled = true;
  $('run-btn').textContent = 'Executando…';
  notice('Executando no simulador local. Os resultados serão exibidos ao concluir.');
  try {
    const response = await fetch('/api/run', {method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(data)});
    const result = await response.json();
    if (!response.ok) throw Error(result.error || 'O simulador não conseguiu executar este cenário.');
    if (thisRevision !== scenarioRevision) {notice('O cenário foi alterado durante a execução. Resultado antigo descartado.');return;}
    display(result);
    notice(`Simulação concluída: ${result.metrics.completed} processo(s) finalizado(s).`, 'success');
  } catch (error) {
    invalidate();
    notice(error.message || 'Falha de comunicação com o servidor local.', 'error');
  } finally {
    busy = false;
    $('run-btn').disabled = false;
    $('run-btn').textContent = 'Executar simulação';
    $('add-btn').disabled = requests.length >= 256;
  }
}
function importCSV(text) {
  const rows = [];
  let declared = null;
  for (const raw of text.replace(/^\uFEFF/,'').split(/\r?\n/)) {
    const line = raw.trim();
    if (!line) continue;
    if (line.startsWith('# os-llm-scenario')) {
      const match = /^# os-llm-scenario v2 seed=\d+ distribution=(uniform|burst|staggered) count=(\d+)$/.exec(line);
      if (!match || declared !== null || rows.length > 0) throw Error('Cabeçalho CSV incompatível com o formato v2.');
      declared = Number(match[2]);
      continue;
    }
    if (line.startsWith('#')) continue;
    const values = line.split(',').map((s) => s.trim());
    if (values.length !== 4) throw Error('CSV inválido: cada linha deve possuir 4 campos.');
    rows.push(Object.fromEntries(fields.map((key,index) => [key,values[index]])));
    if (rows.length > 256) throw Error('O limite é de 256 requisições.');
  }
  if (declared !== null && rows.length !== declared) throw Error('Quantidade divergente do cabeçalho CSV.');
  if (!rows.length) throw Error('O CSV não contém requisições.');
  const previous = requests;
  requests = rows;
  try {validateForm();} catch(e) {requests = previous; throw e;}
  renderRows(); invalidate();
  notice(`Importadas ${rows.length} requisições. Confira os parâmetros e execute.`,'success');
}
$('policy').addEventListener('change', () => {$('quantum').disabled = $('policy').value !== 'rr';invalidate();});
$('workers').addEventListener('change', invalidate);
$('quantum').addEventListener('change', invalidate);
$('preset-btn').addEventListener('click', () => {requests = demo.map(r => ({...r}));renderRows();invalidate();notice('Cenário de exemplo restaurado.');});
$('add-btn').addEventListener('click', () => {
  if (requests.length >= 256) return;
  const used = new Set(requests.map(r=>r.id));
  let id = 1;
  while (used.has(String(id))) id++;
  requests.push({id:String(id), arrival_tick:'0',total_tokens:'5',priority:'1'});
  renderRows();invalidate();
});
$('import-btn').addEventListener('click', () => $('file').click());
$('file').addEventListener('change', async () => {
  const file = $('file').files?.[0];
  if (!file) return;
  try {
    if (file.size > 65536) throw Error('CSV excede 64 KB.');
    importCSV(await file.text());
  } catch(error) {notice(error.message,'error');}
  $('file').value = '';
});
$('run-btn').addEventListener('click', run);
const navLinks = [...document.querySelectorAll('.side-nav a')];
function syncNavigation() {
  const current = window.location.hash || '#overview';
  for (const link of navLinks) {
    const active = link.hash === current;
    link.classList.toggle('active', active);
    if (active) link.setAttribute('aria-current', 'location');
    else link.removeAttribute('aria-current');
  }
}
window.addEventListener('hashchange', syncNavigation);
syncNavigation();
renderRows();
