import pytest

from osgeo import ogr

###############################################################################
# Create table from data/poly.shp


@pytest.fixture(scope="module")
def poly_feat():

    with ogr.Open("data/poly.shp") as shp_ds:
        shp_lyr = shp_ds.GetLayer(0)

        return [feat for feat in shp_lyr]
