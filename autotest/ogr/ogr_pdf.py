#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGR PDF driver.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2012, Even Rouault <even dot rouault at spatialys.com>
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
import ogrtest
from osgeo import gdal
from osgeo import ogr
from osgeo import osr
import pytest


def has_read_support():

    if ogr.GetDriverByName('PDF') is None:
        return False

    # Check read support
    gdal_pdf_drv = gdal.GetDriverByName('PDF')
    md = gdal_pdf_drv.GetMetadata()
    if 'HAVE_POPPLER' not in md and 'HAVE_PODOFO' not in md and 'HAVE_PDFIUM' not in md:
        return False

    return True

###############################################################################
# Test write support


def test_ogr_pdf_1(name='tmp/ogr_pdf_1.pdf', write_attributes='YES'):

    if ogr.GetDriverByName('PDF') is None:
        pytest.skip()

    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)

    ds = ogr.GetDriverByName('PDF').CreateDataSource(name, options=['STREAM_COMPRESS=NONE', 'MARGIN=10', 'OGR_WRITE_ATTRIBUTES=%s' % write_attributes, 'OGR_LINK_FIELD=linkfield'])

    lyr = ds.CreateLayer('first_layer', srs=sr)

    lyr.CreateField(ogr.FieldDefn('strfield', ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn('intfield', ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn('realfield', ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn('linkfield', ogr.OFTString))

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(2 49)'))
    feat.SetField('strfield', 'super tex !')
    feat.SetField('linkfield', 'http://gdal.org/')
    feat.SetStyleString('LABEL(t:{strfield},dx:5,dy:10,a:45,p:4)')
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('LINESTRING(2 48,3 50)'))
    feat.SetField('strfield', 'str')
    feat.SetField('intfield', 1)
    feat.SetField('realfield', 2.34)
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON((2 48,2 49,3 49,3 48,2 48))'))
    feat.SetField('linkfield', 'http://gdal.org/')
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON((2 48,2 49,3 49,3 48,2 48),(2.25 48.25,2.25 48.75,2.75 48.75,2.75 48.25,2.25 48.25))'))
    lyr.CreateFeature(feat)

    for i in range(10):
        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetStyleString('SYMBOL(c:#FF0000,id:"ogr-sym-%d",s:10)' % i)
        feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(%f 49.1)' % (2 + i * 0.05)))
        lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetStyleString('SYMBOL(c:#000000,id:"../gcore/data/byte.tif")')
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(2.5 49.1)'))
    lyr.CreateFeature(feat)

    ds = None

    # Do a quick test to make sure the text came out OK.
    wantedstream = 'BT\n' + \
        '362.038672 362.038672 -362.038672 362.038672 18.039040 528.960960 Tm\n' + \
        '0.000000 0.000000 0.000000 rg\n' + \
        '/F1 0.023438 Tf\n' + \
        '(super tex !) Tj\n' + \
        'ET'

    with open(name, 'rb') as f:
        data = f.read(8192)
        assert wantedstream.encode('utf-8') in data, \
            'Wrong text data in written PDF stream'

    
###############################################################################
# Test read support


def test_ogr_pdf_2(name='tmp/ogr_pdf_1.pdf', has_attributes=True):

    if not has_read_support():
        pytest.skip()

    ds = ogr.Open(name)
    assert ds is not None

    lyr = ds.GetLayerByName('first_layer')
    assert lyr is not None

    if has_attributes:
        assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('strfield')).GetType() == ogr.OFTString
        assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('intfield')).GetType() == ogr.OFTInteger
        assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('realfield')).GetType() == ogr.OFTReal
    else:
        assert lyr.GetLayerDefn().GetFieldCount() == 0

    if has_attributes:
        feat = lyr.GetNextFeature()
    # This won't work properly until text support is added to the
    # PDF vector feature reader
    # if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('POINT(2 49)')) != 0:
    #    feat.DumpReadable()
    #    gdaltest.post_reason('fail')
    #    return 'fail'

    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('LINESTRING(2 48,3 50)')) != 0:
        feat.DumpReadable()
        pytest.fail()

    if has_attributes:
        if feat.GetField('strfield') != 'str':
            feat.DumpReadable()
            pytest.fail()
        if feat.GetField('intfield') != 1:
            feat.DumpReadable()
            pytest.fail()
        if feat.GetFieldAsDouble('realfield') != pytest.approx(2.34, abs=1e-10):
            feat.DumpReadable()
            pytest.fail()

    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('POLYGON((2 48,2 49,3 49,3 48,2 48))')) != 0:
        feat.DumpReadable()
        pytest.fail()

    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('POLYGON((2 48,2 49,3 49,3 48,2 48),(2.25 48.25,2.25 48.75,2.75 48.75,2.75 48.25,2.25 48.25))')) != 0:
        feat.DumpReadable()
        pytest.fail()

    for i in range(10):
        feat = lyr.GetNextFeature()
        if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('POINT(%f 49.1)' % (2 + i * 0.05))) != 0:
            feat.DumpReadable()
            pytest.fail('fail with ogr-sym-%d' % i)

    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('POINT(2.5 49.1)')) != 0:
        feat.DumpReadable()
        pytest.fail('fail with raster icon')

    ds = None

###############################################################################
# Test write support without writing attributes


def test_ogr_pdf_3():
    return test_ogr_pdf_1('tmp/ogr_pdf_2.pdf', 'NO')

###############################################################################
# Check read support without writing attributes


def test_ogr_pdf_4():
    return test_ogr_pdf_2('tmp/ogr_pdf_2.pdf', False)


###############################################################################
# Switch from poppler to podofo if both are available

def test_ogr_pdf_4_podofo():

    gdal_pdf_drv = gdal.GetDriverByName('PDF')
    if gdal_pdf_drv is None:
        pytest.skip()

    md = gdal_pdf_drv.GetMetadata()
    if 'HAVE_POPPLER' in md and 'HAVE_PODOFO' in md:
        gdal.SetConfigOption("GDAL_PDF_LIB", "PODOFO")
        print('Using podofo now')
        ret = test_ogr_pdf_4()
        gdal.SetConfigOption("GDAL_PDF_LIB", None)
        return ret
    pytest.skip()

###############################################################################
# Test read support with OGR_PDF_READ_NON_STRUCTURED=YES


def test_ogr_pdf_5():

    if not has_read_support():
        pytest.skip()

    with gdaltest.config_option('OGR_PDF_READ_NON_STRUCTURED', 'YES'):
        ds = ogr.Open('data/pdf/drawing.pdf')
    assert ds is not None

    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 8

###############################################################################
# Test read support of polygon with holes and Bezier curves


def test_ogr_pdf_bezier_curve_and_polygon_holes():

    if not has_read_support():
        pytest.skip()

    with gdaltest.config_option('OGR_PDF_READ_NON_STRUCTURED', 'YES'):
        ds = ogr.Open('data/pdf/bezier_curve_and_polygon_holes.pdf')
    assert ds is not None

    lyr = ds.GetLayer(0)
    feat = lyr.GetFeature(2)
    expected_wkt = """POLYGON ((8444.60213541667 6332.05489588889,8444.71174575 6331.98745444444,8444.82702558333 6331.926391,8444.94655754167 6331.87099688889,8445.06892425 6331.82056344444,8445.19270833333 6331.774382,8445.31649241667 6331.73174388889,8445.438859125 6331.69194044444,8445.55839108333 6331.654263,8445.67367091667 6331.61800288889,8445.78328125 6331.58245144444,8445.89289158333 6331.58256955556,8446.00817141667 6331.58339633334,8446.127703375 6331.58564044444,8446.25007008333 6331.59001055556,8446.37385416667 6331.59721533333,8446.49763825 6331.60796344444,8446.62000495833 6331.62296355556,8446.73953691667 6331.64292433333,8446.85481675 6331.66855444444,8446.96442708333 6331.70056255555,8447.17360801042 6331.77851588889,8447.37641075 6331.87064255556,8447.57354398958 6331.97694255555,8447.76571641667 6332.09741588889,8447.95363671875 6332.23206255555,8448.13801358333 6332.38088255556,8448.31955569792 6332.54387588889,8448.49897175 6332.72104255556,8448.67697042709 6332.91238255556,8448.85426041667 6333.11789588889,8449.02056575 6333.43679588889,8449.16419308334 6333.75569588889,8449.28372504167 6334.07459588889,8449.37774425 6334.39349588889,8449.44483333333 6334.71239588889,8449.48357491667 6335.03129588889,8449.492551625 6335.35019588889,8449.47034608333 6335.66909588889,8449.41554091667 6335.98799588889,8449.32671875 6336.30689588889,8449.19572967709 6336.69654444444,8449.01584116667 6337.08548433334,8448.78776190625 6337.47300688889,8448.51220058333 6337.85840344444,8448.18986588542 6338.24096533333,8447.8214665 6338.61998388889,8447.40771111458 6338.99475044445,8446.94930841667 6339.36455633333,8446.44696709375 6339.72869288889,8445.90139583333 6340.08645144444,8445.41240145833 6340.39082377778,8444.93758083334 6340.66472344445,8444.47693395833 6340.90602444444,8444.03046083333 6341.11260077778,8443.59816145833 6341.28232644444,8443.18003583333 6341.41307544444,8442.77608395833 6341.50272177778,8442.38630583334 6341.54913944444,8442.01070145833 6341.55020244444,8441.64927083333 6341.50378477778,8441.33402301042 6341.45429622222,8441.02657075 6341.37716966667,8440.72762273958 6341.27311377778,8440.43788766667 6341.14283722222,8440.15807421875 6340.98704866667,8439.88889108333 6340.80645677778,8439.63104694792 6340.60177022222,8439.3852505 6340.37369766667,8439.15221042708 6340.12294777778,8438.93263541667 6339.85022922222,8438.82999384375 6339.67294444445,8438.73514783333 6339.494951,8438.64880607291 6339.31554022222,8438.57167725 6339.13400344444,8438.50447005208 6338.949632,8438.44789316667 6338.76171722222,8438.40265528125 6338.56955044444,8438.36946508333 6338.372423,8438.34903126042 6338.16962622222,8438.3420625 6337.96045144444,8438.34572405208 6337.77997766667,8438.35718116667 6337.59383455556,8438.37714253125 6337.40343944444,8438.40631683333 6337.21020966667,8438.44541276042 6337.01556255555,8438.495139 6336.82091544444,8438.55620423958 6336.62768566667,8438.62931716667 6336.43729055555,8438.71518646875 6336.25114744444,8438.81452083333 6336.07067366667,8438.92436739584 6335.89693222222,8439.04130083334 6335.72956877778,8439.16532114583 6335.56787466666,8439.29642833334 6335.41114122222,8439.43462239583 6335.25865977778,8439.57990333333 6335.10972166667,8439.73227114583 6334.96361822222,8439.89172583333 6334.81964077778,8440.05826739583 6334.67708066667,8440.23189583333 6334.53522922222,8440.51537083333 6334.36881066667,8440.79884583333 6334.22436077778,8441.08232083333 6334.10258822222,8441.36579583334 6334.00420166667,8441.64927083333 6333.92990977778,8441.93274583333 6333.88042122222,8442.21622083333 6333.85644466667,8442.49969583333 6333.85868877778,8442.78317083333 6333.88786222222,8443.06664583333 6333.94467366667,8443.34634116667 6333.990737,8443.61753225 6334.05806033333,8443.87880170833 6334.14664366667,8444.12873216667 6334.256487,8444.36590625 6334.38759033333,8444.58890658333 6334.53995366667,8444.79631579167 6334.713577,8444.9867165 6334.90846033333,8445.15869133333 6335.12460366667,8445.31082291667 6335.362007,8445.41027539583 6335.50385844445,8445.4969715 6335.64641855556,8445.57232860417 6335.790396,8445.63776408334 6335.93649944444,8445.6946953125 6336.08543755556,8445.74453966666 6336.237919,8445.78871452083 6336.39465244444,8445.82863725 6336.55634655556,8445.86572522917 6336.72371,8445.90139583333 6336.89745144444,8445.90104148959 6337.0745,8445.89856108334 6337.25083988889,8445.89182855208 6337.42576244444,8445.87871783334 6337.598559,8445.85710286459 6337.76852088889,8445.82485758333 6337.93493944444,8445.77985592708 6338.097106,8445.71997183333 6338.25431188889,8445.64307923958 6338.40584844444,8445.54705208333 6338.551007,8445.54705208333 6338.551007,8445.85214205208 6338.37384033333,8446.13101058334 6338.19667366667,8446.38578373958 6338.019507,8446.61858758333 6337.84234033333,8446.83154817708 6337.66517366667,8447.02679158333 6337.488007,8447.20644386458 6337.31084033333,8447.37263108333 6337.13367366667,8447.52747930209 6336.956507,8447.67311458333 6336.77934033333,8447.77575615625 6336.60229177778,8447.87060216667 6336.42595188889,8447.95694392708 6336.25102933333,8448.03407275 6336.07823277778,8448.10127994792 6335.90827088889,8448.15785683334 6335.74185233333,8448.20309471875 6335.57968577778,8448.23628491667 6335.42247988889,8448.25671873958 6335.27094333333,8448.2636875 6335.12578477778,8448.2601440625 6334.98735855556,8448.24951375 6334.85460166667,8448.2317965625 6334.72609677778,8448.2069925 6334.60042655555,8448.1751015625 6334.47617366667,8448.13612375 6334.35192077778,8448.0900590625 6334.22625055555,8448.0369075 6334.09774566667,8447.9766690625 6333.96498877778,8447.90934375 6333.82656255556,8447.79961530208 6333.69179777778,8447.68350866667 6333.57049766667,8447.56173253125 6333.46195355555,8447.43499558333 6333.36545677778,8447.30400651042 6333.28029866667,8447.169474 6333.20577055555,8447.03210673958 6333.14116377778,8446.89261341667 6333.08576966667,8446.75170271875 6333.03887955556,8446.61008333333 6332.99978477778,8446.50035488542 6333.000021,8446.38424825 6333.00167455556,8446.26247211458 6333.00616277777,8446.13573516667 6333.014903,8446.00474609375 6333.02931255556,8445.87021358333 6333.05080877778,8445.73284632292 6333.080809,8445.593353 6333.12073055556,8445.45244230208 6333.17199077778,8445.31082291667 6333.236007,8444.60213541667 6332.05489588889),(8443.65721875 6339.614007,8443.82730375 6339.50085655556,8443.983215 6339.37495011111,8444.1249525 6339.237705,8444.25251625 6339.09053855556,8444.36590625 6338.93486811111,8444.4651225 6338.772111,8444.550165 6338.60368455556,8444.62103375 6338.43100611111,8444.67772875 6338.255493,8444.72025 6338.07856255556,8444.78060655209 6337.90139588889,8444.82041116667 6337.72422922222,8444.84037253125 6337.54706255555,8444.84119933333 6337.36989588889,8444.82360026042 6337.19272922222,8444.788284 6337.01556255555,8444.73595923958 6336.83839588889,8444.66733466667 6336.66122922222,8444.58311896875 6336.48406255556,8444.48402083333 6336.30689588889,8444.3708670625 6336.168824,8444.24495691667 6336.03854744445,8444.10770777083 6335.91677488889,8443.960537 6335.804215,8443.80486197917 6335.70157644444,8443.64210008333 6335.60956788889,8443.4736686875 6335.528898,8443.30098516667 6335.46027544444,8443.12546689583 6335.40440888889,8442.94853125 6335.362007,8442.76805216667 6335.33696744445,8442.58190358334 6335.33177055556,8442.391502875 6335.344999,8442.19826741667 6335.37523544444,8442.00361458333 6335.42106255555,8441.80896175 6335.481063,8441.61572629167 6335.55381944444,8441.42532558333 6335.63791455556,8441.239177 6335.731931,8441.05869791667 6335.83445144444,8440.85329665625 6335.97961,8440.66277783333 6336.13114655556,8440.48785013541 6336.28835244444,8440.32922225 6336.450519,8440.18760286458 6336.61693755556,8440.06370066667 6336.78689944444,8439.95822434375 6336.959696,8439.87188258333 6337.13461855556,8439.80538407292 6337.31095844444,8439.7594375 6337.488007,8439.69908094792 6337.69387466667,8439.65927633334 6337.887695,8439.63931496875 6338.071594,8439.63848816667 6338.24769766667,8439.65608723958 6338.418132,8439.6914035 6338.585023,8439.74372826042 6338.75049666667,8439.81235283333 6338.916679,8439.89656853125 6339.085696,8439.99566666667 6339.25967366667,8440.06677164583 6339.36243033333,8440.139294 6339.45810033333,8440.21465110416 6339.54668366666,8440.29426033333 6339.62818033333,8440.3795390625 6339.70259033333,8440.47190466667 6339.76991366667,8440.57277452083 6339.83015033333,8440.683566 6339.88330033333,8440.80569647917 6339.92936366667,8440.94058333333 6339.96834033333,8441.05031178125 6340.03235655556,8441.16641841667 6340.08361677778,8441.28819455208 6340.12353833333,8441.4149315 6340.15353855556,8441.54592057292 6340.17503477778,8441.68045308333 6340.18944433333,8441.81782034375 6340.19818455555,8441.95731366667 6340.20267277778,8442.09822436458 6340.20432633333,8442.23984375 6340.20456255555,8442.38158125 6340.168893,8442.52331875 6340.13180611111,8442.66505625 6340.09188455556,8442.80679375 6340.047711,8442.94853125 6339.99786811111,8443.09026875 6339.94093855556,8443.23200625 6339.875505,8443.37374375 6339.80015011111,8443.51548125 6339.71345655556,8443.65721875 6339.614007))"""
    if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt(expected_wkt)) != 0:
        feat.DumpReadable()
        pytest.fail()


###############################################################################
# Test read support with a non-OGR datasource


def test_ogr_pdf_online_1():

    if not has_read_support():
        pytest.skip()

    if not gdaltest.download_file('http://www.terragotech.com/images/pdf/webmap_urbansample.pdf', 'webmap_urbansample.pdf'):
        pytest.skip()

    expected_layers = [
        ["Cadastral Boundaries", ogr.wkbPolygon],
        ["Water Lines", ogr.wkbLineString],
        ["Sewerage Lines", ogr.wkbLineString],
        ["Sewerage Jump-Ups", ogr.wkbLineString],
        ["Roads", ogr.wkbUnknown],
        ["Water Points", ogr.wkbPoint],
        ["Sewerage Pump Stations", ogr.wkbPoint],
        ["Sewerage Man Holes", ogr.wkbPoint],
        ["BPS - Buildings", ogr.wkbPolygon],
        ["BPS - Facilities", ogr.wkbPolygon],
        ["BPS - Water Sources", ogr.wkbPoint],
    ]

    ds = ogr.Open('tmp/cache/webmap_urbansample.pdf')
    assert ds is not None

    assert ds.GetLayerCount() == len(expected_layers)

    for i in range(ds.GetLayerCount()):
        assert ds.GetLayer(i).GetName() == expected_layers[i][0], \
            ('%d : %s' % (i, ds.GetLayer(i).GetName()))

        assert ds.GetLayer(i).GetGeomType() == expected_layers[i][1], \
            ('%d : %d' % (i, ds.GetLayer(i).GetGeomType()))

    lyr = ds.GetLayerByName('Water Points')
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('POINT (724431.316665166523308 7672947.212302438914776)')) != 0:
        feat.DumpReadable()
        pytest.fail()
    assert feat.GetField('ID') == 'VL46'

###############################################################################
# Test read support of non-structured content


def test_ogr_pdf_online_2():

    if not has_read_support():
        pytest.skip()

    if not gdaltest.download_file('https://download.osgeo.org/gdal/data/pdf/340711752_Azusa_FSTopo.pdf', '340711752_Azusa_FSTopo.pdf'):
        pytest.skip()

    expected_layers = [
        ['Other_5', 0],
        ['Quadrangle_Extent_Other_4', 0],
        ['Quadrangle_Extent_State_Outline', 0],
        ['Adjacent_Quadrangle_Diagram_Other_3', 0],
        ['Adjacent_Quadrangle_Diagram_Quadrangle_Extent', 0],
        ['Adjacent_Quadrangle_Diagram_Quad_Outlines', 0],
        ['Quadrangle_Other', 0],
        ['Quadrangle_Labels_Unplaced_Labels_Road_Shields_-_Vertical', 0],
        ['Quadrangle_Labels_Road_Shields_-_Horizontal', 0],
        ['Quadrangle_Labels_Road_Shields_-_Vertical', 0],
        ['Quadrangle_Neatline/Mask_Neatline', 0],
        ['Quadrangle_Neatline/Mask_Mask', 0],
        ['Quadrangle_Culture_Features', 0],
        ['Quadrangle_Large_Tanks', 0],
        ['Quadrangle_Linear_Transportation_Features', 0],
        ['Quadrangle_Railroads_', 0],
        ['Quadrangle_Linear_Culture_Features', 0],
        ['Quadrangle_Linear_Landform_Features', 0],
        ['Quadrangle_Boundaries', 0],
        ['Quadrangle_PLSS', 0],
        ['Quadrangle_Survey_Lines', 0],
        ['Quadrangle_Linear_Drainage_Features', 0],
        ['Quadrangle_Contour_Labels', 0],
        ['Quadrangle_Contours', 0],
        ['Quadrangle_2_5`_Tics_Interior_Grid_Intersections', 0],
        ['Quadrangle_2_5`_Tics_Grid_Tics_along_Neatline', 0],
        ['Quadrangle_UTM_Grid_Interior_Grid_Intersections', 0],
        ['Quadrangle_UTM_Grid_Grid_Tics_along_Neatline', 0],
        ['Quadrangle_UTM_Grid_UTM_Grid_Lines', 0],
        ['Quadrangle_Large_Buildings', 0],
        ['Quadrangle_Drainage_Polygons', 0],
        ['Quadrangle_Ownership', 0],
        ['Quadrangle_Builtup_Areas', 0],
        ['Quadrangle_WoodlandUSGS_P', 0],
    ]

    ds = ogr.Open('tmp/cache/340711752_Azusa_FSTopo.pdf')
    assert ds is not None

    if ds.GetLayerCount() != len(expected_layers):
        for lyr in ds:
            print(lyr.GetName(), lyr.GetGeomType())
        pytest.fail(ds.GetLayerCount())

    for i in range(ds.GetLayerCount()):
        assert ds.GetLayer(i).GetName() == expected_layers[i][0], \
            ('%d : %s' % (i, ds.GetLayer(i).GetName()))

        assert ds.GetLayer(i).GetGeomType() == expected_layers[i][1], \
            ('%d : %d' % (i, ds.GetLayer(i).GetGeomType()))


###############################################################################
# Test PDF with no attributes


def test_ogr_pdf_no_attributes():

    if not has_read_support():
        pytest.skip()

    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)

    filename = '/vsimem/test_ogr_pdf_no_attributes.pdf'
    ds = ogr.GetDriverByName('PDF').CreateDataSource(filename, options=['STREAM_COMPRESS=NONE'])
    lyr = ds.CreateLayer('first_layer', srs=sr)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('LINESTRING(2 49,3 50)'))
    lyr.CreateFeature(feat)
    ds = None

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 1
    ds = None

    gdal.Unlink(filename)

###############################################################################
# Cleanup


def test_ogr_pdf_cleanup():

    if ogr.GetDriverByName('PDF') is None:
        pytest.skip()

    ogr.GetDriverByName('PDF').DeleteDataSource('tmp/ogr_pdf_1.pdf')
    ogr.GetDriverByName('PDF').DeleteDataSource('tmp/ogr_pdf_2.pdf')



