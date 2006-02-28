#!/usr/bin/env python
#******************************************************************************
#  $Id$
# 
#  Project:  CFS OGC MapServer
#  Purpose:  Script to create WKT and PROJ.4 dictionaries for EPSG GCS/PCS
#            codes.
#  Author:   Frank Warmerdam, warmerdam@pobox.com
# 
#******************************************************************************
#  Copyright (c) 2001, Frank Warmerdam
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
#******************************************************************************
# 
# $Log$
# Revision 1.14  2006/02/28 05:36:13  fwarmerdam
# Added some comments in proj.4 output.  Not sure how this got lost.
#
# Revision 1.13  2004/11/01 17:25:39  fwarmerdam
# ensure SQL and PROJ.4 strings are SQL escaped for PostGIS
#
# Revision 1.12  2004/05/10 17:09:30  warmerda
# improve PostGIS output
#
# Revision 1.11  2004/05/03 20:00:54  warmerda
# Don't double up no_defs.
#
# Revision 1.10  2004/05/03 19:58:56  warmerda
# Fixed up PROJ.4 error handling.
#
# Revision 1.9  2004/04/29 13:47:33  warmerda
# improve handling of untranslatable codes
#
# Revision 1.8  2004/03/10 19:08:42  warmerda
# added -list to usage
#
# Revision 1.7  2003/03/21 22:23:27  warmerda
# added xml support
#
# Revision 1.6  2002/12/13 06:36:18  warmerda
# added postgis output
#
# Revision 1.5  2002/12/03 04:43:11  warmerda
# remove time checking!
#
# Revision 1.4  2001/03/23 03:41:16  warmerda
# fixed bug in print statement
#
# Revision 1.3  2001/03/21 02:28:21  warmerda
# fixed proj.4 catalog output
#
# Revision 1.2  2001/03/15 03:20:12  warmerda
# various improvements
#
# Revision 1.1  2001/03/14 20:38:51  warmerda
# New
#
#

import osr
import sys
import string
import gdal

# =============================================================================
def Usage():

    print 'Usage: epsg_tr.py [-wkt] [-pretty_wkt] [-proj4] [-xml] [-postgis]'
    print '                  [-skip] [-list filename] [start_code [end_code]]'
    sys.exit(1)

# =============================================================================
def trHandleCode(code, gen_dict_line, report_error, output_format):

    import time

    try:
        err = prj_srs.ImportFromEPSG( code )
    except:
        err = 1

    if err != 0 and report_error:
        print 'Unable to lookup ',code,', either not a valid EPSG'
        print 'code, or it the EPSG csv files are not accessable.'
        sys.exit(2)
    else:
        if output_format == '-pretty_wkt':
            if gen_dict_line:
                print 'EPSG:',code

            print prj_srs.ExportToPrettyWkt()

        if output_format == '-xml':
            print prj_srs.ExportToXML()
            
        if output_format == '-wkt':
            if gen_dict_line:
                print 'EPSG:',code
                    
            print prj_srs.ExportToWkt()
                
        if output_format == '-proj4':
            out_string = prj_srs.ExportToProj4()

            name = prj_srs.GetAttrValue('PROJCS')
            if name is None:
                name = prj_srs.GetAttrValue('GEOGCS')

            if name is None:
                name = 'Unknown'
            
            print '# %s' % name
            if err == 0 and string.find(out_string,'+proj=') > -1:
                print '<%s> %s <>' % (str(code), out_string)
            else:
                print '# Unable to translate coordinate system EPSG:%d into PROJ.4 format.' % code
                print '#'

        if output_format == '-postgis':
            name = prj_srs.GetAttrValue('PROJCS')
            if name is None:
                name = prj_srs.GetAttrValue('GEOGCS')

            try:
                proj4text = prj_srs.ExportToProj4()
            except:
                err = 1
            wkt = prj_srs.ExportToWkt()
            
            print '---'
            print '--- EPSG %d : %s' % (code, name)
            print '---'

            if err:
                print '-- (unable to translate)'
            else:
                wkt = gdal.EscapeString(wkt,scheme=gdal.CPLES_SQL)
                proj4text = gdal.EscapeString(proj4text,scheme=gdal.CPLES_SQL)
                print 'INSERT INTO "spatial_ref_sys" ("srid","auth_name","auth_srid","srtext","proj4text") VALUES (%s,\'EPSG\',%s,\'%s\',\'%s\');' % \
                      (str(code),str(code),wkt,proj4text)
            
# =============================================================================

if __name__ == '__main__':

    start_code = -1
    end_code = -1
    list_file = None
    output_format = '-pretty_wkt'
    report_error = 1
    
    # Parse command line arguments.
    
    i = 1
    while i < len(sys.argv):
        arg = sys.argv[i]

        if arg == '-wkt' or arg == '-pretty_wkt' or arg == '-proj4' \
           or arg == '-postgis' or arg == '-xml':
            output_format = arg

        elif arg[:5] == '-skip':
            report_error = 0
            
        elif arg == '-list' and i < len(sys.argv)-1:
            i = i + 1
            list_file = sys.argv[i]
            
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
        print 'BEGIN;'

    # Do we need to produce a single output definition, or include a
    # dictionary line for each entry?

    gen_dict_line = start_code != end_code

    # loop over all codes to generate output
    
    prj_srs = osr.SpatialReference()

    if start_code != -1:
        for code in range(start_code,end_code+1):
            trHandleCode(code, gen_dict_line, report_error, output_format)

    # loop over codes read from list file.

    elif list_file is not None:

        list_fd = open( list_file )
        line = list_fd.readline()
        while len(line) > 0:
            try:
                c_offset = string.find(line,',')
                if c_offset > 0:
                    line = line[:c_offset]
                    
                code = string.atoi(line)
            except:
                code = -1

            if code <> -1:
                trHandleCode(code, gen_dict_line, report_error, output_format)
                
            line = list_fd.readline()

    else:
        Usage()
        
    # Output COMMIT transaction for PostGIS
    if output_format == '-postgis':
        print 'COMMIT;'
        print 'VACUUM ANALYZE;'


