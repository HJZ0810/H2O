import json,os,subprocess,unittest
from pathlib import Path
BIN=Path(os.environ['EXAM_CORE_BIN'])
class V2(unittest.TestCase):
 def setUp(self):
  self.p=subprocess.Popen([str(BIN)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True,encoding='utf8')
  self.send('link',up=True);self.recv('hello_ack',protocol=2)
 def tearDown(self):
  self.p.stdin.close();self.p.wait(timeout=3);self.p.stdout.close()
 def send(self,op,**d):
  self.p.stdin.write(json.dumps(dict(op=op,**d),ensure_ascii=False)+'\n');self.p.stdin.flush();return json.loads(self.p.stdout.readline())
 def recv(self,typ,**d):return self.send('receive',payload={'type':typ,'data':d})
 def restore(self,current=None,score=0,revision=0,hp=3,hints=1,removed=None):
  r=self.send('resume')['command'];self.assertIsNotNone(r)
  return self.recv('resume_ack',protocol=2,request_id=r['data']['request_id'],exam_id='SESSION',revision=revision,started=current is not None,finished=current is not None and current['type']=='game_over',score=score,hp=hp,hints_left=hints,removed_options=removed or [],current=current)
 def q(self):return {'type':'question','data':{'level':1,'qid':'T1','type':'single','text':'测试题','options':[{'key':'A','text':'甲'},{'key':'B','text':'乙'}]}}
 def test_handshake_needs_resume_and_start_uses_session_and_revision(self):
  s=self.restore();self.assertTrue(s['statusKnown']);c=self.send('start')['command'];self.assertEqual(c['data']['exam_id'],'SESSION');self.assertEqual(c['data']['revision'],0);self.assertTrue(c['data']['request_id'])
 def test_resume_restores_question_and_hint_removals(self):
  s=self.restore(self.q(),20,4,2,0,['B']);self.assertEqual(s['phase'],'question');self.assertEqual(s['score'],20);self.assertEqual(s['removed'],2);self.assertEqual(s['hints'],0)
 def test_lost_answer_recovers_judge_without_adding_score_twice(self):
  self.restore(self.q(),0,2);self.send('choose',index=0);self.send('submit',now=100);self.send('tick',now=12101)
  j={'type':'judge','data':{'level':1,'qid':'T1','correct':True,'correct_answer':'A','score':10,'hp':3,'explain':'解析'}}
  s=self.restore(j,10,3);self.assertEqual(s['phase'],'judge');self.assertEqual(s['score'],10);c=self.send('advance')['command'];self.assertEqual(c['type'],'continue');self.assertEqual(c['data']['revision'],3)
 def test_delayed_reply_cannot_replace_current_state(self):
  self.restore(self.q(),0,2);self.send('choose',index=0);c=self.send('submit')['command'];s=self.recv('judge',protocol=2,request_id='OTHER',exam_id='SESSION',revision=3,total_score=10,qid='T1',correct=True,correct_answer='A',score=10,hp=3,explain='解析');self.assertTrue(s['busy']);self.assertEqual(s['phase'],'question')
 def test_finished_snapshot_restores_result_without_start(self):
  s=self.restore({'type':'game_over','data':{'total_score':350,'rank':1,'certificate':'TEST-CERT','message':'完成'}},350,99);self.assertEqual(s['phase'],'finished');self.assertEqual(s['score'],350);self.assertIsNone(self.send('start')['command'])
if __name__=='__main__':unittest.main(verbosity=2)
