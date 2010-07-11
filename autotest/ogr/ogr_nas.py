#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  NAS Reading Driver testing.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
# 
###############################################################################
# Copyright (c) 2010, Even Rouault <even dot rouault at mines dash paris dot org>
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
import string

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
import ogr
import osr

###############################################################################
# Test reading a NAS file
#

def ogr_nas_1():

    try:
        drv = ogr.GetDriverByName('NAS')
    except:
        drv = None

    if drv is None:
        return 'skip'

    if not gdaltest.download_file('http://www.geodatenzentrum.de/gdz1/abgabe/testdaten/vektor/nas_testdaten_peine.zip', 'nas_testdaten_peine.zip'):
        return 'skip'

    try:
        os.stat('tmp/cache/BKG_NAS_Peine.xml')
    except:
        try:
            gdaltest.unzip( 'tmp/cache', 'tmp/cache/nas_testdaten_peine.zip')
            try:
                os.stat('tmp/cache/BKG_NAS_Peine.xml')
            except:
                return 'skip'
        except:
            return 'skip'

    ds = ogr.Open('tmp/cache/BKG_NAS_Peine.xml')
    if ds is None:
        gdaltest.post_reason('could not open dataset')
        return 'fail'

    if ds.GetLayerCount() != 41:
        gdaltest.post_reason('did not get expected layer count')
        return 'fail'

    lyr = ds.GetLayerByName('AX_Wohnplatz')
    feat = lyr.GetNextFeature()
    geom = feat.GetGeometryRef()

    if feat.GetField('name') != 'Ziegelei' or geom.ExportToWkt() != 'POINT (3575300 5805100)':
        feat.DumpReadable()
        return 'fail'

    relation_lyr = ds.GetLayerByName('ALKIS_beziehungen')
    feat = relation_lyr.GetNextFeature()
    if feat.GetField('beziehung_von') != 'DENIBKG1000001UG' or \
       feat.GetField('beziehungsart') != 'istTeilVon' or \
       feat.GetField('beziehung_zu') != 'DENIBKG1000000T6':
        feat.DumpReadable()
        return 'fail'

    ds = None

    return 'success'

gdaltest_list = [ 
    ogr_nas_1 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_nas' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

