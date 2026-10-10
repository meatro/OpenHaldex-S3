Import("env")
import json
import subprocess
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

# Git commit hash (short), with -dirty suffix if working tree has changes
git_commit = 'unknown'
try:
    git_commit = subprocess.check_output(
        ['git', 'rev-parse', '--short=8', 'HEAD'],
        cwd=str(project_dir),
        stderr=subprocess.DEVNULL,
        text=True
    ).strip()
    status = subprocess.check_output(
        ['git', 'status', '--porcelain'],
        cwd=str(project_dir),
        stderr=subprocess.DEVNULL,
        text=True
    ).strip()
    if status:
        git_commit += '-dirty'
except Exception:
    pass

# Force a quoted C string literal
env.Append(CPPDEFINES=[
    'OPENHALDEX_VERSION=\\"%s\\"' % version,
    'OPENHALDEX_GIT_COMMIT=\\"%s\\"' % git_commit,
])