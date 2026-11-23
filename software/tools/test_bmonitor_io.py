"""Compile and run the standalone monitor paths using the actual firmware and ArduinoJson."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

TOOLS = Path(__file__).resolve().parent
SRC = TOOLS.parent / 'B-monitor'
cc = shutil.which('g++') or r'D:\PRO\codeblocks-25.03mingw-nosetup\MinGW\bin\g++.exe'
env = {**os.environ, 'PATH': str(Path(cc).parent) + os.pathsep + os.environ['PATH']}
sketch = (SRC / 'BMonitor.ino').read_text('utf8')
# Include actual entry points with host ADC/LEDC, clock, mutex and publish adapters.
parts = sketch[sketch.index('static bmon::Sample sampleSensors'):sketch.index('// ---------------- OneNET MQTT')]
parts += sketch[sketch.index('static void report()'):sketch.index('static uint32_t nextConnectAt')]
# Arduino String(uint32_t) has a formatting constructor. Use the STL equivalent.
parts = parts.replace('String(++msgSeq)', 'std::to_string(++msgSeq)')
with tempfile.TemporaryDirectory() as temp:
    folder = Path(temp)
    (folder / 'monitor_functions.inc').write_text(parts, encoding='utf8')
    for board in ['yd2540', 'verify']:
        exe = folder / (board + '.exe')
        subprocess.run([cc, '-std=c++17', '-I'+str(folder), '-I'+str(SRC),
                        '-I'+str(TOOLS.parent / 'libraries/ArduinoJson/src'),
                        *(['-DBOARD_VERIFY'] if board == 'verify' else []),
                        str(TOOLS / 'test_bmonitor_io.cpp'), '-o', str(exe)], check=True, env=env)
        subprocess.run([str(exe)], check=True, env=env)
