"""Run production startup/recording in disposable, separately configured folders."""
from pathlib import Path
import hashlib
import json
import shutil
import struct
import subprocess
import sys
import time

executable = Path(sys.argv[1]).resolve()
root = Path(sys.argv[2]).resolve() / str(time.time_ns())
root.mkdir(parents=True)
results = []
cases = [
    ('fresh', None, (1920, 1080)),
    ('saved-360p', (640, 360), (640, 360)),
    ('saved-portrait', (720, 1280), (720, 1280)),
    ('missing-dimensions', (0, 0), (1920, 1080)),
    ('recording', (1280, 720), (1280, 720)),
]
for name, configured, expected in cases:
    directory = root / name
    (directory / 'Data').mkdir(parents=True)
    copy = directory / executable.name
    shutil.copy2(executable, copy)
    if configured:
        settings = [40, 40, 1000, 740, 0, *configured, 250, 2]
        (directory / 'Data/mmconfig.ini').write_text(''.join(f'{x}\n' for x in settings))
    command = [str(copy), *map(str, expected)]
    if name == 'recording':
        command.append('record')
    run = subprocess.run(command, cwd=directory, stdout=subprocess.PIPE,
                         stderr=subprocess.STDOUT, timeout=35)
    (directory / 'result.log').write_bytes(run.stdout)
    result = {'case': name, 'exit_code': run.returncode, 'log': str(directory / 'result.log')}
    if name == 'recording' and (directory / 'preview-1080p.mov').exists():
        data = (directory / 'preview-1080p.mov').read_bytes()
        samples = []
        cursor = 0
        while (start := data.find(b'\x89PNG\r\n\x1a\n', cursor)) >= 0:
            position = start + 8
            width, height = struct.unpack_from('>II', data, position + 8)
            while position + 12 <= len(data):
                size = struct.unpack_from('>I', data, position)[0]
                kind = data[position + 4:position + 8]
                position += size + 12
                if kind == b'IEND':
                    break
            samples.append({'width': width, 'height': height,
                            'sha256': hashlib.sha256(data[start:position]).hexdigest()})
            cursor = position
        result['png_samples'] = samples
        if not samples or any((x['width'], x['height']) != (1920, 1080) for x in samples):
            result['exit_code'] = 1
    results.append(result)
    print(json.dumps(result), flush=True)
(root / 'results.json').write_text(json.dumps(results, indent=2), encoding='utf-8')
print(f'Results: {root}')
sys.exit(any(result['exit_code'] != 0 for result in results))
