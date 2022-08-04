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
# Copyright (c) 2021, Idan Miara <idan@miara.com>
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

import os
import tempfile
from unittest import mock, TestCase

from osgeo import gdal
from osgeo_utils import gdal2tiles

import pytest

class AttrDict(dict):
    def __init__(self, *args, **kwargs):
        super(AttrDict, self).__init__(*args, **kwargs)
        self.__dict__ = self


class OptionParserInputOutputTest(TestCase):

    def test_vanilla_input_output(self):
        _, input_file = tempfile.mkstemp()
        output_folder = tempfile.mkdtemp()
        parsed_input, parsed_output, options = gdal2tiles.process_args([input_file, output_folder])

        self.assertEqual(parsed_input, input_file)
        self.assertEqual(parsed_output, output_folder)
        self.assertNotEqual(options, {})

    def test_output_folder_is_the_input_file_folder_when_none_passed(self):
        _, input_file = tempfile.mkstemp()
        _, parsed_output, _ = gdal2tiles.process_args([input_file])

        self.assertEqual(parsed_output, os.path.basename(input_file))

    def _asserts_exits_with_code_2(self, params):
        with self.assertRaises(SystemExit) as cm:
            gdal2tiles.process_args(params)

        e = cm.exception
        self.assertEqual(str(e), '2')

    def test_exits_when_0_args_passed(self):
        self._asserts_exits_with_code_2([])

    def test_exits_when_more_than_2_free_parameters(self):
        self._asserts_exits_with_code_2(['input1.tiff', 'input2.tiff', 'output_folder'])

    def test_exits_when_input_file_does_not_exist(self):
        self._asserts_exits_with_code_2(['foobar.tiff'])

    def test_exits_when_first_param_is_not_a_file(self):
        folder = tempfile.gettempdir()
        self._asserts_exits_with_code_2([folder])


def mock_GetDriverByName_wo_webp(name, orig_GetDriverByName=gdal.GetDriverByName):
    if name == 'WEBP':
        return None
    return orig_GetDriverByName(name)


# pylint:disable=E1101
class OptionParserPostProcessingTest(TestCase):

    def setUp(self):
        self.DEFAULT_OPTIONS = {
            'verbose': True,
            'resampling': 'near',
            'title': '',
            'url': '',
            'tiledriver': 'PNG'
        }
        self.DEFAULT_ATTRDICT_OPTIONS = AttrDict(self.DEFAULT_OPTIONS)

    def _setup_gdal_patch(self, mock_gdal):
        mock_gdal.TermProgress_nocb = True
        mock_gdal.RegenerateOverview = True
        mock_gdal.GetCacheMax = lambda: 1024 * 1024
        return mock_gdal

    def test_title_is_untouched_if_set(self):
        title = "fizzbuzz"
        self.DEFAULT_ATTRDICT_OPTIONS['title'] = title

        options = gdal2tiles.options_post_processing(
            self.DEFAULT_ATTRDICT_OPTIONS, "bar.tiff", "baz")

        self.assertEqual(options.title, title)

    def test_title_default_to_input_filename_if_not_set(self):
        input_file = "foo/bar/fizz/buzz.tiff"

        options = gdal2tiles.options_post_processing(
            self.DEFAULT_ATTRDICT_OPTIONS, input_file, "baz")

        self.assertEqual(options.title, os.path.basename(input_file))

    def test_url_stays_empty_if_not_passed(self):
        options = gdal2tiles.options_post_processing(
            self.DEFAULT_ATTRDICT_OPTIONS, "foo.tiff", "baz")

        self.assertEqual(options.url, "")

    def test_url_ends_with_the_output_folder_last_component(self):
        output_folder = "foo/bar/fizz"
        url = "www.mysite.com/storage"
        self.DEFAULT_ATTRDICT_OPTIONS['url'] = url

        options = gdal2tiles.options_post_processing(
            self.DEFAULT_ATTRDICT_OPTIONS, "foo.tiff", output_folder)

        self.assertEqual(options.url, url + "/fizz/")

        # With already present trailing slashes
        output_folder = "foo/bar/fizz/"
        url = "www.mysite.com/storage/"
        self.DEFAULT_ATTRDICT_OPTIONS['url'] = url

        options = gdal2tiles.options_post_processing(
            self.DEFAULT_ATTRDICT_OPTIONS, "foo.tiff", output_folder)

        self.assertEqual(options.url, url + "fizz/")

    @mock.patch('osgeo_utils.gdal2tiles.gdal', spec=AttrDict())
    def test_average_resampling_supported_with_latest_gdal(self, mock_gdal):
        self._setup_gdal_patch(mock_gdal)
        self.DEFAULT_ATTRDICT_OPTIONS['resampling'] = "average"

        gdal2tiles.options_post_processing(self.DEFAULT_ATTRDICT_OPTIONS, "foo.tiff", "/bar/")
        # No error means it worked as expected

    def test_antialias_resampling_supported_with_numpy(self):
        gdal2tiles.numpy_available = True
        self.DEFAULT_ATTRDICT_OPTIONS['resampling'] = "antialias"

        gdal2tiles.options_post_processing(self.DEFAULT_ATTRDICT_OPTIONS, "foo.tiff", "/bar/")
        # No error means it worked as expected

    def test_antialias_resampling_not_supported_wout_numpy(self):
        gdal2tiles.numpy_available = False
        if hasattr(gdal2tiles, "numpy"):
            del gdal2tiles.numpy

        self.DEFAULT_ATTRDICT_OPTIONS['resampling'] = "antialias"

        with self.assertRaises(SystemExit):
            gdal2tiles.options_post_processing(self.DEFAULT_ATTRDICT_OPTIONS, "foo.tiff", "/bar/")

    def test_zoom_option_not_specified(self):
        self.DEFAULT_ATTRDICT_OPTIONS["zoom"] = None

        options = gdal2tiles.options_post_processing(self.DEFAULT_ATTRDICT_OPTIONS, "foo.tiff", "baz")

        self.assertEqual(options.zoom, [None, None])

    def test_zoom_option_single_level(self):
        self.DEFAULT_ATTRDICT_OPTIONS["zoom"] = "10"

        options = gdal2tiles.options_post_processing(self.DEFAULT_ATTRDICT_OPTIONS, "foo.tiff", "baz")

        self.assertEqual(options.zoom, [10, 10])

    def test_zoom_option_two_levels(self):
        self.DEFAULT_ATTRDICT_OPTIONS["zoom"] = '14-24'

        options = gdal2tiles.options_post_processing(self.DEFAULT_ATTRDICT_OPTIONS, "foo.tiff", "baz")

        self.assertEqual(options.zoom, [14, 24])

    def test_zoom_option_two_levels_automatic_max(self):
        self.DEFAULT_ATTRDICT_OPTIONS["zoom"] = '14-'

        options = gdal2tiles.options_post_processing(self.DEFAULT_ATTRDICT_OPTIONS, "foo.tiff", "baz")

        self.assertEqual(options.zoom, [14, None])

    @mock.patch('osgeo_utils.gdal2tiles.gdal.GetDriverByName', side_effect=mock_GetDriverByName_wo_webp)
    def test_tiledriver_wout_webp(self, mock_GetDriverByName_wo_webp):
        self.DEFAULT_ATTRDICT_OPTIONS['tiledriver'] = "WEBP"
        self.DEFAULT_ATTRDICT_OPTIONS['webp_quality'] = 70

        with self.assertRaises(SystemExit):
            gdal2tiles.options_post_processing(self.DEFAULT_ATTRDICT_OPTIONS, "foo.tiff", "/bar/")
 
    @mock.patch('osgeo_utils.gdal2tiles.gdal.GetDriverByName', side_effect=mock_GetDriverByName_wo_webp)
    def test_tiledriver_wout_webp_png_accepted(self, mock_GetDriverByName_wo_webp):
        self.DEFAULT_ATTRDICT_OPTIONS['tiledriver'] = "PNG"

        options = gdal2tiles.options_post_processing(self.DEFAULT_ATTRDICT_OPTIONS, "foo.tiff", "/bar/")

        self.assertEqual(options.tiledriver, "PNG")

    def test_tiledriver_webp_quality_option_valid(self):
        if gdal.GetDriverByName('WEBP') is None:
            pytest.skip()

        self.DEFAULT_ATTRDICT_OPTIONS['tiledriver'] = "WEBP"
        self.DEFAULT_ATTRDICT_OPTIONS["webp_quality"] = 1
        self.DEFAULT_ATTRDICT_OPTIONS["webp_lossless"] = False

        options = gdal2tiles.options_post_processing(self.DEFAULT_ATTRDICT_OPTIONS, "foo.tiff", "/bar/")

        self.assertEqual(options.tiledriver, 'WEBP')
        self.assertEqual(options.webp_quality, 1)


        self.DEFAULT_ATTRDICT_OPTIONS["webp_quality"] = 100

        options = gdal2tiles.options_post_processing(self.DEFAULT_ATTRDICT_OPTIONS, "foo.tiff", "/bar/")

        self.assertEqual(options.tiledriver, 'WEBP')
        self.assertEqual(options.webp_quality, 100)


        self.DEFAULT_ATTRDICT_OPTIONS["webp_quality"] = 50

        options = gdal2tiles.options_post_processing(self.DEFAULT_ATTRDICT_OPTIONS, "foo.tiff", "/bar/")

        self.assertEqual(options.tiledriver, 'WEBP')
        self.assertEqual(options.webp_quality, 50)


        self.DEFAULT_ATTRDICT_OPTIONS["webp_quality"] = 10.5

        options = gdal2tiles.options_post_processing(self.DEFAULT_ATTRDICT_OPTIONS, "foo.tiff", "/bar/")

        self.assertEqual(options.tiledriver, 'WEBP')
        self.assertEqual(options.webp_quality, 10)

    def test_tiledriver_webp_quality_option_invalid(self):
        if gdal.GetDriverByName('WEBP') is None:
            pytest.skip()

        self.DEFAULT_ATTRDICT_OPTIONS['tiledriver'] = "WEBP"
        self.DEFAULT_ATTRDICT_OPTIONS["webp_lossless"] = False
        self.DEFAULT_ATTRDICT_OPTIONS["webp_quality"] = -20

        with self.assertRaises(SystemExit):
            gdal2tiles.options_post_processing(self.DEFAULT_ATTRDICT_OPTIONS, "foo.tiff", "/bar/")


        self.DEFAULT_ATTRDICT_OPTIONS["webp_quality"] = 0

        with self.assertRaises(SystemExit):
            gdal2tiles.options_post_processing(self.DEFAULT_ATTRDICT_OPTIONS, "foo.tiff", "/bar/")


        self.DEFAULT_ATTRDICT_OPTIONS["webp_quality"] = 140

        with self.assertRaises(SystemExit):
            gdal2tiles.options_post_processing(self.DEFAULT_ATTRDICT_OPTIONS, "foo.tiff", "/bar/")


