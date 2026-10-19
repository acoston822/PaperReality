"""Transport asset-free simulator captures through the public check annotations.
This reads only the test harness artifact, never user game files or artwork.
"""
import base64
from pathlib import Path
import sys

page = sys.argv[1]
assert page in ('general', 'audio', 'sidebar')
encoded = base64.b64encode((Path('build-uikit-capture') / (page + '.png')).read_bytes()).decode()
chunks = [encoded[i:i + 60000] for i in range(0, len(encoded), 60000)]
start, end = map(int, sys.argv[2:4])
for index in range(start, min(end, len(chunks))):
    print(f'::notice title=Asset-free UIKit screenshot::UIKIT_PNG|{page}|{index}|{len(chunks)}|{chunks[index]}')
