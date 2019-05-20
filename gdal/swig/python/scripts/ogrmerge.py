#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR samples
# Purpose:  Merge the content of several vector datasets into a single one.
# Author:   Even Rouault <even dot rouault at spatialys dot com>
#
###############################################################################
# Copyright (c) 2017, Even Rouault <even dot rouault at spatialys dot com>
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

import glob
import os
import os.path
import sys

from osgeo import gdal
from osgeo import ogr

###############################################################
# Usage()


def Usage():
    print('ogrmerge.py -o out_dsname src_dsname [src_dsname]*')
    print('            [-f format] [-single] [-nln layer_name_template]')
    print('            [-update | -overwrite_ds] [-append | -overwrite_layer]')
    print('            [-src_geom_type geom_type_name[,geom_type_name]*]')
    print('            [-dsco NAME=VALUE]* [-lco NAME=VALUE]*')
    print('            [-s_srs srs_def] [-t_srs srs_def | -a_srs srs_def]')
    print('            [-progress] [-skipfailures] [--help-general]')
    print('')
    print('Options specific to -single:')
    print('            [-field_strategy FirstLayer|Union|Intersection]')
    print('            [-src_layer_field_name name]')
    print('            [-src_layer_field_content layer_name_template]')
    print('')
    print('* layer_name_template can contain the following substituable '
          'variables:')
    print('     {AUTO_NAME}  : {DS_BASENAME}_{LAYER_NAME} if they are '
          'different')
    print('                    or {LAYER_NAME} if they are identical')
    print('     {DS_NAME}    : name of the source dataset')
    print('     {DS_BASENAME}: base name of the source dataset')
    print('     {DS_INDEX}   : index of the source dataset')
    print('     {LAYER_NAME} : name of the source layer')
    print('     {LAYER_INDEX}: index of the source layer')

    return 1


def DoesDriverHandleExtension(drv, ext):
    exts = drv.GetMetadataItem(gdal.DMD_EXTENSIONS)
    return exts is not None and exts.lower().find(ext.lower()) >= 0


def GetExtension(filename):
    ext = os.path.splitext(filename)[1]
    if ext.startswith('.'):
        ext = ext[1:]
    return ext


def GetOutputDriversFor(filename):
    drv_list = []
    ext = GetExtension(filename)
    if ext.lower() == 'vrt':
        return ['VRT']
    for i in range(gdal.GetDriverCount()):
        drv = gdal.GetDriver(i)
        if (drv.GetMetadataItem(gdal.DCAP_CREATE) is not None or
            drv.GetMetadataItem(gdal.DCAP_CREATECOPY) is not None) and \
           drv.GetMetadataItem(gdal.DCAP_VECTOR) is not None:
            if ext and DoesDriverHandleExtension(drv, ext):
                drv_list.append(drv.ShortName)
            else:
                prefix = drv.GetMetadataItem(gdal.DMD_CONNECTION_PREFIX)
                if prefix is not None and filename.lower().startswith(prefix.lower()):
                    drv_list.append(drv.ShortName)

    return drv_list


def GetOutputDriverFor(filename):
    drv_list = GetOutputDriversFor(filename)
    ext = GetExtension(filename)
    if not drv_list:
        if not ext:
            return 'ESRI Shapefile'
        else:
            raise Exception("Cannot guess driver for %s" % filename)
    elif len(drv_list) > 1:
        print("Several drivers matching %s extension. Using %s" % (ext if ext else '', drv_list[0]))
    return drv_list[0]

#############################################################################


def _VSIFPrintfL(f, s):
    gdal.VSIFWriteL(s, 1, len(s), f)

#############################################################################


def EQUAL(x, y):
    return x.lower() == y.lower()

#############################################################################


def _GetGeomType(src_geom_type_name):
    if EQUAL(src_geom_type_name, "GEOMETRY"):
        return ogr.wkbUnknown
    try:
        max_geom_type = ogr.wkbTriangle
    except:
        # GDAL 2.1 compat
        max_geom_type = ogr.wkbSurface
    for i in range(max_geom_type + 1):
        if EQUAL(src_geom_type_name,
                 ogr.GeometryTypeToName(i).replace(' ', '')):
            return i
    return None

#############################################################################


def _Esc(x):
    return gdal.EscapeString(x, gdal.CPLES_XML)


class XMLWriter(object):

    def __init__(self, f):
        self.f = f
        self.inc = 0
        self.elements = []

    def _indent(self):
        return '  ' * self.inc

    def open_element(self, name, attrs=None):
        xml_attrs = ''
        if attrs is not None:
            for key in attrs:
                xml_attrs = xml_attrs + ' %s=\"%s\"' % (key, _Esc(attrs[key].encode('utf-8')))
        x = '%s<%s%s>\n' % (self._indent(), name, xml_attrs)
        x = x.encode('utf-8')
        _VSIFPrintfL(self.f, x)
        self.inc = self.inc + 1
        self.elements.append(name)

    def write_element_value(self, name, value, attrs=None):
        xml_attrs = ''
        if attrs is not None:
            for key in attrs:
                xml_attrs = xml_attrs + ' %s=\"%s\"' % (key, _Esc(attrs[key].encode('utf-8')))
        x = '%s<%s%s>%s</%s>\n' % (self._indent(), name, xml_attrs,
                      _Esc(value.encode('utf-8')), name)
        x = x.encode('utf-8')
        _VSIFPrintfL(self.f, x)

    def close_element(self, closing_name=None):
        self.inc = self.inc - 1
        name = self.elements[-1]
        if closing_name is not None:
            assert name == closing_name
        self.elements = self.elements[0:-1]
        _VSIFPrintfL(self.f, '%s</%s>\n' % (self._indent(), name))


###############################################################
# process()


def process(argv, progress=None, progress_arg=None):

    if not argv:
        return Usage()

    dst_filename = None
    output_format = None
    src_datasets = []
    overwrite_ds = False
    overwrite_layer = False
    update = False
    append = False
    single_layer = False
    layer_name_template = None
    skip_failures = False
    src_geom_types = []
    field_strategy = None
    src_layer_field_name = None
    src_layer_field_content = None
    a_srs = None
    s_srs = None
    t_srs = None
    dsco = []
    lco = []

    i = 0
    while i < len(argv):
        arg = argv[i]
        if (arg == '-f' or arg == '-of') and i + 1 < len(argv):
            i = i + 1
            output_format = argv[i]
        elif arg == '-o' and i + 1 < len(argv):
            i = i + 1
            dst_filename = argv[i]
        elif arg == '-progress':
            progress = ogr.TermProgress_nocb
            progress_arg = None
        elif arg == '-q' or arg == '-quiet':
            pass
        elif arg[0:5] == '-skip':
            skip_failures = True
        elif arg == '-update':
            update = True
        elif arg == '-overwrite_ds':
            overwrite_ds = True
        elif arg == '-overwrite_layer':
            overwrite_layer = True
            update = True
        elif arg == '-append':
            append = True
            update = True
        elif arg == '-single':
            single_layer = True
        elif arg == '-a_srs' and i + 1 < len(argv):
            i = i + 1
            a_srs = argv[i]
        elif arg == '-s_srs' and i + 1 < len(argv):
            i = i + 1
            s_srs = argv[i]
        elif arg == '-t_srs' and i + 1 < len(argv):
            i = i + 1
            t_srs = argv[i]
        elif arg == '-nln' and i + 1 < len(argv):
            i = i + 1
            layer_name_template = argv[i]
        elif arg == '-field_strategy' and i + 1 < len(argv):
            i = i + 1
            field_strategy = argv[i]
        elif arg == '-src_layer_field_name' and i + 1 < len(argv):
            i = i + 1
            src_layer_field_name = argv[i]
        elif arg == '-src_layer_field_content' and i + 1 < len(argv):
            i = i + 1
            src_layer_field_content = argv[i]
        elif arg == '-dsco' and i + 1 < len(argv):
            i = i + 1
            dsco.append(argv[i])
        elif arg == '-lco' and i + 1 < len(argv):
            i = i + 1
            lco.append(argv[i])
        elif arg == '-src_geom_type' and i + 1 < len(argv):
            i = i + 1
            src_geom_type_names = argv[i].split(',')
            for src_geom_type_name in src_geom_type_names:
                src_geom_type = _GetGeomType(src_geom_type_name)
                if src_geom_type is None:
                    print('ERROR: Unrecognized geometry type: %s' %
                          src_geom_type_name)
                    return 1
                src_geom_types.append(src_geom_type)
        elif arg[0] == '-':
            print('ERROR: Unrecognized argument : %s' % arg)
            return Usage()
        else:
            if '*' in arg:
                if sys.version_info < (3,0,0):
                    src_datasets += [fn.decode(sys.getfilesystemencoding()) for fn in glob.glob(arg)]
                else:
                    src_datasets += glob.glob(arg)
            else:
                src_datasets.append(arg)
        i = i + 1

    if dst_filename is None:
        print('Missing -o')
        return 1

    if update:
        if output_format is not None:
            print('ERROR: -f incompatible with -update')
            return 1
        if dsco:
            print('ERROR: -dsco incompatible with -update')
            return 1
        output_format = ''
    else:
        if output_format is None:
            output_format = GetOutputDriverFor(dst_filename)

    if src_layer_field_content is None:
        src_layer_field_content = '{AUTO_NAME}'
    elif src_layer_field_name is None:
        src_layer_field_name = 'source_ds_lyr'

    if not single_layer and output_format == 'ESRI Shapefile' and \
       dst_filename.lower().endswith('.shp'):
        print('ERROR: Non-single layer mode incompatible with non-directory '
              'shapefile output')
        return 1

    if not src_datasets:
        print('ERROR: No source datasets')
        return 1

    if layer_name_template is None:
        if single_layer:
            layer_name_template = 'merged'
        else:
            layer_name_template = '{AUTO_NAME}'

    vrt_filename = None
    if not EQUAL(output_format, 'VRT'):
        dst_ds = gdal.OpenEx(dst_filename, gdal.OF_VECTOR | gdal.OF_UPDATE)
        if dst_ds is not None:
            if not update and not overwrite_ds:
                print('ERROR: Destination dataset already exists, ' +
                      'but -update nor -overwrite_ds are specified')
                return 1
            if overwrite_ds:
                drv = dst_ds.GetDriver()
                dst_ds = None
                if drv.GetDescription() == 'OGR_VRT':
                    # We don't want to destroy the sources of the VRT
                    gdal.Unlink(dst_filename)
                else:
                    drv.Delete(dst_filename)
        elif update:
            print('ERROR: Destination dataset does not exist')
            return 1
        if dst_ds is None:
            drv = gdal.GetDriverByName(output_format)
            if drv is None:
                print('ERROR: Invalid driver: %s' % output_format)
                return 1
            dst_ds = drv.Create(
                dst_filename, 0, 0, 0, gdal.GDT_Unknown, dsco)
            if dst_ds is None:
                return 1

        vrt_filename = '/vsimem/_ogrmerge_.vrt'
    else:
        if gdal.VSIStatL(dst_filename) and not overwrite_ds:
            print('ERROR: Destination dataset already exists, ' +
                  'but -overwrite_ds are specified')
            return 1
        vrt_filename = dst_filename

    f = gdal.VSIFOpenL(vrt_filename, 'wb')
    if f is None:
        print('ERROR: Cannot create %s' % vrt_filename)
        return 1

    writer = XMLWriter(f)
    writer.open_element('OGRVRTDataSource')

    if single_layer:

        ogr_vrt_union_layer_written = False

        for src_ds_idx, src_dsname in enumerate(src_datasets):
            src_ds = ogr.Open(src_dsname)
            if src_ds is None:
                print('ERROR: Cannot open %s' % src_dsname)
                if skip_failures:
                    continue
                gdal.VSIFCloseL(f)
                gdal.Unlink(vrt_filename)
                return 1
            for src_lyr_idx, src_lyr in enumerate(src_ds):
                if src_geom_types:
                    gt = ogr.GT_Flatten(src_lyr.GetGeomType())
                    if gt not in src_geom_types:
                        continue

                if not ogr_vrt_union_layer_written:
                    ogr_vrt_union_layer_written = True
                    writer.open_element('OGRVRTUnionLayer',
                                        attrs={'name': layer_name_template})

                    if src_layer_field_name is not None:
                        writer.write_element_value('SourceLayerFieldName',
                                                   src_layer_field_name)

                    if field_strategy is not None:
                        writer.write_element_value('FieldStrategy',
                                                   field_strategy)

                layer_name = src_layer_field_content

                src_lyr_name = src_lyr.GetName()
                try:
                    src_lyr_name = src_lyr_name.decode('utf-8')
                except AttributeError:
                    pass

                basename = None
                if os.path.exists(src_dsname):
                    basename = os.path.basename(src_dsname)
                    if '.' in basename:
                        basename = '.'.join(basename.split(".")[0:-1])

                if basename == src_lyr_name:
                    layer_name = layer_name.replace('{AUTO_NAME}', basename)
                elif basename is None:
                    layer_name = layer_name.replace(
                        '{AUTO_NAME}',
                        'Dataset%d_%s' % (src_ds_idx, src_lyr_name))
                else:
                    layer_name = layer_name.replace(
                        '{AUTO_NAME}', basename + '_' + src_lyr_name)

                if basename is not None:
                    layer_name = layer_name.replace('{DS_BASENAME}', basename)
                else:
                    layer_name = layer_name.replace('{DS_BASENAME}',
                                                    src_dsname)
                layer_name = layer_name.replace('{DS_NAME}', '%s' %
                                                src_dsname)
                layer_name = layer_name.replace('{DS_INDEX}', '%d' %
                                                src_ds_idx)
                layer_name = layer_name.replace('{LAYER_NAME}',
                                                src_lyr_name)
                layer_name = layer_name.replace('{LAYER_INDEX}', '%d' %
                                                src_lyr_idx)

                if t_srs is not None:
                    writer.open_element('OGRVRTWarpedLayer')

                writer.open_element('OGRVRTLayer',
                                    attrs={'name': layer_name})
                attrs = {}
                if EQUAL(output_format, 'VRT') and \
                   os.path.exists(src_dsname) and \
                   not os.path.isabs(src_dsname) and \
                   '/' not in vrt_filename and \
                   '\\' not in vrt_filename:
                    attrs['relativeToVRT'] = '1'
                if single_layer:
                    attrs['shared'] = '1'
                writer.write_element_value('SrcDataSource', src_dsname,
                                           attrs=attrs)
                writer.write_element_value('SrcLayer', src_lyr.GetName())

                if a_srs is not None:
                    writer.write_element_value('LayerSRS', a_srs)

                writer.close_element('OGRVRTLayer')

                if t_srs is not None:
                    if s_srs is not None:
                        writer.write_element_value('SrcSRS', s_srs)

                    writer.write_element_value('TargetSRS', t_srs)

                    writer.close_element('OGRVRTWarpedLayer')

        if ogr_vrt_union_layer_written:
            writer.close_element('OGRVRTUnionLayer')

    else:

        for src_ds_idx, src_dsname in enumerate(src_datasets):
            src_ds = ogr.Open(src_dsname)
            if src_ds is None:
                print('ERROR: Cannot open %s' % src_dsname)
                if skip_failures:
                    continue
                gdal.VSIFCloseL(f)
                gdal.Unlink(vrt_filename)
                return 1
            for src_lyr_idx, src_lyr in enumerate(src_ds):
                if src_geom_types:
                    gt = ogr.GT_Flatten(src_lyr.GetGeomType())
                    if gt not in src_geom_types:
                        continue

                src_lyr_name = src_lyr.GetName()
                try:
                    src_lyr_name = src_lyr_name.decode('utf-8')
                except AttributeError:
                    pass

                layer_name = layer_name_template
                basename = None
                if os.path.exists(src_dsname):
                    basename = os.path.basename(src_dsname)
                    if '.' in basename:
                        basename = '.'.join(basename.split(".")[0:-1])

                if basename == src_lyr_name:
                    layer_name = layer_name.replace('{AUTO_NAME}', basename)
                elif basename is None:
                    layer_name = layer_name.replace(
                        '{AUTO_NAME}',
                        'Dataset%d_%s' % (src_ds_idx, src_lyr_name))
                else:
                    layer_name = layer_name.replace(
                        '{AUTO_NAME}', basename + '_' + src_lyr_name)

                if basename is not None:
                    layer_name = layer_name.replace('{DS_BASENAME}', basename)
                elif '{DS_BASENAME}' in layer_name:
                    if skip_failures:
                        if '{DS_INDEX}' not in layer_name:
                            layer_name = layer_name.replace(
                                '{DS_BASENAME}', 'Dataset%d' % src_ds_idx)
                    else:
                        print('ERROR: Layer name template %s '
                              'includes {DS_BASENAME} '
                              'but %s is not a file' %
                              (layer_name_template, src_dsname))

                        gdal.VSIFCloseL(f)
                        gdal.Unlink(vrt_filename)
                        return 1
                layer_name = layer_name.replace('{DS_NAME}', '%s' %
                                                src_dsname)
                layer_name = layer_name.replace('{DS_INDEX}', '%d' %
                                                src_ds_idx)
                layer_name = layer_name.replace('{LAYER_NAME}',
                                                src_lyr_name)
                layer_name = layer_name.replace('{LAYER_INDEX}', '%d' %
                                                src_lyr_idx)

                if t_srs is not None:
                    writer.open_element('OGRVRTWarpedLayer')

                writer.open_element('OGRVRTLayer',
                                    attrs={'name': layer_name})
                attrs = {}
                if EQUAL(output_format, 'VRT') and \
                   os.path.exists(src_dsname) and \
                   not os.path.isabs(src_dsname) and \
                   '/' not in vrt_filename and \
                   '\\' not in vrt_filename:
                    attrs['relativeToVRT'] = '1'
                if single_layer:
                    attrs['shared'] = '1'
                writer.write_element_value('SrcDataSource', src_dsname,
                                           attrs=attrs)
                writer.write_element_value('SrcLayer', src_lyr_name)

                if a_srs is not None:
                    writer.write_element_value('LayerSRS', a_srs)

                writer.close_element('OGRVRTLayer')

                if t_srs is not None:
                    if s_srs is not None:
                        writer.write_element_value('SrcSRS', s_srs)

                    writer.write_element_value('TargetSRS', t_srs)

                    writer.close_element('OGRVRTWarpedLayer')

    writer.close_element('OGRVRTDataSource')

    gdal.VSIFCloseL(f)

    ret = 0
    if not EQUAL(output_format, 'VRT'):
        accessMode = None
        if append:
            accessMode = 'append'
        elif overwrite_layer:
            accessMode = 'overwrite'
        ret = gdal.VectorTranslate(dst_ds, vrt_filename,
                                   accessMode=accessMode,
                                   layerCreationOptions=lco,
                                   skipFailures=skip_failures,
                                   callback=progress,
                                   callback_data=progress_arg)
        if ret == 1:
            ret = 0
        else:
            ret = 1
        gdal.Unlink(vrt_filename)

    return ret

###############################################################
# Entry point


def main():
    argv = sys.argv
    if sys.version_info < (3,0,0):
        argv = [fn.decode(sys.getfilesystemencoding()) for fn in argv]
    argv = ogr.GeneralCmdLineProcessor(argv)
    if argv is None:
        return 1
    return process(argv[1:])


if __name__ == '__main__':
    sys.exit(main())
