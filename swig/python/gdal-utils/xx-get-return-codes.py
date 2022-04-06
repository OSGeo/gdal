'''Run a series of python scripts and display their exit codes.
Quick and dirty script for: Standardize gdal-utils scripts return codes #5561'''
import subprocess
# import glob
import sys
from pathlib import Path

# here = r"D:\code\public\gdal\swig\python\gdal-utils\osgeo_utils"
here = Path(__file__).parent.absolute()
print(here)

scripts = list(Path(here).glob("osgeo_utils/**/*.py" ))

i = '.' # progress meter step
results = {}
for s in scripts:
    file = Path(s)
    print(i, end='\r')
    if not 'gdal_auth.py' in file.name:
        # skip gdal_auth because it doesn't take inputs

        r = subprocess.run([sys.executable,
            file],
            shell=True,
            capture_output=True,
            text=True,
            )

        results[file.relative_to(here)] = r.returncode
    i = i+'.'

# sort by return code value and display results
results = sorted(results.items(), key=lambda x:x[1])
print('\n')
[print(x[1], str(x[0])) for x in results]