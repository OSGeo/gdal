#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write round-tripping of SRS for HFA/Imagine format.
# Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
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


import pytest

from osgeo import gdal
from osgeo import osr


###############################################################################
# Write a HFA/Imagine and read it back to check its SRS
epsg_list = [
    [2758, False],  # tmerc
    [2036, True],  # sterea   # failure caused by revert done in r22803
    [2046, False],  # tmerc
    [3031, True],  # stere
    [32661, True],  # stere
    [3035, False],  # laea
    [2062, False],  # lcc
    [2065, False],  # krovak
    [2066, False],  # cass
    [2964, False],  # aea
    [3410, True],  # cea
    [3786, True],  # eqc
    [2934, True],  # merc
    [27200, False],  # nzmg
    [2057, True],  # omerc
    [29100, False],  # poly
    [2056, False],  # somerc
    [2027, False],  # utm
    [4326, False],  # longlat
]


@pytest.mark.parametrize(
    'epsg_code,epsg_broken',
    epsg_list,
    ids=[str(r[0]) for r in epsg_list],
)
def test_hfa_srs(epsg_code, epsg_broken):
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(epsg_code)

    ds = gdal.GetDriverByName('HFA').Create('/vsimem/TestHFASRS.img', 1, 1)
    ds.SetProjection(sr.ExportToWkt())
    ds = None

    ds = gdal.Open('/vsimem/TestHFASRS.img')
    wkt = ds.GetProjectionRef()
    sr2 = osr.SpatialReference()
    sr2.SetFromUserInput(wkt)
    ds = None

    gdal.Unlink('/vsimem/TestHFASRS.img')

    # For EPSG:2065. Those 2 datums are translated into D_S_JTSK in ESRI WKT... So for the purpose of
    # comparison, substitute one for another
    if sr.ExportToWkt().find('"System_Jednotne_Trigonometricke_Site_Katastralni_Ferro"') != -1 and \
       sr2.ExportToWkt().find('"System_Jednotne_Trigonometricke_Site_Katastralni"') != -1:
        wkt2 = sr2.ExportToWkt().replace('"System_Jednotne_Trigonometricke_Site_Katastralni"', '"System_Jednotne_Trigonometricke_Site_Katastralni_Ferro"')
        sr2.SetFromUserInput(wkt2)

    if (epsg_code == 4326 and sr2.GetAuthorityCode(None) != '4326') or sr.IsSame(sr2) != 1:
        if epsg_broken:
            pytest.xfail('did not get expected SRS. known to be broken currently. FIXME!')

        print(sr)
        print(sr2)
        assert False, 'did not get expected SRS'
