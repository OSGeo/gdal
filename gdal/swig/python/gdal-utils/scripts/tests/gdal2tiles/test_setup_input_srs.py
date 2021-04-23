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

from osgeo_utils import gdal2tiles


class AttrDict(dict):
    def __init__(self, *args, **kwargs):
        super(AttrDict, self).__init__(*args, **kwargs)
        self.__dict__ = self


class SetupInputSrsTest(TestCase):

    def setUp(self):
        self.DEFAULT_OPTIONS = {
            's_srs': None,
        }
        self.DEFAULT_ATTRDICT_OPTIONS = AttrDict(self.DEFAULT_OPTIONS)

        self.nodata_in_input = [1, 2, 3, 4]
        self.RASTER_COUNT = 4

        self.mock_dataset = mock.MagicMock()
        self.mock_dataset.GetRasterBand().GetNoDataValue = mock.MagicMock(
            side_effect=self.nodata_in_input)

    @mock.patch('osgeo_utils.gdal2tiles.osr')
    def test_reads_values_from_input_dataset_with_projection_when_no_options(self, mock_osr):
        expected_srs = mock.MagicMock()
        expected_wkt = "expected_wkt"
        self.mock_dataset.GetProjection = mock.MagicMock(return_value=expected_wkt)
        mock_osr.SpatialReference = mock.MagicMock(return_value=expected_srs)

        input_srs, input_srs_wkt = gdal2tiles.setup_input_srs(
            self.mock_dataset, self.DEFAULT_ATTRDICT_OPTIONS)

        self.assertEqual(input_srs, expected_srs)
        self.assertEqual(input_srs_wkt, expected_wkt)
        mock_osr.SpatialReference().ImportFromWkt.assert_called_with(expected_wkt)

    @mock.patch('osgeo_utils.gdal2tiles.osr')
    def test_reads_values_from_input_dataset_wout_projection_with_gcps_when_no_options(self,
                                                                                       mock_osr):
        expected_wkt = "expected_wkt"
        expected_srs = mock.MagicMock()
        self.mock_dataset.GetProjection = mock.MagicMock(return_value=None)
        self.mock_dataset.GetGCPCount = mock.MagicMock(return_value=1)
        self.mock_dataset.GetGCPProjection = mock.MagicMock(return_value=expected_wkt)
        mock_osr.SpatialReference = mock.MagicMock(return_value=expected_srs)

        input_srs, input_srs_wkt = gdal2tiles.setup_input_srs(
            self.mock_dataset, self.DEFAULT_ATTRDICT_OPTIONS)

        self.assertEqual(input_srs, expected_srs)
        self.assertEqual(input_srs_wkt, expected_wkt)
        mock_osr.SpatialReference().ImportFromWkt.assert_called_with(expected_wkt)

    # def test_reads_values_from_input_dataset_with_neither_project_nor_gcps(self):

    @mock.patch('osgeo_utils.gdal2tiles.osr')
    def test_uses_the_passed_arguments_in_priority(self, mock_osr):
        option_srs = "o_srs"
        self.DEFAULT_ATTRDICT_OPTIONS['s_srs'] = option_srs
        expected_srs = mock.MagicMock()
        expected_wkt = "expected_wkt"
        mock_osr.SpatialReference = mock.MagicMock(return_value=expected_srs)
        mock_osr.SpatialReference().ExportToWkt = mock.MagicMock(return_value=expected_wkt)

        input_srs, input_srs_wkt = gdal2tiles.setup_input_srs(
            self.mock_dataset, self.DEFAULT_ATTRDICT_OPTIONS)

        self.assertEqual(input_srs, expected_srs)
        self.assertEqual(input_srs_wkt, expected_wkt)
        mock_osr.SpatialReference().SetFromUserInput.assert_called_with(option_srs)
