#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test functioning of the PAM metadata support.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2009-2013, Even Rouault <even dot rouault at spatialys.com>
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
import shutil
import stat


import gdaltest
from osgeo import gdal
from osgeo import osr
import pytest

###############################################################################
# Check that we can read PAM metadata for existing PNM file.


def test_pam_1():

    gdaltest.pam_setting = gdal.GetConfigOption('GDAL_PAM_ENABLED', "NULL")
    gdal.SetConfigOption('GDAL_PAM_ENABLED', 'YES')

    ds = gdal.Open("data/byte.pnm")

    base_md = ds.GetMetadata()
    assert len(base_md) == 2 and base_md['other'] == 'red' and base_md['key'] == 'value', \
        'Default domain metadata missing'

    xml_md = ds.GetMetadata('xml:test')

    assert len(xml_md) == 1, 'xml:test metadata missing'

    assert isinstance(xml_md, list), 'xml:test metadata not returned as list.'

    expected_xml = """<?xml version="2.0"?>
<TestXML>Value</TestXML>
"""

    assert xml_md[0] == expected_xml, 'xml does not match'

###############################################################################
# Verify that we can write XML to a new file.


def test_pam_2():

    driver = gdal.GetDriverByName('PNM')
    ds = driver.Create('tmp/pam.pgm', 10, 10)
    band = ds.GetRasterBand(1)

    band.SetMetadata({'other': 'red', 'key': 'value'})

    expected_xml = """<?xml version="2.0"?>
<TestXML>Value</TestXML>
"""

    band.SetMetadata([expected_xml], 'xml:test')

    band.SetNoDataValue(100)

    ds = None

###############################################################################
# Check that we can read PAM metadata for existing PNM file.


def test_pam_3():

    ds = gdal.Open("tmp/pam.pgm")

    band = ds.GetRasterBand(1)
    base_md = band.GetMetadata()
    assert len(base_md) == 2 and base_md['other'] == 'red' and base_md['key'] == 'value', \
        'Default domain metadata missing'

    xml_md = band.GetMetadata('xml:test')

    assert len(xml_md) == 1, 'xml:test metadata missing'

    assert isinstance(xml_md, list), 'xml:test metadata not returned as list.'

    expected_xml = """<?xml version="2.0"?>
<TestXML>Value</TestXML>
"""

    assert xml_md[0] == expected_xml, 'xml does not match'

    assert band.GetNoDataValue() == 100, 'nodata not saved via pam'

    ds = None
    ds = gdal.Open('tmp/pam.pgm', gdal.GA_Update)
    assert ds.GetRasterBand(1).DeleteNoDataValue() == 0
    ds = None

    ds = gdal.Open('tmp/pam.pgm')
    assert ds.GetRasterBand(1).GetNoDataValue() is None, \
        'got nodata value whereas none was expected'

###############################################################################
# Check that PAM binary encoded nodata values work properly.
#


def test_pam_4():

    # Copy test dataset to tmp directory so that the .aux.xml file
    # won't be rewritten with the statistics in the master dataset.
    shutil.copyfile('data/mfftest.hdr.aux.xml', 'tmp/mfftest.hdr.aux.xml')
    shutil.copyfile('data/mfftest.hdr', 'tmp/mfftest.hdr')
    shutil.copyfile('data/mfftest.r00', 'tmp/mfftest.r00')

    ds = gdal.Open('tmp/mfftest.hdr')
    stats = ds.GetRasterBand(1).GetStatistics(0, 1)

    assert stats[0] == 0 and stats[1] == 4, \
        'Got wrong min/max, likely nodata not working?'

###############################################################################
# Verify that .aux files that don't match the configuration of the
# dependent file are not utilized. (#2471)
#


def test_pam_5():

    ds = gdal.Open('data/sasha.tif')
    filelist = ds.GetFileList()
    ds = None

    assert len(filelist) == 1, 'did not get expected file list.'

###############################################################################
# Verify we can read nodata values from .aux files (#2505)
#


def test_pam_6():

    ds = gdal.Open('data/f2r23.tif')
    assert ds.GetRasterBand(1).GetNoDataValue() == 0, \
        'did not get expected .aux sourced nodata.'
    ds = None

    assert not os.path.exists('data/f2r23.tif.aux.xml'), \
        'did not expect .aux.xml to be created.'

###############################################################################
# Verify we can create overviews on PNG with PAM disabled (#3693)
#


def test_pam_7():

    gdal.SetConfigOption('GDAL_PAM_ENABLED', 'NO')

    shutil.copyfile('data/stefan_full_rgba.png', 'tmp/stefan_full_rgba.png')
    ds = gdal.Open('tmp/stefan_full_rgba.png')
    ds.BuildOverviews('NEAR', [2])
    ds = None

    ds = gdal.Open('tmp/stefan_full_rgba.png')
    ovr_count = ds.GetRasterBand(1).GetOverviewCount()
    ds = None

    os.remove('tmp/stefan_full_rgba.png')
    os.remove('tmp/stefan_full_rgba.png.ovr')

    assert ovr_count == 1

###############################################################################
# Test that Band.SetDescription() goes through PAM (#3780)
#


def test_pam_8():

    gdal.SetConfigOption('GDAL_PAM_ENABLED', 'YES')

    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/pam_8.tif', 1, 1, 1)
    ds.GetRasterBand(1).SetDescription('foo')
    ds = None

    ds = gdal.Open('/vsimem/pam_8.tif')
    desc = ds.GetRasterBand(1).GetDescription()
    ds = None

    gdal.GetDriverByName('GTiff').Delete('/vsimem/pam_8.tif')

    assert desc == 'foo'

###############################################################################
# Test that we can retrieve projection from xml:ESRI domain
#


def test_pam_9():

    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/pam_9.tif', 1, 1, 1)
    ds = None

    f = gdal.VSIFOpenL('/vsimem/pam_9.tif.aux.xml', 'wb')
    content = """<PAMDataset>
  <Metadata domain="xml:ESRI" format="xml">
    <GeodataXform xsi:type="typens:IdentityXform" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:typens="http://www.esri.com/schemas/ArcGIS/9.2">
      <SpatialReference xsi:type="typens:ProjectedCoordinateSystem">
        <WKT>PROJCS[&quot;NAD_1983_UTM_Zone_14N&quot;,GEOGCS[&quot;GCS_North_American_1983&quot;,DATUM[&quot;D_North_American_1983&quot;,SPHEROID[&quot;GRS_1980&quot;,6378137.0,298.257222101]],PRIMEM[&quot;Greenwich&quot;,0.0],UNIT[&quot;Degree&quot;,0.0174532925199433]],PROJECTION[&quot;Transverse_Mercator&quot;],PARAMETER[&quot;false_easting&quot;,500000.0],PARAMETER[&quot;false_northing&quot;,0.0],PARAMETER[&quot;central_meridian&quot;,-99.0],PARAMETER[&quot;scale_factor&quot;,0.9996],PARAMETER[&quot;latitude_of_origin&quot;,0.0],UNIT[&quot;Meter&quot;,1.0]]</WKT>
        <HighPrecision>true</HighPrecision>
      </SpatialReference>
    </GeodataXform>
  </Metadata>
</PAMDataset>"""
    gdal.VSIFWriteL(content, 1, len(content), f)
    gdal.VSIFCloseL(f)

    ds = gdal.Open('/vsimem/pam_9.tif')
    wkt = ds.GetProjectionRef()
    ds = None

    gdal.GetDriverByName('GTiff').Delete('/vsimem/pam_9.tif')

    expected_wkt = """PROJCS["NAD83 / UTM zone 14N",GEOGCS["NAD83",DATUM["North_American_Datum_1983",SPHEROID["GRS 1980",6378137,298.257222101,AUTHORITY["EPSG","7019"]],AUTHORITY["EPSG","6269"]],PRIMEM["Greenwich",0],UNIT["Degree",0.0174532925199433]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",-99],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Easting",EAST],AXIS["Northing",NORTH]]"""

    assert wkt == expected_wkt

###############################################################################
# Test serializing and deserializing of various band metadata


def test_pam_10():

    src_ds = gdal.Open('data/testserialization.asc')
    ds = gdal.GetDriverByName('AAIGRID').CreateCopy('/vsimem/pam_10.asc', src_ds)
    src_ds = None
    ds = None

    ds = gdal.Open('/vsimem/pam_10.asc')

    gcps = ds.GetGCPs()
    assert len(gcps) == 2 and ds.GetGCPCount() == 2

    assert ds.GetGCPProjection().find("WGS 84") != -1

    assert (gcps[0].GCPPixel == 0 and gcps[0].GCPLine == 1 and \
       gcps[0].GCPX == 2 and gcps[0].GCPY == 3 and gcps[0].GCPZ == 4)

    assert (gcps[1].GCPPixel == 1 and gcps[1].GCPLine == 2 and \
       gcps[1].GCPX == 3 and gcps[1].GCPY == 4 and gcps[1].GCPZ == 5)

    band = ds.GetRasterBand(1)
    assert band.GetDescription() == 'MyDescription'

    assert band.GetUnitType() == 'MyUnit'

    assert band.GetOffset() == 1

    assert band.GetScale() == 2

    assert band.GetRasterColorInterpretation() == gdal.GCI_PaletteIndex

    assert band.GetCategoryNames() == ['Cat1', 'Cat2']

    ct = band.GetColorTable()
    assert ct.GetColorEntry(0) == (0, 0, 0, 255)
    assert ct.GetColorEntry(1) == (1, 1, 1, 255)

    assert band.GetMaximum() == 0

    assert band.GetMinimum() == 2

    assert band.GetMetadata() == {'STATISTICS_MEAN': '1', 'STATISTICS_MINIMUM': '2', 'STATISTICS_MAXIMUM': '0', 'STATISTICS_STDDEV': '3'}

    ds = None

    gdal.Unlink('/vsimem/pam_10.asc')
    gdal.Unlink('/vsimem/pam_10.asc.aux.xml')

###############################################################################
# Test PamProxyDb mechanism


def test_pam_11():

    # Create a read-only directory
    try:
        os.chmod('tmpdirreadonly', stat.S_IRUSR | stat.S_IWUSR | stat.S_IXUSR)
        shutil.rmtree('tmpdirreadonly')
    except OSError:
        pass
    os.mkdir('tmpdirreadonly')
    shutil.copy('data/byte.tif', 'tmpdirreadonly/byte.tif')

    # FIXME: how do we create a read-only dir on windows ?
    # The following has no effect
    os.chmod('tmpdirreadonly', stat.S_IRUSR | stat.S_IXUSR)

    # Test that the directory is really read-only
    try:
        f = open('tmpdirreadonly/test', 'w')
        if f is not None:
            f.close()
            pytest.skip()
    except IOError:
        pass

    # Compute statistics --> the saving as .aux.xml should fail
    ds = gdal.Open('tmpdirreadonly/byte.tif')
    stats = ds.GetRasterBand(1).ComputeStatistics(False)
    assert stats[0] == 74, 'did not get expected minimum'
    gdal.ErrorReset()
    with gdaltest.error_handler():
        ds = None
    error_msg = gdal.GetLastErrorMsg()
    assert error_msg.startswith('Unable to save auxiliary information'), \
        'warning was expected at that point'

    # Check that we actually have no saved statistics
    ds = gdal.Open('tmpdirreadonly/byte.tif')
    stats = ds.GetRasterBand(1).GetStatistics(False, False)
    assert stats[3] == -1, 'did not expected to have stats at that point'
    ds = None

    # This must be run as an external process so we can override GDAL_PAM_PROXY_DIR
    # at the beginning of the process
    import test_py_scripts
    ret = test_py_scripts.run_py_script_as_external_script('.', 'pamproxydb', '-test1')
    assert ret.find('success') != -1, ('pamproxydb.py -test1 failed %s' % ret)

    # Test loading an existing proxydb
    ret = test_py_scripts.run_py_script_as_external_script('.', 'pamproxydb', '-test2')
    assert ret.find('success') != -1, ('pamproxydb.py -test2 failed %s' % ret)

###############################################################################
# Test histogram with 64bit counts


def test_pam_12():

    shutil.copy('data/byte.tif', 'tmp')
    open('tmp/byte.tif.aux.xml', 'wt').write("""<PAMDataset>
  <PAMRasterBand band="1">
    <Histograms>
      <HistItem>
        <HistMin>-0.5</HistMin>
        <HistMax>255.5</HistMax>
        <BucketCount>256</BucketCount>
        <IncludeOutOfRange>1</IncludeOutOfRange>
        <Approximate>0</Approximate>
        <HistCounts>6000000000|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|1|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|6|0|0|0|0|0|0|0|0|37|0|0|0|0|0|0|0|57|0|0|0|0|0|0|0|62|0|0|0|0|0|0|0|66|0|0|0|0|0|0|0|0|72|0|0|0|0|0|0|0|31|0|0|0|0|0|0|0|24|0|0|0|0|0|0|0|12|0|0|0|0|0|0|0|0|7|0|0|0|0|0|0|0|12|0|0|0|0|0|0|0|5|0|0|0|0|0|0|0|3|0|0|0|0|0|0|0|1|0|0|0|0|0|0|0|0|2|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|1|0|0|0|0|0|0|0|1</HistCounts>
      </HistItem>
    </Histograms>
  </PAMRasterBand>
</PAMDataset>""")

    ds = gdal.Open('tmp/byte.tif')
    (mini, maxi, _, hist1) = ds.GetRasterBand(1).GetDefaultHistogram()
    hist2 = ds.GetRasterBand(1).GetHistogram(include_out_of_range=1, approx_ok=0)
    ds.SetMetadataItem('FOO', 'BAR')
    ds.GetRasterBand(1).SetDefaultHistogram(mini, maxi, hist1)
    ds = None
    aux_xml = open('tmp/byte.tif.aux.xml', 'rt').read()
    gdal.Unlink('tmp/byte.tif')
    gdal.Unlink('tmp/byte.tif.aux.xml')

    assert hist1 == hist2
    assert hist1[0] == 6000000000
    assert '<HistCounts>6000000000|' in aux_xml

###############################################################################
# Test various stuff with PAM disabled
#


def test_pam_13():

    gdal.SetConfigOption('GDAL_PAM_ENABLED', 'NO')

    tmpfilename = '/vsimem/tmp.pgm'
    ds = gdal.GetDriverByName('PNM').Create(tmpfilename, 1, 1)
    # if ds.GetRasterBand(1).SetColorTable(None) == 0:
    #    gdaltest.post_reason('fail')
    #    return 'fail'
    gdal.PushErrorHandler()
    ret = ds.GetRasterBand(1).SetNoDataValue(0)
    gdal.PopErrorHandler()
    assert ret != 0

    gdal.PushErrorHandler()
    ret = ds.GetRasterBand(1).DeleteNoDataValue()
    gdal.PopErrorHandler()
    assert ret != 0

    ds = None

    assert gdal.VSIStatL(tmpfilename + '.aux.xml') is None

    gdal.Unlink(tmpfilename)

    gdal.SetConfigOption('GDAL_PAM_ENABLED', 'YES')

###############################################################################
# Test that existing PAM metadata is preserved when new is added
# https://github.com/OSGeo/gdal/issues/1430


def test_pam_metadata_preserved():

    tmpfilename = '/vsimem/tmp.pgm'
    ds = gdal.GetDriverByName('PNM').Create(tmpfilename, 1, 1)
    ds.SetMetadataItem('foo', 'bar')
    ds = None
    ds = gdal.Open(tmpfilename)
    ds.GetRasterBand(1).SetMetadataItem('bar', 'baz')
    ds = None
    ds = gdal.Open(tmpfilename)
    assert ds.GetMetadataItem('foo') == 'bar'
    assert ds.GetRasterBand(1).GetMetadataItem('bar') == 'baz'
    ds = None
    gdal.GetDriverByName('PNM').Delete(tmpfilename)

###############################################################################
# Test that we can retrieve GCPs from xml:ESRI domain
#

def test_pam_esri_GeodataXform_gcp():

    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/test_pam_esri_GeodataXform_gcp.tif', 20, 20, 1)
    ds = None

    gdal.FileFromMemBuffer('/vsimem/test_pam_esri_GeodataXform_gcp.tif.aux.xml',
"""<PAMDataset>
  <Metadata domain="xml:ESRI" format="xml">
    <GeodataXform xsi:type="typens:PolynomialXform" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:typens="http://www.esri.com/schemas/ArcGIS/10.3">
      <PolynomialOrder>1</PolynomialOrder>
      <SpatialReference xsi:type="typens:ProjectedCoordinateSystem">
        <WKT>PROJCS["NW Africa Grid",GEOGCS["GCS_NTF",DATUM["D_NTF",SPHEROID["Clarke_1880_IGN",6378249.2,293.4660212936265]],PRIMEM["Greenwich",0.0],UNIT["Degree",0.0174532925199433]],PROJECTION["Lambert_Conformal_Conic"],PARAMETER["False_Easting",1000000.0],PARAMETER["False_Northing",500000.0],PARAMETER["Central_Meridian",0.0],PARAMETER["Standard_Parallel_1",34.0],PARAMETER["Scale_Factor",0.9994],PARAMETER["Latitude_Of_Origin",34.0],UNIT["Meter",1.0]]</WKT>
        <XOrigin>-36981000</XOrigin>
        <YOrigin>-28020400</YOrigin>
        <XYScale>118575067.20124523</XYScale>
        <ZOrigin>-100000</ZOrigin>
        <ZScale>10000</ZScale>
        <MOrigin>-100000</MOrigin>
        <MScale>10000</MScale>
        <XYTolerance>0.001</XYTolerance>
        <ZTolerance>0.001</ZTolerance>
        <MTolerance>0.001</MTolerance>
        <HighPrecision>true</HighPrecision>
      </SpatialReference>
      <SourceGCPs xsi:type="typens:ArrayOfDouble">
        <Double>1</Double>
        <Double>-2</Double>
        <Double>3</Double>
        <Double>-4</Double>
        <Double>5</Double>
        <Double>-6</Double>
      </SourceGCPs>
      <TargetGCPs xsi:type="typens:ArrayOfDouble">
        <Double>7</Double>
        <Double>8</Double>
        <Double>9</Double>
        <Double>10</Double>
        <Double>11</Double>
        <Double>12</Double>
      </TargetGCPs>
    </GeodataXform>
  </Metadata>
</PAMDataset>""")

    ds = gdal.Open('/vsimem/test_pam_esri_GeodataXform_gcp.tif')
    gcps = ds.GetGCPs()
    sr_gt = ds.GetSpatialRef()
    sr_gcp = ds.GetGCPSpatialRef()

    gdal.GetDriverByName('GTiff').Delete('/vsimem/test_pam_esri_GeodataXform_gcp.tif')

    assert len(gcps) == 3
    assert gcps[0].GCPPixel == 1
    assert gcps[0].GCPLine == 2
    assert gcps[0].GCPX == 7
    assert gcps[0].GCPY == 8
    assert gcps[0].GCPZ == 0
    assert gcps[1].GCPPixel == 3
    assert gcps[1].GCPLine == 4
    assert gcps[1].GCPX == 9
    assert gcps[1].GCPY == 10
    assert gcps[1].GCPZ == 0
    assert sr_gt is None
    assert sr_gcp is not None

    ds = None

###############################################################################


def test_pam_metadata_coordinate_epoch():

    tmpfilename = '/vsimem/tmp.pgm'
    ds = gdal.GetDriverByName('PNM').Create(tmpfilename, 1, 1)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    srs.SetCoordinateEpoch(2021.3)
    ds.SetSpatialRef(srs)
    ds = None

    ds = gdal.Open(tmpfilename)
    srs = ds.GetSpatialRef()
    assert srs.GetCoordinateEpoch() == 2021.3
    ds = None

    gdal.GetDriverByName('PNM').Delete(tmpfilename)

###############################################################################
# Check that PAM handles correctly equality of NaN nodata values (#4847)

def test_pam_nodata_nan():

    outfilename = '/vsimem/test_pam_nodata_nan.tif'
    src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1, 1, gdal.GDT_Float32)
    src_ds.GetRasterBand(1).SetNoDataValue(float('nan'))
    gdal.GetDriverByName('GTiff').CreateCopy(outfilename, src_ds)
    # Check that no PAM file is generated
    assert gdal.VSIStatL(outfilename + '.aux.xml') is None
    gdal.GetDriverByName('GTiff').Delete(outfilename)

###############################################################################
# Cleanup.

def test_pam_cleanup():
    gdaltest.clean_tmp()
    if gdaltest.pam_setting != 'NULL':
        gdal.SetConfigOption('GDAL_PAM_ENABLED', gdaltest.pam_setting)
    else:
        gdal.SetConfigOption('GDAL_PAM_ENABLED', None)

    try:
        os.chmod('tmpdirreadonly', stat.S_IRUSR | stat.S_IWUSR | stat.S_IXUSR)
        shutil.rmtree('tmpdirreadonly')
    except OSError:
        pass
    try:
        shutil.rmtree('tmppamproxydir')
    except OSError:
        pass




