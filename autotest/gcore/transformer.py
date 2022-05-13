#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test the GenImgProjTransformer capabilities.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2008, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
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



import gdaltest
from osgeo import gdal
from osgeo import osr
import pytest
import math

###############################################################################
# Test simple Geotransform based transformer.


def test_transformer_1():

    ds = gdal.Open('data/byte.tif')
    tr = gdal.Transformer(ds, None, [])

    (success, pnt) = tr.TransformPoint(0, 20, 10)

    assert success and pnt[0] == pytest.approx(441920, abs=0.00000001) and pnt[1] == pytest.approx(3750720, abs=0.00000001) and pnt[2] == 0.0, \
        'got wrong forward transform result.'

    (success, pnt) = tr.TransformPoint(1, pnt[0], pnt[1], pnt[2])

    assert success and pnt[0] == pytest.approx(20, abs=0.00000001) and pnt[1] == pytest.approx(10, abs=0.00000001) and pnt[2] == 0.0, \
        'got wrong reverse transform result.'

###############################################################################
# Test GCP based transformer with polynomials.


def test_transformer_2():

    ds = gdal.Open('data/gcps.vrt')
    tr = gdal.Transformer(ds, None, ['METHOD=GCP_POLYNOMIAL'])

    (success, pnt) = tr.TransformPoint(0, 20, 10)

    assert success and pnt[0] == pytest.approx(441920, abs=0.001) and pnt[1] == pytest.approx(3750720, abs=0.001) and pnt[2] == 0, \
        'got wrong forward transform result.'

    (success, pnt) = tr.TransformPoint(1, pnt[0], pnt[1], pnt[2])

    assert success and pnt[0] == pytest.approx(20, abs=0.001) and pnt[1] == pytest.approx(10, abs=0.001) and pnt[2] == 0, \
        'got wrong reverse transform result.'

###############################################################################
# Test GCP based transformer with thin plate splines.

def test_transformer_3():

    ds = gdal.Open('data/gcps.vrt')
    tr = gdal.Transformer(ds, None, ['METHOD=GCP_TPS'])

    (success, pnt) = tr.TransformPoint(0, 20, 10)

    assert success and pnt[0] == pytest.approx(441920, abs=0.001) and pnt[1] == pytest.approx(3750720, abs=0.001) and pnt[2] == 0, \
        'got wrong forward transform result.'

    (success, pnt) = tr.TransformPoint(1, pnt[0], pnt[1], pnt[2])

    assert success and pnt[0] == pytest.approx(20, abs=0.001) and pnt[1] == pytest.approx(10, abs=0.001) and pnt[2] == 0, \
        'got wrong reverse transform result.'

###############################################################################
# Test geolocation based transformer.


def test_transformer_4():

    ds = gdal.Open('data/sstgeo.vrt')
    tr = gdal.Transformer(ds, None, ['METHOD=GEOLOC_ARRAY'])

    (success, pnt) = tr.TransformPoint(0, 20, 10)

    assert success and pnt[0] == pytest.approx(-81.961341857910156, abs=0.000001) and pnt[1] == pytest.approx(29.612689971923828, abs=0.000001) and pnt[2] == 0, \
        'got wrong forward transform result.'

    (success, pnt) = tr.TransformPoint(1, pnt[0], pnt[1], pnt[2])

    assert success and pnt[0] == pytest.approx(20, abs=0.000001) and pnt[1] == pytest.approx(10, abs=0.000001) and pnt[2] == 0, \
        'got wrong reverse transform result.'

###############################################################################
# Test RPC based transformer.


def test_transformer_5():

    ds = gdal.Open('data/rpc.vrt')
    tr = gdal.Transformer(ds, None, ['METHOD=RPC', 'RPC_PIXEL_ERROR_THRESHOLD=0.05'])

    (success, pnt) = tr.TransformPoint(0, 20.5, 10.5)
    assert success and pnt[0] == pytest.approx(125.64830100509131, abs=0.000001) and pnt[1] == pytest.approx(39.869433991997553, abs=0.000001) and pnt[2] == 0, \
        'got wrong forward transform result.'

    (success, pnt) = tr.TransformPoint(1, pnt[0], pnt[1], pnt[2])
    assert success and pnt[0] == pytest.approx(20.5, abs=0.05) and pnt[1] == pytest.approx(10.5, abs=0.05) and pnt[2] == 0, \
        'got wrong reverse transform result.'

    # Try with a different height.

    (success, pnt) = tr.TransformPoint(0, 20.5, 10.5, 30)

    assert success and pnt[0] == pytest.approx(125.64828521533849, abs=0.000001) and pnt[1] == pytest.approx(39.869345204440144, abs=0.000001) and pnt[2] == 30, \
        'got wrong forward transform result.(2)'

    (success, pnt) = tr.TransformPoint(1, pnt[0], pnt[1], pnt[2])

    assert success and pnt[0] == pytest.approx(20.5, abs=0.05) and pnt[1] == pytest.approx(10.5, abs=0.05) and pnt[2] == 30, \
        'got wrong reverse transform result.(2)'

    # Test RPC_HEIGHT option
    tr = gdal.Transformer(ds, None, ['METHOD=RPC', 'RPC_HEIGHT=30'])

    (success, pnt) = tr.TransformPoint(0, 20.5, 10.5)

    assert success and pnt[0] == pytest.approx(125.64828521533849, abs=0.000001) and pnt[1] == pytest.approx(39.869345204440144, abs=0.000001), \
        'got wrong forward transform result.(3)'

    (success, pnt) = tr.TransformPoint(1, pnt[0], pnt[1], pnt[2])

    assert success and pnt[0] == pytest.approx(20.5, abs=0.1) and pnt[1] == pytest.approx(10.5, abs=0.1), \
        'got wrong reverse transform result.(3)'

    # Test RPC_DEM and RPC_HEIGHT_SCALE options

    # (long,lat)=(125.64828521533849 39.869345204440144) -> (Easting,Northing)=(213324.662167036 4418634.47813677) in EPSG:32652
    ds_dem = gdal.GetDriverByName('GTiff').Create('/vsimem/dem.tif', 100, 100, 1)
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(32652)
    ds_dem.SetProjection(sr.ExportToWkt())
    ds_dem.SetGeoTransform([213300, 200, 0, 4418700, 0, -200])
    ds_dem.GetRasterBand(1).Fill(15)
    ds_dem = None

    tr = gdal.Transformer(ds, None, ['METHOD=RPC', 'RPC_HEIGHT_SCALE=2', 'RPC_DEM=/vsimem/dem.tif'])

    (success, pnt) = tr.TransformPoint(0, 20.5, 10.5, 0)

    assert success and pnt[0] == pytest.approx(125.64828521533849, abs=0.000001) and pnt[1] == pytest.approx(39.869345204440144, abs=0.000001), \
        'got wrong forward transform result.(4)'

    (success, pnt) = tr.TransformPoint(1, pnt[0], pnt[1], pnt[2])

    assert success and pnt[0] == pytest.approx(20.5, abs=0.05) and pnt[1] == pytest.approx(10.5, abs=0.05), \
        'got wrong reverse transform result.(4)'

    tr = None

    # Test RPC_DEMINTERPOLATION=cubic

    tr = gdal.Transformer(ds, None, ['METHOD=RPC', 'RPC_HEIGHT_SCALE=2', 'RPC_DEM=/vsimem/dem.tif', 'RPC_DEMINTERPOLATION=cubic'])

    (success, pnt) = tr.TransformPoint(0, 20.5, 10.5, 0)

    assert success and pnt[0] == pytest.approx(125.64828521533849, abs=0.000001) and pnt[1] == pytest.approx(39.869345204440144, abs=0.000001), \
        'got wrong forward transform result.(5)'

    (success, pnt) = tr.TransformPoint(1, pnt[0], pnt[1], pnt[2])

    assert success and pnt[0] == pytest.approx(20.5, abs=0.05) and pnt[1] == pytest.approx(10.5, abs=0.05), \
        'got wrong reverse transform result.(5)'

    tr = None

    # Test RPC_DEMINTERPOLATION=near

    tr = gdal.Transformer(ds, None, ['METHOD=RPC', 'RPC_HEIGHT_SCALE=2', 'RPC_DEM=/vsimem/dem.tif', 'RPC_DEMINTERPOLATION=near'])

    (success, pnt) = tr.TransformPoint(0, 20.5, 10.5, 0)

    assert success and pnt[0] == pytest.approx(125.64828521503811, abs=0.000001) and pnt[1] == pytest.approx(39.869345204874911, abs=0.000001), \
        'got wrong forward transform result.(6)'

    (success, pnt) = tr.TransformPoint(1, pnt[0], pnt[1], pnt[2])

    assert success and pnt[0] == pytest.approx(20.5, abs=0.05) and pnt[1] == pytest.approx(10.5, abs=0.05), \
        'got wrong reverse transform result.(6)'

    tr = None

    # Test outside DEM extent : default behaviour --> error
    tr = gdal.Transformer(ds, None, ['METHOD=RPC', 'RPC_HEIGHT_SCALE=2', 'RPC_DEM=/vsimem/dem.tif'])

    (success, pnt) = tr.TransformPoint(0, 40000, 0, 0)
    assert success == 0

    (success, pnt) = tr.TransformPoint(1, 125, 40, 0)
    assert success == 0

    tr = None

    # Test outside DEM extent with RPC_DEM_MISSING_VALUE=0
    ds_dem = gdal.GetDriverByName('GTiff').Create('/vsimem/dem.tif', 100, 100, 1)
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(32652)
    ds_dem.SetProjection(sr.ExportToWkt())
    ds_dem.SetGeoTransform([213300, 1, 0, 4418700, 0, -1])
    ds_dem.GetRasterBand(1).Fill(15)
    ds_dem = None
    tr = gdal.Transformer(ds, None, ['METHOD=RPC', 'RPC_HEIGHT_SCALE=2', 'RPC_DEM=/vsimem/dem.tif', 'RPC_DEM_MISSING_VALUE=0'])

    (success, pnt) = tr.TransformPoint(0, -99.5, 0.5, 0)
    assert success and pnt[0] == pytest.approx(125.64746155942839, abs=0.000001) and pnt[1] == pytest.approx(39.869506789921168, abs=0.000001), \
        'got wrong forward transform result.'

    (success, pnt) = tr.TransformPoint(1, pnt[0], pnt[1], pnt[2])
    assert success and pnt[0] == pytest.approx(-99.5, abs=0.05) and pnt[1] == pytest.approx(0.5, abs=0.05), \
        'got wrong reverse transform result.'

    tr = None

    gdal.Unlink('/vsimem/dem.tif')

###############################################################################
# Test RPC convergence bug (bug # 5395)

def test_transformer_6():

    ds = gdal.Open('data/rpc_5395.vrt')
    tr = gdal.Transformer(ds, None, ['METHOD=RPC'])

    (success, pnt) = tr.TransformPoint(0, 0.5, 0.5)

    assert success and pnt[0] == pytest.approx(28.26163232, abs=0.0001) and pnt[1] == pytest.approx(-27.79853245, abs=0.0001) and pnt[2] == 0, \
        'got wrong forward transform result.'

###############################################################################
# Test Transformer.TransformPoints


def test_transformer_7():

    ds = gdal.Open('data/byte.tif')
    tr = gdal.Transformer(ds, None, [])

    (pnt, success) = tr.TransformPoints(0, [(20, 10)])

    assert success[0] != 0 and pnt[0][0] == pytest.approx(441920, abs=0.00000001) and pnt[0][1] == pytest.approx(3750720, abs=0.00000001) and pnt[0][2] == 0.0, \
        'got wrong forward transform result.'

###############################################################################
# Test handling of nodata in RPC DEM (#5680)


def test_transformer_8():

    ds = gdal.Open('data/rpc.vrt')

    # (long,lat)=(125.64828521533849 39.869345204440144) -> (Easting,Northing)=(213324.662167036 4418634.47813677) in EPSG:32652
    ds_dem = gdal.GetDriverByName('GTiff').Create('/vsimem/dem.tif', 100, 100, 1, gdal.GDT_Int16)
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(32652)
    ds_dem.SetProjection(sr.ExportToWkt())
    ds_dem.SetGeoTransform([213300, 1, 0, 4418700, 0, -1])
    ds_dem.GetRasterBand(1).SetNoDataValue(-32768)
    ds_dem.GetRasterBand(1).Fill(-32768)
    ds_dem = None

    for method in ['near', 'bilinear', 'cubic']:
        tr = gdal.Transformer(ds, None, ['METHOD=RPC', 'RPC_DEM=/vsimem/dem.tif', 'RPC_DEMINTERPOLATION=%s' % method])

        (success, pnt) = tr.TransformPoint(0, 20, 10, 0)

        if success:
            print(success, pnt)
            pytest.fail('got wrong forward transform result.')

        (success, pnt) = tr.TransformPoint(1, 125.64828521533849, 39.869345204440144, 0)

        if success:
            print(success, pnt)
            pytest.fail('got wrong reverse transform result.')

    gdal.Unlink('/vsimem/dem.tif')

###############################################################################
# Test RPC DEM line optimization


def test_transformer_9():

    ds = gdal.Open('data/rpc.vrt')

    # (long,lat)=(125.64828521533849 39.869345204440144) -> (Easting,Northing)=(213324.662167036 4418634.47813677) in EPSG:32652
    ds_dem = gdal.GetDriverByName('GTiff').Create('/vsimem/dem.tif', 100, 100, 1, gdal.GDT_Byte)
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    ds_dem.SetProjection(sr.ExportToWkt())
    ds_dem.SetGeoTransform([125.647968621436, 1.2111052640051412e-05, 0, 39.869926216038, 0, -8.6569068979969188e-06])
    import random
    random.seed(0)
    data = ''.join([chr(40 + int(10 * random.random())) for _ in range(100 * 100)])
    ds_dem.GetRasterBand(1).WriteRaster(0, 0, 100, 100, data)
    ds_dem = None

    for method in ['near', 'bilinear', 'cubic']:
        tr = gdal.Transformer(ds, None, ['METHOD=RPC', 'RPC_DEM=/vsimem/dem.tif', 'RPC_DEMINTERPOLATION=%s' % method])

        points = [(125.64828521533849, 39.869345204440144)] * 10
        (pnt, success) = tr.TransformPoints(1, points)
        assert success[0], method
        pnt_optimized = pnt[0]

        (success, pnt) = tr.TransformPoint(1, 125.64828521533849, 39.869345204440144, 0)
        assert success, method

        assert pnt == pnt_optimized, method

    gdal.Unlink('/vsimem/dem.tif')

###############################################################################
# Test RPC DEM transform from geoid height to ellipsoidal height


def test_transformer_10():

    # Create fake vertical shift grid
    out_ds = gdal.GetDriverByName('GTX').Create('tmp/fake.gtx', 10, 10, 1, gdal.GDT_Float32)
    out_ds.SetGeoTransform([-180, 36, 0, 90, 0, -18])
    sr = osr.SpatialReference()
    sr.SetWellKnownGeogCS('WGS84')
    out_ds.SetProjection(sr.ExportToWkt())
    out_ds.GetRasterBand(1).Fill(100)
    out_ds = None

    # Create a fake DEM
    ds_dem = gdal.GetDriverByName('GTiff').Create('/vsimem/dem.tif', 100, 100, 1, gdal.GDT_Byte)
    ds_dem.SetGeoTransform([125.647968621436, 1.2111052640051412e-05, 0, 39.869926216038, 0, -8.6569068979969188e-06])
    import random
    random.seed(0)
    data = ''.join([chr(40 + int(10 * random.random())) for _ in range(100 * 100)])
    ds_dem.GetRasterBand(1).WriteRaster(0, 0, 100, 100, data)
    ds_dem = None

    ds_dem = gdal.Open('/vsimem/dem.tif')
    vrt_dem = gdal.GetDriverByName('VRT').CreateCopy('/vsimem/dem.vrt', ds_dem)
    ds_dem = None

    vrt_dem.SetProjection("""COMPD_CS["WGS 84 + my_height",
    GEOGCS["WGS 84",
        DATUM["WGS_1984",
            SPHEROID["WGS 84",6378137,298.257223563,
                AUTHORITY["EPSG","7030"]],
            AUTHORITY["EPSG","6326"]],
        PRIMEM["Greenwich",0,
            AUTHORITY["EPSG","8901"]],
        UNIT["degree",0.0174532925199433,
            AUTHORITY["EPSG","9122"]],
        AUTHORITY["EPSG","4326"]],
    VERT_CS["my_height",
        VERT_DATUM["my_height",0,
            EXTENSION["PROJ4_GRIDS","./tmp/fake.gtx"]],
        UNIT["metre",1,
            AUTHORITY["EPSG","9001"]],
        AXIS["Up",UP]]]""")
    vrt_dem = None

    ds = gdal.Open('data/rpc.vrt')

    tr = gdal.Transformer(ds, None, ['METHOD=RPC', 'RPC_DEM=/vsimem/dem.vrt'])
    (success, pnt) = tr.TransformPoint(1, 125.64828521533849, 39.869345204440144, 0)
    assert success and pnt[0] == pytest.approx(27.31476045569616, abs=1e-5) and pnt[1] == pytest.approx(-53.328814757762302, abs=1e-5) and pnt[2] == 0, \
        'got wrong result: %s' % str(pnt)

    tr = gdal.Transformer(ds, None, ['METHOD=RPC', 'RPC_DEM=/vsimem/dem.vrt', 'RPC_DEM_APPLY_VDATUM_SHIFT=FALSE'])
    (success, pnt) = tr.TransformPoint(1, 125.64828521533849, 39.869345204440144, 0)

    assert success and pnt[0] == pytest.approx(21.445626206892484, abs=1e-5) and pnt[1] == pytest.approx(1.6460100520871492, abs=1e-5) and pnt[2] == 0, \
        'got wrong result.'

    gdal.GetDriverByName('GTX').Delete('tmp/fake.gtx')
    gdal.Unlink('/vsimem/dem.tif')
    gdal.Unlink('/vsimem/dem.vrt')

###############################################################################
# Test failed inverse RPC transform (#6162)


def test_transformer_11():

    ds = gdal.GetDriverByName('MEM').Create('', 6600, 4400)
    rpc = [
        'HEIGHT_OFF=1113.66579196784',
        'HEIGHT_SCALE=12.5010114250099',
        'LAT_OFF=38.8489906468112',
        'LAT_SCALE=-0.106514418771489',
        'LINE_DEN_COEFF=1 -0.000147949809772754 -0.000395269640841174 -1.15825619524758e-05 -0.001613476071797 5.20818134468044e-05 -2.87546958936308e-05 0.00139252754800089 0.00103224907048726 -5.0328770407996e-06 8.03722313022155e-06 0.000236052289425919 0.000208478107633822 -8.11629138727222e-06 0.000168941442517399 0.000392113144410504 3.13299811375497e-06 -1.50306451132806e-07 -1.96870155855449e-06 6.84425679628047e-07',
        'LINE_NUM_COEFF=0.00175958077249233 1.38380980570961 -1.10937056344449 -2.64222540811728e-05 0.00242330787142254 0.000193743606261641 -0.000149740797138056 0.000348558508286103 -8.44646294793856e-05 3.10853483444725e-05 6.94899990982205e-05 -0.00348125387930033 -0.00481553689971959 -7.80038440894703e-06 0.00410332555882184 0.00269594666059233 5.94355882183947e-06 -6.12499223746471e-05 -2.16490482825638e-05 -1.95059491792213e-06',
        'LINE_OFF=2199.80872158044',
        'LINE_SCALE=2202.03966104116',
        'LONG_OFF=77.3374268058015',
        'LONG_SCALE=0.139483831686384',
        'SAMP_DEN_COEFF=1 0.000220381598198686 -5.9113079248377e-05 -0.000123013508187712 -2.69270454504924e-05 3.85090208529735e-05 -5.05359221990966e-05 0.000207017095461956 0.000441092857548974 1.47302072491805e-05 9.4840973108768e-06 -0.000810344094204395 -0.000690502911945615 -1.07959445293954e-05 0.000801157109076503 0.000462754838815978 9.13256389877791e-06 7.49571761868177e-06 -5.00612460432453e-06 -2.25925949180435e-06',
        'SAMP_NUM_COEFF=-0.00209214639511201 -0.759096012299728 -0.903450038473527 5.43928095403867e-05 -0.000717672934172181 -0.000168790405106395 -0.00015564609496447 0.0013261576802665 -0.000398331147368139 -3.84712681506314e-05 2.70041394522796e-05 0.00254362585790201 0.00332988183285888 3.36326833370395e-05 0.00445687297094153 0.00290078876854111 3.59552237739047e-05 7.16492495304347e-05 -5.6782194494005e-05 2.32051448455541e-06',
        'SAMP_OFF=3300.39818049514',
        'SAMP_SCALE=3302.50400920438'
    ]
    ds.SetMetadata(rpc, 'RPC')

    tr = gdal.Transformer(ds, None, ['METHOD=RPC', 'RPC_HEIGHT=4000'])
    (success, pnt) = tr.TransformPoint(0, 0, 0, 0)
    assert not success, pnt

    # But this one should succeed
    tr = gdal.Transformer(ds, None, ['METHOD=RPC', 'RPC_HEIGHT=1150'])
    (success, pnt) = tr.TransformPoint(0, 0, 0, 0)
    assert success and pnt[0] == pytest.approx(77.350939956024618, abs=1e-7) and pnt[1] == pytest.approx(38.739703990877814, abs=1e-7)

###############################################################################
# Test degenerate cases of TPS transformer


def test_transformer_12():

    ds = gdal.Open("""
    <VRTDataset rasterXSize="20" rasterYSize="20">
  <GCPList Projection="PROJCS[&quot;NAD27 / UTM zone 11N&quot;,GEOGCS[&quot;NAD27&quot;,DATUM[&quot;North_American_Datum_1927&quot;,SPHEROID[&quot;Clarke 1866&quot;,6378206.4,294.9786982139006,AUTHORITY[&quot;EPSG&quot;,&quot;7008&quot;]],AUTHORITY[&quot;EPSG&quot;,&quot;6267&quot;]],PRIMEM[&quot;Greenwich&quot;,0],UNIT[&quot;degree&quot;,0.0174532925199433],AUTHORITY[&quot;EPSG&quot;,&quot;4267&quot;]],PROJECTION[&quot;Transverse_Mercator&quot;],PARAMETER[&quot;latitude_of_origin&quot;,0],PARAMETER[&quot;central_meridian&quot;,-117],PARAMETER[&quot;scale_factor&quot;,0.9996],PARAMETER[&quot;false_easting&quot;,500000],PARAMETER[&quot;false_northing&quot;,0],UNIT[&quot;metre&quot;,1,AUTHORITY[&quot;EPSG&quot;,&quot;9001&quot;]],AUTHORITY[&quot;EPSG&quot;,&quot;26711&quot;]]">
    <GCP Id="" Pixel="0" Line="0" X="0" Y="0"/>
    <GCP Id="" Pixel="20" Line="0" X="20" Y="0"/>
    <GCP Id="" Pixel="0" Line="20" X="0" Y="20"/>
    <GCP Id="" Pixel="20" Line="20" X="20" Y="20"/>
    <GCP Id="" Pixel="0" Line="0" X="0" Y="0"/> <!-- duplicate entry -->
  </GCPList>
  <VRTRasterBand dataType="Byte" band="1">
    <ColorInterp>Gray</ColorInterp>
    <SimpleSource>
      <SourceFilename relativeToVRT="1">data/byte.tif</SourceFilename>
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>""")

    tr = gdal.Transformer(ds, None, ['METHOD=GCP_TPS'])
    assert tr is not None
    (success, pnt) = tr.TransformPoint(0, 0, 0)
    assert success and pnt[0] == pytest.approx(0, abs=1e-7) and pnt[1] == pytest.approx(0, abs=1e-7)

    ds = gdal.Open("""
    <VRTDataset rasterXSize="20" rasterYSize="20">
  <GCPList Projection="PROJCS[&quot;NAD27 / UTM zone 11N&quot;,GEOGCS[&quot;NAD27&quot;,DATUM[&quot;North_American_Datum_1927&quot;,SPHEROID[&quot;Clarke 1866&quot;,6378206.4,294.9786982139006,AUTHORITY[&quot;EPSG&quot;,&quot;7008&quot;]],AUTHORITY[&quot;EPSG&quot;,&quot;6267&quot;]],PRIMEM[&quot;Greenwich&quot;,0],UNIT[&quot;degree&quot;,0.0174532925199433],AUTHORITY[&quot;EPSG&quot;,&quot;4267&quot;]],PROJECTION[&quot;Transverse_Mercator&quot;],PARAMETER[&quot;latitude_of_origin&quot;,0],PARAMETER[&quot;central_meridian&quot;,-117],PARAMETER[&quot;scale_factor&quot;,0.9996],PARAMETER[&quot;false_easting&quot;,500000],PARAMETER[&quot;false_northing&quot;,0],UNIT[&quot;metre&quot;,1,AUTHORITY[&quot;EPSG&quot;,&quot;9001&quot;]],AUTHORITY[&quot;EPSG&quot;,&quot;26711&quot;]]">
    <GCP Id="" Pixel="0" Line="0" X="0" Y="0"/>
    <GCP Id="" Pixel="20" Line="0" X="20" Y="0"/>
    <GCP Id="" Pixel="0" Line="20" X="0" Y="20"/>
    <GCP Id="" Pixel="20" Line="20" X="20" Y="20"/>
    <GCP Id="" Pixel="0" Line="0" X="10" Y="10"/> <!-- same pixel,line -->
  </GCPList>
  <VRTRasterBand dataType="Byte" band="1">
    <ColorInterp>Gray</ColorInterp>
    <SimpleSource>
      <SourceFilename relativeToVRT="1">data/byte.tif</SourceFilename>
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>""")

    gdal.ErrorReset()
    with gdaltest.error_handler():
        tr = gdal.Transformer(ds, None, ['METHOD=GCP_TPS'])
    assert gdal.GetLastErrorMsg() != ''

    ds = gdal.Open("""
    <VRTDataset rasterXSize="20" rasterYSize="20">
  <GCPList Projection="PROJCS[&quot;NAD27 / UTM zone 11N&quot;,GEOGCS[&quot;NAD27&quot;,DATUM[&quot;North_American_Datum_1927&quot;,SPHEROID[&quot;Clarke 1866&quot;,6378206.4,294.9786982139006,AUTHORITY[&quot;EPSG&quot;,&quot;7008&quot;]],AUTHORITY[&quot;EPSG&quot;,&quot;6267&quot;]],PRIMEM[&quot;Greenwich&quot;,0],UNIT[&quot;degree&quot;,0.0174532925199433],AUTHORITY[&quot;EPSG&quot;,&quot;4267&quot;]],PROJECTION[&quot;Transverse_Mercator&quot;],PARAMETER[&quot;latitude_of_origin&quot;,0],PARAMETER[&quot;central_meridian&quot;,-117],PARAMETER[&quot;scale_factor&quot;,0.9996],PARAMETER[&quot;false_easting&quot;,500000],PARAMETER[&quot;false_northing&quot;,0],UNIT[&quot;metre&quot;,1,AUTHORITY[&quot;EPSG&quot;,&quot;9001&quot;]],AUTHORITY[&quot;EPSG&quot;,&quot;26711&quot;]]">
    <GCP Id="" Pixel="0" Line="0" X="0" Y="0"/>
    <GCP Id="" Pixel="20" Line="0" X="20" Y="0"/>
    <GCP Id="" Pixel="0" Line="20" X="0" Y="20"/>
    <GCP Id="" Pixel="20" Line="20" X="20" Y="20"/>
    <GCP Id="" Pixel="10" Line="10" X="20" Y="20"/> <!-- same X,Y -->
  </GCPList>
  <VRTRasterBand dataType="Byte" band="1">
    <ColorInterp>Gray</ColorInterp>
    <SimpleSource>
      <SourceFilename relativeToVRT="1">data/byte.tif</SourceFilename>
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>""")

    gdal.ErrorReset()
    with gdaltest.error_handler():
        tr = gdal.Transformer(ds, None, ['METHOD=GCP_TPS'])
    assert gdal.GetLastErrorMsg() != ''

###############################################################################
# Test inverse RPC transform at DEM edge (#6377)


def test_transformer_13():

    ds = gdal.GetDriverByName('MEM').Create('', 6600, 4400)
    rpc = [
        "HEIGHT_OFF=79.895358112544",
        "HEIGHT_SCALE=71.8479951519956",
        "LAT_OFF=39.1839631741725",
        "LAT_SCALE=-0.0993355184710674",
        "LINE_DEN_COEFF=1 8.18889582174233e-05 -0.000585027621468826 0.00141894885228522 -0.000585589558894143 2.26848970721562e-05 0.0004556101949561 -0.000807782279739336 -0.00042471862816941 -0.000569244978738162 1.48442578097541e-05 4.05131290592846e-05 2.84884306250279e-05 -5.18205692205965e-06 -6.313878273056e-07 1.53979251356426e-05 -7.18376115203249e-06 -6.17331013601745e-05 -7.21314704472095e-05 4.12297300238455e-06",
        "LINE_NUM_COEFF=-0.00742236358913794 -1.34432796989641 1.14742235955483 -0.00419813954264328 -0.00234215180175534 -0.00624463816085957 0.00678228413157904 0.0020362389986917 -0.00187712244349171 -8.03499198655765e-08 -0.00058862905508099 -0.00738644673656152 -0.00769111767189179 0.00076485216017804 0.00714033152180546 0.00597946564795612 -0.000632882594479344 -0.000167672086277102 0.00055226160003967 1.01784884515205e-06",
        "LINE_OFF=2199.71134437189",
        "LINE_SCALE=2197.27163235171",
        "LONG_OFF=-108.129788954851",
        "LONG_SCALE=0.135395601856691",
        "SAMP_DEN_COEFF=1 -0.000817668457487893 -0.00151956231901818 0.00117149108953055 0.000514723430775277 0.000357856819755055 0.000655430235824068 -0.00100177312999255 -0.000488725013873637 -0.000500795084518271 -3.31511569640467e-06 4.60608554396048e-05 4.71371559254521e-05 -3.47487113243818e-06 1.0984752288197e-05 1.6421626141648e-05 -6.2866141729034e-06 -6.32966599886646e-05 -7.06552514786235e-05 3.89288575686084e-06",
        "SAMP_NUM_COEFF=0.00547379112608157 0.807100297014362 0.845388298829057 0.01082483811889 -0.00320368761068744 0.00357867636379949 0.00459377712275926 -0.00324853865239341 -0.00218177030092682 2.99823054607907e-05 0.000946829367823539 0.00428577519330827 0.0045745876325088 -0.000396201144848935 0.00488772258958395 0.00435309486883759 -0.000402737234433541 0.000402935809278189 0.000642374929382851 -5.26793321752838e-06",
        "SAMP_OFF=3299.3111821927",
        "SAMP_SCALE=3297.19448149873"
    ]
    ds.SetMetadata(rpc, 'RPC')

    tr = gdal.Transformer(ds, None, ['METHOD=RPC', 'RPC_DEM=data/transformer_13_dem.tif'])
    (success, pnt) = tr.TransformPoint(0, 6600, 24)
    assert success and pnt[0] == pytest.approx(-108.00066000065341, abs=1e-7) and pnt[1] == pytest.approx(39.157694013439489, abs=1e-7)

###############################################################################
# Test inverse RPC transform when iterations do oscillations (#6377)


def test_transformer_14():
    ds = gdal.GetDriverByName('MEM').Create('', 4032, 2688)
    rpc = ["MIN_LAT=0",
           "MAX_LAT=0",
           "MIN_LONG=0",
           "MAX_LONG=0",
           "HEIGHT_OFF=244.72924043124081",
           "HEIGHT_SCALE=391.44066987292678",
           "LAT_OFF=0.095493639758799986",
           "LAT_SCALE=-0.0977494003085103",
           "LINE_DEN_COEFF=1 1.73399671259238e-05 -6.18169396309642e-06 -3.11498839490863e-05 -1.18048814815295e-05 -5.46123898974842e-05 -2.51203895820587e-05 -5.77299008756702e-05 -1.37836923606953e-05 -3.24029327866125e-06 2.06307542696228e-07 -5.16777154466466e-08 2.98762926005741e-07 3.17761145061869e-08 1.48077371641094e-07 -7.69738626480047e-08 2.94990048269861e-08 -3.37468052222007e-08 -3.67859879729462e-08 8.79847359414426e-10 ",
           "LINE_NUM_COEFF=0.000721904493927027 1.02330510505135 -1.27742813759689 -0.0973049949136407 -0.014260789316429 0.00229308399354221 -0.0016640916975237 0.0124508639909873 0.00336835383694126 1.1987123734283e-05 -1.85240614830659e-05 4.40716454954686e-05 2.3198555492418e-05 -8.31659287301587e-08 -5.10329082923063e-05 2.56477008932482e-05 1.01465909326012e-05 1.04407036240869e-05 4.27413648628578e-05 2.91696764503125e-07 ",
           "LINE_OFF=1343.99369782095",
           "LINE_SCALE=1343.96638400536",
           "LONG_OFF=-0.034423410000698595",
           "LONG_SCALE=0.143444599019706",
           "SAMP_DEN_COEFF=1 1.83636704399141e-05 3.55794197969218e-06 -1.33255440425932e-05 -4.25424777986987e-06 -3.95287146748821e-05 1.35786181318561e-05 -3.86131208639696e-05 -1.10085128708761e-05 -1.26863939055319e-05 -2.88045902675552e-07 -1.58732907217101e-07 4.08999884183478e-07 6.6854211618061e-08 -1.46399266323942e-07 -4.69718293745237e-08 -4.14626818788491e-08 -3.00588241056424e-07 4.54784506604435e-08 3.24214474149225e-08 ",
           "SAMP_NUM_COEFF=-0.0112062780844554 -1.05096833835297 -0.704023055461029 0.0384547265206585 -0.00987134340336078 -0.00310989611092616 -0.00116937850565916 -0.0102714370609919 0.000930565787504389 7.03834691339565e-05 -3.83216250787844e-05 -3.67841179314918e-05 2.45498653278515e-05 1.06302833544472e-05 -6.26921822677631e-05 1.29769009118128e-05 1.1336284460811e-05 -3.01250967502161e-05 -7.60511798099513e-06 -4.45260900205512e-07 ",
           "SAMP_OFF=2015.99417232167",
           "SAMP_SCALE=2015.9777295656"
          ]
    ds.SetMetadata(rpc, 'RPC')

    old_rpc_inverse_verbose = gdal.GetConfigOption('RPC_INVERSE_VERBOSE')
    gdal.SetConfigOption('RPC_INVERSE_VERBOSE', 'YES')
    old_rpc_inverse_log = gdal.GetConfigOption('RPC_INVERSE_LOG')
    gdal.SetConfigOption('RPC_INVERSE_LOG', '/vsimem/transformer_14.csv')
    tr = gdal.Transformer(ds, None, ['METHOD=RPC', 'RPC_DEM=data/transformer_14_dem.tif'])
    gdal.SetConfigOption('RPC_INVERSE_VERBOSE', old_rpc_inverse_verbose)
    gdal.SetConfigOption('RPC_INVERSE_LOG', old_rpc_inverse_log)
    (success, pnt) = tr.TransformPoint(0, 0, 0)
    assert success and pnt[0] == pytest.approx(1.9391846640653961e-05, abs=1e-7) and pnt[1] == pytest.approx(-0.0038824752244123275, abs=1e-7)

    f = gdal.VSIFOpenL('/vsimem/transformer_14.csvt', 'rb')
    if f is not None:
        content = gdal.VSIFReadL(1, 1000, f).decode('ASCII')
        gdal.VSIFCloseL(f)
    assert content.startswith('Integer,Real,Real,Real,String,Real,Real')

    f = gdal.VSIFOpenL('/vsimem/transformer_14.csv', 'rb')
    if f is not None:
        content = gdal.VSIFReadL(1, 1000, f).decode('ASCII')
        gdal.VSIFCloseL(f)
    assert content.startswith("""iter,long,lat,height,WKT,error_pixel_x,error_pixel_y
0,""")

    gdal.Unlink('/vsimem/transformer_14.csvt')
    gdal.Unlink('/vsimem/transformer_14.csv')

###############################################################################
# Test inverse RPC transform with DEM in [-180,180] but guessed longitude going
# beyond


def test_transformer_15():

    ds = gdal.GetDriverByName('MEM').Create('', 6600, 4400)
    rpc = [
        "HEIGHT_OFF=50",
        "HEIGHT_SCALE=10",
        "LAT_OFF=0",
        "LAT_SCALE=-0.105215174097221",
        "LINE_DEN_COEFF=1 0.000113241782375585 -7.43522609681362e-05 3.71535308900828e-08 0.000338102551252493 4.57912749279076e-07 -7.00823537484445e-08 9.01275640119332e-05 -0.000209723741981335 4.74972505083506e-09 7.2855438070338e-10 -3.16688523275456e-05 -4.49211037453489e-05 8.29706981496464e-12 7.58128162744879e-06 6.82536481272507e-07 2.58661007069147e-11 -3.9697791887986e-08 -5.06502821928986e-08 4.66978771069271e-11 ",
        "LINE_NUM_COEFF=0.00673418095252569 -1.38985626744028 -0.645141238074041 1.4661564574111e-05 0.00022202101663831 -4.32910472926433e-06 -1.90048143949724e-06 -0.00374486341484218 -0.00041396053769863 1.14148334846788e-09 -1.20458144309064e-07 -0.00618122456017927 -0.00783023711720404 1.32704635552568e-09 -0.0032532225011433 -0.00355239507036451 6.20315160857432e-10 -4.75170167074672e-07 1.37652348819162e-07 9.53393990126859e-12 ",
        "LINE_OFF=2191.17012189569",
        "LINE_SCALE=2197.23749680132",
        "LONG_OFF=179.9",
        "LONG_SCALE=0.124705315627701",
        "SAMP_DEN_COEFF=1 2.1759243891474e-05 -5.96367842636917e-06 6.03714873685079e-09 -0.000109764869260479 4.67786228725161e-07 -4.22013004308237e-07 0.000110379633112327 -0.00011679427405351 9.30515192365439e-09 1.09243100743555e-09 -3.29956656106618e-05 -4.1918167733603e-05 2.78076356769406e-11 -4.57011012884097e-06 -1.01453844685279e-05 2.60244039950836e-11 -3.52960090220746e-08 -4.49975439524006e-08 1.80641651820794e-11 ",
        "SAMP_NUM_COEFF=-0.000776008179693209 -0.380692820465227 1.0647540743599 1.72660888229082e-06 0.0028311896140173 -1.0466632896253e-06 3.17728359468736e-06 -0.000150466834838251 0.000566231304705376 -7.34786296240808e-11 5.90340934437437e-07 -0.00169917056998316 -0.00231193494979752 -2.71280972480264e-09 0.00482224415396017 0.00596164573794891 7.58464598771136e-09 -1.46075167736497e-07 -4.64652272450397e-07 -5.59268858101854e-13 ",
        "SAMP_OFF=3300.0420831968",
        "SAMP_SCALE=3295.90302088781",
    ]
    ds.SetMetadata(rpc, 'RPC')

    sr = osr.SpatialReference()
    sr.SetWellKnownGeogCS('WGS84')
    demE179 = gdal.GetDriverByName('GTiff').Create('/vsimem/demE179.tif', 10, 10)
    demE179.SetProjection(sr.ExportToWkt())
    demE179.SetGeoTransform([179, 0.1, 0, 0.5, 0, -0.1])
    demE179.GetRasterBand(1).Fill(50)
    demW180 = gdal.GetDriverByName('GTiff').Create('/vsimem/demW180.tif', 10, 10)
    demW180.SetProjection(sr.ExportToWkt())
    demW180.SetGeoTransform([-180, 0.1, 0, 0.5, 0, -0.1])
    demW180.GetRasterBand(1).Fill(50)
    gdal.BuildVRT('/vsimem/transformer_15_dem.vrt', [demE179, demW180])
    demE179 = None
    demW180 = None

    tr = gdal.Transformer(ds, None, ['METHOD=RPC', 'RPC_DEM=/vsimem/transformer_15_dem.vrt'])
    (success, pnt) = tr.TransformPoint(0, 0, 0)
    assert success and pnt[0] == pytest.approx(180.02280735469199, abs=1e-7) and pnt[1] == pytest.approx(0.061069145746997976, abs=1e-7)

    (success, pnt_forward) = tr.TransformPoint(1, pnt[0], pnt[1], 0)
    assert success and pnt_forward[0] == pytest.approx(0, abs=0.1) and pnt_forward[1] == pytest.approx(0, abs=0.1), \
        'got wrong reverse transform result.'

    (success, pnt_forward) = tr.TransformPoint(1, pnt[0] - 360, pnt[1], 0)
    assert success and pnt_forward[0] == pytest.approx(0, abs=0.1) and pnt_forward[1] == pytest.approx(0, abs=0.1), \
        'got wrong reverse transform result.'

    # Now test around -180
    ds = gdal.GetDriverByName('MEM').Create('', 6600, 4400)
    rpc.remove('LONG_OFF=179.9')
    rpc += ['LONG_OFF=-179.9']
    ds.SetMetadata(rpc, 'RPC')

    tr = gdal.Transformer(ds, None, ['METHOD=RPC', 'RPC_DEM=/vsimem/transformer_15_dem.vrt'])
    (success, pnt) = tr.TransformPoint(0, 6600, 4400)
    assert success and pnt[0] == pytest.approx(-180.02313813793387, abs=1e-7) and pnt[1] == pytest.approx(-0.061398913932229765, abs=1e-7)

    (success, pnt_forward) = tr.TransformPoint(1, pnt[0], pnt[1], 0)
    assert success and pnt_forward[0] == pytest.approx(6600, abs=0.1) and pnt_forward[1] == pytest.approx(4400, abs=0.1), \
        'got wrong reverse transform result.'

    (success, pnt_forward) = tr.TransformPoint(1, pnt[0] + 360, pnt[1], 0)
    assert success and pnt_forward[0] == pytest.approx(6600, abs=0.1) and pnt_forward[1] == pytest.approx(4400, abs=0.1), \
        'got wrong reverse transform result.'

    gdal.Unlink('/vsimem/demE179.tif')
    gdal.Unlink('/vsimem/demW180.tif')
    gdal.Unlink('/vsimem/transformer_15_dem.tif')
    gdal.Unlink('/vsimem/transformer_15_dem.vrt')

###############################################################################
# Test approximate sub-transformers in GenImgProjTransformer
# (we mostly test that the parameters are well recognized and serialized)


def test_transformer_16():

    gdal.Translate('/vsimem/transformer_16.tif', 'data/byte.tif', options="-gcp 0 0 440720.000 3751320.000 -gcp 0 20 440720.000 3750120.000 -gcp 20 0 441920.000 3751320.000 -gcp 20 20 441920.000 3750120.000 -a_srs EPSG:26711")
    gdal.Warp('/vsimem/transformer_16.vrt', '/vsimem/transformer_16.tif', options='-of VRT -t_srs EPSG:4326 -et 0 -to SRC_APPROX_ERROR_IN_SRS_UNIT=6.05 -to SRC_APPROX_ERROR_IN_PIXEL=0.1 -to REPROJECTION_APPROX_ERROR_IN_SRC_SRS_UNIT=6.1 -to REPROJECTION_APPROX_ERROR_IN_DST_SRS_UNIT=0.0001')
    f = gdal.VSIFOpenL('/vsimem/transformer_16.vrt', 'rb')
    if f is not None:
        content = gdal.VSIFReadL(1, 10000, f).decode('ASCII')
        gdal.VSIFCloseL(f)
    assert ('<MaxErrorForward>6.05</MaxErrorForward>' in content and \
       '<MaxErrorReverse>0.1</MaxErrorReverse>' in content and \
       '<MaxErrorForward>0.0001</MaxErrorForward>' in content and \
       '<MaxErrorReverse>6.1</MaxErrorReverse>' in content)
    ds = gdal.Translate('', '/vsimem/transformer_16.vrt', format='MEM')
    assert ds.GetRasterBand(1).Checksum() == 4727
    ds = None
    gdal.Unlink('/vsimem/transformer_16.tif')
    gdal.Unlink('/vsimem/transformer_16.vrt')


###############################################################################
# Test RPC DEM with unexisting RPC DEM file

def test_transformer_17():

    ds = gdal.Open('data/rpc.vrt')
    with gdaltest.error_handler():
        tr = gdal.Transformer(ds, None, ['METHOD=RPC', 'RPC_DEM=/vsimem/i/donot/exist/dem.tif'])
    assert tr is None


def test_transformer_longlat_wrap_outside_180():

    ds = gdal.GetDriverByName('MEM').Create('', 360, 1, 1)
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    ds.SetProjection(sr.ExportToWkt())
    ds.SetGeoTransform([-180, 1, 0, 0, 0, -1])
    tr = gdal.Transformer(ds, ds, [])

    (success, pnt) = tr.TransformPoint(0, -0.5, 0.5, 0)
    assert success
    assert pnt[0] == pytest.approx(359.5, abs=0.000001), pnt
    assert pnt[1] == pytest.approx(0.5, abs=0.000001), pnt



###############################################################################
# Test reprojection transformer without reverse path
# NOTE: in case the inverse airy method is implemented some day, this test
# might fail

def test_transformer_no_reverse_method():
    tr = gdal.Transformer(None, None, ['SRC_SRS=+proj=longlat +ellps=GRS80', 'DST_SRS=+proj=airy +ellps=GRS80'])
    assert tr

    (success, pnt) = tr.TransformPoint(0, 2, 49)
    assert success
    assert pnt[0] == pytest.approx(141270.54731856665, abs=1e-3), pnt
    assert pnt[1] == pytest.approx(4656605.104980032, abs=1e-3), pnt

    with gdaltest.error_handler():
        (success, pnt) = tr.TransformPoint(1, 2, 49)
    assert not success

###############################################################################
# Test precision of GCP based transformer with thin plate splines and lots of GCPs (2115).

def test_transformer_tps_precision():

    ds = gdal.Open('data/gcps_2115.vrt')
    tr = gdal.Transformer(ds, None, ['METHOD=GCP_TPS'])
    assert tr, 'tps transformation could not be computed'

    success = True
    maxDiffResult = 0.0
    for gcp in ds.GetGCPs():
        (s, result) = tr.TransformPoint(0, gcp.GCPPixel, gcp.GCPLine)
        success &= s
        diffResult = math.sqrt((gcp.GCPX - result[0])**2 + (gcp.GCPY -result[1])**2)
        maxDiffResult = max(maxDiffResult, diffResult)
    assert success, 'at least one point could not be transformed'
    assert maxDiffResult < 1e-3, 'at least one transformation exceeds the error bound'


###############################################################################
def test_transformer_image_no_srs():

    ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    ds.SetGeoTransform([0, 10, 0, 0, 0, -10])
    tr = gdal.Transformer(ds, None, ['COORDINATE_OPERATION=+proj=unitconvert +xy_in=1 +xy_out=2'])
    assert tr
    (success, pnt) = tr.TransformPoint(0, 10, 20, 0)
    assert success
    assert pnt[0] == pytest.approx(50), pnt
    assert pnt[1] == pytest.approx(-100), pnt

###############################################################################
# Test RPC_DEM_SRS by adding vertical component egm 96 geoid

def test_transformer_dem_overrride_srs():
    ds = gdal.Open('data/rpc.vrt')
    ds_dem = gdal.GetDriverByName('GTiff').Create('/vsimem/dem.tif', 100, 100, 1)
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(32652)
    ds_dem.SetProjection(sr.ExportToWkt())
    ds_dem.SetGeoTransform([213300, 200, 0, 4418700, 0, -200])
    ds_dem.GetRasterBand(1).Fill(15)
    ds_dem = None
    tr = gdal.Transformer(ds, None, ['METHOD=RPC', 'RPC_HEIGHT_SCALE=2', 'RPC_DEM=/vsimem/dem.tif', 'RPC_DEM_SRS=EPSG:32652+5773'])

    (success, pnt) = tr.TransformPoint(0, 0.5, 0.5, 0)
    assert success and pnt[0] == pytest.approx(125.64813723085801, abs=0.000001) and pnt[1] == pytest.approx(39.869345977927146, abs=0.000001), \
        'got wrong forward transform result.'

    (success, pnt) = tr.TransformPoint(1, pnt[0], pnt[1], pnt[2])
    assert success and pnt[0] == pytest.approx(0.5, abs=0.05) and pnt[1] == pytest.approx(0.5, abs=0.05), \
        'got wrong reverse transform result.'

    gdal.Unlink('/vsimem/dem.tif')
