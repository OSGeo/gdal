import os
from unittest import TestCase
from xml.etree import ElementTree

import gdal2tiles


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
