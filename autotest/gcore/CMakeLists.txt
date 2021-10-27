# CMake4GDAL project is distributed under X/MIT license. See accompanying file LICENSE.txt.

foreach (
  test_target IN
  ITEMS aaigrid_read
        aaigrid_write
        asyncreader
        basic_test
        bmp_read
        bmp_write
        colortable
        envi_read
        gcps2geotransform
        gdal_api_proxy
        gdal_stats
        geoloc
        gtiff_write
        hdf4_read
        hdf4_write
        hfa_read
        hfa_rfc40
        hfa_srs
        hfa_write
        histogram
        identify
        mask
        minixml
        misc
        nodatamaskband
        numpy_rw
        overviewds
        pam
        pamproxydb
        pixfun
        pnm_read
        pnm_write
        rasterio
        rat
        rfc30
        testnonboundtoswig
        thread_test
        tiff_ovr
        tiff_read
        tiff_srs
        tiff_write
        transformer
        virtualmem
        vrt_read
        vrtmisc
        vsiaz
        vsicrypt
        vsicurl
        vsicurl_streaming
        vsifile
        vsigs
        vsihdfs
        vsioss
        vsis3
        vsistdin
        vsiswift
        vsiwebhdfs
        vsizip)
  add_test(NAME gcore_${test_target} COMMAND ${Python_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/${test_target}.py)
  set_property(TEST gcore_${test_target} PROPERTY LABELS gcore-test)
  set_autotest_properties(TESTS gcore_${test_target})
endforeach ()
foreach (
  test_target IN
  ITEMS gdal_api_proxy
        misc
        rasterio
        thread_test
        tiff_ovr
        tiff_read
        tiff_srs
        tiff_write
        virtualmem
        vsiaz
        vsicurl
        vsicurl_streaming
        vsigs
        vsioss
        vsis3
        vsiwebhdfs)
  set_property(TEST gcore_${test_target} PROPERTY TIMEOUT 20)
endforeach ()
set_property(TEST gcore_hdf4_read PROPERTY TIMEOUT 60)
