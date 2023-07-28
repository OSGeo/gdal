import os

import pytest

from osgeo import gdal


@pytest.fixture(scope="session")
def jpeg_version():

    dirname = os.path.dirname(__file__)

    with gdal.Open(f"{dirname}/data/jpeg/albania.jpg") as ds:
        assert ds.GetMetadataItem("JPEG_QUALITY", "IMAGE_STRUCTURE") == "80"

        cs = ds.GetRasterBand(2).Checksum()

        if cs == 34296:
            return "9b"
        elif cs == 34298:
            return "8"
        else:
            return "pre8"
