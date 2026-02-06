Import("env")
import json
from pathlib import Path

project_dir = Path(env['PROJECT_DIR'])
version_path = project_dir / 'version.json'
version = 'openhaldex-s3-unknown'

try:
    data = json.loads(version_path.read_text(encoding='utf-8'))
    if isinstance(data, dict) and data.get('version'):
        version = str(data['version'])
except Exception:
    pass

# Force a quoted C string literal
env.Append(CPPDEFINES=['OPENHALDEX_VERSION=\\"%s\\"' % version])