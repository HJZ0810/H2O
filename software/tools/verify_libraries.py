"""Verify bundled library versions and deterministic content hashes (offline)."""
import hashlib,json,re
from pathlib import Path
root=Path(__file__).resolve().parents[1]/'libraries'
lock=json.loads((root/'lock.json').read_text(encoding='utf-8-sig'))
assert lock['tree_sha256_algorithm']=='sorted-sha256-space-space-posix-path-LF', 'Unknown lock hash method'
for lib in lock['libraries']:
    folder=root/lib['directory']
    version=re.search(r'^version=(.*)$',(folder/'library.properties').read_text(encoding='utf-8-sig'),re.M).group(1).strip()
    assert version==lib['version'],lib['name']+' version mismatch'
    manifest=''.join(hashlib.sha256(f.read_bytes()).hexdigest()+'  '+f.relative_to(folder).as_posix()+'\n'
        for f in sorted(folder.rglob('*'),key=lambda p:p.relative_to(folder).as_posix()) if f.is_file())
    actual=hashlib.sha256(manifest.encode('utf-8')).hexdigest()
    assert actual==lib['tree_sha256'],lib['name']+' content mismatch'
    print(lib['name']+' '+version+': PASS')
print(f'Bundled library verification PASS: {len(lock["libraries"])} libraries')
