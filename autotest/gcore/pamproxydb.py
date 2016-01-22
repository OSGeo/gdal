#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test functioning of the ProxyDB PAM metadata support
# Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
#
###############################################################################
# Copyright (c) 2011, Even Rouault <even dot rouault at mines-paris dot org>
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
import os
try:
    os.putenv('CPL_SHOW_MEM_STATS', '')
except:
    pass

# Must to be launched from pam.py/pam_11()
# Test creating a new proxydb
if len(sys.argv) == 2 and sys.argv[1] == '-test1':

    from osgeo import gdal
    import os
    import shutil

    try:
        shutil.rmtree('tmppamproxydir')
    except:
        pass
    os.mkdir('tmppamproxydir')

    gdal.SetConfigOption('GDAL_PAM_PROXY_DIR', 'tmppamproxydir')

    # Compute statistics. They should be saved in the  .aux.xml in the proxyDB
    ds = gdal.Open('tmpdirreadonly/byte.tif')
    stats = ds.GetRasterBand(1).ComputeStatistics(False)
    gdal.ErrorReset()
    ds = None
    error_msg = gdal.GetLastErrorMsg()
    if error_msg != '':
        print('did not expected error message')
        sys.exit(1)

    # Check that the .aux.xml in the proxyDB exists
    filelist = gdal.ReadDir('tmppamproxydir')
    if not '000000_tmpdirreadonly_byte.tif.aux.xml' in filelist:
        print('did not get find 000000_tmpdirreadonly_byte.tif.aux.xml on filesystem')
        sys.exit(1)

    # Test altering a value to check that the file will be used
    f = open('tmppamproxydir/000000_tmpdirreadonly_byte.tif.aux.xml', 'w')
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

    ds = gdal.Open('tmpdirreadonly/byte.tif')
    filelist = ds.GetFileList()
    if len(filelist) != 2:
        print('did not get find 000000_tmpdirreadonly_byte.tif.aux.xml in dataset GetFileList()')
        print(filelist)
        sys.exit(1)

    stats = ds.GetRasterBand(1).GetStatistics(False, False)
    if stats[0] != -9999:
        print('did not get expected minimum')
        sys.exit(1)
    ds = None

    # Check that proxy overviews work
    ds = gdal.Open('tmpdirreadonly/byte.tif')
    ds.BuildOverviews('NEAR', overviewlist = [2])
    ds = None

    filelist = gdal.ReadDir('tmppamproxydir')
    if not '000001_tmpdirreadonly_byte.tif.ovr' in filelist:
        print('did not get find 000001_tmpdirreadonly_byte.tif.ovr')
        sys.exit(1)

    ds = gdal.Open('tmpdirreadonly/byte.tif')
    filelist = ds.GetFileList()
    if len(filelist) != 3:
        print('did not get find 000001_tmpdirreadonly_byte.tif.ovr in dataset GetFileList()')
        print(filelist)
        sys.exit(1)
    nb_ovr = ds.GetRasterBand(1).GetOverviewCount()
    ds = None

    if nb_ovr != 1:
        print('did not get expected overview count')
        sys.exit(1)

    print('success')

    sys.exit(0)

# Must to be launched from pam.py/pam_11()
# Test loading an existing proxydb
if len(sys.argv) == 2 and sys.argv[1] == '-test2':

    from osgeo import gdal

    gdal.SetConfigOption('GDAL_PAM_PROXY_DIR', 'tmppamproxydir')

    ds = gdal.Open('tmpdirreadonly/byte.tif')
    filelist = ds.GetFileList()
    if len(filelist) != 3:
        print('did not get find 000000_tmpdirreadonly_byte.tif.aux.xml and/or 000001_tmpdirreadonly_byte.tif.ovr in dataset GetFileList()')
        print(filelist)
        sys.exit(1)

    stats = ds.GetRasterBand(1).GetStatistics(False, False)
    if stats[0] != -9999:
        print('did not get expected minimum')
        sys.exit(1)

    nb_ovr = ds.GetRasterBand(1).GetOverviewCount()
    ds = None

    if nb_ovr != 1:
        print('did not get expected overview count')
        sys.exit(1)

    print('success')

    sys.exit(0)
