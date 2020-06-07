#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test driver matadata xml
# Author:   Rene Buffat <buffat at gmail dot com>
#
###############################################################################
# Copyright (c) 2020, Even Rouault <even dot rouault at spatialys dot com>
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
from xml.etree.ElementTree import fromstring

all_driver_names = [gdal.GetDriver(i).GetDescription() for i in range(gdal.GetDriverCount())]
ogr_driver_names = [driver_name for driver_name in all_driver_names
                    if gdal.GetDriverByName(driver_name).GetMetadataItem('DCAP_VECTOR') == 'YES']
gdal_driver_names = [driver_name for driver_name in all_driver_names
                     if gdal.GetDriverByName(driver_name).GetMetadataItem('DCAP_RASTER') == 'YES']


@pytest.mark.parametrize('driver_name', all_driver_names)
def test_metadata_openoptionlist(driver_name):
    """ Test if DMD_OPENOPTIONLIST metadataitem is present and can be parsed """

    driver = gdal.GetDriverByName(driver_name)
    openoptionlist_xml = driver.GetMetadataItem('DMD_OPENOPTIONLIST')

    if openoptionlist_xml is not None and len(openoptionlist_xml) > 0:
        assert "OpenOptionList" in openoptionlist_xml

        # do not fail
        print(openoptionlist_xml)
        fromstring(openoptionlist_xml)


@pytest.mark.parametrize('driver_name', all_driver_names)
def test_metadata_creationoptionslist(driver_name):
    """ Test if DMD_CREATIONOPTIONLIST metadataitem is present and can be parsed """

    driver = gdal.GetDriverByName(driver_name)
    creationoptions_xml = driver.GetMetadataItem('DMD_CREATIONOPTIONLIST')

    if creationoptions_xml is not None and len(creationoptions_xml) > 0:
        assert "CreationOptionList" in creationoptions_xml

        # do not fail
        print(creationoptions_xml)
        fromstring(creationoptions_xml)


@pytest.mark.parametrize('driver_name', ogr_driver_names)
def test_metadata_layer_creationoptionslist(driver_name):
    """ Test if DS_LAYER_CREATIONOPTIONLIST metadataitem is present and can be parsed """

    driver = gdal.GetDriverByName(driver_name)
    layer_creationoptions_xml = driver.GetMetadataItem('DS_LAYER_CREATIONOPTIONLIST')

    if layer_creationoptions_xml is not None and len(layer_creationoptions_xml) > 0:
        assert "LayerCreationOptionList" in layer_creationoptions_xml

        # do not fail
        print(layer_creationoptions_xml)
        fromstring(layer_creationoptions_xml)
