#!/usr/bin/env python
###############################################################################
# $Id: run_all.py,v 1.6 2006/03/21 03:16:17 fwarmerdam Exp $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  High level test executive ... it runs sub test scripts. 
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
# 
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
# 
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
# 
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.
###############################################################################
# 
#  $Log: run_all.py,v $
#  Revision 1.6  2006/03/21 03:16:17  fwarmerdam
#  return failed count
#
#  Revision 1.5  2005/01/22 06:12:00  fwarmerdam
#  allow passing of subdirs to run as args.
#
#  Revision 1.4  2004/02/25 15:14:36  warmerda
#  Removed alg directory, no tests there anyways.
#
#  Revision 1.3  2004/02/04 21:11:52  warmerda
#  added osr and alg directories
#
#  Revision 1.2  2003/04/16 19:39:38  warmerda
#  added gdrivers
#
#  Revision 1.1  2003/03/17 05:43:07  warmerda
#  New
#

import sys
sys.path.append( 'pymod' )
import gdaltest

test_list = []
for i in range(1,len(sys.argv)):
    test_list.append( sys.argv[i] )

if len(test_list) == 0:
    test_list = [ 'ogr', 'gcore', 'gdrivers', 'osr' ]

gdaltest.setup_run( 'gdalautotest_all' )

gdaltest.run_all( test_list, [] )

errors = gdaltest.summarize()

sys.exit( errors )

