# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  MapServer
# Purpose:  XML validator
# Author:   Even Rouault, <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (C) 2013, Even Rouault
# Portions coming from EOxServer
# ( https://github.com/EOxServer/eoxserver/blob/master/eoxserver/services/testbase.py )
#   Copyright (C) 2011 EOX IT Services GmbH
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies of this Software or works derived from this Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
###############################################################################

import os
import sys
from lxml import etree
import gdaltest

###############################################################################
# Remove mime header if found


def ingest_file_and_strip_mime(filename):
    data = ''
    f = open(filename, 'rt')
    for line in f.readlines():
        if line == '\r\n':
            continue
        if line == '\n':
            continue
        if 'Content-Type' in line:
            continue
        data = data + line
    f.close()
    return data

###############################################################################
# Replace http://schemas.opengis.net/foo by $(ogc_schemas_location)/foo


def substitute_ogc_schemas_location(location, ogc_schemas_location):
    if ogc_schemas_location is not None and \
            location.startswith('http://schemas.opengis.net/'):
        location = ogc_schemas_location + '/' + location[len('http://schemas.opengis.net/'):]
    return location

###############################################################################
# Replace http://inspire.ec.europa.eu/schemas/foo by $(inspire_schemas_location)/foo


def substitute_inspire_schemas_location(location, inspire_schemas_location):
    if inspire_schemas_location is not None and \
            location.startswith('http://inspire.ec.europa.eu/schemas/'):
        location = inspire_schemas_location + '/' + location[len('http://inspire.ec.europa.eu/schemas/'):]
    return location

###############################################################################
# Validation function


def validate(xml_filename_or_content, xsd_filename=None,
             application_schema_ns=None,
             ogc_schemas_location=None,
             inspire_schemas_location=None):

    try:
        if xml_filename_or_content.startswith('<'):
            doc = etree.XML(xml_filename_or_content)
        else:
            doc = etree.XML(ingest_file_and_strip_mime(xml_filename_or_content))
    except etree.Error as e:
        print(str(e))
        return False

    # Special case if this is a schema
    if doc.tag == '{http://www.w3.org/2001/XMLSchema}schema':
        for child in doc:
            if child.tag == '{http://www.w3.org/2001/XMLSchema}import':
                location = child.get('schemaLocation')
                location = substitute_ogc_schemas_location(location, ogc_schemas_location)
                location = substitute_inspire_schemas_location(location, inspire_schemas_location)
                child.set('schemaLocation', location)
        try:
            etree.XMLSchema(etree.XML(etree.tostring(doc)))
            return True
        except etree.Error as e:
            print(str(e))
            return False

    schema_locations = doc.get("{http://www.w3.org/2001/XMLSchema-instance}schemaLocation")
    if schema_locations is None:
        print('No schemaLocation found')
        return False

    # Our stripped GetFeature document have an empty timeStamp, put a
    # fake value one instead
    if doc.get('timeStamp') == '':
        doc.set('timeStamp', '1970-01-01T00:00:00Z')

    locations = schema_locations.split()

    # get schema locations
    schema_def = etree.Element("schema", attrib={
        "elementFormDefault": "qualified",
        "version": "1.0.0",
    }, nsmap={
        None: "http://www.w3.org/2001/XMLSchema"
    }
    )

    tempfiles = []
    import_dict = {}

    # Special case for the main application schema
    for ns, location in zip(locations[::2], locations[1::2]):
        if ns == application_schema_ns:
            if xsd_filename is not None:
                location = xsd_filename
            else:
                location = os.path.splitext(xml_filename_or_content)[0] + '.xsd'

            # Remove mime-type header line if found to generate a valid .xsd
            sanitized_content = ingest_file_and_strip_mime(location)
            location = '/tmp/tmpschema%d.xsd' % len(tempfiles)
            f = open(location, 'wb')
            f.write(sanitized_content)
            tempfiles.append(location)
            f.close()

            xsd = etree.XML(sanitized_content)
            for child in xsd:
                if child.tag == '{http://www.w3.org/2001/XMLSchema}import':
                    sub_ns = child.get('namespace')
                    sub_location = child.get('schemaLocation')
                    sub_location = substitute_ogc_schemas_location(sub_location, ogc_schemas_location)
                    sub_location = substitute_inspire_schemas_location(sub_location, inspire_schemas_location)
                    etree.SubElement(schema_def, "import", attrib={
                        "namespace": sub_ns,
                        "schemaLocation": sub_location
                    }
                    )
                    import_dict[sub_ns] = sub_location

            etree.SubElement(schema_def, "import", attrib={
                "namespace": ns,
                "schemaLocation": location
            }
            )
            import_dict[ns] = location

    # Add each schemaLocation as an import
    for ns, location in zip(locations[::2], locations[1::2]):
        if ns == application_schema_ns:
            continue
        location = substitute_ogc_schemas_location(location, ogc_schemas_location)
        location = substitute_inspire_schemas_location(location, inspire_schemas_location)
        if ns not in import_dict:
            etree.SubElement(schema_def, "import", attrib={
                "namespace": ns,
                "schemaLocation": location
            }
            )
            import_dict[ns] = location

    # TODO: ugly workaround. But otherwise, the doc is not recognized as schema
    # print(etree.tostring(schema_def))
    schema = etree.XMLSchema(etree.XML(etree.tostring(schema_def)))

    try:
        schema.assertValid(doc)
        ret = True
    except etree.Error as e:
        print(str(e))
        ret = False

    for filename in tempfiles:
        os.remove(filename)

    return ret

###############################################################################
# Transform absolute schemaLocations into relative ones


def transform_abs_links_to_ref_links(path, level=0):
    for filename in os.listdir(path):
        filename = os.path.join(path, filename)
        if os.path.isdir(filename) and 'examples' not in filename:
            transform_abs_links_to_ref_links(filename, level + 1)
        elif filename.endswith('.xsd'):
            f = open(filename, 'rt')
            lines = f.readlines()
            f.close()
            rewrite = False
            for i, ln in enumerate(lines):
                if ln[-1] == '\n':
                    ln = ln[0:-1]
                pos = ln.find('http://schemas.opengis.net/')
                if pos >= 0:
                    rewrite = True
                    s = ln[0:pos]
                    s += "../" * level
                    s = s + ln[pos + len('http://schemas.opengis.net/'):]
                    ln = s
                    lines[i] = ln

                pos = ln.find('http://www.w3.org/1999/xlink.xsd')
                if pos >= 0:
                    rewrite = True
                    s = ln[0:pos]
                    s += "../" * level
                    s = s + ln[pos + len('http://www.w3.org/1999/'):]
                    ln = s
                    lines[i] = ln

                pos = ln.find('http://www.w3.org/2001/xml.xsd')
                if pos >= 0:
                    rewrite = True
                    s = ln[0:pos]
                    s += "../" * level
                    s += ln[pos + len('http://www.w3.org/2001/'):]
                    ln = s
                    lines[i] = ln

            if rewrite:
                f = open(filename, 'wt')
                f.writelines(lines)
                f.close()

###############################################################################
# Transform absolute schemaLocations into relative ones


def transform_inspire_abs_links_to_ref_links(path, level=0):
    for filename in os.listdir(path):
        filename = os.path.join(path, filename)
        if os.path.isdir(filename) and 'examples' not in filename:
            transform_inspire_abs_links_to_ref_links(filename, level + 1)
        elif filename.endswith('.xsd'):
            f = open(filename, 'rt')
            lines = f.readlines()
            f.close()
            rewrite = False
            for i, ln in enumerate(lines):
                ln = lines[i]
                if ln[-1] == '\n':
                    ln = ln[0:-1]

                pos = ln.find('schemaLocation="http://inspire.ec.europa.eu/schemas/')
                if pos >= 0:
                    pos += len('schemaLocation="')
                    rewrite = True
                    s = ln[0:pos] + "../" * level
                    s += ln[pos + len('http://inspire.ec.europa.eu/schemas/'):]
                    ln = s
                    lines[i] = ln

                pos = ln.find('http://portele.de/')
                if pos >= 0:
                    rewrite = True
                    s = ln[0:pos]
                    s = s + ln[pos + len('http://portele.de/'):]
                    ln = s
                    lines[i] = ln

                pos = ln.find('http://schemas.opengis.net/')
                if pos >= 0:
                    rewrite = True
                    s = ln[0:pos]
                    s += "../" * level + "../SCHEMAS_OPENGIS_NET/"
                    s = s + ln[pos + len('http://schemas.opengis.net/'):]
                    ln = s
                    lines[i] = ln

            if rewrite:
                f = open(filename, 'wb')
                f.writelines(lines)
                f.close()

###############################################################################
# Download OGC schemas


def download_ogc_schemas(ogc_schemas_url='http://schemas.opengis.net/SCHEMAS_OPENGIS_NET.zip',
                         xlink_xsd_url='http://www.w3.org/1999/xlink.xsd',
                         xml_xsd_url='http://www.w3.org/2001/xml.xsd',
                         target_dir='.',
                         target_subdir='SCHEMAS_OPENGIS_NET',
                         force_download=False,
                         max_download_duration=None):
    try:
        os.mkdir(target_dir)
    except OSError:
        pass

    try:
        os.stat(target_dir + '/' + 'SCHEMAS_OPENGIS_NET.zip')
    except OSError:
        if not gdaltest.download_file(ogc_schemas_url, target_dir + '/' + 'SCHEMAS_OPENGIS_NET.zip', base_dir='.', force_download=force_download, max_download_duration=max_download_duration):
            return False

    try:
        os.stat(target_dir + '/' + target_subdir + '/wfs')
    except OSError:
        try:
            os.mkdir(target_dir + '/' + target_subdir)
        except OSError:
            pass

        gdaltest.unzip(target_dir + '/' + target_subdir, target_dir + '/' + 'SCHEMAS_OPENGIS_NET.zip')
        try:
            os.stat(target_dir + '/' + target_subdir + '/wfs')
        except OSError:
            print('Cannot unzip SCHEMAS_OPENGIS_NET.zip')
            return False

    try:
        os.stat(target_dir + '/' + target_subdir + '/xlink.xsd')
    except OSError:
        if not gdaltest.download_file(xlink_xsd_url, target_dir + '/' + target_subdir + '/xlink.xsd', base_dir='.', force_download=force_download, max_download_duration=max_download_duration):
            if not gdaltest.download_file('http://even.rouault.free.fr/xlink.xsd', target_dir + '/' + target_subdir + '/xlink.xsd', base_dir='.', force_download=force_download, max_download_duration=max_download_duration):
                return False

    try:
        os.stat(target_dir + '/' + target_subdir + '/xml.xsd')
    except OSError:
        if not gdaltest.download_file(xml_xsd_url, target_dir + '/' + target_subdir + '/xml.xsd', base_dir='.', force_download=force_download, max_download_duration=max_download_duration):
            if not gdaltest.download_file('http://even.rouault.free.fr/xml.xsd', target_dir + '/' + target_subdir + '/xml.xsd', base_dir='.', force_download=force_download, max_download_duration=max_download_duration):
                return False

    transform_abs_links_to_ref_links(target_dir + '/' + target_subdir)

    return True

###############################################################################
# Download INSPIRE schemas


def download_inspire_schemas(target_dir='.',
                             target_subdir='inspire_schemas',
                             force_download=False):

    if not download_ogc_schemas(target_dir=target_dir, force_download=force_download):
        return False

    try:
        os.stat(target_dir + '/' + 'inspire_common_1.0.1.zip')
    except OSError:
        gdaltest.download_file('http://inspire.ec.europa.eu/schemas/common/1.0.1.zip', target_dir + '/' + 'inspire_common_1.0.1.zip', base_dir='.', force_download=force_download)

    try:
        os.stat(target_dir + '/' + 'inspire_vs_1.0.1.zip')
    except OSError:
        gdaltest.download_file('http://inspire.ec.europa.eu/schemas/inspire_vs/1.0.1.zip', target_dir + '/' + 'inspire_vs_1.0.1.zip', base_dir='.', force_download=force_download)

    for subdir in ['', '/common', '/inspire_vs', '/inspire_dls', '/inspire_dls/1.0']:
        try:
            os.mkdir(target_dir + '/' + target_subdir + subdir)
        except OSError:
            pass

    try:
        os.stat(target_dir + '/' + target_subdir + '/common/1.0')
    except OSError:
        gdaltest.unzip(target_dir + '/' + target_subdir + '/common', target_dir + '/' + 'inspire_common_1.0.1.zip')
        try:
            os.stat(target_dir + '/' + target_subdir + '/common/1.0')
        except OSError:
            print('Cannot unzip inspire_common_1.0.1.zip')
            return False

    try:
        os.stat(target_dir + '/' + target_subdir + '/inspire_vs/1.0')
    except OSError:
        gdaltest.unzip(target_dir + '/' + target_subdir + '/inspire_vs', target_dir + '/' + 'inspire_vs_1.0.1.zip')
        try:
            os.stat(target_dir + '/' + target_subdir + '/inspire_vs/1.0')
        except OSError:
            print('Cannot unzip inspire_vs_1.0.1.zip')
            return False

    try:
        os.stat(target_dir + '/' + target_subdir + '/inspire_dls/1.0/inspire_dls.xsd')
    except OSError:
        gdaltest.download_file('http://inspire.ec.europa.eu/schemas/inspire_dls/1.0/inspire_dls.xsd', target_dir + '/' + target_subdir + '/inspire_dls/1.0/inspire_dls.xsd', base_dir='.', force_download=force_download)

    try:
        os.stat(target_dir + '/' + target_subdir + '/oi/3.0/Orthoimagery.xsd')
    except OSError:
        try:
            os.makedirs(target_dir + '/' + target_subdir + '/oi/3.0')
        except OSError:
            pass
        gdaltest.download_file('http://inspire.ec.europa.eu/schemas/oi/3.0/Orthoimagery.xsd', target_dir + '/' + target_subdir + '/oi/3.0/Orthoimagery.xsd', base_dir='.', force_download=force_download)
        gdaltest.download_file('http://portele.de/ShapeChangeAppinfo.xsd', target_dir + '/' + target_subdir + '/oi/3.0/ShapeChangeAppinfo.xsd', base_dir='.', force_download=force_download)

    try:
        os.stat(target_dir + '/' + target_subdir + '/base/3.3/BaseTypes.xsd')
    except OSError:
        try:
            os.makedirs(target_dir + '/' + target_subdir + '/base/3.3')
        except OSError:
            pass
        gdaltest.download_file('http://inspire.ec.europa.eu/schemas/base/3.3/BaseTypes.xsd', target_dir + '/' + target_subdir + '/base/3.3/BaseTypes.xsd', base_dir='.', force_download=force_download)

    transform_inspire_abs_links_to_ref_links(target_dir + '/' + target_subdir)

    return True

###############################################################################
# has_local_ogc_schemas()


def has_local_ogc_schemas(path):

    # Autodetect OGC schemas
    try:
        os.stat(path + '/wfs')
        os.stat(path + '/xlink.xsd')
        os.stat(path + '/xml.xsd')
        return True
    except OSError:
        return False

###############################################################################
# has_local_inspire_schemas()


def has_local_inspire_schemas(path):

    # Autodetect INSPIRE schemas
    try:
        os.stat(path + '/common/1.0/common.xsd')
        os.stat(path + '/inspire_vs/1.0/inspire_vs.xsd')
        os.stat(path + '/inspire_dls/1.0/inspire_dls.xsd')
        os.stat(path + '/oi/3.0/Orthoimagery.xsd')
        os.stat(path + '/base/3.3/BaseTypes.xsd')
        return True
    except OSError:
        return False


def Usage():
    print('Usage: xmlvalidate.py [-target_dir dir] [-download_ogc_schemas] [-ogc_schemas_location path]')
    print('                      [-download_inspire_schemas] [-inspire_schemas_location path]')
    print('                      [-app_schema_ns ns] [-schema some.xsd]')
    print('                      some.xml')
    return 255


def main(argv):
    if len(argv) < 2:
        return Usage()
    else:
        print('command line usage is not implemented yet')
        return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
