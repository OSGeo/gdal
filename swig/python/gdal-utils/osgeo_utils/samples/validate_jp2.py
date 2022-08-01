#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# ******************************************************************************
#  $Id$
#
#  Project:  GDAL
#  Purpose:  Validate JPEG2000 file structure
#  Author:   Even Rouault, <even dot rouault at spatialys dot com>
#
# ******************************************************************************
#  Copyright (c) 2015, European Union (European Environment Agency)
#
#  Permission is hereby granted, free of charge, to any person obtaining a
#  copy of this software and associated documentation files (the "Software"),
#  to deal in the Software without restriction, including without limitation
#  the rights to use, copy, modify, merge, publish, distribute, sublicense,
#  and/or sell copies of the Software, and to permit persons to whom the
#  Software is furnished to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice shall be included
#  in all copies or substantial portions of the Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
#  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
#  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
#  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
#  DEALINGS IN THE SOFTWARE.
# ******************************************************************************

import os
import sys
from osgeo import gdal
from osgeo import osr


def Usage():
    print('Usage: validate_jp2 [-expected_gmljp2] [-inspire_tg] [-datatype imagery|non_imagery]')
    print('                    [-oidoc in.xml] [-ogc_schemas_location path|disabled] test.jp2')
    print('')
    print('Options:')
    print('-expected_gmljp2: hint to indicate that a GMLJP2 box should be present.')
    print('-inspire_tg: Validate using Inspire Orthoimagery technical guidelines.')
    print('-datatype imagery|non_imagery: To specify the nature of the data. Defaults is imagery.')
    print('                               Only used by -inspire_tg')
    print('-oidoc: XML document conforming with Inspire Orthoimagery GML application schema.')
    print('-ogc_schemas_location: Path to directory with OGC schemas. Needed for GMLJP2 validation.')
    return 2


XML_TYPE_IDX = 0
XML_VALUE_IDX = 1
XML_FIRST_CHILD_IDX = 2


def find_xml_node(ar, element_name, only_attributes=False):
    # type = ar[XML_TYPE_IDX]
    value = ar[XML_VALUE_IDX]
    if value == element_name:
        return ar
    for child_idx in range(XML_FIRST_CHILD_IDX, len(ar)):
        child = ar[child_idx]
        if only_attributes and child[XML_TYPE_IDX] != gdal.CXT_Attribute:
            continue
        found = find_xml_node(child, element_name)
        if found is not None:
            return found
    return None


def get_attribute_val(ar, attr_name):
    node = find_xml_node(ar, attr_name, True)
    if node is None or node[XML_TYPE_IDX] != gdal.CXT_Attribute:
        return None
    if len(ar) > XML_FIRST_CHILD_IDX and \
            node[XML_FIRST_CHILD_IDX][XML_TYPE_IDX] == gdal.CXT_Text:
        return node[XML_FIRST_CHILD_IDX][XML_VALUE_IDX]
    return None


def find_message(ar):
    msg = get_attribute_val(ar, "message")
    if msg is None:
        return 'unknown'
    return msg


def find_element_with_name(ar, element_name, name, attribute_name='name'):
    typ = ar[XML_TYPE_IDX]
    value = ar[XML_VALUE_IDX]
    if typ == gdal.CXT_Element and value == element_name and get_attribute_val(ar, attribute_name) == name:
        return ar
    for child_idx in range(XML_FIRST_CHILD_IDX, len(ar)):
        child = ar[child_idx]
        found = find_element_with_name(child, element_name, name, attribute_name)
        if found:
            return found
    return None


def find_jp2box(ar, jp2box_name):
    return find_element_with_name(ar, 'JP2Box', jp2box_name)


def find_marker(ar, marker_name):
    return find_element_with_name(ar, 'Marker', marker_name)


def get_count_and_indices_of_jp2boxes(ar):
    the_dic = {}
    for child_idx in range(XML_FIRST_CHILD_IDX, len(ar)):
        child = ar[child_idx]
        if child[XML_TYPE_IDX] == gdal.CXT_Element and child[XML_VALUE_IDX] == 'JP2Box':
            jp2box_name = get_attribute_val(child, 'name')
            if jp2box_name in the_dic:
                the_dic[jp2box_name] = (the_dic[jp2box_name][0] + 1, the_dic[jp2box_name][1])
            else:
                the_dic[jp2box_name] = (1, child_idx)

    return the_dic


def get_count_of_uuidboxes(ar):
    the_dic = {}
    for child_idx in range(XML_FIRST_CHILD_IDX, len(ar)):
        child = ar[child_idx]
        if child[XML_TYPE_IDX] == gdal.CXT_Element and child[XML_VALUE_IDX] == 'JP2Box':
            jp2box_name = get_attribute_val(child, 'name')
            if jp2box_name == 'uuid':
                uuid = get_element_val(find_xml_node(child, 'UUID'))
                if uuid in the_dic:
                    the_dic[uuid] += 1
                else:
                    the_dic[uuid] = 1

    return the_dic


def find_field(ar, field_name):
    return find_element_with_name(ar, 'Field', field_name)


def get_element_val(node):
    if node is None:
        return None
    for child_idx in range(XML_FIRST_CHILD_IDX, len(node)):
        child = node[child_idx]
        if child[XML_TYPE_IDX] == gdal.CXT_Text:
            return child[XML_VALUE_IDX]
    return None


def get_field_val(ar, field_name):
    return get_element_val(find_field(ar, field_name))


def gdalOpenWithOpenJPEGDriverPreferably(filename):
    drivers = []
    jp2openjpeg_drv = gdal.GetDriverByName('JP2OpenJPEG')
    if jp2openjpeg_drv:
        # Deregister all drivers except JP2OpenJPEG if it exists
        for drvname in ['JP2KAK', 'JP2ECW', 'JP2OpenJPEG', 'JP2MrSID', 'JPEG2000']:
            drv = gdal.GetDriverByName(drvname)
            if drvname != 'JP2OpenJPEG' and drv is not None:
                drv.Deregister()
            drivers.append(drv)

    ds = gdal.Open(filename)

    # Re-register drivers
    if jp2openjpeg_drv:
        jp2openjpeg_drv.Deregister()
        for drv in drivers:
            if drv is not None:
                drv.Register()

    return ds


def get_gmljp2(filename):
    ds = gdalOpenWithOpenJPEGDriverPreferably(filename)
    if ds is None:
        return None
    mdd = ds.GetMetadata('xml:gml.root-instance')
    if mdd is None:
        return None
    return mdd[0]


class ErrorReport(object):
    def __init__(self, collect_internally=False):
        self.error_count = 0
        self.warning_count = 0
        self.collect_internally = collect_internally
        self.error_array = []
        self.warning_array = []

    def EmitError(self, category, msg, requirement=None, conformance_class=None):
        self.error_count += 1

        if category == 'PROFILE_1' and conformance_class is None:
            conformance_class = 'A.8.14'

        if requirement is not None and conformance_class is not None:
            full_msg = 'ERROR[%s, Requirement %d, Conformance class %s]: %s' % (category, requirement, conformance_class, msg)
        elif requirement is not None:
            full_msg = 'ERROR[%s, Requirement %d]: %s' % (category, requirement, msg)
        elif conformance_class is not None:
            full_msg = 'ERROR[%s, Conformance class %s]: %s' % (category, conformance_class, msg)
        else:
            full_msg = 'ERROR[%s]: %s' % (category, msg)
        if self.collect_internally:
            self.error_array.append(full_msg)
        else:
            print(full_msg)

    def EmitWarning(self, category, msg, recommendation=None):
        self.warning_count += 1
        if recommendation is not None:
            full_msg = 'WARNING[%s, Recommendation %d]: %s' % (category, recommendation, msg)
        else:
            full_msg = 'WARNING[%s]: %s' % (category, msg)
        if self.collect_internally:
            self.warning_array.append(full_msg)
        else:
            print(full_msg)

# Report JP2 boxes errors


def find_remaining_bytes(error_report, ar, parent_node_name=None):
    typ = ar[XML_TYPE_IDX]
    value = ar[XML_VALUE_IDX]
    if typ == gdal.CXT_Element and value == 'JP2Box':
        parent_node_name = get_attribute_val(ar, 'name')
    if typ == gdal.CXT_Element and value == 'RemainingBytes':
        error_report.EmitError('GENERAL', 'Remaining bytes in JP2 box %s: %s' % (parent_node_name, get_element_val(ar)))

    for child_idx in range(XML_FIRST_CHILD_IDX, len(ar)):
        child = ar[child_idx]
        find_remaining_bytes(error_report, child, parent_node_name)


# Report codestream errors
def find_errors(error_report, ar, parent_node=None):
    typ = ar[XML_TYPE_IDX]
    value = ar[XML_VALUE_IDX]
    if typ == gdal.CXT_Element and value == 'Error':
        parent_node_name = ''
        if parent_node is not None:
            parent_node_name = get_attribute_val(parent_node, 'name')
            if parent_node_name is None:
                parent_node_name = parent_node[XML_VALUE_IDX]
        error_report.EmitError('GENERAL', 'Codestream error found on element %s: %s' % (parent_node_name, find_message(ar)))

    for child_idx in range(XML_FIRST_CHILD_IDX, len(ar)):
        child = ar[child_idx]
        find_errors(error_report, child, ar)


def validate_bitsize(error_report, inspire_tg, val_ori, field_name, datatype):
    val = val_ori
    signedness = "unsigned"
    nbits = 0
    if val is not None:
        if val >= 128:
            signedness = "signed"
            val -= 128
        val += 1
        nbits = val
    if inspire_tg and val != 1 and val != 8 and val != 16 and val != 32:
        error_report.EmitError('INSPIRE_TG', '%s=%s (%s %d bits), which is not allowed' % (field_name, str(val_ori), signedness, nbits), requirement=24, conformance_class='A.8.9')
    elif inspire_tg and datatype == 'imagery' and ((val != 1 and val != 8 and val != 16) or val_ori >= 128):
        error_report.EmitError('INSPIRE_TG', '%s=%s (%s %d bits), which is not allowed for Orthoimagery (but OK for other data)' % (field_name, str(val_ori), signedness, nbits), requirement=27, conformance_class='A.8.9')
    elif val is None or val > 37:
        error_report.EmitError('GENERAL', '%s=%s (%s %d bits), which is not allowed' % (field_name, str(val_ori), signedness, nbits))


def int_or_none(val):
    if val is None:
        return None
    return int(val)


def check_geojp2_gmljp2_consistency(filename, error_report):
    gdal.SetConfigOption('GDAL_USE_GEOJP2', 'YES')
    gdal.SetConfigOption('GDAL_USE_GMLJP2', 'NO')
    ds = gdalOpenWithOpenJPEGDriverPreferably(filename)
    if ds is None:
        error_report.EmitError('GENERAL', 'Cannot open %s with a JPEG2000 compatible driver' % filename)
        return
    geojp2_gt = ds.GetGeoTransform()
    geojp2_wkt = ds.GetProjectionRef()
    geojp2_gcps = ds.GetGCPCount()
    ds = None

    gdal.SetConfigOption('GDAL_USE_GEOJP2', 'NO')
    gdal.SetConfigOption('GDAL_USE_GMLJP2', 'YES')
    ds = gdalOpenWithOpenJPEGDriverPreferably(filename)
    gmljp2_gt = ds.GetGeoTransform()
    gmljp2_wkt = ds.GetProjectionRef()
    ds = None

    gdal.SetConfigOption('GDAL_USE_GEOJP2', None)
    gdal.SetConfigOption('GDAL_USE_GMLJP2', None)

    if geojp2_gcps == 0:
        diff = False
        for i in range(6):
            if abs(geojp2_gt[i] - gmljp2_gt[i] > 1e-8):
                diff = True
        if diff:
            error_report.EmitError('GENERAL', 'Inconsistent geotransform between GeoJP2 (%s) and GMLJP2 (%s)' % (str(geojp2_gt), str(gmljp2_gt)))

    geojp2_sr = osr.SpatialReference()
    geojp2_sr.ImportFromWkt(geojp2_wkt)
    geojp2_epsg_code = geojp2_sr.GetAuthorityCode(None)
    gmljp2_sr = osr.SpatialReference()
    gmljp2_sr.ImportFromWkt(gmljp2_wkt)
    gmljp2_epsg_code = gmljp2_sr.GetAuthorityCode(None)
    if geojp2_sr.IsSame(gmljp2_sr) == 0 and geojp2_epsg_code != gmljp2_epsg_code:
        geojp2_proj4 = geojp2_sr.ExportToProj4()
        gmljp2_proj4 = gmljp2_sr.ExportToProj4()
        if geojp2_proj4 != gmljp2_proj4:
            error_report.EmitError('GENERAL', 'Inconsistent SRS between GeoJP2 (wkt=%s, proj4=%s) and GMLJP2 (wkt=%s, proj4=%s)' % (geojp2_wkt, geojp2_proj4, gmljp2_wkt, gmljp2_proj4))


# Check consistency of georeferencing of OrthoimageCoverage with the one embedded in the JPEG2000 file
def check_oi_rg_consistency(filename, serialized_oi_rg, error_report):
    if gdal.GetDriverByName('JP2OpenJPEG') is None:
        return

    ds = gdalOpenWithOpenJPEGDriverPreferably(filename)
    if ds is None:
        error_report.EmitError('GENERAL', 'Cannot open %s with a JPEG2000 compatible driver' % filename)
        return 'fail'
    gt = ds.GetGeoTransform()
    wkt = ds.GetProjectionRef()
    gcps = ds.GetGCPCount()
    ds = None

    gmljp2_from_oi = """<gml:FeatureCollection xmlns:gml="http://www.opengis.net/gml" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://www.opengis.net/gml http://schemas.opengis.net/gml/3.1.1/profiles/gmlJP2Profile/1.0.0/gmlJP2Profile.xsd">
        <gml:boundedBy>
        <gml:Null>withheld</gml:Null>
        </gml:boundedBy>
        <gml:featureMember>
        <gml:FeatureCollection>
            <gml:featureMember>
            <gml:RectifiedGridCoverage dimension="2" gml:id="RGC0001">
                <gml:rectifiedGridDomain>
                    %s
                </gml:rectifiedGridDomain>
                <gml:rangeSet>
                <gml:File>
                    <gml:rangeParameters />
                    <gml:fileName>gmljp2://codestream/0</gml:fileName>
                    <gml:fileStructure>Record Interleaved</gml:fileStructure>
                </gml:File>
                </gml:rangeSet>
            </gml:RectifiedGridCoverage>
            </gml:featureMember>
        </gml:FeatureCollection>
        </gml:featureMember>
    </gml:FeatureCollection>""" % serialized_oi_rg
    gdal.SetConfigOption('GMLJP2OVERRIDE', '/vsimem/override.gml')
    gdal.FileFromMemBuffer('/vsimem/override.gml', gmljp2_from_oi)
    fake_in_ds = gdal.GetDriverByName('MEM').Create('', 10, 10, 1)
    fake_in_ds.SetGeoTransform([0, 60, 0, 0, 0, -60])
    gdal.GetDriverByName('JP2OpenJPEG').CreateCopy('/vsimem/temp.jp2', fake_in_ds, options=['GeoJP2=NO'])
    gdal.SetConfigOption('GMLJP2OVERRIDE', None)
    gdal.Unlink('/vsimem/override.gml')

    ds = gdalOpenWithOpenJPEGDriverPreferably('/vsimem/temp.jp2')
    oi_gt = ds.GetGeoTransform()
    oi_wkt = ds.GetProjectionRef()
    ds = None
    gdal.Unlink('/vsimem/temp.jp2')

    if gcps == 0:
        diff = False
        for i in range(6):
            if abs(oi_gt[i] - gt[i] > 1e-8):
                diff = True
        if diff:
            error_report.EmitError('INSPIRE_TG', 'Inconsistent geotransform between OrthoImagery (%s) and GMLJP2/GeoJP2 (%s)' % (str(oi_gt), str(gt)), conformance_class='A.8.8')

    sr = osr.SpatialReference()
    sr.ImportFromWkt(wkt)
    epsg_code = sr.GetAuthorityCode(None)
    oi_sr = osr.SpatialReference()
    oi_sr.ImportFromWkt(oi_wkt)
    oi_epsg_code = oi_sr.GetAuthorityCode(None)
    if sr.IsSame(oi_sr) == 0 and epsg_code != oi_epsg_code:
        proj4 = sr.ExportToProj4()
        oi_proj4 = oi_sr.ExportToProj4()
        if proj4 != oi_proj4:
            error_report.EmitError('INSPIRE_TG', 'Inconsistent SRS between OrthoImagery (wkt=%s, proj4=%s) and GMLJP2/GeoJP2 (wkt=%s, proj4=%s)' % (wkt, proj4, oi_wkt, oi_proj4), conformance_class='A.8.8')


def validate(filename, oidoc, inspire_tg, expected_gmljp2, ogc_schemas_location, datatype='imagery', error_report=None, expected_ftyp_branding=None):

    if error_report is None:
        error_report = ErrorReport()

    ar = gdal.GetJPEG2000Structure(filename, ['ALL=YES'])
    if ar is None:
        error_report.error_count = 1
        return error_report

    find_remaining_bytes(error_report, ar)
    find_errors(error_report, ar)

    if inspire_tg and ar[XML_VALUE_IDX] != 'JP2File':
        error_report.EmitError('INSPIRE_TG', 'The file contains only a JPEG2000 codestream, instead of being a JP2 file')
        return error_report

    ihdr = None
    ihdr_nc = 0
    pclr = None
    bpc_vals = []

    if ar[XML_VALUE_IDX] == 'JP2File':

        # Check "jP " box
        if not find_jp2box(ar, 'jP  '):
            error_report.EmitError('GENERAL', '"jp  " box not found')

        # Detect GMLJP2 and validate it if possible
        gmljp2 = get_gmljp2(filename)
        gmljp2_found = gmljp2 is not None
        if expected_gmljp2 and not gmljp2_found:
            error_report.EmitError('GMLJP2', 'No GMLJP2 box found whereas it was expected')
        if gmljp2_found and inspire_tg and 'gmljp2:GMLJP2CoverageCollection' in gmljp2:
            error_report.EmitError('INSPIRE_TG', 'GMLJP2 v2 box found, but Inspire TG require GMLJP2 v1', conformance_class='A.8.6')
        if gmljp2_found and ogc_schemas_location != 'disabled':
            if ogc_schemas_location is not None:
                sys.path.append(os.path.dirname(os.path.realpath(__file__)) + '/../../../../autotest/pymod')
                try:
                    import xmlvalidate
                    xml_validate_found = True
                except ImportError:
                    error_report.EmitWarning('GMLJP2', 'xmlvalidate not found or not runnable')
                    xml_validate_found = False
                if xml_validate_found:
                    if not xmlvalidate.validate(gmljp2, ogc_schemas_location=ogc_schemas_location):
                        error_report.EmitError('GMLJP2', 'Validation of GMLJP2 document failed', conformance_class='A.8.6')
            else:
                error_report.EmitWarning('GMLJP2', '-ogc_schemas_location not specified')

        # Check that there's only one GeoTIFF box
        count_uuidboxes = get_count_of_uuidboxes(ar)
        if 'B14BF8BD083D4B43A5AE8CD7D5A6CE03' in count_uuidboxes:
            count_geotiff_boxes = count_uuidboxes['B14BF8BD083D4B43A5AE8CD7D5A6CE03']
            if count_geotiff_boxes > 1:
                error_report.EmitError('GeoJP2', '%d GeoTIFF UUID box found' % count_geotiff_boxes)

        # Check the content of a GeoTIFF UUID box
        geotiff_found = find_element_with_name(ar, "UUID", "GeoTIFF", attribute_name="description") is not None
        decoded_geotiff = find_xml_node(ar, "DecodedGeoTIFF")
        if geotiff_found:
            if not decoded_geotiff:
                error_report.EmitError('GeoJP2', 'GeoTIFF UUID box found, but content is not valid GeoTIFF')
            else:
                vrtdataset = find_xml_node(ar, "VRTDataset")
                w = get_attribute_val(vrtdataset, "rasterXSize")
                if w != '1':
                    error_report.EmitError('GeoJP2', 'GeoTIFF should have width of 1 pixel, not %s' % str(w))
                h = get_attribute_val(vrtdataset, "rasterXSize")
                if h != '1':
                    error_report.EmitError('GeoJP2', 'GeoTIFF should have height of 1 pixel, not %s' % str(h))

        # Check that information of GeoJP2 and GMLJP2 are consistent
        if geotiff_found and gmljp2_found:
            check_geojp2_gmljp2_consistency(filename, error_report)

        # Check "ftyp" box
        ftyp = find_jp2box(ar, 'ftyp')
        if ftyp:
            JP2CLFound = False
            JPXCLFound = False

            if expected_ftyp_branding is None:
                if gmljp2_found and 'gmljp2:GMLJP2CoverageCollection' in gmljp2:
                    expected_ftyp_branding = 'jpx '
                else:
                    expected_ftyp_branding = 'jp2 '

            if get_field_val(ftyp, 'BR') != expected_ftyp_branding:
                error_report.EmitError('GENERAL', 'ftyp.BR = "%s" instead of "%s"' % (get_field_val(ftyp, 'BR'), expected_ftyp_branding))

            if get_field_val(ftyp, 'MinV') != '0':
                error_report.EmitError('GENERAL', 'ftyp.MinV = "%s" instead of 0' % get_field_val(ftyp, 'MinV'))

            for i in range(10):
                val = get_field_val(ftyp, 'CL%d' % i)
                if val is None:
                    break
                if val == 'jp2 ':
                    JP2CLFound = True
                if val == 'jpx ':
                    JPXCLFound = True
            if not JP2CLFound:
                error_report.EmitError('GENERAL', '"jp2 " not found in compatibility list of ftyp')
            if gmljp2_found and not JPXCLFound:
                if inspire_tg:
                    error_report.EmitError('INSPIRE_TG', '"jpx " not found in compatibility list of ftyp, but GMLJP2 box present')
                else:
                    error_report.EmitWarning('GENERAL', '"jpx " not found in compatibility list of ftyp, but GMLJP2 box present')
        else:
            error_report.EmitError('GENERAL', '"ftyp" box not found')

        # Check "rreq" box
        rreq = find_jp2box(ar, 'rreq')
        if inspire_tg and gmljp2_found and not rreq:
            error_report.EmitError('INSPIRE_TG', '"rreq" box not found, but GMLJP2 box present')
        elif rreq:
            NSF = get_field_val(rreq, 'NSF')
            if NSF is None:
                error_report.EmitError('GENERAL', 'rreq.NSF not found')
                NSF = 0
            NSF = int(NSF)
            SF67Found = False
            NSF_found = 0
            for i in range(1000):
                val = get_field_val(rreq, 'SF%d' % i)
                if val is None:
                    break
                if val == '67':
                    SF67Found = True
                val = get_field_val(rreq, 'SM%d' % i)
                if val is None:
                    error_report.EmitError('GENERAL', 'rreq.SM[%d] not found' % i)
                    break
                NSF_found += 1
            if NSF != NSF_found:
                error_report.EmitError('GENERAL', 'rreq.NSF (=%d) != NSF_found (=%d)' % (NSF, NSF_found))
            if gmljp2_found and not SF67Found:
                if inspire_tg:
                    error_report.EmitError('INSPIRE_TG', '"rreq" box does not advertise standard flag 67 whereas GMLJP2 box is present')
                else:
                    error_report.EmitWarning('GENERAL', '"rreq" box does not advertise standard flag 67 whereas GMLJP2 box is present')

            NVF = get_field_val(rreq, 'NVF')
            if NVF is None:
                error_report.EmitError('GENERAL', 'rreq.NVF not found')
                NVF = 0
            NVF = int(NVF)
            NVF_found = 0
            for i in range(1000):
                val = get_field_val(rreq, 'VF%d' % i)
                if val is None:
                    break
                val = get_field_val(rreq, 'VM%d' % i)
                if val is None:
                    error_report.EmitError('GENERAL', 'rreq.VM[%d] not found' % i)
                    break
                NVF_found += 1
            if NVF != NVF_found:
                error_report.EmitError('GENERAL', 'rreq.NVF (=%d) != NVF_found (=%d)' % (NVF, NVF_found))

        # Check "jp2h" box
        jp2h = find_jp2box(ar, 'jp2h')
        if not jp2h:
            error_report.EmitError('GENERAL', '"jp2h" box not found')
        else:
            # Check "ihdr" subbox
            ihdr = find_jp2box(jp2h, 'ihdr')
            if not ihdr:
                error_report.EmitError('GENERAL', '"ihdr" box not found')
            else:
                ihdr_height = int_or_none(get_field_val(ihdr, 'HEIGHT'))
                if ihdr_height is None:
                    error_report.EmitError('GENERAL', 'invalid value ihdr.HEIGHT = %s' % str(ihdr_height))
                    ihdr_height = 0
                elif inspire_tg and ihdr_height > 2**31:
                    error_report.EmitError('INSPIRE_TG', 'ihdr.height = %d, whereas only 31 bits are allowed for Profile 1' % ihdr_height)

                ihdr_width = int_or_none(get_field_val(ihdr, 'WIDTH'))
                if ihdr_width is None:
                    error_report.EmitError('GENERAL', 'invalid value ihdr.WIDTH = %s' % str(ihdr_width))
                    ihdr_width = 0
                elif inspire_tg and ihdr_width > 2**31:
                    error_report.EmitError('INSPIRE_TG', 'ihdr.width = %d, whereas only 31 bits are allowed for Profile 1' % ihdr_width)

                ihdr_nc = int_or_none(get_field_val(ihdr, 'NC'))
                if ihdr_nc is None or ihdr_nc > 16384:
                    error_report.EmitError('GENERAL', 'invalid value ihdr.NC = %s' % str(ihdr_nc))
                    ihdr_nc = 0

                ihdr_bpcc = int_or_none(get_field_val(ihdr, 'BPC'))
                if ihdr_bpcc != 255:
                    validate_bitsize(error_report, inspire_tg, ihdr_bpcc, 'ihdr.bpcc', datatype)

                ihdr_c = int_or_none(get_field_val(ihdr, 'C'))
                if ihdr_c != 7:
                    error_report.EmitError('GENERAL', 'ihdr.C = %s instead of 7' % str(ihdr_c))

                ihdr_unkc = int_or_none(get_field_val(ihdr, 'UnkC'))
                if ihdr_unkc is None or ihdr_unkc > 1:
                    error_report.EmitError('GENERAL', 'ihdr.UnkC = %s instead of 0 or 1' % str(ihdr_unkc))

                ihdr_ipr = int_or_none(get_field_val(ihdr, 'IPR'))
                if ihdr_ipr is None or ihdr_ipr > 1:
                    error_report.EmitError('GENERAL', 'ihdr.IPR = %s instead of 0 or 1' % str(ihdr_ipr))

            # Check optional "bpcc" subbox
            bpcc = find_jp2box(jp2h, 'bpcc')
            if ihdr and ihdr_bpcc == 255:
                if not bpcc:
                    error_report.EmitError('GENERAL', '"bpcc" box not found whereas ihdr.bpcc requires it')
            elif ihdr and bpcc and ihdr_bpcc != 255:
                error_report.EmitWarning('GENERAL', '"bpcc" box found whereas ihdr.bpcc does not require it')
            if ihdr and bpcc:
                for i in range(16384):
                    val = get_field_val(bpcc, 'BPC%d' % i)
                    if val is None:
                        break
                    val = int(val)
                    bpc_vals.append(val)

                    validate_bitsize(error_report, inspire_tg, val, 'bpcc.BPC[%d]' % i, datatype)

                if len(bpc_vals) != ihdr_nc:
                    error_report.EmitWarning('GENERAL', '"bpcc" box has %d elements whereas ihdr.nc = %d' % (len(bpc_vals), ihdr_nc))

            if ihdr and not bpcc:
                bpc_vals = [ihdr_bpcc for i in range(ihdr_nc)]

            # Check "colr" subbox
            colr = find_jp2box(jp2h, 'colr')
            if not colr:
                error_report.EmitError('GENERAL', '"colr" box not found')
            else:
                meth = int_or_none(get_field_val(colr, 'METH'))
                if meth != 1 and meth != 2:
                    error_report.EmitWarning('GENERAL', 'Unknown value %s for colr.METH' % str(meth))

                prec = int_or_none(get_field_val(colr, 'PREC'))
                if prec is None or (inspire_tg and prec != 0):
                    error_report.EmitWarning('GENERAL', 'Unknown value %s for colr.PREC' % str(prec))

                approx = int_or_none(get_field_val(colr, 'APPROX'))
                if approx or (inspire_tg and approx != 0):
                    error_report.EmitWarning('GENERAL', 'Unknown value %s for colr.APPROX' % str(approx))

                if meth == 1:
                    enum_cs = int_or_none(get_field_val(colr, 'EnumCS'))
                    if enum_cs is None or (inspire_tg and enum_cs != 16 and enum_cs != 17 and enum_cs != 18):
                        error_report.EmitWarning('GENERAL', 'Unknown value %s for colr.EnumCS' % str(enum_cs))
                else:
                    enum_cs = None

            # Check optional "pclr" subbox
            pclr = find_jp2box(jp2h, 'pclr')
            if ihdr and pclr:
                if inspire_tg and ihdr_nc != 1:
                    error_report.EmitError('INSPIRE_TG', 'pclr box found but ihdr.nc = %d' % (ihdr_nc))
                pclr_NE = int_or_none(get_field_val(pclr, 'NE'))
                if pclr_NE is None:
                    error_report.EmitWarning('GENERAL', 'Invalid value %s for pclr.NE' % str(pclr_NE))
                    pclr_NE = 0

                pclr_NPC = int_or_none(get_field_val(pclr, 'NPC'))
                if pclr_NPC is None:
                    error_report.EmitWarning('GENERAL', 'Invalid value %s for pclr.NPC' % str(pclr_NPC))
                    pclr_NPC = 0
                if inspire_tg and pclr_NPC != 3:
                    error_report.EmitError('INSPIRE_TG', 'pclr.NPC(=%d) != 3 (for color table)' % (pclr_NPC), conformance_class='A.8.6')

                if ihdr_bpcc == 7 and pclr_NE > 256:
                    error_report.EmitError('GENERAL', '%d entries in pclr box, but 8 bit depth' % (pclr_NE))
                for i in range(pclr_NPC):
                    val = get_field_val(pclr, 'B%d' % i)
                    if val is None:
                        error_report.EmitError('GENERAL', 'pclr.B%d not found' % (i))
                        break
                    val = int(val)
                    validate_bitsize(error_report, inspire_tg, val, 'pclr.B[%d]' % i, datatype)
                if pclr_NE > 0 and pclr_NPC > 0:
                    val = get_field_val(pclr, 'C_%d_%d' % (pclr_NE - 1, pclr_NPC - 1))
                    if val is None:
                        error_report.EmitError('GENERAL', 'pclr.C_%d_%d not found' % (pclr_NE - 1, pclr_NPC - 1))

            # Check optional "cmap" subbox
            cmap = find_jp2box(jp2h, 'cmap')
            if cmap:
                if pclr is None:
                    error_report.EmitError('GENERAL', 'cmap box found but no pclr box')
                else:
                    cmap_count = 0
                    PCOL_mapping = {}
                    for i in range(16384):
                        CMP = get_field_val(cmap, 'CMP%d' % i)
                        if CMP is None:
                            break
                        CMP = int(CMP)
                        if CMP >= ihdr_nc:
                            error_report.EmitError('GENERAL', 'cmap.CMP[%d] = %d is invalid' % (i, CMP))

                        MTYP = get_field_val(cmap, 'MTYP%d' % i)
                        if MTYP is None:
                            error_report.EmitError('GENERAL', 'cmap.MTYP[%d] missing' % i)
                            break
                        MTYP = int(MTYP)
                        if inspire_tg and MTYP != 1:
                            error_report.EmitError('INSPIRE_TG', 'cmap.MTYP[%d] = %d is invalid' % (i, MTYP))

                        PCOL = get_field_val(cmap, 'PCOL%d' % i)
                        if PCOL is None:
                            error_report.EmitError('GENERAL', 'cmap.PCOL[%d] missing' % i)
                            break
                        PCOL = int(PCOL)
                        if ihdr_nc == 1 and PCOL >= pclr_NPC:
                            error_report.EmitError('GENERAL', 'cmap.PCOL[%d] = %d is invalid' % (i, PCOL))
                        if MTYP == 1:
                            if PCOL in PCOL_mapping:
                                error_report.EmitError('GENERAL', 'cmap.PCOL[%d] = %d is invalid since already used' % (i, PCOL))
                            PCOL_mapping[PCOL] = True

                        cmap_count += 1

                    if ihdr_nc == 1 and cmap_count != pclr_NPC:
                        error_report.EmitError('GENERAL', 'cmap box contains %d channel definitions but pclr.NPC = %d' % (cmap_count, pclr_NPC))

            else:
                if pclr:
                    error_report.EmitError('GENERAL', 'cmap box not found but pclr exists')

            # Check optional "cdef" subbox
            cdef = find_jp2box(jp2h, 'cdef')
            transparent_index = -1
            if cdef and pclr:
                error_report.EmitWarning('GENERAL', 'cdef found and pclr also. Ignoring cdef as it is unusual')
            elif ihdr and cdef:
                cdef_N = int(get_field_val(cdef, 'N'))
                if cdef_N != ihdr_nc:
                    error_report.EmitError('GENERAL', 'cdef.N = %d whereas ihdr.nc = %d' % (cdef_N, ihdr_nc))
                cdef_count = 0
                cn_mapping = {}
                typ_alpha_used = False
                asoc_mapping = {}
                asoc_whole_used = False
                for i in range(16384):
                    cn = get_field_val(cdef, 'Cn%d' % i)
                    if cn is None:
                        break
                    cn = int(cn)
                    if cn < 0 or cn >= ihdr_nc:
                        error_report.EmitError('GENERAL', 'cdef.cn[%d] = %d is invalid' % (i, cn))
                    elif cn in cn_mapping:
                        error_report.EmitError('GENERAL', 'cdef.cn[%d] = %d is invalid since already used' % (i, cn))
                    cn_mapping[cn] = True

                    typ = get_field_val(cdef, 'Typ%d' % i)
                    if typ is None:
                        error_report.EmitError('GENERAL', 'cdef.typ[%d] missing' % i)
                        break
                    typ = int(typ)
                    if typ != 0 and typ != 1 and typ != 2 and typ != 65535:
                        error_report.EmitError('GENERAL', 'cdef.typ[%d] = %d is invalid' % (i, typ))
                    if typ == 1 or typ == 2:
                        if inspire_tg and cn < len(bpc_vals) and bpc_vals[cn] != 0:
                            error_report.EmitWarning('INSPIRE_TG', 'Bit depth of alpha channel should be 1 (BPCC 0), but its BPCC is %d' % bpc_vals[cn], recommendation=38)
                        if typ_alpha_used and inspire_tg:
                            error_report.EmitError('GENERAL', 'cdef.typ[%d] = %d is invalid since another alpha channel has already been defined' % (i, typ))
                        transparent_index = cn
                        typ_alpha_used = True

                    asoc = get_field_val(cdef, 'Asoc%d' % i)
                    if asoc is None:
                        error_report.EmitError('GENERAL', 'cdef.asoc[%d] missing' % i)
                        break
                    asoc = int(asoc)
                    if inspire_tg and asoc == 0:
                        if not(typ == 1 or typ == 2):
                            error_report.EmitError('GENERAL', 'cdef.asoc[%d] = %d whereas cdef.typ[%d] = %d' % (i, asoc, i, typ))
                        if asoc_whole_used:
                            error_report.EmitError('GENERAL', 'cdef.asoc[%d] = %d is invalid since another band has already been associated to whole image' % (i, asoc))
                        asoc_whole_used = True
                    elif colr and asoc > 0 and asoc < 65535:
                        if asoc > ihdr_nc:
                            error_report.EmitError('GENERAL', 'cdef.asoc[%d] = %d is invalid' % (i, asoc))
                        elif asoc in asoc_mapping:
                            error_report.EmitError('GENERAL', 'cdef.asoc[%d] = %d is invalid since already used' % asoc)
                        asoc_mapping[asoc] = True

                    cdef_count += 1

                if cdef_count != cdef_N:
                    error_report.EmitError('GENERAL', 'cdef.N = %d whereas there are %d channel definitions' % (cdef_N, cdef_count))

            # Check that all bands have the same bit-depth, except the alpha band than can (should) be 1-bit
            if inspire_tg:
                for i, bpc_val in enumerate(bpc_vals):
                    if i == transparent_index:
                        if bpc_val != bpc_vals[0] and bpc_val != 0:
                            error_report.EmitError('INSPIRE_TG', 'Band %d has bpc=%d, which is different from first band whose value is %d' % (i, bpc_val, bpc_vals[0]), requirement=25)
                    elif bpc_val != bpc_vals[0]:
                        error_report.EmitError('INSPIRE_TG', 'Band %d has bpc=%d, which is different from first band whose value is %d' % (i, bpc_val, bpc_vals[0]), requirement=25)

            # Check optional "res " subbox
            res = find_jp2box(jp2h, 'res ')
            if res:
                count_boxes = get_count_and_indices_of_jp2boxes(res)
                for key in count_boxes:
                    (val, _) = count_boxes[key]
                    if val > 1:
                        error_report.EmitError('GENERAL', '"%s" box expected to be found zero or one time, but present %d times' % (key, val))
                    if key not in ('resd', 'resc'):
                        error_report.EmitWarning('GENERAL', '"%s" box not expected' % key)

            # Check number of sub-boxes
            count_boxes = get_count_and_indices_of_jp2boxes(jp2h)
            for key in count_boxes:
                (val, _) = count_boxes[key]
                if val > 1:
                    error_report.EmitError('GENERAL', '"%s" box expected to be found zero or one time, but present %d times' % (key, val))
                if key not in ('ihdr', 'bpcc', 'colr', 'pclr', 'cmap', 'cdef', 'res '):
                    error_report.EmitWarning('GENERAL', '"%s" box not expected' % key)

            # Check order of boxes
            last_idx = -1
            for box_name in ['ihdr', 'bpcc', 'colr', 'pclr', 'cmap', 'cdef', 'res ']:
                if box_name in count_boxes:
                    (_, idx) = count_boxes[box_name]
                    if idx < last_idx:
                        error_report.EmitWarning('GENERAL', '"%s" box not at expected index' % box_name)
                    last_idx = idx

        # Check "jp2c" box
        jp2c = find_jp2box(ar, 'jp2c')
        if not jp2c:
            error_report.EmitError('GENERAL', '"jp2c" box not found')

        if ihdr and ihdr_ipr == 1 and not find_jp2box(ar, 'jp2i'):
            error_report.EmitWarning('GENERAL', 'ihdr.ipr = 1 but no jp2i box found')
        elif ihdr and ihdr_ipr == 0 and find_jp2box(ar, 'jp2i'):
            error_report.EmitWarning('GENERAL', 'ihdr.ipr = 0 but jp2i box found')

        # Check number of boxes
        count_boxes = get_count_and_indices_of_jp2boxes(ar)
        for key in count_boxes:
            (val, _) = count_boxes[key]
            if key in ('jP  ', 'ftyp', 'rreq', 'jp2h', 'jp2c'):
                if key == 'jp2c':
                    if inspire_tg and val > 1:
                        error_report.EmitError('INSPIRE_TG', '"%s" box expected to be found one time, but present %d times' % (key, val), requirement=23, conformance_class='A.8.15')
                elif val > 1:
                    error_report.EmitError('GENERAL', '"%s" box expected to be found zero or one time, but present %d times' % (key, val))
            elif key not in ('jp2i', 'asoc', 'xml ', 'uuid', 'uinf'):
                error_report.EmitWarning('GENERAL', '"%s" box not expected' % key)

        # Check order of boxes
        last_idx = -1
        for box_name in ['jP  ', 'ftyp', 'rreq', 'jp2h', 'jp2c', 'jp2i']:
            if box_name in count_boxes:
                (_, idx) = count_boxes[box_name]
                if idx < last_idx:
                    error_report.EmitWarning('GENERAL', '"%s" box not at expected index' % box_name)
                last_idx = idx
        if inspire_tg:
            for box_name in ['asoc', 'xml ', 'uuid', 'uinf']:
                if box_name in count_boxes:
                    (_, idx) = count_boxes[box_name]
                    if idx < last_idx:
                        error_report.EmitWarning('INSPIRE_TG', '"%s" box not at expected index' % box_name)
                    last_idx = idx

    cs = find_xml_node(ar, 'JP2KCodeStream')
    if cs is None:
        return error_report

    soc = find_marker(cs, 'SOC')
    if not soc:
        error_report.EmitError('GENERAL', 'No SOC marker found')

    # Validate content of SIZ marker
    siz = find_marker(cs, 'SIZ')
    Csiz = 0
    Xsiz = 0
    Ysiz = 0
    XOsiz = 0
    YOsiz = 0
    Rsiz = 0
    tab_Ssiz = []
    if not siz:
        error_report.EmitError('GENERAL', 'No SIZ marker found')
    else:
        while True:
            Csiz = get_field_val(siz, 'Csiz')
            if Csiz is None:
                error_report.EmitError('GENERAL', 'SIZ.Csiz not found')
                break
            Csiz = int(Csiz)

            Rsiz = int(get_field_val(siz, 'Rsiz'))
            if inspire_tg and Rsiz != 2:
                error_report.EmitError('INSPIRE_TG', 'SIZ.Rsiz=%d found but 2 (Profile 1) expected' % Rsiz, requirement=21)

            Xsiz = int(get_field_val(siz, 'Xsiz'))
            Ysiz = int(get_field_val(siz, 'Ysiz'))
            XOsiz = int(get_field_val(siz, 'XOsiz'))
            YOsiz = int(get_field_val(siz, 'YOsiz'))
            XTsiz = int(get_field_val(siz, 'XTsiz'))
            YTsiz = int(get_field_val(siz, 'YTsiz'))
            XTOSiz = int(get_field_val(siz, 'XTOSiz'))
            YTOSiz = int(get_field_val(siz, 'YTOSiz'))

            if (inspire_tg or Rsiz == 2) and Xsiz >= 2**31:
                error_report.EmitError('PROFILE_1', 'Xsiz = %d, whereas only 31 bits are allowed for Profile 1' % Xsiz)
            if (inspire_tg or Rsiz == 2) and Ysiz >= 2**31:
                error_report.EmitError('PROFILE_1', 'Ysiz = %d, whereas only 31 bits are allowed for Profile 1' % Ysiz)
            if (inspire_tg or Rsiz == 2) and XOsiz >= 2**31:
                error_report.EmitError('PROFILE_1', 'XOsiz = %d, whereas only 31 bits are allowed for Profile 1' % XOsiz)
            if (inspire_tg or Rsiz == 2) and YOsiz >= 2**31:
                error_report.EmitError('PROFILE_1', 'YOsiz = %d, whereas only 31 bits are allowed for Profile 1' % YOsiz)
            if (inspire_tg or Rsiz == 2) and XTOSiz >= 2**31:
                error_report.EmitError('PROFILE_1', 'XTOSiz = %d, whereas only 31 bits are allowed for Profile 1' % XTOSiz)
            if (inspire_tg or Rsiz == 2) and YTOSiz >= 2**31:
                error_report.EmitError('PROFILE_1', 'YTOSiz = %d, whereas only 31 bits are allowed for Profile 1' % YTOSiz)
            if ihdr and ihdr_width != Xsiz - XOsiz:
                error_report.EmitError('GENERAL', 'ihdr_width(=%d) != Xsiz (=%d)- XOsiz(=%d)' % (ihdr_width, Xsiz, XOsiz))
            if ihdr and ihdr_height != Ysiz - YOsiz:
                error_report.EmitError('GENERAL', 'ihdr_height(=%d) != Ysiz(=%d) - YOsiz(=%d)' % (ihdr_height, Ysiz, YOsiz))
            if ihdr and ihdr_nc != Csiz:
                error_report.EmitError('GENERAL', 'ihdr_nc(=%d) != Csiz (=%d)' % (ihdr_nc, Csiz))

            min_XYRSiz = 255
            for i in range(Csiz):
                Ssiz = get_field_val(siz, 'Ssiz%d' % i)
                if Ssiz is None:
                    error_report.EmitError('GENERAL', 'SIZ.Ssiz[%d] not found' % i)
                    break
                Ssiz = int(Ssiz)
                validate_bitsize(error_report, inspire_tg, Ssiz, 'SIZ.Ssiz[%d]' % i, datatype)
                tab_Ssiz.append(Ssiz)

                if bpc_vals and i < len(bpc_vals) and bpc_vals[i] != Ssiz:
                    error_report.EmitError('GENERAL', 'SIZ.Ssiz[%d]=%s, whereas bpcc[%d]=%s' % (i, str(Ssiz), i, str(bpc_vals[i])))

                XRsiz = get_field_val(siz, 'XRsiz%d' % i)
                if XRsiz is None:
                    error_report.EmitError('GENERAL', 'SIZ.XRsiz[%d] not found' % i)
                    break
                XRsiz = int(XRsiz)
                if XRsiz == 0:
                    error_report.EmitError('GENERAL', 'XRsiz[%d] = %d is invalid' % (i, XRsiz))
                elif XRsiz < min_XYRSiz:
                    min_XYRSiz = XRsiz

                YRsiz = get_field_val(siz, 'YRsiz%d' % i)
                if YRsiz is None:
                    error_report.EmitError('GENERAL', 'SIZ.YRsiz[%d] not found' % i)
                    break
                YRsiz = int(YRsiz)
                if YRsiz == 0:
                    error_report.EmitError('GENERAL', 'YRsiz[%d] = %d is invalid' % (i, YRsiz))
                elif YRsiz < min_XYRSiz:
                    min_XYRSiz = YRsiz

            tiled = not (XTsiz + XTOSiz >= Xsiz and YTsiz + YTOSiz >= Ysiz)
            if (inspire_tg or Rsiz == 2) and XTsiz / min_XYRSiz > 1024 and tiled:
                error_report.EmitError('PROFILE_1', 'XTsiz / min_XYRSiz = %f > 1024' % (1.0 * XTsiz / min_XYRSiz))
            if (inspire_tg or Rsiz == 2) and XTsiz != YTsiz and tiled:
                error_report.EmitError('PROFILE_1', 'XTsiz (=%d) != YTsiz (=%d)' % (XTsiz, YTsiz))

            break

    # Check GMLJP2 RectifiedGrid envelope against codestream dimensions
    if gmljp2_found:
        gmljp2_node = gdal.ParseXMLString(gmljp2)
        rg = find_xml_node(gmljp2_node, 'gml:RectifiedGrid')
        if rg is None:
            rg = find_xml_node(gmljp2_node, 'RectifiedGrid')
        if rg is None:
            error_report.EmitError('GMLJP2', 'Cannot find RectifiedGrid in GMLJP2')
        else:
            low = find_xml_node(rg, 'low')
            if low is None:
                low = find_xml_node(rg, 'gml:low')
            high = find_xml_node(rg, 'high')
            if high is None:
                high = find_xml_node(rg, 'gml:high')
            if low and high:
                low = get_element_val(low)
                (low_x, low_y) = low.split(' ')[0:2]
                low_x = int(low_x)
                low_y = int(low_y)

                if low_x != XOsiz:
                    error_report.EmitError('GMLJP2', 'RectifiedGrid.limits.GridEnvelope.low[x] != XOsiz')
                if low_y != YOsiz:
                    error_report.EmitError('GMLJP2', 'RectifiedGrid.limits.GridEnvelope.low[y] != YOsiz')

                high = get_element_val(high)
                (high_x, high_y) = high.split(' ')[0:2]
                high_x = int(high_x)
                high_y = int(high_y)

                if high_x != Xsiz - 1:
                    error_report.EmitError('GMLJP2', 'RectifiedGrid.limits.GridEnvelope.high[x] != Xsiz - 1')
                if high_y != Ysiz - 1:
                    error_report.EmitError('GMLJP2', 'RectifiedGrid.limits.GridEnvelope.high[y] != Ysiz - 1')
            else:
                error_report.EmitError('GMLJP2', 'Cannot find low/high node in RectifiedGrid')

    # Check against Orthoimagery document
    if oidoc:
        oidoc_content = open(oidoc).read()
        oidoc_node = gdal.ParseXMLString(oidoc_content)
        if oidoc_node is None:
            error_report.EmitError('GENERAL', 'Cannot parse %s' % oidoc)
        else:
            oic = find_xml_node(oidoc_node, 'OrthoimageCoverage')
            if oic is None:
                oic = find_xml_node(oidoc_node, 'oi:OrthoimageCoverage')
            if oic is None:
                error_report.EmitError('GENERAL', 'Cannot find OrthoimageCoverage in %s' % oidoc)
            else:
                # Check RectifiedGrid envelope against codestream dimensions
                rg = find_xml_node(oic, 'gml:RectifiedGrid')
                if rg is None:
                    rg = find_xml_node(oic, 'RectifiedGrid')
                if rg is None:
                    if expected_gmljp2:
                        error_report.EmitError('INSPIRE_TG', 'Cannot find RectifiedGrid in OrthoImageryCoverage')
                else:
                    low = find_xml_node(rg, 'low')
                    if low is None:
                        low = find_xml_node(rg, 'gml:low')
                    high = find_xml_node(rg, 'high')
                    if high is None:
                        high = find_xml_node(rg, 'gml:high')
                    if low and high:
                        low = get_element_val(low)
                        (low_x, low_y) = low.split(' ')[0:2]
                        low_x = int(low_x)
                        low_y = int(low_y)

                        if low_x != XOsiz:
                            error_report.EmitError('INSPIRE_TG', 'RectifiedGrid.limits.GridEnvelope.low[x](=%d) != XOsiz(=%d)' % (low_x, XOsiz), conformance_class='A.8.6')
                        if low_y != YOsiz:
                            error_report.EmitError('INSPIRE_TG', 'RectifiedGrid.limits.GridEnvelope.low[y](=%d) != YOsiz(=%d)' % (low_y, YOsiz), conformance_class='A.8.6')

                        high = get_element_val(high)
                        (high_x, high_y) = high.split(' ')[0:2]
                        high_x = int(high_x)
                        high_y = int(high_y)

                        if high_x != Xsiz - 1:
                            error_report.EmitError('INSPIRE_TG', 'RectifiedGrid.limits.GridEnvelope.high[x](=%d) != Xsiz(=%d) - 1' % (high_x, Xsiz), conformance_class='A.8.6')
                        if high_y != Ysiz - 1:
                            error_report.EmitError('INSPIRE_TG', 'RectifiedGrid.limits.GridEnvelope.high[y](=%d) != Ysiz(=%d) - 1' % (high_y, Ysiz), conformance_class='A.8.6')
                    else:
                        error_report.EmitError('INSPIRE_TG', 'Cannot find low/high node in RectifiedGrid')

                    check_oi_rg_consistency(filename, gdal.SerializeXMLTree(rg), error_report)

                rangetype = find_xml_node(oic, 'gmlcov:rangeType')
                if rangetype is None:
                    rangetype = find_xml_node(oic, 'rangeType')
                if rangetype is None:
                    error_report.EmitError('INSPIRE_TG', 'Cannot find gmlcov:rangeType in OrthoImageryCoverage')
                else:
                    datarecord = find_xml_node(rangetype, 'swe:DataRecord')
                    if datarecord is None:
                        datarecord = find_xml_node(rangetype, 'DataRecord')
                    if datarecord is None:
                        error_report.EmitError('INSPIRE_TG', 'Cannot find swe:DataRecord in OrthoImageryCoverage.rangeType')
                    else:
                        count_fields = 0
                        min_vals = []
                        max_vals = []
                        for child_idx in range(XML_FIRST_CHILD_IDX, len(datarecord)):
                            child = datarecord[child_idx]
                            if child[XML_TYPE_IDX] == gdal.CXT_Element and (child[XML_VALUE_IDX] == 'swe:field' or child[XML_VALUE_IDX] == 'field'):
                                count_fields += 1

                                interval = None
                                constraint = find_xml_node(child, 'swe:constraint')
                                if constraint is None:
                                    constraint = find_xml_node(child, 'constraint')
                                if constraint is not None:
                                    AllowedValues = find_xml_node(constraint, 'swe:AllowedValues')
                                    if AllowedValues is None:
                                        AllowedValues = find_xml_node(constraint, 'AllowedValues')
                                    if AllowedValues is not None:
                                        interval = find_xml_node(AllowedValues, 'swe:interval')
                                        if interval is None:
                                            interval = find_xml_node(AllowedValues, 'interval')
                                        if interval is not None:
                                            interval = get_element_val(interval)
                                if interval is None:
                                    error_report.EmitError('INSPIRE_TG', 'Cannot find constraint.AllowedValues.interval for field %d' % count_fields)
                                    min_vals.append(None)
                                    max_vals.append(None)
                                else:
                                    (min_val, max_val) = interval.split(' ')
                                    min_val = int(min_val)
                                    max_val = int(max_val)
                                    min_vals.append(min_val)
                                    max_vals.append(max_val)

                        # Check number of fields regarding number of components
                        if pclr is None:
                            if count_fields != Csiz:
                                error_report.EmitError('INSPIRE_TG', 'count(OrthoImageryCoverage.rangeType.field)(=%d) != Csiz(=%d) ' % (count_fields, Csiz), conformance_class='A.8.6')
                            else:
                                # Check consistency of each channel bit-depth
                                # with the corresponding rangeType.field.
                                for i in range(Csiz):
                                    if tab_Ssiz[i] >= 128:
                                        tab_Ssiz[i] -= 128
                                        minSsiz = - 2 ** tab_Ssiz[i]
                                        maxSsiz = 2 ** tab_Ssiz[i] - 1
                                    else:
                                        minSsiz = 0
                                        maxSsiz = 2 ** (tab_Ssiz[i] + 1) - 1
                                    if min_vals[i] is not None and max_vals[i] is not None:
                                        if min_vals[i] != minSsiz:
                                            error_report.EmitError('INSPIRE_TG', 'rangeType.field[%d].min(=%d) != min(Ssiz[%d])(=%d)' % (i, min_vals[i], i, minSsiz), conformance_class='A.8.6')
                                        if max_vals[i] != maxSsiz:
                                            error_report.EmitError('INSPIRE_TG', 'rangeType.field[%d].max(=%d) != max(Ssiz[%d])(=%d)' % (i, max_vals[i], i, maxSsiz), conformance_class='A.8.6')
                        else:
                            if count_fields != 3:
                                error_report.EmitError('INSPIRE_TG', 'count(OrthoImageryCoverage.rangeType.field)(=%d) != 3 (for color table)' % (count_fields), conformance_class='A.8.6')

    # Validate content of COD marker
    cod = find_marker(cs, 'COD')
    if not cod:
        error_report.EmitError('GENERAL', 'No COD marker found')
    else:
        while True:
            SPcod_transformation = get_field_val(cod, 'SPcod_transformation')
            if SPcod_transformation is None:
                error_report.EmitError('GENERAL', 'cod.SPcod_transformation not found (and perhaps other fields)')
                break

            Scod = int_or_none(get_field_val(cod, 'Scod'))

            SPcod_NumDecompositions = int(get_field_val(cod, 'SPcod_NumDecompositions'))
            if (inspire_tg or Rsiz == 2) and siz:
                if XTsiz <= Xsiz - XOsiz and YTsiz <= Ysiz - YOsiz:
                    max_dim = max(XTsiz, YTsiz)
                else:
                    max_dim = max(Xsiz - XOsiz, Ysiz - YOsiz)
                if max_dim > 128 * 2**SPcod_NumDecompositions:
                    error_report.EmitError('PROFILE_1', 'Not enough decomposition levels = %d (max_dim=%d, 128 * 2**SPcod_NumDecompositions=%d)' % (SPcod_NumDecompositions, max_dim, 128 * 2**SPcod_NumDecompositions))

            SPcod_xcb_minus_2 = int(get_field_val(cod, 'SPcod_xcb_minus_2'))
            SPcod_ycb_minus_2 = int(get_field_val(cod, 'SPcod_ycb_minus_2'))
            if (inspire_tg or Rsiz == 2) and SPcod_xcb_minus_2 > 6 - 2:
                error_report.EmitError('PROFILE_1', 'SPcod_xcb_minus_2 = %d, whereas max allowed for Profile 1 is 4' % SPcod_xcb_minus_2)
            if (inspire_tg or Rsiz == 2) and SPcod_ycb_minus_2 > 6 - 2:
                error_report.EmitError('PROFILE_1', 'SPcod_ycb_minus_2 = %d, whereas max allowed for Profile 1 is 4' % SPcod_ycb_minus_2)
            if SPcod_xcb_minus_2 + SPcod_ycb_minus_2 > 8:
                error_report.EmitError('GENERAL', 'SPcod_xcb_minus_2 + SPcod_ycb_minus_2 = %d, whereas max allowed is 8' % (SPcod_xcb_minus_2 + SPcod_ycb_minus_2))

            for i in range(SPcod_NumDecompositions + 1):
                SPcod_Precincts = get_field_val(cod, 'SPcod_Precincts%d' % i)
                if SPcod_Precincts is not None and (Scod & 1) == 0:
                    error_report.EmitWarning(
                        'GENERAL',
                        'User-defined precincts %d found but '
                        'SPcod_transformation did not advertise it' % i)
                elif SPcod_Precincts is None and (Scod & 1) != 0:
                    error_report.EmitWarning('GENERAL', 'No user-defined precincts %d defined but SPcod_transformation advertized it' % i)
                elif SPcod_Precincts is None and inspire_tg:
                    error_report.EmitWarning('INSPIRE_TG', 'No user-defined precincts %d defined' % i, recommendation=39)

            break

    # Check QCD marker
    qcd = find_marker(cs, 'QCD')
    if not qcd:
        error_report.EmitError('GENERAL', 'No QCD marker found')

    # Check SOT marker
    sot = find_marker(cs, 'SOT')
    if not sot:
        error_report.EmitError('GENERAL', 'No SOT marker found')

    # Check RGN marker presence
    rgn = find_marker(cs, 'RGN')
    if inspire_tg and rgn:
        error_report.EmitError('INSPIRE_TG', 'RGN marker found, which is not allowed', requirement=26, conformance_class='A.8.16')

    # Check EOC marker
    eoc = find_marker(cs, 'EOC')
    if not eoc:
        error_report.EmitError('GENERAL', 'No EOC marker found')

    return error_report


def main(argv=sys.argv):
    i = 1
    filename = None
    oidoc = None
    ogc_schemas_location = None
    inspire_tg = False
    expected_gmljp2 = False
    datatype = 'imagery'
    while i < len(argv):
        if argv[i] == "-oidoc":
            if i >= len(argv) - 1:
                return Usage()
            oidoc = argv[i + 1]
            i = i + 1
        elif argv[i] == "-ogc_schemas_location":
            if i >= len(argv) - 1:
                return Usage()
            ogc_schemas_location = argv[i + 1]
            i = i + 1
        elif argv[i] == "-datatype":
            if i >= len(argv) - 1:
                return Usage()
            datatype = argv[i + 1]
            i = i + 1
        elif argv[i] == "-inspire_tg":
            inspire_tg = True
        elif argv[i] == "-expected_gmljp2":
            expected_gmljp2 = True
        elif argv[i][0] == '-':
            return Usage()
        elif filename is None:
            filename = argv[i]
        else:
            return Usage()

        i = i + 1

    if filename is None:
        return Usage()

    if ogc_schemas_location is None:
        try:
            os.stat('SCHEMAS_OPENGIS_NET')
            ogc_schemas_location = 'SCHEMAS_OPENGIS_NET'
        except OSError:
            pass

    if ogc_schemas_location is not None:
        try:
            os.stat('%s/xml.xsd' % ogc_schemas_location)
        except OSError:
            try:
                os.stat('%s/SCHEMAS_OPENGIS_NET/xml.xsd' % ogc_schemas_location)
                ogc_schemas_location = '%s/SCHEMAS_OPENGIS_NET' % ogc_schemas_location
            except OSError:
                print('Cannot find %s/xml.xsd. -ogc_schemas_location value is probably wrong' % ogc_schemas_location)
                return 1

    return validate(filename, oidoc, inspire_tg, expected_gmljp2, ogc_schemas_location, datatype).error_count


if __name__ == '__main__':
    sys.exit(main(sys.argv))
