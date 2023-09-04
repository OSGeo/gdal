#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test GetSubdatasetInfo API on GeoTIFF.
# Author:   Alessandro Pasotti, elpaso@itopen.it
#
#
###############################################################################
# Copyright (c) 2023, Alessandro Pasotti, elpaso@itopen.it
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


import pytest

from osgeo import gdal

pytestmark = pytest.mark.require_driver("GTiff")


###############################################################################
# Test gdal subdataset informational functions


@pytest.mark.parametrize(
    "filename,path_component,subdataset_component",
    (
        ("GTIFF_DIR:1:/data/twoimages.tif", "/data/twoimages.tif", "1"),
        (r"GTIFF_DIR:1:C:\data\twoimages.tif", r"C:\data\twoimages.tif", "1"),
    ),
)
def test_gdal_subdataset_get_filename(filename, path_component, subdataset_component):

    info = gdal.GetSubdatasetInfo(filename)
    if path_component == "":
        assert info is None
    else:
        assert info.GetPathComponent() == path_component
        assert info.GetSubdatasetComponent() == subdataset_component


@pytest.mark.parametrize(
    "subdataset_component,new_path",
    (
        ("GTIFF_DIR:1:/data/twoimages.tif", "GTIFF_DIR:1:/new/test.tif"),
        (r"GTIFF_DIR:1:C:\data\twoimages.tif", "GTIFF_DIR:1:/new/test.tif"),
    ),
)
def test_gdal_subdataset_modify_filename(subdataset_component, new_path):

    info = gdal.GetSubdatasetInfo(subdataset_component)
    if new_path == "":
        assert info is None
    else:
        assert info.ModifyPathComponent("/new/test.tif") == new_path
