#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test some functions of HFA driver.  Most testing in ../gcore/hfa_*
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008-2011, Even Rouault <even dot rouault at mines-paris dot org>
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
import array

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Verify we can read the special histogram metadata from a provided image.

def hfa_histread():

    ds = gdal.Open('../gcore/data/utmsmall.img')
    md = ds.GetRasterBand(1).GetMetadata()
    ds = None

    if md['STATISTICS_MINIMUM'] != '8':
        gdaltest.post_reason( 'STATISTICS_MINIMUM is wrong.' )
        return 'fail'
    
    if md['STATISTICS_MEDIAN'] != '148':
        gdaltest.post_reason( 'STATISTICS_MEDIAN is wrong.' )
        return 'fail'

    if md['STATISTICS_HISTOMAX'] != '255':
        gdaltest.post_reason( 'STATISTICS_HISTOMAX is wrong.' )
        return 'fail'

    if md['STATISTICS_HISTOBINVALUES'] != '0|0|0|0|0|0|0|0|8|0|0|0|0|0|0|0|23|0|0|0|0|0|0|0|0|29|0|0|0|0|0|0|0|46|0|0|0|0|0|0|0|69|0|0|0|0|0|0|0|99|0|0|0|0|0|0|0|0|120|0|0|0|0|0|0|0|178|0|0|0|0|0|0|0|193|0|0|0|0|0|0|0|212|0|0|0|0|0|0|0|281|0|0|0|0|0|0|0|0|365|0|0|0|0|0|0|0|460|0|0|0|0|0|0|0|533|0|0|0|0|0|0|0|544|0|0|0|0|0|0|0|0|626|0|0|0|0|0|0|0|653|0|0|0|0|0|0|0|673|0|0|0|0|0|0|0|629|0|0|0|0|0|0|0|0|586|0|0|0|0|0|0|0|541|0|0|0|0|0|0|0|435|0|0|0|0|0|0|0|348|0|0|0|0|0|0|0|341|0|0|0|0|0|0|0|0|284|0|0|0|0|0|0|0|225|0|0|0|0|0|0|0|237|0|0|0|0|0|0|0|172|0|0|0|0|0|0|0|0|159|0|0|0|0|0|0|0|105|0|0|0|0|0|0|0|824|':
        gdaltest.post_reason( 'STATISTICS_HISTOBINVALUES is wrong.' )
        return 'fail'

    if md['STATISTICS_SKIPFACTORX'] != '1':
        gdaltest.post_reason( 'STATISTICS_SKIPFACTORX is wrong.' )
        return 'fail'

    if md['STATISTICS_SKIPFACTORY'] != '1':
        gdaltest.post_reason( 'STATISTICS_SKIPFACTORY is wrong.' )
        return 'fail'

    if md['STATISTICS_EXCLUDEDVALUES'] != '0':
        gdaltest.post_reason( 'STATISTICS_EXCLUDEDVALUE is wrong.' )
        return 'fail'


    return 'success'
    
###############################################################################
# Verify that if we copy this test image to a new Imagine file the histogram
# info is preserved.

def hfa_histwrite():

    drv = gdal.GetDriverByName('HFA')
    ds_src = gdal.Open('../gcore/data/utmsmall.img')
    out_ds = drv.CreateCopy( 'tmp/work.img', ds_src )
    del out_ds
    ds_src = None
    
    # Remove .aux.xml file as histogram can be written in it
    tmpAuxXml = 'tmp/work.img.aux.xml'
    if os.path.exists(tmpAuxXml): os.remove(tmpAuxXml)

    ds = gdal.Open('tmp/work.img')
    md = ds.GetRasterBand(1).GetMetadata()
    ds = None
    
    drv.Delete( 'tmp/work.img' )

    if md['STATISTICS_MINIMUM'] != '8':
        gdaltest.post_reason( 'STATISTICS_MINIMUM is wrong.' )
        return 'fail'
    
    if md['STATISTICS_MEDIAN'] != '148':
        gdaltest.post_reason( 'STATISTICS_MEDIAN is wrong.' )
        return 'fail'

    if md['STATISTICS_HISTOMAX'] != '255':
        gdaltest.post_reason( 'STATISTICS_HISTOMAX is wrong.' )
        return 'fail'


    if md['STATISTICS_HISTOBINVALUES'] != '0|0|0|0|0|0|0|0|8|0|0|0|0|0|0|0|23|0|0|0|0|0|0|0|0|29|0|0|0|0|0|0|0|46|0|0|0|0|0|0|0|69|0|0|0|0|0|0|0|99|0|0|0|0|0|0|0|0|120|0|0|0|0|0|0|0|178|0|0|0|0|0|0|0|193|0|0|0|0|0|0|0|212|0|0|0|0|0|0|0|281|0|0|0|0|0|0|0|0|365|0|0|0|0|0|0|0|460|0|0|0|0|0|0|0|533|0|0|0|0|0|0|0|544|0|0|0|0|0|0|0|0|626|0|0|0|0|0|0|0|653|0|0|0|0|0|0|0|673|0|0|0|0|0|0|0|629|0|0|0|0|0|0|0|0|586|0|0|0|0|0|0|0|541|0|0|0|0|0|0|0|435|0|0|0|0|0|0|0|348|0|0|0|0|0|0|0|341|0|0|0|0|0|0|0|0|284|0|0|0|0|0|0|0|225|0|0|0|0|0|0|0|237|0|0|0|0|0|0|0|172|0|0|0|0|0|0|0|0|159|0|0|0|0|0|0|0|105|0|0|0|0|0|0|0|824|':
        gdaltest.post_reason( 'STATISTICS_HISTOBINVALUES is wrong.' )
        return 'fail'

    return 'success'
    
###############################################################################
# Verify that if we copy this test image to a new Imagine file and then re-write the
# histogram information, the new histogram can then be read back in.
def hfa_histrewrite():

    drv = gdal.GetDriverByName('HFA')
    ds_src = gdal.Open('../gcore/data/utmsmall.img')
    out_ds = drv.CreateCopy( 'tmp/work.img', ds_src )
    del out_ds
    ds_src = None
    
    # Remove .aux.xml file as histogram can be written in it
    tmpAuxXml = 'tmp/work.img.aux.xml'
    if os.path.exists(tmpAuxXml):
        os.remove(tmpAuxXml)
    
    # A new histogram which is different to what is in the file. It won't match the data,
    # but we are just testing the re-writing of the histogram, so we don't mind. 
    newHist = '8|23|29|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|46|0|0|0|0|0|0|0|69|0|0|0|0|0|0|0|99|0|0|0|0|0|0|0|0|120|0|0|0|0|0|0|0|178|0|0|0|0|0|0|0|193|0|0|0|0|0|0|0|212|0|0|0|0|0|0|0|281|0|0|0|0|0|0|0|0|365|0|0|0|0|0|0|0|460|0|0|0|0|0|0|0|533|0|0|0|0|0|0|0|544|0|0|0|0|0|0|0|0|626|0|0|0|0|0|0|0|653|0|0|0|0|0|0|0|673|0|0|0|0|0|0|0|629|0|0|0|0|0|0|0|0|586|0|0|0|0|0|0|0|541|0|0|0|0|0|0|0|435|0|0|0|0|0|0|0|348|0|0|0|0|0|0|0|341|0|0|0|0|0|0|0|0|284|0|0|0|0|0|0|0|225|0|0|0|0|0|0|0|237|0|0|0|0|0|0|0|172|0|0|0|0|0|0|0|0|159|0|0|0|0|0|0|0|105|0|0|0|0|0|0|0|824|'

    ds = gdal.Open('tmp/work.img', gdal.GA_Update)
    band = ds.GetRasterBand(1)
    band.SetMetadataItem('STATISTICS_HISTOBINVALUES', newHist)
    ds = None
    
    if os.path.exists(tmpAuxXml):
        os.remove(tmpAuxXml)
    
    ds = gdal.Open('tmp/work.img')
    histStr = ds.GetRasterBand(1).GetMetadataItem('STATISTICS_HISTOBINVALUES')
    ds = None
    
    drv.Delete( 'tmp/work.img' )

    if histStr != newHist:
        gdaltest.post_reason( 'Rewritten STATISTICS_HISTOBINVALUES is wrong.' )
        return 'fail'

    return 'success'
    
###############################################################################
# Verify we can read metadata of int.img.

def hfa_int_stats_1():

    ds = gdal.Open('data/int.img')
    md = ds.GetRasterBand(1).GetMetadata()
    ds = None

    if md['STATISTICS_MINIMUM'] != '40918':
        gdaltest.post_reason( 'STATISTICS_MINIMUM is wrong.' )
        return 'fail'

    if md['STATISTICS_MAXIMUM'] != '41134':
        gdaltest.post_reason( 'STATISTICS_MAXIMUM is wrong.' )
        return 'fail'

    if md['STATISTICS_MEDIAN'] != '41017':
        gdaltest.post_reason( 'STATISTICS_MEDIAN is wrong.' )
        return 'fail'

    if md['STATISTICS_MODE'] != '41013':
        gdaltest.post_reason( 'STATISTICS_MODE is wrong.' )
        return 'fail'

    if md['STATISTICS_HISTOMIN'] != '40918':
        gdaltest.post_reason( 'STATISTICS_HISTOMIN is wrong.' )
        return 'fail'

    if md['STATISTICS_HISTOMAX'] != '41134':
        gdaltest.post_reason( 'STATISTICS_HISTOMAX is wrong.' )
        return 'fail'

    if md['LAYER_TYPE'] != 'athematic':
        gdaltest.post_reason( 'LAYER_TYPE is wrong.' )
        return 'fail'

    return 'success'

###############################################################################
# Verify we can read band statistics of int.img.

def hfa_int_stats_2():

    ds = gdal.Open('data/int.img')
    stats = ds.GetRasterBand(1).GetStatistics(False, True)
    ds = None

    tolerance = 0.0001

    if abs(stats[0] - 40918.0) > tolerance:
        gdaltest.post_reason( 'Minimum value is wrong.' )
        return 'fail'

    if abs(stats[1] - 41134.0) > tolerance:
        gdaltest.post_reason( 'Maximum value is wrong.' )
        return 'fail'

    if abs(stats[2] - 41019.784218148) > tolerance:
        gdaltest.post_reason( 'Mean value is wrong.' )
        return 'fail'

    if abs(stats[3] - 44.637237445468) > tolerance:
        gdaltest.post_reason( 'StdDev value is wrong.' )
        return 'fail'

    return 'success'

###############################################################################
# Verify we can read metadata of float.img.

def hfa_float_stats_1():

    ds = gdal.Open('data/float.img')
    md = ds.GetRasterBand(1).GetMetadata()
    ds = None

    tolerance = 0.0001

    min = float(md['STATISTICS_MINIMUM'])
    if abs(min - 40.91858291626) > tolerance:
        gdaltest.post_reason( 'STATISTICS_MINIMUM is wrong.' )
        return 'fail'

    max = float(md['STATISTICS_MAXIMUM'])
    if abs(max - 41.134323120117) > tolerance:
        gdaltest.post_reason( 'STATISTICS_MAXIMUM is wrong.' )
        return 'fail'

    median = float(md['STATISTICS_MEDIAN'])
    if abs(median - 41.017182931304) > tolerance:
        gdaltest.post_reason( 'STATISTICS_MEDIAN is wrong.' )
        return 'fail'

    mod = float(md['STATISTICS_MODE'])
    if abs(mod - 41.0104410499) > tolerance:
        gdaltest.post_reason( 'STATISTICS_MODE is wrong.' )
        return 'fail'

    histMin = float(md['STATISTICS_HISTOMIN'])
    if abs(histMin - 40.91858291626) > tolerance:
        gdaltest.post_reason( 'STATISTICS_HISTOMIN is wrong.' )
        return 'fail'

    histMax = float(md['STATISTICS_HISTOMAX'])
    if abs(histMax - 41.134323120117) > tolerance:
        gdaltest.post_reason( 'STATISTICS_HISTOMAX is wrong.' )
        return 'fail'

    if md['LAYER_TYPE'] != 'athematic':
        gdaltest.post_reason( 'LAYER_TYPE is wrong.' )
        return 'fail'

    return 'success'

###############################################################################
# Verify we can read band statistics of float.img.

def hfa_float_stats_2():

    ds = gdal.Open('data/float.img')
    stats = ds.GetRasterBand(1).GetStatistics(False, True)
    ds = None
    
    tolerance = 0.0001

    if abs(stats[0] - 40.91858291626) > tolerance:
        gdaltest.post_reason( 'Minimum value is wrong.' )
        return 'fail'

    if abs(stats[1] - 41.134323120117) > tolerance:
        gdaltest.post_reason( 'Maximum value is wrong.' )
        return 'fail'

    if abs(stats[2] - 41.020284249223) > tolerance:
        gdaltest.post_reason( 'Mean value is wrong.' )
        return 'fail'

    if abs(stats[3] - 0.044636441749041) > tolerance:
        gdaltest.post_reason( 'StdDev value is wrong.' )
        return 'fail'

    return 'success'

###############################################################################
# Verify we can read image data.

def hfa_int_read():

    ds = gdal.Open('data/int.img')
    band = ds.GetRasterBand(1)
    cs = band.Checksum()
    band.ReadRaster(100, 100, 1, 1)
    ds = None

    if cs != 6691:
        gdaltest.post_reason( 'Checksum value is wrong.' )
        return 'fail'

    return 'success'

###############################################################################
# Verify we can read image data.

def hfa_float_read():

    ds = gdal.Open('data/float.img')
    band = ds.GetRasterBand(1)
    cs = band.Checksum()
    data = band.ReadRaster(100, 100, 1, 1)
    ds = None

    if cs != 23529:
        gdaltest.post_reason( 'Checksum value is wrong.' )
        return 'fail'

    # Read raw data into tuple of float numbers
    import struct
    value = struct.unpack('f' * 1, data)[0]

    if abs(value - 41.021659851074219) > 0.0001:
        gdaltest.post_reason( 'Pixel value is wrong.' )
        return 'fail'

    return 'success'
 
###############################################################################
# verify we can read PE_STRING coordinate system.

def hfa_pe_read():

    ds = gdal.Open('data/87test.img')
    wkt = ds.GetProjectionRef()
    expected = 'PROJCS["World_Cube",GEOGCS["GCS_WGS_1984",DATUM["WGS_1984",SPHEROID["WGS_84",6378137.0,298.257223563]],PRIMEM["Greenwich",0.0],UNIT["Degree",0.0174532925199433]],PROJECTION["Cube"],PARAMETER["False_Easting",0.0],PARAMETER["False_Northing",0.0],PARAMETER["Central_Meridian",0.0],PARAMETER["Option",1.0],UNIT["Meter",1.0]]'

    if wkt != expected:
        print(wkt)
        gdaltest.post_reason( 'failed to read pe string as expected.' )
        return 'fail'

    return 'success'
 
###############################################################################
# Verify we can write PE_STRING nodes.

def hfa_pe_write():

    drv = gdal.GetDriverByName('HFA')
    ds_src = gdal.Open('data/87test.img')
    out_ds = drv.CreateCopy( 'tmp/87test.img', ds_src )
    del out_ds
    ds_src = None

    expected = 'PROJCS["World_Cube",GEOGCS["GCS_WGS_1984",DATUM["WGS_1984",SPHEROID["WGS_84",6378137.0,298.257223563]],PRIMEM["Greenwich",0.0],UNIT["Degree",0.017453292519943295]],PROJECTION["Cube"],PARAMETER["False_Easting",0.0],PARAMETER["False_Northing",0.0],PARAMETER["Central_Meridian",0.0],PARAMETER["Option",1.0],UNIT["Meter",1.0]]'
    
    ds = gdal.Open('tmp/87test.img')
    wkt = ds.GetProjectionRef()

    if wkt != expected:
        print('')
        print(expected)
        print(wkt)
        gdaltest.post_reason( 'failed to write pe string as expected.' )
        return 'fail'

    ds = None
    drv.Delete( 'tmp/87test.img' )
    return 'success'
 
###############################################################################
# Verify we can write and read large metadata items.

def hfa_metadata_1():

    drv = gdal.GetDriverByName('HFA')
    ds = drv.Create( 'tmp/md_1.img', 100, 150, 1, gdal.GDT_Byte )
    
    md_val = '0123456789' * 60
    md = { 'test' : md_val }
    ds.GetRasterBand(1).SetMetadata( md )
    ds = None

    ds = gdal.Open( 'tmp/md_1.img' )
    md = ds.GetRasterBand(1).GetMetadata()
    if md['test'] != md_val:
        print(md['test'])
        print(md_val)
        gdaltest.post_reason( 'got wrong metadata back' )
        return 'fail'
    ds = None
    
    return 'success'
 
###############################################################################
# Verify that writing metadata multiple times does not result in duplicate
# nodes.

def hfa_metadata_2():

    ds = gdal.Open( 'tmp/md_1.img', gdal.GA_Update )
    md = ds.GetRasterBand(1).GetMetadata()
    md['test'] = '0123456789'
    md['xxx'] = '123'
    ds.GetRasterBand(1).SetMetadata( md )
    ds = None

    ds = gdal.Open( 'tmp/md_1.img' )
    md = ds.GetRasterBand(1).GetMetadata()
    if 'xxx' not in md:
        gdaltest.post_reason('metadata rewrite seems not to have worked')
        return 'fail'

    if md['xxx'] != '123' or md['test'] != '0123456789':
        print(md)
        gdaltest.post_reason('got wrong metadata back')
        return 'fail'

    ds = None
    gdal.GetDriverByName('HFA').Delete( 'tmp/md_1.img' )
    
    return 'success'
 
###############################################################################
# Verify we can grow the RRD list in cases where this requires
# moving the HFAEntry to the end of the file.  (bug #1109)

def hfa_grow_rrdlist():

    import shutil

    shutil.copyfile('data/bug_1109.img' , 'tmp/bug_1109.img')
    #os.system("copy data\\bug_1109.img tmp")

    # Add two overview levels.
    ds = gdal.Open('tmp/bug_1109.img',gdal.GA_Update)
    result = ds.BuildOverviews( overviewlist = [4,8] )
    ds = None

    if result != 0:
        gdaltest.post_reason( 'BuildOverviews failed.' )
        return 'fail'

    # Verify overviews are now findable.
    ds = gdal.Open( 'tmp/bug_1109.img' )
    if ds.GetRasterBand(1).GetOverviewCount() != 3:
        gdaltest.post_reason( 'Overview count wrong.' )
        print(ds.GetRasterBand(1).GetOverviewCount())
        return 'fail'

    ds = None
    gdal.GetDriverByName('HFA').Delete( 'tmp/bug_1109.img' )
    
    return 'success'
 
###############################################################################
# Make sure an old .ige file is deleted when creating a new dataset. (#1784)

def hfa_clean_ige():

    # Create an imagine file, forcing creation of an .ige file.

    drv = gdal.GetDriverByName('HFA')
    src_ds = gdal.Open('data/byte.tif')

    out_ds = drv.CreateCopy( 'tmp/igetest.img', src_ds,
                    options = [ 'USE_SPILL=YES' ] )
    out_ds = None

    try:
        open( 'tmp/igetest.ige' )
    except:
        gdaltest.post_reason( 'ige file not created with USE_SPILL=YES' )
        return 'fail'

    # confirm ige shows up in file list.
    ds = gdal.Open('tmp/igetest.img')
    filelist = ds.GetFileList()
    ds = None

    found = 0
    for item in filelist:
        if item[-11:] == 'igetest.ige':
            found = 1
            
    if not found:
        print(filelist)
        gdaltest.post_reason( 'no igetest.ige in file list!' )
        return 'fail'

    # Create a file without a spill file, and verify old ige cleaned up.
    
    out_ds = drv.CreateCopy( 'tmp/igetest.img', src_ds )
    del out_ds

    try:
        open( 'tmp/igetest.ige' )
        gdaltest.post_reason( 'ige file not cleaned up properly.' )
        return 'fail'
    except:
        pass

    drv.Delete( 'tmp/igetest.img' )

    return 'success'
 
###############################################################################
# Verify that we can read this corrupt .aux file without hanging (#1907)

def hfa_corrupt_aux():

    # NOTE: we depend on being able to open .aux files as a weak sort of
    # dataset.

    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    ds = gdal.Open( 'data/F0116231.aux' )
    gdal.PopErrorHandler()

    if ds.RasterXSize != 1104:
        gdaltest.post_reason( 'did not get expected dataset characteristics' )
        return 'fail'
    
    if gdal.GetLastErrorType() != 2 \
       or gdal.GetLastErrorMsg().find('Corrupt (looping)') == -1:
        gdaltest.post_reason( 'Did not get expected warning.' )
        return 'fail'

    ds = None

    return 'success'
 
###############################################################################
# support MapInformation for units (#1967)

def hfa_mapinformation_units():

    # NOTE: we depend on being able to open .aux files as a weak sort of
    # dataset.

    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    ds = gdal.Open( 'data/fg118-91.aux' )
    gdal.PopErrorHandler()

    wkt = ds.GetProjectionRef()
    expected_wkt = """PROJCS["NAD_1983_StatePlane_Virginia_North_FIPS_4501_Feet",GEOGCS["GCS_North_American_1983",DATUM["North_American_Datum_1983",SPHEROID["GRS_1980",6378137,298.257222101]],PRIMEM["Greenwich",0],UNIT["Degree",0.0174532925199432955],AUTHORITY["EPSG","4269"]],PROJECTION["Lambert_Conformal_Conic_2SP"],PARAMETER["False_Easting",11482916.66666666],PARAMETER["False_Northing",6561666.666666666],PARAMETER["Central_Meridian",-78.5],PARAMETER["Standard_Parallel_1",38.03333333333333],PARAMETER["Standard_Parallel_2",39.2],PARAMETER["Latitude_Of_Origin",37.66666666666666],UNIT["Foot_US",0.304800609601219241]]"""

    if gdaltest.equal_srs_from_wkt( expected_wkt, wkt ):
        return 'success'
    else:
        return 'fail'
 
###############################################################################
# Write nodata value.

def hfa_nodata_write():

    drv = gdal.GetDriverByName( 'HFA' )
    ds = drv.Create( 'tmp/nodata.img', 7, 7, 1, gdal.GDT_Byte )

    p = [ 1, 2, 1, 4, 1, 2, 1 ]
    raw_data = array.array( 'h', p ).tostring()

    for line in range( 7 ):
        ds.WriteRaster( 0, line, 7, 1, raw_data,
                        buf_type = gdal.GDT_Int16 )

    b = ds.GetRasterBand(1)
    b.SetNoDataValue( 1 )

    ds = None

    return 'success'

###############################################################################
# Verify written nodata value.

def hfa_nodata_read():

    ds = gdal.Open( 'tmp/nodata.img' )
    b = ds.GetRasterBand(1)

    if b.GetNoDataValue() != 1:
        gdaltest.post_reason( 'failed to preserve nodata value' )
        return 'fail'

    stats = b.GetStatistics(False, True)

    tolerance = 0.0001

    if abs(stats[0] - 2) > tolerance:
        gdaltest.post_reason( 'Minimum value is wrong.' )
        return 'fail'

    if abs(stats[1] - 4) > tolerance:
        gdaltest.post_reason( 'Maximum value is wrong.' )
        return 'fail'

    if abs(stats[2] - 2.6666666666667) > tolerance:
        gdaltest.post_reason( 'Mean value is wrong.' )
        return 'fail'

    if abs(stats[3] - 0.94280904158206) > tolerance:
        gdaltest.post_reason( 'StdDev value is wrong.' )
        return 'fail'

    b = None
    ds = None
    
    gdal.GetDriverByName( 'HFA' ).Delete( 'tmp/nodata.img' )

    return 'success'

###############################################################################
# Verify we read simple affine geotransforms properly.

def hfa_rotated_read():

    ds = gdal.Open( 'data/fg118-91.aux' )

    check_gt = ( 11856857.07898215, 0.895867662235625, 0.02684252936279331,
                 7041861.472946444, 0.01962103617166367, -0.9007880319529181)

    gt_epsilon = (abs(check_gt[1])+abs(check_gt[2])) / 100.0
                
    new_gt = ds.GetGeoTransform()
    for i in range(6):
        if abs(new_gt[i]-check_gt[i]) > gt_epsilon:
            print('')
            print('old = ', check_gt)
            print('new = ', new_gt)
            gdaltest.post_reason( 'Geotransform differs.' )
            return 'fail'

    ds = None
    return 'success'

###############################################################################
# Verify we can write affine geotransforms.

def hfa_rotated_write():

    # make sure we aren't preserving info in .aux.xml file
    try:
        os.remove( 'tmp/rot.img.aux.xml' )
    except:
        pass
    
    drv = gdal.GetDriverByName('HFA')
    ds = drv.Create( 'tmp/rot.img', 100, 150, 1, gdal.GDT_Byte )
    
    check_gt = ( 11856857.07898215, 0.895867662235625, 0.02684252936279331,
                 7041861.472946444, 0.01962103617166367, -0.9007880319529181)

    expected_wkt = """PROJCS["NAD83 / Virginia North",
    GEOGCS["NAD83",
        DATUM["North_American_Datum_1983",
            SPHEROID["GRS 1980",6378137,298.257222101,
                AUTHORITY["EPSG","7019"]],
            AUTHORITY["EPSG","6269"]],
        PRIMEM["Greenwich",0,
            AUTHORITY["EPSG","8901"]],
        UNIT["degree",0.01745329251994328,
            AUTHORITY["EPSG","9122"]],
        AUTHORITY["EPSG","4269"]],
    PROJECTION["Lambert_Conformal_Conic_2SP"],
    PARAMETER["standard_parallel_1",39.2],
    PARAMETER["standard_parallel_2",38.03333333333333],
    PARAMETER["latitude_of_origin",37.66666666666666],
    PARAMETER["central_meridian",-78.5],
    PARAMETER["false_easting",11482916.66666667],
    PARAMETER["false_northing",6561666.666666667],
    UNIT["us_survey_feet",0.3048006096012192]]"""

    # For some reason we are now no longer able to preserve the authority
    # nodes and other info in the above, so we revert to the following.
    # (see #2755 for followup).

    expected_wkt = """PROJCS["NAD83_Virginia_North",GEOGCS["GCS_North_American_1983",DATUM["North_American_Datum_1983",SPHEROID["GRS_1980",6378137,298.257222101]],PRIMEM["Greenwich",0],UNIT["Degree",0.017453292519943295]],PROJECTION["Lambert_Conformal_Conic_2SP"],PARAMETER["standard_parallel_1",39.2],PARAMETER["standard_parallel_2",38.03333333333333],PARAMETER["latitude_of_origin",37.66666666666666],PARAMETER["central_meridian",-78.5],PARAMETER["false_easting",11482916.66666667],PARAMETER["false_northing",6561666.666666667],PARAMETER["scale_factor",1.0],UNIT["Foot_US",0.30480060960121924]]"""
    
    ds.SetGeoTransform( check_gt )
    ds.SetProjection( expected_wkt )

    ds = None
    
    ds = gdal.Open( 'tmp/rot.img' )
    gt_epsilon = (abs(check_gt[1])+abs(check_gt[2])) / 100.0
                
    new_gt = ds.GetGeoTransform()
    for i in range(6):
        if abs(new_gt[i]-check_gt[i]) > gt_epsilon:
            print('')
            print('old = ', check_gt)
            print('new = ', new_gt)
            gdaltest.post_reason( 'Geotransform differs.' )
            return 'fail'

    wkt = ds.GetProjection()
    if not gdaltest.equal_srs_from_wkt( expected_wkt, wkt ):
        return 'fail'
    
    ds = None

    gdal.GetDriverByName( 'HFA' ).Delete( 'tmp/rot.img' )

    return 'success'


###############################################################################
# Test creating an in memory copy.

def hfa_vsimem():

    tst = gdaltest.GDALTest( 'HFA', 'byte.tif', 1, 4672 )

    return tst.testCreateCopy( vsimem = 1 )

###############################################################################
# Test that PROJCS[] names are preserved as the mapinfo.proName in
# the .img file.  (#2422)

def hfa_proName():

    drv = gdal.GetDriverByName('HFA')
    src_ds = gdal.Open('data/stateplane.vrt')
    dst_ds = drv.CreateCopy( 'tmp/proname.img', src_ds )

    del dst_ds
    src_ds = None

    # Make sure we don't have interference from an .aux.xml
    try:
        os.remove('tmp/proname.img.aux.xml')
    except:
        pass

    ds = gdal.Open( 'tmp/proname.img' )

    srs = ds.GetProjectionRef()
    if srs[:55] != 'PROJCS["NAD_1983_StatePlane_Ohio_South_FIPS_3402_Feet",':
        gdaltest.post_reason( 'did not get expected PROJCS name.' )
        print(srs)
        result = 'fail'
    else:
        result = 'success'

    ds = None

    drv.Delete( 'tmp/proname.img' )

    return result


###############################################################################
# Read a compressed file where no block has been written (#2523)

def hfa_read_empty_compressed():

    drv = gdal.GetDriverByName('HFA')
    ds = drv.Create('tmp/emptycompressed.img', 64, 64, 1, options = [ 'COMPRESSED=YES' ] )
    ds = None

    ds = gdal.Open('tmp/emptycompressed.img')
    if ds.GetRasterBand(1).Checksum() != 0:
        result = 'fail'
    else:
        result = 'success'

    ds = None

    drv.Delete( 'tmp/emptycompressed.img' )

    return result

###############################################################################
# Verify "unique values" based color table (#2419)

def hfa_unique_values_color_table():

    ds = gdal.Open( 'data/i8u_c_i.img' )

    ct = ds.GetRasterBand(1).GetRasterColorTable()

    if ct.GetCount() != 256:
        print(ct.GetCount())
        gdaltest.post_reason( 'got wrong color count' )
        return 'fail'

    if ct.GetColorEntry(253) != (0,0,0,0) \
       or ct.GetColorEntry(254) != (255,255,170,255) \
       or ct.GetColorEntry(255) != (255,255,255,255):

        print(ct.GetColorEntry(253))
        print(ct.GetColorEntry(254))
        print(ct.GetColorEntry(255))
    
        gdaltest.post_reason( 'Got wrong colors' )
        return 'fail'

    ct = None
    ds = None

    return 'success'

###############################################################################
# Verify "unique values" based histogram.

def hfa_unique_values_hist():

    try:
        gdal.RasterAttributeTable()
    except:
        return 'skip'

    ds = gdal.Open( 'data/i8u_c_i.img' )

    md = ds.GetRasterBand(1).GetMetadata()

    expected = '12603|1|0|0|45|1|0|0|0|0|656|177|0|0|5026|1062|0|0|2|0|0|0|0|0|0|0|0|0|0|0|0|0|75|1|0|0|207|158|0|0|8|34|0|0|0|0|538|57|0|10|214|20|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|1|31|0|0|9|625|67|0|0|118|738|117|3004|1499|491|187|1272|513|1|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|16|3|0|0|283|123|5|1931|835|357|332|944|451|80|40|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|12|5|0|0|535|1029|118|0|33|246|342|0|0|10|8|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|169|439|0|0|6|990|329|0|0|120|295|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|164|42|0|0|570|966|0|0|18|152|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|45|106|0|0|16|16517|'
    if md['STATISTICS_HISTOBINVALUES'] != expected:
        print(md['STATISTICS_HISTOBINVALUES'])
        gdaltest.post_reason( 'Unexpected HISTOBINVALUES.' )
        return 'fail'

    if md['STATISTICS_HISTOMIN'] != '0' \
       or md['STATISTICS_HISTOMAX'] != '255':
        print(md)
        gdaltest.post_reason( "unexpected histomin/histomax value." )
        return 'fail'

    # lets also check the RAT to ensure it has the BinValues column added.

    rat = ds.GetRasterBand(1).GetDefaultRAT()

    if rat.GetColumnCount() != 6 \
       or rat.GetTypeOfCol(0) != gdal.GFT_Real \
       or rat.GetUsageOfCol(0) != gdal.GFU_MinMax:
        print(rat.GetColumnCount())
        print(rat.GetTypeOfCol(0))
        print(rat.GetUsageOfCol(0))
        gdaltest.post_reason( 'BinValues column wrong.')
        return 'fail'

    if rat.GetValueAsInt( 2, 0 ) != 4:
        print(rat.GetValueAsInt( 2, 0 ))
        gdaltest.post_reason( 'BinValues value wrong.' )
        return 'fail'

    rat = None
    
    ds = None

    return 'success'

###############################################################################
# Verify reading of 3rd order XFORM polynomials.

def hfa_xforms_3rd():

    ds = gdal.Open( 'data/42BW_420730_VT2.aux' )

    check_list = [
        ('XFORM_STEPS', 2),
        ('XFORM0_POLYCOEFMTX[3]', -0.151286406053458),
        ('XFORM0_POLYCOEFVECTOR[1]', 401692.559078924),
        ('XFORM1_ORDER', 3),
        ('XFORM1_FWD_POLYCOEFMTX[0]', -0.560405515080768),
        ('XFORM1_FWD_POLYCOEFMTX[17]', -1.01593898110617e-08),
        ('XFORM1_REV_POLYCOEFMTX[17]', 4.01319402177037e-09),
        ('XFORM1_REV_POLYCOEFVECTOR[0]', 2605.41812438735) ]

    xform_md = ds.GetMetadata( 'XFORMS' )

    for check_item in check_list:
        try:
            value = float(xform_md[check_item[0]])
        except:
            gdaltest.post_reason( 'metadata item %d missing' % check_item[0])
            return 'fail'

        if abs(value - check_item[1]) > abs(value/100000.0):
            gdaltest.post_reason( 'metadata item %s has wrong value: %.15g' % \
                                  (check_item[0], value) )
            return 'fail'

    # Check that the GCPs are as expected implying that the evaluation
    # function for XFORMs if working ok.
    
    gcps = ds.GetGCPs()

    if gcps[0].GCPPixel != 0.5 \
       or gcps[0].GCPLine != 0.5 \
       or abs(gcps[0].GCPX - 1667635.007) > 0.001 \
       or abs(gcps[0].GCPY - 2620003.171) > 0.001:
        print(gcps[0].GCPPixel, gcps[0].GCPLine, gcps[0].GCPX, gcps[0].GCPY)
        gdaltest.post_reason( 'GCP 0 value wrong.' )
        return 'fail'
    
    if abs(gcps[14].GCPPixel - 1769.7) > 0.1 \
       or abs(gcps[14].GCPLine  - 2124.9) > 0.1 \
       or abs(gcps[14].GCPX - 1665221.064) > 0.001 \
       or abs(gcps[14].GCPY - 2632414.379) > 0.001:
        print(gcps[14].GCPPixel, gcps[14].GCPLine, gcps[14].GCPX, gcps[14].GCPY)
        gdaltest.post_reason( 'GCP 14 value wrong.' )
        return 'fail'
    
    ds = None

    return 'success'

###############################################################################
# Verify that we can clear an existing color table

def hfa_delete_colortable():
    # copy a file to tmp dir to modify.
    open('tmp/i8u.img','wb').write(open('data/i8u_c_i.img', 'rb').read())

    # clear color table.
    ds = gdal.Open( 'tmp/i8u.img', gdal.GA_Update )

    try:
        ds.GetRasterBand(1).SetColorTable
    except:
        # OG python bindings don't have SetColorTable, and if we use
        # SetRasterColorTable, it doesn't work either as None isn't a valid
        # value for them
        ds = None
        gdal.GetDriverByName('HFA').Delete('tmp/i8u.img')
        return 'skip'

    ds.GetRasterBand(1).SetColorTable(None)
    ds = None

    # check color table gone.
    ds = gdal.Open( 'tmp/i8u.img' )
    if ds.GetRasterBand(1).GetColorTable() != None:
        gdaltest.post_reason( 'failed to remove color table' )
        return 'fail'

    ds = None

    gdal.GetDriverByName('HFA').Delete('tmp/i8u.img')

    return 'success'

###############################################################################
# Verify that we can clear an existing color table (#2842)

def hfa_delete_colortable2():

    # copy a file to tmp dir to modify.
    src_ds = gdal.Open('../gcore/data/8bit_pal.bmp')
    ds = gdal.GetDriverByName('HFA').CreateCopy('tmp/hfa_delete_colortable2.img', src_ds)
    src_ds = None
    ds = None

    # clear color table.
    ds = gdal.Open( 'tmp/hfa_delete_colortable2.img', gdal.GA_Update )

    try:
        ds.GetRasterBand(1).SetColorTable
    except:
        # OG python bindings don't have SetColorTable, and if we use
        # SetRasterColorTable, it doesn't work either as None isn't a valid
        # value for them
        ds = None
        gdal.GetDriverByName('HFA').Delete('tmp/hfa_delete_colortable2.img')
        return 'skip'

    ds.GetRasterBand(1).SetColorTable(None)
    ds = None

    # check color table gone.
    ds = gdal.Open( 'tmp/hfa_delete_colortable2.img' )
    if ds.GetRasterBand(1).GetColorTable() != None:
        gdaltest.post_reason( 'failed to remove color table' )
        return 'fail'

    ds = None

    gdal.GetDriverByName('HFA').Delete('tmp/hfa_delete_colortable2.img')

    return 'success'

###############################################################################
# Verify we can read the special histogram metadata from a provided image.

def hfa_excluded_values():

    ds = gdal.Open('data/dem10.img')
    md = ds.GetRasterBand(1).GetMetadata()
    ds = None

    if md['STATISTICS_EXCLUDEDVALUES'] != '0,8,9':
        gdaltest.post_reason( 'STATISTICS_EXCLUDEDVALUE is wrong.' )
        return 'fail'


    return 'success'
    
###############################################################################
# verify that we propogate nodata to overviews in .img/.rrd format.

def hfa_ov_nodata():

    drv = gdal.GetDriverByName( 'HFA' )
    src_ds = gdal.Open('data/nodata_int.asc')
    wrk_ds = drv.CreateCopy( '/vsimem/ov_nodata.img', src_ds )
    src_ds = None

    wrk_ds.BuildOverviews( overviewlist = [2] )
    wrk_ds = None

    wrk2_ds = gdal.Open( '/vsimem/ov_nodata.img' )
    ovb = wrk2_ds.GetRasterBand(1).GetOverview(0)

    if ovb.GetNoDataValue() != -99999:
        gdaltest.post_reason( 'nodata not propagated to .img overview.' )
        return 'fail'

    if ovb.GetMaskFlags() != gdal.GMF_NODATA:
        gdaltest.post_reason( 'mask flag not as expected.' )
        return 'fail'
    
    # Confirm that a .ovr file was *not* produced.
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    try:
        wrk3_ds = gdal.Open('/vsimem/ov_nodata.img.ovr')
    except:
        wrk3_ds = None
    gdal.PopErrorHandler()
    
    if wrk3_ds is not None:
        gdaltest.post_reason( 'this test result is invalid since .ovr file was created, why?' )
        return 'fail'

    wrk2_ds = None
    drv.Delete( '/vsimem/ov_nodata.img' )
    
    return 'success'
 
###############################################################################
# Confirm that we can read 8bit grayscale overviews for 1bit images.

def hfa_read_bit2grayscale():

    ds = gdal.Open( 'data/small1bit.img' )
    band = ds.GetRasterBand(1)
    ov = band.GetOverview(0)

    if ov.Checksum() != 4247:
        gdaltest.post_reason( 'did not get expected overview checksum' )
        return 'fail'

    ds_md = ds.GetMetadata()
    if ds_md['PyramidResamplingType'] != 'AVERAGE_BIT2GRAYSCALE':
        gdaltest.post_reason( 'wrong pyramid resampling type metadata.' )
        return 'fail'

    return 'success'
    
###############################################################################
# Confirm that we can create overviews in rrd format for an .img file with
# the bit2grayscale algorithm (#2914)

def hfa_write_bit2grayscale():

    import shutil
    
    shutil.copyfile('data/small1bit.img' , 'tmp/small1bit.img')
    shutil.copyfile('data/small1bit.rrd' , 'tmp/small1bit.rrd')

    gdal.SetConfigOption( 'USE_RRD', 'YES' )
    gdal.SetConfigOption( 'HFA_USE_RRD', 'YES' )
    
    ds = gdal.Open( 'tmp/small1bit.img', gdal.GA_Update )
    ds.BuildOverviews( resampling = 'average_bit2grayscale',
                       overviewlist = [2] )

    ov = ds.GetRasterBand(1).GetOverview(1)

    if ov.Checksum() != 57325:
        gdaltest.post_reason( 'wrong checksum for greyscale overview.' )
        return 'fail'
		
    ds = None

    gdal.GetDriverByName('HFA').Delete('tmp/small1bit.img')
    
    gdal.SetConfigOption( 'USE_RRD', 'NO' )
    gdal.SetConfigOption( 'HFA_USE_RRD', 'NO' )

    # as an aside, confirm the .rrd file was deleted.
    try:
        open('tmp/small1bit.rrd')
        gdaltest.post_reason( 'tmp/small1bit.rrd not deleted!' )
        return 'fail'
    except:
        pass
    
    return 'success'
    
###############################################################################
# Verify handling of camera model metadata (#2675)

def hfa_camera_md():

    ds = gdal.Open( '/vsisparse/data/251_sparse.xml' )

    md = ds.GetMetadata( 'CAMERA_MODEL' )

    check_list = [ ('direction','EMOD_FORWARD'),
                   ('forSrcAffine[0]','0.025004093931786'),
                   ('invDstAffine[0]','1'),
                   ('coeffs[1]','-0.008'),
                   ('elevationType','EPRJ_ELEVATION_TYPE_HEIGHT') ]
    for check_item in check_list:
        try:
            value = md[check_item[0]]
        except:
            gdaltest.post_reason( 'metadata item %d missing' % check_item[0])
            return 'fail'

        if value != check_item[1]:
            gdaltest.post_reason( 'metadata item %s has wrong value: %s' % \
                                  (check_item[0], value) )
            return 'fail'

    # Check that the SRS is reasonable.

    srs_wkt = md['outputProjection']
    exp_wkt = 'PROJCS["UTM Zone 17, Northern Hemisphere",GEOGCS["NAD27",DATUM["North_American_Datum_1927",SPHEROID["Clarke 1866",6378206.4,294.978698213898,AUTHORITY["EPSG","7008"]],TOWGS84[-10,158,187,0,0,0,0],AUTHORITY["EPSG","6267"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9108"]],AUTHORITY["EPSG","4267"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",-81],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["Meter",1],AUTHORITY["EPSG","26717"]]'

    if not gdaltest.equal_srs_from_wkt(srs_wkt,exp_wkt):
        gdaltest.post_reason( 'wrong outputProjection' )
        return 'fail'

    ds = None
    return 'success'
   
###############################################################################
# Check that overviews with an .rde file are properly supported in file list,
# and fetching actual overviews.

def hfa_rde_overviews():

    # Create an imagine file, forcing creation of an .ige file.

    ds = gdal.Open( 'data/spill.img' )

    exp_cs = 1631
    cs = ds.GetRasterBand(1).Checksum()

    if exp_cs != cs:
        print( cs )
        gdaltest.post_reason( 'did not get expected band checksum' )
        return 'fail'

    exp_cs = 340
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    
    if exp_cs != cs:
        print( cs )
        gdaltest.post_reason( 'did not get expected overview checksum' )
        return 'fail'

    filelist = ds.GetFileList()
    exp_filelist = ['data/spill.img', 'data/spill.ige', 'data/spill.rrd', 'data/spill.rde']
    exp_filelist_win32 = ['data/spill.img', 'data\\spill.ige', 'data\\spill.rrd', 'data\\spill.rde']
    if filelist != exp_filelist and filelist != exp_filelist_win32:
        print( filelist )
        gdaltest.post_reason( 'did not get expected file list.' )
        return 'fail'

    ds = None
    
    return 'success'
 
###############################################################################
# Check that we can copy and rename a complex file set, and that the internal filenames
# in the .img and .rrd seem to be updated properly.

def hfa_copyfiles():

    drv = gdal.GetDriverByName( 'HFA' )
    drv.CopyFiles( 'tmp/newnamexxx_after_copy.img', 'data/spill.img' )

    drv.Rename( 'tmp/newnamexxx.img', 'tmp/newnamexxx_after_copy.img' )

    ds = gdal.Open( 'tmp/newnamexxx.img' )
    
    exp_cs = 340
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    
    if exp_cs != cs:
        print( cs )
        gdaltest.post_reason( 'did not get expected overview checksum' )
        return 'fail'

    filelist = ds.GetFileList()
    exp_filelist = ['tmp/newnamexxx.img', 'tmp/newnamexxx.ige', 'tmp/newnamexxx.rrd', 'tmp/newnamexxx.rde']
    exp_filelist_win32 = ['tmp/newnamexxx.img', 'tmp\\newnamexxx.ige', 'tmp\\newnamexxx.rrd', 'tmp\\newnamexxx.rde']
    if filelist != exp_filelist and filelist != exp_filelist_win32:
        print( filelist )
        gdaltest.post_reason( 'did not get expected file list.' )
        return 'fail'

    ds = None

    # Check that the filenames in the actual files seem to have been updated.
    img = open('tmp/newnamexxx.img', 'rb').read()
    img = str(img)
    if img.find('newnamexxx.rrd') == -1:
        gdaltest.post_reason( 'RRDNames not updated?' )
        return 'fail'

    if img.find('newnamexxx.ige') == -1:
        gdaltest.post_reason( 'spill file not updated?' )
        return 'fail'

    rrd = open('tmp/newnamexxx.rrd', 'rb').read()
    rrd = str(rrd)
    if rrd.find('newnamexxx.img') == -1:
        gdaltest.post_reason( 'DependentFile not updated?' )
        return 'fail'

    if rrd.find('newnamexxx.rde') == -1:
        gdaltest.post_reason( 'overview spill file not updated?' )
        return 'fail'

    drv.Delete( 'tmp/newnamexxx.img' )

    return 'success'


 
###############################################################################
# Test the ability to write a RAT (#999)

def hfa_write_rat():

    drv = gdal.GetDriverByName( 'HFA' )

    src_ds = gdal.Open( 'data/i8u_c_i.img' )

    rat = src_ds.GetRasterBand(1).GetDefaultRAT()

    dst_ds = drv.Create( 'tmp/write_rat.img', 100, 100, 1, gdal.GDT_Byte )

    dst_ds.GetRasterBand(1).SetDefaultRAT( rat )

    dst_ds = None
    src_ds = None

    rat = None

    ds = gdal.Open( 'tmp/write_rat.img' )
    rat = ds.GetRasterBand(1).GetDefaultRAT()

    if rat.GetColumnCount() != 6 \
       or rat.GetTypeOfCol(0) != gdal.GFT_Real \
       or rat.GetUsageOfCol(0) != gdal.GFU_Generic: # should be GFU_MinMax
        print(rat.GetColumnCount())
        print(rat.GetTypeOfCol(0))
        print(rat.GetUsageOfCol(0))
        gdaltest.post_reason( 'BinValues column wrong.')
        return 'fail'

    if rat.GetValueAsInt( 2, 0 ) != 4:
        print(rat.GetValueAsInt( 2, 0 ))
        gdaltest.post_reason( 'BinValues value wrong.' )
        return 'fail'

    if rat.GetValueAsInt( 4, 5 ) != 656:
        print(rat.GetValueAsInt( 4, 5 ))
        gdaltest.post_reason( 'Histogram value wrong.' )
        return 'fail'

    rat = None
    ds = None

    drv.Delete( 'tmp/write_rat.img' )
    
    return 'success'

###############################################################################
# Test STATISTICS creation option

def hfa_createcopy_statistics():

    tmpAuxXml = '../gcore/data/byte.tif.aux.xml'
    try:
        os.remove(tmpAuxXml)
    except:
        pass
    ds_src = gdal.Open('../gcore/data/byte.tif')
    out_ds = gdal.GetDriverByName('HFA').CreateCopy( '/vsimem/byte.img', ds_src, options = ['STATISTICS=YES'] )
    del out_ds
    ds_src = None
    if os.path.exists(tmpAuxXml): os.remove(tmpAuxXml)

    gdal.Unlink( '/vsimem/byte.img.aux.xml' )

    ds = gdal.Open('/vsimem/byte.img')
    md = ds.GetRasterBand(1).GetMetadata()
    ds = None

    gdal.GetDriverByName('HFA').Delete('/vsimem/byte.img')

    if md['STATISTICS_MINIMUM'] != '74':
        gdaltest.post_reason( 'STATISTICS_MINIMUM is wrong.' )
        print(md['STATISTICS_MINIMUM'])
        return 'fail'

    return 'success'


###############################################################################
#

gdaltest_list = [
    hfa_histread,
    hfa_histwrite,
    hfa_histrewrite,
    hfa_int_stats_1,
    hfa_int_stats_2,
    hfa_float_stats_1,
    hfa_float_stats_2,
    hfa_int_read,
    hfa_float_read,
    hfa_pe_read,
    hfa_pe_write,
    hfa_metadata_1,
    hfa_metadata_2,
    hfa_grow_rrdlist,
    hfa_clean_ige,
    hfa_corrupt_aux,
    hfa_mapinformation_units,
    hfa_nodata_write,
    hfa_nodata_read,
    hfa_rotated_read,
    hfa_rotated_write,
    hfa_vsimem,
    hfa_proName,
    hfa_read_empty_compressed,
    hfa_unique_values_color_table,
    hfa_unique_values_hist,
    hfa_xforms_3rd,
    hfa_delete_colortable,
    hfa_delete_colortable2,
    hfa_excluded_values,
    hfa_ov_nodata,
    hfa_read_bit2grayscale,
    hfa_write_bit2grayscale,
    hfa_camera_md,
    hfa_rde_overviews,
    hfa_copyfiles,
    hfa_write_rat,
    hfa_createcopy_statistics ]

if __name__ == '__main__':

    gdaltest.setup_run( 'hfa' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

