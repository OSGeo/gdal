#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for PDS driver.
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
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

import os
from osgeo import gdal
from osgeo import osr


import gdaltest
import pytest

###############################################################################
# Read a truncated and modified version of http://download.osgeo.org/gdal/data/pds/mc02.img


def test_pds_1():

    tst = gdaltest.GDALTest('PDS', 'pds/mc02_truncated.img', 1, 47151)
    expected_prj = """PROJCS["SIMPLE_CYLINDRICAL MARS",GEOGCS["GCS_MARS",DATUM["D_MARS",SPHEROID[""MARS"",3396000,0]],PRIMEM["Reference_Meridian",0],UNIT["degree",0.0174532925199433]],PROJECTION["Equirectangular"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",0],PARAMETER["false_easting",0],PARAMETER["false_northing",0],PARAMETER["pseudo_standard_parallel_1",0],UNIT["metre",1]]"""
    expected_gt = (-10668384.903788566589355, 926.115274429321289, 0, 3852176.483988761901855, 0, -926.115274429321289)
    gdal.SetConfigOption('PDS_SampleProjOffset_Shift', '-0.5')
    gdal.SetConfigOption('PDS_LineProjOffset_Shift', '-0.5')
    tst.testOpen(check_prj=expected_prj,
                       check_gt=expected_gt)
    gdal.SetConfigOption('PDS_SampleProjOffset_Shift', None)
    gdal.SetConfigOption('PDS_LineProjOffset_Shift', None)


###############################################################################
# Read a truncated and modified version of ftp://pdsimage2.wr.usgs.gov/cdroms/magellan/mg_1103/fl78n018/fl73n003.img

def test_pds_2():

    tst = gdaltest.GDALTest('PDS', 'pds/fl73n003_truncated.img', 1, 34962)
    expected_prj = """PROJCS["SINUSOIDAL VENUS",
    GEOGCS["GCS_VENUS",
        DATUM["D_VENUS",
            SPHEROID["VENUS",6051000,0]],
        PRIMEM["Reference_Meridian",0],
        UNIT["degree",0.0174532925199433]],
    PROJECTION["Sinusoidal"],
    PARAMETER["longitude_of_center",18],
    PARAMETER["false_easting",0],
    PARAMETER["false_northing",0],UNIT["metre",1]]"""
    expected_gt = (587861.55900404998, 75.000002980232239, 0.0, -7815243.4746123618, 0.0, -75.000002980232239)
    gdal.SetConfigOption('PDS_SampleProjOffset_Shift', '-0.5')
    gdal.SetConfigOption('PDS_LineProjOffset_Shift', '-0.5')
    tst.testOpen(check_prj=expected_prj,
                       check_gt=expected_gt)
    gdal.SetConfigOption('PDS_SampleProjOffset_Shift', None)
    gdal.SetConfigOption('PDS_LineProjOffset_Shift', None)

    ds = gdal.Open('data/pds/fl73n003_truncated.img')
    assert ds.GetRasterBand(1).GetNoDataValue() == 7
    assert ds.GetRasterBand(1).GetScale() == 0.2
    assert ds.GetRasterBand(1).GetOffset() == -20.2

    # Per #3939 we would also like to test a dataset with MISSING_CONSTANT.
    ds = gdal.Open('data/pds/fl73n003_alt_truncated.img')
    assert ds.GetRasterBand(1).GetNoDataValue() == 7

###############################################################################
# Read a truncated and modified version of ftp://pdsimage2.wr.usgs.gov/cdroms/messenger/MSGRMDS_1001/DATA/2004_232/EN0001426030M.IMG
# 16bits image


def test_pds_3():

    # Shut down warning about missing projection
    gdal.PushErrorHandler('CPLQuietErrorHandler')

    tst = gdaltest.GDALTest('PDS', 'pds/EN0001426030M_truncated.IMG', 1, 1367)

    gt_expected = (0, 1, 0, 0, 0, 1)
    tst.testOpen(check_gt=gt_expected)

    ds = gdal.Open('data/pds/EN0001426030M_truncated.IMG')
    assert ds.GetRasterBand(1).GetNoDataValue() == 0

    gdal.PopErrorHandler()

###############################################################################
# Read a hacked example of reading a detached file with an offset #3177.


def test_pds_4():

    tst = gdaltest.GDALTest('PDS', 'pds/pds_3177.lbl', 1, 3418)
    gt_expected = (6119184.3590369327, 1.0113804322107001, 0.0, -549696.39009125973, 0.0, -1.0113804322107001)
    gdal.SetConfigOption('PDS_SampleProjOffset_Shift', '-0.5')
    gdal.SetConfigOption('PDS_LineProjOffset_Shift', '-0.5')
    tst.testOpen(check_gt=gt_expected)
    gdal.SetConfigOption('PDS_SampleProjOffset_Shift', None)
    gdal.SetConfigOption('PDS_LineProjOffset_Shift', None)

###############################################################################
# Read a hacked example of reading a detached file with an offset #3355.


def test_pds_5():

    tst = gdaltest.GDALTest('PDS', 'pds/pds_3355.lbl', 1, 2748)
    return tst.testOpen()

###############################################################################
# Read an image via the PDS label.  This is a distinct mode of the PDS
# driver mostly intended to support jpeg2000 files with PDS labels.


def test_pds_6():

    if os.path.exists('data/byte.tif.aux.xml'):
        os.unlink('data/byte.tif.aux.xml')

    tst = gdaltest.GDALTest('PDS', 'pds/ESP_013951_1955_RED.LBL', 1, 4672)

    gt_expected = (-6139197.5, 0.5, 0.0, 936003.0, 0.0, -0.5)

    gdal.SetConfigOption('PDS_SampleProjOffset_Shift', '-0.5')
    gdal.SetConfigOption('PDS_LineProjOffset_Shift', '-0.5')
    tst.testOpen(check_gt=gt_expected)
    gdal.SetConfigOption('PDS_SampleProjOffset_Shift', None)
    gdal.SetConfigOption('PDS_LineProjOffset_Shift', None)

    ds = gdal.Open('data/pds/ESP_013951_1955_RED.LBL')

    assert len(ds.GetFileList()) == 2, 'failed to get expected file list.'

    expected_wkt = 'PROJCS["EQUIRECTANGULAR MARS",GEOGCS["GCS_MARS",DATUM["D_MARS",SPHEROID["MARS_localRadius",3394839.8133163,0]],PRIMEM["Reference_Meridian",0],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]]],PROJECTION["Equirectangular"],PARAMETER["standard_parallel_1",15],PARAMETER["central_meridian",180],PARAMETER["false_easting",0],PARAMETER["false_northing",0],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Easting",EAST],AXIS["Northing",NORTH]]'
    wkt = ds.GetProjection()
    if expected_wkt != wkt:
        print('Got: ', wkt)
        print('Exp: ', expected_wkt)
        pytest.fail('did not get expected coordinate system.')

    
###############################################################################
# Read an uncompressed image via the PDS label. (#3943)


def test_pds_7():

    tst = gdaltest.GDALTest('PDS', 'pds/LDEM_4.LBL', 1, 50938,
                            0, 0, 1440, 2)
    gt_expected = (-5450622.3254203796, 7580.8377265930176, 0.0, 2721520.7438468933, 0.0, -7580.8377265930176)
    prj_expected = """PROJCS["SIMPLE_CYLINDRICAL MOON",
    GEOGCS["GCS_MOON",
        DATUM["D_MOON",
            SPHEROID["MOON",1737400,0]],
        PRIMEM["Reference_Meridian",0],
        UNIT["degree",0.0174532925199433]],
    PROJECTION["Equirectangular"],
    PARAMETER["latitude_of_origin",0],
    PARAMETER["central_meridian",180],
    PARAMETER["standard_parallel_1",0],
    PARAMETER["false_easting",0],
    PARAMETER["false_northing",0],UNIT["metre",1]]"""

    gdal.SetConfigOption('PDS_SampleProjOffset_Shift', '-0.5')
    gdal.SetConfigOption('PDS_LineProjOffset_Shift', '-0.5')
    tst.testOpen(check_prj=prj_expected,
                       check_gt=gt_expected)
    gdal.SetConfigOption('PDS_SampleProjOffset_Shift', None)
    gdal.SetConfigOption('PDS_LineProjOffset_Shift', None)

###############################################################################
# Test applying adjustment offsets via configuration variables for the
# geotransform (#3940)


def test_pds_8():

    # values for MAGELLAN FMAP data.
    gdal.SetConfigOption('PDS_SampleProjOffset_Shift', '1.5')
    gdal.SetConfigOption('PDS_LineProjOffset_Shift', '1.5')
    gdal.SetConfigOption('PDS_SampleProjOffset_Mult', '1.0')
    gdal.SetConfigOption('PDS_LineProjOffset_Mult', '-1.0')

    tst = gdaltest.GDALTest('PDS', 'pds/mc02_truncated.img', 1, 47151)

    expected_gt = (10670237.134337425, 926.11527442932129, 0.0, -3854028.7145376205, 0.0, -926.11527442932129)

    result = tst.testOpen(check_gt=expected_gt)

    # clear config settings
    gdal.SetConfigOption('PDS_SampleProjOffset_Shift', None)
    gdal.SetConfigOption('PDS_LineProjOffset_Shift', None)
    gdal.SetConfigOption('PDS_SampleProjOffset_Mult', None)
    gdal.SetConfigOption('PDS_LineProjOffset_Mult', None)

    return result

###############################################################################
# Test a PDS with an image compressed in a ZIP, and with nodata expressed as
# an hexadecimal floating point value (#3939)


def test_pds_9():

    # Derived from http://pdsimage.wr.usgs.gov/data/co-v_e_j_s-radar-3-sbdr-v1.0/CORADR_0035/DATA/BIDR/BIEQI49N071_D035_T00AS01_V02.LBL
    tst = gdaltest.GDALTest('PDS', 'pds/PDS_WITH_ZIP_IMG.LBL', 1, 0)

    tst.testOpen()

    ds = gdal.Open('data/pds/PDS_WITH_ZIP_IMG.LBL')
    got_nd = ds.GetRasterBand(1).GetNoDataValue()
    expected_nd = -3.40282265508890445e+38
    assert abs((got_nd - expected_nd) / expected_nd) <= 1e-5

    assert ds.GetProjectionRef()

###############################################################################
# Test PDS label with nested arrays (#6970)


def test_pds_10():

    gdal.FileFromMemBuffer('/vsimem/pds_10',
                           """PDS_VERSION_ID                       = "PDS3"
DATA_FORMAT                          = "PDS"
^IMAGE                               = 1 <BYTES>

# Non sensical but just to parse nested arrays
NOTE                                 = ((1, 2, 3))
PRODUCT_ID                           = ({1, 2}, {3,4})

OBJECT                               = IMAGE
    BANDS                            = 1
    BAND_STORAGE_TYPE                = "BAND SEQUENTIAL"
    LINES                            = 1
    LINE_SAMPLES                     = 1
    SAMPLE_BITS                      = 8

END_OBJECT                           = IMAGE

END
""")

    ds = gdal.Open('/vsimem/pds_10')

    assert ds.GetMetadataItem('NOTE') == '((1,2,3))'

    assert ds.GetMetadataItem('PRODUCT_ID') == '({1,2},{3,4})', \
        ds.GetMetadataItem('NOTE')

    gdal.FileFromMemBuffer('/vsimem/pds_10',
                           """PDS_VERSION_ID                       = "PDS3"
# Unpaired
NOTE                                 = (x, y}
END
""")

    with gdaltest.error_handler():
        gdal.Open('/vsimem/pds_10')

    gdal.FileFromMemBuffer('/vsimem/pds_10',
                           """PDS_VERSION_ID                       = "PDS3"
# Unpaired
NOTE                                 = {x, y)
END
""")

    with gdaltest.error_handler():
        gdal.Open('/vsimem/pds_10')

    gdal.Unlink('/vsimem/pds_10')

###############################################################################
# Read a hacked example of reading an image where the line offset is not
# a multiple of the record size
# https://github.com/OSGeo/gdal/issues/955


def test_pds_line_offset_not_multiple_of_record():

    tst = gdaltest.GDALTest('PDS', 'pds/map_000_038_truncated.lbl', 1, 14019)
    return tst.testOpen()


###############################################################################
# Read http://pds-geosciences.wustl.edu/mro/mro-m-crism-3-rdr-targeted-v1/mrocr_2104/trdr/2010/2010_095/hsp00017ba0/hsp00017ba0_01_ra218s_trr3.lbl
# Test ability of using OBJECT = FILE section to support CRISM
# as well as BAND_STORAGE_TYPE = LINE_INTERLEAVED

def test_pds_band_storage_type_line_interleaved():

    tst = gdaltest.GDALTest('PDS', 'pds/hsp00017ba0_01_ra218s_trr3_truncated.lbl', 1, 64740)
    return tst.testOpen()



def test_pds_oblique_cylindrical_read():

    # This dataset is a champion in its category. It features:
    # - POSITIVE_LONGITUDE_DIRECTION = WEST
    # - MAP_PROJECTION_ROTATION      = 90.0
    # - oblique cylindrical projection

    # https://pds-imaging.jpl.nasa.gov/data/cassini/cassini_orbiter/CORADR_0101_V03/DATA/BIDR/BIBQH03N123_D101_T020S03_V03.LBL
    ds = gdal.Open('data/pds/BIBQH03N123_D101_T020S03_V03_truncated.IMG')
    srs = ds.GetSpatialRef()
    assert srs.ExportToProj4() == '+proj=ob_tran +R=2575000 +o_proj=eqc +o_lon_p=-257.744003 +o_lat_p=120.374532 +lon_0=-303.571748 +wktext +no_defs'
    gt = ds.GetGeoTransform()
    assert gt == pytest.approx((-5347774.07796, 0, 351.11116, -2561707.02336, 351.11116, 0))

    geog_srs = srs.CloneGeogCS()
    ct = osr.CoordinateTransformation(srs, geog_srs)

    def to_lon_lat(pixel, line):
        x = gt[0] + pixel * gt[1] + line * gt[2]
        y = gt[3] + pixel * gt[4] + line * gt[5]
        lon, lat, _ = ct.TransformPoint(x, y)
        return lon, lat

    # Check consistency of the corners of the image with the long,lat bounds
    # in the metadata

    # MAXIMUM_LATITUDE             = 32.37062573<DEG>
    # MINIMUM_LATITUDE             = -31.41702033<DEG>
    # EASTERNMOST_LONGITUDE        = 75.792673220<DEG>
    # WESTERNMOST_LONGITUDE        = 169.8235459<DEG>

    _, lat = to_lon_lat(0, 0)
    assert lat == pytest.approx(-31.097321393323572) # MINIMUM_LATITUDE

    lon, _ = to_lon_lat(ds.RasterXSize, 0)
    assert lon == pytest.approx(-169.8290961385244) # WESTERNMOST_LONGITUDE * -1

    _, lat = to_lon_lat(0, ds.RasterYSize)
    assert lat == pytest.approx(-31.421452666874025) # MINIMUM_LATITUDE

    lon, _ = to_lon_lat(ds.RasterXSize, ds.RasterYSize)
    assert lon == pytest.approx(-75.787124149033) # EASTERNMOST_LONGITUDE * -1


###############################################################################

def test_pds_sharp_on_continuing_line():

    gdal.FileFromMemBuffer('/vsimem/test',
                           """PDS_VERSION_ID                       = "PDS3"

NOTE = (#9933FF,
        #FFFF33)

^IMAGE                               = 1 <BYTES>
OBJECT                               = IMAGE
    BANDS                            = 1
    BAND_STORAGE_TYPE                = "BAND SEQUENTIAL"
    LINES                            = 1
    LINE_SAMPLES                     = 1
    SAMPLE_BITS                      = 8

END_OBJECT                           = IMAGE

END
""")

    ds = gdal.Open('/vsimem/test')

    assert ds.GetMetadataItem('NOTE') == '(#9933FF,#FFFF33)'

    gdal.Unlink('/vsimem/test')


###############################################################################

def test_pds_sharp_comma_continuing_line():

    gdal.FileFromMemBuffer('/vsimem/test',
                           """PDS_VERSION_ID                       = "PDS3"

NOTE = ("a"
        ,"b")

^IMAGE                               = 1 <BYTES>
OBJECT                               = IMAGE
    BANDS                            = 1
    BAND_STORAGE_TYPE                = "BAND SEQUENTIAL"
    LINES                            = 1
    LINE_SAMPLES                     = 1
    SAMPLE_BITS                      = 8

END_OBJECT                           = IMAGE

END
""")

    ds = gdal.Open('/vsimem/test')

    assert ds.GetMetadataItem('NOTE') == '("a","b")'

    gdal.Unlink('/vsimem/test')


###############################################################################
# Test reading a Mercator_2SP dataset (#2490)

def test_pds_mercator_2SP():

    # Dataset from https://sbnarchive.psi.edu/pds3/dawn/fc/DWNCLCFC2_2/DATA/CE_LAMO_Q_00N_036E_MER_CLR.IMG
    ds = gdal.Open('data/pds/CE_LAMO_Q_00N_036E_MER_CLR_truncated.IMG')
    expected_wkt = """PROJCRS["MERCATOR 1_CERES",
    BASEGEOGCRS["GCS_1_CERES",
        DATUM["D_1_CERES",
            ELLIPSOID["1_CERES",470000,0,
                LENGTHUNIT["metre",1,
                    ID["EPSG",9001]]]],
        PRIMEM["Reference_Meridian",0,
            ANGLEUNIT["degree",0.0174532925199433,
                ID["EPSG",9122]]]],
    CONVERSION["unnamed",
        METHOD["Mercator (variant B)",
            ID["EPSG",9805]],
        PARAMETER["Latitude of 1st standard parallel",-12.99,
            ANGLEUNIT["degree",0.0174532925199433],
            ID["EPSG",8823]],
        PARAMETER["Longitude of natural origin",36,
            ANGLEUNIT["degree",0.0174532925199433],
            ID["EPSG",8802]],
        PARAMETER["False easting",0,
            LENGTHUNIT["metre",1],
            ID["EPSG",8806]],
        PARAMETER["False northing",0,
            LENGTHUNIT["metre",1],
            ID["EPSG",8807]]],
    CS[Cartesian,2],
        AXIS["easting",east,
            ORDER[1],
            LENGTHUNIT["metre",1,
                ID["EPSG",9001]]],
        AXIS["northing",north,
            ORDER[2],
            LENGTHUNIT["metre",1,
                ID["EPSG",9001]]]]"""
    expected_srs = osr.SpatialReference()
    expected_srs.ImportFromWkt(expected_wkt)
    srs = ds.GetSpatialRef()
    assert srs.IsSame(expected_srs), srs.ExportToWkt()
