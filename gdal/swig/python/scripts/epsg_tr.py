#!/usr/bin/env python
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
#  Copyright (c) 2009-2010, Even Rouault <even dot rouault at mines-paris dot org>
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
    print('                  [-skip] [-list filename] [start_code [end_code]]')
    sys.exit(1)

# =============================================================================


def trHandleCode(code, gen_dict_line, report_error, output_format):

    try:
        err = prj_srs.ImportFromEPSG(code)
    except:
        err = 1

    if err != 0 and report_error:
        print('Unable to lookup %d, either not a valid EPSG' % code)
        print('code, or it the EPSG CSV files are not accessible.')
        sys.exit(2)
    else:
        if output_format == '-pretty_wkt':
            if gen_dict_line:
                print('EPSG:%d' % code)

            print(prj_srs.ExportToPrettyWkt())

        if output_format == '-xml':
            print(prj_srs.ExportToXML())

        if output_format == '-wkt':
            if gen_dict_line:
                print('EPSG:%d' % code)

            print(prj_srs.ExportToWkt())

        if output_format == '-proj4':
            out_string = prj_srs.ExportToProj4()

            name = prj_srs.GetAttrValue('COMPD_CS')
            if name is None:
                name = prj_srs.GetAttrValue('PROJCS')
            if name is None:
                name = prj_srs.GetAttrValue('GEOGCS')
            if name is None:
                name = prj_srs.GetAttrValue('GEOCCS')

            if name is None:
                name = 'Unknown'

            print('# %s' % name)
            if err == 0 and out_string.find('+proj=') > -1:
                print('<%s> %s <>' % (str(code), out_string))
            else:
                print('# Unable to translate coordinate system '
                      'EPSG:%d into PROJ.4 format.' % code)
                print('#')

        if output_format == '-postgis':

            name = prj_srs.GetAttrValue('COMPD_CS')
            if name is None:
                name = prj_srs.GetAttrValue('PROJCS')
            if name is None:
                name = prj_srs.GetAttrValue('GEOGCS')
            if name is None:
                name = prj_srs.GetAttrValue('GEOCCS')

            try:
                proj4text = prj_srs.ExportToProj4()
            except:
                err = 1
            wkt = prj_srs.ExportToWkt()

            print('---')
            print('--- EPSG %d : %s' % (code, name))
            print('---')

            if err:
                print('-- (unable to translate)')
            else:
                wkt = gdal.EscapeString(wkt, scheme=gdal.CPLES_SQL)
                proj4text = gdal.EscapeString(proj4text, scheme=gdal.CPLES_SQL)
                print('INSERT INTO "spatial_ref_sys" ("srid","auth_name","auth_srid","srtext","proj4text") VALUES (%s,\'EPSG\',%s,\'%s\',\'%s\');' %
                      (str(code), str(code), wkt, proj4text))

        # INGRES COPY command input.
        if output_format == '-copy':

            try:
                wkt = prj_srs.ExportToWkt()
                proj4text = prj_srs.ExportToProj4()

                print('%d\t%d%s\t%d\t%d%s\t%d%s\n'
                      % (code, 4, 'EPSG', code, len(wkt), wkt,
                          len(proj4text), proj4text))
            except:
                pass

# =============================================================================


if __name__ == '__main__':

    start_code = -1
    end_code = -1
    list_file = None
    output_format = '-pretty_wkt'
    report_error = 1

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

        elif arg[:5] == '-skip':
            report_error = 0

        elif arg == '-list' and i < len(argv) - 1:
            i = i + 1
            list_file = argv[i]

        elif arg[0] == '-':
            Usage()

        elif int(arg) > 0:

            if start_code == -1:
                start_code = int(arg)
                end_code = int(arg)
            elif end_code == start_code:
                end_code = int(arg)
            else:
                Usage()
        else:
            Usage()

        i = i + 1

    # Output BEGIN transaction for PostGIS
    if output_format == '-postgis':
        print('BEGIN;')

    # Do we need to produce a single output definition, or include a
    # dictionary line for each entry?

    gen_dict_line = start_code != end_code

    # loop over all codes to generate output

    prj_srs = osr.SpatialReference()

    if start_code != -1:
        for code in range(start_code, end_code + 1):
            trHandleCode(code, gen_dict_line, report_error, output_format)

    # loop over codes read from list file.

    elif list_file is not None:

        list_fd = open(list_file)
        line = list_fd.readline()
        while line:
            try:
                c_offset = line.find(',')
                if c_offset > 0:
                    line = line[:c_offset]

                code = int(line)
            except:
                code = -1

            if code != -1:
                trHandleCode(code, gen_dict_line, report_error, output_format)

            line = list_fd.readline()

    else:
        Usage()

    # Output COMMIT transaction for PostGIS
    if output_format == '-postgis':
        print('COMMIT;')
        print('VACUUM ANALYZE spatial_ref_sys;')
