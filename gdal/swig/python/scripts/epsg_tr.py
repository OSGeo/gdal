#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# ******************************************************************************
#  $Id$
#
#  Project:  CFS OGC MapServer
#  Purpose:  Script to create WKT and PROJ.4 dictionaries for EPSG GCS/PCS
#            codes.
#  Author:   Frank Warmerdam, warmerdam@pobox.com
#
# ******************************************************************************
#  Copyright (c) 2001, Frank Warmerdam
#  Copyright (c) 2009-2010, 2019, Even Rouault <even dot rouault at spatialys.com>
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

import sys

from osgeo import osr
from osgeo import gdal

# =============================================================================


def Usage():

    print('Usage: epsg_tr.py [-wkt] [-pretty_wkt] [-proj4] [-xml] [-postgis]')
    print('                  [-authority name]')
    sys.exit(1)

# =============================================================================


def trHandleCode(set_srid, srs, auth_name, code, deprecated, output_format):

    if output_format == '-pretty_wkt':
        print('%s:%s' % (auth_name, str(code)))
        print(srs.ExportToPrettyWkt())

    if output_format == '-xml':
        print(srs.ExportToXML())

    if output_format == '-wkt':
        print('EPSG:%d' % code)
        print(srs.ExportToWkt())

    if output_format == '-proj4':
        out_string = srs.ExportToProj4()

        name = srs.GetName()

        print('# %s' % name)
        if out_string.find('+proj=') > -1:
            print('<%s> %s <>' % (str(code), out_string))
        else:
            print('# Unable to translate coordinate system '
                    '%s:%s into PROJ.4 format.' % (auth_name, str(code)))
            print('#')

    if output_format == '-postgis':

        if code in set_srid:
            if auth_name == 'ESRI':
                if int(code) < 32767:
                    return
        assert code not in set_srid, (auth_name, code)
        set_srid.add(code)

        name = srs.GetName()
        if deprecated and 'deprecated' not in name:
            name += " (deprecated)"
        wkt = srs.ExportToWkt()
        proj4text = srs.ExportToProj4()

        print('---')
        print('--- %s %s : %s' % (auth_name, str(code), name))
        print('---')

        if proj4text is None or len(proj4text) == 0:
            print('-- (unable to translate to PROJ.4)')
        else:
            wkt = gdal.EscapeString(wkt, scheme=gdal.CPLES_SQL)
            proj4text = gdal.EscapeString(proj4text, scheme=gdal.CPLES_SQL)
            print('INSERT INTO "spatial_ref_sys" ("srid","auth_name","auth_srid","srtext","proj4text") VALUES (%d,\'%s\',%d,\'%s\',\'%s\');' %
                    (int(code), auth_name, int(code), wkt, proj4text))

    # INGRES COPY command input.
    if output_format == '-copy':

        try:
            wkt = srs.ExportToWkt()
            proj4text = srs.ExportToProj4()

            print('%s\t%d%s\t%s\t%d%s\t%d%s\n'
                    % (str(code), 4, auth_name, str(code), len(wkt), wkt,
                        len(proj4text), proj4text))
        except:
            pass

# =============================================================================


if __name__ == '__main__':

    output_format = '-pretty_wkt'
    authority = None

    argv = gdal.GeneralCmdLineProcessor(sys.argv)
    if argv is None:
        sys.exit(0)

    # Parse command line arguments.

    i = 1
    while i < len(argv):
        arg = argv[i]

        if arg == '-wkt' or arg == '-pretty_wkt' or arg == '-proj4' \
           or arg == '-postgis' or arg == '-xml' or arg == '-copy':
            output_format = arg

        elif arg == '-authority':
            i = i + 1
            authority = argv[i]

        elif arg[0] == '-':
            Usage()

        else:
            Usage()

        i = i + 1

    # Output BEGIN transaction for PostGIS
    if output_format == '-postgis':
        print('BEGIN;')

    # loop over all codes to generate output
    
    if authority:
        authorities = [ authority ]
    elif output_format == '-postgis' :
        authorities = [ 'EPSG', 'ESRI' ]
    else:
        authorities = [ 'EPSG', 'ESRI', 'IGNF' ]

    set_srid = set()
    for authority in authorities:
        if authority in ('EPSG', 'ESRI'):
            set_codes_geographic = set()
            set_codes_geographic_3d = set()
            set_codes_projected = set()
            set_codes_geocentric = set()
            set_codes_compound = set()
            set_deprecated = set()

            for crs_info in osr.GetCRSInfoListFromDatabase(authority):
                code = int(crs_info.code)
                if crs_info.type == osr.OSR_CRS_TYPE_COMPOUND:
                    set_codes_compound.add(code)
                elif crs_info.type == osr.OSR_CRS_TYPE_GEOGRAPHIC_3D:
                    set_codes_geographic_3d.add(code)
                elif crs_info.type == osr.OSR_CRS_TYPE_GEOGRAPHIC_2D:
                    set_codes_geographic.add(code)
                elif crs_info.type == osr.OSR_CRS_TYPE_PROJECTED:
                    set_codes_projected.add(code)
                elif crs_info.type == osr.OSR_CRS_TYPE_GEOCENTRIC:
                    set_codes_geocentric.add(code)

                if crs_info.deprecated:
                    set_deprecated.add(code)

            set_codes_geographic = sorted(set_codes_geographic)
            set_codes_geographic_3d = sorted(set_codes_geographic_3d)
            set_codes_projected = sorted(set_codes_projected)
            set_codes_geocentric = sorted(set_codes_geocentric)
            set_codes_compound = sorted(set_codes_compound)
            for typestr, set_codes in (('Geographic 2D CRS', set_codes_geographic),
                                       ('Projected CRS', set_codes_projected),
                                       ('Geocentric CRS', set_codes_geocentric),
                                       ('Compound CRS', set_codes_compound),
                                       ('Geographic 3D CRS', set_codes_geographic_3d)):
                if set_codes and output_format == '-postgis':
                    print('-' * 80)
                    print('--- ' + authority + ' ' + typestr)
                    print('-' * 80)

                for code in set_codes:
                    srs = osr.SpatialReference()
                    srs.SetFromUserInput(authority + ':' + str(code))
                    deprecated = False
                    if code in set_deprecated:
                        deprecated = True
                    trHandleCode(set_srid, srs, authority, str(code), deprecated, output_format)

        else:
            for crs_info in osr.GetCRSInfoListFromDatabase(authority):
                srs = osr.SpatialReference()
                srs.SetFromUserInput(authority + ':' + crs_info.code)
                trHandleCode(set_srid, srs, authority, crs_info.code, crs_info.deprecated, output_format)

    # Output COMMIT transaction for PostGIS
    if output_format == '-postgis':
        print('COMMIT;')
        print('VACUUM ANALYZE spatial_ref_sys;')
