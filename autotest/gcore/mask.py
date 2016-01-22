#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test RFC 15 "mask band" default functionality (nodata/alpha/etc)
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008-2012, Even Rouault <even dot rouault at mines-paris dot org>
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

import os
import sys

sys.path.append( '../pymod' )

import gdaltest
from osgeo import gdal
from osgeo import gdalconst

###############################################################################
# Verify the checksum and flags for "all valid" case.

def mask_1():

    try:
        x = gdalconst.GMF_ALL_VALID
        gdaltest.have_ng = 1
    except:
        gdaltest.have_ng = 0
        return 'skip'

    ds = gdal.Open('data/byte.tif')
    
    if ds is None:
        gdaltest.post_reason( 'Failed to open test dataset.' )
        return 'fail'

    band = ds.GetRasterBand(1)

    if band.GetMaskFlags() != gdal.GMF_ALL_VALID:
        gdaltest.post_reason( 'Did not get expected mask.' )
        return 'fail'

    cs = band.GetMaskBand().Checksum()
    if cs != 4873:
        gdaltest.post_reason( 'Got wrong mask checksum' )
        print(cs)
        return 'fail'
    
    return 'success' 

###############################################################################
# Verify the checksum and flags for "nodata" case.

def mask_2():

    if gdaltest.have_ng == 0:
        return 'skip'

    ds = gdal.Open('data/byte.vrt')
    
    if ds is None:
        gdaltest.post_reason( 'Failed to open test dataset.' )
        return 'fail'

    band = ds.GetRasterBand(1)

    if band.GetMaskFlags() != gdal.GMF_NODATA:
        gdaltest.post_reason( 'Did not get expected mask.' )
        return 'fail'

    cs = band.GetMaskBand().Checksum()
    if cs != 4209:
        gdaltest.post_reason( 'Got wrong mask checksum' )
        print(cs)
        return 'fail'
    
    return 'success' 

###############################################################################
# Verify the checksum and flags for "alpha" case.

def mask_3():

    if gdaltest.have_ng == 0:
        return 'skip'

    ds = gdal.Open( 'data/stefan_full_rgba.png' )

    if ds is None:
        gdaltest.post_reason( 'Failed to open test dataset.' )
        return 'fail'

    # Test first mask.
    
    band = ds.GetRasterBand(1)

    if band.GetMaskFlags() != gdal.GMF_ALPHA + gdal.GMF_PER_DATASET:
        gdaltest.post_reason( 'Did not get expected mask.' )
        return 'fail'

    cs = band.GetMaskBand().Checksum()
    if cs != 10807:
        gdaltest.post_reason( 'Got wrong mask checksum' )
        print(cs)
        return 'fail'

    # Verify second and third same as first.

    band_2 = ds.GetRasterBand(2)
    band_3 = ds.GetRasterBand(3)
    
    # We have commented the following tests as SWIG >= 1.3.37 is buggy !
    #  or str(band_2.GetMaskBand()) != str(band.GetMaskBand()) \
    #   or str(band_3.GetMaskBand()) != str(band.GetMaskBand())
    if band_2.GetMaskFlags() != band.GetMaskFlags() \
       or band_3.GetMaskFlags() != band.GetMaskFlags():
        gdaltest.post_reason( 'Band 2 or 3 does not seem to match first mask')
        return 'fail'

    # Verify alpha has no mask.
    band = ds.GetRasterBand(4)
    if band.GetMaskFlags() != gdal.GMF_ALL_VALID:
        gdaltest.post_reason( 'Did not get expected mask for alpha.' )
        return 'fail'

    cs = band.GetMaskBand().Checksum()
    if cs != 36074:
        gdaltest.post_reason( 'Got wrong alpha mask checksum' )
        print(cs)
        return 'fail'
    
    return 'success' 

###############################################################################
# Copy a *real* masked dataset, and confirm masks copied properly.

def mask_4():

    if gdaltest.have_ng == 0:
        return 'skip'

    src_ds = gdal.Open('../gdrivers/data/masked.jpg')
    
    if src_ds is None:
        gdaltest.post_reason( 'Failed to open test dataset.' )
        return 'fail'

    # NOTE: for now we copy to PNM since it does everything (overviews too)
    # externally. Should eventually test with gtiff, hfa.
    drv = gdal.GetDriverByName('PNM')
    ds = drv.CreateCopy( 'tmp/mask_4.pnm', src_ds )
    src_ds = None

    # confirm we got the custom mask on the copied dataset.
    if ds.GetRasterBand(1).GetMaskFlags() != gdal.GMF_PER_DATASET:
        gdaltest.post_reason( 'did not get expected mask flags' )
        print(ds.GetRasterBand(1).GetMaskFlags())
        return 'fail'

    msk = ds.GetRasterBand(1).GetMaskBand()
    cs = msk.Checksum()
    expected_cs = 770
    
    if cs != expected_cs:
        gdaltest.post_reason( 'Did not get expected checksum' )
        print(cs)
        return 'fail'

    msk = None
    ds = None

    return 'success'

###############################################################################
# Create overviews for masked file, and verify the overviews have proper
# masks built for them.

def mask_5():

    # This crashes with libtiff 3.8.2, so skip it
    md = gdal.GetDriverByName('GTiff').GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('BigTIFF') == -1:
        return 'skip'

    if gdaltest.have_ng == 0:
        return 'skip'

    ds = gdal.Open('tmp/mask_4.pnm',gdal.GA_Update)

    if ds is None:
        gdaltest.post_reason( 'Failed to open test dataset.' )
        return 'fail'

    # so that we instanciate the mask band before
    ds.GetRasterBand(1).GetMaskFlags()

    ds.BuildOverviews( overviewlist = [2,4] )

    # confirm mask flags on overview.
    ovr = ds.GetRasterBand(1).GetOverview(1)
    
    if ovr.GetMaskFlags() != gdal.GMF_PER_DATASET:
        gdaltest.post_reason( 'did not get expected mask flags' )
        print(ovr.GetMaskFlags())
        return 'fail'

    msk = ovr.GetMaskBand()
    cs = msk.Checksum()
    expected_cs = 20505
    
    if cs != expected_cs:
        gdaltest.post_reason( 'Did not get expected checksum' )
        print(cs)
        return 'fail'
    ovr = None
    msk = None
    ds = None

    # Reopen and confirm we still get same results.
    ds = gdal.Open('tmp/mask_4.pnm')
    
    # confirm mask flags on overview.
    ovr = ds.GetRasterBand(1).GetOverview(1)
    
    if ovr.GetMaskFlags() != gdal.GMF_PER_DATASET:
        gdaltest.post_reason( 'did not get expected mask flags' )
        print(ovr.GetMaskFlags())
        return 'fail'

    msk = ovr.GetMaskBand()
    cs = msk.Checksum()
    expected_cs = 20505
    
    if cs != expected_cs:
        gdaltest.post_reason( 'Did not get expected checksum' )
        print(cs)
        return 'fail'

    ovr = None
    msk = None
    ds = None

    gdal.GetDriverByName('PNM').Delete( 'tmp/mask_4.pnm' )

    return 'success'

###############################################################################
# Test a TIFF file with 1 band and an embedded mask of 1 bit

def mask_6():

    if gdaltest.have_ng == 0:
        return 'skip'

    gdal.SetConfigOption('GDAL_TIFF_INTERNAL_MASK_TO_8BIT', 'FALSE')
    ds = gdal.Open('data/test_with_mask_1bit.tif')
    
    if ds is None:
        gdaltest.post_reason( 'Failed to open test dataset.' )
        return 'fail'

    band = ds.GetRasterBand(1)

    if band.GetMaskFlags() != gdal.GMF_PER_DATASET:
        gdaltest.post_reason( 'Did not get expected mask.' )
        return 'fail'

    cs = band.GetMaskBand().Checksum()
    if cs != 100:
        gdaltest.post_reason( 'Got wrong mask checksum' )
        print(cs)
        return 'fail'

    gdal.SetConfigOption('GDAL_TIFF_INTERNAL_MASK_TO_8BIT', 'TRUE')

    return 'success' 


###############################################################################
# Test a TIFF file with 3 bands and an embedded mask of 1 band of 1 bit

def mask_7():

    if gdaltest.have_ng == 0:
        return 'skip'

    gdal.SetConfigOption('GDAL_TIFF_INTERNAL_MASK_TO_8BIT', 'FALSE')
    ds = gdal.Open('data/test3_with_1mask_1bit.tif')
    
    if ds is None:
        gdaltest.post_reason( 'Failed to open test dataset.' )
        return 'fail'

    for i in (1,2,3):
        band = ds.GetRasterBand(i)

        if band.GetMaskFlags() != gdal.GMF_PER_DATASET:
            gdaltest.post_reason( 'Did not get expected mask.' )
            return 'fail'

        cs = band.GetMaskBand().Checksum()
        if cs != 100:
            gdaltest.post_reason( 'Got wrong mask checksum' )
            print(cs)
            return 'fail'

    gdal.SetConfigOption('GDAL_TIFF_INTERNAL_MASK_TO_8BIT', 'TRUE')

    return 'success' 

###############################################################################
# Test a TIFF file with 1 band and an embedded mask of 8 bit.
# Note : The TIFF6 specification, page 37, only allows 1 BitsPerSample && 1 SamplesPerPixel,

def mask_8():

    if gdaltest.have_ng == 0:
        return 'skip'

    ds = gdal.Open('data/test_with_mask_8bit.tif')
    
    if ds is None:
        gdaltest.post_reason( 'Failed to open test dataset.' )
        return 'fail'

    band = ds.GetRasterBand(1)

    if band.GetMaskFlags() != gdal.GMF_PER_DATASET:
        gdaltest.post_reason( 'Did not get expected mask.' )
        return 'fail'

    cs = band.GetMaskBand().Checksum()
    if cs != 1222:
        gdaltest.post_reason( 'Got wrong mask checksum' )
        print(cs)
        return 'fail'

    return 'success' 

###############################################################################
# Test a TIFF file with 3 bands with an embedded mask of 1 bit with 3 bands.
# Note : The TIFF6 specification, page 37, only allows 1 BitsPerSample && 1 SamplesPerPixel,

def mask_9():

    if gdaltest.have_ng == 0:
        return 'skip'

    ds = gdal.Open('data/test3_with_mask_1bit.tif')
    
    if ds is None:
        gdaltest.post_reason( 'Failed to open test dataset.' )
        return 'fail'

    for i in (1,2,3):
        band = ds.GetRasterBand(i)

        if band.GetMaskFlags() != 0:
            gdaltest.post_reason( 'Did not get expected mask.' )
            return 'fail'

        cs = band.GetMaskBand().Checksum()
        if cs != 100:
            gdaltest.post_reason( 'Got wrong mask checksum' )
            print(cs)
            return 'fail'

    return 'success' 

###############################################################################
# Test a TIFF file with 3 bands with an embedded mask of 8 bit with 3 bands.
# Note : The TIFF6 specification, page 37, only allows 1 BitsPerSample && 1 SamplesPerPixel,

def mask_10():

    if gdaltest.have_ng == 0:
        return 'skip'

    ds = gdal.Open('data/test3_with_mask_8bit.tif')
    
    if ds is None:
        gdaltest.post_reason( 'Failed to open test dataset.' )
        return 'fail'

    for i in (1,2,3):
        band = ds.GetRasterBand(i)

        if band.GetMaskFlags() != 0:
            gdaltest.post_reason( 'Did not get expected mask.' )
            return 'fail'

        cs = band.GetMaskBand().Checksum()
        if cs != 1222:
            gdaltest.post_reason( 'Got wrong mask checksum' )
            print(cs)
            return 'fail'

    return 'success' 

###############################################################################
# Test a TIFF file with an overview, an embedded mask of 1 bit, and an embedded
# mask for the overview

def mask_11():

    if gdaltest.have_ng == 0:
        return 'skip'

    gdal.SetConfigOption('GDAL_TIFF_INTERNAL_MASK_TO_8BIT', 'FALSE')
    ds = gdal.Open('data/test_with_mask_1bit_and_ovr.tif')
    
    if ds is None:
        gdaltest.post_reason( 'Failed to open test dataset.' )
        return 'fail'

    band = ds.GetRasterBand(1)

    # Let's fetch the mask
    if band.GetMaskFlags() != gdal.GMF_PER_DATASET:
        gdaltest.post_reason( 'Did not get expected mask.' )
        return 'fail'

    cs = band.GetMaskBand().Checksum()
    if cs != 100:
        gdaltest.post_reason( 'Got wrong mask checksum' )
        print(cs)
        return 'fail'

    # Let's fetch the overview
    band = ds.GetRasterBand(1).GetOverview(0)
    cs = band.Checksum()
    if cs != 1126:
        gdaltest.post_reason( 'Got wrong overview checksum' )
        print(cs)
        return 'fail'

    # Let's fetch the mask of the overview
    if band.GetMaskFlags() != gdal.GMF_PER_DATASET:
        gdaltest.post_reason( 'Did not get expected mask.' )
        return 'fail'

    cs = band.GetMaskBand().Checksum()
    if cs != 25:
        gdaltest.post_reason( 'Got wrong checksum for the mask of the overview' )
        print(cs)
        return 'fail'

    # Let's fetch the overview of the mask == the mask of the overview
    band = ds.GetRasterBand(1).GetMaskBand().GetOverview(0)
    cs = band.Checksum()
    if cs != 25:
        gdaltest.post_reason( 'Got wrong checksum for the overview of the mask' )
        print(cs)
        return 'fail'

    gdal.SetConfigOption('GDAL_TIFF_INTERNAL_MASK_TO_8BIT', 'TRUE')
    
    return 'success' 


###############################################################################
# Test a TIFF file with 3 bands, an overview, an embedded mask of 1 bit, and an embedded
# mask for the overview

def mask_12():

    if gdaltest.have_ng == 0:
        return 'skip'

    gdal.SetConfigOption('GDAL_TIFF_INTERNAL_MASK_TO_8BIT', 'FALSE')
    ds = gdal.Open('data/test3_with_mask_1bit_and_ovr.tif')

    if ds is None:
        gdaltest.post_reason( 'Failed to open test dataset.' )
        return 'fail'

    for i in (1,2,3):
        band = ds.GetRasterBand(i)

        # Let's fetch the mask
        if band.GetMaskFlags() != gdal.GMF_PER_DATASET:
            gdaltest.post_reason( 'Did not get expected mask.' )
            return 'fail'

        cs = band.GetMaskBand().Checksum()
        if cs != 100:
            gdaltest.post_reason( 'Got wrong mask checksum' )
            print(cs)
            return 'fail'

        # Let's fetch the overview
        band = ds.GetRasterBand(i).GetOverview(0)
        cs = band.Checksum()
        if cs != 1126:
            gdaltest.post_reason( 'Got wrong overview checksum' )
            print(cs)
            return 'fail'

        # Let's fetch the mask of the overview
        if band.GetMaskFlags() != gdal.GMF_PER_DATASET:
            gdaltest.post_reason( 'Did not get expected mask.' )
            return 'fail'

        cs = band.GetMaskBand().Checksum()
        if cs != 25:
            gdaltest.post_reason( 'Got wrong checksum for the mask of the overview' )
            print(cs)
            return 'fail'

        # Let's fetch the overview of the mask == the mask of the overview
        band = ds.GetRasterBand(i).GetMaskBand().GetOverview(0)
        cs = band.Checksum()
        if cs != 25:
            gdaltest.post_reason( 'Got wrong checksum for the overview of the mask' )
            print(cs)
            return 'fail'

    gdal.SetConfigOption('GDAL_TIFF_INTERNAL_MASK_TO_8BIT', 'TRUE')

    return 'success' 

###############################################################################
# Test creation of external TIFF mask band

def mask_13():

    if gdaltest.have_ng == 0:
        return 'skip'

    gdal.SetConfigOption('GDAL_TIFF_INTERNAL_MASK','NO')

    src_ds = gdal.Open('data/byte.tif')

    if src_ds is None:
        gdaltest.post_reason( 'Failed to open test dataset.' )
        return 'fail'

    drv = gdal.GetDriverByName('GTiff')
    ds = drv.CreateCopy( 'tmp/byte_with_mask.tif', src_ds )
    src_ds = None

    ds.CreateMaskBand(gdal.GMF_PER_DATASET)

    cs = ds.GetRasterBand(1).GetMaskBand().Checksum()
    if cs != 0:
        print(cs)
        gdaltest.post_reason( 'Got wrong checksum for the the mask' )
        return 'fail'

    ds.GetRasterBand(1).GetMaskBand().Fill(1)

    cs = ds.GetRasterBand(1).GetMaskBand().Checksum()
    if cs != 400:
        print(cs)
        gdaltest.post_reason( 'Got wrong checksum for the the mask' )
        return 'fail'

    ds = None

    try:
        os.stat('tmp/byte_with_mask.tif.msk')
    except:
        gdaltest.post_reason( 'tmp/byte_with_mask.tif.msk is absent' )
        return 'fail'

    ds = gdal.Open('tmp/byte_with_mask.tif')

    if ds.GetRasterBand(1).GetMaskFlags() != gdal.GMF_PER_DATASET:
        gdaltest.post_reason( 'wrong mask flags' )
        return 'fail'

    cs = ds.GetRasterBand(1).GetMaskBand().Checksum()
    if cs != 400:
        print(cs)
        gdaltest.post_reason( 'Got wrong checksum for the the mask' )
        return 'fail'

    ds = None

    drv.Delete( 'tmp/byte_with_mask.tif' )

    try:
        os.stat('tmp/byte_with_mask.tif.msk')
        gdaltest.post_reason( 'tmp/byte_with_mask.tif.msk is still there' )
        return 'fail'
    except:
        pass

    return 'success' 

###############################################################################
# Test creation of internal TIFF mask band

def mask_14():

    if gdaltest.have_ng == 0:
        return 'skip'

    src_ds = gdal.Open('data/byte.tif')

    if src_ds is None:
        gdaltest.post_reason( 'Failed to open test dataset.' )
        return 'fail'

    drv = gdal.GetDriverByName('GTiff')
    gdal.SetConfigOption('GDAL_TIFF_INTERNAL_MASK_TO_8BIT', 'FALSE')
    ds = drv.CreateCopy( 'tmp/byte_with_mask.tif', src_ds )
    gdal.SetConfigOption('GDAL_TIFF_INTERNAL_MASK_TO_8BIT', 'TRUE')
    src_ds = None

    gdal.SetConfigOption('GDAL_TIFF_INTERNAL_MASK', 'YES')
    ds.CreateMaskBand(gdal.GMF_PER_DATASET)
    gdal.SetConfigOption('GDAL_TIFF_INTERNAL_MASK', 'NO')

    cs = ds.GetRasterBand(1).GetMaskBand().Checksum()
    if cs != 0:
        print(cs)
        gdaltest.post_reason( 'Got wrong checksum for the the mask (1)' )
        return 'fail'

    ds.GetRasterBand(1).GetMaskBand().Fill(1)

    cs = ds.GetRasterBand(1).GetMaskBand().Checksum()
    if cs != 400:
        print(cs)
        gdaltest.post_reason( 'Got wrong checksum for the the mask (2)' )
        return 'fail'

    ds = None

    try:
        os.stat('tmp/byte_with_mask.tif.msk')
        gdaltest.post_reason( 'tmp/byte_with_mask.tif.msk shouldn not exist' )
        return 'fail'
    except:
        pass

    gdal.SetConfigOption('GDAL_TIFF_INTERNAL_MASK_TO_8BIT', 'FALSE')
    ds = gdal.Open('tmp/byte_with_mask.tif')

    if ds.GetRasterBand(1).GetMaskFlags() != gdal.GMF_PER_DATASET:
        gdaltest.post_reason( 'wrong mask flags' )
        return 'fail'

    gdal.SetConfigOption('GDAL_TIFF_INTERNAL_MASK_TO_8BIT', 'TRUE')
    
    cs = ds.GetRasterBand(1).GetMaskBand().Checksum()
    if cs != 400:
        print(cs)
        gdaltest.post_reason( 'Got wrong checksum for the the mask (3)' )
        return 'fail'

    ds = None

    drv.Delete( 'tmp/byte_with_mask.tif' )

    return 'success' 

###############################################################################
# Test creation of internal TIFF overview, mask band and mask band of overview

def mask_and_ovr(order, method):

    if gdaltest.have_ng == 0:
        return 'skip'

    src_ds = gdal.Open('data/byte.tif')

    if src_ds is None:
        gdaltest.post_reason( 'Failed to open test dataset.' )
        return 'fail'

    drv = gdal.GetDriverByName('GTiff')
    ds = drv.CreateCopy( 'tmp/byte_with_ovr_and_mask.tif', src_ds )
    src_ds = None

    if order == 1:
        gdal.SetConfigOption('GDAL_TIFF_INTERNAL_MASK', 'YES')
        ds.CreateMaskBand(gdal.GMF_PER_DATASET)
        gdal.SetConfigOption('GDAL_TIFF_INTERNAL_MASK', 'NO')
        ds.BuildOverviews( method, overviewlist = [2, 4] )
        gdal.SetConfigOption('GDAL_TIFF_INTERNAL_MASK', 'YES')
        ds.GetRasterBand(1).GetOverview(0).CreateMaskBand(gdal.GMF_PER_DATASET)
        ds.GetRasterBand(1).GetOverview(1).CreateMaskBand(gdal.GMF_PER_DATASET)
        gdal.SetConfigOption('GDAL_TIFF_INTERNAL_MASK', 'NO')
    elif order == 2:
        gdal.SetConfigOption('GDAL_TIFF_INTERNAL_MASK', 'YES')
        ds.BuildOverviews( method, overviewlist = [2, 4] )
        ds.CreateMaskBand(gdal.GMF_PER_DATASET)
        ds.GetRasterBand(1).GetOverview(0).CreateMaskBand(gdal.GMF_PER_DATASET)
        ds.GetRasterBand(1).GetOverview(1).CreateMaskBand(gdal.GMF_PER_DATASET)
        gdal.SetConfigOption('GDAL_TIFF_INTERNAL_MASK', 'NO')
    elif order == 3:
        gdal.SetConfigOption('GDAL_TIFF_INTERNAL_MASK', 'YES')
        ds.BuildOverviews( method, overviewlist = [2, 4] )
        ds.GetRasterBand(1).GetOverview(0).CreateMaskBand(gdal.GMF_PER_DATASET)
        ds.GetRasterBand(1).GetOverview(1).CreateMaskBand(gdal.GMF_PER_DATASET)
        ds.CreateMaskBand(gdal.GMF_PER_DATASET)
        gdal.SetConfigOption('GDAL_TIFF_INTERNAL_MASK', 'NO')
    elif order == 4:
        gdal.SetConfigOption('GDAL_TIFF_INTERNAL_MASK', 'YES')
        ds.CreateMaskBand(gdal.GMF_PER_DATASET)
        ds.GetRasterBand(1).GetMaskBand().Fill(1)
        # The overview for the mask will be implicitely created and computed
        ds.BuildOverviews( method, overviewlist = [2, 4] )
        gdal.SetConfigOption('GDAL_TIFF_INTERNAL_MASK', 'NO')

    if order < 4:
        ds = None
        ds = gdal.Open('tmp/byte_with_ovr_and_mask.tif', gdal.GA_Update)
        ds.GetRasterBand(1).GetMaskBand().Fill(1)
        # The overview of the mask will be implictely recomputed
        ds.BuildOverviews( method, overviewlist = [2, 4] )

    ds = None

    try:
        os.stat('tmp/byte_with_ovr_and_mask.tif.msk')
        gdaltest.post_reason( 'tmp/byte_with_mask.tif.msk shouldn not exist' )
        return 'fail'
    except:
        pass

    gdal.SetConfigOption('GDAL_TIFF_INTERNAL_MASK_TO_8BIT', 'FALSE')
    ds = gdal.Open('tmp/byte_with_ovr_and_mask.tif')

    if ds.GetRasterBand(1).GetMaskFlags() != gdal.GMF_PER_DATASET:
        gdaltest.post_reason( 'wrong mask flags' )
        return 'fail'

    gdal.SetConfigOption('GDAL_TIFF_INTERNAL_MASK_TO_8BIT', 'TRUE')

    cs = ds.GetRasterBand(1).GetMaskBand().Checksum()
    if cs != 400:
        print(cs)
        gdaltest.post_reason( 'Got wrong checksum for the the mask' )
        return 'fail'

    cs = ds.GetRasterBand(1).GetOverview(0).GetMaskBand().Checksum()
    if cs != 100:
        print(cs)
        gdaltest.post_reason( 'Got wrong checksum for the the mask of the first overview' )
        return 'fail'

    cs = ds.GetRasterBand(1).GetOverview(1).GetMaskBand().Checksum()
    if cs != 25:
        print(cs)
        gdaltest.post_reason( 'Got wrong checksum for the the mask of the second overview' )
        return 'fail'

    ds = None

    drv.Delete( 'tmp/byte_with_ovr_and_mask.tif' )

    return 'success' 


def mask_15():
    return mask_and_ovr(1, 'NEAREST')

def mask_16():
    return mask_and_ovr(2, 'NEAREST')

def mask_17():
    return mask_and_ovr(3, 'NEAREST')

def mask_18():
    return mask_and_ovr(4, 'NEAREST')

def mask_15_avg():
    return mask_and_ovr(1, 'AVERAGE')

def mask_16_avg():
    return mask_and_ovr(2, 'AVERAGE')

def mask_17_avg():
    return mask_and_ovr(3, 'AVERAGE')

def mask_18_avg():
    return mask_and_ovr(4, 'AVERAGE')
    
###############################################################################
# Test NODATA_VALUES mask

def mask_19():

    if gdaltest.have_ng == 0:
        return 'skip'

    ds = gdal.Open('data/test_nodatavalues.tif')

    if ds is None:
        gdaltest.post_reason( 'Failed to open test dataset.' )
        return 'fail'

    if ds.GetRasterBand(1).GetMaskFlags() != gdal.GMF_PER_DATASET + gdal.GMF_NODATA:
        gdaltest.post_reason( 'did not get expected mask flags' )
        print(ds.GetRasterBand(1).GetMaskFlags())
        return 'fail'

    msk = ds.GetRasterBand(1).GetMaskBand()
    cs = msk.Checksum()
    expected_cs = 11043

    if cs != expected_cs:
        gdaltest.post_reason( 'Did not get expected checksum' )
        print(cs)
        return 'fail'

    msk = None
    ds = None

    return 'success'

###############################################################################
# Extensive test of nodata mask for all data types

def mask_20():

    if gdaltest.have_ng == 0:
        return 'skip'

    types = [ gdal.GDT_Byte, gdal.GDT_Int16, gdal.GDT_UInt16,
              gdal.GDT_Int32, gdal.GDT_UInt32, gdal.GDT_Float32, gdal.GDT_Float64,
              gdal.GDT_CFloat32, gdal.GDT_CFloat64 ]

    nodatavalue = [ 1, -1, 1, -1, 1, 0.5, 0.5, 0.5, 0.5 ]

    drv = gdal.GetDriverByName('GTiff')
    for i in range(len(types)):
        ds = drv.Create('tmp/mask20.tif', 1, 1, 1, types[i])
        ds.GetRasterBand(1).Fill(nodatavalue[i])
        ds.GetRasterBand(1).SetNoDataValue(nodatavalue[i])

        if ds.GetRasterBand(1).GetMaskFlags() != gdal.GMF_NODATA:
            gdaltest.post_reason( 'did not get expected mask flags for type %s' % gdal.GetDataTypeName(types[i]) )
            return 'fail'

        msk = ds.GetRasterBand(1).GetMaskBand()
        if msk.Checksum() != 0:
            gdaltest.post_reason( 'did not get expected mask checksum for type %s : %d' % (gdal.GetDataTypeName(types[i]), msk.Checksum()) )
            return 'fail'

        msk = None
        ds = None
        drv.Delete('tmp/mask20.tif')

    return 'success'

###############################################################################
# Extensive test of NODATA_VALUES mask for all data types

def mask_21():

    if gdaltest.have_ng == 0:
        return 'skip'

    types = [ gdal.GDT_Byte, gdal.GDT_Int16, gdal.GDT_UInt16,
              gdal.GDT_Int32, gdal.GDT_UInt32, gdal.GDT_Float32, gdal.GDT_Float64,
              gdal.GDT_CFloat32, gdal.GDT_CFloat64 ]

    nodatavalue = [ 1, -1, 1, -1, 1, 0.5, 0.5, 0.5, 0.5 ]

    drv = gdal.GetDriverByName('GTiff')
    for i in range(len(types)):
        ds = drv.Create('tmp/mask21.tif', 1, 1, 3, types[i])
        md = {}
        md['NODATA_VALUES'] = '%f %f %f' % (nodatavalue[i], nodatavalue[i], nodatavalue[i])
        ds.SetMetadata(md)
        ds.GetRasterBand(1).Fill(nodatavalue[i])
        ds.GetRasterBand(2).Fill(nodatavalue[i])
        ds.GetRasterBand(3).Fill(nodatavalue[i])

        if ds.GetRasterBand(1).GetMaskFlags() != gdal.GMF_PER_DATASET + gdal.GMF_NODATA:
            gdaltest.post_reason( 'did not get expected mask flags for type %s' % gdal.GetDataTypeName(types[i]) )
            return 'fail'

        msk = ds.GetRasterBand(1).GetMaskBand()
        if msk.Checksum() != 0:
            gdaltest.post_reason( 'did not get expected mask checksum for type %s : %d' % (gdal.GetDataTypeName(types[i]), msk.Checksum()) )
            return 'fail'

        msk = None
        ds = None
        drv.Delete('tmp/mask21.tif')

    return 'success'

###############################################################################
# Test creation of external TIFF mask band just after Create()

def mask_22():

    if gdaltest.have_ng == 0:
        return 'skip'

    drv = gdal.GetDriverByName('GTiff')
    ds = drv.Create( 'tmp/mask_22.tif', 20 ,20 )
    ds.CreateMaskBand(gdal.GMF_PER_DATASET)

    cs = ds.GetRasterBand(1).GetMaskBand().Checksum()
    if cs != 0:
        print(cs)
        gdaltest.post_reason( 'Got wrong checksum for the the mask' )
        return 'fail'

    ds.GetRasterBand(1).GetMaskBand().Fill(1)

    cs = ds.GetRasterBand(1).GetMaskBand().Checksum()
    if cs != 400:
        print(cs)
        gdaltest.post_reason( 'Got wrong checksum for the the mask' )
        return 'fail'

    ds = None

    try:
        os.stat('tmp/mask_22.tif.msk')
    except:
        gdaltest.post_reason( 'tmp/mask_22.tif.msk is absent' )
        return 'fail'

    ds = gdal.Open('tmp/mask_22.tif')

    if ds.GetRasterBand(1).GetMaskFlags() != gdal.GMF_PER_DATASET:
        gdaltest.post_reason( 'wrong mask flags' )
        return 'fail'

    cs = ds.GetRasterBand(1).GetMaskBand().Checksum()
    if cs != 400:
        print(cs)
        gdaltest.post_reason( 'Got wrong checksum for the the mask' )
        return 'fail'

    ds = None

    drv.Delete( 'tmp/mask_22.tif' )

    try:
        os.stat('tmp/mask_22.tif.msk')
        gdaltest.post_reason( 'tmp/mask_22.tif.msk is still there' )
        return 'fail'
    except:
        pass

    return 'success' 

###############################################################################
# Test CreateCopy() of a dataset with a mask into a JPEG-compressed TIFF with
# internal mask (#3800)

def mask_23():

    if gdaltest.have_ng == 0:
        return 'skip'

    drv = gdal.GetDriverByName('GTiff')
    md = drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('JPEG') == -1:
        return 'skip'

    src_ds = drv.Create( 'tmp/mask_23_src.tif', 3000, 2000, 3, options = ['TILED=YES','SPARSE_OK=YES'] )
    src_ds.CreateMaskBand(gdal.GMF_PER_DATASET)
    
    gdal.SetConfigOption('GDAL_TIFF_INTERNAL_MASK', 'YES')
    old_val = gdal.GetCacheMax()
    gdal.SetCacheMax(15000000)
    gdal.ErrorReset()
    ds = drv.CreateCopy( 'tmp/mask_23_dst.tif', src_ds, options = ['TILED=YES','COMPRESS=JPEG'])
    gdal.SetConfigOption('GDAL_TIFF_INTERNAL_MASK', 'NO')
    gdal.SetCacheMax(old_val)
    
    ds = None
    error_msg = gdal.GetLastErrorMsg()
    src_ds = None

    drv.Delete( 'tmp/mask_23_src.tif' )
    drv.Delete( 'tmp/mask_23_dst.tif' )    
    
    # 'ERROR 1: TIFFRewriteDirectory:Error fetching directory count' was triggered before
    if error_msg != '':
        return 'fail'

    return 'success' 

###############################################################################
# Test on a GDT_UInt16 RGBA (#5692)

def mask_24():

    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/mask_24.tif', 100, 100, 4, \
                gdal.GDT_UInt16, options = ['PHOTOMETRIC=RGB', 'ALPHA=YES'] )
    ds.GetRasterBand(1).Fill(65565)
    ds.GetRasterBand(2).Fill(65565)
    ds.GetRasterBand(3).Fill(65565)
    ds.GetRasterBand(4).Fill(65565)

    if ds.GetRasterBand(1).GetMaskFlags() != gdal.GMF_ALPHA + gdal.GMF_PER_DATASET:
        gdaltest.post_reason( 'Did not get expected mask.' )
        return 'fail'
    mask = ds.GetRasterBand(1).GetMaskBand()

    # IRasterIO() optimized case
    import struct
    if struct.unpack('B', mask.ReadRaster(0,0,1,1))[0] != 255:
        gdaltest.post_reason('fail')
        print(struct.unpack('B', mask.ReadRaster(0,0,1,1))[0])
        return 'fail'

    # IReadBlock() code path
    (blockx, blocky) = mask.GetBlockSize()
    if struct.unpack('B' * blockx * blocky, mask.ReadBlock(0,0))[0] != 255:
        gdaltest.post_reason('fail')
        print(struct.unpack('B' * blockx * blocky, mask.ReadBlock(0,0))[0])
        return 'fail'
    mask.FlushCache()

    # Test special case where dynamics is only 0-255
    ds.GetRasterBand(4).Fill(255)
    if struct.unpack('B', mask.ReadRaster(0,0,1,1))[0] != 1:
        gdaltest.post_reason('fail')
        print(struct.unpack('B', mask.ReadRaster(0,0,1,1))[0])
        return 'fail'

    ds = None

    gdal.Unlink('/vsimem/mask_24.tif')

    return 'success'

###############################################################################
# Cleanup.


gdaltest_list = [
    mask_1,
    mask_2,
    mask_3,
    mask_4,
    mask_5,
    mask_6,
    mask_7,
    mask_8,
    mask_9,
    mask_10,
    mask_11,
    mask_12,
    mask_13,
    mask_14,
    mask_15,
    mask_16,
    mask_17,
    mask_18,
    mask_15_avg,
    mask_16_avg,
    mask_17_avg,
    mask_18_avg,
    mask_19,
    mask_20,
    mask_21,
    mask_22,
    mask_23,
    mask_24 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'mask' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

