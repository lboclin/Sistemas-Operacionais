"""I25 server tests, without pretending the mock is an actual C engine."""
import http.client
import json
import os
from pathlib import Path
import sys
import tempfile
from threading import Thread
import unittest

PROJECT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(PROJECT / 'apps' / 'visual'))
import server


class ServerTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.bin = Path(self.tmp.name) / 'mock_cli'
        self.bin.write_text('''#!/usr/bin/env python3
import json,sys
from pathlib import Path
args=sys.argv
scenario=Path(args[args.index('--scenario')+1])
export=Path(args[args.index('--export-json')+1])
policy=args[args.index('--policy')+1]
rows=[]
for line in scenario.read_text().splitlines():
    rid,arr,total,pred,priority=map(int,line.split(','))
    rows.append(dict(id=rid,arrival_tick=arr,total_tokens=total,predicted_tokens=pred,priority=priority))
name={'fcfs':'FCFS','rr':'Round-Robin','ema':'SJF-EMA','sjf':'SJF-estimated','oracle':'SJF-oracle','priority':'Priority-aging'}[policy]
result={'schema':'os-llm-run','version':1,'finished':True,'input':rows,
 'config':{'policy':name,'worker_count':int(args[args.index('--workers')+1])},
 'requests':[dict(id=r['id'],state='TERMINATED',first_run_tick=r['arrival_tick'],finished_tick=r['arrival_tick']+r['total_tokens'],waiting_ticks=0) for r in rows],
 'metrics':{'completed':len(rows),'average_wait_ticks':0,'makespan_ticks':sum(r['total_tokens'] for r in rows),'throughput_per_tick':0.3}, 'events':[]}
export.write_text(json.dumps(result))
''',encoding='utf-8')
        self.bin.chmod(0o755)
        self.payload = {'requests':[{'id':'1','arrival_tick':'0','total_tokens':'6','predicted_tokens':'5','priority':'1'},
                                   {'id':'2','arrival_tick':'2','total_tokens':'2','predicted_tokens':'2','priority':'3'}],
                        'policy':'fcfs','workers':'1','quantum':'2'}

    def test_validation_bounds_and_boolean_rejection(self):
        v=server.validate(self.payload)
        self.assertEqual(v['requests'][0]['total_tokens'],6)
        self.payload['requests'][0]['arrival_tick']='18446744073709551614'
        self.assertEqual(server.validate(self.payload)['requests'][0]['arrival_tick'],2**64-2)
        for bad in ('18446744073709551615','0.5','-1','1,2','999999999999999999999999'):
            self.payload['requests'][0]['arrival_tick']=bad
            with self.assertRaises(server.InputError): server.validate(self.payload)
        self.payload['requests'][0]['arrival_tick']=0
        self.payload['requests'][0]['id']=True
        with self.assertRaises(server.InputError): server.validate(self.payload)

    def test_duplicate_and_fields_rejected(self):
        self.payload['requests'][1]['id']='1'
        with self.assertRaisesRegex(server.InputError,'duplicado'): server.validate(self.payload)
        self.payload['requests'][1]['id']='2'
        self.payload['path']='/etc/passwd'
        with self.assertRaises(server.InputError): server.validate(self.payload)
        del self.payload['path']
        self.payload['policy']='; whoami'
        with self.assertRaises(server.InputError): server.validate(self.payload)

    def test_mock_engine_contract_and_one_shot_execution(self):
        result=server.simulate(self.bin,self.payload)
        self.assertTrue(result['finished'])
        self.assertEqual(result['metrics']['completed'],2)
        self.assertEqual(result['config']['policy'],'FCFS')
        self.payload['policy']='rr'
        self.assertEqual(server.simulate(self.bin,self.payload)['config']['policy'],'Round-Robin')

    def test_http_security_and_lifecycle(self):
        server.Handler.cli=self.bin
        api=server.ThreadingHTTPServer(('127.0.0.1',0),server.Handler)
        thread=Thread(target=api.serve_forever,daemon=True)
        thread.start()
        self.addCleanup(lambda: (api.shutdown(),api.server_close(),thread.join(timeout=3)))
        def call(method,path,body=None,headers=None):
            conn=http.client.HTTPConnection('127.0.0.1',api.server_port,timeout=3)
            conn.request(method,path,body=body,headers=headers or {})
            r=conn.getresponse();data=r.read();status=r.status;hs=dict(r.getheaders());conn.close()
            return status,data,hs
        host={'Host':f'127.0.0.1:{api.server_port}'}
        self.assertEqual(call('GET','/')[0],200)
        self.assertEqual(call('GET','/app.js')[0],200)
        self.assertEqual(call('GET','/../../etc/passwd')[0],404)
        self.assertEqual(call('GET','/',headers={'Host':'evil.tld'})[0],403)
        self.assertIn('Content-Security-Policy',call('GET','/',headers=host)[2])
        raw=json.dumps(self.payload).encode()
        good={**host,'Content-Type':'application/json'}
        status,data,_=call('POST','/api/run',raw,good)
        self.assertEqual(status,200,data)
        self.assertEqual(json.loads(data)['metrics']['completed'],2)
        self.assertEqual(call('POST','/api/run',raw,{**good,'Origin':'https://evil.tld'})[0],403)
        self.assertEqual(call('POST','/api/run',raw,{**host,'Content-Type':'text/plain'})[0],415)
        self.assertEqual(call('POST','/api/run',b'{}',{**good})[0],400)
        self.assertEqual(call('POST','/api/other',raw,good)[0],404)

    @unittest.skipUnless(os.environ.get('OSLLM_CLI'), 'requires actual compiled C binary')
    def test_real_compiled_simulator(self):
        actual=Path(os.environ['OSLLM_CLI'])
        self.assertTrue(actual.is_file())
        result=server.simulate(actual,self.payload)
        self.assertEqual(result['metrics']['completed'],2)
        self.assertEqual({r['state'] for r in result['requests']},{'TERMINATED'})
        self.assertTrue(result['events'])

if __name__ == '__main__': unittest.main()
