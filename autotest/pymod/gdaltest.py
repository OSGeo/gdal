# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Python Library supporting GDAL/OGR Test Suite
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import contextlib
import io
import json
import math
import os
import os.path
import shlex
import socket
import stat
import subprocess
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from queue import Queue
from threading import Thread

import pytest

from osgeo import gdal, osr

jp2kak_drv = None
jpeg2000_drv = None
jp2ecw_drv = None
jp2mrsid_drv = None
jp2openjpeg_drv = None
jp2kak_drv_unregistered = False
jpeg2000_drv_unregistered = False
jp2ecw_drv_unregistered = False
jp2mrsid_drv_unregistered = False
jp2openjpeg_drv_unregistered = False

###############################################################################


def clean_tmp():
    all_files = os.listdir("tmp")
    for filename in all_files:
        if filename in ["CVS", "do-not-remove"]:
            continue

        try:
            os.remove("tmp/" + filename)
        except OSError:
            pass


###############################################################################


def testCreateCopyInterruptCallback(pct, message, user_data):
    # pylint: disable=unused-argument
    return pct <= 0.5


###############################################################################


class GDALTest:
    def __init__(
        self,
        drivername,
        filename,
        band,
        chksum,
        xoff=0,
        yoff=0,
        xsize=0,
        ysize=0,
        options=None,
        filename_absolute=0,
        chksum_after_reopening=None,
        open_options=None,
        tmpdir=None,
    ):
        self.driver = None
        self.drivername = drivername
        self.filename = filename

        if isinstance(self.filename, os.PathLike):
            self.filename = str(self.filename)

        self.filename_absolute = filename_absolute
        self.band = band
        self.chksum = chksum
        if chksum_after_reopening is not None:
            if isinstance(chksum_after_reopening, list):
                self.chksum_after_reopening = chksum_after_reopening
            else:
                self.chksum_after_reopening = [chksum_after_reopening]
        elif chksum is None:
            self.chksum_after_reopening = None
        else:
            self.chksum_after_reopening = [chksum]
        self.xoff = xoff
        self.yoff = yoff
        self.xsize = xsize
        self.ysize = ysize
        self.options = [] if options is None else options
        self.open_options = open_options

        if tmpdir is None:
            self.tmpdir = "tmp/"
        else:
            self.tmpdir = tmpdir

    def testDriver(self):
        if self.driver is None:
            self.driver = gdal.GetDriverByName(self.drivername)
            if self.driver is None:
                pytest.skip(self.drivername + " driver not found!")

    def testOpen(
        self,
        check_prj=None,
        check_gt=None,
        gt_epsilon=None,
        check_stat=None,
        check_approx_stat=None,
        stat_epsilon=None,
        skip_checksum=None,
        check_min=None,
        check_max=None,
        check_filelist=True,
    ):
        """check_prj - projection reference, check_gt - geotransformation
        matrix (tuple), gt_epsilon - geotransformation tolerance,
        check_stat - band statistics (tuple), stat_epsilon - statistics
        tolerance."""
        self.testDriver()

        if self.filename_absolute:
            wrk_filename = self.filename
        else:
            wrk_filename = "data/" + self.filename

        if self.open_options:
            ds = gdal.OpenEx(
                wrk_filename, gdal.OF_RASTER, open_options=self.open_options
            )
        else:
            ds = gdal.Open(wrk_filename, gdal.GA_ReadOnly)

        assert ds is not None, "Failed to open dataset: " + wrk_filename

        assert (
            ds.GetDriver().ShortName == gdal.GetDriverByName(self.drivername).ShortName
        ), "The driver of the returned dataset is %s instead of %s." % (
            ds.GetDriver().ShortName,
            self.drivername,
        )

        if self.xsize == 0 and self.ysize == 0:
            self.xsize = ds.RasterXSize
            self.ysize = ds.RasterYSize

        if (
            check_filelist
            and ds.GetDriver().GetMetadataItem("DCAP_VIRTUALIO") is not None
        ):
            fl = ds.GetFileList()
            if fl is not None and fl and wrk_filename == fl[0]:

                # Copy all files in /vsimem/
                mainfile_dirname = os.path.dirname(fl[0])
                for filename in fl:
                    target_filename = (
                        "/vsimem/tmp_testOpen/" + filename[len(mainfile_dirname) + 1 :]
                    )
                    if stat.S_ISDIR(gdal.VSIStatL(filename).mode):
                        gdal.Mkdir(target_filename, 0)
                    else:
                        f = gdal.VSIFOpenL(filename, "rb")
                        assert f is not None, "File %s does not exist" % filename
                        gdal.VSIFSeekL(f, 0, 2)
                        size = gdal.VSIFTellL(f)
                        gdal.VSIFSeekL(f, 0, 0)
                        data = gdal.VSIFReadL(1, size, f)
                        gdal.VSIFCloseL(f)
                        if data is None:
                            data = ""
                        gdal.FileFromMemBuffer(target_filename, data)

                # Try to open the in-memory file
                main_virtual_filename = "/vsimem/tmp_testOpen/" + os.path.basename(
                    fl[0]
                )
                virtual_ds = gdal.Open(main_virtual_filename)
                virtual_ds_is_None = virtual_ds is None
                virtual_ds = None

                # Make sure the driver is specific enough by trying to open
                # with all other drivers but it
                drivers = []
                for i in range(gdal.GetDriverCount()):
                    drv_name = gdal.GetDriver(i).ShortName
                    if drv_name.lower() != self.drivername.lower() and not (
                        (
                            drv_name.lower() == "gif"
                            and self.drivername.lower() == "biggif"
                        )
                        or (
                            drv_name.lower() == "biggif"
                            and self.drivername.lower() == "gif"
                        )
                    ):
                        drivers += [drv_name]
                other_ds = None
                with gdal.ExceptionMgr(useExceptions=False):
                    other_ds = gdal.OpenEx(
                        main_virtual_filename, gdal.OF_RASTER, allowed_drivers=drivers
                    )
                other_ds_is_None = other_ds is None
                other_ds_driver_name = None
                if not other_ds_is_None:
                    other_ds_driver_name = other_ds.GetDriver().ShortName
                other_ds = None

                for filename in gdal.ReadDirRecursive("/vsimem/tmp_testOpen"):
                    gdal.Unlink("/vsimem/tmp_testOpen/" + filename)

                assert (
                    not virtual_ds_is_None
                ), "File list is not complete or driver does not support /vsimem/"
                assert (
                    other_ds_is_None
                ), "When excluding %s, dataset is still opened by driver %s" % (
                    self.drivername,
                    other_ds_driver_name,
                )

        # Do we need to check projection?
        if check_prj is not None:
            new_prj = ds.GetProjection()

            src_osr = osr.SpatialReference()
            src_osr.SetFromUserInput(check_prj)

            new_osr = osr.SpatialReference(wkt=new_prj)

            if not src_osr.IsSame(new_osr):
                print("")
                print("old = %s" % src_osr.ExportToPrettyWkt())
                print("new = %s" % new_osr.ExportToPrettyWkt())
                pytest.fail("Projections differ")

        # Do we need to check geotransform?
        if check_gt:
            # Default to 100th of pixel as our test value.
            if gt_epsilon is None:
                gt_epsilon = (abs(check_gt[1]) + abs(check_gt[2])) / 100.0

            new_gt = ds.GetGeoTransform()
            for i in range(6):
                if new_gt[i] != pytest.approx(check_gt[i], abs=gt_epsilon):
                    print("")
                    print("old = ", check_gt)
                    print("new = ", new_gt)
                    pytest.fail("Geotransform differs.")

        oBand = ds.GetRasterBand(self.band)
        if skip_checksum is None:
            chksum = oBand.Checksum(self.xoff, self.yoff, self.xsize, self.ysize)

        # Do we need to check approximate statistics?
        if check_approx_stat:
            # Default to 1000th of pixel value range as our test value.
            if stat_epsilon is None:
                stat_epsilon = abs(check_approx_stat[1] - check_approx_stat[0]) / 1000.0

            new_stat = oBand.GetStatistics(1, 1)
            for i in range(4):

                # NOTE - mloskot: Poor man Nan/Inf value check. It's poor
                # because we need to support old and buggy Python 2.3.
                # Tested on Linux, Mac OS X and Windows, with Python 2.3/2.4/2.5.
                sv = str(new_stat[i]).lower()
                assert not ("n" in sv or "i" in sv or "#" in sv), (
                    "NaN or infinity value encountered '%s'." % sv
                )

                if new_stat[i] != pytest.approx(check_approx_stat[i], abs=stat_epsilon):
                    print("")
                    print("old = ", check_approx_stat)
                    print("new = ", new_stat)
                    pytest.fail("Approximate statistics differs.")

        # Do we need to check statistics?
        if check_stat:
            # Default to 1000th of pixel value range as our test value.
            if stat_epsilon is None:
                stat_epsilon = abs(check_stat[1] - check_stat[0]) / 1000.0

            # FIXME: how to test approximate statistic results?
            new_stat = oBand.GetStatistics(1, 1)

            new_stat = oBand.GetStatistics(0, 1)
            for i in range(4):

                sv = str(new_stat[i]).lower()
                assert not ("n" in sv or "i" in sv or "#" in sv), (
                    "NaN or infinity value encountered '%s'." % sv
                )

                if new_stat[i] != pytest.approx(check_stat[i], abs=stat_epsilon):
                    print("")
                    print("old = ", check_stat)
                    print("new = ", new_stat)
                    pytest.fail("Statistics differs.")

        if check_min:
            assert oBand.GetMinimum() == check_min, "Unexpected minimum value %s" % str(
                oBand.GetMinimum()
            )

        if check_max:
            assert oBand.GetMaximum() == check_max, "Unexpected maximum value %s" % str(
                oBand.GetMaximum()
            )

        ds = None

        assert not is_file_open(wrk_filename), "file still open after dataset closing"

        if skip_checksum is not None:
            return
        if self.chksum is None or chksum == self.chksum:
            return
        pytest.fail(
            'Checksum for band %d in "%s" is %d, but expected %d.'
            % (self.band, self.filename, chksum, self.chksum)
        )

    def testCreateCopy(
        self,
        check_minmax=1,
        check_gt=0,
        check_srs=None,
        vsimem=0,
        new_filename=None,
        strict_in=0,
        skip_preclose_test=0,
        delete_copy=1,
        gt_epsilon=None,
        check_checksum_not_null=None,
        interrupt_during_copy=False,
        dest_open_options=None,
        quiet_error_handler=True,
    ):

        self.testDriver()

        if self.filename_absolute:
            wrk_filename = self.filename
        else:
            wrk_filename = "data/" + self.filename

        if self.open_options:
            src_ds = gdal.OpenEx(
                wrk_filename, gdal.OF_RASTER, open_options=self.open_options
            )
        else:
            src_ds = gdal.Open(wrk_filename, gdal.GA_ReadOnly)

        if self.band > 0:
            minmax = src_ds.GetRasterBand(self.band).ComputeRasterMinMax()

        src_gt = src_ds.GetGeoTransform()

        if new_filename is None:
            if vsimem:
                new_filename = "/vsimem/" + os.path.basename(self.filename) + ".tst"
            else:
                new_filename = os.path.join(
                    self.tmpdir, os.path.basename(self.filename) + ".tst"
                )

        if quiet_error_handler:
            gdal.PushErrorHandler("CPLQuietErrorHandler")
        if interrupt_during_copy:
            new_ds = self.driver.CreateCopy(
                new_filename,
                src_ds,
                strict=strict_in,
                options=self.options,
                callback=testCreateCopyInterruptCallback,
            )
        else:
            new_ds = self.driver.CreateCopy(
                new_filename, src_ds, strict=strict_in, options=self.options
            )
        if quiet_error_handler:
            gdal.PopErrorHandler()

        if interrupt_during_copy:
            if new_ds is None:
                with error_handler():
                    self.driver.Delete(new_filename)
                return
            new_ds = None
            self.driver.Delete(new_filename)
            pytest.fail("CreateCopy() should have failed due to interruption")

        assert new_ds is not None, (
            "Failed to create test file using CreateCopy method."
            + "\n"
            + gdal.GetLastErrorMsg()
        )

        assert (
            new_ds.GetDriver().ShortName
            == gdal.GetDriverByName(self.drivername).ShortName
        ), "The driver of the returned dataset is %s instead of %s." % (
            new_ds.GetDriver().ShortName,
            self.drivername,
        )

        if self.band > 0 and skip_preclose_test == 0:
            bnd = new_ds.GetRasterBand(self.band)
            if check_checksum_not_null is True:
                assert bnd.Checksum() != 0, "Got null checksum on still-open file."
            elif self.chksum is not None and bnd.Checksum() != self.chksum:
                pytest.fail(
                    "Did not get expected checksum on still-open file.\n"
                    "    Got %d instead of %d." % (bnd.Checksum(), self.chksum)
                )
            if check_minmax:
                got_minmax = bnd.ComputeRasterMinMax()
                assert got_minmax == minmax, (
                    "Did not get expected min/max values on still-open file.\n"
                    "    Got %g,%g instead of %g,%g."
                    % (got_minmax[0], got_minmax[1], minmax[0], minmax[1])
                )

        bnd = None
        new_ds = None

        # hopefully it's closed now!

        if dest_open_options is not None:
            new_ds = gdal.OpenEx(
                new_filename, gdal.OF_RASTER, open_options=dest_open_options
            )
        else:
            new_ds = gdal.Open(new_filename)
        assert new_ds is not None, "Failed to open dataset: " + new_filename

        if self.band > 0:
            bnd = new_ds.GetRasterBand(self.band)
            if check_checksum_not_null is True:
                assert bnd.Checksum() != 0, "Got null checksum on reopened file."
            elif (
                self.chksum_after_reopening is not None
                and bnd.Checksum() not in self.chksum_after_reopening
            ):
                pytest.fail(
                    "Did not get expected checksum on reopened file.\n"
                    "    Got %d instead of %s."
                    % (bnd.Checksum(), str(self.chksum_after_reopening))
                )

            if check_minmax:
                got_minmax = bnd.ComputeRasterMinMax()
                assert got_minmax == minmax, (
                    "Did not get expected min/max values on reopened file.\n"
                    "    Got %g,%g instead of %g,%g."
                    % (got_minmax[0], got_minmax[1], minmax[0], minmax[1])
                )

        # Do we need to check the geotransform?
        if check_gt:
            if gt_epsilon is None:
                eps = 0.00000001
            else:
                eps = gt_epsilon
            new_gt = new_ds.GetGeoTransform()
            if (
                new_gt[0] != pytest.approx(src_gt[0], abs=eps)
                or new_gt[1] != pytest.approx(src_gt[1], abs=eps)
                or new_gt[2] != pytest.approx(src_gt[2], abs=eps)
                or new_gt[3] != pytest.approx(src_gt[3], abs=eps)
                or new_gt[4] != pytest.approx(src_gt[4], abs=eps)
                or new_gt[5] != pytest.approx(src_gt[5], abs=eps)
            ):
                print("")
                print("old = ", src_gt)
                print("new = ", new_gt)
                pytest.fail("Geotransform differs.")

        # Do we need to check the SRS?
        if check_srs == True:
            src_srs = src_ds.GetSpatialRef()
            new_srs = new_ds.GetSpatialRef()
            if src_srs is None and new_srs is not None:
                pytest.fail("src_srs is None and new_srs is not None")
            elif src_srs is not None and new_srs is None:
                pytest.fail("src_srs is not None and new_srs is None")
            elif (
                src_srs is not None
                and new_srs is not None
                and not src_srs.IsSame(new_srs)
            ):
                print("")
                print("old = %s" % src_srs.ExportToPrettyWkt())
                print("new = %s" % new_srs.ExportToPrettyWkt())
                pytest.fail("Projections differ")
        elif check_srs is not None:
            new_prj = new_ds.GetProjection()

            src_osr = osr.SpatialReference(wkt=src_ds.GetProjection())
            new_osr = osr.SpatialReference(wkt=new_prj)

            if not src_osr.IsSame(new_osr):
                print("")
                print("old = %s" % src_osr.ExportToPrettyWkt())
                print("new = %s" % new_osr.ExportToPrettyWkt())
                pytest.fail("Projections differ")

        bnd = None
        new_ds = None
        src_ds = None

        if gdal.GetConfigOption("CPL_DEBUG", "OFF") != "ON" and delete_copy == 1:
            self.driver.Delete(new_filename)

    def testCreate(
        self,
        vsimem=0,
        new_filename=None,
        out_bands=1,
        check_minmax=1,
        dest_open_options=None,
        delete_output_file=True,
    ):
        self.testDriver()

        if self.filename_absolute:
            wrk_filename = self.filename
        else:
            wrk_filename = "data/" + self.filename

        if self.open_options:
            src_ds = gdal.OpenEx(
                wrk_filename, gdal.OF_RASTER, open_options=self.open_options
            )
        else:
            src_ds = gdal.Open(wrk_filename, gdal.GA_ReadOnly)

        xsize = src_ds.RasterXSize
        ysize = src_ds.RasterYSize
        src_img = src_ds.GetRasterBand(self.band).ReadRaster(0, 0, xsize, ysize)
        minmax = src_ds.GetRasterBand(self.band).ComputeRasterMinMax()

        if new_filename is None:
            if vsimem:
                new_filename = "/vsimem/" + self.filename + ".tst"
            else:
                new_filename = os.path.join(
                    self.tmpdir, os.path.basename(self.filename) + ".tst"
                )

        new_ds = self.driver.Create(
            new_filename,
            xsize,
            ysize,
            out_bands,
            src_ds.GetRasterBand(self.band).DataType,
            options=self.options,
        )
        assert new_ds is not None, "Failed to create test file using Create method."

        src_ds = None

        try:
            for band in range(1, out_bands + 1):
                new_ds.GetRasterBand(band).WriteRaster(0, 0, xsize, ysize, src_img)
        except Exception:
            pytest.fail("Failed to write raster bands to test file.")

        for band in range(1, out_bands + 1):
            assert (
                self.chksum is None
                or new_ds.GetRasterBand(band).Checksum() == self.chksum
            ), (
                "Did not get expected checksum on still-open file.\n"
                "    Got %d instead of %d."
                % (new_ds.GetRasterBand(band).Checksum(), self.chksum)
            )

            computed_minmax = new_ds.GetRasterBand(band).ComputeRasterMinMax()
            if computed_minmax != minmax and check_minmax:
                print("expect: ", minmax)
                print("got: ", computed_minmax)
                pytest.fail("Did not get expected min/max values on still-open file.")

        new_ds = None

        if dest_open_options is not None:
            new_ds = gdal.OpenEx(
                new_filename, gdal.OF_RASTER, open_options=dest_open_options
            )
        else:
            new_ds = gdal.Open(new_filename)
        assert new_ds is not None, "Failed to open dataset: " + new_filename

        for band in range(1, out_bands + 1):
            assert (
                self.chksum is None
                or new_ds.GetRasterBand(band).Checksum() == self.chksum
            ), (
                "Did not get expected checksum on reopened file."
                "    Got %d instead of %d."
                % (new_ds.GetRasterBand(band).Checksum(), self.chksum)
            )

            assert (
                new_ds.GetRasterBand(band).ComputeRasterMinMax() == minmax
                or not check_minmax
            ), "Did not get expected min/max values on reopened file."

        assert new_ds.FlushCache() == gdal.CE_None
        new_ds = None

        if delete_output_file and gdal.GetConfigOption("CPL_DEBUG", "OFF") != "ON":
            self.driver.Delete(new_filename)

    def testSetGeoTransform(self):
        self.testDriver()

        wrk_filename = "data/" + self.filename
        if self.open_options:
            src_ds = gdal.OpenEx(
                wrk_filename, gdal.OF_RASTER, open_options=self.open_options
            )
        else:
            src_ds = gdal.Open(wrk_filename, gdal.GA_ReadOnly)

        xsize = src_ds.RasterXSize
        ysize = src_ds.RasterYSize

        new_filename = os.path.join(
            self.tmpdir, os.path.basename(self.filename) + ".tst"
        )
        new_ds = self.driver.Create(
            new_filename,
            xsize,
            ysize,
            1,
            src_ds.GetRasterBand(self.band).DataType,
            options=self.options,
        )
        assert new_ds is not None, "Failed to create test file using Create method."

        gt = (123.0, 1.18, 0.0, 456.0, 0.0, -1.18)
        assert (
            new_ds.SetGeoTransform(gt) is gdal.CE_None
        ), "Failed to set geographic transformation."

        src_ds = None
        new_ds = None

        new_ds = gdal.Open(new_filename)
        assert new_ds is not None, "Failed to open dataset: " + new_filename

        eps = 0.00000001
        new_gt = new_ds.GetGeoTransform()
        if (
            new_gt[0] != pytest.approx(gt[0], abs=eps)
            or new_gt[1] != pytest.approx(gt[1], abs=eps)
            or new_gt[2] != pytest.approx(gt[2], abs=eps)
            or new_gt[3] != pytest.approx(gt[3], abs=eps)
            or new_gt[4] != pytest.approx(gt[4], abs=eps)
            or new_gt[5] != pytest.approx(gt[5], abs=eps)
        ):
            print("")
            print("old = ", gt)
            print("new = ", new_gt)
            pytest.fail("Did not get expected geotransform.")

        new_ds = None

        if gdal.GetConfigOption("CPL_DEBUG", "OFF") != "ON":
            self.driver.Delete(new_filename)

    def testSetProjection(self, prj=None, expected_prj=None):
        self.testDriver()

        wrk_filename = "data/" + self.filename
        if self.open_options:
            src_ds = gdal.OpenEx(
                wrk_filename, gdal.OF_RASTER, open_options=self.open_options
            )
        else:
            src_ds = gdal.Open(wrk_filename, gdal.GA_ReadOnly)

        xsize = src_ds.RasterXSize
        ysize = src_ds.RasterYSize

        new_filename = os.path.join(
            self.tmpdir, os.path.basename(self.filename) + ".tst"
        )
        new_ds = self.driver.Create(
            new_filename,
            xsize,
            ysize,
            1,
            src_ds.GetRasterBand(self.band).DataType,
            options=self.options,
        )
        assert new_ds is not None, "Failed to create test file using Create method."

        gt = (123.0, 1.18, 0.0, 456.0, 0.0, -1.18)
        if prj is None:
            # This is a challenging SRS since it has non-meter linear units.
            prj = 'PROJCS["NAD83 / Ohio South",GEOGCS["NAD83",DATUM["North_American_Datum_1983",SPHEROID["GRS 1980",6378137,298.257222101,AUTHORITY["EPSG","7019"]],AUTHORITY["EPSG","6269"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.01745329251994328,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4269"]],PROJECTION["Lambert_Conformal_Conic_2SP"],PARAMETER["standard_parallel_1",40.03333333333333],PARAMETER["standard_parallel_2",38.73333333333333],PARAMETER["latitude_of_origin",38],PARAMETER["central_meridian",-82.5],PARAMETER["false_easting",1968500],PARAMETER["false_northing",0],UNIT["US survey foot",0.3048006096012192]]'

        src_osr = osr.SpatialReference()
        src_osr.ImportFromWkt(prj)

        new_ds.SetGeoTransform(gt)
        assert (
            new_ds.SetProjection(prj) is gdal.CE_None
        ), "Failed to set geographic projection string."

        src_ds = None
        new_ds = None

        new_ds = gdal.Open(new_filename)
        assert new_ds is not None, "Failed to open dataset: " + new_filename

        expected_osr = osr.SpatialReference()
        if expected_prj is None:
            expected_osr = src_osr
        else:
            expected_osr.ImportFromWkt(expected_prj)

        new_osr = osr.SpatialReference()
        new_osr.ImportFromWkt(new_ds.GetProjection())
        if not new_osr.IsSame(expected_osr):
            print("Got: ")
            print(new_osr.ExportToPrettyWkt())
            print("Expected:")
            print(expected_osr.ExportToPrettyWkt())
            pytest.fail("Did not get expected projection reference.")

        new_ds = None

        if gdal.GetConfigOption("CPL_DEBUG", "OFF") != "ON":
            self.driver.Delete(new_filename)

    def testSetMetadata(self):
        self.testDriver()

        wrk_filename = "data/" + self.filename
        if self.open_options:
            src_ds = gdal.OpenEx(
                wrk_filename, gdal.OF_RASTER, open_options=self.open_options
            )
        else:
            src_ds = gdal.Open(wrk_filename, gdal.GA_ReadOnly)

        xsize = src_ds.RasterXSize
        ysize = src_ds.RasterYSize

        new_filename = os.path.join(
            self.tmpdir, os.path.basename(self.filename) + ".tst"
        )
        new_ds = self.driver.Create(
            new_filename,
            xsize,
            ysize,
            1,
            src_ds.GetRasterBand(self.band).DataType,
            options=self.options,
        )
        assert new_ds is not None, "Failed to create test file using Create method."

        new_ds.SetMetadata({"TEST_KEY": "TestValue"})
        # FIXME
        # if new_ds.SetMetadata( dict ) is not gdal.CE_None:
        #     print new_ds.SetMetadata( dict )
        #     post_reason( 'Failed to set metadata item.' )
        #     return 'fail'

        src_ds = None
        new_ds = None

        new_ds = gdal.Open(new_filename)
        assert new_ds is not None, "Failed to open dataset: " + new_filename

        md_dict = new_ds.GetMetadata()

        assert "TEST_KEY" in md_dict, "Metadata item TEST_KEY does not exist."

        assert md_dict["TEST_KEY"] == "TestValue", "Did not get expected metadata item."

        new_ds = None

        if gdal.GetConfigOption("CPL_DEBUG", "OFF") != "ON":
            self.driver.Delete(new_filename)

    def testSetNoDataValue(self, delete=False):
        self.testDriver()

        wrk_filename = "data/" + self.filename
        if self.open_options:
            src_ds = gdal.OpenEx(
                wrk_filename, gdal.OF_RASTER, open_options=self.open_options
            )
        else:
            src_ds = gdal.Open(wrk_filename, gdal.GA_ReadOnly)

        xsize = src_ds.RasterXSize
        ysize = src_ds.RasterYSize

        new_filename = os.path.join(
            self.tmpdir, os.path.basename(self.filename) + ".tst"
        )
        dt = src_ds.GetRasterBand(self.band).DataType
        new_ds = self.driver.Create(
            new_filename,
            xsize,
            ysize,
            1,
            dt,
            options=self.options,
        )
        assert new_ds is not None, "Failed to create test file using Create method."

        if dt == gdal.GDT_Int8:
            nodata = -11
        elif self.options is None or "PIXELTYPE=SIGNEDBYTE" not in self.options:
            nodata = 130
        else:
            nodata = 11
        assert (
            new_ds.GetRasterBand(1).SetNoDataValue(nodata) is gdal.CE_None
        ), "Failed to set NoData value."

        src_ds = None
        new_ds = None

        if delete:
            mode = gdal.GA_Update
        else:
            mode = gdal.GA_ReadOnly
        new_ds = gdal.Open(new_filename, mode)
        assert new_ds is not None, "Failed to open dataset: " + new_filename

        assert (
            nodata == new_ds.GetRasterBand(1).GetNoDataValue()
        ), "Did not get expected NoData value."

        if delete:
            assert (
                new_ds.GetRasterBand(1).DeleteNoDataValue() == 0
            ), "Did not manage to delete nodata value"

        new_ds = None

        if delete:
            new_ds = gdal.Open(new_filename)
            assert (
                new_ds.GetRasterBand(1).GetNoDataValue() is None
            ), "Got nodata value whereas none was expected"
            new_ds = None

        if gdal.GetConfigOption("CPL_DEBUG", "OFF") != "ON":
            self.driver.Delete(new_filename)

    def testSetNoDataValueAndDelete(self):
        self.testSetNoDataValue(delete=True)

    def testSetDescription(self):
        self.testDriver()

        wrk_filename = "data/" + self.filename
        if self.open_options:
            src_ds = gdal.OpenEx(
                wrk_filename, gdal.OF_RASTER, open_options=self.open_options
            )
        else:
            src_ds = gdal.Open(wrk_filename, gdal.GA_ReadOnly)

        xsize = src_ds.RasterXSize
        ysize = src_ds.RasterYSize

        new_filename = os.path.join(
            self.tmpdir, os.path.basename(self.filename) + ".tst"
        )
        new_ds = self.driver.Create(
            new_filename,
            xsize,
            ysize,
            1,
            src_ds.GetRasterBand(self.band).DataType,
            options=self.options,
        )
        assert new_ds is not None, "Failed to create test file using Create method."

        description = "Description test string"
        new_ds.GetRasterBand(1).SetDescription(description)

        src_ds = None
        new_ds = None

        new_ds = gdal.Open(new_filename)
        assert new_ds is not None, "Failed to open dataset: " + new_filename

        assert (
            description == new_ds.GetRasterBand(1).GetDescription()
        ), "Did not get expected description string."

        new_ds = None

        if gdal.GetConfigOption("CPL_DEBUG", "OFF") != "ON":
            self.driver.Delete(new_filename)

    def testSetUnitType(self):
        self.testDriver()

        wrk_filename = "data/" + self.filename
        if self.open_options:
            src_ds = gdal.OpenEx(
                wrk_filename, gdal.OF_RASTER, open_options=self.open_options
            )
        else:
            src_ds = gdal.Open(wrk_filename, gdal.GA_ReadOnly)

        xsize = src_ds.RasterXSize
        ysize = src_ds.RasterYSize

        new_filename = os.path.join(
            self.tmpdir, os.path.basename(self.filename) + ".tst"
        )
        new_ds = self.driver.Create(
            new_filename,
            xsize,
            ysize,
            1,
            src_ds.GetRasterBand(self.band).DataType,
            options=self.options,
        )
        assert new_ds is not None, "Failed to create test file using Create method."

        unit = "mg/m3"
        assert (
            new_ds.GetRasterBand(1).SetUnitType(unit) is gdal.CE_None
        ), "Failed to set unit type."

        src_ds = None
        new_ds = None

        new_ds = gdal.Open(new_filename)
        assert new_ds is not None, "Failed to open dataset: " + new_filename

        new_unit = new_ds.GetRasterBand(1).GetUnitType()
        if new_unit != unit:
            print("")
            print("old = ", unit)
            print("new = ", new_unit)
            pytest.fail("Did not get expected unit type.")

        new_ds = None

        if gdal.GetConfigOption("CPL_DEBUG", "OFF") != "ON":
            self.driver.Delete(new_filename)


def approx_equal(a, b):
    a = float(a)
    b = float(b)
    if a == 0 and b != 0:
        return 0

    if b / a != pytest.approx(1.0, abs=0.00000000001):
        return 0
    return 1


def user_srs_to_wkt(user_text):
    srs = osr.SpatialReference()
    srs.SetFromUserInput(user_text)
    return srs.ExportToWkt()


def equal_srs_from_wkt(expected_wkt, got_wkt, verbose=True):
    expected_srs = osr.SpatialReference()
    expected_srs.SetAxisMappingStrategy(osr.OAMS_AUTHORITY_COMPLIANT)
    expected_srs.ImportFromWkt(expected_wkt)

    got_srs = osr.SpatialReference()
    got_srs.SetAxisMappingStrategy(osr.OAMS_AUTHORITY_COMPLIANT)
    got_srs.ImportFromWkt(got_wkt)

    if got_srs.IsSame(expected_srs):
        return True
    if verbose:
        print("Expected:\n%s" % expected_wkt)
        print("Got:     \n%s" % got_wkt)
        print("SRS differs from expected.")
    return False


###############################################################################
# Compare two sets of RPC metadata, and establish if they are essentially
# equivalent or not.


def check_rpcs_equal(md1, md2):
    __tracebackhide__ = True

    simple_fields = [
        "LINE_OFF",
        "SAMP_OFF",
        "LAT_OFF",
        "LONG_OFF",
        "HEIGHT_OFF",
        "LINE_SCALE",
        "SAMP_SCALE",
        "LAT_SCALE",
        "LONG_SCALE",
        "HEIGHT_SCALE",
    ]
    coef_fields = [
        "LINE_NUM_COEFF",
        "LINE_DEN_COEFF",
        "SAMP_NUM_COEFF",
        "SAMP_DEN_COEFF",
    ]

    for sf in simple_fields:

        assert sf in md1
        assert sf in md2
        assert approx_equal(float(md1[sf]), float(md2[sf]))

    for cf in coef_fields:

        list1 = md1[cf].split()
        list2 = md2[cf].split()

        assert len(list1) == 20, "%s value list length wrong(1)" % cf

        assert len(list2) == 20, "%s value list length wrong(2)" % cf

        for i in range(20):
            assert approx_equal(
                float(list1[i]), float(list2[i])
            ), "%s[%d] values differ." % (cf, i)


###############################################################################
# Test if geotransforms are equal with an epsilon tolerance
#


def check_geotransform(gt1, gt2, gt_epsilon):
    __tracebackhide__ = True
    assert gt1 == pytest.approx(gt2, abs=gt_epsilon), "Geotransform differs."


###############################################################################
# Download file at url 'url' and put it as 'filename' in 'tmp/cache/'
#
# If 'filename' already exits in 'tmp/cache/', it is not downloaded
# If GDAL_DOWNLOAD_TEST_DATA is not defined, the function fails
# If GDAL_DOWNLOAD_TEST_DATA is defined, 'url' is downloaded  as 'filename' in 'tmp/cache/'


def download_file(
    url,
    filename=None,
    download_size=-1,
    force_download=False,
    max_download_duration=None,
    base_dir="tmp/cache",
    chunk_size=1024,
):

    if filename is None:
        filename = os.path.basename(url)
    elif filename.startswith(base_dir + "/"):
        filename = filename[len(base_dir + "/") :]

    if os.path.exists(os.path.join(base_dir, filename)):
        return True

    if not (force_download or download_test_data()):
        return False

    val = None
    start_time = time.time()
    try:
        handle = gdalurlopen(url)
        if handle is None:
            return False
        if download_size == -1:
            try:
                handle_info = handle.info()
                download_size = int(handle_info["content-length"])
                print("Downloading %s (length = %d bytes)..." % (url, download_size))
            except Exception:
                print("Downloading %s..." % (url))
        else:
            print("Downloading %d bytes from %s..." % (download_size, url))
    except Exception:
        return False

    if download_size >= 0:
        sys.stdout.write("Progress: ")
    nLastTick = -1
    val = b""
    while len(val) < download_size or download_size < 0:
        to_read = chunk_size
        if download_size >= 0 and len(val) + to_read > download_size:
            to_read = download_size - len(val)
        try:
            chunk = handle.read(to_read)
        except Exception:
            print("Did not get expected data length.")
            return False
        val += chunk
        if len(chunk) < to_read:
            if download_size < 0:
                break
            print("Did not get expected data length.")
            return False
        if download_size >= 0:
            nThisTick = int(40 * len(val) / download_size)
            while nThisTick > nLastTick:
                nLastTick = nLastTick + 1
                if nLastTick % 4 == 0:
                    sys.stdout.write("%d" % int((nLastTick / 4) * 10))
                else:
                    sys.stdout.write(".")
            sys.stdout.flush()
            nLastTick = nThisTick
            if nThisTick == 40:
                sys.stdout.write(" - done.\n")

        current_time = time.time()
        if (
            max_download_duration is not None
            and current_time - start_time > max_download_duration
        ):
            print("Download aborted due to timeout.")
            return False

    try:
        os.stat(base_dir)
    except OSError:
        os.mkdir(base_dir)

    try:
        open(base_dir + "/" + filename, "wb").write(val)
        return True
    except IOError:
        print("Cannot write %s" % (filename))
        return False


# Attempt to download file using `download_file`; skip test in case of failure
def download_or_skip(url, *args, **kwargs):
    __tracebackhide__ = True

    msg = io.StringIO()
    with contextlib.redirect_stdout(msg):
        success = download_file(url, *args, **kwargs)

    if not success:
        if download_test_data():
            pytest.skip(f"Failed to download {url} : {msg.getvalue()}")
        else:
            pytest.skip("GDAL_DOWNLOAD_TEST_DATA is not set to YES")


###############################################################################
# GDAL data type to python struct format
def gdal_data_type_to_python_struct_format(datatype):
    type_char = "B"
    if datatype == gdal.GDT_Int16:
        type_char = "h"
    elif datatype == gdal.GDT_UInt16:
        type_char = "H"
    elif datatype == gdal.GDT_Int32:
        type_char = "i"
    elif datatype == gdal.GDT_UInt32:
        type_char = "I"
    elif datatype == gdal.GDT_Float32:
        type_char = "f"
    elif datatype == gdal.GDT_Float64:
        type_char = "d"
    return type_char


###############################################################################
# Compare the values of the pixels


def compare_ds(ds1, ds2, xoff=0, yoff=0, width=0, height=0, verbose=1):
    import struct

    if width == 0:
        width = ds1.RasterXSize
    if height == 0:
        height = ds1.RasterYSize
    data1 = ds1.GetRasterBand(1).ReadRaster(xoff, yoff, width, height)
    type_char = gdal_data_type_to_python_struct_format(ds1.GetRasterBand(1).DataType)
    val_array1 = struct.unpack(type_char * width * height, data1)

    data2 = ds2.GetRasterBand(1).ReadRaster(xoff, yoff, width, height)
    type_char = gdal_data_type_to_python_struct_format(ds2.GetRasterBand(1).DataType)
    val_array2 = struct.unpack(type_char * width * height, data2)

    maxdiff = 0.0
    ndiffs = 0
    for i in range(width * height):
        diff = val_array1[i] - val_array2[i]
        if diff != 0:
            # print(val_array1[i])
            # print(val_array2[i])
            ndiffs = ndiffs + 1
            if abs(diff) > maxdiff:
                maxdiff = abs(diff)
                if verbose:
                    print(
                        "Diff at pixel (%d, %d) : %f"
                        % (i % width, i / width, float(diff))
                    )
            elif ndiffs < 10:
                if verbose:
                    print(
                        "Diff at pixel (%d, %d) : %f"
                        % (i % width, i / width, float(diff))
                    )
    if maxdiff != 0 and verbose:
        print("Max diff : %d" % (maxdiff))
        print("Number of diffs : %d" % (ndiffs))

    return maxdiff


###############################################################################
# Deregister all JPEG2000 drivers, except the one passed as an argument


def deregister_all_jpeg2000_drivers_but(name_of_driver_to_keep):
    global jp2kak_drv, jpeg2000_drv, jp2ecw_drv, jp2mrsid_drv, jp2openjpeg_drv
    global jp2kak_drv_unregistered, jpeg2000_drv_unregistered, jp2ecw_drv_unregistered, jp2mrsid_drv_unregistered, jp2openjpeg_drv_unregistered

    # Deregister other potential conflicting JPEG2000 drivers that will
    # be re-registered in the cleanup
    jp2kak_drv = gdal.GetDriverByName("JP2KAK")
    if name_of_driver_to_keep != "JP2KAK" and jp2kak_drv:
        gdal.Debug("gdaltest", "Deregistering JP2KAK")
        jp2kak_drv.Deregister()
        jp2kak_drv_unregistered = True

    jpeg2000_drv = gdal.GetDriverByName("JPEG2000")
    if name_of_driver_to_keep != "JPEG2000" and jpeg2000_drv:
        gdal.Debug("gdaltest", "Deregistering JPEG2000")
        jpeg2000_drv.Deregister()
        jpeg2000_drv_unregistered = True

    jp2ecw_drv = gdal.GetDriverByName("JP2ECW")
    if name_of_driver_to_keep != "JP2ECW" and jp2ecw_drv:
        gdal.Debug("gdaltest.", "Deregistering JP2ECW")
        jp2ecw_drv.Deregister()
        jp2ecw_drv_unregistered = True

    jp2mrsid_drv = gdal.GetDriverByName("JP2MrSID")
    if name_of_driver_to_keep != "JP2MrSID" and jp2mrsid_drv:
        gdal.Debug("gdaltest.", "Deregistering JP2MrSID")
        jp2mrsid_drv.Deregister()
        jp2mrsid_drv_unregistered = True

    jp2openjpeg_drv = gdal.GetDriverByName("JP2OpenJPEG")
    if name_of_driver_to_keep != "JP2OpenJPEG" and jp2openjpeg_drv:
        gdal.Debug("gdaltest.", "Deregistering JP2OpenJPEG")
        jp2openjpeg_drv.Deregister()
        jp2openjpeg_drv_unregistered = True

    return True


###############################################################################
# Re-register all JPEG2000 drivers previously disabled by
# deregister_all_jpeg2000_drivers_but


def reregister_all_jpeg2000_drivers():
    global jp2kak_drv, jpeg2000_drv, jp2ecw_drv, jp2mrsid_drv, jp2openjpeg_drv
    global jp2kak_drv_unregistered, jpeg2000_drv_unregistered, jp2ecw_drv_unregistered, jp2mrsid_drv_unregistered, jp2openjpeg_drv_unregistered

    if jp2kak_drv_unregistered:
        jp2kak_drv.Register()
        jp2kak_drv_unregistered = False
        gdal.Debug("gdaltest", "Registering JP2KAK")

    if jpeg2000_drv_unregistered:
        jpeg2000_drv.Register()
        jpeg2000_drv_unregistered = False
        gdal.Debug("gdaltest", "Registering JPEG2000")

    if jp2ecw_drv_unregistered:
        jp2ecw_drv.Register()
        jp2ecw_drv_unregistered = False
        gdal.Debug("gdaltest", "Registering JP2ECW")

    if jp2mrsid_drv_unregistered:
        jp2mrsid_drv.Register()
        jp2mrsid_drv_unregistered = False
        gdal.Debug("gdaltest", "Registering JP2MrSID")

    if jp2openjpeg_drv_unregistered:
        jp2openjpeg_drv.Register()
        jp2openjpeg_drv_unregistered = False
        gdal.Debug("gdaltest", "Registering JP2OpenJPEG")

    return True


###############################################################################
# Determine if the filesystem supports sparse files.
# Currently, this will only work on Linux (or any *NIX that has the stat
# command line utility)


def filesystem_supports_sparse_files(path):

    if gdal.GetConfigOption("TRAVIS", None):
        return False

    try:
        (ret, err) = runexternal_out_and_err(f'stat -f -c "%T" {path}')
    except OSError:
        return False

    if err != "":
        print("Cannot determine if filesystem supports sparse files")
        return False

    if "fat32" in ret:
        print("File system does not support sparse files")
        return False

    if "wslfs" in ret or "0x53464846" in ret:
        # wslfs for older stat versions
        print(
            "Windows Subsystem for Linux FS is at the time of "
            + "writing not known to support sparse files"
        )
        return False

    # Add here any missing filesystem supporting sparse files
    # See http://en.wikipedia.org/wiki/Comparison_of_file_systems
    filesystems_supporting_sparse_files = {
        "ext3",
        "ext4",
        "reiser",
        "xfs",
        "jfs",
        "zfs",
        "ntfs",
    }

    for fs in filesystems_supporting_sparse_files:
        if fs in ret:
            return True

    print("Filesystem %s is not believed to support sparse files" % ret)
    return False


###############################################################################
# Unzip a file


def unzip(target_dir, zipfilename, verbose=False):

    try:
        import zipfile

        zf = zipfile.ZipFile(zipfilename)
    except ImportError:
        os.system("unzip -d " + target_dir + " " + zipfilename)
        return

    for filename in zf.namelist():
        if verbose:
            print(filename)
        outfilename = os.path.join(target_dir, filename)
        if filename.endswith("/"):
            if not os.path.exists(outfilename):
                os.makedirs(outfilename)
        else:
            outdirname = os.path.dirname(outfilename)
            if not os.path.exists(outdirname):
                os.makedirs(outdirname)

            outfile = open(outfilename, "wb")
            outfile.write(zf.read(filename))
            outfile.close()

    return


isnan = math.isnan

###############################################################################
# Return NaN


def NaN():
    return float("nan")


###############################################################################
# Return positive infinity


def posinf():
    return float("inf")


###############################################################################
# Return negative infinity


def neginf():
    return float("-inf")


###############################################################################
# Has the user requested to download test data
def download_test_data():
    val = gdal.GetConfigOption("GDAL_DOWNLOAD_TEST_DATA", None)
    return val == "yes" or val == "YES"


###############################################################################
# Has the user requested to run the slow tests
def run_slow_tests():
    val = gdal.GetConfigOption("GDAL_RUN_SLOW_TESTS", None)
    return val == "yes" or val == "YES"


###############################################################################
# Return true if the platform support symlinks


def support_symlink():
    if sys.platform.startswith("linux"):
        return True
    if sys.platform.find("freebsd") != -1:
        return True
    if sys.platform == "darwin":
        return True
    if sys.platform.find("sunos") != -1:
        return True
    return False


###############################################################################
# Return True if the test must be skipped


def skip_on_travis():
    __tracebackhide__ = True
    val = gdal.GetConfigOption("TRAVIS", None)
    if val is not None:
        pytest.skip("Test skipped on Travis")


###############################################################################
# Return True if the provided name is in TRAVIS_BRANCH or BUILD_NAME


def is_travis_branch(name):
    if "TRAVIS_BRANCH" in os.environ:
        val = os.environ["TRAVIS_BRANCH"]
        if name in val:
            return True
    if "BUILD_NAME" in os.environ:
        val = os.environ["BUILD_NAME"]
        if name in val:
            return True
    return False


###############################################################################
# Return True if we run under CI


def is_ci():
    return "CI" in os.environ


###############################################################################
# find_lib_linux()
# Parse /proc/self/maps to find an occurrence of libXXXXX.so.*


def find_lib_linux(libname):

    f = open("/proc/self/maps")
    lines = f.readlines()
    f.close()

    for line in lines:
        if line.rfind("/lib" + libname) == -1 or line.find(".so") == -1:
            continue

        i = line.find(" ")
        if i < 0:
            continue
        line = line[i + 1 :]
        i = line.find(" ")
        if i < 0:
            continue
        line = line[i + 1 :]
        i = line.find(" ")
        if i < 0:
            continue
        line = line[i + 1 :]
        i = line.find(" ")
        if i < 0:
            continue
        line = line[i + 1 :]
        i = line.find(" ")
        if i < 0:
            continue
        line = line[i + 1 :]

        soname = line.lstrip().rstrip("\n")
        if soname.rfind("/lib" + libname) == -1:
            continue

        return soname

    return None


###############################################################################
# find_lib_sunos()
# Parse output of pmap to find an occurrence of libXXX.so.*


def find_lib_sunos(libname):

    pid = os.getpid()
    lines, _ = runexternal_out_and_err("pmap %d" % pid)

    for line in lines.split("\n"):
        if line.rfind("/lib" + libname) == -1 or line.find(".so") == -1:
            continue

        i = line.find("/")
        if i < 0:
            continue
        line = line[i:]

        soname = line.lstrip().rstrip("\n")
        if soname.rfind("/lib" + libname) == -1:
            continue

        return soname

    return None


###############################################################################
# find_lib_windows()
# use Module32First() / Module32Next() API on the current process


def find_lib_windows(libname):

    try:
        import ctypes
    except ImportError:
        return None

    kernel32 = ctypes.windll.kernel32

    MAX_MODULE_NAME32 = 255
    MAX_PATH = 260

    TH32CS_SNAPMODULE = 0x00000008

    class MODULEENTRY32(ctypes.Structure):
        _fields_ = [
            ("dwSize", ctypes.c_int),
            ("th32ModuleID", ctypes.c_int),
            ("th32ProcessID", ctypes.c_int),
            ("GlblcntUsage", ctypes.c_int),
            ("ProccntUsage", ctypes.c_int),
            ("modBaseAddr", ctypes.c_char_p),
            ("modBaseSize", ctypes.c_int),
            ("hModule", ctypes.c_void_p),
            ("szModule", ctypes.c_char * (MAX_MODULE_NAME32 + 1)),
            ("szExePath", ctypes.c_char * MAX_PATH),
        ]

    Module32First = kernel32.Module32First
    Module32First.argtypes = [ctypes.c_void_p, ctypes.POINTER(MODULEENTRY32)]
    Module32First.rettypes = ctypes.c_int

    Module32Next = kernel32.Module32Next
    Module32Next.argtypes = [ctypes.c_void_p, ctypes.POINTER(MODULEENTRY32)]
    Module32Next.rettypes = ctypes.c_int

    CreateToolhelp32Snapshot = kernel32.CreateToolhelp32Snapshot
    CreateToolhelp32Snapshot.argtypes = [ctypes.c_int, ctypes.c_int]
    CreateToolhelp32Snapshot.rettypes = ctypes.c_void_p

    CloseHandle = kernel32.CloseHandle
    CloseHandle.argtypes = [ctypes.c_void_p]
    CloseHandle.rettypes = ctypes.c_int

    GetLastError = kernel32.GetLastError
    GetLastError.argtypes = []
    GetLastError.rettypes = ctypes.c_int

    snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0)
    if snapshot is None:
        return None

    soname = None

    i = 0
    while True:
        entry = MODULEENTRY32()
        entry.dwSize = ctypes.sizeof(MODULEENTRY32)
        pentry = ctypes.pointer(entry)
        if i == 0:
            ret = Module32First(snapshot, pentry)
        else:
            ret = Module32Next(snapshot, pentry)
        i = i + 1
        if ret == 0:
            break

        try:
            path = entry.szExePath.decode("latin1")
        except Exception:
            continue

        i = path.rfind("\\" + libname)
        if i < 0:
            continue
        if path[i + 1 :].find("\\") >= 0:
            continue
        # Avoid matching gdal_PLUGIN.dll
        if path[i + 1 :].find("_") >= 0:
            continue
        soname = path
        break

    CloseHandle(snapshot)

    return soname


###############################################################################
# find_lib()


def find_lib(mylib):
    if sys.platform.startswith("linux"):
        return find_lib_linux(mylib)
    if sys.platform.startswith("sunos"):
        return find_lib_sunos(mylib)
    if sys.platform.startswith("win32"):
        return find_lib_windows(mylib)
    # sorry mac users or other BSDs
    # should be doable
    return None


###############################################################################
# get_opened_files()


get_opened_files_has_warned = False


def get_opened_files():
    if not sys.platform.startswith("linux"):
        return []
    fdpath = "/proc/%d/fd" % os.getpid()
    if not os.path.exists(fdpath):
        global get_opened_files_has_warned
        if not get_opened_files_has_warned:
            get_opened_files_has_warned = True
            print("get_opened_files() not supported due to /proc not being readable")
        return []
    file_numbers = os.listdir(fdpath)
    filenames = []
    for fd in file_numbers:
        try:
            filename = os.readlink("%s/%s" % (fdpath, fd))
            if (
                not filename.startswith("/dev/")
                and not filename.startswith("pipe:")
                and filename.find("proj.db") < 0
            ):
                filenames.append(filename)
        except OSError:
            pass
    return filenames


###############################################################################
# is_file_open()


def is_file_open(filename):
    for got_filename in get_opened_files():
        if filename in got_filename:
            return True
    return False


###############################################################################
# built_against_curl()


def built_against_curl():
    return "CURL_ENABLED=YES" in gdal.VersionInfo("BUILD_INFO")


###############################################################################
# error_handler()
# Allow use of "with" for an ErrorHandler that always pops at the scope close.
# Defaults to suppressing errors and warnings.


@contextlib.contextmanager
def error_handler(error_name="CPLQuietErrorHandler"):
    handler = gdal.PushErrorHandler(error_name)
    try:
        yield handler
    finally:
        gdal.PopErrorHandler()


###############################################################################
# Temporarily define a new value of block cache


@contextlib.contextmanager
def SetCacheMax(val):
    oldval = gdal.GetCacheMax()
    gdal.SetCacheMax(val)
    try:
        yield
    finally:
        gdal.SetCacheMax(oldval)


###############################################################################
# Temporarily enable exceptions for gdal, ogr and osr modules


@contextlib.contextmanager
def enable_exceptions():
    from osgeo import ogr, osr

    with gdal.ExceptionMgr(useExceptions=True), osr.ExceptionMgr(
        useExceptions=True
    ), ogr.ExceptionMgr(useExceptions=True):
        yield


###############################################################################
# Temporarily disable exceptions for gdal, ogr and osr modules


@contextlib.contextmanager
def disable_exceptions():
    from osgeo import ogr, osr

    with gdal.ExceptionMgr(useExceptions=False), osr.ExceptionMgr(
        useExceptions=False
    ), ogr.ExceptionMgr(useExceptions=False):
        yield


###############################################################################
# Temporarily define a configuration option


config_option = gdal.config_option


###############################################################################
# Temporarily define a set of configuration options


config_options = gdal.config_options


###############################################################################
# Temporarily define VSI credentials

credential_keys = set()


@contextlib.contextmanager
def credentials(prefix, options):
    global credential_keys
    # Special processing for nested with credentials() call on the same key
    clear_credentials = prefix not in credential_keys
    credential_keys.add(prefix)
    for key in options:
        gdal.SetCredential(prefix, key, options[key])
    try:
        yield
    finally:
        if clear_credentials:
            credential_keys.remove(prefix)
            gdal.ClearCredentials(prefix)


###############################################################################
# Temporarily create a file


@contextlib.contextmanager
def tempfile(filename, content):
    gdal.FileFromMemBuffer(filename, content)
    try:
        yield
    finally:
        gdal.Unlink(filename)


###############################################################################


def gdalurlopen(url, timeout=10):
    old_timeout = socket.getdefaulttimeout()
    socket.setdefaulttimeout(timeout)
    proxy = None

    if "GDAL_HTTP_PROXY" in os.environ:
        proxy = os.environ["GDAL_HTTP_PROXY"]
        protocol = "http"

    if "GDAL_HTTPS_PROXY" in os.environ and url.startswith("https"):
        proxy = os.environ["GDAL_HTTPS_PROXY"]
        protocol = "https"

    if proxy is not None:
        if "GDAL_HTTP_PROXYUSERPWD" in os.environ:
            proxyuserpwd = os.environ["GDAL_HTTP_PROXYUSERPWD"]
            proxy_handler = urllib.request.ProxyHandler(
                {protocol: f"{protocol}://{proxyuserpwd}@{proxy}"}
            )
        else:
            proxyuserpwd = None
            proxy_handler = urllib.request.ProxyHandler(
                {protocol: f"{protocol}://{proxy}"}
            )

        opener = urllib.request.build_opener(proxy_handler, urllib.request.HTTPHandler)

        urllib.request.install_opener(opener)

    import http.client

    try:
        handle = urllib.request.urlopen(url)
        socket.setdefaulttimeout(old_timeout)
        return handle
    except urllib.error.HTTPError as e:
        print(f"HTTP service for {url} is down (HTTP Error: {e.code})")
        socket.setdefaulttimeout(old_timeout)
        return None
    except urllib.error.ContentTooShortError:
        print(f"HTTP content too short for {url}.")
        socket.setdefaulttimeout(old_timeout)
        return None
    except urllib.error.URLError as e:
        print(f"HTTP service for {url} is down (URL Error: {e.reason})")
        socket.setdefaulttimeout(old_timeout)
        return None
    except socket.timeout:
        print(f"HTTP service for {url} timed out")
        socket.setdefaulttimeout(old_timeout)
        return None
    except http.client.RemoteDisconnected as e:
        print(f"HTTP service for {url} is not available: RemoteDisconnected : {e}")
        socket.setdefaulttimeout(old_timeout)
        return None


def runexternal(
    cmd,
    strin=None,
    check_memleak=True,
    display_live_on_parent_stdout=False,
    encoding="latin1",
):
    # pylint: disable=unused-argument
    if sys.platform == "win32":
        command = cmd
    else:
        command = shlex.split(cmd)
    if strin is None:
        p = subprocess.Popen(command, stdout=subprocess.PIPE)
    else:
        p = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
        p.stdin.write(strin.encode("ascii"))
        p.stdin.close()

    if p.stdout is not None:
        if display_live_on_parent_stdout:
            ret = ""
            ret_stdout = p.stdout
            while True:
                c = ret_stdout.read(1).decode(encoding)
                if c == "":
                    break
                ret = ret + c
                sys.stdout.write(c)
        else:
            ret = p.stdout.read().decode(encoding)
    else:
        ret = ""

    waitcode = p.wait()
    if waitcode != 0:
        ret = f"{ret}\nERROR ret code = {waitcode}"

    return ret


def _read_in_thread(f, q):
    q.put(f.read())
    f.close()


def runexternal_out_and_err(cmd, check_memleak=True, encoding="ascii"):
    # pylint: disable=unused-argument
    if sys.platform == "win32":
        command = cmd
    else:
        command = shlex.split(cmd)
    p = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    if p.stdout is not None:
        q_stdout = Queue()
        t_stdout = Thread(target=_read_in_thread, args=(p.stdout, q_stdout))
        t_stdout.start()
    else:
        q_stdout = None
        ret_stdout = ""

    if p.stderr is not None:
        q_stderr = Queue()
        t_stderr = Thread(target=_read_in_thread, args=(p.stderr, q_stderr))
        t_stderr.start()
    else:
        q_stderr = None
        ret_stderr = ""

    if q_stdout is not None:
        ret_stdout = q_stdout.get().decode(encoding)
    if q_stderr is not None:
        ret_stderr = q_stderr.get().decode(encoding)

    waitcode = p.wait()
    if waitcode != 0:
        ret_stderr = f"{ret_stderr}\nERROR ret code = {waitcode}"

    return (ret_stdout, ret_stderr)


###############################################################################
# Validate JSON according to a JSON schema
#
# "jsn" should be a string or a json object
# "schema" should be a path to file containing a JSON schema. If the
# file is not found relative to the current working directory or GDAL_DATA,
# the test will fail.
def validate_json(jsn, schema):
    __tracebackhide__ = True

    try:
        import jsonschema
    except ImportError:
        pytest.skip("jsonschema module not available")

    if not os.path.exists(schema):
        gdal_data = gdal.GetConfigOption("GDAL_DATA")

        if gdal_data and os.path.exists(os.path.join(gdal_data, schema)):
            schema = os.path.join(gdal_data, schema)
        else:
            pytest.fail(f"Could not find schema {schema}")

    if isinstance(jsn, str):
        jsn = json.loads(jsn)

    schema = json.loads(open(schema, "rb").read())

    if sys.version_info >= (3, 8):
        from importlib.metadata import version

        jsonschema_version = version("jsonschema")
    else:
        from pkg_resources import get_distribution

        jsonschema_version = get_distribution("jsonschema").version

    def versiontuple(v):
        return tuple(map(int, (v.split("."))))

    # jsonschema 4.18 deprecates automatic resolution of "$ref" for security
    # reason. Use a custom retrieve method.
    # Cf https://python-jsonschema.readthedocs.io/en/latest/referencing/#automatically-retrieving-resources-over-http
    if versiontuple(jsonschema_version) >= (4, 18):
        from referencing import Registry, Resource

        def retrieve_remote_file(uri: str):
            if not uri.startswith("http://") and not uri.startswith("https://"):
                raise Exception(f"Cannot retrieve {uri}")
            os.makedirs("tmp/cache", exist_ok=True)
            filename = "tmp/cache/" + os.path.basename(uri)
            if not download_file(uri, filename=filename, force_download=True):
                raise Exception(f"Cannot download {uri}")
            response = open(filename, "rb").read()
            return Resource.from_contents(json.loads(response))

        registry = Registry(retrieve=retrieve_remote_file)
        validator_cls = jsonschema.validators.validator_for(schema)
        validator_cls(schema, registry=registry).validate(jsn)

    else:
        # jsonschema < 4.18
        try:
            jsonschema.validate(instance=jsn, schema=schema)
        except jsonschema.exceptions.RefResolutionError:
            pytest.skip("Failed to resolve remote reference in JSON schema")


###############################################################################
# Close and reopen a dataset


def reopen(ds, update=False, open_options=None):

    ds_loc = ds.GetDescription()
    ds_drv = ds.GetDriver()

    ds.Close()

    flags = 0
    if update:
        flags = gdal.OF_UPDATE

    if open_options is None:
        open_options = {}

    return gdal.OpenEx(
        ds_loc,
        flags,
        allowed_drivers=[ds_drv.GetDescription()],
        open_options=open_options,
    )


def vsi_open(path, mode="r"):
    return gdal.VSIFile(path, mode)


def vrt_has_open_support():
    drv = gdal.GetDriverByName("VRT")
    return drv is not None and drv.GetMetadataItem(gdal.DMD_OPENOPTIONLIST) is not None
