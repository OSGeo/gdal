#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdal2tiles.py testing
# Author:   Gregory Bataille <gregory.bataille@gmail.com>
#
###############################################################################
# Copyright (c) 2017, Gregory Bataille <gregory.bataille@gmail.com>
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

from unittest import mock, TestCase

from osgeo import gdal, osr

from osgeo_utils import gdal2tiles


class AttrDict(dict):
    def __init__(self, *args, **kwargs):
        super(AttrDict, self).__init__(*args, **kwargs)
        self.__dict__ = self


class ReprojectDatasetTest(TestCase):

    def setUp(self):
        self.DEFAULT_OPTIONS = {
            'verbose': True,
            'resampling': 'near',
            'title': '',
            'url': '',
        }
        self.DEFAULT_ATTRDICT_OPTIONS = AttrDict(self.DEFAULT_OPTIONS)
        self.mercator_srs = osr.SpatialReference()
        self.mercator_srs.ImportFromEPSG(3857)
        self.geodetic_srs = osr.SpatialReference()
        self.geodetic_srs.ImportFromEPSG(4326)

    def test_raises_if_no_from_or_to_srs(self):
        with self.assertRaises(gdal2tiles.GDALError):
            gdal2tiles.reproject_dataset(None, None, self.mercator_srs)
        with self.assertRaises(gdal2tiles.GDALError):
            gdal2tiles.reproject_dataset(None, self.mercator_srs, None)

    def test_returns_dataset_unchanged_if_in_destination_srs_and_no_gcps(self):
        from_ds = mock.MagicMock()
        from_ds.GetGCPCount = mock.MagicMock(return_value=0)

        to_ds = gdal2tiles.reproject_dataset(from_ds, self.mercator_srs, self.mercator_srs)

        self.assertEqual(from_ds, to_ds)

    @mock.patch('osgeo_utils.gdal2tiles.gdal', spec=gdal)
    def test_returns_warped_vrt_dataset_when_from_srs_different_from_to_srs(self, mock_gdal):
        mock_gdal.AutoCreateWarpedVRT = mock.MagicMock(spec=gdal.Dataset)
        from_ds = mock.MagicMock(spec=gdal.Dataset)
        from_ds.GetGCPCount = mock.MagicMock(return_value=0)

        gdal2tiles.reproject_dataset(from_ds, self.mercator_srs, self.geodetic_srs)

        mock_gdal.AutoCreateWarpedVRT.assert_called_with(from_ds,
                                                         self.mercator_srs.ExportToWkt(),
                                                         self.geodetic_srs.ExportToWkt())
