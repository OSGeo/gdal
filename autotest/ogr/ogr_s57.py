#!/usr/bin/env python
###############################################################################
# $Id: ogr_s57.py,v 1.3 2006/02/16 21:02:03 fwarmerdam Exp $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test Some S57 Driver features.
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
#  $Log: ogr_s57.py,v $
#  Revision 1.3  2006/02/16 21:02:03  fwarmerdam
#  added DSSI support
#
#  Revision 1.2  2004/10/27 19:39:04  fwarmerdam
#  Fixed test geometry in step 4.
#
#  Revision 1.1  2004/08/30 18:33:22  warmerda
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
import osr
import gdal

###############################################################################
# Verify we can open the test file.

def ogr_s57_1():

    gdaltest.s57_ds = None

    # Clear S57 options if set or our results will be messed up.
    if gdal.GetConfigOption( 'OGR_S57_OPTIONS', '' ) != '':
        gdal.SetConfigOption( 'OGR_S57_OPTIONS', '' )
        
    gdaltest.s57_ds = ogr.Open( 'data/1B5X02NE.000' )    
    if gdaltest.s57_ds is None:
        gdaltest.post_reason( 'failed to open test file.' )
        return 'fail'

    return 'success'

###############################################################################
# Verify we have the set of expected layers and that some rough information
# matches our expectations. 

def ogr_s57_2():
    if gdaltest.s57_ds is None:
        return 'skip'

    layer_list = [ ('DSID', ogr.wkbNone, 1),
                   ('COALNE', ogr.wkbLineString, 1),
                   ('DEPARE', ogr.wkbUnknown, 4),
                   ('DEPCNT', ogr.wkbLineString, 4),
                   ('LNDARE', ogr.wkbUnknown, 1),
                   ('LNDELV', ogr.wkbUnknown, 2),
                   ('SBDARE', ogr.wkbUnknown, 2),
                   ('SLCONS', ogr.wkbUnknown, 1),
                   ('SLOTOP', ogr.wkbLineString, 1),
                   ('SOUNDG', ogr.wkbMultiPoint, 2),
                   ('M_COVR', ogr.wkbPolygon, 1),
                   ('M_NSYS', ogr.wkbPolygon, 1),
                   ('M_QUAL', ogr.wkbPolygon, 1) ]


    if gdaltest.s57_ds.GetLayerCount() != len(layer_list):
        gdaltest.post_reason( 'Did not get expected number of layers, likely cant find support files.' )
        return 'fail'
    
    for i in range(len(layer_list)):
        lyr = gdaltest.s57_ds.GetLayer( i )
        lyr_info = layer_list[i]
        
        if lyr.GetName() != lyr_info[0]:
            gdaltest.post_reason( 'Expected layer %d to be %s but it was %s.'\
                                  % (i+1, lyr_info[0], lyr.GetName()) )
            return 'fail'

        count = lyr.GetFeatureCount(force=1)
        if count != lyr_info[2]:
            gdaltest.post_reason( 'Expected %d features in layer %s, but got %d.' % (lyr_info[2], lyr_info[0], count) )
            return 'fail'

        if lyr.GetLayerDefn().GetGeomType() != lyr_info[1]:
            gdaltest.post_reason( 'Expected %d layer type in layer %s, but got %d.' % (lyr_info[1], lyr_info[0], lyr.GetLayerDefn().GetGeomType()) )
            return 'fail'
            
    return 'success'

###############################################################################
# Check the COALNE feature. 

def ogr_s57_3():
    if gdaltest.s57_ds is None:
        return 'skip'

    feat = gdaltest.s57_ds.GetLayerByName('COALNE').GetNextFeature()

    if feat is None:
        gdaltest.post_reason( 'Did not get expected COALNE feature at all.' )
        return 'fail'
    
    if feat.GetField( 'RCID' ) != 1 \
           or feat.GetField( 'LNAM' ) != 'FFFF7F4F0FB002D3' \
           or feat.GetField( 'OBJL' ) != 30 \
           or feat.GetField( 'AGEN' ) != 65535:
        gdaltest.post_reason( 'COALNE: did not get expected attributes' )
        return 'fail'

    wkt = 'LINESTRING (60.97683400 -32.49442600,60.97718200 -32.49453800,60.97742400 -32.49477400,60.97774800 -32.49504000,60.97791600 -32.49547200,60.97793000 -32.49581800,60.97794400 -32.49617800,60.97804400 -32.49647600,60.97800200 -32.49703800,60.97800200 -32.49726600,60.97805800 -32.49749400,60.97812800 -32.49773200,60.97827000 -32.49794800,60.97910200 -32.49848600,60.97942600 -32.49866600)'

    if ogrtest.check_feature_geometry( feat, wkt ):
        return 'fail'

    feat.Destroy()

    return 'success'

###############################################################################
# Check the M_QUAL feature.

def ogr_s57_4():
    if gdaltest.s57_ds is None:
        return 'skip'

    feat = gdaltest.s57_ds.GetLayerByName('M_QUAL').GetNextFeature()

    if feat is None:
        gdaltest.post_reason( 'Did not get expected M_QUAL feature at all.' )
        return 'fail'
    
    if feat.GetField( 'RCID' ) != 15 \
           or feat.GetField( 'OBJL' ) != 308 \
           or feat.GetField( 'AGEN' ) != 65535:
        gdaltest.post_reason( 'M_QUAL: did not get expected attributes' )
        return 'fail'

    wkt = 'POLYGON ((60.97683400 -32.49534000,60.97683400 -32.49762000,60.97683400 -32.49866600,60.97869000 -32.49866600,60.97942600 -32.49866600,60.98215200 -32.49866600,60.98316600 -32.49866600,60.98316600 -32.49755800,60.98316600 -32.49477000,60.98316600 -32.49350000,60.98146800 -32.49350000,60.98029800 -32.49350000,60.97947400 -32.49350000,60.97901600 -32.49350000,60.97683400 -32.49350000,60.97683400 -32.49442600,60.97683400 -32.49469800,60.97683400 -32.49534000))'

    if ogrtest.check_feature_geometry( feat, wkt ):
        return 'fail'

    feat.Destroy()

    return 'success'

###############################################################################
# Check the SOUNDG feature.

def ogr_s57_5():
    if gdaltest.s57_ds is None:
        return 'skip'

    feat = gdaltest.s57_ds.GetLayerByName('SOUNDG').GetNextFeature()

    if feat is None:
        gdaltest.post_reason( 'Did not get expected SOUNDG feature at all.' )
        return 'fail'
    
    if feat.GetField( 'RCID' ) != 20 \
           or feat.GetField( 'OBJL' ) != 129 \
           or feat.GetField( 'AGEN' ) != 65535:
        gdaltest.post_reason( 'SOUNDG: did not get expected attributes' )
        return 'fail'

    wkt = 'MULTIPOINT (60.98164400 -32.49449000 3.400,60.98134400 -32.49642400 1.400,60.97814200 -32.49487400 -3.200,60.98071200 -32.49519600 1.200)'

    if ogrtest.check_feature_geometry( feat, wkt ):
        return 'fail'

    feat.Destroy()

    return 'success'

###############################################################################
#  Cleanup

def ogr_s57_cleanup():

    if gdaltest.s57_ds is not None:
        gdaltest.s57_ds.Destroy()
        gdaltest.s57_ds = None

    return 'success'

gdaltest_list = [ 
    ogr_s57_1,
    ogr_s57_2,
    ogr_s57_3,
    ogr_s57_4,
    ogr_s57_5,
    ogr_s57_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_s57' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

