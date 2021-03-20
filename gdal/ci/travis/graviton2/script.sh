#!/bin/sh

set -e

export PYTEST="python3 -m pytest -vv -p no:sugar --color=no"

(cd "$PWD/autotest/cpp" && make quick_test || echo "error in quick_test")

# install pip and use it to install test dependencies
sudo sh -c "curl -sSL 'https://bootstrap.pypa.io/get-pip.py' | python3"
sudo pip3 install -U -r "$PWD/autotest/requirements.txt"

# Run all the Python autotests

# OSError: /var/snap/lxd/common/lxd/storage-pools/instances/containers/travis-job-osgeo-gdal-494090391/rootfs/usr/lib/aarch64-linux-gnu/libsqlite3.so.0.8.6: cannot open shared object file: No such file or directory
(cd autotest/ogr && pytest ogr_virtualogr.py) || echo "ogr_virtualogr.py failed"
rm autotest/ogr/ogr_virtualogr.py

cd autotest && $PYTEST
