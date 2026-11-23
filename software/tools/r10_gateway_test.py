"""Loopback-only fixture and HTTP regression tests. No broker, cloud client or real credentials."""
import hashlib
import http.client
import json
from pathlib import Path
import secrets
import threading
import time
import unittest
from contextlib import contextmanager
from types import SimpleNamespace
from unittest.mock import patch

import recorder
from ws_bridge import subscription
import r10_control

WEB = Path(__file__).resolve().parents[2] / 'web'
DEVICE = 'yhyegj-01'


class LocalHandler(recorder.Handler):
    def static(self, filename, mime):
        body = (WEB / filename).read_bytes()
        self.send_response(200)
        self.send_header('Content-Type', mime)
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path == '/h2o/':
            return self.static('dashboard.html', 'text/html; charset=utf-8')
        if self.path == '/h2o/lib/chart.umd.min.js':
            return self.static('lib/chart.umd.min.js', 'text/javascript')
        self.path = self.path.removeprefix('/h2o/api')
        return super().do_GET()

    def do_POST(self):
        self.path = self.path.removeprefix('/h2o/api')
        return super().do_POST()


@contextmanager
def fixture():
    server = recorder.ThreadingHTTPServer(('127.0.0.1', 0), LocalHandler)
    port = server.server_port
    origin = f'http://127.0.0.1:{port}'
    token = secrets.token_urlsafe(32)
    env = dict(H2O_CONTROL_ORIGIN=origin, H2O_ONENET_PRODUCT='mock-product',
               H2O_ONENET_AUTHORIZATION='mock-cloud', H2O_CONTROL_DEVICES=DEVICE,
               H2O_OPERATOR_SHA256=hashlib.sha256(token.encode()).hexdigest())
    calls = []

    def writer(*args):
        calls.append(args)
        return True

    with patch.object(recorder, 'control', r10_control.Control(env, writer)), patch.object(recorder, 'data', {}):
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            yield SimpleNamespace(port=port, origin=origin, token=token, calls=calls)
        finally:
            server.shutdown()
            server.server_close()
            thread.join()


def post(f, msg, **overrides):
    conn = http.client.HTTPConnection('127.0.0.1', f.port, timeout=3)
    try:
        headers = {'Origin': f.origin, 'Content-Type': 'application/json', 'Authorization': 'Bearer ' + f.token}
        conn.request('POST', '/h2o/api/control', json.dumps(msg), headers | overrides)
        response = conn.getresponse()
        return response.status, json.loads(response.read())
    finally:
        conn.close()


class GatewayTests(unittest.TestCase):
    def test_telemetry_subscription(self):
        topic = f"h2o/{DEVICE}/telemetry"
        self.assertEqual(subscription(json.dumps({"sub": topic})), topic)
        for frame in [{"sub":"h2o/+/telemetry"}, {"sub":"#"}, {"sub":"h2o/D/cmd"}, {"pub":topic}, {"sub":topic,"data":{}}]:
            self.assertIsNone(subscription(json.dumps(frame)))

    def test_guardrails(self):
        with fixture() as f:
            msg = dict(dev=DEVICE, key='servo', value=90)
            for change, headers, expected in [({}, {'Authorization':''}, 401), ({}, {'Origin':'https://wrong.invalid'}, 403),
                    ({'dev':'other'}, {}, 403), ({'value':True}, {}, 400), ({'value':181}, {}, 400),
                    ({'key':'fan_level','value':-1}, {}, 400), ({'key':'fan_level','value':101}, {}, 400),
                    ({'key':'other'}, {}, 400), ({'value':1.5}, {}, 400), ({'product_id':'other'}, {}, 400)]:
                with self.subTest(change=change,expected=expected):
                    self.assertEqual(post(f,msg | change,**headers)[0], expected)
            self.assertFalse(f.calls)
            self.assertEqual(post(f,msg)[0],200)
            self.assertEqual(post(f,msg)[0],429)
            self.assertEqual(f.calls,[('mock-product',DEVICE,'servo',90,'mock-cloud')])
            with patch.object(recorder,'control',r10_control.Control({})):
                self.assertEqual(post(f,msg)[0],503)

    def test_official_envelope(self):
        for status, cloud, expected in [(200, {'code':0,'data':{'code':200}},True), (200, {'code':0,'data':{'code':503}},False),
                (200,{'code':0},False),(200,{'code':1,'data':{'code':200}},False),(302,{},False)]:
            with self.subTest(status=status,cloud=cloud), patch.object(r10_control.http.client,'HTTPSConnection') as cls:
                conn = cls.return_value
                response = conn.getresponse.return_value
                response.status = status
                response.read.return_value = json.dumps(cloud).encode()
                self.assertEqual(r10_control.write_property('P','D','fan_level',0,'mock-cloud'), expected)
                cls.assert_called_once_with('iot-api.heclouds.com',timeout=8)
                method, path, body, headers = conn.request.call_args.args
                self.assertEqual((method,path),('POST','/thingmodel/set-device-property'))
                self.assertEqual(json.loads(body),{'product_id':'P','device_name':'D','params':{'fan_level':0}})
                self.assertEqual(headers['authorization'],'mock-cloud')

    def test_recording(self):
        with fixture():
            msg=SimpleNamespace(topic=f'h2o/{DEVICE}/telemetry',retain=True,payload=b'{"ts":123,"air":0}')
            recorder.on_message(None,None,msg)
            self.assertFalse(recorder.data)
            msg.retain=False
            recorder.on_message(None,None,msg)
            latest=recorder.data[DEVICE]['latest']
            self.assertEqual(latest['air'],0)
            self.assertLess(abs(time.time()-latest['ts']),1)


if __name__ == '__main__':
    unittest.main()
