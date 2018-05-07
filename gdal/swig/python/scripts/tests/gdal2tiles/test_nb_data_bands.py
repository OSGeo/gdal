from unittest import mock, TestCase
from osgeo import gdal

import gdal2tiles


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
