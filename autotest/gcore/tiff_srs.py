#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write round-tripping of SRS for GeoTIFF format.
# Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
#
###############################################################################
# Copyright (c) 2011-2012, Even Rouault <even dot rouault at mines-paris dot org>
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

import sys

from osgeo import gdal
from osgeo import osr

sys.path.append( '../pymod' )

import gdaltest


###############################################################################
# Write a geotiff and read it back to check its SRS

class TestTiffSRS:
    def __init__( self, epsg_code, use_epsg_code, expected_fail ):
        self.epsg_code = epsg_code
        self.use_epsg_code = use_epsg_code
        self.expected_fail = expected_fail

    def test( self ):
        sr = osr.SpatialReference()
        if isinstance(self.epsg_code, str):
            sr.SetFromUserInput(self.epsg_code)
        else:
            sr.ImportFromEPSG(self.epsg_code)
            if self.use_epsg_code == 0:
                proj4str = sr.ExportToProj4()
                #print(proj4str)
                sr.SetFromUserInput(proj4str)

        ds = gdal.GetDriverByName('GTiff').Create('/vsimem/TestTiffSRS.tif',1,1)
        ds.SetProjection(sr.ExportToWkt())
        ds = None

        ds = gdal.Open('/vsimem/TestTiffSRS.tif')
        wkt = ds.GetProjectionRef()
        sr2 = osr.SpatialReference()
        sr2.SetFromUserInput(wkt)
        ds = None

        gdal.Unlink('/vsimem/TestTiffSRS.tif')

        if sr.IsSame(sr2) != 1:
            if self.expected_fail:
                print('did not get expected SRS. known to be broken currently. FIXME!')
                #print(sr)
                #print(sr2)
                return 'expected_fail'

            gdaltest.post_reason('did not get expected SRS')
            print(sr)
            print(sr2)
            return 'fail'

        return 'success'

###############################################################################
# Test fix for #4677:
def tiff_srs_without_linear_units():

    sr = osr.SpatialReference()
    sr.ImportFromProj4('+proj=vandg +datum=WGS84')

    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/tiff_srs_without_linear_units.tif',1,1)
    ds.SetProjection(sr.ExportToWkt())
    ds = None

    ds = gdal.Open('/vsimem/tiff_srs_without_linear_units.tif')
    wkt = ds.GetProjectionRef()
    sr2 = osr.SpatialReference()
    sr2.SetFromUserInput(wkt)
    ds = None

    gdal.Unlink('/vsimem/tiff_srs_without_linear_units.tif')

    if sr.IsSame(sr2) != 1:

        gdaltest.post_reason('did not get expected SRS')
        print(sr)
        print(sr2)
        return 'fail'

    return 'success'

###############################################################################
# Test COMPDCS without VerticalCSType

def tiff_srs_compd_cs():

    sr = osr.SpatialReference()
    # EPSG:7400 without the Authority
    sr.SetFromUserInput("""COMPD_CS["unknown",
    GEOGCS["NTF (Paris)",
        DATUM["Nouvelle_Triangulation_Francaise_Paris",
            SPHEROID["Clarke 1880 (IGN)",6378249.2,293.4660212936265,
                AUTHORITY["EPSG","7011"]],
            TOWGS84[-168,-60,320,0,0,0,0],
            AUTHORITY["EPSG","6807"]],
        PRIMEM["Paris",2.5969213],
        UNIT["grad",0.01570796326794897],
        AUTHORITY["EPSG","4807"]],
    VERT_CS["NGF-IGN69 height",
        VERT_DATUM["Nivellement General de la France - IGN69",2005,
            AUTHORITY["EPSG","5119"]],
        UNIT["metre",1,
            AUTHORITY["EPSG","9001"]],
        AXIS["Up",UP]]]""")

    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/tiff_srs_compd_cs.tif',1,1)
    ds.SetProjection(sr.ExportToWkt())
    ds = None

    gdal.SetConfigOption('GTIFF_REPORT_COMPD_CS','YES')
    ds = gdal.Open('/vsimem/tiff_srs_compd_cs.tif')
    wkt = ds.GetProjectionRef()
    gdal.SetConfigOption('GTIFF_REPORT_COMPD_CS',None)
    sr2 = osr.SpatialReference()
    sr2.SetFromUserInput(wkt)
    ds = None

    gdal.Unlink('/vsimem/tiff_srs_compd_cs.tif')

    if sr.IsSame(sr2) != 1:

        gdaltest.post_reason('did not get expected SRS')
        print(sr)
        print(sr2)
        return 'fail'

    return 'success'

###############################################################################
# Test reading a GeoTIFF with both StdParallel1 and ScaleAtNatOrigin defined (#5791)

def tiff_srs_weird_mercator_2sp():
    
    ds = gdal.Open('data/weird_mercator_2sp.tif')
    gdal.PushErrorHandler()
    wkt = ds.GetProjectionRef()
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('warning expected')
        return 'fail'
    sr2 = osr.SpatialReference()
    sr2.SetFromUserInput(wkt)
    ds = None

    sr = osr.SpatialReference()
    # EPSG:7400 without the Authority
    sr.SetFromUserInput("""PROJCS["Global Mercator",
    GEOGCS["NAD83",
        DATUM["North_American_Datum_1983",
            SPHEROID["GRS 1980",6378137,298.2572221010002,
                AUTHORITY["EPSG","7019"]],
            TOWGS84[0,0,0,0,0,0,0],
            AUTHORITY["EPSG","6269"]],
        PRIMEM["Greenwich",0],
        UNIT["degree",0.0174532925199433],
        AUTHORITY["EPSG","4269"]],
    PROJECTION["Mercator_2SP"],
    PARAMETER["standard_parallel_1",47.667],
    PARAMETER["central_meridian",0],
    PARAMETER["false_easting",0],
    PARAMETER["false_northing",0],
    UNIT["metre",1,
        AUTHORITY["EPSG","9001"]]]""")

    if sr.IsSame(sr2) != 1:

        gdaltest.post_reason('did not get expected SRS')
        print(sr)
        print(sr2)
        return 'fail'

    return 'success'


gdaltest_list = []

tiff_srs_list = [ 2758, #tmerc
                  2036, #sterea
                  2046, #tmerc
                  3031, #polar stere (ticket #3220)
                  3032, #polar stere (ticket #3220)
                  32661, #stere
                  3035, #laea
                  2062, #lcc 1SP
                  [2065, True, True], #krovak
                  [2066, False, True], #cass
                  2964, #aea
                  3410, #cea
                  3786, #eqc spherical, method=9823
                  32663, #eqc elliptical, method=9842
                  4087, # eqc WGS 84 / World Equidistant Cylindrical method=1028
                  4088, # eqc World Equidistant Cylindrical (Sphere) method=1029
                  2934, #merc
                  27200, #nzmg
                  2057, #omerc Hotine_Oblique_Mercator_Azimuth_Center
                  3591, #omerc Hotine_Oblique_Mercator
                  29100, #poly
                  2056, #somerc
                  2027, #utm
                  4326, #longlat
                  26943, #lcc 2SP,
                  4328, #geocentric
                  3994, #mercator 2SP
                  26920, # UTM NAD83 special case
                  26720, # UTM NAD27 special case
                  32630, # UTM WGS84 north special case
                  32730, # UTM WGS84 south special case
]

for item in tiff_srs_list:
    try:
        epsg_code = item[0]
        epsg_broken = item[1]
        epsg_proj4_broken = item[2]
    except:
        epsg_code = item
        epsg_broken = False
        epsg_proj4_broken = False

    ut = TestTiffSRS( epsg_code, 1, epsg_broken )
    gdaltest_list.append( (ut.test, "tiff_srs_epsg_%d" % epsg_code) )
    ut = TestTiffSRS( epsg_code, 0, epsg_proj4_broken )
    gdaltest_list.append( (ut.test, "tiff_srs_proj4_of_epsg_%d" % epsg_code) )

tiff_srs_list_proj4 = [ ['eqdc', '+proj=eqdc +lat_0=%.16g +lon_0=%.16g +lat_1=%.16g +lat_2=%.16g" +x_0=%.16g +y_0=%.16g' % (1,2,3,4,5,6)],
                        ['mill', '+proj=mill +lat_0=%.16g +lon_0=%.16g +x_0=%.16g +y_0=%.16g +R_A' % (1,2,3,4)],
                        ['gnom', '+proj=gnom +lat_0=%.16g +lon_0=%.16g +x_0=%.16g +y_0=%.16g' % (1,2,3,4)],
                        ['robin', '+proj=robin +lon_0=%.16g +x_0=%.16g +y_0=%.16g' % (1,2,3)],
                        ['sinu', '+proj=sinu +lon_0=%.16g +x_0=%.16g +y_0=%.16g' % (1,2,3)],
                        ]
for (title, proj4) in tiff_srs_list_proj4:
    ut = TestTiffSRS( proj4, 0, False )
    gdaltest_list.append( (ut.test, "tiff_srs_proj4_%s" % title) )

gdaltest_list.append( tiff_srs_without_linear_units )
gdaltest_list.append( tiff_srs_compd_cs )
gdaltest_list.append( tiff_srs_weird_mercator_2sp )


if __name__ == '__main__':

    gdaltest.setup_run( 'tiff_srs' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

