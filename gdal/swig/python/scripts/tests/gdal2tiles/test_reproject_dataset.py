from unittest import mock, TestCase

from osgeo import gdal, osr

import gdal2tiles


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

    @mock.patch('gdal2tiles.gdal', spec=gdal)
    def test_returns_warped_vrt_dataset_when_from_srs_different_from_to_srs(self, mock_gdal):
        mock_gdal.AutoCreateWarpedVRT = mock.MagicMock(spec=gdal.Dataset)
        from_ds = mock.MagicMock(spec=gdal.Dataset)
        from_ds.GetGCPCount = mock.MagicMock(return_value=0)

        gdal2tiles.reproject_dataset(from_ds, self.mercator_srs, self.geodetic_srs)

        mock_gdal.AutoCreateWarpedVRT.assert_called_with(from_ds,
                                                         self.mercator_srs.ExportToWkt(),
                                                         self.geodetic_srs.ExportToWkt())
