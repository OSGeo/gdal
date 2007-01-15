#!/usr/bin/env python
###############################################################################
# $Id: ogr_csv.py,v 1.5 2004/08/17 15:40:13 warmerda Exp $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test OGR CSV driver functionality.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
# 
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
# 
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
# 
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.
###############################################################################
# 
#  $Log: ogr_csv.py,v $
#  Revision 1.5  2004/08/17 15:40:13  warmerda
#  added test 10 - capabilitie testing
#
#  Revision 1.4  2004/08/17 14:57:31  warmerda
#  cleanup properlyi
#
#  Revision 1.3  2004/08/17 14:57:07  warmerda
#  added append test
#
#  Revision 1.2  2004/08/16 21:29:07  warmerda
#  Added deletelayer test
#
#  Revision 1.1  2004/08/16 20:23:02  warmerda
#  New
#
#

import os
import sys
import string

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
import ogr
import gdal

###############################################################################
# Open CSV datasource.

def ogr_csv_1():

    gdaltest.csv_ds = None
    gdaltest.csv_ds = ogr.Open( 'data/prime_meridian.csv' )
    
    if gdaltest.csv_ds is not None:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Verify the some attributes read properly.
#

def ogr_csv_2():
    if gdaltest.csv_ds is None:
        return 'skip'

    lyr = gdaltest.csv_ds.GetLayerByName( 'prime_meridian' )

    expect = ['8901', '8902', '8903', '8904' ]
    
    tr = ogrtest.check_features_against_list( lyr,'PRIME_MERIDIAN_CODE',expect)
    if not tr:
        return 'fail'

    lyr.ResetReading()

    expect = [ '', 'Instituto Geografico e Cadastral; Lisbon',
               'Institut Geographique National (IGN), Paris',
               'Instituto Geografico "Augustin Cadazzi" (IGAC); Bogota' ]
    
    tr = ogrtest.check_features_against_list( lyr,'INFORMATION_SOURCE',expect)
    if not tr:
        return 'fail'

    lyr.ResetReading()

    return 'success'

###############################################################################
# Copy prime_meridian.csv to a new subtree under the tmp directory.

def ogr_csv_3():
    if gdaltest.csv_ds is None:
        return 'skip'

    #######################################################
    # Ensure any old copy of our working datasource is cleaned up
    try:
        gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
        ogr.GetDriverByName('CSV').DeleteDataSource( 'tmp/csvwrk' )
        gdal.PopErrorHandler()
    except:
        pass

    #######################################################
    # Create CSV datasource (directory)
    gdaltest.csv_tmpds = \
         ogr.GetDriverByName('CSV').CreateDataSource( 'tmp/csvwrk' )

    #######################################################
    # Create layer (.csv file)
    gdaltest.csv_lyr1 = gdaltest.csv_tmpds.CreateLayer( 'pm1' )

    #######################################################
    # Setup Schema
    ogrtest.quick_create_layer_def( gdaltest.csv_lyr1,
                                    [ ('PRIME_MERIDIAN_CODE', ogr.OFTInteger),
                                      ('INFORMATION_SOURCE', ogr.OFTString) ] )
    
    #######################################################
    # Copy in matching prime meridian fields.

    dst_feat = ogr.Feature( feature_def = gdaltest.csv_lyr1.GetLayerDefn() )

    srclyr = gdaltest.csv_ds.GetLayerByName( 'prime_meridian' )
    
    feat = srclyr.GetNextFeature()
    
    while feat is not None:

        dst_feat.SetFrom( feat )
        gdaltest.csv_lyr1.CreateFeature( dst_feat )

        feat.Destroy()
        feat = srclyr.GetNextFeature()

    dst_feat.Destroy()
        
    return 'success'
    

###############################################################################
# Verify the some attributes read properly.
#
# NOTE: one weird thing is that in this pass the prime_meridian_code field
# is typed as integer instead of string since it is created literally.

def ogr_csv_4():
    if gdaltest.csv_ds is None:
        return 'skip'

    lyr = gdaltest.csv_lyr1

    expect = [8901, 8902, 8903, 8904 ]
    
    tr = ogrtest.check_features_against_list( lyr,'PRIME_MERIDIAN_CODE',expect)
    if not tr:
        return 'fail'

    lyr.ResetReading()

    expect = [ '', 'Instituto Geografico e Cadastral; Lisbon',
               'Institut Geographique National (IGN), Paris',
               'Instituto Geografico "Augustin Cadazzi" (IGAC); Bogota' ]
    
    tr = ogrtest.check_features_against_list( lyr,'INFORMATION_SOURCE',expect)
    if not tr:
        return 'fail'

    lyr.ResetReading()
    
    return 'success'

###############################################################################
# Copy prime_meridian.csv again, in CRLF mode.

def ogr_csv_5():
    if gdaltest.csv_ds is None:
        return 'skip'

    #######################################################
    # Create layer (.csv file)
    options = ['LINEFORMAT=CRLF',]
    gdaltest.csv_lyr2 = gdaltest.csv_tmpds.CreateLayer( 'pm2',
                                                        options = options )

    #######################################################
    # Setup Schema
    ogrtest.quick_create_layer_def( gdaltest.csv_lyr2,
                                    [ ('PRIME_MERIDIAN_CODE', ogr.OFTInteger),
                                      ('INFORMATION_SOURCE', ogr.OFTString) ] )
    
    #######################################################
    # Copy in matching prime meridian fields.

    dst_feat = ogr.Feature( feature_def = gdaltest.csv_lyr2.GetLayerDefn() )

    srclyr = gdaltest.csv_ds.GetLayerByName( 'prime_meridian' )
    srclyr.ResetReading()
    
    feat = srclyr.GetNextFeature()
    
    while feat is not None:

        dst_feat.SetFrom( feat )
        gdaltest.csv_lyr2.CreateFeature( dst_feat )

        feat.Destroy()
        feat = srclyr.GetNextFeature()

    dst_feat.Destroy()
        
    return 'success'
    

###############################################################################
# Verify the some attributes read properly.
#

def ogr_csv_6():
    if gdaltest.csv_ds is None:
        return 'skip'

    lyr = gdaltest.csv_lyr2

    expect = [8901, 8902, 8903, 8904 ]
    
    tr = ogrtest.check_features_against_list( lyr,'PRIME_MERIDIAN_CODE',expect)
    if not tr:
        return 'fail'

    lyr.ResetReading()

    expect = [ '', 'Instituto Geografico e Cadastral; Lisbon',
               'Institut Geographique National (IGN), Paris',
               'Instituto Geografico "Augustin Cadazzi" (IGAC); Bogota' ]
    
    tr = ogrtest.check_features_against_list( lyr,'INFORMATION_SOURCE',expect)
    if not tr:
        return 'fail'

    lyr.ResetReading()
    
    return 'success'

###############################################################################
# Delete a layer and verify it seems to have worked properly.
#

def ogr_csv_7():
    if gdaltest.csv_ds is None:
        return 'skip'

    lyr = gdaltest.csv_tmpds.GetLayer(0)
    if lyr.GetName() != 'pm1':
        gdaltest.post_reason( 'unexpected name for first layer' )
        return 'fail'

    err = gdaltest.csv_tmpds.DeleteLayer(0)

    if err != 0:
        gdaltest.post_reason( 'got error code from DeleteLayer' )
        return 'fail'

    if gdaltest.csv_tmpds.GetLayerCount() != 1 \
       or gdaltest.csv_tmpds.GetLayer(0).GetName() != 'pm2':
        gdaltest.post_reason( 'Layer not destroyed properly?' )
        return 'fail'

    gdaltest.csv_tmpds.Destroy()
    gdaltest.csv_tmpds = None

    return 'success'

###############################################################################
# Reopen and append a record then close.
#

def ogr_csv_8():
    if gdaltest.csv_ds is None:
        return 'skip'

    gdaltest.csv_tmpds = ogr.Open( 'tmp/csvwrk', update = 1 )

    lyr = gdaltest.csv_tmpds.GetLayer(0)

    feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )

    feat.SetField( 'PRIME_MERIDIAN_CODE', '7000' )
    feat.SetField( 'INFORMATION_SOURCE', 'This is a newline test\n' )

    lyr.CreateFeature( feat )

    feat.Destroy()

    gdaltest.csv_tmpds.Destroy()
    gdaltest.csv_tmpds = None

    return 'success'

###############################################################################
# Verify the some attributes read properly.
#

def ogr_csv_9():
    if gdaltest.csv_ds is None:
        return 'skip'

    gdaltest.csv_tmpds = ogr.Open( 'tmp/csvwrk', update=1 )

    lyr = gdaltest.csv_tmpds.GetLayer(0)

    expect = [ '8901', '8902', '8903', '8904', '7000' ]
    
    tr = ogrtest.check_features_against_list( lyr,'PRIME_MERIDIAN_CODE',expect)
    if not tr:
        return 'fail'

    lyr.ResetReading()

    expect = [ '', 'Instituto Geografico e Cadastral; Lisbon',
               'Institut Geographique National (IGN), Paris',
               'Instituto Geografico "Augustin Cadazzi" (IGAC); Bogota',
               'This is a newline test\n' ]
    
    tr = ogrtest.check_features_against_list( lyr,'INFORMATION_SOURCE',expect)
    if not tr:
        return 'fail'

    lyr.ResetReading()
    
    return 'success'

###############################################################################
# Verify some capabilities and related stuff.
#

def ogr_csv_10():
    if gdaltest.csv_ds is None:
        return 'skip'

    lyr = gdaltest.csv_ds.GetLayerByName( 'prime_meridian' )

    if lyr.TestCapability( 'SequentialWrite' ):
        gdaltest.post_reason( 'should not have write access to readonly layer')
        return 'fail'

    if lyr.TestCapability( 'RandomRead' ):
        gdaltest.post_reason( 'CSV files dont efficiently support random reading.')
        return 'fail'

    if lyr.TestCapability( 'FastGetExtent' ):
        gdaltest.post_reason( 'CSV files do not support getextent' )
        return 'fail'

    if lyr.TestCapability( 'FastFeatureCount' ):
        gdaltest.post_reason( 'CSV files do not support fast feature count')
        return 'fail'

    if not ogr.GetDriverByName('CSV').TestCapability( 'DeleteDataSource' ):
        gdaltest.post_reason( 'CSV files do support DeleteDataSource' )
        return 'fail'

    if not ogr.GetDriverByName('CSV').TestCapability( 'CreateDataSource' ):
        gdaltest.post_reason( 'CSV files do support CreateDataSource' )
        return 'fail'

    if gdaltest.csv_ds.TestCapability( 'CreateLayer' ):
        gdaltest.post_reason( 'readonly datasource should not CreateLayer' )
        return 'fail'

    if gdaltest.csv_ds.TestCapability( 'DeleteLayer' ):
        gdaltest.post_reason( 'should not have deletelayer on readonly ds.')
        return 'fail'

    lyr = gdaltest.csv_tmpds.GetLayer(0)

    if not lyr.TestCapability( 'SequentialWrite' ):
        gdaltest.post_reason( 'should have write access to updatable layer')
        return 'fail'

    if not gdaltest.csv_tmpds.TestCapability( 'CreateLayer' ):
        gdaltest.post_reason( 'should have createlayer on updatable ds.')
        return 'fail'

    if not gdaltest.csv_tmpds.TestCapability( 'DeleteLayer' ):
        gdaltest.post_reason( 'should have deletelayer on updatable ds.')
        return 'fail'

    return 'success'

###############################################################################
# 

def ogr_csv_cleanup():

    if gdaltest.csv_ds is None:
        return 'skip'

    gdaltest.csv_ds.Destroy()
    gdaltest.csv_ds = None

    try:
        gdaltest.csv_lyr1 = None
        gdaltest.csv_tmpds.Destroy()
        gdaltest.csv_tmpds = None
    except:
        pass

    try:
        gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
        ogr.GetDriverByName('CSV').DeleteDataSource( 'tmp/csvwrk' )
        gdal.PopErrorHandler()
    except:
        pass

    return 'success'

gdaltest_list = [
    ogr_csv_1,
    ogr_csv_2,
    ogr_csv_3,
    ogr_csv_4,
    ogr_csv_5,
    ogr_csv_6,
    ogr_csv_6,
    ogr_csv_7,
    ogr_csv_8,
    ogr_csv_9,
    ogr_csv_10,
    ogr_csv_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_csv' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

