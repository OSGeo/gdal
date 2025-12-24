#!/bin/bash

set -eu

source ${GDAL_SOURCE_DIR:=..}/scripts/setdevenv.sh

autotest/cpp/gdal_unit_test --gtest_filter=-test_cpl.CPLSpawn:test_cpl.CPLGetCurrentThreadCount

# Random failures
rm -f autotest/gcore/vsiaz.py
rm -f autotest/gcore/vsigs.py
rm -f autotest/gcore/vsis3.py
rm -f autotest/gcore/vsizip.py
rm -f autotest/gcore/vsioss.py

pytest autotest/alg -k "not test_warp_52 and not test_warp_rpc_source_has_geotransform"
# Excluded tests starting at test_tiff_read_multi_threaded are due to lack of virtual memory
pytest autotest/gcore -k "not transformer and not virtualmem and not test_vrt_protocol_netcdf_component_name and not test_vsicrypt_3 and not test_pixfun_sqrt and not test_rasterio_rms_halfsize_downsampling_float and not test_tiff_read_multi_threaded and not test_tiff_write_35 and not test_tiff_write_137 and not test_tiff_write_compression_create_and_createcopy"
pytest autotest/gdrivers/zarr*.py

