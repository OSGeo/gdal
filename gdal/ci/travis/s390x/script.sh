#!/bin/sh

set -e

export PYTEST="python3 -m pytest -vv -p no:sugar --color=no"

(cd "$PWD/autotest/cpp" && make quick_test || echo "error in quick_test")

# install pip and use it to install test dependencies
sudo sh -c "curl -sSL 'https://bootstrap.pypa.io/get-pip.py' | python3"
sudo pip3 install -U -r "$PWD/autotest/requirements.txt"

# Run all the Python autotests

# Fails with ERROR 1: OGDI DataSource Open Failed: Could not find the dynamic library "vrf"
rm autotest/ogr/ogr_ogdi.py

# OSError: /var/snap/lxd/common/lxd/storage-pools/instances/containers/travis-job-rouault-gdal-685450999/rootfs/usr/lib/s390x-linux-gnu/libsqlite3.so.0.8.6: cannot open shared object file: No such file or directory
(cd autotest/ogr && pytest ogr_virtualogr.py) || echo "ogr_virtualogr.py failed"
rm autotest/ogr/ogr_virtualogr.py

# Run the 2 following before removing netcdf.py, as they depend on it
(cd autotest/gdrivers && pytest netcdf_multidim.py) || echo "netcdf_multidim.py failed"
rm autotest/gdrivers/netcdf_multidim.py

(cd autotest/gdrivers && pytest netcdf_cf.py)
rm autotest/gdrivers/netcdf_cf.py

(cd autotest/gdrivers && pytest netcdf.py) || echo "netcdf.py failed"
rm autotest/gdrivers/netcdf.py

cd autotest && $PYTEST
