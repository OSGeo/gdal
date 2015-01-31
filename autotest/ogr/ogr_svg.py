#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test SVG driver functionality.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
# 
###############################################################################
# Copyright (c) 2011, Even Rouault <even dot rouault at mines-paris dot org>
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

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
from osgeo import ogr

def ogr_svg_init():
    gdaltest.svg_ds = None

    try:
        gdaltest.svg_ds = ogr.Open( 'data/test.svg' )
    except:
        gdaltest.svg_ds = None

    if gdaltest.svg_ds is None:
        gdaltest.have_svg = 0
    else:
        gdaltest.have_svg = 1

    if not gdaltest.have_svg:
        return 'skip'

    if gdaltest.svg_ds.GetLayerCount() != 3:
        gdaltest.post_reason( 'wrong number of layers' )
        return 'fail'

    return 'success'

###############################################################################
# Test

def ogr_svg_1():
    if not gdaltest.have_svg:
        return 'skip'
    
    if gdaltest.svg_ds is None:
        return 'fail'

    lyr = gdaltest.svg_ds.GetLayerByName( 'points' )
    if lyr.GetFeatureCount() != 1:
        gdaltest.post_reason('wrong number of features')
        return 'fail'

    feat = lyr.GetNextFeature()

    if feat.GetFieldAsString('building') != 'yes':
        feat.DumpReadable()
        return 'fail'

    if ogrtest.check_feature_geometry( feat, 'POINT (-13610535.695141600444913 4561593.930507560260594)',
                                       max_error = 0.0001 ) != 0:
        feat.DumpReadable()
        return 'fail'

    lyr = gdaltest.svg_ds.GetLayerByName( 'lines' )
    if lyr.GetFeatureCount() != 1:
        gdaltest.post_reason('wrong number of features')
        return 'fail'

    feat = lyr.GetNextFeature()

    if ogrtest.check_feature_geometry( feat, 'LINESTRING (-13609855.59 4561479.26,-13609856.21 4561474.27,-13609860.03 4561468.87,-13609865.74 4561465.69,-13609869.54 4561465.06)',
                                       max_error = 0.0001 ) != 0:
        feat.DumpReadable()
        return 'fail'

    lyr = gdaltest.svg_ds.GetLayerByName( 'polygons' )
    if lyr.GetFeatureCount() != 1:
        gdaltest.post_reason('wrong number of features')
        return 'fail'

    feat = lyr.GetNextFeature()

    if ogrtest.check_feature_geometry( feat, 'POLYGON ((-13610027.72 4562403.66,-13609661.58 4562462.95,-13609671.33 4562516.4,-13609676.11 4562532.65,-13609692.36 4562552.71,-13609711.46 4562609.08,-13609721.97 4562634.89,-13609727.7 4562650.16,-13609727.7 4562666.41,-13609716.23 4562699.85,-13609698.09 4562758.14,-13609697.13 4562771.51,-13609706.68 4562811.64,-13609720.06 4562843.18,-13609723.88 4562863.23,-13609725.8 4562891.91,-13609721.02 4562919.61,-13609713.37 4562938.72,-13609701.91 4562954.97,-13609688.53 4562968.34,-13609668.47 4562979.8,-13609614.96 4562993.17,-13609589.16 4563005.6,-13609552.85 4563037.14,-13609530.88 4563053.37,-13609474.5 4563076.3,-13609487.81 4563109.75,-13609491.89 4563149.38,-13609478.48 4563157.66,-13609467.67 4563171.31,-13609462.25 4563189.21,-13609420.46 4563189.32,-13609401.89 4563191.92,-13609395.2 4563201.47,-13609287.23 4563264.53,-13609303.48 4563291.29,-13609330.23 4563313.26,-13609339.78 4563326.63,-13609342.66 4563340.96,-13609344.56 4563458.48,-13609341.7 4563482.38,-13609322.59 4563518.68,-13609304.43 4563574.1,-13609290.1 4563592.26,-13609289.15 4563615.19,-13609290.1 4563655.32,-13609287.23 4563675.38,-13609271.95 4563703.09,-13609263.35 4563739.4,-13609258.57 4563762.32,-13609250.73 4563760.48,-13609226.84 4563718.43,-13609214.42 4563688.81,-13609204.87 4563661.1,-13609191.49 4563641.03,-13609170.47 4563629.56,-13609137.03 4563632.44,-13609109.32 4563648.68,-13609097.85 4563676.39,-13609100.72 4563712.7,-13609102.63 4563800.59,-13609116.0 4563819.7,-13609156.13 4563850.28,-13609151.55 4563861.7,-13609044.54 4563885.58,-13609057.92 4563945.78,-13609058.88 4563959.15,-13609031.17 4563987.81,-13609014.93 4563969.66,-13608988.17 4563981.13,-13608918.43 4563946.74,-13608834.46 4563870.62,-13608756.43 4563811.52,-13608716.02 4563488.23,-13608439.74 4563228.22,-13608483.69 4563167.08,-13608471.27 4563156.56,-13608461.72 4563132.68,-13608457.89 4563102.1,-13608460.76 4563077.26,-13608464.58 4563054.33,-13608444.52 4563044.78,-13608428.02 4562925.57,-13608408.91 4562672.38,-13608471.97 4562671.42,-13608514.96 4562653.27,-13608586.63 4562653.27,-13608728.76 4562628.85,-13609304.17 4562530.01,-13609354.81 4562401.98,-13609349.2 4562281.92,-13609401.52 4562278.13,-13609426.46 4562253.26,-13609385.39 4562165.98,-13609374.88 4561992.09,-13609361.7 4561946.97,-13609413.36 4561935.48,-13609402.85 4561884.85,-13609429.6 4561890.57,-13609487.88 4561880.07,-13609495.53 4561931.65,-13609442.02 4561942.18,-13609454.44 4562017.65,-13609601.59 4561997.59,-13609881.1 4561949.97,-13609858.97 4561817.17,-13609878.07 4561814.31,-13609891.08 4561883.51,-13609912.1 4561880.63,-13609918.79 4561924.59,-13609922.03 4561942.33,-13609949.37 4561938.35,-13609971.5 4562072.19,-13610108.74 4562049.23,-13610117.33 4562098.92,-13610151.73 4562094.14,-13610154.59 4562109.42,-13610224.34 4562097.96,-13610222.43 4562079.81,-13610381.03 4562053.05,-13610401.1 4562051.15,-13610386.77 4561907.82,-13610286.45 4561921.2,-13610210.01 4561869.6,-13610188.04 4561873.42,-13610173.7 4561778.84,-13610177.52 4561770.24,-13610184.21 4561764.51,-13610403.97 4561729.15,-13610429.77 4561711.0,-13610442.18 4561704.31,-13610574.04 4561683.29,-13610579.77 4561748.26,-13610620.86 4561745.4,-13610652.39 4562062.61,-13610802.4 4562037.77,-13610765.13 4561654.62,-13610860.68 4561641.25,-13610892.21 4562001.46,-13610848.26 4562007.19,-13610850.37 4562027.78,-13610870.24 4562264.22,-13610821.87 4562270.93,-13610819.96 4562245.14,-13610749.26 4562250.86,-13610754.03 4562306.29,-13610861.04 4562285.26,-13610864.69 4562319.19,-13610873.3 4562318.24,-13610882.85 4562463.47,-13610835.12 4562470.5,-13610816.0 4562473.37,-13610816.96 4562481.96,-13610737.66 4562493.43,-13610746.26 4562564.13,-13610782.56 4562558.4,-13610784.48 4562580.38,-13610826.52 4562575.6,-13610828.43 4562590.89,-13610845.63 4562588.98,-13610891.49 4562583.24,-13610895.3 4562621.46,-13610829.38 4562629.11,-13610844.67 4562786.76,-13610613.45 4562823.06,-13610570.46 4562334.82,-13610699.56 4562312.97,-13610695.75 4562293.86,-13610689.66 4562256.58,-13610543.47 4562263.26,-13610548.25 4562316.77,-13610487.1 4562326.32,-13610483.27 4562297.66,-13610443.14 4562303.38,-13610446.97 4562333.96,-13610027.72 4562403.66))',
                                       max_error = 0.0001 ) != 0:
        feat.DumpReadable()
        return 'fail'

    return 'success'

###############################################################################
# 

def ogr_svg_cleanup():

    if gdaltest.svg_ds is not None:
        gdaltest.svg_ds.Destroy()
    gdaltest.svg_ds = None
    return 'success'

gdaltest_list = [ 
    ogr_svg_init,
    ogr_svg_1,
    ogr_svg_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_svg' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

