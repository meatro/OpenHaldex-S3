Import("env")
import os
import subprocess
from pathlib import Path

project_dir = Path(env['PROJECT_DIR'])
dot_file = project_dir / 'docs' / 'src' / 'gen4_and_5_wiring.dot'
svg_file = project_dir / 'docs' / 'images' / 'gen4_and_5_wiring.svg'

if not dot_file.exists():
    print(f"Warning: {dot_file} not found, skipping diagram generation")
else:
    os.makedirs(svg_file.parent, exist_ok=True)
    
    # Only regenerate if dot file is newer than svg
    should_build = True
    if svg_file.exists():
        if dot_file.stat().st_mtime <= svg_file.stat().st_mtime:
            should_build = False
    
    if should_build:
        try:
            result = subprocess.run(
                ['dot', '-Tsvg', str(dot_file), '-o', str(svg_file)],
                capture_output=True, text=True
            )
            if result.returncode != 0:
                print(f"Warning: graphviz dot failed: {result.stderr}")
            else:
                print(f"Generated {svg_file}")
        except FileNotFoundError:
            print("Warning: graphviz 'dot' command not found, skipping diagram generation")
        except Exception as e:
            print(f"Warning: Failed to generate diagram: {e}")
