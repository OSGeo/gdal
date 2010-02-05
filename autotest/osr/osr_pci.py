#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test some PCI specific translation issues.
# Author:   drey Kiselev, dron@remotesensing.org
# 
###############################################################################
# Copyright (c) 2004, Andrey Kiselev <dron@remotesensing.org>
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
import osr

###############################################################################
# Test the osr.SpatialReference.ImportFromPCI() function.
#

def osr_pci_1():
    
    srs = osr.SpatialReference()
    srs.ImportFromPCI('EC          E015', 'METRE', \
		      (0.0, 0.0, 45.0, 54.5, 47.0, 62.0, 0.0, 0.0, 0.0, 0.0, \
		       0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0))

    if abs(srs.GetProjParm(osr.SRS_PP_STANDARD_PARALLEL_1)-47.0)>0.0000005 \
       or abs(srs.GetProjParm(osr.SRS_PP_STANDARD_PARALLEL_2)-62.0)>0.0000005 \
       or abs(srs.GetProjParm(osr.SRS_PP_LATITUDE_OF_CENTER)-54.5)>0.0000005 \
       or abs(srs.GetProjParm(osr.SRS_PP_LONGITUDE_OF_CENTER)-45.0)>0.0000005 \
       or abs(srs.GetProjParm(osr.SRS_PP_FALSE_EASTING)-0.0)>0.0000005 \
       or abs(srs.GetProjParm(osr.SRS_PP_FALSE_NORTHING)-0.0)>0.0000005:
        gdaltest.post_reason('Can not import Equidistant Conic projection.')
        return 'fail'

    return 'success'

###############################################################################
# Test the osr.SpatialReference.ExportToPCI() function.
#

def osr_pci_2():
    
    srs = osr.SpatialReference()
    srs.ImportFromWkt("""PROJCS["unnamed",GEOGCS["NAD27",\
    DATUM["North_American_Datum_1927",\
    SPHEROID["Clarke 1866",6378206.4,294.9786982139006,\
    AUTHORITY["EPSG","7008"]],AUTHORITY["EPSG","6267"]],\
    PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433],\
    AUTHORITY["EPSG","4267"]],PROJECTION["Lambert_Conformal_Conic_2SP"],\
    PARAMETER["standard_parallel_1",33.90363402777778],\
    PARAMETER["standard_parallel_2",33.62529002777778],\
    PARAMETER["latitude_of_origin",33.76446202777777],\
    PARAMETER["central_meridian",-117.4745428888889],\
    PARAMETER["false_easting",0],PARAMETER["false_northing",0],\
    UNIT["metre",1,AUTHORITY["EPSG","9001"]]]""")

    (proj, units, parms) = srs.ExportToPCI()

    if proj != 'LCC         D-01' or units != 'METRE' \
       or abs(parms[2] - -117.4745429) > 0.0000005 \
       or abs(parms[3] - 33.76446203) > 0.0000005 \
       or abs(parms[4] - 33.90363403) > 0.0000005 \
       or abs(parms[5] - 33.62529003) > 0.0000005:
        gdaltest.post_reason('Can not import Lambert Conformal Conic projection.')
        return 'fail'

    return 'success'

###############################################################################
# Test MGRS interpretation. (#3379)
#

def osr_pci_3():
    
    srs = osr.SpatialReference()
    srs.ImportFromPCI('UTM    13   D000', 'METRE', \
		      (0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, \
		       0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0))

    wkt = srs.ExportToWkt()
    if wkt.find('13, Northern Hemi') == -1:
        gdaltest.post_reason( 'did not default to northern hemisphere!' )

        
    srs = osr.SpatialReference()
    srs.ImportFromPCI('UTM    13 G D000', 'METRE', \
		      (0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, \
		       0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0))

    wkt = srs.ExportToWkt()
    if wkt.find('13, Southern Hemi') == -1:
        gdaltest.post_reason( 'did get southern  hemisphere!' )

    srs = osr.SpatialReference()
    srs.ImportFromPCI('UTM    13 X D000', 'METRE', \
		      (0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, \
		       0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0))

    wkt = srs.ExportToWkt()
    if wkt.find('13, Northern Hemi') == -1:
        gdaltest.post_reason( 'did get southern  hemisphere!' )
        
    return 'success'

gdaltest_list = [ 
    osr_pci_1,
    osr_pci_2,
    osr_pci_3,
    None ]

if __name__ == '__main__':

    gdaltest.setup_run( 'osr_pci' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

