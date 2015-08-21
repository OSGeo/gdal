# -*- coding: utf-8 -*-
###############################################################################
# $Id$
# 
# Project:  GDAL/OGR Test Suite
# Purpose:  Python Library supporting GDAL/OGR Test Suite
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

import contextlib
import os
import sys
import time

from osgeo import gdal
from osgeo import osr

cur_name = 'default'

success_counter = 0
failure_counter = 0
expected_failure_counter = 0
blow_counter = 0
skip_counter = 0
failure_summary = []

reason = None
count_skipped_tests_download = 0
count_skipped_tests_slow = 0
start_time = None
end_time = None

jp2kak_drv = None
jpeg2000_drv = None
jp2ecw_drv = None
jp2mrsid_drv = None
jp2openjpeg_drv = None
jp2kak_drv_unregistered = False
jpeg2000_drv_unregistered = False
jp2ecw_drv_unregistered = False
jp2mrsid_drv_unregistered = False
jp2openjpeg_drv_unregistered = False

from sys import version_info
if version_info >= (3,0,0):
    import gdaltest_python3 as gdaltestaux
else:
    import gdaltest_python2 as gdaltestaux

# Process commandline arguments for stuff like --debug, --locale, --config

argv = gdal.GeneralCmdLineProcessor( sys.argv )

###############################################################################

def setup_run( name ):

    if 'APPLY_LOCALE' in os.environ:
        import locale
        locale.setlocale(locale.LC_ALL, '')

    global cur_name
    cur_name = name

###############################################################################

def run_tests( test_list ):
    global success_counter, failure_counter, expected_failure_counter, blow_counter, skip_counter
    global reason, failure_summary, cur_name
    global start_time, end_time

    set_time = start_time is None
    if set_time:
        start_time = time.time()
    had_errors_this_script = 0
    
    for test_item in test_list:
        if test_item is None:
            continue

        try:
            (func, name) = test_item
            if func.__name__[:4] == 'test':
                outline = '  TEST: ' + func.__name__[4:] + ': ' + name + ' ... ' 
            else:
                outline = '  TEST: ' + func.__name__ + ': ' + name + ' ... ' 
        except:
            func = test_item
            name = func.__name__
            outline =  '  TEST: ' + name + ' ... '

        sys.stdout.write( outline )
        sys.stdout.flush()
            
        reason = None
        result = run_func(func)
        
        if result[:4] == 'fail':
            if had_errors_this_script == 0:
                failure_summary.append( 'Script: ' + cur_name )
                had_errors_this_script = 1
            failure_summary.append( outline + result )
            if reason is not None:
                failure_summary.append( '    ' + reason )

        if reason is not None:
            print(('    ' + reason))

        if result == 'success':
            success_counter = success_counter + 1
        elif result == 'expected_fail':
            expected_failure_counter = expected_failure_counter + 1
        elif result == 'fail':
            failure_counter = failure_counter + 1
        elif result == 'skip':
            skip_counter = skip_counter + 1
        else:
            blow_counter = blow_counter + 1

    if set_time:
        end_time = time.time()

###############################################################################

def get_lineno_2framesback( frames ):
    try:
        import inspect
        frame = inspect.currentframe()
        while frames > 0:
            frame = frame.f_back
            frames = frames-1

        return frame.f_lineno
    except:
        return -1

###############################################################################

def post_reason( msg, frames=2 ):
    lineno = get_lineno_2framesback( frames )
    global reason

    if lineno >= 0:
        reason = 'line %d: %s' % (lineno, msg)
    else:
        reason = msg

###############################################################################

def summarize():
    global count_skipped_tests_download, count_skipped_tests_slow
    global success_counter, failure_counter, blow_counter, skip_counter
    global cur_name
    global start_time, end_time
    
    print('')
    if cur_name is not None:
        print('Test Script: %s' % cur_name)
    print('Succeeded: %d' % success_counter)
    print('Failed:    %d (%d blew exceptions)' \
          % (failure_counter+blow_counter, blow_counter))
    print('Skipped:   %d' % skip_counter)
    print('Expected fail:%d' % expected_failure_counter)
    if start_time is not None:
        duration = end_time - start_time
        if duration >= 60:
            print('Duration:  %02dm%02.1fs' % (duration / 60., duration % 60.))
        else:
            print('Duration:  %02.2fs' % duration)
    if count_skipped_tests_download != 0:
        print('As GDAL_DOWNLOAD_TEST_DATA environment variable is not defined, %d tests relying on data to downloaded from the Web have been skipped' % count_skipped_tests_download)
    if count_skipped_tests_slow != 0:
        print('As GDAL_RUN_SLOW_TESTS environment variable is not defined, %d "slow" tests have been skipped' % count_skipped_tests_slow)
    print('')

    sys.path.append( 'gcore' )
    sys.path.append( '../gcore' )
    import testnonboundtoswig
    # Do it twice to ensure that cleanup routines properly do their jobs
    for i in range(2):
        testnonboundtoswig.OSRCleanup()
        testnonboundtoswig.GDALDestroyDriverManager()
        testnonboundtoswig.OGRCleanupAll()

    return failure_counter + blow_counter

###############################################################################

def run_all( dirlist, option_list ):

    global start_time, end_time
    global cur_name

    start_time = time.time()

    for dir_name in dirlist:
        files = os.listdir(dir_name)

        old_path = sys.path
        sys.path.append('.')
        
        for file in files:
            if not file[-3:] == '.py':
                continue

            module = file[:-3]
            try:
                wd = os.getcwd()
                os.chdir( dir_name )
                
                exec("import " + module)
                try:
                    print('Running tests from %s/%s' % (dir_name,file))
                    setup_run( '%s/%s' % (dir_name,file) )
                    exec("run_tests( " + module + ".gdaltest_list)")
                except:
                    pass
                
                os.chdir( wd )

            except:
                os.chdir( wd )
                print('... failed to load %s ... skipping.' % file)

                import traceback
                traceback.print_exc()

        # We only add the tool directory to the python path long enough
        # to load the tool files.
        sys.path = old_path

    end_time = time.time()
    cur_name = None

    if len(failure_summary) > 0:
        print('')
        print(' ------------ Failures ------------')
        for item in failure_summary:
            print(item)
        print(' ----------------------------------')

###############################################################################

def clean_tmp():
    all_files = os.listdir('tmp')
    for file in all_files:
        if file == 'CVS' or file == 'do-not-remove':
            continue

        try:
            os.remove( 'tmp/' + file )
        except:
            pass
    return 'success'

###############################################################################
def testCreateCopyInterruptCallback(pct, message, user_data):
    if pct > 0.5:
        return 0 # to stop
    else:
        return 1 # to continue

###############################################################################

class GDALTest:
    def __init__(self, drivername, filename, band, chksum,
                 xoff = 0, yoff = 0, xsize = 0, ysize = 0, options = [],
                 filename_absolute = 0 ):
        self.driver = None
        self.drivername = drivername
        self.filename = filename
        self.filename_absolute = filename_absolute
        self.band = band
        self.chksum = chksum
        self.xoff = xoff
        self.yoff = yoff
        self.xsize = xsize
        self.ysize = ysize
        self.options = options

    def testDriver(self):
        if self.driver is None:
            self.driver = gdal.GetDriverByName( self.drivername )
            if self.driver is None:
                post_reason( self.drivername + ' driver not found!' )
                return 'fail'

        return 'success'

    def testOpen(self, check_prj = None, check_gt = None, gt_epsilon = None, \
                 check_stat = None, check_approx_stat = None, \
                 stat_epsilon = None, skip_checksum = None):
        """check_prj - projection reference, check_gt - geotransformation
        matrix (tuple), gt_epsilon - geotransformation tolerance,
        check_stat - band statistics (tuple), stat_epsilon - statistics
        tolerance."""
        if self.testDriver() == 'fail':
            return 'skip'

        if self.filename_absolute:
            wrk_filename = self.filename
        else:
            wrk_filename = 'data/' + self.filename

        ds = gdal.Open( wrk_filename, gdal.GA_ReadOnly )

        if ds is None:
            post_reason( 'Failed to open dataset: ' + wrk_filename )
            return 'fail'
            
        if ds.GetDriver().ShortName != gdal.GetDriverByName( self.drivername ).ShortName:
            post_reason( 'The driver of the returned dataset is %s instead of %s.' % ( ds.GetDriver().ShortName, self.drivername ) )
            return 'fail'
            
        if self.xsize == 0 and self.ysize == 0:
            self.xsize = ds.RasterXSize
            self.ysize = ds.RasterYSize

        # Do we need to check projection?
        if check_prj is not None:
            new_prj = ds.GetProjection()
            
            src_osr = osr.SpatialReference()
            src_osr.SetFromUserInput( check_prj )
            
            new_osr = osr.SpatialReference( wkt=new_prj )

            if not src_osr.IsSame(new_osr):
                print('')
                print('old = %s' % src_osr.ExportToPrettyWkt())
                print('new = %s' % new_osr.ExportToPrettyWkt())
                post_reason( 'Projections differ' )
                return 'fail'

        # Do we need to check geotransform?
        if check_gt:
            # Default to 100th of pixel as our test value.
            if gt_epsilon is None:
                gt_epsilon = (abs(check_gt[1])+abs(check_gt[2])) / 100.0
                
            new_gt = ds.GetGeoTransform()
            for i in range(6):
                if abs(new_gt[i]-check_gt[i]) > gt_epsilon:
                    print('')
                    print('old = ', check_gt)
                    print('new = ', new_gt)
                    post_reason( 'Geotransform differs.' )
                    return 'fail'

        oBand = ds.GetRasterBand(self.band)
        if skip_checksum is None:
            chksum = oBand.Checksum(self.xoff, self.yoff, self.xsize, self.ysize)

        # Do we need to check approximate statistics?
        if check_approx_stat:
            # Default to 1000th of pixel value range as our test value.
            if stat_epsilon is None:
                stat_epsilon = \
                    abs(check_approx_stat[1] - check_approx_stat[0]) / 1000.0

            new_stat = oBand.GetStatistics(1, 1)
            for i in range(4):

                # NOTE - mloskot: Poor man Nan/Inf value check. It's poor
                # because we need to support old and buggy Python 2.3.
                # Tested on Linux, Mac OS X and Windows, with Python 2.3/2.4/2.5.
                sv = str(new_stat[i]).lower()
                if sv.find('n') >= 0 or sv.find('i') >= 0 or sv.find('#') >= 0:
                    post_reason( 'NaN or Invinite value encountered '%'.' % sv )
                    return 'fail'

                if abs(new_stat[i]-check_approx_stat[i]) > stat_epsilon:
                    print('')
                    print('old = ', check_approx_stat)
                    print('new = ', new_stat)
                    post_reason( 'Approximate statistics differs.' )
                    return 'fail'

        # Do we need to check statistics?
        if check_stat:
            # Default to 1000th of pixel value range as our test value.
            if stat_epsilon is None:
                stat_epsilon = abs(check_stat[1] - check_stat[0]) / 1000.0

            # FIXME: how to test approximate statistic results?
            new_stat = oBand.GetStatistics(1, 1)

            new_stat = oBand.GetStatistics(0, 1)
            for i in range(4):

                sv = str(new_stat[i]).lower()
                if sv.find('n') >= 0 or sv.find('i') >= 0 or sv.find('#') >= 0:
                    post_reason( 'NaN or Invinite value encountered '%'.' % sv )
                    return 'fail'

                if abs(new_stat[i]-check_stat[i]) > stat_epsilon:
                    print('')
                    print('old = ', check_stat)
                    print('new = ', new_stat)
                    post_reason( 'Statistics differs.' )
                    return 'fail'

        ds = None

        if is_file_open(wrk_filename):
            post_reason('file still open after dataset closing')
            return 'fail'

        if skip_checksum is not None:
            return 'success'
        elif self.chksum is None or chksum == self.chksum:
            return 'success'
        else:
            post_reason('Checksum for band %d in "%s" is %d, but expected %d.' \
                        % (self.band, self.filename, chksum, self.chksum) )
            return 'fail'

    def testCreateCopy(self, check_minmax = 1, check_gt = 0, check_srs = None,
                       vsimem = 0, new_filename = None, strict_in = 0,
                       skip_preclose_test = 0, delete_copy = 1, gt_epsilon = None,
                       check_checksum_not_null = None, interrupt_during_copy = False):

        if self.testDriver() == 'fail':
            return 'skip'

        if self.filename_absolute:
            wrk_filename = self.filename
        else:
            wrk_filename = 'data/' + self.filename

        src_ds = gdal.Open( wrk_filename )
        if self.band > 0:
            minmax = src_ds.GetRasterBand(self.band).ComputeRasterMinMax()
            
        src_prj = src_ds.GetProjection()
        src_gt = src_ds.GetGeoTransform()

        if new_filename is None:
            if vsimem:
                new_filename = '/vsimem/' + self.filename + '.tst'
            else:
                new_filename = 'tmp/' + self.filename + '.tst'

        gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
        if interrupt_during_copy:
            new_ds = self.driver.CreateCopy( new_filename, src_ds,
                                         strict = strict_in,
                                         options = self.options,
                                         callback = testCreateCopyInterruptCallback)
        else:
            new_ds = self.driver.CreateCopy( new_filename, src_ds,
                                            strict = strict_in,
                                            options = self.options )
        gdal.PopErrorHandler()

        if interrupt_during_copy:
            if new_ds is None:
                gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
                self.driver.Delete( new_filename )
                gdal.PopErrorHandler()
                return 'success'
            else:
                post_reason( 'CreateCopy() should have failed due to interruption')
                new_ds = None
                self.driver.Delete( new_filename )
                return 'fail'

        if new_ds is None:
            post_reason( 'Failed to create test file using CreateCopy method.'\
                         + '\n' + gdal.GetLastErrorMsg() )
            return 'fail'
            
        if new_ds.GetDriver().ShortName != gdal.GetDriverByName( self.drivername ).ShortName:
            post_reason( 'The driver of the returned dataset is %s instead of %s.' % ( new_ds.GetDriver().ShortName, self.drivername ) )
            return 'fail'

        if self.band > 0 and skip_preclose_test == 0:
            bnd = new_ds.GetRasterBand(self.band)
            if check_checksum_not_null is True:
                if bnd.Checksum() == 0:
                    post_reason('Got null checksum on still-open file.')
                    return 'fail'
            elif self.chksum is not None and bnd.Checksum() != self.chksum:
                post_reason(
                    'Did not get expected checksum on still-open file.\n' \
                    '    Got %d instead of %d.' % (bnd.Checksum(),self.chksum))
                return 'fail'
            if check_minmax:
                got_minmax = bnd.ComputeRasterMinMax()
                if got_minmax != minmax:
                    post_reason( \
                    'Did not get expected min/max values on still-open file.\n' \
                    '    Got %g,%g instead of %g,%g.' \
                    % ( got_minmax[0], got_minmax[1], minmax[0], minmax[1] ) )
                    return 'fail'

        bnd = None
        new_ds = None

        # hopefully it's closed now!

        new_ds = gdal.Open( new_filename )
        if new_ds is None:
            post_reason( 'Failed to open dataset: ' + new_filename )
            return 'fail'

        if self.band > 0:
            bnd = new_ds.GetRasterBand(self.band)
            if check_checksum_not_null is True:
                if bnd.Checksum() == 0:
                    post_reason('Got null checksum on reopened file.')
                    return 'fail'
            elif self.chksum is not None and bnd.Checksum() != self.chksum:
                post_reason( 'Did not get expected checksum on reopened file.\n'
                             '    Got %d instead of %d.' \
                             % (bnd.Checksum(), self.chksum) )
                return 'fail'

            if check_minmax:
                got_minmax = bnd.ComputeRasterMinMax()
                if got_minmax != minmax:
                    post_reason( \
                    'Did not get expected min/max values on reopened file.\n' \
                    '    Got %g,%g instead of %g,%g.' \
                    % ( got_minmax[0], got_minmax[1], minmax[0], minmax[1] ) )
                    return 'fail'

        # Do we need to check the geotransform?
        if check_gt:
            if gt_epsilon is None: 
                eps = 0.00000001
            else:
                eps = gt_epsilon
            new_gt = new_ds.GetGeoTransform()
            if abs(new_gt[0] - src_gt[0]) > eps \
               or abs(new_gt[1] - src_gt[1]) > eps \
               or abs(new_gt[2] - src_gt[2]) > eps \
               or abs(new_gt[3] - src_gt[3]) > eps \
               or abs(new_gt[4] - src_gt[4]) > eps \
               or abs(new_gt[5] - src_gt[5]) > eps:
                print('')
                print('old = ', src_gt)
                print('new = ', new_gt)
                post_reason( 'Geotransform differs.' )
                return 'fail'

        # Do we need to check the geotransform?
        if check_srs is not None:
            new_prj = new_ds.GetProjection()
            
            src_osr = osr.SpatialReference( wkt=src_prj )
            new_osr = osr.SpatialReference( wkt=new_prj )

            if not src_osr.IsSame(new_osr):
                print('')
                print('old = %s' % src_osr.ExportToPrettyWkt())
                print('new = %s' % new_osr.ExportToPrettyWkt())
                post_reason( 'Projections differ' )
                return 'fail'

        bnd = None
        new_ds = None
        src_ds = None

        if gdal.GetConfigOption( 'CPL_DEBUG', 'OFF' ) != 'ON' and delete_copy == 1 :
            self.driver.Delete( new_filename )

        return 'success'

    def testCreate(self, vsimem = 0, new_filename = None, out_bands = 1,
                   check_minmax = 1 ):
        if self.testDriver() == 'fail':
            return 'skip'

        if self.filename_absolute:
            wrk_filename = self.filename
        else:
            wrk_filename = 'data/' + self.filename

        src_ds = gdal.Open( wrk_filename )
        xsize = src_ds.RasterXSize
        ysize = src_ds.RasterYSize
        src_img = src_ds.GetRasterBand(self.band).ReadRaster(0,0,xsize,ysize)
        minmax = src_ds.GetRasterBand(self.band).ComputeRasterMinMax()

        if new_filename is None:
            if vsimem:
                new_filename = '/vsimem/' + self.filename + '.tst'
            else:
                new_filename = 'tmp/' + self.filename + '.tst'

        new_ds = self.driver.Create( new_filename, xsize, ysize, out_bands,
                                     src_ds.GetRasterBand(self.band).DataType,
                                     options = self.options )
        if new_ds is None:
            post_reason( 'Failed to create test file using Create method.' )
            return 'fail'
        
        src_ds = None

        try:
            for band in range(1,out_bands+1):
                new_ds.GetRasterBand(band).WriteRaster( 0, 0, xsize, ysize, src_img )
        except:
            post_reason( 'Failed to write raster bands to test file.' )
            return 'fail'

        for band in range(1,out_bands+1):
            if self.chksum is not None \
               and new_ds.GetRasterBand(band).Checksum() != self.chksum:
                post_reason(
                    'Did not get expected checksum on still-open file.\n' \
                    '    Got %d instead of %d.' \
                    % (new_ds.GetRasterBand(band).Checksum(),self.chksum))

                return 'fail'

            computed_minmax = new_ds.GetRasterBand(band).ComputeRasterMinMax()
            if computed_minmax != minmax and check_minmax:
                post_reason( 'Did not get expected min/max values on still-open file.' )
                print('expect: ', minmax)
                print('got: ', computed_minmax) 
                return 'fail'
        
        new_ds = None

        new_ds = gdal.Open( new_filename )
        if new_ds is None:
            post_reason( 'Failed to open dataset: ' + new_filename )
            return 'fail'

        for band in range(1,out_bands+1):
            if self.chksum is not None \
               and new_ds.GetRasterBand(band).Checksum() != self.chksum:
                post_reason( 'Did not get expected checksum on reopened file.' \
                    '    Got %d instead of %d.' \
                    % (new_ds.GetRasterBand(band).Checksum(),self.chksum))
                return 'fail'
            
            if new_ds.GetRasterBand(band).ComputeRasterMinMax() != minmax and check_minmax:
                post_reason( 'Did not get expected min/max values on reopened file.' )
                return 'fail'
        
        new_ds = None
        
        if gdal.GetConfigOption( 'CPL_DEBUG', 'OFF' ) != 'ON':
            self.driver.Delete( new_filename )

        return 'success'

    def testSetGeoTransform(self):
        if self.testDriver() == 'fail':
            return 'skip'

        src_ds = gdal.Open( 'data/' + self.filename )
        xsize = src_ds.RasterXSize
        ysize = src_ds.RasterYSize

        new_filename = 'tmp/' + self.filename + '.tst'
        new_ds = self.driver.Create( new_filename, xsize, ysize, 1,
                                     src_ds.GetRasterBand(self.band).DataType,
                                     options = self.options  )
        if new_ds is None:
            post_reason( 'Failed to create test file using Create method.' )
            return 'fail'
        
        gt = (123.0, 1.18, 0.0, 456.0, 0.0, -1.18 )
        if new_ds.SetGeoTransform( gt ) is not gdal.CE_None:
            post_reason( 'Failed to set geographic transformation.' )
            return 'fail'

        src_ds = None
        new_ds = None

        new_ds = gdal.Open( new_filename )
        if new_ds is None:
            post_reason( 'Failed to open dataset: ' + new_filename )
            return 'fail'

        eps = 0.00000001
        new_gt = new_ds.GetGeoTransform()
        if abs(new_gt[0] - gt[0]) > eps \
            or abs(new_gt[1] - gt[1]) > eps \
            or abs(new_gt[2] - gt[2]) > eps \
            or abs(new_gt[3] - gt[3]) > eps \
            or abs(new_gt[4] - gt[4]) > eps \
            or abs(new_gt[5] - gt[5]) > eps:
            print('')
            print('old = ', gt)
            print('new = ', new_gt)
            post_reason( 'Did not get expected geotransform.' )
            return 'fail'

        new_ds = None
        
        if gdal.GetConfigOption( 'CPL_DEBUG', 'OFF' ) != 'ON':
            self.driver.Delete( new_filename )

        return 'success'

    def testSetProjection(self, prj = None, expected_prj = None ):
        if self.testDriver() == 'fail':
            return 'skip'

        src_ds = gdal.Open( 'data/' + self.filename )
        xsize = src_ds.RasterXSize
        ysize = src_ds.RasterYSize

        new_filename = 'tmp/' + self.filename + '.tst'
        new_ds = self.driver.Create( new_filename, xsize, ysize, 1,
                                     src_ds.GetRasterBand(self.band).DataType,
                                     options = self.options  )
        if new_ds is None:
            post_reason( 'Failed to create test file using Create method.' )
            return 'fail'
        
        gt = (123.0, 1.18, 0.0, 456.0, 0.0, -1.18 )
        if prj is None:
            # This is a challenging SRS since it has non-meter linear units.
            prj='PROJCS["NAD83 / Ohio South",GEOGCS["NAD83",DATUM["North_American_Datum_1983",SPHEROID["GRS 1980",6378137,298.257222101,AUTHORITY["EPSG","7019"]],AUTHORITY["EPSG","6269"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.01745329251994328,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4269"]],PROJECTION["Lambert_Conformal_Conic_2SP"],PARAMETER["standard_parallel_1",40.03333333333333],PARAMETER["standard_parallel_2",38.73333333333333],PARAMETER["latitude_of_origin",38],PARAMETER["central_meridian",-82.5],PARAMETER["false_easting",1968500],PARAMETER["false_northing",0],UNIT["feet",0.3048006096012192]]'

        src_osr = osr.SpatialReference()
        src_osr.ImportFromWkt(prj)
        
        new_ds.SetGeoTransform( gt )
        if new_ds.SetProjection( prj ) is not gdal.CE_None:
            post_reason( 'Failed to set geographic projection string.' )
            return 'fail'

        src_ds = None
        new_ds = None

        new_ds = gdal.Open( new_filename )
        if new_ds is None:
            post_reason( 'Failed to open dataset: ' + new_filename )
            return 'fail'

        expected_osr = osr.SpatialReference()
        if expected_prj is None:
            expected_osr = src_osr
        else:
            expected_osr.ImportFromWkt( expected_prj )
            
        new_osr = osr.SpatialReference()
        new_osr.ImportFromWkt(new_ds.GetProjection())
        if not new_osr.IsSame(expected_osr):
            post_reason( 'Did not get expected projection reference.' )
            print('Got: ')
            print(new_osr.ExportToPrettyWkt())
            print('Expected:')
            print(expected_osr.ExportToPrettyWkt())
            return 'fail'

        new_ds = None
        
        if gdal.GetConfigOption( 'CPL_DEBUG', 'OFF' ) != 'ON':
            self.driver.Delete( new_filename )

        return 'success'

    def testSetMetadata(self):
        if self.testDriver() == 'fail':
            return 'skip'

        src_ds = gdal.Open( 'data/' + self.filename )
        xsize = src_ds.RasterXSize
        ysize = src_ds.RasterYSize

        new_filename = 'tmp/' + self.filename + '.tst'
        new_ds = self.driver.Create( new_filename, xsize, ysize, 1,
                                     src_ds.GetRasterBand(self.band).DataType,
                                     options = self.options  )
        if new_ds is None:
            post_reason( 'Failed to create test file using Create method.' )
            return 'fail'
        
        dict = {}
        dict['TEST_KEY'] = 'TestValue'
        new_ds.SetMetadata( dict )
# FIXME
        #if new_ds.SetMetadata( dict ) is not gdal.CE_None:
            #print new_ds.SetMetadata( dict )
            #post_reason( 'Failed to set metadata item.' )
            #return 'fail'

        src_ds = None
        new_ds = None

        new_ds = gdal.Open( new_filename )
        if new_ds is None:
            post_reason( 'Failed to open dataset: ' + new_filename )
            return 'fail'

        md_dict = new_ds.GetMetadata()

        if (not 'TEST_KEY' in md_dict):
            post_reason( 'Metadata item TEST_KEY does not exist.')
            return 'fail'

        if md_dict['TEST_KEY'] != 'TestValue':
            post_reason( 'Did not get expected metadata item.' )
            return 'fail'

        new_ds = None
        
        if gdal.GetConfigOption( 'CPL_DEBUG', 'OFF' ) != 'ON':
            self.driver.Delete( new_filename )

        return 'success'

    def testSetNoDataValue(self, delete = False):
        if self.testDriver() == 'fail':
            return 'skip'

        src_ds = gdal.Open( 'data/' + self.filename )
        xsize = src_ds.RasterXSize
        ysize = src_ds.RasterYSize

        new_filename = 'tmp/' + self.filename + '.tst'
        new_ds = self.driver.Create( new_filename, xsize, ysize, 1,
                                     src_ds.GetRasterBand(self.band).DataType,
                                     options = self.options  )
        if new_ds is None:
            post_reason( 'Failed to create test file using Create method.' )
            return 'fail'
        
        nodata = 11
        if new_ds.GetRasterBand(1).SetNoDataValue(nodata) is not gdal.CE_None:
            post_reason( 'Failed to set NoData value.' )
            return 'fail'

        src_ds = None
        new_ds = None

        if delete:
            mode = gdal.GA_Update
        else:
            mode = gdal.GA_ReadOnly
        new_ds = gdal.Open( new_filename, mode )
        if new_ds is None:
            post_reason( 'Failed to open dataset: ' + new_filename )
            return 'fail'

        if nodata != new_ds.GetRasterBand(1).GetNoDataValue():
            post_reason( 'Did not get expected NoData value.' )
            return 'fail'

        if delete:
            if new_ds.GetRasterBand(1).DeleteNoDataValue() != 0:
                post_reason( 'Did not manage to delete nodata value' )
                return 'fail'

        new_ds = None
        
        if delete:
            new_ds = gdal.Open (new_filename)
            if new_ds.GetRasterBand(1).GetNoDataValue() is not None:
                post_reason( 'Got nodata value whereas none was expected' )
                return 'fail'
            new_ds = None
        
        if gdal.GetConfigOption( 'CPL_DEBUG', 'OFF' ) != 'ON':
            self.driver.Delete( new_filename )

        return 'success'

    def testSetNoDataValueAndDelete(self):
        return self.testSetNoDataValue(delete = True)
        
    def testSetDescription(self):
        if self.testDriver() == 'fail':
            return 'skip'

        src_ds = gdal.Open( 'data/' + self.filename )
        xsize = src_ds.RasterXSize
        ysize = src_ds.RasterYSize

        new_filename = 'tmp/' + self.filename + '.tst'
        new_ds = self.driver.Create( new_filename, xsize, ysize, 1,
                                     src_ds.GetRasterBand(self.band).DataType,
                                     options = self.options  )
        if new_ds is None:
            post_reason( 'Failed to create test file using Create method.' )
            return 'fail'
        
        description = "Description test string"
        new_ds.GetRasterBand(1).SetDescription(description)

        src_ds = None
        new_ds = None

        new_ds = gdal.Open( new_filename )
        if new_ds is None:
            post_reason( 'Failed to open dataset: ' + new_filename )
            return 'fail'

        if description != new_ds.GetRasterBand(1).GetDescription():
            post_reason( 'Did not get expected description string.' )
            return 'fail'

        new_ds = None
        
        if gdal.GetConfigOption( 'CPL_DEBUG', 'OFF' ) != 'ON':
            self.driver.Delete( new_filename )

        return 'success'


def approx_equal( a, b ):
    a = float(a)
    b = float(b)
    if a == 0 and b != 0:
        return 0

    if abs(b/a - 1.0) > .00000000001:
        return 0
    else:
        return 1
    
    
def user_srs_to_wkt( user_text ):
    srs = osr.SpatialReference()
    srs.SetFromUserInput( user_text )
    return srs.ExportToWkt()

def equal_srs_from_wkt( expected_wkt, got_wkt ):
    expected_srs = osr.SpatialReference()
    expected_srs.ImportFromWkt( expected_wkt )

    got_srs = osr.SpatialReference()
    got_srs.ImportFromWkt( got_wkt )

    if got_srs.IsSame( expected_srs ):
        return 1
    else:
        print('Expected:\n%s' % expected_wkt)
        print('Got:     \n%s' % got_wkt)
        
        post_reason( 'SRS differs from expected.' )
        return 0

    
###############################################################################
# Compare two sets of RPC metadata, and establish if they are essentially
# equivelent or not. 

def rpcs_equal( md1, md2 ):

    simple_fields = [ 'LINE_OFF', 'SAMP_OFF', 'LAT_OFF', 'LONG_OFF',
                      'HEIGHT_OFF', 'LINE_SCALE', 'SAMP_SCALE', 'LAT_SCALE',
                      'LONG_SCALE', 'HEIGHT_SCALE' ]
    coef_fields = [ 'LINE_NUM_COEFF', 'LINE_DEN_COEFF',
                    'SAMP_NUM_COEFF', 'SAMP_DEN_COEFF' ]

    for sf in simple_fields:

        try:
            if not approx_equal(float(md1[sf]),float(md2[sf])):
                post_reason( '%s values differ.' % sf )
                print(md1[sf])
                print(md2[sf])
                return 0
        except:
            post_reason( '%s value missing or corrupt.' % sf )
            print(md1)
            print(md2)
            return 0

    for cf in coef_fields:

        try:
            list1 = md1[cf].split()
            list2 = md2[cf].split()

        except:
            post_reason( '%s value missing or corrupt.' % cf )
            print(md1[cf])
            print(md2[cf])
            return 0

        if len(list1) != 20:
            post_reason( '%s value list length wrong(1)' % cf )
            print(list1)
            return 0

        if len(list2) != 20:
            post_reason( '%s value list length wrong(2)' % cf )
            print(list2)
            return 0

        for i in range(20):
            if not approx_equal(float(list1[i]),float(list2[i])):
                post_reason( '%s[%d] values differ.' % (cf,i) )
                print(list1[i], list2[i])
                return 0

    return 1

###############################################################################
# Test if geotransforms are equal with an epsilon tolerance
#

def geotransform_equals(gt1, gt2, gt_epsilon):
    for i in range(6):
        if abs(gt1[i]-gt2[i]) > gt_epsilon:
            print('')
            print('gt1 = ', gt1)
            print('gt2 = ', gt2)
            post_reason( 'Geotransform differs.' )
            return False
    return True


###############################################################################
# Download file at url 'url' and put it as 'filename' in 'tmp/cache/'
#
# If 'filename' already exits in 'tmp/cache/', it is not downloaded
# If GDAL_DOWNLOAD_TEST_DATA is not defined, the function fails
# If GDAL_DOWNLOAD_TEST_DATA is defined, 'url' is downloaded  as 'filename' in 'tmp/cache/'

def download_file(url, filename, download_size = -1, force_download = False, max_download_duration = None, base_dir = 'tmp/cache'):
    
    if filename.startswith(base_dir + '/'):
        filename = filename[len(base_dir + '/'):]

    global count_skipped_tests_download
    try:
        os.stat( base_dir + '/' + filename )
        return True
    except:
        if 'GDAL_DOWNLOAD_TEST_DATA' in os.environ or force_download:
            val = None
            import time
            start_time = time.time()
            try:
                handle = gdalurlopen(url)
                if handle is None:
                    return False
                if download_size == -1:
                    try:
                        handle_info = handle.info()
                        download_size = int(handle_info['content-length'])
                        print('Downloading %s (length = %d bytes)...' % (url, download_size))
                    except:
                        print('Downloading %s...' % (url))
                else:
                    print('Downloading %d bytes from %s...' % (download_size, url))
            except:
                return False

            if download_size >= 0:
                sys.stdout.write('Progress: ')
            nLastTick = -1
            val = ''.encode('ascii')
            while len(val) < download_size or download_size < 0:
                chunk_size = 1024
                if download_size >= 0 and len(val) + chunk_size > download_size:
                    chunk_size = download_size - len(val)
                try:
                    chunk = handle.read(chunk_size)
                except:
                    print('Did not get expected data length.')
                    return False
                if len(chunk) < chunk_size:
                    if download_size < 0:
                        break
                    print('Did not get expected data length.')
                    return False
                val = val + chunk
                if download_size >= 0:
                    nThisTick = int(40 * len(val) / download_size)
                    while nThisTick > nLastTick:
                        nLastTick = nLastTick + 1
                        if nLastTick % 4 == 0:
                            sys.stdout.write( "%d" % int((nLastTick / 4) * 10) )
                        else:
                            sys.stdout.write(".")
                    nLastTick = nThisTick
                    if nThisTick == 40:
                        sys.stdout.write(" - done.\n" )

                current_time = time.time()
                if max_download_duration is not None and current_time - start_time > max_download_duration:
                    print('Download aborted due to timeout.')
                    return False

            try:
                os.stat( base_dir )
            except:
                os.mkdir(base_dir)

            try:
                open( base_dir + '/' + filename, 'wb').write(val)
                return True
            except:
                print('Cannot write %s' % (filename))
                return False
        else:
            if count_skipped_tests_download == 0:
                print('As GDAL_DOWNLOAD_TEST_DATA environment variable is not defined, some tests relying on data to downloaded from the Web will be skipped')
            count_skipped_tests_download = count_skipped_tests_download + 1
            return False


###############################################################################
# GDAL data type to python struct format
def gdal_data_type_to_python_struct_format(datatype):
    type_char = 'B'
    if datatype == gdal.GDT_Int16:
        type_char = 'h'
    elif datatype == gdal.GDT_UInt16:
        type_char = 'H'
    elif datatype == gdal.GDT_Int32:
        type_char = 'i'
    elif datatype == gdal.GDT_UInt32:
        type_char = 'I'
    elif datatype == gdal.GDT_Float32:
        type_char = 'f'
    elif datatype == gdal.GDT_Float64:
        type_char = 'd'
    return type_char

###############################################################################
# Compare the values of the pixels

def compare_ds(ds1, ds2, xoff = 0, yoff = 0, width = 0, height = 0, verbose=1):
    import struct

    if width == 0:
        width = ds1.RasterXSize
    if height == 0:
        height = ds1.RasterYSize
    data1 = ds1.GetRasterBand(1).ReadRaster(xoff, yoff, width, height)
    type_char = gdal_data_type_to_python_struct_format(ds1.GetRasterBand(1).DataType)
    val_array1 = struct.unpack(type_char * width * height, data1)

    data2 = ds2.GetRasterBand(1).ReadRaster(xoff, yoff, width, height)
    type_char = gdal_data_type_to_python_struct_format(ds2.GetRasterBand(1).DataType)
    val_array2 = struct.unpack(type_char * width * height, data2)

    maxdiff = 0.0
    ndiffs = 0
    for i in range(width*height):
        diff = val_array1[i] - val_array2[i]
        if diff != 0:
            #print(val_array1[i])
            #print(val_array2[i])
            ndiffs = ndiffs + 1
            if abs(diff) > maxdiff:
                maxdiff = abs(diff)
                if verbose:
                    print("Diff at pixel (%d, %d) : %f" % (i % width, i / width, float(diff)))
            elif ndiffs < 10:
                if verbose:
                    print("Diff at pixel (%d, %d) : %f" % (i % width, i / width, float(diff)))
    if maxdiff != 0 and verbose:
        print("Max diff : %d" % (maxdiff))
        print("Number of diffs : %d" % (ndiffs))

    return maxdiff



###############################################################################
# Deregister all JPEG2000 drivers, except the one passed as an argument

def deregister_all_jpeg2000_drivers_but(name_of_driver_to_keep):
    global jp2kak_drv, jpeg2000_drv, jp2ecw_drv, jp2mrsid_drv, jp2openjpeg_drv
    global jp2kak_drv_unregistered,jpeg2000_drv_unregistered,jp2ecw_drv_unregistered,jp2mrsid_drv_unregistered,jp2openjpeg_drv_unregistered

    # Deregister other potential conflicting JPEG2000 drivers that will
    # be re-registered in the cleanup
    try:
        jp2kak_drv = gdal.GetDriverByName('JP2KAK')
        if name_of_driver_to_keep != 'JP2KAK' and jp2kak_drv:
            gdal.Debug('gdaltest','Deregistering JP2KAK')
            jp2kak_drv.Deregister()
            jp2kak_drv_unregistered = True
    except:
        pass

    try:
        jpeg2000_drv = gdal.GetDriverByName('JPEG2000')
        if name_of_driver_to_keep != 'JPEG2000' and jpeg2000_drv:
            gdal.Debug('gdaltest','Deregistering JPEG2000')
            jpeg2000_drv.Deregister()
            jpeg2000_drv_unregistered = True
    except:
        pass

    try:
        jp2ecw_drv = gdal.GetDriverByName('JP2ECW')
        if name_of_driver_to_keep != 'JP2ECW' and jp2ecw_drv:
            gdal.Debug('gdaltest.','Deregistering JP2ECW')
            jp2ecw_drv.Deregister()
            jp2ecw_drv_unregistered = True
    except:
        pass

    try:
        jp2mrsid_drv = gdal.GetDriverByName('JP2MrSID')
        if name_of_driver_to_keep != 'JP2MrSID' and jp2mrsid_drv:
            gdal.Debug('gdaltest.','Deregistering JP2MrSID')
            jp2mrsid_drv.Deregister()
            jp2mrsid_drv_unregistered = True
    except:
        pass

    try:
        jp2openjpeg_drv = gdal.GetDriverByName('JP2OpenJPEG')
        if name_of_driver_to_keep != 'JP2OpenJPEG' and jp2openjpeg_drv:
            gdal.Debug('gdaltest.','Deregistering JP2OpenJPEG')
            jp2openjpeg_drv.Deregister()
            jp2openjpeg_drv_unregistered = True
    except:
        pass

    return True

###############################################################################
# Re-register all JPEG2000 drivers previously disabled by
# deregister_all_jpeg2000_drivers_but

def reregister_all_jpeg2000_drivers():
    global jp2kak_drv, jpeg2000_drv, jp2ecw_drv, jp2mrsid_drv, jp2openjpeg_drv
    global jp2kak_drv_unregistered,jpeg2000_drv_unregistered,jp2ecw_drv_unregistered,jp2mrsid_drv_unregistered, jp2openjpeg_drv_unregistered

    try:
        if jp2kak_drv_unregistered:
            jp2kak_drv.Register()
            jp2kak_drv_unregistered = False
            gdal.Debug('gdaltest','Registering JP2KAK')
    except:
        pass

    try:
        if jpeg2000_drv_unregistered:
            jpeg2000_drv.Register()
            jpeg2000_drv_unregistered = False
            gdal.Debug('gdaltest','Registering JPEG2000')
    except:
        pass

    try:
        if jp2ecw_drv_unregistered:
            jp2ecw_drv.Register()
            jp2ecw_drv_unregistered = False
            gdal.Debug('gdaltest','Registering JP2ECW')
    except:
        pass

    try:
        if jp2mrsid_drv_unregistered:
            jp2mrsid_drv.Register()
            jp2mrsid_drv_unregistered = False
            gdal.Debug('gdaltest','Registering JP2MrSID')
    except:
        pass

    try:
        if jp2openjpeg_drv_unregistered:
            jp2openjpeg_drv.Register()
            jp2openjpeg_drv = False
            gdal.Debug('gdaltest','Registering JP2OpenJPEG')
    except:
        pass

    return True

###############################################################################
# Determine if the filesystem supports sparse files.
# Currently, this will only work on Linux (or any *NIX that has the stat
# command line utility)

def filesystem_supports_sparse_files(path):

    if skip_on_travis():
        return False

    try:
        (ret, err) = runexternal_out_and_err('stat -f -c "%T" ' + path)
    except:
        return False

    if err != '':
        post_reason('Cannot determine if filesystem supports sparse files')
        return False

    if ret.find('fat32') != -1:
        post_reason('File system does not support sparse files')
        return False

    # Add here any missing filesystem supporting sparse files
    # See http://en.wikipedia.org/wiki/Comparison_of_file_systems
    if ret.find('ext3') == -1 and \
        ret.find('ext4') == -1 and \
        ret.find('reiser') == -1 and \
        ret.find('xfs') == -1 and \
        ret.find('jfs') == -1 and \
        ret.find('zfs') == -1 and \
        ret.find('ntfs') == -1 :
        post_reason('Filesystem %s is not believed to support sparse files' % ret)
        return False

    return True

###############################################################################
# Unzip a file

def unzip(target_dir, zipfilename, verbose = False):

    try:
        import zipfile
        zf = zipfile.ZipFile(zipfilename)
    except:
        os.system('unzip -d ' + target_dir + ' ' + zipfilename)
        return

    for filename in zf.namelist():
        if verbose:
            print(filename)
        outfilename = os.path.join(target_dir, filename)
        if filename.endswith('/'):
            if not os.path.exists(outfilename):
               os.makedirs(outfilename)
        else:
            outdirname = os.path.dirname(outfilename)
            if not os.path.exists(outdirname):
               os.makedirs(outdirname)

            outfile = open(outfilename,'wb')
            outfile.write(zf.read(filename))
            outfile.close()

    return

###############################################################################
# Return if a number is the NaN number

def isnan(val):
    if val == val:
        # Python 2.3 unlike later versions return True for nan == nan
        val_str = '%f' % val
        if val_str == 'nan':
            return True
        else:
            return False
    else:
        return True


###############################################################################
# Return NaN

def NaN():
    try:
        # Python >= 2.6
        return float('nan')
    except:
        return 1e400 / 1e400

###############################################################################
# Return positive infinity

def posinf():
    try:
        # Python >= 2.6
        return float('inf')
    except:
        return 1e400
 
###############################################################################
# Return negative infinity

def neginf():
    try:
        # Python >= 2.6
        return float('-inf')
    except:
        return -1e400

###############################################################################
# Has the user requested to run the slow tests

def run_slow_tests():
    global count_skipped_tests_slow
    val = gdal.GetConfigOption('GDAL_RUN_SLOW_TESTS', None)
    if val != 'yes' and val != 'YES':

        if count_skipped_tests_slow == 0:
            print('As GDAL_RUN_SLOW_TESTS environment variable is not defined, some "slow" tests will be skipped')
        count_skipped_tests_slow = count_skipped_tests_slow + 1

        return False
    return True

###############################################################################
# Return true if the platform support symlinks

def support_symlink():
    if sys.platform.startswith('linux'):
        return True
    if sys.platform.find('freebsd') != -1:
        return True
    if sys.platform == 'darwin':
        return True
    if sys.platform.find('sunos') != -1:
        return True
    return False

###############################################################################
# Return True if the test must be skipped

def skip_on_travis():
    val = gdal.GetConfigOption('TRAVIS', None)
    if val is not None:
        post_reason('Test skipped on Travis')
        return True
    return False


###############################################################################
# find_lib_linux()
# Parse /proc/self/maps to find an occurrence of libXXXXX.so.*

def find_lib_linux(libname):

    f = open('/proc/self/maps')
    lines = f.readlines()
    f.close()

    for line in lines:
        if line.rfind('/lib' + libname) == -1 or line.find('.so') == -1:
            continue

        i = line.find(' ')
        if i < 0:
            continue
        line = line[i+1:]
        i = line.find(' ')
        if i < 0:
            continue
        line = line[i+1:]
        i = line.find(' ')
        if i < 0:
            continue
        line = line[i+1:]
        i = line.find(' ')
        if i < 0:
            continue
        line = line[i+1:]
        i = line.find(' ')
        if i < 0:
            continue
        line = line[i+1:]

        soname = line.lstrip().rstrip('\n')
        if soname.rfind('/lib' + libname) == -1:
            continue

        return soname

    return None

###############################################################################
# find_lib_sunos()
# Parse output of pmap to find an occurrence of libXXX.so.*

def find_lib_sunos(libname):

    pid = os.getpid()
    (lines, err) = runexternal_out_and_err('pmap %d' % pid)
    
    for line in lines.split('\n'):
        if line.rfind('/lib' + libname) == -1 or line.find('.so') == -1:
            continue

        i = line.find('/')
        if i < 0:
            continue
        line = line[i:]

        soname = line.lstrip().rstrip('\n')
        if soname.rfind('/lib' + libname) == -1:
            continue

        return soname

    return None

###############################################################################
# find_lib_windows()
# use Module32First() / Module32Next() API on the current process

def find_lib_windows(libname):

    try:
        import ctypes
    except:
        return None

    kernel32 = ctypes.windll.kernel32

    MAX_MODULE_NAME32 = 255
    MAX_PATH = 260

    TH32CS_SNAPMODULE = 0x00000008

    class MODULEENTRY32(ctypes.Structure):
        _fields_ = [
            ("dwSize", ctypes.c_int),
            ("th32ModuleID", ctypes.c_int),
            ("th32ProcessID", ctypes.c_int),
            ("GlblcntUsage", ctypes.c_int),
            ("ProccntUsage", ctypes.c_int),
            ("modBaseAddr", ctypes.c_char_p),
            ("modBaseSize", ctypes.c_int),
            ("hModule", ctypes.c_void_p),
            ("szModule", ctypes.c_char * (MAX_MODULE_NAME32 + 1)),
            ("szExePath", ctypes.c_char * MAX_PATH)
        ]

    Module32First = kernel32.Module32First
    Module32First.argtypes = [ ctypes.c_void_p, ctypes.POINTER(MODULEENTRY32) ]
    Module32First.rettypes = ctypes.c_int

    Module32Next = kernel32.Module32Next
    Module32Next.argtypes = [ ctypes.c_void_p, ctypes.POINTER(MODULEENTRY32) ]
    Module32Next.rettypes = ctypes.c_int

    CreateToolhelp32Snapshot = kernel32.CreateToolhelp32Snapshot
    CreateToolhelp32Snapshot.argtypes = [ ctypes.c_int, ctypes.c_int ]
    CreateToolhelp32Snapshot.rettypes = ctypes.c_void_p

    CloseHandle = kernel32.CloseHandle
    CloseHandle.argtypes = [ ctypes.c_void_p ]
    CloseHandle.rettypes = ctypes.c_int

    GetLastError = kernel32.GetLastError
    GetLastError.argtypes = []
    GetLastError.rettypes = ctypes.c_int

    snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,0)
    if snapshot is None:
        return None

    soname = None

    i = 0
    while True:
        entry = MODULEENTRY32()
        entry.dwSize = ctypes.sizeof(MODULEENTRY32)
        pentry = ctypes.pointer(entry)
        if i == 0:
            ret = Module32First(snapshot, pentry)
        else:
            ret = Module32Next(snapshot, pentry)
        i = i + 1
        if ret == 0:
            break

        try:
            path = entry.szExePath.decode('latin1')
        except:
            continue

        i = path.rfind('\\' + libname)
        if i < 0:
            continue
        if path[i+1:].find('\\') >= 0:
            continue
        soname = path
        break

    CloseHandle(snapshot)

    return soname

###############################################################################
# find_lib()

def find_lib(mylib):
    if sys.platform.startswith('linux'):
        return find_lib_linux(mylib)
    elif sys.platform.startswith('sunos'):
        return find_lib_sunos(mylib)
    elif sys.platform.startswith('win32'):
        return find_lib_windows(mylib)
    else:
        # sorry mac users or other BSDs
        # should be doable, but not in a blindless way
        return None

###############################################################################
# get_opened_files()

def get_opened_files():
    if not sys.platform.startswith('linux'):
        return []
    fdpath = '/proc/%d/fd' % os.getpid()
    file_numbers = os.listdir(fdpath)
    filenames = []
    for fd in file_numbers:
        try:
            filename = os.readlink('%s/%s' % (fdpath, fd))
            if not filename.startswith('/dev/') and not filename.startswith('pipe:'):
                filenames.append(filename)
        except:
            pass
    return filenames

###############################################################################
# is_file_open()

def is_file_open(filename):
    for got_filename in get_opened_files():
        if got_filename.find(filename) >= 0:
            return True
    return False

###############################################################################
# error_handler()
# Allow use of "with" for an ErrorHandler that always pops at the scope close.
# Defaults to suppressing errors and warnings.

@contextlib.contextmanager
def error_handler(error_name = 'CPLQuietErrorHandler'):
  handler = gdal.PushErrorHandler(error_name)
  try:
    yield handler
  finally:
    gdal.PopErrorHandler()

###############################################################################
run_func = gdaltestaux.run_func
urlescape = gdaltestaux.urlescape
gdalurlopen = gdaltestaux.gdalurlopen
spawn_async = gdaltestaux.spawn_async
wait_process = gdaltestaux.wait_process
runexternal = gdaltestaux.runexternal
read_in_thread = gdaltestaux.read_in_thread
runexternal_out_and_err = gdaltestaux.runexternal_out_and_err
