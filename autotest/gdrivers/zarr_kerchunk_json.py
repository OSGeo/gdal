#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test Kerchunk JSON support in Zarr driver
# Author:   Even Rouault <even.rouault@spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even.rouault@spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import json
import os
import sys
import threading

import pytest
import webserver

from osgeo import gdal

pytestmark = pytest.mark.require_driver("ZARR")


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def load_zarr():
    # Make sure the ZARR driver is fully loaded so that /vsikerchunk_json_ref
    # is available
    with pytest.raises(Exception):
        gdal.Open("ZARR:")


###############################################################################


@pytest.mark.parametrize(
    "filename",
    [
        "data/zarr/kerchunk_json/json_ref_v0_min/ref.json",
        "data/zarr/kerchunk_json/json_ref_v1_min/ref.json",
    ],
)
@pytest.mark.parametrize("VSIKERCHUNK_USE_STREAMING_PARSER", ["AUTO", "YES", "NO"])
def test_vsikerchunk_json_ref_stat(filename, VSIKERCHUNK_USE_STREAMING_PARSER):

    with gdal.config_option(
        "VSIKERCHUNK_USE_STREAMING_PARSER", VSIKERCHUNK_USE_STREAMING_PARSER
    ):

        with pytest.raises(Exception, match="Invalid /vsikerchunk_json_ref/ syntax"):
            gdal.VSIStatL("/vsikerchunk_json_ref/")

        with pytest.raises(Exception, match="Invalid /vsikerchunk_json_ref/ syntax"):
            gdal.VSIStatL("/vsikerchunk_json_ref/{foo")

        with pytest.raises(Exception, match="Load json file /i_do/not/exist failed"):
            gdal.VSIStatL("/vsikerchunk_json_ref/{/i_do/not/exist}")

        stat = gdal.VSIStatL("/vsikerchunk_json_ref/{" + filename + "}")
        assert stat
        assert (stat.mode & 16384) != 0

        stat = gdal.VSIStatL("/vsikerchunk_json_ref/{" + filename + "}/.zgroup")
        assert stat
        assert (stat.mode & 32768) != 0
        assert stat.size == 17

        stat = gdal.VSIStatL("/vsikerchunk_json_ref/{" + filename + "}/x")
        assert stat
        assert (stat.mode & 16384) != 0

        stat = gdal.VSIStatL("/vsikerchunk_json_ref/{" + filename + "}/x/0")
        assert stat
        assert (stat.mode & 32768) != 0
        assert stat.size == 1

        stat = gdal.VSIStatL("/vsikerchunk_json_ref/{" + filename + "}/i_do_not_exist")
        assert stat is None


###############################################################################


@pytest.mark.parametrize(
    "filename",
    [
        "data/zarr/kerchunk_json/json_ref_v0_min/ref.json",
        "data/zarr/kerchunk_json/json_ref_v1_min/ref.json",
    ],
)
def test_vsikerchunk_json_ref_open(filename):

    with pytest.raises(Exception, match="Invalid /vsikerchunk_json_ref/ syntax"):
        gdal.VSIFOpenL("/vsikerchunk_json_ref/", "rb")

    with pytest.raises(Exception, match="Invalid /vsikerchunk_json_ref/ syntax"):
        gdal.VSIFOpenL("/vsikerchunk_json_ref/{foo", "rb")

    with pytest.raises(Exception, match="Load json file /i_do/not/exist failed"):
        gdal.VSIFOpenL("/vsikerchunk_json_ref/{/i_do/not/exist}", "rb")

    assert gdal.VSIFOpenL("/vsikerchunk_json_ref/{" + filename + "}", "rb") is None

    assert (
        gdal.VSIFOpenL(
            "/vsikerchunk_json_ref/{" + filename + "}/i_do_not_exist",
            "rb",
        )
        is None
    )

    with gdal.VSIFile("/vsikerchunk_json_ref/{" + filename + "}/.zgroup", "rb") as f:
        assert f.read() == b'{"zarr_format":2}'

    with gdal.VSIFile("/vsikerchunk_json_ref/{" + filename + "}/x/0", "rb") as f:
        assert f.read() == b"\x01"


###############################################################################


@pytest.mark.parametrize(
    "filename",
    [
        "data/zarr/kerchunk_json/json_ref_v0_min/ref.json",
        "data/zarr/kerchunk_json/json_ref_v1_min/ref.json",
    ],
)
def test_vsikerchunk_json_ref_readdir(filename):

    with pytest.raises(Exception, match="Invalid /vsikerchunk_json_ref/ syntax"):
        gdal.ReadDir("/vsikerchunk_json_ref/")

    with pytest.raises(Exception, match="Invalid /vsikerchunk_json_ref/ syntax"):
        gdal.ReadDir("/vsikerchunk_json_ref/{foo")

    with pytest.raises(Exception, match="Load json file /i_do/not/exist failed"):
        gdal.ReadDir("/vsikerchunk_json_ref/{/i_do/not/exist}")

    filelist = gdal.ReadDir("/vsikerchunk_json_ref/{" + filename + "}")
    assert len(filelist) == 3
    assert set(filelist) == set([".zattrs", ".zgroup", "x"])

    filelist = gdal.ReadDirRecursive("/vsikerchunk_json_ref/{" + filename + "}")
    assert len(filelist) == 6
    assert set(filelist) == set(
        [".zattrs", ".zgroup", "x/", "x/.zarray", "x/.zattrs", "x/0"]
    )

    filelist = gdal.ReadDir("/vsikerchunk_json_ref/{" + filename + "}/x")
    assert len(filelist) == 3
    assert set(filelist) == set([".zattrs", ".zarray", "0"])

    assert (
        gdal.ReadDir("/vsikerchunk_json_ref/{" + filename + "}/i_do_not_exist") is None
    )


###############################################################################


@pytest.mark.parametrize(
    "content",
    [
        """{".zgroup": {"zarr_format":2}}""",
        """{".zgroup": {"zarr_format":2}, "foo":["ok",18446744073709551615,1]}""",
        """{".zgroup": {"zarr_format":2}, "foo":["ok",0,4294967295]}""",
        """{".zgroup": {"zarr_format":2}, "foo":{}}""",
        """{".zgroup": {"zarr_format":2}, "foo":""}""",
        """{".zgroup": {"zarr_format":2}, "foo":"\\u0000"}""",  # not sure if really expected but found in https://archive.podaac.earthdata.nasa.gov/podaac-ops-cumulus-docs/ghrsst/open/docs/MUR-JPL-L4-GLOB-v4.1_combined-ref.json
    ],
)
@pytest.mark.parametrize("VSIKERCHUNK_USE_STREAMING_PARSER", ["AUTO", "YES", "NO"])
def test_zarr_kerchunk_json_ok(tmp_vsimem, content, VSIKERCHUNK_USE_STREAMING_PARSER):

    json_filename = str(tmp_vsimem / "test.json")
    gdal.FileFromMemBuffer(json_filename, content)

    with gdal.config_option(
        "VSIKERCHUNK_USE_STREAMING_PARSER", VSIKERCHUNK_USE_STREAMING_PARSER
    ):
        assert (
            gdal.VSIStatL("/vsikerchunk_json_ref/{" + json_filename + "}") is not None
        )


###############################################################################


@pytest.mark.parametrize(
    "content",
    [
        "",
        """{".zgroup": {"zarr_format":2} """,  # missing closing }
        """{".zgroup": {"zarr_format":2}, "foo":null}""",
        """{".zgroup": {"zarr_format":2}, "foo":true}""",
        """{".zgroup": {"zarr_format":2}, "foo":0}""",
        """{".zgroup": {"zarr_format":2}, "foo":0.5}""",
        """{".zgroup": {"zarr_format":2}, "foo":[]}""",
        """{".zgroup": {"zarr_format":2}, "foo":[0]}""",
        """{".zgroup": {"zarr_format":2}, "foo":[true]}""",
        """{".zgroup": {"zarr_format":2}, "foo":[null]}""",
        """{".zgroup": {"zarr_format":2}, "foo":[{}]}""",
        """{".zgroup": {"zarr_format":2}, "foo":["ok","bad"]}""",
        """{".zgroup": {"zarr_format":2}, "foo":["ok",0,"bad"]}""",
        """{".zgroup": {"zarr_format":2}, "foo":["ok",-1,1]}""",
        """{".zgroup": {"zarr_format":2}, "foo":["ok",18446744073709551616,1]}""",
        """{".zgroup": {"zarr_format":2}, "foo":["ok",0,-1]}""",
        """{".zgroup": {"zarr_format":2}, "foo":["ok",0,4294967296]}""",
        """{".zgroup": {"zarr_format":2}, "foo":["ok",0,1,"unexpected"]}""",
        """{".zgroup": {"zarr_format":2}, "foo":["ok",0.5,1]}""",
        """{".zgroup": {"zarr_format":2}, "foo":["ok",0,0.5]}""",
        """{".zgroup": {"zarr_format":2}, "foo":"base64:_"}""",
        """{"version":"1","refs":{".zgroup": {"zarr_format":2}},"gen":[]}""",
        """{"version":"1","refs":{".zgroup": {"zarr_format":2}},"templates":[]}""",
    ],
)
@pytest.mark.parametrize("VSIKERCHUNK_USE_STREAMING_PARSER", ["AUTO", "YES", "NO"])
def test_zarr_kerchunk_json_fail_exception(
    tmp_vsimem, content, VSIKERCHUNK_USE_STREAMING_PARSER
):

    if "18446744073709551616" in content and VSIKERCHUNK_USE_STREAMING_PARSER == "NO":
        pytest.skip("libjson-c does not detect overflow")

    json_filename = str(tmp_vsimem / "test.json")
    gdal.FileFromMemBuffer(json_filename, content)

    with gdal.config_option(
        "VSIKERCHUNK_USE_STREAMING_PARSER", VSIKERCHUNK_USE_STREAMING_PARSER
    ):
        with pytest.raises(Exception):
            gdal.VSIStatL("/vsikerchunk_json_ref/{" + json_filename + "}/foo")


###############################################################################


@pytest.mark.parametrize(
    "content",
    [
        """{".zgroup": {"zarr_format":2}, "foo":["https://localhost:1/",0,1]}""",
    ],
)
def test_zarr_kerchunk_json_open_fail(tmp_vsimem, content):

    json_filename = str(tmp_vsimem / "test.json")
    gdal.FileFromMemBuffer(json_filename, content)

    with pytest.raises(Exception):
        with gdal.VSIFile("/vsikerchunk_json_ref/{" + json_filename + "}/foo", "rb"):
            pass


###############################################################################


def test_zarr_kerchunk_json_open_very_large(tmp_vsimem):

    json_filename = str(tmp_vsimem / "test.json")
    content = """{".zgroup": {"zarr_format":2}"""
    content += " " * (10 * 1024 * 1024)
    content += "}"
    gdal.FileFromMemBuffer(json_filename, content)
    assert gdal.VSIStatL("/vsikerchunk_json_ref/{" + json_filename + "}") is not None


###############################################################################


@pytest.mark.parametrize(
    "filename",
    [
        "data/zarr/kerchunk_json/json_ref_v0_min/ref.json",
        "/vsikerchunk_json_ref/{data/zarr/kerchunk_json/json_ref_v0_min/ref.json}",
        "ZARR:data/zarr/kerchunk_json/json_ref_v0_min/ref.json",
        "ZARR:/vsikerchunk_json_ref/{data/zarr/kerchunk_json/json_ref_v0_min/ref.json}",
        "ZARR:/vsikerchunk_json_ref/data/zarr/kerchunk_json/json_ref_v0_min/ref.json",
        # v1
        "data/zarr/kerchunk_json/json_ref_v1_min/ref.json",
        "/vsikerchunk_json_ref/{data/zarr/kerchunk_json/json_ref_v1_min/ref.json}",
        "ZARR:data/zarr/kerchunk_json/json_ref_v1_min/ref.json",
        "ZARR:/vsikerchunk_json_ref/{data/zarr/kerchunk_json/json_ref_v1_min/ref.json}",
        "ZARR:/vsikerchunk_json_ref/data/zarr/kerchunk_json/json_ref_v1_min/ref.json",
    ],
)
def test_zarr_kerchunk_json_gdal_open(filename):
    with gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER) as ds:
        rg = ds.GetRootGroup()
        ar = rg.OpenMDArray("x")
        assert ar.Read() == b"\x01"


###############################################################################


@pytest.mark.require_curl()
def test_zarr_kerchunk_json_gdal_open_remote_file_accessing_local_resources():

    webserver_process = None
    webserver_port = 0

    webserver_process, webserver_port = webserver.launch(
        handler=webserver.DispatcherHttpHandler
    )
    if webserver_port == 0:
        pytest.skip()

    gdal.VSICurlClearCache()

    j = {
        ".zgroup": {"zarr_format": 2},
        ".zattrs": {},
        "x/.zarray": {
            "zarr_format": 2,
            "shape": [1],
            "chunks": [1],
            "compressor": None,
            "dtype": ">u1",
            "order": "C",
            "filters": [],
            "fill_value": None,
        },
        "x/.zattrs": {"_ARRAY_DIMENSIONS": ["my_dim_without_indexing_variable"]},
        "x/0": [
            os.path.join(os.getcwd(), "data/zarr/kerchunk_json/json_ref_v0_min/0.bin")
        ],
    }
    content = json.dumps(j).encode("ASCII")

    try:
        handler = webserver.FileHandler({"/ref.json.zarr": content})
        with webserver.install_http_handler(handler), gdal.OpenEx(
            f"/vsicurl/http://localhost:{webserver_port}/ref.json.zarr",
            gdal.OF_MULTIDIM_RASTER,
        ) as ds:
            rg = ds.GetRootGroup()
            ar = rg.OpenMDArray("x")
            with pytest.raises(Exception, match="tries to access local file"):
                ar.Read()

        with webserver.install_http_handler(handler), gdal.OpenEx(
            f"/vsicurl/http://localhost:{webserver_port}/ref.json.zarr",
            gdal.OF_MULTIDIM_RASTER,
        ) as ds:
            rg = ds.GetRootGroup()
            ar = rg.OpenMDArray("x")
            with gdal.config_option(
                "GDAL_ALLOW_REMOTE_RESOURCE_TO_ACCESS_LOCAL_FILE", "NO"
            ):
                with pytest.raises(Exception, match="tries to access local file"):
                    ar.Read()

        with webserver.install_http_handler(handler), gdal.OpenEx(
            f"/vsicurl/http://localhost:{webserver_port}/ref.json.zarr",
            gdal.OF_MULTIDIM_RASTER,
        ) as ds:
            rg = ds.GetRootGroup()
            ar = rg.OpenMDArray("x")
            with gdal.config_option(
                "GDAL_ALLOW_REMOTE_RESOURCE_TO_ACCESS_LOCAL_FILE", "YES"
            ):
                assert ar.Read() == b"\x01"

    finally:
        webserver.server_stop(webserver_process, webserver_port)

        gdal.VSICurlClearCache()


###############################################################################
# Test VSIKERCHUNK_USE_CACHE


@pytest.mark.require_driver("PARQUET")
def test_vsikerchunk_json_ref_cache(tmp_vsimem, tmp_path):

    j = {
        ".zgroup": {"zarr_format": 2},
        ".zattrs": {},
        "x/.zarray": {
            "zarr_format": 2,
            "shape": [1],
            "chunks": [1],
            "compressor": None,
            "dtype": ">u1",
            "order": "C",
            "filters": [],
            "fill_value": None,
        },
        "x/.zattrs": {},
        "x/0": [
            os.path.join(os.getcwd(), "data/zarr/kerchunk_json/json_ref_v0_min/0.bin")
        ],
    }

    json_filename = tmp_vsimem / "test_vsikerchunk_json_ref_cache.json"
    content = json.dumps(j).encode("ASCII")
    gdal.FileFromMemBuffer(json_filename, content)

    cache_dir = str(tmp_path / "cache")
    with gdal.config_options(
        {
            "VSIKERCHUNK_CACHE_DIR": cache_dir,
            "CPL_VSI_MEM_MTIME": "0",
        }
    ):
        ds = gdal.OpenEx(
            json_filename,
            gdal.OF_MULTIDIM_RASTER,
            open_options=["CACHE_KERCHUNK_JSON=YES"],
        )

        assert gdal.VSIStatL(cache_dir) is not None

        s = gdal.VSIStatL(json_filename)
        ds_cache_dir = str(
            tmp_path
            / "cache"
            / f"test_vsikerchunk_json_ref_cache_{s.size}_{s.mtime}.zarr"
        )

        assert gdal.VSIStatL(ds_cache_dir) is not None
        assert gdal.VSIStatL(os.path.join(ds_cache_dir, ".lock")) is None
        assert gdal.VSIStatL(os.path.join(ds_cache_dir, ".zmetadata")) is not None
        assert gdal.VSIStatL(os.path.join(ds_cache_dir, "x", "refs.0.parq")) is not None

        rg = ds.GetRootGroup()
        ar = rg.OpenMDArray("x")
        assert ar.Read() == b"\x01"

        # Trick: recreate a test_vsikerchunk_json_ref_cache.json file in
        # another directory to get a new filename
        # and avoid the in-memory cache to trigger...
        json_filename = tmp_vsimem / "subdir" / "test_vsikerchunk_json_ref_cache.json"
        # Mutate the content to demonstrate we don't read from the .json file
        # but from the cached Kerchunk Parquet dataset
        gdal.FileFromMemBuffer(json_filename, content.replace(b".bin", b".xxx"))

        ds = gdal.OpenEx(
            json_filename,
            gdal.OF_MULTIDIM_RASTER,
            open_options=["CACHE_KERCHUNK_JSON=YES"],
        )
        rg = ds.GetRootGroup()
        ar = rg.OpenMDArray("x")
        assert ar.Read() == b"\x01"


###############################################################################
# Test VSIKERCHUNK_USE_CACHE


@pytest.mark.require_driver("PARQUET")
def test_vsikerchunk_json_ref_cache_concurrent_generation(tmp_vsimem, tmp_path):

    j = {
        ".zgroup": {"zarr_format": 2},
        ".zattrs": {},
        "x/.zarray": {
            "zarr_format": 2,
            "shape": [1],
            "chunks": [1],
            "compressor": None,
            "dtype": ">u1",
            "order": "C",
            "filters": [],
            "fill_value": None,
        },
        "x/.zattrs": {},
        "x/0": [
            os.path.join(os.getcwd(), "data/zarr/kerchunk_json/json_ref_v0_min/0.bin")
        ],
    }

    json_filename = tmp_vsimem / "test_vsikerchunk_json_ref_cache_concurrent.json"
    content = json.dumps(j).encode("ASCII")
    gdal.FileFromMemBuffer(json_filename, content)
    cache_dir = str(tmp_path / "cache")

    error = [False]

    def check():
        try:

            with gdal.config_options(
                {
                    "VSIKERCHUNK_CACHE_DIR": cache_dir,
                    "CPL_VSI_MEM_MTIME": "0",
                    "VSIKERCHUNK_FOR_TESTS": "WAIT_BEFORE_CONVERT_TO_PARQUET_REF",
                }
            ):
                s = gdal.VSIStatL(json_filename)
                ds_cache_dir = str(
                    tmp_path
                    / "cache"
                    / f"test_vsikerchunk_json_ref_cache_concurrent_{s.size}_{s.mtime}.zarr"
                )

                ds = gdal.OpenEx(
                    json_filename,
                    gdal.OF_MULTIDIM_RASTER,
                    open_options=["CACHE_KERCHUNK_JSON=YES"],
                )

                assert gdal.VSIStatL(ds_cache_dir) is not None
                # assert gdal.VSIStatL(os.path.join(ds_cache_dir, ".lock")) is None
                assert (
                    gdal.VSIStatL(os.path.join(ds_cache_dir, ".zmetadata")) is not None
                )
                assert (
                    gdal.VSIStatL(os.path.join(ds_cache_dir, "x", "refs.0.parq"))
                    is not None
                )

                rg = ds.GetRootGroup()
                ar = rg.OpenMDArray("x")
                assert ar.Read() == b"\x01"

        except Exception:
            error[0] = True
            raise

    threads = [threading.Thread(target=check) for i in range(2)]
    for t in threads:
        t.start()
    for t in threads:
        t.join()

    assert not error[0]


###############################################################################
# Test VSIKERCHUNK_USE_CACHE


@pytest.mark.require_driver("PARQUET")
def test_vsikerchunk_json_ref_cache_stalled_lock(tmp_vsimem, tmp_path):

    j = {
        ".zgroup": {"zarr_format": 2},
        ".zattrs": {},
        "x/.zarray": {
            "zarr_format": 2,
            "shape": [1],
            "chunks": [1],
            "compressor": None,
            "dtype": ">u1",
            "order": "C",
            "filters": [],
            "fill_value": None,
        },
        "x/.zattrs": {},
        "x/0": [
            os.path.join(os.getcwd(), "data/zarr/kerchunk_json/json_ref_v0_min/0.bin")
        ],
    }

    json_filename = tmp_vsimem / "test_vsikerchunk_json_ref_cache_stalled_lock.json"
    content = json.dumps(j).encode("ASCII")
    gdal.FileFromMemBuffer(json_filename, content)
    cache_dir = str(tmp_path / "cache")

    with gdal.config_options(
        {
            "VSIKERCHUNK_CACHE_DIR": cache_dir,
            "CPL_VSI_MEM_MTIME": "0",
            "VSIKERCHUNK_FOR_TESTS": "SHORT_DELAY_STALLED_LOCK",
        }
    ):
        gdal.OpenEx(
            json_filename,
            gdal.OF_MULTIDIM_RASTER,
            open_options=["CACHE_KERCHUNK_JSON=YES"],
        )

        assert gdal.VSIStatL(cache_dir) is not None

        s = gdal.VSIStatL(json_filename)
        ds_cache_dir = str(
            tmp_path
            / "cache"
            / f"test_vsikerchunk_json_ref_cache_stalled_lock_{s.size}_{s.mtime}.zarr"
        )

        assert gdal.VSIStatL(ds_cache_dir) is not None
        assert gdal.VSIStatL(os.path.join(ds_cache_dir, ".lock")) is None
        assert gdal.VSIStatL(os.path.join(ds_cache_dir, ".zmetadata")) is not None
        assert gdal.VSIStatL(os.path.join(ds_cache_dir, "x", "refs.0.parq")) is not None

        open(os.path.join(ds_cache_dir, ".lock"), "wb").close()
        assert gdal.VSIStatL(os.path.join(ds_cache_dir, ".lock")) is not None
        gdal.Unlink(os.path.join(ds_cache_dir, ".zmetadata"))
        assert gdal.VSIStatL(os.path.join(ds_cache_dir, ".zmetadata")) is None

        # Trick: recreate a test_vsikerchunk_json_ref_cache_stalled_lock.json file in
        # another directory to get a new filename
        # and avoid the in-memory cache to trigger...
        json_filename = (
            tmp_vsimem / "subdir" / "test_vsikerchunk_json_ref_cache_stalled_lock.json"
        )
        gdal.FileFromMemBuffer(json_filename, content)
        gdal.OpenEx(
            json_filename,
            gdal.OF_MULTIDIM_RASTER,
            open_options=["CACHE_KERCHUNK_JSON=YES"],
        )


###############################################################################
# Test VSIKERCHUNK_USE_CACHE


@pytest.mark.require_driver("PARQUET")
@pytest.mark.skipif(sys.platform == "win32", reason="Incorrect platform")
def test_vsikerchunk_json_ref_cache_cannot_create_cache_dir(tmp_vsimem, tmp_path):

    j = {
        ".zgroup": {"zarr_format": 2},
        ".zattrs": {},
        "x/.zarray": {
            "zarr_format": 2,
            "shape": [1],
            "chunks": [1],
            "compressor": None,
            "dtype": ">u1",
            "order": "C",
            "filters": [],
            "fill_value": None,
        },
        "x/.zattrs": {},
        "x/0": [
            os.path.join(os.getcwd(), "data/zarr/kerchunk_json/json_ref_v0_min/0.bin")
        ],
    }

    json_filename = (
        tmp_vsimem / "test_vsikerchunk_json_ref_cache_cannot_create_cache_dir.json"
    )
    content = json.dumps(j).encode("ASCII")
    gdal.FileFromMemBuffer(json_filename, content)

    with gdal.config_options(
        {
            "VSIKERCHUNK_CACHE_DIR": "/i_do/not/exist",
            "CPL_VSI_MEM_MTIME": "0",
        }
    ):
        with pytest.raises(
            Exception,
            match="Cannot create directory /i_do/not/exist/test_vsikerchunk_json_ref_cache_cannot_create_cache_dir_",
        ):
            gdal.OpenEx(
                json_filename,
                gdal.OF_MULTIDIM_RASTER,
                open_options=["CACHE_KERCHUNK_JSON=YES"],
            )


###############################################################################
# Test VSIKERCHUNK_USE_CACHE


@pytest.mark.require_driver("PARQUET")
@pytest.mark.parametrize(
    "i,content,exception_msg",
    [
        (
            0,
            """{".zgroup": {"zarr_format":2}, "x/.zarray":{"zarr_format": 2, "shape": [1],"chunks": [0], "compressor": null, "dtype": ">u1", "order": "C", "filters":[], "fill_value": null}, "x/0" : ["foo",0,1]}""",
            "Invalid Zarr array definition for x",
        ),
        (
            1,
            """{".zgroup": {"zarr_format":2}, "x/.zarray":{"zarr_format": 2, "shape": [0],"chunks": [1], "compressor": null, "dtype": ">u1", "order": "C", "filters":[], "fill_value": null}, "x/0" : ["foo",0,1]}""",
            "Invalid Zarr array definition for x",
        ),
        (
            2,
            """{".zgroup": {"zarr_format":2}, "x/.zarray":{"zarr_format": 2, "shape": [1,2],"chunks": [1], "compressor": null, "dtype": ">u1", "order": "C", "filters":[], "fill_value": null}, "x/0" : ["foo",0,1]}""",
            "Invalid Zarr array definition for x",
        ),
        (
            3,
            """{".zgroup": {"zarr_format":2}, "x/.zarray":{"zarr_format": 2, "shape": [4000000000,4000000000,4],"chunks": [1,1,1], "compressor": null, "dtype": ">u1", "order": "C", "filters":[], "fill_value": null}, "x/0" : ["foo",0,1]}""",
            "Invalid Zarr array definition for x",
        ),
        (
            4,
            """{".zgroup": {"zarr_format":2}, "invalid/.zarray": "foo"}""",
            "JSON parsing error",
        ),
        (
            5,
            """{".zgroup": {"zarr_format":2}, "x/.zarray":{"zarr_format": 2, "shape": [1],"chunks": [1], "compressor": null, "dtype": ">u1", "order": "C", "filters":[], "fill_value": null}, "x/1" : ["foo",0,1]}""",
            "Invalid key indices: x/1",
        ),
    ],
    ids=[0, 1, 2, 3, 4, 5],
)
def test_vsikerchunk_json_ref_cache_wrong_json(
    tmp_path, tmp_vsimem, i, content, exception_msg
):

    json_filename = tmp_vsimem / (
        f"test_vsikerchunk_json_ref_cache_wrong_json_{i}.json"
    )
    gdal.FileFromMemBuffer(json_filename, content)
    cache_dir = str(tmp_path / "cache")

    with gdal.config_options(
        {
            "VSIKERCHUNK_CACHE_DIR": cache_dir,
            "CPL_VSI_MEM_MTIME": "0",
        }
    ):
        with pytest.raises(Exception, match=exception_msg):
            gdal.OpenEx(
                json_filename,
                gdal.OF_MULTIDIM_RASTER,
                open_options=["CACHE_KERCHUNK_JSON=YES"],
            )


###############################################################################
# Test CreateCopy() with CONVERT_TO_KERCHUNK_PARQUET_REFERENCE=YES


@pytest.mark.require_driver("PARQUET")
def test_vsikerchunk_json_ref_create_copy(tmp_vsimem):

    j = {
        ".zgroup": {"zarr_format": 2},
        ".zattrs": {},
        "x/.zarray": {
            "zarr_format": 2,
            "shape": [1],
            "chunks": [1],
            "compressor": None,
            "dtype": ">u1",
            "order": "C",
            "filters": [],
            "fill_value": None,
        },
        "x/.zattrs": {},
        "x/0": [
            os.path.join(os.getcwd(), "data/zarr/kerchunk_json/json_ref_v0_min/0.bin")
        ],
    }

    json_filename = tmp_vsimem / "test_vsikerchunk_json_ref_create_copy.json"
    content = json.dumps(j).encode("ASCII")
    gdal.FileFromMemBuffer(json_filename, content)

    out_filename = tmp_vsimem / "out.zarr"

    pct_tab = [0]

    def callback(pct, msg, user_data):
        assert pct >= pct_tab[0]
        pct_tab[0] = pct

    gdal.Translate(
        out_filename,
        json_filename,
        format="ZARR",
        creationOptions=["CONVERT_TO_KERCHUNK_PARQUET_REFERENCE=YES"],
        callback=callback,
    )

    assert pct_tab[0] == 1.0

    ds = gdal.OpenEx("ZARR:" + str(out_filename), gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    ar = rg.OpenMDArray("x")
    assert ar.Read() == b"\x01"


###############################################################################


@pytest.mark.parametrize(
    "ref_filename,expected_filename",
    [
        (
            "data/zarr/kerchunk_json/json_ref_v0_min/ref.json",
            "data/zarr/kerchunk_json/json_ref_v0_min/0.bin",
        ),
        (
            "data/zarr/kerchunk_json/json_ref_v1_min/ref.json",
            "data/zarr/kerchunk_json/json_ref_v1_min/0.bin",
        ),
    ],
)
def test_vsikerchunk_json_ref_GetFileMetadata(ref_filename, expected_filename):

    assert gdal.GetFileMetadata("/vsikerchunk_json_ref/", None) == {}
    with pytest.raises(Exception, match="Invalid /vsikerchunk_json_ref/ syntax"):
        assert gdal.GetFileMetadata("/vsikerchunk_json_ref/", "CHUNK_INFO")
    with pytest.raises(Exception, match="Invalid /vsikerchunk_json_ref/ syntax"):
        assert gdal.GetFileMetadata("/vsikerchunk_json_ref/{foo", "CHUNK_INFO")
    assert (
        gdal.GetFileMetadata("/vsikerchunk_json_ref{/i_do/not/exist}", "CHUNK_INFO")
        == {}
    )
    assert (
        gdal.GetFileMetadata(
            "/vsikerchunk_json_ref/{" + ref_filename + "}/non_existing", "CHUNK_INFO"
        )
        == {}
    )
    got_md = gdal.GetFileMetadata(
        "/vsikerchunk_json_ref/{" + ref_filename + "}/x/0", "CHUNK_INFO"
    )
    got_md["FILENAME"] = got_md["FILENAME"].replace("\\", "/")
    assert got_md == {
        "FILENAME": expected_filename,
        "OFFSET": "0",
        "SIZE": "1",
    }
