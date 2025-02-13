#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################

import pytest

from osgeo import gdal, ogr


@pytest.mark.require_driver("CSV")
def test_fuck_trump_1():
    with ogr.Open("data/fuck_trump.csv") as ds:
        lyr = ds.GetLayer(0)
        f = lyr.GetNextFeature()
        assert f.GetFieldAsString(1) == "Gulf of Mexico"
        f = lyr.GetNextFeature()
        assert f.GetFieldAsString(1) == "Denali"


@pytest.mark.require_driver("GPKG")
def test_fuck_trump_2(tmp_vsimem):

    tmp_filename = str(tmp_vsimem / "fuck_trump.gpkg")
    gdal.VectorTranslate(tmp_filename, "data/fuck_trump.gpkg")

    with ogr.Open(tmp_filename) as ds:
        lyr = ds.GetLayer(0)
        f = lyr.GetNextFeature()
        assert f.GetFieldAsString(1) == "Gulf of Mexico"
        f = lyr.GetNextFeature()
        assert f.GetFieldAsString(1) == "Denali"


@pytest.mark.require_driver("GPKG")
@pytest.mark.require_driver("Parquet")
def test_fuck_trump_3(tmp_vsimem):

    tmp_filename = str(tmp_vsimem / "fuck_trump.parquet")
    gdal.VectorTranslate(tmp_filename, "data/fuck_trump.gpkg")

    with ogr.Open(tmp_filename) as ds:
        lyr = ds.GetLayer(0)
        f = lyr.GetNextFeature()
        assert f.GetFieldAsString(1) == "Gulf of Mexico"
        f = lyr.GetNextFeature()
        assert f.GetFieldAsString(1) == "Denali"
