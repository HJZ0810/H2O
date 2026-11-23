"""Exercise ordinary dashboard controls against the loopback fixture. Requires Playwright + Edge."""
import json
import time
from pathlib import Path
from urllib.parse import urlsplit
from playwright.sync_api import expect, sync_playwright
from r10_gateway_test import DEVICE, fixture, recorder

OUT = Path.home() / 'Pictures' / 'Agent截图' / 'H2O-R10-B'


def run():
    OUT.mkdir(parents=True,exist_ok=True)
    with fixture() as f, sync_playwright() as engine:
        at = time.time()-600
        points = [dict(ts=at+i*5,temperature=20+i%8/2,humidity=58-i%11,smoke=85+i%15,air=60,fan_level=0,servo=90,onenet=1) for i in range(38)]
        historical = [*points, dict(ts=1, temperature=99)]
        recorder.data[DEVICE] = dict(at=points[-1]['ts'],points=historical,latest=points[-1])
        browser=engine.chromium.launch(channel='msedge')
        for name,width,height in [('desktop',1440,900),('mobile',390,844)]:
            ctx=browser.new_context(viewport={'width':width,'height':height},device_scale_factor=2,service_workers='block')
            remote=[]
            def restrict(route):
                if urlsplit(route.request.url).netloc == f'127.0.0.1:{f.port}':
                    return route.continue_()
                remote.append(route.request.url)
                return route.abort()
            ctx.route('**/*',restrict)
            sockets=[]
            def socket(ws):
                assert urlsplit(ws.url).netloc==f'127.0.0.1:{f.port}'
                ws.on_message(lambda raw:sockets.append((ws,json.loads(raw)['sub'])))
            ctx.route_web_socket('**/*',socket)
            page=ctx.new_page(); errors=[]
            page.on('pageerror',lambda e:errors.append(str(e)))
            page.clock.install()
            page.goto(f.origin+'/h2o/')
            elem=lambda id:page.locator('#'+id)
            expect(elem('fresh')).to_contain_text('历史快照');expect(elem('stOne')).to_have_text('未知');expect(elem('stSrv')).to_have_text('待上报')
            expect(elem('ptCount')).to_have_text(str(len(points)))
            expect(elem('mode')).to_have_text('需要令牌');expect(elem('sendFan')).to_be_disabled()
            if name=='desktop':page.screenshot(path=OUT/'01-history.png',full_page=True)
            ws,topic=sockets[-1];assert topic==f'h2o/{DEVICE}/telemetry'
            def emit(topic,d):ws.send(json.dumps(dict(t='msg',topic=topic,data=json.dumps(d))))
            emit(topic,dict(temperature=0,humidity=62,smoke=105,air=73,fan_level=20,servo=45,onenet=1));expect(elem('stSrv')).to_have_text('在线');expect(elem('vTemp')).to_have_text('0°C')
            elem('token').fill('mock-wrong');elem('fan_level').fill('20');elem('sendFan').click();expect(elem('receipt')).to_contain_text('操作令牌无效');expect(elem('token')).to_have_value('')
            elem('token').fill(f.token); elem('fan_level').fill('101');expect(elem('sendFan')).to_be_disabled();elem('fan_level').fill('55');elem('sendFan').click();expect(elem('receipt')).to_contain_text('设备已确认');expect(elem('vFan')).to_have_text('20%')
            page.wait_for_timeout(1050);elem('servo').fill('120');elem('sendServo').click();expect(elem('receipt')).to_contain_text('舵机 120° · 设备已确认');expect(elem('vServo')).to_have_text('45°')
            assert [call[2:4] for call in f.calls[-2:]]==[('fan_level',55),('servo',120)]
            emit(topic,dict(smoke=-1,air=0,fan_level=55,servo=120,onenet=1));expect(elem('vSmoke')).to_have_text('--相对值');expect(elem('vHumi')).to_have_text('--%RH');expect(elem('vAir')).to_have_text('0相对值');expect(elem('vServo')).to_have_text('120°')
            elem('lock').click();expect(elem('sendFan')).to_be_disabled();expect(elem('token')).to_have_value('')
            assert page.evaluate('document.documentElement.scrollWidth<=window.innerWidth');page.screenshot(path=OUT/f'02-live-{name}.png',full_page=True)
            # The gateway can keep recording after this browser loses its live stream.
            ws.close()
            expect(elem('stSrv')).to_have_text('已中断')
            later = dict(ts=page.evaluate('Date.now()')/1000+3, temperature=19, servo=73)
            recorder.data[DEVICE] = dict(at=later['ts'], latest=later, points=[*points, later])
            page.clock.fast_forward(61000)
            expect(elem('vServo')).to_have_text('73°')
            expect(elem('fresh')).to_contain_text('历史快照')
            expect(elem('stOne')).to_have_text('未知')
            assert page.evaluate('series.temperature.at(-1)') == 19
            if name=='desktop':page.screenshot(path=OUT/'04-recovered-history.png',full_page=True)
            # Fetching the same snapshot twice must not duplicate chart samples.
            count = elem('ptCount').inner_text()
            page.evaluate('loadHistory()')
            expect(elem('ptCount')).to_have_text(count)
            recorder.data[DEVICE] = dict(at=points[-1]['ts'], latest=points[-1], points=historical)
            elem('token').fill(f.token);elem('dev').fill('mock-readonly');expect(elem('sendServo')).to_be_disabled();page.get_by_role('button',name='切换设备').click();expect(elem('token')).to_have_value('');expect(elem('token')).to_be_disabled();expect(elem('mode')).to_have_text('只读');expect(elem('ptCount')).to_have_text('0');expect(elem('vFan')).to_have_text('--%')
            ws2,_=sockets[-1];ws2.send(json.dumps(dict(t='msg',topic=topic,data='{"servo":150}')));expect(elem('vServo')).to_have_text('--°');expect(elem('stOne')).to_have_text('未知')
            assert page.evaluate('localStorage.length + sessionStorage.length') == 0
            if name=='desktop':
                pending=[]
                page.route('**/history?dev=yhyegj-01*',lambda route:pending.append(route))
                elem('dev').fill(DEVICE);page.get_by_role('button',name='切换设备').click();expect(elem('mode')).to_have_text('需要令牌')
                assert pending
                elem('dev').fill('mock-readonly');page.get_by_role('button',name='切换设备').click();expect(elem('receipt')).to_contain_text('未开放')
                pending[0].fulfill(json=dict(dev=DEVICE,latest=points[-1],at=points[-1]['ts'],points=points));page.wait_for_timeout(100)
                expect(elem('ptCount')).to_have_text('0');expect(elem('vTemp')).to_have_text('--°C')
            if name=='mobile':page.screenshot(path=OUT/'03-readonly-mobile.png',full_page=True)
            page.route('**/control/status?dev=*', lambda route: route.fulfill(status=503, json={}))
            page.get_by_role('button', name='切换设备').click()
            expect(elem('receipt')).to_contain_text('无法查询控制权限')
            expect(elem('sendFan')).to_be_disabled()
            if name=='mobile':page.screenshot(path=OUT/'05-gateway-unavailable-mobile.png',full_page=True)
            assert not errors and not remote,(errors,remote);ctx.close(); time.sleep(1.05)
        browser.close();print('PASS desktop + mobile; four mock device confirmations;',OUT)


if __name__=='__main__':run()
