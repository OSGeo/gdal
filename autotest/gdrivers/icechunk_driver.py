#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test Icechunk driver
# Author:   Even Rouault <even.rouault@spatialys.com>
#
###############################################################################
# Copyright (c) 2026, Even Rouault <even.rouault@spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import array
import os
import shutil
import struct
import threading
from pathlib import Path

import pytest

from osgeo import gdal

pytestmark = [
    pytest.mark.require_driver("Icechunk"),
    pytest.mark.require_driver("ZARR"),
]

FORCE_REGENERATE = False


def _import_zarr():
    import zarr

    return zarr


def _import_icechunk():
    import icechunk

    return icechunk


def test_icechunk_empty_repo():

    dirname = "data/icechunk/empty_repo"
    if FORCE_REGENERATE or not os.path.exists(dirname):

        icechunk = _import_icechunk()

        if os.path.exists(dirname):
            shutil.rmtree(dirname)
        storage = icechunk.local_filesystem_storage(dirname)
        icechunk.Repository.create(storage)

    assert gdal.ReadDirRecursive("/vsiicechunk/{data/icechunk/empty_repo}") is None
    assert gdal.VSIStatL("/vsiicechunk/{data/icechunk/empty_repo}").size == 0

    ds = gdal.OpenEx("data/icechunk/empty_repo", gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    assert rg.GetGroupNames() == []
    assert rg.GetMDArrayNames() == []

    with gdal.alg.driver.icechunk.list_branches(
        input="data/icechunk/scalar_array"
    ) as alg:
        assert alg.Output() == [
            {
                "commit_message": "first commit",
                "name": "main",
                "timestamp": "2026-06-02T22:43:52.596Z",
            }
        ]

    with gdal.alg.driver.icechunk.list_tags(input="data/icechunk/scalar_array") as alg:
        assert alg.Output() == []

    assert gdal.OpenEx("data/icechunk/empty_repo/repo", gdal.OF_MULTIDIM_RASTER)

    assert gdal.OpenEx("ICECHUNK:data/icechunk/empty_repo", gdal.OF_MULTIDIM_RASTER)

    assert gdal.OpenEx(
        "ICECHUNK:data/icechunk/empty_repo/repo", gdal.OF_MULTIDIM_RASTER
    )


def test_icechunk_scalar_array_v1():

    dirname = "data/icechunk/scalar_array_v1"
    if FORCE_REGENERATE or not os.path.exists(dirname):

        icechunk = _import_icechunk()
        zarr = _import_zarr()

        # Only with icechunk installed with "pip install icechunk<2"
        assert str(icechunk.spec_version()) == "1"

        if os.path.exists(dirname):
            shutil.rmtree(dirname)
        storage = icechunk.local_filesystem_storage(dirname)
        repo = icechunk.Repository.create(storage)
        session = repo.writable_session("main")
        store = session.store  # A zarr store
        group = zarr.group(store)
        my_array = group.create("my_array", shape=(), dtype="int32", compressors=None)
        my_array[...] = 1
        session.commit("first commit")

    ds = gdal.OpenEx("data/icechunk/scalar_array_v1", gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    ar = rg.OpenMDArray("my_array")
    assert ar.Read() == array.array("i", [1])

    with gdal.alg.driver.icechunk.list_branches(
        input="data/icechunk/scalar_array_v1"
    ) as alg:
        assert alg.Output() == [
            {
                "commit_message": "first commit",
                "name": "main",
                "timestamp": "2026-06-03T00:22:04.383Z",
            }
        ]

    with gdal.alg.driver.icechunk.list_tags(
        input="data/icechunk/scalar_array_v1"
    ) as alg:
        assert alg.Output() == [
            {
                "commit_message": "first commit",
                "name": "my_tag",
                "timestamp": "2026-06-03T00:22:04.383Z",
            }
        ]

    assert gdal.OpenEx(
        "ICECHUNK:data/icechunk/scalar_array_v1", gdal.OF_MULTIDIM_RASTER
    )


def test_icechunk_regular_array_v1():

    dirname = "data/icechunk/regular_array_v1"
    if FORCE_REGENERATE or not os.path.exists(dirname):

        icechunk = _import_icechunk()
        zarr = _import_zarr()

        # Only with icechunk installed with "pip install icechunk<2"
        assert str(icechunk.spec_version()) == "1"

        if os.path.exists(dirname):
            shutil.rmtree(dirname)
        storage = icechunk.local_filesystem_storage(dirname)
        repo = icechunk.Repository.create(storage)
        session = repo.writable_session("main")
        store = session.store  # A zarr store
        group = zarr.group(store)
        my_array = group.create(
            "my_array", shape=(2, 3), dtype="int32", compressors=None
        )
        my_array[...] = 1
        session.commit("first commit")

    ds = gdal.OpenEx("data/icechunk/regular_array_v1", gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    ar = rg.OpenMDArray("my_array")
    assert ar.Read() == array.array("i", [1] * 6)


def test_icechunk_scalar_array():

    dirname = "data/icechunk/scalar_array"
    if FORCE_REGENERATE or not os.path.exists(dirname):

        icechunk = _import_icechunk()
        zarr = _import_zarr()

        if os.path.exists(dirname):
            shutil.rmtree(dirname)
        storage = icechunk.local_filesystem_storage(dirname)
        repo = icechunk.Repository.create(storage)
        session = repo.writable_session("main")
        store = session.store  # A zarr store
        group = zarr.group(store)
        my_array = group.create("my_array", shape=(), dtype="int32", compressors=None)
        my_array[...] = 1
        session.commit("first commit")

    assert gdal.ReadDirRecursive("/vsiicechunk/{data/icechunk/scalar_array}") == [
        "zarr.json",
        "my_array/",
        "my_array/zarr.json",
        "my_array/c",
    ]
    assert (
        gdal.VSIStatL("/vsiicechunk/{data/icechunk/scalar_array}/zarr.json").size == 66
    )
    assert gdal.VSIStatL("/vsiicechunk/{data/icechunk/scalar_array}/my_array").size == 0
    assert (
        gdal.VSIStatL(
            "/vsiicechunk/{data/icechunk/scalar_array}/my_array/zarr.json"
        ).size
        == 473
    )
    assert (
        gdal.VSIStatL("/vsiicechunk/{data/icechunk/scalar_array}/my_array/c").size == 4
    )

    assert gdal.VSIStatL("/vsiicechunk/{data/icechunk/scalar_array}/invalid") is None
    assert (
        gdal.VSIStatL("/vsiicechunk/{data/icechunk/scalar_array}/my_array/invalid")
        is None
    )
    assert (
        gdal.VSIStatL("/vsiicechunk/{data/icechunk/scalar_array}/my_array/c/0") is None
    )

    assert gdal.ReadDir("/vsiicechunk/{data/icechunk/scalar_array}/invalid") is None
    assert gdal.ReadDir("/vsiicechunk/{data/icechunk/scalar_array}/my_array/c") is None

    assert (
        gdal.VSIFOpenL("/vsiicechunk/{data/icechunk/scalar_array}/invalid", "rb")
        is None
    )
    assert (
        gdal.VSIFOpenL("/vsiicechunk/{data/icechunk/scalar_array}/my_array", "rb")
        is None
    )
    assert (
        gdal.VSIFOpenL("/vsiicechunk/{data/icechunk/scalar_array}/my_array/c/0", "rb")
        is None
    )

    ds = gdal.OpenEx(dirname, gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    ar = rg.OpenMDArray("my_array")
    assert ar.Read() == array.array("i", [1])


def test_icechunk_ClearMemoryCaches(tmp_vsimem):

    dirname = "data/icechunk/scalar_array"
    gdal.alg.vsi.copy(source=dirname, destination=tmp_vsimem, recursive=True)

    ds = gdal.OpenEx(f"{tmp_vsimem}/scalar_array", gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    ar = rg.OpenMDArray("my_array")
    assert ar.Read() == array.array("i", [1])

    # Use Icechunk driver caches
    assert ar.Read() == array.array("i", [1])

    gdal.ClearMemoryCaches()

    # Re-load from files
    assert ar.Read() == array.array("i", [1])

    gdal.alg.vsi.delete(filename=f"{tmp_vsimem}/scalar_array", recursive=True)

    # Still using caches
    assert ar.Read() == array.array("i", [1])

    gdal.ClearMemoryCaches()

    # Now error!
    with pytest.raises(
        Exception,
        match="Cannot open /vsimem/test_icechunk_ClearMemoryCaches/scalar_array",
    ):
        ar.Read()


def test_icechunk_open_branch():

    with gdal.OpenEx(
        "ICECHUNK:data/icechunk/scalar_array?branch=main", gdal.OF_MULTIDIM_RASTER
    ) as ds:
        assert ds.GetRootGroup().GetMDArrayNames() == ["my_array"]

    with pytest.raises(Exception, match="Invalid branch name"):
        gdal.OpenEx(
            "ICECHUNK:data/icechunk/scalar_array?branch=non_existing",
            gdal.OF_MULTIDIM_RASTER,
        )


def test_icechunk_open_tag():

    with gdal.OpenEx(
        "ICECHUNK:data/icechunk/scalar_array_v1?tag=my_tag", gdal.OF_MULTIDIM_RASTER
    ) as ds:
        assert ds.GetRootGroup().GetMDArrayNames() == ["my_array"]

    with pytest.raises(Exception, match="Invalid tag name"):
        gdal.OpenEx(
            "ICECHUNK:data/icechunk/scalar_array_v1?tag=non_existing",
            gdal.OF_MULTIDIM_RASTER,
        )


def test_icechunk_open_unknown_parameter():

    with pytest.raises(Exception, match="Invalid Icechunk connection string"):
        gdal.OpenEx(
            "ICECHUNK:data/icechunk/scalar_array_v1?foo=bar", gdal.OF_MULTIDIM_RASTER
        )


def test_icechunk_algs_error():

    with pytest.raises(Exception, match="Cannot open data/zarr/v3/test.zr3"):
        gdal.alg.driver.icechunk.list_branches(input="data/zarr/v3/test.zr3")

    with pytest.raises(Exception, match="Cannot open data/zarr/v3/test.zr3"):
        gdal.alg.driver.icechunk.list_tags(input="data/zarr/v3/test.zr3")


def test_icechunk_multi_chunks():

    dirname = "data/icechunk/multi_chunks"
    if FORCE_REGENERATE or not os.path.exists(dirname):

        icechunk = _import_icechunk()
        zarr = _import_zarr()

        if os.path.exists(dirname):
            shutil.rmtree(dirname)

        split_config = icechunk.config.ManifestSplittingConfig.from_dict(
            {
                icechunk.config.ManifestSplitCondition.AnyArray(): {
                    icechunk.config.ManifestSplitDimCondition.Any(): 2
                }
            }
        )
        config = icechunk.config.RepositoryConfig(
            manifest=icechunk.config.ManifestConfig(splitting=split_config)
        )

        storage = icechunk.local_filesystem_storage(dirname)
        repo = icechunk.Repository.create(storage, config=config)

        with repo.transaction("main", message="first commit") as store:
            group = zarr.group(store)
            my_array = group.create(
                "my_array",
                shape=(11, 7),
                chunks=(2, 3),
                dtype="uint8",
                compressors=None,
            )
            my_array[:] = 1

        repo = icechunk.Repository.open(storage, config=config)

        with repo.transaction("main", message="overwrite [1,2] and [9,6]") as store:
            group = zarr.open_group(store)
            my_array = group["my_array"]
            my_array[1, 2] = 2
            my_array[9, 6] = 3

    assert gdal.ReadDirRecursive("/vsiicechunk/{data/icechunk/multi_chunks}") == [
        "zarr.json",
        "my_array/",
        "my_array/zarr.json",
        "my_array/c/",
        "my_array/c/0/",
        "my_array/c/0/0",
        "my_array/c/0/1",
        "my_array/c/0/2",
        "my_array/c/1/",
        "my_array/c/1/0",
        "my_array/c/1/1",
        "my_array/c/1/2",
        "my_array/c/2/",
        "my_array/c/2/0",
        "my_array/c/2/1",
        "my_array/c/2/2",
        "my_array/c/3/",
        "my_array/c/3/0",
        "my_array/c/3/1",
        "my_array/c/3/2",
        "my_array/c/4/",
        "my_array/c/4/0",
        "my_array/c/4/1",
        "my_array/c/4/2",
        "my_array/c/5/",
        "my_array/c/5/0",
        "my_array/c/5/1",
        "my_array/c/5/2",
    ]
    assert (
        gdal.VSIStatL("/vsiicechunk/{data/icechunk/multi_chunks}/my_array/c").size == 0
    )
    assert (
        gdal.VSIStatL("/vsiicechunk/{data/icechunk/multi_chunks}/my_array/c/0").size
        == 0
    )
    assert (
        gdal.VSIStatL("/vsiicechunk/{data/icechunk/multi_chunks}/my_array/c/0/0").size
        == 6
    )
    assert (
        gdal.VSIStatL("/vsiicechunk/{data/icechunk/multi_chunks}/my_array/c/6/0")
        is None
    )
    assert (
        gdal.ReadDir("/vsiicechunk/{data/icechunk/multi_chunks}/my_array/c/6") is None
    )
    assert (
        gdal.VSIFOpenL("/vsiicechunk/{data/icechunk/multi_chunks}/my_array/c/6/0", "rb")
        is None
    )
    assert (
        gdal.VSIStatL("/vsiicechunk/{data/icechunk/multi_chunks}/my_array/c/0/0/0")
        is None
    )
    assert (
        gdal.ReadDir("/vsiicechunk/{data/icechunk/multi_chunks}/my_array/c/0/0/0")
        is None
    )
    assert (
        gdal.VSIFOpenL(
            "/vsiicechunk/{data/icechunk/multi_chunks}/my_array/c/0/0/0", "rb"
        )
        is None
    )

    ds = gdal.OpenEx(dirname, gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    ar = rg.OpenMDArray("my_array")
    expected = array.array("B", [1] * (11 * 7))
    expected[1 * 7 + 2] = 2
    expected[9 * 7 + 6] = 3
    assert ar.Read() == expected


def test_icechunk_path_sorting():

    dirname = "data/icechunk/path_sorting"
    if FORCE_REGENERATE or not os.path.exists(dirname):

        icechunk = _import_icechunk()
        zarr = _import_zarr()

        if os.path.exists(dirname):
            shutil.rmtree(dirname)

        storage = icechunk.local_filesystem_storage(dirname)
        repo = icechunk.Repository.create(storage)

        with repo.transaction("main", message="first commit") as store:
            root = zarr.group(store)

            a_minus_b = root.create(
                "a-b",
                shape=(1,),
                dtype="uint8",
                compressors=None,
            )
            a_minus_b[...] = 2
            group_a = root.create_group("a")
            a_slash_b = group_a.create(
                "b",
                shape=(1,),
                dtype="uint8",
                compressors=None,
            )
            a_slash_b[...] = 1

    ds = gdal.OpenEx(dirname, gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    a_slash_b = rg.OpenGroup("a").OpenMDArray("b")
    assert a_slash_b.Read() == array.array("B", [1])
    a_minus_b = rg.OpenMDArray("a-b")
    assert a_minus_b.Read() == array.array("B", [2])


@pytest.mark.require_curl()
@pytest.mark.network
def test_icechunk_virtual_ref_uncompressed():

    dirname = "data/icechunk/virtual_ref_uncompressed"
    if FORCE_REGENERATE or not os.path.exists(dirname):

        icechunk = _import_icechunk()
        zarr = _import_zarr()

        if os.path.exists(dirname):
            shutil.rmtree(dirname)

        config = icechunk.config.RepositoryConfig.default()
        config.set_virtual_chunk_container(
            icechunk.virtual.VirtualChunkContainer(
                "s3://cdn.proj.org/",
                icechunk.storage.s3_store(region="us-west-2", anonymous=True),
            )
        )

        storage = icechunk.local_filesystem_storage(dirname)
        repo = icechunk.Repository.create(storage, config=config)

        with repo.transaction("main", message="first commit") as store:
            root = zarr.group(store)
            root.create(
                "my_array",
                shape=(10,),
                chunks=(1,),
                dtype="uint8",
                compressors=None,
            )
            for i in range(10):
                store.set_virtual_ref(
                    f"/my_array/c/{i}",
                    "s3://cdn.proj.org/test_dummy/foo",
                    offset=i % 4,
                    length=1,
                )

    ds = gdal.OpenEx(dirname, gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    ar = rg.OpenMDArray("my_array")
    assert ar.Read() == array.array(
        "B", [102, 111, 111, 10, 102, 111, 111, 10, 102, 111]
    )


@pytest.mark.require_curl()
@pytest.mark.network
def test_icechunk_virtual_ref_compressed():

    dirname = "data/icechunk/virtual_ref_compressed"
    if FORCE_REGENERATE or not os.path.exists(dirname):

        icechunk = _import_icechunk()
        zarr = _import_zarr()

        if os.path.exists(dirname):
            shutil.rmtree(dirname)

        config = icechunk.config.RepositoryConfig.default()
        config.set_virtual_chunk_container(
            icechunk.virtual.VirtualChunkContainer(
                "s3://cdn.proj.org/",
                icechunk.storage.s3_store(region="us-west-2", anonymous=True),
            )
        )
        config.manifest = icechunk.config.ManifestConfig(
            virtual_chunk_location_compression=icechunk.config.ManifestVirtualChunkLocationCompressionConfig(
                min_num_chunks=1,
            )
        )

        storage = icechunk.local_filesystem_storage(dirname)
        repo = icechunk.Repository.create(storage, config=config)

        with repo.transaction("main", message="first commit") as store:
            root = zarr.group(store)
            root.create(
                "my_array",
                shape=(10,),
                chunks=(1,),
                dtype="uint8",
                compressors=None,
            )
            for i in range(10):
                store.set_virtual_ref(
                    f"/my_array/c/{i}",
                    "s3://cdn.proj.org/test_dummy/foo",
                    offset=i % 4,
                    length=1,
                )

    ds = gdal.OpenEx(dirname, gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    ar = rg.OpenMDArray("my_array")
    assert ar.Read() == array.array(
        "B", [102, 111, 111, 10, 102, 111, 111, 10, 102, 111]
    )


def test_icechunk_physical_chunks():

    dirname = "data/icechunk/physical_chunks"
    if FORCE_REGENERATE or not os.path.exists(dirname):

        icechunk = _import_icechunk()
        zarr = _import_zarr()

        if os.path.exists(dirname):
            shutil.rmtree(dirname)

        config = icechunk.config.RepositoryConfig(inline_chunk_threshold_bytes=1)

        storage = icechunk.local_filesystem_storage(dirname)
        repo = icechunk.Repository.create(storage, config=config)

        with repo.transaction("main", message="first commit") as store:
            root = zarr.group(store)
            my_array = root.create(
                "my_array",
                shape=(4,),
                chunks=(2,),
                dtype="uint8",
                compressors=None,
            )
            my_array[...] = [1, 2, 3, 4]

    ds = gdal.OpenEx(dirname, gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    ar = rg.OpenMDArray("my_array")
    assert ar.Read() == array.array("B", [1, 2, 3, 4])


def test_icechunk_sparse():

    dirname = "data/icechunk/sparse"
    if FORCE_REGENERATE or not os.path.exists(dirname):

        icechunk = _import_icechunk()
        zarr = _import_zarr()

        if os.path.exists(dirname):
            shutil.rmtree(dirname)

        storage = icechunk.local_filesystem_storage(dirname)
        repo = icechunk.Repository.create(storage)

        with repo.transaction("main", message="first commit") as store:
            root = zarr.group(store)
            my_array = root.create(
                "my_array",
                shape=(6,),
                chunks=(2,),
                dtype="uint8",
                compressors=None,
                fill_value=123,
            )
            my_array[0:2] = [10, 20]
            my_array[4:6] = [50, 60]

    ds = gdal.OpenEx(dirname, gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    ar = rg.OpenMDArray("my_array")
    assert ar.Read() == array.array("B", [10, 20, 123, 123, 50, 60])


def test_icechunk_repo_uncompressed():
    """Test opening a dataset whose "repo" file is uncompressed"""

    # Created with:
    # cp -r autotest/gdrivers/data/icechunk/empty_repo autotest/gdrivers/data/icechunk/empty_repo_uncompressed_repofileautotest/gdrivers/data/icechunk/empty_repo_uncompressed_repofile
    # dd if=autotest/gdrivers/data/icechunk/empty_repo_uncompressed_repofile/repo of=1.bin bs=1 count=39
    # dd if=autotest/gdrivers/data/icechunk/empty_repo_uncompressed_repofile/repo bs=1 skip=39 | zstdcat > 2.bin
    # Editing the byte at offset 38 (COMPRESSION_ALG) to be 0
    # cat 1.bin 2.bin > autotest/gdrivers/data/icechunk/empty_repo_uncompressed_repofile/repo

    assert (
        gdal.Open("data/icechunk/empty_repo_uncompressed_repofile")
        .GetDriver()
        .GetDescription()
        == "Icechunk"
    )


def _corrupt_every_byte(src_dir, dst_dir, filename):
    import gdaltest

    length = gdal.VSIStatL(src_dir + "/" + filename).size
    for i in range(length):
        if gdal.VSIStatL(dst_dir):
            gdal.RmdirRecursive(dst_dir)
        gdal.alg.vsi.copy(source=src_dir, destination=dst_dir + "/", recursive=True)
        with gdal.VSIFile(dst_dir + "/" + filename, "rb+") as f:
            f.seek(i)
            val = struct.unpack("B", f.read(1))[0]
            f.seek(i)
            val += 1
            if val == 256:
                val = 0
            f.write(struct.pack("B", val))

        with gdaltest.disable_exceptions():
            if gdal.Open(dst_dir) is None:
                print(i, val, gdal.GetLastErrorMsg())


@pytest.mark.parametrize(
    "offset,val,exception_msg",
    [
        (36, 1, "file version=1 != spec_version=2"),
        (36, 3, "Icechunk version 3 not supported"),
        (37, 7, "Got file type 7, expected 6"),
        (38, 1, "ZSTD decompression failed"),
        (39, 29, "invalid Repo Flatbuffer"),
        (51, 1, "invalid spec_version 0"),
        (193, 5, "Cannot open /vsimem/.*/snapshots/3K4DCY3NG3RE6EK56KZG"),
        (299, 19, r"flexbuffers::VerifyBuffer\(\) failed"),
        (371, 110, 'You need to specify a branch name among "nain"'),
    ],
)
def test_icechunk_repo_corrupted(tmp_vsimem, offset, val, exception_msg):

    src = "data/icechunk/empty_repo_uncompressed_repofile"

    if False:
        # Corrupt every byte. Helped establishing the above currated list
        _corrupt_every_byte(src, str(tmp_vsimem / "test"), "repo")

    gdal.alg.vsi.copy(source=src, destination=tmp_vsimem / "test/", recursive=True)
    with gdal.VSIFile(tmp_vsimem / "test/repo", "rb+") as f:
        f.seek(offset)
        f.write(struct.pack("B", val))

    with pytest.raises(Exception, match=exception_msg):
        gdal.Open(tmp_vsimem / "test")


def _create_repo_file(tmp_path, rootdirname, data):

    import json
    import subprocess

    with open(tmp_path / "repo.json", "w") as f:
        json.dump(data, f)

    subprocess.run(["flatc", "-b", "-o", tmp_path, "repo.fbs", tmp_path / "repo.json"])

    if not os.path.exists(rootdirname):
        os.mkdir(rootdirname, 0o755)
    with open(
        rootdirname / "repo",
        "wb",
    ) as f:
        f.write(b"ICE\xf0\x9f\xa7\x8aCHUNK")
        f.write(b" " * 24)
        f.write(b"\x02")
        f.write(b"\x06")  # repo
        f.write(b"\x00")
        f.write(open(tmp_path / "repo.bin", "rb").read())


def test_icechunk_minimum(tmp_path):

    rootdirname = Path("data/icechunk/minimum")

    if FORCE_REGENERATE or not os.path.exists(rootdirname):

        repo = {
            "spec_version": 2,
            "tags": [],
            "branches": [],
            "deleted_tags": [],
            "snapshots": [],
            "status": {},
            "latest_updates": [],
        }

        _create_repo_file(tmp_path, rootdirname, repo)

        for subdir in ("snapshots", "transactions"):
            if not os.path.exists(rootdirname / subdir):
                os.mkdir(rootdirname / subdir, 0o755)
            open(rootdirname / subdir / "_dummy_file_", "wb").close()

    assert gdal.Open("data/icechunk/minimum")


def test_icechunk_invalid_zstd():

    rootdirname = Path("data/icechunk/test_icechunk_invalid_zstd")

    if FORCE_REGENERATE or not os.path.exists(rootdirname):

        if not os.path.exists(rootdirname):
            os.mkdir(rootdirname, 0o755)
        with open(
            rootdirname / "repo",
            "wb",
        ) as f:
            f.write(b"ICE\xf0\x9f\xa7\x8aCHUNK")
            f.write(b" " * 24)
            f.write(b"\x02")
            f.write(b"\x06")  # repo
            f.write(b"\x01")  # zstd

    with pytest.raises(Exception, match="ZSTD decompression failed"):
        gdal.Open(rootdirname / "repo")


def test_icechunk_repo_offline(tmp_path):

    rootdirname = Path("data/icechunk/repo_offline")

    if FORCE_REGENERATE or not os.path.exists(rootdirname):

        repo = {
            "spec_version": 2,
            "tags": [],
            "branches": [],
            "deleted_tags": [],
            "snapshots": [],
            "status": {"availability": "Offline"},
            "latest_updates": [],
        }

        _create_repo_file(tmp_path, rootdirname, repo)

    with pytest.raises(Exception, match="repository is offline: unknown reason"):
        gdal.Open(rootdirname / "repo")


def test_icechunk_repo_tag_referring_invalid_snapshot(tmp_path):

    rootdirname = Path("data/icechunk/repo_tag_referring_invalid_snapshot")

    if FORCE_REGENERATE or not os.path.exists(rootdirname):

        repo = {
            "spec_version": 2,
            "tags": [{"name": "my_tag"}],
            "branches": [],
            "deleted_tags": [],
            "snapshots": [],
            "status": {},
            "latest_updates": [],
        }

        _create_repo_file(tmp_path, rootdirname, repo)

    with pytest.raises(Exception, match="tag 'my_tag', invalid snapshot_index 0"):
        gdal.Open(rootdirname / "repo")


def test_icechunk_repo_branch_referring_invalid_snapshot(tmp_path):

    rootdirname = Path("data/icechunk/repo_branch_referring_invalid_snapshot")

    if FORCE_REGENERATE or not os.path.exists(rootdirname):

        repo = {
            "spec_version": 2,
            "tags": [],
            "branches": [{"name": "my_branch"}],
            "deleted_tags": [],
            "snapshots": [],
            "status": {},
            "latest_updates": [],
        }

        _create_repo_file(tmp_path, rootdirname, repo)

    with pytest.raises(Exception, match="branch 'my_branch', invalid snapshot_index 0"):
        gdal.Open(rootdirname / "repo")


def test_icechunk_repo_two_tags_same_name(tmp_path):

    rootdirname = Path("data/icechunk/repo_two_tags_same_name")

    if FORCE_REGENERATE or not os.path.exists(rootdirname):

        repo = {
            "spec_version": 2,
            "tags": [{"name": "my_tag"}, {"name": "my_tag"}],
            "branches": [],
            "deleted_tags": [],
            "snapshots": [{"id": {"bytes": [0] * 12}, "message": "my commit"}],
            "status": {},
            "latest_updates": [],
        }

        _create_repo_file(tmp_path, rootdirname, repo)

    with pytest.raises(Exception, match="more than one tag 'my_tag'"):
        gdal.Open(rootdirname / "repo")


def test_icechunk_repo_two_branches_same_name(tmp_path):

    rootdirname = Path("data/icechunk/repo_two_branches_same_name")

    if FORCE_REGENERATE or not os.path.exists(rootdirname):

        repo = {
            "spec_version": 2,
            "tags": [],
            "branches": [{"name": "main"}, {"name": "main"}],
            "deleted_tags": [],
            "snapshots": [{"id": {"bytes": [0] * 12}, "message": "my commit"}],
            "status": {},
            "latest_updates": [],
        }

        _create_repo_file(tmp_path, rootdirname, repo)

    with pytest.raises(Exception, match="more than one branch 'main'"):
        gdal.Open(rootdirname / "repo")


def _create_repo_with_single_snapshot(tmp_path, rootdirname):

    repo = {
        "spec_version": 2,
        "tags": [],
        "branches": [{"name": "main"}],
        "deleted_tags": [],
        "snapshots": [{"id": {"bytes": [0] * 12}, "message": "my commit"}],
        "status": {},
        "latest_updates": [],
    }

    _create_repo_file(tmp_path, rootdirname, repo)


def _create_snapshot_file(tmp_path, rootdirname, filename, data, version=2):

    import json
    import subprocess

    with open(tmp_path / "snapshot.json", "w") as f:
        json.dump(data, f)

    subprocess.run(
        ["flatc", "-b", "-o", tmp_path, "snapshot.fbs", tmp_path / "snapshot.json"]
    )

    if not os.path.exists(rootdirname / "snapshots"):
        os.mkdir(rootdirname / "snapshots", 0o755)
    with open(
        rootdirname / "snapshots" / filename,
        "wb",
    ) as f:
        f.write(b"ICE\xf0\x9f\xa7\x8aCHUNK")
        f.write(b" " * 24)
        f.write(struct.pack("B", version))
        f.write(b"\x01")  # snapshot
        f.write(b"\x00")
        f.write(open(tmp_path / "snapshot.bin", "rb").read())


def test_icechunk_snapshot_missing(tmp_path):

    rootdirname = Path("data/icechunk/test_icechunk_snapshot_missing")

    if FORCE_REGENERATE or not os.path.exists(rootdirname):
        _create_repo_with_single_snapshot(tmp_path, rootdirname)

    with pytest.raises(
        Exception,
        match=r"Cannot open data[/\\]icechunk[/\\]test_icechunk_snapshot_missing[/\\]snapshots[/\\]00000000000000000000",
    ):
        gdal.Open(rootdirname / "repo")


def test_icechunk_snapshot_too_short(tmp_path):

    rootdirname = Path("data/icechunk/test_icechunk_snapshot_too_short")

    if FORCE_REGENERATE or not os.path.exists(rootdirname):
        _create_repo_with_single_snapshot(tmp_path, rootdirname)

        if not os.path.exists(rootdirname / "snapshots"):
            os.mkdir(rootdirname / "snapshots", 0o755)
        with open(rootdirname / "snapshots" / "00000000000000000000", "wb") as f:
            f.write(b"invalid")

    with pytest.raises(Exception, match="too small file"):
        gdal.Open(rootdirname / "repo")


def test_icechunk_snapshot_invalid(tmp_path):

    rootdirname = Path("data/icechunk/test_icechunk_snapshot_invalid")

    if FORCE_REGENERATE or not os.path.exists(rootdirname):
        _create_repo_with_single_snapshot(tmp_path, rootdirname)

        if not os.path.exists(rootdirname / "snapshots"):
            os.mkdir(rootdirname / "snapshots", 0o755)
        with open(rootdirname / "snapshots" / "00000000000000000000", "wb") as f:
            f.write(b"invalid" * 10)

    with pytest.raises(Exception, match="invalid Snapshot Flatbuffer"):
        gdal.Open(rootdirname / "repo")


def test_icechunk_snapshot_invalid_id(tmp_path):

    rootdirname = Path("data/icechunk/test_icechunk_snapshot_invalid_id")

    if FORCE_REGENERATE or not os.path.exists(rootdirname):
        _create_repo_with_single_snapshot(tmp_path, rootdirname)

        snapshot = {
            "id": {"bytes": [255] * 12},
            "nodes": [],
            "message": "my commit",
            "metadata": [],
            "manifest_files": [],
            "manifest_files_v2": [],
        }

        _create_snapshot_file(tmp_path, rootdirname, "00000000000000000000", snapshot)

    with pytest.raises(
        Exception, match="id=ZZZZZZZZZZZZZZZZZZZG != expected 00000000000000000000"
    ):
        gdal.Open(rootdirname / "repo")


def test_icechunk_snapshot_invalid_path(tmp_path):

    rootdirname = Path("data/icechunk/test_icechunk_snapshot_invalid_path")

    if FORCE_REGENERATE or not os.path.exists(rootdirname):
        _create_repo_with_single_snapshot(tmp_path, rootdirname)

        snapshot = {
            "id": {"bytes": [0] * 12},
            "nodes": [
                {
                    "id": {"bytes": [0] * 8},
                    "path": "invalid_path",
                    "user_data": [],
                    "node_data_type": "Array",
                    "node_data": {"shape": [], "manifests": [], "shape_v2": []},
                }
            ],
            "message": "my commit",
            "metadata": [],
            "manifest_files": [],
            "manifest_files_v2": [],
        }

        _create_snapshot_file(tmp_path, rootdirname, "00000000000000000000", snapshot)

    with pytest.raises(Exception, match="invalid node path 'invalid_path'"):
        gdal.Open(rootdirname / "repo")


def test_icechunk_snapshot_path_traversal(tmp_path):

    rootdirname = Path("data/icechunk/test_icechunk_snapshot_path_traversal")

    if FORCE_REGENERATE or not os.path.exists(rootdirname):
        _create_repo_with_single_snapshot(tmp_path, rootdirname)

        snapshot = {
            "id": {"bytes": [0] * 12},
            "nodes": [
                {
                    "id": {"bytes": [0] * 8},
                    "path": "/../x",
                    "user_data": [],
                    "node_data_type": "Array",
                    "node_data": {"shape": [], "manifests": [], "shape_v2": []},
                }
            ],
            "message": "my commit",
            "metadata": [],
            "manifest_files": [],
            "manifest_files_v2": [],
        }

        _create_snapshot_file(tmp_path, rootdirname, "00000000000000000000", snapshot)

    with pytest.raises(Exception, match="path traversal pattern in node path '/../x'"):
        gdal.Open(rootdirname / "repo")


def test_icechunk_snapshot_missing_shape_v2(tmp_path):

    rootdirname = Path("data/icechunk/test_icechunk_snapshot_missing_shape_v2")

    if FORCE_REGENERATE or not os.path.exists(rootdirname):
        _create_repo_with_single_snapshot(tmp_path, rootdirname)

        snapshot = {
            "id": {"bytes": [0] * 12},
            "nodes": [
                {
                    "id": {"bytes": [0] * 8},
                    "path": "/a",
                    "user_data": [],
                    "node_data_type": "Array",
                    "node_data": {"shape": [], "manifests": []},
                }
            ],
            "message": "my commit",
            "metadata": [],
            "manifest_files": [],
            "manifest_files_v2": [
                {},
            ],
        }

        _create_snapshot_file(tmp_path, rootdirname, "00000000000000000000", snapshot)

    with pytest.raises(Exception, match="missing shape_v2 in ArrayData"):
        gdal.Open(rootdirname / "repo")


def test_icechunk_snapshot_shape_v2_zero_num_chunks(tmp_path):

    rootdirname = Path("data/icechunk/test_icechunk_snapshot_shape_v2_zero_num_chunks")

    if FORCE_REGENERATE or not os.path.exists(rootdirname):
        _create_repo_with_single_snapshot(tmp_path, rootdirname)

        snapshot = {
            "id": {"bytes": [0] * 12},
            "nodes": [
                {
                    "id": {"bytes": [0] * 8},
                    "path": "/a",
                    "user_data": [],
                    "node_data_type": "Array",
                    "node_data": {
                        "shape": [],
                        "manifests": [],
                        "shape_v2": [{"array_length": 1, "num_chunks": 0}],
                    },
                }
            ],
            "message": "my commit",
            "metadata": [],
            "manifest_files": [],
            "manifest_files_v2": [
                {},
            ],
        }

        _create_snapshot_file(tmp_path, rootdirname, "00000000000000000000", snapshot)

    with pytest.raises(Exception, match="numChunks == 0"):
        gdal.Open(rootdirname / "repo")


def test_icechunk_snapshot_shape_v2_num_chunks_overflow(tmp_path):

    rootdirname = Path(
        "data/icechunk/test_icechunk_snapshot_shape_v2_num_chunks_overflow"
    )

    if FORCE_REGENERATE or not os.path.exists(rootdirname):
        _create_repo_with_single_snapshot(tmp_path, rootdirname)

        snapshot = {
            "id": {"bytes": [0] * 12},
            "nodes": [
                {
                    "id": {"bytes": [0] * 8},
                    "path": "/a",
                    "user_data": [],
                    "node_data_type": "Array",
                    "node_data": {
                        "shape": [],
                        "manifests": [],
                        "shape_v2": [
                            {"array_length": 4294967295, "num_chunks": 4294967295}
                        ]
                        * 3,
                    },
                }
            ],
            "message": "my commit",
            "metadata": [],
            "manifest_files": [],
            "manifest_files_v2": [
                {},
            ],
        }

        _create_snapshot_file(tmp_path, rootdirname, "00000000000000000000", snapshot)

    with pytest.raises(Exception, match="too many chunks"):
        gdal.Open(rootdirname / "repo")


def test_icechunk_snapshot_shape_v1_array_length_0(tmp_path):

    rootdirname = Path("data/icechunk/test_icechunk_snapshot_shape_v1_array_length_0")

    if FORCE_REGENERATE or not os.path.exists(rootdirname):
        _create_repo_with_single_snapshot(tmp_path, rootdirname)

        snapshot = {
            "id": {"bytes": [0] * 12},
            "nodes": [
                {
                    "id": {"bytes": [0] * 8},
                    "path": "/a",
                    "user_data": [],
                    "node_data_type": "Array",
                    "node_data": {
                        "shape": [{"array_length": 0, "chunk_length": 1}],
                        "manifests": [],
                    },
                }
            ],
            "message": "my commit",
            "metadata": [],
            "manifest_files": [],
            "manifest_files_v2": [
                {},
            ],
        }

        _create_snapshot_file(
            tmp_path, rootdirname, "00000000000000000000", snapshot, version=1
        )

    with pytest.raises(Exception, match="invalid shape in ArrayData"):
        gdal.Open(rootdirname / "repo")


def test_icechunk_snapshot_shape_v1_chunk_length_0(tmp_path):

    rootdirname = Path("data/icechunk/test_icechunk_snapshot_shape_v1_chunk_length_0")

    if FORCE_REGENERATE or not os.path.exists(rootdirname):
        _create_repo_with_single_snapshot(tmp_path, rootdirname)

        snapshot = {
            "id": {"bytes": [0] * 12},
            "nodes": [
                {
                    "id": {"bytes": [0] * 8},
                    "path": "/a",
                    "user_data": [],
                    "node_data_type": "Array",
                    "node_data": {
                        "shape": [{"array_length": 1, "chunk_length": 0}],
                        "manifests": [],
                    },
                }
            ],
            "message": "my commit",
            "metadata": [],
            "manifest_files": [],
            "manifest_files_v2": [
                {},
            ],
        }

        _create_snapshot_file(
            tmp_path, rootdirname, "00000000000000000000", snapshot, version=1
        )

    with pytest.raises(Exception, match="invalid shape in ArrayData"):
        gdal.Open(rootdirname / "repo")


def test_icechunk_snapshot_shape_v1_num_chunks_64bit(tmp_path):

    rootdirname = Path("data/icechunk/test_icechunk_snapshot_shape_v1_num_chunks_64bit")

    if FORCE_REGENERATE or not os.path.exists(rootdirname):
        _create_repo_with_single_snapshot(tmp_path, rootdirname)

        snapshot = {
            "id": {"bytes": [0] * 12},
            "nodes": [
                {
                    "id": {"bytes": [0] * 8},
                    "path": "/a",
                    "user_data": [],
                    "node_data_type": "Array",
                    "node_data": {
                        "shape": [{"array_length": 1 << 63, "chunk_length": 1}],
                        "manifests": [],
                    },
                }
            ],
            "message": "my commit",
            "metadata": [],
            "manifest_files": [],
            "manifest_files_v2": [
                {},
            ],
        }

        _create_snapshot_file(
            tmp_path, rootdirname, "00000000000000000000", snapshot, version=1
        )

    with pytest.raises(Exception, match="too many chunks"):
        gdal.Open(rootdirname / "repo")


def test_icechunk_snapshot_inconsistent_dimension(tmp_path):

    rootdirname = Path("data/icechunk/test_icechunk_snapshot_inconsistent_dimension")

    if FORCE_REGENERATE or not os.path.exists(rootdirname):
        _create_repo_with_single_snapshot(tmp_path, rootdirname)

        snapshot = {
            "id": {"bytes": [0] * 12},
            "nodes": [
                {
                    "id": {"bytes": [0] * 8},
                    "path": "/a",
                    "user_data": [],
                    "node_data_type": "Array",
                    "node_data": {
                        "shape": [],
                        "manifests": [
                            {
                                "object_id": {"bytes": [2] * 12},
                                "extents": [],
                            },
                        ],
                        "shape_v2": [{"array_length": 1, "num_chunks": 1}],
                    },
                }
            ],
            "message": "my commit",
            "metadata": [],
            "manifest_files": [],
            "manifest_files_v2": [],
        }

        _create_snapshot_file(tmp_path, rootdirname, "00000000000000000000", snapshot)

    with pytest.raises(
        Exception, match="array /a: manifest extents has not expected dimension count"
    ):
        gdal.Open(rootdirname / "repo")


def test_icechunk_snapshot_from_0_to_0(tmp_path):

    rootdirname = Path("data/icechunk/test_icechunk_snapshot_from_0_to_0")

    if FORCE_REGENERATE or not os.path.exists(rootdirname):
        _create_repo_with_single_snapshot(tmp_path, rootdirname)

        snapshot = {
            "id": {"bytes": [0] * 12},
            "nodes": [
                {
                    "id": {"bytes": [0] * 8},
                    "path": "/a",
                    "user_data": [],
                    "node_data_type": "Array",
                    "node_data": {
                        "shape": [],
                        "manifests": [
                            {
                                "object_id": {"bytes": [2] * 12},
                                "extents": [{"from": 0, "to": 0}],
                            },
                        ],
                        "shape_v2": [{"array_length": 1, "num_chunks": 1}],
                    },
                }
            ],
            "message": "my commit",
            "metadata": [],
            "manifest_files": [],
            "manifest_files_v2": [],
        }

        _create_snapshot_file(tmp_path, rootdirname, "00000000000000000000", snapshot)

    with pytest.raises(Exception, match="invalid manifest extent"):
        gdal.Open(rootdirname / "repo")


def test_icechunk_snapshot_from_1_to_1(tmp_path):

    rootdirname = Path("data/icechunk/test_icechunk_snapshot_from_1_to_1")

    if FORCE_REGENERATE or not os.path.exists(rootdirname):
        _create_repo_with_single_snapshot(tmp_path, rootdirname)

        snapshot = {
            "id": {"bytes": [0] * 12},
            "nodes": [
                {
                    "id": {"bytes": [0] * 8},
                    "path": "/a",
                    "user_data": [],
                    "node_data_type": "Array",
                    "node_data": {
                        "shape": [],
                        "manifests": [
                            {
                                "object_id": {"bytes": [2] * 12},
                                "extents": [{"from": 1, "to": 1}],
                            },
                        ],
                        "shape_v2": [{"array_length": 1, "num_chunks": 1}],
                    },
                }
            ],
            "message": "my commit",
            "metadata": [],
            "manifest_files": [],
            "manifest_files_v2": [],
        }

        _create_snapshot_file(tmp_path, rootdirname, "00000000000000000000", snapshot)

    with pytest.raises(Exception, match="invalid manifest extent"):
        gdal.Open(rootdirname / "repo")


def test_icechunk_snapshot_from_0_to_2(tmp_path):

    rootdirname = Path("data/icechunk/test_icechunk_snapshot_from_0_to_2")

    if FORCE_REGENERATE or not os.path.exists(rootdirname):
        _create_repo_with_single_snapshot(tmp_path, rootdirname)

        snapshot = {
            "id": {"bytes": [0] * 12},
            "nodes": [
                {
                    "id": {"bytes": [0] * 8},
                    "path": "/a",
                    "user_data": [],
                    "node_data_type": "Array",
                    "node_data": {
                        "shape": [],
                        "manifests": [
                            {
                                "object_id": {"bytes": [2] * 12},
                                "extents": [{"from": 0, "to": 2}],
                            },
                        ],
                        "shape_v2": [{"array_length": 1, "num_chunks": 1}],
                    },
                }
            ],
            "message": "my commit",
            "metadata": [],
            "manifest_files": [],
            "manifest_files_v2": [],
        }

        _create_snapshot_file(tmp_path, rootdirname, "00000000000000000000", snapshot)

    with pytest.raises(Exception, match="invalid manifest extent"):
        gdal.Open(rootdirname / "repo")


def test_icechunk_snapshot_too_many_chunks(tmp_path):

    rootdirname = Path("data/icechunk/test_icechunk_snapshot_too_many_chunks")

    if FORCE_REGENERATE or not os.path.exists(rootdirname):
        _create_repo_with_single_snapshot(tmp_path, rootdirname)

        snapshot = {
            "id": {"bytes": [0] * 12},
            "nodes": [
                {
                    "id": {"bytes": [0] * 8},
                    "path": "/a",
                    "user_data": [],
                    "node_data_type": "Array",
                    "node_data": {
                        "shape": [],
                        "manifests": [
                            {
                                "object_id": {"bytes": [2] * 12},
                                "extents": [{"from": 0, "to": 1}],
                            },
                            {
                                "object_id": {"bytes": [2] * 12},
                                "extents": [{"from": 0, "to": 1}],
                            },
                        ],
                        "shape_v2": [{"array_length": 1, "num_chunks": 1}],
                    },
                }
            ],
            "message": "my commit",
            "metadata": [],
            "manifest_files": [],
            "manifest_files_v2": [],
        }

        _create_snapshot_file(tmp_path, rootdirname, "00000000000000000000", snapshot)

    with pytest.raises(
        Exception,
        match="chunks referenced by manifest extents = 2 > chunks in the array = 1",
    ):
        gdal.Open(rootdirname / "repo")


def test_icechunk_snapshot_manifest_without_id(tmp_path):

    rootdirname = Path("data/icechunk/test_icechunk_snapshot_manifest_without_id")

    if FORCE_REGENERATE or not os.path.exists(rootdirname):
        _create_repo_with_single_snapshot(tmp_path, rootdirname)

        snapshot = {
            "id": {"bytes": [0] * 12},
            "nodes": [
                {
                    "id": {"bytes": [0] * 8},
                    "path": "/a",
                    "user_data": [],
                    "node_data_type": "Array",
                    "node_data": {"shape": [], "manifests": [], "shape_v2": []},
                }
            ],
            "message": "my commit",
            "metadata": [],
            "manifest_files": [],
            "manifest_files_v2": [
                {},
            ],
        }

        _create_snapshot_file(tmp_path, rootdirname, "00000000000000000000", snapshot)

    with pytest.raises(Exception, match="missing manifest id"):
        gdal.Open(rootdirname / "repo")


def test_icechunk_snapshot_manifest_id_non_increasing(tmp_path):

    rootdirname = Path(
        "data/icechunk/test_icechunk_snapshot_manifest_id_non_increasing"
    )

    if FORCE_REGENERATE or not os.path.exists(rootdirname):
        _create_repo_with_single_snapshot(tmp_path, rootdirname)

        snapshot = {
            "id": {"bytes": [0] * 12},
            "nodes": [
                {
                    "id": {"bytes": [0] * 8},
                    "path": "/a",
                    "user_data": [],
                    "node_data_type": "Array",
                    "node_data": {"shape": [], "manifests": [], "shape_v2": []},
                }
            ],
            "message": "my commit",
            "metadata": [],
            "manifest_files": [],
            "manifest_files_v2": [
                {"id": {"bytes": [2] * 12}},
                {"id": {"bytes": [1] * 12}},
            ],
        }

        _create_snapshot_file(tmp_path, rootdirname, "00000000000000000000", snapshot)

    with pytest.raises(
        Exception, match="ManifestInfo array not sorted by increasing id"
    ):
        gdal.Open(rootdirname / "repo")


def test_icechunk_snapshot_manifest_v1_id_non_increasing(tmp_path):

    rootdirname = Path(
        "data/icechunk/test_icechunk_snapshot_manifest_v1_id_non_increasing"
    )

    if FORCE_REGENERATE or not os.path.exists(rootdirname):
        _create_repo_with_single_snapshot(tmp_path, rootdirname)

        snapshot = {
            "id": {"bytes": [0] * 12},
            "nodes": [
                {
                    "id": {"bytes": [0] * 8},
                    "path": "/a",
                    "user_data": [],
                    "node_data_type": "Array",
                    "node_data": {"shape": [], "manifests": [], "shape_v2": []},
                }
            ],
            "message": "my commit",
            "metadata": [],
            "manifest_files": [
                {"id": {"bytes": [2] * 12}, "size_bytes": 0, "num_chunk_refs": 0},
                {"id": {"bytes": [1] * 12}, "size_bytes": 0, "num_chunk_refs": 0},
            ],
            "manifest_files_v2": [],
        }

        _create_snapshot_file(
            tmp_path, rootdirname, "00000000000000000000", snapshot, version=1
        )

    with pytest.raises(
        Exception, match="ManifestInfo array not sorted by increasing id"
    ):
        gdal.Open(rootdirname / "repo")


def _create_simple_repo_and_snapshot_files(
    tmp_path,
    rootdirname,
    chunk_ref_manifest_id=[2] * 12,
    manifest_file_size=0,
    manifest_num_chunk_refs=0,
):

    import json

    _create_repo_with_single_snapshot(tmp_path, rootdirname)

    snapshot = {
        "id": {"bytes": [0] * 12},
        "nodes": [
            {
                "id": {"bytes": [0] * 8},
                "path": "/my_array",
                "user_data": list(
                    json.dumps(
                        {
                            "zarr_format": 3,
                            "node_type": "array",
                            "shape": [1],
                            "data_type": "uint8",
                            "chunk_grid": {
                                "name": "regular",
                                "configuration": {"chunk_shape": [1]},
                            },
                            "chunk_key_encoding": {
                                "name": "default",
                                "configuration": {"separator": "/"},
                            },
                            "fill_value": 0,
                            "codecs": [
                                {"name": "bytes", "configuration": {"endian": "little"}}
                            ],
                            "attributes": {},
                            "dimension_names": ["X"],
                        }
                    ).encode("utf-8")
                ),
                "node_data_type": "Array",
                "node_data": {
                    "shape": [],
                    "manifests": [
                        {
                            "object_id": {"bytes": chunk_ref_manifest_id},
                            "extents": [{"from": 0, "to": 1}],
                        },
                    ],
                    "shape_v2": [{"array_length": 1, "num_chunks": 1}],
                },
            },
            {
                "id": {"bytes": [0] * 8},
                "path": "/",
                "user_data": list(
                    json.dumps({"zarr_format": 3, "node_type": "group"}).encode("utf-8")
                ),
                "node_data_type": "Group",
                "node_data": {},
            },
        ],
        "message": "my commit",
        "metadata": [],
        "manifest_files": [],
        "manifest_files_v2": [
            {
                "id": {"bytes": [2] * 12},
                "size_bytes": manifest_file_size,
                "num_chunk_refs": manifest_num_chunk_refs,
            },
        ],
    }

    _create_snapshot_file(tmp_path, rootdirname, "00000000000000000000", snapshot)

    if not os.path.exists(rootdirname / "transactions"):
        os.mkdir(rootdirname / "transactions", 0o755)
    open(rootdirname / "transactions" / "_dummy_file_", "wb").close()


def test_icechunk_manifests_not_referenced_in_snapshot(tmp_path):

    rootdirname = Path(
        "data/icechunk/test_icechunk_manifests_not_referenced_in_snapshot"
    )

    if FORCE_REGENERATE or not os.path.exists(rootdirname):

        _create_simple_repo_and_snapshot_files(
            tmp_path, rootdirname, chunk_ref_manifest_id=[1] * 12
        )

    ds = gdal.OpenEx(rootdirname, gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    ar = rg.OpenMDArray("my_array")
    assert ar
    with pytest.raises(
        Exception, match="Manifest 040G2081040G2081040G not referenced in snapshot"
    ):
        ar.Read()


def _create_manifest_file(
    tmp_path, rootdirname, manifest_filename, manifest, version=2
):

    import json
    import subprocess

    with open(tmp_path / "manifest.json", "w") as f:
        json.dump(manifest, f)

    subprocess.run(
        ["flatc", "-b", "-o", tmp_path, "manifest.fbs", tmp_path / "manifest.json"]
    )

    if not os.path.exists(rootdirname):
        os.mkdir(rootdirname, 0o755)
    if not os.path.exists(rootdirname / "manifests"):
        os.mkdir(rootdirname / "manifests", 0o755)
    with open(
        rootdirname / "manifests" / manifest_filename,
        "wb",
    ) as f:
        f.write(b"ICE\xf0\x9f\xa7\x8aCHUNK")
        f.write(b" " * 24)
        f.write(struct.pack("B", version))
        f.write(b"\x02")  # manifest
        f.write(b"\x00")
        f.write(open(tmp_path / "manifest.bin", "rb").read())
        return f.tell()


def _create_simple_repo_and_snapshot_with_manifest_file(
    tmp_path, rootdirname, manifest_filename, manifest, manifest_num_chunk_refs=0
):

    manifest_file_size = _create_manifest_file(
        tmp_path, rootdirname, manifest_filename, manifest
    )

    _create_simple_repo_and_snapshot_files(
        tmp_path,
        rootdirname,
        manifest_file_size=manifest_file_size,
        manifest_num_chunk_refs=manifest_num_chunk_refs,
    )


def test_icechunk_manifest_missing(tmp_path):

    rootdirname = Path("data/icechunk/test_icechunk_manifest_missing")

    if FORCE_REGENERATE or not os.path.exists(rootdirname):

        _create_simple_repo_and_snapshot_files(tmp_path, rootdirname)

    ds = gdal.OpenEx(rootdirname, gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    ar = rg.OpenMDArray("my_array")
    assert ar
    with pytest.raises(
        Exception,
        match=r"Cannot open data[/\\]icechunk[/\\]test_icechunk_manifest_missing[/\\]manifests[/\\]081040G2081040G20810",
    ):
        ar.Read()


def test_icechunk_manifest_too_short(tmp_path):

    rootdirname = Path("data/icechunk/test_icechunk_manifest_too_short")

    if FORCE_REGENERATE or not os.path.exists(rootdirname):

        if not os.path.exists(rootdirname):
            os.mkdir(rootdirname, 0o755)

        if not os.path.exists(rootdirname / "manifests"):
            os.mkdir(rootdirname / "manifests", 0o755)

        with open(rootdirname / "manifests" / "081040G2081040G20810", "wb") as f:
            f.write(b"invalid")
            manifest_file_size = f.tell()

        _create_simple_repo_and_snapshot_files(
            tmp_path, rootdirname, manifest_file_size=manifest_file_size
        )

    ds = gdal.OpenEx(rootdirname, gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    ar = rg.OpenMDArray("my_array")
    assert ar
    with pytest.raises(Exception, match="too small file"):
        ar.Read()


def test_icechunk_manifest_invalid(tmp_path):

    rootdirname = Path("data/icechunk/test_icechunk_manifest_invalid")

    if FORCE_REGENERATE or not os.path.exists(rootdirname):

        if not os.path.exists(rootdirname):
            os.mkdir(rootdirname, 0o755)

        if not os.path.exists(rootdirname / "manifests"):
            os.mkdir(rootdirname / "manifests", 0o755)

        with open(rootdirname / "manifests" / "081040G2081040G20810", "wb") as f:
            f.write(b"invalid" * 10)
            manifest_file_size = f.tell()

        _create_simple_repo_and_snapshot_files(
            tmp_path, rootdirname, manifest_file_size=manifest_file_size
        )

    ds = gdal.OpenEx(rootdirname, gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    ar = rg.OpenMDArray("my_array")
    assert ar
    with pytest.raises(Exception, match="invalid Manifest Flatbuffer"):
        ar.Read()


def test_icechunk_manifest_mismatch_id_filename(tmp_path):

    rootdirname = Path("data/icechunk/test_icechunk_manifest_mismatch_id_filename")

    if FORCE_REGENERATE or not os.path.exists(rootdirname):

        manifest = {
            "id": {"bytes": [255] * 12},
            "arrays": [],
        }

        _create_simple_repo_and_snapshot_with_manifest_file(
            tmp_path, rootdirname, "081040G2081040G20810", manifest
        )

    ds = gdal.OpenEx(rootdirname, gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    ar = rg.OpenMDArray("my_array")
    assert ar
    with pytest.raises(
        Exception, match="id=ZZZZZZZZZZZZZZZZZZZG != expected 081040G2081040G20810"
    ):
        ar.Read()


def test_icechunk_manifest_invalid_compression_alg(tmp_path):

    rootdirname = Path("data/icechunk/test_icechunk_manifest_invalid_compression_alg")

    if FORCE_REGENERATE or not os.path.exists(rootdirname):

        manifest = {
            "id": {"bytes": [2] * 12},
            "compression_algorithm": 2,
            "arrays": [],
        }

        _create_simple_repo_and_snapshot_with_manifest_file(
            tmp_path, rootdirname, "081040G2081040G20810", manifest
        )

    ds = gdal.OpenEx(rootdirname, gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    ar = rg.OpenMDArray("my_array")
    assert ar
    with pytest.raises(Exception, match="invalid compression_algorithm = 2"):
        ar.Read()


def test_icechunk_manifest_invalid_location_dictionary(tmp_path):

    rootdirname = Path(
        "data/icechunk/test_icechunk_manifest_invalid_location_dictionary"
    )

    if FORCE_REGENERATE or not os.path.exists(rootdirname):

        manifest = {
            "id": {"bytes": [2] * 12},
            "compression_algorithm": 1,
            "location_dictionary": [
                0x37,
                0xA4,
                0x30,
                0xEC,  # zstd dictionary magic
                0x00,
                0x00,
                0x00,
                0x00,
                0xFF,
                0xFF,
                0xFF,
                0xFF,
            ],
            "arrays": [],
        }

        _create_simple_repo_and_snapshot_with_manifest_file(
            tmp_path, rootdirname, "081040G2081040G20810", manifest
        )

    ds = gdal.OpenEx(rootdirname, gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    ar = rg.OpenMDArray("my_array")
    assert ar
    with pytest.raises(Exception, match=r"ZSTD_DCtx_loadDictionary\(\) failed"):
        ar.Read()


def test_icechunk_manifest_array_id_non_increasing(tmp_path):

    rootdirname = Path("data/icechunk/test_icechunk_manifest_array_id_non_increasing")

    if FORCE_REGENERATE or not os.path.exists(rootdirname):

        manifest = {
            "id": {"bytes": [2] * 12},
            "compression_algorithm": 1,
            "arrays": [
                {"node_id": {"bytes": [1] * 8}, "refs": []},
                {"node_id": {"bytes": [0] * 8}, "refs": []},
            ],
        }

        _create_simple_repo_and_snapshot_with_manifest_file(
            tmp_path, rootdirname, "081040G2081040G20810", manifest
        )

    ds = gdal.OpenEx(rootdirname, gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    ar = rg.OpenMDArray("my_array")
    assert ar
    with pytest.raises(
        Exception, match="arrayManifests array not sorted by increasing node id"
    ):
        ar.Read()


def test_icechunk_manifest_chunk_ref_index_non_increasing(tmp_path):

    rootdirname = Path(
        "data/icechunk/test_icechunk_manifest_chunk_ref_index_non_increasing"
    )

    if FORCE_REGENERATE or not os.path.exists(rootdirname):

        manifest = {
            "id": {"bytes": [2] * 12},
            "compression_algorithm": 1,
            "arrays": [
                {
                    "node_id": {"bytes": [1] * 8},
                    "refs": [
                        {"index": [0], "inline": [0]},
                        {"index": [0], "inline": [0]},
                    ],
                },
            ],
        }

        _create_simple_repo_and_snapshot_with_manifest_file(
            tmp_path, rootdirname, "081040G2081040G20810", manifest
        )

    ds = gdal.OpenEx(rootdirname, gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    ar = rg.OpenMDArray("my_array")
    assert ar
    with pytest.raises(
        Exception,
        match="chunkRefs array for node 040G2081040G20: not sorted by increasing chunk index",
    ):
        ar.Read()


def test_icechunk_manifest_chunk_ref_index_not_same_dim(tmp_path):

    rootdirname = Path(
        "data/icechunk/test_icechunk_manifest_chunk_ref_index_not_same_dim"
    )

    if FORCE_REGENERATE or not os.path.exists(rootdirname):

        manifest = {
            "id": {"bytes": [2] * 12},
            "compression_algorithm": 1,
            "arrays": [
                {
                    "node_id": {"bytes": [1] * 8},
                    "refs": [
                        {"index": [0], "inline": [0]},
                        {"index": [1, 2], "inline": [0]},
                    ],
                },
            ],
        }

        _create_simple_repo_and_snapshot_with_manifest_file(
            tmp_path, rootdirname, "081040G2081040G20810", manifest
        )

    ds = gdal.OpenEx(rootdirname, gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    ar = rg.OpenMDArray("my_array")
    assert ar
    with pytest.raises(
        Exception,
        match="chunkRefs array for node 040G2081040G20: chunk index do not have the same dimension",
    ):
        ar.Read()


def test_icechunk_manifest_chunk_ref_invalid_offset_length(tmp_path):

    rootdirname = Path(
        "data/icechunk/test_icechunk_manifest_chunk_ref_invalid_offset_length"
    )

    if FORCE_REGENERATE or not os.path.exists(rootdirname):

        manifest = {
            "id": {"bytes": [2] * 12},
            "compression_algorithm": 1,
            "arrays": [
                {
                    "node_id": {"bytes": [1] * 8},
                    "refs": [
                        {
                            "index": [0],
                            "chunk_id": {"bytes": [0] * 12},
                            "offset": (1 << 64) - 1,
                            "length": 1,
                        },
                    ],
                },
            ],
        }

        _create_simple_repo_and_snapshot_with_manifest_file(
            tmp_path, rootdirname, "081040G2081040G20810", manifest
        )

    ds = gdal.OpenEx(rootdirname, gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    ar = rg.OpenMDArray("my_array")
    assert ar
    with pytest.raises(Exception, match="chunkRef: invalid offset/size"):
        ar.Read()


def test_icechunk_manifest_chunk_ref_inline_content_with_offset_length(tmp_path):

    rootdirname = Path(
        "data/icechunk/test_icechunk_manifest_chunk_ref_inline_content_with_offset_length"
    )

    if FORCE_REGENERATE or not os.path.exists(rootdirname):

        manifest = {
            "id": {"bytes": [2] * 12},
            "compression_algorithm": 1,
            "arrays": [
                {
                    "node_id": {"bytes": [1] * 8},
                    "refs": [
                        {"index": [0], "inline": [0], "offset": 1},
                    ],
                },
            ],
        }

        _create_simple_repo_and_snapshot_with_manifest_file(
            tmp_path, rootdirname, "081040G2081040G20810", manifest
        )

    ds = gdal.OpenEx(rootdirname, gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    ar = rg.OpenMDArray("my_array")
    assert ar
    with pytest.raises(
        Exception, match="chunkRef: offset/size != 0 found with inline content"
    ):
        ar.Read()


def test_icechunk_manifest_chunk_ref_missing_info(tmp_path):

    rootdirname = Path("data/icechunk/test_icechunk_manifest_chunk_ref_missing_info")

    if FORCE_REGENERATE or not os.path.exists(rootdirname):

        manifest = {
            "id": {"bytes": [2] * 12},
            "compression_algorithm": 1,
            "arrays": [
                {
                    "node_id": {"bytes": [1] * 8},
                    "refs": [
                        {"index": [0]},
                    ],
                },
            ],
        }

        _create_simple_repo_and_snapshot_with_manifest_file(
            tmp_path, rootdirname, "081040G2081040G20810", manifest
        )

    ds = gdal.OpenEx(rootdirname, gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    ar = rg.OpenMDArray("my_array")
    assert ar
    with pytest.raises(
        Exception,
        match="chunkRef node_id 040G2081040G20: not inline, chunk or virtual location",
    ):
        ar.Read()


def test_icechunk_manifest_chunk_ref_mutually_exclusive_info(tmp_path):

    rootdirname = Path(
        "data/icechunk/test_icechunk_manifest_chunk_ref_mutually_exclusive_info"
    )

    if FORCE_REGENERATE or not os.path.exists(rootdirname):

        manifest = {
            "id": {"bytes": [2] * 12},
            "compression_algorithm": 1,
            "arrays": [
                {
                    "node_id": {"bytes": [1] * 8},
                    "refs": [
                        {"index": [0], "inline": [0], "location": "my_loc"},
                    ],
                },
            ],
        }

        _create_simple_repo_and_snapshot_with_manifest_file(
            tmp_path, rootdirname, "081040G2081040G20810", manifest
        )

    ds = gdal.OpenEx(rootdirname, gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    ar = rg.OpenMDArray("my_array")
    assert ar
    with pytest.raises(
        Exception,
        match=r"chunkRef node_id 040G2081040G20: more than one method among inline, chunk or virtual location found. inlineContent.size\(\) = 1, offset = 0, length = 0, chunkId=, location=my_loc",
    ):
        ar.Read()


def test_icechunk_manifest_chunk_ref_compressed_location_fails(tmp_path):

    rootdirname = Path(
        "data/icechunk/test_icechunk_manifest_chunk_ref_compressed_location_fails"
    )

    if FORCE_REGENERATE or not os.path.exists(rootdirname):

        manifest = {
            "id": {"bytes": [2] * 12},
            "compression_algorithm": 1,
            "arrays": [
                {
                    "node_id": {"bytes": [1] * 8},
                    "refs": [
                        {"index": [0], "compressed_location": [0]},
                    ],
                },
            ],
        }

        _create_simple_repo_and_snapshot_with_manifest_file(
            tmp_path, rootdirname, "081040G2081040G20810", manifest
        )

    ds = gdal.OpenEx(rootdirname, gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    ar = rg.OpenMDArray("my_array")
    assert ar
    with pytest.raises(
        Exception,
        match=r"chunkRef node_id 040G2081040G20: ZSTD_decompressDCtx\(\) failed",
    ):
        ar.Read()


@pytest.mark.require_curl()
@pytest.mark.network
def test_icechunk_manifest_chunk_ref_compressed_location_uncompressed(tmp_path):

    rootdirname = Path(
        "data/icechunk/test_icechunk_manifest_chunk_ref_compressed_location_uncompressed"
    )

    if FORCE_REGENERATE or not os.path.exists(rootdirname):

        manifest = {
            "id": {"bytes": [2] * 12},
            "compression_algorithm": 0,
            "arrays": [
                {
                    "node_id": {"bytes": [0] * 8},
                    "refs": [
                        {
                            "index": [0],
                            "compressed_location": list(
                                b"https://s3-us-west-2.amazonaws.com/cdn.proj.org/test_dummy/foo"
                            ),
                            "length": 1,
                        },
                    ],
                },
            ],
        }

        _create_simple_repo_and_snapshot_with_manifest_file(
            tmp_path,
            rootdirname,
            "081040G2081040G20810",
            manifest,
            manifest_num_chunk_refs=1,
        )

    ds = gdal.OpenEx(rootdirname, gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    ar = rg.OpenMDArray("my_array")
    assert ar
    assert ar.Read() == array.array("B", [102])


@pytest.mark.require_curl()
@pytest.mark.network
def test_icechunk_manifest_chunk_ref_location_uncompressed_beyond_file_size(tmp_path):

    rootdirname = Path(
        "data/icechunk/test_icechunk_manifest_chunk_ref_location_uncompressed_beyond_file_size"
    )

    if FORCE_REGENERATE or not os.path.exists(rootdirname):

        manifest = {
            "id": {"bytes": [2] * 12},
            "compression_algorithm": 0,
            "arrays": [
                {
                    "node_id": {"bytes": [0] * 8},
                    "refs": [
                        {
                            "index": [0],
                            "location": "https://s3-us-west-2.amazonaws.com/cdn.proj.org/test_dummy/foo",
                            "offset": 4,
                            "length": 1,
                        },
                    ],
                },
            ],
        }

        _create_simple_repo_and_snapshot_with_manifest_file(
            tmp_path,
            rootdirname,
            "081040G2081040G20810",
            manifest,
            manifest_num_chunk_refs=1,
        )

    ds = gdal.OpenEx(rootdirname, gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    ar = rg.OpenMDArray("my_array")
    assert ar

    with pytest.raises(
        Exception,
        match=r"\(offset,length\)=\(4,1\) beyond /vsicurl/https://s3-us-west-2.amazonaws.com/cdn.proj.org/test_dummy/foo size",
    ):
        ar.Read()


def test_icechunk_manifest_chunk_ref_invalid_location(tmp_path):

    rootdirname = Path(
        "data/icechunk/test_icechunk_manifest_chunk_ref_invalid_location"
    )

    if FORCE_REGENERATE or not os.path.exists(rootdirname):

        manifest = {
            "id": {"bytes": [2] * 12},
            "arrays": [
                {
                    "node_id": {"bytes": [0] * 8},
                    "refs": [
                        {
                            "index": [0],
                            "location": "data/byte.tif",
                            "length": 1,
                        },
                    ],
                },
            ],
        }

        _create_simple_repo_and_snapshot_with_manifest_file(
            tmp_path,
            rootdirname,
            "081040G2081040G20810",
            manifest,
            manifest_num_chunk_refs=1,
        )

    ds = gdal.OpenEx(rootdirname, gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    ar = rg.OpenMDArray("my_array")
    assert ar

    with pytest.raises(
        Exception,
        match="Access to non-network chunk location 'data/byte.tif' disabled by default",
    ):
        ar.Read()

    ds = gdal.OpenEx(rootdirname, gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    ar = rg.OpenMDArray("my_array")
    assert ar

    with gdal.config_option("ICECHUNK_ALLOW_LOCAL_CHUNK_LOCATION", "YES"):
        assert ar.Read() == b"I"


def test_icechunk_manifest_inconsistent_chunk_ref_count(tmp_path):

    rootdirname = Path(
        "data/icechunk/test_icechunk_manifest_inconsistent_chunk_ref_count"
    )

    if FORCE_REGENERATE or not os.path.exists(rootdirname):

        manifest = {
            "id": {"bytes": [2] * 12},
            "arrays": [
                {
                    "node_id": {"bytes": [0] * 8},
                    "refs": [
                        {
                            "index": [0],
                            "inline": [0],
                        },
                    ],
                },
            ],
        }

        _create_simple_repo_and_snapshot_with_manifest_file(
            tmp_path,
            rootdirname,
            "081040G2081040G20810",
            manifest,
            manifest_num_chunk_refs=2,
        )

    ds = gdal.OpenEx(rootdirname, gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    ar = rg.OpenMDArray("my_array")
    assert ar

    with pytest.raises(
        Exception,
        match=r"Actual count of chunk references in manifest .* does not match expected one = 2",
    ):
        ar.Read()


def test_icechunk_manifest_inconsistent_chunk_ref_checksum_last_modified(tmp_path):

    rootdirname = Path(
        "data/icechunk/test_icechunk_manifest_inconsistent_chunk_ref_checksum_last_modified"
    )

    if FORCE_REGENERATE or not os.path.exists(rootdirname):

        manifest = {
            "id": {"bytes": [2] * 12},
            "arrays": [
                {
                    "node_id": {"bytes": [0] * 8},
                    "refs": [
                        {
                            "index": [0],
                            "chunk_id": {"bytes": [0] * 12},
                            "offset": 0,
                            "length": 1,
                            "checksum_last_modified": 1,
                        },
                    ],
                },
            ],
        }

        _create_simple_repo_and_snapshot_with_manifest_file(
            tmp_path,
            rootdirname,
            "081040G2081040G20810",
            manifest,
            manifest_num_chunk_refs=1,
        )

        os.mkdir(rootdirname / "chunks", 0o755)
        open(rootdirname / "chunks" / "00000000000000000000", "wb").write(b"\x01")

    ds = gdal.OpenEx(rootdirname, gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    ar = rg.OpenMDArray("my_array")
    assert ar

    with pytest.raises(
        Exception,
        match=r"Last modified timestamp verification on .* failed",
    ):
        ar.Read()

    ds = gdal.OpenEx(
        f"ICECHUNK:{rootdirname}?ignore-timestamp-etag=yes", gdal.OF_MULTIDIM_RASTER
    )
    rg = ds.GetRootGroup()
    ar = rg.OpenMDArray("my_array")
    assert ar
    assert ar.Read() == b"\x01"


def test_icechunk_manifest_inconsistent_chunk_ref_dim(tmp_path):

    rootdirname = Path(
        "data/icechunk/test_icechunk_manifest_inconsistent_chunk_ref_dim"
    )

    if FORCE_REGENERATE or not os.path.exists(rootdirname):

        manifest = {
            "id": {"bytes": [2] * 12},
            "arrays": [
                {
                    "node_id": {"bytes": [0] * 8},
                    "refs": [
                        {
                            "index": [0, 0],
                            "inline": [0],
                        },
                    ],
                },
            ],
        }

        _create_simple_repo_and_snapshot_with_manifest_file(
            tmp_path,
            rootdirname,
            "081040G2081040G20810",
            manifest,
            manifest_num_chunk_refs=1,
        )

    ds = gdal.OpenEx(rootdirname, gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    ar = rg.OpenMDArray("my_array")
    assert ar

    with pytest.raises(
        Exception,
        match=r"GetChunkRef\(00000000000000\): querying with index of dimension 1 whereas chunk refs have dimension 2",
    ):
        ar.Read()


def test_icechunk_GetFileMetadata():

    assert gdal.GetFileMetadata("/vsiicechunk/", None) == {}

    with pytest.raises(Exception, match="Invalid /vsiicechunk/ syntax"):
        assert gdal.GetFileMetadata("/vsiicechunk/", "CHUNK_INFO")

    with pytest.raises(Exception, match="Invalid /vsiicechunk/ syntax"):
        assert gdal.GetFileMetadata("/vsiicechunk/{foo", "CHUNK_INFO")

    assert (
        gdal.GetFileMetadata(
            "/vsiicechunk/{data/icechunk/scalar_array}/invalid", "CHUNK_INFO"
        )
        == {}
    )

    assert (
        gdal.GetFileMetadata(
            "/vsiicechunk/{data/icechunk/scalar_array}/invalid/zarr.json", "CHUNK_INFO"
        )
        == {}
    )

    assert gdal.GetFileMetadata(
        "/vsiicechunk/{data/icechunk/scalar_array}/zarr.json", "CHUNK_INFO"
    ) == {
        "SIZE": "66",
        "BASE64": "ewogICJhdHRyaWJ1dGVzIjoge30sCiAgInphcnJfZm9ybWF0IjogMywKICAibm9kZV90eXBlIjogImdyb3VwIgp9",
    }

    assert gdal.GetFileMetadata(
        "/vsiicechunk/{data/icechunk/scalar_array}/my_array/zarr.json", "CHUNK_INFO"
    ) == {
        "SIZE": "473",
        "BASE64": "ewogICJzaGFwZSI6IFtdLAogICJkYXRhX3R5cGUiOiAiaW50MzIiLAogICJjaHVua19ncmlkIjogewogICAgIm5hbWUiOiAicmVndWxhciIsCiAgICAiY29uZmlndXJhdGlvbiI6IHsKICAgICAgImNodW5rX3NoYXBlIjogW10KICAgIH0KICB9LAogICJjaHVua19rZXlfZW5jb2RpbmciOiB7CiAgICAibmFtZSI6ICJkZWZhdWx0IiwKICAgICJjb25maWd1cmF0aW9uIjogewogICAgICAic2VwYXJhdG9yIjogIi8iCiAgICB9CiAgfSwKICAiZmlsbF92YWx1ZSI6IDAsCiAgImNvZGVjcyI6IFsKICAgIHsKICAgICAgIm5hbWUiOiAiYnl0ZXMiLAogICAgICAiY29uZmlndXJhdGlvbiI6IHsKICAgICAgICAiZW5kaWFuIjogImxpdHRsZSIKICAgICAgfQogICAgfQogIF0sCiAgImF0dHJpYnV0ZXMiOiB7fSwKICAiemFycl9mb3JtYXQiOiAzLAogICJub2RlX3R5cGUiOiAiYXJyYXkiLAogICJzdG9yYWdlX3RyYW5zZm9ybWVycyI6IFtdCn0=",
    }

    assert gdal.GetFileMetadata(
        "/vsiicechunk/{data/icechunk/scalar_array}/my_array/c", "CHUNK_INFO"
    ) == {"BASE64": "AQAAAA==", "SIZE": "4"}

    assert (
        gdal.GetFileMetadata(
            "/vsiicechunk/{data/icechunk/scalar_array}/my_array/c/0", "CHUNK_INFO"
        )
        == {}
    )

    assert gdal.GetFileMetadata(
        "/vsiicechunk/{data/icechunk/sparse}/my_array/c/0", "CHUNK_INFO"
    ) == {"BASE64": "ChQ=", "SIZE": "2"}

    assert (
        gdal.GetFileMetadata(
            "/vsiicechunk/{data/icechunk/sparse}/my_array/c/1", "CHUNK_INFO"
        )
        == {}
    )

    assert gdal.GetFileMetadata(
        "/vsiicechunk/{data/icechunk/sparse}/my_array/c/2", "CHUNK_INFO"
    ) == {"BASE64": "Mjw=", "SIZE": "2"}

    assert (
        gdal.GetFileMetadata(
            "/vsiicechunk/{data/icechunk/physical_chunks}/my_array/c", "CHUNK_INFO"
        )
        == {}
    )

    assert (
        gdal.GetFileMetadata(
            "/vsiicechunk/{data/icechunk/physical_chunks}/my_array/c/2", "CHUNK_INFO"
        )
        == {}
    )

    assert (
        gdal.GetFileMetadata(
            "/vsiicechunk/{data/icechunk/physical_chunks}/my_array/c/0/0", "CHUNK_INFO"
        )
        == {}
    )

    md = gdal.GetFileMetadata(
        "/vsiicechunk/{data/icechunk/physical_chunks}/my_array/c/0", "CHUNK_INFO"
    )
    md["FILENAME"] = md["FILENAME"].replace("\\", "/")
    assert md == {
        "SIZE": "2",
        "OFFSET": "0",
        "FILENAME": "data/icechunk/physical_chunks/chunks/Q3FTWG07FG2G6Z0JJESG",
    }

    assert gdal.GetFileMetadata(
        "/vsiicechunk/{data/icechunk/virtual_ref_uncompressed}/my_array/c/0",
        "CHUNK_INFO",
    ) == {"FILENAME": "/vsis3/cdn.proj.org/test_dummy/foo", "OFFSET": "0", "SIZE": "1"}

    with pytest.raises(
        Exception,
        match="Access to non-network chunk location 'data/byte.tif' disabled by default",
    ):
        gdal.GetFileMetadata(
            "/vsiicechunk/{data/icechunk/test_icechunk_manifest_chunk_ref_invalid_location}/my_array/c/0",
            "CHUNK_INFO",
        )


def test_icechunk_multi_threaded_access_to_vsiicechunk():

    error = [False]

    for i in range(1000):

        gdal.ClearMemoryCaches()

        def check():
            try:
                assert gdal.GetFileMetadata(
                    "/vsiicechunk/{data/icechunk/sparse}/my_array/c/0", "CHUNK_INFO"
                ) == {"BASE64": "ChQ=", "SIZE": "2"}
                assert gdal.GetFileMetadata(
                    "/vsiicechunk/{data/icechunk/virtual_ref_uncompressed}/my_array/c/0",
                    "CHUNK_INFO",
                ) == {
                    "FILENAME": "/vsis3/cdn.proj.org/test_dummy/foo",
                    "OFFSET": "0",
                    "SIZE": "1",
                }

            except Exception:
                error[0] = True
                raise

        threads = [threading.Thread(target=check) for i in range(4)]
        for t in threads:
            t.start()
        for t in threads:
            t.join()

        assert not error[0]


def test_icechunk_clunky_scalar_array(tmp_path):

    # Test for weird configuration of scalar arrays such as the
    # "/combined/ILTS_PIK_SICOPOLIS1/ctrl_proj_std/crs" array of
    # /vsis3/us-west-2.opendata.source.coop/englacial/ismip6/icechunk-ais

    rootdirname = Path("data/icechunk/test_icechunk_clunky_scalar_array")

    if FORCE_REGENERATE or not os.path.exists(rootdirname):

        chunk_ref_manifest_id = [2] * 12
        manifest = {
            "id": {"bytes": chunk_ref_manifest_id},
            "arrays": [
                {
                    "node_id": {"bytes": [0] * 8},
                    "refs": [
                        {
                            "index": [0],
                            "inline": [0],
                        },
                    ],
                },
            ],
        }

        manifest_filename = "081040G2081040G20810"
        manifest_file_size = _create_manifest_file(
            tmp_path, rootdirname, manifest_filename, manifest
        )

        manifest_num_chunk_refs = 1

        import json

        _create_repo_with_single_snapshot(tmp_path, rootdirname)

        snapshot = {
            "id": {"bytes": [0] * 12},
            "nodes": [
                {
                    "id": {"bytes": [0] * 8},
                    "path": "/my_array",
                    "user_data": list(
                        json.dumps(
                            {
                                "zarr_format": 3,
                                "node_type": "array",
                                "shape": [],
                                "data_type": "uint8",
                                "chunk_key_encoding": {
                                    "name": "default",
                                    "configuration": {"separator": "/"},
                                },
                                "chunk_grid": {
                                    "name": "regular",
                                    "configuration": {"chunk_shape": []},
                                },
                                "fill_value": 0,
                                "codecs": [
                                    {
                                        "name": "bytes",
                                        "configuration": {"endian": "little"},
                                    }
                                ],
                                "attributes": {},
                                "dimension_names": [],
                            }
                        ).encode("utf-8")
                    ),
                    "node_data_type": "Array",
                    "node_data": {
                        "shape": [],
                        "manifests": [
                            {
                                "object_id": {"bytes": chunk_ref_manifest_id},
                                "extents": [{"from": 0, "to": 1}],
                            },
                        ],
                        "shape_v2": [{"array_length": 1, "num_chunks": 1}],
                    },
                },
                {
                    "id": {"bytes": [0] * 8},
                    "path": "/",
                    "user_data": list(
                        json.dumps({"zarr_format": 3, "node_type": "group"}).encode(
                            "utf-8"
                        )
                    ),
                    "node_data_type": "Group",
                    "node_data": {},
                },
            ],
            "message": "my commit",
            "metadata": [],
            "manifest_files": [],
            "manifest_files_v2": [
                {
                    "id": {"bytes": [2] * 12},
                    "size_bytes": manifest_file_size,
                    "num_chunk_refs": manifest_num_chunk_refs,
                },
            ],
        }

        _create_simple_repo_and_snapshot_files(
            tmp_path,
            rootdirname,
            manifest_file_size=manifest_file_size,
            manifest_num_chunk_refs=manifest_num_chunk_refs,
        )

        _create_snapshot_file(tmp_path, rootdirname, "00000000000000000000", snapshot)

        if not os.path.exists(rootdirname / "transactions"):
            os.mkdir(rootdirname / "transactions", 0o755)
        open(rootdirname / "transactions" / "_dummy_file_", "wb").close()

    ds = gdal.OpenEx(rootdirname, gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    ar = rg.OpenMDArray("my_array")
    assert ar
    ar.Read()


@pytest.mark.require_curl()
@pytest.mark.network
def test_icechunk_remote_test_dataset_native_icechunk_v2_GLAD_LCLU():

    # Cf https://icechunk.io/en/stable/sample-datasets/#weatherbench2-era5-native-icechunk-v2

    with gdal.config_option("AWS_NO_SIGN_REQUEST", "YES"):

        url = "/vsis3/icechunk-public-data/v1/glad"

        if gdal.VSIStatL(f"{url}/repo") is None:
            pytest.skip(f"{url}/repo not accessible")

        ds = gdal.OpenEx(url, gdal.OF_MULTIDIM_RASTER)
        assert "lclu" in ds.GetRootGroup().GetMDArrayNames()
        ar = ds.GetRootGroup().OpenMDArray("lclu")
        assert struct.unpack(
            "B", ar.Read(array_start_idx=[0, 500, 500], count=[1, 1, 1])
        ) == (255,)

        # Check that we don't hit flatbuffers default limitation to 1 million
        # tables in verification code. See https://github.com/OSGeo/gdal/issues/14830
        assert (
            ar.Read(array_start_idx=[0, 280000, 720000], count=[1, 16, 16]) is not None
        )


@pytest.mark.require_curl()
@pytest.mark.network
def test_icechunk_remote_test_dataset_native_icechunk_v1_NOAA_GFS():

    # Cf https://icechunk.io/en/stable/sample-datasets/#noaa-gfs-archive-native-icechunk-v1

    with gdal.config_option("AWS_NO_SIGN_REQUEST", "YES"):

        url = "/vsis3/dynamical-noaa-gfs/noaa-gfs-analysis/v0.1.0.icechunk"

        if gdal.VSIStatL(f"{url}/repo") is None:
            pytest.skip(f"{url}/repo not accessible")

        ds = gdal.OpenEx(url, gdal.OF_MULTIDIM_RASTER)
        assert (
            "categorical_freezing_rain_surface" in ds.GetRootGroup().GetMDArrayNames()
        )
        ar = ds.GetRootGroup().OpenMDArray("categorical_freezing_rain_surface")
        assert struct.unpack(
            "f", ar.Read(array_start_idx=[10, 500, 500], count=[1, 1, 1])
        ) == (0,)


def test_icechunk_tag_selection():

    dirname = "data/icechunk/tag_selection"
    if FORCE_REGENERATE or not os.path.exists(dirname):

        import icechunk as ic
        import zarr

        repo = ic.Repository.create(ic.local_filesystem_storage(dirname))
        s = repo.writable_session("main")
        g = zarr.group(s.store)
        a = g.create_array(
            "my_array", shape=(4,), dtype="int32", chunks=(4,), dimension_names=["x"]
        )
        a[:] = [10, 11, 12, 13]
        repo.create_tag("v1", snapshot_id=s.commit("c1"))
        s = repo.writable_session("main")
        zarr.open_group(s.store)["my_array"][:] = [30, 31, 32, 33]
        s.commit("c2")

    ds = gdal.OpenEx(f"ICECHUNK:{dirname}?tag=v1", gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    ar = rg.OpenMDArray("my_array")
    assert struct.unpack("i" * 4, ar.Read()) == (10, 11, 12, 13)


@pytest.mark.require_curl()
@pytest.mark.network
def test_icechunk_remote_missing_virtual_ref():

    dirname = "data/icechunk/missing_virtual_ref"
    if FORCE_REGENERATE or not os.path.exists(dirname):

        import icechunk as ic
        import zarr

        pfx = "s3://icechunk-public-data/"
        cfg = ic.RepositoryConfig.default()
        cfg.set_virtual_chunk_container(
            ic.VirtualChunkContainer(
                pfx, ic.s3_store(region="us-east-1", anonymous=True)
            )
        )
        repo = ic.Repository.create(
            ic.local_filesystem_storage(dirname),
            config=cfg,
            authorize_virtual_chunk_access={pfx: None},
        )
        s = repo.writable_session("main")
        g = zarr.group(s.store)
        g.create_array(
            "my_array", shape=(4,), dtype="int32", chunks=(4,), dimension_names=["x"]
        )
        s.store.set_virtual_ref(
            "my_array/c/0", pfx + "does-not-exist", offset=0, length=16
        )
        s.commit("c")

    ds = gdal.OpenEx(dirname, gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    ar = rg.OpenMDArray("my_array")
    with pytest.raises(
        Exception, match="Cannot open /vsis3/icechunk-public-data/does-not-exist"
    ):
        ar.Read()
