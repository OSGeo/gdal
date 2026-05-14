#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Various test of GDAL core.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2009-2013, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import datetime
import os
import shutil

import gdaltest
import pytest
from test_py_scripts import run_py_script_as_external_script

from osgeo import gdal, osr


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def module_disable_exceptions():
    with gdaltest.disable_exceptions():
        yield


###############################################################################
# Test that the constructor of GDALDataset() behaves well with a big number of
# opened/created datasets


def test_misc_1():

    tab_ds = [None] * 5000
    drv = gdal.GetDriverByName("MEM")
    for i, _ in enumerate(tab_ds):
        name = "mem_%d" % i
        tab_ds[i] = drv.Create(name, 1, 1, 1)
        assert tab_ds[i] is not None


###############################################################################
# Test that OpenShared() works as expected by opening a big number of times
# the same dataset with it. If it did not work, that would exhaust the system
# limit of maximum file descriptors opened at the same time


def test_misc_2():

    tab_ds = [None for i in range(5000)]
    for i, _ in enumerate(tab_ds):
        tab_ds[i] = gdal.OpenShared("data/byte.tif")
        assert tab_ds[i] is not None


###############################################################################
# Test OpenShared() with a dataset whose filename != description (#2797)


@pytest.mark.require_driver("PAUX")
def test_misc_3():

    with gdal.quiet_errors():
        ds = gdal.OpenShared("../gdrivers/data/paux/small16.aux")
    ds.GetRasterBand(1).Checksum()
    cache_size = gdal.GetCacheUsed()

    with gdal.quiet_errors():
        ds2 = gdal.OpenShared("../gdrivers/data/paux/small16.aux")
    ds2.GetRasterBand(1).Checksum()
    cache_size2 = gdal.GetCacheUsed()

    if cache_size != cache_size2:
        print("--> OpenShared didn't work as expected")

    ds = None
    ds2 = None


###############################################################################
# Test Create() with invalid arguments


def test_misc_4():

    with gdal.quiet_errors():

        # Test a few invalid argument
        drv = gdal.GetDriverByName("GTiff")
        drv.Create("tmp/foo", 0, 100, 1)
        drv.Create("tmp/foo", 100, 1, 1)
        drv.Create("tmp/foo", 100, 100, -1)
        drv.Delete("tmp/foo")


###############################################################################


def get_filename(drv, dirname):

    filename = "%s/foo" % dirname
    if drv.ShortName == "GTX":
        filename += ".gtx"
    elif drv.ShortName == "RST":
        filename += ".rst"
    elif drv.ShortName == "SAGA":
        filename += ".sdat"
    elif drv.ShortName == "ADRG":
        filename = "%s/ABCDEF01.GEN" % dirname
    elif drv.ShortName == "SRTMHGT":
        filename = "%s/N48E002.HGT" % dirname
    elif drv.ShortName == "ECW":
        filename += ".ecw"
    elif drv.ShortName == "KMLSUPEROVERLAY":
        filename += ".kmz"
    elif drv.ShortName == "RRASTER":
        filename += ".grd"
    elif drv.ShortName == "KEA":
        filename += ".kea"
    elif drv.ShortName == "GPKG":
        filename += ".gpkg"

    return filename


###############################################################################
# Test Create() with various band numbers (including 0) and datatype


def _misc_5_internal(drv, datatype, nBands):

    dirname = "tmp/tmp/tmp_%s_%d_%s" % (
        drv.ShortName,
        nBands,
        gdal.GetDataTypeName(datatype),
    )
    # print('drv = %s, nBands = %d, datatype = %s' % (drv.ShortName, nBands, gdal.GetDataTypeName(datatype)))
    try:
        os.mkdir(dirname)
    except OSError:
        try:
            os.stat(dirname)
            # Hum the directory already exists... Not expected, but let's try to go on
        except OSError:
            pytest.fail(
                "Cannot create %s for drv = %s, nBands = %d, datatype = %s"
                % (dirname, drv.ShortName, nBands, gdal.GetDataTypeName(datatype))
            )

    filename = get_filename(drv, dirname)
    ds = drv.Create(filename, 100, 100, nBands, datatype)
    if ds is not None and not (drv.ShortName == "GPKG" and nBands == 0):
        set_gt = (2, 1.0 / 10, 0, 49, 0, -1.0 / 10)
        ds.SetGeoTransform(set_gt)
        ds.SetProjection(
            'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563]],PRIMEM["Greenwich",0],UNIT["degree",0.01745329251994328]]'
        )

        # PNM and MFF have no SetGeoTransform() method implemented
        if drv.ShortName not in ["PNM", "MFF", "NULL"]:
            got_gt = ds.GetGeoTransform()
            for i in range(6):
                assert got_gt[i] == pytest.approx(set_gt[i], abs=1e-10), (
                    "Did not get expected GT for drv = %s, nBands = %d, datatype = %s"
                    % (drv.ShortName, nBands, gdal.GetDataTypeName(datatype))
                )

        # if ds.RasterCount > 0:
        #    ds.GetRasterBand(1).Fill(255)
    ds = None
    ds = gdal.Open(filename)
    if ds is None:
        # reason = 'Cannot reopen %s for drv = %s, nBands = %d, datatype = %s' % (dirname, drv.ShortName, nBands, gdal.GetDataTypeName(datatype))
        # gdaltest.post_reason(reason)
        # TODO: Why not return -1?
        pass
    elif ds.RasterCount and drv.ShortName not in ["GSBG", "GS7BG", "NWT_GRD", "netCDF"]:
        creation_data_types = drv.GetMetadataItem(gdal.DMD_CREATIONDATATYPES)
        if creation_data_types and gdal.GetDataTypeName(
            datatype
        ) in creation_data_types.split(" "):
            assert ds.GetRasterBand(1).DataType == datatype, (
                dirname,
                drv.ShortName,
                nBands,
                gdal.GetDataTypeName(datatype),
            )
    ds = None

    try:
        shutil.rmtree(dirname)
    except OSError:
        pytest.fail(
            "Cannot remove %s for drv = %s, nBands = %d, datatype = %s"
            % (dirname, drv.ShortName, nBands, gdal.GetDataTypeName(datatype))
        )


def test_misc_5():

    with gdal.quiet_errors():

        try:
            shutil.rmtree("tmp/tmp")
        except OSError:
            pass

        try:
            os.mkdir("tmp/tmp")
        except OSError:
            try:
                os.stat("tmp/tmp")
                # Hum the directory already exists... Not expected, but let's try to go on
            except OSError:
                pytest.fail("Cannot create tmp/tmp")

        # This is to speed-up the runtime of tests on EXT4 filesystems
        # Do not use this for production environment if you care about data safety
        # w.r.t system/OS crashes, unless you know what you are doing.
        with gdal.config_option("OGR_SQLITE_SYNCHRONOUS", "OFF"):

            # Test Create() with various band numbers, including 0
            for i in range(gdal.GetDriverCount()):
                drv = gdal.GetDriver(i)
                md = drv.GetMetadata()
                if drv.ShortName == "PDF":
                    # PDF Create() is vector-only
                    continue
                if drv.ShortName == "MBTiles":
                    # MBTiles only support some precise resolutions
                    continue
                if "DCAP_CREATE" in md and "DCAP_RASTER" in md:
                    datatype = gdal.GDT_UInt8
                    for nBands in range(6):
                        _misc_5_internal(drv, datatype, nBands)

                    for nBands in [1, 3]:
                        for datatype in (
                            gdal.GDT_Int8,
                            gdal.GDT_UInt16,
                            gdal.GDT_Int16,
                            gdal.GDT_UInt32,
                            gdal.GDT_Int32,
                            gdal.GDT_UInt64,
                            gdal.GDT_Int64,
                            gdal.GDT_Float32,
                            gdal.GDT_Float64,
                            gdal.GDT_CInt16,
                            gdal.GDT_CInt32,
                            gdal.GDT_CFloat32,
                            gdal.GDT_CFloat64,
                        ):
                            _misc_5_internal(drv, datatype, nBands)


###############################################################################
class misc_6_interrupt_callback_class:
    def __init__(self):
        pass

    def cbk(self, pct, message, user_data):
        # pylint: disable=unused-argument
        return pct <= 0.5


###############################################################################
# Test CreateCopy() with a source dataset with various band numbers (including 0) and datatype


def misc_6_internal(datatype, nBands, setDriversDone):

    ds = gdal.GetDriverByName("MEM").Create("", 10, 10, nBands, datatype)
    if nBands > 0:
        ds.GetRasterBand(1).Fill(255)
    ds.SetGeoTransform([2, 1.0 / 10, 0, 49, 0, -1.0 / 10])
    ds.SetProjection(
        'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563]],PRIMEM["Greenwich",0],UNIT["degree",0.01745329251994328]]'
    )
    ds.SetMetadata(["a"])

    for i in range(gdal.GetDriverCount()):
        drv = gdal.GetDriver(i)
        md = drv.GetMetadata()
        if ("DCAP_CREATECOPY" in md or "DCAP_CREATE" in md) and "DCAP_RASTER" in md:
            dirname = "tmp/tmp/tmp_%s_%d_%s" % (
                drv.ShortName,
                nBands,
                gdal.GetDataTypeName(datatype),
            )
            try:
                os.mkdir(dirname)
            except OSError:
                try:
                    os.stat(dirname)
                    # Hum the directory already exists... Not expected, but let's try to go on
                except OSError:
                    reason = (
                        "Cannot create %s before drv = %s, nBands = %d, datatype = %s"
                        % (
                            dirname,
                            drv.ShortName,
                            nBands,
                            gdal.GetDataTypeName(datatype),
                        )
                    )
                    pytest.fail(reason)

            filename = get_filename(drv, dirname)

            dst_ds = drv.CreateCopy(filename, ds)
            has_succeeded = dst_ds is not None
            if dst_ds:
                # check that domain == None doesn't crash
                dst_ds.GetMetadata(None)
                dst_ds.GetMetadataItem("", None)
            dst_ds = None

            size = 0
            stat = gdal.VSIStatL(filename)
            if stat is not None:
                size = stat.size

            try:
                shutil.rmtree(dirname)
            except OSError:
                reason = (
                    "Cannot remove %s after drv = %s, nBands = %d, datatype = %s"
                    % (
                        dirname,
                        drv.ShortName,
                        nBands,
                        gdal.GetDataTypeName(datatype),
                    )
                )
                pytest.fail(reason)

            if has_succeeded and drv.ShortName not in setDriversDone and nBands > 0:
                setDriversDone.add(drv.ShortName)

                # The first list of drivers fail to detect short writing
                # The second one is because they are verbose in stderr
                if (
                    "DCAP_VIRTUALIO" in md
                    and size != 0
                    and drv.ShortName
                    not in [
                        "JPEG2000",
                        "KMLSUPEROVERLAY",
                        "HF2",
                        "ZMap",
                        "DDS",
                        "TileDB",
                    ]
                    and drv.ShortName not in ["GIF", "JP2ECW", "JP2Lura"]
                ):

                    for j in range(10):
                        truncated_size = (size * j) / 10
                        vsimem_filename = (
                            "/vsimem/test_truncate/||maxlength=%d||" % truncated_size
                        ) + get_filename(drv, "")[1:]
                        # print('drv = %s, nBands = %d, datatype = %s, truncated_size = %d' % (drv.ShortName, nBands, gdal.GetDataTypeName(datatype), truncated_size))
                        dst_ds = drv.CreateCopy(vsimem_filename, ds)
                        error_detected = False
                        if dst_ds is None:
                            error_detected = True
                        else:
                            gdal.ErrorReset()
                            dst_ds = None
                            if gdal.GetLastErrorMsg() != "":
                                error_detected = True
                        if not error_detected:
                            msg = (
                                "write error not detected with with drv = %s, nBands = %d, datatype = %s, truncated_size = %d"
                                % (
                                    drv.ShortName,
                                    nBands,
                                    gdal.GetDataTypeName(datatype),
                                    truncated_size,
                                )
                            )
                            print(msg)

                        fl = gdal.ReadDirRecursive("/vsimem/test_truncate")
                        if fl is not None:
                            for myf in fl:
                                gdal.Unlink("/vsimem/test_truncate/" + myf)
                            fl = gdal.ReadDirRecursive("/vsimem/test_truncate")
                            if fl is not None:
                                print(fl)

                if drv.ShortName not in [
                    "ECW",
                    "JP2ECW",
                    "VRT",
                    "XPM",
                    "JPEG2000",
                    "FIT",
                    "RST",
                    "INGR",
                    "USGSDEM",
                    "KMLSUPEROVERLAY",
                    "GMT",
                    "NULL",
                ]:
                    dst_ds = drv.CreateCopy(
                        filename, ds, callback=misc_6_interrupt_callback_class().cbk
                    )
                    if dst_ds is not None:
                        dst_ds = None

                        try:
                            shutil.rmtree(dirname)
                        except OSError:
                            pass

                        pytest.fail(
                            "interruption did not work with drv = %s, nBands = %d, datatype = %s"
                            % (
                                drv.ShortName,
                                nBands,
                                gdal.GetDataTypeName(datatype),
                            )
                        )

                    dst_ds = None

                    try:
                        shutil.rmtree(dirname)
                    except OSError:
                        pass
                    try:
                        os.mkdir(dirname)
                    except OSError:
                        reason = (
                            "Cannot create %s before drv = %s, nBands = %d, datatype = %s"
                            % (
                                dirname,
                                drv.ShortName,
                                nBands,
                                gdal.GetDataTypeName(datatype),
                            )
                        )
                        pytest.fail(reason)

    ds = None


def test_misc_6():

    with gdal.quiet_errors():

        try:
            shutil.rmtree("tmp/tmp")
        except OSError:
            pass

        try:
            os.mkdir("tmp/tmp")
        except OSError:
            try:
                os.stat("tmp/tmp")
                # Hum the directory already exists... Not expected, but let's try to go on
            except OSError:
                pytest.fail("Cannot create tmp/tmp")

        # This is to speed-up the runtime of tests on EXT4 filesystems
        # Do not use this for production environment if you care about data safety
        # w.r.t system/OS crashes, unless you know what you are doing.
        with gdal.config_option("OGR_SQLITE_SYNCHRONOUS", "OFF"):

            datatype = gdal.GDT_UInt8
            setDriversDone = set()
            for nBands in range(6):
                misc_6_internal(datatype, nBands, setDriversDone)

            nBands = 1
            for datatype in (
                gdal.GDT_UInt16,
                gdal.GDT_Int16,
                gdal.GDT_UInt32,
                gdal.GDT_Int32,
                gdal.GDT_Float32,
                gdal.GDT_Float64,
                gdal.GDT_CInt16,
                gdal.GDT_CInt32,
                gdal.GDT_CFloat32,
                gdal.GDT_CFloat64,
            ):
                misc_6_internal(datatype, nBands, setDriversDone)


@pytest.mark.parametrize(
    "driver_name",
    [
        gdal.GetDriver(i).GetName()
        for i in range(gdal.GetDriverCount())
        if "DCAP_UPDATE" in gdal.GetDriver(i).GetMetadata()
        and [
            "DCAP_CREATECOPY" in gdal.GetDriver(i).GetMetadata()
            or "DCAP_CREATE" in gdal.GetDriver(i).GetMetadata()
        ]
        and "DCAP_RASTER" in gdal.GetDriver(i).GetMetadata()
    ],
)
def test_update_metadata(tmp_path, tmp_vsimem, driver_name):
    if driver_name in ("OpenFileGDB"):
        pytest.skip("OpenFileGDB does not support creating raster datasets")

    drv = gdal.GetDriverByName(driver_name)
    if "DCAP_VIRTUALIO" in drv.GetMetadata() and driver_name not in (
        "KEA",
        "netCDF",
        "TileDB",
    ):
        # drivers listed above do not allow writing to /vsimem
        dirname = tmp_vsimem
    else:
        dirname = tmp_path

    filename = get_filename(drv, dirname)

    # create a test dataset that can be updated
    # ECW driver requires at size to be at least 128x128
    with gdal.GetDriverByName("GTiff").Create(
        tmp_vsimem / "in.tif", 128, 128
    ) as src_ds:
        src_ds.GetRasterBand(1).Fill(3)
        src_ds.SetGeoTransform((20, 0.1, 0, 40, 0, -0.1))
        dst_ds = gdal.Translate(
            filename, src_ds, outputSRS="EPSG:4326", format=driver_name
        )
        assert dst_ds
        dst_ds.Close()

    update_ds = gdal.OpenEx(filename, gdal.GA_Update, allowed_drivers=[driver_name])
    assert update_ds

    flags_str = drv.GetMetadataItem(gdal.DMD_UPDATE_ITEMS)
    if "RasterValues" in flags_str:
        assert (
            update_ds.GetRasterBand(1).WriteRaster(
                0, 0, 1, 1, b"\x00", buf_type=gdal.GDT_UInt8
            )
            == gdal.CE_None
        )
    if "GeoTransform" in flags_str and drv.ShortName not in ("netCDF",):
        assert update_ds.SetGeoTransform([0, 1, 0, 0, 0, -1]) == gdal.CE_None
    if "SRS" in flags_str and drv.ShortName not in ("netCDF",):
        srs = osr.SpatialReference()
        srs.SetFromUserInput("WGS84")
        assert update_ds.SetSpatialRef(srs) == gdal.CE_None
    if "NoData" in flags_str:
        assert update_ds.GetRasterBand(1).SetNoDataValue(0) == gdal.CE_None
    if "DatasetMetadata" in flags_str:
        assert update_ds.SetMetadata({"FOO": "BAR"}) == gdal.CE_None
    if "BandMetadata" in flags_str:
        assert update_ds.GetRasterBand(1).SetMetadata({"FOO": "BAR"}) == gdal.CE_None

    update_ds.Close()


###############################################################################
# Test gdal.InvGeoTransform()


def test_misc_7():

    gt = (10, 0.1, 0, 20, 0, -1.0)
    res = gdal.InvGeoTransform(gt)
    expected_inv_gt = (-100.0, 10.0, 0.0, 20.0, 0.0, -1.0)
    for i in range(6):
        assert res[i] == pytest.approx(expected_inv_gt[i], abs=1e-6), res

    gt = (10, 1, 1, 20, 2, 2)
    res = gdal.InvGeoTransform(gt)
    assert not res

    gt = (10, 1e10, 1e10, 20, 2e10, 2e10)
    res = gdal.InvGeoTransform(gt)
    assert not res

    gt = (10, 1e-10, 1e-10, 20, 2e-10, 2e-10)
    res = gdal.InvGeoTransform(gt)
    assert not res

    # Test fix for #1615
    gt = (-2, 1e-8, 1e-9, 52, 1e-9, -1e-8)
    res = gdal.InvGeoTransform(gt)
    expected_inv_gt = (
        -316831683.16831684,
        99009900.990099,
        9900990.099009901,
        5168316831.683168,
        9900990.099009901,
        -99009900.990099,
    )
    for i in range(6):
        assert res[i] == pytest.approx(expected_inv_gt[i], abs=1e-6), res
    res2 = gdal.InvGeoTransform(res)
    for i in range(6):
        assert res2[i] == pytest.approx(gt[i], abs=1e-6), res2


###############################################################################
# Test gdal.ApplyGeoTransform()


def test_misc_8():

    try:
        gdal.ApplyGeoTransform
    except AttributeError:
        pytest.skip()

    gt = (10, 0.1, 0, 20, 0, -1.0)
    res = gdal.ApplyGeoTransform(gt, 10, 1)
    assert res == [11.0, 19.0]


###############################################################################
# Test setting and retrieving > 2 GB values for GDAL max cache (#3689)


def test_misc_9():

    with gdaltest.SetCacheMax(3000000000):
        ret_val = gdal.GetCacheMax()

    assert ret_val == 3000000000, "did not get expected value"


###############################################################################
# Test VSIBufferedReaderHandle (fix done in r21358)


def test_misc_10():

    try:
        os.remove("data/byte.tif.gz.properties")
    except OSError:
        pass

    f = gdal.VSIFOpenL("/vsigzip/./data/byte.tif.gz", "rb")
    gdal.VSIFReadL(1, 1, f)
    gdal.VSIFSeekL(f, 0, 2)
    gdal.VSIFSeekL(f, 0, 0)
    data = gdal.VSIFReadL(1, 4, f)
    gdal.VSIFCloseL(f)

    import struct

    ar = struct.unpack("B" * 4, data)
    assert ar == (73, 73, 42, 0)

    try:
        os.remove("data/byte.tif.gz.properties")
    except OSError:
        pass


###############################################################################
# Test that we can open a symlink whose pointed filename isn't a real
# file, but a filename that GDAL recognizes


def test_misc_11():

    if not gdaltest.support_symlink():
        pytest.skip()

    gdal.Unlink("tmp/symlink.tif")
    os.symlink("GTIFF_DIR:1:data/byte.tif", "tmp/symlink.tif")

    ds = gdal.Open("tmp/symlink.tif")
    if ds is None:
        os.remove("tmp/symlink.tif")
        pytest.fail()
    desc = ds.GetDescription()
    ds = None

    os.remove("tmp/symlink.tif")

    assert desc == "GTIFF_DIR:1:data/byte.tif", "did not get expected description"


###############################################################################
# Test CreateCopy() with a target filename in a non-existing dir


# Started to fail suddenly on May 14th 2025 on this config
@pytest.mark.skipif(
    gdaltest.is_travis_branch("build-windows-conda"), reason="fails for unknown reason"
)
def test_misc_12():

    import test_cli_utilities

    gdal_translate_path = test_cli_utilities.get_gdal_translate_path()

    for i in range(gdal.GetDriverCount()):
        drv = gdal.GetDriver(i)
        md = drv.GetMetadata()
        if ("DCAP_CREATECOPY" in md or "DCAP_CREATE" in md) and "DCAP_RASTER" in md:

            nbands = 1
            if drv.ShortName == "WEBP" or drv.ShortName == "ADRG":
                nbands = 3

            datatype = gdal.GDT_UInt8
            if drv.ShortName == "BT" or drv.ShortName == "BLX":
                datatype = gdal.GDT_Int16
            elif (
                drv.ShortName == "GTX"
                or drv.ShortName == "NTv2"
                or drv.ShortName == "Leveller"
            ):
                datatype = gdal.GDT_Float32

            size = 1201
            if drv.ShortName == "BLX":
                size = 128

            src_ds = gdal.GetDriverByName("GTiff").Create(
                "/vsimem/misc_12_src.tif", size, size, nbands, datatype
            )
            set_gt = (2, 1.0 / size, 0, 49, 0, -1.0 / size)
            src_ds.SetGeoTransform(set_gt)
            src_ds.SetProjection(
                'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563]],PRIMEM["Greenwich",0],UNIT["degree",0.01745329251994328]]'
            )

            # Test to detect crashes
            with gdal.quiet_errors():
                ds = drv.CreateCopy("/nonexistingpath" + get_filename(drv, ""), src_ds)
            if ds is None and gdal.GetLastErrorMsg() == "":
                gdal.Unlink("/vsimem/misc_12_src.tif")
                pytest.fail(
                    "CreateCopy() into non existing dir fails without error message for driver %s"
                    % drv.ShortName
                )
            ds = None

            if gdal_translate_path is not None:
                # Test to detect memleaks
                ds = gdal.GetDriverByName("VRT").CreateCopy("tmp/misc_12.vrt", src_ds)
                out, _ = gdaltest.runexternal_out_and_err(
                    gdal_translate_path
                    + " -of "
                    + drv.ShortName
                    + " tmp/misc_12.vrt /nonexistingpath/"
                    + get_filename(drv, ""),
                    check_memleak=False,
                )
                del ds
                gdal.Unlink("tmp/misc_12.vrt")

                # If DEBUG_VSIMALLOC_STATS is defined, this is an easy way
                # to catch some memory leaks
                if (
                    out.find("VSIMalloc + VSICalloc - VSIFree") != -1
                    and out.find("VSIMalloc + VSICalloc - VSIFree : 0") == -1
                ):
                    if (
                        drv.ShortName == "Rasterlite"
                        and out.find("VSIMalloc + VSICalloc - VSIFree : 1") != -1
                    ):
                        pass
                    else:
                        print("memleak detected for driver %s" % drv.ShortName)

            src_ds = None

            gdal.Unlink("/vsimem/misc_12_src.tif")


###############################################################################
# Test CreateCopy() with incompatible driver types (#5912)


def test_misc_13():

    # Raster-only -> vector-only
    ds = gdal.Open("data/byte.tif")
    with gdal.quiet_errors():
        out_ds = gdal.GetDriverByName("ESRI Shapefile").CreateCopy(
            "/vsimem/out.shp", ds
        )
    assert out_ds is None

    # Raster-only -> vector-only
    ds = gdal.OpenEx("../ogr/data/poly.shp", gdal.OF_VECTOR)
    with gdal.quiet_errors():
        out_ds = gdal.GetDriverByName("GTiff").CreateCopy("/vsimem/out.tif", ds)
    assert out_ds is None


###############################################################################
# Test parsing of CPL_DEBUG and CPL_TIMESTAMP


@pytest.fixture
def debug_output():

    messages = []

    def handle(ecls, ecode, emsg):
        messages.append(emsg)

    def log_message(category, message):
        messages.clear()
        gdal.Debug(category, message)
        return messages[0] if messages else None

    log_message.handle = handle

    with gdaltest.error_handler(handle):
        yield log_message


@pytest.mark.parametrize(
    "booleans",
    [
        ("YES", "NO"),
        ("TRUE", "FALSE"),
        ("ON", "OFF"),
        ("1", "0"),
        (True, False),
        (1, 0),
    ],
    ids=lambda x: f"{type(x[0]).__name__}_" + "_".join(str(y) for y in x),
)
def test_misc_cpl_debug(debug_output, booleans):

    on, off = booleans

    assert debug_output("GDAL", "msg") is None

    with gdal.config_option("CPL_DEBUG", off):
        assert debug_output("GDAL", "msg") is None

    with gdal.config_option("CPL_DEBUG", on):
        assert debug_output("GDAL", "message") == "GDAL: message"

        with gdal.config_option("CPL_TIMESTAMP", off):
            assert debug_output("GDAL", "message") == "GDAL: message"

        with gdal.config_option("CPL_TIMESTAMP", on):
            output = debug_output("GDAL", "message")
            assert str(datetime.datetime.now().year) in output
            assert output.endswith("GDAL: message")


def test_misc_cpl_debug_filtering(debug_output):

    with gdal.config_option("CPL_DEBUG", "GDAL"):
        assert debug_output("GDAL", "msg") == "GDAL: msg"
        assert debug_output("GDAL_WARP", "msg") is None
        assert debug_output("", "msg") == ": msg"

    with gdal.config_option("CPL_DEBUG", "GDAL_WARP_TRANSLATE_ETC"):
        assert debug_output("GDAL", "msg") == "GDAL: msg"
        assert debug_output("TRANSLATE", "msg") == "TRANSLATE: msg"

    with gdal.config_option("CPL_DEBUG", ""):
        assert debug_output("GDAL", "msg") == "GDAL: msg"


###############################################################################
# Test ConfigureLogging()


def test_misc_14():
    import collections
    import logging

    class MockLoggingHandler(logging.Handler):
        def __init__(self, *args, **kwargs):
            super(MockLoggingHandler, self).__init__(*args, **kwargs)
            self.messages = collections.defaultdict(list)

        def emit(self, record):
            self.messages[record.levelname].append(record.getMessage())

    logger = logging.getLogger("gdal_logging_test")
    logger.setLevel(logging.DEBUG)
    logger.propagate = False
    handler = MockLoggingHandler(level=logging.DEBUG)
    logger.addHandler(handler)

    prev_debug = gdal.GetConfigOption("CPL_DEBUG")
    try:
        gdal.ConfigurePythonLogging(logger_name="gdal_logging_test", enable_debug=True)

        assert gdal.GetConfigOption("CPL_DEBUG") == "ON", "should have enabled debug"

        gdal.Debug("test1", "debug1")
        gdal.Error(gdal.CE_Debug, gdal.CPLE_FileIO, "debug2")
        gdal.Error(gdal.CE_None, gdal.CPLE_AppDefined, "info1")
        gdal.Error(gdal.CE_Warning, gdal.CPLE_AssertionFailed, "warning1")
        gdal.Error(gdal.CE_Failure, 99999, "error1")

        expected = {
            "DEBUG": ["test1: debug1", "FileIO: debug2"],
            "INFO": ["AppDefined: info1"],
            "WARNING": ["AssertionFailed: warning1"],
            "ERROR": ["99999: error1"],
        }

        assert handler.messages == expected, "missing log messages"

        gdal.SetErrorHandler("CPLDefaultErrorHandler")
        handler.messages.clear()
        gdal.SetConfigOption("CPL_DEBUG", "OFF")

        gdal.ConfigurePythonLogging(logger_name="gdal_logging_test")

        assert (
            gdal.GetConfigOption("CPL_DEBUG") == "OFF"
        ), "shouldn't have enabled debug"

        # these get suppressed by CPL_DEBUG
        gdal.Debug("test1", "debug3")
        # these don't
        gdal.Error(gdal.CE_Debug, gdal.CPLE_None, "debug4")

        assert handler.messages["DEBUG"] == ["debug4"], "unexpected log messages"

    finally:
        gdal.SetErrorHandler("CPLDefaultErrorHandler")
        gdal.SetConfigOption("CPL_DEBUG", prev_debug)
        logger.removeHandler(handler)


###############################################################################
# Test SetErrorHandler


def test_misc_15():
    messages0 = []

    def handle0(ecls, ecode, emsg):
        messages0.append((ecls, ecode, emsg))

    messages1 = []

    def handle1(ecls, ecode, emsg):
        messages1.append((ecls, ecode, emsg))

    prev_debug = gdal.GetConfigOption("CPL_DEBUG")
    try:
        gdal.SetErrorHandler(handle0)
        gdal.SetConfigOption("CPL_DEBUG", "ON")

        gdal.Debug("foo", "bar")
        gdal.Error(gdal.CE_Debug, gdal.CPLE_FileIO, "debug2")
        gdal.Error(gdal.CE_None, gdal.CPLE_AppDefined, "info1")
        gdal.Error(gdal.CE_Warning, gdal.CPLE_AssertionFailed, "warning1")
        gdal.Error(gdal.CE_Failure, 99999, "error1")

        expected0 = [
            (gdal.CE_Debug, 0, "foo: bar"),
            (gdal.CE_Debug, gdal.CPLE_FileIO, "debug2"),
            (gdal.CE_None, gdal.CPLE_AppDefined, "info1"),
            (gdal.CE_Warning, gdal.CPLE_AssertionFailed, "warning1"),
            (gdal.CE_Failure, 99999, "error1"),
        ]
        assert expected0 == messages0, "SetErrorHandler: mismatched log messages"
        messages0[:] = []

        # Check Push
        gdal.PushErrorHandler(handle1)
        gdal.SetConfigOption("CPL_DEBUG", "OFF")
        gdal.Error(gdal.CE_Debug, gdal.CPLE_FileIO, "debug2")
        gdal.Error(gdal.CE_None, gdal.CPLE_AppDefined, "info1")
        gdal.Error(gdal.CE_Warning, gdal.CPLE_AssertionFailed, "warning1")
        gdal.Error(gdal.CE_Failure, 99999, "error1")

        assert len(messages0) == 0, "PushErrorHandler: unexpected log messages"
        assert len(messages1) == 4, "PushErrorHandler: missing log messages"

        # and pop restores original behaviour
        gdal.PopErrorHandler()
        messages1[:] = []
        gdal.Error(gdal.CE_Debug, gdal.CPLE_FileIO, "debug2")
        gdal.Error(gdal.CE_None, gdal.CPLE_AppDefined, "info1")
        gdal.Error(gdal.CE_Warning, gdal.CPLE_AssertionFailed, "warning1")
        gdal.Error(gdal.CE_Failure, 99999, "error1")

        assert len(messages0) == 4, "PopErrorHandler: missing log messages"
        assert len(messages1) == 0, "PopErrorHandler: unexpected log messages"

    finally:
        gdal.SetErrorHandler("CPLDefaultErrorHandler")
        gdal.SetConfigOption("CPL_DEBUG", prev_debug)


###############################################################################
# Test config option context managers


def test_misc_config_context_mgrs_1():
    # Make sure that config_options context manager does not convert a
    # global config option to a thread-local config option

    try:
        gdal.SetConfigOption("A", "1")

        assert gdal.GetConfigOption("A") == "1"
        assert gdal.GetThreadLocalConfigOption("A") is None

        # temporarily set new thread-local value for A
        with gdal.config_option("A", "2", thread_local=True):
            assert gdal.GetConfigOption("A") == "2"
            assert gdal.GetThreadLocalConfigOption("A") == "2"

        # value of A is restored, and thread-local value is unset
        assert gdal.GetConfigOption("A") == "1"
        assert gdal.GetThreadLocalConfigOption("A") is None

    finally:
        gdal.SetConfigOption("A", None)


def test_misc_config_context_mgrs_2():
    # Make sure that config_options context manager does not convert a
    # thread-local config option to a global config option

    try:
        gdal.SetThreadLocalConfigOption("B", "5")
        assert gdal.GetConfigOption("B") == "5"
        assert gdal.GetThreadLocalConfigOption("B") == "5"

        # temporarily set new global value for B
        # this has no effect, because the thread-local value overrides it
        with gdal.config_option("B", "6", thread_local=False):
            assert gdal.GetConfigOption("B") == "5"
            assert gdal.GetThreadLocalConfigOption("B") == "5"

        # value of B is restored
        assert gdal.GetThreadLocalConfigOption("B") == "5"

        gdal.SetThreadLocalConfigOption("B", None)
        assert gdal.GetConfigOption("B") is None

    finally:
        gdal.SetThreadLocalConfigOption("B", None)


def test_misc_config_context_mgrs_3():
    # Make sure that config_options correctly restores state when
    # a configuration option is set in both thread-local and global
    # contexts.

    try:
        gdal.SetConfigOption("C", "GLOBAL")
        gdal.SetThreadLocalConfigOption("C", "TL")

        # temporarily set new global value for C
        # this has no effect, because the thread-local value overrides it
        with gdal.config_option("C", "XX", thread_local=False):
            assert gdal.GetConfigOption("C") == "TL"
            assert gdal.GetThreadLocalConfigOption("C") == "TL"

        # value of C is restored
        assert gdal.GetThreadLocalConfigOption("C") == "TL"

        # clear thread-local value of C, exposing global value
        gdal.SetThreadLocalConfigOption("C", None)
        assert gdal.GetConfigOption("C") == "GLOBAL"

    finally:
        gdal.SetConfigOption("C", None)
        gdal.SetThreadLocalConfigOption("C", None)


###############################################################################
# Test GetConfigOptions


def test_misc_get_config_options():

    assert gdal.GetConfigOptions() == {}

    try:
        gdal.SetConfigOption("A", "1")

        assert gdal.GetConfigOptions() == {"A": "1"}

        gdal.SetConfigOption("B", "2")

        with gdal.config_options({"A": "9", "C": "8"}):
            assert gdal.GetConfigOptions() == {"A": "9", "B": "2", "C": "8"}

        assert gdal.GetConfigOptions() == {"A": "1", "B": "2"}

    finally:
        gdal.SetConfigOption("A", None)
        gdal.SetConfigOption("B", None)


###############################################################################
# Test GeneralCmdLineProcessor


def test_misc_general_cmd_line_processor(tmp_path):

    processed = gdal.GeneralCmdLineProcessor(
        ["program", 2, tmp_path / "a_path", "a_string"]
    )
    assert processed == ["program", "2", str(tmp_path / "a_path"), "a_string"]


###############################################################################
# Test GDALDriverHasOpenOption()


@pytest.mark.require_driver("GTiff")
@pytest.mark.parametrize(
    "driver_name,open_option,expected",
    [
        ("GTiff", "XXXX", False),
        ("GTiff", "GEOTIFF_KEYS_FLAVOR", True),
    ],
)
def test_misc_gdal_driver_has_open_option(driver_name, open_option, expected):
    driver = gdal.GetDriverByName(driver_name)
    assert driver is not None
    assert driver.HasOpenOption(open_option) == expected


###############################################################################
# Test gdal.quiet_errors() and gdal.quiet_warnings()


@pytest.mark.parametrize("context", ("quiet_errors", "quiet_warnings"))
def test_misc_quiet_errors(tmp_path, context):

    script = f"""
from osgeo import gdal

with gdal.{context}():
    gdal.Error(gdal.CE_Debug, gdal.CPLE_AppDefined, "Debug")
    gdal.Error(gdal.CE_Warning, gdal.CPLE_AppDefined, "Warning")
    gdal.Error(gdal.CE_Failure, gdal.CPLE_AppDefined, "Failure")
"""

    with open(tmp_path / "script.py", "w") as f:
        f.write(script)

    out, err = run_py_script_as_external_script(
        tmp_path, "script", "", return_stderr=True
    )
    if context == "quiet_errors":
        assert "Debug" in err
        assert "Warning" not in err
        assert "Failure" not in err

    if context == "quiet_warnings":
        assert "Debug" in err
        assert "Warning" not in err
        assert "Failure" in err


###############################################################################


def test_misc_cleanup():

    try:
        shutil.rmtree("tmp/tmp")
    except OSError:
        pass
