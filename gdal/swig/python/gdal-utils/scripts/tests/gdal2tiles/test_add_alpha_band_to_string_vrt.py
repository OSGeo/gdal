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


class AddAlphaBandToStringVrtTest(TestCase):

    def test_adds_the_correct_band_info_3_bands(self):
        with open(os.path.join(
                os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                "data",
                "warped_rgb.vrt"), 'r') as f:
            orig_vrt = f.read()

        modif_vrt = gdal2tiles.add_alpha_band_to_string_vrt(orig_vrt)

        vrt_root = ElementTree.fromstring(modif_vrt)
        band4 = vrt_root.findall(".//VRTRasterBand[@band='4']")

        self.assertIsNotNone(band4)
        self.assertEqual(len(band4), 1)

        band4_color = band4[0].find("./ColorInterp")
        self.assertEqual(band4_color.text, "Alpha")

    def test_band_is_in_the_right_place(self):
        """
        This is likely not necessary from a file format/machine standpoint, but is much better for a
        human looking at the file
        """
        with open(os.path.join(
                os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                "data",
                "warped_rgb.vrt"), 'r') as f:
            orig_vrt = f.read()

        modif_vrt = gdal2tiles.add_alpha_band_to_string_vrt(orig_vrt)

        vrt_root = ElementTree.fromstring(modif_vrt)

        nb_bands = 0
        for elem in list(vrt_root):
            if elem.tag == "VRTRasterBand":
                nb_bands += 1
                # Band should be numbered in increasing order, starting at 1
                self.assertEqual(elem.get("band"), str(nb_bands))
            else:
                if nb_bands:
                    # If we have already seen bands, exits, they should be all grouped
                    break

        self.assertEqual(nb_bands, 4)

    def test_adds_the_correct_band_info_1_band(self):
        with open(os.path.join(
                os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                "data",
                "warped_grey.vrt"), 'r') as f:
            orig_vrt = f.read()

        modif_vrt = gdal2tiles.add_alpha_band_to_string_vrt(orig_vrt)

        vrt_root = ElementTree.fromstring(modif_vrt)
        band2 = vrt_root.findall(".//VRTRasterBand[@band='2']")

        self.assertIsNotNone(band2)
        self.assertEqual(len(band2), 1)

        band2_color = band2[0].find("./ColorInterp")
        self.assertEqual(band2_color.text, "Alpha")

    def test_adds_the_alpha_option(self):
        with open(os.path.join(
                os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                "data",
                "warped_rgb.vrt"), 'r') as f:
            orig_vrt = f.read()

        modif_vrt = gdal2tiles.add_alpha_band_to_string_vrt(orig_vrt)

        vrt_root = ElementTree.fromstring(modif_vrt)

        alpha_band_option = vrt_root.find(".//GDALWarpOptions/DstAlphaBand")
        self.assertIsNotNone(alpha_band_option)
        self.assertEqual(alpha_band_option.text, "4")

        with open(os.path.join(
                os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                "data",
                "warped_grey.vrt"), 'r') as f:
            orig_vrt = f.read()

        modif_vrt = gdal2tiles.add_alpha_band_to_string_vrt(orig_vrt)

        vrt_root = ElementTree.fromstring(modif_vrt)

        alpha_band_option = vrt_root.find(".//GDALWarpOptions/DstAlphaBand")
        self.assertIsNotNone(alpha_band_option)
        self.assertEqual(alpha_band_option.text, "2")

    def test_adds_the_init_dest_option(self):
        with open(os.path.join(
                os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                "data",
                "warped_rgb.vrt"), 'r') as f:
            orig_vrt = f.read()

        modif_vrt = gdal2tiles.add_alpha_band_to_string_vrt(orig_vrt)

        vrt_root = ElementTree.fromstring(modif_vrt)

        init_dest_option = vrt_root.find(".//GDALWarpOptions/Option[@name='INIT_DEST']")
        self.assertIsNotNone(init_dest_option)
        self.assertEqual(init_dest_option.text, "0")
