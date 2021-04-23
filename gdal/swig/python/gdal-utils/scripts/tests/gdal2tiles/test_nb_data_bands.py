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
from osgeo import gdal

from osgeo_utils import gdal2tiles


class NbDataBandsTest(TestCase):

    def setUp(self):
        self.ds = mock.MagicMock()
        alphaband = mock.MagicMock()
        alphaband.GetMaskFlags = mock.MagicMock(return_value=gdal.GMF_ALPHA - 1)
        rasterband = mock.MagicMock()
        rasterband.GetMaskBand = mock.MagicMock(return_value=alphaband)
        self.ds.GetRasterBand = mock.MagicMock(return_value=rasterband)

    def test_1_band_means_gray(self):
        self.ds.RasterCount = 1

        self.assertEqual(gdal2tiles.nb_data_bands(self.ds), 1)

    def test_2_bands_means_gray_alpha(self):
        self.ds.RasterCount = 2

        self.assertEqual(gdal2tiles.nb_data_bands(self.ds), 1)

    def test_3_bands_means_rgb(self):
        self.ds.RasterCount = 3

        self.assertEqual(gdal2tiles.nb_data_bands(self.ds), 3)

    def test_4_bands_means_rgba(self):
        self.ds.RasterCount = 4

        self.assertEqual(gdal2tiles.nb_data_bands(self.ds), 3)

    def test_alpha_mask(self):
        SOME_RANDOM_NUMBER = 42
        self.ds.RasterCount = SOME_RANDOM_NUMBER
        self.ds.GetRasterBand(1).GetMaskBand().GetMaskFlags.return_value = gdal.GMF_ALPHA

        # Should consider there is an alpha band
        self.assertEqual(gdal2tiles.nb_data_bands(self.ds), SOME_RANDOM_NUMBER - 1)
