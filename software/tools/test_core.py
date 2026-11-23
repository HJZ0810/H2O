"""Behavior tests run against the actual C++ state machine; no network access."""
import json, subprocess, unittest, os
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
BIN=Path(os.environ.get('EXAM_CORE_BIN',str(ROOT/'validation'/'core_harness.exe')))

class Terminal:
    def __init__(self):
        self.proc=subprocess.Popen([str(BIN)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True,encoding='utf8')
    def send(self,op,**data):
        self.proc.stdin.write(json.dumps(dict(op=op,**data),ensure_ascii=False)+'\n'); self.proc.stdin.flush()
        line=self.proc.stdout.readline()
        if not line: raise RuntimeError('harness exited')
        return json.loads(line)
    def recv(self,typ,**data): return self.send('receive',payload={'type':typ,'data':data})
    def close(self):
        self.proc.stdin.close(); self.proc.wait(timeout=3); self.proc.stdout.close()

class CoreTests(unittest.TestCase):
    def setUp(self):
        self.t=Terminal(); self.t.send('link',up=True); self.t.recv('hello_ack',device_id='TEST'); self.t.send('sync',score=0,hp=3)
    def tearDown(self): self.t.close()
    def question(self,typ='single'):
        self.t.send('start')
        self.t.recv('story',level=1,seq=1,total=1,text='剧情')
        self.t.send('advance')
        return self.t.recv('question',level=1,qid='L1-Q',type=typ,text='一个问题',options=[{'key':k,'text':k+'选项'} for k in 'ABCD'])
    def test_explicit_start_then_story_request(self):
        s=self.t.send('start'); self.assertEqual(s['command']['type'],'start')
        self.t.recv('story',level=1,seq=1,total=2,text='中文剧情')
        s=self.t.send('advance'); self.assertEqual(s['command']['type'],'story_next')
        self.assertIsNone(self.t.send('advance')['command'])
    def test_submit_requires_selection_and_blocks_repeat(self):
        self.question(); self.assertIsNone(self.t.send('submit')['command'])
        self.t.send('choose',index=2)
        cmd=self.t.send('submit')['command']; self.assertEqual(cmd,{'type':'answer','data':{'qid':'L1-Q','choice':'C'}})
        self.assertIsNone(self.t.send('submit')['command'])
    def test_multi_selection_toggles_and_sorts(self):
        self.question('multi')
        for i in [3,0,1,1,2]: self.t.send('choose',index=i)
        self.assertEqual(self.t.send('submit')['command']['data']['choice'],'A,C,D')
    def test_judge_kept_until_ack_and_duplicate_not_counted(self):
        self.question(); self.t.send('choose',index=0); self.t.send('submit')
        self.t.recv('judge',qid='L1-Q',correct=True,correct_answer='A',score=10,hp=3,explain='解析')
        s=self.t.recv('question',level=1,qid='L1-NEXT',type='bool',text='下一题',options=[{'key':'A','text':'对'},{'key':'B','text':'错'}])
        self.assertEqual((s['phase'],s['text'],s['score']),('judge','解析',10))
        s=self.t.recv('judge',qid='L1-Q',correct=True,correct_answer='A',score=10,hp=3,explain='解析')
        self.assertEqual(s['score'],10)
        s=self.t.send('advance'); self.assertEqual((s['phase'],s['qid']),('question','L1-NEXT'))
    def test_late_follow_accepted_after_user_ack(self):
        self.question(); self.t.send('choose',index=0); self.t.send('submit')
        self.t.recv('judge',qid='L1-Q',correct=False,correct_answer='B',score=0,hp=2,explain='解析')
        self.assertIsNone(self.t.send('advance')['command'])
        s=self.t.recv('story',level=1,seq=1,total=2,text='重打剧情')
        self.assertEqual((s['phase'],s['hp']),('story',2))
    def test_hint_removes_selection_and_does_not_submit(self):
        self.question('multi'); self.t.send('choose',index=0)
        self.assertEqual(self.t.send('hint')['command']['type'],'hint')
        self.t.recv('hint_ack',qid='L1-Q',remove_options=['A','D'],remaining_hints=0)
        s=self.t.send('choose',index=0); self.assertEqual(s['selected'],0)
        self.assertEqual(s['removed'],9); self.assertIsNone(self.t.send('hint')['command'])
    def test_disconnect_reconnect_never_auto_restarts(self):
        self.question(); self.t.send('choose',index=1); self.t.send('submit')
        s=self.t.send('link',up=False); self.assertEqual(s['phase'],'recovery')
        s=self.t.send('link',up=True); self.assertIsNone(s['command'])
        self.t.recv('hello_ack',device_id='TEST')
        self.assertIsNone(self.t.send('submit')['command'])
        self.assertIsNone(self.t.send('advance')['command'])
        self.t.send('sync',score=0,hp=3)
        self.assertEqual(self.t.send('start')['command']['type'],'start')
    def test_answer_timeout_never_retries(self):
        self.question(); self.t.send('choose',index=0); self.t.send('submit',now=100)
        s=self.t.send('tick',now=12101); self.assertEqual(s['phase'],'recovery'); self.assertIsNone(s['command'])
        self.assertIsNone(self.t.send('submit')['command'])
    def test_timeout_requires_new_score_snapshot_before_restart(self):
        self.t.send('sync',score=80,hp=3)
        self.question(); self.t.send('choose',index=0); self.t.send('submit',now=100)
        self.t.send('tick',now=12101)
        self.assertIsNone(self.t.send('start',now=12200)['command'])
    def test_inflight_or_queued_snapshot_cannot_cross_recovery_fence(self):
        old=self.t.send('sync',score=80,hp=3)['generation']
        self.question();self.t.send('choose',index=0);self.t.send('submit',now=100)
        paused=self.t.send('tick',now=12101)
        self.assertNotEqual(paused['generation'],old)
        stale=self.t.send('sync',score=80,hp=3,generation=old)
        self.assertFalse(stale['statusKnown'])
        self.assertIsNone(self.t.send('start',now=12200)['command'])
        fresh=self.t.send('sync',score=90,hp=3,generation=paused['generation'])
        self.assertTrue(fresh['statusKnown']);self.assertEqual(fresh['score'],90)
        self.assertEqual(self.t.send('start',now=12300)['command']['type'],'start')
    def test_finished_http_snapshot_resolves_lost_final_reply_without_start(self):
        self.question();self.t.send('choose',index=0);self.t.send('submit',now=100)
        self.t.send('tick',now=12101)
        s=self.t.send('sync',score=350,hp=3,finished=True,certificate='LOCAL-CERT',rank=2)
        self.assertEqual((s['phase'],s['score'],s['certificate']),('finished',350,'LOCAL-CERT'))
        self.assertIsNone(s['command']);self.assertIsNone(self.t.send('start')['command'])
    def test_stale_finished_snapshot_cannot_cross_recovery_fence(self):
        old=self.t.send('sync',score=80,hp=3)['generation']
        self.question();self.t.send('choose',index=0);self.t.send('submit',now=100)
        self.t.send('tick',now=12101)
        s=self.t.send('sync',score=350,hp=3,finished=True,generation=old)
        self.assertEqual(s['phase'],'recovery');self.assertFalse(s['statusKnown'])
    def test_finished_score_stays_known_after_reconnect(self):
        self.t.send('sync',score=350,hp=3,finished=True,certificate='LOCAL-CERT')
        self.t.send('link',up=False);self.t.send('link',up=True);self.t.recv('hello_ack')
        s=self.t.send('sync',score=0,hp=3,finished=False)
        self.assertEqual((s['phase'],s['score'],s['statusKnown']),('finished',350,True))
    def test_http_snapshot_cannot_replace_pending_start(self):
        self.t.send('start')
        s=self.t.send('sync',score=350,hp=3,finished=True)
        self.assertEqual(s['phase'],'menu');self.assertTrue(s['busy']);self.assertEqual(s['score'],0)
    def test_explicit_restart_can_receive_story_after_recovery(self):
        self.question(); self.t.send('link',up=False);self.t.send('link',up=True);self.t.recv('hello_ack',device_id='TEST')
        self.t.send('sync',score=0,hp=3)
        self.assertEqual(self.t.send('start')['command']['type'],'start')
        s=self.t.recv('story',level=1,seq=1,total=2,text='用户确认重开后的剧情')
        self.assertEqual(s['phase'],'story')
    def test_duplicate_question_preserves_selection(self):
        self.question(); self.t.send('choose',index=1)
        s=self.t.recv('question',level=1,qid='L1-Q',type='single',text='一个问题',options=[{'key':k,'text':k+'选项'} for k in 'ABCD'])
        self.assertEqual(s['selected'],2)
    def test_malformed_message_fails_closed(self):
        self.question(); s=self.t.send('raw',payload='{oops')
        self.assertEqual(s['phase'],'recovery'); self.assertIsNone(self.t.send('submit')['command'])
    def test_oversized_and_invalid_options_rejected(self):
        self.question(); s=self.t.recv('question',level=1,qid='BAD',type='multi',text='坏题',options=[{'key':'A','text':'a'},{'key':'A','text':'b'}])
        self.assertEqual(s['phase'],'recovery')
        s=self.t.send('raw',payload='x'*8193); self.assertEqual(s['phase'],'recovery')
    def test_stale_judge_cannot_score_next_question(self):
        self.question(); self.t.send('choose',index=0); self.t.send('submit')
        s=self.t.recv('judge',qid='OTHER',correct=True,score=10,hp=3,explain='stale',correct_answer='A')
        self.assertEqual((s['score'],s['phase']),(0,'question'))
    def test_heartbeat_does_not_interrupt_exam(self):
        self.question(); self.t.send('choose',index=2)
        s=self.t.recv('hello_ack',device_id='TEST'); self.assertEqual((s['phase'],s['selected']),('question',4))
    def test_start_requires_current_connection_score_snapshot(self):
        self.t.send('link',up=False);self.t.send('link',up=True);self.t.recv('hello_ack',device_id='TEST')
        self.assertIsNone(self.t.send('start')['command'])
        self.t.send('sync',score=70,hp=3)
        self.assertEqual(self.t.send('start')['command']['type'],'start')
        s=self.t.recv('story',level=2,seq=1,total=2,text='同步后开考')
        self.assertEqual(s['score'],70)
    def test_old_http_snapshot_cannot_overwrite_active_score(self):
        self.question();self.t.send('choose',index=0);self.t.send('submit')
        self.t.recv('judge',qid='L1-Q',correct=True,correct_answer='A',score=10,hp=3,explain='解析')
        s=self.t.send('sync',score=0,hp=1)
        self.assertEqual((s['score'],s['hp']),(10,3))
    def test_all_four_multi_choices_and_finish_buffer(self):
        self.question('multi')
        for i in range(4): self.t.send('choose',index=i)
        self.assertEqual(self.t.send('submit')['command']['data']['choice'],'A,B,C,D')
        self.t.recv('judge',qid='L1-Q',correct=True,correct_answer='A,B,C,D',score=10,hp=3,explain='最后一题')
        self.t.recv('game_over',total_score=350,rank=1,certificate='ZLC-2025-0350',message='恭喜通关')
        s=self.t.send('advance'); self.assertEqual((s['phase'],s['score']),('finished',350))

    def test_level_transition_burst_does_not_restart_exam(self):
        self.question()
        self.t.send('choose',index=0)
        self.t.send('submit')
        self.t.recv('judge',qid='L1-Q',correct=True,correct_answer='A',score=10,hp=3,explain='解析')
        self.t.recv('level_result',level=1,score=10,hp=3,passed=True)
        self.t.recv('story',level=2,seq=1,total=1,text='第二关剧情')
        self.t.recv('question',level=2,qid='L2-Q1',type='single',text='第二关第一题',
                    options=[{'key':'A','text':'A'},{'key':'B','text':'B'}])
        self.assertEqual(self.t.send('advance')['phase'],'judge')
        self.assertEqual(self.t.send('advance')['phase'],'story')
        self.assertEqual((self.t.send('advance')['phase'],self.t.send('advance')['qid']),
                         ('question','L2-Q1'))

if __name__=='__main__': unittest.main(verbosity=2)
