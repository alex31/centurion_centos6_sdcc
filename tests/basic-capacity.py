"""Load and execute a BASIC program larger than 4 KiB on a disposable disk."""
from pathlib import Path
import subprocess
import sys
from tempfile import TemporaryDirectory

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
from centos_disk import write_file
from centos_text import encode

# A DATA line uses 303 token bytes: a three-byte header, one keyword,
# fifty five-byte numbers and forty-nine commas. The final lines add 23 bytes.
lines = []
for line in range(1, 18):
    values = ['1'] * 50
    if line == 17:
        values[-1] = '9'
    lines.append(f'{line} DATA ' + ','.join(values))
lines.extend(['900 RESTORE 850', '901 READ A', '902 PRINT A'])

with TemporaryDirectory(prefix='basic-capacity-') as directory:
    disk = Path(directory) / 'capacity.img'
    image = bytearray(Path(sys.argv[1]).read_bytes())
    write_file(image, 'SBASIC.BDEMO', 'TXT', encode('\n'.join(lines)))
    disk.write_bytes(image)
    subprocess.run(['node', ROOT / 'tests/basic-capacity.js', disk], check=True)
