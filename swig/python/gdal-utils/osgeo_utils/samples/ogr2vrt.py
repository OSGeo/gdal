#!/usr/bin/env python3
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
# Copyright (c) 2009-2014, Even Rouault <even dot rouault at spatialys.com>
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

import os.path
import sys

from osgeo import ogr, gdal

#############################################################################


def GeomType2Name(typ):
    flat_type = ogr.GT_Flatten(typ)
    dic = {ogr.wkbUnknown: ('wkbUnknown', '25D'),
           ogr.wkbPoint: ('wkbPoint', '25D'),
           ogr.wkbLineString: ('wkbLineString', '25D'),
           ogr.wkbPolygon: ('wkbPolygon', '25D'),
           ogr.wkbMultiPoint: ('wkbMultiPoint', '25D'),
           ogr.wkbMultiLineString: ('wkbMultiLineString', '25D'),
           ogr.wkbMultiPolygon: ('wkbMultiPolygon', '25D'),
           ogr.wkbGeometryCollection: ('wkbGeometryCollection', '25D'),
           ogr.wkbNone: ('wkbNone', ''),
           ogr.wkbLinearRing: ('wkbLinearRing', ''),
           ogr.wkbCircularString: ('wkbCircularString', 'Z'),
           ogr.wkbCompoundCurve: ('wkbCompoundCurve', 'Z'),
           ogr.wkbCurvePolygon: ('wkbCurvePolygon', 'Z'),
           ogr.wkbMultiCurve: ('wkbMultiCurve', 'Z'),
           ogr.wkbMultiSurface: ('wkbMultiSurface', 'Z'),
           ogr.wkbCurve: ('wkbCurve', 'Z'),
           ogr.wkbSurface: ('wkbSurface', 'Z'),
           ogr.wkbPolyhedralSurface: ('wkbPolyhedralSurface', 'Z'),
           ogr.wkbTIN: ('wkbTIN', 'Z'),
           ogr.wkbTriangle: ('wkbTriangle', 'Z')}
    ret = dic[flat_type][0]
    if flat_type != typ:
        if ogr.GT_HasM(typ):
            if ogr.GT_HasZ(typ):
                ret += "ZM"
            else:
                ret += "M"
        else:
            ret += dic[flat_type][1]
    return ret

#############################################################################


def Esc(x):
    return gdal.EscapeString(x, gdal.CPLES_XML)

#############################################################################


def Usage():
    print('Usage: ogr2vrt.py [-relative] [-schema] [-feature_count] [-extent]')
    print('                  in_datasource out_vrtfile [layers]')
    print('')
    return 1


def main(argv=sys.argv):
    infile = None
    outfile = None
    layer_list = []
    relative = "0"
    schema = 0
    feature_count = 0
    extent = 0
    openoptions = []

    argv = gdal.GeneralCmdLineProcessor(argv)
    if argv is None:
        return 0

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
            return Usage()

        elif infile is None:
            infile = arg

        elif outfile is None:
            outfile = arg

        else:
            layer_list.append(arg)

        i = i + 1

    if outfile is None:
        return Usage()

    if schema and feature_count:
        sys.stderr.write('Ignoring -feature_count when used with -schema.\n')
        feature_count = 0

    if schema and extent:
        sys.stderr.write('Ignoring -extent when used with -schema.\n')
        extent = 0

    #############################################################################
    # Open the datasource to read.

    src_ds = gdal.OpenEx(infile, gdal.OF_VECTOR, open_options=openoptions)

    if schema:
        infile = '@dummy@'

    if not layer_list:
        for lyr_idx in range(src_ds.GetLayerCount()):
            layer_list.append(src_ds.GetLayer(lyr_idx).GetLayerDefn().GetName())

    #############################################################################
    # Start the VRT file.

    vrt = '<OGRVRTDataSource>\n'


    #############################################################################
    # Metadata

    mdd_list = src_ds.GetMetadataDomainList()
    if mdd_list is not None:
        for domain in mdd_list:
            if domain == '':
                vrt += '  <Metadata>\n'
            elif len(domain) > 4 and domain[0:4] == 'xml:':
                vrt += '  <Metadata domain="%s" format="xml">\n' % Esc(domain)
            else:
                vrt += '  <Metadata domain="%s">\n' % Esc(domain)
            if len(domain) > 4 and domain[0:4] == 'xml:':
                vrt += src_ds.GetMetadata_List(domain)[0]
            else:
                md = src_ds.GetMetadata(domain)
                for key in md:
                    vrt += '    <MDI key="%s">%s</MDI>\n' % (Esc(key), Esc(md[key]))
            vrt += '  </Metadata>\n'


    #############################################################################
    # Process each source layer.

    for name in layer_list:
        layer = src_ds.GetLayerByName(name)
        layerdef = layer.GetLayerDefn()

        vrt += '  <OGRVRTLayer name="%s">\n' % Esc(name)

        mdd_list = layer.GetMetadataDomainList()
        if mdd_list is not None:
            for domain in mdd_list:
                if domain == '':
                    vrt += '    <Metadata>\n'
                elif len(domain) > 4 and domain[0:4] == 'xml:':
                    vrt += '    <Metadata domain="%s" format="xml">\n' % Esc(domain)
                else:
                    vrt += '    <Metadata domain="%s">\n' % Esc(domain)
                if len(domain) > 4 and domain[0:4] == 'xml:':
                    vrt += layer.GetMetadata_List(domain)[0]
                else:
                    md = layer.GetMetadata(domain)
                    for key in md:
                        vrt += '      <MDI key="%s">%s</MDI>\n' % (Esc(key), Esc(md[key]))
                vrt += '    </Metadata>\n'

        if not os.path.isabs(outfile) and not os.path.isabs(infile) and \
           os.path.dirname(outfile) == '' and os.path.dirname(infile) == '':
            relative = 1

        vrt += '    <SrcDataSource relativeToVRT="%s" shared="%d">%s</SrcDataSource>\n' \
               % (relative, not schema, Esc(infile))

        if openoptions:
            vrt += '    <OpenOptions>\n'
            for option in openoptions:
                (key, value) = option.split('=')
                vrt += '        <OOI key="%s">%s</OOI>\n' % (Esc(key), Esc(value))
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
                src_fd = layerdef.GetGeomFieldDefn(fld_index)
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
                    (xmin, xmax, ymin, ymax) = layer.GetExtent(geom_field=fld_index)
                    vrt += '      <ExtentXMin>%.15g</ExtentXMin>\n' % xmin
                    vrt += '      <ExtentYMin>%.15g</ExtentYMin>\n' % ymin
                    vrt += '      <ExtentXMax>%.15g</ExtentXMax>\n' % xmax
                    vrt += '      <ExtentYMax>%.15g</ExtentYMax>\n' % ymax
                vrt += '    </GeometryField>\n'

        # Process all the fields.
        for fld_index in range(layerdef.GetFieldCount()):
            src_fd = layerdef.GetFieldDefn(fld_index)
            if src_fd.GetType() == ogr.OFTInteger:
                typ = 'Integer'
            elif src_fd.GetType() == ogr.OFTInteger64:
                typ = 'Integer64'
            elif src_fd.GetType() == ogr.OFTString:
                typ = 'String'
            elif src_fd.GetType() == ogr.OFTReal:
                typ = 'Real'
            elif src_fd.GetType() == ogr.OFTStringList:
                typ = 'StringList'
            elif src_fd.GetType() == ogr.OFTIntegerList:
                typ = 'IntegerList'
            elif src_fd.GetType() == ogr.OFTInteger64List:
                typ = 'Integer64List'
            elif src_fd.GetType() == ogr.OFTRealList:
                typ = 'RealList'
            elif src_fd.GetType() == ogr.OFTBinary:
                typ = 'Binary'
            elif src_fd.GetType() == ogr.OFTDate:
                typ = 'Date'
            elif src_fd.GetType() == ogr.OFTTime:
                typ = 'Time'
            elif src_fd.GetType() == ogr.OFTDateTime:
                typ = 'DateTime'
            else:
                typ = 'String'

            vrt += '    <Field name="%s" type="%s"' \
                   % (Esc(src_fd.GetName()), typ)
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
            try:
                if src_fd.IsUnique():
                    vrt += ' unique="true"'
            except AttributeError: # if run with GDAL < 3.2
                pass
            vrt += '/>\n'

        if feature_count:
            vrt += '    <FeatureCount>%d</FeatureCount>\n' % layer.GetFeatureCount()

        vrt += '  </OGRVRTLayer>\n'

    vrt += '</OGRVRTDataSource>\n'

    #############################################################################
    # Write vrt

    open(outfile, 'w').write(vrt)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
