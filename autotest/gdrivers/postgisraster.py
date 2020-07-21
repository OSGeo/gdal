#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  PostGISRaster Testing.
# Author:   Jorge Arevalo <jorge.arevalo@libregis.org>
#           David Zwarg <dzwarg@azavea.com>
#
###############################################################################
# Copyright (c) 2009, Jorge Arevalo <jorge.arevalo@libregis.org>
#               2012, David Zwarg <dzwarg@azavea.com>
# Copyright (c) 2009-2012, Even Rouault <even dot rouault at spatialys.com>
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
###############################################################################

from osgeo import gdal
from osgeo import ogr
from osgeo import osr


import gdaltest
import pytest

#
# To initialize the required PostGISRaster DB instance, run data/load_postgisraster_test_data.sh
#


###############################################################################
#


def test_postgisraster_init():
    gdaltest.postgisrasterDriver = gdal.GetDriverByName('PostGISRaster')
    if gdaltest.postgisrasterDriver is None:
        pytest.skip()

    if gdal.GetConfigOption('APPVEYOR'):
        gdaltest.postgisrasterDriver = None
        pytest.skip()

    val = gdal.GetConfigOption('GDAL_PG_CONNECTION_STRING', None)
    if val is not None:
        gdaltest.pg_connection_string = val
    else:
        gdaltest.pg_connection_string = "host='localhost' dbname='autotest'"

    gdaltest.postgisraster_connection_string_without_schema = "PG:" + gdaltest.pg_connection_string
    gdaltest.postgisraster_connection_string = gdaltest.postgisraster_connection_string_without_schema

    # Make sure we have SRID=26711 in spatial_ref_sys
    with gdaltest.error_handler():
        ds = ogr.Open(gdaltest.postgisraster_connection_string, update=1)
    if ds is None:
        gdaltest.postgisrasterDriver = None
        pytest.skip()

    sr = osr.SpatialReference()
    sr.ImportFromEPSG(26711)
    wkt = sr.ExportToWkt()
    proj4 = sr.ExportToProj4()
    ds.ExecuteSQL("DELETE FROM spatial_ref_sys WHERE auth_srid = 26711")
    ds.ExecuteSQL("INSERT INTO spatial_ref_sys (srid,auth_name,auth_srid,srtext,proj4text) VALUES (26711,'EPSG',26711,'%s','%s')" % (wkt, proj4))
    ds.ExecuteSQL("ALTER DATABASE autotest SET postgis.gdal_enabled_drivers TO 'GTiff PNG JPEG'")
    ds.ExecuteSQL("ALTER DATABASE autotest SET postgis.enable_outdb_rasters = true")
    ds.ExecuteSQL("SELECT pg_reload_conf()")
    ds = None

    gdaltest.postgisraster_connection_string += " schema='gis_schema' "

    with gdaltest.error_handler():
        ds = gdal.Open(gdaltest.postgisraster_connection_string + "table='utm'")

    # If we cannot open the table, try force loading the data
    if ds is None:
        gdaltest.runexternal('bash data/load_postgisraster_test_data.sh')

        with gdaltest.error_handler():
            ds = gdal.Open(gdaltest.postgisraster_connection_string + "table='utm'")

    if ds is None:
        gdaltest.postgisrasterDriver = None

    if gdaltest.postgisrasterDriver is None:
        pytest.skip()

    
###############################################################################
#


def test_postgisraster_test_open_error1():
    if gdaltest.postgisrasterDriver is None:
        pytest.skip()

    with gdaltest.error_handler():
        ds = gdal.Open(gdaltest.postgisraster_connection_string +
                       "table='nonexistent'")
    if ds is None:
        return
    pytest.fail()

###############################################################################
#


def test_postgisraster_test_open_error2():
    if gdaltest.postgisrasterDriver is None:
        pytest.skip()

    # removed mode, as it defaults to one raster per row
    with gdaltest.error_handler():
        ds = gdal.Open(gdaltest.postgisraster_connection_string + "table='utm'")
    assert ds is not None

###############################################################################
#


def test_postgisraster_compare_utm():
    if gdaltest.postgisrasterDriver is None:
        pytest.skip()

    src_ds = gdal.Open('data/utm.tif')
    with gdaltest.error_handler():
        dst_ds = gdal.Open(gdaltest.postgisraster_connection_string + "table='utm'")

    # dataset actually contains many sub-datasets. test the first one
    dst_ds = gdal.Open(dst_ds.GetMetadata('SUBDATASETS')['SUBDATASET_1_NAME'])

    diff = gdaltest.compare_ds(src_ds, dst_ds, width=100, height=100, verbose=1)
    assert diff == 0

###############################################################################
#


def test_postgisraster_compare_small_world():
    if gdaltest.postgisrasterDriver is None:
        pytest.skip()

    src_ds = gdal.Open('data/small_world.tif')
    with gdaltest.error_handler():
        dst_ds = gdal.Open(gdaltest.postgisraster_connection_string + "table='small_world'")

    # dataset actually contains many sub-datasets. test the first one
    dst_ds = gdal.Open(dst_ds.GetMetadata('SUBDATASETS')['SUBDATASET_1_NAME'])

    diff = gdaltest.compare_ds(src_ds, dst_ds, width=40, height=20, verbose=1)
    assert diff == 0

###############################################################################
#


def test_postgisraster_test_utm_open():
    if gdaltest.postgisrasterDriver is None:
        pytest.skip()

    # First open tif file
    src_ds = gdal.Open('data/utm.tif')
    prj = src_ds.GetProjectionRef()
    gt = src_ds.GetGeoTransform()

    # Get band data
    rb = src_ds.GetRasterBand(1)
    rb.GetStatistics(0, 1)
    cs = rb.Checksum()

    with gdaltest.error_handler():
        main_ds = gdal.Open(gdaltest.postgisraster_connection_string + "table='utm'")

        # Try to open PostGISRaster with the same data than original tif file
        tst = gdaltest.GDALTest('PostGISRaster', main_ds.GetMetadata('SUBDATASETS')['SUBDATASET_1_NAME'], 1, cs, filename_absolute=1)
        return tst.testOpen(check_prj=prj, check_gt=gt, skip_checksum=True)

###############################################################################
#


def test_postgisraster_test_small_world_open_b1():
    if gdaltest.postgisrasterDriver is None:
        pytest.skip()

    # First open tif file
    src_ds = gdal.Open('data/small_world.tif')
    prj = src_ds.GetProjectionRef()
    gt = src_ds.GetGeoTransform()

    # Get band data
    rb = src_ds.GetRasterBand(1)
    rb.GetStatistics(0, 1)
    cs = rb.Checksum()

    with gdaltest.error_handler():
        main_ds = gdal.Open(gdaltest.postgisraster_connection_string + "table='small_world'")

        # Try to open PostGISRaster with the same data than original tif file
        tst = gdaltest.GDALTest('PostGISRaster', main_ds.GetMetadata('SUBDATASETS')['SUBDATASET_1_NAME'], 1, cs, filename_absolute=1)
        return tst.testOpen(check_prj=prj, check_gt=gt, skip_checksum=True)

###############################################################################
#


def test_postgisraster_test_small_world_open_b2():
    if gdaltest.postgisrasterDriver is None:
        pytest.skip()

    # First open tif file
    src_ds = gdal.Open('data/small_world.tif')
    prj = src_ds.GetProjectionRef()
    gt = src_ds.GetGeoTransform()

    # Get band data
    rb = src_ds.GetRasterBand(2)
    rb.GetStatistics(0, 1)
    cs = rb.Checksum()

    with gdaltest.error_handler():
        main_ds = gdal.Open(gdaltest.postgisraster_connection_string + "table='small_world'")

        # Try to open PostGISRaster with the same data than original tif file
        tst = gdaltest.GDALTest('PostGISRaster', main_ds.GetMetadata('SUBDATASETS')['SUBDATASET_1_NAME'], 2, cs, filename_absolute=1)
        return tst.testOpen(check_prj=prj, check_gt=gt, skip_checksum=True)

###############################################################################
#


def test_postgisraster_test_small_world_open_b3():
    if gdaltest.postgisrasterDriver is None:
        pytest.skip()

    # First open tif file
    src_ds = gdal.Open('data/small_world.tif')
    prj = src_ds.GetProjectionRef()
    gt = src_ds.GetGeoTransform()

    # Get band data
    rb = src_ds.GetRasterBand(3)
    rb.GetStatistics(0, 1)
    cs = rb.Checksum()

    with gdaltest.error_handler():
        main_ds = gdal.Open(gdaltest.postgisraster_connection_string + "table='small_world'")

        # Checksum for each band can be obtained by gdalinfo -checksum <file>
        tst = gdaltest.GDALTest('PostGISRaster', main_ds.GetMetadata('SUBDATASETS')['SUBDATASET_1_NAME'], 3, cs, filename_absolute=1)

        return tst.testOpen(check_prj=prj, check_gt=gt, skip_checksum=True)


def test_postgisraster_test_create_copy_bad_conn_string():
    if gdaltest.postgisrasterDriver is None:
        pytest.skip()

    with gdaltest.error_handler():
        src_ds = gdal.Open(gdaltest.postgisraster_connection_string + "table='small_world'")

        new_ds = gdaltest.postgisrasterDriver.CreateCopy("bogus connection string", src_ds, strict=True)

        assert new_ds is None


def test_postgisraster_test_create_copy_no_dbname():
    if gdaltest.postgisrasterDriver is None:
        pytest.skip()

    with gdaltest.error_handler():
        src_ds = gdal.Open(gdaltest.postgisraster_connection_string + "table='small_world'")

        # This is set in order to prevent GDAL from attempting to auto-identify
        # a bogus PG: filename to the postgis raster driver
        options = ['APPEND_SUBDATASET=YES']

        new_ds = gdaltest.postgisrasterDriver.CreateCopy("PG: no database name", src_ds, strict=True, options=options)

        assert new_ds is None


def test_postgisraster_test_create_copy_no_tablename():
    if gdaltest.postgisrasterDriver is None:
        pytest.skip()

    with gdaltest.error_handler():
        src_ds = gdal.Open(gdaltest.postgisraster_connection_string + "table='small_world'")

        # This is set in order to prevent GDAL from attempting to auto-identify
        # a bogus PG: filename to the postgis raster driver
        options = ['APPEND_SUBDATASET=YES']

        new_ds = gdaltest.postgisrasterDriver.CreateCopy(gdaltest.postgisraster_connection_string, src_ds, strict=True, options=options)

        assert new_ds is None


def test_postgisraster_test_create_copy_and_delete():
    """
    Test the "CreateCopy" implementation. What to do when we're done?
    Why, test "Delete", of course!
    """
    if gdaltest.postgisrasterDriver is None:
        pytest.skip()

    with gdaltest.error_handler():
        src_ds = gdal.Open(gdaltest.postgisraster_connection_string + "table='small_world'")

        new_ds = gdaltest.postgisrasterDriver.CreateCopy(gdaltest.postgisraster_connection_string + "table='small_world_copy'", src_ds, strict=True)

        assert new_ds is not None

    deleted = gdaltest.postgisrasterDriver.Delete(gdaltest.postgisraster_connection_string + "table='small_world_copy'")

    assert not deleted


def test_postgisraster_test_create_copy_and_delete_phases():
    """
    Create a copy of the dataset, then delete it in phases.
    """
    if gdaltest.postgisrasterDriver is None:
        pytest.skip()

    with gdaltest.error_handler():
        src_ds = gdal.Open(gdaltest.postgisraster_connection_string + "table='small_world'")

        src_md = src_ds.GetMetadata('SUBDATASETS').keys()

        new_ds = gdaltest.postgisrasterDriver.CreateCopy(gdaltest.postgisraster_connection_string + "table='small_world_copy'", src_ds, strict=True)

        new_md = new_ds.GetMetadata('SUBDATASETS').keys()

        # done with src
        src_ds = None

        if new_ds is None:
            pytest.fail('No new dataset was created during copy.')
        elif len(src_md) != len(new_md):
            pytest.fail('Metadata differs between new and old rasters.')

        ds = gdal.Open(gdaltest.postgisraster_connection_string + "table='small_world_copy' mode=2")
        cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(3)]
        assert cs == [30111, 32302, 40026]
        ds = None

        # should delete all raster parts over 50
        deleted = gdaltest.postgisrasterDriver.Delete(gdaltest.postgisraster_connection_string + "table='small_world_copy' where='rid>50'")

        assert not deleted, 'Delete returned an error.'

        src_ds = gdal.Open(gdaltest.postgisraster_connection_string + "table='small_world_copy'")

        src_md = src_ds.GetMetadata('SUBDATASETS').keys()

        if src_ds is None:
            pytest.fail('Could not open reduced dataset (1).')
        elif len(src_md) != 100:
            # The length of the metadata contains two pcs of
            # information per raster, so 50 rasters remaining = 100 keys
            print(len(src_md))
            pytest.fail(
                'Expected 100 keys of metadata for 50 subdataset rasters.')

        # done with src
        src_ds = None

        deleted = gdaltest.postgisrasterDriver.Delete(gdaltest.postgisraster_connection_string + "table='small_world_copy' where='rid<=25'")

        assert not deleted, 'Delete returned an error.'

        src_ds = gdal.Open(gdaltest.postgisraster_connection_string + "table='small_world_copy'")

        src_md = src_ds.GetMetadata('SUBDATASETS').keys()

        if src_ds is None:
            pytest.fail('Could not open reduced dataset (2).')
        elif len(src_md) != 50:
            # The length of the metadata contains two pcs of
            # information per raster, so 25 rasters remaining = 50 keys
            print(len(src_md))
            pytest.fail('Expected 50 keys of metadata for 25 subdataset rasters.')

        # done with src
        src_ds = None

        deleted = gdaltest.postgisrasterDriver.Delete(gdaltest.postgisraster_connection_string + "table='small_world_copy'")

        assert not deleted, 'Delete returned an error.'

    

def test_postgisraster_test_norid():
    """
    Test the ability to connect to a data source if it has no 'rid' column.
    """
    if gdaltest.postgisrasterDriver is None:
        pytest.skip()

    with gdaltest.error_handler():
        src_ds = gdal.Open(gdaltest.postgisraster_connection_string + "table='small_world_noid'")

    src_md = src_ds.GetMetadata('SUBDATASETS')

    # Check each subdataset
    for k in src_md.keys():
        if k[-4:] == 'NAME':
            # Ensure the subdataset has upperleftx and upperlefty coords,
            # as there is no unique key on the table
            assert src_md[k].find('ST_UpperLeftX') >= 0 and src_md[k].find('ST_UpperLeftY') >= 0

    with gdaltest.error_handler():
        ds = gdal.Open(gdaltest.postgisraster_connection_string + "table='small_world_noid' mode=2")
    cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(3)]
    assert cs == [30111, 32302, 40026]


def test_postgisraster_test_serial():
    """
    Test the ability to connect to a data source if it has no primary key,
    but uses a sequence instead.
    """
    if gdaltest.postgisrasterDriver is None:
        pytest.skip()

    with gdaltest.error_handler():
        src_ds = gdal.Open(gdaltest.postgisraster_connection_string + "table='small_world_serial'")

    src_md = src_ds.GetMetadata('SUBDATASETS')

    import re

    # Check each subdataset
    for k in src_md.keys():
        if k[-4:] == 'NAME':
            # Ensure the subdataset has upperleftx and upperlefty coords,
            # as there is no unique key on the table
            assert re.search(r"""where='"serialid" = \d+'""", src_md[k]), (k, ':', src_md[k])

    with gdaltest.error_handler():
        ds = gdal.Open(gdaltest.postgisraster_connection_string + "table='small_world_serial' mode=2")
    cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(3)]
    assert cs == [30111, 32302, 40026]


def test_postgisraster_test_unique():
    """
    Test the ability to connect to a data source if it has no primary key,
    but uses a unique constraint instead.
    """
    if gdaltest.postgisrasterDriver is None:
        pytest.skip()

    with gdaltest.error_handler():
        src_ds = gdal.Open(gdaltest.postgisraster_connection_string + "table='small_world_unique'")

    src_md = src_ds.GetMetadata('SUBDATASETS')

    import re

    # Check each subdataset
    for k in src_md.keys():
        if k[-4:] == 'NAME':
            # Ensure the subdataset has upperleftx and upperlefty coords,
            # as there is no unique key on the table
            assert re.search(r"""where='"uniq" = \d+'""", src_md[k]), (k, ':', src_md[k])

    with gdaltest.error_handler():
        ds = gdal.Open(gdaltest.postgisraster_connection_string + "table='small_world_unique' mode=2")
    cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(3)]
    assert cs == [30111, 32302, 40026]


def test_postgisraster_test_constraint():

    if gdaltest.postgisrasterDriver is None:
        pytest.skip()
    ds = gdal.Open(gdaltest.postgisraster_connection_string + "table='small_world_constraint' mode=2")
    cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(3)]
    assert cs == [30111, 32302, 40026]


def test_postgisraster_test_constraint_with_spi():

    if gdaltest.postgisrasterDriver is None:
        pytest.skip()
    ds = gdal.Open(gdaltest.postgisraster_connection_string + "table='small_world_constraint_with_spi' mode=2")
    cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(3)]
    assert cs == [30111, 32302, 40026]


def test_postgisraster_test_outdb():

    if gdaltest.postgisrasterDriver is None:
        pytest.skip()

    # For some reason fails with
    # ERROR 1: PostGISRasterRasterBand::IRasterIO(): ERROR:  rt_band_load_offline_data: Cannot open offline raster: /home/travis/build/rouault/gdal/autotest/gdrivers/data/small_world.tif
    # See https://api.travis-ci.org/v3/job/428972866/log.txt
    if gdaltest.is_travis_branch('ubuntu_1804'):
        pytest.skip()

    ds = ogr.Open(gdaltest.postgisraster_connection_string_without_schema)
    sql_lyr = ds.ExecuteSQL('SHOW postgis.enable_outdb_rasters')
    has_guc = sql_lyr is not None
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    ds = gdal.Open(gdaltest.postgisraster_connection_string + "table='small_world_outdb_constraint' mode=2")
    cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(3)]
    expected_cs = [30111, 32302, 40026] if has_guc else [0, 0, 0]
    assert cs == expected_cs


def test_postgisraster_test_outdb_client_side():

    if gdaltest.postgisrasterDriver is None:
        pytest.skip()
    ds = gdal.Open(gdaltest.postgisraster_connection_string + "table='small_world_outdb_constraint' mode=2 outdb_resolution=client_side")
    cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(3)]
    assert cs == [30111, 32302, 40026]


def test_postgisraster_test_outdb_client_side_force_ireadblock():

    if gdaltest.postgisrasterDriver is None:
        pytest.skip()
    ds = gdal.Open(gdaltest.postgisraster_connection_string + "table='small_world_outdb_constraint' mode=2 outdb_resolution=client_side")
    with gdaltest.SetCacheMax(0):
        cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(3)]
    assert cs == [30111, 32302, 40026]


def test_postgisraster_test_outdb_client_side_if_possible():

    if gdaltest.postgisrasterDriver is None:
        pytest.skip()

    # For some reason fails with
    # ERROR 1: PostGISRasterRasterBand::IRasterIO(): ERROR:  rt_band_load_offline_data: Cannot open offline raster: /home/travis/build/rouault/gdal/autotest/gdrivers/data/small_world.tif
    # See https://api.travis-ci.org/v3/job/484385907/log.txt
    if gdaltest.is_travis_branch('ubuntu_1804'):
        pytest.skip()

    ds = gdal.Open(gdaltest.postgisraster_connection_string + "table='small_world_outdb_constraint' mode=2 outdb_resolution=client_side_if_possible")
    cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(3)]
    assert cs == [30111, 32302, 40026]


def test_postgisraster_cleanup():

    gdal.Unlink('data/small_world.tif.aux.xml')
    gdal.Unlink('data/utm.tif.aux.xml')



