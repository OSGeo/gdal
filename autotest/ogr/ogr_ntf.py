#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGR NTF driver.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
# 
###############################################################################
# Copyright (c) 2009, Even Rouault <even dot rouault at mines dash paris dot org>
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

# The following tests will download sample data from
# http://www.ordnancesurvey.co.uk/oswebsite/products/meridian2/sampledata/meridian2ntf.exe
# and http://www.ordnancesurvey.co.uk/oswebsite/products/strategi/sampledata/stratntf.exe
#
# That data is subject to the terms of the 'Discover' Data License, that can be found here :
# http://www.ordnancesurvey.co.uk/oswebsite/products/sampledata/discoverdatalicense.html
#
# Verbatim copy of it :

###############################################################################
# 'Discover' Data License
#
# Thank you for your interest in this Sample Data. The terms and conditions below set out a legal agreement
# between you and Ordnance Survey for your use of the Sample Data. Please read these terms carefully. If you do
# not agree to these terms and conditions, you should not use, download or access the Sample Data.
#
#1  The Sample Data belongs to the Crown (or its suppliers).
#
#2  Ordnance Survey grants you a limited, personal, non-exclusive, non-transferable, free-of-charge and fully terminable
# licence to use the Sample Data for the purpose of internal testing and evaluation only. By way of example, this means
# that you are not permitted to (i) sub-license, transfer, share or otherwise #distribute the Sample Data to any other
# person; (ii) incorporate the Sample Data into your products or services (unless solely for the purposes of internal
# testing and evaluation); or (iii) commercially exploit the Sample Data.
#
#3  The Sample Data is provided "as is" and without any warranty as to quality, fitness for purpose, accuracy, availability
# or otherwise. You acknowledge that it is your responsibility to ensure that the Sample Data is suitable for your intended
# purposes.
#
#4  To the fullest extent permitted by law, Ordnance Survey excludes all liability for any loss or damage of whatever nature
# arising from any use of the Sample Data.
#
#5  You agree that Ordnance Survey (and its suppliers) shall retain all rights, title and interest in the Sample Data, including
# but not limited to any and all copyrights, patents, trade marks, trade secrets and all other intellectual property rights.
#
#6  You agree not to tamper with or remove any copyright, trade mark, trade mark symbol or other proprietary notice of
# Ordnance Survey (or its suppliers) contained in the Sample Data.
#
#7  Ordnance Survey may terminate this agreement immediately if you breach any of the terms and conditions.
# Ordnance Survey also reserves the right to terminate the agreement at any time on giving you thirty (30) days written
# notice (which may be given by email or by posting a notification on Ordnance Survey's website).
#
#8  These terms and conditions are governed by English law, and you agree to the exclusive jurisdiction of the English courts.
# (C) Crown copyright and/or database right 2009 Ordnance Survey
#
# v1.0 May 2009
###############################################################################


import os
import sys
from osgeo import ogr
from osgeo import osr

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
def ogr_ntf_1():

    if not gdaltest.download_file('http://www.ordnancesurvey.co.uk/oswebsite/products/strategi/sampledata/stratntf.exe', 'stratntf.exe'):
        return 'skip'

    try:
        os.stat('tmp/cache/SS.ntf')
    except:
        try:
            gdaltest.unzip('tmp/cache', 'tmp/cache/stratntf.exe')
            try:
                os.stat('tmp/cache/SS.ntf')
            except:
                return 'skip'
        except:
            return 'skip'

    ds = ogr.Open('tmp/cache/SS.ntf')
    if ds.GetLayerCount() != 5:
        return 'fail'

    layers = [ ('STRATEGI_POINT', ogr.wkbPoint, 9193),
               ('STRATEGI_LINE', ogr.wkbLineString, 8369),
               ('STRATEGI_TEXT', ogr.wkbPoint, 1335),
               ('STRATEGI_NODE', ogr.wkbNone, 10991),
               ('FEATURE_CLASSES', ogr.wkbNone, 224) ]

    for l in layers:
        lyr = ds.GetLayerByName(l[0])
        if lyr.GetLayerDefn().GetGeomType() != l[1]:
            return 'fail'
        if lyr.GetFeatureCount() != l[2]:
            print(lyr.GetFeatureCount())
            return 'fail'
        if l[1] != ogr.wkbNone:
            if lyr.GetSpatialRef().ExportToWkt().find('OSGB 1936') == -1:
                return 'fail'

    lyr = ds.GetLayerByName('STRATEGI_POINT')
    feat = lyr.GetNextFeature()
    if feat.GetGeometryRef().ExportToWkt() != 'POINT (222904 127850)':
        print(feat.GetGeometryRef().ExportToWkt())
        return 'fail'

    ds.Destroy()

    return 'success'


###############################################################################
def ogr_ntf_2():

    if not gdaltest.download_file('http://www.ordnancesurvey.co.uk/oswebsite/products/meridian2/sampledata/meridian2ntf.exe', 'meridian2ntf.exe'):
        return 'skip'

    try:
        os.stat('tmp/cache/Port_Talbot_NTF/SS78.ntf')
    except:
        try:
            gdaltest.unzip('tmp/cache', 'tmp/cache/meridian2ntf.exe')
            try:
                os.stat('tmp/cache/Port_Talbot_NTF/SS78.ntf')
            except:
                return 'skip'
        except:
            return 'skip'

    ds = ogr.Open('tmp/cache/Port_Talbot_NTF/SS78.ntf')
    if ds.GetLayerCount() != 5:
        return 'fail'

    layers = [ ('MERIDIAN2_POINT', ogr.wkbPoint, 408),
               ('MERIDIAN2_LINE', ogr.wkbLineString, 513),
               ('MERIDIAN2_TEXT', ogr.wkbPoint, 7),
               ('MERIDIAN2_NODE', ogr.wkbNone, 397),
               ('FEATURE_CLASSES', ogr.wkbNone, 50) ]

    for l in layers:
        lyr = ds.GetLayerByName(l[0])
        if lyr.GetLayerDefn().GetGeomType() != l[1]:
            return 'fail'
        if lyr.GetFeatureCount() != l[2]:
            print(lyr.GetFeatureCount())
            return 'fail'
        if l[1] != ogr.wkbNone:
            if lyr.GetSpatialRef().ExportToWkt().find('OSGB 1936') == -1:
                return 'fail'

    lyr = ds.GetLayerByName('MERIDIAN2_POINT')
    feat = lyr.GetNextFeature()
    if feat.GetGeometryRef().ExportToWkt() != 'POINT (275324 189274)':
        print(feat.GetGeometryRef().ExportToWkt())
        return 'fail'

    lyr = ds.GetLayerByName('MERIDIAN2_LINE')
    feat = lyr.GetNextFeature()
    if feat.GetGeometryRef().ExportToWkt() != 'LINESTRING (275324 189274,275233 189114,275153 189048)':
        print(feat.GetGeometryRef().ExportToWkt())
        return 'fail'

    ds.Destroy()

    return 'success'

gdaltest_list = [
    ogr_ntf_1,
    ogr_ntf_2 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_ntf' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

