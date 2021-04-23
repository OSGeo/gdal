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

import os
from unittest import TestCase
from xml.etree import ElementTree

from osgeo_utils import gdal2tiles


class AddGdalWarpOptionStringTest(TestCase):

    def setUp(self):
        with open(os.path.join(
                os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                "data",
                "warped.vrt"), 'r') as f:
            self.orig_vrt = f.read()

    def test_changes_option_tag_based_on_input_options(self):
        modif_vrt = gdal2tiles.add_gdal_warp_options_to_string(self.orig_vrt, {
            "foo": "bar",
            "baz": "biz"
        })
        self.assertIn('<Option name="foo">bar</Option>', modif_vrt)
        self.assertIn('<Option name="baz">biz</Option>', modif_vrt)

    def test_no_changes_if_no_option(self):
        modif_vrt = gdal2tiles.add_gdal_warp_options_to_string(self.orig_vrt, {})

        self.assertEqual(modif_vrt, self.orig_vrt)

    def test_no_changes_if_no_option_tag_present(self):
        vrt_root = ElementTree.fromstring(self.orig_vrt)
        vrt_root.remove(vrt_root.find("GDALWarpOptions"))
        vrt_no_options = ElementTree.tostring(vrt_root).decode()

        modif_vrt = gdal2tiles.add_gdal_warp_options_to_string(vrt_no_options, {
            "foo": "bar",
            "baz": "biz"
        })

        self.assertEqual(modif_vrt, vrt_no_options)
