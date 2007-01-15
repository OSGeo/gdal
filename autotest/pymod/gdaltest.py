###############################################################################
# $Id: gdaltest.py,v 1.31 2007/01/02 15:20:06 fwarmerdam Exp $
# 
# Project:  GDAL/OGR Test Suite
# Purpose:  Python Library supporting GDAL/OGR Test Suite
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
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
# 
# $Log: gdaltest.py,v $
# Revision 1.31  2007/01/02 15:20:06  fwarmerdam
# improve error report
#
# Revision 1.30  2006/06/23 02:22:36  fwarmerdam
# testCreateCopy() now skips band checksum tests if the band
# number is zero.
#
# Revision 1.29  2006/06/17 18:23:56  fwarmerdam
# improved cleanup
#
# Revision 1.28  2006/06/17 18:00:37  fwarmerdam
# Report blowups as a 'fail' too.
#
# Revision 1.27  2006/05/30 03:42:51  fwarmerdam
# added filename_absolute argument
#
# Revision 1.26  2006/03/31 05:18:44  fwarmerdam
# Added some options to testCreate and testCreateCopy
#
# Revision 1.25  2006/03/21 03:16:17  fwarmerdam
# return failed count
#
# Revision 1.24  2005/10/13 01:38:07  fwarmerdam
# added vsimem testing for createCopy
#
# Revision 1.23  2005/07/20 01:53:47  fwarmerdam
# Don't strip off 4 char test name prefix if it is not "test".
#
# Revision 1.22  2005/06/28 22:27:08  fwarmerdam
# Added clean_tmp().
#
# Revision 1.21  2005/05/22 20:51:47  fwarmerdam
# test projections with non-meter units
#
# Revision 1.20  2005/03/21 16:19:57  fwarmerdam
# Do epsilon testing of geotransforms.
#
# Revision 1.19  2005/03/08 14:44:36  fwarmerdam
# Added user_srs_to_wkt().
#
# Revision 1.18  2005/02/15 01:53:04  fwarmerdam
# various updates to createCopy test
#
# Revision 1.17  2005/02/08 18:03:19  fwarmerdam
# added check_gt and check_srs support in testCreateCopy
#
# Revision 1.16  2004/07/09 18:38:30  warmerda
# Added more diagnostics when projection doesn't work.
#
# Revision 1.15  2004/02/04 21:12:09  warmerda
# make sure '.' is in path
#
# Revision 1.14  2003/07/18 04:51:45  warmerda
# added approx_equal
#
# Revision 1.13  2003/04/22 17:59:38  warmerda
# supress spurious copycreate error messages about projections
#
# Revision 1.12  2003/04/02 10:42:04  dron
# Use osr.SpatialReference().IsSame() instead of string comparing in testSetProjection().
#
# Revision 1.11  2003/03/31 12:57:39  dron
# testSetNoDataValue() added.
#
# Revision 1.10  2003/03/28 09:13:15  dron
# New tests: testSetGeoTransform, testSetProjection, testSetMetadata,
# testSetNoDataValue.
#
# Revision 1.9  2003/03/27 15:52:09  dron
# Error handling improved.
#
# Revision 1.8  2003/03/26 20:00:28  dron
# Removed run_tests2() functioan and HashTest class.
#
# Revision 1.7  2003/03/25 17:28:58  dron
# Implemented GDALTest class.
#
# Revision 1.6  2003/03/25 11:13:48  dron
# Added run_tests2() function to run tests with list of parameters.
#
# Revision 1.5  2003/03/17 19:49:26  warmerda
# improved exception handling
#
# Revision 1.4  2003/03/17 05:43:11  warmerda
# added run_all function
#
# Revision 1.3  2003/03/13 16:33:14  warmerda
# dont reraise exceptions by default
#
# Revision 1.2  2003/03/13 16:23:49  warmerda
# added hash test class
#
# Revision 1.1  2003/03/05 05:04:44  warmerda
# New
#
#

import sys
import os
import gdal, osr

cur_name = 'default'

success_counter = 0
failure_counter = 0
blow_counter = 0
skip_counter = 0

reason = None

def setup_run( name ):
    global success_counter, failure_counter, blow_counter, skip_counter
    global cur_name
    
    cur_name = name

def run_tests( test_list ):
    global success_counter, failure_counter, blow_counter, skip_counter
    global cur_name, reason
    
    for test_item in test_list:
        if test_item is None:
            continue

        try:
            (func, name) = test_item
            if func.__name__[:4] == 'test':
                sys.stdout.write( '  TEST: ' + func.__name__[4:] + ': ' + name + ' ... ' )
            else:
                sys.stdout.write( '  TEST: ' + func.__name__ + ': ' + name + ' ... ' )
	except:
            func = test_item
            name = func.__name__
	    sys.stdout.write( '  TEST: ' + name + ' ... ' )

        sys.stdout.flush()
            
        reason = None
        try:
            result = func()
            print result
        except:
            result = 'fail (blowup)'
            print result
            
            import traceback
            traceback.print_exc()


        if reason is not None:
            print '    ' + reason

        if result == 'success':
            success_counter = success_counter + 1
        elif result == 'fail':
            failure_counter = failure_counter + 1
        elif result == 'skip':
            skip_counter = skip_counter + 1
        else:
            blow_counter = blow_counter + 1

def post_reason( msg ):
    global reason

    reason = msg

def summarize():
    global success_counter, failure_counter, blow_counter, skip_counter
    global cur_name
    
    print
    print 'Test Script: %s' % cur_name
    print 'Succeeded: %d' % success_counter
    print 'Failed:    %d (%d blew exceptions)' \
          % (failure_counter+blow_counter, blow_counter)
    print 'Skipped:   %d' % skip_counter
    print

    return failure_counter + blow_counter

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

    def testOpen(self, check_prj = None, check_gt = None, gt_epsilon = None):
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

        if self.xsize == 0 and self.ysize == 0:
            self.xsize = ds.RasterXSize
            self.ysize = ds.RasterYSize

        # Do we need to check the geotransform?
        if check_prj is not None:
            new_prj = ds.GetProjection()
            
            src_osr = osr.SpatialReference()
            src_osr.SetFromUserInput( check_prj )
            
            new_osr = osr.SpatialReference( wkt=new_prj )

            if not src_osr.IsSame(new_osr):
                print
                print 'old = ', src_osr.ExportToPrettyWkt()
                print 'new = ', new_osr.ExportToPrettyWkt()
                post_reason( 'Projections differ' )
                return 'fail'

        # Do we need to check the geotransform?
        if check_gt:
            # Default to 100th of pixel as our test value.
            if gt_epsilon is None:
                gt_epsilon = (abs(check_gt[1])+abs(check_gt[2])) / 100.0
                
            new_gt = ds.GetGeoTransform()
            for i in range(6):
                if abs(new_gt[i]-check_gt[i]) > gt_epsilon:
                    print
                    print 'old = ', check_gt
                    print 'new = ', new_gt
                    post_reason( 'Geotransform differs.' )
                    return 'fail'

        oBand = ds.GetRasterBand( self.band )
        chksum = oBand.Checksum( self.xoff, self.yoff, self.xsize, self.ysize )

        if chksum == self.chksum:
            return 'success'
        else:
            post_reason( 'Checksum for %s is %d, but expected %d.' \
                         % (self.filename, chksum, self.chksum) )
            return 'fail'
	

    def testCreateCopy(self, check_minmax = 1, check_gt = 0, check_srs = None,
                       vsimem = 0, new_filename = None ):
	if self.testDriver() == 'fail':
	    return 'skip'

	src_ds = gdal.Open( 'data/' + self.filename )
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
	new_ds = self.driver.CreateCopy( new_filename, src_ds,
	                                 options = self.options )
        gdal.PopErrorHandler()
	if new_ds is None:
	    post_reason( 'Failed to create test file using CreateCopy method.'\
                         + '\n' + gdal.GetLastErrorMsg() )
	    return 'fail'

        if self.band > 0:
            bnd = new_ds.GetRasterBand(self.band)
            if bnd.Checksum() != self.chksum:
                post_reason( 
                    'Did not get expected checksum on still-open file.\n' \
                    '    Got %d instead of %d.' % (bnd.Checksum(),self.chksum))
                return 'fail'
            got_minmax = bnd.ComputeRasterMinMax()
            if got_minmax != minmax and check_minmax:
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
            if bnd.Checksum() != self.chksum:
                post_reason( 'Did not get expected checksum on reopened file.' )
                return 'fail'
            got_minmax = bnd.ComputeRasterMinMax()
            if got_minmax != minmax and check_minmax:
                post_reason( \
                'Did not get expected min/max values on reopened file.\n' \
                '    Got %g,%g instead of %g,%g.' \
                % ( got_minmax[0], got_minmax[1], minmax[0], minmax[1] ) )
                return 'fail'

        # Do we need to check the geotransform?
        if check_gt:
            eps = 0.00000001
            new_gt = new_ds.GetGeoTransform()
            if abs(new_gt[0] - src_gt[0]) > eps \
               or abs(new_gt[1] - src_gt[1]) > eps \
               or abs(new_gt[2] - src_gt[2]) > eps \
               or abs(new_gt[3] - src_gt[3]) > eps \
               or abs(new_gt[4] - src_gt[4]) > eps \
               or abs(new_gt[5] - src_gt[5]) > eps:
                print
                print 'old = ', src_gt
                print 'new = ', new_gt
                post_reason( 'Geotransform differs.' )
                return 'fail'

        # Do we need to check the geotransform?
        if check_srs is not None:
            new_prj = new_ds.GetProjection()
            
            src_osr = osr.SpatialReference( wkt=src_prj )
            new_osr = osr.SpatialReference( wkt=new_prj )

            if not src_osr.IsSame(new_osr):
                print
                print 'old = ', src_osr.ExportToPrettyWkt()
                print 'new = ', new_osr.ExportToPrettyWkt()
                post_reason( 'Projections differ' )
                return 'fail'

	bnd = None
	new_ds = None
	src_ds = None

	self.driver.Delete( new_filename )

	return 'success'

    def testCreate(self, new_filename = None, out_bands = 3):
	if self.testDriver() == 'fail':
	    return 'skip'

	src_ds = gdal.Open( 'data/' + self.filename )
	xsize = src_ds.RasterXSize
	ysize = src_ds.RasterYSize
	src_img = src_ds.GetRasterBand(self.band).ReadRaster(0,0,xsize,ysize)
	minmax = src_ds.GetRasterBand(self.band).ComputeRasterMinMax()

        if new_filename is None:
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
            if new_ds.GetRasterBand(band).Checksum() != self.chksum:
                post_reason('Did not get expected checksum on still-open file.')
                return 'fail'
        
            if new_ds.GetRasterBand(band).ComputeRasterMinMax() != minmax:
                post_reason( 'Did not get expected min/max values on still-open file.' )
                return 'fail'
	
	new_ds = None

	new_ds = gdal.Open( new_filename )
	if new_ds is None:
	    post_reason( 'Failed to open dataset: ' + new_filename )
	    return 'fail'

        for band in range(1,out_bands+1):
            if new_ds.GetRasterBand(band).Checksum() != self.chksum:
                post_reason( 'Did not get expected checksum on reopened file.' )
                return 'fail'
            
            if new_ds.GetRasterBand(band).ComputeRasterMinMax() != minmax:
                post_reason( 'Did not get expected min/max values on reopened file.' )
                return 'fail'
	
	new_ds = None
	
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

	if gt != new_ds.GetGeoTransform():
	    post_reason( 'Did not get expected geotransform.' )
	    return 'fail'

	new_ds = None
	
	self.driver.Delete( new_filename )

	return 'success'

    def testSetProjection(self, prj = None ):
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

	new_osr = osr.SpatialReference()
	new_osr.ImportFromWkt(new_ds.GetProjection())
	if not new_osr.IsSame(src_osr):
	    post_reason( 'Did not get expected projection reference.' )
            print 'Got: '
            print new_osr.ExportToPrettyWkt()
            print 'Expected:'
            print src_osr.ExportToPrettyWkt()
	    return 'fail'

	new_ds = None
	
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
	if md_dict['TEST_KEY'] != 'TestValue':
	    post_reason( 'Did not get expected metadata item.' )
	    return 'fail'

	new_ds = None
	
	self.driver.Delete( new_filename )

	return 'success'

    def testSetNoDataValue(self):
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

	new_ds = gdal.Open( new_filename )
	if new_ds is None:
	    post_reason( 'Failed to open dataset: ' + new_filename )
	    return 'fail'

	if nodata != new_ds.GetRasterBand(1).GetNoDataValue():
	    post_reason( 'Did not get expected NoData value.' )
	    return 'fail'

	new_ds = None
	
	self.driver.Delete( new_filename )

	return 'success'

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
	
	self.driver.Delete( new_filename )

	return 'success'


###############################################################################

def run_all( dirlist, option_list ):

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
                
                exec "import " + module
                try:
                    exec "test_list = " + module + ".gdaltest_list"

                    print 'Running tests from %s/%s' % (dir_name,file)
                    run_tests( test_list )
                except:
                    pass
                
                os.chdir( wd )

            except:
                os.chdir( wd )
                print '... failed to load %s ... skipping.' % file

                import traceback
                traceback.print_exc()


        # We only add the tool directory to the python path long enough
        # to load the tool files.
        sys.path = old_path

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
