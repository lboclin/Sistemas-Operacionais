#!/usr/bin/env python3
"""Local-only HTTP adapter for the existing OS-LLM C simulator (stdlib only)."""
import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent
ASSETS = {'/': ('index.html', 'text/html; charset=utf-8'),
          '/app.js': ('app.js', 'application/javascript; charset=utf-8'),
          '/timeline.js': ('timeline.js', 'application/javascript; charset=utf-8'),
          '/memory.js': ('memory.js', 'application/javascript; charset=utf-8'),
          '/reports.js': ('reports.js', 'application/javascript; charset=utf-8'),
          '/style.css': ('style.css', 'text/css; charset=utf-8')}
MAX_BODY = 64 * 1024
MAX_OUTPUT = 16 * 1024 * 1024
MAX_REQUESTS = 256
POLICIES = {'fcfs', 'sjf', 'ema', 'oracle', 'rr', 'priority'}
LAB_CHOICES = {'race':('safe','unsafe'),'deadlock':('ordered','deadlock')}
DECIMAL = re.compile(r'(0|[1-9][0-9]*)\Z')
SIGNED = re.compile(r'(0|-?[1-9][0-9]*)\Z')


class InputError(ValueError):
    pass


def integer(value, name, low, high, signed=False):
    # Decimal strings support exact 64-bit tick values without JavaScript rounding.
    if type(value) is int:
        value = str(value)
    if not isinstance(value, str) or len(value) > 22 or not (SIGNED if signed else DECIMAL).fullmatch(value):
        raise InputError(f'{name}: informe um inteiro decimal válido')
    number = int(value)
    if not low <= number <= high:
        raise InputError(f'{name}: fora do intervalo permitido')
    return number


def validate(payload):
    if not isinstance(payload, dict) or not {'requests','policy','workers','quantum'} <= set(payload) or set(payload) - {'requests','policy','workers','quantum','memory'}:
        raise InputError('Configuração inválida: use requests, policy, workers e quantum')
    policy = payload['policy']
    if type(policy) is not str or policy not in POLICIES:
        raise InputError('Política de escalonamento desconhecida')
    workers = integer(payload['workers'], 'workers', 1, 16)
    quantum = integer(payload['quantum'], 'quantum', 1, 4294967295)
    raw = payload['requests']
    if not isinstance(raw, list) or not 1 <= len(raw) <= MAX_REQUESTS:
        raise InputError('Informe de 1 a 256 requisições')
    requests, seen = [], set()
    for position, row in enumerate(raw, 1):
        if not isinstance(row, dict) or set(row) != {'id', 'arrival_tick', 'total_tokens', 'predicted_tokens', 'priority'}:
            raise InputError(f'Requisição {position}: campos inválidos')
        rid = integer(row['id'], 'id', 1, 4294967295)
        if rid in seen:
            raise InputError(f'ID duplicado: {rid}')
        seen.add(rid)
        requests.append({
            'id': rid,
            'arrival_tick': integer(row['arrival_tick'], 'arrival_tick', 0, 18446744073709551614),
            'total_tokens': integer(row['total_tokens'], 'total_tokens', 1, 4294967295),
            'predicted_tokens': integer(row['predicted_tokens'], 'predicted_tokens', 1, 4294967295),
            'priority': integer(row['priority'], 'priority', -2147483648, 2147483647, signed=True)
        })
    memory = payload.get('memory')
    if memory is not None:
        if not isinstance(memory,dict) or set(memory) != {'frames','page_tokens','policy','fault_latency'}:
            raise InputError('Campos inválidos de memória')
        if type(memory['policy']) is not str or memory['policy'] not in ('fifo','lru'):
            raise InputError('Política online deve ser FIFO ou LRU')
        memory = {'frames':integer(memory['frames'],'frames',1,4096),
                  'page_tokens':integer(memory['page_tokens'],'page_tokens',1,4294967295),
                  'policy':memory['policy'],
                  'fault_latency':integer(memory['fault_latency'],'fault_latency',0,18446744073709551614)}
    return {'requests':requests,'policy':policy,'workers':workers,'quantum':quantum,'memory':memory}


def simulate(cli, payload, timeout=25):
    data = validate(payload)
    with tempfile.TemporaryDirectory(prefix='osllm-ui-') as directory:
        scenario, export = Path(directory) / 'scenario.csv', Path(directory) / 'run.json'
        scenario.write_text(''.join(f"{r['id']},{r['arrival_tick']},{r['total_tokens']},"
                                    f"{r['predicted_tokens']},{r['priority']}\n" for r in data['requests']), encoding='ascii')
        args = [str(cli), '--scenario', str(scenario), '--policy', data['policy'],
                '--workers', str(data['workers']), '--export-json', str(export)]
        if data['memory'] is not None:
            mem=data['memory']
            args += ['--frames',str(mem['frames']),'--page-tokens',str(mem['page_tokens']),
                     '--page-policy',mem['policy'],'--page-fault-latency',str(mem['fault_latency'])]
        if data['policy'] == 'rr':
            args += ['--quantum', str(data['quantum'])]
        if data['policy'] == 'ema':
            args += ['--ema-alpha', '0.5', '--ema-initial', '6']
        try:
            run = subprocess.run(args, capture_output=True, timeout=timeout, check=False)
        except subprocess.TimeoutExpired as exc:
            raise InputError('O simulador excedeu o tempo limite') from exc
        if run.returncode != 0:
            raise InputError('O simulador rejeitou a execução ou encontrou um erro')
        if not export.is_file() or export.stat().st_size > MAX_OUTPUT:
            raise InputError('A exportação do simulador está ausente ou excede o limite')
        try:
            result = json.loads(export.read_text(encoding='utf-8'))
        except (ValueError, UnicodeError) as exc:
            raise InputError('O simulador gerou uma exportação inválida') from exc
        source = result.get('input') if isinstance(result, dict) else None
        expected = sorted(tuple(r[k] for k in ('id', 'arrival_tick', 'total_tokens',
                      'predicted_tokens', 'priority')) for r in data['requests'])
        got = sorted(tuple(r.get(k) for k in ('id', 'arrival_tick', 'total_tokens',
                    'predicted_tokens', 'priority')) for r in source) if isinstance(source, list) and all(isinstance(r, dict) for r in source) else None
        if (not isinstance(result, dict) or result.get('schema') != 'os-llm-run' or
            result.get('finished') is not True or got != expected or
            not isinstance(result.get('metrics'), dict) or
            result['metrics'].get('completed') != len(expected) or
            not isinstance(result.get('requests'), list) or
            len(result['requests']) != len(expected) or
            not isinstance(result.get('events'), list)):
            raise InputError('Resultado incompleto ou incompatível com o cenário')
        # Audited J: do not present an internally contradictory C export as a
        # completed simulation, even if the summary claims it has finished.
        if type(result['metrics'].get('completed')) is not int:
            raise InputError('Contagem de conclusões inválida')
        by_id = {row['id']: row for row in data['requests']}
        returned = set()
        for row in result['requests']:
            if not isinstance(row, dict) or type(row.get('id')) is not int:
                raise InputError('PCB exportado inválido')
            rid = row['id']
            if rid not in by_id or rid in returned:
                raise InputError('PCBs exportados divergentes')
            returned.add(rid)
            expected_row = by_id[rid]
            if (row.get('state') != 'TERMINATED' or
                type(row.get('generated_tokens')) is not int or
                row['generated_tokens'] != expected_row['total_tokens'] or
                type(row.get('arrival_tick')) is not int or
                row['arrival_tick'] != expected_row['arrival_tick'] or
                type(row.get('first_run_tick')) is not int or
                type(row.get('finished_tick')) is not int or
                not row['arrival_tick'] <= row['first_run_tick'] <= row['finished_tick']):
                raise InputError('Resultado de PCB inconsistente com entrada')
        if returned != set(by_id):
            raise InputError('PCBs ausentes da exportação')
        last_tick = -1
        for index, event in enumerate(result['events']):
            if (not isinstance(event, dict) or
                type(event.get('sequence')) is not int or event['sequence'] != index or
                type(event.get('tick')) is not int or event['tick'] < last_tick):
                raise InputError('Trace exportado inválido ou fora de ordem')
            last_tick = event['tick']
        return result


def validate_lab(payload):
    if not isinstance(payload,dict) or set(payload) != {'lab','mode','acknowledge'}:
        raise InputError('Contrato inválido do laboratório')
    lab,mode,ack=payload['lab'],payload['mode'],payload['acknowledge']
    if type(lab) is not str or type(mode) is not str or lab not in LAB_CHOICES or mode not in LAB_CHOICES[lab]:
        raise InputError('Laboratório não permitido')
    if type(ack) is not bool or (mode in ('unsafe','deadlock') and not ack):
        raise InputError('Confirmação explícita obrigatória')
    return lab,mode


def execute_lab(binary,mode,timeout=8):
    if binary is None or not binary.is_file() or not os.access(binary,os.X_OK):
        raise InputError('Binário do laboratório indisponível')
    try:
        result=subprocess.run([str(binary),mode],capture_output=True,timeout=timeout,check=False)
    except subprocess.TimeoutExpired as exc:
        raise InputError('Laboratório excedeu watchdog') from exc
    if result.returncode != 0:
        raise InputError('Laboratório terminou com erro')
    return {'output':result.stdout[:2048].decode('utf-8',errors='replace').strip(),'exit_code':result.returncode,'mode':mode}


class Handler(BaseHTTPRequestHandler):
    cli = None
    timeout_seconds = 25
    race_lab = None
    deadlock_lab = None

    def security_headers(self):
        self.send_header('X-Content-Type-Options', 'nosniff')
        self.send_header('Referrer-Policy', 'no-referrer')
        self.send_header('Cache-Control', 'no-store')
        self.send_header('Content-Security-Policy', "default-src 'none'; script-src 'self'; style-src 'self'; connect-src 'self'; img-src 'self' data:; base-uri 'none'; form-action 'none'; frame-ancestors 'none'")

    def send_data(self, status, content, content_type):
        self.send_response(status)
        self.send_header('Content-Type', content_type)
        self.send_header('Content-Length', str(len(content)))
        self.security_headers()
        self.end_headers()
        self.wfile.write(content)

    def safe_host(self):
        # Explicit Host check prevents DNS-rebinding access to this local service.
        host = self.headers.get('Host', '')
        return host in {f'127.0.0.1:{self.server.server_port}', f'localhost:{self.server.server_port}'}

    def do_GET(self):
        if not self.safe_host():
            self.send_data(403, b'Forbidden', 'text/plain; charset=utf-8')
            return
        if self.path not in ASSETS:
            self.send_data(404, b'Not found', 'text/plain; charset=utf-8')
            return
        name, mime = ASSETS[self.path]
        self.send_data(200, (ROOT / 'static' / name).read_bytes(), mime)

    def do_POST(self):
        if not self.safe_host():
            self.send_data(403, b'Forbidden', 'text/plain; charset=utf-8')
            return
        origin = self.headers.get('Origin')
        if origin and origin not in {f'http://127.0.0.1:{self.server.server_port}', f'http://localhost:{self.server.server_port}'}:
            self.send_data(403, b'Origin rejected', 'text/plain; charset=utf-8')
            return
        if self.path not in ('/api/run','/api/lab'):
            self.send_data(404, b'Not found', 'text/plain; charset=utf-8')
            return
        if self.headers.get('Content-Type', '').split(';', 1)[0].strip().lower() != 'application/json':
            self.send_data(415, b'JSON only', 'text/plain; charset=utf-8')
            return
        length = self.headers.get('Content-Length', '')
        if not length.isdecimal() or not 0 < int(length) <= MAX_BODY:
            self.send_data(413, b'Invalid body length', 'text/plain; charset=utf-8')
            return
        try:
            request = json.loads(self.rfile.read(int(length)).decode('utf-8'))
            if self.path == '/api/lab':
                lab, mode=validate_lab(request)
                executable=self.race_lab if lab=='race' else self.deadlock_lab
                result=execute_lab(executable,mode)
            else:
                result=simulate(self.cli,request,timeout=self.timeout_seconds)
            output = json.dumps(result, ensure_ascii=False, allow_nan=False).encode('utf-8')
            if len(output) > MAX_OUTPUT:
                raise InputError('Resposta grande demais para o painel')
            self.send_data(200, output, 'application/json; charset=utf-8')
        except (InputError, ValueError, UnicodeError, TypeError) as exc:
            output = json.dumps({'error': str(exc)}, ensure_ascii=False).encode('utf-8')
            self.send_data(400, output, 'application/json; charset=utf-8')
        except (OSError, BrokenPipeError):
            self.send_data(500, b'{"error":"Falha local do simulador"}', 'application/json; charset=utf-8')

    def log_message(self, fmt, *args):
        # Avoid logging request bodies or private scenario data.
        return


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cli', type=Path, default=Path('build/os_llm_cli'))
    parser.add_argument('--port', type=int, default=8765)
    parser.add_argument('--race-lab',type=Path,default=Path('build/os_llm_race_lab'))
    parser.add_argument('--deadlock-lab',type=Path,default=Path('build/os_llm_deadlock_lab'))
    args = parser.parse_args()
    cli = args.cli.expanduser().resolve()
    if not cli.is_file() or not os.access(cli, os.X_OK):
        parser.error(f'Executável os_llm_cli não encontrado: {cli}. Compile primeiro com CMake.')
    if not 1024 <= args.port <= 65535:
        parser.error('Use uma porta entre 1024 e 65535')
    Handler.cli = cli
    Handler.race_lab=args.race_lab.expanduser().resolve()
    Handler.deadlock_lab=args.deadlock_lab.expanduser().resolve()
    with ThreadingHTTPServer(('127.0.0.1', args.port), Handler) as server:
        print(f'OS-LLM Studio: http://127.0.0.1:{server.server_port}', flush=True)
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass


if __name__ == '__main__':
    main()
