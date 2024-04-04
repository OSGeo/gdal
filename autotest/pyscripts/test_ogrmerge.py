#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  ogrmerge.py testing
# Author:   Even Rouault <even dot rouault at spatialys dot com>
#
###############################################################################
# Copyright (c) 2017, Even Rouault <even dot rouault at spatialys dot com>
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

import sys

import gdaltest
import pytest
import test_py_scripts
from test_py_scripts import samples_path

from osgeo import gdal, ogr

pytestmark = pytest.mark.skipif(
    test_py_scripts.get_py_script("ogrmerge") is None,
    reason="ogrmerge.py not available",
)


@pytest.fixture()
def script_path():
    return test_py_scripts.get_py_script("ogrmerge")


###############################################################################
#


def test_ogrmerge_help(script_path):

    assert "ERROR" not in test_py_scripts.run_py_script(
        script_path, "ogrmerge", "--help"
    )


###############################################################################
#


def test_ogrmerge_version(script_path):

    assert "ERROR" not in test_py_scripts.run_py_script(
        script_path, "ogrmerge", "--version"
    )


###############################################################################
# Test -single


def test_ogrmerge_1(script_path, tmp_path):

    out_shp = str(tmp_path / "out.shp")

    test_py_scripts.run_py_script(
        script_path,
        "ogrmerge",
        f"-single -o {out_shp} "
        + test_py_scripts.get_data_path("ogr")
        + "poly.shp "
        + test_py_scripts.get_data_path("ogr")
        + "poly.shp",
    )

    ds = ogr.Open(out_shp)
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 20
    ds = None


###############################################################################
# Test -append and glob


def test_ogrmerge_2(script_path, tmp_path):

    out_shp = str(tmp_path / "out.shp")

    test_py_scripts.run_py_script(
        script_path,
        "ogrmerge",
        f"-single -o {out_shp} " + test_py_scripts.get_data_path("ogr") + "poly.shp",
    )
    test_py_scripts.run_py_script(
        script_path,
        "ogrmerge",
        f'-append -single -o {out_shp} "'
        + test_py_scripts.get_data_path("ogr")
        + 'p*ly.shp"',
    )

    ds = ogr.Open(out_shp)
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 20
    ds = None


###############################################################################
# Test -overwrite_ds


def test_ogrmerge_3(script_path, tmp_path):

    out_shp = str(tmp_path / "out.shp")

    test_py_scripts.run_py_script(
        script_path,
        "ogrmerge",
        f"-overwrite_ds -o {out_shp} "
        + test_py_scripts.get_data_path("ogr")
        + "poly.shp",
    )
    test_py_scripts.run_py_script(
        script_path,
        "ogrmerge",
        f"-overwrite_ds -single -o {out_shp} "
        + test_py_scripts.get_data_path("ogr")
        + "poly.shp",
    )

    ds = ogr.Open(out_shp)
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 10
    ds = None


###############################################################################
# Test -f VRT


def test_ogrmerge_4(script_path, tmp_path):

    out_vrt = str(tmp_path / "out.vrt")

    test_py_scripts.run_py_script(
        script_path,
        "ogrmerge",
        f"-f VRT -o {out_vrt} " + test_py_scripts.get_data_path("ogr") + "poly.shp",
    )

    ds = ogr.Open(out_vrt)
    lyr = ds.GetLayer(0)
    assert lyr.GetName() == "poly"
    assert lyr.GetFeatureCount() == 10
    ds = None


###############################################################################
# Test -nln


def test_ogrmerge_5(script_path, tmp_path):

    out_vrt = str(tmp_path / "out.vrt")

    test_py_scripts.run_py_script(
        script_path,
        "ogrmerge",
        f"-f VRT -o {out_vrt} "
        + test_py_scripts.get_data_path("ogr")
        + "poly.shp "
        + test_py_scripts.get_data_path("ogr")
        + "shp/testpoly.shp -nln "
        '"foo_{DS_NAME}_{DS_BASENAME}_{DS_INDEX}_{LAYER_NAME}_{LAYER_INDEX}"',
    )

    ds = ogr.Open(out_vrt)
    lyr = ds.GetLayer(0)
    assert (
        lyr.GetName()
        == "foo_" + test_py_scripts.get_data_path("ogr") + "poly.shp_poly_0_poly_0"
    )
    assert lyr.GetFeatureCount() == 10
    lyr = ds.GetLayer(1)
    assert (
        lyr.GetName()
        == "foo_"
        + test_py_scripts.get_data_path("ogr")
        + "shp/testpoly.shp_testpoly_1_testpoly_0"
    )
    assert lyr.GetFeatureCount() == 14
    ds = None


###############################################################################
# Test -src_layer_field_name -src_layer_field_content


def test_ogrmerge_6(script_path, tmp_path):

    out_vrt = str(tmp_path / "out.vrt")

    test_py_scripts.run_py_script(
        script_path,
        "ogrmerge",
        f"-single -f VRT -o {out_vrt} "
        + test_py_scripts.get_data_path("ogr")
        + "poly.shp "
        "-src_layer_field_name source -src_layer_field_content "
        '"foo_{DS_NAME}_{DS_BASENAME}_{DS_INDEX}_{LAYER_NAME}_{LAYER_INDEX}"',
    )

    ds = ogr.Open(out_vrt)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if (
        f["source"]
        != "foo_" + test_py_scripts.get_data_path("ogr") + "poly.shp_poly_0_poly_0"
    ):
        f.DumpReadable()
        pytest.fail()
    ds = None


###############################################################################
# Test -src_geom_type


def test_ogrmerge_7(script_path, tmp_path):

    out_vrt = str(tmp_path / "out.vrt")

    if gdaltest.is_travis_branch("sanitize"):
        pytest.skip("fails on sanitize for unknown reason")

    # No match in -single mode
    test_py_scripts.run_py_script(
        script_path,
        "ogrmerge",
        f"-single -f VRT -o {out_vrt} "
        + test_py_scripts.get_data_path("ogr")
        + "poly.shp "
        "-src_geom_type POINT",
    )

    ds = ogr.Open(out_vrt)
    assert ds.GetLayerCount() == 0
    ds = None


def test_ogrmerge_7a(script_path, tmp_path):

    out_vrt = str(tmp_path / "out.vrt")

    # Match in single mode
    test_py_scripts.run_py_script(
        script_path,
        "ogrmerge",
        f"-single -f VRT -o {out_vrt} "
        + test_py_scripts.get_data_path("ogr")
        + "poly.shp "
        "-src_geom_type POLYGON",
    )

    ds = ogr.Open(out_vrt)
    assert ds.GetLayerCount() == 1
    ds = None


def test_ogrmerge_7b(script_path, tmp_path):

    out_vrt = str(tmp_path / "out.vrt")

    # No match in default mode
    test_py_scripts.run_py_script(
        script_path,
        "ogrmerge",
        f"-f VRT -o {out_vrt} " + test_py_scripts.get_data_path("ogr") + "poly.shp "
        "-src_geom_type POINT",
    )

    ds = ogr.Open(out_vrt)
    assert ds.GetLayerCount() == 0
    ds = None


def test_ogrmerge_7c(script_path, tmp_path):

    out_vrt = str(tmp_path / "out.vrt")

    # Match in default mode
    test_py_scripts.run_py_script(
        script_path,
        "ogrmerge",
        f"-f VRT -o {out_vrt} " + test_py_scripts.get_data_path("ogr") + "poly.shp "
        "-src_geom_type POLYGON",
    )

    ds = ogr.Open(out_vrt)
    assert ds.GetLayerCount() == 1
    ds = None


###############################################################################
# Test -s_srs -t_srs in -single mode


def test_ogrmerge_8(script_path, tmp_path):

    out_vrt = str(tmp_path / "out.vrt")

    test_py_scripts.run_py_script(
        script_path,
        "ogrmerge",
        f"-single -f VRT -o {out_vrt} "
        + test_py_scripts.get_data_path("ogr")
        + "poly.shp "
        "-s_srs EPSG:32630 -t_srs EPSG:4326",
    )

    ds = ogr.Open(out_vrt)
    assert ds is not None
    ds = None

    f = gdal.VSIFOpenL(out_vrt, "rb")
    content = ""
    if f is not None:
        content = gdal.VSIFReadL(1, 10000, f).decode("UTF-8")
        gdal.VSIFCloseL(f)

    assert "<SrcSRS>EPSG:32630</SrcSRS>" in content

    assert "<TargetSRS>EPSG:4326</TargetSRS>" in content


###############################################################################
# Test -s_srs -t_srs in default mode


def test_ogrmerge_9(script_path, tmp_path):

    out_vrt = str(tmp_path / "out.vrt")

    test_py_scripts.run_py_script(
        script_path,
        "ogrmerge",
        f"-f VRT -o {out_vrt} " + test_py_scripts.get_data_path("ogr") + "poly.shp "
        "-s_srs EPSG:32630 -t_srs EPSG:4326",
    )

    ds = ogr.Open(out_vrt)
    assert ds is not None
    ds = None

    f = gdal.VSIFOpenL(out_vrt, "rb")
    content = ""
    if f is not None:
        content = gdal.VSIFReadL(1, 10000, f).decode("UTF-8")
        gdal.VSIFCloseL(f)

    assert "<SrcSRS>EPSG:32630</SrcSRS>" in content

    assert "<TargetSRS>EPSG:4326</TargetSRS>" in content


###############################################################################
# Test -a_srs in -single mode


def test_ogrmerge_10(script_path, tmp_path):

    out_vrt = str(tmp_path / "out.vrt")

    test_py_scripts.run_py_script(
        script_path,
        "ogrmerge",
        f"-single -f VRT -o {out_vrt} "
        + test_py_scripts.get_data_path("ogr")
        + "poly.shp "
        "-a_srs EPSG:32630",
    )

    ds = ogr.Open(out_vrt)
    assert ds is not None
    ds = None

    f = gdal.VSIFOpenL(out_vrt, "rb")
    content = ""
    if f is not None:
        content = gdal.VSIFReadL(1, 10000, f).decode("UTF-8")
        gdal.VSIFCloseL(f)

    assert "<LayerSRS>EPSG:32630</LayerSRS>" in content


###############################################################################
# Test -a_srs in default mode


def test_ogrmerge_11(script_path, tmp_path):

    out_vrt = str(tmp_path / "out.vrt")

    test_py_scripts.run_py_script(
        script_path,
        "ogrmerge",
        f"-f VRT -o {out_vrt} " + test_py_scripts.get_data_path("ogr") + "poly.shp "
        "-a_srs EPSG:32630",
    )

    ds = ogr.Open(out_vrt)
    assert ds is not None
    ds = None

    f = gdal.VSIFOpenL(out_vrt, "rb")
    content = ""
    if f is not None:
        content = gdal.VSIFReadL(1, 10000, f).decode("UTF-8")
        gdal.VSIFCloseL(f)

    assert "<LayerSRS>EPSG:32630</LayerSRS>" in content


###############################################################################
# Test layer names with accents


def test_ogrmerge_12(script_path, tmp_path):

    tmp_json = str(tmp_path / "tmp.json")
    out_vrt = str(tmp_path / "out.vrt")

    with open(tmp_json, "wb") as f:
        f.write(
            b"""{ "type": "FeatureCollection", "name": "\xc3\xa9ven", "features": [ { "type": "Feature", "properties": {}, "geometry": null} ]}"""
        )

    test_py_scripts.run_py_script(
        script_path, "ogrmerge", f"-f VRT -o {out_vrt} {tmp_json}"
    )

    ds = ogr.Open(out_vrt)
    assert ds is not None
    ds = None


###############################################################################
# Validate a geopackage


def has_validate():
    path = samples_path
    if path not in sys.path:
        sys.path.append(path)
    try:
        import validate_gpkg

        validate_gpkg.check
    except ImportError:
        print("Cannot import validate_gpkg")
        return False
    return True


def _validate_check(filename):
    if not has_validate():
        return
    import validate_gpkg

    validate_gpkg.check(filename, extra_checks=True, warning_as_error=True)


###############################################################################
# Test GPKG optimization


@pytest.mark.require_driver("GPKG")
@pytest.mark.parametrize(
    "src_has_spatial_index,dst_has_spatial_index,has_progress,a_srs,s_srs,t_srs",
    [
        (True, True, True, None, None, None),
        (True, False, True, None, None, None),
        (False, True, True, None, None, None),
        (False, False, True, None, None, None),
        (True, True, False, None, None, None),
        (True, False, False, None, None, None),
        (False, True, False, None, None, None),
        (False, False, False, None, None, None),
        (True, True, False, None, None, None),
        (True, True, False, "EPSG:32631", None, None),
        (True, True, False, None, "EPSG:32631", None),  # s_srs ignored
        (True, True, False, None, None, "EPSG:32631"),
        (True, True, False, None, "EPSG:32631", "EPSG:4326"),
    ],
)
def test_ogrmerge_gpkg(
    script_path,
    tmp_path,
    src_has_spatial_index,
    dst_has_spatial_index,
    has_progress,
    a_srs,
    s_srs,
    t_srs,
):

    lco = [] if src_has_spatial_index else ["SPATIAL_INDEX=NO"]

    in_gpkg = str(tmp_path / "in.gpkg")
    out_gpkg = str(tmp_path / "out.gpkg")

    gdal.VectorTranslate(
        in_gpkg,
        test_py_scripts.get_data_path("ogr") + "poly.shp",
        layerCreationOptions=lco,
    )

    ogrmerge_opts = f"-f GPKG -o {out_gpkg} {in_gpkg} -nln poly"
    if not dst_has_spatial_index:
        ogrmerge_opts += " -lco SPATIAL_INDEX=NO"
    if has_progress:
        ogrmerge_opts += " -progress"
    if a_srs:
        ogrmerge_opts += " -a_srs " + a_srs
    if s_srs:
        ogrmerge_opts += " -s_srs " + s_srs
    if t_srs:
        ogrmerge_opts += " -t_srs " + t_srs
    test_py_scripts.run_py_script(script_path, "ogrmerge", ogrmerge_opts)

    _validate_check(out_gpkg)

    src_ds = ogr.Open(in_gpkg)
    src_lyr = src_ds.GetLayer(0)
    expected_fc = src_lyr.GetFeatureCount()

    ds = ogr.Open(out_gpkg)
    lyr = ds.GetLayerByName("poly")
    assert lyr.GetLayerDefn().GetFieldCount() == src_lyr.GetLayerDefn().GetFieldCount()
    assert lyr.GetFeatureCount() == expected_fc
    assert len([f for f in lyr]) == expected_fc
    assert lyr.GetExtent() == src_lyr.GetExtent()
    if a_srs:
        assert (
            lyr.GetSpatialRef().GetAuthorityName(None)
            + ":"
            + lyr.GetSpatialRef().GetAuthorityCode(None)
            == a_srs
        )
    elif t_srs:
        assert (
            lyr.GetSpatialRef().GetAuthorityName(None)
            + ":"
            + lyr.GetSpatialRef().GetAuthorityCode(None)
            == t_srs
        )
    else:
        src_feat = src_lyr.GetNextFeature()
        lyr.ResetReading()
        feat = lyr.GetNextFeature()
        assert feat.ExportToJson() == src_feat.ExportToJson()
        assert lyr.GetSpatialRef().IsSame(src_lyr.GetSpatialRef())

    sql_lyr = ds.ExecuteSQL("SELECT HasSpatialIndex('poly', 'geom')")
    f = sql_lyr.GetNextFeature()
    v = f.GetField(0)
    ds.ReleaseResultSet(sql_lyr)
    assert v == dst_has_spatial_index

    src_ds = None
    ds = None


###############################################################################
# Test GPKG optimization for non-spatial layers


@pytest.mark.require_driver("GPKG")
@pytest.mark.parametrize("has_progress", [True, False])
def test_ogrmerge_gpkg_non_spatial(script_path, tmp_path, has_progress):

    in_gpkg = str(tmp_path / "in.gpkg")
    out_gpkg = str(tmp_path / "out.gpkg")

    src_ds = gdal.VectorTranslate(
        in_gpkg, test_py_scripts.get_data_path("ogr") + "idlink.dbf"
    )
    src_ds.GetLayer(0).SetMetadataItem("foo", "bar")
    src_ds = None

    ogrmerge_opts = f"-f GPKG -o {out_gpkg} {in_gpkg} -nln idlink"
    if has_progress:
        ogrmerge_opts += " -progress"
    test_py_scripts.run_py_script(script_path, "ogrmerge", ogrmerge_opts)

    _validate_check(out_gpkg)

    src_ds = ogr.Open(in_gpkg)
    src_lyr = src_ds.GetLayer(0)
    expected_fc = src_lyr.GetFeatureCount()

    ds = ogr.Open(out_gpkg)
    lyr = ds.GetLayerByName("idlink")
    assert lyr.GetLayerDefn().GetFieldCount() == src_lyr.GetLayerDefn().GetFieldCount()
    assert lyr.GetFeatureCount() == expected_fc
    src_feat = src_lyr.GetNextFeature()
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert feat.ExportToJson() == src_feat.ExportToJson()
    assert len([f for f in lyr]) == expected_fc
    assert lyr.GetMetadata_Dict() == {"foo": "bar"}

    src_ds = None
    ds = None


###############################################################################
# Test GPKG optimization when a curve geometry is in a GEOMETRY typed column


@pytest.mark.require_driver("GPKG")
def test_ogrmerge_gpkg_curve_geom_in_generic_layer(script_path, tmp_path):

    in_gpkg = str(tmp_path / "in.gpkg")
    out_gpkg = str(tmp_path / "out.gpkg")

    src_ds = ogr.GetDriverByName("GPKG").CreateDataSource(in_gpkg)
    src_lyr = src_ds.CreateLayer("test")
    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("CIRCULARSTRING(0 0,1 1,2 0)"))
    assert src_lyr.CreateFeature(f) == ogr.OGRERR_NONE
    src_ds = None

    test_py_scripts.run_py_script(
        script_path,
        "ogrmerge",
        f"-f GPKG -o {out_gpkg} {in_gpkg} -lco SPATIAL_INDEX=NO",
    )

    # Check that the gpkg_geom_CIRCULARSTRING extension is declared
    ds = ogr.Open(out_gpkg)
    sql_lyr = ds.ExecuteSQL(
        "SELECT 1 FROM gpkg_extensions WHERE extension_name = 'gpkg_geom_CIRCULARSTRING'"
    )
    assert sql_lyr.GetFeatureCount() == 1
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    _validate_check(out_gpkg)
