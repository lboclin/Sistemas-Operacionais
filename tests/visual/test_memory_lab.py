"""I27 security and memory config tests."""
from pathlib import Path
import sys
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'apps'/'visual'))
import server

class MemoryLabTests(unittest.TestCase):
    def payload(self):
        return dict(requests=[dict(id='1',arrival_tick='0',total_tokens='3',predicted_tokens='2',priority='0')],policy='fcfs',workers='1',quantum='2')
    def test_legacy_compatible(self):
        self.assertIsNone(server.validate(self.payload())['memory'])
    def test_memory_allowlist(self):
        p=self.payload();p['memory']=dict(frames='2',page_tokens='2',policy='lru',fault_latency='3')
        self.assertEqual(server.validate(p)['memory']['policy'],'lru')
        for key,value in [('frames','0'),('page_tokens','-1'),('policy','opt'),('fault_latency','18446744073709551616')]:
            q=self.payload();q['memory']=dict(p['memory']);q['memory'][key]=value
            with self.assertRaises(server.InputError):server.validate(q)
        p['memory']['exec']='bash';self.assertRaises(server.InputError,server.validate,p)
    def test_lab_allowlist_ack(self):
        self.assertEqual(server.validate_lab(dict(lab='race',mode='safe',acknowledge=False)),('race','safe'))
        self.assertRaises(server.InputError,server.validate_lab,dict(lab='race',mode='unsafe',acknowledge=False))
        self.assertRaises(server.InputError,server.validate_lab,dict(lab='deadlock',mode='deadlock',acknowledge=False))
        self.assertRaises(server.InputError,server.validate_lab,dict(lab='race',mode='shell',acknowledge=True))
        self.assertRaises(server.InputError,server.validate_lab,dict(lab='race',mode='safe',acknowledge=False,path='/bin/sh'))
    def test_subprocess_from_fixed_path(self):
        with tempfile.TemporaryDirectory() as d:
            executable=Path(d)/'mock_lab';executable.write_text('#!/usr/bin/env python3\nimport sys\nprint("mode="+sys.argv[1])\n');executable.chmod(0o755)
            self.assertEqual(server.execute_lab(executable,'ordered')['output'],'mode=ordered')
        self.assertRaises(server.InputError,server.execute_lab,Path('/nonexistent/lab'),'safe')

if __name__=='__main__': unittest.main()
