#******************************************************************************
#  $Id$
# 
#  Project:  GDAL ECW Driver
#  Purpose:  Script to lookup ECW (GDT) coordinate systems and translate
#            into OGC WKT for storage in $GDAL_HOME/data/ecw_cs.dat.
#  Author:   Frank Warmerdam, warmerdam@pobox.com
# 
#******************************************************************************
#  Copyright (c) 2003, Frank Warmerdam
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
# Revision 1.3  2006/09/05 01:23:38  fwarmerdam
# do not alter linear projection parameter units (bug 1280)
#
# Revision 1.2  2006/04/03 01:46:18  fwarmerdam
# Added somenew projections
#
# Revision 1.1  2003/06/19 17:53:34  warmerda
# New
#

import string
import sys
import osr

##############################################################################
# rtod(): radians to degrees.

def r2d( rad ):
    return float(rad) / 0.0174532925199433

##############################################################################
# load_dict()

def load_dict( filename ):
    lines = open( filename ).readlines()

    this_dict = {}
    for line in lines:
        if line[:8] != 'proj_name':
	    tokens = string.split(line,',')
	    for i in range(len(tokens)):
		tokens[i] = string.strip(tokens[i])

            this_dict[tokens[0]] = tokens

    return this_dict

##############################################################################
# Mainline

#dir = 'M:/software/ER Viewer 2.0c/GDT_Data/'
dir = '/u/data/ecw/gdt/'

dict_list = [ 'tranmerc', 'lambert1', 'lamcon2', 'utm', 'albersea', 'mercator',
	      'obmerc_b', 'grinten', 'cassini', 'lambazea', 'datum_sp' ]

dict_dict = {}

for item in dict_list:
    dict_dict[item] = load_dict( dir + item + '.dat' )
    #print 'loaded: %s' % item

pfile = open( dir + 'project.dat' )
pfile.readline()

for line in pfile.readlines():
    try:
        tokens = string.split(string.strip(line),',')
        if len(tokens) < 3:
            continue
        
	for i in range(len(tokens)):
	    tokens[i] = string.strip(tokens[i])

  	id = tokens[0]
        type = tokens[1]

	lsize = float(tokens[2])
	lsize_str = tokens[2]

	dline = dict_dict[type][id]

	srs = osr.SpatialReference()

	# Handle translation of the projection parameters.

	if type != 'utm':
	    fn = float(dline[1])
	    fe = float(dline[2])

        if type == 'tranmerc':
	    srs.SetTM( r2d(dline[5]), r2d(dline[4]), float(dline[3]), fe, fn )

	elif type == 'mercator':
	    srs.SetMercator( r2d(dline[5]), r2d(dline[4]), float(dline[3]), fe, fn )

	elif type == 'grinten':
	    srs.SetVDG( r2d(dline[3]), fe, fn )

	elif type == 'cassini':
	    srs.SetCS( r2d(dline[4]), r2d(dline[3]), fe, fn )

	elif type == 'lambazea':
	    srs.SetLAEA( r2d(dline[5]), r2d(dline[4]),
                         fe, fn )

	elif type == 'lambert1':
	    srs.SetLCC1SP( r2d(dline[5]), r2d(dline[4]),
                           float(dline[3]), fe, fn )

	elif type == 'lamcon2':
	    srs.SetLCC( r2d(dline[7]), r2d(dline[8]), 
		        r2d(dline[9]), r2d(dline[6]), fe, fn )

#	elif type == 'lambert2':
#	    false_en = '+y_0=%.2f +x_0=%.2f' \
#		% (float(dline[12])*lsize, float(dline[13])*lsize)
#	    result = '+proj=lcc %s +lat_0=%s +lon_0=%s +lat_1=%s +lat_2=%s' \
#               % (false_en, r2d(dline[3]), r2d(dline[4]), 
#			r2d(dline[7]), r2d(dline[8]))

	elif type == 'albersea':
	    srs.SetACEA( r2d(dline[3]), r2d(dline[4]),
		         r2d(dline[5]), r2d(dline[6]), fe, fn )

#	elif type == 'obmerc_b':
#	    result = '+proj=omerc %s +lat_0=%s +lonc=%s +alpha=%s +k=%s' \
#               % (false_en, r2d(dline[5]), r2d(dline[6]), r2d(dline[4]), dline[3])

	elif type == 'utm':
	    srs.SetUTM( int(dline[1]), dline[2] != 'S' )

	# Handle Units from projects.dat file.
        if srs.IsProjected():
            srs.SetAttrValue( 'PROJCS', id )
            if lsize_str == '0.30480061':
                srs.SetLinearUnits( 'US Foot', float(lsize_str) )
            elif lsize_str != '1.0':
                srs.SetLinearUnits( 'unnamed', float(lsize_str) )

        wkt = srs.ExportToWkt()
        if len(wkt) > 0:
	    print '%s,%s' % (id, srs.ExportToWkt())
        else:
            print '%s,LOCAL_CS["%s - (unsupported)"]' % (id,id)
		
    except KeyError:
        print '%s,LOCAL_CS["%s - (unsupported)"]' % (id,id)

    except:
        print 'cant translate: ', line
	raise

## Translate datums to their underlying spheroid information.

pfile = open( dir + 'datum.dat' )
pfile.readline()

for line in pfile.readlines():
    tokens = string.split(string.strip(line),',')
    for i in range(len(tokens)):
        tokens[i] = string.strip(tokens[i])

    id = tokens[0]

    sp_name = tokens[2]
    dline = dict_dict['datum_sp'][id]
    srs = osr.SpatialReference()

    if id == 'WGS84':
        srs.SetWellKnownGeogCS( 'WGS84' )
    elif id == 'NAD27': 
        srs.SetWellKnownGeogCS( 'NAD27' )
    elif id == 'NAD83': 
        srs.SetWellKnownGeogCS( 'NAD83' )
    else:
        srs.SetGeogCS( tokens[1], id, sp_name, float(dline[2]), float(dline[4]) )

    print '%s,%s' % (id, srs.ExportToWkt())

