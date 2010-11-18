#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test some PCI specific translation issues.
# Author:   Andrey Kiselev, dron@ak4719.spb.edu
# 
###############################################################################
# Copyright (c) 2004, Andrey Kiselev <dron@ak4719.spb.edu>
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
from osgeo import osr

###############################################################################
# Test the osr.SpatialReference.ImportFromPCI() function.
#

def osr_pci_1():

    prj_parms = (0.0, 0.0, 45.0, 54.5, 47.0, 62.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0)
    srs = osr.SpatialReference()
    srs.ImportFromPCI('EC          E015', 'METRE', prj_parms )

    if abs(srs.GetProjParm(osr.SRS_PP_STANDARD_PARALLEL_1)-47.0)>0.0000005 \
       or abs(srs.GetProjParm(osr.SRS_PP_STANDARD_PARALLEL_2)-62.0)>0.0000005 \
       or abs(srs.GetProjParm(osr.SRS_PP_LATITUDE_OF_CENTER)-54.5)>0.0000005 \
       or abs(srs.GetProjParm(osr.SRS_PP_LONGITUDE_OF_CENTER)-45.0)>0.0000005 \
       or abs(srs.GetProjParm(osr.SRS_PP_FALSE_EASTING)-0.0)>0.0000005 \
       or abs(srs.GetProjParm(osr.SRS_PP_FALSE_NORTHING)-0.0)>0.0000005:
        gdaltest.post_reason('Can not import Equidistant Conic projection.')
        return 'fail'

    expected = 'PROJCS["unnamed",GEOGCS["Unknown - PCI E015",DATUM["Unknown - PCI E015",SPHEROID["Krassowsky 1940",6378245,298.3,AUTHORITY["EPSG","7024"]]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]],PROJECTION["Equidistant_Conic"],PARAMETER["standard_parallel_1",47],PARAMETER["standard_parallel_2",62],PARAMETER["latitude_of_center",54.5],PARAMETER["longitude_of_center",45],PARAMETER["false_easting",0],PARAMETER["false_northing",0],UNIT["Meter",1]]'
    
    if not gdaltest.equal_srs_from_wkt( expected, srs.ExportToWkt() ):
        return 'fail'

    pci_parms = srs.ExportToPCI()
    if pci_parms[0] != 'EC          E015' \
       or pci_parms[1] != 'METRE' \
       or pci_parms[2] != prj_parms:
        print( pci_parms )
        gdaltest.post_reason( 'ExportToPCI result wrong.' )
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

###############################################################################
# Test Datum lookup in pci_datum.txt
#

def osr_pci_4():
    
    prj_parms = (0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)

    srs = osr.SpatialReference()
    srs.ImportFromPCI('LONG/LAT    D506', 'DEGREE', prj_parms )

    expected = 'GEOGCS["Rijksdriehoeks Datum",DATUM["Rijksdriehoeks Datum",SPHEROID["Bessel 1841",6377397.155,299.1528128,AUTHORITY["EPSG","7004"]],TOWGS84[565.04,49.91,465.84,0.4094,-0.3597,1.8685,4.077200000063286]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]]'

    if not gdaltest.equal_srs_from_wkt( expected, srs.ExportToWkt() ):
        return 'fail'

    pci_parms = srs.ExportToPCI()
    if pci_parms[0] != 'LONG/LAT    D506' \
       or pci_parms[1] != 'DEGREE' \
       or pci_parms[2] != prj_parms:
        print( pci_parms )
        gdaltest.post_reason( 'ExportToPCI result wrong.' )
        return 'fail'
    
    return 'success'

###############################################################################
# Test Datum ellisoid lookup in pci_ellpis.txt
#

def osr_pci_5():
    
    prj_parms = (0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)

    srs = osr.SpatialReference()
    srs.ImportFromPCI('LONG/LAT    E224', 'DEGREE', prj_parms )

    expected = 'GEOGCS["Unknown - PCI E224",DATUM["Unknown - PCI E224",SPHEROID["Xian 1980",6378140,298.2569978029123]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]]'

    if not gdaltest.equal_srs_from_wkt( expected, srs.ExportToWkt() ):
        return 'fail'

    pci_parms = srs.ExportToPCI()
    if pci_parms[0] != 'LONG/LAT    E224' \
       or pci_parms[1] != 'DEGREE' \
       or pci_parms[2] != prj_parms:
        print( pci_parms )
        gdaltest.post_reason( 'ExportToPCI result wrong.' )
        return 'fail'
    
    return 'success'

###############################################################################
# Test Datum lookup in pci_datum.txt
#

def osr_pci_6():
    
    prj_parms = (0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)

    srs = osr.SpatialReference()
    srs.ImportFromPCI('LONG/LAT    D030', 'DEGREE', prj_parms )

    expected = 'GEOGCS["AGD84",DATUM["Australian_Geodetic_Datum_1984",SPHEROID["Australian National Spheroid",6378160,298.25,AUTHORITY["EPSG","7003"]],TOWGS84[-134,-48,149,0,0,0,0],AUTHORITY["EPSG","6203"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4203"]]'

    if not gdaltest.equal_srs_from_wkt( expected, srs.ExportToWkt() ):
        return 'fail'

    pci_parms = srs.ExportToPCI()
    if pci_parms[0] != 'LONG/LAT    D030' \
       or pci_parms[1] != 'DEGREE' \
       or pci_parms[2] != prj_parms:
        print( pci_parms )
        gdaltest.post_reason( 'ExportToPCI result wrong.' )
        return 'fail'
    
    return 'success'

###############################################################################
# Make sure we can translate a datum with only the TOWGS84 parameters to
# to identify it.
#

def osr_pci_7():
    
    srs = osr.SpatialReference()
    srs.SetFromUserInput( 'GEOGCS["My GCS",DATUM["My Datum",SPHEROID["Bessel 1841",6377397.155,299.1528128,AUTHORITY["EPSG","7004"]],TOWGS84[565.04,49.91,465.84,0.4094,-0.3597,1.8685,4.077200000063286]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]]' )

    prj_parms = (0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)

    pci_parms = srs.ExportToPCI()
    if pci_parms[0] != 'LONG/LAT    D506' \
       or pci_parms[1] != 'DEGREE' \
       or pci_parms[2] != prj_parms:
        print( pci_parms )
        gdaltest.post_reason( 'ExportToPCI result wrong.' )
        return 'fail'
    
    return 'success'

gdaltest_list = [ 
    osr_pci_1,
    osr_pci_2,
    osr_pci_3,
    osr_pci_4,
    osr_pci_5,
    osr_pci_6,
    osr_pci_7,
    None ]

if __name__ == '__main__':

    gdaltest.setup_run( 'osr_pci' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

