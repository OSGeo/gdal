#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test Kerchunk Parquet support in Zarr driver
# Author:   Even Rouault <even.rouault@spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even.rouault@spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os
import shutil
import sys

import pytest

from osgeo import gdal

pytestmark = pytest.mark.require_driver("ZARR")


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def load_zarr():
    # Make sure the ZARR driver is fully loaded so that /vsikerchunk_parquet_ref
    # is available
    with pytest.raises(Exception):
        gdal.Open("ZARR:")


###############################################################################


@pytest.mark.parametrize(
    "filename",
    [
        "data/zarr/kerchunk_parquet/parquet_ref_min",
        "data/zarr/kerchunk_parquet/parquet_ref_0_dim",
        "data/zarr/kerchunk_parquet/parquet_ref_0_dim_missing_size",
        "data/zarr/kerchunk_parquet/parquet_ref_0_dim_path_dot_dot",
    ],
)
def test_vsikerchunk_parquet_ref_stat(filename):

    with pytest.raises(Exception, match="Invalid /vsikerchunk_parquet_ref/ syntax"):
        gdal.VSIStatL("/vsikerchunk_parquet_ref/")

    with pytest.raises(Exception, match="Invalid /vsikerchunk_parquet_ref/ syntax"):
        gdal.VSIStatL("/vsikerchunk_parquet_ref/not_starting_with_brace")

    with pytest.raises(Exception, match="Invalid /vsikerchunk_parquet_ref/ syntax"):
        gdal.VSIStatL("/vsikerchunk_parquet_ref/{foo")

    with pytest.raises(Exception, match="Load json file"):
        gdal.VSIStatL("/vsikerchunk_parquet_ref/{/i_do/not/exist}")

    stat = gdal.VSIStatL("/vsikerchunk_parquet_ref/{" + filename + "}")
    assert stat
    assert (stat.mode & 16384) != 0

    stat = gdal.VSIStatL("/vsikerchunk_parquet_ref/{" + filename + "}/.zgroup")
    assert stat
    assert (stat.mode & 32768) != 0
    assert stat.size == 17

    stat = gdal.VSIStatL("/vsikerchunk_parquet_ref/{" + filename + "}/x")
    assert stat
    assert (stat.mode & 16384) != 0

    if gdal.GetDriverByName("PARQUET") is not None:
        stat = gdal.VSIStatL("/vsikerchunk_parquet_ref/{" + filename + "}/x/0")
        assert stat
        assert (stat.mode & 32768) != 0
        assert stat.size == 1

    stat = gdal.VSIStatL("/vsikerchunk_parquet_ref/{" + filename + "}/i_do_not_exist")
    assert stat is None


###############################################################################


@pytest.mark.parametrize(
    "filename",
    [
        "data/zarr/kerchunk_parquet/parquet_ref_min",
        "data/zarr/kerchunk_parquet/parquet_ref_0_dim",
        "data/zarr/kerchunk_parquet/parquet_ref_0_dim_missing_size",
    ],
)
def test_vsikerchunk_parquet_ref_open(filename):

    with pytest.raises(Exception, match="Invalid /vsikerchunk_parquet_ref/ syntax"):
        gdal.VSIFOpenL("/vsikerchunk_parquet_ref/", "rb")

    with pytest.raises(Exception, match="Invalid /vsikerchunk_parquet_ref/ syntax"):
        gdal.VSIFOpenL("/vsikerchunk_parquet_ref/{foo", "rb")

    with pytest.raises(Exception, match="Load json file"):
        gdal.VSIFOpenL("/vsikerchunk_parquet_ref/{/i_do/not/exist}", "rb")

    assert gdal.VSIFOpenL("/vsikerchunk_parquet_ref/{" + filename + "}", "rb") is None

    assert (
        gdal.VSIFOpenL(
            "/vsikerchunk_parquet_ref/{" + filename + "}/i_do_not_exist",
            "rb",
        )
        is None
    )

    with gdal.VSIFile("/vsikerchunk_parquet_ref/{" + filename + "}/.zgroup", "rb") as f:
        assert f.read() == b'{"zarr_format":2}'

    if gdal.GetDriverByName("PARQUET") is not None:
        with gdal.VSIFile("/vsikerchunk_parquet_ref/{" + filename + "}/x/0", "rb") as f:
            assert f.read() == b"\x01"


###############################################################################


@pytest.mark.parametrize(
    "filename",
    [
        "data/zarr/kerchunk_parquet/parquet_ref_min",
        "data/zarr/kerchunk_parquet/parquet_ref_0_dim",
        "data/zarr/kerchunk_parquet/parquet_ref_0_dim_missing_size",
    ],
)
def test_vsikerchunk_parquet_ref_readdir(filename):

    with pytest.raises(Exception, match="Invalid /vsikerchunk_parquet_ref/ syntax"):
        gdal.ReadDir("/vsikerchunk_parquet_ref/")

    with pytest.raises(Exception, match="Invalid /vsikerchunk_parquet_ref/ syntax"):
        gdal.ReadDir("/vsikerchunk_parquet_ref/{foo")

    with pytest.raises(Exception, match="Load json file"):
        gdal.ReadDir("/vsikerchunk_parquet_ref/{/i_do/not/exist}")

    filelist = gdal.ReadDir("/vsikerchunk_parquet_ref/{" + filename + "}")
    assert len(filelist) == 3
    assert set(filelist) == set([".zattrs", ".zgroup", "x"])

    if gdal.GetDriverByName("PARQUET") is not None:
        filelist = gdal.ReadDirRecursive("/vsikerchunk_parquet_ref/{" + filename + "}")
        assert len(filelist) == 6
        assert set(filelist) == set(
            [".zattrs", ".zgroup", "x/", "x/.zarray", "x/.zattrs", "x/0"]
        )

        filelist = gdal.ReadDir("/vsikerchunk_parquet_ref/{" + filename + "}/x")
        assert len(filelist) == 3
        assert set(filelist) == set([".zattrs", ".zarray", "0"])

    assert (
        gdal.ReadDir("/vsikerchunk_parquet_ref/{" + filename + "}/i_do_not_exist")
        is None
    )


###############################################################################


@pytest.mark.require_driver("PARQUET")
def test_vsikerchunk_parquet_ref_dim2():

    filelist = gdal.ReadDir(
        "/vsikerchunk_parquet_ref/{data/zarr/kerchunk_parquet/parquet_ref_2_dim}/ar"
    )
    assert len(filelist) == 6
    assert set(filelist) == set([".zarray", ".zattrs", "0.0", "0.1", "1.0", "1.1"])

    filelist = gdal.ReadDir(
        "/vsikerchunk_parquet_ref/{data/zarr/kerchunk_parquet/parquet_ref_2_dim}/ar", 4
    )
    assert len(filelist) == 4
    assert set(filelist) == set([".zarray", ".zattrs", "0.0", "0.1"])

    for x in range(2):
        for y in range(2):
            stat = gdal.VSIStatL(
                "/vsikerchunk_parquet_ref/{data/zarr/kerchunk_parquet/parquet_ref_2_dim}/ar/%d.%d"
                % (x, y)
            )
            assert stat
            assert (stat.mode & 32768) != 0
            assert stat.size == 6

    stat = gdal.VSIStatL(
        "/vsikerchunk_parquet_ref/{data/zarr/kerchunk_parquet/parquet_ref_2_dim}/ar/0.2"
    )
    assert stat is None

    stat = gdal.VSIStatL(
        "/vsikerchunk_parquet_ref/{data/zarr/kerchunk_parquet/parquet_ref_2_dim}/ar/2.0"
    )
    assert stat is None

    stat = gdal.VSIStatL(
        "/vsikerchunk_parquet_ref/{data/zarr/kerchunk_parquet/parquet_ref_2_dim}/ar/0.-1"
    )
    assert stat is None

    stat = gdal.VSIStatL(
        "/vsikerchunk_parquet_ref/{data/zarr/kerchunk_parquet/parquet_ref_2_dim}/ar/0.a"
    )
    assert stat is None


###############################################################################


@pytest.mark.parametrize(
    "content,exception_msg",
    [
        ("", "JSON parsing error"),
        ("""{"record_size":1}""", "key 'metadata' missing or not of type dict"),
        (
            """{"metadata": null, "record_size":1}""",
            "key 'metadata' missing or not of type dict",
        ),
        (
            """{"metadata": {".zgroup": {"zarr_format":2}, "foo":"bar"}, "record_size":1}""",
            "invalid value type for key 'foo'",
        ),
        (
            """{"metadata": {".zgroup": {"zarr_format":2}, "x/.zarray": {"zarr_format": 2, "shape": [1],"chunks": [1], "compressor": null, "dtype": ">u1", "order": "C", "filters":[], "fill_value": null}}}""",
            "key 'record_size' missing or not of type integer",
        ),
        (
            """{"metadata": {".zgroup": {"zarr_format":2}, "x/.zarray":{"zarr_format": 2, "shape": [1],"chunks": [1], "compressor": null, "dtype": ">u1", "order": "C", "filters":[], "fill_value": null}},"record_size": null}""",
            "key 'record_size' missing or not of type integer",
        ),
        (
            """{"metadata": {".zgroup": {"zarr_format":2}, "x/.zarray": {"zarr_format": 2, "shape": [1],"chunks": [1], "compressor": null, "dtype": ">u1", "order": "C", "filters":[], "fill_value": null}},"record_size": 0}""",
            "Invalid 'record_size'",
        ),
        (
            """{"metadata": {".zgroup": {"zarr_format":2}, "x/.zarray":{"zarr_format": 2, "chunks": [1], "compressor": null, "dtype": ">u1", "order": "C", "filters":[], "fill_value": null}},"record_size": 1}""",
            "missing 'shape' entry for key 'x/.zarray'",
        ),
        (
            """{"metadata": {".zgroup": {"zarr_format":2}, "x/.zarray":{"zarr_format": 2, "shape": [1], "compressor": null, "dtype": ">u1", "order": "C", "filters":[], "fill_value": null}},"record_size": 1}""",
            "missing 'chunks' entry for key 'x/.zarray'",
        ),
        (
            """{"metadata": {".zgroup": {"zarr_format":2}, "x/.zarray":{"zarr_format": 2, "shape": [0],"chunks": [1], "compressor": null, "dtype": ">u1", "order": "C", "filters":[], "fill_value": null}},"record_size": 1}""",
            r"shape\[0\]=0 in array definition for key 'x/.zarray'",
        ),
        (
            """{"metadata": {".zgroup": {"zarr_format":2}, "x/.zarray":{"zarr_format": 2, "shape": [1],"chunks": [0], "compressor": null, "dtype": ">u1", "order": "C", "filters":[], "fill_value": null}},"record_size": 1}""",
            r"chunks\[0\]=0 in array definition for key 'x/.zarray'",
        ),
        (
            """{"metadata": {".zgroup": {"zarr_format":2}, "x/.zarray": {"zarr_format": 2, "shape": [1],"chunks": [2,3], "compressor": null, "dtype": ">u1", "order": "C", "filters":[], "fill_value": null}},"record_size": 1}""",
            "'shape' and 'chunks' entries have not the same number of values for key 'x/.zarray'",
        ),
        (
            """{"metadata": {".zgroup": {"zarr_format":2}, "x/.zarray":{"zarr_format": 2, "shape": [1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,33],"chunks": [1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,33], "compressor": null, "dtype": ">u1", "order": "C", "filters":[], "fill_value": null}},"record_size": 1}""",
            "'shape' has too many dimensions for key 'x/.zarray'",
        ),
        (
            """{"metadata": {".zgroup": {"zarr_format":2}, "x/.zarray": {"zarr_format": 2, "shape": [4000000000,4000000000,4],"chunks": [1,1,1], "compressor": null, "dtype": ">u1", "order": "C", "filters":[], "fill_value": null}},"record_size": 1}""",
            "> UINT64_MAX",
        ),
    ],
)
def test_zarr_kerchunk_parquet_fail_exception(tmp_vsimem, content, exception_msg):

    json_filename = str(tmp_vsimem / ".zmetadata")
    gdal.FileFromMemBuffer(json_filename, content)

    with pytest.raises(Exception, match=exception_msg):
        gdal.VSIStatL("/vsikerchunk_parquet_ref/{" + str(tmp_vsimem) + "}")


###############################################################################


@pytest.mark.parametrize(
    "filename",
    [
        "ZARR:data/zarr/kerchunk_parquet/parquet_ref_min",
        "ZARR:/vsikerchunk_parquet_ref/{data/zarr/kerchunk_parquet/parquet_ref_min}",
        "ZARR:/vsikerchunk_parquet_ref/{data/zarr/kerchunk_parquet/parquet_ref_0_dim}",
        "ZARR:/vsikerchunk_parquet_ref/{data/zarr/kerchunk_parquet/parquet_ref_0_dim_inline_content}",
    ],
)
def test_zarr_kerchunk_parquet_gdal_open(filename):
    with gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER) as ds:
        rg = ds.GetRootGroup()
        ar = rg.OpenMDArray("x")
        if gdal.GetDriverByName("PARQUET") is not None:
            assert ar.Read() == b"\x01"
        elif gdal.GetDriverByName("ADBC") is None:
            with pytest.raises(Exception):
                ar.Read()


###############################################################################


def test_zarr_kerchunk_parquet_gdal_open_dim2():
    with gdal.OpenEx(
        "ZARR:/vsikerchunk_parquet_ref/{data/zarr/kerchunk_parquet/parquet_ref_2_dim}",
        gdal.OF_MULTIDIM_RASTER,
    ) as ds:
        rg = ds.GetRootGroup()
        ar = rg.OpenMDArray("ar")
        if gdal.GetDriverByName("PARQUET") is not None:
            assert ar.Read() == b"\x00\x01\x02\x00\x03\x04\x05\x03\x00\x01\x02\x00"
        elif gdal.GetDriverByName("ADBC") is None:
            with pytest.raises(Exception):
                ar.Read()


###############################################################################


@pytest.mark.skipif(sys.platform == "win32", reason="Fails for some reason on Windows")
def test_zarr_kerchunk_parquet_gdal_open_missing_bin_file(tmp_path):

    shutil.copytree(
        "data/zarr/kerchunk_parquet/parquet_ref_2_dim", tmp_path / "parquet_ref_2_dim"
    )
    os.unlink(tmp_path / "parquet_ref_2_dim/ar/4.bin")

    with gdal.OpenEx(
        f"ZARR:/vsikerchunk_parquet_ref/{{{tmp_path}/parquet_ref_2_dim}}",
        gdal.OF_MULTIDIM_RASTER,
    ) as ds:
        rg = ds.GetRootGroup()
        ar = rg.OpenMDArray("ar")
        with pytest.raises(Exception):
            ar.Read()


###############################################################################


@pytest.mark.require_driver("PARQUET")
def test_zarr_kerchunk_parquet_invalid_parquet_struct():
    with gdal.OpenEx(
        "ZARR:data/zarr/kerchunk_parquet/parquet_ref_invalid_parquet_struct",
        gdal.OF_MULTIDIM_RASTER,
    ) as ds:
        rg = ds.GetRootGroup()
        ar = rg.OpenMDArray("x")
        with pytest.raises(
            Exception,
            match="has an unexpected field structure",
        ):
            ar.Read()


###############################################################################


@pytest.mark.parametrize(
    "ref_filename,expected_md",
    [
        (
            "data/zarr/kerchunk_parquet/parquet_ref_0_dim",
            {
                "FILENAME": "data/zarr/kerchunk_parquet/parquet_ref_0_dim/x/0.bin",
                "OFFSET": "0",
                "SIZE": "1",
            },
        ),
        (
            "data/zarr/kerchunk_parquet/parquet_ref_0_dim_missing_size",
            {
                "FILENAME": "data/zarr/kerchunk_parquet/parquet_ref_0_dim_missing_size/x/0.bin",
                "OFFSET": "0",
                "SIZE": "1",
            },
        ),
        (
            "data/zarr/kerchunk_parquet/parquet_ref_0_dim_inline_content",
            {"BASE64": "AQ==", "SIZE": "1"},
        ),
    ],
)
@pytest.mark.require_driver("PARQUET")
def test_vsikerchunk_parquet_ref_GetFileMetadata(ref_filename, expected_md):

    assert gdal.GetFileMetadata("/vsikerchunk_parquet_ref/", None) == {}
    with pytest.raises(Exception, match="Invalid /vsikerchunk_parquet_ref/ syntax"):
        assert gdal.GetFileMetadata("/vsikerchunk_parquet_ref/", "CHUNK_INFO")
    with pytest.raises(Exception, match="Invalid /vsikerchunk_parquet_ref/ syntax"):
        assert gdal.GetFileMetadata("/vsikerchunk_parquet_ref/{foo", "CHUNK_INFO")
    assert (
        gdal.GetFileMetadata("/vsikerchunk_parquet_ref{/i_do/not/exist}", "CHUNK_INFO")
        == {}
    )
    assert (
        gdal.GetFileMetadata(
            "/vsikerchunk_parquet_ref/{" + ref_filename + "}/non_existing", "CHUNK_INFO"
        )
        == {}
    )
    got_md = gdal.GetFileMetadata(
        "/vsikerchunk_parquet_ref/{" + ref_filename + "}/x/0", "CHUNK_INFO"
    )
    if "FILENAME" in got_md:
        got_md["FILENAME"] = got_md["FILENAME"].replace("\\", "/")
    assert got_md == expected_md
