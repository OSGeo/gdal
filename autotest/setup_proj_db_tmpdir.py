# coding: utf-8

import os
import shutil
import sys

from osgeo import osr

autotest_dir = sys.argv[1]

proj_db_tmpdir = os.path.join(autotest_dir, "gcore", "tmp", "proj_db_tmpdir")
# print("Creating " + proj_db_tmpdir)
if not os.path.exists(proj_db_tmpdir):
    os.mkdir(proj_db_tmpdir, 0o755)
src_proj_db_found = False
for path in osr.GetPROJSearchPaths():
    if os.path.exists(os.path.join(path, "proj.db")):
        src_proj_db_found = True
        shutil.copy(os.path.join(path, "proj.db"), proj_db_tmpdir)
        break

if not src_proj_db_found:
    print("Cannot find source proj.db")
    sys.exit(1)
