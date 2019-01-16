#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test DB2 V7.2 WKB support. DB2 7.2 had a corrupt WKB format
#           and OGR supports reading and writing it for compatibility (done
#           on behalf of Safe Software).
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
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



from osgeo import ogr
import pytest

###############################################################################
# Create a point in DB2 format, and verify the byte order flag.


def test_ogr_db2_hack_1():

    if ogr.SetGenerate_DB2_V72_BYTE_ORDER(1) != 0:
        pytest.skip()

    # XDR Case.
    geom = ogr.CreateGeometryFromWkt('POINT(10 20)')
    wkb = geom.ExportToWkb(byte_order=ogr.wkbXDR).decode('latin1')
    geom.Destroy()

    assert wkb[0] == '0', 'WKB wkbXDR point geometry has wrong byte order'

    # NDR Case.
    geom = ogr.CreateGeometryFromWkt('POINT(10 20)')
    wkb = geom.ExportToWkb(byte_order=ogr.wkbNDR).decode('latin1')
    geom.Destroy()

    assert wkb[0] == '1', 'WKB wkbNDR point geometry has wrong byte order'

###############################################################################
# Verify that we can turn DB2 V7.2 mode back off!


def test_ogr_db2_hack_2():

    assert ogr.SetGenerate_DB2_V72_BYTE_ORDER(0) == 0, \
        'SetGenerate to turn off hack failed!'

    # XDR Case.
    geom = ogr.CreateGeometryFromWkt('POINT(10 20)')
    wkb = geom.ExportToWkb(byte_order=ogr.wkbXDR).decode('latin1')
    geom.Destroy()

    assert wkb[0] == chr(0), 'WKB wkbXDR point geometry has wrong byte order'

    # NDR Case.
    geom = ogr.CreateGeometryFromWkt('POINT(10 20)')
    wkb = geom.ExportToWkb(byte_order=ogr.wkbNDR).decode('latin1')
    geom.Destroy()

    assert wkb[0] == chr(1), 'WKB wkbNDR point geometry has wrong byte order'


###############################################################################
# Try a more complex geometry, and verify we can read it back.

def test_ogr_db2_hack_3():

    if ogr.SetGenerate_DB2_V72_BYTE_ORDER(1) != 0:
        pytest.skip()

    wkt = 'MULTIPOLYGON (((10.00121344 2.99853145,10.00121344 1.99853145,11.00121343 1.99853148,11.00121343 2.99853148)),((10.00121344 2.99853145,10.00121344 3.99853145,9.00121345 3.99853143,9.00121345 2.99853143)))'

    geom = ogr.CreateGeometryFromWkt(wkt)
    wkb = geom.ExportToWkb()
    geom.Destroy()

    # Check primary byte order value.
    assert wkb.decode('latin1')[0] == '0' or wkb.decode('latin1')[0] == '1', \
        'corrupt primary geometry byte order'

    # Check component geometry byte order
    assert wkb.decode('latin1')[9] == '0' or wkb.decode('latin1')[9] == '1', \
        'corrupt sub-geometry byte order'

    geom = ogr.CreateGeometryFromWkb(wkb)
    assert geom.ExportToWkt() == wkt, ('Conversion to/from DB2 format seems to have '
                             'corrupted geometry.')

    geom.Destroy()

    ogr.SetGenerate_DB2_V72_BYTE_ORDER(0)



