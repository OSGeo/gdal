#!/bin/bash

set -eu

source ${GDAL_SOURCE_DIR:=..}/scripts/setdevenv.sh

autotest/cpp/gdal_unit_test --gtest_filter=-test_cpl.CPLSM_signed:test_cpl.CPLSpawn:test_cpl.CPLUTF8ForceToASCII:test_cpl.CPLGetCurrentThreadCount

pytest autotest/alg
pytest autotest/gcore -k "not transformer and not virtualmem and not test_vrt_protocol_netcdf_component_name and not test_vsicrypt_3"
pytest autotest/gdrivers/zarr*.py

