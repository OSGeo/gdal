#!/usr/bin/env pytest
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



from osgeo import osr

###############################################################################
# Test for the http://trac.osgeo.org/gdal/ticket/3787 problem.
# Spherical datums should have inverse flattening parameter 0.0, not 1.0.
#


def test_osr_erm_1():

    for sphere_datum in ['SPHERE', 'SPHERE2', 'USSPHERE']:
        srs = osr.SpatialReference()
        srs.ImportFromERM('MRWORLD', sphere_datum, 'METRE')

        assert srs.GetInvFlattening() == 0.0 and not abs(srs.GetSemiMajor() - srs.GetSemiMinor() > 0.0000005), \
            'Wrong ERMapper spherical datum parameters (bug #3787). Be sure your "ecw_cs.wkt" is from 20890 revision or newer.'

    
###############################################################################
# Confirm that unsupported SRSes will be translated from/to EPSG:n
# format (#3955)
#


def test_osr_erm_2():

    srs = osr.SpatialReference()
    assert srs.ImportFromERM('EPSG:3395', 'EPSG:3395', 'METRE') == 0 and srs.IsProjected(), \
        'EPSG:n import failed.'

    srs2 = osr.SpatialReference()
    srs2.SetFromUserInput('EPSG:3395')

    assert srs2.IsSame(srs), 'EPSG:n import does not match.'



