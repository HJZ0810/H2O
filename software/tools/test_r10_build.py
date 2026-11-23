"""Exercise the normal PowerShell build entry point without an ESP32 toolchain."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time
import unittest


SCRIPT = Path(__file__).with_name('build-r10.ps1')
SHELL = shutil.which('powershell') or shutil.which('pwsh')


@unittest.skipUnless(os.name == 'nt' and SHELL, 'Windows PowerShell build entry point')
class BuildRuns(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        tools = self.root / 'software' / 'tools'
        tools.mkdir(parents=True)
        shutil.copyfile(SCRIPT, tools / SCRIPT.name)
        self.source = self.root / 'software' / 'B-monitor'
        self.source.mkdir()
        (self.source / 'BMonitor.ino').write_text('void setup() {}')
        self.status = self.root / 'out' / 'monitor-yd2540' / 'status.json'
        stub = self.root / 'fake-cli.cmd'
        stub.write_text('@echo off\n"' + __import__('sys').executable + '" "%~dp0fake_cli.py" %*\nexit /b %errorlevel%\n')
        (self.root / 'fake_cli.py').write_text('''import os, sys, time
from pathlib import Path
args=sys.argv; out=Path(args[args.index('--output-dir')+1]); build=Path(args[args.index('--build-path')+1]); sketch=Path(args[-1]); out.mkdir(exist_ok=True,parents=True); build.mkdir(exist_ok=True,parents=True)
assert (sketch/'OneNetTrust.h').exists() == (os.environ.get('EXPECT_TRUST')=='1')
if os.getenv('WAIT'):
    (out/'started').touch(); time.sleep(3)
if os.getenv('FAIL'): sys.exit(9)
names=['BMonitor.ino.bootloader.bin','BMonitor.ino.partitions.bin','boot_app0.bin','BMonitor.ino.bin']
(build/'flash_args').write_text('\\n'.join(f'{a} {n}' for a,n in zip(['0x0','0x8000','0xe000','0x10000'], names)))
for name in names:
    if name=='boot_app0.bin' and os.getenv('MISSING'): continue
    (build/name).write_bytes(b'firmware')
    if name!='boot_app0.bin': (out/name).write_bytes(b'firmware')
''')
        self.cmd = [SHELL, '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', str(tools / SCRIPT.name), '-Target', 'monitor', '-OutputRoot', str(self.root / 'out'), '-ArduinoCli', str(stub)]

    def invoke(self, success=True, **env):
        run = subprocess.run(self.cmd, env={**os.environ, **env}, capture_output=True)
        self.assertEqual(run.returncode == 0, success, run.stderr.decode(errors='replace'))

    def result(self):
        return json.loads(self.status.read_text(encoding='utf-8-sig'))

    def test_deleted_input_and_failed_or_partial_build(self):
        trust = self.source / 'OneNetTrust.h'
        trust.touch()
        self.invoke(EXPECT_TRUST='1')
        trust.unlink()
        self.invoke()
        result = self.result()
        self.assertEqual(result['state'], 'succeeded')
        binary = self.status.parent / 'runs' / result['run'] / 'bin'
        self.assertTrue((binary / 'boot_app0.bin').is_file())
        self.assertTrue((binary / 'flash_args').is_file())
        for error in ['FAIL', 'MISSING']:
            self.invoke(False, **{error: '1'})
            failed = self.result()
            self.assertEqual(failed['state'], 'failed')
            self.assertNotEqual(failed['run'], result['run'])
            self.assertTrue((binary / 'BMonitor.ino.bin').is_file())

    def test_competing_run_preserves_active_status(self):
        proc = subprocess.Popen(self.cmd, env={**os.environ, 'WAIT': '1'}, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            for _ in range(100):
                if list(self.root.glob('out/**/started')): break
                time.sleep(.05)
            self.assertIsNone(proc.poll(), 'first build did not start')
            self.invoke(False)
            self.assertEqual(proc.communicate(timeout=12)[0] is not None, True)
            self.assertEqual(proc.returncode, 0)
            self.assertEqual(self.result()['state'], 'succeeded')
        finally:
            if proc.poll() is None: proc.kill()
            proc.communicate()


if __name__ == '__main__':
    unittest.main(verbosity=2)
