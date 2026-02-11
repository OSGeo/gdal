#!/usr/bin/env python
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test functioning of the ProxyDB PAM metadata support
# Author:   Even Rouault, <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2011, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os
import sys

try:
    os.putenv("CPL_SHOW_MEM_STATS", "")
except OSError:
    pass

# Must to be launched from pam.py/pam_11()
# Test creating a new proxydb
if len(sys.argv) == 2 and sys.argv[1] == "-test1":

    import shutil

    from osgeo import gdal

    try:
        shutil.rmtree("tmppamproxydir")
    except OSError:
        pass
    os.mkdir("tmppamproxydir")

    gdal.SetConfigOption("GDAL_PAM_PROXY_DIR", "tmppamproxydir")

    # Compute statistics. They should be saved in the  .aux.xml in the proxyDB
    ds = gdal.Open("tmpdirreadonly/byte.tif")
    stats = ds.GetRasterBand(1).ComputeStatistics(False)
    gdal.ErrorReset()
    ds = None
    error_msg = gdal.GetLastErrorMsg()
    if error_msg != "":
        print("did not expected error message")
        sys.exit(1)

    # Check that the .aux.xml in the proxyDB exists
    filelist = gdal.ReadDir("tmppamproxydir")
    if "000000_tmpdirreadonly_byte.tif.aux.xml" not in filelist:
        print("did not get find 000000_tmpdirreadonly_byte.tif.aux.xml on filesystem")
        sys.exit(1)

    # Test altering a value to check that the file will be used
    f = open("tmppamproxydir/000000_tmpdirreadonly_byte.tif.aux.xml", "w")
    f.write("""<PAMDataset>
  <PAMRasterBand band="1">
    <Metadata>
      <MDI key="STATISTICS_MAXIMUM">255</MDI>
      <MDI key="STATISTICS_MEAN">126.765</MDI>
      <MDI key="STATISTICS_MINIMUM">-9999</MDI>
      <MDI key="STATISTICS_STDDEV">22.928470838676</MDI>
    </Metadata>
  </PAMRasterBand>
</PAMDataset>""")
    f.close()

    ds = gdal.Open("tmpdirreadonly/byte.tif")
    filelist = ds.GetFileList()
    if len(filelist) != 2:
        print(
            "did not get find 000000_tmpdirreadonly_byte.tif.aux.xml in dataset GetFileList()"
        )
        print(filelist)
        sys.exit(1)

    stats = ds.GetRasterBand(1).GetStatistics(False, False)
    if stats[0] != -9999:
        print("did not get expected minimum")
        sys.exit(1)
    ds = None

    # Check that proxy overviews work
    ds = gdal.Open("tmpdirreadonly/byte.tif")
    gdal.PushErrorHandler()
    assert ds.BuildOverviews("NEAR", overviewlist=[2]) == gdal.CE_None
    gdal.PopErrorHandler()
    ds = None

    filelist = gdal.ReadDir("tmppamproxydir")
    if "000001_tmpdirreadonly_byte.tif.ovr" not in filelist:
        print("did not get find 000001_tmpdirreadonly_byte.tif.ovr")
        sys.exit(1)

    ds = gdal.Open("tmpdirreadonly/byte.tif")
    filelist = ds.GetFileList()
    if len(filelist) != 3:
        print(
            "did not get find 000001_tmpdirreadonly_byte.tif.ovr in dataset GetFileList()"
        )
        print(filelist)
        sys.exit(1)
    nb_ovr = ds.GetRasterBand(1).GetOverviewCount()
    ds = None

    if nb_ovr != 1:
        print("did not get expected overview count")
        sys.exit(1)

    print("success")

    sys.exit(0)

# Must to be launched from pam.py/pam_11()
# Test loading an existing proxydb
if len(sys.argv) == 2 and sys.argv[1] == "-test2":

    from osgeo import gdal

    gdal.SetConfigOption("GDAL_PAM_PROXY_DIR", "tmppamproxydir")

    ds = gdal.Open("tmpdirreadonly/byte.tif")
    filelist = ds.GetFileList()
    if len(filelist) != 3:
        print(
            "did not get find 000000_tmpdirreadonly_byte.tif.aux.xml and/or 000001_tmpdirreadonly_byte.tif.ovr in dataset GetFileList()"
        )
        print(filelist)
        sys.exit(1)

    stats = ds.GetRasterBand(1).GetStatistics(False, False)
    if stats[0] != -9999:
        print("did not get expected minimum")
        sys.exit(1)

    nb_ovr = ds.GetRasterBand(1).GetOverviewCount()
    ds = None

    if nb_ovr != 1:
        print("did not get expected overview count")
        sys.exit(1)

    print("success")

    sys.exit(0)
