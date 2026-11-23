"""Rebuild the editable R10 annotation candidate from the pinned wide release."""
import hashlib
import json
from pathlib import Path
from zipfile import ZipFile

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT.parent / 'R9-1-4-RC1/H2O-R9-1-4-RC1.epro2'
OUT = ROOT / 'source/H2O-R10-YD254.epro2'
assert hashlib.sha256(BASE.read_bytes()).hexdigest() == '43d7dce986439bf3068e9f61bdc7d4c9eeb8e57a0de46f89ccb0e7b6bd577c8f'
team = json.loads((ROOT / 'evidence/course.json').read_text(encoding='utf8'))['team']
label = ' '.join([team[3], '物联网4班', team[4], *team[5].split('，')])
labels = {
    '8377bc1af12c3586': (label, 50), 'f4af01b6fc122718': (label, 50),
    '9b1be462ca044aa3': ('H2O-A R10 2026-11', 45),
    '28664a9fff81a236': ('H2O-B R10 2026-11', 42),
    '2e9aa98d1b323691': ('两排距25.40', 48),
    'a6ef6905dcc51938': ('SG90 转接 / MG996R', 58),
}
changed = []
pack = lambda obj: json.dumps(obj, ensure_ascii=False, separators=(',', ':'))
with ZipFile(BASE) as src, ZipFile(OUT, 'w') as dst:
    for member in src.infolist():
        data = src.read(member)
        if member.filename == 'project2.json':
            meta = json.loads(data); meta['title'] = OUT.stem; data = pack(meta).encode()
        if member.filename.endswith('.epru'):
            lines = data.decode().split('\n')
            for i, line in enumerate(lines):
                if '||' not in line: continue
                header, payload = line.split('||', 1)
                obj = json.loads(header); ident = obj.get('id')
                if ident not in labels and ident != 'db8122cd2b731fed': continue
                item = json.loads(payload.removesuffix('|')); before = item.copy()
                if ident in labels:
                    assert obj['type'] == 'STRING'
                    item['text'], item['fontSize'] = labels[ident]
                    if ident == '28664a9fff81a236':  # move version out of the J10 connector footprint
                        item.update(x=1370, y=-2190)
                else:
                    assert item['layerId'] == 13
                    item['coords'] = [851.1811,-535.9055,-185,-535.9055,-185,-1535.9055,851.1811,-1535.9055]
                    item['text'] = dict(item['text'], y=-1278.1027, text='25.400mm')
                lines[i] = header + '||' + pack(item) + '|'
                changed.append({'id':ident, 'before':before, 'after':item})
            data = '\n'.join(lines).encode(); member.filename = OUT.stem + '.epru'
        dst.writestr(member, data)
assert len(changed) == 7
with ZipFile(OUT) as check, ZipFile(BASE) as original:
    assert check.testzip() is None
    old = original.read(next(s for s in original.namelist() if s.endswith('.epru'))).splitlines()
    new = check.read(OUT.stem+'.epru').splitlines(); assert len(old) == len(new)
    assert sum(a != b for a,b in zip(old,new)) == 7
    for rec in changed:
        assert sum(line.endswith(('||'+pack(rec['after'])+'|').encode()) and json.loads(line.split(b'||')[0]).get('id') == rec['id'] for line in new) == 1
(ROOT/'evidence/source-diff.json').write_text(json.dumps(changed, ensure_ascii=False, indent=2),encoding='utf8')
print(OUT, '7 objects read back, other records identical')
