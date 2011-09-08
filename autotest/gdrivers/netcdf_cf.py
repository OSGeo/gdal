#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test NetCDF driver CF compliance.
# Author:   Etienne Tourigny <etourigny.dev at gmail dot com>
# 
###############################################################################
# Copyright (c) 2011, Etienne Tourigny <etourigny.dev at gmail dot com>
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

import os
import sys
import gdal

sys.path.append( '../pymod' )

import gdaltest

#for cf compliance checks
import shlex, subprocess, imp
import osr

###############################################################################
# CF compliance check functions

#check for necessary files and software
def netcdf_cf_setup():

    #global vars
    gdaltest.netcdf_cf_method = None
    gdaltest.netcdf_cf_files = None
    gdaltest.netcdf_drv = gdal.GetDriverByName( 'NETCDF' )
    #if netcdf is not supported, skip detection
    if gdaltest.netcdf_drv is None:
        return

    #try local method
    try:
        imp.find_module( 'cdms2' )
    except ImportError:
        print 'NOTICE: cdms2 not installed!'
        print '        see installation notes at http://pypi.python.org/pypi/cfchecker'
    else:
        xml_dir = './data/netcdf_cf_xml'
        tmp_dir = './tmp/cache'
        files = dict()
        files['a'] = xml_dir+'/area-type-table.xml'
        files['s'] = tmp_dir+'/cf-standard-name-table-v18.xml'
        #either find udunits path in UDUNITS_PATH, or based on location of udunits app, or copy all .xml files to data
        #opt_u = '/home/soft/share/udunits/udunits2.xml'
        files['u'] = xml_dir+'/udunits2.xml'
        #look for xml files
        if not ( os.path.exists(files['a']) and os.path.exists(files['s']) and os.path.exists(files['u']) ):
            print 'NOTICE: cdms2 installed, but necessary xml files are not found!'
            print '        the following files must exist:'
            print '        '+xml_dir+'/area-type-table.xml from http://cf-pcmdi.llnl.gov/documents/cf-standard-names/area-type-table/1/area-type-table.xml'
            print '        '+tmp_dir+'/cf-standard-name-table-v18.xml - http://cf-pcmdi.llnl.gov/documents/cf-standard-names/standard-name-table/18/cf-standard-name-table.xml'
            print '        '+xml_dir+'/udunits2*.xml from a UDUNITS2 install'
            #try to get cf-standard-name-table
            if not os.path.exists(files['s']):
                print '        downloading v18 of cf-standard-name-table.xml from http://cf-pcmdi.llnl.gov ...'
                if not gdaltest.download_file('http://cf-pcmdi.llnl.gov/documents/cf-standard-names/standard-name-table/18/cf-standard-name-table.xml',
                                              'cf-standard-name-table-v18.xml'):
                    print '        Failed to download, please get it and try again.'

        if os.path.exists(files['a']) and os.path.exists(files['s']) and os.path.exists(files['u']):
            gdaltest.netcdf_cf_method = 'local'
            gdaltest.netcdf_cf_files = files
            return
        else:
            print 'NOTICE: cdms2 installed, but necessary xml files are not found!'

 
    #http method with curl, should use python module but easier for now
    #dont use this for now...
    #try:
    #    bla = subprocess.Popen( ['curl'],stdout=None, stderr=None )
    #except (OSError, ValueError):
    #    command = None
    #else:
    #    method = 'http'
    #    command = shlex.split( 'curl --form cfversion="1.5" --form upload=@' + ifile + ' --form submit=\"Check file\" "http://puma.nerc.ac.uk/cgi-bin/cf-checker.pl"' )
    #    return command

    return 

def netcdf_cf_get_command(ifile, version='auto'):

    command = ''
    #fetch method obtained previously
    method = gdaltest.netcdf_cf_method
#    print 'method: '+method
    if method is not None:
        if method is 'local':
            command = ['./netcdf_cfchecks.py', \
                       '-a', gdaltest.netcdf_cf_files['a'], \
                       '-s', gdaltest.netcdf_cf_files['s'], \
                       '-u', gdaltest.netcdf_cf_files['u'], \
                           '-v', version, ifile]

    return command
        

###############################################################################
# CF compliance check functions

def netcdf_cf_check_file(ifile,version='auto'):

    print 'checking file ' + ifile

    output_all = ''

    if ( not os.path.exists(ifile) ):
        print 'ERROR - file ' + ifile + ' not found!'
        return 'fail'

    command = netcdf_cf_get_command(ifile, version='auto')
#    print "command: ",command
    if command is None:
        print 'no suitable method found, skipping'
        return 'skip'

    try:
#        print command
        proc = subprocess.Popen( command, \
                                     stderr=subprocess.PIPE, stdout=subprocess.PIPE )
    except (OSError, ValueError):
        print 'ERROR with command - ' + ' '.join(command)
        return 'fail'
        
    output_all = proc.stdout.read()
    output_err = ''
    output_warn = ''

    for line in output_all.splitlines( ):
        #optimize this with regex
        if 'ERROR' in line and not 'ERRORS' in line:
#        if 'ERROR' in line:
            output_err = output_err + '\n' + line
        elif 'WARNING' in line and not 'WARNINGS' in line:
#        elif 'WARNING' in line:
            output_warn = output_warn + '\n' + line

    result = 'success'
    if output_err is not '':
        result = 'fail'
        print '\n=> ERRORS for file ' + ifile + ' : ' + output_err + '\n'
    if output_warn is not '':
        print '\n=> WARNINGS for file ' + ifile + ' : ' + output_warn + '\n'

    return result

###############################################################################
# basic CF compliance check test

def netcdf_cf_1():

    netcdf_cf_setup()

    if gdaltest.netcdf_drv is None or gdaltest.netcdf_cf_method is None:
        return 'skip'

    src_file = 'data/cf-bug636.nc'

    result = netcdf_cf_check_file( src_file )
    return result
        
###############################################################################
# basic CF compliance check test

def netcdf_cf_2():

    if gdaltest.netcdf_drv is None or gdaltest.netcdf_cf_method is None:
        return 'skip'

#    src_file = 'data/cf_geog.nc'
    src_file = 'data/cf-bug636.nc'
    dst_file = 'tmp/netcdf2.nc'
    src_ds = gdal.Open( src_file )
    if ( src_ds is None ):
        return 'fail'
    dst_ds = gdaltest.netcdf_drv.CreateCopy( dst_file, src_ds )
    if ( dst_ds is None ):
        return 'fail'

    result1 = netcdf_cf_check_file( src_file )
    result2 = netcdf_cf_check_file( dst_file )

    if result1 is 'fail' or result2 is 'fail':
        result = 'fail'
    else:
        result = 'success'

    src_ds = None
    base_ds = None
    gdaltest.clean_tmp()

    return result
        
###############################################################################

gdaltest_list = [
    netcdf_cf_1, netcdf_cf_2 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'netcdf_cf' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

