#!/bin/bash

set -e

. ../scripts/setdevenv.sh

export PYTEST="python3 -m pytest -vv -p no:sugar --color=no"

make quicktest

PYTEST_SKIP=
PYTEST_XFAIL="gcore/tiff_ovr.py gdrivers/gribmultidim.py gdrivers/mbtiles.py gdrivers/vrtwarp.py gdrivers/wcs.py utilities/test_gdalwarp.py pyscripts/test_gdal_pansharpen.py"

# Fails with ERROR 1: OGDI DataSource Open Failed: Could not find the dynamic library "vrf"
PYTEST_SKIP="ogr/ogr_ogdi.py $PYTEST_SKIP"

# Stalls on it. Probably not enough memory
PYTEST_SKIP="gdrivers/jp2openjpeg.py $PYTEST_SKIP"

# Failures for the following tests. See https://github.com/OSGeo/gdal/runs/1425843044

# depends on tiff_ovr.py that is going to be removed below
(cd autotest && $PYTEST utilities/test_gdaladdo.py)
PYTEST_SKIP="autotest/utilities/test_gdaladdo.py $PYTEST_SKIP"

for i in $PYTEST_XFAIL ; do
    (cd autotest && $PYTEST $i || echo "Ignoring failure")
done

(cd autotest && $PYTEST $(echo " $PYTEST_SKIP $PYTEST_XFAIL" | sed -r 's/[[:space:]]+/ --ignore=/g'))
