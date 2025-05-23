#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test Python threading
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2016, Even Rouault <even dot rouault at spatialys dot com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import shutil
import threading

import gdaltest
import pytest

from osgeo import gdal


def my_error_handler(err_type, err_no, err_msg):
    # pylint: disable=unused-argument
    pass


def thread_test_1_worker(args_dict):
    for i in range(1000):
        ds = gdal.Open("data/byte.tif")
        if (i % 2) == 0:
            if ds.GetRasterBand(1).Checksum() != 4672:
                args_dict["ret"] = False
        else:
            ds.GetRasterBand(1).ReadRaster()
    for i in range(1000):
        with gdaltest.disable_exceptions(), gdaltest.error_handler():
            gdal.Open("i_dont_exist")


def test_thread_test_1():

    threads = []
    args_array = []
    for i in range(4):
        args_dict = {"ret": True}
        t = threading.Thread(target=thread_test_1_worker, args=(args_dict,))
        args_array.append(args_dict)
        threads.append(t)
        t.start()

    ret = True
    for i in range(4):
        threads[i].join()
        if not args_array[i]:
            ret = False

    assert ret


def launch_threads(get_band, expected_cs, on_mask_band=False):
    res = [True]

    def verify_checksum():
        for i in range(1000):
            got_cs = get_band().Checksum()
            if got_cs != expected_cs:
                res[0] = False
                assert False, (got_cs, expected_cs)

    threads = [threading.Thread(target=verify_checksum) for i in range(2)]
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    assert res[0]


def test_thread_safe_open():

    ds = gdal.OpenEx("data/byte.tif", gdal.OF_RASTER | gdal.OF_THREAD_SAFE)
    assert ds.IsThreadSafe(gdal.OF_RASTER)
    assert not ds.IsThreadSafe(gdal.OF_RASTER | gdal.OF_UPDATE)

    def get_band():
        return ds.GetRasterBand(1)

    launch_threads(get_band, 4672)

    # Check that GetThreadSafeDataset() on an already thread-safe dataset
    # return itself.
    ref_count_before = ds.GetRefCount()
    thread_safe_ds = ds.GetThreadSafeDataset(gdal.OF_RASTER)
    assert ds.GetRefCount() == ref_count_before + 1
    assert thread_safe_ds.GetRefCount() == ref_count_before + 1
    del thread_safe_ds
    assert ds.GetRefCount() == ref_count_before


def test_thread_safe_create():

    ds = gdal.OpenEx("data/byte.tif", gdal.OF_RASTER)
    assert not ds.IsThreadSafe(gdal.OF_RASTER)
    assert ds.GetRefCount() == 1
    thread_safe_ds = ds.GetThreadSafeDataset(gdal.OF_RASTER)
    assert thread_safe_ds.IsThreadSafe(gdal.OF_RASTER)
    assert ds.GetRefCount() == 2
    del ds

    def get_band():
        return thread_safe_ds.GetRasterBand(1)

    launch_threads(get_band, 4672)


def test_thread_safe_create_close_src_ds():

    ds = gdal.OpenEx("data/byte.tif", gdal.OF_RASTER)
    thread_safe_ds = ds.GetThreadSafeDataset(gdal.OF_RASTER)
    ds.Close()
    with pytest.raises(Exception):
        thread_safe_ds.RasterCount


def test_thread_safe_src_cannot_be_reopened(tmp_vsimem):

    tmpfilename = str(tmp_vsimem / "byte.tif")
    gdal.Translate(tmpfilename, "data/byte.tif")

    with gdal.OpenEx(tmpfilename, gdal.OF_RASTER | gdal.OF_THREAD_SAFE) as ds:
        gdal.Unlink(tmpfilename)
        with pytest.raises(Exception):
            ds.GetRasterBand(1).Checksum()


@pytest.mark.parametrize(
    "flag", [gdal.OF_UPDATE, gdal.OF_VECTOR, gdal.OF_MULTIDIM_RASTER, gdal.OF_GNM]
)
def test_thread_safe_incompatible_open_flags(flag):
    with pytest.raises(Exception, match="mutually exclusive"):
        gdal.OpenEx("data/byte.tif", gdal.OF_THREAD_SAFE | flag)


def test_thread_safe_src_alter_after_opening(tmp_vsimem):

    tmpfilename = str(tmp_vsimem / "byte.tif")

    gdal.Translate(tmpfilename, "data/byte.tif")
    with gdal.OpenEx(tmpfilename, gdal.OF_RASTER | gdal.OF_THREAD_SAFE) as ds:
        gdal.GetDriverByName("GTiff").Create(
            tmpfilename, ds.RasterXSize + 1, ds.RasterYSize, ds.RasterCount
        )
        with pytest.raises(Exception):
            ds.GetRasterBand(1).Checksum()

    gdal.Translate(tmpfilename, "data/byte.tif")
    with gdal.OpenEx(tmpfilename, gdal.OF_RASTER | gdal.OF_THREAD_SAFE) as ds:
        gdal.GetDriverByName("GTiff").Create(
            tmpfilename, ds.RasterXSize, ds.RasterYSize + 1, ds.RasterCount
        )
        with pytest.raises(Exception):
            ds.GetRasterBand(1).Checksum()

    gdal.Translate(tmpfilename, "data/byte.tif")
    with gdal.OpenEx(tmpfilename, gdal.OF_RASTER | gdal.OF_THREAD_SAFE) as ds:
        gdal.GetDriverByName("GTiff").Create(
            tmpfilename, ds.RasterXSize, ds.RasterYSize, ds.RasterCount + 1
        )
        with pytest.raises(Exception):
            ds.GetRasterBand(1).Checksum()

    gdal.Translate(tmpfilename, "data/byte.tif")
    with gdal.OpenEx(tmpfilename, gdal.OF_RASTER | gdal.OF_THREAD_SAFE) as ds:
        gdal.GetDriverByName("GTiff").Create(
            tmpfilename, ds.RasterXSize, ds.RasterYSize, ds.RasterCount, gdal.GDT_Int16
        )
        with pytest.raises(Exception):
            ds.GetRasterBand(1).Checksum()

    gdal.Translate(tmpfilename, "data/byte.tif")
    with gdal.OpenEx(tmpfilename, gdal.OF_RASTER | gdal.OF_THREAD_SAFE) as ds:
        gdal.GetDriverByName("GTiff").Create(
            tmpfilename,
            ds.RasterXSize,
            ds.RasterYSize,
            ds.RasterCount,
            options=["TILED=YES"],
        )
        with pytest.raises(Exception):
            ds.GetRasterBand(1).Checksum()

    with gdal.Translate(tmpfilename, "data/byte.tif") as ds:
        ds.BuildOverviews("NEAR", [2])
    with gdal.OpenEx(tmpfilename, gdal.OF_RASTER | gdal.OF_THREAD_SAFE) as ds:
        ds.GetRasterBand(1).GetOverviewCount()
        gdal.GetDriverByName("GTiff").Create(
            tmpfilename, ds.RasterXSize, ds.RasterYSize, ds.RasterCount
        )
        with pytest.raises(Exception):
            ds.GetRasterBand(1).GetOverview(0).Checksum()


def test_thread_safe_mask_band():

    with gdal.Open("data/stefan_full_rgba.tif") as src_ds:
        expected_cs = src_ds.GetRasterBand(1).GetMaskBand().Checksum()

    with gdal.OpenEx(
        "data/stefan_full_rgba.tif", gdal.OF_RASTER | gdal.OF_THREAD_SAFE
    ) as ds:

        def get_band():
            return ds.GetRasterBand(1).GetMaskBand()

        launch_threads(get_band, expected_cs)

    with gdal.OpenEx(
        "data/stefan_full_rgba.tif", gdal.OF_RASTER | gdal.OF_THREAD_SAFE
    ) as ds:
        band = ds.GetRasterBand(1).GetMaskBand()

        def get_band():
            return band

        launch_threads(get_band, expected_cs)


def test_thread_safe_mask_of_mask_band():

    with gdal.Open("data/stefan_full_rgba.tif") as src_ds:
        expected_cs = src_ds.GetRasterBand(1).GetMaskBand().GetMaskBand().Checksum()

    with gdal.OpenEx(
        "data/stefan_full_rgba.tif", gdal.OF_RASTER | gdal.OF_THREAD_SAFE
    ) as ds:

        def get_band():
            return ds.GetRasterBand(1).GetMaskBand().GetMaskBand()

        launch_threads(get_band, expected_cs)


def test_thread_safe_mask_band_implicit_mem_ds():

    with gdal.Open("data/stefan_full_rgba.tif") as src_ds:
        ds = gdal.GetDriverByName("MEM").CreateCopy("", src_ds)
        expected_cs = src_ds.GetRasterBand(1).GetMaskBand().Checksum()
        assert ds.GetRasterBand(1).GetMaskBand().Checksum() == expected_cs

    ds = ds.GetThreadSafeDataset(gdal.OF_RASTER)

    def get_band():
        return ds.GetRasterBand(1).GetMaskBand()

    launch_threads(get_band, expected_cs)


def test_thread_safe_mask_band_explicit_mem_ds():

    with gdal.Open("data/stefan_full_rgba.tif") as src_ds:
        ds = gdal.GetDriverByName("MEM").Create(
            "", src_ds.RasterXSize, src_ds.RasterYSize, 1
        )
        ds.GetRasterBand(1).CreateMaskBand(gdal.GMF_PER_DATASET)
        ds.GetRasterBand(1).GetMaskBand().WriteRaster(
            0,
            0,
            src_ds.RasterXSize,
            src_ds.RasterYSize,
            src_ds.GetRasterBand(1).GetMaskBand().ReadRaster(),
        )

        expected_cs = src_ds.GetRasterBand(1).GetMaskBand().Checksum()
        assert ds.GetRasterBand(1).GetMaskBand().Checksum() == expected_cs

    ds = ds.GetThreadSafeDataset(gdal.OF_RASTER)

    def get_band():
        return ds.GetRasterBand(1).GetMaskBand()

    launch_threads(get_band, expected_cs)


def test_thread_safe_overview(tmp_path):

    tmpfilename = str(tmp_path / "byte.tif")
    shutil.copy("data/byte.tif", tmpfilename)
    with gdal.Open(tmpfilename, gdal.GA_Update) as ds:
        ds.BuildOverviews("NEAR", [2])
        expected_cs = ds.GetRasterBand(1).GetOverview(0).Checksum()

    with gdal.OpenEx(tmpfilename, gdal.OF_RASTER | gdal.OF_THREAD_SAFE) as ds:
        assert ds.GetRasterBand(1).GetOverviewCount() == 1
        assert ds.GetRasterBand(1).GetOverview(-1) is None
        assert ds.GetRasterBand(1).GetOverview(1) is None

        def get_band():
            return ds.GetRasterBand(1).GetOverview(0)

        launch_threads(get_band, expected_cs)

    with gdal.OpenEx(tmpfilename, gdal.OF_RASTER | gdal.OF_THREAD_SAFE) as ds:
        band = ds.GetRasterBand(1).GetOverview(0)

        def get_band():
            return band

        launch_threads(get_band, expected_cs)


def test_thread_safe_overview_mem_ds():

    with gdal.Open("data/byte.tif") as src_ds:
        ds = gdal.GetDriverByName("MEM").CreateCopy("", src_ds)
        ds.BuildOverviews("NEAR", [2])
        expected_cs = ds.GetRasterBand(1).GetOverview(0).Checksum()

    ds = ds.GetThreadSafeDataset(gdal.OF_RASTER)

    assert ds.GetRasterBand(1).GetOverviewCount() == 1

    def get_band():
        return ds.GetRasterBand(1).GetOverview(0)

    launch_threads(get_band, expected_cs)

    def get_band():
        return ds.GetRasterBand(1).GetSampleOverview(1)

    launch_threads(get_band, expected_cs)

    band = ds.GetRasterBand(1).GetOverview(0)

    def get_band():
        return band

    launch_threads(get_band, expected_cs)


def test_thread_safe_open_options(tmp_path):

    tmpfilename = str(tmp_path / "byte.tif")
    shutil.copy("data/byte.tif", tmpfilename)
    with gdal.Open(tmpfilename, gdal.GA_Update) as ds:
        ds.BuildOverviews("NEAR", [2])
        expected_cs = ds.GetRasterBand(1).GetOverview(0).Checksum()

    with gdal.OpenEx(
        tmpfilename,
        gdal.OF_RASTER | gdal.OF_THREAD_SAFE,
        open_options=["OVERVIEW_LEVEL=0"],
    ) as ds:

        def get_band():
            return ds.GetRasterBand(1)

        launch_threads(get_band, expected_cs)


@pytest.mark.require_driver("HDF5")
@pytest.mark.require_driver("netCDF")
def test_thread_safe_reuse_same_driver_as_prototype():
    """Checks that thread-safe mode honours the opening driver"""

    with gdal.OpenEx(
        "../gdrivers/data/netcdf/byte_hdf5_starting_at_offset_1024.nc",
        gdal.OF_RASTER | gdal.OF_THREAD_SAFE,
        allowed_drivers=["HDF5"],
    ) as ds:

        ret = [False]

        def thread_func():
            assert ds.GetMetadataItem("_NCProperties") is not None
            ret[0] = True

        t = threading.Thread(target=thread_func)
        t.start()
        t.join()
        assert ret[0]


def test_thread_safe_no_rat():

    ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    ds = ds.GetThreadSafeDataset(gdal.OF_RASTER)
    assert ds.GetRasterBand(1).GetDefaultRAT() is None


def test_thread_safe_rat():

    ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    ds.GetRasterBand(1).SetDefaultRAT(gdal.RasterAttributeTable())
    ds = ds.GetThreadSafeDataset(gdal.OF_RASTER)
    assert ds.GetRasterBand(1).GetDefaultRAT() is not None


@pytest.mark.require_driver("HFA")
def test_thread_safe_unsupported_rat():

    with gdal.OpenEx(
        "../gdrivers/data/hfa/87test.img", gdal.OF_RASTER | gdal.OF_THREAD_SAFE
    ) as ds:
        with pytest.raises(
            Exception,
            match="not supporting a non-GDALDefaultRasterAttributeTable implementation",
        ):
            ds.GetRasterBand(1).GetDefaultRAT()


def test_thread_safe_many_datasets():

    tab_ds = [
        gdal.OpenEx(
            "data/byte.tif" if (i % 3) < 2 else "data/utmsmall.tif",
            gdal.OF_RASTER | gdal.OF_THREAD_SAFE,
        )
        for i in range(100)
    ]

    res = [True]

    def check():
        for _ in range(10):
            for i, ds in enumerate(tab_ds):
                if ds.GetRasterBand(1).Checksum() != (4672 if (i % 3) < 2 else 50054):
                    res[0] = False

    threads = [threading.Thread(target=check) for i in range(2)]
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    assert res[0]


def test_thread_safe_BeginAsyncReader():

    with gdal.OpenEx("data/byte.tif", gdal.OF_RASTER | gdal.OF_THREAD_SAFE) as ds:
        with pytest.raises(Exception, match="not supported"):
            ds.BeginAsyncReader(0, 0, ds.RasterXSize, ds.RasterYSize)


def test_thread_safe_GetVirtualMem():

    pytest.importorskip("numpy")
    gdaltest.importorskip_gdal_array()

    with gdal.OpenEx("data/byte.tif", gdal.OF_RASTER | gdal.OF_THREAD_SAFE) as ds:
        with pytest.raises(Exception, match="not supported"):
            ds.GetRasterBand(1).GetVirtualMemAutoArray(gdal.GF_Read)


def test_thread_safe_GetMetadadata(tmp_vsimem):

    filename = str(tmp_vsimem / "test.tif")
    with gdal.GetDriverByName("GTiff").Create(filename, 1, 1) as ds:
        ds.SetMetadataItem("foo", "bar")
        ds.GetRasterBand(1).SetMetadataItem("bar", "baz")

    with gdal.OpenEx(filename, gdal.OF_RASTER | gdal.OF_THREAD_SAFE) as ds:
        assert ds.GetMetadataItem("foo") == "bar"
        assert ds.GetMetadataItem("not existing") is None
        assert ds.GetMetadata() == {"foo": "bar"}
        assert ds.GetMetadata("not existing") == {}
        assert ds.GetRasterBand(1).GetMetadataItem("bar") == "baz"
        assert ds.GetRasterBand(1).GetMetadataItem("not existing") is None
        assert ds.GetRasterBand(1).GetMetadata() == {"bar": "baz"}
        assert ds.GetRasterBand(1).GetMetadata("not existing") == {}


def test_thread_safe_GetUnitType(tmp_vsimem):

    filename = str(tmp_vsimem / "test.tif")
    with gdal.GetDriverByName("GTiff").Create(filename, 1, 1) as ds:
        ds.GetRasterBand(1).SetUnitType("foo")

    with gdal.OpenEx(filename, gdal.OF_RASTER | gdal.OF_THREAD_SAFE) as ds:
        assert ds.GetRasterBand(1).GetUnitType() == "foo"


def test_thread_safe_GetColorTable(tmp_vsimem):

    filename = str(tmp_vsimem / "test.tif")
    with gdal.GetDriverByName("GTiff").Create(filename, 1, 1) as ds:
        ct = gdal.ColorTable()
        ct.SetColorEntry(0, (1, 2, 3, 255))
        ds.GetRasterBand(1).SetColorTable(ct)

    with gdal.OpenEx(filename, gdal.OF_RASTER | gdal.OF_THREAD_SAFE) as ds:
        res = [None]

        def thread_job():
            res[0] = ds.GetRasterBand(1).GetColorTable()

        t = threading.Thread(target=thread_job)
        t.start()
        t.join()
        assert res[0]
        assert res[0].GetColorEntry(0) == (1, 2, 3, 255)
        ct = ds.GetRasterBand(1).GetColorTable()
        assert ct.GetColorEntry(0) == (1, 2, 3, 255)


def test_thread_safe_GetSpatialRef():

    with gdal.OpenEx("data/byte.tif", gdal.OF_RASTER | gdal.OF_THREAD_SAFE) as ds:

        res = [True]

        def check():
            for i in range(100):

                if len(ds.GetGCPs()) != 0:
                    res[0] = False
                    assert False

                if ds.GetGCPSpatialRef():
                    res[0] = False
                    assert False

                if ds.GetGCPProjection():
                    res[0] = False
                    assert False

                srs = ds.GetSpatialRef()
                if not srs:
                    res[0] = False
                    assert False
                if not srs.IsProjected():
                    res[0] = False
                    assert False
                if "NAD27 / UTM zone 11N" not in srs.ExportToWkt():
                    res[0] = False
                    assert False

                if "NAD27 / UTM zone 11N" not in ds.GetProjectionRef():
                    res[0] = False
                    assert False

        threads = [threading.Thread(target=check) for i in range(2)]
        for t in threads:
            t.start()
        for t in threads:
            t.join()
        assert res[0]


def test_thread_safe_GetGCPs():

    with gdal.OpenEx(
        "data/byte_gcp_pixelispoint.tif", gdal.OF_RASTER | gdal.OF_THREAD_SAFE
    ) as ds:

        res = [True]

        def check():
            for i in range(100):

                if len(ds.GetGCPs()) != 4:
                    res[0] = False
                    assert False

                gcp_srs = ds.GetGCPSpatialRef()
                if gcp_srs is None:
                    res[0] = False
                    assert False
                if not gcp_srs.IsGeographic():
                    res[0] = False
                    assert False
                if "unretrievable - using WGS84" not in gcp_srs.ExportToWkt():
                    res[0] = False
                    assert False

                gcp_wkt = ds.GetGCPProjection()
                if not gcp_wkt:
                    res[0] = False
                    assert False
                if "unretrievable - using WGS84" not in gcp_wkt:
                    res[0] = False
                    assert False

                if ds.GetSpatialRef():
                    res[0] = False
                    assert False
                if ds.GetProjectionRef() != "":
                    res[0] = False
                    assert False

        threads = [threading.Thread(target=check) for i in range(2)]
        for t in threads:
            t.start()
        for t in threads:
            t.join()
        assert res[0]
