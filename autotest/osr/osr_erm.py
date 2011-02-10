#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test ERMapper spatial reference implementation.
# Author:   Andrey Kiselev, dron@ak4719.spb.edu
# 
###############################################################################
# Copyright (c) 2010, Andrey Kiselev <dron@ak4719.spb.edu>
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

import os
import sys

sys.path.append( '../pymod' )

import gdaltest
try:
    from osgeo import osr
except:
    import osr

###############################################################################
# Test for the http://trac.osgeo.org/gdal/ticket/3787 problem.
# Spherical datums should have inverse flattening parameter 0.0, not 1.0.
#

def osr_erm_1():
    
    for sphere_datum in ['SPHERE', 'SPHERE2', 'USSPHERE']:
        srs = osr.SpatialReference()
        srs.ImportFromERM('MRWORLD', sphere_datum, 'METRE')

        if srs.GetInvFlattening() != 0.0 \
           or abs(srs.GetSemiMajor() - srs.GetSemiMinor() > 0.0000005):
            gdaltest.post_reason('Wrong ERMapper spherical datum parameters (bug #3787). Be sure your "ecw_cs.wkt" is from 20890 revision or newer.')
            return 'fail'

    return 'success'

###############################################################################
# Confirm that unsupported SRSes will be translated from/to EPSG:n
# format (#3955)
#

def osr_erm_2():

    srs = osr.SpatialReference()
    if srs.ImportFromERM( 'EPSG:3395', 'EPSG:3395', 'METRE' ) != 0 \
       or not srs.IsProjected():
        gdaltest.post_reason( 'EPSG:n import failed.' )
        return 'fail'

    srs2 = osr.SpatialReference()
    srs2.SetFromUserInput('EPSG:3395')

    if not srs2.IsSame(srs):
        gdaltest.post_reason( 'EPSG:n import does not match.' )
        return 'fail'

    return 'success'

gdaltest_list = [ 
    osr_erm_1,
    osr_erm_2,
    None ]

if __name__ == '__main__':

    gdaltest.setup_run( 'osr_erm' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

