#!/bin/bash

set -e

. ../scripts/setdevenv.sh

export PYTEST="python3 -m pytest --capture=no -ra -vv -p no:sugar --color=no"

export GDAL_JIT_DEBUG=YES

# Run C++ tests
make quicktest

mv autotest/gdrivers/rl2.py autotest/gdrivers/rl2.py.dis

(cd autotest && $PYTEST)
