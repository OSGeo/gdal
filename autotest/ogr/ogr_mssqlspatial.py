#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test MSSQLSpatial driver functionality.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2018, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################


import gdaltest
import ogrtest
import pytest

from osgeo import gdal, ogr, osr

pytestmark = pytest.mark.require_driver("MSSQLSpatial")


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def module_disable_exceptions():
    with gdaltest.disable_exceptions():
        yield


###############################################################################
# Open Database.


@pytest.fixture(scope="module")
def mssql_ds():

    val = gdal.GetConfigOption("OGR_MSSQL_CONNECTION_STRING", None)
    if val is None:
        # localhost doesn't work under chroot
        dsname = "MSSQL:server=127.0.0.1;database=TestDB;driver=ODBC Driver 17 for SQL Server;UID=SA;PWD=DummyPassw0rd"
    else:
        dsname = val

    ds = ogr.Open(dsname, update=1)

    if ds is None:
        if val is None:
            pytest.skip(
                f"OGR_MSSSQL_CONNECTION_STRING not specified; MS SQL is not available using default connection string {dsname}"
            )
        else:
            pytest.skip(
                f"MS SQL is not available using supplied OGR_MSSQL_CONNECTION_STRING {dsname}"
            )

    return ds


@pytest.fixture(scope="module")
def mssql_version(mssql_ds):
    # Fetch and store the major-version number of the SQL Server engine in use
    with mssql_ds.ExecuteSQL("SELECT SERVERPROPERTY('ProductVersion')") as sql_lyr:
        feat = sql_lyr.GetNextFeature()

    version_str = feat.GetFieldAsString(0)

    version_major = -1
    if "." in version_str:
        version_major_str = version_str[0 : version_str.find(".")]
        if version_major_str.isdigit():
            version_major = int(version_major_str)

    return version_major


@pytest.fixture(scope="module")
def mssql_has_z_m(mssql_version):
    # Check whether the database server provides support for Z and M values,
    # available since SQL Server 2012

    return mssql_version >= 11


###############################################################################
# Create table from data/poly.shp


@pytest.fixture()
def tpoly(mssql_ds):

    shp_ds = ogr.Open("data/poly.shp")
    shp_lyr = shp_ds.GetLayer(0)

    ######################################################
    # Create Layer
    sql_lyr = mssql_ds.CreateLayer("tpoly", srs=shp_lyr.GetSpatialRef())

    ######################################################
    # Setup Schema
    ogrtest.quick_create_layer_def(
        sql_lyr,
        [
            ("AREA", ogr.OFTReal),
            ("EAS_ID", ogr.OFTInteger),
            ("PRFEDEA", ogr.OFTString),
            ("SHORTNAME", ogr.OFTString, 8),
            ("INT64", ogr.OFTInteger64),
        ],
    )

    ######################################################
    # Copy in poly.shp

    dst_feat = ogr.Feature(feature_def=sql_lyr.GetLayerDefn())

    feat = shp_lyr.GetNextFeature()

    while feat is not None:

        dst_feat.SetFrom(feat)
        dst_feat.SetField("INT64", 1234567890123)
        sql_lyr.CreateFeature(dst_feat)

        feat = shp_lyr.GetNextFeature()

    dst_feat = None

    assert (
        sql_lyr.GetFeatureCount() == shp_lyr.GetFeatureCount()
    ), "not matching feature count"

    got_srs = sql_lyr.GetSpatialRef()
    expected_srs = shp_lyr.GetSpatialRef()
    assert got_srs.GetAuthorityCode(None) == expected_srs.GetAuthorityCode(
        None
    ), "not matching spatial ref"

    yield

    mssql_ds.ExecuteSQL("DELLAYER:tpoly")


###############################################################################
# Verify that stuff we just wrote is still OK.


@pytest.mark.usefixtures("tpoly")
def test_ogr_mssqlspatial_3(mssql_ds, poly_feat):

    mssqlspatial_lyr = mssql_ds.GetLayerByName("tpoly")
    assert mssqlspatial_lyr.GetDataset().GetDescription() == mssql_ds.GetDescription()

    assert mssqlspatial_lyr.GetGeometryColumn() == "ogr_geometry"

    assert mssqlspatial_lyr.GetFeatureCount() == 10

    expect = [168, 169, 166, 158, 165]

    with ogrtest.attribute_filter(mssqlspatial_lyr, "eas_id < 170"):
        ogrtest.check_features_against_list(mssqlspatial_lyr, "eas_id", expect)

        assert mssqlspatial_lyr.GetFeatureCount() == 5

    mssqlspatial_lyr.ResetReading()

    for i in range(len(poly_feat)):
        orig_feat = poly_feat[i]
        read_feat = mssqlspatial_lyr.GetNextFeature()

        ogrtest.check_feature_geometry(
            read_feat, orig_feat.GetGeometryRef(), max_error=0.001
        )

        for fld in range(3):
            if orig_feat.GetField(fld) != read_feat.GetField(fld):
                orig_feat.DumpReadable()
                read_feat.DumpReadable()
                assert False, "Attribute %d does not match" % fld
        assert read_feat.GetField("INT64") == 1234567890123

        read_feat = None
        orig_feat = None


###############################################################################
# Write more features with a bunch of different geometries, and verify the
# geometries are still OK.


@pytest.mark.usefixtures("tpoly")
def test_ogr_mssqlspatial_4(mssql_ds, mssql_has_z_m):

    mssqlspatial_lyr = mssql_ds.GetLayer("tpoly")

    dst_feat = ogr.Feature(feature_def=mssqlspatial_lyr.GetLayerDefn())
    wkt_list = ["10", "2", "1", "4", "5", "6"]

    # If the database engine supports 3D features, include one in the tests
    if mssql_has_z_m:
        wkt_list.append("3d_1")

    use_bcp = "BCP" in gdal.GetDriverByName("MSSQLSpatial").GetMetadataItem(
        gdal.DMD_LONGNAME
    )
    if use_bcp:
        MSSQLSPATIAL_USE_BCP = gdal.GetConfigOption("MSSQLSPATIAL_USE_BCP", "YES")
        if MSSQLSPATIAL_USE_BCP.upper() in ("NO", "OFF", "FALSE"):
            use_bcp = False

    for item in wkt_list:
        wkt_filename = "data/wkb_wkt/" + item + ".wkt"
        wkt = open(wkt_filename).read()
        geom = ogr.CreateGeometryFromWkt(wkt)

        ######################################################################
        # Write geometry as a new feature.

        dst_feat.SetGeometryDirectly(geom)
        dst_feat.SetField("PRFEDEA", item)
        dst_feat.SetFID(-1)
        assert mssqlspatial_lyr.CreateFeature(dst_feat) == ogr.OGRERR_NONE, (
            "CreateFeature failed creating feature "
            + 'from file "'
            + wkt_filename
            + '"'
        )

        ######################################################################
        # Before reading back the record, verify that the newly added feature
        # is returned from the CreateFeature method with a newly assigned FID.

        if not use_bcp:
            assert (
                dst_feat.GetFID() != -1
            ), "Assigned FID was not returned in the new feature"

        ######################################################################
        # Read back the feature and get the geometry.

        mssqlspatial_lyr.SetAttributeFilter("PRFEDEA = '%s'" % item)
        feat_read = mssqlspatial_lyr.GetNextFeature()

        ogrtest.check_feature_geometry(feat_read, geom)

    mssqlspatial_lyr.ResetReading()  # to close implicit transaction


###############################################################################
# Run test_ogrsf


@pytest.mark.usefixtures("tpoly")
def test_ogr_mssqlspatial_test_ogrsf(mssql_ds):

    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path()
        + " -ro '"
        + mssql_ds.GetDescription()
        + "' tpoly"
    )

    assert ret.find("INFO") != -1 and ret.find("ERROR") == -1


###############################################################################
# Verify features can be created in an existing table that includes a geometry
# column but is not registered in the "geometry_columns" table.


def test_ogr_mssqlspatial_create_feature_in_unregistered_table(mssql_ds):

    # Create a feature that specifies a spatial-reference system
    spatial_reference = osr.SpatialReference()
    spatial_reference.ImportFromEPSG(4326)

    feature = ogr.Feature(ogr.FeatureDefn("Unregistered"))
    feature.SetGeometryDirectly(
        ogr.CreateGeometryFromWkt("POINT (10 20)", spatial_reference)
    )

    # Create a table that includes a geometry column but is not registered in
    # the "geometry_columns" table
    mssql_ds.ExecuteSQL(
        "CREATE TABLE Unregistered"
        + "("
        + "ObjectID int IDENTITY(1,1) NOT NULL PRIMARY KEY,"
        + "Shape geometry NOT NULL"
        + ");"
    )

    # Create a new MSSQLSpatial data source, one that will find the table just
    # created and make it available via GetLayerByName()
    with gdaltest.config_option("MSSQLSPATIAL_USE_GEOMETRY_COLUMNS", "NO"):
        test_ds = ogr.Open(mssql_ds.GetDescription(), update=1)

    assert test_ds is not None, "cannot open data source"

    # Get a layer backed by the newly created table and verify that (as it is
    # unregistered) it has no associated spatial-reference system
    unregistered_layer = test_ds.GetLayerByName("Unregistered")
    assert unregistered_layer is not None, "did not get Unregistered layer"

    unregistered_spatial_reference = unregistered_layer.GetSpatialRef()
    assert (
        unregistered_spatial_reference is None
    ), "layer Unregistered unexpectedly has an SRS"

    # Verify creating the feature in the layer succeeds despite the lack of an
    # associated spatial-reference system
    assert (
        unregistered_layer.CreateFeature(feature) == ogr.OGRERR_NONE
    ), "CreateFeature failed"

    # Verify the created feature received the spatial-reference system of the
    # original, as none was associated with the table
    unregistered_layer.ResetReading()
    created_feature = unregistered_layer.GetNextFeature()
    assert created_feature is not None, "did not get feature"

    created_feature_geometry = created_feature.GetGeometryRef()
    created_spatial_reference = created_feature_geometry.GetSpatialReference()
    assert (created_spatial_reference == spatial_reference) or (
        (created_spatial_reference is not None)
        and created_spatial_reference.IsSame(
            spatial_reference, options=["IGNORE_DATA_AXIS_TO_SRS_AXIS_MAPPING=YES"]
        )
    ), "created-feature SRS does not match original"


###############################################################################
# Test all datatype support


def test_ogr_mssqlspatial_datatypes(mssql_ds):

    try:
        lyr = mssql_ds.CreateLayer("test_ogr_mssql_datatypes")

        fd = ogr.FieldDefn("int32", ogr.OFTInteger)
        assert lyr.CreateField(fd) == 0

        fd = ogr.FieldDefn("int16", ogr.OFTInteger)
        fd.SetSubType(ogr.OFSTInt16)
        assert lyr.CreateField(fd) == 0

        fd = ogr.FieldDefn("int64", ogr.OFTInteger64)
        assert lyr.CreateField(fd) == 0

        fd = ogr.FieldDefn("float32", ogr.OFTReal)
        fd.SetSubType(ogr.OFSTFloat32)
        assert lyr.CreateField(fd) == 0

        fd = ogr.FieldDefn("float64", ogr.OFTReal)
        assert lyr.CreateField(fd) == 0

        fd = ogr.FieldDefn("numeric", ogr.OFTReal)
        fd.SetWidth(12)
        fd.SetPrecision(6)
        assert lyr.CreateField(fd) == 0

        fd = ogr.FieldDefn("numeric_width_9_prec_0_from_int", ogr.OFTInteger)
        fd.SetWidth(9)
        fd.SetPrecision(0)
        assert lyr.CreateField(fd) == 0

        fd = ogr.FieldDefn("numeric_width_9_prec_0_from_real", ogr.OFTReal)
        fd.SetWidth(9)
        fd.SetPrecision(0)
        assert lyr.CreateField(fd) == 0

        fd = ogr.FieldDefn("numeric_width_18_prec_0", ogr.OFTInteger64)
        fd.SetWidth(18)
        fd.SetPrecision(0)
        assert lyr.CreateField(fd) == 0

        fd = ogr.FieldDefn("string", ogr.OFTString)
        assert lyr.CreateField(fd) == 0

        fd = ogr.FieldDefn("string_limited", ogr.OFTString)
        fd.SetWidth(5)
        assert lyr.CreateField(fd) == 0

        fd = ogr.FieldDefn("date", ogr.OFTDate)
        assert lyr.CreateField(fd) == 0

        fd = ogr.FieldDefn("time", ogr.OFTTime)
        assert lyr.CreateField(fd) == 0

        fd = ogr.FieldDefn("datetime", ogr.OFTDateTime)
        assert lyr.CreateField(fd) == 0

        fd = ogr.FieldDefn("uid", ogr.OFTString)
        fd.SetSubType(ogr.OFSTUUID)
        assert lyr.CreateField(fd) == 0

        fd = ogr.FieldDefn("binary", ogr.OFTBinary)
        assert lyr.CreateField(fd) == 0

        lyr.StartTransaction()
        f = ogr.Feature(lyr.GetLayerDefn())
        f["int32"] = (1 << 31) - 1
        f["int16"] = (1 << 15) - 1
        f["int64"] = (1 << 63) - 1
        f["float32"] = 1.25
        f["float64"] = 1.25123456789
        f["numeric"] = 123456.789012
        f["numeric_width_9_prec_0_from_int"] = 123456789
        f["numeric_width_9_prec_0_from_real"] = 123456789
        f["numeric_width_18_prec_0"] = 12345678912345678
        f["string"] = "unlimited string"
        f["string_limited"] = "abcd\u00e9"
        f["date"] = "2021/12/11"
        f["time"] = "12:34:56"
        f["datetime"] = "2021/12/11 12:34:56"
        f["uid"] = "6F9619FF-8B86-D011-B42D-00C04FC964FF"
        f["binary"] = b"\x01\x23\x46\x57\x89\xab\xcd\xef"
        lyr.CreateFeature(f)
        lyr.CommitTransaction()

        test_ds = ogr.Open(mssql_ds.GetDescription(), update=0)
        lyr = test_ds.GetLayer("test_ogr_mssql_datatypes")
        lyr_defn = lyr.GetLayerDefn()

        fd = lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex("int32"))
        assert fd.GetType() == ogr.OFTInteger
        assert fd.GetSubType() == ogr.OFSTNone
        assert fd.GetWidth() == 0

        fd = lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex("int16"))
        assert fd.GetType() == ogr.OFTInteger
        assert fd.GetSubType() == ogr.OFSTInt16
        assert fd.GetWidth() == 0

        fd = lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex("int64"))
        assert fd.GetType() == ogr.OFTInteger64
        assert fd.GetSubType() == ogr.OFSTNone
        assert fd.GetWidth() == 0

        fd = lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex("float32"))
        assert fd.GetType() == ogr.OFTReal
        assert fd.GetSubType() == ogr.OFSTFloat32
        assert fd.GetWidth() == 0

        fd = lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex("float64"))
        assert fd.GetType() == ogr.OFTReal
        assert fd.GetSubType() == ogr.OFSTNone
        assert fd.GetWidth() == 0

        fd = lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex("numeric"))
        assert fd.GetType() == ogr.OFTReal
        assert fd.GetSubType() == ogr.OFSTNone
        assert fd.GetWidth() == 12
        assert fd.GetPrecision() == 6

        fd = lyr_defn.GetFieldDefn(
            lyr_defn.GetFieldIndex("numeric_width_9_prec_0_from_int")
        )
        assert fd.GetType() == ogr.OFTInteger
        assert fd.GetSubType() == ogr.OFSTNone
        assert fd.GetWidth() == 9
        assert fd.GetPrecision() == 0

        fd = lyr_defn.GetFieldDefn(
            lyr_defn.GetFieldIndex("numeric_width_9_prec_0_from_real")
        )
        assert fd.GetType() == ogr.OFTInteger
        assert fd.GetSubType() == ogr.OFSTNone
        assert fd.GetWidth() == 9
        assert fd.GetPrecision() == 0

        fd = lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex("numeric_width_18_prec_0"))
        assert fd.GetType() == ogr.OFTInteger64
        assert fd.GetSubType() == ogr.OFSTNone
        assert fd.GetWidth() == 18
        assert fd.GetPrecision() == 0

        fd = lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex("string"))
        assert fd.GetType() == ogr.OFTString
        assert fd.GetWidth() == 0

        fd = lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex("string_limited"))
        assert fd.GetType() == ogr.OFTString
        assert fd.GetWidth() == 5

        fd = lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex("date"))
        assert fd.GetType() == ogr.OFTDate

        fd = lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex("time"))
        # We get a string currently
        # assert fd.GetType() == ogr.OFTTime

        fd = lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex("datetime"))
        assert fd.GetType() == ogr.OFTDateTime

        fd = lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex("uid"))
        assert fd.GetType() == ogr.OFTString
        assert fd.GetSubType() == ogr.OFSTUUID

        fd = lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex("binary"))
        assert fd.GetType() == ogr.OFTBinary

        f = lyr.GetNextFeature()
        assert f["int32"] == (1 << 31) - 1
        assert f["int16"] == (1 << 15) - 1
        assert f["int64"] == (1 << 63) - 1
        assert f["float32"] == 1.25
        assert f["float64"] == 1.25123456789
        assert f["numeric"] == 123456.789012
        assert f["numeric_width_9_prec_0_from_int"] == 123456789
        assert f["numeric_width_9_prec_0_from_real"] == 123456789
        assert f["numeric_width_18_prec_0"] == 12345678912345678
        assert f["string"] == "unlimited string"
        assert f["string_limited"] == "abcd\u00e9"
        assert f["date"] == "2021/12/11"
        assert f["time"] == "12:34:56" or f["time"].startswith("12:34:56.0")
        assert f["datetime"] == "2021/12/11 12:34:56"
        assert f["uid"] == "6F9619FF-8B86-D011-B42D-00C04FC964FF"
        assert f["binary"] == "0123465789ABCDEF"

    finally:
        mssql_ds.ExecuteSQL("DELLAYER:test_ogr_mssql_datatypes")


###############################################################################
#


def test_ogr_mssqlspatial_bulk_insert(mssql_ds):
    """Test issue GH https://github.com/OSGeo/gdal/issues/7787"""

    use_bcp = "BCP" in gdal.GetDriverByName("MSSQLSpatial").GetMetadataItem(
        gdal.DMD_LONGNAME
    )

    if not use_bcp:
        pytest.skip("BCP not available")

    with gdal.config_option("MSSQLSPATIAL_BCP_SIZE", "2"):

        source_ds = gdal.OpenEx("data/poly.shp", gdal.OF_VECTOR)

        assert source_ds

        try:
            with gdal.quiet_errors():
                ds = gdal.VectorTranslate(
                    mssql_ds.GetDescription(),
                    "data/poly.shp",
                    layerCreationOptions=[
                        "OVERWRITE=YES",
                    ],
                )

                lyr = ds.GetLayerByName("poly")
                assert lyr
                assert lyr.GetFeatureCount() == 10
                del lyr
        finally:
            mssql_ds.ExecuteSQL("DROP TABLE poly")


###############################################################################
#


def test_ogr_mssqlspatial_geography_polygon_vertex_order(mssql_ds):
    """Test issue GH https://github.com/OSGeo/gdal/issues/1128"""

    source_ds = gdal.OpenEx("data/shp/testpoly.shp", gdal.OF_VECTOR)

    assert source_ds

    try:
        with gdal.quiet_errors():
            ds = gdal.VectorTranslate(
                mssql_ds.GetDescription(),
                "data/shp/testpoly.shp",
                options="-nln poly_vertex_order -s_srs EPSG:32632 -t_srs EPSG:4326 -lco OVERWRITE=YES -lco GEOM_TYPE=GEOGRAPHY",
            )

            lyr = ds.GetLayerByName("poly_vertex_order")
            assert lyr

            # Simple polygon, no inner rings
            feature = lyr.GetFeature(1)
            geometry = feature.GetGeometryRef()
            boundary = geometry.GetGeometryRef(0)
            # Outer ring
            assert not boundary.IsClockwise()

            # Inner ring
            feature = lyr.GetFeature(13)
            geometry = feature.GetGeometryRef()
            boundary = geometry.GetGeometryRef(0)
            # Outer ring
            assert not boundary.IsClockwise()
            # Inner ring
            inner_ring = geometry.GetGeometryRef(1)
            assert inner_ring.IsClockwise()

            del lyr
    finally:
        mssql_ds.ExecuteSQL("DROP TABLE poly_vertex_order")


###############################################################################
#


@pytest.mark.require_driver("GPKG")
def test_binary_field_bcp(mssql_ds):
    """Test for issue GH #3040 MSSQLSpatial: Cannot write binary field with MSSQLSPATIAL_USE_BCP = TRUE"""

    filename = "/vsimem/test_binary_field.gpkg"
    ds = ogr.GetDriverByName("GPKG").CreateDataSource(filename)
    src_lyr = ds.CreateLayer("test_binary_field", geom_type=ogr.wkbPoint)
    src_lyr.CreateField(ogr.FieldDefn("binfield", ogr.OFTBinary))
    src_lyr.CreateField(ogr.FieldDefn("strfield", ogr.OFTString))
    src_lyr.CreateField(ogr.FieldDefn("intfield", ogr.OFTInteger))

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(1 2)"))
    f["binfield"] = b"\x00\x7f\xff\x00\x7f\xff"
    f["strfield"] = "some text"
    f["intfield"] = 1
    src_lyr.CreateFeature(f)

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(2 3)"))
    f["binfield"] = b""
    f["strfield"] = "some text"
    # leave int undefined
    src_lyr.CreateFeature(f)

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(3 4)"))
    f["binfield"] = b"\xff\x00\x7f\xff\x00\x7f"
    f["strfield"] = "some text"
    f["intfield"] = 3
    src_lyr.CreateFeature(f)

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(4 5)"))
    f["binfield"] = b"\x00\x7f\xff\x00\x7f\xff"
    # leave str undefined
    f["intfield"] = 4
    src_lyr.CreateFeature(f)

    with gdal.config_option("MSSQLSPATIAL_USE_BCP", "TRUE"):

        try:
            res_ds = gdal.VectorTranslate(
                mssql_ds.GetDescription(),
                filename,
                options=[
                    "-nln",
                    "test_binary_field",
                    "-lco",
                    "OVERWRITE=YES",
                    "-a_srs",
                    "EPSG:4326",
                ],
            )

            assert res_ds is not None

            lyr = res_ds.GetLayerByName("test_binary_field")

            assert lyr is not None

            f = lyr.GetFeature(1)
            assert f.GetFieldAsBinary("binfield") == b"\x00\x7f\xff\x00\x7f\xff"
            assert f.GetFieldAsString("strfield") == "some text"
            assert f.GetFieldAsInteger("intfield") == 1

            f = lyr.GetFeature(2)
            assert f.GetFieldAsBinary("binfield") == b""
            assert f.GetFieldAsString("strfield") == "some text"
            # undefined
            assert f.GetFieldAsInteger("intfield") == 0

            f = lyr.GetFeature(3)
            assert f.GetFieldAsBinary("binfield") == b"\xff\x00\x7f\xff\x00\x7f"
            assert f.GetFieldAsString("strfield") == "some text"
            assert f.GetFieldAsInteger("intfield") == 3

            f = lyr.GetFeature(4)
            assert f.GetFieldAsBinary("binfield") == b"\x00\x7f\xff\x00\x7f\xff"
            assert f.GetFieldAsString("strfield") == ""
            assert f.GetFieldAsInteger("intfield") == 4

        finally:
            mssql_ds.ExecuteSQL("DROP TABLE test_binary_field")
            mssql_ds.ExecuteSQL(
                "DELETE from geometry_columns WHERE f_table_name = 'test_binary_field'"
            )


###############################################################################
#


@pytest.mark.usefixtures("tpoly")
def test_geometry_column_identification(mssql_ds):
    """Test for issue GH #1190
    SQL Server data source: geometry column not identified by ogr2ogr"""

    mssql_ds.ExecuteSQL(
        "SELECT *, ogr_geometry.STCentroid() AS shape INTO dbo.poly_shape FROM dbo.tpoly"
    )

    try:
        ds = ogr.Open(mssql_ds.GetDescription())
        with ds.ExecuteSQL("SELECT eas_id, shape FROM poly_shape") as lyr:
            f = lyr.GetFeature(1)
        geom = f.GetGeometryRef()
        assert geom is not None
        assert geom.GetGeometryName() == "POINT"
        assert f.GetFieldCount() == 1

    finally:
        mssql_ds.ExecuteSQL("DROP TABLE poly_shape")
