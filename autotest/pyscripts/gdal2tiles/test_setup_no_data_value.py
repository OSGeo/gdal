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


class SetupNoDataValueTest(TestCase):

    def setUp(self):
        self.DEFAULT_OPTIONS = {
            'srcnodata': None,
            'verbose': True,
        }
        self.DEFAULT_ATTRDICT_OPTIONS = AttrDict(self.DEFAULT_OPTIONS)

        self.nodata_in_input = [1, 2, 3, 4]
        self.RASTER_COUNT = 4

        self.mock_dataset = mock.MagicMock()
        self.mock_dataset.RasterCount = self.RASTER_COUNT
        self.mock_dataset.GetRasterBand().GetNoDataValue = mock.MagicMock(
            side_effect=self.nodata_in_input)

    def test_reads_values_from_input_dataset(self):
        nodata = gdal2tiles.setup_no_data_values(self.mock_dataset, self.DEFAULT_ATTRDICT_OPTIONS)

        self.assertEqual(nodata, self.nodata_in_input)

    def test_uses_the_passed_arguments_in_priority(self):
        arg_nodata = "0,2,4,8"
        self.DEFAULT_ATTRDICT_OPTIONS['srcnodata'] = arg_nodata

        nodata = gdal2tiles.setup_no_data_values(self.mock_dataset, self.DEFAULT_ATTRDICT_OPTIONS)

        self.assertEqual(nodata, [float(x) for x in arg_nodata.split(',')])

    def test_extends_passed_arguments_if_not_enough_values_given(self):
        def test_with_args(arg_nodata):
            self.DEFAULT_ATTRDICT_OPTIONS['srcnodata'] = arg_nodata

            nodata = gdal2tiles.setup_no_data_values(
                self.mock_dataset, self.DEFAULT_ATTRDICT_OPTIONS)

            extended_args = ",".join([arg_nodata] * self.RASTER_COUNT)

            self.assertEqual(
                nodata,
                [float(x) for x in extended_args.split(',')][:self.RASTER_COUNT]
            )

        test_with_args("0,2,4")
        test_with_args("0,2")
        test_with_args("0")
