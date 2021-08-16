#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test NetCDF driver support.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2011, Even Rouault <even dot rouault at spatialys.com>
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
import imp  # for netcdf_cf_setup()
from gdrivers.netcdf import netcdf_setup, netcdf_test_copy  # noqa
netcdf_setup; # to please pyflakes
from osgeo import gdal
from osgeo import osr


import gdaltest
import pytest

pytestmark = pytest.mark.require_driver('netCDF')

###############################################################################
# Netcdf CF compliance Functions
###############################################################################

###############################################################################
# check for necessary files and software


def netcdf_cf_setup():

    # global vars
    gdaltest.netcdf_cf_method = None
    gdaltest.netcdf_cf_files = None
    gdaltest.netcdf_cf_check_error = ''

    # if netcdf is not supported, skip detection
    if gdaltest.netcdf_drv is None:
        pytest.skip()

    # skip if on windows
    if os.name != 'posix':
        pytest.skip('NOTICE: will skip CF checks because OS is not posix!')

    # try local method
    cdms2_installed = False
    try:
        imp.find_module('cdms2')
        cdms2_installed = True
    except ImportError:
        print('NOTICE: cdms2 not installed!')
        print('        see installation notes at http://pypi.python.org/pypi/cfchecker')
    if cdms2_installed:
        xml_dir = './data/netcdf_cf_xml'
        tmp_dir = './tmp/cache'
        files = dict()
        files['a'] = xml_dir + '/area-type-table.xml'
        files['s'] = tmp_dir + '/cf-standard-name-table-v18.xml'
        # either find udunits path in UDUNITS_PATH, or based on location of udunits app, or copy all .xml files to data
        # opt_u = '/home/soft/share/udunits/udunits2.xml'
        files['u'] = xml_dir + '/udunits2.xml'
        # look for xml files
        if not (os.path.exists(files['a']) and os.path.exists(files['s']) and os.path.exists(files['u'])):
            print('NOTICE: cdms2 installed, but necessary xml files are not found!')
            print('        the following files must exist:')
            print('        ' + xml_dir + '/area-type-table.xml from http://cf-pcmdi.llnl.gov/documents/cf-standard-names/area-type-table/1/area-type-table.xml')
            print('        ' + tmp_dir + '/cf-standard-name-table-v18.xml - http://cf-pcmdi.llnl.gov/documents/cf-standard-names/standard-name-table/18/cf-standard-name-table.xml')
            print('        ' + xml_dir + '/udunits2*.xml from a UDUNITS2 install')
            # try to get cf-standard-name-table
            if not os.path.exists(files['s']):
                # print '        downloading cf-standard-name-table.xml (v18) from http://cf-pcmdi.llnl.gov ...'
                if not gdaltest.download_file('http://cf-pcmdi.llnl.gov/documents/cf-standard-names/standard-name-table/18/cf-standard-name-table.xml',
                                              'cf-standard-name-table-v18.xml'):
                    print('        Failed to download, please get it and try again.')

        if os.path.exists(files['a']) and os.path.exists(files['s']) and os.path.exists(files['u']):
            gdaltest.netcdf_cf_method = 'local'
            gdaltest.netcdf_cf_files = files
            print('NOTICE: netcdf CF compliance checks: using local checker script')
            return

    # skip http method if GDAL_DOWNLOAD_TEST_DATA and GDAL_RUN_SLOW_TESTS are not defined
    if not gdaltest.download_test_data():
        print('NOTICE: skipping netcdf CF compliance checks')
        print('to enable remote http checker script, define GDAL_DOWNLOAD_TEST_DATA=YES')
        return

    if not gdaltest.run_slow_tests():
        print('NOTICE: skipping netcdf CF compliance checks')
        return

    # http method with curl, should use python module but easier for now
    success = False
    try:
        gdaltest.runexternal_out_and_err('curl')
    except OSError:
        print('no curl executable')
    else:
        # make sure script is responding
        handle = gdaltest.gdalurlopen("http://puma.nerc.ac.uk/cgi-bin/cf-checker.pl")
        if handle is not None:
            success = True
        else:
            print('script not responding')
    if success:
        gdaltest.netcdf_cf_method = 'http'
        print('NOTICE: netcdf CF compliance checks: using remote HTTP '
              'checker script, consider installing cdms2 locally')
        return

    if gdaltest.netcdf_cf_method is None:
        print('NOTICE: skipping netcdf CF compliance checks')


###############################################################################
# build a command used to check ifile


def netcdf_cf_get_command(ifile, version='auto'):

    command = ''
    # fetch method obtained previously
    method = gdaltest.netcdf_cf_method
    if method is not None:
        if method == 'local':
            command = './netcdf_cfchecks.py -a ' + gdaltest.netcdf_cf_files['a'] \
                + ' -s ' + gdaltest.netcdf_cf_files['s'] \
                + ' -u ' + gdaltest.netcdf_cf_files['u'] \
                + ' -v ' + version + ' ' + ifile
        elif method == 'http':
            # command = shlex.split( 'curl --form cfversion="1.5" --form upload=@' + ifile + ' --form submit=\"Check file\" "http://puma.nerc.ac.uk/cgi-bin/cf-checker.pl"' )
            # switch to 1.5 as driver now supports, and auto when it becomes available
            version = '1.5'
            command = 'curl --form cfversion=' + version + ' --form upload=@' + ifile + ' --form submit=\"Check file\" "http://puma.nerc.ac.uk/cgi-bin/cf-checker.pl"'

    return command


###############################################################################
# Check a file for CF compliance
def netcdf_cf_check_file(ifile, version='auto', silent=True):
    # pylint: disable=unused-argument
    gdaltest.netcdf_cf_check_error = ''

    if not os.path.exists(ifile):
        pytest.skip()

    output_all = ''

    command = netcdf_cf_get_command(ifile, version='auto')
    if command is None or command == '':
        pytest.skip('no suitable method found, skipping')

    try:
        if gdaltest.netcdf_cf_method == 'http':
            print('calling ' + command)
        (ret, err) = gdaltest.runexternal_out_and_err(command)
    except OSError:
        pytest.fail('ERROR with command - ' + command)

    # There should be a ERRORS detected summary
    if 'ERRORS detected' not in ret:
        print(err)
        pytest.fail('ERROR with command - ' + command)

    output_all = ret
    output_err = ''
    output_warn = ''

    for line in output_all.splitlines():
        # optimize this with regex
        if 'ERROR' in line and 'ERRORS' not in line:
            output_err = output_err + '\n' + line
        elif 'WARNING' in line and 'WARNINGS' not in line:
            output_warn = output_warn + '\n' + line

    result = 'success'

    if output_err != '':
        result = 'fail'
    if output_err != '':
        gdaltest.netcdf_cf_check_error += output_err.strip()
        if not silent:
            print('=> CF check ERRORS for file ' + ifile + ' : ' + output_err)

    if output_warn != '':
        if not silent:
            print('CF check WARNINGS for file ' + ifile + ' : ' + output_warn)

    return result


###############################################################################
# Netcdf CF projection Functions and data
###############################################################################

###############################################################################
# Definitions to test projections that are supported by CF

# Tuple structure:
#  0: Short code (e.g. AEA) - (no GDAL significance, just for filenames etc.)
#  1: official name from CF-1 conventions
#  2: EPSG code, or WKT, to tell GDAL to do reprojection
#  3: Actual attribute official name of grid mapping
#  4: List of required attributes to define projection
#  5: List of required coordinate variable standard name attributes

netcdf_cfproj_tuples = [
    ("AEA", "Albers Equal Area", "EPSG:3577", "albers_conical_equal_area",
        ['standard_parallel', 'longitude_of_central_meridian',
         'latitude_of_projection_origin', 'false_easting', 'false_northing'],
     ['projection_x_coordinate', 'projection_y_coordinate']),
    ("AZE", "Azimuthal Equidistant",
        # Didn't have EPSG suitable for AU
        "+proj=aeqd +lat_0=-37 +lon_0=145 +x_0=0 +y_0=0 +ellps=WGS84 +datum=WGS84 +units=m +no_defs",
        "azimuthal_equidistant",
        ['longitude_of_projection_origin',
         'latitude_of_projection_origin', 'false_easting', 'false_northing'],
     ['projection_x_coordinate', 'projection_y_coordinate']),
    ("LAZEA", "Lambert azimuthal equal area",
        # Specify proj4 since no appropriate LAZEA for AU.
        # "+proj=laea +lat_0=0 +lon_0=134 +x_0=0 +y_0=0 +ellps=GRS80 +towgs84=0,0,0,0,0,0,0 +units=m +no_defs",
        "+proj=laea +lat_0=-37 +lon_0=145 +x_0=0 +y_0=0 +ellps=WGS84 +datum=WGS84 +units=m +no_defs",
        "lambert_azimuthal_equal_area",
        ['longitude_of_projection_origin',
         'latitude_of_projection_origin', 'false_easting', 'false_northing'],
     ['projection_x_coordinate', 'projection_y_coordinate']),
    ("LC_2SP", "Lambert conformal", "EPSG:3112", "lambert_conformal_conic",
        ['standard_parallel',
         'longitude_of_central_meridian',
         'latitude_of_projection_origin', 'false_easting', 'false_northing'],
     ['projection_x_coordinate', 'projection_y_coordinate']),
    # TODO: Test LCC with 1SP
    ("LCEA", "Lambert Cylindrical Equal Area",
        "+proj=cea +lat_ts=-37 +lon_0=145 +x_0=0 +y_0=0 +ellps=WGS84 +datum=WGS84 +units=m +no_defs",
        "lambert_cylindrical_equal_area",
        ['longitude_of_central_meridian',
         'standard_parallel',  # TODO: OR 'scale_factor_at_projection_origin'
         'false_easting', 'false_northing'],
     ['projection_x_coordinate', 'projection_y_coordinate']),
    # 2 entries for Mercator, since attribs different for 1SP or 2SP
    ("M-1SP", "Mercator",
        "+proj=merc +lon_0=145 +k_0=1 +x_0=0 +y_0=0 +ellps=WGS84 +datum=WGS84 +units=m +no_defs",
        "mercator",
        ['longitude_of_projection_origin',
         'scale_factor_at_projection_origin',
         'false_easting', 'false_northing'],
     ['projection_x_coordinate', 'projection_y_coordinate']),
    # Commented out as it seems GDAL itself's support of Mercator with 2SP
    #  is a bit dodgy
    ("M-2SP", "Mercator",
        "+proj=merc +lat_ts=-37 +lon_0=145 +x_0=0 +y_0=0 +ellps=WGS84 +datum=WGS84 +units=m +no_defs",
        # Trying with full WKT:
        # """PROJCS["unnamed", GEOGCS["WGS 84", DATUM["WGS_1984", SPHEROID["WGS 84",6378137,298.257223563, AUTHORITY["EPSG","7030"]], AUTHORITY["EPSG","6326"]], PRIMEM["Greenwich",0], UNIT["degree",0.0174532925199433], AUTHORITY["EPSG","4326"]], PROJECTION["Mercator_2SP"], PARAMETER["central_meridian",146], PARAMETER["standard_parallel_1",-37], PARAMETER["latitude_of_origin",0], PARAMETER["false_easting",0], PARAMETER["false_northing",0], UNIT["metre",1, AUTHORITY["EPSG","9001"]]]""",
        "mercator",
        ['longitude_of_projection_origin',
         'standard_parallel',
         'false_easting', 'false_northing'],
     ['projection_x_coordinate', 'projection_y_coordinate']),
    ("Ortho", "Orthographic",
        "+proj=ortho +lat_0=-37 +lon_0=145 +x_0=0 +y_0=0 +ellps=WGS84 +datum=WGS84 +units=m +no_defs",
        "orthographic",
        ['longitude_of_projection_origin',
         'latitude_of_projection_origin',
         'false_easting', 'false_northing'],
     ['projection_x_coordinate', 'projection_y_coordinate']),
    # Seems GDAL may have problems with Polar stereographic, as it
    #  considers these "local coordinate systems"
    ("PSt", "Polar stereographic",
        "+proj=stere +lat_ts=-37 +lat_0=-90 +lon_0=145 +k_0=1.0 +x_0=0 +y_0=0 +ellps=WGS84 +datum=WGS84 +units=m +no_defs",
        "polar_stereographic",
        ['straight_vertical_longitude_from_pole',
         'latitude_of_projection_origin',
         'standard_parallel',
         'false_easting', 'false_northing'],
     ['projection_x_coordinate', 'projection_y_coordinate']),
    ("St", "Stereographic",
        "+proj=stere +lat_0=-37 +lon_0=145 +x_0=0 +y_0=0 +ellps=WGS84 +datum=WGS84 +units=m +no_defs",
        # 'PROJCS["unnamed", GEOGCS["WGS 84", DATUM["WGS_1984", SPHEROID["WGS 84",6378137,298.257223563, AUTHORITY["EPSG","7030"]], AUTHORITY["EPSG","6326"]], PRIMEM["Greenwich",0], UNIT["degree",0.0174532925199433], AUTHORITY["EPSG","4326"]], PROJECTION["Stereographic"], PARAMETER["latitude_of_origin",-37.5], PARAMETER["central_meridian",145], PARAMETER["scale_factor",1], PARAMETER["false_easting",0], PARAMETER["false_northing",0], UNIT["metre",1, AUTHORITY["EPSG","9001"]]]',
        "stereographic",
        ['longitude_of_projection_origin',
         'latitude_of_projection_origin',
         'scale_factor_at_projection_origin',
         'false_easting', 'false_northing'],
     ['projection_x_coordinate', 'projection_y_coordinate']),
    # Note: Rotated Pole not in this list, as seems not GDAL-supported
    ("TM", "Transverse Mercator", "EPSG:32655",  # UTM Zone 55N
        "transverse_mercator",
        ['scale_factor_at_central_meridian',
         'longitude_of_central_meridian',
         'latitude_of_projection_origin',
         'false_easting', 'false_northing'],
        ['projection_x_coordinate', 'projection_y_coordinate']),
    ("GEOS", "Geostationary_satellite",
        "+proj=geos +h=35785831 +lon_0=145 +datum=WGS84 +sweep=y +units=m",
        "geostationary",
        ['longitude_of_projection_origin',
         'perspective_point_height',
         'sweep_angle_axis',
         'false_easting', 'false_northing'],
        ['projection_x_coordinate', 'projection_y_coordinate'])
]

# By default, we will use GeoTIFF as the 'intermediate' raster format
# for gdalwarp'ing into before gdal_translate to NetCDF.
# But since Gratify can't act as a storage format for certain projections
# (e.g. Mercator-2SP), we will choose other intermediate formats for certain
# projection.
# The following array maps projection short code, to driver format to use

netcdf_cfproj_def_int_format = "GTiff"

netcdf_cfproj_int_fmt_maps = {
    "M-2SP": 'HFA'
}

netcdf_cfproj_format_fnames = {"HFA": "img", "GTiff": "tif", "NITF": "nitf",
                               "ERS": "ers"}

###############################################################################
# Check support for given projection tuple definitions
# For each projection, warp the original file and then create a netcdf


def netcdf_cfproj_testcopy(projTuples, origTiff, interFormats, inPath, outPath,
                           resFilename):
    """Test a Geotiff file can be converted to NetCDF, and projection in
    CF-1 conventions can be successfully maintained. Save results to file.

    :arg: projTuples - list of tuples
    :arg: interFormats - dict of intermediate format overrides
    :arg: outPath - path to save output
    :arg: resFilename - results filename to write to.
    """

    silent = True
    gdaltest.netcdf_drv_silent = True
    bWriteGdalTags = "YES"
    # silent = False
    gdaltest.netcdf_drv_silent = False
#    bWriteGdalTags="NO"

    result = 'success'

    # Test if ncdump is available
    try:
        (_, err) = gdaltest.runexternal_out_and_err('ncdump -h')
    except OSError:
        # nothing is supported as ncdump not found
        pytest.skip('NOTICE: netcdf version not found')

    i = err.find('netcdf library version ')
    # version not found
    if i == -1:
        pytest.skip('NOTICE: netcdf version not found')

    if not os.path.exists(outPath):
        os.makedirs(outPath)
    resFile = open(os.path.join(outPath, resFilename), "w")

    if not os.path.exists(outPath):
        os.makedirs(outPath)

    heading = "Testing GDAL translation results to NetCDF\n"
    resFile.write(heading)
    resFile.write(len(heading) * "=" + "\n")

#    now = datetime.datetime.now()
#    resFile.write("*Date/time:* %s\n" % (now.strftime("%Y-%m-%d %H:%M")))
    resFile.write("\n")

    resPerProj = {}

    dsTiff = gdal.Open(os.path.join(inPath, origTiff), gdal.GA_ReadOnly)
    s_srs_wkt = dsTiff.GetProjection()

    # objects to hold the various tests
    i_t = 0
    tst_res = {}

    for proj in projTuples:
        try:
            intFmt = interFormats[proj[0]]
        except KeyError:
            intFmt = netcdf_cfproj_def_int_format

        intExt = netcdf_cfproj_format_fnames[intFmt]

        # Our little results data structures
        if not silent:
            print("")
            print("Testing %s (%s) translation:" % (proj[0], proj[1]))

        if not silent:
            print("About to create raster in chosen SRS")
        # projVrt = os.path.join(outPath, "%s_%s.vrt" % \
        #    (origTiff.rstrip('.tif'), proj[0] ))
        projRaster = os.path.join(outPath, "%s_%s.%s" %
                                  (os.path.basename(origTiff).rstrip('.tif'), proj[0], intExt))
        srs = osr.SpatialReference()
        srs.SetFromUserInput(proj[2])
        t_srs_wkt = srs.ExportToWkt()
        if not silent:
            print("going to warp file " + origTiff + "\n" + s_srs_wkt + "\ninto file " + projRaster + "\n" + t_srs_wkt)
        dswarp = gdal.AutoCreateWarpedVRT(dsTiff, s_srs_wkt, t_srs_wkt, gdal.GRA_NearestNeighbour, 0)
        drv_inter = gdal.GetDriverByName(intFmt)
        drv_netcdf = gdal.GetDriverByName("netcdf")
        dsw = drv_inter.CreateCopy(projRaster, dswarp, 0)
        if not silent:
            print("Warped %s to %s" % (proj[0], projRaster))

        projNc = os.path.join(outPath, "%s_%s.nc" %
                              (origTiff.rstrip('.tif'), proj[0]))
        # Force GDAL tags to be written to make testing easier, with preserved datum etc
        # ncCoOpts = "-co WRITE_GDAL_TAGS=yes"
        if not silent:
            print("About to translate to NetCDF")
        dst = drv_netcdf.CreateCopy(projNc, dsw, 0, ['WRITE_GDAL_TAGS=' + bWriteGdalTags])
        # For drivers like HFA, line below ESSENTIAL so that all info is
        # saved to new raster file - which we'll reopen later and want
        # to be fully updated.
        dsw = None
        del dst
        if not silent:
            print("Translated to %s" % (projNc))

        transWorked, resDetails = netcdf_cfproj_test_cf(proj, projNc)
        resPerProj[proj[0]] = resDetails

        resFile.write("%s (%s): " % (proj[0], proj[1]))
        if transWorked:
            resFile.write("OK\n")
        else:
            resFile.write("BAD\n")
            if 'missingProjName' in resPerProj[proj[0]]:
                resFile.write("\tMissing proj name '%s'\n" %
                              (resPerProj[proj[0]]['missingProjName']))
            for attrib in resPerProj[proj[0]]['missingAttrs']:
                resFile.write("\tMissing attrib '%s'\n" % (attrib))
            for cVarStdName in resPerProj[proj[0]]['missingCoordVarStdNames']:
                resFile.write("\tMissing coord var with std name '%s'\n"
                              % (cVarStdName))
            if 'cfcheck_error' in resPerProj[proj[0]]:
                resFile.write("\tFailed cf check: %s\n" %
                              (resPerProj[proj[0]]['cfcheck_error']))

        # test file copy
        # We now copy to a new file, just to be safe
        projNc2 = projNc.rstrip('.nc') + '2.nc'
        projRaster2 = os.path.join(outPath, "%s_%s2.%s" %
                                   (origTiff.rstrip('.tif'), proj[0], intExt))

        tst_res[i_t + 1] = netcdf_test_copy(projRaster, 1, None, projNc2, [], 'NETCDF')
        tst_res[i_t + 2] = netcdf_test_copy(projNc2, 1, None, projRaster2, [], intFmt)

        if tst_res[i_t + 1] == 'fail' or tst_res[i_t + 2] == 'fail':
            result = 'fail'

        i_t = i_t + 2

    resFile.close()

    if not silent:
        print("\n" + "*" * 80)
        print("Saved results to file %s" % (os.path.join(outPath, resFilename)))

    # result = 'success'
    resFile = open(os.path.join(outPath, resFilename), "r")
    resStr = resFile.read()
    if resStr.find('BAD') != -1:
        print('\nCF projection tests failed, here is the output (stored in file %s)\n' %
              (os.path.join(outPath, resFilename)))
        print(resStr)
        result = 'fail'

    return result

###############################################################################
# Test an NC file has valid conventions according to passed-in proj tuple
# Note: current testing strategy is a fairly simple attribute search.
# this could use GDAL NetCDF driver for getting attribs instead.


def netcdf_cfproj_test_cf(proj, projNc):

    transWorked = True

    command = 'ncdump -h ' + projNc
    (ret, err) = gdaltest.runexternal_out_and_err(command)
    if err != '':
        print(err)
    dumpStr = ret

    resDetails = {}
    resDetails['missingAttrs'] = []
    resDetails['missingCoordVarStdNames'] = []
    if (':grid_mapping_name = "%s"' % (proj[3])) not in dumpStr:
        transWorked = False
        resDetails['missingProjName'] = proj[3]
    # Check attributes in the projection are included.
    for attrib in proj[4]:
        # The ':' prefix and ' ' suffix is to help check for exact name,
        # e.g. to catch the standard_parallel_1 and 2 issue.
        if (":" + attrib + " ") not in dumpStr:
            transWorked = False
            resDetails['missingAttrs'].append(attrib)
    #        print "**Error for proj '%s': CF-1 attrib '%s' not found.**" % \
    #           (proj[0], attrib)
    # Now we check the required X and Y attributes are included (e.g. Rotated
    # Pole has special names required here.
    for coordVarStdName in proj[5]:
        if coordVarStdName not in dumpStr:
            transWorked = False
            resDetails['missingCoordVarStdNames'].append(coordVarStdName)

    # Final check use the cf-checker.
    result_cf = netcdf_cf_check_file(projNc, 'auto', True)
    if result_cf == 'fail':
        resDetails['cfcheck_error'] = gdaltest.netcdf_cf_check_error
        transWorked = False

    return transWorked, resDetails


###############################################################################
# Netcdf CF Tests
###############################################################################

###############################################################################
# test copy and CF compliance for lat/lon (no datum, no GEOGCS) file, tif->nc->tif
def test_netcdf_cf_1(netcdf_setup):  # noqa
    # setup netcdf and netcdf_cf environment
    netcdf_cf_setup()

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    # tst1 = gdaltest.GDALTest( 'NETCDF', 'netcdf/trmm.tif', 1, 14 )
    # result = tst1.testCreateCopy(check_gt=1, check_srs=1, new_filename='tmp/netcdf_cf_1.nc', delete_copy = 0)
    result = netcdf_test_copy('data/netcdf/trmm.nc', 1, 14, 'tmp/netcdf_cf_1.nc')
    if result != 'fail':
        # tst2 = gdaltest.GDALTest( 'GTIFF', '../tmp/netcdf_cf_1.nc', 1, 14 )
        # result = tst2.testCreateCopy(check_gt=1, check_srs=1, new_filename='tmp/netcdf_cf_1.tiff', delete_copy = 0)
        result = netcdf_test_copy('tmp/netcdf_cf_1.nc', 1, 14, 'tmp/netcdf_cf_1.tif', [], 'GTIFF')

    result_cf = 'success'
    if gdaltest.netcdf_cf_method is not None:
        result_cf = netcdf_cf_check_file('tmp/netcdf_18.nc', 'auto', False)

    if result != 'fail' and result_cf != 'fail':
        return
    pytest.fail()


###############################################################################
# test copy and CF compliance for lat/lon (no datum, no GEOGCS) file, nc->nc
def test_netcdf_cf_2():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    result = netcdf_test_copy('data/netcdf/trmm.nc', 1, 14, 'tmp/netcdf_cf_2.nc')

    result_cf = 'success'
    if gdaltest.netcdf_cf_method is not None:
        result_cf = netcdf_cf_check_file('tmp/netcdf_cf_2.nc', 'auto', False)

    if result != 'fail' and result_cf != 'fail':
        return
    pytest.fail()


###############################################################################
# test copy and CF compliance for lat/lon (W*S84) file, tif->nc->tif
# note: this test fails in trunk (before r23246)
def test_netcdf_cf_3():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    result = 'success'
    result_cf = 'success'

    result = netcdf_test_copy('data/netcdf/trmm-wgs84.tif', 1, 14, 'tmp/netcdf_cf_3.nc')

    if result == 'success':
        # tst = gdaltest.GDALTest( 'GTIFF', '../tmp/netcdf_cf_3.nc', 1, 14 )
        # result = tst.testCreateCopy(check_gt=1, check_srs=1, new_filename='tmp/netcdf_cf_3.tif', delete_copy = 0)
        result = netcdf_test_copy('tmp/netcdf_cf_3.nc', 1, 14, 'tmp/netcdf_cf_3.tif', [], 'GTIFF')

    result_cf = 'success'
    if gdaltest.netcdf_cf_method is not None:
        result_cf = netcdf_cf_check_file('tmp/netcdf_cf_3.nc', 'auto', False)

    if result != 'fail' and result_cf != 'fail':
        return
    pytest.fail()

###############################################################################
# test support for various CF projections


def test_netcdf_cf_4():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    result = netcdf_cfproj_testcopy(netcdf_cfproj_tuples, 'netcdf/melb-small.tif',
                                    netcdf_cfproj_int_fmt_maps,
                                    'data', 'tmp', 'translate_results.txt')
#    result = netcdf_cfproj_testcopy(netcdf_cfproj_tuples1, 'netcdf/melb-small.tif', \
#                                    'data', 'tmp', 'translate_results.txt')

    return result

###############################################################################
# test CF support for dims and variables in different groups

def test_netcdf_cf_6():

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    ifiles = ('data/netcdf/cf_dimsindiff_4326.nc',
              'NETCDF:data/netcdf/cf_nasa_4326.nc:/science/grids/data/temp',
              'NETCDF:data/netcdf/cf_nasa_4326.nc:/science/grids/imagingGeometry/lookAngle')
    for ifile in ifiles:
        ds = gdal.Open(ifile)
        prj = ds.GetProjection()
        sr = osr.SpatialReference()
        sr.ImportFromWkt(prj)
        proj_out = sr.ExportToProj4()

        assert proj_out == '+proj=longlat +ellps=WGS84 +no_defs'


###############################################################################
# test check sums
def test_netcdf_cf_7(netcdf_setup):  # noqa
    # setup netcdf and netcdf_cf environment
    netcdf_cf_setup()

    if gdaltest.netcdf_drv is None:
        pytest.skip()

    checks = (('data/netcdf/cf_dimsindiff_4326.nc', 1, 2041),
              ('NETCDF:data/netcdf/cf_nasa_4326.nc:/science/grids/data/temp', 1, 2041),
              ('NETCDF:data/netcdf/cf_nasa_4326.nc:/science/grids/imagingGeometry/lookAngle', 1, 476),
              ('NETCDF:data/netcdf/cf_nasa_4326.nc:/science/grids/imagingGeometry/lookAngle', 4, 476))

    for infile, band, checksum in checks:
        ds = gdal.Open(infile, gdal.GA_ReadOnly)
        assert ds.GetRasterBand(band).Checksum() == checksum

