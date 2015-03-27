#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  OGR Python samples
# Purpose:  Create OGR VRT from source datasource
# Author:   Frank Warmerdam, warmerdam@pobox.com
#
###############################################################################
# Copyright (c) 2009, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2009-2014, Even Rouault <even dot rouault at mines-paris dot org>
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

try:
    from osgeo import ogr, gdal
except ImportError:
    import  ogr, gdal

import sys

#############################################################################

def GeomType2Name( type ):
    if type == ogr.wkbUnknown:
        return 'wkbUnknown'
    elif type == ogr.wkbPoint:
        return 'wkbPoint'
    elif type == ogr.wkbLineString:
        return 'wkbLineString'
    elif type == ogr.wkbPolygon:
        return 'wkbPolygon'
    elif type == ogr.wkbMultiPoint:
        return 'wkbMultiPoint'
    elif type == ogr.wkbMultiLineString:
        return 'wkbMultiLineString'
    elif type == ogr.wkbMultiPolygon:
        return 'wkbMultiPolygon'
    elif type == ogr.wkbGeometryCollection:
        return 'wkbGeometryCollection'
    elif type == ogr.wkbNone:
        return 'wkbNone'
    elif type == ogr.wkbLinearRing:
        return 'wkbLinearRing'
    else:
        return 'wkbUnknown'

#############################################################################
def Esc(x):
    return gdal.EscapeString( x, gdal.CPLES_XML )

#############################################################################
def Usage():
    print('Usage: ogr2vrt.py [-relative] [-schema] [-feature_count] [-extent]')
    print('                  in_datasource out_vrtfile [layers]')
    print('')
    sys.exit(1)

#############################################################################
# Argument processing.

infile = None
outfile = None
layer_list = []
relative = "0"
schema=0
feature_count=0
extent=0
openoptions = []

argv = gdal.GeneralCmdLineProcessor( sys.argv )
if argv is None:
    sys.exit( 0 )
        
i = 1
while i < len(argv):
    arg = argv[i]

    if arg == '-relative':
        relative = "1"

    elif arg == '-schema':
        schema = 1

    elif arg == '-feature_count':
        feature_count = 1

    elif arg == '-extent':
        extent = 1

    elif arg == '-oo':
        i += 1
        openoptions.append(argv[i])

    elif arg[0] == '-':
        Usage()

    elif infile is None:
        infile = arg

    elif outfile is None:
        outfile = arg

    else:
        layer_list.append( arg )

    i = i + 1

if outfile is None:
    Usage()

if schema and feature_count:
    sys.stderr.write('Ignoring -feature_count when used with -schema.\n')
    feature_count = 0

if schema and extent:
    sys.stderr.write('Ignoring -extent when used with -schema.\n')
    extent = 0
    
#############################################################################
# Open the datasource to read.

src_ds = gdal.OpenEx( infile, gdal.OF_VECTOR, open_options = openoptions )

if schema:
    infile = '@dummy@'

if len(layer_list) == 0:
    for lyr_idx in range(src_ds.GetLayerCount()):
        layer_list.append( src_ds.GetLayer(lyr_idx).GetLayerDefn().GetName() )

#############################################################################
# Start the VRT file.

vrt = '<OGRVRTDataSource>\n'

#############################################################################
#	Process each source layer.

for name in layer_list:
    layer = src_ds.GetLayerByName(name)
    layerdef = layer.GetLayerDefn()

    vrt += '  <OGRVRTLayer name="%s">\n' % Esc(name)
    vrt += '    <SrcDataSource relativeToVRT="%s" shared="%d">%s</SrcDataSource>\n' \
           % (relative,not schema,Esc(infile))

    if len(openoptions) > 0:
        vrt += '    <OpenOptions>\n' 
        for option in openoptions:
            (key, value) = option.split('=')
            vrt += '        <OOI key="%s">%s</OOI>\n'  % (Esc(key), Esc(value))
        vrt += '    </OpenOptions>\n' 

    if schema:
        vrt += '    <SrcLayer>@dummy@</SrcLayer>\n' 
    else:
        vrt += '    <SrcLayer>%s</SrcLayer>\n' % Esc(name)

    # Historic format for mono-geometry layers
    if layerdef.GetGeomFieldCount() == 0:
        vrt += '    <GeometryType>wkbNone</GeometryType>\n'
    elif layerdef.GetGeomFieldCount() == 1 and \
         layerdef.GetGeomFieldDefn(0).IsNullable():
        vrt += '    <GeometryType>%s</GeometryType>\n' \
            % GeomType2Name(layerdef.GetGeomType())
        srs = layer.GetSpatialRef()
        if srs is not None:
            vrt += '    <LayerSRS>%s</LayerSRS>\n' \
                % (Esc(srs.ExportToWkt()))
        if extent:
            (xmin, xmax, ymin, ymax) = layer.GetExtent()
            vrt += '    <ExtentXMin>%.15g</ExtentXMin>\n' % xmin
            vrt += '    <ExtentYMin>%.15g</ExtentYMin>\n' % ymin
            vrt += '    <ExtentXMax>%.15g</ExtentXMax>\n' % xmax
            vrt += '    <ExtentYMax>%.15g</ExtentYMax>\n' % ymax

    # New format for multi-geometry field support
    else:
        for fld_index in range(layerdef.GetGeomFieldCount()):
            src_fd = layerdef.GetGeomFieldDefn( fld_index )
            vrt += '    <GeometryField name="%s"' % src_fd.GetName()
            if src_fd.IsNullable() == 0:
                vrt += ' nullable="false"'
            vrt += '>\n'
            vrt += '      <GeometryType>%s</GeometryType>\n' \
                    % GeomType2Name(src_fd.GetType())
            srs = src_fd.GetSpatialRef()
            if srs is not None:
                vrt += '      <SRS>%s</SRS>\n' \
                        % (Esc(srs.ExportToWkt()))
            if extent:
                (xmin, xmax, ymin, ymax) = layer.GetExtent(geom_field = fld_index)
                vrt += '      <ExtentXMin>%.15g</ExtentXMin>\n' % xmin
                vrt += '      <ExtentYMin>%.15g</ExtentYMin>\n' % ymin
                vrt += '      <ExtentXMax>%.15g</ExtentXMax>\n' % xmax
                vrt += '      <ExtentYMax>%.15g</ExtentYMax>\n' % ymax
            vrt += '    </GeometryField>\n'

    # Process all the fields.
    for fld_index in range(layerdef.GetFieldCount()):
        src_fd = layerdef.GetFieldDefn( fld_index )
        if src_fd.GetType() == ogr.OFTInteger:
            type = 'Integer'
        elif src_fd.GetType() == ogr.OFTInteger64:
            type = 'Integer64'
        elif src_fd.GetType() == ogr.OFTString:
            type = 'String'
        elif src_fd.GetType() == ogr.OFTReal:
            type = 'Real'
        elif src_fd.GetType() == ogr.OFTStringList:
            type = 'StringList'
        elif src_fd.GetType() == ogr.OFTIntegerList:
            type = 'IntegerList'
        elif src_fd.GetType() == ogr.OFTInteger64List:
            type = 'Integer64List'
        elif src_fd.GetType() == ogr.OFTRealList:
            type = 'RealList'
        elif src_fd.GetType() == ogr.OFTBinary:
            type = 'Binary'
        elif src_fd.GetType() == ogr.OFTDate:
            type = 'Date'
        elif src_fd.GetType() == ogr.OFTTime:
            type = 'Time'
        elif src_fd.GetType() == ogr.OFTDateTime:
            type = 'DateTime'
        else:
            type = 'String'

        vrt += '    <Field name="%s" type="%s"' \
               % (Esc(src_fd.GetName()), type)
        if src_fd.GetSubType() != ogr.OFSTNone:
            vrt += ' subtype="%s"' % ogr.GetFieldSubTypeName(src_fd.GetSubType())
        if not schema:
            vrt += ' src="%s"' % Esc(src_fd.GetName())
        if src_fd.GetWidth() > 0:
            vrt += ' width="%d"' % src_fd.GetWidth()
        if src_fd.GetPrecision() > 0:
            vrt += ' precision="%d"' % src_fd.GetPrecision()
        if src_fd.IsNullable() == 0:
            vrt += ' nullable="false"'
        vrt += '/>\n'

    if feature_count:
        vrt += '    <FeatureCount>%d</FeatureCount>\n' % layer.GetFeatureCount()

    vrt += '  </OGRVRTLayer>\n'

vrt += '</OGRVRTDataSource>\n' 

#############################################################################
# Write vrt

open(outfile,'w').write(vrt)
