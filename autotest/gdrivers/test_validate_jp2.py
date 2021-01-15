#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test validate_jp2 script
# Author:   Even Rouault <even dot rouault at spatialys dot com>
#
###############################################################################
# Copyright (c) 2015, European Union (European Environment Agency)
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
import sys
from osgeo import gdal
import pytest

import gdaltest
from test_py_scripts import samples_path

###############################################################################
# Verify we have the JP2OpenJPEG driver.


def test_validate_jp2_1():

    gdaltest.has_validate_jp2_and_build_jp2 = False
    gdaltest.jp2openjpeg_drv = gdal.GetDriverByName('JP2OpenJPEG')
    if gdaltest.jp2openjpeg_drv is None:
        pytest.skip()


    path = samples_path
    if path not in sys.path:
        sys.path.append(path)

    try:
        import validate_jp2
        import build_jp2_from_xml
        validate_jp2.validate
        build_jp2_from_xml.build_file
    except (ImportError, AttributeError):
        pytest.skip()

    gdaltest.has_validate_jp2_and_build_jp2 = True
    gdaltest.deregister_all_jpeg2000_drivers_but('JP2OpenJPEG')

###############################################################################


def validate(filename, inspire_tg=True, expected_gmljp2=True, oidoc=None):

    try:
        os.stat('tmp/cache/SCHEMAS_OPENGIS_NET')
        ogc_schemas_location = 'tmp/cache/SCHEMAS_OPENGIS_NET'
    except OSError:
        ogc_schemas_location = 'disabled'

    if ogc_schemas_location != 'disabled':
        try:
            import xmlvalidate
            xmlvalidate.validate  # to make pyflakes happy
        except (ImportError, AttributeError):
            ogc_schemas_location = 'disabled'

    path = samples_path
    if path not in sys.path:
        sys.path.append(path)

    import validate_jp2
    error_report = validate_jp2.ErrorReport(collect_internally=True)
    return validate_jp2.validate(filename, oidoc, inspire_tg, expected_gmljp2, ogc_schemas_location, error_report=error_report)

###############################################################################
# Highly corrupted file


def test_validate_jp2_2():

    if not gdaltest.has_validate_jp2_and_build_jp2:
        pytest.skip()

    import build_jp2_from_xml

    build_jp2_from_xml.build_file('data/test_validate_jp2/byte_corrupted.xml', '/vsimem/out.jp2')
    error_report = validate('/vsimem/out.jp2', oidoc='data/test_validate_jp2/byte_oi.xml')
    gdal.Unlink('/vsimem/out.jp2')

    expected_errors = ['ERROR[GeoJP2]: 2 GeoTIFF UUID box found',
                       'ERROR[GeoJP2]: GeoTIFF should have width of 1 pixel, not 2',
                       'ERROR[GeoJP2]: GeoTIFF should have height of 1 pixel, not 2',
                       'ERROR[GENERAL]: Inconsistent geotransform between GeoJP2 ((440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0)) and GMLJP2 ((-440780.0, 60.0, 0.0, -3751260.0, 0.0, -60.0))',
                       'ERROR[GENERAL]: Inconsistent SRS between GeoJP2 (wkt=PROJCS["NAD27 / UTM zone 11N",GEOGCS["NAD27",DATUM["North_American_Datum_1927",SPHEROID["Clarke 1866",6378206.4,294.978698213898,AUTHORITY["EPSG","7008"]],AUTHORITY["EPSG","6267"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4267"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",-117],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Easting",EAST],AXIS["Northing",NORTH],AUTHORITY["EPSG","26711"]], proj4=+proj=utm +zone=11 +datum=NAD27 +units=m +no_defs) and GMLJP2 (wkt=PROJCS["WGS 84 / UTM zone 31N",GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4326"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",3],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Easting",EAST],AXIS["Northing",NORTH],AUTHORITY["EPSG","32631"]], proj4=+proj=utm +zone=31 +datum=WGS84 +units=m +no_defs)',
                       'ERROR[GENERAL]: ftyp.BR = "XXXX" instead of "jp2 "',
                       'ERROR[GENERAL]: ftyp.MinV = "1" instead of 0',
                       'ERROR[INSPIRE_TG]: "jpx " not found in compatibility list of ftyp, but GMLJP2 box present',
                       'ERROR[INSPIRE_TG]: "rreq" box does not advertise standard flag 67 whereas GMLJP2 box is present',
                       'ERROR[GENERAL]: ihdr.C = 6 instead of 7',
                       'ERROR[GENERAL]: ihdr.UnkC = 2 instead of 0 or 1',
                       'ERROR[GENERAL]: "ihdr" box expected to be found zero or one time, but present 2 times',
                       'ERROR[INSPIRE_TG, Requirement 23, Conformance class A.8.15]: "jp2c" box expected to be found one time, but present 2 times',
                       'ERROR[GENERAL]: "ftyp" box expected to be found zero or one time, but present 2 times',
                       'ERROR[INSPIRE_TG, Requirement 21]: SIZ.Rsiz=0 found but 2 (Profile 1) expected',
                       'ERROR[GENERAL]: ihdr_width(=21) != Xsiz (=20)- XOsiz(=0)',
                       'ERROR[GENERAL]: ihdr_height(=19) != Ysiz(=20) - YOsiz(=0)',
                       'ERROR[GENERAL]: ihdr_nc(=1) != Csiz (=2)',
                       'ERROR[INSPIRE_TG, Requirement 24, Conformance class A.8.9]: SIZ.Ssiz[0]=6 (unsigned 7 bits), which is not allowed',
                       'ERROR[GENERAL]: SIZ.Ssiz[0]=6, whereas bpcc[0]=7',
                       'ERROR[INSPIRE_TG, Requirement 24, Conformance class A.8.9]: SIZ.Ssiz[1]=6 (unsigned 7 bits), which is not allowed',
                       'ERROR[GMLJP2]: RectifiedGrid.limits.GridEnvelope.low[x] != XOsiz',
                       'ERROR[GMLJP2]: RectifiedGrid.limits.GridEnvelope.low[y] != YOsiz',
                       'ERROR[GMLJP2]: RectifiedGrid.limits.GridEnvelope.high[x] != Xsiz - 1',
                       'ERROR[GMLJP2]: RectifiedGrid.limits.GridEnvelope.high[y] != Ysiz - 1',
                       'ERROR[INSPIRE_TG, Conformance class A.8.6]: count(OrthoImageryCoverage.rangeType.field)(=1) != Csiz(=2) ',
                       'ERROR[PROFILE_1, Conformance class A.8.14]: SPcod_xcb_minus_2 = 5, whereas max allowed for Profile 1 is 4']

    if set(error_report.error_array) != set(expected_errors):
        import pprint
        pp = pprint.PrettyPrinter()
        pp.pprint(error_report.error_array)
        pytest.fail('did not get expected errors')

    expected_warnings = [
        'WARNING[GENERAL]: Unknown value 1 for colr.PREC',
        'WARNING[GENERAL]: Unknown value 1 for colr.APPROX',
        'WARNING[GENERAL]: Unknown value 1 for colr.EnumCS',
        'WARNING[GENERAL]: ihdr.ipr = 1 but no jp2i box found',
        'WARNING[INSPIRE_TG]: "asoc" box not at expected index',
        'WARNING[INSPIRE_TG]: "uuid" box not at expected index',
        'WARNING[INSPIRE_TG, Recommendation 39]: No user-defined precincts 0 defined'
    ]
    if set(error_report.warning_array) != set(expected_warnings):
        import pprint
        pp = pprint.PrettyPrinter()
        pp.pprint(error_report.warning_array)
        pytest.fail('did not get expected errors')


###############################################################################
# Another highly corrupted file


def test_validate_jp2_3():

    if not gdaltest.has_validate_jp2_and_build_jp2:
        pytest.skip()

    import build_jp2_from_xml

    build_jp2_from_xml.build_file('data/test_validate_jp2/stefan_full_rgba_corrupted.xml', '/vsimem/out.jp2')
    gdal.PushErrorHandler()
    error_report = validate('/vsimem/out.jp2', oidoc='data/test_validate_jp2/stefan_full_rgba_oi.xml')
    gdal.PopErrorHandler()
    gdal.Unlink('/vsimem/out.jp2')

    expected_errors = ['ERROR[GMLJP2]: No GMLJP2 box found whereas it was expected',
                       'ERROR[GENERAL]: cdef.N = 5 whereas ihdr.nc = 4',
                       'ERROR[GENERAL]: cdef.cn[2] = 4 is invalid',
                       'ERROR[GENERAL]: cdef.cn[4] = 3 is invalid since already used',
                       'ERROR[GENERAL]: cdef.typ[4] = 1 is invalid since another alpha channel has already been defined',
                       'ERROR[GENERAL]: cdef.asoc[4] = 0 is invalid since another band has already been associated to whole image',
                       'ERROR[PROFILE_1, Conformance class A.8.14]: Xsiz = 2200000162, whereas only 31 bits are allowed for Profile 1',
                       'ERROR[PROFILE_1, Conformance class A.8.14]: Ysiz = 2200000150, whereas only 31 bits are allowed for Profile 1',
                       'ERROR[PROFILE_1, Conformance class A.8.14]: XOsiz = 2200000000, whereas only 31 bits are allowed for Profile 1',
                       'ERROR[PROFILE_1, Conformance class A.8.14]: YOsiz = 2200000000, whereas only 31 bits are allowed for Profile 1',
                       'ERROR[GENERAL]: ihdr_nc(=4) != Csiz (=5)',
                       'ERROR[INSPIRE_TG, Requirement 24, Conformance class A.8.9]: SIZ.Ssiz[3]=2 (unsigned 3 bits), which is not allowed',
                       'ERROR[GENERAL]: SIZ.Ssiz[3]=2, whereas bpcc[3]=7',
                       'ERROR[INSPIRE_TG, Requirement 24, Conformance class A.8.9]: SIZ.Ssiz[4]=2 (unsigned 3 bits), which is not allowed',
                       'ERROR[PROFILE_1, Conformance class A.8.14]: XTsiz / min_XYRSiz = 2200000062.000000 > 1024',
                       'ERROR[PROFILE_1, Conformance class A.8.14]: XTsiz (=2200000062) != YTsiz (=2200000050)',
                       'ERROR[INSPIRE_TG]: Cannot find RectifiedGrid in OrthoImageryCoverage',
                       'ERROR[INSPIRE_TG, Conformance class A.8.6]: count(OrthoImageryCoverage.rangeType.field)(=4) != Csiz(=5) ',
                       'ERROR[PROFILE_1, Conformance class A.8.14]: Not enough decomposition levels = 0 (max_dim=162, 128 * 2**SPcod_NumDecompositions=128)']

    if error_report.error_array != expected_errors:
        import pprint
        pp = pprint.PrettyPrinter()
        pp.pprint(error_report.error_array)
        pytest.fail('did not get expected errors')

    expected_warnings = [
        'WARNING[INSPIRE_TG, Recommendation 38]: Bit depth of alpha channel should be 1 (BPCC 0), but its BPCC is 7',
        'WARNING[INSPIRE_TG, Recommendation 38]: Bit depth of alpha channel should be 1 (BPCC 0), but its BPCC is 7'
    ]
    if error_report.warning_array != expected_warnings:
        import pprint
        pp = pprint.PrettyPrinter()
        pp.pprint(error_report.warning_array)
        pytest.fail('did not get expected errors')


###############################################################################
# Another highly corrupted file


def test_validate_jp2_4():

    if not gdaltest.has_validate_jp2_and_build_jp2:
        pytest.skip()

    import build_jp2_from_xml

    build_jp2_from_xml.build_file('data/test_validate_jp2/almost_nojp2box.xml', '/vsimem/out.jp2')
    gdal.PushErrorHandler()
    error_report = validate('/vsimem/out.jp2', expected_gmljp2=False)
    gdal.PopErrorHandler()
    gdal.Unlink('/vsimem/out.jp2')

    expected_errors = [
        'ERROR[GENERAL]: "ftyp" box not found',
        'ERROR[GENERAL]: "jp2h" box not found',
        'ERROR[GENERAL]: No SIZ marker found',
        'ERROR[GENERAL]: No COD marker found',
        'ERROR[GENERAL]: No QCD marker found',
        'ERROR[GENERAL]: No SOT marker found',
        'ERROR[GENERAL]: No EOC marker found']

    if error_report.error_array != expected_errors:
        import pprint
        pp = pprint.PrettyPrinter()
        pp.pprint(error_report.error_array)
        pytest.fail('did not get expected errors')

    expected_warnings = [
    ]
    if error_report.warning_array != expected_warnings:
        import pprint
        pp = pprint.PrettyPrinter()
        pp.pprint(error_report.warning_array)
        pytest.fail('did not get expected errors')


###############################################################################
# Slightly less corrupted file. Test mainly issues with JP2boxes and color table
# Also a RGN marker


def test_validate_jp2_5():

    if not gdaltest.has_validate_jp2_and_build_jp2:
        pytest.skip()

    import build_jp2_from_xml

    build_jp2_from_xml.build_file('data/test_validate_jp2/utmsmall_pct_corrupted.xml', '/vsimem/out.jp2')
    gdal.PushErrorHandler()
    error_report = validate('/vsimem/out.jp2', oidoc='data/test_validate_jp2/utmsmall_pct_oi.xml')
    gdal.PopErrorHandler()
    gdal.Unlink('/vsimem/out.jp2')

    expected_errors = [
        'ERROR[INSPIRE_TG]: "jpx " not found in compatibility list of ftyp, but GMLJP2 box present',
        'ERROR[INSPIRE_TG]: "rreq" box not found, but GMLJP2 box present',
        'ERROR[INSPIRE_TG]: pclr box found but ihdr.nc = 2',
        'ERROR[INSPIRE_TG, Conformance class A.8.6]: pclr.NPC(=4) != 3 (for color table)',
        'ERROR[INSPIRE_TG]: cmap.MTYP[1] = 0 is invalid',
        'ERROR[GENERAL]: cmap.CMP[2] = 2 is invalid',
        'ERROR[GENERAL]: cmap.PCOL[2] = 0 is invalid since already used',
        'ERROR[GENERAL]: ihdr_nc(=2) != Csiz (=1)',
        'ERROR[INSPIRE_TG, Conformance class A.8.8]: Inconsistent geotransform between OrthoImagery ((440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0)) and GMLJP2/GeoJP2 ((40720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0))',
        'ERROR[INSPIRE_TG, Requirement 26, Conformance class A.8.16]: RGN marker found, which is not allowed']

    if error_report.error_array != expected_errors:
        import pprint
        pp = pprint.PrettyPrinter()
        pp.pprint(error_report.error_array)
        pytest.fail('did not get expected errors')

    expected_warnings = [
    ]
    if error_report.warning_array != expected_warnings:
        import pprint
        pp = pprint.PrettyPrinter()
        pp.pprint(error_report.warning_array)
        pytest.fail('did not get expected errors')


###############################################################################
# Nominal case with single band data


def test_validate_jp2_6():

    if not gdaltest.has_validate_jp2_and_build_jp2:
        pytest.skip()

    error_report = validate('data/test_validate_jp2/byte.jp2', oidoc='data/test_validate_jp2/byte_oi.xml')
    gdal.Unlink('/vsimem/out.jp2')

    expected_errors = []
    if error_report.error_array != expected_errors:
        import pprint
        pp = pprint.PrettyPrinter()
        pp.pprint(error_report.error_array)
        pytest.fail('did not get expected errors')

    expected_warnings = []
    if error_report.warning_array != expected_warnings:
        import pprint
        pp = pprint.PrettyPrinter()
        pp.pprint(error_report.warning_array)
        pytest.fail('did not get expected errors')


###############################################################################
# Nominal case with RGBA data


def test_validate_jp2_7():

    if not gdaltest.has_validate_jp2_and_build_jp2:
        pytest.skip()

    error_report = validate('data/test_validate_jp2/stefan_full_rgba.jp2', oidoc='data/test_validate_jp2/stefan_full_rgba_oi.xml', expected_gmljp2=False)
    gdal.Unlink('/vsimem/out.jp2')

    expected_errors = []
    if error_report.error_array != expected_errors:
        import pprint
        pp = pprint.PrettyPrinter()
        pp.pprint(error_report.error_array)
        pytest.fail('did not get expected errors')

    expected_warnings = []
    if error_report.warning_array != expected_warnings:
        import pprint
        pp = pprint.PrettyPrinter()
        pp.pprint(error_report.warning_array)
        pytest.fail('did not get expected errors')


###############################################################################
# Nominal case with color table data


def test_validate_jp2_8():

    if not gdaltest.has_validate_jp2_and_build_jp2:
        pytest.skip()

    error_report = validate('data/test_validate_jp2/utmsmall_pct.jp2', oidoc='data/test_validate_jp2/utmsmall_pct_oi.xml')
    gdal.Unlink('/vsimem/out.jp2')

    expected_errors = []
    if error_report.error_array != expected_errors:
        import pprint
        pp = pprint.PrettyPrinter()
        pp.pprint(error_report.error_array)
        pytest.fail('did not get expected errors')

    expected_warnings = []
    if error_report.warning_array != expected_warnings:
        import pprint
        pp = pprint.PrettyPrinter()
        pp.pprint(error_report.warning_array)
        pytest.fail('did not get expected errors')


###############################################################################


def test_validate_jp2_cleanup():

    if gdaltest.has_validate_jp2_and_build_jp2:
        gdaltest.reregister_all_jpeg2000_drivers()




