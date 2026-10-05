#!/bin/bash

set -eu

source ${GDAL_SOURCE_DIR:=..}/scripts/setdevenv.sh

autotest/cpp/gdal_unit_test --gtest_filter=-test_cpl.CPLSpawn:test_cpl.CPLGetCurrentThreadCount

# Random failures
rm -f autotest/gcore/vsiaz_real_instance_auto.py
rm -f autotest/gcore/vsiaz.py
rm -f autotest/gcore/vsigs.py
rm -f autotest/gcore/vsis3.py
rm -f autotest/gcore/vsizip.py
rm -f autotest/gcore/vsioss.py

# SIGABRT in VSIFOpenExL() on any HTTP connection attempt
rm -f autotest/gcore/vsiswift.py

pytest autotest/alg -k "not test_warp_52 and not test_warp_rpc_source_has_geotransform"

GCORE_K="not transformer"
GCORE_K+=" and not virtualmem"
GCORE_K+=" and not test_vrt_protocol_netcdf_component_name"
GCORE_K+=" and not test_vsicrypt_3"
GCORE_K+=" and not test_pixfun_sqrt"
GCORE_K+=" and not test_rasterio_rms_halfsize_downsampling_float"
# Excluded tests starting at test_tiff_read_multi_threaded are due to lack of virtual memory
GCORE_K+=" and not test_tiff_read_multi_threaded"
GCORE_K+=" and not test_tiff_write_35"
GCORE_K+=" and not test_tiff_write_137"
GCORE_K+=" and not test_tiff_write_compression_create_and_createcopy"
# Hangs in VSIFWriteL()
GCORE_K+=" and not test_vsigzip_multi_thread"

pytest autotest/gcore -o faulthandler_timeout=300 -k "$GCORE_K"
pytest autotest/gdrivers/zarr*.py

